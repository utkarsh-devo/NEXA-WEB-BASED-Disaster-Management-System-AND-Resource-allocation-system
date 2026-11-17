#include "controllers/JsonMappers.hpp"
#include "utilities/Strings.hpp"

namespace nexa::api {

namespace {
Json::Value obj() { return Json::Value(Json::objectValue); }
Json::Value list(const std::string& csv) { Json::Value a(Json::arrayValue); for (const auto& x : util::splitList(csv)) a.append(x); return a; }
Json::Value time(long epoch) { return epoch > 0 ? Json::Value(util::isoTime(epoch)) : Json::Value(Json::nullValue); }
Json::Value stats(const ds::DynamicArray<LabStat>& s) { Json::Value a(Json::arrayValue); for (const auto& x : s) { Json::Value o = obj(); o["key"] = x.key; o["value"] = x.value; a.append(o); } return a; }
}

Json::Value toJson(const std::string& s) { return Json::Value(s); }
Json::Value toJson(long v) { return J(v); }

Json::Value toJson(const IncidentCategory& c) {
    Json::Value o = obj(); o["id"] = c.id; o["parentId"] = c.parentId; o["name"] = c.name; o["code"] = c.code;
    o["timeSensitivity"] = c.timeSensitivity; o["defaultCapabilities"] = list(c.defaultCapabilities); return o;
}
Json::Value toJson(const CategoryNode& c) {
    Json::Value o = obj(); o["id"] = c.id; o["parentId"] = c.parentId; o["depth"] = c.depth; o["name"] = c.name; o["code"] = c.code;
    o["timeSensitivity"] = c.timeSensitivity; o["openIncidents"] = J(c.openIncidents); return o;
}
Json::Value toJson(const Incident& i) {
    Json::Value o = obj();
    o["id"] = J(i.id); o["code"] = i.code; o["title"] = i.title; o["description"] = i.description;
    o["categoryId"] = i.categoryId; o["categoryName"] = i.categoryName; o["categoryPath"] = i.categoryPath;
    o["severity"] = i.severity; o["severityLabel"] = severityLabel(i.severity);
    o["lat"] = i.lat; o["lon"] = i.lon; o["locationName"] = i.locationName;
    o["status"] = toString(i.status); o["affectedPeople"] = J(i.affectedPeople);
    o["requiredCapabilities"] = list(i.requiredCapabilities); o["requiredUnits"] = i.requiredUnits; o["assignedUnits"] = i.assignedUnits;
    o["reportedBy"] = i.reportedBy; o["reporterContact"] = i.reporterContact;
    o["createdAt"] = time(i.createdAt); o["updatedAt"] = time(i.updatedAt); o["resolvedAt"] = time(i.resolvedAt);
    o["createdEpoch"] = J(i.createdAt);
    o["priorityScore"] = i.priorityScore; o["simulated"] = i.simulated; o["scenarioId"] = J(i.scenarioId);
    return o;
}
Json::Value toJson(const IncidentEvent& e) {
    Json::Value o = obj(); o["id"] = J(e.id); o["incidentId"] = J(e.incidentId); o["type"] = e.type; o["message"] = e.message;
    o["actor"] = e.actor; o["createdAt"] = time(e.createdAt); return o;
}
Json::Value toJson(const PriorityBreakdown& b) {
    Json::Value o = obj(); o["severity"] = b.severity; o["affected"] = b.affected; o["unmet"] = b.unmet; o["age"] = b.age;
    o["status"] = b.status; o["timeSensitivity"] = b.timeSensitivity; o["total"] = b.total; return o;
}
Json::Value toJson(const IncidentDetail& d) {
    Json::Value o = toJson(d.incident);
    o["timeline"] = arr(d.timeline); o["assigned"] = arr(d.assigned); o["deploymentIds"] = arr(d.deploymentIds);
    o["requests"] = arr(d.requests); o["priority"] = toJson(d.priority);
    return o;
}
Json::Value toJson(const QueryStats& s) {
    Json::Value o = obj(); o["index"] = s.index; o["sortAlgorithm"] = s.sortAlgorithm; o["comparisons"] = J(s.comparisons);
    o["scanned"] = J(s.scanned); o["microseconds"] = s.microseconds; return o;
}
Json::Value toJson(const ResourceType& t) {
    Json::Value o = obj(); o["id"] = t.id; o["name"] = t.name; o["category"] = t.category; o["defaultCapabilities"] = list(t.defaultCapabilities); return o;
}
Json::Value toJson(const Resource& r) {
    Json::Value o = obj();
    o["id"] = J(r.id); o["code"] = r.code; o["name"] = r.name; o["typeId"] = r.typeId; o["typeName"] = r.typeName; o["category"] = r.category;
    o["capacity"] = r.capacity; o["available"] = r.available; o["status"] = toString(r.status); o["lat"] = r.lat; o["lon"] = r.lon;
    o["capabilities"] = list(r.capabilities); o["organization"] = r.organization; o["updatedAt"] = time(r.updatedAt);
    o["utilisation"] = r.capacity ? (double)(r.capacity - r.available) / r.capacity : 0.0;
    return o;
}
Json::Value toJson(const ResourceHistoryEntry& h) {
    Json::Value o = obj(); o["resourceId"] = J(h.resourceId); o["action"] = h.action; o["detail"] = h.detail; o["at"] = time(h.at); return o;
}
Json::Value toJson(const ResourceDetail& d) {
    Json::Value o = toJson(d.resource);
    o["history"] = arr(d.history); o["activeDeployments"] = arr(d.activeDeployments);
    Json::Value c(Json::arrayValue);
    for (const auto& iv : d.commitments) { Json::Value x = obj(); x["deploymentId"] = J(iv.id); x["from"] = time(iv.start); x["until"] = iv.end >= svc::OPEN_END ? Json::Value("open") : time(iv.end); c.append(x); }
    o["commitments"] = c;
    return o;
}
Json::Value toJson(const ResourceListStats& s) {
    Json::Value o = obj(); o["index"] = s.index; o["sortAlgorithm"] = s.sortAlgorithm; o["comparisons"] = J(s.comparisons);
    o["scanned"] = J(s.scanned); o["microseconds"] = s.microseconds; return o;
}
Json::Value toJson(const Hospital& h) {
    Json::Value o = obj();
    o["id"] = J(h.id); o["name"] = h.name; o["category"] = h.category; o["lat"] = h.lat; o["lon"] = h.lon;
    o["totalBeds"] = h.totalBeds; o["availableBeds"] = h.availableBeds; o["icuBeds"] = h.icuBeds; o["icuAvailable"] = h.icuAvailable;
    o["capabilities"] = list(h.capabilities); o["contact"] = h.contact; o["address"] = h.address; o["updatedAt"] = time(h.updatedAt);
    o["occupancy"] = h.totalBeds ? (double)(h.totalBeds - h.availableBeds) / h.totalBeds : 0.0;
    return o;
}
Json::Value toJson(const BedReservation& r) {
    Json::Value o = obj(); o["id"] = J(r.id); o["hospitalId"] = J(r.hospitalId); o["incidentId"] = J(r.incidentId); o["beds"] = r.beds;
    o["from"] = time(r.from); o["until"] = time(r.until); o["released"] = r.released; return o;
}
Json::Value toJson(const HospitalCandidate& c) {
    Json::Value o = obj(); o["hospital"] = toJson(c.hospital); o["straightKm"] = c.straightKm; o["roadKm"] = c.roadKm; o["etaMinutes"] = c.etaMinutes;
    o["routable"] = c.routable; o["score"] = c.score; o["reason"] = c.reason; return o;
}
Json::Value toJson(const WindowLoad& w) {
    Json::Value o = obj(); o["from"] = time(w.from); o["until"] = time(w.until); o["reservedBeds"] = w.reservedBeds; o["reservations"] = w.reservations; return o;
}
Json::Value toJson(const EmergencyRequest& r) {
    Json::Value o = obj();
    o["id"] = J(r.id); o["incidentId"] = J(r.incidentId); o["assistanceType"] = r.assistanceType; o["description"] = r.description;
    o["urgency"] = r.urgency; o["peopleAffected"] = J(r.peopleAffected); o["status"] = toString(r.status); o["urgencyScore"] = r.urgencyScore;
    o["requestedBy"] = r.requestedBy; o["createdAt"] = time(r.createdAt); o["processedAt"] = time(r.processedAt); o["deploymentId"] = J(r.deploymentId);
    return o;
}
Json::Value toJson(const QueueSnapshot& q) { Json::Value o = obj(); o["fifo"] = arr(q.fifo); o["priority"] = arr(q.priority); return o; }
Json::Value toJson(const ProcessResult& r) {
    Json::Value o = obj(); o["processed"] = r.processed; o["queueUsed"] = r.queueUsed; o["message"] = r.message; o["allocated"] = r.allocated;
    if (r.processed) o["request"] = toJson(r.request);
    if (r.allocated) o["deployment"] = toJson(r.deployment);
    return o;
}
Json::Value toJson(const DeploymentResource& d) {
    Json::Value o = obj(); o["resourceId"] = J(d.resourceId); o["resourceCode"] = d.resourceCode; o["units"] = d.units; o["suitability"] = d.suitability;
    o["routeKm"] = d.routeKm; o["etaMinutes"] = d.etaMinutes; o["reason"] = d.reason; o["released"] = d.released; o["releasedAt"] = time(d.releasedAt); return o;
}
Json::Value toJson(const Deployment& d) {
    Json::Value o = obj(); o["id"] = J(d.id); o["incidentId"] = J(d.incidentId); o["requestId"] = J(d.requestId); o["status"] = toString(d.status);
    o["createdAt"] = time(d.createdAt); o["completedAt"] = time(d.completedAt); o["createdBy"] = d.createdBy; o["notes"] = d.notes;
    o["resources"] = arr(d.resources);
    return o;
}
Json::Value toJson(const AllocationMatrixView& m) {
    Json::Value o = obj(), cells(Json::arrayValue);
    for (const auto& c : m.cells) { Json::Value x = obj(); x["incidentId"] = J(c.incidentId); x["resourceId"] = J(c.resourceId); x["units"] = c.units; x["incidentCode"] = c.incidentCode; x["resourceCode"] = c.resourceCode; cells.append(x); }
    o["cells"] = cells; o["rows"] = J(m.rows); o["cols"] = J(m.cols); o["density"] = m.density; o["sparseBytes"] = J(m.sparseBytes); o["denseBytes"] = J(m.denseBytes);
    return o;
}
Json::Value toJson(const Location& l) { Json::Value o = obj(); o["id"] = J(l.id); o["name"] = l.name; o["kind"] = l.kind; o["lat"] = l.lat; o["lon"] = l.lon; return o; }
Json::Value toJson(const Road& r) {
    Json::Value o = obj(); o["id"] = J(r.id); o["fromId"] = J(r.fromId); o["toId"] = J(r.toId); o["name"] = r.name; o["distanceKm"] = r.distanceKm;
    o["travelMinutes"] = r.travelMinutes; o["penalty"] = r.penalty; o["blocked"] = r.blocked; return o;
}
Json::Value toJson(const User& u) {
    Json::Value o = obj(); o["id"] = J(u.id); o["username"] = u.username; o["displayName"] = u.displayName; o["role"] = toString(u.role);
    o["active"] = u.active; o["createdAt"] = time(u.createdAt); o["lastLoginAt"] = time(u.lastLoginAt); return o;
}
Json::Value toJson(const Session& s) {
    Json::Value o = obj(); o["token"] = s.token; o["userId"] = J(s.userId); o["username"] = s.username; o["displayName"] = s.displayName;
    o["role"] = toString(s.role); o["expiresAt"] = time(s.expiresAt);
    Json::Value perms(Json::arrayValue);
    for (int p = 0; p <= (int)Permission::ManageSettings; ++p) if (roleCan(s.role, (Permission)p)) perms.append(toString((Permission)p));
    o["permissions"] = perms;
    return o;
}
Json::Value toJson(const SimulationScenario& s) {
    Json::Value o = obj(); o["id"] = J(s.id); o["name"] = s.name; o["type"] = s.type; o["description"] = s.description; o["centerLat"] = s.centerLat;
    o["centerLon"] = s.centerLon; o["incidentCount"] = s.incidentCount; o["seed"] = (Json::UInt)s.seed; o["createdAt"] = time(s.createdAt); return o;
}
Json::Value toJson(const SimulationEvent& e) {
    Json::Value o = obj(); o["id"] = J(e.id); o["scenarioId"] = J(e.scenarioId); o["seq"] = e.seq; o["type"] = e.eventType; o["summary"] = e.summary;
    o["entityId"] = J(e.entityId); o["lat"] = e.lat; o["lon"] = e.lon; o["createdAt"] = time(e.createdAt); return o;
}
Json::Value toJson(const StepResult& s) {
    Json::Value o = obj(); o["acted"] = s.acted; o["action"] = s.action; o["incidentId"] = J(s.incidentId); o["message"] = s.message; o["events"] = arr(s.events);
    if (s.deployment.id) o["deployment"] = toJson(s.deployment);
    return o;
}
Json::Value toJson(const SystemLog& l) {
    Json::Value o = obj(); o["id"] = J(l.id); o["level"] = l.level; o["source"] = l.source; o["message"] = l.message; o["actor"] = l.actor; o["createdAt"] = time(l.createdAt); return o;
}
Json::Value toJson(const PriorityQueueRow& r) {
    Json::Value o = obj(); o["incidentId"] = J(r.incidentId); o["code"] = r.code; o["title"] = r.title; o["status"] = r.status; o["severity"] = r.severity;
    o["priority"] = r.priority; o["createdAt"] = time(r.createdAt); o["rank"] = r.rank; o["breakdown"] = toJson(r.breakdown); return o;
}
Json::Value toJson(const PriorityWeights& w) {
    Json::Value o = obj(); o["severity"] = w.severity; o["affected"] = w.affected; o["unmet"] = w.unmet; o["age"] = w.age; o["status"] = w.status;
    o["timeSensitivity"] = w.timeSensitivity; o["ageSaturationHours"] = w.ageSaturationHours; o["affectedSaturation"] = w.affectedSaturation; return o;
}
Json::Value toJson(const SuitabilityWeights& w) {
    Json::Value o = obj(); o["capability"] = w.capability; o["proximity"] = w.proximity; o["availability"] = w.availability; o["capacity"] = w.capacity;
    o["proximityScaleKm"] = w.proximityScaleKm; o["detourFactor"] = w.detourFactor; return o;
}
Json::Value toJson(const RankedResource& r) {
    Json::Value o = obj();
    o["resourceId"] = J(r.resourceId); o["code"] = r.code; o["name"] = r.name; o["category"] = r.category; o["capabilities"] = list(r.capabilities);
    o["available"] = r.available; o["capacity"] = r.capacity; o["lat"] = r.lat; o["lon"] = r.lon;
    o["straightKm"] = r.straightKm; o["roadKm"] = r.roadKm; o["etaMinutes"] = r.etaMinutes; o["routable"] = r.routable;
    Json::Value c = obj(); c["capability"] = r.capabilityMatch; c["proximity"] = r.proximity; c["availability"] = r.availability; c["capacity"] = r.capacityFit;
    o["components"] = c; o["score"] = r.score; o["matched"] = list(r.matchedCapabilities); o["missing"] = list(r.missingCapabilities); o["reason"] = r.reason;
    return o;
}
Json::Value toJson(const RankingResult& r) {
    Json::Value o = obj(); o["incidentId"] = J(r.incidentId); o["candidates"] = arr(r.candidates); o["considered"] = J(r.considered);
    o["rejectedUnavailable"] = J(r.rejectedUnavailable); o["rejectedCapability"] = J(r.rejectedCapability); o["medianRoadKm"] = r.medianRoadKm;
    o["algorithm"] = r.algorithm; o["comparisons"] = J(r.comparisons); o["microseconds"] = r.microseconds; o["unitsNeeded"] = r.unitsNeeded;
    o["requiredCapabilities"] = list(r.requiredCapabilities);
    return o;
}
Json::Value toJson(const SearchOutcome& s) {
    Json::Value o = obj(); o["method"] = s.method; o["found"] = s.found; o["results"] = arr(s.results); o["comparisons"] = J(s.comparisons);
    o["microseconds"] = s.microseconds; o["note"] = s.note; return o;
}
Json::Value toJson(const SpatialHit& h) {
    Json::Value o = obj(); o["kind"] = h.kind; o["id"] = J(h.id); o["name"] = h.name; o["category"] = h.category; o["status"] = h.status;
    o["lat"] = h.lat; o["lon"] = h.lon; o["distanceKm"] = h.distanceKm; o["available"] = h.available; o["capacity"] = h.capacity; return o;
}
Json::Value toJson(const SpatialQueryStats& s) {
    Json::Value o = obj(); o["method"] = s.method; o["candidatesExamined"] = J(s.candidatesExamined); o["microseconds"] = s.microseconds; o["results"] = J(s.results); return o;
}
Json::Value toJson(const Cluster& c) {
    Json::Value o = obj(); o["id"] = c.id; o["centerLat"] = c.centerLat; o["centerLon"] = c.centerLon; o["radiusKm"] = c.radiusKm;
    o["incidentIds"] = arr(c.incidentIds); o["maxSeverity"] = c.maxSeverity; return o;
}
Json::Value toJson(const ds::QuadCell& c) {
    Json::Value o = obj(); o["minLat"] = c.bounds.minLat; o["minLon"] = c.bounds.minLon; o["maxLat"] = c.bounds.maxLat; o["maxLon"] = c.bounds.maxLon;
    o["count"] = J(c.count); o["depth"] = c.depth; return o;
}
Json::Value toJson(const RouteStop& s) { Json::Value o = obj(); o["locationId"] = J(s.locationId); o["name"] = s.name; o["kind"] = s.kind; o["lat"] = s.lat; o["lon"] = s.lon; return o; }
Json::Value toJson(const RouteResult& r) {
    Json::Value o = obj(); o["found"] = r.found; o["algorithm"] = r.algorithm; o["weightMode"] = r.weightMode; o["stops"] = arr(r.stops); o["roadIds"] = arr(r.roadIds);
    o["totalWeight"] = r.totalWeight; o["distanceKm"] = r.distanceKm; o["minutes"] = r.minutes; o["snapStartKm"] = r.snapStartKm; o["snapEndKm"] = r.snapEndKm;
    o["settled"] = J(r.settled); o["relaxations"] = J(r.relaxations); o["microseconds"] = r.microseconds; o["note"] = r.note;
    return o;
}
Json::Value toJson(const GraphSummary& g) {
    Json::Value o = obj(); o["vertices"] = g.vertices; o["arcs"] = J(g.arcs); o["roads"] = g.roads; o["blockedRoads"] = g.blockedRoads;
    o["components"] = g.components; o["listBytes"] = J(g.listBytes); o["matrixBytes"] = J(g.matrixBytes); return o;
}
Json::Value toJson(const TraversalView& t) {
    Json::Value o = obj(); o["algorithm"] = t.algorithm; o["order"] = arr(t.order);
    Json::Value d(Json::arrayValue); for (int x : t.depth) d.append(x);
    o["depth"] = d; o["parentLocation"] = arr(t.parentLocation); o["reachable"] = t.reachable; o["unreachable"] = t.unreachable; o["operations"] = J(t.operations);
    return o;
}
Json::Value toJson(const MstView& m) {
    Json::Value o = obj(), edges(Json::arrayValue);
    for (const auto& e : m.edges) { Json::Value x = obj(); x["a"] = toJson(e.a); x["b"] = toJson(e.b); x["weight"] = e.weight; x["roadId"] = J(e.roadId); edges.append(x); }
    o["algorithm"] = m.algorithm; o["edges"] = edges; o["totalWeight"] = m.totalWeight; o["components"] = m.components; o["operations"] = J(m.operations); o["microseconds"] = m.microseconds;
    return o;
}
Json::Value toJson(const CountItem& c) { Json::Value o = obj(); o["label"] = c.label; o["count"] = J(c.count); o["value"] = c.value; return o; }
Json::Value toJson(const DashboardSummary& d) {
    Json::Value o = obj(), inc = obj(), res = obj(), req = obj(), beds = obj();
    inc["total"] = J(d.incidentsTotal); inc["open"] = J(d.incidentsOpen); inc["pending"] = J(d.pending); inc["active"] = J(d.active);
    inc["contained"] = J(d.contained); inc["resolved"] = J(d.resolved); inc["archived"] = J(d.archived); inc["critical"] = J(d.critical);
    inc["affectedPeopleOpen"] = J(d.affectedPeopleOpen);
    res["total"] = J(d.resourcesTotal); res["available"] = J(d.resourcesAvailable); res["deployed"] = J(d.resourcesDeployed);
    res["maintenance"] = J(d.resourcesMaintenance); res["inactive"] = J(d.resourcesInactive); res["unitsTotal"] = J(d.unitsTotal);
    res["unitsDeployed"] = J(d.unitsDeployed); res["utilisationPct"] = d.utilisationPct;
    req["pending"] = J(d.requestsPending); req["escalated"] = J(d.requestsEscalated);
    beds["total"] = J(d.bedsTotal); beds["available"] = J(d.bedsAvailable); beds["icuTotal"] = J(d.icuTotal); beds["icuAvailable"] = J(d.icuAvailable);
    o["incidents"] = inc; o["resources"] = res; o["requests"] = req; o["beds"] = beds;
    o["deploymentsActive"] = J(d.deploymentsActive); o["avgResponseMinutes"] = d.avgResponseMinutes;
    o["severity"] = arr(d.severity); o["categories"] = arr(d.categories); o["resourceUtilisation"] = arr(d.resourceUtilisation);
    o["topPriority"] = arr(d.topPriority); o["recentDeployments"] = arr(d.recentDeployments); o["recentActivity"] = arr(d.recentActivity);
    o["generatedAt"] = time(d.generatedAt);
    return o;
}
Json::Value toJson(const TimelineView& t) {
    Json::Value o = obj(), b(Json::arrayValue);
    for (const auto& x : t.buckets) { Json::Value v = obj(); v["start"] = time(x.start); v["incidents"] = J(x.incidents); v["deployments"] = J(x.deployments); v["resolved"] = J(x.resolved); b.append(v); }
    o["from"] = time(t.from); o["bucketSeconds"] = J(t.bucketSeconds); o["buckets"] = b;
    Json::Value q = obj(); q["fromBucket"] = J(t.queryFromBucket); q["toBucket"] = J(t.queryToBucket); q["incidents"] = t.rangeIncidents;
    q["deployments"] = t.rangeDeployments; q["peakIncidents"] = t.peakIncidents;
    o["rangeQuery"] = q;
    return o;
}
Json::Value toJson(const ResponseStats& r) {
    Json::Value o = obj(); o["samples"] = J(r.samples); o["averageMinutes"] = r.averageMinutes; o["medianMinutes"] = r.medianMinutes; o["p90Minutes"] = r.p90Minutes;
    o["fastestMinutes"] = r.fastestMinutes; o["slowestMinutes"] = r.slowestMinutes; o["resolvedCount"] = J(r.resolvedCount); o["averageResolutionHours"] = r.averageResolutionHours;
    return o;
}
Json::Value toJson(const ImportReport& r) { Json::Value o = obj(); o["imported"] = J(r.imported); o["failed"] = J(r.failed); o["errors"] = arr(r.errors); return o; }
Json::Value toJson(const LabOutput& l) {
    Json::Value o = obj();
    o["structure"] = l.structure; o["operation"] = l.operation;
    Json::Value nodes(Json::arrayValue);
    for (const auto& n : l.nodes) { Json::Value x = obj(); x["id"] = n.id; x["parent"] = n.parent; x["label"] = n.label; x["color"] = n.color; x["edge"] = n.edge; x["depth"] = n.depth; nodes.append(x); }
    o["nodes"] = nodes;
    Json::Value frames(Json::arrayValue);
    for (const auto& f : l.frames) { Json::Value x = obj(); x["values"] = arr(f.values); x["i"] = J(f.i); x["j"] = J(f.j); x["action"] = f.action; frames.append(x); }
    o["frames"] = frames;
    o["result"] = arr(l.result); o["log"] = arr(l.log); o["stats"] = stats(l.stats); o["complexity"] = stats(l.complexity);
    Json::Value table(Json::arrayValue);
    for (const auto& row : l.table) table.append(arr(row));
    o["table"] = table;
    return o;
}
Json::Value toJson(const BenchmarkRow& b) {
    Json::Value o = obj(); o["variant"] = b.variant; o["n"] = J(b.n); o["microseconds"] = b.microseconds; o["comparisons"] = b.comparisons;
    o["operations"] = b.operations; o["memoryBytes"] = b.memoryBytes; o["note"] = b.note; return o;
}
Json::Value toJson(const CatalogEntry& c) {
    Json::Value o = obj(); o["name"] = c.name; o["group"] = c.group; o["application"] = c.application; o["file"] = c.file; o["complexity"] = stats(c.complexity); return o;
}
Json::Value toJson(const ds::Trie::Match& m) { Json::Value o = obj(); o["word"] = m.word; o["ids"] = arr(m.ids); return o; }
Json::Value toJson(const ds::SuffixArray::Occurrence& oc) { Json::Value o = obj(); o["incidentId"] = J(oc.docId); o["offset"] = J(oc.offset); return o; }
Json::Value toJson(const BootstrapReport& b) {
    Json::Value o = obj(); o["categories"] = J(b.categories); o["resourceTypes"] = J(b.resourceTypes); o["locations"] = J(b.locations); o["roads"] = J(b.roads);
    o["resources"] = J(b.resources); o["hospitals"] = J(b.hospitals); o["incidents"] = J(b.incidents); o["events"] = J(b.events);
    o["requests"] = J(b.requests); o["deployments"] = J(b.deployments); o["users"] = J(b.users); o["milliseconds"] = b.milliseconds;
    return o;
}

} // namespace nexa::api
