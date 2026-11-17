// Unit tests: sorting, searching, graph algorithms, backtracking
#include "support/TestFramework.hpp"
#include "algorithms/Sorting.hpp"
#include "algorithms/Searching.hpp"
#include "algorithms/GraphAlgorithms.hpp"
#include "algorithms/Backtracking.hpp"

using namespace nexa;
using ds::DynamicArray;

namespace {
DynamicArray<long> pseudoRandom(int n, long mod) {
    DynamicArray<long> a; unsigned long x = 12345;
    for (int i = 0; i < n; ++i) { x = x * 6364136223846793005ULL + 1442695040888963407ULL; a.pushBack((long)((x >> 33) % (unsigned long)mod)); }
    return a;
}
bool sortedAsc(const DynamicArray<long>& a) { for (std::size_t i = 1; i < a.size(); ++i) if (a[i] < a[i - 1]) return false; return true; }
struct Item { long key; int order; };
}

TEST(Sorting, AllComparisonSortsAgree) {
    auto less = [](long a, long b) { return a < b; };
    for (int n : {0, 1, 2, 17, 500}) {
        auto base = pseudoRandom(n, 1000);
        auto m = base, q = base, h = base, ins = base;
        algo::OpCounter cm, cq;
        algo::mergeSort(m, less, &cm);
        algo::quickSort(q, less, &cq);
        algo::heapSort(h, less);
        algo::insertionSort(ins, less);
        EXPECT_TRUE(sortedAsc(m)); EXPECT_TRUE(sortedAsc(q)); EXPECT_TRUE(sortedAsc(h)); EXPECT_TRUE(sortedAsc(ins));
        for (int i = 0; i < n; ++i) { EXPECT_EQ(m[i], q[i]); EXPECT_EQ(m[i], h[i]); }
        if (n == 500) { EXPECT_GT(cm.comparisons, 0u); EXPECT_GT(cq.comparisons, 0u); }
    }
}

TEST(Sorting, QuickSortHandlesSortedInputWithMedianOfThree) {
    DynamicArray<long> a; for (long i = 0; i < 5000; ++i) a.pushBack(i);
    algo::OpCounter c;
    algo::quickSort(a, [](long x, long y) { return x < y; }, &c);
    EXPECT_TRUE(sortedAsc(a));
    EXPECT_LT(c.comparisons, 200000u);    // far from n^2/2 = 12.5M
}

TEST(Sorting, NonComparisonSortsAndStability) {
    auto a = pseudoRandom(300, 100000);
    auto r = a;
    algo::radixSortBy(r, [](long x) { return x; });
    EXPECT_TRUE(sortedAsc(r));
    DynamicArray<Item> items;
    for (int i = 0; i < 50; ++i) items.pushBack(Item{(long)(i % 5) + 1, i});
    auto c = items;
    algo::countingSortBy(c, [](const Item& x) { return x.key; }, 1, 5);
    for (std::size_t i = 1; i < c.size(); ++i) {
        EXPECT_LE(c[i - 1].key, c[i].key);
        if (c[i - 1].key == c[i].key) EXPECT_LT(c[i - 1].order, c[i].order);   // stable
    }
    auto m = items;
    algo::mergeSort(m, [](const Item& x, const Item& y) { return x.key < y.key; });
    for (std::size_t i = 0; i < m.size(); ++i) EXPECT_EQ(m[i].order, c[i].order);
    DynamicArray<double> d{3.2, 0.5, 9.9, 4.4, 0.1, 7.0, 5.5};
    algo::bucketSortBy(d, [](double x) { return x; }, 0.0, 10.0, 4);
    for (std::size_t i = 1; i < d.size(); ++i) EXPECT_LE(d[i - 1], d[i]);
    EXPECT_THROW(algo::countingSortBy(c, [](const Item&) { return 9L; }, 1, 5), std::out_of_range);
}

TEST(Searching, LinearBinaryMedian) {
    auto a = pseudoRandom(201, 10000);
    long target = a[77];
    EXPECT_GE(algo::linearSearch(a, [&](long v) { return v == target; }), 0);
    auto s = a;
    algo::mergeSort(s, [](long x, long y) { return x < y; });
    long idx = algo::binarySearch(s, target, [](long v) { return v; });
    ASSERT_GE(idx, 0);
    EXPECT_EQ(s[idx], target);
    EXPECT_EQ(algo::binarySearch(s, -5L, [](long v) { return v; }), -1);
    auto lessL = [](long x, long y) { return x < y; };
    EXPECT_EQ(algo::medianSearch(a, lessL), s[100]);
    for (long k : {0L, 5L, 100L, 200L}) {
        EXPECT_EQ(algo::medianOfMediansSelect(a, k, lessL), s[k]);
        EXPECT_EQ(algo::quickSelect(a, k, lessL), s[k]);
    }
    EXPECT_THROW(algo::medianSearch(DynamicArray<long>(), lessL), std::out_of_range);
}

namespace {
// 0 -1- 1 -1- 2
// |           |
// 10 ------- 1 (weights)   and isolated vertex 4
void sampleGraph(ds::Graph& g) {
    for (long id : {100L, 101L, 102L, 103L, 104L}) g.addVertex(ds::VertexInfo{id, "v", "junction", 0, 0});
    g.addEdge(100, 101, ds::EdgeData{1, 1, 2, 0});
    g.addEdge(101, 102, ds::EdgeData{2, 1, 2, 0});
    g.addEdge(100, 103, ds::EdgeData{3, 10, 5, 0});
    g.addEdge(103, 102, ds::EdgeData{4, 1, 30, 0});
}
}

TEST(Graph, BfsDfsComponents) {
    ds::Graph g; sampleGraph(g);
    auto b = algo::bfs(g, g.indexOf(100));
    EXPECT_EQ(b.order.size(), 4u);
    EXPECT_EQ(b.depth[g.indexOf(102)], 2);
    EXPECT_EQ(b.depth[g.indexOf(104)], -1);
    auto d = algo::dfs(g, g.indexOf(100));
    EXPECT_EQ(d.order.size(), 4u);
    DynamicArray<int> comp;
    EXPECT_EQ(algo::connectedComponents(g, comp), 2);
    EXPECT_THROW(g.addEdge(100, 999, ds::EdgeData{}), std::invalid_argument);
    EXPECT_THROW(g.addEdge(100, 101, ds::EdgeData{9, -1, 0, 0}), std::invalid_argument);
}

TEST(Graph, DijkstraRespectsWeightModeAndBlocks) {
    ds::Graph g; sampleGraph(g);
    int s = g.indexOf(100), t = g.indexOf(102);
    auto byDist = algo::dijkstra(g, s, ds::WeightMode::Distance);
    EXPECT_NEAR(byDist.dist[t], 2.0, 1e-9);
    auto path = algo::reconstructPath(byDist.parent, s, t);
    ASSERT_EQ(path.size(), 3u);
    EXPECT_EQ(path[1], g.indexOf(101));
    auto byTime = algo::dijkstra(g, s, ds::WeightMode::Time);
    EXPECT_NEAR(byTime.dist[t], 4.0, 1e-9);
    g.setBlocked(1, true);                   // block 100-101
    auto blocked = algo::dijkstra(g, s, ds::WeightMode::Distance);
    EXPECT_NEAR(blocked.dist[t], 11.0, 1e-9);
    auto none = algo::reconstructPath(blocked.parent, s, g.indexOf(104));
    EXPECT_TRUE(none.empty());
}

TEST(Graph, KruskalAndPrimAgree) {
    ds::Graph g; sampleGraph(g);
    auto k = algo::kruskal(g, ds::WeightMode::Distance);
    auto p = algo::prim(g, ds::WeightMode::Distance);
    EXPECT_NEAR(k.totalWeight, 3.0, 1e-9);   // edges 1 + 1 + 1
    EXPECT_NEAR(p.totalWeight, k.totalWeight, 1e-9);
    EXPECT_EQ(k.edges.size(), 3u);
    EXPECT_EQ(k.components, 2);
    EXPECT_EQ(p.components, 2);
    auto m = g.buildAdjacencyMatrix(ds::WeightMode::Distance);
    auto order = algo::bfsOnMatrix(m, g.vertexCount(), g.indexOf(100));
    EXPECT_EQ(order.size(), 4u);
}

TEST(Backtracking, MazeAndQueens) {
    DynamicArray<int> grid{1, 0, 0, 0,
                           1, 1, 0, 1,
                           0, 1, 0, 0,
                           1, 1, 1, 1};
    auto r = algo::ratInMaze(grid, 4, 4);
    EXPECT_TRUE(r.found);
    EXPECT_EQ(r.path[0], 0);
    EXPECT_EQ(r.path[r.path.size() - 1], 15);
    DynamicArray<int> blocked{1, 0, 0, 1};
    EXPECT_FALSE(algo::ratInMaze(blocked, 2, 2).found);
    EXPECT_EQ(algo::nQueens(8).solutionCount, 92u);
    EXPECT_EQ(algo::nQueens(6).solutionCount, 4u);
    EXPECT_EQ(algo::nQueens(3).solutionCount, 0u);
    EXPECT_EQ(algo::nQueens(8, 1).firstSolution.size(), 8u);
}
