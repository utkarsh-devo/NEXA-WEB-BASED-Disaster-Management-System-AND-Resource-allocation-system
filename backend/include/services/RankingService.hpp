#pragma once
// ============================================================================
// NEXA :: Module 6 — Resource Search & Ranking Engine
// Pipeline for one incident:
//   KD-Tree pre-selection (nearest N that pass hard filters)
//   -> one Dijkstra from the incident (road km + minutes to every vertex)
//   -> suitability score per candidate (formula in EngineConfig.hpp)
//   -> sort with the selected algorithm (merge = stable default)
//   -> median road distance via Median Search (selection), for context.
// ============================================================================
#include "ServiceContext.hpp"
#include "SpatialService.hpp"
#include "NetworkService.hpp"

namespace nexa::svc {

struct RankedResource {
    long resourceId = 0;
    std::string code, name, category, capabilities;
    int available = 0, capacity = 0;
    double lat = 0, lon = 0;
    double straightKm = 0, roadKm = 0, etaMinutes = 0;
    bool routable = true;
    double capabilityMatch = 0, proximity = 0, availability = 0, capacityFit = 0;
    double score = 0;
    std::string matchedCapabilities, missingCapabilities;
    std::string reason;
};

struct RankOptions {
    std::string category;                 // optional resource category filter
    std::string algorithm = "merge";      // merge | quick | heap | insertion
    std::size_t limit = 10;
    bool useRoadNetwork = true;
    int unitsNeeded = 0;                  // 0 = incident's remaining requirement
};

struct RankingResult {
    long incidentId = 0;
    DynamicArray<RankedResource> candidates;
    std::size_t considered = 0, rejectedUnavailable = 0, rejectedCapability = 0;
    double medianRoadKm = 0;
    std::string algorithm;
    std::size_t comparisons = 0;
    double microseconds = 0;
    int unitsNeeded = 0;
    std::string requiredCapabilities;
};

struct SearchOutcome {
    std::string method;
    bool found = false;
    DynamicArray<Resource> results;
    std::size_t comparisons = 0;
    double microseconds = 0;
    std::string note;
};

class RankingService {
public:
    RankingService(ServiceContext& c, SpatialService& s, NetworkService& n) : ctx_(c), spatial_(s), network_(n) {}

    RankingResult rankForIncident(long incidentId, const RankOptions& opt);
    /// field: id | code | category | capability ; method: linear | binary | hash
    SearchOutcome search(const std::string& field, const std::string& value, const std::string& method);

private:
    ServiceContext& ctx_;
    SpatialService& spatial_;
    NetworkService& network_;
};

} // namespace nexa::svc
