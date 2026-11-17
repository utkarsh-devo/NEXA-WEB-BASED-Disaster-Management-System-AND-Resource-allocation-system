#pragma once
// ============================================================================
// NEXA :: domain enumerations + string conversion (used in DB and JSON)
// ============================================================================
#include <string>

namespace nexa::model {

enum class IncidentStatus { Pending, Active, Contained, Resolved, Archived };
enum class ResourceStatus { Available, Deployed, Maintenance, Inactive };
enum class RequestStatus  { Queued, Escalated, Processing, Fulfilled, Cancelled };
enum class DeploymentStatus { Active, Completed, Cancelled };
enum class Role { Admin, IncidentOperator, ResourceCoordinator, MedicalCoordinator, Viewer };

#define NEXA_ENUM_IO(Type, ...)                                                        \
    inline const char* toString(Type v) {                                              \
        static const char* names[] = {__VA_ARGS__};                                    \
        return names[static_cast<int>(v)];                                             \
    }                                                                                  \
    inline bool parse(const std::string& s, Type& out) {                               \
        static const char* names[] = {__VA_ARGS__};                                    \
        for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); ++i)              \
            if (s == names[i]) { out = static_cast<Type>(i); return true; }            \
        return false;                                                                  \
    }

NEXA_ENUM_IO(IncidentStatus, "PENDING", "ACTIVE", "CONTAINED", "RESOLVED", "ARCHIVED")
NEXA_ENUM_IO(ResourceStatus, "AVAILABLE", "DEPLOYED", "MAINTENANCE", "INACTIVE")
NEXA_ENUM_IO(RequestStatus, "QUEUED", "ESCALATED", "PROCESSING", "FULFILLED", "CANCELLED")
NEXA_ENUM_IO(DeploymentStatus, "ACTIVE", "COMPLETED", "CANCELLED")
NEXA_ENUM_IO(Role, "ADMIN", "INCIDENT_OPERATOR", "RESOURCE_COORDINATOR", "MEDICAL_COORDINATOR", "VIEWER")

#undef NEXA_ENUM_IO

inline const char* severityLabel(int s) {
    static const char* names[] = {"UNKNOWN", "LOW", "MODERATE", "HIGH", "SEVERE", "CRITICAL"};
    return (s >= 1 && s <= 5) ? names[s] : names[0];
}

} // namespace nexa::model
