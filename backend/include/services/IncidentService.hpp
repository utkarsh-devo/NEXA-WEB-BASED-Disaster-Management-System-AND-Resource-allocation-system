#pragma once
// ============================================================================
// NEXA :: Module 2 — Incident Management
// Registration -> validation -> MySQL -> in-memory indexes (Registry/AVL/
// Trie/SuffixArray/KD/Quad) -> classification (KaryTree) -> priority
// (heap) -> timeline event (LinkedList) -> live update.
// ============================================================================
#include "ServiceContext.hpp"
#include "PriorityEngine.hpp"
#include "SpatialService.hpp"

namespace nexa::svc {

struct IncidentInput {
    std::string title, description, locationName, requiredCapabilities, reportedBy, reporterContact;
    int categoryId = 0, severity = 0, requiredUnits = 1;
    double lat = 0, lon = 0;
    long affectedPeople = 0;
    bool simulated = false;
    long scenarioId = 0;
    long createdAt = 0;       // optional (simulation / import); 0 = now
};

struct IncidentQuery {
    std::string status;            // "" = open incidents, "ALL" = everything, or a status name
    int categoryId = 0;            // includes descendants in the category tree
    int minSeverity = 1, maxSeverity = 5;
    std::string prefix;            // Trie prefix match on title/code/location words
    std::string text;              // SuffixArray substring match on descriptions
    std::string sortBy = "priority";   // priority | created | severity | affected
    std::string algorithm = "merge";   // merge | quick | heap | insertion | counting (severity only)
    std::size_t limit = 200;
};

struct QueryStats { std::string index; std::string sortAlgorithm; std::size_t comparisons = 0; std::size_t scanned = 0; double microseconds = 0; };

struct IncidentDetail {
    Incident incident;
    DynamicArray<IncidentEvent> timeline;
    DynamicArray<DeploymentResource> assigned;   // active deployments
    DynamicArray<long> deploymentIds;
    DynamicArray<EmergencyRequest> requests;
    PriorityBreakdown priority;
};

struct CategoryNode { int id = 0, parentId = 0, depth = 0; std::string name, code; double timeSensitivity = 0; std::size_t openIncidents = 0; };

class IncidentService {
public:
    IncidentService(ServiceContext& c, PriorityEngine& p, SpatialService& s, AuditService& a) : ctx_(c), priority_(p), spatial_(s), audit_(a) {}

    Incident create(const IncidentInput& in, const std::string& actor);
    Incident update(long id, const IncidentInput& in, const std::string& actor);
    Incident changeStatus(long id, IncidentStatus st, const std::string& actor, const std::string& note = "");
    /// Hard-deletes a PENDING incident with no deployments; otherwise archives it.
    std::string removeOrArchive(long id, const std::string& actor);
    void addNote(long id, const std::string& text, const std::string& actor);
    bool undoLastAction(const std::string& actor, std::string& description);

    DynamicArray<Incident> list(const IncidentQuery& q, QueryStats* stats = nullptr);
    IncidentDetail detail(long id);
    Incident get(long id);
    DynamicArray<ds::Trie::Match> suggest(const std::string& prefix, std::size_t limit = 8);
    DynamicArray<ds::SuffixArray::Occurrence> textSearch(const std::string& pattern, std::size_t* comparisons = nullptr);
    DynamicArray<IncidentEvent> recentEvents(std::size_t limit);
    DynamicArray<CategoryNode> categories();

    /// Shared helper: persists an event, appends to the timeline list, publishes.
    IncidentEvent addEvent(long incidentId, const std::string& type, const std::string& message, const std::string& actor);
    /// Called by bootstrap & by update: (re)indexes words for one incident.
    void indexText(const Incident& i);
    void unindexText(const Incident& i);
    std::string categoryPath(int categoryId);
    bool isDescendant(int categoryId, int ancestorId);

private:
    ServiceContext& ctx_;
    PriorityEngine& priority_;
    SpatialService& spatial_;
    AuditService& audit_;

    void validate(const IncidentInput& in);
    void rebuildDescriptionIndexIfDirty();
};

} // namespace nexa::svc
