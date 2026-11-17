// ============================================================================
// NEXA :: MySQL repository implementations (see MySqlRepositories.hpp)
// ============================================================================
#include "repositories/MySqlRepositories.hpp"
#include <drogon/orm/Exception.h>
#include <future>
#include <memory>
#include "utilities/Errors.hpp"

namespace nexa::repo {

using drogon::orm::DbClientPtr;
using drogon::orm::Result;
using drogon::orm::Row;

namespace {

std::string str(const Row& r, const char* c) { return r[c].isNull() ? std::string() : r[c].as<std::string>(); }
long lng(const Row& r, const char* c) { return r[c].isNull() ? 0L : (long)r[c].as<int64_t>(); }
int  in(const Row& r, const char* c) { return r[c].isNull() ? 0 : r[c].as<int>(); }
double dbl(const Row& r, const char* c) { return r[c].isNull() ? 0.0 : r[c].as<double>(); }
int64_t i64(long v) { return (int64_t)v; }

template <typename Fn>
auto guarded(const char* what, Fn fn) -> decltype(fn()) {
    try { return fn(); }
    catch (const NexaError&) { throw; }
    catch (const drogon::orm::DrogonDbException& e) { throw PersistenceError(std::string(what) + ": " + e.base().what()); }
    catch (const std::exception& e) { throw PersistenceError(std::string(what) + ": " + e.what()); }
}

/// Runs `body(tx)` inside a transaction and waits for the COMMIT result.
template <typename Fn>
void inTransaction(const DbClientPtr& db, const char* what, Fn body) {
    auto done = std::make_shared<std::promise<bool>>();
    auto fut = done->get_future();
    {
        std::shared_ptr<drogon::orm::Transaction> tx;
        try {
            tx = db->newTransaction([done](bool committed) { try { done->set_value(committed); } catch (...) {} });
            body(*tx);
        } catch (const drogon::orm::DrogonDbException& e) {
            if (tx) tx->rollback();
            throw PersistenceError(std::string(what) + " rolled back: " + e.base().what());
        } catch (...) {
            if (tx) tx->rollback();
            throw;
        }
    }   // last reference released -> COMMIT is sent
    if (fut.wait_for(std::chrono::seconds(15)) != std::future_status::ready || !fut.get())
        throw PersistenceError(std::string(what) + ": commit failed");
}

const char* TS = "CAST(COALESCE(UNIX_TIMESTAMP(%s),0) AS SIGNED)";
std::string ts(const char* col, const char* alias) { char b[160]; std::snprintf(b, sizeof b, TS, col); return std::string(b) + " AS " + alias; }

// ------------------------------------------------------------------ categories
struct MyCategories : ICategoryRepository {
    DbClientPtr db; explicit MyCategories(DbClientPtr d) : db(std::move(d)) {}
    DynamicArray<IncidentCategory> findAll() override {
        return guarded("categories.findAll", [&] {
            DynamicArray<IncidentCategory> out;
            for (const auto& r : db->execSqlSync("SELECT id, COALESCE(parent_id,0) AS parent_id, name, code, time_sensitivity, default_capabilities FROM incident_categories ORDER BY id"))
                out.pushBack(IncidentCategory{in(r, "id"), in(r, "parent_id"), str(r, "name"), str(r, "code"), dbl(r, "time_sensitivity"), str(r, "default_capabilities")});
            return out;
        });
    }
    int insert(IncidentCategory& c) override {
        return guarded("categories.insert", [&] {
            db->execSqlSync("INSERT INTO incident_categories(id, parent_id, name, code, time_sensitivity, default_capabilities) VALUES(?, NULLIF(?,0), ?, ?, ?, ?)",
                            c.id, c.parentId, c.name, c.code, c.timeSensitivity, c.defaultCapabilities);
            return c.id;
        });
    }
};

// ------------------------------------------------------------------ incidents
const std::string INCIDENT_COLS =
    "i.id, i.code, i.title, i.description, i.category_id, c.name AS category_name, i.severity, i.latitude, i.longitude, i.location_name, i.status, "
    "i.affected_people, i.required_capabilities, i.required_units, i.reported_by, i.reporter_contact, " + ts("i.created_at", "created_at") + ", " +
    ts("i.updated_at", "updated_at") + ", " + ts("i.resolved_at", "resolved_at") + ", i.simulated, COALESCE(i.scenario_id,0) AS scenario_id";

Incident toIncident(const Row& r) {
    Incident i;
    i.id = lng(r, "id"); i.code = str(r, "code"); i.title = str(r, "title"); i.description = str(r, "description");
    i.categoryId = in(r, "category_id"); i.categoryName = str(r, "category_name"); i.severity = in(r, "severity");
    i.lat = dbl(r, "latitude"); i.lon = dbl(r, "longitude"); i.locationName = str(r, "location_name");
    parse(str(r, "status"), i.status);
    i.affectedPeople = lng(r, "affected_people"); i.requiredCapabilities = str(r, "required_capabilities"); i.requiredUnits = in(r, "required_units");
    i.reportedBy = str(r, "reported_by"); i.reporterContact = str(r, "reporter_contact");
    i.createdAt = lng(r, "created_at"); i.updatedAt = lng(r, "updated_at"); i.resolvedAt = lng(r, "resolved_at");
    i.simulated = in(r, "simulated") != 0; i.scenarioId = lng(r, "scenario_id");
    return i;
}

struct MyIncidents : IIncidentRepository {
    DbClientPtr db; explicit MyIncidents(DbClientPtr d) : db(std::move(d)) {}
    DynamicArray<Incident> findAll() override {
        return guarded("incidents.findAll", [&] {
            DynamicArray<Incident> out;
            for (const auto& r : db->execSqlSync("SELECT " + INCIDENT_COLS + " FROM incidents i JOIN incident_categories c ON c.id = i.category_id ORDER BY i.id")) out.pushBack(toIncident(r));
            return out;
        });
    }
    long insert(Incident& i) override {
        return guarded("incidents.insert", [&] {
            auto res = db->execSqlSync(
                "INSERT INTO incidents(code, title, description, category_id, severity, latitude, longitude, location_name, status, affected_people, required_capabilities, "
                "required_units, reported_by, reporter_contact, created_at, updated_at, resolved_at, simulated, scenario_id) "
                "VALUES(NULLIF(?,''),?,?,?,?,?,?,?,?,?,?,?,?,?,FROM_UNIXTIME(?),FROM_UNIXTIME(?),FROM_UNIXTIME(NULLIF(?,0)),?,NULLIF(?,0))",
                i.code,   // usually empty -> NULL; the service assigns INC-YYYY-NNNN right after insert
                i.title, i.description, i.categoryId, i.severity, i.lat, i.lon, i.locationName, std::string(toString(i.status)), i64(i.affectedPeople),
                i.requiredCapabilities, i.requiredUnits, i.reportedBy, i.reporterContact, i64(i.createdAt), i64(i.updatedAt), i64(i.resolvedAt),
                i.simulated ? 1 : 0, i64(i.scenarioId));
            i.id = (long)res.insertId();
            return i.id;
        });
    }
    void update(const Incident& i) override {
        guarded("incidents.update", [&] {
            db->execSqlSync(
                "UPDATE incidents SET code=?, title=?, description=?, category_id=?, severity=?, latitude=?, longitude=?, location_name=?, status=?, affected_people=?, "
                "required_capabilities=?, required_units=?, reporter_contact=?, updated_at=FROM_UNIXTIME(?), resolved_at=FROM_UNIXTIME(NULLIF(?,0)) WHERE id=?",
                i.code, i.title, i.description, i.categoryId, i.severity, i.lat, i.lon, i.locationName, std::string(toString(i.status)), i64(i.affectedPeople),
                i.requiredCapabilities, i.requiredUnits, i.reporterContact, i64(i.updatedAt), i64(i.resolvedAt), i64(i.id));
            return 0;
        });
    }
    void remove(long id) override {
        guarded("incidents.remove", [&] { db->execSqlSync("DELETE FROM incidents WHERE id=?", i64(id)); return 0; });
    }
};

// ------------------------------------------------------------------ events
struct MyEvents : IEventRepository {
    DbClientPtr db; explicit MyEvents(DbClientPtr d) : db(std::move(d)) {}
    DynamicArray<IncidentEvent> findAll() override {
        return guarded("events.findAll", [&] {
            DynamicArray<IncidentEvent> out;
            for (const auto& r : db->execSqlSync("SELECT id, incident_id, event_type, message, actor, " + ts("created_at", "created_at") + " FROM incident_events ORDER BY created_at, id"))
                out.pushBack(IncidentEvent{lng(r, "id"), lng(r, "incident_id"), str(r, "event_type"), str(r, "message"), str(r, "actor"), lng(r, "created_at")});
            return out;
        });
    }
    long insert(IncidentEvent& e) override {
        return guarded("events.insert", [&] {
            auto res = db->execSqlSync("INSERT INTO incident_events(incident_id, event_type, message, actor, created_at) VALUES(?,?,?,?,FROM_UNIXTIME(?))",
                                       i64(e.incidentId), e.type, e.message, e.actor, i64(e.createdAt));
            e.id = (long)res.insertId();
            return e.id;
        });
    }
};

// ------------------------------------------------------------------ resources
const std::string RESOURCE_COLS =
    "r.id, r.code, r.name, r.type_id, t.name AS type_name, t.category, r.capacity, r.available, r.status, r.latitude, r.longitude, r.capabilities, "
    "COALESCE(tm.name,'') AS organization, " + ts("r.updated_at", "updated_at");

Resource toResource(const Row& r) {
    Resource x;
    x.id = lng(r, "id"); x.code = str(r, "code"); x.name = str(r, "name"); x.typeId = in(r, "type_id"); x.typeName = str(r, "type_name");
    x.category = str(r, "category"); x.capacity = in(r, "capacity"); x.available = in(r, "available"); parse(str(r, "status"), x.status);
    x.lat = dbl(r, "latitude"); x.lon = dbl(r, "longitude"); x.capabilities = str(r, "capabilities"); x.organization = str(r, "organization");
    x.updatedAt = lng(r, "updated_at");
    return x;
}

void execResourceUpdate(drogon::orm::DbClient& c, const Resource& r) {
    c.execSqlSync("INSERT IGNORE INTO response_teams(name) VALUES(?)", r.organization);
    c.execSqlSync("UPDATE resources SET code=?, name=?, type_id=?, team_id=(SELECT id FROM response_teams WHERE name=?), capacity=?, available=?, status=?, "
                  "latitude=?, longitude=?, capabilities=?, updated_at=FROM_UNIXTIME(?) WHERE id=?",
                  r.code, r.name, r.typeId, r.organization, r.capacity, r.available, std::string(toString(r.status)), r.lat, r.lon, r.capabilities, i64(r.updatedAt), i64(r.id));
}

struct MyResources : IResourceRepository {
    DbClientPtr db; explicit MyResources(DbClientPtr d) : db(std::move(d)) {}
    DynamicArray<ResourceType> findTypes() override {
        return guarded("resource_types.findAll", [&] {
            DynamicArray<ResourceType> out;
            for (const auto& r : db->execSqlSync("SELECT id, name, category, default_capabilities FROM resource_types ORDER BY id"))
                out.pushBack(ResourceType{in(r, "id"), str(r, "name"), str(r, "category"), str(r, "default_capabilities")});
            return out;
        });
    }
    int insertType(ResourceType& t) override {
        return guarded("resource_types.insert", [&] {
            db->execSqlSync("INSERT INTO resource_types(id, name, category, default_capabilities) VALUES(?,?,?,?)", t.id, t.name, t.category, t.defaultCapabilities);
            return t.id;
        });
    }
    DynamicArray<Resource> findAll() override {
        return guarded("resources.findAll", [&] {
            DynamicArray<Resource> out;
            for (const auto& r : db->execSqlSync("SELECT " + RESOURCE_COLS + " FROM resources r JOIN resource_types t ON t.id = r.type_id LEFT JOIN response_teams tm ON tm.id = r.team_id ORDER BY r.id"))
                out.pushBack(toResource(r));
            return out;
        });
    }
    long insert(Resource& r) override {
        return guarded("resources.insert", [&] {
            db->execSqlSync("INSERT IGNORE INTO response_teams(name) VALUES(?)", r.organization);
            auto res = db->execSqlSync("INSERT INTO resources(code, name, type_id, team_id, capacity, available, status, latitude, longitude, capabilities, updated_at) "
                                       "VALUES(?,?,?,(SELECT id FROM response_teams WHERE name=?),?,?,?,?,?,?,FROM_UNIXTIME(?))",
                                       r.code, r.name, r.typeId, r.organization, r.capacity, r.available, std::string(toString(r.status)), r.lat, r.lon, r.capabilities, i64(r.updatedAt));
            r.id = (long)res.insertId();
            return r.id;
        });
    }
    void update(const Resource& r) override { guarded("resources.update", [&] { execResourceUpdate(*db, r); return 0; }); }
    DynamicArray<ResourceHistoryEntry> findHistory() override {
        return guarded("resource_availability.findAll", [&] {
            DynamicArray<ResourceHistoryEntry> out;
            for (const auto& r : db->execSqlSync("SELECT resource_id, action, detail, " + ts("created_at", "created_at") + " FROM resource_availability ORDER BY created_at, id"))
                out.pushBack(ResourceHistoryEntry{lng(r, "resource_id"), str(r, "action"), str(r, "detail"), lng(r, "created_at")});
            return out;
        });
    }
    void insertHistory(const ResourceHistoryEntry& h) override {
        guarded("resource_availability.insert", [&] {
            db->execSqlSync("INSERT INTO resource_availability(resource_id, action, detail, created_at) VALUES(?,?,?,FROM_UNIXTIME(?))", i64(h.resourceId), h.action, h.detail, i64(h.at));
            return 0;
        });
    }
};

// ------------------------------------------------------------------ hospitals
void execHospitalUpdate(drogon::orm::DbClient& c, const Hospital& h) {
    c.execSqlSync("UPDATE hospitals SET name=?, category=?, latitude=?, longitude=?, total_beds=?, available_beds=?, icu_beds=?, icu_available=?, capabilities=?, "
                  "contact=?, address=?, updated_at=FROM_UNIXTIME(?) WHERE id=?",
                  h.name, h.category, h.lat, h.lon, h.totalBeds, h.availableBeds, h.icuBeds, h.icuAvailable, h.capabilities, h.contact, h.address, i64(h.updatedAt), i64(h.id));
}

struct MyHospitals : IHospitalRepository {
    DbClientPtr db; explicit MyHospitals(DbClientPtr d) : db(std::move(d)) {}
    DynamicArray<Hospital> findAll() override {
        return guarded("hospitals.findAll", [&] {
            DynamicArray<Hospital> out;
            for (const auto& r : db->execSqlSync("SELECT id, name, category, latitude, longitude, total_beds, available_beds, icu_beds, icu_available, capabilities, contact, address, " +
                                                 ts("updated_at", "updated_at") + " FROM hospitals ORDER BY id")) {
                Hospital h;
                h.id = lng(r, "id"); h.name = str(r, "name"); h.category = str(r, "category"); h.lat = dbl(r, "latitude"); h.lon = dbl(r, "longitude");
                h.totalBeds = in(r, "total_beds"); h.availableBeds = in(r, "available_beds"); h.icuBeds = in(r, "icu_beds"); h.icuAvailable = in(r, "icu_available");
                h.capabilities = str(r, "capabilities"); h.contact = str(r, "contact"); h.address = str(r, "address"); h.updatedAt = lng(r, "updated_at");
                out.pushBack(h);
            }
            return out;
        });
    }
    long insert(Hospital& h) override {
        return guarded("hospitals.insert", [&] {
            auto res = db->execSqlSync("INSERT INTO hospitals(name, category, latitude, longitude, total_beds, available_beds, icu_beds, icu_available, capabilities, contact, address, updated_at) "
                                       "VALUES(?,?,?,?,?,?,?,?,?,?,?,FROM_UNIXTIME(?))",
                                       h.name, h.category, h.lat, h.lon, h.totalBeds, h.availableBeds, h.icuBeds, h.icuAvailable, h.capabilities, h.contact, h.address, i64(h.updatedAt));
            h.id = (long)res.insertId();
            return h.id;
        });
    }
    void update(const Hospital& h) override { guarded("hospitals.update", [&] { execHospitalUpdate(*db, h); return 0; }); }
    void remove(long id) override { guarded("hospitals.remove", [&] { db->execSqlSync("DELETE FROM hospitals WHERE id=?", i64(id)); return 0; }); }
    DynamicArray<BedReservation> findReservations() override {
        return guarded("bed_reservations.findAll", [&] {
            DynamicArray<BedReservation> out;
            for (const auto& r : db->execSqlSync("SELECT id, hospital_id, incident_id, beds, " + ts("reserved_from", "reserved_from") + ", " + ts("reserved_until", "reserved_until") +
                                                 ", released FROM bed_reservations ORDER BY id")) {
                BedReservation b;
                b.id = lng(r, "id"); b.hospitalId = lng(r, "hospital_id"); b.incidentId = lng(r, "incident_id"); b.beds = in(r, "beds");
                b.from = lng(r, "reserved_from"); b.until = lng(r, "reserved_until"); b.released = in(r, "released") != 0;
                out.pushBack(b);
            }
            return out;
        });
    }
    long commitReservation(const Hospital& h, BedReservation& r, const IncidentEvent* ev) override {
        inTransaction(db, "bed reservation", [&](drogon::orm::DbClient& tx) {
            execHospitalUpdate(tx, h);
            auto res = tx.execSqlSync("INSERT INTO bed_reservations(hospital_id, incident_id, beds, reserved_from, reserved_until, released) VALUES(?,?,?,FROM_UNIXTIME(?),FROM_UNIXTIME(?),0)",
                                      i64(r.hospitalId), i64(r.incidentId), r.beds, i64(r.from), i64(r.until));
            r.id = (long)res.insertId();
            if (ev) tx.execSqlSync("INSERT INTO incident_events(incident_id, event_type, message, actor, created_at) VALUES(?,?,?,?,FROM_UNIXTIME(?))",
                                   i64(ev->incidentId), ev->type, ev->message, ev->actor, i64(ev->createdAt));
        });
        return r.id;
    }
    void commitRelease(const Hospital& h, const BedReservation& r) override {
        inTransaction(db, "bed release", [&](drogon::orm::DbClient& tx) {
            execHospitalUpdate(tx, h);
            tx.execSqlSync("UPDATE bed_reservations SET released=1, reserved_until=FROM_UNIXTIME(?) WHERE id=?", i64(r.until), i64(r.id));
        });
    }
};

// ------------------------------------------------------------------ requests
void execRequestUpdate(drogon::orm::DbClient& c, const EmergencyRequest& r) {
    c.execSqlSync("UPDATE emergency_requests SET status=?, urgency_score=?, processed_at=FROM_UNIXTIME(NULLIF(?,0)), deployment_id=NULLIF(?,0) WHERE id=?",
                  std::string(toString(r.status)), r.urgencyScore, i64(r.processedAt), i64(r.deploymentId), i64(r.id));
}

struct MyRequests : IRequestRepository {
    DbClientPtr db; explicit MyRequests(DbClientPtr d) : db(std::move(d)) {}
    DynamicArray<EmergencyRequest> findAll() override {
        return guarded("requests.findAll", [&] {
            DynamicArray<EmergencyRequest> out;
            for (const auto& r : db->execSqlSync("SELECT id, incident_id, assistance_type, description, urgency, people_affected, status, urgency_score, requested_by, " +
                                                 ts("created_at", "created_at") + ", " + ts("processed_at", "processed_at") + ", COALESCE(deployment_id,0) AS deployment_id FROM emergency_requests ORDER BY id")) {
                EmergencyRequest e;
                e.id = lng(r, "id"); e.incidentId = lng(r, "incident_id"); e.assistanceType = str(r, "assistance_type"); e.description = str(r, "description");
                e.urgency = in(r, "urgency"); e.peopleAffected = lng(r, "people_affected"); parse(str(r, "status"), e.status); e.urgencyScore = dbl(r, "urgency_score");
                e.requestedBy = str(r, "requested_by"); e.createdAt = lng(r, "created_at"); e.processedAt = lng(r, "processed_at"); e.deploymentId = lng(r, "deployment_id");
                out.pushBack(e);
            }
            return out;
        });
    }
    long insert(EmergencyRequest& e) override {
        return guarded("requests.insert", [&] {
            auto res = db->execSqlSync("INSERT INTO emergency_requests(incident_id, assistance_type, description, urgency, people_affected, status, urgency_score, requested_by, created_at) "
                                       "VALUES(?,?,?,?,?,?,?,?,FROM_UNIXTIME(?))",
                                       i64(e.incidentId), e.assistanceType, e.description, e.urgency, i64(e.peopleAffected), std::string(toString(e.status)), e.urgencyScore, e.requestedBy, i64(e.createdAt));
            e.id = (long)res.insertId();
            return e.id;
        });
    }
    void update(const EmergencyRequest& e) override { guarded("requests.update", [&] { execRequestUpdate(*db, e); return 0; }); }
};

// ------------------------------------------------------------------ deployments
void execIncidentState(drogon::orm::DbClient& c, const Incident& i) {
    c.execSqlSync("UPDATE incidents SET status=?, updated_at=FROM_UNIXTIME(?) WHERE id=?", std::string(toString(i.status)), i64(i.updatedAt), i64(i.id));
}
void execEvents(drogon::orm::DbClient& c, DynamicArray<IncidentEvent>& evs) {
    for (auto& e : evs) {
        auto res = c.execSqlSync("INSERT INTO incident_events(incident_id, event_type, message, actor, created_at) VALUES(?,?,?,?,FROM_UNIXTIME(?))",
                                 i64(e.incidentId), e.type, e.message, e.actor, i64(e.createdAt));
        e.id = (long)res.insertId();
    }
}

struct MyDeployments : IDeploymentRepository {
    DbClientPtr db; explicit MyDeployments(DbClientPtr d) : db(std::move(d)) {}
    DynamicArray<Deployment> findAll() override {
        return guarded("deployments.findAll", [&] {
            DynamicArray<Deployment> out;
            for (const auto& r : db->execSqlSync("SELECT id, incident_id, COALESCE(request_id,0) AS request_id, status, created_by, notes, " + ts("created_at", "created_at") + ", " +
                                                 ts("completed_at", "completed_at") + " FROM deployments ORDER BY id")) {
                Deployment d;
                d.id = lng(r, "id"); d.incidentId = lng(r, "incident_id"); d.requestId = lng(r, "request_id"); parse(str(r, "status"), d.status);
                d.createdBy = str(r, "created_by"); d.notes = str(r, "notes"); d.createdAt = lng(r, "created_at"); d.completedAt = lng(r, "completed_at");
                out.pushBack(d);
            }
            auto items = db->execSqlSync("SELECT dr.deployment_id, dr.resource_id, r.code, dr.units, dr.suitability, dr.route_km, dr.eta_minutes, dr.reason, dr.released, " +
                                         ts("dr.released_at", "released_at") + " FROM deployment_resources dr JOIN resources r ON r.id = dr.resource_id ORDER BY dr.deployment_id");
            for (const auto& r : items) {
                long depId = lng(r, "deployment_id");
                Deployment* d = out.findFirst([&](const Deployment& x) { return x.id == depId; });
                if (!d) continue;
                DeploymentResource x;
                x.resourceId = lng(r, "resource_id"); x.resourceCode = str(r, "code"); x.units = in(r, "units"); x.suitability = dbl(r, "suitability");
                x.routeKm = dbl(r, "route_km"); x.etaMinutes = dbl(r, "eta_minutes"); x.reason = str(r, "reason");
                x.released = in(r, "released") != 0; x.releasedAt = lng(r, "released_at");
                d->resources.pushBack(x);
            }
            return out;
        });
    }
    long commitAllocation(Deployment& d, const DynamicArray<Resource>& updated, DynamicArray<IncidentEvent>& evs,
                          const EmergencyRequest* req, const Incident* inc) override {
        inTransaction(db, "allocation", [&](drogon::orm::DbClient& tx) {
            // optimistic concurrency: re-check availability inside the transaction (row lock)
            for (const auto& r : updated) {
                auto cur = tx.execSqlSync("SELECT available, status FROM resources WHERE id=? FOR UPDATE", i64(r.id));
                if (cur.empty()) throw PersistenceError("resource " + std::to_string(r.id) + " no longer exists");
            }
            auto res = tx.execSqlSync("INSERT INTO deployments(incident_id, request_id, status, created_by, notes, created_at) VALUES(?,NULLIF(?,0),?,?,?,FROM_UNIXTIME(?))",
                                      i64(d.incidentId), i64(d.requestId), std::string(toString(d.status)), d.createdBy, d.notes, i64(d.createdAt));
            d.id = (long)res.insertId();
            for (const auto& x : d.resources)
                tx.execSqlSync("INSERT INTO deployment_resources(deployment_id, resource_id, units, suitability, route_km, eta_minutes, reason, released) VALUES(?,?,?,?,?,?,?,0)",
                               i64(d.id), i64(x.resourceId), x.units, x.suitability, x.routeKm, x.etaMinutes, x.reason);
            for (const auto& r : updated) {
                execResourceUpdate(tx, r);
                tx.execSqlSync("INSERT INTO resource_availability(resource_id, action, detail, created_at) VALUES(?,?,?,FROM_UNIXTIME(?))",
                               i64(r.id), std::string("DEPLOYED"), "Deployment #" + std::to_string(d.id) + "; available " + std::to_string(r.available) + "/" + std::to_string(r.capacity), i64(d.createdAt));
            }
            execEvents(tx, evs);
            if (req) { EmergencyRequest copy = *req; copy.deploymentId = d.id; execRequestUpdate(tx, copy); }
            if (inc) execIncidentState(tx, *inc);
        });
        return d.id;
    }
    void commitRelease(const Deployment& d, const DynamicArray<Resource>& updated, DynamicArray<IncidentEvent>& evs, const Incident* inc) override {
        inTransaction(db, "release", [&](drogon::orm::DbClient& tx) {
            tx.execSqlSync("UPDATE deployments SET status=?, completed_at=FROM_UNIXTIME(NULLIF(?,0)) WHERE id=?", std::string(toString(d.status)), i64(d.completedAt), i64(d.id));
            for (const auto& x : d.resources)
                tx.execSqlSync("UPDATE deployment_resources SET released=?, released_at=FROM_UNIXTIME(NULLIF(?,0)) WHERE deployment_id=? AND resource_id=?",
                               x.released ? 1 : 0, i64(x.releasedAt), i64(d.id), i64(x.resourceId));
            for (const auto& r : updated) {
                execResourceUpdate(tx, r);
                tx.execSqlSync("INSERT INTO resource_availability(resource_id, action, detail, created_at) VALUES(?,?,?,FROM_UNIXTIME(?))",
                               i64(r.id), std::string("RELEASED"), "Deployment #" + std::to_string(d.id) + "; available " + std::to_string(r.available) + "/" + std::to_string(r.capacity), i64(r.updatedAt));
            }
            execEvents(tx, evs);
            if (inc) execIncidentState(tx, *inc);
        });
    }
};

// ------------------------------------------------------------------ locations / roads
struct MyLocations : ILocationRepository {
    DbClientPtr db; explicit MyLocations(DbClientPtr d) : db(std::move(d)) {}
    DynamicArray<Location> findLocations() override {
        return guarded("locations.findAll", [&] {
            DynamicArray<Location> out;
            for (const auto& r : db->execSqlSync("SELECT id, name, kind, latitude, longitude FROM locations ORDER BY id"))
                out.pushBack(Location{lng(r, "id"), str(r, "name"), str(r, "kind"), dbl(r, "latitude"), dbl(r, "longitude")});
            return out;
        });
    }
    DynamicArray<Road> findRoads() override {
        return guarded("roads.findAll", [&] {
            DynamicArray<Road> out;
            for (const auto& r : db->execSqlSync("SELECT id, from_location_id, to_location_id, name, distance_km, travel_minutes, penalty, blocked FROM roads ORDER BY id"))
                out.pushBack(Road{lng(r, "id"), lng(r, "from_location_id"), lng(r, "to_location_id"), str(r, "name"), dbl(r, "distance_km"), dbl(r, "travel_minutes"), dbl(r, "penalty"), in(r, "blocked") != 0});
            return out;
        });
    }
    long insertLocation(Location& l) override {
        return guarded("locations.insert", [&] {
            auto res = db->execSqlSync("INSERT INTO locations(name, kind, latitude, longitude) VALUES(?,?,?,?)", l.name, l.kind, l.lat, l.lon);
            l.id = (long)res.insertId();
            return l.id;
        });
    }
    long insertRoad(Road& r) override {
        return guarded("roads.insert", [&] {
            auto res = db->execSqlSync("INSERT INTO roads(from_location_id, to_location_id, name, distance_km, travel_minutes, penalty, blocked) VALUES(?,?,?,?,?,?,?)",
                                       i64(r.fromId), i64(r.toId), r.name, r.distanceKm, r.travelMinutes, r.penalty, r.blocked ? 1 : 0);
            r.id = (long)res.insertId();
            return r.id;
        });
    }
    void updateRoad(const Road& r) override {
        guarded("roads.update", [&] {
            db->execSqlSync("UPDATE roads SET name=?, distance_km=?, travel_minutes=?, penalty=?, blocked=? WHERE id=?", r.name, r.distanceKm, r.travelMinutes, r.penalty, r.blocked ? 1 : 0, i64(r.id));
            return 0;
        });
    }
};

// ------------------------------------------------------------------ users
struct MyUsers : IUserRepository {
    DbClientPtr db; explicit MyUsers(DbClientPtr d) : db(std::move(d)) {}
    DynamicArray<User> findAll() override {
        return guarded("users.findAll", [&] {
            DynamicArray<User> out;
            for (const auto& r : db->execSqlSync("SELECT u.id, u.username, u.display_name, u.password_hash, u.salt, ro.code AS role, u.active, " + ts("u.created_at", "created_at") + ", " +
                                                 ts("u.last_login_at", "last_login_at") + " FROM users u JOIN roles ro ON ro.id = u.role_id ORDER BY u.id")) {
                User u;
                u.id = lng(r, "id"); u.username = str(r, "username"); u.displayName = str(r, "display_name"); u.passwordHash = str(r, "password_hash"); u.salt = str(r, "salt");
                parse(str(r, "role"), u.role); u.active = in(r, "active") != 0; u.createdAt = lng(r, "created_at"); u.lastLoginAt = lng(r, "last_login_at");
                out.pushBack(u);
            }
            return out;
        });
    }
    long insert(User& u) override {
        return guarded("users.insert", [&] {
            auto res = db->execSqlSync("INSERT INTO users(username, display_name, password_hash, salt, role_id, active, created_at) VALUES(?,?,?,?,(SELECT id FROM roles WHERE code=?),?,FROM_UNIXTIME(?))",
                                       u.username, u.displayName, u.passwordHash, u.salt, std::string(toString(u.role)), u.active ? 1 : 0, i64(u.createdAt));
            u.id = (long)res.insertId();
            return u.id;
        });
    }
    void update(const User& u) override {
        guarded("users.update", [&] {
            db->execSqlSync("UPDATE users SET display_name=?, password_hash=?, salt=?, role_id=(SELECT id FROM roles WHERE code=?), active=?, last_login_at=FROM_UNIXTIME(NULLIF(?,0)) WHERE id=?",
                            u.displayName, u.passwordHash, u.salt, std::string(toString(u.role)), u.active ? 1 : 0, i64(u.lastLoginAt), i64(u.id));
            return 0;
        });
    }
};

// ------------------------------------------------------------------ simulations
struct MySimulations : ISimulationRepository {
    DbClientPtr db; explicit MySimulations(DbClientPtr d) : db(std::move(d)) {}
    DynamicArray<SimulationScenario> findScenarios() override {
        return guarded("simulation_scenarios.findAll", [&] {
            DynamicArray<SimulationScenario> out;
            for (const auto& r : db->execSqlSync("SELECT id, name, scenario_type, description, center_lat, center_lon, incident_count, seed, " + ts("created_at", "created_at") + " FROM simulation_scenarios ORDER BY id")) {
                SimulationScenario s;
                s.id = lng(r, "id"); s.name = str(r, "name"); s.type = str(r, "scenario_type"); s.description = str(r, "description");
                s.centerLat = dbl(r, "center_lat"); s.centerLon = dbl(r, "center_lon"); s.incidentCount = in(r, "incident_count");
                s.seed = (unsigned)lng(r, "seed"); s.createdAt = lng(r, "created_at");
                out.pushBack(s);
            }
            return out;
        });
    }
    long insertScenario(SimulationScenario& s) override {
        return guarded("simulation_scenarios.insert", [&] {
            auto res = db->execSqlSync("INSERT INTO simulation_scenarios(name, scenario_type, description, center_lat, center_lon, incident_count, seed, created_at) VALUES(?,?,?,?,?,?,?,FROM_UNIXTIME(?))",
                                       s.name, s.type, s.description, s.centerLat, s.centerLon, s.incidentCount, (int64_t)s.seed, i64(s.createdAt));
            s.id = (long)res.insertId();
            return s.id;
        });
    }
    DynamicArray<SimulationEvent> findEvents(long scenarioId) override {
        return guarded("simulation_events.find", [&] {
            DynamicArray<SimulationEvent> out;
            for (const auto& r : db->execSqlSync("SELECT id, scenario_id, seq, event_type, summary, entity_id, latitude, longitude, " + ts("created_at", "created_at") +
                                                 " FROM simulation_events WHERE scenario_id=? ORDER BY seq", i64(scenarioId))) {
                SimulationEvent e;
                e.id = lng(r, "id"); e.scenarioId = lng(r, "scenario_id"); e.seq = in(r, "seq"); e.eventType = str(r, "event_type"); e.summary = str(r, "summary");
                e.entityId = lng(r, "entity_id"); e.lat = dbl(r, "latitude"); e.lon = dbl(r, "longitude"); e.createdAt = lng(r, "created_at");
                out.pushBack(e);
            }
            return out;
        });
    }
    long insertEvent(SimulationEvent& e) override {
        return guarded("simulation_events.insert", [&] {
            auto res = db->execSqlSync("INSERT INTO simulation_events(scenario_id, seq, event_type, summary, entity_id, latitude, longitude, created_at) VALUES(?,?,?,?,?,?,?,FROM_UNIXTIME(?))",
                                       i64(e.scenarioId), e.seq, e.eventType, e.summary, i64(e.entityId), e.lat, e.lon, i64(e.createdAt));
            e.id = (long)res.insertId();
            return e.id;
        });
    }
};

// ------------------------------------------------------------------ logs
struct MyLogs : ILogRepository {
    DbClientPtr db; explicit MyLogs(DbClientPtr d) : db(std::move(d)) {}
    void insert(SystemLog& l) override {
        guarded("system_logs.insert", [&] {
            auto res = db->execSqlSync("INSERT INTO system_logs(level, source, message, actor, created_at) VALUES(?,?,?,?,FROM_UNIXTIME(?))", l.level, l.source, l.message, l.actor, i64(l.createdAt));
            l.id = (long)res.insertId();
            return 0;
        });
    }
    DynamicArray<SystemLog> recent(int limit) override {
        return guarded("system_logs.recent", [&] {
            DynamicArray<SystemLog> out;
            for (const auto& r : db->execSqlSync("SELECT id, level, source, message, actor, " + ts("created_at", "created_at") + " FROM system_logs ORDER BY id DESC LIMIT ?", limit))
                out.pushBack(SystemLog{lng(r, "id"), str(r, "level"), str(r, "source"), str(r, "message"), str(r, "actor"), lng(r, "created_at")});
            return out;
        });
    }
};

} // namespace

RepositoryBundle makeMySqlBundle(const DbClientPtr& db) {
    RepositoryBundle b;
    b.categories = std::make_shared<MyCategories>(db);
    b.incidents = std::make_shared<MyIncidents>(db);
    b.events = std::make_shared<MyEvents>(db);
    b.resources = std::make_shared<MyResources>(db);
    b.hospitals = std::make_shared<MyHospitals>(db);
    b.requests = std::make_shared<MyRequests>(db);
    b.deployments = std::make_shared<MyDeployments>(db);
    b.locations = std::make_shared<MyLocations>(db);
    b.users = std::make_shared<MyUsers>(db);
    b.simulations = std::make_shared<MySimulations>(db);
    b.logs = std::make_shared<MyLogs>(db);
    return b;
}

} // namespace nexa::repo
