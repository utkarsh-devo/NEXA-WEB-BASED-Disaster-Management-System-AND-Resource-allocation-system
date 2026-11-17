// ============================================================================
// NEXA :: NexaCore::bootstrap — rebuilds the OperationalStore from MySQL
// (or the in-memory repositories). Order matters: taxonomy and network
// first, then entities, then relationships, then derived priorities.
// ============================================================================
#include "services/NexaCore.hpp"
#include <chrono>

namespace nexa::svc {

BootstrapReport NexaCore::bootstrap() {
    std::lock_guard<std::recursive_mutex> g(store.mtx);
    auto t0 = std::chrono::steady_clock::now();
    BootstrapReport rep;
    auto& s = store;

    // ---- reset ----
    s.freeLists();
    s.incidents.clear(); s.resources.clear(); s.hospitals.clear(); s.requests.clear(); s.deployments.clear();
    s.locations.clear(); s.roadRecords.clear(); s.reservations.clear();
    s.incidentWords.clear(); s.resourceWords.clear(); s.resourceByCode.clear(); s.hospitalByName.clear();
    s.incidentByTime.clear(); s.incidentQueue.clear(); s.urgentRequests.clear(); s.fifoRequests.clear();
    s.links.clear(); s.unitsMatrix.clear(); s.categoryById.clear(); s.actions.clear();
    s.descriptionsDirty = true;

    // ---- 1. taxonomy (K-ary tree) ----
    s.taxonomy.setRoot(0, "All Disasters", IncidentCategory{0, -1, "All Disasters", "ROOT", 0.5, ""});
    auto cats = repos.categories->findAll();
    // insert parents before children: repeat passes until no progress (handles any row order)
    DynamicArray<bool> placed; for (std::size_t i = 0; i < cats.size(); ++i) placed.pushBack(false);
    bool progress = true;
    while (progress) {
        progress = false;
        for (std::size_t i = 0; i < cats.size(); ++i) {
            if (placed[i] || !s.taxonomy.find(cats[i].parentId)) continue;
            s.taxonomy.addChild(cats[i].parentId, cats[i].id, cats[i].name, cats[i]);
            s.categoryById.insert(cats[i].id, cats[i]);
            placed[i] = true; progress = true; ++rep.categories;
        }
    }
    for (std::size_t i = 0; i < cats.size(); ++i) if (!placed[i]) util::Logger::instance().warn("bootstrap", "category " + cats[i].name + " has unknown parent; skipped");

    s.resourceTypes = repos.resources->findTypes();
    rep.resourceTypes = s.resourceTypes.size();

    // ---- 2. road network ----
    for (const auto& l : repos.locations->findLocations()) s.locations.upsert(l);
    for (const auto& r : repos.locations->findRoads()) s.roadRecords.upsert(r);
    rep.locations = s.locations.size(); rep.roads = s.roadRecords.size();
    network.rebuildGraph();

    // ---- 3. resources ----
    for (const auto& r : repos.resources->findAll()) {
        s.resources.upsert(r);
        s.resourceByCode.insert(r.code, r.id);
        resources.indexText(r);
    }
    for (const auto& h : repos.resources->findHistory()) s.history(h.resourceId).insertBack(h);
    rep.resources = s.resources.size();

    // ---- 4. hospitals + reservations ----
    for (const auto& h : repos.hospitals->findAll()) { s.hospitals.upsert(h); s.hospitalByName.insert(util::lower(h.name), h.id); }
    for (const auto& r : repos.hospitals->findReservations()) {
        s.reservations.upsert(r);
        if (!r.released) s.windowsOf(s.bedWindows, r.hospitalId).insert(ds::Interval{r.from, r.until, r.id});
    }
    rep.hospitals = s.hospitals.size();

    // ---- 5. incidents + timelines ----
    for (auto i : repos.incidents->findAll()) {
        i.assignedUnits = 0;
        i.categoryPath = incidents.categoryPath(i.categoryId);
        if (const IncidentCategory* c = s.categoryById.find(i.categoryId)) i.categoryName = c->name;
        s.incidents.upsert(i);
        s.incidentByTime.insert(TimeKey{i.createdAt, i.id}, i.id);
        incidents.indexText(i);
    }
    rep.incidents = s.incidents.size();
    auto events = repos.events->findAll();
    algo::mergeSort(events, [](const IncidentEvent& a, const IncidentEvent& b) { return a.createdAt < b.createdAt || (a.createdAt == b.createdAt && a.id < b.id); });
    for (const auto& e : events) s.timeline(e.incidentId).insertBack(e);
    rep.events = events.size();

    // ---- 6. deployments -> relationships, commitments, assigned units ----
    for (const auto& d : repos.deployments->findAll()) {
        s.deployments.upsert(d);
        allocation.indexDeployment(d);
        if (d.status == DeploymentStatus::Active)
            if (Incident* i = s.incidents.get(d.incidentId))
                for (const auto& dr : d.resources) if (!dr.released) i->assignedUnits += dr.units;
    }
    rep.deployments = s.deployments.size();

    // ---- 7. spatial indexes (balanced bulk build) ----
    spatial.rebuildAll();

    // ---- 8. priorities, then requests (urgency uses incident priority) ----
    priority.refreshAll();
    auto reqs = repos.requests->findAll();
    for (auto r : reqs) {
        if (r.status == RequestStatus::Processing) r.status = RequestStatus::Queued;   // crash recovery: re-queue
        s.requests.upsert(r);
        requests.enqueue(r);
        s.requestLog(r.id).insertBack(util::isoTime(r.createdAt) + " loaded with status " + toString(r.status));
    }
    rep.requests = reqs.size();

    // ---- 9. users ----
    auth.load();
    rep.users = auth.userCount();

    rep.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    util::Logger::instance().info("bootstrap", "store rebuilt: " + std::to_string(rep.incidents) + " incidents, " + std::to_string(rep.resources) +
                                  " resources, " + std::to_string(rep.locations) + " locations, " + std::to_string(rep.roads) + " roads in " +
                                  std::to_string((int)rep.milliseconds) + " ms");
    return rep;
}

} // namespace nexa::svc
