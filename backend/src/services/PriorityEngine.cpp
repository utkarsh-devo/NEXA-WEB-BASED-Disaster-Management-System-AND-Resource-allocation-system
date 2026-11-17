#include "services/PriorityEngine.hpp"
#include <cmath>

namespace nexa::svc {

PriorityBreakdown PriorityEngine::compute(const Incident& i, long now) const {
    const PriorityWeights& w = ctx_.config.priority;
    PriorityBreakdown b;
    double S = (i.severity < 1 ? 1 : (i.severity > 5 ? 5 : i.severity)) / 5.0;
    double A = std::log10(1.0 + (double)(i.affectedPeople > 0 ? i.affectedPeople : 0)) / std::log10(1.0 + w.affectedSaturation);
    if (A > 1) A = 1;
    int required = i.requiredUnits > 0 ? i.requiredUnits : 1;
    int unmetUnits = required - i.assignedUnits;
    double U = unmetUnits > 0 ? (double)unmetUnits / required : 0.0;
    double ageH = (double)(now - i.createdAt) / 3600.0;
    double T = ageH <= 0 ? 0 : (ageH >= w.ageSaturationHours ? 1.0 : ageH / w.ageSaturationHours);
    double St = i.status == IncidentStatus::Pending ? 1.0 : (i.status == IncidentStatus::Active ? 0.7 : (i.status == IncidentStatus::Contained ? 0.25 : 0.0));
    double Ts = 0.5;
    if (const IncidentCategory* c = ctx_.store.categoryById.find(i.categoryId)) Ts = c->timeSensitivity;
    double sum = w.sum();
    b.severity = 100 * w.severity * S / sum;
    b.affected = 100 * w.affected * A / sum;
    b.unmet = 100 * w.unmet * U / sum;
    b.age = 100 * w.age * T / sum;
    b.status = 100 * w.status * St / sum;
    b.timeSensitivity = 100 * w.timeSensitivity * Ts / sum;
    b.total = std::round((b.severity + b.affected + b.unmet + b.age + b.status + b.timeSensitivity) * 100.0) / 100.0;
    return b;
}

double PriorityEngine::refresh(long incidentId) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    Incident* inc = ctx_.store.incidents.get(incidentId);
    if (!inc) { ctx_.store.incidentQueue.remove(incidentId); return 0; }
    if (!isOpen(inc->status)) { inc->priorityScore = 0; ctx_.store.incidentQueue.remove(incidentId); return 0; }
    double p = compute(*inc, util::nowEpoch()).total;
    inc->priorityScore = p;
    ctx_.store.incidentQueue.push(incidentId, p);   // push == update when present
    return p;
}

void PriorityEngine::refreshAll() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    DynamicArray<long> ids;
    for (const Incident& i : ctx_.store.incidents.items()) ids.pushBack(i.id);
    for (long id : ids) refresh(id);
}

PriorityQueueRow PriorityEngine::row(long id, double p) {
    PriorityQueueRow r;
    r.incidentId = id; r.priority = p;
    if (const Incident* i = ctx_.store.incidents.get(id)) {
        r.code = i->code; r.title = i->title; r.status = toString(i->status);
        r.severity = i->severity; r.createdAt = i->createdAt;
        r.breakdown = compute(*i, util::nowEpoch());
    }
    return r;
}

DynamicArray<PriorityQueueRow> PriorityEngine::orderedQueue(std::size_t limit) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    DynamicArray<PriorityQueueRow> out;
    auto order = ctx_.store.incidentQueue.snapshotOrdered();
    int rank = 0;
    for (const auto& e : order) {
        if (limit && out.size() >= limit) break;
        PriorityQueueRow r = row(e.id, e.priority);
        r.rank = ++rank;
        out.pushBack(r);
    }
    return out;
}

DynamicArray<PriorityQueueRow> PriorityEngine::heapArray() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    DynamicArray<PriorityQueueRow> out;
    int idx = 0;
    for (const auto& e : ctx_.store.incidentQueue.raw()) { PriorityQueueRow r = row(e.id, e.priority); r.rank = idx++; out.pushBack(r); }
    return out;
}

long PriorityEngine::peekNext() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    return ctx_.store.incidentQueue.empty() ? 0 : ctx_.store.incidentQueue.top().id;
}

void PriorityEngine::setWeights(const PriorityWeights& w) {
    w.validate();
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    ctx_.config.priority = w;
    refreshAll();
}

} // namespace nexa::svc
