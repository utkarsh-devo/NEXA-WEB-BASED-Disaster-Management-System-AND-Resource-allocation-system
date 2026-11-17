// Integration tests: the end-to-end disaster-response workflow (spec §5) on
// the in-memory repositories + demo seed. Verifies consistency between the
// "database" and the in-memory structures, transactional allocation, and
// the main validation rules.
#include "support/TestFramework.hpp"
#include "repositories/InMemoryRepositories.hpp"
#include "services/NexaCore.hpp"

using namespace nexa;
using namespace nexa::svc;

namespace {
struct Fixture {
    EngineConfig cfg;
    RecordingSink sink;
    repo::RepositoryBundle bundle = repo::makeInMemoryBundle();
    std::unique_ptr<NexaCore> core;
    Fixture() { core = std::make_unique<NexaCore>(cfg, bundle, sink); core->seedDemoData(); }
};
IncidentInput sampleIncident() {
    IncidentInput in;
    in.title = "Test gas leak"; in.description = "Gas leak near school with children inside"; in.categoryId = 23; in.severity = 4;
    in.lat = 28.6290; in.lon = 77.2200; in.locationName = "Connaught Place"; in.affectedPeople = 300;
    in.requiredCapabilities = "hazmat,medical"; in.requiredUnits = 2;
    return in;
}
}

TEST(Workflow, SeedBuildsConsistentStore) {
    Fixture f;
    auto& s = f.core->store;
    EXPECT_GT(s.incidents.size(), 10u);
    EXPECT_GT(s.resources.size(), 40u);
    EXPECT_EQ(s.hospitals.size(), 14u);
    EXPECT_GT(s.roads.vertexCount(), 50);
    EXPECT_EQ(s.incidents.size(), f.bundle.incidents->findAll().size());
    EXPECT_EQ(s.resources.size(), f.bundle.resources->findAll().size());
    // a fresh bootstrap from the repositories reproduces the same state
    std::size_t deployments = s.deployments.size(), links = s.links.size();
    auto rep = f.core->bootstrap();
    EXPECT_EQ(rep.incidents, s.incidents.size());
    EXPECT_EQ(s.deployments.size(), deployments);
    EXPECT_EQ(s.links.size(), links);
    EXPECT_GT(links, 0u);
    // disconnected relief camp is detected (pontoon road blocked)
    auto summary = f.core->network.summary();
    EXPECT_GE(summary.components, 2);
    EXPECT_EQ(summary.blockedRoads, 1);
}

TEST(Workflow, EndToEndRegistrationToResolution) {
    Fixture f;
    auto& c = *f.core;
    // Stage 1-3: registration, classification, priority
    Incident inc = c.incidents.create(sampleIncident(), "tester");
    EXPECT_TRUE(inc.code.rfind("INC-", 0) == 0);
    EXPECT_EQ(inc.categoryPath, std::string("Technological > Gas Leak / Hazmat"));
    EXPECT_GT(inc.priorityScore, 50.0);
    EXPECT_TRUE(c.store.incidentQueue.contains(inc.id));
    EXPECT_TRUE(c.store.incidentWords.search("gas") != nullptr);
    EXPECT_GE(c.incidents.textSearch("children").size(), 1u);
    // Stage 4-7: ranking with spatial + road network
    RankOptions opt; opt.limit = 5;
    auto rank = c.ranking.rankForIncident(inc.id, opt);
    ASSERT_GE(rank.candidates.size(), 1u);
    for (std::size_t i = 1; i < rank.candidates.size(); ++i) EXPECT_GE(rank.candidates[i - 1].score, rank.candidates[i].score);
    EXPECT_GT(rank.candidates[0].capabilityMatch, 0.0);
    EXPECT_TRUE(rank.candidates[0].routable);
    EXPECT_GT(rank.medianRoadKm, 0.0);
    // Stage 8: allocation (automatic)
    double before = inc.priorityScore;
    ds::HashTable<long, int> availBefore;
    for (const auto& x : c.store.resources.items()) availBefore.insert(x.id, x.available);
    AllocationRequest ar; ar.incidentId = inc.id;
    Deployment d = c.allocation.allocate(ar, "tester");
    EXPECT_GT(d.id, 0);
    EXPECT_GE(d.resources.size(), 1u);
    EXPECT_FALSE(d.resources[0].reason.empty());       // records WHY
    Incident after = c.incidents.get(inc.id);
    EXPECT_EQ(after.status, model::IncidentStatus::Active);
    EXPECT_GT(after.assignedUnits, 0);
    EXPECT_LT(after.priorityScore, before);            // unmet requirement dropped
    const Resource* r = c.store.resources.get(d.resources[0].resourceId);
    EXPECT_LT(r->available, r->capacity);
    EXPECT_TRUE(c.store.links.find(inc.id, r->id) != nullptr);
    EXPECT_EQ(c.store.unitsMatrix.get(inc.id, r->id), d.resources[0].units);
    // DB agrees with memory
    bool persisted = false;
    for (const auto& x : f.bundle.resources->findAll()) if (x.id == r->id) persisted = x.available == r->available;
    EXPECT_TRUE(persisted);
    // cannot resolve with active deployment
    EXPECT_THROW(c.incidents.changeStatus(inc.id, model::IncidentStatus::Resolved, "tester"), ConflictError);
    // Stage 9: completion restores availability
    c.allocation.complete(d.id, "tester");
    const Resource* r2 = c.store.resources.get(d.resources[0].resourceId);
    EXPECT_EQ(r2->available, *availBefore.find(r2->id));      // restored exactly
    EXPECT_TRUE(c.store.links.find(inc.id, r2->id) == nullptr);
    Incident resolved = c.incidents.changeStatus(inc.id, model::IncidentStatus::Resolved, "tester");
    EXPECT_EQ(resolved.status, model::IncidentStatus::Resolved);
    EXPECT_FALSE(c.store.incidentQueue.contains(inc.id));
    // timeline is chronological and complete
    auto det = c.incidents.detail(inc.id);
    EXPECT_GE(det.timeline.size(), 5u);
    EXPECT_EQ(det.timeline[0].type, std::string("REGISTERED"));
    // live updates were published
    bool sawDeployment = false;
    for (const auto& e : f.sink.events) if (e.type == "deployment.created") sawDeployment = true;
    EXPECT_TRUE(sawDeployment);
}

TEST(Workflow, AllocationIsTransactional) {
    Fixture f;
    auto& c = *f.core;
    Incident inc = c.incidents.create(sampleIncident(), "tester");
    auto snapshot = c.store.resources.items();
    auto* dep = static_cast<repo::MemDeploymentRepository*>(f.bundle.deployments.get());
    dep->failNextCommit = true;
    AllocationRequest ar; ar.incidentId = inc.id;
    EXPECT_THROW(c.allocation.allocate(ar, "tester"), PersistenceError);
    // nothing changed in memory or in the "database"
    for (const auto& r : snapshot) EXPECT_EQ(c.store.resources.get(r.id)->available, r.available);
    EXPECT_EQ(c.incidents.get(inc.id).assignedUnits, 0);
    EXPECT_EQ(c.incidents.get(inc.id).status, model::IncidentStatus::Pending);
    // and a retry succeeds
    EXPECT_NO_THROW(c.allocation.allocate(ar, "tester"));
}

TEST(Workflow, ValidationAndConflictRules) {
    Fixture f;
    auto& c = *f.core;
    IncidentInput bad = sampleIncident(); bad.severity = 9;
    EXPECT_THROW(c.incidents.create(bad, "t"), ValidationError);
    bad = sampleIncident(); bad.lat = 0; bad.lon = 0;
    EXPECT_THROW(c.incidents.create(bad, "t"), ValidationError);       // missing location
    bad = sampleIncident(); bad.categoryId = 999;
    EXPECT_THROW(c.incidents.create(bad, "t"), ValidationError);
    // duplicate resource code
    ResourceInput ri; ri.code = c.store.resources.items()[0].code; ri.name = "dup"; ri.typeId = 1; ri.lat = 28.6; ri.lon = 77.2;
    EXPECT_THROW(c.resources.create(ri, "t"), ConflictError);
    // unavailable resource cannot be allocated
    Incident inc = c.incidents.create(sampleIncident(), "t");
    long maintenanceId = 0;
    for (const auto& r : c.store.resources.items()) if (r.status == model::ResourceStatus::Maintenance) maintenanceId = r.id;
    ASSERT_TRUE(maintenanceId != 0);
    AllocationRequest ar; ar.incidentId = inc.id; ar.items.pushBack(AllocationItem{maintenanceId, 1});
    EXPECT_THROW(c.allocation.allocate(ar, "t"), ConflictError);
    // single-unit vehicle cannot be on two simultaneous deployments
    AllocationRequest first; first.incidentId = inc.id;
    Deployment d = c.allocation.allocate(first, "t");
    long busy = 0;
    for (const auto& dr : d.resources) if (c.store.resources.get(dr.resourceId)->capacity == 1) busy = dr.resourceId;
    if (busy) {
        Incident other = c.incidents.create(sampleIncident(), "t");
        AllocationRequest second; second.incidentId = other.id; second.items.pushBack(AllocationItem{busy, 1});
        EXPECT_THROW(c.allocation.allocate(second, "t"), ConflictError);
        // reassignment moves it cleanly
        Deployment moved = c.allocation.reassign(busy, d.id, other.id, "t");
        EXPECT_EQ(moved.incidentId, other.id);
        EXPECT_TRUE(c.store.links.find(inc.id, busy) == nullptr);
        EXPECT_TRUE(c.store.links.find(other.id, busy) != nullptr);
    }
}

TEST(Workflow, RequestQueuesFifoVsPriority) {
    Fixture f;
    auto& c = *f.core;
    Incident inc = c.incidents.create(sampleIncident(), "t");
    RequestInput low; low.incidentId = inc.id; low.assistanceType = "supplies"; low.urgency = 1; low.peopleAffected = 1;
    RequestInput high = low; high.assistanceType = "medical"; high.urgency = 5; high.peopleAffected = 100;
    auto q0 = c.requests.queues();
    EmergencyRequest a = c.requests.create(low, "t");
    EmergencyRequest b = c.requests.create(high, "t");
    EXPECT_EQ(a.status, model::RequestStatus::Queued);
    EXPECT_EQ(b.status, model::RequestStatus::Escalated);
    auto q = c.requests.queues();
    EXPECT_EQ(q.fifo.size(), q0.fifo.size() + 1);
    // priority queue is served first even though b arrived later
    ProcessResult pr = c.requests.processNext("auto", "t");
    EXPECT_EQ(pr.queueUsed, std::string("priority"));
    // escalate the low one and verify it leaves the FIFO queue
    c.requests.escalate(a.id, "t");
    bool inFifo = false;
    for (const auto& r : c.requests.queues().fifo) if (r.id == a.id) inFifo = true;
    EXPECT_FALSE(inFifo);
}

TEST(Workflow, SpatialNetworkHospitalsAnalyticsLab) {
    Fixture f;
    auto& c = *f.core;
    SpatialFilter sf; sf.kind = "resource"; sf.availableOnly = false;
    SpatialQueryStats a, b;
    auto kd = c.spatial.nearest(28.63, 77.22, 5, sf, &a);
    auto lin = c.spatial.nearestLinear(28.63, 77.22, 5, sf, &b);
    ASSERT_EQ(kd.size(), 5u);
    EXPECT_EQ(kd[0].id, lin[0].id);
    ds::GeoRect box{28.5, 77.1, 28.7, 77.3};
    EXPECT_EQ(c.spatial.region(box, "incident", "quadtree").size(), c.spatial.region(box, "incident", "linear").size());
    EXPECT_EQ(c.spatial.region(box, "incident", "rangetree").size(), c.spatial.region(box, "incident", "linear").size());
    auto routes = c.network.compareRoutes(28.6315, 77.2167, 28.5708, 77.3260);
    ASSERT_GE(routes.size(), 3u);
    EXPECT_TRUE(routes[0].found);
    EXPECT_LE(routes[0].distanceKm, routes[1].distanceKm + 1e-6);   // distance-optimal is never longer
    auto k = c.network.minimumSpanningTree("kruskal", ds::WeightMode::Distance);
    auto p = c.network.minimumSpanningTree("prim", ds::WeightMode::Distance);
    EXPECT_NEAR(k.totalWeight, p.totalWeight, 1e-6);
    auto hs = c.hospitals.suitable(28.63, 77.22, "trauma", 2, 3);
    ASSERT_GE(hs.size(), 1u);
    long hid = hs[0].hospital.id; int beds = hs[0].hospital.availableBeds;
    Incident inc = c.incidents.create(sampleIncident(), "t");
    auto res = c.hospitals.reserve(hid, inc.id, 2, 6, "t");
    EXPECT_EQ(c.hospitals.get(hid).availableBeds, beds - 2);
    EXPECT_GE(c.hospitals.load(hid, res.from, res.until).reservedBeds, 2);
    c.hospitals.release(res.id, "t");
    EXPECT_EQ(c.hospitals.get(hid).availableBeds, beds);
    EXPECT_THROW(c.hospitals.reserve(hid, inc.id, 100000, 1, "t"), ConflictError);
    auto dash = c.analytics.dashboard();
    EXPECT_EQ(dash.incidentsTotal, (long)c.store.incidents.size());
    EXPECT_GT(dash.unitsTotal, 0);
    auto tl = c.analytics.timeline(2, 1, -1, -1);
    EXPECT_GE(tl.rangeIncidents, 1.0);
    EXPECT_FALSE(c.analytics.exportCsv("incidents").empty());
    auto lab = c.lab.sort("quick", ds::DynamicArray<long>{5, 3, 9, 1, 7});
    EXPECT_EQ(lab.result[0], 1);
    EXPECT_GT(lab.frames.size(), 2u);
    auto bench = c.lab.benchmark("search", 1000, 3);
    EXPECT_EQ(bench.size(), 3u);
}

TEST(Workflow, SimulationIsReproducibleAndReplayable) {
    Fixture f;
    auto& c = *f.core;
    ScenarioInput in; in.type = "earthquake"; in.incidentCount = 4; in.seed = 42; in.centerLat = 28.63; in.centerLon = 77.22;
    auto s1 = c.simulation.create(in, "t");
    std::size_t created = 0; std::string firstTitle;
    for (const auto& i : c.store.incidents.items()) if (i.scenarioId == s1.id) { ++created; if (firstTitle.empty()) firstTitle = i.title; EXPECT_TRUE(i.simulated); }
    EXPECT_EQ(created, 4u);
    auto step = c.simulation.step(s1.id, "t");
    EXPECT_TRUE(step.acted);
    auto replay = c.simulation.replay(s1.id);
    EXPECT_GT(replay.size(), 6u);
    for (std::size_t i = 1; i < replay.size(); ++i) EXPECT_LT(replay[i - 1].seq, replay[i].seq);
    // same seed => same incidents
    Fixture g;
    auto s2 = g.core->simulation.create(in, "t");
    std::string otherTitle;
    for (const auto& i : g.core->store.incidents.items()) if (i.scenarioId == s2.id && otherTitle.empty()) otherTitle = i.title;
    EXPECT_EQ(firstTitle, otherTitle);
}

TEST(Workflow, AuthAndRoles) {
    Fixture f;
    auto& a = f.core->auth;
    EXPECT_THROW(a.login("admin", "wrong"), AuthError);
    Session s = a.login("admin", "Nexa@2026");
    EXPECT_EQ(a.authenticate(s.token).username, std::string("admin"));
    Session v = a.login("viewer", "Viewer@2026");
    EXPECT_THROW(a.require(v, Permission::Allocate), ForbiddenError);
    EXPECT_NO_THROW(a.require(v, Permission::View));
    a.logout(s.token);
    EXPECT_THROW(a.authenticate(s.token), AuthError);
    EXPECT_THROW(a.createUser("x", "x", "short", model::Role::Viewer, "t"), ValidationError);
}
