#pragma once
// ============================================================================
// NEXA :: MultiLinkedList<V>  (orthogonal list)
// Represents the many-to-many relationship incident <-> resource.
// Every relationship node carries TWO links:
//     nextForIncident  -> next resource assigned to the same incident
//     nextForResource  -> next incident served by the same resource
// so we can traverse "all resources of incident X" and
// "all incidents of resource Y" without scanning everything.
//
//   link(i, r) .................. O(Hi + Hr)  (header lookup, list heads)
//   unlink(i, r) ................ O(Hi + Hr + deg)
//   resourcesOf(i) .............. O(Hi + deg(i))
//   incidentsOf(r) .............. O(Hr + deg(r))
//  Hi/Hr = number of header nodes (incidents/resources with >=1 link)
// ============================================================================
#include <cstddef>
#include "DynamicArray.hpp"

namespace nexa::ds {

template <typename V>
class MultiLinkedList {
    struct Rel {
        long incidentId, resourceId;
        V value;
        Rel* nextForIncident;
        Rel* nextForResource;
    };
    struct Header { long id; Rel* first; Header* next; };

public:
    MultiLinkedList() = default;
    MultiLinkedList(const MultiLinkedList&) = delete;
    MultiLinkedList& operator=(const MultiLinkedList&) = delete;
    ~MultiLinkedList() { clear(); }

    /// Creates or updates the relationship node.
    void link(long incidentId, long resourceId, const V& value) {
        if (Rel* existing = findRel(incidentId, resourceId)) { existing->value = value; return; }
        Header* hi = header(incidentHeads_, incidentId, true);
        Header* hr = header(resourceHeads_, resourceId, true);
        Rel* r = new Rel{incidentId, resourceId, value, hi->first, hr->first};
        hi->first = r;
        hr->first = r;
        ++count_;
    }

    bool unlink(long incidentId, long resourceId) {
        Header* hi = header(incidentHeads_, incidentId, false);
        Header* hr = header(resourceHeads_, resourceId, false);
        if (!hi || !hr) return false;
        Rel* target = nullptr;
        // detach from incident chain
        for (Rel** pp = &hi->first; *pp; pp = &(*pp)->nextForIncident)
            if ((*pp)->resourceId == resourceId) { target = *pp; *pp = target->nextForIncident; break; }
        if (!target) return false;
        // detach from resource chain
        for (Rel** pp = &hr->first; *pp; pp = &(*pp)->nextForResource)
            if (*pp == target) { *pp = target->nextForResource; break; }
        delete target;
        --count_;
        return true;
    }

    V* find(long incidentId, long resourceId) {
        Rel* r = findRel(incidentId, resourceId);
        return r ? &r->value : nullptr;
    }

    /// fn(resourceId, value)
    template <typename Fn> void forEachResourceOf(long incidentId, Fn fn) const {
        Header* h = headerConst(incidentHeads_, incidentId);
        for (Rel* r = h ? h->first : nullptr; r; r = r->nextForIncident) fn(r->resourceId, r->value);
    }
    /// fn(incidentId, value)
    template <typename Fn> void forEachIncidentOf(long resourceId, Fn fn) const {
        Header* h = headerConst(resourceHeads_, resourceId);
        for (Rel* r = h ? h->first : nullptr; r; r = r->nextForResource) fn(r->incidentId, r->value);
    }
    /// fn(incidentId, resourceId, value)
    template <typename Fn> void forEach(Fn fn) const {
        for (Header* h = incidentHeads_; h; h = h->next)
            for (Rel* r = h->first; r; r = r->nextForIncident) fn(r->incidentId, r->resourceId, r->value);
    }

    std::size_t degreeOfIncident(long id) const { std::size_t d = 0; forEachResourceOf(id, [&](long, const V&) { ++d; }); return d; }
    std::size_t degreeOfResource(long id) const { std::size_t d = 0; forEachIncidentOf(id, [&](long, const V&) { ++d; }); return d; }
    std::size_t size() const { return count_; }

    void clear() {
        for (Header* h = incidentHeads_; h;) {
            for (Rel* r = h->first; r;) { Rel* n = r->nextForIncident; delete r; r = n; }
            Header* n = h->next; delete h; h = n;
        }
        for (Header* h = resourceHeads_; h;) { Header* n = h->next; delete h; h = n; }
        incidentHeads_ = resourceHeads_ = nullptr;
        count_ = 0;
    }

private:
    Header* incidentHeads_ = nullptr;
    Header* resourceHeads_ = nullptr;
    std::size_t count_ = 0;

    static Header* headerConst(Header* list, long id) {
        for (Header* h = list; h; h = h->next) if (h->id == id) return h;
        return nullptr;
    }
    Header* header(Header*& list, long id, bool create) {
        Header* h = headerConst(list, id);
        if (!h && create) { h = new Header{id, nullptr, list}; list = h; }
        return h;
    }
    Rel* findRel(long incidentId, long resourceId) const {
        Header* h = headerConst(incidentHeads_, incidentId);
        for (Rel* r = h ? h->first : nullptr; r; r = r->nextForIncident)
            if (r->resourceId == resourceId) return r;
        return nullptr;
    }
};

} // namespace nexa::ds
