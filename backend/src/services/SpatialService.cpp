#include "services/SpatialService.hpp"
#include "algorithms/Sorting.hpp"
#include "utilities/Geo.hpp"
#include <chrono>

namespace nexa::svc {

namespace {
double microsSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
}
bool hasCapability(const std::string& list, const std::string& cap) {
    if (cap.empty()) return true;
    for (const auto& c : util::splitList(list)) if (c == cap) return true;
    return false;
}
}

// ------------------------------------------------------------------ indexing
void SpatialService::indexResource(const Resource& r) {
    ds::GeoPoint p{r.id, r.lat, r.lon, TAG_RESOURCE};
    ctx_.store.resourceKd.insert(p);
    ds::GeoPoint g = p; g.id = gridId(TAG_RESOURCE, r.id);
    ctx_.store.spatialGrid.insert(g);
}
void SpatialService::unindexResource(const Resource& r) {
    ctx_.store.resourceKd.remove(r.id);
    ctx_.store.spatialGrid.remove(gridId(TAG_RESOURCE, r.id), r.lat, r.lon);
}
void SpatialService::indexHospital(const Hospital& h) {
    ctx_.store.hospitalKd.insert(ds::GeoPoint{h.id, h.lat, h.lon, TAG_HOSPITAL});
    ctx_.store.spatialGrid.insert(ds::GeoPoint{gridId(TAG_HOSPITAL, h.id), h.lat, h.lon, TAG_HOSPITAL});
}
void SpatialService::unindexHospital(const Hospital& h) {
    ctx_.store.hospitalKd.remove(h.id);
    ctx_.store.spatialGrid.remove(gridId(TAG_HOSPITAL, h.id), h.lat, h.lon);
}
void SpatialService::indexIncident(const Incident& i) {
    ctx_.store.incidentKd.insert(ds::GeoPoint{i.id, i.lat, i.lon, TAG_INCIDENT});
    ctx_.store.spatialGrid.insert(ds::GeoPoint{gridId(TAG_INCIDENT, i.id), i.lat, i.lon, TAG_INCIDENT});
    rangeDirty_ = true;
}
void SpatialService::unindexIncident(const Incident& i) {
    ctx_.store.incidentKd.remove(i.id);
    ctx_.store.spatialGrid.remove(gridId(TAG_INCIDENT, i.id), i.lat, i.lon);
    rangeDirty_ = true;
}

void SpatialService::rebuildAll() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto& s = ctx_.store;
    DynamicArray<ds::GeoPoint> rp, hp, ip, vp;
    s.spatialGrid.clear();
    for (const auto& r : s.resources.items()) { rp.pushBack({r.id, r.lat, r.lon, TAG_RESOURCE}); s.spatialGrid.insert({gridId(TAG_RESOURCE, r.id), r.lat, r.lon, TAG_RESOURCE}); }
    for (const auto& h : s.hospitals.items()) { hp.pushBack({h.id, h.lat, h.lon, TAG_HOSPITAL}); s.spatialGrid.insert({gridId(TAG_HOSPITAL, h.id), h.lat, h.lon, TAG_HOSPITAL}); }
    for (const auto& i : s.incidents.items()) { ip.pushBack({i.id, i.lat, i.lon, TAG_INCIDENT}); s.spatialGrid.insert({gridId(TAG_INCIDENT, i.id), i.lat, i.lon, TAG_INCIDENT}); }
    for (int v = 0; v < s.roads.vertexCount(); ++v) vp.pushBack({v, s.roads.vertex(v).lat, s.roads.vertex(v).lon, 0});
    s.resourceKd.build(rp); s.hospitalKd.build(hp); s.incidentKd.build(ip); s.vertexKd.build(vp);
    rangeDirty_ = true;
}

// ------------------------------------------------------------------ filters
bool SpatialService::accepts(const ds::GeoPoint& p, const SpatialFilter& f) const {
    auto& s = ctx_.store;
    if (p.tag == TAG_RESOURCE) {
        const Resource* r = s.resources.get(p.id);
        if (!r) return false;
        if (f.availableOnly && (r->status != ResourceStatus::Available || r->available <= 0)) return false;
        if (!f.category.empty() && r->category != f.category) return false;
        return hasCapability(r->capabilities, f.capability);
    }
    if (p.tag == TAG_HOSPITAL) {
        const Hospital* h = s.hospitals.get(p.id);
        if (!h) return false;
        if (f.availableOnly && h->availableBeds <= 0) return false;
        if (!f.category.empty() && h->category != f.category) return false;
        return hasCapability(h->capabilities, f.capability);
    }
    if (p.tag == TAG_INCIDENT) {
        const Incident* i = s.incidents.get(p.id);
        if (!i) return false;
        if (f.availableOnly && (i->status == IncidentStatus::Resolved || i->status == IncidentStatus::Archived)) return false;
        return true;
    }
    return false;
}

SpatialHit SpatialService::toHit(const ds::GeoPoint& p, double km) const {
    SpatialHit h; h.id = p.id; h.lat = p.lat; h.lon = p.lon; h.distanceKm = km;
    auto& s = ctx_.store;
    if (p.tag == TAG_RESOURCE) {
        h.kind = "resource";
        if (const Resource* r = s.resources.get(p.id)) { h.name = r->code + " " + r->name; h.category = r->category; h.status = toString(r->status); h.available = r->available; h.capacity = r->capacity; }
    } else if (p.tag == TAG_HOSPITAL) {
        h.kind = "hospital";
        if (const Hospital* x = s.hospitals.get(p.id)) { h.name = x->name; h.category = x->category; h.available = x->availableBeds; h.capacity = x->totalBeds; h.status = x->availableBeds > 0 ? "ACCEPTING" : "FULL"; }
    } else {
        h.kind = "incident";
        if (const Incident* i = s.incidents.get(p.id)) { h.name = i->code + " " + i->title; h.category = i->categoryName; h.status = toString(i->status); h.available = i->severity; }
    }
    return h;
}

// ------------------------------------------------------------------ queries
DynamicArray<SpatialHit> SpatialService::nearest(double lat, double lon, std::size_t k, const SpatialFilter& f, SpatialQueryStats* stats) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto t0 = std::chrono::steady_clock::now();
    ds::KDTree& tree = f.kind == "hospital" ? ctx_.store.hospitalKd : (f.kind == "incident" ? ctx_.store.incidentKd : ctx_.store.resourceKd);
    std::size_t visited = 0;
    auto filter = [&](const ds::GeoPoint& p) { return accepts(p, f); };
    auto hits = tree.kNearest(lat, lon, k, filter, &visited);
    DynamicArray<SpatialHit> out;
    for (const auto& h : hits) out.pushBack(toHit(h.point, h.distanceKm));
    if (stats) { stats->method = "kd_tree"; stats->candidatesExamined = visited; stats->microseconds = microsSince(t0); stats->results = out.size(); }
    return out;
}

DynamicArray<SpatialHit> SpatialService::nearestLinear(double lat, double lon, std::size_t k, const SpatialFilter& f, SpatialQueryStats* stats) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto t0 = std::chrono::steady_clock::now();
    DynamicArray<SpatialHit> all;
    std::size_t examined = 0;
    auto consider = [&](const ds::GeoPoint& p) { ++examined; if (accepts(p, f)) all.pushBack(toHit(p, geo::haversineKm(lat, lon, p.lat, p.lon))); };
    if (f.kind == "hospital") for (const auto& h : ctx_.store.hospitals.items()) consider({h.id, h.lat, h.lon, TAG_HOSPITAL});
    else if (f.kind == "incident") for (const auto& i : ctx_.store.incidents.items()) consider({i.id, i.lat, i.lon, TAG_INCIDENT});
    else for (const auto& r : ctx_.store.resources.items()) consider({r.id, r.lat, r.lon, TAG_RESOURCE});
    algo::mergeSort(all, [](const SpatialHit& a, const SpatialHit& b) { return a.distanceKm < b.distanceKm; });
    DynamicArray<SpatialHit> out;
    for (std::size_t i = 0; i < all.size() && i < k; ++i) out.pushBack(all[i]);
    if (stats) { stats->method = "linear_scan"; stats->candidatesExamined = examined; stats->microseconds = microsSince(t0); stats->results = out.size(); }
    return out;
}

DynamicArray<SpatialHit> SpatialService::withinRadius(double lat, double lon, double km, const SpatialFilter& f, SpatialQueryStats* stats) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto t0 = std::chrono::steady_clock::now();
    DynamicArray<SpatialHit> out;
    auto filter = [&](const ds::GeoPoint& p) { return accepts(p, f); };
    auto run = [&](ds::KDTree& t) { for (const auto& h : t.withinRadius(lat, lon, km, filter)) out.pushBack(toHit(h.point, h.distanceKm)); };
    if (f.kind == "hospital" || f.kind == "all") run(ctx_.store.hospitalKd);
    if (f.kind == "resource" || f.kind == "all") run(ctx_.store.resourceKd);
    if (f.kind == "incident" || f.kind == "all") run(ctx_.store.incidentKd);
    if (f.kind == "all") algo::mergeSort(out, [](const SpatialHit& a, const SpatialHit& b) { return a.distanceKm < b.distanceKm; });
    if (stats) { stats->method = "kd_tree_radius"; stats->microseconds = microsSince(t0); stats->results = out.size(); }
    return out;
}

DynamicArray<SpatialHit> SpatialService::region(const ds::GeoRect& r, const std::string& kind, const std::string& method, SpatialQueryStats* stats) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    if (r.minLat > r.maxLat || r.minLon > r.maxLon) throw ValidationError("region: min must be <= max");
    auto t0 = std::chrono::steady_clock::now();
    DynamicArray<SpatialHit> out;
    std::size_t examined = 0;
    int wantTag = kind == "hospital" ? TAG_HOSPITAL : (kind == "incident" ? TAG_INCIDENT : (kind == "resource" ? TAG_RESOURCE : 0));
    auto clat = (r.minLat + r.maxLat) / 2, clon = (r.minLon + r.maxLon) / 2;
    auto emit = [&](const ds::GeoPoint& p) {
        ds::GeoPoint q = p; q.id = p.id % 100000000L;
        out.pushBack(toHit(q, geo::haversineKm(clat, clon, p.lat, p.lon)));
    };
    std::string used = method;
    if (method == "rangetree" && wantTag == TAG_INCIDENT) {
        if (rangeDirty_) {
            DynamicArray<ds::GeoPoint> pts;
            for (const auto& i : ctx_.store.incidents.items()) pts.pushBack({i.id, i.lat, i.lon, TAG_INCIDENT});
            incidentRange_.build(pts);
            rangeDirty_ = false;
        }
        auto pts = incidentRange_.query(r, &examined);
        for (const auto& p : pts) emit(p);
    } else if (method == "linear") {
        auto scan = [&](int tag, double lat, double lon, long id) { ++examined; if (r.contains(lat, lon)) emit({id, lat, lon, tag}); };
        if (!wantTag || wantTag == TAG_RESOURCE) for (const auto& x : ctx_.store.resources.items()) scan(TAG_RESOURCE, x.lat, x.lon, x.id);
        if (!wantTag || wantTag == TAG_HOSPITAL) for (const auto& x : ctx_.store.hospitals.items()) scan(TAG_HOSPITAL, x.lat, x.lon, x.id);
        if (!wantTag || wantTag == TAG_INCIDENT) for (const auto& x : ctx_.store.incidents.items()) scan(TAG_INCIDENT, x.lat, x.lon, x.id);
    } else {
        used = "quadtree";
        auto pts = ctx_.store.spatialGrid.queryRect(r, [&](const ds::GeoPoint& p) { return !wantTag || p.tag == wantTag; });
        examined = pts.size();
        for (const auto& p : pts) emit(p);
    }
    if (stats) { stats->method = used; stats->candidatesExamined = examined; stats->microseconds = microsSince(t0); stats->results = out.size(); }
    return out;
}

DynamicArray<Cluster> SpatialService::clusters(double epsKm, int minPoints) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    if (epsKm <= 0 || minPoints < 1) throw ValidationError("clusters: eps must be > 0 and minPoints >= 1");
    auto& s = ctx_.store;
    DynamicArray<long> ids;
    for (const auto& i : s.incidents.items()) if (i.status != IncidentStatus::Resolved && i.status != IncidentStatus::Archived) ids.pushBack(i.id);
    ds::HashTable<long, int> label;           // -1 noise, >0 cluster id
    DynamicArray<Cluster> out;
    SpatialFilter openOnly; openOnly.kind = "incident"; openOnly.availableOnly = true;
    auto filter = [&](const ds::GeoPoint& p) { return accepts(p, openOnly); };
    int next = 0;
    for (long id : ids) {
        if (label.contains(id)) continue;
        const Incident* inc = s.incidents.get(id);
        auto nb = s.incidentKd.withinRadius(inc->lat, inc->lon, epsKm, filter);
        if ((int)nb.size() < minPoints) { label.insert(id, -1); continue; }
        Cluster c; c.id = ++next;
        ds::Queue<long> q;
        for (const auto& h : nb) q.enqueue(h.point.id);
        label.insert(id, c.id);
        c.incidentIds.pushBack(id);
        while (!q.isEmpty()) {
            long x = q.dequeue();
            int* l = label.find(x);
            if (l && *l > 0) continue;
            bool wasNoise = l && *l == -1;
            label.insert(x, c.id);
            c.incidentIds.pushBack(x);
            if (wasNoise) continue;                           // border point
            const Incident* xi = s.incidents.get(x);
            auto nb2 = s.incidentKd.withinRadius(xi->lat, xi->lon, epsKm, filter);
            if ((int)nb2.size() >= minPoints) for (const auto& h : nb2) { int* hl = label.find(h.point.id); if (!hl || *hl == -1) q.enqueue(h.point.id); }
        }
        // centroid + radius
        for (long m : c.incidentIds) { const Incident* mi = s.incidents.get(m); c.centerLat += mi->lat; c.centerLon += mi->lon; if (mi->severity > c.maxSeverity) c.maxSeverity = mi->severity; }
        c.centerLat /= (double)c.incidentIds.size(); c.centerLon /= (double)c.incidentIds.size();
        for (long m : c.incidentIds) { const Incident* mi = s.incidents.get(m); double d = geo::haversineKm(c.centerLat, c.centerLon, mi->lat, mi->lon); if (d > c.radiusKm) c.radiusKm = d; }
        out.pushBack(c);
    }
    return out;
}

DynamicArray<ds::QuadCell> SpatialService::densityCells() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    return ctx_.store.spatialGrid.leafCells();
}

int SpatialService::snapToVertex(double lat, double lon, double* snapKm) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto hit = ctx_.store.vertexKd.kNearest(lat, lon, 1);
    if (hit.empty()) return -1;
    if (snapKm) *snapKm = hit[0].distanceKm;
    return (int)hit[0].point.id;
}

} // namespace nexa::svc
