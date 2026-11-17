#include "services/ResourceService.hpp"
#include "algorithms/Sorting.hpp"
#include "utilities/Geo.hpp"
#include "utilities/Validation.hpp"
#include <chrono>

namespace nexa::svc {

void ResourceService::validate(const ResourceInput& in, long selfId) {
    validate::required(in.code, "Resource code");
    validate::maxLength(in.code, 32, "Resource code");
    validate::required(in.name, "Resource name");
    validate::maxLength(in.name, 120, "Resource name");
    validate::range(in.capacity, 1, 100000, "Capacity");
    int avail = in.available < 0 ? in.capacity : in.available;
    validate::range(avail, 0, in.capacity, "Available units");
    validate::coordinates(in.lat, in.lon);
    if (ctx_.store.resourceTypes.findIndex([&](const ResourceType& t) { return t.id == in.typeId; }) < 0) throw ValidationError("Unknown resource type");
    ResourceStatus st;
    if (!parse(in.status, st)) throw ValidationError("Invalid resource status");
    if (const long* existing = ctx_.store.resourceByCode.find(util::trim(in.code)))
        if (*existing != selfId) throw ConflictError("Resource code '" + in.code + "' is already registered");
}

void ResourceService::indexText(const Resource& r) {
    ctx_.store.resourceWords.insert(r.code, r.id);
    for (const auto& w : util::words(r.name)) ctx_.store.resourceWords.insert(w, r.id);
    for (const auto& w : util::words(r.organization)) ctx_.store.resourceWords.insert(w, r.id);
}
void ResourceService::unindexText(const Resource& r) {
    ctx_.store.resourceWords.remove(r.code, r.id);
    for (const auto& w : util::words(r.name)) ctx_.store.resourceWords.remove(w, r.id);
    for (const auto& w : util::words(r.organization)) ctx_.store.resourceWords.remove(w, r.id);
}

void ResourceService::recordHistory(long resourceId, const std::string& action, const std::string& detail, bool persist) {
    ResourceHistoryEntry h{resourceId, action, detail, util::nowEpoch()};
    if (persist) ctx_.repos.resources->insertHistory(h);
    ctx_.store.history(resourceId).insertBack(h);
}

Resource ResourceService::create(const ResourceInput& in, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    validate(in, 0);
    const ResourceType* t = ctx_.store.resourceTypes.findFirst([&](const ResourceType& x) { return x.id == in.typeId; });
    Resource r;
    r.code = util::trim(in.code); r.name = util::trim(in.name);
    r.typeId = t->id; r.typeName = t->name; r.category = t->category;
    r.capacity = in.capacity; r.available = in.available < 0 ? in.capacity : in.available;
    parse(in.status, r.status);
    r.lat = in.lat; r.lon = in.lon;
    r.capabilities = util::normaliseList(in.capabilities.empty() ? t->defaultCapabilities : in.capabilities);
    r.organization = util::trim(in.organization);
    r.updatedAt = util::nowEpoch();
    ctx_.repos.resources->insert(r);                  // 1. MySQL
    ctx_.store.resources.upsert(r);                   // 2. registry (array + hash + AVL)
    ctx_.store.resourceByCode.insert(r.code, r.id);   // 3. code hash
    indexText(r);                                     // 4. trie
    spatial_.indexResource(r);                        // 5. KD + Quad
    recordHistory(r.id, "REGISTERED", "Registered with " + std::to_string(r.capacity) + " unit(s)");
    audit_.record("AUDIT", "resources", "registered " + r.code, actor);
    ctx_.sink.publish(LiveEvent{"resource.updated", r.id, r.code + " " + r.name, "registered", r.lat, r.lon});
    return r;
}

Resource ResourceService::update(long id, const ResourceInput& in, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    Resource* cur = ctx_.store.resources.get(id);
    if (!cur) throw NotFoundError("Resource " + std::to_string(id) + " not found");
    validate(in, id);
    Resource before = *cur;
    int committed = before.capacity - before.available;           // units out on deployments
    if (in.capacity < committed) throw ConflictError("Capacity cannot drop below the " + std::to_string(committed) + " unit(s) currently deployed");
    const ResourceType* t = ctx_.store.resourceTypes.findFirst([&](const ResourceType& x) { return x.id == in.typeId; });
    Resource r = before;
    r.code = util::trim(in.code); r.name = util::trim(in.name);
    r.typeId = t->id; r.typeName = t->name; r.category = t->category;
    r.capacity = in.capacity;
    r.available = in.capacity - committed;                          // availability follows deployments, not the form
    ResourceStatus st = ResourceStatus::Available; parse(in.status, st);
    if (committed > 0 && st != ResourceStatus::Deployed && st != ResourceStatus::Available) throw ConflictError("Resource has active deployments; release them first");
    r.status = committed > 0 ? (r.available > 0 ? ResourceStatus::Available : ResourceStatus::Deployed) : (st == ResourceStatus::Deployed ? ResourceStatus::Available : st);
    r.lat = in.lat; r.lon = in.lon;
    r.capabilities = util::normaliseList(in.capabilities);
    r.organization = util::trim(in.organization);
    r.updatedAt = util::nowEpoch();
    ctx_.repos.resources->update(r);
    unindexText(before); spatial_.unindexResource(before);
    ctx_.store.resourceByCode.erase(before.code);
    *cur = r;
    ctx_.store.resourceByCode.insert(r.code, r.id);
    indexText(r); spatial_.indexResource(r);
    recordHistory(id, "UPDATED", "Details updated");
    audit_.record("AUDIT", "resources", "updated " + r.code, actor);
    ctx_.sink.publish(LiveEvent{"resource.updated", r.id, r.code + " " + r.name, toString(r.status), r.lat, r.lon});
    return r;
}

Resource ResourceService::setStatus(long id, ResourceStatus st, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    Resource* cur = ctx_.store.resources.get(id);
    if (!cur) throw NotFoundError("Resource " + std::to_string(id) + " not found");
    if (st == ResourceStatus::Deployed) throw ValidationError("DEPLOYED is set by the allocation engine, not manually");
    if (cur->available < cur->capacity && st != ResourceStatus::Available) throw ConflictError("Resource has units on active deployments; release them first");
    Resource r = *cur;
    r.status = st; r.updatedAt = util::nowEpoch();
    ctx_.repos.resources->update(r);
    *cur = r;
    recordHistory(id, "STATUS", std::string("Status → ") + toString(st));
    audit_.record("AUDIT", "resources", r.code + " status " + toString(st), actor);
    ctx_.sink.publish(LiveEvent{"resource.updated", r.id, r.code + " " + r.name, toString(st), r.lat, r.lon});
    return r;
}

Resource ResourceService::deactivate(long id, const std::string& actor) { return setStatus(id, ResourceStatus::Inactive, actor); }

Resource ResourceService::get(long id) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    const Resource* r = ctx_.store.resources.get(id);
    if (!r) throw NotFoundError("Resource " + std::to_string(id) + " not found");
    return *r;
}

Resource ResourceService::getByCode(const std::string& code) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    const long* id = ctx_.store.resourceByCode.find(util::trim(code));
    if (!id) throw NotFoundError("No resource with code " + code);
    return *ctx_.store.resources.get(*id);
}

DynamicArray<ResourceType> ResourceService::types() { std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx); return ctx_.store.resourceTypes; }

DynamicArray<ds::Trie::Match> ResourceService::suggest(const std::string& prefix) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    return ctx_.store.resourceWords.withPrefix(util::trim(prefix), 8);
}

DynamicArray<Resource> ResourceService::list(const ResourceQuery& q, ResourceListStats* stats) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    auto t0 = std::chrono::steady_clock::now();
    ResourceListStats st;
    DynamicArray<Resource> rows;
    auto keep = [&](const Resource& r) {
        if (!q.category.empty() && r.category != q.category) return false;
        if (!q.status.empty() && std::string(toString(r.status)) != q.status) return false;
        if (q.availableOnly && (r.status != ResourceStatus::Available || r.available <= 0)) return false;
        if (!q.capability.empty()) { bool ok = false; for (const auto& c : util::splitList(r.capabilities)) if (c == util::lower(q.capability)) ok = true; if (!ok) return false; }
        return true;
    };
    if (!q.prefix.empty()) {
        st.index = "trie";
        ds::HashTable<long, bool> seen;
        for (const auto& m : ctx_.store.resourceWords.withPrefix(q.prefix, 500))
            for (long id : m.ids) if (!seen.contains(id)) { seen.insert(id, true); const Resource* r = ctx_.store.resources.get(id); ++st.scanned; if (r && keep(*r)) rows.pushBack(*r); }
    } else {
        st.index = "linear_search";        // filters over the registry array
        for (const auto& r : ctx_.store.resources.items()) { ++st.scanned; if (keep(r)) rows.pushBack(r); }
    }
    algo::OpCounter c;
    st.sortAlgorithm = q.algorithm;
    auto alg = algo::parseSortAlgorithm(q.algorithm);
    if (q.sortBy == "capacity" || q.sortBy == "available") {
        bool cap = q.sortBy == "capacity";
        if (q.algorithm == "radix") {
            // radix sorts ascending; key = (MAX - value) gives descending order
            algo::radixSortBy(rows, [cap](const Resource& r) { return 1000000L - (cap ? r.capacity : r.available); }, &c);
        } else algo::sortWith(alg, rows, [cap](const Resource& a, const Resource& b) { return cap ? a.capacity > b.capacity : a.available > b.available; }, &c);
    } else if (q.sortBy == "distance" && geo::validLatLon(q.nearLat, q.nearLon)) {
        auto d = [&](const Resource& r) { return geo::haversineKm(q.nearLat, q.nearLon, r.lat, r.lon); };
        if (q.algorithm == "bucket") algo::bucketSortBy(rows, d, 0.0, 60.0, 12, &c);
        else algo::sortWith(alg, rows, [&](const Resource& a, const Resource& b) { return d(a) < d(b); }, &c);
    } else if (q.sortBy == "utilisation") {
        algo::sortWith(alg, rows, [](const Resource& a, const Resource& b) {
            return (double)(a.capacity - a.available) / a.capacity > (double)(b.capacity - b.available) / b.capacity; }, &c);
    } else {
        algo::sortWith(alg, rows, [](const Resource& a, const Resource& b) { return a.code < b.code; }, &c);
    }
    st.comparisons = c.comparisons;
    if (q.limit && rows.size() > q.limit) { DynamicArray<Resource> cut; for (std::size_t i = 0; i < q.limit; ++i) cut.pushBack(rows[i]); rows = cut; }
    st.microseconds = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    if (stats) *stats = st;
    return rows;
}

ResourceDetail ResourceService::detail(long id) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    ResourceDetail d;
    d.resource = get(id);
    ctx_.store.history(id).forEach([&](const ResourceHistoryEntry& h) { d.history.pushBack(h); });
    ctx_.store.links.forEachIncidentOf(id, [&](long, long depId) { d.activeDeployments.pushBack(depId); });
    if (auto** t = ctx_.store.commitments.find(id)) (*t)->inorder([&](const ds::Interval& iv) { d.commitments.pushBack(iv); });
    return d;
}

} // namespace nexa::svc
