#pragma once
// ============================================================================
// NEXA :: Graph algorithms over ds::Graph
//
//  bfs ................. O(V + E)          reachability, hop counts
//  dfs ................. O(V + E)          branch exploration (iterative, ds::Stack)
//  components .......... O(V + E)          disconnected regions
//  dijkstra ............ O((V + E) log V)  shortest path, NON-NEGATIVE weights only
//  kruskal ............. O(E log E)        minimum spanning forest (DisjointSet)
//  prim ................ O((V + E) log V)  minimum spanning forest (IndexedPriorityQueue)
//  bfsOnMatrix ......... O(V^2)            benchmark: same BFS on adjacency matrix
//
// Dijkstra returns the path that is shortest *for the chosen weights*.
// With WeightMode::Time it is the fastest route only to the extent that the
// stored travel-time estimates are valid; NEXA has no live traffic feed.
// MST results describe network connectivity cost; they are NOT routes.
// ============================================================================
#include <cstddef>
#include "OpCounter.hpp"
#include "Sorting.hpp"
#include "../data_structures/Graph.hpp"
#include "../data_structures/Queue.hpp"
#include "../data_structures/Stack.hpp"
#include "../data_structures/DisjointSet.hpp"
#include "../data_structures/PriorityQueue.hpp"

namespace nexa::algo {

using ds::Graph;
using ds::WeightMode;

struct TraversalResult {
    DynamicArray<int> order;      // visit order (dense indices)
    DynamicArray<int> parent;     // -1 for root / unreached
    DynamicArray<int> depth;      // hops from source, -1 if unreached
};

struct ShortestPathResult {
    DynamicArray<double> dist;
    DynamicArray<int> parent;
    std::size_t relaxations = 0;
    std::size_t settled = 0;
};

struct MstEdge { int u = 0, v = 0; double w = 0; long roadId = 0; };
struct MstResult {
    DynamicArray<MstEdge> edges;
    double totalWeight = 0;
    int components = 0;           // >1 means the road network is disconnected
};

namespace detail {
inline void initArrays(int n, DynamicArray<int>& parent, DynamicArray<int>& depth) {
    parent.clear(); depth.clear();
    for (int i = 0; i < n; ++i) { parent.pushBack(-1); depth.pushBack(-1); }
}
}

inline TraversalResult bfs(const Graph& g, int src, OpCounter* c = nullptr) {
    TraversalResult r;
    int n = g.vertexCount();
    detail::initArrays(n, r.parent, r.depth);
    if (src < 0 || src >= n) return r;
    ds::Queue<int> q;
    q.enqueue(src); r.depth[src] = 0;
    while (!q.isEmpty()) {
        int u = q.dequeue();
        r.order.pushBack(u);
        for (auto* e = g.adjacency(u); e; e = e->next) {
            op(c);
            if (e->data.blocked || r.depth[e->to] != -1) continue;
            r.depth[e->to] = r.depth[u] + 1;
            r.parent[e->to] = u;
            q.enqueue(e->to);
        }
    }
    return r;
}

inline TraversalResult dfs(const Graph& g, int src, OpCounter* c = nullptr) {
    TraversalResult r;
    int n = g.vertexCount();
    detail::initArrays(n, r.parent, r.depth);
    if (src < 0 || src >= n) return r;
    DynamicArray<bool> visited((std::size_t)(n > 0 ? n : 0));
    for (int i = 0; i < n; ++i) visited.pushBack(false);
    ds::Stack<int> st;
    st.push(src); r.depth[src] = 0;
    while (!st.isEmpty()) {
        int u = st.pop();
        if (visited[u]) continue;
        visited[u] = true;
        r.order.pushBack(u);
        for (auto* e = g.adjacency(u); e; e = e->next) {
            op(c);
            if (e->data.blocked || visited[e->to]) continue;
            r.parent[e->to] = u;
            r.depth[e->to] = r.depth[u] + 1;
            st.push(e->to);
        }
    }
    return r;
}

/// componentOf[v] = component number; returns number of components.
inline int connectedComponents(const Graph& g, DynamicArray<int>& componentOf) {
    int n = g.vertexCount();
    componentOf.clear();
    for (int i = 0; i < n; ++i) componentOf.pushBack(-1);
    int comp = 0;
    for (int s = 0; s < n; ++s) {
        if (componentOf[s] != -1) continue;
        ds::Queue<int> q; q.enqueue(s); componentOf[s] = comp;
        while (!q.isEmpty()) {
            int u = q.dequeue();
            for (auto* e = g.adjacency(u); e; e = e->next)
                if (!e->data.blocked && componentOf[e->to] == -1) { componentOf[e->to] = comp; q.enqueue(e->to); }
        }
        ++comp;
    }
    return comp;
}

inline ShortestPathResult dijkstra(const Graph& g, int src, WeightMode mode, OpCounter* c = nullptr) {
    ShortestPathResult r;
    int n = g.vertexCount();
    for (int i = 0; i < n; ++i) { r.dist.pushBack(Graph::INF); r.parent.pushBack(-1); }
    if (src < 0 || src >= n) return r;
    DynamicArray<bool> done((std::size_t)(n > 0 ? n : 0));
    for (int i = 0; i < n; ++i) done.pushBack(false);
    ds::IndexedPriorityQueue pq(false);     // min-first
    r.dist[src] = 0;
    pq.push(src, 0);
    while (!pq.empty()) {
        auto top = pq.pop();
        int u = (int)top.id;
        if (done[u]) continue;
        done[u] = true; ++r.settled;
        for (auto* e = g.adjacency(u); e; e = e->next) {
            if (e->data.blocked) continue;
            double nd = r.dist[u] + e->data.weight(mode);
            cmp(c);
            if (nd < r.dist[e->to]) {
                r.dist[e->to] = nd;
                r.parent[e->to] = u;
                ++r.relaxations; op(c);
                pq.push(e->to, nd);           // push == decrease-key when already queued
            }
        }
    }
    return r;
}

/// Reconstructs src->dst path from a parent array. Empty if unreachable.
inline DynamicArray<int> reconstructPath(const DynamicArray<int>& parent, int src, int dst) {
    DynamicArray<int> rev, path;
    if (dst < 0 || dst >= (int)parent.size()) return path;
    for (int v = dst; v != -1; v = parent[v]) { rev.pushBack(v); if (v == src) break; }
    if (rev.empty() || rev[rev.size() - 1] != src) return path;
    for (long i = (long)rev.size() - 1; i >= 0; --i) path.pushBack(rev[i]);
    return path;
}

inline MstResult kruskal(const Graph& g, WeightMode mode, OpCounter* c = nullptr) {
    MstResult res;
    int n = g.vertexCount();
    DynamicArray<MstEdge> edges;
    for (int u = 0; u < n; ++u)
        for (auto* e = g.adjacency(u); e; e = e->next)
            if (!e->data.blocked && u < e->to) edges.pushBack(MstEdge{u, e->to, e->data.weight(mode), e->data.roadId});
    mergeSort(edges, [](const MstEdge& a, const MstEdge& b) { return a.w < b.w; }, c);
    ds::DisjointSet dsu(n);
    for (const MstEdge& e : edges) {
        op(c);
        if (dsu.unite(e.u, e.v)) { res.edges.pushBack(e); res.totalWeight += e.w; }
    }
    res.components = dsu.setCount();
    return res;
}

inline MstResult prim(const Graph& g, WeightMode mode, OpCounter* c = nullptr) {
    MstResult res;
    int n = g.vertexCount();
    DynamicArray<bool> inTree((std::size_t)(n > 0 ? n : 0));
    DynamicArray<double> best((std::size_t)(n > 0 ? n : 0));
    DynamicArray<int> via((std::size_t)(n > 0 ? n : 0));
    DynamicArray<long> viaRoad((std::size_t)(n > 0 ? n : 0));
    for (int i = 0; i < n; ++i) { inTree.pushBack(false); best.pushBack(Graph::INF); via.pushBack(-1); viaRoad.pushBack(0); }
    for (int start = 0; start < n; ++start) {        // restart per component -> spanning forest
        if (inTree[start]) continue;
        ++res.components;
        ds::IndexedPriorityQueue pq(false);
        best[start] = 0; pq.push(start, 0);
        while (!pq.empty()) {
            int u = (int)pq.pop().id;
            if (inTree[u]) continue;
            inTree[u] = true;
            if (via[u] != -1) { res.edges.pushBack(MstEdge{via[u], u, best[u], viaRoad[u]}); res.totalWeight += best[u]; }
            for (auto* e = g.adjacency(u); e; e = e->next) {
                if (e->data.blocked || inTree[e->to]) continue;
                double w = e->data.weight(mode);
                cmp(c);
                if (w < best[e->to]) { best[e->to] = w; via[e->to] = u; viaRoad[e->to] = e->data.roadId; pq.push(e->to, w); }
            }
        }
    }
    return res;
}

/// BFS on a dense adjacency matrix (row-major, INF = no edge). Benchmark only.
inline DynamicArray<int> bfsOnMatrix(const DynamicArray<double>& m, int n, int src, OpCounter* c = nullptr) {
    DynamicArray<int> order;
    if (src < 0 || src >= n) return order;
    DynamicArray<bool> seen((std::size_t)(n > 0 ? n : 0));
    for (int i = 0; i < n; ++i) seen.pushBack(false);
    ds::Queue<int> q; q.enqueue(src); seen[src] = true;
    while (!q.isEmpty()) {
        int u = q.dequeue(); order.pushBack(u);
        for (int v = 0; v < n; ++v) {          // scans the whole row: O(V) per vertex
            op(c);
            if (!seen[v] && v != u && m[(std::size_t)u * n + v] != Graph::INF) { seen[v] = true; q.enqueue(v); }
        }
    }
    return order;
}

} // namespace nexa::algo
