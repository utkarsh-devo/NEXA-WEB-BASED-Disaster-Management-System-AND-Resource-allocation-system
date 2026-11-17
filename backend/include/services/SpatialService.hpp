#pragma once
// ============================================================================
// NEXA :: Module 7 — Spatial Intelligence & Resource Discovery
//  * KD-Tree   : nearest / k-nearest / radius (exact Haversine, see Geo.hpp)
//  * Quad-Tree : region (rectangle) queries + density grid for the map
//  * Range-Tree: guaranteed O(log^2 n + m) rectangle queries (incidents)
//  * DBSCAN    : incident clusters, neighbourhood queries via the KD-Tree
// Spatial proximity is ONE input to suitability; it never decides
// allocation alone (spec rule 5).
// ============================================================================
#include "ServiceContext.hpp"
#include "../data_structures/RangeTree.hpp"

namespace nexa::svc {

struct SpatialHit {
    std::string kind;        // resource | hospital | incident
    long id = 0;
    std::string name, category, status;
    double lat = 0, lon = 0;
    double distanceKm = 0;
    int available = 0, capacity = 0;
};

struct SpatialQueryStats {
    std::string method;
    std::size_t candidatesExamined = 0;
    double microseconds = 0;
    std::size_t results = 0;
};

struct SpatialFilter {
    std::string kind = "resource";      // resource | hospital | incident | all
    std::string category;               // resource category filter (optional)
    std::string capability;             // required capability (optional)
    bool availableOnly = true;
};

struct Cluster {
    int id = 0;
    double centerLat = 0, centerLon = 0;
    double radiusKm = 0;
    DynamicArray<long> incidentIds;
    int maxSeverity = 0;
};

class SpatialService {
public:
    explicit SpatialService(ServiceContext& c) : ctx_(c) {}

    // ---- index maintenance (called by the entity services) ----
    void indexResource(const Resource& r);
    void unindexResource(const Resource& r);
    void indexHospital(const Hospital& h);
    void unindexHospital(const Hospital& h);
    void indexIncident(const Incident& i);
    void unindexIncident(const Incident& i);
    void rebuildAll();

    // ---- queries ----
    DynamicArray<SpatialHit> nearest(double lat, double lon, std::size_t k, const SpatialFilter& f, SpatialQueryStats* stats = nullptr);
    DynamicArray<SpatialHit> nearestLinear(double lat, double lon, std::size_t k, const SpatialFilter& f, SpatialQueryStats* stats = nullptr);
    DynamicArray<SpatialHit> withinRadius(double lat, double lon, double km, const SpatialFilter& f, SpatialQueryStats* stats = nullptr);
    /// method: quadtree | rangetree | linear
    DynamicArray<SpatialHit> region(const ds::GeoRect& r, const std::string& kind, const std::string& method, SpatialQueryStats* stats = nullptr);
    DynamicArray<Cluster> clusters(double epsKm, int minPoints);
    DynamicArray<ds::QuadCell> densityCells();

    /// Nearest road-graph vertex (dense index) to a coordinate; -1 if no graph.
    int snapToVertex(double lat, double lon, double* snapKm = nullptr);

private:
    ServiceContext& ctx_;
    ds::RangeTree2D incidentRange_;
    bool rangeDirty_ = true;

    /// Quad-tree ids must be unique across entity kinds: tag * 10^8 + id (fits 32-bit long).
    static long gridId(int tag, long id) { return (long)tag * 100000000L + id; }
    bool accepts(const ds::GeoPoint& p, const SpatialFilter& f) const;
    SpatialHit toHit(const ds::GeoPoint& p, double km) const;
};

} // namespace nexa::svc
