#pragma once
// ============================================================================
// NEXA :: analytics/reports, CSV import-export, simulation, DSA lab and the
// live WebSocket feed.
// ============================================================================
#include <drogon/HttpController.h>
#include <drogon/WebSocketController.h>
#include <mutex>
#include "services/ServiceContext.hpp"
#include "data_structures/LinkedList.hpp"

namespace nexa::api {

using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
using Cb = std::function<void(const HttpResponsePtr&)>;

class AnalyticsController : public drogon::HttpController<AnalyticsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AnalyticsController::timeline, "/api/analytics/timeline", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::response, "/api/analytics/response", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::categories, "/api/analytics/categories", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::hospitals, "/api/analytics/hospitals", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::statuses, "/api/analytics/status", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::exportCsv, "/api/export/{1}", drogon::Get);
    ADD_METHOD_TO(AnalyticsController::importCsv, "/api/import/{1}", drogon::Post);
    METHOD_LIST_END
    void timeline(const HttpRequestPtr& req, Cb&& cb);
    void response(const HttpRequestPtr& req, Cb&& cb);
    void categories(const HttpRequestPtr& req, Cb&& cb);
    void hospitals(const HttpRequestPtr& req, Cb&& cb);
    void statuses(const HttpRequestPtr& req, Cb&& cb);
    void exportCsv(const HttpRequestPtr& req, Cb&& cb, const std::string& entity);
    void importCsv(const HttpRequestPtr& req, Cb&& cb, const std::string& entity);
};

class SimulationController : public drogon::HttpController<SimulationController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SimulationController::list, "/api/simulations", drogon::Get);
    ADD_METHOD_TO(SimulationController::create, "/api/simulations", drogon::Post);
    ADD_METHOD_TO(SimulationController::replay, "/api/simulations/{1}/replay", drogon::Get);
    ADD_METHOD_TO(SimulationController::step, "/api/simulations/{1}/step", drogon::Post);
    ADD_METHOD_TO(SimulationController::resolve, "/api/simulations/{1}/resolve", drogon::Post);
    METHOD_LIST_END
    void list(const HttpRequestPtr& req, Cb&& cb);
    void create(const HttpRequestPtr& req, Cb&& cb);
    void replay(const HttpRequestPtr& req, Cb&& cb, long id);
    void step(const HttpRequestPtr& req, Cb&& cb, long id);
    void resolve(const HttpRequestPtr& req, Cb&& cb, long id);
};

class LabController : public drogon::HttpController<LabController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(LabController::catalog, "/api/lab/catalog", drogon::Get);
    ADD_METHOD_TO(LabController::benchmark, "/api/lab/benchmark", drogon::Get);
    ADD_METHOD_TO(LabController::runLab, "/api/lab/run/{1}", drogon::Post);
    METHOD_LIST_END
    void catalog(const HttpRequestPtr& req, Cb&& cb);
    void benchmark(const HttpRequestPtr& req, Cb&& cb);
    void runLab(const HttpRequestPtr& req, Cb&& cb, const std::string& kind);
};

/// Pushes svc::LiveEvent to every connected browser as JSON text frames.
class LiveBroadcaster : public svc::IEventSink {
public:
    static LiveBroadcaster& instance();
    void publish(const svc::LiveEvent& e) override;
    void add(const drogon::WebSocketConnectionPtr& c);
    void remove(const drogon::WebSocketConnectionPtr& c);
    std::size_t connections();
private:
    std::mutex m_;
    ds::LinkedList<drogon::WebSocketConnectionPtr> conns_;
};

class LiveFeedController : public drogon::WebSocketController<LiveFeedController> {
public:
    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& message, const drogon::WebSocketMessageType& type) override;
    void handleNewConnection(const HttpRequestPtr& req, const drogon::WebSocketConnectionPtr& conn) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws/live");
    WS_PATH_LIST_END
};

} // namespace nexa::api
