#include "controllers/OperationsControllers.hpp"
#include "controllers/ApiSupport.hpp"
#include "controllers/JsonMappers.hpp"

namespace nexa::api {

using svc::Permission;

namespace {
svc::IncidentInput incidentInput(const Json::Value& b) {
    svc::IncidentInput in;
    in.title = sv(b, "title"); in.description = sv(b, "description"); in.locationName = sv(b, "locationName");
    in.categoryId = iv(b, "categoryId"); in.severity = iv(b, "severity"); in.requiredUnits = iv(b, "requiredUnits", 1);
    in.lat = dv(b, "lat"); in.lon = dv(b, "lon"); in.affectedPeople = lv(b, "affectedPeople");
    in.reportedBy = sv(b, "reportedBy"); in.reporterContact = sv(b, "reporterContact");
    // capabilities may arrive as an array or a comma string
    if (b.isMember("requiredCapabilities") && b["requiredCapabilities"].isArray()) {
        std::string s;
        for (const auto& x : b["requiredCapabilities"]) { if (!s.empty()) s += ","; s += x.asString(); }
        in.requiredCapabilities = s;
    } else in.requiredCapabilities = sv(b, "requiredCapabilities");
    return in;
}
svc::ResourceInput resourceInput(const Json::Value& b) {
    svc::ResourceInput in;
    in.code = sv(b, "code"); in.name = sv(b, "name"); in.organization = sv(b, "organization"); in.status = sv(b, "status", "AVAILABLE");
    in.typeId = iv(b, "typeId"); in.capacity = iv(b, "capacity", 1); in.available = iv(b, "available", -1);
    in.lat = dv(b, "lat"); in.lon = dv(b, "lon");
    if (b.isMember("capabilities") && b["capabilities"].isArray()) {
        std::string s;
        for (const auto& x : b["capabilities"]) { if (!s.empty()) s += ","; s += x.asString(); }
        in.capabilities = s;
    } else in.capabilities = sv(b, "capabilities");
    return in;
}
Json::Value ok(const char* key, const std::string& value) { Json::Value o(Json::objectValue); o[key] = value; return o; }
}

// ------------------------------------------------------------------ incidents
void IncidentController::list(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        svc::IncidentQuery q;
        q.status = qs(req, "status"); q.categoryId = qi(req, "categoryId"); q.minSeverity = qi(req, "minSeverity", 1); q.maxSeverity = qi(req, "maxSeverity", 5);
        q.prefix = qs(req, "prefix"); q.text = qs(req, "text"); q.sortBy = qs(req, "sortBy", "priority"); q.algorithm = qs(req, "algorithm", "merge");
        q.limit = (std::size_t)qi(req, "limit", 200);
        svc::QueryStats st;
        auto rows = core().incidents.list(q, &st);
        Json::Value o(Json::objectValue);
        o["items"] = arr(rows); o["stats"] = toJson(st); o["count"] = J(rows.size());
        return o;
    });
}
void IncidentController::create(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::ManageIncidents, [&](const svc::Session& s) { return toJson(core().incidents.create(incidentInput(body(req)), actorOf(s))); }, 201);
}
void IncidentController::get(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return toJson(core().incidents.detail(id)); });
}
void IncidentController::update(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageIncidents, [&](const svc::Session& s) { return toJson(core().incidents.update(id, incidentInput(body(req)), actorOf(s))); });
}
void IncidentController::remove(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageIncidents, [&](const svc::Session& s) { return ok("result", core().incidents.removeOrArchive(id, actorOf(s))); });
}
void IncidentController::status(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageIncidents, [&](const svc::Session& s) {
        Json::Value b = body(req);
        model::IncidentStatus st;
        if (!parse(sv(b, "status"), st)) throw ValidationError("status must be PENDING, ACTIVE, CONTAINED, RESOLVED or ARCHIVED");
        return toJson(core().incidents.changeStatus(id, st, actorOf(s), sv(b, "note")));
    });
}
void IncidentController::note(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageIncidents, [&](const svc::Session& s) {
        core().incidents.addNote(id, sv(body(req), "text"), actorOf(s));
        return toJson(core().incidents.detail(id));
    }, 201);
}
void IncidentController::categories(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().incidents.categories()); });
}

// ------------------------------------------------------------------ resources
void ResourceController::list(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        svc::ResourceQuery q;
        q.category = qs(req, "category"); q.status = qs(req, "status"); q.capability = qs(req, "capability"); q.prefix = qs(req, "prefix");
        q.availableOnly = qb(req, "availableOnly"); q.sortBy = qs(req, "sortBy", "code"); q.algorithm = qs(req, "algorithm", "merge");
        q.nearLat = qd(req, "lat"); q.nearLon = qd(req, "lon"); q.limit = (std::size_t)qi(req, "limit", 500);
        svc::ResourceListStats st;
        auto rows = core().resources.list(q, &st);
        Json::Value o(Json::objectValue);
        o["items"] = arr(rows); o["stats"] = toJson(st); o["count"] = J(rows.size());
        return o;
    });
}
void ResourceController::create(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::ManageResources, [&](const svc::Session& s) { return toJson(core().resources.create(resourceInput(body(req)), actorOf(s))); }, 201);
}
void ResourceController::get(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return toJson(core().resources.detail(id)); });
}
void ResourceController::update(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageResources, [&](const svc::Session& s) { return toJson(core().resources.update(id, resourceInput(body(req)), actorOf(s))); });
}
void ResourceController::deactivate(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageResources, [&](const svc::Session& s) { return toJson(core().resources.deactivate(id, actorOf(s))); });
}
void ResourceController::status(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageResources, [&](const svc::Session& s) {
        model::ResourceStatus st;
        if (!parse(sv(body(req), "status"), st)) throw ValidationError("status must be AVAILABLE, MAINTENANCE or INACTIVE");
        return toJson(core().resources.setStatus(id, st, actorOf(s)));
    });
}
void ResourceController::types(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().resources.types()); });
}

// ------------------------------------------------------------------ search
void SearchController::suggestIncidents(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().incidents.suggest(qs(req, "q"), (std::size_t)qi(req, "limit", 8))); });
}
void SearchController::textIncidents(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        std::size_t cmps = 0;
        auto occ = core().incidents.textSearch(qs(req, "q"), &cmps);
        Json::Value o(Json::objectValue);
        o["occurrences"] = arr(occ); o["comparisons"] = J(cmps); o["index"] = "suffix_array";
        return o;
    });
}
void SearchController::suggestResources(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().resources.suggest(qs(req, "q"))); });
}
void SearchController::lookupResources(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return toJson(core().ranking.search(qs(req, "field", "id"), qs(req, "value"), qs(req, "method", "hash"))); });
}
void SearchController::resourceByCode(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return toJson(core().resources.getByCode(qs(req, "code"))); });
}

// ------------------------------------------------------------------ requests
void RequestController::list(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().requests.list(qs(req, "status"))); });
}
void RequestController::create(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::ManageRequests, [&](const svc::Session& s) {
        Json::Value b = body(req);
        svc::RequestInput in;
        in.incidentId = lv(b, "incidentId"); in.assistanceType = sv(b, "assistanceType"); in.description = sv(b, "description");
        in.urgency = iv(b, "urgency", 3); in.peopleAffected = lv(b, "peopleAffected"); in.requestedBy = sv(b, "requestedBy");
        return toJson(core().requests.create(in, actorOf(s)));
    }, 201);
}
void RequestController::get(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        Json::Value o = toJson(core().requests.get(id));
        o["history"] = arr(core().requests.history(id));
        return o;
    });
}
void RequestController::escalate(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageRequests, [&](const svc::Session& s) { return toJson(core().requests.escalate(id, actorOf(s))); });
}
void RequestController::cancel(const HttpRequestPtr& req, Cb&& cb, long id) {
    run(req, cb, Permission::ManageRequests, [&](const svc::Session& s) { return toJson(core().requests.cancel(id, actorOf(s))); });
}
void RequestController::queues(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return toJson(core().requests.queues()); });
}
void RequestController::process(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::Allocate, [&](const svc::Session& s) { return toJson(core().requests.processNext(sv(body(req), "mode", "auto"), actorOf(s))); });
}

// ------------------------------------------------------------------ priority
void PriorityController::queue(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        Json::Value o(Json::objectValue);
        o["queue"] = arr(core().priority.orderedQueue((std::size_t)qi(req, "limit", 0)));
        o["weights"] = toJson(core().priority.weights());
        o["formula"] = "P = 100·(wS·S + wA·A + wU·U + wT·T + wSt·St + wTs·Ts) / Σw";
        return o;
    });
}
void PriorityController::heap(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) { return arr(core().priority.heapArray()); });
}
void PriorityController::refresh(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::ManageIncidents, [&](const svc::Session&) { core().priority.refreshAll(); return arr(core().priority.orderedQueue(0)); });
}
void PriorityController::next(const HttpRequestPtr& req, Cb&& cb) {
    run(req, cb, Permission::View, [&](const svc::Session&) {
        long id = core().priority.peekNext();
        if (!id) return Json::Value(Json::nullValue);
        return toJson(core().incidents.detail(id));
    });
}

} // namespace nexa::api
