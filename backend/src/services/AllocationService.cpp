#include "services/AllocationService.hpp"
#include "algorithms/Sorting.hpp"
#include <cmath>
#include <cstdio>

namespace nexa::svc {

namespace {
std::string fmt1(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); return b; }
}

void AllocationService::indexDeployment(const Deployment& d) {
    auto& s = ctx_.store;
    if (d.status != DeploymentStatus::Active) return;
    for (const auto& dr : d.resources) {
        if (dr.released) continue;
        s.links.link(d.incidentId, dr.resourceId, d.id);
        s.unitsMatrix.add(d.incidentId, dr.resourceId, dr.units);
        s.windowsOf(s.commitments, dr.resourceId).insert(ds::Interval{d.createdAt, OPEN_END, d.id});
    }
}

Deployment AllocationService::allocate(const AllocationRequest& req, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto& s = ctx_.store;
    const Incident* incPtr = s.incidents.get(req.incidentId);
    if (!incPtr) throw NotFoundError("Incident " + std::to_string(req.incidentId) + " not found");
    if (!PriorityEngine::isOpen(incPtr->status)) throw ConflictError("Cannot deploy to a " + std::string(toString(incPtr->status)) + " incident");
    Incident inc = *incPtr;
    const EmergencyRequest* reqPtr = nullptr;
    if (req.requestId) {
        reqPtr = s.requests.get(req.requestId);
        if (!reqPtr) throw NotFoundError("Emergency request " + std::to_string(req.requestId) + " not found");
        if (reqPtr->incidentId != inc.id) throw ValidationError("Request belongs to a different incident");
        if (reqPtr->status == RequestStatus::Fulfilled || reqPtr->status == RequestStatus::Cancelled) throw ConflictError("Request is already closed");
    }

    // ---- 1. choose resources -------------------------------------------------
    DynamicArray<DeploymentResource> chosen;
    long now = util::nowEpoch();
    if (req.items.empty()) {
        RankOptions opt; opt.limit = 20; opt.category = req.category; opt.unitsNeeded = req.autoUnits;
        RankingResult rk = ranking_.rankForIncident(inc.id, opt);
        int need = rk.unitsNeeded;
        for (const auto& c : rk.candidates) {
            if (need <= 0) break;
            const Resource* r = s.resources.get(c.resourceId);
            if (r->capacity == 1 && s.windowsOf(s.commitments, r->id).anyOverlap(now, OPEN_END)) continue;
            if (s.links.find(inc.id, r->id)) continue;     // already serving this incident
            int units = c.available < need ? c.available : need;
            DeploymentResource dr;
            dr.resourceId = c.resourceId; dr.resourceCode = c.code; dr.units = units;
            dr.suitability = c.score; dr.routeKm = c.roadKm; dr.etaMinutes = c.etaMinutes;
            dr.reason = "Auto-selected (rank " + std::to_string(chosen.size() + 1) + ", suitability " + fmt1(c.score) + "): " + c.reason;
            chosen.pushBack(dr);
            need -= units;
        }
        if (chosen.empty()) throw ConflictError("No available resource satisfies this incident's requirements (" +
            std::to_string(rk.rejectedUnavailable) + " unavailable, " + std::to_string(rk.rejectedCapability) + " lacking capabilities)");
    } else {
        // manual selection — validated and explained, same scoring for the record
        RankOptions opt; opt.limit = 1000;
        RankingResult rk = ranking_.rankForIncident(inc.id, opt);
        for (const auto& it : req.items) {
            const Resource* r = s.resources.get(it.resourceId);
            if (!r) throw NotFoundError("Resource " + std::to_string(it.resourceId) + " not found");
            if (it.units < 1) throw ValidationError("Units must be at least 1 for " + r->code);
            if (r->status != ResourceStatus::Available) throw ConflictError(r->code + " is " + toString(r->status) + " and cannot be allocated");
            if (it.units > r->available) throw ConflictError(r->code + " has only " + std::to_string(r->available) + " unit(s) free");
            if (r->capacity == 1 && s.windowsOf(s.commitments, r->id).anyOverlap(now, OPEN_END)) throw ConflictError(r->code + " is already committed to another active deployment");
            if (s.links.find(inc.id, r->id)) throw ConflictError(r->code + " is already deployed to this incident");
            if (chosen.findIndex([&](const DeploymentResource& d) { return d.resourceId == r->id; }) >= 0) throw ValidationError("Duplicate resource " + r->code + " in request");
            DeploymentResource dr;
            dr.resourceId = r->id; dr.resourceCode = r->code; dr.units = it.units;
            long idx = rk.candidates.findIndex([&](const RankedResource& c) { return c.resourceId == r->id; });
            if (idx >= 0) {
                const RankedResource& c = rk.candidates[idx];
                dr.suitability = c.score; dr.routeKm = c.roadKm; dr.etaMinutes = c.etaMinutes;
                dr.reason = "Operator-selected (rank " + std::to_string(idx + 1) + " of " + std::to_string(rk.candidates.size()) + ", suitability " + fmt1(c.score) + "): " + c.reason;
            } else {
                // not in the ranked pool (e.g. capability mismatch or far away) — still allowed, but recorded
                RouteResult route = network_.routeBetweenPoints(r->lat, r->lon, inc.lat, inc.lon, ds::WeightMode::Distance);
                dr.routeKm = route.found ? route.distanceKm : 0;
                dr.etaMinutes = route.found ? route.minutes : 0;
                dr.reason = "Operator override: resource was outside the ranked candidate pool (capability mismatch or distance)";
            }
            chosen.pushBack(dr);
        }
    }

    // ---- 2. prepare all changes (nothing applied yet) ---------------------------
    Deployment d;
    d.incidentId = inc.id; d.requestId = req.requestId;
    d.status = DeploymentStatus::Active;
    d.createdAt = now; d.createdBy = actor; d.notes = req.notes;
    d.resources = chosen;
    DynamicArray<Resource> updated;
    DynamicArray<IncidentEvent> events;
    int totalUnits = 0;
    for (const auto& dr : chosen) {
        Resource r = *s.resources.get(dr.resourceId);
        r.available -= dr.units;
        if (r.available == 0) r.status = ResourceStatus::Deployed;
        r.updatedAt = now;
        updated.pushBack(r);
        totalUnits += dr.units;
        IncidentEvent e; e.incidentId = inc.id; e.type = "DEPLOYED"; e.actor = actor; e.createdAt = now;
        e.message = dr.resourceCode + " × " + std::to_string(dr.units) + " dispatched — " + fmt1(dr.routeKm) + " km, ETA ~" +
                    std::to_string((int)std::round(dr.etaMinutes)) + " min. " + dr.reason;
        events.pushBack(e);
    }
    inc.assignedUnits += totalUnits;
    if (inc.status == IncidentStatus::Pending) {
        inc.status = IncidentStatus::Active;
        IncidentEvent e; e.incidentId = inc.id; e.type = "STATUS_CHANGED"; e.actor = "allocation-engine"; e.createdAt = now;
        e.message = "Status → ACTIVE (first resources dispatched)";
        events.pushBack(e);
    }
    inc.updatedAt = now;
    EmergencyRequest reqCopy;
    if (reqPtr) { reqCopy = *reqPtr; reqCopy.status = RequestStatus::Fulfilled; reqCopy.processedAt = now; }

    // ---- 3. ONE transaction -----------------------------------------------------
    try {
        ctx_.repos.deployments->commitAllocation(d, updated, events, reqPtr ? &reqCopy : nullptr, &inc);
    } catch (const NexaError&) {
        throw;
    } catch (const std::exception& ex) {
        throw PersistenceError(std::string("Allocation rolled back: ") + ex.what());
    }

    // ---- 4. apply to in-memory structures (only after commit) ---------------
    s.deployments.upsert(d);
    indexDeployment(d);
    for (const auto& r : updated) {
        *s.resources.get(r.id) = r;
        resources_.recordHistory(r.id, "DEPLOYED", "Deployment #" + std::to_string(d.id) + " to " + inc.code, true);
        ctx_.sink.publish(LiveEvent{"resource.updated", r.id, r.code + " " + r.name, toString(r.status), r.lat, r.lon});
    }
    *s.incidents.get(inc.id) = inc;
    for (const auto& e : events) s.timeline(inc.id).insertBack(e);
    if (reqPtr) {
        reqCopy.deploymentId = d.id;
        *s.requests.get(reqCopy.id) = reqCopy;
        s.requestLog(reqCopy.id).insertBack(util::isoTime(now) + " fulfilled by deployment #" + std::to_string(d.id));
    }
    double p = priority_.refresh(inc.id);
    audit_.record("AUDIT", "allocation", "deployment #" + std::to_string(d.id) + " -> " + inc.code + " (" + std::to_string(totalUnits) + " units)", actor);
    ctx_.sink.publish(LiveEvent{"deployment.created", d.id, inc.code + " " + inc.title,
                                std::to_string(chosen.size()) + " resource(s) dispatched; priority now " + std::to_string((int)p), inc.lat, inc.lon, inc.severity});
    return *s.deployments.get(d.id);
}

Deployment AllocationService::release(long deploymentId, DeploymentStatus finalStatus, const std::string& actor, const std::string& reason, long onlyResourceId) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto& s = ctx_.store;
    const Deployment* dp = s.deployments.get(deploymentId);
    if (!dp) throw NotFoundError("Deployment " + std::to_string(deploymentId) + " not found");
    if (dp->status != DeploymentStatus::Active) throw ConflictError("Deployment is already " + std::string(toString(dp->status)));
    Deployment d = *dp;
    Incident inc = *s.incidents.get(d.incidentId);
    long now = util::nowEpoch();
    DynamicArray<Resource> updated;
    DynamicArray<IncidentEvent> events;
    DynamicArray<DeploymentResource> releasedNow;
    for (auto& dr : d.resources) {
        if (dr.released || (onlyResourceId && dr.resourceId != onlyResourceId)) continue;
        Resource r = *s.resources.get(dr.resourceId);
        r.available += dr.units;
        if (r.available > r.capacity) r.available = r.capacity;
        if (r.status == ResourceStatus::Deployed) r.status = ResourceStatus::Available;
        r.updatedAt = now;
        updated.pushBack(r);
        dr.released = true; dr.releasedAt = now;
        releasedNow.pushBack(dr);
        inc.assignedUnits -= dr.units;
        IncidentEvent e; e.incidentId = inc.id; e.type = "RELEASED"; e.actor = actor; e.createdAt = now;
        e.message = dr.resourceCode + " released (" + std::string(toString(finalStatus)) + ")" + (reason.empty() ? "" : " — " + reason);
        events.pushBack(e);
    }
    if (releasedNow.empty()) throw NotFoundError("Resource is not part of this active deployment");
    if (inc.assignedUnits < 0) inc.assignedUnits = 0;
    bool allReleased = true;
    for (const auto& dr : d.resources) if (!dr.released) allReleased = false;
    if (allReleased) { d.status = finalStatus; d.completedAt = now; }
    inc.updatedAt = now;

    ctx_.repos.deployments->commitRelease(d, updated, events, &inc);   // ONE transaction

    for (const auto& dr : releasedNow) {
        s.links.unlink(d.incidentId, dr.resourceId);
        s.unitsMatrix.add(d.incidentId, dr.resourceId, -dr.units);
        s.windowsOf(s.commitments, dr.resourceId).remove(d.id, d.createdAt);
    }
    for (const auto& r : updated) {
        *s.resources.get(r.id) = r;
        resources_.recordHistory(r.id, "RELEASED", "Released from deployment #" + std::to_string(d.id), true);
        ctx_.sink.publish(LiveEvent{"resource.updated", r.id, r.code + " " + r.name, toString(r.status), r.lat, r.lon});
    }
    *s.deployments.get(d.id) = d;
    *s.incidents.get(inc.id) = inc;
    for (const auto& e : events) s.timeline(inc.id).insertBack(e);
    priority_.refresh(inc.id);
    audit_.record("AUDIT", "allocation", "deployment #" + std::to_string(d.id) + " " + toString(d.status), actor);
    ctx_.sink.publish(LiveEvent{"deployment.closed", d.id, inc.code + " " + inc.title, toString(d.status), inc.lat, inc.lon, inc.severity});
    return d;
}

Deployment AllocationService::complete(long id, const std::string& actor) { return release(id, DeploymentStatus::Completed, actor, "mission complete"); }
Deployment AllocationService::cancel(long id, const std::string& actor, const std::string& reason) { return release(id, DeploymentStatus::Cancelled, actor, reason.empty() ? "cancelled" : reason); }

Deployment AllocationService::reassign(long resourceId, long fromDeploymentId, long toIncidentId, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    const Deployment* dp = ctx_.store.deployments.get(fromDeploymentId);
    if (!dp) throw NotFoundError("Deployment not found");
    if (dp->incidentId == toIncidentId) throw ValidationError("Resource is already serving that incident");
    if (!ctx_.store.incidents.contains(toIncidentId)) throw NotFoundError("Target incident not found");
    int units = 0;
    for (const auto& dr : dp->resources) if (dr.resourceId == resourceId && !dr.released) units = dr.units;
    if (!units) throw NotFoundError("Resource is not active in that deployment");
    release(fromDeploymentId, DeploymentStatus::Completed, actor, "reassigned to incident #" + std::to_string(toIncidentId), resourceId);
    AllocationRequest req;
    req.incidentId = toIncidentId;
    req.items.pushBack(AllocationItem{resourceId, units});
    req.notes = "Reassigned from deployment #" + std::to_string(fromDeploymentId);
    return allocate(req, actor);
}

DynamicArray<Deployment> AllocationService::list(const std::string& status) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    DynamicArray<Deployment> out;
    ctx_.store.deployments.forEachOrdered([&](const Deployment& d) { if (status.empty() || status == toString(d.status)) out.pushBack(d); });
    algo::mergeSort(out, [](const Deployment& a, const Deployment& b) { return a.createdAt > b.createdAt || (a.createdAt == b.createdAt && a.id > b.id); });
    return out;
}

Deployment AllocationService::get(long id) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    const Deployment* d = ctx_.store.deployments.get(id);
    if (!d) throw NotFoundError("Deployment " + std::to_string(id) + " not found");
    return *d;
}

AllocationMatrixView AllocationService::matrix() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto& s = ctx_.store;
    AllocationMatrixView v;
    ds::HashTable<long, bool> rows, cols;
    for (const auto& e : s.unitsMatrix.entries()) {
        MatrixCell c; c.incidentId = e.row; c.resourceId = e.col; c.units = e.value;
        if (const Incident* i = s.incidents.get(e.row)) c.incidentCode = i->code;
        if (const Resource* r = s.resources.get(e.col)) c.resourceCode = r->code;
        v.cells.pushBack(c);
        rows.insert(e.row, true); cols.insert(e.col, true);
    }
    v.rows = s.incidents.size(); v.cols = s.resources.size();
    v.density = (v.rows && v.cols) ? (double)v.cells.size() / ((double)v.rows * (double)v.cols) : 0;
    v.sparseBytes = v.cells.size() * sizeof(ds::SparseMatrix<int>::Entry);
    v.denseBytes = v.rows * v.cols * sizeof(int);
    return v;
}

} // namespace nexa::svc
