// ============================================================================
// NEXA — server entry point
//
//   nexa_server [--config config.json] [--memory] [--seed] [--port 8080]
//
//   --memory   run without MySQL (in-memory repositories, demo data, nothing persisted)
//   --seed     insert the simulated demo dataset if the database is empty
//              (also done automatically when "auto_seed": true in config.json)
// ============================================================================
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include "controllers/ApiSupport.hpp"
#include "controllers/InsightControllers.hpp"
#include "repositories/InMemoryRepositories.hpp"
#include "repositories/MySqlRepositories.hpp"
#include "services/NexaCore.hpp"
#include "utilities/Logger.hpp"

using namespace nexa;

namespace {

svc::EngineConfig readEngineConfig(const Json::Value& nexa) {
    svc::EngineConfig c;
    if (nexa.isNull()) return c;
    const Json::Value& p = nexa["priority_weights"];
    if (p.isObject()) {
        c.priority.severity = p.get("severity", c.priority.severity).asDouble();
        c.priority.affected = p.get("affected", c.priority.affected).asDouble();
        c.priority.unmet = p.get("unmet", c.priority.unmet).asDouble();
        c.priority.age = p.get("age", c.priority.age).asDouble();
        c.priority.status = p.get("status", c.priority.status).asDouble();
        c.priority.timeSensitivity = p.get("time_sensitivity", c.priority.timeSensitivity).asDouble();
        c.priority.ageSaturationHours = p.get("age_saturation_hours", c.priority.ageSaturationHours).asDouble();
        c.priority.affectedSaturation = p.get("affected_saturation", c.priority.affectedSaturation).asDouble();
    }
    const Json::Value& s = nexa["suitability_weights"];
    if (s.isObject()) {
        c.suitability.capability = s.get("capability", c.suitability.capability).asDouble();
        c.suitability.proximity = s.get("proximity", c.suitability.proximity).asDouble();
        c.suitability.availability = s.get("availability", c.suitability.availability).asDouble();
        c.suitability.capacity = s.get("capacity", c.suitability.capacity).asDouble();
        c.suitability.proximityScaleKm = s.get("proximity_scale_km", c.suitability.proximityScaleKm).asDouble();
        c.suitability.detourFactor = s.get("detour_factor", c.suitability.detourFactor).asDouble();
    }
    c.escalationThreshold = nexa.get("escalation_threshold", c.escalationThreshold).asDouble();
    c.candidatePool = nexa.get("candidate_pool", c.candidatePool).asInt();
    c.averageSpeedKmh = nexa.get("average_speed_kmh", c.averageSpeedKmh).asDouble();
    c.storage = nexa.get("storage", c.storage).asString();
    c.priority.validate();
    c.suitability.validate();
    return c;
}

std::string mysqlConnectionString(const Json::Value& m) {
    auto get = [&](const char* k, const char* def) { return m.get(k, def).asString(); };
    return "host=" + get("host", "127.0.0.1") + " port=" + std::to_string(m.get("port", 3306).asInt()) + " dbname=" + get("database", "nexa") +
           " user=" + get("user", "root") + " password=" + get("password", "") + " client_encoding=utf8mb4";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string configPath = "config.json";
    bool forceMemory = false, seed = false;
    int portOverride = 0;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--config") && i + 1 < argc) configPath = argv[++i];
        else if (!std::strcmp(argv[i], "--memory")) forceMemory = true;
        else if (!std::strcmp(argv[i], "--seed")) seed = true;
        else if (!std::strcmp(argv[i], "--port") && i + 1 < argc) portOverride = std::atoi(argv[++i]);
    }

    auto& app = drogon::app();
    app.loadConfigFile(configPath);
    const Json::Value& custom = app.getCustomConfig();
    const Json::Value& nexaCfg = custom["nexa"];

    util::Logger::instance().setFile(nexaCfg.get("log_file", "../logs/nexa.log").asString());
    util::Logger::instance().setEcho(true);

    svc::EngineConfig cfg;
    try { cfg = readEngineConfig(nexaCfg); }
    catch (const std::exception& e) { std::fprintf(stderr, "Invalid scoring configuration: %s\n", e.what()); return 2; }
    if (forceMemory) cfg.storage = "memory";

    // ---- repositories ----
    repo::RepositoryBundle bundle;
    drogon::orm::DbClientPtr db;
    if (cfg.storage == "memory") {
        bundle = repo::makeInMemoryBundle();
        seed = true;
        util::Logger::instance().warn("main", "IN-MEMORY mode: data is not persisted");
    } else {
        db = drogon::orm::DbClient::newMysqlClient(mysqlConnectionString(custom["mysql"]), (size_t)custom["mysql"].get("connections", 4).asInt());
        for (int i = 0; i < 100 && !db->hasAvailableConnections(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!db->hasAvailableConnections()) {
            std::fprintf(stderr, "Cannot connect to MySQL (check custom_config.mysql in %s, and that database/schema/schema.sql was applied).\n"
                                 "Tip: run with --memory to start NEXA without MySQL.\n", configPath.c_str());
            return 3;
        }
        bundle = repo::makeMySqlBundle(db);
        if (nexaCfg.get("auto_seed", true).asBool()) seed = true;
    }

    // ---- engine ----
    auto& sink = api::LiveBroadcaster::instance();
    auto core = std::make_unique<svc::NexaCore>(cfg, bundle, sink);
    try {
        if (seed && core->seedDemoData()) util::Logger::instance().info("main", "demo dataset inserted (SIMULATED data)");
        else core->bootstrap();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Start-up failed: %s\n", e.what());
        return 4;
    }
    api::setCore(core.get());

    // ---- CORS (frontend dev server on another origin) ----
    app.registerSyncAdvice([](const drogon::HttpRequestPtr& req) -> drogon::HttpResponsePtr {
        if (req->method() == drogon::Options) {
            auto r = drogon::HttpResponse::newHttpResponse();
            r->addHeader("Access-Control-Allow-Origin", "*");
            r->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
            r->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            r->addHeader("Access-Control-Max-Age", "86400");
            return r;
        }
        return nullptr;
    });
    app.registerPostHandlingAdvice([](const drogon::HttpRequestPtr&, const drogon::HttpResponsePtr& resp) {
        resp->addHeader("Access-Control-Allow-Origin", "*");
    });

    // aging: priorities rise with incident age — refresh every 60 s
    app.getLoop()->runEvery(60.0, [] { if (auto* c = api::corePtr()) c->priority.refreshAll(); });

    if (portOverride > 0) app.addListener("0.0.0.0", (uint16_t)portOverride);
    util::Logger::instance().info("main", std::string("NEXA engine ready (storage: ") + cfg.storage + ")");
    app.run();
    api::setCore(nullptr);
    return 0;
}
