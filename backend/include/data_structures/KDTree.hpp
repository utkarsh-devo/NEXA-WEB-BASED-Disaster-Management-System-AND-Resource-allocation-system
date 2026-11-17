#pragma once
// ============================================================================
// NEXA :: KDTree (k = 3 on unit-sphere vectors, see utilities/Geo.hpp)
// Application: nearest resources / hospitals to an incident, K-NN and
// radius queries.
//
//  build ............. O(n log^2 n) (sort by axis at each level), balanced
//  insert ............ O(h)  — h grows if many inserts after build
//  remove ............ lazy tombstone, O(h); auto-rebuild when > 50% dead
//  nearest / kNearest  ~O(log n + k) on well-distributed data, but O(n) in the
//                      worst case (skewed data / unlucky queries). Not always
//                      logarithmic — see spec rule 8.
//  radius query ...... O(sqrt-ish + m) typical, O(n) worst
// ============================================================================
#include <cstddef>
#include <cmath>
#include "DynamicArray.hpp"
#include "BinaryHeap.hpp"
#include "../algorithms/Sorting.hpp"
#include "../utilities/Geo.hpp"

namespace nexa::ds {

struct GeoPoint {
    long id = 0;
    double lat = 0, lon = 0;
    int tag = 0;            // caller-defined category (resource / hospital / incident)
};

struct GeoHit {
    GeoPoint point;
    double distanceKm = 0;
    bool operator<(const GeoHit& o) const { return distanceKm < o.distanceKm; }
};

class KDTree {
    struct Node {
        GeoPoint p; geo::Vec3 v; int axis; bool dead = false;
        Node* left = nullptr; Node* right = nullptr;
    };
public:
    KDTree() = default;
    KDTree(const KDTree&) = delete;
    KDTree& operator=(const KDTree&) = delete;
    ~KDTree() { destroy(root_); }

    void build(const DynamicArray<GeoPoint>& pts) {
        destroy(root_); root_ = nullptr; size_ = dead_ = 0;
        DynamicArray<Node*> nodes;
        for (const GeoPoint& p : pts) nodes.pushBack(makeNode(p));
        root_ = buildRec(nodes, 0, (long)nodes.size() - 1, 0);
        size_ = pts.size();
    }

    void insert(const GeoPoint& p) {
        Node* n = makeNode(p);
        if (!root_) { n->axis = 0; root_ = n; ++size_; return; }
        Node* cur = root_;
        while (true) {
            int ax = cur->axis;
            bool goLeft = coord(n->v, ax) < coord(cur->v, ax);
            Node*& next = goLeft ? cur->left : cur->right;
            if (!next) { n->axis = (ax + 1) % 3; next = n; break; }
            cur = next;
        }
        ++size_;
    }

    /// Lazy delete by id. Returns true if found.
    bool remove(long id) {
        Node* n = findById(root_, id);
        if (!n) return false;
        n->dead = true; ++dead_;
        if (dead_ * 2 > size_) rebuild();
        return true;
    }

    std::size_t size() const { return size_ - dead_; }

    /// k nearest live points, optionally filtered. Results sorted by distance.
    template <typename Filter>
    DynamicArray<GeoHit> kNearest(double lat, double lon, std::size_t k, Filter accept, std::size_t* visited = nullptr) const {
        BinaryHeap<GeoHit, MaxCompare<GeoHit>> heap;    // max-heap of current best k (by chord, stored in distanceKm)
        geo::Vec3 q = geo::toUnitVector(lat, lon);
        std::size_t vis = 0;
        if (k > 0) knnRec(root_, q, k, accept, heap, vis);
        if (visited) *visited = vis;
        DynamicArray<GeoHit> out;
        while (!heap.empty()) out.pushBack(heap.extract());
        // heap gives farthest first -> reverse, convert chord -> km
        DynamicArray<GeoHit> res;
        for (long i = (long)out.size() - 1; i >= 0; --i) {
            GeoHit h = out[i];
            h.distanceKm = geo::chordToKm(h.distanceKm);
            res.pushBack(h);
        }
        return res;
    }
    DynamicArray<GeoHit> kNearest(double lat, double lon, std::size_t k) const {
        return kNearest(lat, lon, k, [](const GeoPoint&) { return true; });
    }

    /// All live points within radiusKm, sorted by distance.
    template <typename Filter>
    DynamicArray<GeoHit> withinRadius(double lat, double lon, double radiusKm, Filter accept) const {
        DynamicArray<GeoHit> out;
        geo::Vec3 q = geo::toUnitVector(lat, lon);
        radiusRec(root_, q, geo::kmToChord(radiusKm), accept, out);
        for (auto& h : out) h.distanceKm = geo::chordToKm(h.distanceKm);
        algo::mergeSort(out, [](const GeoHit& a, const GeoHit& b) { return a.distanceKm < b.distanceKm; });
        return out;
    }
    DynamicArray<GeoHit> withinRadius(double lat, double lon, double radiusKm) const {
        return withinRadius(lat, lon, radiusKm, [](const GeoPoint&) { return true; });
    }

    template <typename Fn> void forEach(Fn fn) const { walk(root_, fn); }
    int height() const { return heightRec(root_); }

private:
    Node* root_ = nullptr;
    std::size_t size_ = 0, dead_ = 0;

    static double coord(const geo::Vec3& v, int ax) { return ax == 0 ? v.x : (ax == 1 ? v.y : v.z); }
    static double chord(const geo::Vec3& a, const geo::Vec3& b) {
        double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    static Node* makeNode(const GeoPoint& p) { Node* n = new Node; n->p = p; n->v = geo::toUnitVector(p.lat, p.lon); n->axis = 0; return n; }

    static Node* buildRec(DynamicArray<Node*>& a, long lo, long hi, int depth) {
        if (lo > hi) return nullptr;
        int ax = depth % 3;
        // sort the sub-range by the current axis, take the median
        DynamicArray<Node*> part;
        for (long i = lo; i <= hi; ++i) part.pushBack(a[i]);
        algo::quickSort(part, [ax](Node* x, Node* y) { return coord(x->v, ax) < coord(y->v, ax); });
        for (long i = lo; i <= hi; ++i) a[i] = part[i - lo];
        long mid = lo + (hi - lo) / 2;
        Node* n = a[mid];
        n->axis = ax;
        n->left = buildRec(a, lo, mid - 1, depth + 1);
        n->right = buildRec(a, mid + 1, hi, depth + 1);
        return n;
    }

    template <typename Filter, typename Heap>
    static void knnRec(Node* n, const geo::Vec3& q, std::size_t k, Filter& accept, Heap& heap, std::size_t& vis) {
        if (!n) return;
        ++vis;
        if (!n->dead && accept(n->p)) {
            double d = chord(q, n->v);
            if (heap.size() < k) heap.insert(GeoHit{n->p, d});
            else if (d < heap.peek().distanceKm) { heap.extract(); heap.insert(GeoHit{n->p, d}); }
        }
        double diff = coord(q, n->axis) - coord(n->v, n->axis);
        Node* nearSide = diff < 0 ? n->left : n->right;
        Node* farSide  = diff < 0 ? n->right : n->left;
        knnRec(nearSide, q, k, accept, heap, vis);
        // prune: the far side can only help if the splitting plane is closer than the current k-th best
        if (heap.size() < k || std::fabs(diff) < heap.peek().distanceKm) knnRec(farSide, q, k, accept, heap, vis);
    }

    template <typename Filter>
    static void radiusRec(Node* n, const geo::Vec3& q, double r, Filter& accept, DynamicArray<GeoHit>& out) {
        if (!n) return;
        double d = chord(q, n->v);
        if (!n->dead && d <= r && accept(n->p)) out.pushBack(GeoHit{n->p, d});
        double diff = coord(q, n->axis) - coord(n->v, n->axis);
        if (diff - r <= 0) radiusRec(n->left, q, r, accept, out);   // query ball reaches left half-space
        if (diff + r >= 0) radiusRec(n->right, q, r, accept, out);
    }

    static Node* findById(Node* n, long id) {
        if (!n) return nullptr;
        if (!n->dead && n->p.id == id) return n;
        if (Node* l = findById(n->left, id)) return l;
        return findById(n->right, id);
    }
    template <typename Fn> static void walk(Node* n, Fn& fn) { if (!n) return; if (!n->dead) fn(n->p); walk(n->left, fn); walk(n->right, fn); }
    static int heightRec(Node* n) { if (!n) return 0; int a = heightRec(n->left), b = heightRec(n->right); return 1 + (a > b ? a : b); }
    static void destroy(Node* n) { if (!n) return; destroy(n->left); destroy(n->right); delete n; }

    void rebuild() {
        DynamicArray<GeoPoint> live;
        auto collect = [&](const GeoPoint& p) { live.pushBack(p); };
        walk(root_, collect);
        build(live);
    }
};

} // namespace nexa::ds
