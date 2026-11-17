#pragma once
// ============================================================================
// NEXA :: composition root of the service layer.
// Owns the configuration, repositories, the OperationalStore and every
// service, wired in dependency order. Drogon controllers receive a
// reference to this object; tests construct it with in-memory repositories.
// ============================================================================
#include <memory>
#include "ServiceContext.hpp"
#include "PriorityEngine.hpp"
#include "SpatialService.hpp"
#include "NetworkService.hpp"
#include "IncidentService.hpp"
#include "ResourceService.hpp"
#include "RankingService.hpp"
#include "AllocationService.hpp"
#include "RequestService.hpp"
#include "HospitalService.hpp"
#include "AnalyticsService.hpp"
#include "DsaLabService.hpp"
#include "AuthService.hpp"
#include "SimulationService.hpp"

namespace nexa::svc {

struct BootstrapReport {
    std::size_t categories = 0, resourceTypes = 0, locations = 0, roads = 0, resources = 0, hospitals = 0, incidents = 0,
                events = 0, requests = 0, deployments = 0, users = 0;
    double milliseconds = 0;
};

class NexaCore {
public:
    NexaCore(const EngineConfig& cfg, repo::RepositoryBundle repos, IEventSink& sink)
        : config(cfg), repos(std::move(repos)), ctx{config, this->repos, store, sink},
          audit(ctx), priority(ctx), spatial(ctx), network(ctx, spatial),
          incidents(ctx, priority, spatial, audit), resources(ctx, spatial, audit),
          ranking(ctx, spatial, network),
          allocation(ctx, ranking, network, priority, incidents, resources, audit),
          requests(ctx, priority, allocation, incidents, audit),
          hospitals(ctx, spatial, network, incidents, audit),
          analytics(ctx, priority, incidents, resources, spatial),
          auth(ctx, audit),
          simulation(ctx, incidents, requests, allocation, priority, network) {}

    NexaCore(const NexaCore&) = delete;
    NexaCore& operator=(const NexaCore&) = delete;

    /// Rebuilds every in-memory structure from the repositories (start-up / reload).
    BootstrapReport bootstrap();
    /// Inserts the simulated Delhi-NCR demo dataset when the database is empty.
    /// Returns false when data already exists (nothing is changed).
    bool seedDemoData();

    EngineConfig config;
    repo::RepositoryBundle repos;
    OperationalStore store;
    ServiceContext ctx;

    AuditService audit;
    PriorityEngine priority;
    SpatialService spatial;
    NetworkService network;
    IncidentService incidents;
    ResourceService resources;
    RankingService ranking;
    AllocationService allocation;
    RequestService requests;
    HospitalService hospitals;
    AnalyticsService analytics;
    DsaLabService lab;
    AuthService auth;
    SimulationService simulation;
};

} // namespace nexa::svc
