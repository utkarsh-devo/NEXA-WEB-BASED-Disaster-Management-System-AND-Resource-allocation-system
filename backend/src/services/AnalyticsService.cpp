#include "services/AnalyticsService.hpp"
#include "algorithms/Searching.hpp"
#include "algorithms/Sorting.hpp"
#include "data_structures/SegmentTree.hpp"
#include "utilities/Csv.hpp"
#include <cmath>

namespace nexa::svc {

namespace {
std::string num(double v) { char b[32]; std::snprintf(b, sizeof b, "%.6g", v); return b; }
}

DynamicArray<double> AnalyticsService::responseMinutes() {
    // response time = first deployment time - incident registration time
    auto& s = ctx_.store;
    ds::HashTable<long, long> first;
    for (const auto& d : s.deployments.items()) {
        long* f = first.find(d.incidentId);
        if (!f || d.createdAt < *f) first.insert(d.incidentId, d.createdAt);
    }
    DynamicArray<double> out;
    first.forEach([&](long incId, long t) {
        if (const Incident* i = s.incidents.get(incId)) { double m = (double)(t - i->createdAt) / 60.0; if (m >= 0) out.pushBack(m); }
    });
    return out;
}

DashboardSummary AnalyticsService::dashboard() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto& s = ctx_.store;
    priority_.refreshAll();                 // apply aging before reporting
    DashboardSummary d;
    d.generatedAt = util::nowEpoch();
    long sev[6] = {0};
    for (const auto& i : s.incidents.items()) {
        ++d.incidentsTotal;
        switch (i.status) {
            case IncidentStatus::Pending: ++d.pending; break;
            case IncidentStatus::Active: ++d.active; break;
            case IncidentStatus::Contained: ++d.contained; break;
            case IncidentStatus::Resolved: ++d.resolved; break;
            case IncidentStatus::Archived: ++d.archived; break;
        }
        if (PriorityEngine::isOpen(i.status)) {
            ++d.incidentsOpen;
            d.affectedPeopleOpen += i.affectedPeople;
            if (i.severity >= 1 && i.severity <= 5) ++sev[i.severity];
            if (i.severity == 5) ++d.critical;
        }
    }
    for (int k = 1; k <= 5; ++k) d.severity.pushBack(CountItem{model::severityLabel(k), sev[k], (double)k});

    ds::HashTable<std::string, long> capByCat, usedByCat;
    for (const auto& r : s.resources.items()) {
        ++d.resourcesTotal;
        if (r.status == ResourceStatus::Available) ++d.resourcesAvailable;
        else if (r.status == ResourceStatus::Deployed) ++d.resourcesDeployed;
        else if (r.status == ResourceStatus::Maintenance) ++d.resourcesMaintenance;
        else ++d.resourcesInactive;
        if (r.status == ResourceStatus::Inactive) continue;
        d.unitsTotal += r.capacity;
        d.unitsDeployed += r.capacity - r.available;
        long* c = capByCat.find(r.category); capByCat.insert(r.category, (c ? *c : 0) + r.capacity);
        long* u = usedByCat.find(r.category); usedByCat.insert(r.category, (u ? *u : 0) + (r.capacity - r.available));
    }
    // partially-deployed resources count as deployed for the "deployed" tile
    for (const auto& r : s.resources.items()) if (r.status == ResourceStatus::Available && r.available < r.capacity) ++d.resourcesDeployed;
    d.utilisationPct = d.unitsTotal ? std::round(1000.0 * d.unitsDeployed / d.unitsTotal) / 10.0 : 0;
    capByCat.forEach([&](const std::string& cat, long cap) {
        const long* used = usedByCat.find(cat);
        d.resourceUtilisation.pushBack(CountItem{cat, used ? *used : 0, cap ? std::round(1000.0 * (used ? *used : 0) / cap) / 10.0 : 0});
    });
    algo::quickSort(d.resourceUtilisation, [](const CountItem& a, const CountItem& b) { return a.value > b.value || (a.value == b.value && a.label < b.label); });

    for (const auto& r : s.requests.items()) {
        if (r.status == RequestStatus::Queued) ++d.requestsPending;
        if (r.status == RequestStatus::Escalated) { ++d.requestsPending; ++d.requestsEscalated; }
    }
    for (const auto& h : s.hospitals.items()) { d.bedsTotal += h.totalBeds; d.bedsAvailable += h.availableBeds; d.icuTotal += h.icuBeds; d.icuAvailable += h.icuAvailable; }
    for (const auto& dep : s.deployments.items()) if (dep.status == DeploymentStatus::Active) ++d.deploymentsActive;

    d.categories = categoryDistribution(false);
    d.topPriority = priority_.orderedQueue(6);
    DynamicArray<Deployment> deps = s.deployments.items();
    algo::mergeSort(deps, [](const Deployment& a, const Deployment& b) { return a.createdAt > b.createdAt || (a.createdAt == b.createdAt && a.id > b.id); });
    for (std::size_t i = 0; i < deps.size() && i < 6; ++i) d.recentDeployments.pushBack(deps[i]);
    d.recentActivity = incidents_.recentEvents(12);
    auto rm = responseMinutes();
    double sum = 0; for (double m : rm) sum += m;
    d.avgResponseMinutes = rm.empty() ? 0 : std::round(10.0 * sum / rm.size()) / 10.0;
    return d;
}

DynamicArray<CountItem> AnalyticsService::categoryDistribution(bool includeClosed) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto& s = ctx_.store;
    ds::HashTable<int, long> counts;
    for (const auto& i : s.incidents.items()) {
        if (!includeClosed && !PriorityEngine::isOpen(i.status)) continue;
        // roll up to the top-level category (child of the taxonomy root)
        auto* n = s.taxonomy.find(i.categoryId);
        while (n && n->parent && n->parent->parent) n = n->parent;
        int key = n ? n->id : i.categoryId;
        long* c = counts.find(key);
        counts.insert(key, c ? *c + 1 : 1);
    }
    DynamicArray<CountItem> out;
    counts.forEach([&](int id, long c) { auto* n = s.taxonomy.find(id); out.pushBack(CountItem{n ? n->label : "Other", c, (double)id}); });
    algo::quickSort(out, [](const CountItem& a, const CountItem& b) { return a.count > b.count || (a.count == b.count && a.label < b.label); });
    return out;
}

DynamicArray<CountItem> AnalyticsService::statusDistribution() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    long c[5] = {0};
    for (const auto& i : ctx_.store.incidents.items()) ++c[(int)i.status];
    DynamicArray<CountItem> out;
    for (int k = 0; k < 5; ++k) out.pushBack(CountItem{toString((IncidentStatus)k), c[k], 0});
    return out;
}

DynamicArray<CountItem> AnalyticsService::hospitalUtilisation() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    DynamicArray<CountItem> out;
    for (const auto& h : ctx_.store.hospitals.items())
        out.pushBack(CountItem{h.name, h.totalBeds - h.availableBeds, h.totalBeds ? std::round(1000.0 * (h.totalBeds - h.availableBeds) / h.totalBeds) / 10.0 : 0});
    algo::mergeSort(out, [](const CountItem& a, const CountItem& b) { return a.value > b.value; });
    return out;
}

TimelineView AnalyticsService::timeline(int days, int bucketHours, int qFrom, int qTo) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    if (days < 1 || days > 365) throw ValidationError("days must be 1..365");
    if (bucketHours < 1 || bucketHours > 168) throw ValidationError("bucketHours must be 1..168");
    TimelineView v;
    v.bucketSeconds = (long)bucketHours * 3600;
    long now = util::nowEpoch();
    long n = ((long)days * 24 + bucketHours - 1) / bucketHours;
    v.from = now - n * v.bucketSeconds;
    DynamicArray<double> inc, dep, res;
    for (long i = 0; i < n; ++i) { inc.pushBack(0); dep.pushBack(0); res.pushBack(0); }
    auto bucket = [&](long t) -> long { if (t < v.from || t > now) return -1; long b = (t - v.from) / v.bucketSeconds; return b >= n ? n - 1 : b; };
    for (const auto& i : ctx_.store.incidents.items()) {
        long b = bucket(i.createdAt); if (b >= 0) inc[b] += 1;
        if (i.resolvedAt) { long r = bucket(i.resolvedAt); if (r >= 0) res[r] += 1; }
    }
    for (const auto& d : ctx_.store.deployments.items()) { long b = bucket(d.createdAt); if (b >= 0) dep[b] += 1; }
    ds::SegmentTree incTree, depTree;
    incTree.build(inc); depTree.build(dep);
    for (long i = 0; i < n; ++i) v.buckets.pushBack(TimelineBucket{v.from + i * v.bucketSeconds, (long)inc[i], (long)dep[i], (long)res[i]});
    long lo = qFrom < 0 ? 0 : qFrom, hi = (qTo < 0 || qTo >= n) ? n - 1 : qTo;
    if (lo > hi) lo = hi;
    v.queryFromBucket = lo; v.queryToBucket = hi;
    v.rangeIncidents = incTree.rangeSum((std::size_t)lo, (std::size_t)hi);
    v.rangeDeployments = depTree.rangeSum((std::size_t)lo, (std::size_t)hi);
    v.peakIncidents = incTree.rangeMax((std::size_t)lo, (std::size_t)hi);
    return v;
}

ResponseStats AnalyticsService::responseStats() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    ResponseStats r;
    auto m = responseMinutes();
    r.samples = (long)m.size();
    auto less = [](double a, double b) { return a < b; };
    if (!m.empty()) {
        double sum = 0, mn = m[0], mx = m[0];
        for (double x : m) { sum += x; if (x < mn) mn = x; if (x > mx) mx = x; }
        r.averageMinutes = sum / m.size(); r.fastestMinutes = mn; r.slowestMinutes = mx;
        r.medianMinutes = algo::medianSearch(m, less);
        r.p90Minutes = algo::quickSelect(m, (long)((m.size() - 1) * 0.9), less);
    }
    double hours = 0;
    for (const auto& i : ctx_.store.incidents.items()) if (i.resolvedAt > 0) { ++r.resolvedCount; hours += (double)(i.resolvedAt - i.createdAt) / 3600.0; }
    r.averageResolutionHours = r.resolvedCount ? hours / r.resolvedCount : 0;
    return r;
}

std::string AnalyticsService::exportCsv(const std::string& entity) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    std::string out;
    using util::Row;
    if (entity == "incidents") {
        out += util::csvLine(Row{"id", "code", "title", "category", "severity", "status", "lat", "lon", "location", "affected_people", "required_capabilities", "required_units", "assigned_units", "priority", "created_at", "simulated"});
        ctx_.store.incidents.forEachOrdered([&](const Incident& i) {
            out += util::csvLine(Row{std::to_string(i.id), i.code, i.title, i.categoryName, std::to_string(i.severity), toString(i.status), num(i.lat), num(i.lon), i.locationName,
                                     std::to_string(i.affectedPeople), i.requiredCapabilities, std::to_string(i.requiredUnits), std::to_string(i.assignedUnits), num(i.priorityScore),
                                     util::isoTime(i.createdAt), i.simulated ? "yes" : "no"});
        });
    } else if (entity == "resources") {
        out += util::csvLine(Row{"id", "code", "name", "type_id", "category", "capacity", "available", "status", "lat", "lon", "capabilities", "organization"});
        ctx_.store.resources.forEachOrdered([&](const Resource& r) {
            out += util::csvLine(Row{std::to_string(r.id), r.code, r.name, std::to_string(r.typeId), r.category, std::to_string(r.capacity), std::to_string(r.available),
                                     toString(r.status), num(r.lat), num(r.lon), r.capabilities, r.organization});
        });
    } else if (entity == "deployments") {
        out += util::csvLine(Row{"deployment_id", "incident_id", "status", "created_at", "completed_at", "resource_code", "units", "suitability", "route_km", "eta_min", "reason"});
        ctx_.store.deployments.forEachOrdered([&](const Deployment& d) {
            for (const auto& r : d.resources)
                out += util::csvLine(Row{std::to_string(d.id), std::to_string(d.incidentId), toString(d.status), util::isoTime(d.createdAt), util::isoTime(d.completedAt),
                                         r.resourceCode, std::to_string(r.units), num(r.suitability), num(r.routeKm), num(r.etaMinutes), r.reason});
        });
    } else if (entity == "hospitals") {
        out += util::csvLine(Row{"id", "name", "category", "lat", "lon", "total_beds", "available_beds", "icu_beds", "icu_available", "capabilities"});
        ctx_.store.hospitals.forEachOrdered([&](const Hospital& h) {
            out += util::csvLine(Row{std::to_string(h.id), h.name, h.category, num(h.lat), num(h.lon), std::to_string(h.totalBeds), std::to_string(h.availableBeds),
                                     std::to_string(h.icuBeds), std::to_string(h.icuAvailable), h.capabilities});
        });
    } else throw ValidationError("entity must be incidents, resources, deployments or hospitals");
    return out;
}

ImportReport AnalyticsService::importCsv(const std::string& entity, const std::string& csv, const std::string& actor) {
    ImportReport rep;
    auto rows = util::parseCsv(csv);
    if (rows.size() < 2) throw ValidationError("CSV must contain a header row and at least one data row");
    const auto& header = rows[0];
    auto col = [&](const char* name) -> long { for (std::size_t i = 0; i < header.size(); ++i) if (util::lower(util::trim(header[i])) == name) return (long)i; return -1; };
    auto field = [&](const util::Row& r, const char* name) -> std::string { long c = col(name); return (c >= 0 && c < (long)r.size()) ? util::trim(r[c]) : ""; };
    auto toD = [](const std::string& s) { try { return std::stod(s); } catch (...) { return 0.0; } };
    auto toL = [](const std::string& s, long def) { try { return std::stol(s); } catch (...) { return def; } };
    for (std::size_t i = 1; i < rows.size(); ++i) {
        try {
            if (entity == "resources") {
                ResourceInput in;
                in.code = field(rows[i], "code"); in.name = field(rows[i], "name");
                in.typeId = (int)toL(field(rows[i], "type_id"), 0);
                in.capacity = (int)toL(field(rows[i], "capacity"), 1);
                in.available = (int)toL(field(rows[i], "available"), -1);
                in.lat = toD(field(rows[i], "lat")); in.lon = toD(field(rows[i], "lon"));
                in.capabilities = field(rows[i], "capabilities"); in.organization = field(rows[i], "organization");
                resources_.create(in, actor);
            } else if (entity == "incidents") {
                IncidentInput in;
                in.title = field(rows[i], "title"); in.description = field(rows[i], "description");
                in.categoryId = (int)toL(field(rows[i], "category_id"), 0);
                in.severity = (int)toL(field(rows[i], "severity"), 0);
                in.lat = toD(field(rows[i], "lat")); in.lon = toD(field(rows[i], "lon"));
                in.locationName = field(rows[i], "location");
                in.affectedPeople = toL(field(rows[i], "affected_people"), 0);
                in.requiredCapabilities = field(rows[i], "required_capabilities");
                in.requiredUnits = (int)toL(field(rows[i], "required_units"), 1);
                incidents_.create(in, actor);
            } else throw ValidationError("import supports resources or incidents");
            ++rep.imported;
        } catch (const NexaError& e) {
            ++rep.failed;
            rep.errors.pushBack("row " + std::to_string(i + 1) + ": " + e.what());
            if (e.code() == "VALIDATION_ERROR" && std::string(e.what()).find("import supports") != std::string::npos) throw;
        }
    }
    return rep;
}

} // namespace nexa::svc
