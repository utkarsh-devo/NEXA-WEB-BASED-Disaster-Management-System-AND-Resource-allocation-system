#pragma once
// ============================================================================
// NEXA :: RangeTree2D — static 2-D range tree (lat = primary, lon = secondary)
// Primary structure: balanced BST over points sorted by latitude.
// Each node stores an associated array of its subtree's points sorted by
// longitude, so the secondary dimension is answered by binary search.
// Application: analytical rectangle queries, e.g. "incidents with
// severity-weighted coordinates inside this district box", and as the exact
// alternative to the QuadTree for region queries.
//
//  build ........ O(n log n) time and space
//  query ........ O(log^2 n + m)  (m = reported points) — guaranteed,
//                 unlike QuadTree/KDTree which can degrade.
//  Static: rebuild after bulk changes (NEXA rebuilds on index refresh).
// ============================================================================
#include <cstddef>
#include "DynamicArray.hpp"
#include "KDTree.hpp"     // GeoPoint
#include "QuadTree.hpp"   // GeoRect
#include "../algorithms/Sorting.hpp"

namespace nexa::ds {

class RangeTree2D {
    struct Node {
        double splitLat;             // max lat in left subtree (leaf: its point's lat)
        GeoPoint* leafPoint = nullptr;
        DynamicArray<GeoPoint> byLon;     // associated structure
        Node* left = nullptr; Node* right = nullptr;
    };
public:
    RangeTree2D() = default;
    RangeTree2D(const RangeTree2D&) = delete;
    RangeTree2D& operator=(const RangeTree2D&) = delete;
    ~RangeTree2D() { destroy(root_); }

    void build(const DynamicArray<GeoPoint>& pts) {
        destroy(root_); root_ = nullptr;
        points_ = pts;
        algo::mergeSort(points_, [](const GeoPoint& a, const GeoPoint& b) { return a.lat < b.lat; });
        if (!points_.empty()) root_ = buildRec(0, (long)points_.size() - 1);
    }

    std::size_t size() const { return points_.size(); }

    /// Reports every point with minLat<=lat<=maxLat and minLon<=lon<=maxLon.
    DynamicArray<GeoPoint> query(const GeoRect& r, std::size_t* nodesVisited = nullptr) const {
        DynamicArray<GeoPoint> out;
        std::size_t visited = 0;
        if (!root_) return out;
        // find split node where search paths for minLat and maxLat diverge
        // (comparisons are chosen so duplicate latitudes on both sides are reported)
        Node* v = root_;
        while (!isLeaf(v) && (r.maxLat < v->splitLat || r.minLat > v->splitLat)) {
            ++visited;
            v = r.maxLat < v->splitLat ? v->left : v->right;
        }
        if (isLeaf(v)) { reportLeaf(v, r, out); if (nodesVisited) *nodesVisited = visited + 1; return out; }
        // left path: every right child hanging off is fully inside lat range
        for (Node* n = v->left; n; ) {
            ++visited;
            if (isLeaf(n)) { reportLeaf(n, r, out); break; }
            if (r.minLat <= n->splitLat) { reportLon(n->right, r, out); n = n->left; }
            else n = n->right;
        }
        // right path: every left child hanging off is fully inside lat range
        for (Node* n = v->right; n; ) {
            ++visited;
            if (isLeaf(n)) { reportLeaf(n, r, out); break; }
            if (r.maxLat >= n->splitLat) { reportLon(n->left, r, out); n = n->right; }
            else n = n->left;
        }
        if (nodesVisited) *nodesVisited = visited;
        return out;
    }

private:
    Node* root_ = nullptr;
    DynamicArray<GeoPoint> points_;   // sorted by lat; leaves point into it

    static bool isLeaf(const Node* n) { return n->leafPoint != nullptr; }

    Node* buildRec(long lo, long hi) {
        Node* n = new Node;
        if (lo == hi) {
            n->leafPoint = &points_[lo];
            n->splitLat = points_[lo].lat;
            n->byLon.pushBack(points_[lo]);
            return n;
        }
        long mid = lo + (hi - lo) / 2;
        n->splitLat = points_[mid].lat;
        n->left = buildRec(lo, mid);
        n->right = buildRec(mid + 1, hi);
        // merge children's lon-sorted arrays (like merge sort) -> O(n) per level
        const auto& a = n->left->byLon; const auto& b = n->right->byLon;
        std::size_t i = 0, j = 0;
        while (i < a.size() && j < b.size()) n->byLon.pushBack(b[j].lon < a[i].lon ? b[j++] : a[i++]);
        while (i < a.size()) n->byLon.pushBack(a[i++]);
        while (j < b.size()) n->byLon.pushBack(b[j++]);
        return n;
    }
    static void reportLeaf(const Node* n, const GeoRect& r, DynamicArray<GeoPoint>& out) {
        if (r.contains(n->leafPoint->lat, n->leafPoint->lon)) out.pushBack(*n->leafPoint);
    }
    /// binary search the associated lon array, then scan the matching run
    static void reportLon(const Node* n, const GeoRect& r, DynamicArray<GeoPoint>& out) {
        if (!n) return;
        const auto& arr = n->byLon;
        long lo = 0, hi = (long)arr.size();
        while (lo < hi) { long mid = lo + (hi - lo) / 2; if (arr[mid].lon < r.minLon) lo = mid + 1; else hi = mid; }
        for (long i = lo; i < (long)arr.size() && arr[i].lon <= r.maxLon; ++i) out.pushBack(arr[i]);
    }
    static void destroy(Node* n) { if (!n) return; destroy(n->left); destroy(n->right); delete n; }
};

} // namespace nexa::ds
