#include "controllers/GeoControllers.hpp"
#include "controllers/ApiSupport.hpp"
#include "controllers/JsonMappers.hpp"

namespace nexa::api {

using svc::Permission;

namespace {
svc::SpatialFilter filterFrom(const HttpRequestPtr& req) {
    svc::SpatialFilter f;
    f.kind = qs(req, "kind", "resource"); f.category = qs(req, "category"); f.capability = qs(req, "capability");
    f.availableOnly = qb(req, "availableOnly", true);
    return f;
}
ds::WeightMode modeFrom(const HttpRequestPtr& req) { return ds::parseWeightMode(qs(req, "mode", "distance")); }
Json::Value withStats(const Json::Value& items, const svc::SpatialQueryStats& st) { Json::Value o(Json::objectValue); o["items"] = items; o["stats"] = toJson(st); return o; }
svc::HospitalInput hospitalInput(const Json::Value& b) {
    svc::HospitalInput in;
    in.name = sv(b, "name"); in.category = sv(b, "category", "general"); in.contact = sv(b, "contact"); in.address = sv(b, "address");
    in.lat = dv(b, "lat"); in.lon = dv(b, "lon");
    in.totalBeds = iv(b, "totalBeds"); in.availableBeds = iv(b, "availableBeds", -1); in.icuBeds = iv(b, "icuBeds"); in.icuAvailable = iv(b, "icuAvailable", -1);
    if (b.isMember("capabilities") && b["capabilities"].isArray()) {
        std::string s; for (const auto& x : b["capabilities"]) { if (!s.empty()) s += ","; s += x.asString(); }
        in.capabilities = s;
    } else in.capabilities = sv(b, "capabilities");
    return in;
}
}

// ------------------------------------------------------------------ spatial
void SpatialController::nearest(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        svc::SpatialQueryStats st;
        auto hits = core().spatial.nearest(qd(req, "lat"), qd(req, "lon"), (std::size_t)qi(req, "k", 8), filterFrom(req), &st);
        return withStats(arr(hits), st);
    });
}
void SpatialController::radius(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        double km = qd(req, "km", 5);
        if (km <= 0 || km > 100) throw ValidationError("km must be between 0 and 100");
        svc::SpatialQueryStats st;
        auto hits = core().spatial.withinRadius(qd(req, "lat"), qd(req, "lon"), km, filterFrom(req), &st);
        return withStats(arr(hits), st);
    });
}
void SpatialController::region(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        ds::GeoRect r{qd(req, "minLat"), qd(req, "minLon"), qd(req, "maxLat"), qd(req, "maxLon")};
        svc::SpatialQueryStats st;
        auto hits = core().spatial.region(r, qs(req, "kind", "all"), qs(req, "method", "quadtree"), &st);
        return withStats(arr(hits), st);
    });
}
void SpatialController::clusters(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().spatial.clusters(qd(req, "eps", 2.5), qi(req, "minPoints", 2))); });
}
void SpatialController::density(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().spatial.densityCells()); });
}
void SpatialController::compare(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        svc::SpatialQueryStats kd, lin;
        auto f = filterFrom(req);
        std::size_t k = (std::size_t)qi(req, "k", 5);
        auto a = core().spatial.nearest(qd(req, "lat"), qd(req, "lon"), k, f, &kd);
        auto b = core().spatial.nearestLinear(qd(req, "lat"), qd(req, "lon"), k, f, &lin);
        Json::Value o(Json::objectValue);
        o["kdTree"] = withStats(arr(a), kd); o["linear"] = withStats(arr(b), lin);
        o["sameNearest"] = !a.empty() && !b.empty() && a[0].id == b[0].id;
        return o;
    });
}
void SpatialController::layers(const HttpRequestPtr& req, Cb&& cb) {
    // everything the map needs in one call
    run(req, cb, Permission::View, [&](const svc::Session&) {
        auto& c = core();
        Json::Value o(Json::objectValue);
        svc::IncidentQuery q; q.status = qb(req, "includeClosed") ? "ALL" : ""; q.limit = 1000;
        o["incidents"] = arr(c.incidents.list(q));
        svc::ResourceQuery rq; rq.limit = 2000;
        o["resources"] = arr(c.resources.list(rq));
        o["hospitals"] = arr(c.hospitals.list("name"));
        return o;
    });
}

// ------------------------------------------------------------------ network
void NetworkController::graph(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        std::lock_guard<std::recursive_mutex> g(core().store.mtx);
        Json::Value o(Json::objectValue), locs(Json::arrayValue), roads(Json::arrayValue);
        core().store.locations.forEachOrdered([&](const model::Location& l) { locs.append(toJson(l)); });
        core().store.roadRecords.forEachOrdered([&](const model::Road& r) { roads.append(toJson(r)); });
        o["locations"] = locs; o["roads"] = roads;
        return o;
    });
}
void NetworkController::summary(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return toJson(core().network.summary()); });
}
void NetworkController::route(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        if (!qs(req, "from").empty() && !qs(req, "to").empty())
            return toJson(core().network.routeBetweenLocations(ql(req, "from"), ql(req, "to"), modeFrom(req)));
        return toJson(core().network.routeBetweenPoints(qd(req, "fromLat"), qd(req, "fromLon"), qd(req, "toLat"), qd(req, "toLon"), modeFrom(req)));
    });
}
void NetworkController::compare(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        return arr(core().network.compareRoutes(qd(req, "fromLat"), qd(req, "fromLon"), qd(req, "toLat"), qd(req, "toLon")));
    });
}
void NetworkController::explore(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return toJson(core().network.explore(ql(req, "source"), qs(req, "algorithm", "bfs"))); });
}
void NetworkController::mst(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return toJson(core().network.minimumSpanningTree(qs(req, "algorithm", "kruskal"), modeFrom(req))); });
}
void NetworkController::components(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        ds::DynamicArray<long> ids; ds::DynamicArray<int> comp;
        int n = core().network.components(ids, comp);
        Json::Value o(Json::objectValue), m(Json::arrayValue);
        for (std::size_t i = 0; i < ids.size(); ++i) { Json::Value x(Json::objectValue); x["locationId"] = J(ids[i]); x["component"] = comp[i]; m.append(x); }
        o["count"] = n; o["membership"] = m;
        return o;
    });
}
void NetworkController::blockRoad(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageNetwork, [&](const svc::Session& s) {
        core().network.setRoadBlocked(id, bv(body(req), "blocked", true), actorOf(s));
        return toJson(core().network.summary());
    });
}

// ------------------------------------------------------------------ ranking & allocation
void AllocationController::rank(const HttpRequestPtr& req, Cb&& cb, long incidentId) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        svc::RankOptions opt;
        opt.category = qs(req, "category"); opt.algorithm = qs(req, "algorithm", "merge"); opt.limit = (std::size_t)qi(req, "limit", 10);
        opt.useRoadNetwork = qb(req, "road", true); opt.unitsNeeded = qi(req, "units", 0);
        Json::Value o = toJson(core().ranking.rankForIncident(incidentId, opt));
        o["weights"] = toJson(core().config.suitability);
        o["formula"] = "Q = 100·(wC·C + wP·P + wAv·Av + wK·K) / Σw,  P = 1/(1 + d/scale)";
        return o;
    });
}
void AllocationController::list(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().allocation.list(qs(req, "status"))); });
}
void AllocationController::allocate(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::Allocate, [&](const svc::Session& s) {
        Json::Value b = body(req);
        svc::AllocationRequest ar;
        ar.incidentId = lv(b, "incidentId"); ar.requestId = lv(b, "requestId"); ar.autoUnits = iv(b, "units");
        ar.category = sv(b, "category"); ar.notes = sv(b, "notes");
        if (b.isMember("items")) {
            if (!b["items"].isArray()) throw ValidationError("items must be an array");
            for (const auto& x : b["items"]) ar.items.pushBack(svc::AllocationItem{lv(x, "resourceId"), iv(x, "units", 1)});
        }
        return toJson(core().allocation.allocate(ar, actorOf(s)));
    }, 201);
}
void AllocationController::get(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return toJson(core().allocation.get(id)); });
}
void AllocationController::complete(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::Allocate, [&](const svc::Session& s) { return toJson(core().allocation.complete(id, actorOf(s))); });
}
void AllocationController::cancel(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::Allocate, [&](const svc::Session& s) { return toJson(core().allocation.cancel(id, actorOf(s), sv(body(req), "reason"))); });
}
void AllocationController::reassign(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::Allocate, [&](const svc::Session& s) {
        Json::Value b = body(req);
        return toJson(core().allocation.reassign(lv(b, "resourceId"), lv(b, "fromDeploymentId"), lv(b, "toIncidentId"), actorOf(s)));
    });
}
void AllocationController::matrix(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return toJson(core().allocation.matrix()); });
}

// ------------------------------------------------------------------ hospitals
void HospitalController::list(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().hospitals.list(qs(req, "sortBy", "name"))); });
}
void HospitalController::create(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::ManageHospitals, [&](const svc::Session& s) { return toJson(core().hospitals.create(hospitalInput(body(req)), actorOf(s))); }, 201);
}
void HospitalController::get(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        Json::Value o = toJson(core().hospitals.get(id));
        o["reservations"] = arr(core().hospitals.reservations(id));
        return o;
    });
}
void HospitalController::update(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageHospitals, [&](const svc::Session& s) { return toJson(core().hospitals.update(id, hospitalInput(body(req)), actorOf(s))); });
}
void HospitalController::remove(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageHospitals, [&](const svc::Session& s) { core().hospitals.remove(id, actorOf(s)); Json::Value o(Json::objectValue); o["ok"] = true; return o; });
}
void HospitalController::reserve(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageHospitals, [&](const svc::Session& s) {
        Json::Value b = body(req);
        return toJson(core().hospitals.reserve(id, lv(b, "incidentId"), iv(b, "beds", 1), dv(b, "hours", 12), actorOf(s)));
    }, 201);
}
void HospitalController::load(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        long now = util::nowEpoch();
        return toJson(core().hospitals.load(id, ql(req, "from", now), ql(req, "until", now + 24 * 3600)));
    });
}
void HospitalController::suitable(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        std::string cap = qs(req, "capability");
        int beds = qi(req, "beds", 1);
        std::size_t limit = (std::size_t)qi(req, "limit", 6);
        if (ql(req, "incidentId")) return arr(core().hospitals.suitableForIncident(ql(req, "incidentId"), cap, beds, limit));
        return arr(core().hospitals.suitable(qd(req, "lat"), qd(req, "lon"), cap, beds, limit));
    });
}
void HospitalController::reservations(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().hospitals.reservations(ql(req, "hospitalId"))); });
}
void HospitalController::release(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageHospitals, [&](const svc::Session& s) { return toJson(core().hospitals.release(id, actorOf(s))); });
}

} // namespace nexa::api
