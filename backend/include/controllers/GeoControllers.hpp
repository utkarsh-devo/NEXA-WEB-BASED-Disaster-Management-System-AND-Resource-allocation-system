#pragma once
// ============================================================================
// NEXA :: spatial discovery, road network, allocation, ranking and hospital endpoints
// ============================================================================
#include <drogon/HttpController.h>

namespace nexa::api {

using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
using Cb = std::function<void(const HttpResponsePtr&)>;

class SpatialController : public drogon::HttpController<SpatialController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SpatialController::nearest, "/api/spatial/nearest", drogon::Get);
    ADD_METHOD_TO(SpatialController::radius, "/api/spatial/radius", drogon::Get);
    ADD_METHOD_TO(SpatialController::region, "/api/spatial/region", drogon::Get);
    ADD_METHOD_TO(SpatialController::clusters, "/api/spatial/clusters", drogon::Get);
    ADD_METHOD_TO(SpatialController::density, "/api/spatial/density", drogon::Get);
    ADD_METHOD_TO(SpatialController::compare, "/api/spatial/compare", drogon::Get);
    ADD_METHOD_TO(SpatialController::layers, "/api/spatial/layers", drogon::Get);
    METHOD_LIST_END
    void nearest(const HttpRequestPtr& req, Cb&& cb);
    void radius(const HttpRequestPtr& req, Cb&& cb);
    void region(const HttpRequestPtr& req, Cb&& cb);
    void clusters(const HttpRequestPtr& req, Cb&& cb);
    void density(const HttpRequestPtr& req, Cb&& cb);
    void compare(const HttpRequestPtr& req, Cb&& cb);
    void layers(const HttpRequestPtr& req, Cb&& cb);
};

class NetworkController : public drogon::HttpController<NetworkController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(NetworkController::graph, "/api/network/graph", drogon::Get);
    ADD_METHOD_TO(NetworkController::summary, "/api/network/summary", drogon::Get);
    ADD_METHOD_TO(NetworkController::route, "/api/network/route", drogon::Get);
    ADD_METHOD_TO(NetworkController::compare, "/api/network/compare", drogon::Get);
    ADD_METHOD_TO(NetworkController::explore, "/api/network/explore", drogon::Get);
    ADD_METHOD_TO(NetworkController::mst, "/api/network/mst", drogon::Get);
    ADD_METHOD_TO(NetworkController::components, "/api/network/components", drogon::Get);
    ADD_METHOD_TO(NetworkController::blockRoad, "/api/network/roads/{1}", drogon::Patch, drogon::Put);
    METHOD_LIST_END
    void graph(const HttpRequestPtr& req, Cb&& cb);
    void summary(const HttpRequestPtr& req, Cb&& cb);
    void route(const HttpRequestPtr& req, Cb&& cb);
    void compare(const HttpRequestPtr& req, Cb&& cb);
    void explore(const HttpRequestPtr& req, Cb&& cb);
    void mst(const HttpRequestPtr& req, Cb&& cb);
    void components(const HttpRequestPtr& req, Cb&& cb);
    void blockRoad(const HttpRequestPtr& req, Cb&& cb, long id);
};

class AllocationController : public drogon::HttpController<AllocationController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AllocationController::rank, "/api/ranking/{1}", drogon::Get);
    ADD_METHOD_TO(AllocationController::list, "/api/deployments", drogon::Get);
    ADD_METHOD_TO(AllocationController::allocate, "/api/deployments", drogon::Post);
    ADD_METHOD_TO(AllocationController::get, "/api/deployments/{1}", drogon::Get);
    ADD_METHOD_TO(AllocationController::complete, "/api/deployments/{1}/complete", drogon::Post);
    ADD_METHOD_TO(AllocationController::cancel, "/api/deployments/{1}/cancel", drogon::Post);
    ADD_METHOD_TO(AllocationController::reassign, "/api/allocation/reassign", drogon::Post);
    ADD_METHOD_TO(AllocationController::matrix, "/api/allocation/matrix", drogon::Get);
    METHOD_LIST_END
    void rank(const HttpRequestPtr& req, Cb&& cb, long incidentId);
    void list(const HttpRequestPtr& req, Cb&& cb);
    void allocate(const HttpRequestPtr& req, Cb&& cb);
    void get(const HttpRequestPtr& req, Cb&& cb, long id);
    void complete(const HttpRequestPtr& req, Cb&& cb, long id);
    void cancel(const HttpRequestPtr& req, Cb&& cb, long id);
    void reassign(const HttpRequestPtr& req, Cb&& cb);
    void matrix(const HttpRequestPtr& req, Cb&& cb);
};

class HospitalController : public drogon::HttpController<HospitalController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HospitalController::list, "/api/hospitals", drogon::Get);
    ADD_METHOD_TO(HospitalController::create, "/api/hospitals", drogon::Post);
    ADD_METHOD_TO(HospitalController::get, "/api/hospitals/{1}", drogon::Get);
    ADD_METHOD_TO(HospitalController::update, "/api/hospitals/{1}", drogon::Put);
    ADD_METHOD_TO(HospitalController::remove, "/api/hospitals/{1}", drogon::Delete);
    ADD_METHOD_TO(HospitalController::reserve, "/api/hospitals/{1}/reserve", drogon::Post);
    ADD_METHOD_TO(HospitalController::load, "/api/hospitals/{1}/load", drogon::Get);
    ADD_METHOD_TO(HospitalController::suitable, "/api/medical/suitable", drogon::Get);
    ADD_METHOD_TO(HospitalController::reservations, "/api/reservations", drogon::Get);
    ADD_METHOD_TO(HospitalController::release, "/api/reservations/{1}/release", drogon::Post);
    METHOD_LIST_END
    void list(const HttpRequestPtr& req, Cb&& cb);
    void create(const HttpRequestPtr& req, Cb&& cb);
    void get(const HttpRequestPtr& req, Cb&& cb, long id);
    void update(const HttpRequestPtr& req, Cb&& cb, long id);
    void remove(const HttpRequestPtr& req, Cb&& cb, long id);
    void reserve(const HttpRequestPtr& req, Cb&& cb, long id);
    void load(const HttpRequestPtr& req, Cb&& cb, long id);
    void suitable(const HttpRequestPtr& req, Cb&& cb);
    void reservations(const HttpRequestPtr& req, Cb&& cb);
    void release(const HttpRequestPtr& req, Cb&& cb, long id);
};

} // namespace nexa::api
