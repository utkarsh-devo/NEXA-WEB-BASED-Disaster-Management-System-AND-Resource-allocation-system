#pragma once
// ============================================================================
// NEXA :: model/view -> JSON (jsoncpp). Field names are camelCase and match
// the React frontend (frontend/src/services/api.js).
// ============================================================================
#include <json/json.h>
#include "services/NexaCore.hpp"

namespace nexa::api {

using namespace nexa::model;
using namespace nexa::svc;

inline Json::Value J(long v) { return Json::Value((Json::Int64)v); }
inline Json::Value J(int v) { return Json::Value(v); }
inline Json::Value J(std::size_t v) { return Json::Value((Json::UInt64)v); }
inline Json::Value J(double v) { return Json::Value(v); }
inline Json::Value J(bool v) { return Json::Value(v); }
inline Json::Value J(const std::string& v) { return Json::Value(v); }
inline Json::Value J(const char* v) { return Json::Value(v); }

Json::Value toJson(const IncidentCategory& c);
Json::Value toJson(const CategoryNode& c);
Json::Value toJson(const Incident& i);
Json::Value toJson(const IncidentEvent& e);
Json::Value toJson(const IncidentDetail& d);
Json::Value toJson(const QueryStats& s);
Json::Value toJson(const ResourceType& t);
Json::Value toJson(const Resource& r);
Json::Value toJson(const ResourceHistoryEntry& h);
Json::Value toJson(const ResourceDetail& d);
Json::Value toJson(const ResourceListStats& s);
Json::Value toJson(const Hospital& h);
Json::Value toJson(const BedReservation& r);
Json::Value toJson(const HospitalCandidate& c);
Json::Value toJson(const WindowLoad& w);
Json::Value toJson(const EmergencyRequest& r);
Json::Value toJson(const QueueSnapshot& q);
Json::Value toJson(const ProcessResult& r);
Json::Value toJson(const DeploymentResource& d);
Json::Value toJson(const Deployment& d);
Json::Value toJson(const AllocationMatrixView& m);
Json::Value toJson(const Location& l);
Json::Value toJson(const Road& r);
Json::Value toJson(const User& u);
Json::Value toJson(const Session& s);
Json::Value toJson(const SimulationScenario& s);
Json::Value toJson(const SimulationEvent& e);
Json::Value toJson(const StepResult& s);
Json::Value toJson(const SystemLog& l);
Json::Value toJson(const PriorityBreakdown& b);
Json::Value toJson(const PriorityQueueRow& r);
Json::Value toJson(const PriorityWeights& w);
Json::Value toJson(const SuitabilityWeights& w);
Json::Value toJson(const RankedResource& r);
Json::Value toJson(const RankingResult& r);
Json::Value toJson(const SearchOutcome& s);
Json::Value toJson(const SpatialHit& h);
Json::Value toJson(const SpatialQueryStats& s);
Json::Value toJson(const Cluster& c);
Json::Value toJson(const ds::QuadCell& c);
Json::Value toJson(const RouteStop& s);
Json::Value toJson(const RouteResult& r);
Json::Value toJson(const GraphSummary& g);
Json::Value toJson(const TraversalView& t);
Json::Value toJson(const MstView& m);
Json::Value toJson(const CountItem& c);
Json::Value toJson(const DashboardSummary& d);
Json::Value toJson(const TimelineView& t);
Json::Value toJson(const ResponseStats& r);
Json::Value toJson(const ImportReport& r);
Json::Value toJson(const LabOutput& o);
Json::Value toJson(const BenchmarkRow& b);
Json::Value toJson(const CatalogEntry& c);
Json::Value toJson(const ds::Trie::Match& m);
Json::Value toJson(const ds::SuffixArray::Occurrence& o);
Json::Value toJson(const BootstrapReport& b);
Json::Value toJson(const std::string& s);
Json::Value toJson(long v);

template <typename T>
Json::Value arr(const ds::DynamicArray<T>& items) {
    Json::Value a(Json::arrayValue);
    for (const auto& x : items) a.append(toJson(x));
    return a;
}

} // namespace nexa::api
