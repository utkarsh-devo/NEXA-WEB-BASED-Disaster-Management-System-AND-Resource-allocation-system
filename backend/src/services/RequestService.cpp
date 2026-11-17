#include "services/RequestService.hpp"
#include "utilities/Validation.hpp"
#include <cmath>

namespace nexa::svc {

double RequestService::urgencyScore(const EmergencyRequest& r) {
    double people = std::log10(1.0 + (double)(r.peopleAffected > 0 ? r.peopleAffected : 0)) / 3.0;
    if (people > 1) people = 1;
    double incidentP = 0;
    if (const Incident* i = ctx_.store.incidents.get(r.incidentId)) incidentP = i->priorityScore;
    double s = 50.0 * (r.urgency / 5.0) + 20.0 * people + 30.0 * (incidentP / 100.0);
    return std::round(s * 100.0) / 100.0;
}

void RequestService::enqueue(const EmergencyRequest& r) {
    if (r.status == RequestStatus::Escalated) ctx_.store.urgentRequests.push(r.id, r.urgencyScore);
    else if (r.status == RequestStatus::Queued) ctx_.store.fifoRequests.enqueue(r.id);
}

EmergencyRequest RequestService::create(const RequestInput& in, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    static const char* types[] = {"medical", "rescue", "fire", "evacuation", "supplies", "shelter"};
    const Incident* inc = ctx_.store.incidents.get(in.incidentId);
    if (!inc) throw ValidationError("Request must reference an existing incident");
    if (!PriorityEngine::isOpen(inc->status)) throw ConflictError("Incident is " + std::string(toString(inc->status)) + "; requests are not accepted");
    validate::oneOf(in.assistanceType, types, 6, "Assistance type");
    validate::range(in.urgency, 1, 5, "Urgency");
    validate::maxLength(in.description, 2000, "Description");
    validate::nonNegative((double)in.peopleAffected, "People affected");

    EmergencyRequest r;
    r.incidentId = in.incidentId; r.assistanceType = in.assistanceType; r.description = util::trim(in.description);
    r.urgency = in.urgency; r.peopleAffected = in.peopleAffected; r.requestedBy = in.requestedBy.empty() ? actor : in.requestedBy;
    r.createdAt = util::nowEpoch();
    r.urgencyScore = urgencyScore(r);
    bool escalate = r.urgency == 5 || r.urgencyScore >= ctx_.config.escalationThreshold;
    r.status = escalate ? RequestStatus::Escalated : RequestStatus::Queued;
    ctx_.repos.requests->insert(r);
    ctx_.store.requests.upsert(r);
    enqueue(r);
    ctx_.store.requestLog(r.id).insertBack(util::isoTime(r.createdAt) + " received (score " + std::to_string((int)r.urgencyScore) + ") -> " +
                                           (escalate ? "priority queue" : "FIFO queue"));
    incidents_.addEvent(r.incidentId, "REQUEST", "Request #" + std::to_string(r.id) + " for " + r.assistanceType + " (urgency " + std::to_string(r.urgency) +
                        ", score " + std::to_string((int)r.urgencyScore) + ")" + (escalate ? " — ESCALATED" : ""), actor);
    ctx_.sink.publish(LiveEvent{"request.created", r.id, "Request #" + std::to_string(r.id) + " " + r.assistanceType, escalate ? "escalated" : "queued", inc->lat, inc->lon, inc->severity});
    return r;
}

EmergencyRequest RequestService::escalate(long id, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    EmergencyRequest* r = ctx_.store.requests.get(id);
    if (!r) throw NotFoundError("Request not found");
    if (r->status != RequestStatus::Queued) throw ConflictError("Only QUEUED requests can be escalated (current: " + std::string(toString(r->status)) + ")");
    EmergencyRequest copy = *r;
    copy.status = RequestStatus::Escalated;
    copy.urgencyScore = urgencyScore(copy) + 15;          // manual escalation bonus (documented)
    ctx_.repos.requests->update(copy);
    ctx_.store.fifoRequests.removeFirst([id](long x) { return x == id; });   // O(n) removal from the FIFO queue
    ctx_.store.urgentRequests.push(id, copy.urgencyScore);
    *r = copy;
    ctx_.store.requestLog(id).insertBack(util::isoTime(util::nowEpoch()) + " escalated by " + actor);
    incidents_.addEvent(copy.incidentId, "REQUEST", "Request #" + std::to_string(id) + " escalated to the priority queue", actor);
    return copy;
}

EmergencyRequest RequestService::cancel(long id, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    EmergencyRequest* r = ctx_.store.requests.get(id);
    if (!r) throw NotFoundError("Request not found");
    if (r->status == RequestStatus::Fulfilled || r->status == RequestStatus::Cancelled) throw ConflictError("Request already closed");
    EmergencyRequest copy = *r;
    copy.status = RequestStatus::Cancelled; copy.processedAt = util::nowEpoch();
    ctx_.repos.requests->update(copy);
    ctx_.store.fifoRequests.removeFirst([id](long x) { return x == id; });
    ctx_.store.urgentRequests.remove(id);
    *r = copy;
    ctx_.store.requestLog(id).insertBack(util::isoTime(copy.processedAt) + " cancelled by " + actor);
    return copy;
}

ProcessResult RequestService::processNext(const std::string& mode, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto& s = ctx_.store;
    ProcessResult res;
    long id = 0;
    if ((mode == "auto" || mode == "priority") && !s.urgentRequests.empty()) { id = s.urgentRequests.pop().id; res.queueUsed = "priority"; }
    else if ((mode == "auto" || mode == "fifo") && !s.fifoRequests.isEmpty()) { id = s.fifoRequests.dequeue(); res.queueUsed = "fifo"; }
    if (!id) { res.queueUsed = "none"; res.message = "No pending requests in the selected queue"; return res; }

    EmergencyRequest* r = s.requests.get(id);
    EmergencyRequest copy = *r;
    copy.status = RequestStatus::Processing;
    ctx_.repos.requests->update(copy);
    *r = copy;
    s.requestLog(id).insertBack(util::isoTime(util::nowEpoch()) + " processing from " + res.queueUsed + " queue");
    res.processed = true;

    static const struct { const char* type; const char* category; } map[] = {
        {"medical", "ambulance"}, {"fire", "fire_engine"}, {"rescue", ""}, {"evacuation", ""}, {"supplies", ""}, {"shelter", "shelter"}};
    AllocationRequest ar;
    ar.incidentId = copy.incidentId;
    ar.requestId = id;
    ar.autoUnits = 1;
    for (const auto& m : map) if (copy.assistanceType == m.type) ar.category = m.category;
    ar.notes = "Auto-allocation for request #" + std::to_string(id);
    try {
        res.deployment = allocation_.allocate(ar, actor);
        res.allocated = true;
        res.message = "Deployment #" + std::to_string(res.deployment.id) + " created";
    } catch (const NexaError& e) {
        // could not be served now -> back to the waiting state (documented re-queue policy)
        EmergencyRequest back = *s.requests.get(id);
        back.status = res.queueUsed == "priority" ? RequestStatus::Escalated : RequestStatus::Queued;
        ctx_.repos.requests->update(back);
        *s.requests.get(id) = back;
        enqueue(back);
        s.requestLog(id).insertBack(util::isoTime(util::nowEpoch()) + " could not be served: " + e.what() + " — re-queued");
        res.message = std::string("Re-queued: ") + e.what();
    }
    res.request = *s.requests.get(id);
    audit_.record("INFO", "requests", "processed request #" + std::to_string(id) + ": " + res.message, actor);
    return res;
}

DynamicArray<EmergencyRequest> RequestService::list(const std::string& status) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    DynamicArray<EmergencyRequest> out;
    ctx_.store.requests.forEachOrdered([&](const EmergencyRequest& r) { if (status.empty() || status == toString(r.status)) out.pushBack(r); });
    return out;
}

EmergencyRequest RequestService::get(long id) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    const EmergencyRequest* r = ctx_.store.requests.get(id);
    if (!r) throw NotFoundError("Request not found");
    return *r;
}

DynamicArray<std::string> RequestService::history(long id) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    DynamicArray<std::string> out;
    ctx_.store.requestLog(id).forEach([&](const std::string& s) { out.pushBack(s); });
    return out;
}

QueueSnapshot RequestService::queues() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    QueueSnapshot q;
    ctx_.store.fifoRequests.forEach([&](long id) { if (const EmergencyRequest* r = ctx_.store.requests.get(id)) q.fifo.pushBack(*r); });
    for (const auto& e : ctx_.store.urgentRequests.snapshotOrdered()) if (const EmergencyRequest* r = ctx_.store.requests.get(e.id)) q.priority.pushBack(*r);
    return q;
}

} // namespace nexa::svc
