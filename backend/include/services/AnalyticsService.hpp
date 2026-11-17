#pragma once
// ============================================================================
// NEXA :: Module 1 (Command Dashboard) + Module 11 (Operational Analytics)
// Everything is computed from live/stored records — never hard-coded.
//  * Segment Tree    : time-bucketed incident & deployment counts, range sum/max
//  * Counting Sort   : severity distribution (bounded key 1..5)
//  * Quick Sort      : category ranking
//  * Median Search   : median response time; quick-select for P90
//  * K-ary Tree      : category roll-up to parent categories
//  * Sparse Matrix   : allocation matrix statistics
// ============================================================================
#include "ServiceContext.hpp"
#include "PriorityEngine.hpp"
#include "IncidentService.hpp"
#include "ResourceService.hpp"
#include "SpatialService.hpp"

namespace nexa::svc {

struct CountItem { std::string label; long count = 0; double value = 0; };

struct DashboardSummary {
    long incidentsTotal = 0, incidentsOpen = 0, pending = 0, active = 0, contained = 0, resolved = 0, archived = 0, critical = 0;
    long resourcesTotal = 0, resourcesAvailable = 0, resourcesDeployed = 0, resourcesMaintenance = 0, resourcesInactive = 0;
    long unitsTotal = 0, unitsDeployed = 0;
    long requestsPending = 0, requestsEscalated = 0;
    long bedsTotal = 0, bedsAvailable = 0, icuTotal = 0, icuAvailable = 0;
    long deploymentsActive = 0, affectedPeopleOpen = 0;
    double avgResponseMinutes = 0, utilisationPct = 0;
    DynamicArray<CountItem> severity;            // 1..5
    DynamicArray<CountItem> categories;          // top-level categories, open incidents
    DynamicArray<CountItem> resourceUtilisation; // per resource category, value = %
    DynamicArray<PriorityQueueRow> topPriority;
    DynamicArray<Deployment> recentDeployments;
    DynamicArray<IncidentEvent> recentActivity;
    long generatedAt = 0;
};

struct TimelineBucket { long start = 0; long incidents = 0; long deployments = 0; long resolved = 0; };
struct TimelineView {
    long from = 0, bucketSeconds = 0;
    DynamicArray<TimelineBucket> buckets;
    double rangeIncidents = 0, rangeDeployments = 0, peakIncidents = 0;   // segment tree answers for [qFrom,qTo]
    long queryFromBucket = 0, queryToBucket = 0;
};

struct ResponseStats {
    long samples = 0;
    double averageMinutes = 0, medianMinutes = 0, p90Minutes = 0, fastestMinutes = 0, slowestMinutes = 0;
    long resolvedCount = 0;
    double averageResolutionHours = 0;
};

struct ImportReport { long imported = 0, failed = 0; DynamicArray<std::string> errors; };

class AnalyticsService {
public:
    AnalyticsService(ServiceContext& c, PriorityEngine& p, IncidentService& i, ResourceService& r, SpatialService& s)
        : ctx_(c), priority_(p), incidents_(i), resources_(r), spatial_(s) {}

    DashboardSummary dashboard();
    TimelineView timeline(int days, int bucketHours, int queryFrom, int queryTo);
    ResponseStats responseStats();
    DynamicArray<CountItem> categoryDistribution(bool includeClosed);
    DynamicArray<CountItem> hospitalUtilisation();
    DynamicArray<CountItem> statusDistribution();

    std::string exportCsv(const std::string& entity);     // incidents | resources | deployments | hospitals
    ImportReport importCsv(const std::string& entity, const std::string& csv, const std::string& actor);

private:
    ServiceContext& ctx_;
    PriorityEngine& priority_;
    IncidentService& incidents_;
    ResourceService& resources_;
    SpatialService& spatial_;
    DynamicArray<double> responseMinutes();
};

} // namespace nexa::svc
