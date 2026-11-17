#pragma once
// ============================================================================
// NEXA :: Graph  — weighted road network
// Vertices = geographic locations (stations, hospitals, junctions, incident
// sites). Edges = road segments.
// Two representations are kept and can be compared in the benchmark:
//   * Adjacency List   : per-vertex singly linked edge list.  space O(V+E)
//   * Adjacency Matrix : dense V x V weight array, built on demand. space O(V^2)
// External location ids (database ids) are mapped to dense indices 0..V-1
// through a HashTable.
//
// Weight modes: DISTANCE (km), TIME (minutes, from speed estimates in the
// seed data — NOT live traffic) and COST (distance + configured penalty).
// Blocked edges (e.g. flooded road) are skipped by every algorithm.
// ============================================================================
#include <cstddef>
#include <limits>
#include <string>
#include <stdexcept>
#include "DynamicArray.hpp"
#include "HashTable.hpp"

namespace nexa::ds {

enum class WeightMode { Distance, Time, Cost };

inline WeightMode parseWeightMode(const std::string& s) {
    if (s == "time") return WeightMode::Time;
    if (s == "cost") return WeightMode::Cost;
    return WeightMode::Distance;
}

struct EdgeData {
    long   roadId        = 0;
    double distanceKm    = 0;
    double travelMinutes = 0;
    double penalty       = 0;
    bool   blocked       = false;

    double weight(WeightMode m) const {
        switch (m) {
            case WeightMode::Time: return travelMinutes;
            case WeightMode::Cost: return distanceKm + penalty;
            default:               return distanceKm;
        }
    }
};

struct VertexInfo {
    long id = 0;
    std::string name;
    std::string kind;   // station | hospital | junction | incident | shelter
    double lat = 0, lon = 0;
};

class Graph {
public:
    struct EdgeNode { int to; EdgeData data; EdgeNode* next; };
    static constexpr double INF = std::numeric_limits<double>::infinity();

    Graph() = default;
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;
    ~Graph() { clear(); }

    /// Returns dense index of vertex; adds it if new.  O(1) average
    int addVertex(const VertexInfo& info) {
        if (int* existing = index_.find(info.id)) { vertices_[*existing] = info; return *existing; }
        int idx = (int)vertices_.size();
        vertices_.pushBack(info);
        heads_.pushBack(nullptr);
        index_.insert(info.id, idx);
        return idx;
    }

    /// Adds an undirected road (two directed arcs). Throws for unknown vertices
    /// or negative weights (Dijkstra requires non-negative weights).
    void addEdge(long fromId, long toId, const EdgeData& d, bool bidirectional = true) {
        if (d.distanceKm < 0 || d.travelMinutes < 0 || d.penalty < 0)
            throw std::invalid_argument("Negative edge weights are not allowed in the NEXA road graph");
        int u = indexOf(fromId), v = indexOf(toId);
        if (u < 0 || v < 0) throw std::invalid_argument("addEdge: unknown vertex");
        heads_[u] = new EdgeNode{v, d, heads_[u]};
        ++edgeCount_;
        if (bidirectional && u != v) { heads_[v] = new EdgeNode{u, d, heads_[v]}; ++edgeCount_; }
    }

    /// Removes every arc u->v and v->u. Returns number of arcs removed.
    int removeEdge(long fromId, long toId) {
        int u = indexOf(fromId), v = indexOf(toId);
        if (u < 0 || v < 0) return 0;
        return removeArcs(u, v) + removeArcs(v, u);
    }

    /// Marks a road blocked/unblocked (both directions). Returns true if found.
    bool setBlocked(long roadId, bool blocked) {
        bool found = false;
        for (std::size_t u = 0; u < heads_.size(); ++u)
            for (EdgeNode* e = heads_[u]; e; e = e->next)
                if (e->data.roadId == roadId) { e->data.blocked = blocked; found = true; }
        return found;
    }

    int indexOf(long id) const { const int* p = index_.find(id); return p ? *p : -1; }
    const VertexInfo& vertex(int idx) const { return vertices_.at((std::size_t)idx); }
    int vertexCount() const { return (int)vertices_.size(); }
    std::size_t arcCount() const { return edgeCount_; }
    EdgeNode* adjacency(int u) const { return heads_[(std::size_t)u]; }

    /// Dense adjacency matrix (INF = no edge). Built from the list. O(V^2 + E)
    DynamicArray<double> buildAdjacencyMatrix(WeightMode mode) const {
        int n = vertexCount();
        DynamicArray<double> m((std::size_t)n * n);
        for (long i = 0; i < (long)n * n; ++i) m.pushBack(INF);
        for (int u = 0; u < n; ++u) {
            m[(std::size_t)u * n + u] = 0;
            for (EdgeNode* e = heads_[u]; e; e = e->next) {
                if (e->data.blocked) continue;
                double w = e->data.weight(mode);
                double& cell = m[(std::size_t)u * n + e->to];
                if (w < cell) cell = w;
            }
        }
        return m;
    }

    void clear() {
        for (std::size_t u = 0; u < heads_.size(); ++u) {
            EdgeNode* e = heads_[u];
            while (e) { EdgeNode* n = e->next; delete e; e = n; }
        }
        heads_.clear(); vertices_.clear(); index_.clear(); edgeCount_ = 0;
    }

private:
    DynamicArray<VertexInfo> vertices_;
    DynamicArray<EdgeNode*> heads_;
    HashTable<long, int> index_{CollisionStrategy::SeparateChaining, 64};
    std::size_t edgeCount_ = 0;

    int removeArcs(int u, int v) {
        int removed = 0;
        for (EdgeNode** pp = &heads_[u]; *pp;) {
            if ((*pp)->to == v) { EdgeNode* d = *pp; *pp = d->next; delete d; ++removed; --edgeCount_; }
            else pp = &(*pp)->next;
        }
        return removed;
    }
};

} // namespace nexa::ds
