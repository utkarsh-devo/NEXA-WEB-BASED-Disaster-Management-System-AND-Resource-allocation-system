#pragma once
// ============================================================================
// NEXA :: Repository interfaces (persistence boundary)
// Services depend only on these interfaces. Two implementations exist:
//   * MySqlRepositories   — production, Drogon ORM DbClient (src/repositories)
//   * InMemoryRepositories — tests and the `"storage": "memory"` demo mode
// Multi-record operations (allocation / release) are single methods so the
// MySQL implementation can run them inside ONE transaction (spec rule 11).
// ============================================================================
#include <memory>
#include <string>
#include "../models/Models.hpp"

namespace nexa::repo {

using namespace nexa::model;

struct ICategoryRepository {
    virtual ~ICategoryRepository() = default;
    virtual DynamicArray<IncidentCategory> findAll() = 0;
    virtual int insert(IncidentCategory& c) = 0;
};

struct IIncidentRepository {
    virtual ~IIncidentRepository() = default;
    virtual DynamicArray<Incident> findAll() = 0;
    virtual long insert(Incident& i) = 0;              // sets i.id
    virtual void update(const Incident& i) = 0;
    virtual void remove(long id) = 0;                  // hard delete (only for incidents without deployments)
};

struct IEventRepository {
    virtual ~IEventRepository() = default;
    virtual DynamicArray<IncidentEvent> findAll() = 0;
    virtual long insert(IncidentEvent& e) = 0;
};

struct IResourceRepository {
    virtual ~IResourceRepository() = default;
    virtual DynamicArray<ResourceType> findTypes() = 0;
    virtual int insertType(ResourceType& t) = 0;
    virtual DynamicArray<Resource> findAll() = 0;
    virtual long insert(Resource& r) = 0;
    virtual void update(const Resource& r) = 0;
    virtual DynamicArray<ResourceHistoryEntry> findHistory() = 0;
    virtual void insertHistory(const ResourceHistoryEntry& h) = 0;
};

struct IHospitalRepository {
    virtual ~IHospitalRepository() = default;
    virtual DynamicArray<Hospital> findAll() = 0;
    virtual long insert(Hospital& h) = 0;
    virtual void update(const Hospital& h) = 0;
    virtual void remove(long id) = 0;
    virtual DynamicArray<BedReservation> findReservations() = 0;
    /// Transactional: hospital bed counters + reservation row together.
    virtual long commitReservation(const Hospital& h, BedReservation& r, const IncidentEvent* ev) = 0;
    virtual void commitRelease(const Hospital& h, const BedReservation& r) = 0;
};

struct IRequestRepository {
    virtual ~IRequestRepository() = default;
    virtual DynamicArray<EmergencyRequest> findAll() = 0;
    virtual long insert(EmergencyRequest& r) = 0;
    virtual void update(const EmergencyRequest& r) = 0;
};

struct IDeploymentRepository {
    virtual ~IDeploymentRepository() = default;
    virtual DynamicArray<Deployment> findAll() = 0;
    /// ONE transaction: deployment + deployment_resources + resource availability
    /// + resource_availability log + incident events + optional request update.
    virtual long commitAllocation(Deployment& d, const DynamicArray<Resource>& updatedResources,
                                  DynamicArray<IncidentEvent>& events, const EmergencyRequest* request,
                                  const Incident* incident) = 0;
    /// ONE transaction: deployment status + restored resource availability + events.
    virtual void commitRelease(const Deployment& d, const DynamicArray<Resource>& updatedResources,
                               DynamicArray<IncidentEvent>& events, const Incident* incident) = 0;
};

struct ILocationRepository {
    virtual ~ILocationRepository() = default;
    virtual DynamicArray<Location> findLocations() = 0;
    virtual DynamicArray<Road> findRoads() = 0;
    virtual long insertLocation(Location& l) = 0;
    virtual long insertRoad(Road& r) = 0;
    virtual void updateRoad(const Road& r) = 0;
};

struct IUserRepository {
    virtual ~IUserRepository() = default;
    virtual DynamicArray<User> findAll() = 0;
    virtual long insert(User& u) = 0;
    virtual void update(const User& u) = 0;
};

struct ISimulationRepository {
    virtual ~ISimulationRepository() = default;
    virtual DynamicArray<SimulationScenario> findScenarios() = 0;
    virtual long insertScenario(SimulationScenario& s) = 0;
    virtual DynamicArray<SimulationEvent> findEvents(long scenarioId) = 0;
    virtual long insertEvent(SimulationEvent& e) = 0;
};

struct ILogRepository {
    virtual ~ILogRepository() = default;
    virtual void insert(SystemLog& l) = 0;
    virtual DynamicArray<SystemLog> recent(int limit) = 0;
};

/// All repositories, handed to the service layer as one bundle.
struct RepositoryBundle {
    std::shared_ptr<ICategoryRepository> categories;
    std::shared_ptr<IIncidentRepository> incidents;
    std::shared_ptr<IEventRepository> events;
    std::shared_ptr<IResourceRepository> resources;
    std::shared_ptr<IHospitalRepository> hospitals;
    std::shared_ptr<IRequestRepository> requests;
    std::shared_ptr<IDeploymentRepository> deployments;
    std::shared_ptr<ILocationRepository> locations;
    std::shared_ptr<IUserRepository> users;
    std::shared_ptr<ISimulationRepository> simulations;
    std::shared_ptr<ILogRepository> logs;
};

} // namespace nexa::repo
