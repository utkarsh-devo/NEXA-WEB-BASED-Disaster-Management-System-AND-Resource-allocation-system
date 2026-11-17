#include "services/NetworkService.hpp"
#include "utilities/Geo.hpp"
#include <chrono>

namespace nexa::svc {

namespace {
const char* modeName(ds::WeightMode m) { return m == ds::WeightMode::Time ? "time" : (m == ds::WeightMode::Cost ? "cost" : "distance"); }
double micros(std::chrono::steady_clock::time_point t0) { return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count(); }
}

void NetworkService::rebuildGraph() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto& s = ctx_.store;
    s.roads.clear();
    s.locations.forEachOrdered([&](const Location& l) { s.roads.addVertex(ds::VertexInfo{l.id, l.name, l.kind, l.lat, l.lon}); });
    s.roadRecords.forEachOrdered([&](const Road& r) {
        if (s.roads.indexOf(r.fromId) < 0 || s.roads.indexOf(r.toId) < 0) {
            util::Logger::instance().warn("network", "road " + std::to_string(r.id) + " references a missing location — skipped");
            return;
        }
        s.roads.addEdge(r.fromId, r.toId, ds::EdgeData{r.id, r.distanceKm, r.travelMinutes, r.penalty, r.blocked});
    });
    DynamicArray<ds::GeoPoint> vp;
    for (int v = 0; v < s.roads.vertexCount(); ++v) vp.pushBack({v, s.roads.vertex(v).lat, s.roads.vertex(v).lon, 0});
    s.vertexKd.build(vp);
}

RouteStop NetworkService::stopOf(int v) const {
    const auto& info = ctx_.store.roads.vertex(v);
    return RouteStop{info.id, info.name, info.kind, info.lat, info.lon};
}

GraphSummary NetworkService::summary() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto& s = ctx_.store;
    GraphSummary r;
    r.vertices = s.roads.vertexCount();
    r.arcs = s.roads.arcCount();
    r.roads = (int)s.roadRecords.size();
    for (const auto& x : s.roadRecords.items()) if (x.blocked) ++r.blockedRoads;
    DynamicArray<int> comp;
    r.components = algo::connectedComponents(s.roads, comp);
    r.listBytes = (std::size_t)r.vertices * sizeof(void*) + r.arcs * sizeof(ds::Graph::EdgeNode);
    r.matrixBytes = (std::size_t)r.vertices * (std::size_t)r.vertices * sizeof(double);
    return r;
}

RouteResult NetworkService::runRoute(int s, int t, ds::WeightMode mode, const char* name) {
    auto& gph = ctx_.store.roads;
    RouteResult r;
    r.weightMode = name;
    auto t0 = std::chrono::steady_clock::now();
    algo::OpCounter c;
    auto sp = algo::dijkstra(gph, s, mode, &c);
    r.microseconds = micros(t0);
    r.settled = sp.settled; r.relaxations = sp.relaxations;
    auto path = algo::reconstructPath(sp.parent, s, t);
    if (path.empty()) { r.found = false; r.note = "No path: destination is in a disconnected region or roads are blocked"; return r; }
    r.found = true;
    r.totalWeight = sp.dist[t];
    for (std::size_t i = 0; i < path.size(); ++i) {
        r.stops.pushBack(stopOf(path[i]));
        if (i + 1 < path.size()) {
            // pick the cheapest arc between consecutive vertices under this mode
            const ds::Graph::EdgeNode* best = nullptr;
            for (auto* e = gph.adjacency(path[i]); e; e = e->next)
                if (e->to == path[i + 1] && !e->data.blocked && (!best || e->data.weight(mode) < best->data.weight(mode))) best = e;
            if (best) { r.roadIds.pushBack(best->data.roadId); r.distanceKm += best->data.distanceKm; r.minutes += best->data.travelMinutes; }
        }
    }
    return r;
}

RouteResult NetworkService::routeBetweenLocations(long fromId, long toId, ds::WeightMode mode) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    int s = ctx_.store.roads.indexOf(fromId), t = ctx_.store.roads.indexOf(toId);
    if (s < 0 || t < 0) throw NotFoundError("Unknown source or destination location");
    return runRoute(s, t, mode, modeName(mode));
}

RouteResult NetworkService::routeBetweenPoints(double lat1, double lon1, double lat2, double lon2, ds::WeightMode mode) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    if (!geo::validLatLon(lat1, lon1) || !geo::validLatLon(lat2, lon2)) throw ValidationError("Invalid coordinates");
    if (ctx_.store.roads.vertexCount() == 0) throw ConflictError("Road network is empty — load map data first");
    double sk = 0, ek = 0;
    int s = spatial_.snapToVertex(lat1, lon1, &sk), t = spatial_.snapToVertex(lat2, lon2, &ek);
    RouteResult r = runRoute(s, t, mode, modeName(mode));
    r.snapStartKm = sk; r.snapEndKm = ek;
    if (r.found) {
        double lastMile = sk + ek;
        r.distanceKm += lastMile;
        r.minutes += lastMile / ctx_.config.averageSpeedKmh * 60.0;
        r.stops.insertAt(0, RouteStop{0, "Origin", "point", lat1, lon1});
        r.stops.pushBack(RouteStop{0, "Destination", "point", lat2, lon2});
        r.note = "Includes " + std::to_string((int)(lastMile * 1000)) + " m straight-line access to/from the road network";
    }
    return r;
}

DynamicArray<RouteResult> NetworkService::compareRoutes(double lat1, double lon1, double lat2, double lon2) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    DynamicArray<RouteResult> out;
    out.pushBack(routeBetweenPoints(lat1, lon1, lat2, lon2, ds::WeightMode::Distance));
    out.pushBack(routeBetweenPoints(lat1, lon1, lat2, lon2, ds::WeightMode::Time));
    out.pushBack(routeBetweenPoints(lat1, lon1, lat2, lon2, ds::WeightMode::Cost));
    // Alternative: for each road of the shortest path, block it and re-run; keep the best detour.
    const RouteResult& best = out[0];
    if (best.found) {
        RouteResult alt; bool have = false;
        for (long road : best.roadIds) {
            const Road* rr = ctx_.store.roadRecords.get(road);
            if (!rr || rr->blocked) continue;
            ctx_.store.roads.setBlocked(road, true);
            RouteResult cand = routeBetweenPoints(lat1, lon1, lat2, lon2, ds::WeightMode::Distance);
            ctx_.store.roads.setBlocked(road, false);
            if (cand.found && (!have || cand.totalWeight < alt.totalWeight)) { alt = cand; have = true; }
        }
        if (have) { alt.weightMode = "alternative"; alt.note = "Best route that avoids at least one road of the shortest path"; out.pushBack(alt); }
    }
    return out;
}

TraversalView NetworkService::explore(long sourceLocationId, const std::string& algorithm) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto& gph = ctx_.store.roads;
    int s = gph.indexOf(sourceLocationId);
    if (s < 0) throw NotFoundError("Unknown source location");
    algo::OpCounter c;
    auto tr = algorithm == "dfs" ? algo::dfs(gph, s, &c) : algo::bfs(gph, s, &c);
    TraversalView v;
    v.algorithm = algorithm == "dfs" ? "dfs" : "bfs";
    for (int x : tr.order) {
        v.order.pushBack(stopOf(x));
        v.depth.pushBack(tr.depth[x]);
        v.parentLocation.pushBack(tr.parent[x] >= 0 ? gph.vertex(tr.parent[x]).id : 0);
    }
    v.reachable = (int)tr.order.size();
    v.unreachable = gph.vertexCount() - v.reachable;
    v.operations = c.operations;
    return v;
}

MstView NetworkService::minimumSpanningTree(const std::string& algorithm, ds::WeightMode mode) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto& gph = ctx_.store.roads;
    algo::OpCounter c;
    auto t0 = std::chrono::steady_clock::now();
    auto res = algorithm == "prim" ? algo::prim(gph, mode, &c) : algo::kruskal(gph, mode, &c);
    MstView v;
    v.microseconds = micros(t0);
    v.algorithm = algorithm == "prim" ? "prim" : "kruskal";
    for (const auto& e : res.edges) v.edges.pushBack(MstView::Edge{stopOf(e.u), stopOf(e.v), e.w, e.roadId});
    v.totalWeight = res.totalWeight;
    v.components = res.components;
    v.operations = c.comparisons + c.operations;
    return v;
}

int NetworkService::components(DynamicArray<long>& locationIds, DynamicArray<int>& componentOf) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    int n = algo::connectedComponents(ctx_.store.roads, componentOf);
    locationIds.clear();
    for (int v = 0; v < ctx_.store.roads.vertexCount(); ++v) locationIds.pushBack(ctx_.store.roads.vertex(v).id);
    return n;
}

void NetworkService::setRoadBlocked(long roadId, bool blocked, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    Road* r = ctx_.store.roadRecords.get(roadId);
    if (!r) throw NotFoundError("Road " + std::to_string(roadId) + " not found");
    Road updated = *r;
    updated.blocked = blocked;
    ctx_.repos.locations->updateRoad(updated);      // persist first
    *r = updated;
    ctx_.store.roads.setBlocked(roadId, blocked);
    util::Logger::instance().audit("network", std::string(blocked ? "blocked" : "reopened") + " road " + std::to_string(roadId), actor);
    ctx_.sink.publish(LiveEvent{"network.updated", roadId, updated.name, blocked ? "Road blocked" : "Road reopened"});
}

int NetworkService::distancesFrom(double lat, double lon, DynamicArray<double>& roadKm, DynamicArray<double>& minutes, double& snapKm) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    roadKm.clear(); minutes.clear();
    if (ctx_.store.roads.vertexCount() == 0) return -1;
    int s = spatial_.snapToVertex(lat, lon, &snapKm);
    auto byKm = algo::dijkstra(ctx_.store.roads, s, ds::WeightMode::Distance);
    auto byMin = algo::dijkstra(ctx_.store.roads, s, ds::WeightMode::Time);
    roadKm = byKm.dist;
    minutes = byMin.dist;
    return s;
}

} // namespace nexa::svc
