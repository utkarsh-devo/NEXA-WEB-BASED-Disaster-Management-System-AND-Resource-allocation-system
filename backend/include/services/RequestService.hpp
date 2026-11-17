#pragma once
// ============================================================================
// NEXA :: Module 4 — Emergency Request Management
// Two explicit processing disciplines (spec: "distinguish ordinary FIFO
// request processing from priority-based emergency processing"):
//   * ds::Queue<long>            — ordinary requests, strict arrival order
//   * ds::IndexedPriorityQueue   — escalated requests, highest urgency first
// processNext() serves the priority queue first, then the FIFO queue.
// ============================================================================
#include "ServiceContext.hpp"
#include "AllocationService.hpp"
#include "PriorityEngine.hpp"
#include "IncidentService.hpp"

namespace nexa::svc {

struct RequestInput {
    long incidentId = 0;
    std::string assistanceType, description, requestedBy;
    int urgency = 3;
    long peopleAffected = 0;
};

struct ProcessResult {
    bool processed = false;
    std::string queueUsed;        // priority | fifo | none
    EmergencyRequest request;
    bool allocated = false;
    Deployment deployment;
    std::string message;
};

struct QueueSnapshot {
    DynamicArray<EmergencyRequest> fifo;      // front -> rear
    DynamicArray<EmergencyRequest> priority;  // processing order
};

class RequestService {
public:
    RequestService(ServiceContext& c, PriorityEngine& p, AllocationService& a, IncidentService& i, AuditService& au)
        : ctx_(c), priority_(p), allocation_(a), incidents_(i), audit_(au) {}

    EmergencyRequest create(const RequestInput& in, const std::string& actor);
    EmergencyRequest escalate(long id, const std::string& actor);
    EmergencyRequest cancel(long id, const std::string& actor);
    /// mode: auto (priority first) | fifo | priority
    ProcessResult processNext(const std::string& mode, const std::string& actor);

    DynamicArray<EmergencyRequest> list(const std::string& status);
    EmergencyRequest get(long id);
    DynamicArray<std::string> history(long id);
    QueueSnapshot queues();
    double urgencyScore(const EmergencyRequest& r);

    /// Bootstrap: put a loaded request back into the correct queue.
    void enqueue(const EmergencyRequest& r);

private:
    ServiceContext& ctx_;
    PriorityEngine& priority_;
    AllocationService& allocation_;
    IncidentService& incidents_;
    AuditService& audit_;
};

} // namespace nexa::svc
