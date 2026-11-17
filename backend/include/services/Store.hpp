#pragma once
// ============================================================================
// NEXA :: OperationalStore — the in-memory "engine room".
// Holds every custom data structure that indexes the live operational state.
// MySQL is the source of truth; on start-up the store is (re)built from the
// repositories, and every service updates MySQL FIRST, then the store, so
// both stay consistent (spec rule 10). A single mutex serialises access —
// simple, correct, and fast enough for this workload.
//
//  structure                 key -> value                   used by
//  ------------------------  -----------------------------  -----------------------------
//  Registry<Incident>        id -> record (array+hash+AVL)  incidents, dashboard
//  AVLTree incidentByTime    (createdAt,id) -> id           timeline / history ordering
//  Trie incidentWords        word -> incident ids           autocomplete
//  SuffixArray descriptions  substring -> incident ids      full-text pattern search
//  HashTable timelines       incident id -> LinkedList      event timeline
//  IndexedPriorityQueue      incident id -> priority        priority engine
//  Registry<Resource>        id -> record                   resources
//  HashTable resourceByCode  "AMB-101" -> id                direct lookup
//  IntervalTree (per res.)   [from,until] -> deployment id  conflict detection
//  Registry<Hospital>        id -> record                   hospitals
//  BST hospitalByName        name -> id                     ordered listing
//  KDTree x4                 resources/hospitals/incidents/road vertices
//  QuadTree spatialGrid      all entities                   region queries, density grid
//  Graph roads               road network                   routing
//  Queue / PriorityQueue     request ids                    request processing
//  MultiLinkedList links     incident <-> resource          deployments
//  SparseMatrix units        (incident, resource) -> units  allocation matrix
//  KaryTree taxonomy         category hierarchy             classification
//  Stack actions             operator action history        undo / audit
// ============================================================================
#include <mutex>
#include <string>
#include <climits>
#include "../models/Models.hpp"
#include "../data_structures/DynamicArray.hpp"
#include "../data_structures/HashTable.hpp"
#include "../data_structures/AVLTree.hpp"
#include "../data_structures/BST.hpp"
#include "../data_structures/LinkedList.hpp"
#include "../data_structures/Stack.hpp"
#include "../data_structures/Queue.hpp"
#include "../data_structures/PriorityQueue.hpp"
#include "../data_structures/Trie.hpp"
#include "../data_structures/SuffixArray.hpp"
#include "../data_structures/KDTree.hpp"
#include "../data_structures/QuadTree.hpp"
#include "../data_structures/IntervalTree.hpp"
#include "../data_structures/Graph.hpp"
#include "../data_structures/MultiLinkedList.hpp"
#include "../data_structures/SparseMatrix.hpp"
#include "../data_structures/KaryTree.hpp"

namespace nexa::svc {

using namespace nexa::model;
using ds::DynamicArray;

/// Array-backed registry: O(1) lookup through a hash index, O(1) delete by
/// swapping with the last slot, ordered iteration through an AVL index.
template <typename T>
class Registry {
public:
    T* get(long id) { std::size_t* s = slot_.find(id); return s ? &items_[*s] : nullptr; }
    const T* get(long id) const { const std::size_t* s = slot_.find(id); return s ? &items_[*s] : nullptr; }
    bool contains(long id) const { return slot_.contains(id); }

    T& upsert(const T& v) {
        if (T* existing = get(v.id)) { *existing = v; return *existing; }
        items_.pushBack(v);
        slot_.insert(v.id, items_.size() - 1);
        ordered_.insert(v.id, v.id);
        return items_.back();
    }
    bool remove(long id) {
        std::size_t* s = slot_.find(id);
        if (!s) return false;
        std::size_t idx = *s, last = items_.size() - 1;
        if (idx != last) { items_[idx] = items_[last]; slot_.insert(items_[idx].id, idx); }
        items_.popBack();
        slot_.erase(id);
        ordered_.remove(id);
        return true;
    }
    /// Ascending id order (AVL in-order traversal).
    template <typename Fn> void forEachOrdered(Fn fn) const {
        ordered_.inorder([&](long id, long) { fn(*get(id)); });
    }
    const DynamicArray<T>& items() const { return items_; }
    std::size_t size() const { return items_.size(); }
    void clear() { items_.clear(); slot_.clear(); ordered_.clear(); }

private:
    DynamicArray<T> items_;
    ds::HashTable<long, std::size_t> slot_{ds::CollisionStrategy::SeparateChaining, 64};
    ds::AVLTree<long, long> ordered_;
};

/// Open-ended commitment windows end at 2038-01-19 (fits a 32-bit `long`, as on MSVC/Windows).
constexpr long OPEN_END = 2147483000L;

struct TimeKey {
    long t = 0, id = 0;
    bool operator<(const TimeKey& o) const { return t < o.t || (t == o.t && id < o.id); }
};

struct OperatorAction {
    std::string type;        // INCIDENT_STATUS
    long entityId = 0;
    std::string previous;    // value to restore on undo
    std::string actor;
    long at = 0;
};

/// Spatial tag values stored in GeoPoint.tag
enum SpatialTag { TAG_RESOURCE = 1, TAG_HOSPITAL = 2, TAG_INCIDENT = 3 };

struct OperationalStore {
    std::recursive_mutex mtx;

    // ---- taxonomy ----
    ds::KaryTree<IncidentCategory> taxonomy{0};
    ds::HashTable<int, IncidentCategory> categoryById{ds::CollisionStrategy::LinearProbing, 32};

    // ---- incidents ----
    Registry<Incident> incidents;
    ds::AVLTree<TimeKey, long> incidentByTime;
    ds::Trie incidentWords;
    ds::SuffixArray descriptions;
    bool descriptionsDirty = true;
    ds::HashTable<long, ds::LinkedList<IncidentEvent>*> timelines{ds::CollisionStrategy::SeparateChaining, 64};
    ds::IndexedPriorityQueue incidentQueue{true};

    // ---- resources ----
    Registry<Resource> resources;
    ds::HashTable<std::string, long> resourceByCode{ds::CollisionStrategy::DoubleHashing, 64};
    ds::Trie resourceWords;
    ds::HashTable<long, ds::LinkedList<ResourceHistoryEntry>*> resourceHistory{ds::CollisionStrategy::SeparateChaining, 64};
    ds::HashTable<long, ds::IntervalTree*> commitments{ds::CollisionStrategy::SeparateChaining, 64}; // resource id -> windows (interval id = deployment id)
    DynamicArray<ResourceType> resourceTypes;

    // ---- hospitals ----
    Registry<Hospital> hospitals;
    ds::BST<std::string, long> hospitalByName;
    Registry<BedReservation> reservations;
    ds::HashTable<long, ds::IntervalTree*> bedWindows{ds::CollisionStrategy::SeparateChaining, 32};  // hospital id -> windows (interval id = reservation id)

    // ---- spatial ----
    ds::KDTree resourceKd, hospitalKd, incidentKd, vertexKd;
    ds::QuadTree spatialGrid{ds::GeoRect{-90, -180, 90, 180}};

    // ---- network ----
    ds::Graph roads;
    Registry<Location> locations;
    Registry<Road> roadRecords;

    // ---- requests ----
    Registry<EmergencyRequest> requests;
    ds::Queue<long> fifoRequests;
    ds::IndexedPriorityQueue urgentRequests{true};
    ds::HashTable<long, ds::LinkedList<std::string>*> requestHistory{ds::CollisionStrategy::SeparateChaining, 64};

    // ---- deployments ----
    Registry<Deployment> deployments;
    ds::MultiLinkedList<long> links;         // value = deployment id
    ds::SparseMatrix<int> unitsMatrix;       // (incident id, resource id) -> units committed

    // ---- operator actions ----
    ds::Stack<OperatorAction> actions;

    OperationalStore() = default;
    OperationalStore(const OperationalStore&) = delete;
    OperationalStore& operator=(const OperationalStore&) = delete;
    ~OperationalStore() { freeLists(); }

    void freeLists() {
        timelines.forEach([](long, ds::LinkedList<IncidentEvent>* l) { delete l; });
        resourceHistory.forEach([](long, ds::LinkedList<ResourceHistoryEntry>* l) { delete l; });
        requestHistory.forEach([](long, ds::LinkedList<std::string>* l) { delete l; });
        commitments.forEach([](long, ds::IntervalTree* t) { delete t; });
        bedWindows.forEach([](long, ds::IntervalTree* t) { delete t; });
        timelines.clear(); resourceHistory.clear(); requestHistory.clear(); commitments.clear(); bedWindows.clear();
    }

    ds::LinkedList<IncidentEvent>& timeline(long incidentId) {
        if (auto** l = timelines.find(incidentId)) return **l;
        auto* l = new ds::LinkedList<IncidentEvent>();
        timelines.insert(incidentId, l);
        return *l;
    }
    ds::LinkedList<ResourceHistoryEntry>& history(long resourceId) {
        if (auto** l = resourceHistory.find(resourceId)) return **l;
        auto* l = new ds::LinkedList<ResourceHistoryEntry>();
        resourceHistory.insert(resourceId, l);
        return *l;
    }
    ds::IntervalTree& windowsOf(ds::HashTable<long, ds::IntervalTree*>& table, long ownerId) {
        if (auto** t = table.find(ownerId)) return **t;
        auto* t = new ds::IntervalTree();
        table.insert(ownerId, t);
        return *t;
    }
    ds::LinkedList<std::string>& requestLog(long requestId) {
        if (auto** l = requestHistory.find(requestId)) return **l;
        auto* l = new ds::LinkedList<std::string>();
        requestHistory.insert(requestId, l);
        return *l;
    }
};

} // namespace nexa::svc
