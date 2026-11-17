// Unit tests: BST, AVL, Red-Black, B-Tree, B+Tree, Threaded BT, K-ary, Heap, PQ, HashTable
#include "support/TestFramework.hpp"
#include "data_structures/BST.hpp"
#include "data_structures/AVLTree.hpp"
#include "data_structures/RedBlackTree.hpp"
#include "data_structures/BTree.hpp"
#include "data_structures/BPlusTree.hpp"
#include "data_structures/ThreadedBinaryTree.hpp"
#include "data_structures/KaryTree.hpp"
#include "data_structures/BinaryHeap.hpp"
#include "data_structures/PriorityQueue.hpp"
#include "data_structures/HashTable.hpp"
#include <string>

using namespace nexa::ds;

TEST(BST, InsertSearchDeleteTraversals) {
    BST<int, std::string> t;
    int keys[] = {50, 30, 70, 20, 40, 60, 80};
    for (int k : keys) t.insert(k, "v" + std::to_string(k));
    EXPECT_EQ(t.size(), 7u);
    EXPECT_EQ(*t.search(40), "v40");
    EXPECT_EQ(t.search(45), nullptr);
    std::string pre;
    t.preorder([&](int k, const std::string&) { pre += std::to_string(k) + ","; });
    EXPECT_EQ(pre, std::string("50,30,20,40,70,60,80,"));
    EXPECT_TRUE(t.remove(30));          // two children
    EXPECT_TRUE(t.remove(80));          // leaf
    EXPECT_FALSE(t.remove(999));
    std::string in;
    t.inorder([&](int k, const std::string&) { in += std::to_string(k) + ","; });
    EXPECT_EQ(in, std::string("20,40,50,60,70,"));
}

TEST(BST, DegeneratesOnSortedInput) {
    BST<int, int> t;
    for (int i = 0; i < 200; ++i) t.insert(i, i);
    EXPECT_EQ(t.height(), 200);         // linked-list shaped — motivates AVL
}

TEST(AVL, StaysBalancedOnSortedInput) {
    AVLTree<int, int> t;
    for (int i = 0; i < 1000; ++i) t.insert(i, i * 2);
    EXPECT_LE(t.height(), 15);          // 1.44 log2(1000) ~ 14.4
    EXPECT_EQ(*t.search(777), 1554);
    for (int i = 0; i < 1000; i += 2) EXPECT_TRUE(t.remove(i));
    EXPECT_EQ(t.size(), 500u);
    EXPECT_EQ(t.search(4), nullptr);
    int prev = -1; bool ordered = true;
    t.inorder([&](int k, int) { if (k <= prev) ordered = false; prev = k; });
    EXPECT_TRUE(ordered);
    // every node balanced
    std::function<int(const AVLTree<int, int>::Node*)> check = [&](const AVLTree<int, int>::Node* n) -> int {
        if (!n) return 0;
        int l = check(n->left), r = check(n->right);
        EXPECT_LE(std::abs(l - r), 1);
        return 1 + (l > r ? l : r);
    };
    check(t.root());
}

TEST(AVL, RangeQuery) {
    AVLTree<int, int> t;
    for (int i = 1; i <= 50; ++i) t.insert(i, i);
    int sum = 0;
    t.range(10, 20, [&](int k, int) { sum += k; });
    EXPECT_EQ(sum, 165);
}

TEST(RedBlack, PropertiesHoldAfterInsertDelete) {
    RedBlackTree<int, int> t;
    for (int i = 0; i < 500; ++i) t.insert((i * 37) % 501, i);
    EXPECT_GT(t.validate(), 0);
    for (int i = 0; i < 500; i += 3) t.remove((i * 37) % 501);
    EXPECT_GT(t.validate(), 0);
    EXPECT_LE(t.height(), 2 * 9 + 2);
}

TEST(BTree, InsertSearchRemoveKeepsOrder) {
    BTree<int> b(3);
    for (int i = 0; i < 300; ++i) b.insert((i * 7919) % 1009);
    EXPECT_EQ(b.size(), 300u);
    EXPECT_TRUE(b.search((5 * 7919) % 1009));
    for (int i = 0; i < 300; i += 2) EXPECT_TRUE(b.remove((i * 7919) % 1009));
    EXPECT_EQ(b.size(), 150u);
    EXPECT_FALSE(b.search((4 * 7919) % 1009));
    EXPECT_TRUE(b.search((5 * 7919) % 1009));
    int prev = -1; bool ordered = true; std::size_t n = 0;
    b.inorder([&](int k) { if (k <= prev) ordered = false; prev = k; ++n; });
    EXPECT_TRUE(ordered);
    EXPECT_EQ(n, 150u);
}

TEST(BPlusTree, RangeScanViaLeafChain) {
    BPlusTree<int, int> t(4);
    for (int i = 100; i >= 1; --i) t.insert(i, i * i);
    EXPECT_EQ(t.size(), 100u);
    EXPECT_EQ(*t.search(9), 81);
    long sum = 0; int count = 0;
    t.range(10, 19, [&](int k, int) { sum += k; ++count; });
    EXPECT_EQ(count, 10);
    EXPECT_EQ(sum, 145);
    EXPECT_TRUE(t.remove(15));
    EXPECT_EQ(t.search(15), nullptr);
    EXPECT_GT(t.height(), 2);
}

TEST(ThreadedBinaryTree, StacklessInorder) {
    ThreadedBinaryTree<int> t;
    int keys[] = {50, 30, 70, 20, 40, 60, 80, 35};
    for (int k : keys) t.insert(k);
    EXPECT_FALSE(t.insert(50));
    std::string s, r;
    t.inorder([&](int k) { s += std::to_string(k) + ","; });
    t.reverseInorder([&](int k) { r += std::to_string(k) + ","; });
    EXPECT_EQ(s, std::string("20,30,35,40,50,60,70,80,"));
    EXPECT_EQ(r, std::string("80,70,60,50,40,35,30,20,"));
    EXPECT_TRUE(t.contains(35));
    EXPECT_FALSE(t.contains(36));
}

TEST(KaryTree, CategoryHierarchy) {
    KaryTree<int> t(5);
    t.setRoot(0, "All");
    t.addChild(0, 1, "Natural");
    t.addChild(1, 10, "Flood");
    t.addChild(1, 11, "Earthquake");
    auto path = t.pathToRoot(10);
    EXPECT_EQ(path.size(), 2u + 1u);
    EXPECT_EQ(path[1]->label, std::string("Natural"));
    EXPECT_EQ(t.findByLabel("flood")->id, 10);
    EXPECT_THROW(t.addChild(99, 12, "x"), std::invalid_argument);
    EXPECT_TRUE(t.removeSubtree(1));
    EXPECT_EQ(t.size(), 1u);
}

TEST(BinaryHeap, MaxMinAndBuild) {
    BinaryHeap<int> mx;
    BinaryHeap<int, MinCompare<int>> mn;
    int vals[] = {5, 1, 9, 3, 7};
    for (int v : vals) { mx.insert(v); mn.insert(v); }
    EXPECT_EQ(mx.extract(), 9);
    EXPECT_EQ(mn.extract(), 1);
    DynamicArray<int> arr{4, 10, 3, 5, 1};
    BinaryHeap<int> b; b.buildHeap(arr);
    EXPECT_EQ(b.peek(), 10);
    int last = 1000;
    while (!b.empty()) { int x = b.extract(); EXPECT_LE(x, last); last = x; }
}

TEST(IndexedPriorityQueue, UpdateRemoveAndTieBreak) {
    IndexedPriorityQueue pq(true);
    pq.push(1, 10); pq.push(2, 50); pq.push(3, 30); pq.push(4, 50);
    EXPECT_EQ(pq.top().id, 2);           // tie 2 vs 4 -> earlier first
    pq.update(3, 99);
    EXPECT_EQ(pq.top().id, 3);
    EXPECT_TRUE(pq.remove(3));
    auto order = pq.snapshotOrdered();
    EXPECT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0].id, 2);
    EXPECT_EQ(order[1].id, 4);
    EXPECT_EQ(order[2].id, 1);
    EXPECT_EQ(pq.size(), 3u);            // snapshot did not consume
    pq.update(1, 0.5);
    EXPECT_EQ(pq.pop().id, 2);
}

TEST(HashTable, AllStrategiesBehaveIdentically) {
    CollisionStrategy all[] = {CollisionStrategy::SeparateChaining, CollisionStrategy::LinearProbing,
                               CollisionStrategy::QuadraticProbing, CollisionStrategy::DoubleHashing};
    for (auto s : all) {
        HashTable<long, long> h(s, 8);
        for (long i = 0; i < 2000; ++i) h.insert(i * 31, i);
        EXPECT_EQ(h.size(), 2000u);
        EXPECT_LE(h.loadFactor(), 0.76);
        EXPECT_GT(h.rehashCount(), 0u);
        for (long i = 0; i < 2000; i += 2) EXPECT_TRUE(h.erase(i * 31));
        EXPECT_EQ(h.size(), 1000u);
        EXPECT_EQ(h.find(62), nullptr);
        ASSERT_TRUE(h.find(31) != nullptr);
        EXPECT_EQ(*h.find(31), 1);
        h.insert(31, 77);                 // overwrite
        EXPECT_EQ(*h.find(31), 77);
        EXPECT_EQ(h.size(), 1000u);
    }
    HashTable<std::string, int> names;
    names.insert("AMB-101", 1); names.insert("FIRE-7", 2);
    EXPECT_EQ(*names.find("FIRE-7"), 2);
    EXPECT_FALSE(names.contains("fire-7"));
}
