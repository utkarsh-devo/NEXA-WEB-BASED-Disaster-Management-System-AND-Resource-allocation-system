#include "services/DsaLabService.hpp"
#include "algorithms/Backtracking.hpp"
#include "algorithms/GraphAlgorithms.hpp"
#include "algorithms/Searching.hpp"
#include "algorithms/Sorting.hpp"
#include "data_structures/AVLTree.hpp"
#include "data_structures/BPlusTree.hpp"
#include "data_structures/BST.hpp"
#include "data_structures/BTree.hpp"
#include "data_structures/BinaryHeap.hpp"
#include "data_structures/HashTable.hpp"
#include "data_structures/IntervalTree.hpp"
#include "data_structures/KDTree.hpp"
#include "data_structures/KaryTree.hpp"
#include "data_structures/LinkedList.hpp"
#include "data_structures/MultiLinkedList.hpp"
#include "data_structures/QuadTree.hpp"
#include "data_structures/Queue.hpp"
#include "data_structures/RangeTree.hpp"
#include "data_structures/RedBlackTree.hpp"
#include "data_structures/SegmentTree.hpp"
#include "data_structures/SparseMatrix.hpp"
#include "data_structures/Stack.hpp"
#include "data_structures/SuffixArray.hpp"
#include "data_structures/SuffixTree.hpp"
#include "data_structures/ThreadedBinaryTree.hpp"
#include "data_structures/Trie.hpp"
#include "utilities/Errors.hpp"
#include "utilities/Geo.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>

namespace nexa::svc {

namespace {
using Clock = std::chrono::steady_clock;
double us(Clock::time_point t0) { return std::chrono::duration<double, std::micro>(Clock::now() - t0).count(); }
std::string s(long v) { return std::to_string(v); }
std::string d2(double v) { char b[32]; std::snprintf(b, sizeof b, "%.2f", v); return b; }
LabStat st(const std::string& k, const std::string& v) { return LabStat{k, v}; }
LabStat st(const std::string& k, long v) { return LabStat{k, std::to_string(v)}; }

struct Rng {
    unsigned long long x;
    explicit Rng(unsigned seed) : x(seed ? seed : 7) {}
    unsigned long long next() { x = x * 6364136223846793005ULL + 1442695040888963407ULL; return x >> 17; }
    double uniform() { return (double)(next() % 1000000) / 1000000.0; }
};

void limitInput(std::size_t n, std::size_t max, const char* what) {
    if (n > max) throw ValidationError(std::string(what) + ": at most " + std::to_string(max) + " values allowed in the laboratory");
}
std::string join(const DynamicArray<long>& v) { std::string o; for (std::size_t i = 0; i < v.size(); ++i) { if (i) o += ", "; o += s(v[i]); } return o; }

// ---- tree exporters -------------------------------------------------------
template <typename Node, typename LabelFn>
void exportBinary(const Node* n, const std::string& parent, const std::string& edge, int depth, DynamicArray<LabNode>& out, LabelFn label) {
    if (!n) return;
    LabNode x; x.id = s(n->key); x.parent = parent; x.edge = edge; x.depth = depth;
    x.label = label(n);
    out.pushBack(x);
    exportBinary(n->left, x.id, "L", depth + 1, out, label);
    exportBinary(n->right, x.id, "R", depth + 1, out, label);
}

template <typename Tree>
void exportRB(const Tree& t, const typename Tree::Node* n, const std::string& parent, const std::string& edge, int depth, DynamicArray<LabNode>& out) {
    if (n == t.nil()) return;
    LabNode x; x.id = s(n->key); x.parent = parent; x.edge = edge; x.depth = depth; x.label = s(n->key);
    x.color = n->color == Tree::RED ? "red" : "black";
    out.pushBack(x);
    exportRB(t, n->left, x.id, "L", depth + 1, out);
    exportRB(t, n->right, x.id, "R", depth + 1, out);
}

template <typename Node>
void exportMultiway(const Node* n, const std::string& parent, int depth, int& counter, DynamicArray<LabNode>& out) {
    if (!n) return;
    LabNode x; x.id = "n" + s(++counter); x.parent = parent; x.depth = depth;
    for (std::size_t i = 0; i < n->keys.size(); ++i) { if (i) x.label += " | "; x.label += s(n->keys[i]); }
    if (n->keys.empty()) x.label = "∅";
    x.color = n->leaf ? "leaf" : "internal";
    out.pushBack(x);
    if (!n->leaf) for (const auto* c : n->kids) exportMultiway(c, x.id, depth + 1, counter, out);
}

DynamicArray<LabStat> cx(std::initializer_list<std::pair<const char*, const char*>> items) {
    DynamicArray<LabStat> o;
    for (const auto& p : items) o.pushBack(LabStat{p.first, p.second});
    return o;
}
} // namespace

// ============================================================ linear
LabOutput DsaLabService::linear(const std::string& structure, const DynamicArray<LabOp>& ops) {
    limitInput(ops.size(), 200, "operations");
    LabOutput o; o.structure = structure; o.operation = "sequence";
    auto snapshot = [&](const DynamicArray<long>& v, const std::string& action) { LabFrame f; f.values = v; f.action = action; o.frames.pushBack(f); };
    if (structure == "stack") {
        ds::Stack<long> stck;
        for (const auto& op : ops) {
            try {
                if (op.op == "push") { stck.push(op.value); o.log.pushBack("push(" + s(op.value) + ")"); }
                else if (op.op == "pop") { o.log.pushBack("pop() -> " + s(stck.pop())); }
                else if (op.op == "peek") { o.log.pushBack("peek() -> " + s(stck.peek())); }
                else o.log.pushBack("unknown op " + op.op);
            } catch (const std::exception& e) { o.log.pushBack(op.op + ": " + e.what()); }
            DynamicArray<long> v; stck.forEach([&](long x) { v.pushBack(x); });    // top first
            snapshot(v, op.op);
        }
        o.stats.pushBack(st("size", (long)stck.size()));
        o.complexity = cx({{"push", "O(1)"}, {"pop", "O(1)"}, {"peek", "O(1)"}, {"space", "O(n)"}});
    } else if (structure == "queue") {
        ds::Queue<long> q(4);
        for (const auto& op : ops) {
            try {
                if (op.op == "enqueue") { q.enqueue(op.value); o.log.pushBack("enqueue(" + s(op.value) + ")"); }
                else if (op.op == "dequeue") o.log.pushBack("dequeue() -> " + s(q.dequeue()));
                else if (op.op == "front") o.log.pushBack("front() -> " + s(q.front()));
                else if (op.op == "rear") o.log.pushBack("rear() -> " + s(q.rear()));
                else o.log.pushBack("unknown op " + op.op);
            } catch (const std::exception& e) { o.log.pushBack(op.op + ": " + e.what()); }
            DynamicArray<long> v; q.forEach([&](long x) { v.pushBack(x); });
            snapshot(v, op.op);
        }
        o.stats.pushBack(st("size", (long)q.size()));
        o.complexity = cx({{"enqueue", "O(1) amortised"}, {"dequeue", "O(1)"}, {"front/rear", "O(1)"}, {"space", "O(n)"}});
    } else if (structure == "linkedlist") {
        ds::LinkedList<long> l;
        for (const auto& op : ops) {
            try {
                if (op.op == "insertFront") { l.insertFront(op.value); o.log.pushBack("insertFront(" + s(op.value) + ")"); }
                else if (op.op == "insertBack") { l.insertBack(op.value); o.log.pushBack("insertBack(" + s(op.value) + ")"); }
                else if (op.op == "insertAt") { l.insertAt((std::size_t)op.position, op.value); o.log.pushBack("insertAt(" + s(op.position) + ", " + s(op.value) + ")"); }
                else if (op.op == "remove") { bool ok = l.removeFirst([&](long x) { return x == op.value; }); o.log.pushBack("remove(" + s(op.value) + ") -> " + (ok ? "removed" : "not found")); }
                else if (op.op == "search") { long idx = 0, found = -1; l.forEach([&](long x) { if (found < 0 && x == op.value) found = idx; ++idx; }); o.log.pushBack("search(" + s(op.value) + ") -> index " + s(found)); }
                else o.log.pushBack("unknown op " + op.op);
            } catch (const std::exception& e) { o.log.pushBack(op.op + ": " + e.what()); }
            DynamicArray<long> v; l.forEach([&](long x) { v.pushBack(x); });
            snapshot(v, op.op);
        }
        o.stats.pushBack(st("size", (long)l.size()));
        o.complexity = cx({{"insert front/back", "O(1)"}, {"insert at position", "O(n)"}, {"search / delete", "O(n)"}, {"space", "O(n)"}});
    } else if (structure == "array") {
        DynamicArray<long> a;
        for (const auto& op : ops) {
            try {
                if (op.op == "append") { a.pushBack(op.value); o.log.pushBack("append(" + s(op.value) + ") capacity=" + s((long)a.capacity())); }
                else if (op.op == "insertAt") { a.insertAt((std::size_t)op.position, op.value); o.log.pushBack("insertAt(" + s(op.position) + ") shifted " + s((long)a.size() - 1 - op.position) + " elements"); }
                else if (op.op == "removeAt") { a.removeAt((std::size_t)op.position); o.log.pushBack("removeAt(" + s(op.position) + ")"); }
                else if (op.op == "update") { a.at((std::size_t)op.position) = op.value; o.log.pushBack("a[" + s(op.position) + "] = " + s(op.value)); }
                else if (op.op == "search") o.log.pushBack("linear search(" + s(op.value) + ") -> " + s(a.findIndex([&](long x) { return x == op.value; })));
                else o.log.pushBack("unknown op " + op.op);
            } catch (const std::exception& e) { o.log.pushBack(op.op + ": " + e.what()); }
            snapshot(a, op.op);
        }
        o.stats.pushBack(st("size", (long)a.size())); o.stats.pushBack(st("capacity", (long)a.capacity()));
        o.complexity = cx({{"access/update", "O(1)"}, {"append", "O(1) amortised"}, {"insert/remove at i", "O(n)"}, {"search", "O(n)"}});
    } else throw ValidationError("structure must be stack, queue, linkedlist or array");
    if (!o.frames.empty()) o.result = o.frames.back().values;
    return o;
}

// ============================================================ sort
LabOutput DsaLabService::sort(const std::string& algorithm, const DynamicArray<long>& values) {
    limitInput(values.size(), 300, "values");
    LabOutput o; o.structure = "array"; o.operation = algorithm;
    DynamicArray<long> a = values;
    algo::OpCounter c;
    const std::size_t maxFrames = 700;
    o.frames.pushBack(LabFrame{a, -1, -1, "start"});
    algo::Trace trace = [&](const char* action, long i, long j) { if (o.frames.size() < maxFrames) o.frames.pushBack(LabFrame{a, i, j, action}); };
    auto less = [](long x, long y) { return x < y; };
    auto t0 = Clock::now();
    if (algorithm == "merge") { algo::mergeSort(a, less, &c, &trace); o.complexity = cx({{"time", "O(n log n) all cases"}, {"space", "O(n)"}, {"stable", "yes"}}); }
    else if (algorithm == "quick") { algo::quickSort(a, less, &c, &trace); o.complexity = cx({{"time avg", "O(n log n)"}, {"time worst", "O(n²)"}, {"space", "O(log n)"}, {"stable", "no"}, {"pivot", "median-of-three"}}); }
    else if (algorithm == "heap") { algo::heapSort(a, less, &c, &trace); o.complexity = cx({{"time", "O(n log n)"}, {"space", "O(1)"}, {"stable", "no"}}); }
    else if (algorithm == "insertion") { algo::insertionSort(a, less, &c, &trace); o.complexity = cx({{"time best", "O(n)"}, {"time worst", "O(n²)"}, {"space", "O(1)"}, {"stable", "yes"}}); }
    else if (algorithm == "counting") {
        long mn = 0, mx = 0;
        for (std::size_t i = 0; i < a.size(); ++i) { if (i == 0 || a[i] < mn) mn = a[i]; if (i == 0 || a[i] > mx) mx = a[i]; }
        if (mx - mn > 100000) throw ValidationError("Counting sort needs a bounded key range (max - min <= 100000)");
        algo::countingSortBy(a, [](long x) { return x; }, mn, mx, &c, &trace);
        o.complexity = cx({{"time", "O(n + k)"}, {"space", "O(n + k)"}, {"k", "key range"}, {"stable", "yes"}});
        o.stats.pushBack(st("key range k", mx - mn + 1));
    } else if (algorithm == "radix") {
        for (long v : a) if (v < 0) throw ValidationError("Radix sort here requires non-negative integers");
        algo::radixSortBy(a, [](long x) { return x; }, &c, &trace);
        o.complexity = cx({{"time", "O(d·(n + b)), b = 10"}, {"space", "O(n + b)"}, {"stable", "yes"}});
    } else if (algorithm == "bucket") {
        long mn = 0, mx = 1;
        for (std::size_t i = 0; i < a.size(); ++i) { if (i == 0 || a[i] < mn) mn = a[i]; if (i == 0 || a[i] > mx) mx = a[i]; }
        algo::bucketSortBy(a, [](long x) { return (double)x; }, (double)mn, (double)mx + 1, 10, &c, &trace);
        o.complexity = cx({{"time avg", "O(n + k)"}, {"time worst", "O(n²)"}, {"space", "O(n + k)"}, {"buckets", "10"}});
    } else throw ValidationError("algorithm must be merge, quick, heap, insertion, counting, radix or bucket");
    double micro = us(t0);
    o.frames.pushBack(LabFrame{a, -1, -1, "done"});
    o.result = a;
    o.stats.pushBack(st("n", (long)a.size()));
    o.stats.pushBack(st("comparisons", (long)c.comparisons));
    o.stats.pushBack(st("moves/swaps", (long)c.swaps));
    o.stats.pushBack(st("other operations", (long)c.operations));
    o.stats.pushBack(st("time (µs, incl. trace)", d2(micro)));
    if (o.frames.size() >= maxFrames) o.log.pushBack("Animation truncated after " + s((long)maxFrames) + " frames; counts are complete.");
    return o;
}

// ============================================================ search
LabOutput DsaLabService::search(const std::string& algorithm, const DynamicArray<long>& values, long target) {
    limitInput(values.size(), 2000, "values");
    if (values.empty()) throw ValidationError("Provide at least one value");
    LabOutput o; o.structure = "array"; o.operation = algorithm;
    algo::OpCounter c;
    auto less = [](long x, long y) { return x < y; };
    if (algorithm == "linear") {
        DynamicArray<long> a = values;
        long idx = algo::linearSearch(a, [&](const long& x) { o.frames.pushBack(LabFrame{a, (long)(&x - a.begin()), -1, "probe"}); return x == target; }, &c);
        o.result = a;
        o.log.pushBack(idx >= 0 ? "Found " + s(target) + " at index " + s(idx) : s(target) + " not present");
        o.stats.pushBack(st("index", idx));
        o.complexity = cx({{"time", "O(n)"}, {"precondition", "none (unsorted OK)"}});
    } else if (algorithm == "binary") {
        DynamicArray<long> a = values;
        algo::mergeSort(a, less);
        o.log.pushBack("Input sorted with merge sort (binary search requires sorted data)");
        long idx = algo::binarySearch(a, target, [&](const long& x) { o.frames.pushBack(LabFrame{a, (long)(&x - a.begin()), -1, "mid"}); return x; }, &c);
        o.result = a;
        o.log.pushBack(idx >= 0 ? "Found " + s(target) + " at sorted index " + s(idx) : s(target) + " not present");
        o.stats.pushBack(st("index", idx));
        o.complexity = cx({{"time", "O(log n)"}, {"precondition", "sorted ascending"}, {"space", "O(1)"}});
    } else if (algorithm == "median") {
        long m = algo::medianSearch(values, less, &c);
        DynamicArray<long> sorted = values; algo::mergeSort(sorted, less);
        o.result = sorted;
        o.log.pushBack("Median (lower) = " + s(m) + " found by median-of-medians selection WITHOUT sorting");
        o.log.pushBack("Sorted view shown only for verification");
        o.stats.pushBack(st("median", m));
        long rank = ((long)values.size() - 1) / 2;
        o.stats.pushBack(st("rank k", rank));
        o.frames.pushBack(LabFrame{sorted, rank, -1, "median"});
        o.complexity = cx({{"time", "O(n) worst case (median of medians)"}, {"space", "O(n) (works on a copy)"}, {"vs sort", "O(n log n) to sort then index"}});
    } else if (algorithm == "quickselect") {
        long k = target < 0 ? 0 : (target >= (long)values.size() ? (long)values.size() - 1 : target);
        long v = algo::quickSelect(values, k, less, &c);
        DynamicArray<long> sorted = values; algo::mergeSort(sorted, less);
        o.result = sorted;
        o.frames.pushBack(LabFrame{sorted, k, -1, "kth"});
        o.log.pushBack("k-th smallest (k=" + s(k) + ") = " + s(v));
        o.stats.pushBack(st("value", v));
        o.complexity = cx({{"time avg", "O(n)"}, {"time worst", "O(n²)"}});
    } else throw ValidationError("algorithm must be linear, binary, median or quickselect");
    o.stats.pushBack(st("comparisons", (long)c.comparisons));
    o.stats.pushBack(st("n", (long)values.size()));
    return o;
}

// ============================================================ trees
LabOutput DsaLabService::tree(const std::string& type, const DynamicArray<long>& inserts, const DynamicArray<long>& deletes, long searchKey, int order) {
    limitInput(inserts.size(), 200, "values");
    LabOutput o; o.structure = type; o.operation = "build";
    auto traversal = [&](const char* name, const DynamicArray<long>& seq) { o.log.pushBack(std::string(name) + ": " + join(seq)); };
    if (type == "bst" || type == "avl") {
        algo::OpCounter c;
        DynamicArray<long> in, pre, post;
        if (type == "bst") {
            ds::BST<long, long> t;
            for (long v : inserts) t.insert(v, v, &c);
            for (long v : deletes) o.log.pushBack("delete " + s(v) + (t.remove(v) ? " ✓" : " (absent)"));
            algo::OpCounter sc;
            bool found = t.search(searchKey, &sc) != nullptr;
            t.inorder([&](long k, long) { in.pushBack(k); }); t.preorder([&](long k, long) { pre.pushBack(k); }); t.postorder([&](long k, long) { post.pushBack(k); });
            exportBinary(t.root(), "", "", 0, o.nodes, [](const auto* n) { return s(n->key); });
            o.stats.pushBack(st("height", (long)t.height())); o.stats.pushBack(st("size", (long)t.size()));
            o.stats.pushBack(st("search " + s(searchKey), found ? "found" : "not found")); o.stats.pushBack(st("search comparisons", (long)sc.comparisons));
            o.complexity = cx({{"search/insert/delete", "O(h): O(log n) avg, O(n) worst"}, {"traversal", "O(n)"}});
        } else {
            ds::AVLTree<long, long> t;
            for (long v : inserts) t.insert(v, v, &c);
            for (long v : deletes) o.log.pushBack("delete " + s(v) + (t.remove(v) ? " ✓" : " (absent)"));
            algo::OpCounter sc;
            bool found = t.search(searchKey, &sc) != nullptr;
            t.inorder([&](long k, long) { in.pushBack(k); }); t.preorder([&](long k, long) { pre.pushBack(k); }); t.postorder([&](long k, long) { post.pushBack(k); });
            exportBinary(t.root(), "", "", 0, o.nodes, [](const auto* n) { return s(n->key) + "  h" + s(n->height); });
            o.stats.pushBack(st("height", (long)t.height())); o.stats.pushBack(st("size", (long)t.size()));
            o.stats.pushBack(st("rotations", (long)t.rotations()));
            o.stats.pushBack(st("search " + s(searchKey), found ? "found" : "not found")); o.stats.pushBack(st("search comparisons", (long)sc.comparisons));
            o.complexity = cx({{"search/insert/delete", "O(log n) worst"}, {"height bound", "≈ 1.44 log₂(n+2)"}, {"rotations per insert", "≤ 2"}});
        }
        o.stats.pushBack(st("insert comparisons", (long)c.comparisons));
        traversal("inorder", in); traversal("preorder", pre); traversal("postorder", post);
        o.result = in;
    } else if (type == "rb") {
        ds::RedBlackTree<long, long> t;
        for (long v : inserts) t.insert(v, v);
        for (long v : deletes) o.log.pushBack("delete " + s(v) + (t.remove(v) ? " ✓" : " (absent)"));
        exportRB(t, t.root(), "", "", 0, o.nodes);
        DynamicArray<long> in; t.inorder([&](long k, long) { in.pushBack(k); });
        traversal("inorder", in); o.result = in;
        o.stats.pushBack(st("height", (long)t.height())); o.stats.pushBack(st("rotations", (long)t.rotations()));
        o.stats.pushBack(st("black-height (valid if > 0)", (long)t.validate()));
        o.stats.pushBack(st("search " + s(searchKey), t.search(searchKey) ? "found" : "not found"));
        o.complexity = cx({{"search/insert/delete", "O(log n) worst"}, {"height bound", "≤ 2 log₂(n+1)"}, {"rotations", "insert ≤ 2, delete ≤ 3"}});
    } else if (type == "btree") {
        ds::BTree<long> t(order >= 2 ? order : 2);
        for (long v : inserts) t.insert(v);
        for (long v : deletes) o.log.pushBack("delete " + s(v) + (t.remove(v) ? " ✓" : " (absent)"));
        int counter = 0; exportMultiway(t.root(), "", 0, counter, o.nodes);
        DynamicArray<long> in; t.inorder([&](long k) { in.pushBack(k); });
        traversal("inorder", in); o.result = in;
        std::size_t visited = 0; bool found = t.search(searchKey, &visited);
        o.stats.pushBack(st("minimum degree t", (long)t.minDegree())); o.stats.pushBack(st("height", (long)t.height()));
        o.stats.pushBack(st("search " + s(searchKey), found ? "found" : "not found")); o.stats.pushBack(st("nodes visited", (long)visited));
        o.complexity = cx({{"search/insert/delete", "O(t log_t n)"}, {"node visits (disk reads)", "O(log_t n)"}, {"keys per node", "t-1 … 2t-1"}});
    } else if (type == "bplus") {
        ds::BPlusTree<long, long> t((std::size_t)(order >= 3 ? order : 3));
        for (long v : inserts) t.insert(v, v);
        for (long v : deletes) o.log.pushBack("delete " + s(v) + (t.remove(v) ? " ✓ (lazy)" : " (absent)"));
        int counter = 0; exportMultiway(t.root(), "", 0, counter, o.nodes);
        DynamicArray<long> chain;
        long lo = -2147483647L, hi = 2147483647L;
        t.range(lo, hi, [&](long k, long) { chain.pushBack(k); });
        traversal("leaf chain", chain); o.result = chain;
        o.stats.pushBack(st("height", (long)t.height())); o.stats.pushBack(st("max keys per node", (long)(order >= 3 ? order : 3)));
        o.stats.pushBack(st("search " + s(searchKey), t.search(searchKey) ? "found" : "not found"));
        o.complexity = cx({{"search/insert", "O(log n)"}, {"range scan", "O(log n + m) via leaf chain"}, {"delete", "lazy (no rebalancing) — documented"}});
    } else if (type == "threaded") {
        ds::ThreadedBinaryTree<long> t;
        for (long v : inserts) t.insert(v);
        // export real child links only
        struct Ex {
            static void run(const ds::ThreadedBinaryTree<long>::Node* n, const std::string& p, const std::string& e, int d, DynamicArray<LabNode>& out, DynamicArray<std::string>& log) {
                if (!n) return;
                LabNode x; x.id = std::to_string(n->key); x.parent = p; x.edge = e; x.depth = d; x.label = x.id; out.pushBack(x);
                if (n->leftThread && n->left) log.pushBack(x.id + " ⇠ thread to predecessor " + std::to_string(n->left->key));
                if (n->rightThread && n->right) log.pushBack(x.id + " ⇢ thread to successor " + std::to_string(n->right->key));
                if (!n->leftThread) run(n->left, x.id, "L", d + 1, out, log);
                if (!n->rightThread) run(n->right, x.id, "R", d + 1, out, log);
            }
        };
        Ex::run(t.root(), "", "", 0, o.nodes, o.log);
        DynamicArray<long> in, rev; t.inorder([&](long k) { in.pushBack(k); }); t.reverseInorder([&](long k) { rev.pushBack(k); });
        traversal("stackless inorder", in); traversal("reverse inorder (predecessor threads)", rev);
        o.result = in;
        o.stats.pushBack(st("size", (long)t.size())); o.stats.pushBack(st("extra space for traversal", "O(1)"));
        o.complexity = cx({{"insert/search", "O(h)"}, {"inorder traversal", "O(n) time, O(1) space"}});
    } else if (type == "kary") {
        int k = order >= 2 ? order : 3;
        ds::KaryTree<long> t((std::size_t)k);
        for (std::size_t i = 0; i < inserts.size(); ++i) {
            if (i == 0) { t.setRoot((int)i, s(inserts[i]), inserts[i]); continue; }
            t.addChild((int)((i - 1) / k), (int)i, s(inserts[i]), inserts[i]);       // complete k-ary layout
        }
        t.preorder([&](ds::KaryTree<long>::Node* n) { LabNode x; x.id = s(n->id); x.parent = n->parent ? s(n->parent->id) : ""; x.depth = n->depth; x.label = n->label; o.nodes.pushBack(x); });
        DynamicArray<long> level; t.levelOrder([&](ds::KaryTree<long>::Node* n) { level.pushBack(n->value); });
        traversal("level order", level); o.result = level;
        o.stats.pushBack(st("k", (long)k)); o.stats.pushBack(st("height", (long)t.height())); o.stats.pushBack(st("nodes", (long)t.size()));
        o.complexity = cx({{"children of i", "k·i+1 … k·i+k (complete layout)"}, {"traversal", "O(n)"}, {"NEXA use", "disaster-category taxonomy"}});
    } else throw ValidationError("type must be bst, avl, rb, btree, bplus, threaded or kary");
    return o;
}

// ============================================================ heap
LabOutput DsaLabService::heap(const std::string& mode, const DynamicArray<long>& values, int extractCount) {
    limitInput(values.size(), 120, "values");
    LabOutput o; o.structure = mode == "min" ? "min-heap" : "max-heap"; o.operation = "insert/extract";
    algo::OpCounter c;
    auto run = [&](auto& h) {
        for (long v : values) { h.insert(v); o.frames.pushBack(LabFrame{h.raw(), (long)h.raw().size() - 1, -1, "insert " + s(v)}); }
        for (int i = 0; i < extractCount && !h.empty(); ++i) { long top = h.extract(); o.log.pushBack("extract -> " + s(top)); o.frames.pushBack(LabFrame{h.raw(), 0, -1, "extract " + s(top)}); }
        const auto& arr = h.raw();
        for (std::size_t i = 0; i < arr.size(); ++i) {
            LabNode x; x.id = "i" + s((long)i); x.parent = i ? "i" + s((long)(i - 1) / 2) : ""; x.edge = i ? (i % 2 ? "L" : "R") : ""; x.label = s(arr[i]);
            int d = 0; for (std::size_t j = i + 1; j > 1; j >>= 1) ++d; x.depth = d;
            o.nodes.pushBack(x);
        }
        o.result = arr;
    };
    if (mode == "min") { ds::BinaryHeap<long, ds::MinCompare<long>> h(ds::MinCompare<long>(), &c); run(h); }
    else { ds::BinaryHeap<long> h(ds::MaxCompare<long>(), &c); run(h); }
    DynamicArray<long> sorted = values;
    algo::OpCounter hc;
    algo::heapSort(sorted, [](long a, long b) { return a < b; }, &hc);
    o.log.pushBack("heap sort (ascending): " + join(sorted));
    o.stats.pushBack(st("comparisons", (long)c.comparisons)); o.stats.pushBack(st("swaps", (long)c.swaps));
    o.stats.pushBack(st("heap sort comparisons", (long)hc.comparisons));
    o.complexity = cx({{"insert", "O(log n)"}, {"extract", "O(log n)"}, {"peek", "O(1)"}, {"build heap", "O(n)"}, {"heap sort", "O(n log n), in place"}});
    return o;
}

// ============================================================ hash
LabOutput DsaLabService::hash(const std::string& strategy, const DynamicArray<long>& keys, int capacity, const DynamicArray<long>& deletes) {
    limitInput(keys.size(), 300, "keys");
    ds::CollisionStrategy cs = strategy == "linear" ? ds::CollisionStrategy::LinearProbing :
                               strategy == "quadratic" ? ds::CollisionStrategy::QuadraticProbing :
                               strategy == "double" ? ds::CollisionStrategy::DoubleHashing : ds::CollisionStrategy::SeparateChaining;
    LabOutput o; o.structure = std::string("hash table (") + ds::toString(cs) + ")"; o.operation = "insert";
    ds::HashTable<long, long> h(cs, (std::size_t)(capacity > 0 && capacity <= 1024 ? capacity : 8));
    for (long k : keys) {
        std::size_t before = h.rehashCount();
        h.insert(k, k);
        if (h.rehashCount() != before) o.log.pushBack("load factor exceeded → rehash to capacity " + s((long)h.capacity()));
    }
    for (long k : deletes) o.log.pushBack("delete " + s(k) + (h.erase(k) ? " ✓" + std::string(cs == ds::CollisionStrategy::SeparateChaining ? "" : " (tombstone)") : " (absent)"));
    h.forEachBucket([&](std::size_t i, const DynamicArray<long>& ks, int state) {
        DynamicArray<std::string> row;
        row.pushBack(s((long)i));
        row.pushBack(state == 0 ? "empty" : (state == 2 ? "deleted" : "occupied"));
        std::string items; for (std::size_t j = 0; j < ks.size(); ++j) { if (j) items += " → "; items += s(ks[j]); }
        row.pushBack(items);
        o.table.pushBack(row);
    });
    for (long k : keys) h.find(k);   // lookups -> probe count
    o.stats.pushBack(st("size", (long)h.size())); o.stats.pushBack(st("capacity", (long)h.capacity()));
    o.stats.pushBack(st("load factor", d2(h.loadFactor())));
    o.stats.pushBack(st("insert collisions", (long)h.collisions()));
    o.stats.pushBack(st("lookup probes (all keys)", (long)h.probes()));
    o.stats.pushBack(st("longest chain / cluster", (long)h.longestCluster()));
    o.stats.pushBack(st("rehashes", (long)h.rehashCount()));
    o.complexity = cx({{"insert/find/delete", "O(1) average, O(n) worst"}, {"hash (int)", "splitmix64 finaliser"},
                       {"probe sequence", cs == ds::CollisionStrategy::LinearProbing ? "h + i" : cs == ds::CollisionStrategy::QuadraticProbing ? "h + i(i+1)/2" : cs == ds::CollisionStrategy::DoubleHashing ? "h1 + i·h2 (h2 odd)" : "chain in bucket"},
                       {"max load", cs == ds::CollisionStrategy::SeparateChaining ? "0.75" : "0.60"}});
    return o;
}

// ============================================================ graph
LabOutput DsaLabService::graph(int vertices, const DynamicArray<LabEdge>& edges, const std::string& algorithm, long source, long target) {
    if (vertices < 1 || vertices > 60) throw ValidationError("vertices must be 1..60");
    limitInput(edges.size(), 400, "edges");
    LabOutput o; o.structure = "graph"; o.operation = algorithm;
    ds::Graph g;
    for (int v = 0; v < vertices; ++v) g.addVertex(ds::VertexInfo{v, "V" + s(v), "lab", 0, 0});
    for (const auto& e : edges) {
        if (e.u < 0 || e.v < 0 || e.u >= vertices || e.v >= vertices) throw ValidationError("edge references a vertex outside 0.." + s(vertices - 1));
        g.addEdge(e.u, e.v, ds::EdgeData{0, e.w, e.w, 0});
    }
    // adjacency list + matrix (the two representations)
    for (int u = 0; u < vertices; ++u) {
        std::string line = "V" + s(u) + ":";
        for (auto* e = g.adjacency(u); e; e = e->next) line += " → V" + s(e->to) + "(" + d2(e->data.distanceKm) + ")";
        o.log.pushBack(line);
    }
    auto m = g.buildAdjacencyMatrix(ds::WeightMode::Distance);
    for (int u = 0; u < vertices && vertices <= 20; ++u) {
        DynamicArray<std::string> row;
        for (int v = 0; v < vertices; ++v) { double w = m[(std::size_t)u * vertices + v]; row.pushBack(w == ds::Graph::INF ? "∞" : d2(w)); }
        o.table.pushBack(row);
    }
    int src = (int)(source >= 0 && source < vertices ? source : 0);
    algo::OpCounter c;
    auto t0 = Clock::now();
    auto emitTree = [&](const DynamicArray<int>& parent, const DynamicArray<int>& depth, const char* color) {
        for (int v = 0; v < vertices; ++v) {
            LabNode x; x.id = s(v); x.label = "V" + s(v); x.depth = depth.size() ? depth[v] : 0;
            x.parent = parent[v] >= 0 ? s(parent[v]) : ""; x.color = depth.size() && depth[v] < 0 ? "unreached" : color;
            o.nodes.pushBack(x);
        }
    };
    if (algorithm == "bfs" || algorithm == "dfs") {
        auto r = algorithm == "dfs" ? algo::dfs(g, src, &c) : algo::bfs(g, src, &c);
        for (int v : r.order) o.result.pushBack(v);
        emitTree(r.parent, r.depth, "visited");
        o.log.pushBack(std::string(algorithm == "dfs" ? "DFS" : "BFS") + " order: " + join(o.result));
        o.complexity = cx({{"time (list)", "O(V + E)"}, {"time (matrix)", "O(V²)"}, {"space", "O(V)"}, {"structure", algorithm == "dfs" ? "stack" : "queue"}});
    } else if (algorithm == "dijkstra") {
        auto r = algo::dijkstra(g, src, ds::WeightMode::Distance, &c);
        DynamicArray<int> depth; for (int v = 0; v < vertices; ++v) depth.pushBack(r.dist[v] == ds::Graph::INF ? -1 : 0);
        emitTree(r.parent, depth, "settled");
        for (int v = 0; v < vertices; ++v) o.log.pushBack("dist(V" + s(src) + "→V" + s(v) + ") = " + (r.dist[v] == ds::Graph::INF ? std::string("∞") : d2(r.dist[v])));
        int t = (int)(target >= 0 && target < vertices ? target : vertices - 1);
        auto path = algo::reconstructPath(r.parent, src, t);
        for (int v : path) o.result.pushBack(v);
        o.log.pushBack(path.empty() ? "V" + s(t) + " unreachable" : "shortest path: " + join(o.result));
        o.stats.pushBack(st("relaxations", (long)r.relaxations)); o.stats.pushBack(st("settled", (long)r.settled));
        o.complexity = cx({{"time", "O((V + E) log V) with binary heap"}, {"weights", "non-negative only"}, {"space", "O(V)"}});
    } else if (algorithm == "kruskal" || algorithm == "prim") {
        auto r = algorithm == "prim" ? algo::prim(g, ds::WeightMode::Distance, &c) : algo::kruskal(g, ds::WeightMode::Distance, &c);
        DynamicArray<int> parent; for (int v = 0; v < vertices; ++v) parent.pushBack(-1);
        for (const auto& e : r.edges) { o.log.pushBack("MST edge V" + s(e.u) + " — V" + s(e.v) + " (" + d2(e.w) + ")"); if (parent[e.v] < 0) parent[e.v] = e.u; else if (parent[e.u] < 0) parent[e.u] = e.v; }
        DynamicArray<int> depth; for (int v = 0; v < vertices; ++v) depth.pushBack(0);
        emitTree(parent, depth, "mst");
        o.stats.pushBack(st("total weight", d2(r.totalWeight))); o.stats.pushBack(st("components", (long)r.components));
        o.complexity = algorithm == "prim" ? cx({{"time", "O((V + E) log V)"}, {"strategy", "grow one tree from a vertex"}})
                                           : cx({{"time", "O(E log E)"}, {"strategy", "sorted edges + union-find"}});
    } else if (algorithm == "components") {
        DynamicArray<int> comp;
        int n = algo::connectedComponents(g, comp);
        for (int v = 0; v < vertices; ++v) { LabNode x; x.id = s(v); x.label = "V" + s(v); x.color = "c" + s(comp[v]); o.nodes.pushBack(x); o.result.pushBack(comp[v]); }
        o.stats.pushBack(st("components", (long)n));
        o.complexity = cx({{"time", "O(V + E)"}});
    } else throw ValidationError("algorithm must be bfs, dfs, dijkstra, kruskal, prim or components");
    o.stats.pushBack(st("vertices", (long)vertices)); o.stats.pushBack(st("arcs", (long)g.arcCount()));
    o.stats.pushBack(st("comparisons", (long)c.comparisons)); o.stats.pushBack(st("operations", (long)c.operations));
    o.stats.pushBack(st("time (µs)", d2(us(t0))));
    o.stats.pushBack(st("list memory (bytes)", (long)(vertices * sizeof(void*) + g.arcCount() * sizeof(ds::Graph::EdgeNode))));
    o.stats.pushBack(st("matrix memory (bytes)", (long)((long)vertices * vertices * (long)sizeof(double))));
    return o;
}

// ============================================================ spatial
LabOutput DsaLabService::spatial(const std::string& structure, int points, unsigned seed, double qLat, double qLon, double radiusKm, int k) {
    if (points < 1 || points > 3000) throw ValidationError("points must be 1..3000");
    if (!geo::validLatLon(qLat, qLon)) throw ValidationError("invalid query point");
    LabOutput o; o.structure = structure; o.operation = "query";
    Rng rng(seed);
    DynamicArray<ds::GeoPoint> pts;
    for (int i = 0; i < points; ++i) pts.pushBack(ds::GeoPoint{i, 28.40 + 0.45 * rng.uniform(), 76.95 + 0.60 * rng.uniform(), 0});
    ds::GeoRect box{qLat - radiusKm / 111.195, qLon - radiusKm / (111.195 * std::cos(geo::toRad(qLat))), qLat + radiusKm / 111.195, qLon + radiusKm / (111.195 * std::cos(geo::toRad(qLat)))};
    ds::HashTable<long, bool> hit;
    // brute force baseline
    auto t0 = Clock::now();
    std::size_t bruteInBox = 0;
    for (const auto& p : pts) if (box.contains(p.lat, p.lon)) ++bruteInBox;
    double bruteUs = us(t0);
    if (structure == "kd") {
        ds::KDTree t; t.build(pts);
        auto t1 = Clock::now();
        std::size_t visited = 0;
        auto knn = t.kNearest(qLat, qLon, (std::size_t)(k > 0 ? k : 5), [](const ds::GeoPoint&) { return true; }, &visited);
        double kdUs = us(t1);
        for (const auto& h : knn) { hit.insert(h.point.id, true); o.log.pushBack("#" + s(h.point.id) + " at " + d2(h.distanceKm) + " km"); }
        auto t2 = Clock::now();
        DynamicArray<ds::GeoHit> all;
        for (const auto& p : pts) all.pushBack(ds::GeoHit{p, geo::haversineKm(qLat, qLon, p.lat, p.lon)});
        algo::mergeSort(all, [](const ds::GeoHit& a, const ds::GeoHit& b) { return a.distanceKm < b.distanceKm; });
        double linUs = us(t2);
        bool agree = !knn.empty() && knn[0].point.id == all[0].point.id;
        o.stats.pushBack(st("k", (long)knn.size())); o.stats.pushBack(st("nodes visited (KD)", (long)visited)); o.stats.pushBack(st("points (linear scan)", (long)points));
        o.stats.pushBack(st("KD query µs", d2(kdUs))); o.stats.pushBack(st("linear scan + sort µs", d2(linUs)));
        o.stats.pushBack(st("nearest matches brute force", agree ? "yes" : "NO"));
        o.stats.pushBack(st("tree height", (long)t.height()));
        o.complexity = cx({{"build", "O(n log² n)"}, {"nearest (typical)", "≈ O(log n)"}, {"nearest (worst)", "O(n) — not guaranteed logarithmic"}, {"metric", "exact Haversine via 3-D unit vectors"}});
    } else if (structure == "quad") {
        ds::QuadTree q(ds::GeoRect{28.0, 76.5, 29.0, 77.8});
        for (const auto& p : pts) q.insert(p);
        auto t1 = Clock::now();
        auto res = q.queryRect(box);
        double qUs = us(t1);
        for (const auto& p : res) hit.insert(p.id, true);
        for (const auto& c : q.leafCells()) {
            DynamicArray<std::string> row;
            row.pushBack(d2(c.bounds.minLat)); row.pushBack(d2(c.bounds.minLon)); row.pushBack(d2(c.bounds.maxLat)); row.pushBack(d2(c.bounds.maxLon)); row.pushBack(s((long)c.count)); row.pushBack(s((long)c.depth));
            o.table.pushBack(row);
        }
        o.stats.pushBack(st("points in box", (long)res.size())); o.stats.pushBack(st("brute force agrees", res.size() == bruteInBox ? "yes" : "NO"));
        o.stats.pushBack(st("quadtree µs", d2(qUs))); o.stats.pushBack(st("linear scan µs", d2(bruteUs)));
        o.stats.pushBack(st("depth", (long)q.depth())); o.stats.pushBack(st("leaf cells", (long)o.table.size()));
        o.complexity = cx({{"insert", "O(depth)"}, {"range query", "O(depth + cells + m), worst O(n)"}, {"split", "capacity 4, max depth 14"}});
    } else if (structure == "range") {
        ds::RangeTree2D rt; rt.build(pts);
        auto t1 = Clock::now();
        std::size_t visited = 0;
        auto res = rt.query(box, &visited);
        double rUs = us(t1);
        for (const auto& p : res) hit.insert(p.id, true);
        o.stats.pushBack(st("points in box", (long)res.size())); o.stats.pushBack(st("brute force agrees", res.size() == bruteInBox ? "yes" : "NO"));
        o.stats.pushBack(st("range tree µs", d2(rUs))); o.stats.pushBack(st("linear scan µs", d2(bruteUs))); o.stats.pushBack(st("primary nodes visited", (long)visited));
        o.complexity = cx({{"build", "O(n log n)"}, {"query", "O(log² n + m) guaranteed"}, {"space", "O(n log n)"}});
    } else throw ValidationError("structure must be kd, quad or range");
    for (const auto& p : pts) {
        if (points > 600 && !hit.contains(p.id) && (p.id % 3)) continue;        // thin out large plots
        LabNode x; x.id = s(p.id); x.label = d2(p.lat) + "," + d2(p.lon); x.color = hit.contains(p.id) ? "hit" : "point";
        x.parent = std::to_string(p.lat); x.edge = std::to_string(p.lon);      // coordinates carried in parent/edge fields
        o.nodes.pushBack(x);
    }
    return o;
}

// ============================================================ text
LabOutput DsaLabService::text(const std::string& structure, const std::string& corpus, const std::string& pattern) {
    if (corpus.size() > 5000) throw ValidationError("corpus must be at most 5000 characters");
    LabOutput o; o.structure = structure; o.operation = "search '" + pattern + "'";
    if (structure == "trie") {
        ds::Trie t;
        long wid = 0;
        DynamicArray<std::string> ws;
        std::string cur;
        for (char ch : corpus + " ") { if (ch == ' ' || ch == ',' || ch == '\n' || ch == '.') { if (!cur.empty()) { t.insert(cur, ++wid); ws.pushBack(cur); } cur.clear(); } else cur += ch; }
        auto m = t.withPrefix(pattern, 50);
        for (const auto& x : m) o.log.pushBack(x.word + "  (occurs " + s((long)x.ids.size()) + "×)");
        o.stats.pushBack(st("distinct words", (long)t.wordCount())); o.stats.pushBack(st("prefix matches", (long)m.size()));
        o.stats.pushBack(st("exact word present", t.search(pattern) ? "yes" : "no"));
        o.complexity = cx({{"insert/search", "O(L·σ)"}, {"prefix listing", "O(P·σ + output)"}, {"NEXA use", "incident/resource autocomplete"}});
    } else if (structure == "suffix_array") {
        ds::SuffixArray sa;
        sa.addDocument(1, corpus);
        sa.build();
        std::size_t cmps = 0;
        auto occ = sa.find(pattern, &cmps);
        for (const auto& x : occ) { o.result.pushBack(x.offset); }
        const auto& arr = sa.suffixArray(); const auto& lcp = sa.lcp(); const auto& txt = sa.text();
        for (std::size_t i = 0; i < arr.size() && i < 60; ++i) {
            DynamicArray<std::string> row;
            std::string suf = txt.substr((std::size_t)arr[i], 24);
            for (char& ch : suf) if (ch == '\x01') ch = '$';
            row.pushBack(s((long)i)); row.pushBack(s(arr[i])); row.pushBack(s(lcp[i])); row.pushBack(suf);
            o.table.pushBack(row);
        }
        o.log.pushBack("occurrences at offsets: " + join(o.result));
        o.stats.pushBack(st("text length", (long)txt.size())); o.stats.pushBack(st("occurrences", (long)occ.size())); o.stats.pushBack(st("binary-search comparisons", (long)cmps));
        o.complexity = cx({{"build", "O(n log n) prefix doubling + radix"}, {"LCP", "O(n) Kasai"}, {"search", "O(m log n + occ)"}, {"space", "O(n)"}});
    } else if (structure == "suffix_tree") {
        if (corpus.size() > 1500) throw ValidationError("suffix tree demo limited to 1500 characters (naive O(n²) build)");
        ds::SuffixTree t;
        auto t0 = Clock::now();
        t.build(corpus);
        double buildUs = us(t0);
        auto occ = t.occurrences(pattern);
        DynamicArray<long> sorted = occ; algo::mergeSort(sorted, [](long a, long b) { return a < b; });
        o.result = sorted;
        o.log.pushBack("occurrences at offsets: " + join(sorted));
        o.stats.pushBack(st("nodes", (long)t.nodeCount())); o.stats.pushBack(st("occurrences", (long)occ.size())); o.stats.pushBack(st("build µs", d2(buildUs)));
        o.complexity = cx({{"build here", "O(n²) naive insertion (Ukkonen: O(n))"}, {"search", "O(m·σ + occ)"}, {"space", "O(n) nodes"}});
    } else throw ValidationError("structure must be trie, suffix_array or suffix_tree");
    return o;
}

// ============================================================ interval / segment
LabOutput DsaLabService::intervals(const DynamicArray<long>& flat, long qFrom, long qTo) {
    if (flat.size() % 2) throw ValidationError("intervals must be given as start,end pairs");
    limitInput(flat.size() / 2, 200, "intervals");
    LabOutput o; o.structure = "interval tree"; o.operation = "overlap [" + s(qFrom) + ", " + s(qTo) + "]";
    ds::IntervalTree t;
    for (std::size_t i = 0; i + 1 < flat.size(); i += 2) t.insert(ds::Interval{flat[i], flat[i + 1], (long)(i / 2) + 1});
    for (const auto& iv : t.allOverlaps(qFrom, qTo)) { o.result.pushBack(iv.id); o.log.pushBack("#" + s(iv.id) + " [" + s(iv.start) + ", " + s(iv.end) + "] overlaps"); }
    t.inorder([&](const ds::Interval& iv) { DynamicArray<std::string> row; row.pushBack(s(iv.id)); row.pushBack(s(iv.start)); row.pushBack(s(iv.end)); o.table.pushBack(row); });
    o.stats.pushBack(st("intervals", (long)t.size())); o.stats.pushBack(st("overlapping", (long)o.result.size()));
    o.stats.pushBack(st("any overlap", t.anyOverlap(qFrom, qTo) ? "yes" : "no"));
    o.complexity = cx({{"insert/delete", "O(log n) (AVL-balanced)"}, {"any overlap", "O(log n)"}, {"all overlaps", "O(min(n, k log n))"}, {"NEXA use", "resource commitments, bed reservations"}});
    return o;
}

LabOutput DsaLabService::segment(const DynamicArray<long>& values, long l, long r, long addL, long addR, long addValue) {
    limitInput(values.size(), 512, "values");
    if (values.empty()) throw ValidationError("Provide at least one value");
    LabOutput o; o.structure = "segment tree"; o.operation = "range query";
    DynamicArray<double> v; for (long x : values) v.pushBack((double)x);
    ds::SegmentTree t; t.build(v);
    long n = (long)values.size();
    auto clamp = [&](long x) { return x < 0 ? 0 : (x >= n ? n - 1 : x); };
    l = clamp(l); r = clamp(r); if (l > r) { long tmp = l; l = r; r = tmp; }
    o.log.pushBack("sum[" + s(l) + ".." + s(r) + "] = " + d2(t.rangeSum((std::size_t)l, (std::size_t)r)));
    o.log.pushBack("max[" + s(l) + ".." + s(r) + "] = " + d2(t.rangeMax((std::size_t)l, (std::size_t)r)));
    if (addValue != 0) {
        long a = clamp(addL), b = clamp(addR); if (a > b) { long tmp = a; a = b; b = tmp; }
        t.rangeAdd((std::size_t)a, (std::size_t)b, (double)addValue);
        o.log.pushBack("lazy range add +" + s(addValue) + " on [" + s(a) + ".." + s(b) + "]");
        o.log.pushBack("sum[" + s(l) + ".." + s(r) + "] after update = " + d2(t.rangeSum((std::size_t)l, (std::size_t)r)));
        o.log.pushBack("max[" + s(l) + ".." + s(r) + "] after update = " + d2(t.rangeMax((std::size_t)l, (std::size_t)r)));
    }
    for (long i = 0; i < n; ++i) o.result.pushBack((long)std::llround(t.rangeSum((std::size_t)i, (std::size_t)i)));
    o.stats.pushBack(st("n", n)); o.stats.pushBack(st("tree cells", 4 * n));
    o.complexity = cx({{"build", "O(n)"}, {"range sum / max", "O(log n)"}, {"range add (lazy)", "O(log n)"}, {"NEXA use", "analytics timeline buckets"}});
    return o;
}

LabOutput DsaLabService::sparse(const DynamicArray<long>& triplets, long rows, long cols) {
    if (triplets.size() % 3) throw ValidationError("sparse entries must be row,col,value triplets");
    if (rows < 1 || cols < 1 || rows > 100000 || cols > 100000) throw ValidationError("rows/cols must be 1..100000");
    LabOutput o; o.structure = "sparse matrix"; o.operation = "build";
    ds::SparseMatrix<long> m(rows, cols);
    for (std::size_t i = 0; i + 2 < triplets.size(); i += 3) {
        if (triplets[i] < 0 || triplets[i + 1] < 0 || triplets[i] >= rows || triplets[i + 1] >= cols) throw ValidationError("entry outside matrix bounds");
        m.set(triplets[i], triplets[i + 1], triplets[i + 2]);
    }
    for (const auto& e : m.entries()) { DynamicArray<std::string> row; row.pushBack(s(e.row)); row.pushBack(s(e.col)); row.pushBack(s(e.value)); o.table.pushBack(row); }
    auto t = m.transpose();
    o.log.pushBack("transpose has " + s((long)t.nonZeroCount()) + " non-zeros");
    o.stats.pushBack(st("non-zeros", (long)m.nonZeroCount())); o.stats.pushBack(st("density", d2(100.0 * m.density()) + " %"));
    o.stats.pushBack(st("sparse bytes", (long)(m.nonZeroCount() * sizeof(ds::SparseMatrix<long>::Entry))));
    o.stats.pushBack(st("dense bytes", (long)((double)rows * cols * sizeof(long))));
    o.complexity = cx({{"get", "O(log nnz)"}, {"set", "O(nnz)"}, {"space", "O(nnz) vs O(rows·cols)"}});
    return o;
}

LabOutput DsaLabService::multilist(const DynamicArray<long>& pairs) {
    if (pairs.size() % 2) throw ValidationError("links must be incident,resource pairs");
    limitInput(pairs.size() / 2, 200, "links");
    LabOutput o; o.structure = "multi-linked list"; o.operation = "link";
    ds::MultiLinkedList<long> m;
    for (std::size_t i = 0; i + 1 < pairs.size(); i += 2) m.link(pairs[i], pairs[i + 1], (long)(i / 2) + 1);
    ds::HashTable<long, bool> incs, ress;
    m.forEach([&](long a, long b, long) { incs.insert(a, true); ress.insert(b, true); });
    incs.forEach([&](long inc, bool) { std::string line = "incident " + s(inc) + " → resources:"; m.forEachResourceOf(inc, [&](long r, long) { line += " " + s(r); }); o.log.pushBack(line); });
    ress.forEach([&](long r, bool) { std::string line = "resource " + s(r) + " → incidents:"; m.forEachIncidentOf(r, [&](long i, long) { line += " " + s(i); }); o.log.pushBack(line); });
    o.stats.pushBack(st("relationships", (long)m.size()));
    o.complexity = cx({{"link", "O(H + deg)"}, {"resources of incident", "O(H + deg)"}, {"incidents of resource", "O(H + deg)"}, {"NEXA use", "deployment relationships"}});
    return o;
}

// ============================================================ backtracking
LabOutput DsaLabService::backtracking(const std::string& problem, int n, const DynamicArray<long>& grid, int rows, int cols) {
    LabOutput o; o.structure = "backtracking"; o.operation = problem;
    if (problem == "nqueens") {
        if (n < 1 || n > 12) throw ValidationError("n must be 1..12");
        auto t0 = Clock::now();
        auto r = algo::nQueens(n);
        double micro = us(t0);
        for (long c : r.firstSolution) o.result.pushBack(c);
        for (int row = 0; row < (int)r.firstSolution.size(); ++row) {
            DynamicArray<std::string> line;
            for (int c = 0; c < n; ++c) line.pushBack(r.firstSolution[row] == c ? "Q" : ".");
            o.table.pushBack(line);
        }
        o.stats.pushBack(st("solutions", (long)r.solutionCount)); o.stats.pushBack(st("nodes visited", (long)r.nodesVisited));
        o.stats.pushBack(st("backtracks", (long)r.backtracks)); o.stats.pushBack(st("time µs", d2(micro)));
        o.complexity = cx({{"time", "O(n!) worst"}, {"safety check", "O(1) with column/diagonal arrays"}});
    } else if (problem == "maze") {
        if (rows < 1 || cols < 1 || rows * cols > 900) throw ValidationError("maze must have 1..900 cells");
        DynamicArray<int> g; for (long v : grid) g.pushBack((int)v);
        auto r = algo::ratInMaze(g, rows, cols);
        for (int c : r.path) o.result.pushBack(c);
        o.log.pushBack(r.found ? "path found with " + s((long)r.path.size()) + " cells" : "no path from top-left to bottom-right");
        o.stats.pushBack(st("cells explored", (long)r.explored)); o.stats.pushBack(st("backtracks", (long)r.backtracks));
        o.complexity = cx({{"time", "O(rows·cols) with visited marking"}, {"framing", "rescue robot in a collapsed building"}});
    } else throw ValidationError("problem must be nqueens or maze");
    return o;
}

// ============================================================ benchmarks (§8.4)
DynamicArray<BenchmarkRow> DsaLabService::benchmark(const std::string& suite, long n, int repeats) {
    if (n < 10 || n > 50000) throw ValidationError("n must be 10..50000");
    if (repeats < 1 || repeats > 15) throw ValidationError("repeats must be 1..15");
    DynamicArray<BenchmarkRow> rows;
    Rng rng(20261116);
    DynamicArray<long> data;
    for (long i = 0; i < n; ++i) data.pushBack((long)(rng.next() % (unsigned long long)(n * 10)));
    // median of `repeats` timings to reduce noise
    auto measure = [&](const std::string& name, auto&& fn, const std::string& note = "") {
        DynamicArray<double> times; algo::OpCounter last; double mem = 0;
        for (int r = 0; r < repeats; ++r) { algo::OpCounter c; auto t0 = Clock::now(); mem = fn(c); times.pushBack(us(t0)); last = c; }
        double med = algo::medianSearch(times, [](double a, double b) { return a < b; });
        rows.pushBack(BenchmarkRow{name, n, med, (double)last.comparisons, (double)(last.operations + last.swaps), mem, note});
    };
    if (suite == "search") {
        DynamicArray<long> sorted = data; algo::mergeSort(sorted, [](long a, long b) { return a < b; });
        ds::HashTable<long, long> h; for (long i = 0; i < n; ++i) h.insert(data[i], i);
        DynamicArray<long> queries; for (int q = 0; q < 200; ++q) queries.pushBack(data[(std::size_t)(rng.next() % (unsigned long long)n)]);
        measure("linear search ×200", [&](algo::OpCounter& c) { for (long q : queries) algo::linearSearch(data, [&](long x) { return x == q; }, &c); return (double)n * sizeof(long); });
        measure("binary search ×200", [&](algo::OpCounter& c) { for (long q : queries) algo::binarySearch(sorted, q, [](long x) { return x; }, &c); return (double)n * sizeof(long); }, "requires sorted copy");
        measure("hash lookup ×200", [&](algo::OpCounter& c) { for (long q : queries) { h.find(q); ++c.comparisons; } return (double)h.capacity() * 24; }, "separate chaining");
    } else if (suite == "bst_avl") {
        DynamicArray<long> seq; for (long i = 0; i < n; ++i) seq.pushBack(i);
        long cap = n > 5000 ? 5000 : n;   // unbalanced BST on sorted input is O(n²)
        measure("BST insert random", [&](algo::OpCounter& c) { ds::BST<long, long> t; for (long v : data) t.insert(v, v, &c); c.operations = (std::size_t)t.height(); return (double)t.size() * 32; }, "operations = final height");
        measure("AVL insert random", [&](algo::OpCounter& c) { ds::AVLTree<long, long> t; for (long v : data) t.insert(v, v, &c); c.operations = (std::size_t)t.height(); return (double)t.size() * 40; }, "operations = final height");
        measure("BST insert sorted (n≤5000)", [&](algo::OpCounter& c) { ds::BST<long, long> t; for (long i = 0; i < cap; ++i) t.insert(i, i, &c); c.operations = (std::size_t)t.height(); return (double)t.size() * 32; }, "degenerates to a list");
        measure("AVL insert sorted", [&](algo::OpCounter& c) { ds::AVLTree<long, long> t; for (long v : seq) t.insert(v, v, &c); c.operations = (std::size_t)t.height(); return (double)t.size() * 40; }, "stays balanced");
    } else if (suite == "sorting") {
        auto less = [](long a, long b) { return a < b; };
        measure("merge sort", [&](algo::OpCounter& c) { auto a = data; algo::mergeSort(a, less, &c); return (double)n * 2 * sizeof(long); });
        measure("quick sort", [&](algo::OpCounter& c) { auto a = data; algo::quickSort(a, less, &c); return (double)n * sizeof(long); });
        measure("heap sort", [&](algo::OpCounter& c) { auto a = data; algo::heapSort(a, less, &c); return (double)n * sizeof(long); });
        measure("counting sort", [&](algo::OpCounter& c) { auto a = data; algo::countingSortBy(a, [](long x) { return x; }, 0, n * 10, &c); return (double)(n + n * 10) * sizeof(long); }, "k = 10n");
        measure("radix sort", [&](algo::OpCounter& c) { auto a = data; algo::radixSortBy(a, [](long x) { return x; }, &c); return (double)n * 2 * sizeof(long); });
        measure("bucket sort", [&](algo::OpCounter& c) { auto a = data; algo::bucketSortBy(a, [](long x) { return (double)x; }, 0.0, (double)n * 10, (std::size_t)(n / 4 + 1), &c); return (double)n * 2 * sizeof(long); });
        if (n <= 5000) measure("insertion sort", [&](algo::OpCounter& c) { auto a = data; algo::insertionSort(a, less, &c); return (double)n * sizeof(long); }, "O(n²) — only n ≤ 5000");
    } else if (suite == "hashing") {
        ds::CollisionStrategy all[] = {ds::CollisionStrategy::SeparateChaining, ds::CollisionStrategy::LinearProbing, ds::CollisionStrategy::QuadraticProbing, ds::CollisionStrategy::DoubleHashing};
        for (auto cs : all) {
            measure(ds::toString(cs), [&](algo::OpCounter& c) {
                ds::HashTable<long, long> h(cs, 16);
                for (long i = 0; i < n; ++i) h.insert(data[i] * 7 + i, i);
                for (long i = 0; i < n; ++i) h.find(data[i] * 7 + i);
                c.comparisons = h.collisions(); c.operations = h.probes();
                return (double)h.capacity() * 24;
            }, "comparisons = insert collisions, ops = lookup probes");
        }
    } else if (suite == "graph_repr") {
        int V = (int)(n > 3000 ? 3000 : n);
        ds::Graph g;
        for (int v = 0; v < V; ++v) g.addVertex(ds::VertexInfo{v, "", "", 0, 0});
        for (int v = 1; v < V; ++v) g.addEdge(v, (long)(rng.next() % (unsigned long long)v), ds::EdgeData{0, 1, 1, 0});     // spanning tree
        for (int e = 0; e < V * 2; ++e) g.addEdge((long)(rng.next() % (unsigned long long)V), (long)(rng.next() % (unsigned long long)V), ds::EdgeData{0, 1, 1, 0});
        auto m = g.buildAdjacencyMatrix(ds::WeightMode::Distance);
        measure("BFS adjacency list", [&](algo::OpCounter& c) { algo::bfs(g, 0, &c); return (double)(V * sizeof(void*) + g.arcCount() * sizeof(ds::Graph::EdgeNode)); }, "V = " + s(V) + ", sparse graph");
        measure("BFS adjacency matrix", [&](algo::OpCounter& c) { algo::bfsOnMatrix(m, V, 0, &c); return (double)V * V * sizeof(double); });
        measure("Dijkstra (list + heap)", [&](algo::OpCounter& c) { algo::dijkstra(g, 0, ds::WeightMode::Distance, &c); return 0.0; });
    } else if (suite == "spatial") {
        DynamicArray<ds::GeoPoint> pts;
        for (long i = 0; i < n; ++i) pts.pushBack(ds::GeoPoint{i, 28.4 + 0.5 * rng.uniform(), 76.9 + 0.6 * rng.uniform(), 0});
        ds::KDTree kd; kd.build(pts);
        DynamicArray<ds::GeoPoint> qs; for (int q = 0; q < 100; ++q) qs.pushBack(ds::GeoPoint{q, 28.4 + 0.5 * rng.uniform(), 76.9 + 0.6 * rng.uniform(), 0});
        measure("linear nearest ×100", [&](algo::OpCounter& c) {
            for (const auto& q : qs) { double best = 1e18; for (const auto& p : pts) { ++c.comparisons; double d = geo::haversineKm(q.lat, q.lon, p.lat, p.lon); if (d < best) best = d; } }
            return (double)n * sizeof(ds::GeoPoint); });
        measure("KD-tree nearest ×100", [&](algo::OpCounter& c) {
            for (const auto& q : qs) { std::size_t v = 0; kd.kNearest(q.lat, q.lon, 1, [](const ds::GeoPoint&) { return true; }, &v); c.comparisons += v; }
            return (double)n * 80; }, "comparisons = nodes visited");
    } else throw ValidationError("suite must be search, bst_avl, sorting, hashing, graph_repr or spatial");
    return rows;
}

// ============================================================ catalog
DynamicArray<CatalogEntry> DsaLabService::catalog() {
    DynamicArray<CatalogEntry> c;
    auto add = [&](const char* name, const char* group, const char* app, const char* file, std::initializer_list<std::pair<const char*, const char*>> cplx) {
        c.pushBack(CatalogEntry{name, group, app, file, cx(cplx)});
    };
    add("Dynamic Array", "Linear", "Registries of incidents, resources, hospitals", "DynamicArray.hpp", {{"access", "O(1)"}, {"append", "O(1)*"}, {"insert/delete", "O(n)"}});
    add("Stack", "Linear", "Operator action history (undo)", "Stack.hpp", {{"push/pop", "O(1)"}});
    add("Queue", "Linear", "FIFO emergency request intake", "Queue.hpp", {{"enqueue/dequeue", "O(1)"}});
    add("Linked List", "Linear", "Incident timeline, resource & request history", "LinkedList.hpp", {{"insert end", "O(1)"}, {"search", "O(n)"}});
    add("Multi-Linked List", "Linear", "Incident ↔ resource deployment relationships", "MultiLinkedList.hpp", {{"traverse by either side", "O(H + deg)"}});
    add("Sparse Matrix", "Linear", "Incident × resource allocation matrix", "SparseMatrix.hpp", {{"get", "O(log nnz)"}, {"space", "O(nnz)"}});
    add("Searching", "Search & Sort", "Resource lookup: linear / binary / median selection", "Searching.hpp", {{"linear", "O(n)"}, {"binary", "O(log n)"}, {"median of medians", "O(n)"}});
    add("Sorting", "Search & Sort", "Ranking resources, incidents, analytics", "Sorting.hpp", {{"merge/heap", "O(n log n)"}, {"quick", "O(n log n) avg"}, {"counting/radix", "O(n + k)"}});
    add("BST", "Trees", "Hospital index by name", "BST.hpp", {{"ops", "O(h)"}});
    add("AVL Tree", "Trees", "Balanced id & time indexes", "AVLTree.hpp", {{"ops", "O(log n)"}});
    add("Red-Black Tree", "Trees", "Balanced-tree comparison in the lab", "RedBlackTree.hpp", {{"ops", "O(log n)"}});
    add("B-Tree", "Trees", "Explains MySQL index behaviour", "BTree.hpp", {{"ops", "O(log_t n) node visits"}});
    add("B+ Tree", "Trees", "Range scans over a leaf chain", "BPlusTree.hpp", {{"range", "O(log n + m)"}});
    add("Threaded Binary Tree", "Trees", "Stackless ordered traversal", "ThreadedBinaryTree.hpp", {{"inorder", "O(n) time, O(1) space"}});
    add("K-ary Tree", "Trees", "Disaster-category taxonomy", "KaryTree.hpp", {{"traverse", "O(n)"}});
    add("Binary Heap / Priority Queue", "Priority", "Incident priority engine, urgent requests, Dijkstra, Prim", "BinaryHeap.hpp / PriorityQueue.hpp", {{"push/pop/update", "O(log n)"}});
    add("Graph", "Graphs", "Road network routing & connectivity", "Graph.hpp / GraphAlgorithms.hpp", {{"BFS/DFS", "O(V+E)"}, {"Dijkstra", "O((V+E) log V)"}, {"Kruskal", "O(E log E)"}});
    add("Hash Table", "Hashing", "Id/code lookups, priority-queue positions", "HashTable.hpp", {{"ops", "O(1) avg"}});
    add("KD Tree", "Spatial", "Nearest resources / hospitals", "KDTree.hpp", {{"nearest", "≈O(log n), worst O(n)"}});
    add("Quad Tree", "Spatial", "Map regions & density grid", "QuadTree.hpp", {{"range", "worst O(n)"}});
    add("Range Tree", "Spatial", "Guaranteed rectangle queries", "RangeTree.hpp", {{"query", "O(log² n + m)"}});
    add("Interval Tree", "Range & Temporal", "Resource commitments, bed reservations", "IntervalTree.hpp", {{"overlap", "O(log n)"}});
    add("Segment Tree", "Range & Temporal", "Analytics timeline range sums", "SegmentTree.hpp", {{"query/update", "O(log n)"}});
    add("Trie", "Text", "Autocomplete search", "Trie.hpp", {{"insert/search", "O(L)"}});
    add("Suffix Array", "Text", "Substring search in descriptions", "SuffixArray.hpp", {{"search", "O(m log n)"}});
    add("Suffix Tree", "Text", "Pattern search comparison", "SuffixTree.hpp", {{"search", "O(m)"}});
    add("Backtracking", "Backtracking", "Rat in a Maze, N-Queens", "Backtracking.hpp", {{"N-Queens", "O(n!)"}});
    return c;
}

} // namespace nexa::svc
