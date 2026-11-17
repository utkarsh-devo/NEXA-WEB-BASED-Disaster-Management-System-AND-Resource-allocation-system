#pragma once
// ============================================================================
// NEXA :: In-memory repository implementations.
// Used by unit/integration tests and by the `"storage": "memory"` demo mode
// (runs NEXA without MySQL — data is lost on restart). Each "table" is a
// DynamicArray with an auto-increment counter. "Transactions" validate every
// referenced row first and only then apply all changes (all-or-nothing).
// ============================================================================
#include <memory>
#include <mutex>
#include "Repositories.hpp"
#include "../utilities/Errors.hpp"

namespace nexa::repo {

template <typename T>
class MemTable {
public:
    DynamicArray<T> all() { std::lock_guard<std::mutex> g(m_); return rows_; }
    long insert(T& v) { std::lock_guard<std::mutex> g(m_); v.id = ++next_; rows_.pushBack(v); return v.id; }
    void update(const T& v) {
        std::lock_guard<std::mutex> g(m_);
        T* row = rows_.findFirst([&](const T& r) { return r.id == v.id; });
        if (!row) throw PersistenceError("row not found for update");
        *row = v;
    }
    void remove(long id) { std::lock_guard<std::mutex> g(m_); rows_.removeIf([&](const T& r) { return r.id == id; }); }
    bool exists(long id) { std::lock_guard<std::mutex> g(m_); return rows_.findIndex([&](const T& r) { return r.id == id; }) >= 0; }
private:
    std::mutex m_;
    DynamicArray<T> rows_;
    long next_ = 0;
};

// categories are seeded with explicit ids (they mirror the reference rows in schema.sql)
struct MemCategoryRepositoryExplicit : ICategoryRepository {
    std::mutex m; DynamicArray<IncidentCategory> rows; int next = 0;
    DynamicArray<IncidentCategory> findAll() override { std::lock_guard<std::mutex> g(m); return rows; }
    int insert(IncidentCategory& c) override {
        std::lock_guard<std::mutex> g(m);
        if (!c.id) c.id = ++next; else if (c.id > next) next = c.id;
        rows.pushBack(c);
        return c.id;
    }
};

struct MemIncidentRepository : IIncidentRepository {
    MemTable<Incident> t;
    DynamicArray<Incident> findAll() override { return t.all(); }
    long insert(Incident& i) override { return t.insert(i); }
    void update(const Incident& i) override { t.update(i); }
    void remove(long id) override { t.remove(id); }
};

struct MemEventRepository : IEventRepository {
    MemTable<IncidentEvent> t;
    DynamicArray<IncidentEvent> findAll() override { return t.all(); }
    long insert(IncidentEvent& e) override { return t.insert(e); }
};

struct MemResourceRepository : IResourceRepository {
    std::mutex m; DynamicArray<ResourceType> types; int nextType = 0;
    MemTable<Resource> t;
    DynamicArray<ResourceHistoryEntry> history;
    DynamicArray<ResourceType> findTypes() override { std::lock_guard<std::mutex> g(m); return types; }
    int insertType(ResourceType& rt) override { std::lock_guard<std::mutex> g(m); if (!rt.id) rt.id = ++nextType; else if (rt.id > nextType) nextType = rt.id; types.pushBack(rt); return rt.id; }
    DynamicArray<Resource> findAll() override { return t.all(); }
    long insert(Resource& r) override { return t.insert(r); }
    void update(const Resource& r) override { t.update(r); }
    DynamicArray<ResourceHistoryEntry> findHistory() override { std::lock_guard<std::mutex> g(m); return history; }
    void insertHistory(const ResourceHistoryEntry& h) override { std::lock_guard<std::mutex> g(m); history.pushBack(h); }
};

struct MemHospitalRepository : IHospitalRepository {
    MemTable<Hospital> t;
    MemTable<BedReservation> res;
    std::shared_ptr<IEventRepository> events;
    DynamicArray<Hospital> findAll() override { return t.all(); }
    long insert(Hospital& h) override { return t.insert(h); }
    void update(const Hospital& h) override { t.update(h); }
    void remove(long id) override { t.remove(id); }
    DynamicArray<BedReservation> findReservations() override { return res.all(); }
    long commitReservation(const Hospital& h, BedReservation& r, const IncidentEvent* ev) override {
        if (!t.exists(h.id)) throw PersistenceError("hospital missing");
        t.update(h);
        long id = res.insert(r);
        if (ev && events) { IncidentEvent e = *ev; events->insert(e); }
        return id;
    }
    void commitRelease(const Hospital& h, const BedReservation& r) override { t.update(h); res.update(r); }
};

struct MemRequestRepository : IRequestRepository {
    MemTable<EmergencyRequest> t;
    DynamicArray<EmergencyRequest> findAll() override { return t.all(); }
    long insert(EmergencyRequest& r) override { return t.insert(r); }
    void update(const EmergencyRequest& r) override { t.update(r); }
};

struct MemDeploymentRepository : IDeploymentRepository {
    MemTable<Deployment> t;
    std::shared_ptr<MemResourceRepository> resources;
    std::shared_ptr<MemEventRepository> events;
    std::shared_ptr<MemRequestRepository> requests;
    std::shared_ptr<MemIncidentRepository> incidents;
    bool failNextCommit = false;      // test hook: simulate a DB failure mid-transaction

    DynamicArray<Deployment> findAll() override { return t.all(); }

    long commitAllocation(Deployment& d, const DynamicArray<Resource>& updated, DynamicArray<IncidentEvent>& evs,
                          const EmergencyRequest* req, const Incident* inc) override {
        // validation phase (nothing written yet)
        for (const Resource& r : updated) if (!resources->t.exists(r.id)) throw PersistenceError("resource missing");
        if (failNextCommit) { failNextCommit = false; throw PersistenceError("simulated database failure"); }
        // apply phase
        t.insert(d);
        for (const Resource& r : updated) resources->t.update(r);
        for (IncidentEvent& e : evs) events->insert(e);
        if (req) { EmergencyRequest copy = *req; copy.deploymentId = d.id; requests->update(copy); }
        if (inc) incidents->update(*inc);
        return d.id;
    }
    void commitRelease(const Deployment& d, const DynamicArray<Resource>& updated, DynamicArray<IncidentEvent>& evs,
                       const Incident* inc) override {
        t.update(d);
        for (const Resource& r : updated) resources->t.update(r);
        for (IncidentEvent& e : evs) events->insert(e);
        if (inc) incidents->update(*inc);
    }
};

struct MemLocationRepository : ILocationRepository {
    MemTable<Location> locs;
    MemTable<Road> roads;
    DynamicArray<Location> findLocations() override { return locs.all(); }
    DynamicArray<Road> findRoads() override { return roads.all(); }
    long insertLocation(Location& l) override { return locs.insert(l); }
    long insertRoad(Road& r) override { return roads.insert(r); }
    void updateRoad(const Road& r) override { roads.update(r); }
};

struct MemUserRepository : IUserRepository {
    MemTable<User> t;
    DynamicArray<User> findAll() override { return t.all(); }
    long insert(User& u) override { return t.insert(u); }
    void update(const User& u) override { t.update(u); }
};

struct MemSimulationRepository : ISimulationRepository {
    MemTable<SimulationScenario> s;
    MemTable<SimulationEvent> e;
    DynamicArray<SimulationScenario> findScenarios() override { return s.all(); }
    long insertScenario(SimulationScenario& x) override { return s.insert(x); }
    DynamicArray<SimulationEvent> findEvents(long id) override { return e.all().filter([id](const SimulationEvent& v) { return v.scenarioId == id; }); }
    long insertEvent(SimulationEvent& x) override { return e.insert(x); }
};

struct MemLogRepository : ILogRepository {
    MemTable<SystemLog> t;
    void insert(SystemLog& l) override { t.insert(l); }
    DynamicArray<SystemLog> recent(int limit) override {
        auto all = t.all(); DynamicArray<SystemLog> out;
        for (long i = (long)all.size() - 1; i >= 0 && (int)out.size() < limit; --i) out.pushBack(all[i]);
        return out;
    }
};

/// Builds a fully wired in-memory bundle.
inline RepositoryBundle makeInMemoryBundle() {
    RepositoryBundle b;
    auto inc = std::make_shared<MemIncidentRepository>();
    auto ev = std::make_shared<MemEventRepository>();
    auto res = std::make_shared<MemResourceRepository>();
    auto req = std::make_shared<MemRequestRepository>();
    auto dep = std::make_shared<MemDeploymentRepository>();
    auto hos = std::make_shared<MemHospitalRepository>();
    dep->resources = res; dep->events = ev; dep->requests = req; dep->incidents = inc;
    hos->events = ev;
    b.categories = std::make_shared<MemCategoryRepositoryExplicit>();
    b.incidents = inc; b.events = ev; b.resources = res; b.requests = req; b.deployments = dep; b.hospitals = hos;
    b.locations = std::make_shared<MemLocationRepository>();
    b.users = std::make_shared<MemUserRepository>();
    b.simulations = std::make_shared<MemSimulationRepository>();
    b.logs = std::make_shared<MemLogRepository>();
    return b;
}

} // namespace nexa::repo
