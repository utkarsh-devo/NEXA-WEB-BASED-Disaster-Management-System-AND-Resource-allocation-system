#include "controllers/InsightControllers.hpp"
#include "controllers/ApiSupport.hpp"
#include "controllers/JsonMappers.hpp"
#include "utilities/Logger.hpp"

namespace nexa::api {

using svc::Permission;

// ------------------------------------------------------------------ analytics
void AnalyticsController::timeline(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        return toJson(core().analytics.timeline(qi(req, "days", 2), qi(req, "bucketHours", 2), qi(req, "from", -1), qi(req, "to", -1)));
    });
}
void AnalyticsController::response(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return toJson(core().analytics.responseStats()); });
}
void AnalyticsController::categories(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().analytics.categoryDistribution(qb(req, "includeClosed", true))); });
}
void AnalyticsController::hospitals(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().analytics.hospitalUtilisation()); });
}
void AnalyticsController::statuses(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().analytics.statusDistribution()); });
}
void AnalyticsController::exportCsv(const HttpRequestPtr& req, Cb&& cb, const std::string& entity) {
    runRaw(req, cb, Permission::View, [&](const svc::Session& s) {
        std::string csv = core().analytics.exportCsv(entity);
        auto r = drogon::HttpResponse::newHttpResponse();
        r->setBody(csv);
        r->setContentTypeCodeAndCustomString(drogon::CT_CUSTOM, "text/csv; charset=utf-8");
        r->addHeader("Content-Disposition", "attachment; filename=\"nexa-" + entity + ".csv\"");
        util::Logger::instance().audit("export", entity + " exported", s.username);
        return r;
    });
}
void AnalyticsController::importCsv(const HttpRequestPtr& req, Cb&& cb, const std::string& entity) {
    Permission p = entity == "incidents" ? Permission::ManageIncidents : Permission::ManageResources;
    run(req, cb, p, [&](const svc::Session& s) {
        std::string text(req->getBody());
        if (auto j = req->getJsonObject()) text = sv(*j, "csv");      // also accept {"csv": "..."}
        return toJson(core().analytics.importCsv(entity, text, actorOf(s)));
    });
}

// ------------------------------------------------------------------ simulation
void SimulationController::list(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().simulation.list()); });
}
void SimulationController::create(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::RunSimulation, [&](const svc::Session& s) {
        Json::Value b = body(req);
        svc::ScenarioInput in;
        in.name = sv(b, "name"); in.type = sv(b, "type", "earthquake");
        in.centerLat = dv(b, "centerLat", in.centerLat); in.centerLon = dv(b, "centerLon", in.centerLon);
        in.radiusKm = dv(b, "radiusKm", 6); in.incidentCount = iv(b, "incidentCount", 6); in.seed = (unsigned)lv(b, "seed", 0);
        return toJson(core().simulation.create(in, actorOf(s)));
    }, 201);
}
void SimulationController::replay(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        Json::Value o(Json::objectValue);
        o["scenario"] = toJson(core().simulation.get(id));
        o["events"] = arr(core().simulation.replay(id));
        return o;
    });
}
void SimulationController::step(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::RunSimulation, [&](const svc::Session& s) { return toJson(core().simulation.step(id, actorOf(s))); });
}
void SimulationController::resolve(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::RunSimulation, [&](const svc::Session& s) { return toJson(core().simulation.resolveNext(id, actorOf(s))); });
}

// ------------------------------------------------------------------ DSA lab
void LabController::catalog(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().lab.catalog()); });
}
void LabController::benchmark(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().lab.benchmark(qs(req, "suite", "sorting"), ql(req, "n", 2000), qi(req, "repeats", 5))); });
}
void LabController::runLab(const HttpRequestPtr& req, Cb&& cb, const std::string& kind) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        Json::Value b = body(req);
        auto& lab = core().lab;
        if (kind == "linear") {
            ds::DynamicArray<svc::LabOp> ops;
            if (b.isMember("ops") && b["ops"].isArray())
                for (const auto& x : b["ops"]) ops.pushBack(svc::LabOp{sv(x, "op"), lv(x, "value"), lv(x, "position", -1)});
            return toJson(lab.linear(sv(b, "structure", "stack"), ops));
        }
        if (kind == "sort") return toJson(lab.sort(sv(b, "algorithm", "merge"), longs(b, "values")));
        if (kind == "search") return toJson(lab.search(sv(b, "algorithm", "binary"), longs(b, "values"), lv(b, "target")));
        if (kind == "tree") return toJson(lab.tree(sv(b, "type", "avl"), longs(b, "values"), longs(b, "deletes"), lv(b, "search"), iv(b, "order", 3)));
        if (kind == "heap") return toJson(lab.heap(sv(b, "mode", "max"), longs(b, "values"), iv(b, "extract", 0)));
        if (kind == "hash") return toJson(lab.hash(sv(b, "strategy", "chaining"), longs(b, "keys"), iv(b, "capacity", 8), longs(b, "deletes")));
        if (kind == "graph") {
            ds::DynamicArray<svc::LabEdge> edges;
            if (b.isMember("edges") && b["edges"].isArray())
                for (const auto& x : b["edges"]) edges.pushBack(svc::LabEdge{lv(x, "u"), lv(x, "v"), dv(x, "w", 1)});
            return toJson(lab.graph(iv(b, "vertices", 6), edges, sv(b, "algorithm", "bfs"), lv(b, "source"), lv(b, "target", -1)));
        }
        if (kind == "spatial") return toJson(lab.spatial(sv(b, "structure", "kd"), iv(b, "points", 300), (unsigned)lv(b, "seed", 7), dv(b, "lat", 28.61), dv(b, "lon", 77.21), dv(b, "radiusKm", 5), iv(b, "k", 5)));
        if (kind == "text") return toJson(lab.text(sv(b, "structure", "trie"), sv(b, "corpus"), sv(b, "pattern")));
        if (kind == "interval") return toJson(lab.intervals(longs(b, "intervals"), lv(b, "from"), lv(b, "to")));
        if (kind == "segment") return toJson(lab.segment(longs(b, "values"), lv(b, "l"), lv(b, "r"), lv(b, "addL"), lv(b, "addR"), lv(b, "add")));
        if (kind == "backtracking") return toJson(lab.backtracking(sv(b, "problem", "nqueens"), iv(b, "n", 8), longs(b, "grid"), iv(b, "rows"), iv(b, "cols")));
        if (kind == "sparse") return toJson(lab.sparse(longs(b, "entries"), lv(b, "rows", 10), lv(b, "cols", 10)));
        if (kind == "multilist") return toJson(lab.multilist(longs(b, "links")));
        throw NotFoundError("Unknown lab module '" + kind + "'");
    });
}

// ------------------------------------------------------------------ live feed
LiveBroadcaster& LiveBroadcaster::instance() { static LiveBroadcaster b; return b; }

void LiveBroadcaster::publish(const svc::LiveEvent& e) {
    Json::Value o(Json::objectValue);
    o["type"] = e.type; o["entityId"] = J(e.entityId); o["title"] = e.title; o["detail"] = e.detail;
    o["lat"] = e.lat; o["lon"] = e.lon; o["severity"] = e.severity; o["at"] = util::isoTime(util::nowEpoch());
    Json::StreamWriterBuilder w; w["indentation"] = "";
    std::string text = Json::writeString(w, o);
    std::lock_guard<std::mutex> g(m_);
    conns_.forEach([&](const drogon::WebSocketConnectionPtr& c) { if (c && c->connected()) c->send(text); });
}
void LiveBroadcaster::add(const drogon::WebSocketConnectionPtr& c) { std::lock_guard<std::mutex> g(m_); conns_.insertBack(c); }
void LiveBroadcaster::remove(const drogon::WebSocketConnectionPtr& c) { std::lock_guard<std::mutex> g(m_); conns_.removeFirst([&](const drogon::WebSocketConnectionPtr& x) { return x == c; }); }
std::size_t LiveBroadcaster::connections() { std::lock_guard<std::mutex> g(m_); return conns_.size(); }

void LiveFeedController::handleNewConnection(const HttpRequestPtr& req, const drogon::WebSocketConnectionPtr& conn) {
    try {
        core().auth.authenticate(req->getParameter("token"));
        LiveBroadcaster::instance().add(conn);
        conn->send("{\"type\":\"hello\",\"detail\":\"NEXA live feed connected\"}");
    } catch (const std::exception&) {
        conn->send("{\"type\":\"error\",\"detail\":\"unauthorised\"}");
        conn->shutdown();
    }
}
void LiveFeedController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& message, const drogon::WebSocketMessageType& type) {
    if (type == drogon::WebSocketMessageType::Ping) return;
    if (message == "ping") conn->send("{\"type\":\"pong\"}");
}
void LiveFeedController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) { LiveBroadcaster::instance().remove(conn); }

} // namespace nexa::api
