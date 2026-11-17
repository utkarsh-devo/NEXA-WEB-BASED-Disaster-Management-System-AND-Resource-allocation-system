#pragma once
// ============================================================================
// NEXA :: BinaryHeap<T, Compare>
// Array-backed complete binary tree. Compare(a,b)==true means a must sit
// ABOVE b. MaxCompare -> Max-Heap, MinCompare -> Min-Heap.
//   insert ........ O(log n)      extract ....... O(log n)
//   peek .......... O(1)          buildHeap ..... O(n)  (bottom-up heapify)
// Children of i: 2i+1, 2i+2   Parent of i: (i-1)/2
// ============================================================================
#include <cstddef>
#include <stdexcept>
#include <utility>
#include "DynamicArray.hpp"
#include "../algorithms/OpCounter.hpp"

namespace nexa::ds {

template <typename T> struct MaxCompare { bool operator()(const T& a, const T& b) const { return b < a; } };
template <typename T> struct MinCompare { bool operator()(const T& a, const T& b) const { return a < b; } };

template <typename T, typename Compare = MaxCompare<T>>
class BinaryHeap {
public:
    explicit BinaryHeap(Compare cmp = Compare(), algo::OpCounter* counter = nullptr) : cmp_(cmp), counter_(counter) {}

    void insert(const T& v) {
        data_.pushBack(v);
        siftUp(data_.size() - 1);
    }

    const T& peek() const {
        if (data_.empty()) throw std::underflow_error("BinaryHeap::peek on empty heap");
        return data_[0];
    }

    T extract() {
        if (data_.empty()) throw std::underflow_error("BinaryHeap::extract on empty heap");
        T top = data_[0];
        data_[0] = data_.back();
        data_.popBack();
        if (!data_.empty()) heapify(0);
        return top;
    }

    /// Floyd's bottom-up build: O(n)
    void buildHeap(const DynamicArray<T>& items) {
        data_ = items;
        if (data_.size() < 2) return;
        for (long i = (long)data_.size() / 2 - 1; i >= 0; --i) heapify((std::size_t)i);
    }

    /// Sift-down from index i (a.k.a. heapify / percolate-down)
    void heapify(std::size_t i) {
        std::size_t n = data_.size();
        while (true) {
            std::size_t best = i, l = 2 * i + 1, r = 2 * i + 2;
            if (l < n) { algo::cmp(counter_); if (cmp_(data_[l], data_[best])) best = l; }
            if (r < n) { algo::cmp(counter_); if (cmp_(data_[r], data_[best])) best = r; }
            if (best == i) return;
            std::swap(data_[i], data_[best]); algo::swp(counter_);
            i = best;
        }
    }

    bool empty() const { return data_.empty(); }
    std::size_t size() const { return data_.size(); }
    const DynamicArray<T>& raw() const { return data_; }   // level-order array (for visualisation)
    void clear() { data_.clear(); }

private:
    DynamicArray<T> data_;
    Compare cmp_;
    algo::OpCounter* counter_;

    void siftUp(std::size_t i) {
        while (i > 0) {
            std::size_t p = (i - 1) / 2;
            algo::cmp(counter_);
            if (!cmp_(data_[i], data_[p])) return;
            std::swap(data_[i], data_[p]); algo::swp(counter_);
            i = p;
        }
    }
};

} // namespace nexa::ds
