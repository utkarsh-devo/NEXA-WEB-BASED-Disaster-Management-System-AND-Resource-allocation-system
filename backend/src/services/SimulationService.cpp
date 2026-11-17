#include "services/SimulationService.hpp"
#include "utilities/Geo.hpp"
#include "utilities/Validation.hpp"
#include "algorithms/Sorting.hpp"
#include <cmath>

namespace nexa::svc {

namespace {
/// Numerical Recipes LCG — deterministic for a given seed.
struct Lcg {
    unsigned long long s;
    explicit Lcg(unsigned seed) : s(seed ? seed : 1) {}
    unsigned next() { s = s * 6364136223846793005ULL + 1442695040888963407ULL; return (unsigned)(s >> 33); }
    double uniform() { return (double)next() / 2147483648.0; }          // [0,1)
    int range(int lo, int hi) { return lo + (int)(uniform() * (hi - lo + 1)); }
};

struct Template { int categoryId; const char* title; const char* description; const char* caps; int minSev, maxSev; };

const Template EARTHQUAKE[] = {
    {24, "Building collapse", "Partial collapse of a residential block after tremors; people reported trapped under debris", "rescue,heavy_lifting,medical", 4, 5},
    {11, "Structural damage cluster", "Multiple buildings cracked; residents evacuated to open grounds", "rescue,evacuation,medical", 3, 4},
    {21, "Post-quake fire", "Fire broke out after a gas cylinder burst in damaged shops", "fire_suppression,medical", 3, 5},
    {23, "Gas pipeline leak", "Strong smell of gas near a damaged pipeline; area being cordoned", "hazmat,evacuation", 3, 4},
};
const Template FLOOD[] = {
    {12, "Low-lying area flooded", "Water level rising rapidly; families stranded on rooftops", "water_rescue,evacuation,medical", 3, 5},
    {33, "Drinking water shortage", "Relief camp reports shortage of potable water and food packets", "food_water,shelter", 2, 3},
    {32, "Waterborne illness cases", "Cluster of diarrhoea cases reported at a relief camp", "medical", 2, 4},
    {14, "Embankment breach", "Section of embankment eroded; nearby colony at risk", "rescue,heavy_lifting,evacuation", 3, 5},
};
const Template INDUSTRIAL[] = {
    {22, "Factory fire", "Fire in a chemical storage unit; thick smoke over the industrial area", "fire_suppression,hazmat,medical", 4, 5},
    {23, "Toxic gas release", "Workers complain of breathing difficulty after a valve failure", "hazmat,evacuation,medical", 3, 5},
    {21, "Warehouse fire", "Fire spreading across adjoining warehouses", "fire_suppression", 3, 4},
};
const Template CYCLONE[] = {
    {13, "Storm damage", "High winds uprooted trees and damaged roofs; power lines down", "evacuation,shelter,rescue", 3, 4},
    {12, "Waterlogging", "Heavy rain caused severe waterlogging; vehicles stranded", "water_rescue,evacuation", 2, 4},
    {24, "Wall collapse", "Boundary wall collapsed onto parked vehicles; injuries reported", "rescue,medical", 3, 5},
};

void pick(const std::string& type, Lcg& rng, const Template*& out) {
    const Template* sets[] = {EARTHQUAKE, FLOOD, INDUSTRIAL, CYCLONE};
    const int sizes[] = {4, 4, 3, 3};
    int which = type == "flood" ? 1 : type == "industrial_fire" ? 2 : type == "cyclone" ? 3 : type == "multi" ? rng.range(0, 3) : 0;
    out = &sets[which][rng.range(0, sizes[which] - 1)];
}
} // namespace

void SimulationService::ensureLoaded() {
    if (loaded_) return;
    for (const auto& s : ctx_.repos.simulations->findScenarios()) scenarios_.upsert(s);
    loaded_ = true;
}

int SimulationService::nextSeq(long scenarioId) { return (int)ctx_.repos.simulations->findEvents(scenarioId).size() + 1; }

SimulationEvent SimulationService::record(long scenarioId, const std::string& type, const std::string& summary, long entityId, double lat, double lon) {
    SimulationEvent e;
    e.scenarioId = scenarioId; e.seq = nextSeq(scenarioId); e.eventType = type; e.summary = summary;
    e.entityId = entityId; e.lat = lat; e.lon = lon; e.createdAt = util::nowEpoch();
    ctx_.repos.simulations->insertEvent(e);
    ctx_.sink.publish(LiveEvent{"simulation.step", scenarioId, type, summary, lat, lon});
    return e;
}

SimulationScenario SimulationService::create(const ScenarioInput& in, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    ensureLoaded();
    static const char* types[] = {"earthquake", "flood", "industrial_fire", "cyclone", "multi"};
    validate::oneOf(in.type, types, 5, "Scenario type");
    validate::coordinates(in.centerLat, in.centerLon);
    validate::range(in.incidentCount, 1, 40, "Incident count");
    if (in.radiusKm <= 0 || in.radiusKm > 50) throw ValidationError("Radius must be between 0 and 50 km");
    SimulationScenario sc;
    sc.name = in.name.empty() ? std::string("Simulated ") + in.type : util::trim(in.name);
    sc.type = in.type; sc.centerLat = in.centerLat; sc.centerLon = in.centerLon; sc.incidentCount = in.incidentCount;
    sc.seed = in.seed ? in.seed : (unsigned)(util::nowEpoch() & 0x7fffffff);
    sc.description = "SIMULATED scenario (seed " + std::to_string(sc.seed) + ") — not real emergency information";
    sc.createdAt = util::nowEpoch();
    ctx_.repos.simulations->insertScenario(sc);
    scenarios_.upsert(sc);
    record(sc.id, "SCENARIO", sc.name + " started around (" + std::to_string(sc.centerLat).substr(0, 7) + ", " + std::to_string(sc.centerLon).substr(0, 7) + ")", sc.id, sc.centerLat, sc.centerLon);

    Lcg rng(sc.seed);
    for (int k = 0; k < in.incidentCount; ++k) {
        const Template* t = nullptr;
        pick(in.type, rng, t);
        // uniform point in a disc: r = R*sqrt(u), theta = 2*pi*v
        double r = in.radiusKm * std::sqrt(rng.uniform()), th = 2 * geo::PI * rng.uniform();
        double lat = in.centerLat + (r * std::cos(th)) / 111.195;
        double lon = in.centerLon + (r * std::sin(th)) / (111.195 * std::cos(geo::toRad(in.centerLat)));
        std::string locality = "Sector";
        if (ctx_.store.roads.vertexCount() > 0) {
            auto hit = ctx_.store.vertexKd.kNearest(lat, lon, 1);       // nearest named place for the title
            if (!hit.empty()) locality = ctx_.store.roads.vertex((int)hit[0].point.id).name;
        }
        IncidentInput ii;
        ii.title = std::string(t->title) + " near " + locality;
        ii.description = t->description;
        ii.categoryId = t->categoryId;
        ii.severity = rng.range(t->minSev, t->maxSev);
        ii.lat = lat; ii.lon = lon; ii.locationName = locality;
        ii.affectedPeople = rng.range(5, 60) * ii.severity * ii.severity;
        ii.requiredCapabilities = t->caps;
        ii.requiredUnits = ii.severity >= 4 ? 3 : (ii.severity == 3 ? 2 : 1);
        ii.reportedBy = "simulation";
        ii.simulated = true; ii.scenarioId = sc.id;
        ii.createdAt = util::nowEpoch() - rng.range(0, 40) * 60;
        if (!ctx_.store.categoryById.contains(ii.categoryId)) continue;
        Incident inc = incidents_.create(ii, actor);
        record(sc.id, "INCIDENT", inc.code + " " + inc.title + " (severity " + std::to_string(inc.severity) + ")", inc.id, inc.lat, inc.lon);
        record(sc.id, "PRIORITY", inc.code + " priority " + std::to_string((int)inc.priorityScore), inc.id, inc.lat, inc.lon);
        if (rng.uniform() < 0.6) {
            static const char* assist[] = {"medical", "rescue", "fire", "evacuation", "supplies", "shelter"};
            RequestInput rq; rq.incidentId = inc.id; rq.assistanceType = assist[rng.range(0, 5)];
            rq.urgency = inc.severity; rq.peopleAffected = inc.affectedPeople / 3; rq.description = "Simulated field request"; rq.requestedBy = "simulation";
            try { requests_.create(rq, actor); } catch (const NexaError&) {}
        }
    }
    return sc;
}

StepResult SimulationService::step(long scenarioId, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    ensureLoaded();
    if (!scenarios_.contains(scenarioId)) throw NotFoundError("Scenario not found");
    StepResult res;
    priority_.refreshAll();
    // walk the priority order; first scenario incident still lacking units gets resources
    for (const auto& row : priority_.orderedQueue(0)) {
        const Incident* inc = ctx_.store.incidents.get(row.incidentId);
        if (!inc || inc->scenarioId != scenarioId || inc->assignedUnits >= inc->requiredUnits) continue;
        Incident snapshot = *inc;
        res.events.pushBack(record(scenarioId, "PRIORITY", "Highest-priority unmet incident: " + snapshot.code + " (score " + std::to_string((int)row.priority) + ")", snapshot.id, snapshot.lat, snapshot.lon));
        AllocationRequest ar; ar.incidentId = snapshot.id; ar.notes = "Simulation step";
        try {
            Deployment d = allocation_.allocate(ar, actor);
            for (const auto& dr : d.resources) {
                const Resource* r = ctx_.store.resources.get(dr.resourceId);
                res.events.pushBack(record(scenarioId, "SELECTION", dr.resourceCode + " selected — " + dr.reason, dr.resourceId, r ? r->lat : 0, r ? r->lon : 0));
                RouteResult route = r ? network_.routeBetweenPoints(r->lat, r->lon, snapshot.lat, snapshot.lon, ds::WeightMode::Time) : RouteResult{};
                std::string path;
                for (std::size_t i = 0; i < route.stops.size(); ++i) { if (i) path += " → "; path += route.stops[i].name; }
                res.events.pushBack(record(scenarioId, "ROUTE", dr.resourceCode + (route.found ? ": " + path : ": no road path (straight-line estimate)"), dr.resourceId, snapshot.lat, snapshot.lon));
            }
            res.events.pushBack(record(scenarioId, "DEPLOYMENT", "Deployment #" + std::to_string(d.id) + " → " + snapshot.code, d.id, snapshot.lat, snapshot.lon));
            res.acted = true; res.action = "DEPLOYED"; res.incidentId = snapshot.id; res.deployment = d;
            res.message = "Deployed " + std::to_string(d.resources.size()) + " resource(s) to " + snapshot.code;
            return res;
        } catch (const NexaError& e) {
            res.events.pushBack(record(scenarioId, "SELECTION", snapshot.code + ": " + e.what(), snapshot.id, snapshot.lat, snapshot.lon));
        }
    }
    res.action = "IDLE";
    res.message = "No further allocation possible (all scenario incidents resourced or no suitable resources free)";
    return res;
}

StepResult SimulationService::resolveNext(long scenarioId, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    ensureLoaded();
    if (!scenarios_.contains(scenarioId)) throw NotFoundError("Scenario not found");
    StepResult res;
    const Incident* target = nullptr;
    for (const auto& i : ctx_.store.incidents.items())
        if (i.scenarioId == scenarioId && i.status == IncidentStatus::Active && (!target || i.createdAt < target->createdAt)) target = &i;
    if (!target) { res.action = "IDLE"; res.message = "No active scenario incident to resolve"; return res; }
    long incId = target->id;
    DynamicArray<long> deps;
    for (const auto& d : ctx_.store.deployments.items()) if (d.incidentId == incId && d.status == DeploymentStatus::Active) deps.pushBack(d.id);
    for (long d : deps) allocation_.complete(d, actor);
    Incident done = incidents_.changeStatus(incId, IncidentStatus::Resolved, actor, "resolved in simulation");
    res.events.pushBack(record(scenarioId, "RESOLUTION", done.code + " resolved; " + std::to_string(deps.size()) + " deployment(s) completed", incId, done.lat, done.lon));
    res.acted = true; res.action = "RESOLVED"; res.incidentId = incId; res.message = done.code + " resolved";
    return res;
}

DynamicArray<SimulationScenario> SimulationService::list() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    ensureLoaded();
    DynamicArray<SimulationScenario> out;
    scenarios_.forEachOrdered([&](const SimulationScenario& s) { out.pushBack(s); });
    return out;
}

SimulationScenario SimulationService::get(long id) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    ensureLoaded();
    const SimulationScenario* s = scenarios_.get(id);
    if (!s) throw NotFoundError("Scenario not found");
    return *s;
}

DynamicArray<SimulationEvent> SimulationService::replay(long scenarioId) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    get(scenarioId);
    auto ev = ctx_.repos.simulations->findEvents(scenarioId);
    algo::mergeSort(ev, [](const SimulationEvent& a, const SimulationEvent& b) { return a.seq < b.seq; });
    return ev;
}

} // namespace nexa::svc
