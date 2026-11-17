#include "services/AuthService.hpp"
#include "utilities/Sha256.hpp"
#include "utilities/Validation.hpp"

namespace nexa::svc {

void AuthService::validatePassword(const std::string& p) {
    if (p.size() < 8) throw ValidationError("Password must be at least 8 characters");
    bool digit = false, alpha = false;
    for (char c : p) { if (c >= '0' && c <= '9') digit = true; if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) alpha = true; }
    if (!digit || !alpha) throw ValidationError("Password must contain letters and digits");
}

void AuthService::load() {
    std::lock_guard<std::mutex> g(m_);
    users_.clear(); byName_.clear();
    for (const auto& u : ctx_.repos.users->findAll()) { users_.upsert(u); byName_.insert(util::lower(u.username), u.id); }
}

Session AuthService::login(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> g(m_);
    const long* id = byName_.find(util::lower(util::trim(username)));
    User* u = id ? users_.get(*id) : nullptr;
    // same error for unknown user / wrong password (no user enumeration)
    if (!u || !u->active || !crypto::safeEquals(crypto::hashPassword(password, u->salt), u->passwordHash)) {
        util::Logger::instance().warn("auth", "failed login for '" + username + "'");
        throw AuthError("Invalid username or password");
    }
    Session s;
    s.token = crypto::randomHex(32);
    s.userId = u->id; s.username = u->username; s.displayName = u->displayName; s.role = u->role;
    s.expiresAt = util::nowEpoch() + sessionHours * 3600;
    sessions_.insert(s.token, s);
    u->lastLoginAt = util::nowEpoch();
    try { ctx_.repos.users->update(*u); } catch (...) {}
    audit_.record("AUDIT", "auth", "login", u->username);
    return s;
}

void AuthService::logout(const std::string& token) { std::lock_guard<std::mutex> g(m_); sessions_.erase(token); }

Session AuthService::authenticate(const std::string& token) {
    std::lock_guard<std::mutex> g(m_);
    if (token.empty()) throw AuthError("Authentication required");
    Session* s = sessions_.find(token);
    if (!s) throw AuthError("Session expired or invalid — please sign in again");
    if (s->expiresAt < util::nowEpoch()) { sessions_.erase(token); throw AuthError("Session expired — please sign in again"); }
    const User* u = users_.get(s->userId);
    if (!u || !u->active) { sessions_.erase(token); throw AuthError("Account disabled"); }
    s->role = u->role;   // role changes take effect immediately
    return *s;
}

void AuthService::require(const Session& s, Permission p) {
    if (!roleCan(s.role, p)) throw ForbiddenError(std::string("Your role (") + toString(s.role) + ") cannot perform: " + toString(p));
}

DynamicArray<User> AuthService::listUsers() {
    std::lock_guard<std::mutex> g(m_);
    DynamicArray<User> out;
    users_.forEachOrdered([&](const User& u) { User c = u; c.passwordHash.clear(); c.salt.clear(); out.pushBack(c); });
    return out;
}

std::size_t AuthService::userCount() { std::lock_guard<std::mutex> g(m_); return users_.size(); }

User AuthService::createUser(const std::string& username, const std::string& displayName, const std::string& password, Role role, const std::string& actor) {
    std::lock_guard<std::mutex> g(m_);
    std::string un = util::lower(util::trim(username));
    validate::required(un, "Username");
    validate::maxLength(un, 40, "Username");
    for (char c : un) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) throw ValidationError("Username may contain a-z, 0-9, '.', '_' and '-' only");
    if (byName_.contains(un)) throw ConflictError("Username already exists");
    validatePassword(password);
    User u;
    u.username = un; u.displayName = displayName.empty() ? un : util::trim(displayName); u.role = role; u.active = true;
    u.salt = crypto::randomHex(16);
    u.passwordHash = crypto::hashPassword(password, u.salt);
    u.createdAt = util::nowEpoch();
    ctx_.repos.users->insert(u);
    users_.upsert(u);
    byName_.insert(un, u.id);
    audit_.record("AUDIT", "users", "created user " + un + " (" + toString(role) + ")", actor);
    User c = u; c.passwordHash.clear(); c.salt.clear();
    return c;
}

User AuthService::updateUser(long id, const std::string& displayName, Role role, bool active, const std::string& actor) {
    std::lock_guard<std::mutex> g(m_);
    User* u = users_.get(id);
    if (!u) throw NotFoundError("User not found");
    User c = *u;
    if (!displayName.empty()) c.displayName = util::trim(displayName);
    c.role = role; c.active = active;
    // never lock out the last administrator
    if (u->role == Role::Admin && (role != Role::Admin || !active)) {
        int admins = 0;
        for (const auto& x : users_.items()) if (x.role == Role::Admin && x.active) ++admins;
        if (admins <= 1) throw ConflictError("At least one active administrator is required");
    }
    ctx_.repos.users->update(c);
    *u = c;
    audit_.record("AUDIT", "users", "updated user " + c.username, actor);
    User out = c; out.passwordHash.clear(); out.salt.clear();
    return out;
}

void AuthService::resetPassword(long id, const std::string& password, const std::string& actor) {
    std::lock_guard<std::mutex> g(m_);
    User* u = users_.get(id);
    if (!u) throw NotFoundError("User not found");
    validatePassword(password);
    User c = *u;
    c.salt = crypto::randomHex(16);
    c.passwordHash = crypto::hashPassword(password, c.salt);
    ctx_.repos.users->update(c);
    *u = c;
    // invalidate the user's sessions
    DynamicArray<std::string> drop;
    sessions_.forEach([&](const std::string& t, const Session& s) { if (s.userId == id) drop.pushBack(t); });
    for (const auto& t : drop) sessions_.erase(t);
    audit_.record("AUDIT", "users", "password reset for " + c.username, actor);
}

} // namespace nexa::svc
