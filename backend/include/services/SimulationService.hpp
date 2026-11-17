#pragma once
// ============================================================================
// NEXA :: §8.1 Disaster Simulation + §8.5 Scenario Replay
// Scenarios are generated with a seeded linear congruential generator, so
// the same seed reproduces the same incidents (auditable & reproducible).
// Every generated incident is flagged `simulated` — never real data.
// Each step of the response (incident, priority, selection, route,
// deployment, resolution) is written to simulation_events for replay.
// ============================================================================
#include "ServiceContext.hpp"
#include "IncidentService.hpp"
#include "RequestService.hpp"
#include "AllocationService.hpp"
#include "PriorityEngine.hpp"
#include "NetworkService.hpp"

namespace nexa::svc {

struct ScenarioInput {
    std::string name, type = "earthquake";   // earthquake | flood | industrial_fire | cyclone | multi
    double centerLat = 28.6139, centerLon = 77.2090;
    double radiusKm = 6;
    int incidentCount = 6;
    unsigned seed = 0;                        // 0 => derived from time
};

struct StepResult {
    bool acted = false;
    std::string action;          // DEPLOYED | RESOLVED | IDLE
    long incidentId = 0;
    std::string message;
    Deployment deployment;
    DynamicArray<SimulationEvent> events;
};

class SimulationService {
public:
    SimulationService(ServiceContext& c, IncidentService& i, RequestService& r, AllocationService& a, PriorityEngine& p, NetworkService& n)
        : ctx_(c), incidents_(i), requests_(r), allocation_(a), priority_(p), network_(n) {}

    SimulationScenario create(const ScenarioInput& in, const std::string& actor);
    /// Dispatch to the highest-priority open incident of the scenario.
    StepResult step(long scenarioId, const std::string& actor);
    /// Complete deployments of the oldest ACTIVE scenario incident and resolve it.
    StepResult resolveNext(long scenarioId, const std::string& actor);
    DynamicArray<SimulationScenario> list();
    DynamicArray<SimulationEvent> replay(long scenarioId);
    SimulationScenario get(long id);

private:
    ServiceContext& ctx_;
    IncidentService& incidents_;
    RequestService& requests_;
    AllocationService& allocation_;
    PriorityEngine& priority_;
    NetworkService& network_;
    Registry<SimulationScenario> scenarios_;
    bool loaded_ = false;
    void ensureLoaded();
    SimulationEvent record(long scenarioId, const std::string& type, const std::string& summary, long entityId, double lat, double lon);
    int nextSeq(long scenarioId);
};

} // namespace nexa::svc
