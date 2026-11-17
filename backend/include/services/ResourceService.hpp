#pragma once
// ============================================================================
// NEXA :: Module 3 — Resource Management
// Registry (array + hash + AVL), code HashTable, Trie for name search,
// LinkedList history per resource, KD/Quad spatial indexes.
// ============================================================================
#include "ServiceContext.hpp"
#include "SpatialService.hpp"

namespace nexa::svc {

struct ResourceInput {
    std::string code, name, capabilities, organization, status = "AVAILABLE";
    int typeId = 0, capacity = 1, available = -1;   // -1 => same as capacity
    double lat = 0, lon = 0;
};

struct ResourceQuery {
    std::string category, status, capability, prefix;
    bool availableOnly = false;
    std::string sortBy = "code";        // code | capacity | available | distance | utilisation
    std::string algorithm = "merge";    // merge | quick | heap | radix (capacity/available) | bucket (distance)
    double nearLat = 0, nearLon = 0;    // for distance sort
    std::size_t limit = 500;
};

struct ResourceListStats { std::string index, sortAlgorithm; std::size_t comparisons = 0, scanned = 0; double microseconds = 0; };

struct ResourceDetail {
    Resource resource;
    DynamicArray<ResourceHistoryEntry> history;
    DynamicArray<long> activeDeployments;
    DynamicArray<ds::Interval> commitments;
};

class ResourceService {
public:
    ResourceService(ServiceContext& c, SpatialService& s, AuditService& a) : ctx_(c), spatial_(s), audit_(a) {}

    Resource create(const ResourceInput& in, const std::string& actor);
    Resource update(long id, const ResourceInput& in, const std::string& actor);
    Resource setStatus(long id, ResourceStatus st, const std::string& actor);
    /// Resources with deployment history are deactivated, never deleted.
    Resource deactivate(long id, const std::string& actor);

    DynamicArray<Resource> list(const ResourceQuery& q, ResourceListStats* stats = nullptr);
    ResourceDetail detail(long id);
    Resource get(long id);
    /// O(1) average lookup through the code HashTable.
    Resource getByCode(const std::string& code);
    DynamicArray<ResourceType> types();
    DynamicArray<ds::Trie::Match> suggest(const std::string& prefix);

    /// Used by allocation inside its transaction workflow (no persistence here).
    void recordHistory(long resourceId, const std::string& action, const std::string& detail, bool persist = true);
    void indexText(const Resource& r);
    void unindexText(const Resource& r);

private:
    ServiceContext& ctx_;
    SpatialService& spatial_;
    AuditService& audit_;
    void validate(const ResourceInput& in, long selfId);
};

} // namespace nexa::svc
