#pragma once
// ============================================================================
// NEXA :: Module 5 — Priority & Emergency Processing Engine
// Maintains the IndexedPriorityQueue (binary max-heap + position hash) of
// every open incident. Scores follow the documented formula in
// EngineConfig.hpp and are recomputed whenever an input changes (new
// incident, status change, deployment, request) or on demand (aging).
// ============================================================================
#include "ServiceContext.hpp"

namespace nexa::svc {

struct PriorityBreakdown {
    double severity = 0, affected = 0, unmet = 0, age = 0, status = 0, timeSensitivity = 0;
    double total = 0;
};

struct PriorityQueueRow {
    long incidentId = 0;
    std::string code, title, status;
    int severity = 0;
    double priority = 0;
    long createdAt = 0;
    int rank = 0;
    PriorityBreakdown breakdown;
};

class PriorityEngine {
public:
    explicit PriorityEngine(ServiceContext& c) : ctx_(c) {}

    PriorityBreakdown compute(const Incident& i, long now) const;
    static bool isOpen(IncidentStatus s) { return s == IncidentStatus::Pending || s == IncidentStatus::Active || s == IncidentStatus::Contained; }

    /// Recomputes one incident and repositions it in the heap (or removes it if closed). O(log n)
    double refresh(long incidentId);
    /// Recomputes every open incident (aging). O(n log n)
    void refreshAll();

    /// Full processing order (non-destructive snapshot).
    DynamicArray<PriorityQueueRow> orderedQueue(std::size_t limit = 0);
    /// Level-order heap array, for the heap visualisation.
    DynamicArray<PriorityQueueRow> heapArray();
    /// Highest-priority open incident id, or 0.
    long peekNext();

    PriorityWeights weights() const { return ctx_.config.priority; }
    void setWeights(const PriorityWeights& w);

private:
    ServiceContext& ctx_;
    PriorityQueueRow row(long id, double p);
};

} // namespace nexa::svc
