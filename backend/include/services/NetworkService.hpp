#pragma once
// ============================================================================
// NEXA :: Module 8 — Emergency Network & Route Analysis
// Road network = ds::Graph (adjacency list + on-demand adjacency matrix).
// Arbitrary coordinates (incident sites, resources) are snapped to the
// nearest graph vertex through the vertex KD-Tree; the snap distance is
// added as straight-line "last mile" and reported separately.
// ============================================================================
#include "ServiceContext.hpp"
#include "SpatialService.hpp"
#include "../algorithms/GraphAlgorithms.hpp"

namespace nexa::svc {

struct RouteStop { long locationId = 0; std::string name, kind; double lat = 0, lon = 0; };

struct RouteResult {
    bool found = false;
    std::string algorithm = "dijkstra";
    std::string weightMode = "distance";
    DynamicArray<RouteStop> stops;
    DynamicArray<long> roadIds;
    double totalWeight = 0;          // in units of the weight mode
    double distanceKm = 0;           // road km along the path
    double minutes = 0;              // estimated (seeded road speeds) — not live traffic
    double snapStartKm = 0, snapEndKm = 0;
    std::size_t settled = 0, relaxations = 0;
    double microseconds = 0;
    std::string note;
};

struct GraphSummary {
    int vertices = 0;
    std::size_t arcs = 0;
    int roads = 0, blockedRoads = 0, components = 0;
    std::size_t listBytes = 0, matrixBytes = 0;
};

struct TraversalView {
    std::string algorithm;
    DynamicArray<RouteStop> order;
    DynamicArray<int> depth;
    DynamicArray<long> parentLocation;
    int reachable = 0, unreachable = 0;
    std::size_t operations = 0;
};

struct MstView {
    std::string algorithm;
    struct Edge { RouteStop a, b; double weight = 0; long roadId = 0; };
    DynamicArray<Edge> edges;
    double totalWeight = 0;
    int components = 0;
    std::size_t operations = 0;
    double microseconds = 0;
};

class NetworkService {
public:
    NetworkService(ServiceContext& c, SpatialService& s) : ctx_(c), spatial_(s) {}

    /// Builds the Graph from the location/road registries.
    void rebuildGraph();

    GraphSummary summary();
    RouteResult routeBetweenLocations(long fromLocationId, long toLocationId, ds::WeightMode mode);
    RouteResult routeBetweenPoints(double lat1, double lon1, double lat2, double lon2, ds::WeightMode mode);
    /// distance / time / cost optimal routes + best alternative avoiding the shortest path's busiest edge
    DynamicArray<RouteResult> compareRoutes(double lat1, double lon1, double lat2, double lon2);
    TraversalView explore(long sourceLocationId, const std::string& algorithm);
    MstView minimumSpanningTree(const std::string& algorithm, ds::WeightMode mode);
    /// component id per location (dense order) and count
    int components(DynamicArray<long>& locationIds, DynamicArray<int>& componentOf);
    void setRoadBlocked(long roadId, bool blocked, const std::string& actor);

    /// One Dijkstra from `lat,lon` -> road km to every vertex (used by ranking).
    /// Returns snap vertex (-1 if no graph).
    int distancesFrom(double lat, double lon, DynamicArray<double>& roadKm, DynamicArray<double>& minutes, double& snapKm);

    RouteStop stopOf(int vertex) const;

private:
    ServiceContext& ctx_;
    SpatialService& spatial_;
    RouteResult runRoute(int s, int t, ds::WeightMode mode, const char* modeName);
};

} // namespace nexa::svc
