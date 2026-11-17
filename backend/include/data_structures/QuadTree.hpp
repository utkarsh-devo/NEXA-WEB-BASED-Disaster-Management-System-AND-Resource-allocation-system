#pragma once
// ============================================================================
// NEXA :: QuadTree — point quadtree over a lat/lon bounding box
// Each node covers a rectangle; when it holds more than CAPACITY points it
// splits into NW/NE/SW/SE children (up to MAX_DEPTH).
// Application: "resources/incidents inside the map region the operator drew",
// map density grid (leaf cells with counts) for cluster display.
//
//  insert ............. O(depth)
//  queryRect .......... O(depth + cells visited + m); worst case O(n)
//                       (all points in one deep branch) — not always log n.
//  remove ............. O(depth + capacity)
// ============================================================================
#include <cstddef>
#include "DynamicArray.hpp"
#include "KDTree.hpp"   // GeoPoint
#include "../utilities/Geo.hpp"

namespace nexa::ds {

struct GeoRect {
    double minLat = 0, minLon = 0, maxLat = 0, maxLon = 0;
    bool contains(double lat, double lon) const { return lat >= minLat && lat <= maxLat && lon >= minLon && lon <= maxLon; }
    bool intersects(const GeoRect& o) const { return !(o.minLat > maxLat || o.maxLat < minLat || o.minLon > maxLon || o.maxLon < minLon); }
};

struct QuadCell { GeoRect bounds; std::size_t count = 0; int depth = 0; };

class QuadTree {
    static constexpr std::size_t CAPACITY = 4;
    static constexpr int MAX_DEPTH = 14;
    struct Node {
        GeoRect b; int depth;
        DynamicArray<GeoPoint> pts;
        Node* child[4] = {nullptr, nullptr, nullptr, nullptr};
        bool leaf() const { return child[0] == nullptr; }
    };
public:
    explicit QuadTree(GeoRect bounds = GeoRect{-90, -180, 90, 180}) { root_ = new Node{bounds, 0, {}}; }
    QuadTree(const QuadTree&) = delete;
    QuadTree& operator=(const QuadTree&) = delete;
    ~QuadTree() { destroy(root_); }

    bool insert(const GeoPoint& p) {
        if (!root_->b.contains(p.lat, p.lon)) return false;
        insertRec(root_, p);
        ++size_;
        return true;
    }

    bool remove(long id, double lat, double lon) {
        bool ok = removeRec(root_, id, lat, lon);
        if (ok) --size_;
        return ok;
    }

    template <typename Filter>
    DynamicArray<GeoPoint> queryRect(const GeoRect& r, Filter accept) const {
        DynamicArray<GeoPoint> out;
        rectRec(root_, r, accept, out);
        return out;
    }
    DynamicArray<GeoPoint> queryRect(const GeoRect& r) const { return queryRect(r, [](const GeoPoint&) { return true; }); }

    /// Circle query: bounding-box prune, then exact Haversine check.
    template <typename Filter>
    DynamicArray<GeoHit> queryRadius(double lat, double lon, double km, Filter accept) const {
        double dLat = km / 111.195;
        double cosLat = std::cos(geo::toRad(lat));
        double dLon = cosLat > 1e-6 ? km / (111.195 * cosLat) : 180.0;
        GeoRect box{lat - dLat, lon - dLon, lat + dLat, lon + dLon};
        DynamicArray<GeoHit> out;
        auto pts = queryRect(box, accept);
        for (const GeoPoint& p : pts) {
            double d = geo::haversineKm(lat, lon, p.lat, p.lon);
            if (d <= km) out.pushBack(GeoHit{p, d});
        }
        return out;
    }

    /// Leaf cells containing points — used as a density grid on the map.
    DynamicArray<QuadCell> leafCells() const { DynamicArray<QuadCell> out; leavesRec(root_, out); return out; }

    std::size_t size() const { return size_; }
    int depth() const { return depthRec(root_); }
    void clear() { GeoRect b = root_->b; destroy(root_); root_ = new Node{b, 0, {}}; size_ = 0; }

private:
    Node* root_;
    std::size_t size_ = 0;

    static int quadrant(const Node* n, double lat, double lon) {
        double midLat = (n->b.minLat + n->b.maxLat) / 2, midLon = (n->b.minLon + n->b.maxLon) / 2;
        bool north = lat >= midLat, east = lon >= midLon;
        return north ? (east ? 1 : 0) : (east ? 3 : 2);   // 0 NW, 1 NE, 2 SW, 3 SE
    }
    static void subdivide(Node* n) {
        double midLat = (n->b.minLat + n->b.maxLat) / 2, midLon = (n->b.minLon + n->b.maxLon) / 2;
        int d = n->depth + 1;
        n->child[0] = new Node{GeoRect{midLat, n->b.minLon, n->b.maxLat, midLon}, d, {}};
        n->child[1] = new Node{GeoRect{midLat, midLon, n->b.maxLat, n->b.maxLon}, d, {}};
        n->child[2] = new Node{GeoRect{n->b.minLat, n->b.minLon, midLat, midLon}, d, {}};
        n->child[3] = new Node{GeoRect{n->b.minLat, midLon, midLat, n->b.maxLon}, d, {}};
        for (const GeoPoint& p : n->pts) insertRec(n->child[quadrant(n, p.lat, p.lon)], p);
        n->pts.clear();
    }
    static void insertRec(Node* n, const GeoPoint& p) {
        while (!n->leaf()) n = n->child[quadrant(n, p.lat, p.lon)];
        n->pts.pushBack(p);
        if (n->pts.size() > CAPACITY && n->depth < MAX_DEPTH) subdivide(n);
    }
    static bool removeRec(Node* n, long id, double lat, double lon) {
        while (!n->leaf()) n = n->child[quadrant(n, lat, lon)];
        return n->pts.removeIf([id](const GeoPoint& p) { return p.id == id; }) > 0;
    }
    template <typename Filter>
    static void rectRec(const Node* n, const GeoRect& r, Filter& accept, DynamicArray<GeoPoint>& out) {
        if (!n->b.intersects(r)) return;
        if (n->leaf()) {
            for (const GeoPoint& p : n->pts) if (r.contains(p.lat, p.lon) && accept(p)) out.pushBack(p);
            return;
        }
        for (Node* c : n->child) rectRec(c, r, accept, out);
    }
    static void leavesRec(const Node* n, DynamicArray<QuadCell>& out) {
        if (n->leaf()) { if (!n->pts.empty()) out.pushBack(QuadCell{n->b, n->pts.size(), n->depth}); return; }
        for (Node* c : n->child) leavesRec(c, out);
    }
    static int depthRec(const Node* n) {
        if (n->leaf()) return n->depth;
        int best = 0;
        for (Node* c : n->child) { int d = depthRec(c); if (d > best) best = d; }
        return best;
    }
    static void destroy(Node* n) { if (!n) return; for (Node* c : n->child) destroy(c); delete n; }
};

} // namespace nexa::ds
