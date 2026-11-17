#include "controllers/ApiSupport.hpp"
#include "utilities/Logger.hpp"
#include <atomic>

namespace nexa::api {

namespace { std::atomic<svc::NexaCore*> g_core{nullptr}; }

void setCore(svc::NexaCore* c) { g_core.store(c); }
svc::NexaCore* corePtr() { return g_core.load(); }
svc::NexaCore& core() {
    svc::NexaCore* c = g_core.load();
    if (!c) throw NexaError(503, "STARTING", "NEXA engine is still starting — retry in a moment");
    return *c;
}

HttpResponsePtr jsonResponse(const Json::Value& v, int status) {
    auto r = drogon::HttpResponse::newHttpJsonResponse(v);
    r->setStatusCode(static_cast<drogon::HttpStatusCode>(status));
    return r;
}

HttpResponsePtr errorResponse(int status, const std::string& code, const std::string& message) {
    Json::Value e(Json::objectValue), inner(Json::objectValue);
    inner["code"] = code;
    inner["message"] = message;
    e["error"] = inner;
    return jsonResponse(e, status);
}

std::string bearerToken(const HttpRequestPtr& req) {
    std::string h = req->getHeader("Authorization");
    const std::string prefix = "Bearer ";
    if (h.size() > prefix.size() && h.compare(0, prefix.size(), prefix) == 0) return h.substr(prefix.size());
    return req->getParameter("token");      // WebSocket / file download links
}

svc::Session authorize(const HttpRequestPtr& req, svc::Permission p) {
    svc::Session s = core().auth.authenticate(bearerToken(req));
    core().auth.require(s, p);
    return s;
}

std::string actorOf(const svc::Session& s) { return s.username; }

Json::Value body(const HttpRequestPtr& req) {
    auto j = req->getJsonObject();
    if (j) return *j;
    if (!std::string(req->getBody()).empty()) throw ValidationError("Request body must be valid JSON");
    return Json::Value(Json::objectValue);
}

std::string sv(const Json::Value& j, const char* key, const std::string& def) {
    if (!j.isMember(key) || j[key].isNull()) return def;
    return j[key].isString() ? j[key].asString() : j[key].toStyledString();
}
long lv(const Json::Value& j, const char* key, long def) {
    if (!j.isMember(key) || j[key].isNull()) return def;
    if (j[key].isNumeric()) return (long)j[key].asInt64();
    if (j[key].isString()) { try { return std::stol(j[key].asString()); } catch (...) {} }
    throw ValidationError(std::string(key) + " must be a number");
}
int iv(const Json::Value& j, const char* key, int def) { return (int)lv(j, key, def); }
double dv(const Json::Value& j, const char* key, double def) {
    if (!j.isMember(key) || j[key].isNull()) return def;
    if (j[key].isNumeric()) return j[key].asDouble();
    if (j[key].isString()) { try { return std::stod(j[key].asString()); } catch (...) {} }
    throw ValidationError(std::string(key) + " must be a number");
}
bool bv(const Json::Value& j, const char* key, bool def) {
    if (!j.isMember(key) || j[key].isNull()) return def;
    if (j[key].isBool()) return j[key].asBool();
    if (j[key].isNumeric()) return j[key].asInt() != 0;
    return j[key].asString() == "true";
}
ds::DynamicArray<long> longs(const Json::Value& j, const char* key) {
    ds::DynamicArray<long> out;
    if (!j.isMember(key) || j[key].isNull()) return out;
    if (!j[key].isArray()) throw ValidationError(std::string(key) + " must be an array of numbers");
    for (Json::ArrayIndex i = 0; i < j[key].size(); ++i) {
        const Json::Value& x = j[key][i];
        if (!x.isNumeric()) throw ValidationError(std::string(key) + " must contain only numbers");
        out.pushBack((long)x.asInt64());
    }
    return out;
}

std::string qs(const HttpRequestPtr& req, const char* key, const std::string& def) {
    const std::string& v = req->getParameter(key);
    return v.empty() ? def : v;
}
long ql(const HttpRequestPtr& req, const char* key, long def) {
    const std::string& v = req->getParameter(key);
    if (v.empty()) return def;
    try { return std::stol(v); } catch (...) { throw ValidationError(std::string(key) + " must be an integer"); }
}
int qi(const HttpRequestPtr& req, const char* key, int def) { return (int)ql(req, key, def); }
double qd(const HttpRequestPtr& req, const char* key, double def) {
    const std::string& v = req->getParameter(key);
    if (v.empty()) return def;
    try { return std::stod(v); } catch (...) { throw ValidationError(std::string(key) + " must be a number"); }
}
bool qb(const HttpRequestPtr& req, const char* key, bool def) {
    const std::string& v = req->getParameter(key);
    if (v.empty()) return def;
    return v == "1" || v == "true" || v == "yes";
}

namespace {
template <typename Fn>
void guard(Callback& cb, Fn fn) {
    try { fn(); }
    catch (const NexaError& e) {
        if (e.status() >= 500) util::Logger::instance().error("api", e.what());
        cb(errorResponse(e.status(), e.code(), e.what()));
    } catch (const std::exception& e) {
        util::Logger::instance().error("api", std::string("unhandled: ") + e.what());
        cb(errorResponse(500, "INTERNAL_ERROR", e.what()));
    } catch (...) {
        cb(errorResponse(500, "INTERNAL_ERROR", "Unknown server error"));
    }
}
} // namespace

void run(const HttpRequestPtr& req, Callback& cb, svc::Permission p, const std::function<Json::Value(const svc::Session&)>& fn, int okStatus) {
    guard(cb, [&] { svc::Session s = authorize(req, p); Json::Value out = fn(s); cb(jsonResponse(out, okStatus)); });
}
void runPublic(const HttpRequestPtr&, Callback& cb, const std::function<Json::Value()>& fn, int okStatus) {
    guard(cb, [&] { Json::Value out = fn(); cb(jsonResponse(out, okStatus)); });
}
void runRaw(const HttpRequestPtr& req, Callback& cb, svc::Permission p, const std::function<HttpResponsePtr(const svc::Session&)>& fn) {
    guard(cb, [&] { svc::Session s = authorize(req, p); cb(fn(s)); });
}

} // namespace nexa::api
