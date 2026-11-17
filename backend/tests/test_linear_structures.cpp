// Unit tests: DynamicArray, Stack, Queue, LinkedList, MultiLinkedList, SparseMatrix
#include "support/TestFramework.hpp"
#include "data_structures/DynamicArray.hpp"
#include "data_structures/Stack.hpp"
#include "data_structures/Queue.hpp"
#include "data_structures/LinkedList.hpp"
#include "data_structures/MultiLinkedList.hpp"
#include "data_structures/SparseMatrix.hpp"

using namespace nexa::ds;

TEST(DynamicArray, PushInsertRemoveGrow) {
    DynamicArray<int> a;
    for (int i = 0; i < 100; ++i) a.pushBack(i);
    EXPECT_EQ(a.size(), 100u);
    EXPECT_GE(a.capacity(), 100u);
    a.insertAt(0, -1);
    EXPECT_EQ(a[0], -1);
    EXPECT_EQ(a[1], 0);
    a.removeAt(0);
    EXPECT_EQ(a[0], 0);
    EXPECT_EQ(a.removeIf([](int x) { return x % 2 == 0; }), 50u);
    EXPECT_EQ(a[0], 1);
    EXPECT_THROW(a.at(500), std::out_of_range);
    DynamicArray<int> copy = a;
    copy[0] = 99;
    EXPECT_EQ(a[0], 1);   // deep copy
    EXPECT_EQ(a.findIndex([](int x) { return x == 7; }), 3);
}

TEST(Stack, LifoAndBounds) {
    Stack<int> s(3);
    s.push(1); s.push(2); s.push(3);
    EXPECT_TRUE(s.isFull());
    EXPECT_THROW(s.push(4), std::overflow_error);
    EXPECT_EQ(s.peek(), 3);
    EXPECT_EQ(s.pop(), 3);
    EXPECT_EQ(s.pop(), 2);
    EXPECT_EQ(s.pop(), 1);
    EXPECT_TRUE(s.isEmpty());
    EXPECT_THROW(s.pop(), std::underflow_error);
}

TEST(Queue, FifoWrapAroundAndGrowth) {
    Queue<int> q(2);
    for (int i = 0; i < 10; ++i) q.enqueue(i);
    EXPECT_EQ(q.front(), 0);
    EXPECT_EQ(q.rear(), 9);
    for (int i = 0; i < 5; ++i) EXPECT_EQ(q.dequeue(), i);
    for (int i = 10; i < 15; ++i) q.enqueue(i);   // wraps
    EXPECT_TRUE(q.removeFirst([](int x) { return x == 11; }));
    int expected[] = {5, 6, 7, 8, 9, 10, 12, 13, 14};
    for (int e : expected) EXPECT_EQ(q.dequeue(), e);
    EXPECT_THROW(q.dequeue(), std::underflow_error);
}

TEST(LinkedList, InsertPositionsSearchUpdateDelete) {
    LinkedList<int> l;
    l.insertBack(2); l.insertFront(1); l.insertBack(4); l.insertAt(2, 3);
    int i = 1;
    l.forEach([&](int v) { EXPECT_EQ(v, i++); });
    EXPECT_EQ(l.size(), 4u);
    EXPECT_TRUE(l.update([](int v) { return v == 3; }, [](int& v) { v = 30; }));
    EXPECT_EQ(l.get(2), 30);
    EXPECT_TRUE(l.removeFirst([](int v) { return v == 1; }));
    l.removeAt(l.size() - 1);
    EXPECT_EQ(l.size(), 2u);
    EXPECT_EQ(l.get(0), 2);
    EXPECT_EQ(l.find([](int v) { return v == 99; }), nullptr);
}

TEST(MultiLinkedList, ManyToManyTraversal) {
    MultiLinkedList<int> m;
    m.link(1, 100, 5); m.link(1, 101, 6); m.link(2, 100, 7);
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.degreeOfIncident(1), 2u);
    EXPECT_EQ(m.degreeOfResource(100), 2u);
    long sum = 0;
    m.forEachIncidentOf(100, [&](long inc, int v) { sum += inc * 10 + v; });
    EXPECT_EQ(sum, (1 * 10 + 5) + (2 * 10 + 7));
    m.link(1, 100, 50);                 // update, not duplicate
    EXPECT_EQ(*m.find(1, 100), 50);
    EXPECT_TRUE(m.unlink(1, 100));
    EXPECT_FALSE(m.unlink(1, 100));
    EXPECT_EQ(m.degreeOfResource(100), 1u);
    EXPECT_EQ(m.size(), 2u);
}

TEST(SparseMatrix, StoresOnlyNonZero) {
    SparseMatrix<int> s(1000, 1000);
    s.set(5, 7, 3); s.set(1, 2, 4); s.set(5, 1, 1);
    EXPECT_EQ(s.nonZeroCount(), 3u);
    EXPECT_EQ(s.get(5, 7), 3);
    EXPECT_EQ(s.get(999, 999), 0);
    s.add(5, 7, 2);
    EXPECT_EQ(s.get(5, 7), 5);
    EXPECT_EQ(s.rowSum(5), 6);
    s.set(1, 2, 0);                     // zero => removed
    EXPECT_EQ(s.nonZeroCount(), 2u);
    auto t = s.transpose();
    EXPECT_EQ(t.get(7, 5), 5);
    EXPECT_LT(s.density(), 0.001);
}
