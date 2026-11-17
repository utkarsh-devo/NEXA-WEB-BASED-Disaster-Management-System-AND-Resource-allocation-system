#pragma once
// ============================================================================
// NEXA :: Authentication & role-based authorisation (spec §8.3)
// Passwords: salted + iterated SHA-256 (utilities/Sha256.hpp).
// Sessions: random 256-bit bearer tokens in a HashTable with expiry.
// ============================================================================
#include <mutex>
#include "ServiceContext.hpp"

namespace nexa::svc {

enum class Permission { View, ManageIncidents, ManageResources, ManageRequests, Allocate, ManageHospitals, ManageNetwork, RunSimulation, ManageUsers, ManageSettings };

inline const char* toString(Permission p) {
    static const char* n[] = {"view", "manage_incidents", "manage_resources", "manage_requests", "allocate", "manage_hospitals", "manage_network", "run_simulation", "manage_users", "manage_settings"};
    return n[(int)p];
}

/// Role -> permission matrix (documented in README).
inline bool roleCan(Role r, Permission p) {
    switch (r) {
        case Role::Admin: return true;
        case Role::IncidentOperator:
            return p == Permission::View || p == Permission::ManageIncidents || p == Permission::ManageRequests || p == Permission::Allocate || p == Permission::RunSimulation;
        case Role::ResourceCoordinator:
            return p == Permission::View || p == Permission::ManageResources || p == Permission::Allocate || p == Permission::ManageRequests || p == Permission::ManageNetwork;
        case Role::MedicalCoordinator:
            return p == Permission::View || p == Permission::ManageHospitals || p == Permission::ManageRequests;
        case Role::Viewer: return p == Permission::View;
    }
    return false;
}

struct Session { std::string token; long userId = 0; std::string username, displayName; Role role = Role::Viewer; long expiresAt = 0; };

class AuthService {
public:
    AuthService(ServiceContext& c, AuditService& a) : ctx_(c), audit_(a) {}

    void load();                               // users from repository
    Session login(const std::string& username, const std::string& password);
    void logout(const std::string& token);
    Session authenticate(const std::string& token);
    void require(const Session& s, Permission p);

    DynamicArray<User> listUsers();
    User createUser(const std::string& username, const std::string& displayName, const std::string& password, Role role, const std::string& actor);
    User updateUser(long id, const std::string& displayName, Role role, bool active, const std::string& actor);
    void resetPassword(long id, const std::string& password, const std::string& actor);
    std::size_t userCount();

    long sessionHours = 12;

private:
    ServiceContext& ctx_;
    AuditService& audit_;
    std::mutex m_;
    Registry<User> users_;
    ds::HashTable<std::string, long> byName_{ds::CollisionStrategy::QuadraticProbing, 16};
    ds::HashTable<std::string, Session> sessions_{ds::CollisionStrategy::SeparateChaining, 64};
    static void validatePassword(const std::string& p);
};

} // namespace nexa::svc
