#pragma once
// ============================================================================
// NEXA :: domain models
// Plain value types shared by repositories, services and controllers.
// Timestamps are UNIX epoch seconds (UTC). Capability lists are stored as
// comma-separated lower-case tokens, e.g. "medical,trauma,transport".
// ============================================================================
#include <string>
#include "Enums.hpp"
#include "../data_structures/DynamicArray.hpp"

namespace nexa::model {

using ds::DynamicArray;

struct IncidentCategory {
    int id = 0;
    int parentId = 0;              // 0 = child of root
    std::string name;
    std::string code;              // e.g. "FLOOD"
    double timeSensitivity = 0.5;  // 0..1, used by the priority formula
    std::string defaultCapabilities;
};

struct Incident {
    long id = 0;
    std::string code;              // INC-2026-0001
    std::string title;
    std::string description;
    int categoryId = 0;
    std::string categoryName;
    std::string categoryPath;      // "Natural > Flood" (from KaryTree)
    int severity = 1;              // 1..5
    double lat = 0, lon = 0;
    std::string locationName;
    IncidentStatus status = IncidentStatus::Pending;
    long affectedPeople = 0;
    std::string requiredCapabilities;
    int requiredUnits = 1;
    int assignedUnits = 0;         // derived from active deployments
    std::string reportedBy;
    std::string reporterContact;
    long createdAt = 0, updatedAt = 0, resolvedAt = 0;
    double priorityScore = 0;      // derived (PriorityEngine)
    bool simulated = false;
    long scenarioId = 0;
};

struct IncidentEvent {
    long id = 0;
    long incidentId = 0;
    std::string type;              // REGISTERED | STATUS_CHANGED | PRIORITY | DEPLOYED | RELEASED | NOTE | REQUEST
    std::string message;
    std::string actor;
    long createdAt = 0;
};

struct ResourceType {
    int id = 0;
    std::string name;
    std::string category;          // ambulance | fire_engine | rescue_vehicle | medical_supplies | rescue_equipment
                                   // food_water | shelter | personnel | special_team
    std::string defaultCapabilities;
};

struct Resource {
    long id = 0;
    std::string code;              // AMB-101
    std::string name;
    int typeId = 0;
    std::string typeName;
    std::string category;
    int capacity = 1;              // units of this resource (vehicles, crates, people, beds)
    int available = 1;             // units currently free
    ResourceStatus status = ResourceStatus::Available;
    double lat = 0, lon = 0;
    std::string capabilities;
    std::string organization;
    long updatedAt = 0;
};

struct ResourceHistoryEntry {
    long resourceId = 0;
    std::string action;            // REGISTERED | UPDATED | DEPLOYED | RELEASED | STATUS
    std::string detail;
    long at = 0;
};

struct Hospital {
    long id = 0;
    std::string name;
    std::string category;          // general | trauma | burn | pediatric | multispecialty
    double lat = 0, lon = 0;
    int totalBeds = 0, availableBeds = 0;
    int icuBeds = 0, icuAvailable = 0;
    std::string capabilities;      // trauma,burn,icu,pediatric,cardiac
    std::string contact;
    std::string address;
    long updatedAt = 0;
};

struct BedReservation {
    long id = 0;
    long hospitalId = 0;
    long incidentId = 0;
    int beds = 0;
    long from = 0, until = 0;
    bool released = false;
};

struct EmergencyRequest {
    long id = 0;
    long incidentId = 0;
    std::string assistanceType;    // medical | rescue | fire | evacuation | supplies | shelter
    std::string description;
    int urgency = 3;               // 1..5 as reported
    long peopleAffected = 0;
    RequestStatus status = RequestStatus::Queued;
    double urgencyScore = 0;       // derived
    std::string requestedBy;
    long createdAt = 0, processedAt = 0;
    long deploymentId = 0;
};

struct DeploymentResource {
    long resourceId = 0;
    std::string resourceCode;
    int units = 1;
    double suitability = 0;
    double routeKm = 0;
    double etaMinutes = 0;
    std::string reason;            // WHY this resource was chosen (spec Module 9)
    bool released = false;         // set when this resource is released / reassigned early
    long releasedAt = 0;
};

struct Deployment {
    long id = 0;
    long incidentId = 0;
    long requestId = 0;
    DeploymentStatus status = DeploymentStatus::Active;
    long createdAt = 0, completedAt = 0;
    std::string createdBy;
    std::string notes;
    DynamicArray<DeploymentResource> resources;
};

struct Location {
    long id = 0;
    std::string name;
    std::string kind;              // station | hospital | junction | shelter | depot
    double lat = 0, lon = 0;
};

struct Road {
    long id = 0;
    long fromId = 0, toId = 0;
    std::string name;
    double distanceKm = 0;
    double travelMinutes = 0;      // estimate from road class speed — NOT live traffic
    double penalty = 0;
    bool blocked = false;
};

struct User {
    long id = 0;
    std::string username;
    std::string displayName;
    std::string passwordHash;      // hex
    std::string salt;              // hex
    Role role = Role::Viewer;
    bool active = true;
    long createdAt = 0, lastLoginAt = 0;
};

struct SimulationScenario {
    long id = 0;
    std::string name;
    std::string type;              // earthquake | flood | industrial_fire | cyclone | multi
    std::string description;
    double centerLat = 0, centerLon = 0;
    int incidentCount = 0;
    unsigned seed = 0;
    long createdAt = 0;
};

struct SimulationEvent {
    long id = 0;
    long scenarioId = 0;
    int seq = 0;
    std::string eventType;         // INCIDENT | PRIORITY | SELECTION | ROUTE | DEPLOYMENT | RESOLUTION
    std::string summary;
    long entityId = 0;
    double lat = 0, lon = 0;
    long createdAt = 0;
};

struct SystemLog {
    long id = 0;
    std::string level;             // INFO | WARN | ERROR | AUDIT
    std::string source;
    std::string message;
    std::string actor;
    long createdAt = 0;
};

} // namespace nexa::model
