#pragma once
// ============================================================================
// NEXA :: incident, resource, request, priority and search endpoints
// Route note: search/suggest endpoints live under /api/search/* so they can
// never collide with /api/<entity>/{id} patterns.
// ============================================================================
#include <drogon/HttpController.h>

namespace nexa::api {

using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
using Cb = std::function<void(const HttpResponsePtr&)>;

class IncidentController : public drogon::HttpController<IncidentController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(IncidentController::list, "/api/incidents", drogon::Get);
    ADD_METHOD_TO(IncidentController::create, "/api/incidents", drogon::Post);
    ADD_METHOD_TO(IncidentController::get, "/api/incidents/{1}", drogon::Get);
    ADD_METHOD_TO(IncidentController::update, "/api/incidents/{1}", drogon::Put);
    ADD_METHOD_TO(IncidentController::remove, "/api/incidents/{1}", drogon::Delete);
    ADD_METHOD_TO(IncidentController::status, "/api/incidents/{1}/status", drogon::Patch, drogon::Post);
    ADD_METHOD_TO(IncidentController::note, "/api/incidents/{1}/notes", drogon::Post);
    ADD_METHOD_TO(IncidentController::categories, "/api/categories", drogon::Get);
    METHOD_LIST_END
    void list(const HttpRequestPtr& req, Cb&& cb);
    void create(const HttpRequestPtr& req, Cb&& cb);
    void get(const HttpRequestPtr& req, Cb&& cb, long id);
    void update(const HttpRequestPtr& req, Cb&& cb, long id);
    void remove(const HttpRequestPtr& req, Cb&& cb, long id);
    void status(const HttpRequestPtr& req, Cb&& cb, long id);
    void note(const HttpRequestPtr& req, Cb&& cb, long id);
    void categories(const HttpRequestPtr& req, Cb&& cb);
};

class ResourceController : public drogon::HttpController<ResourceController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ResourceController::list, "/api/resources", drogon::Get);
    ADD_METHOD_TO(ResourceController::create, "/api/resources", drogon::Post);
    ADD_METHOD_TO(ResourceController::get, "/api/resources/{1}", drogon::Get);
    ADD_METHOD_TO(ResourceController::update, "/api/resources/{1}", drogon::Put);
    ADD_METHOD_TO(ResourceController::deactivate, "/api/resources/{1}", drogon::Delete);
    ADD_METHOD_TO(ResourceController::status, "/api/resources/{1}/status", drogon::Patch, drogon::Post);
    ADD_METHOD_TO(ResourceController::types, "/api/resource-types", drogon::Get);
    METHOD_LIST_END
    void list(const HttpRequestPtr& req, Cb&& cb);
    void create(const HttpRequestPtr& req, Cb&& cb);
    void get(const HttpRequestPtr& req, Cb&& cb, long id);
    void update(const HttpRequestPtr& req, Cb&& cb, long id);
    void deactivate(const HttpRequestPtr& req, Cb&& cb, long id);
    void status(const HttpRequestPtr& req, Cb&& cb, long id);
    void types(const HttpRequestPtr& req, Cb&& cb);
};

class SearchController : public drogon::HttpController<SearchController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SearchController::suggestIncidents, "/api/search/incidents/suggest", drogon::Get);
    ADD_METHOD_TO(SearchController::textIncidents, "/api/search/incidents/text", drogon::Get);
    ADD_METHOD_TO(SearchController::suggestResources, "/api/search/resources/suggest", drogon::Get);
    ADD_METHOD_TO(SearchController::lookupResources, "/api/search/resources/lookup", drogon::Get);
    ADD_METHOD_TO(SearchController::resourceByCode, "/api/search/resources/code", drogon::Get);
    METHOD_LIST_END
    void suggestIncidents(const HttpRequestPtr& req, Cb&& cb);
    void textIncidents(const HttpRequestPtr& req, Cb&& cb);
    void suggestResources(const HttpRequestPtr& req, Cb&& cb);
    void lookupResources(const HttpRequestPtr& req, Cb&& cb);
    void resourceByCode(const HttpRequestPtr& req, Cb&& cb);
};

class RequestController : public drogon::HttpController<RequestController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(RequestController::list, "/api/requests", drogon::Get);
    ADD_METHOD_TO(RequestController::create, "/api/requests", drogon::Post);
    ADD_METHOD_TO(RequestController::get, "/api/requests/{1}", drogon::Get);
    ADD_METHOD_TO(RequestController::escalate, "/api/requests/{1}/escalate", drogon::Post);
    ADD_METHOD_TO(RequestController::cancel, "/api/requests/{1}/cancel", drogon::Post);
    ADD_METHOD_TO(RequestController::queues, "/api/request-queue", drogon::Get);
    ADD_METHOD_TO(RequestController::process, "/api/request-queue/process", drogon::Post);
    METHOD_LIST_END
    void list(const HttpRequestPtr& req, Cb&& cb);
    void create(const HttpRequestPtr& req, Cb&& cb);
    void get(const HttpRequestPtr& req, Cb&& cb, long id);
    void escalate(const HttpRequestPtr& req, Cb&& cb, long id);
    void cancel(const HttpRequestPtr& req, Cb&& cb, long id);
    void queues(const HttpRequestPtr& req, Cb&& cb);
    void process(const HttpRequestPtr& req, Cb&& cb);
};

class PriorityController : public drogon::HttpController<PriorityController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(PriorityController::queue, "/api/priority/queue", drogon::Get);
    ADD_METHOD_TO(PriorityController::heap, "/api/priority/heap", drogon::Get);
    ADD_METHOD_TO(PriorityController::refresh, "/api/priority/refresh", drogon::Post);
    ADD_METHOD_TO(PriorityController::next, "/api/priority/next", drogon::Get);
    METHOD_LIST_END
    void queue(const HttpRequestPtr& req, Cb&& cb);
    void heap(const HttpRequestPtr& req, Cb&& cb);
    void refresh(const HttpRequestPtr& req, Cb&& cb);
    void next(const HttpRequestPtr& req, Cb&& cb);
};

} // namespace nexa::api
