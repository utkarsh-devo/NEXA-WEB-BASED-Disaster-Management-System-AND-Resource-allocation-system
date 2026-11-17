#include "services/IncidentService.hpp"
#include "algorithms/Sorting.hpp"
#include "utilities/Validation.hpp"
#include <chrono>
#include <ctime>

namespace nexa::svc {

namespace {
long currentYear() { std::time_t t = std::time(nullptr); std::tm tmv{};
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    return 1900 + tmv.tm_year; }
}

// ------------------------------------------------------------------ helpers
void IncidentService::validate(const IncidentInput& in) {
    validate::required(in.title, "Title");
    validate::maxLength(in.title, 160, "Title");
    validate::maxLength(in.description, 4000, "Description");
    validate::range(in.severity, 1, 5, "Severity");
    validate::coordinates(in.lat, in.lon);
    validate::nonNegative((double)in.affectedPeople, "Affected people");
    validate::range(in.requiredUnits, 1, 200, "Required units");
    if (!ctx_.store.categoryById.contains(in.categoryId)) throw ValidationError("Unknown disaster category");
}

std::string IncidentService::categoryPath(int categoryId) {
    auto path = ctx_.store.taxonomy.pathToRoot(categoryId);
    std::string s;
    for (std::size_t i = 1; i < path.size(); ++i) { if (i > 1) s += " > "; s += path[i]->label; }   // skip root
    return s;
}

bool IncidentService::isDescendant(int categoryId, int ancestorId) {
    for (auto* n = ctx_.store.taxonomy.find(categoryId); n; n = n->parent) if (n->id == ancestorId) return true;
    return false;
}

void IncidentService::indexText(const Incident& i) {
    for (const auto& w : util::words(i.title)) ctx_.store.incidentWords.insert(w, i.id);
    for (const auto& w : util::words(i.locationName)) ctx_.store.incidentWords.insert(w, i.id);
    ctx_.store.incidentWords.insert(i.code, i.id);
    ctx_.store.descriptionsDirty = true;
}
void IncidentService::unindexText(const Incident& i) {
    for (const auto& w : util::words(i.title)) ctx_.store.incidentWords.remove(w, i.id);
    for (const auto& w : util::words(i.locationName)) ctx_.store.incidentWords.remove(w, i.id);
    ctx_.store.incidentWords.remove(i.code, i.id);
    ctx_.store.descriptionsDirty = true;
}

void IncidentService::rebuildDescriptionIndexIfDirty() {
    auto& s = ctx_.store;
    if (!s.descriptionsDirty) return;
    s.descriptions.clearDocuments();
    s.incidents.forEachOrdered([&](const Incident& i) { s.descriptions.addDocument(i.id, i.title + " " + i.description); });
    s.descriptions.build();
    s.descriptionsDirty = false;
}

IncidentEvent IncidentService::addEvent(long incidentId, const std::string& type, const std::string& message, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    IncidentEvent e; e.incidentId = incidentId; e.type = type; e.message = message; e.actor = actor; e.createdAt = util::nowEpoch();
    ctx_.repos.events->insert(e);
    ctx_.store.timeline(incidentId).insertBack(e);
    return e;
}

// ------------------------------------------------------------------ commands
Incident IncidentService::create(const IncidentInput& in, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    validate(in);
    Incident i;
    i.title = util::trim(in.title);
    i.description = util::trim(in.description);
    i.categoryId = in.categoryId;
    const IncidentCategory* cat = ctx_.store.categoryById.find(in.categoryId);
    i.categoryName = cat->name;
    i.severity = in.severity;
    i.lat = in.lat; i.lon = in.lon;
    i.locationName = util::trim(in.locationName);
    i.affectedPeople = in.affectedPeople;
    i.requiredCapabilities = util::normaliseList(in.requiredCapabilities.empty() ? cat->defaultCapabilities : in.requiredCapabilities);
    i.requiredUnits = in.requiredUnits;
    i.reportedBy = in.reportedBy.empty() ? actor : in.reportedBy;
    i.reporterContact = in.reporterContact;
    i.status = IncidentStatus::Pending;
    i.createdAt = i.updatedAt = in.createdAt > 0 ? in.createdAt : util::nowEpoch();
    i.simulated = in.simulated;
    i.scenarioId = in.scenarioId;

    // 1) persist (source of truth); 2) assign human-readable code
    ctx_.repos.incidents->insert(i);
    i.code = "INC-" + std::to_string(currentYear()) + "-" + util::padNumber(i.id, 4);
    i.categoryPath = categoryPath(i.categoryId);
    ctx_.repos.incidents->update(i);

    // 3) in-memory indexes
    ctx_.store.incidents.upsert(i);
    ctx_.store.incidentByTime.insert(TimeKey{i.createdAt, i.id}, i.id);
    indexText(i);
    spatial_.indexIncident(i);

    // 4) classification + priority
    double p = priority_.refresh(i.id);
    addEvent(i.id, "REGISTERED", "Incident registered — " + i.categoryPath + ", severity " + model::severityLabel(i.severity) +
             (i.simulated ? " [SIMULATED]" : ""), actor);
    addEvent(i.id, "PRIORITY", "Priority score computed: " + std::to_string((int)p), "priority-engine");

    audit_.record("AUDIT", "incidents", "created " + i.code, actor);
    ctx_.sink.publish(LiveEvent{"incident.created", i.id, i.code + " " + i.title, i.categoryPath, i.lat, i.lon, i.severity});
    return *ctx_.store.incidents.get(i.id);
}

Incident IncidentService::update(long id, const IncidentInput& in, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    Incident* cur = ctx_.store.incidents.get(id);
    if (!cur) throw NotFoundError("Incident " + std::to_string(id) + " not found");
    validate(in);
    Incident before = *cur;
    Incident i = before;
    i.title = util::trim(in.title);
    i.description = util::trim(in.description);
    i.categoryId = in.categoryId;
    i.categoryName = ctx_.store.categoryById.find(in.categoryId)->name;
    i.categoryPath = categoryPath(in.categoryId);
    i.severity = in.severity;
    i.lat = in.lat; i.lon = in.lon;
    i.locationName = util::trim(in.locationName);
    i.affectedPeople = in.affectedPeople;
    i.requiredCapabilities = util::normaliseList(in.requiredCapabilities);
    i.requiredUnits = in.requiredUnits;
    i.reporterContact = in.reporterContact;
    i.updatedAt = util::nowEpoch();
    ctx_.repos.incidents->update(i);

    unindexText(before);
    spatial_.unindexIncident(before);
    *cur = i;
    indexText(i);
    spatial_.indexIncident(i);
    double p = priority_.refresh(id);
    std::string changes;
    if (before.severity != i.severity) changes += " severity " + std::to_string(before.severity) + "→" + std::to_string(i.severity) + ";";
    if (before.affectedPeople != i.affectedPeople) changes += " affected " + std::to_string(before.affectedPeople) + "→" + std::to_string(i.affectedPeople) + ";";
    if (before.lat != i.lat || before.lon != i.lon) changes += " location moved;";
    addEvent(id, "UPDATED", "Incident details updated." + changes + " Priority now " + std::to_string((int)p), actor);
    ctx_.sink.publish(LiveEvent{"incident.updated", id, i.code + " " + i.title, "updated", i.lat, i.lon, i.severity});
    return *ctx_.store.incidents.get(id);
}

Incident IncidentService::changeStatus(long id, IncidentStatus st, const std::string& actor, const std::string& note) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    Incident* cur = ctx_.store.incidents.get(id);
    if (!cur) throw NotFoundError("Incident " + std::to_string(id) + " not found");
    if (cur->status == st) return *cur;
    if (cur->status == IncidentStatus::Archived) throw ConflictError("Archived incidents cannot change status");
    if (st == IncidentStatus::Resolved || st == IncidentStatus::Archived) {
        // operational rule: cannot close while resources are still deployed
        bool active = false;
        ctx_.store.links.forEachResourceOf(id, [&](long, long) { active = true; });
        if (active) throw ConflictError("Release or complete active deployments before resolving this incident");
    }
    Incident i = *cur;
    OperatorAction act{"INCIDENT_STATUS", id, toString(i.status), actor, util::nowEpoch()};
    i.status = st;
    i.updatedAt = util::nowEpoch();
    if (st == IncidentStatus::Resolved) i.resolvedAt = i.updatedAt;
    ctx_.repos.incidents->update(i);
    *cur = i;
    ctx_.store.actions.push(act);
    priority_.refresh(id);
    addEvent(id, "STATUS_CHANGED", std::string("Status → ") + toString(st) + (note.empty() ? "" : " — " + note), actor);
    ctx_.sink.publish(LiveEvent{st == IncidentStatus::Resolved ? "incident.resolved" : "incident.updated", id, i.code + " " + i.title, toString(st), i.lat, i.lon, i.severity});
    return i;
}

std::string IncidentService::removeOrArchive(long id, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    Incident* cur = ctx_.store.incidents.get(id);
    if (!cur) throw NotFoundError("Incident " + std::to_string(id) + " not found");
    bool everDeployed = false;
    for (const auto& d : ctx_.store.deployments.items()) if (d.incidentId == id) { everDeployed = true; break; }
    bool hasRequests = false;
    for (const auto& r : ctx_.store.requests.items()) if (r.incidentId == id) { hasRequests = true; break; }
    if (cur->status == IncidentStatus::Pending && !everDeployed && !hasRequests) {
        Incident copy = *cur;
        ctx_.repos.incidents->remove(id);
        unindexText(copy);
        spatial_.unindexIncident(copy);
        ctx_.store.incidentByTime.remove(TimeKey{copy.createdAt, copy.id});
        ctx_.store.incidentQueue.remove(id);
        if (auto** l = ctx_.store.timelines.find(id)) { delete *l; ctx_.store.timelines.erase(id); }
        ctx_.store.incidents.remove(id);
        audit_.record("AUDIT", "incidents", "deleted " + copy.code, actor);
        ctx_.sink.publish(LiveEvent{"incident.deleted", id, copy.code, "deleted"});
        return "deleted";
    }
    changeStatus(id, IncidentStatus::Archived, actor, "archived (has operational history)");
    return "archived";
}

void IncidentService::addNote(long id, const std::string& text, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    if (!ctx_.store.incidents.contains(id)) throw NotFoundError("Incident not found");
    validate::required(text, "Note");
    validate::maxLength(text, 1000, "Note");
    addEvent(id, "NOTE", util::trim(text), actor);
}

bool IncidentService::undoLastAction(const std::string& actor, std::string& description) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    while (!ctx_.store.actions.isEmpty()) {
        OperatorAction a = ctx_.store.actions.pop();
        if (a.type != "INCIDENT_STATUS" || !ctx_.store.incidents.contains(a.entityId)) continue;
        IncidentStatus prev;
        if (!parse(a.previous, prev)) continue;
        Incident* cur = ctx_.store.incidents.get(a.entityId);
        if (cur->status == IncidentStatus::Archived) continue;
        changeStatus(a.entityId, prev, actor, "undo of action by " + a.actor);
        ctx_.store.actions.pop();   // don't keep the undo itself on the stack
        description = "Restored " + cur->code + " to " + a.previous;
        return true;
    }
    description = "Nothing to undo";
    return false;
}

// ------------------------------------------------------------------ queries
Incident IncidentService::get(long id) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    const Incident* i = ctx_.store.incidents.get(id);
    if (!i) throw NotFoundError("Incident " + std::to_string(id) + " not found");
    return *i;
}

DynamicArray<Incident> IncidentService::list(const IncidentQuery& q, QueryStats* stats) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto t0 = std::chrono::steady_clock::now();
    auto& s = ctx_.store;
    QueryStats st;
    DynamicArray<Incident> rows;
    auto statusOk = [&](const Incident& i) {
        if (q.status == "ALL") return true;
        if (q.status.empty()) return PriorityEngine::isOpen(i.status);
        return std::string(toString(i.status)) == q.status;
    };
    auto keep = [&](const Incident& i) {
        if (!statusOk(i)) return false;
        if (i.severity < q.minSeverity || i.severity > q.maxSeverity) return false;
        if (q.categoryId && !isDescendant(i.categoryId, q.categoryId)) return false;
        return true;
    };
    if (!q.prefix.empty()) {
        st.index = "trie";
        ds::HashTable<long, bool> seen;
        for (const auto& m : s.incidentWords.withPrefix(q.prefix, 500))
            for (long id : m.ids) if (!seen.contains(id)) { seen.insert(id, true); if (const Incident* i = s.incidents.get(id)) { ++st.scanned; if (keep(*i)) rows.pushBack(*i); } }
    } else if (!q.text.empty()) {
        st.index = "suffix_array";
        rebuildDescriptionIndexIfDirty();
        ds::HashTable<long, bool> seen;
        for (const auto& o : s.descriptions.find(q.text))
            if (!seen.contains(o.docId)) { seen.insert(o.docId, true); if (const Incident* i = s.incidents.get(o.docId)) { ++st.scanned; if (keep(*i)) rows.pushBack(*i); } }
    } else {
        st.index = "avl_inorder";
        s.incidents.forEachOrdered([&](const Incident& i) { ++st.scanned; if (keep(i)) rows.pushBack(i); });
    }

    algo::OpCounter c;
    st.sortAlgorithm = q.algorithm;
    if (q.sortBy == "severity" && q.algorithm == "counting") {
        // counting sort on the bounded key 1..5, then reverse for descending order
        algo::countingSortBy(rows, [](const Incident& i) { return (long)(6 - i.severity); }, 1, 5, &c);
    } else {
        auto alg = algo::parseSortAlgorithm(q.algorithm);
        st.sortAlgorithm = q.algorithm == "quick" || q.algorithm == "heap" || q.algorithm == "insertion" ? q.algorithm : "merge";
        if (q.sortBy == "created") algo::sortWith(alg, rows, [](const Incident& a, const Incident& b) { return a.createdAt > b.createdAt; }, &c);
        else if (q.sortBy == "severity") algo::sortWith(alg, rows, [](const Incident& a, const Incident& b) { return a.severity > b.severity || (a.severity == b.severity && a.priorityScore > b.priorityScore); }, &c);
        else if (q.sortBy == "affected") algo::sortWith(alg, rows, [](const Incident& a, const Incident& b) { return a.affectedPeople > b.affectedPeople; }, &c);
        else algo::sortWith(alg, rows, [](const Incident& a, const Incident& b) { return a.priorityScore > b.priorityScore; }, &c);
    }
    st.comparisons = c.comparisons;
    if (q.limit && rows.size() > q.limit) { DynamicArray<Incident> cut; for (std::size_t i = 0; i < q.limit; ++i) cut.pushBack(rows[i]); rows = cut; }
    st.microseconds = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    if (stats) *stats = st;
    return rows;
}

IncidentDetail IncidentService::detail(long id) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    IncidentDetail d;
    d.incident = get(id);
    d.incident.categoryPath = categoryPath(d.incident.categoryId);
    ctx_.store.timeline(id).forEach([&](const IncidentEvent& e) { d.timeline.pushBack(e); });
    ctx_.store.links.forEachResourceOf(id, [&](long resourceId, long depId) {
        const Deployment* dep = ctx_.store.deployments.get(depId);
        if (!dep) return;
        for (const auto& dr : dep->resources) if (dr.resourceId == resourceId) d.assigned.pushBack(dr);
        if (d.deploymentIds.findIndex([&](long x) { return x == depId; }) < 0) d.deploymentIds.pushBack(depId);
    });
    for (const auto& r : ctx_.store.requests.items()) if (r.incidentId == id) d.requests.pushBack(r);
    d.priority = priority_.compute(d.incident, util::nowEpoch());
    return d;
}

DynamicArray<ds::Trie::Match> IncidentService::suggest(const std::string& prefix, std::size_t limit) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    if (util::trim(prefix).empty()) return {};
    return ctx_.store.incidentWords.withPrefix(util::trim(prefix), limit);
}

DynamicArray<ds::SuffixArray::Occurrence> IncidentService::textSearch(const std::string& pattern, std::size_t* comparisons) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    validate::required(pattern, "Search pattern");
    rebuildDescriptionIndexIfDirty();
    return ctx_.store.descriptions.find(pattern, comparisons);
}

DynamicArray<IncidentEvent> IncidentService::recentEvents(std::size_t limit) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    DynamicArray<IncidentEvent> all;
    ctx_.store.timelines.forEach([&](long, ds::LinkedList<IncidentEvent>* l) {
        // only the newest few per incident are candidates
        std::size_t taken = 0;
        l->forEachReverse([&](const IncidentEvent& e) { if (taken++ < limit) all.pushBack(e); });
    });
    algo::mergeSort(all, [](const IncidentEvent& a, const IncidentEvent& b) { return a.createdAt > b.createdAt || (a.createdAt == b.createdAt && a.id > b.id); });
    DynamicArray<IncidentEvent> out;
    for (std::size_t i = 0; i < all.size() && i < limit; ++i) out.pushBack(all[i]);
    return out;
}

DynamicArray<CategoryNode> IncidentService::categories() {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    DynamicArray<CategoryNode> out;
    ds::HashTable<int, std::size_t> open;
    for (const auto& i : ctx_.store.incidents.items()) {
        if (!PriorityEngine::isOpen(i.status)) continue;
        for (auto* n = ctx_.store.taxonomy.find(i.categoryId); n; n = n->parent) {      // roll up to ancestors
            std::size_t* c = open.find(n->id);
            open.insert(n->id, c ? *c + 1 : 1);
        }
    }
    ctx_.store.taxonomy.preorder([&](ds::KaryTree<IncidentCategory>::Node* n) {
        CategoryNode c; c.id = n->id; c.parentId = n->parent ? n->parent->id : -1; c.depth = n->depth;
        c.name = n->label; c.code = n->value.code; c.timeSensitivity = n->value.timeSensitivity;
        const std::size_t* k = open.find(n->id); c.openIncidents = k ? *k : 0;
        out.pushBack(c);
    });
    return out;
}

} // namespace nexa::svc
