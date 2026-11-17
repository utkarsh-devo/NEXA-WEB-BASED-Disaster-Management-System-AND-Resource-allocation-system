#pragma once
// ============================================================================
// NEXA :: Module 9 — Resource Allocation & Deployment Tracking
// Validation (availability, units, capability, no conflicting simultaneous
// deployment via the per-resource IntervalTree) -> route per resource ->
// ONE repository transaction -> in-memory structures (MultiLinkedList,
// SparseMatrix, IntervalTree, Registry, LinkedList timeline, heap).
// If the transaction fails nothing in memory is touched.
// ============================================================================
#include "ServiceContext.hpp"
#include "RankingService.hpp"
#include "NetworkService.hpp"
#include "PriorityEngine.hpp"
#include "IncidentService.hpp"
#include "ResourceService.hpp"

namespace nexa::svc {

struct AllocationItem { long resourceId = 0; int units = 1; };

struct AllocationRequest {
    long incidentId = 0;
    long requestId = 0;                    // optional emergency request being fulfilled
    DynamicArray<AllocationItem> items;    // empty => automatic selection from the ranking
    int autoUnits = 0;                     // automatic mode: units wanted (0 = remaining requirement)
    std::string category;                  // automatic mode: optional category filter
    std::string notes;
};

struct MatrixCell { long incidentId = 0, resourceId = 0; int units = 0; std::string incidentCode, resourceCode; };

struct AllocationMatrixView {
    DynamicArray<MatrixCell> cells;
    std::size_t rows = 0, cols = 0;
    double density = 0;
    std::size_t sparseBytes = 0, denseBytes = 0;
};

class AllocationService {
public:
    AllocationService(ServiceContext& c, RankingService& r, NetworkService& n, PriorityEngine& p, IncidentService& i, ResourceService& rs, AuditService& a)
        : ctx_(c), ranking_(r), network_(n), priority_(p), incidents_(i), resources_(rs), audit_(a) {}

    Deployment allocate(const AllocationRequest& req, const std::string& actor);
    Deployment complete(long deploymentId, const std::string& actor);
    Deployment cancel(long deploymentId, const std::string& actor, const std::string& reason);
    /// Moves one resource from an active deployment to another incident.
    Deployment reassign(long resourceId, long fromDeploymentId, long toIncidentId, const std::string& actor);

    DynamicArray<Deployment> list(const std::string& status);
    Deployment get(long id);
    AllocationMatrixView matrix();

    /// Rebuild helpers used by the bootstrapper.
    void indexDeployment(const Deployment& d);

private:
    ServiceContext& ctx_;
    RankingService& ranking_;
    NetworkService& network_;
    PriorityEngine& priority_;
    IncidentService& incidents_;
    ResourceService& resources_;
    AuditService& audit_;

    Deployment release(long deploymentId, DeploymentStatus finalStatus, const std::string& actor, const std::string& reason, long onlyResourceId = 0);
};

} // namespace nexa::svc
