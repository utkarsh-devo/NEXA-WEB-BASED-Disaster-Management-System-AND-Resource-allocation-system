#include "controllers/SystemControllers.hpp"
#include "controllers/ApiSupport.hpp"
#include "controllers/JsonMappers.hpp"

namespace nexa::api {

using svc::Permission;

// ------------------------------------------------------------------ auth
void AuthController::login(const HttpRequestPtr& req, Cb&& cb) {
    runPublic(req, cb, [&] {
        Json::Value b = body(req);
        return toJson(core().auth.login(sv(b, "username"), sv(b, "password")));
    });
}
void AuthController::logout(const HttpRequestPtr& req, Cb&& cb) {
    runPublic(req, cb, [&] { core().auth.logout(bearerToken(req)); Json::Value o(Json::objectValue); o["ok"] = true; return o; });
}
void AuthController::me(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session& s) { Json::Value o = toJson(s); o.removeMember("token"); return o; });
}

// ------------------------------------------------------------------ users
void UserController::list(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::ManageUsers, [&](const svc::Session&) { return arr(core().auth.listUsers()); });
}
void UserController::create(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::ManageUsers, [&](const svc::Session& s) {
        Json::Value b = body(req);
        Role role;
        if (!parse(sv(b, "role", "VIEWER"), role)) throw ValidationError("Unknown role");
        return toJson(core().auth.createUser(sv(b, "username"), sv(b, "displayName"), sv(b, "password"), role, actorOf(s)));
    }, 201);
}
void UserController::update(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageUsers, [&](const svc::Session& s) {
        Json::Value b = body(req);
        Role role;
        if (!parse(sv(b, "role", "VIEWER"), role)) throw ValidationError("Unknown role");
        return toJson(core().auth.updateUser(id, sv(b, "displayName"), role, bv(b, "active", true), actorOf(s)));
    });
}
void UserController::resetPassword(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageUsers, [&](const svc::Session& s) {
        core().auth.resetPassword(id, sv(body(req), "password"), actorOf(s));
        Json::Value o(Json::objectValue); o["ok"] = true; return o;
    });
}

// ------------------------------------------------------------------ system
void SystemController::health(const HttpRequestPtr& req, Cb&& cb) {
    runPublic(req, cb, [&] {
        Json::Value o(Json::objectValue);
        svc::NexaCore* c = corePtr();
        o["status"] = c ? "ok" : "starting";
        o["service"] = "NEXA engine";
        if (c) {
            o["storage"] = c->config.storage;
            o["incidents"] = J(c->store.incidents.size());
            o["resources"] = J(c->store.resources.size());
            o["graphVertices"] = c->store.roads.vertexCount();
        }
        o["dataNotice"] = "Demo dataset is SIMULATED — not real emergency information";
        return o;
    });
}
void SystemController::dashboard(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return toJson(core().analytics.dashboard()); });
}
void SystemController::activity(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().incidents.recentEvents((std::size_t)qi(req, "limit", 30))); });
}
void SystemController::logs(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::ManageSettings, [&](const svc::Session&) { return arr(core().audit.recent(qi(req, "limit", 100))); });
}
void SystemController::reload(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::ManageSettings, [&](const svc::Session&) { return toJson(core().bootstrap()); });
}
void SystemController::getWeights(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        Json::Value o(Json::objectValue);
        o["priority"] = toJson(core().config.priority);
        o["suitability"] = toJson(core().config.suitability);
        o["escalationThreshold"] = core().config.escalationThreshold;
        o["candidatePool"] = core().config.candidatePool;
        o["averageSpeedKmh"] = core().config.averageSpeedKmh;
        return o;
    });
}
void SystemController::putWeights(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::ManageSettings, [&](const svc::Session& s) {
        Json::Value b = body(req);
        auto& cfg = core().config;
        if (b.isMember("suitability")) {
            const Json::Value& x = b["suitability"];
            svc::SuitabilityWeights w = cfg.suitability;
            w.capability = dv(x, "capability", w.capability); w.proximity = dv(x, "proximity", w.proximity);
            w.availability = dv(x, "availability", w.availability); w.capacity = dv(x, "capacity", w.capacity);
            w.proximityScaleKm = dv(x, "proximityScaleKm", w.proximityScaleKm); w.detourFactor = dv(x, "detourFactor", w.detourFactor);
            w.validate();
            cfg.suitability = w;
        }
        if (b.isMember("escalationThreshold")) {
            double t = dv(b, "escalationThreshold");
            if (t < 0 || t > 100) throw ValidationError("escalationThreshold must be 0..100");
            cfg.escalationThreshold = t;
        }
        if (b.isMember("priority")) {
            const Json::Value& x = b["priority"];
            svc::PriorityWeights w = cfg.priority;
            w.severity = dv(x, "severity", w.severity); w.affected = dv(x, "affected", w.affected); w.unmet = dv(x, "unmet", w.unmet);
            w.age = dv(x, "age", w.age); w.status = dv(x, "status", w.status); w.timeSensitivity = dv(x, "timeSensitivity", w.timeSensitivity);
            w.ageSaturationHours = dv(x, "ageSaturationHours", w.ageSaturationHours); w.affectedSaturation = dv(x, "affectedSaturation", w.affectedSaturation);
            core().priority.setWeights(w);     // validates and re-prioritises every open incident
        }
        core().audit.record("AUDIT", "settings", "scoring weights updated", actorOf(s));
        Json::Value o(Json::objectValue);
        o["priority"] = toJson(cfg.priority); o["suitability"] = toJson(cfg.suitability); o["escalationThreshold"] = cfg.escalationThreshold;
        return o;
    });
}
void SystemController::undo(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::ManageIncidents, [&](const svc::Session& s) {
        std::string msg;
        bool ok = core().incidents.undoLastAction(actorOf(s), msg);
        Json::Value o(Json::objectValue); o["undone"] = ok; o["message"] = msg; return o;
    });
}

} // namespace nexa::api
