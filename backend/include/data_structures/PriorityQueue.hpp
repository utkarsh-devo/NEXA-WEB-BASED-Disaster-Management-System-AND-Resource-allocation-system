#pragma once
// ============================================================================
// NEXA :: IndexedPriorityQueue
// A binary-heap priority queue of (id, priority) pairs that additionally
// keeps id -> heap position in a HashTable, so the priority of an element
// already in the queue can be changed (increase/decrease-key) or the
// element removed in O(log n). This is required because incident priority
// changes as incidents age, escalate or get resources assigned.
//
// Ties are broken by an increasing sequence number (earlier = first), so
// equal-priority incidents are served FIFO — a documented, deterministic rule.
//
//   push / pop / update / remove ... O(log n)
//   top / contains ................. O(1) average
//   snapshotOrdered ................ O(n log n)  (works on a copy)
// ============================================================================
#include <cstddef>
#include <stdexcept>
#include <utility>
#include "DynamicArray.hpp"
#include "HashTable.hpp"

namespace nexa::ds {

class IndexedPriorityQueue {
public:
    struct Entry { long id = 0; double priority = 0; unsigned long seq = 0; };

    explicit IndexedPriorityQueue(bool maxFirst = true) : maxFirst_(maxFirst) {}

    void push(long id, double priority) {
        if (pos_.contains(id)) { update(id, priority); return; }
        heap_.pushBack(Entry{id, priority, seq_++});
        pos_.insert(id, heap_.size() - 1);
        siftUp(heap_.size() - 1);
    }

    const Entry& top() const {
        if (heap_.empty()) throw std::underflow_error("PriorityQueue is empty");
        return heap_[0];
    }

    Entry pop() {
        if (heap_.empty()) throw std::underflow_error("PriorityQueue is empty");
        Entry t = heap_[0];
        removeAtIndex(0);
        return t;
    }

    bool update(long id, double priority) {
        std::size_t* p = pos_.find(id);
        if (!p) return false;
        std::size_t i = *p;
        double old = heap_[i].priority;
        heap_[i].priority = priority;
        if (better(priority, old)) siftUp(i); else siftDown(i);
        return true;
    }

    bool remove(long id) {
        std::size_t* p = pos_.find(id);
        if (!p) return false;
        removeAtIndex(*p);
        return true;
    }

    bool contains(long id) const { return pos_.contains(id); }
    double priorityOf(long id) const { const std::size_t* p = pos_.find(id); return p ? heap_[*p].priority : 0.0; }
    std::size_t size() const { return heap_.size(); }
    bool empty() const { return heap_.empty(); }
    const DynamicArray<Entry>& raw() const { return heap_; }

    /// Full processing order without disturbing the live queue.
    DynamicArray<Entry> snapshotOrdered() const {
        IndexedPriorityQueue copy(maxFirst_);
        for (const Entry& e : heap_) { copy.heap_.pushBack(e); copy.pos_.insert(e.id, copy.heap_.size() - 1); }
        copy.seq_ = seq_;
        DynamicArray<Entry> out;
        while (!copy.empty()) out.pushBack(copy.pop());
        return out;
    }

    void clear() { heap_.clear(); pos_.clear(); }

private:
    DynamicArray<Entry> heap_;
    HashTable<long, std::size_t> pos_{CollisionStrategy::SeparateChaining, 64};
    bool maxFirst_;
    unsigned long seq_ = 0;

    bool better(double a, double b) const { return maxFirst_ ? a > b : a < b; }
    bool above(const Entry& a, const Entry& b) const {
        if (a.priority != b.priority) return better(a.priority, b.priority);
        return a.seq < b.seq;   // FIFO tie-break
    }
    void swapAt(std::size_t i, std::size_t j) {
        Entry t = heap_[i]; heap_[i] = heap_[j]; heap_[j] = t;
        pos_.insert(heap_[i].id, i);
        pos_.insert(heap_[j].id, j);
    }
    void siftUp(std::size_t i) {
        while (i > 0) {
            std::size_t p = (i - 1) / 2;
            if (!above(heap_[i], heap_[p])) break;
            swapAt(i, p); i = p;
        }
    }
    void siftDown(std::size_t i) {
        std::size_t n = heap_.size();
        while (true) {
            std::size_t b = i, l = 2 * i + 1, r = 2 * i + 2;
            if (l < n && above(heap_[l], heap_[b])) b = l;
            if (r < n && above(heap_[r], heap_[b])) b = r;
            if (b == i) return;
            swapAt(i, b); i = b;
        }
    }
    void removeAtIndex(std::size_t i) {
        long id = heap_[i].id;
        std::size_t last = heap_.size() - 1;
        if (i != last) swapAt(i, last);
        heap_.popBack();
        pos_.erase(id);
        if (i < heap_.size()) { siftUp(i); siftDown(i); }
    }
};

} // namespace nexa::ds
