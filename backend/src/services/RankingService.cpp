#include "services/RankingService.hpp"
#include "algorithms/Searching.hpp"
#include "algorithms/Sorting.hpp"
#include "utilities/Geo.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>

namespace nexa::svc {

namespace {
std::string fmt1(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); return b; }
}

RankingResult RankingService::rankForIncident(long incidentId, const RankOptions& opt) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto t0 = std::chrono::steady_clock::now();
    auto& s = ctx_.store;
    const Incident* incPtr = s.incidents.get(incidentId);
    if (!incPtr) throw NotFoundError("Incident " + std::to_string(incidentId) + " not found");
    const Incident inc = *incPtr;
    const SuitabilityWeights& w = ctx_.config.suitability;

    RankingResult res;
    res.incidentId = incidentId;
    res.requiredCapabilities = inc.requiredCapabilities;
    int remaining = inc.requiredUnits - inc.assignedUnits;
    res.unitsNeeded = opt.unitsNeeded > 0 ? opt.unitsNeeded : (remaining > 0 ? remaining : 1);
    auto required = util::splitList(inc.requiredCapabilities);

    // ---- 1. KD-tree pre-selection with hard filters -----------------------
    auto capMatchOf = [&](const Resource& r, std::string* matched, std::string* missing) {
        if (required.empty()) return 1.0;
        auto have = util::splitList(r.capabilities);
        int hit = 0;
        for (const auto& req : required) {
            bool ok = have.findIndex([&](const std::string& h) { return h == req; }) >= 0;
            if (ok) { ++hit; if (matched) *matched += (matched->empty() ? "" : ",") + req; }
            else if (missing) *missing += (missing->empty() ? "" : ",") + req;
        }
        return (double)hit / (double)required.size();
    };
    auto filter = [&](const ds::GeoPoint& p) {
        const Resource* r = s.resources.get(p.id);
        if (!r) return false;
        ++res.considered;
        if (r->status != ResourceStatus::Available || r->available <= 0) { ++res.rejectedUnavailable; return false; }
        if (!opt.category.empty() && r->category != opt.category) return false;
        if (capMatchOf(*r, nullptr, nullptr) <= 0) { ++res.rejectedCapability; return false; }
        return true;
    };
    auto pool = s.resourceKd.kNearest(inc.lat, inc.lon, (std::size_t)ctx_.config.candidatePool, filter);

    // ---- 2. road distances: ONE Dijkstra from the incident ------------------
    DynamicArray<double> roadKm, minutes;
    double incSnap = 0;
    int src = opt.useRoadNetwork ? network_.distancesFrom(inc.lat, inc.lon, roadKm, minutes, incSnap) : -1;

    // ---- 3. score ----------------------------------------------------------
    DynamicArray<RankedResource> ranked;
    for (const auto& hit : pool) {
        const Resource& r = *s.resources.get(hit.point.id);
        RankedResource x;
        x.resourceId = r.id; x.code = r.code; x.name = r.name; x.category = r.category; x.capabilities = r.capabilities;
        x.available = r.available; x.capacity = r.capacity; x.lat = r.lat; x.lon = r.lon;
        x.straightKm = hit.distanceKm;
        x.capabilityMatch = capMatchOf(r, &x.matchedCapabilities, &x.missingCapabilities);
        if (src >= 0) {
            double rSnap = 0;
            int v = spatial_.snapToVertex(r.lat, r.lon, &rSnap);
            if (v >= 0 && roadKm[v] != ds::Graph::INF) {
                x.roadKm = roadKm[v] + rSnap + incSnap;
                x.etaMinutes = minutes[v] + (rSnap + incSnap) / ctx_.config.averageSpeedKmh * 60.0;
                x.routable = true;
            } else { x.routable = false; }
        } else x.routable = false;
        if (!x.routable) {
            x.roadKm = x.straightKm * w.detourFactor;
            x.etaMinutes = x.roadKm / ctx_.config.averageSpeedKmh * 60.0;
        }
        x.proximity = 1.0 / (1.0 + x.roadKm / w.proximityScaleKm);
        x.availability = (double)r.available / (double)r.capacity;
        x.capacityFit = res.unitsNeeded > 0 ? ((double)r.available >= res.unitsNeeded ? 1.0 : (double)r.available / res.unitsNeeded) : 1.0;
        double sum = w.sum();
        x.score = 100.0 * (w.capability * x.capabilityMatch + w.proximity * x.proximity + w.availability * x.availability + w.capacity * x.capacityFit) / sum;
        x.score = std::round(x.score * 100.0) / 100.0;
        x.reason = "capability " + std::to_string((int)std::round(x.capabilityMatch * required.size())) + "/" + std::to_string(required.empty() ? 0 : (int)required.size()) +
                   (x.matchedCapabilities.empty() ? "" : " (" + x.matchedCapabilities + ")") +
                   "; " + fmt1(x.roadKm) + " km " + (x.routable ? "by road" : "est. (no road path)") +
                   " ~" + std::to_string((int)std::round(x.etaMinutes)) + " min; " +
                   std::to_string(r.available) + "/" + std::to_string(r.capacity) + " units free";
        ranked.pushBack(x);
    }

    // ---- 4. sort (descending score; ties -> shorter road distance) ----------
    algo::OpCounter c;
    auto better = [](const RankedResource& a, const RankedResource& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.roadKm < b.roadKm;
    };
    algo::sortWith(algo::parseSortAlgorithm(opt.algorithm), ranked, better, &c);
    res.algorithm = opt.algorithm == "quick" || opt.algorithm == "heap" || opt.algorithm == "insertion" ? opt.algorithm : "merge";
    res.comparisons = c.comparisons;

    // ---- 5. median road distance (Median Search / selection) ----------------
    if (!ranked.empty()) {
        DynamicArray<double> d;
        for (const auto& x : ranked) d.pushBack(x.roadKm);
        res.medianRoadKm = algo::medianSearch(d, [](double a, double b) { return a < b; });
    }
    for (std::size_t i = 0; i < ranked.size() && i < opt.limit; ++i) res.candidates.pushBack(ranked[i]);
    res.microseconds = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    return res;
}

SearchOutcome RankingService::search(const std::string& field, const std::string& value, const std::string& method) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto t0 = std::chrono::steady_clock::now();
    auto& s = ctx_.store;
    SearchOutcome out;
    algo::OpCounter c;
    std::string v = util::trim(value);
    if (v.empty()) throw ValidationError("Search value is required");

    if (field == "id") {
        long id = 0;
        try { id = std::stol(v); } catch (...) { throw ValidationError("Resource id must be a number"); }
        if (method == "hash") {
            out.method = "hash_table";
            const Resource* r = s.resources.get(id);    // Registry hash index
            c.comparisons = 1;
            if (r) out.results.pushBack(*r);
        } else if (method == "binary") {
            out.method = "binary_search";
            DynamicArray<Resource> sorted;
            s.resources.forEachOrdered([&](const Resource& r) { sorted.pushBack(r); });   // AVL in-order => sorted by id
            long idx = algo::binarySearch(sorted, id, [](const Resource& r) { return r.id; }, &c);
            if (idx >= 0) out.results.pushBack(sorted[idx]);
            out.note = "Array obtained from the AVL index in sorted order; binary search requires sorted input";
        } else {
            out.method = "linear_search";
            long idx = algo::linearSearch(s.resources.items(), [&](const Resource& r) { return r.id == id; }, &c);
            if (idx >= 0) out.results.pushBack(s.resources.items()[idx]);
        }
    } else if (field == "code") {
        if (method == "hash") {
            out.method = "hash_table";
            c.comparisons = 1;
            if (const long* id = s.resourceByCode.find(v)) out.results.pushBack(*s.resources.get(*id));
        } else if (method == "binary") {
            out.method = "binary_search";
            DynamicArray<Resource> sorted = s.resources.items();
            algo::mergeSort(sorted, [](const Resource& a, const Resource& b) { return a.code < b.code; });
            long idx = algo::binarySearch(sorted, v, [](const Resource& r) { return r.code; }, &c);
            if (idx >= 0) out.results.pushBack(sorted[idx]);
            out.note = "Sorted by code with merge sort first (O(n log n)), then O(log n) search";
        } else {
            out.method = "linear_search";
            long idx = algo::linearSearch(s.resources.items(), [&](const Resource& r) { return r.code == v; }, &c);
            if (idx >= 0) out.results.pushBack(s.resources.items()[idx]);
        }
    } else if (field == "category" || field == "capability") {
        out.method = "linear_search";
        std::string needle = util::lower(v);
        out.results = algo::linearSearchAll(s.resources.items(), [&](const Resource& r) {
            if (field == "category") return r.category == needle;
            for (const auto& x : util::splitList(r.capabilities)) if (x == needle) return true;
            return false;
        }, &c);
        out.note = "Multi-result attribute search scans every record (O(n))";
    } else throw ValidationError("field must be id, code, category or capability");

    out.found = !out.results.empty();
    out.comparisons = c.comparisons;
    out.microseconds = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    return out;
}

} // namespace nexa::svc
