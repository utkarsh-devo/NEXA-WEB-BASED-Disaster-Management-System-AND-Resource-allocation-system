#pragma once
// ============================================================================
// NEXA :: SegmentTree — range SUM and range MAX over a fixed-size array,
// with lazy propagation for range-add updates.
// Application: analytics timelines. Index = time bucket (hour/day);
// value = incidents registered (or deployments). "How many incidents between
// day 3 and day 9?" and "peak hour in a window" are O(log n).
//
//  build ............... O(n)
//  pointUpdate / pointAdd O(log n)
//  rangeAdd ............ O(log n)  (lazy)
//  rangeSum / rangeMax . O(log n)
//  space ............... O(4n)
// ============================================================================
#include <cstddef>
#include <stdexcept>
#include "DynamicArray.hpp"

namespace nexa::ds {

class SegmentTree {
public:
    explicit SegmentTree(std::size_t n = 0) { DynamicArray<double> z; for (std::size_t i = 0; i < n; ++i) z.pushBack(0); build(z); }

    void build(const DynamicArray<double>& values) {
        n_ = values.size();
        sum_.clear(); max_.clear(); lazy_.clear();
        std::size_t cells = n_ ? 4 * n_ : 1;
        for (std::size_t i = 0; i < cells; ++i) { sum_.pushBack(0); max_.pushBack(0); lazy_.pushBack(0); }
        if (n_) buildRec(values, 1, 0, (long)n_ - 1);
    }

    void pointAdd(std::size_t idx, double delta) { check(idx, idx); rangeAdd(idx, idx, delta); }
    void pointSet(std::size_t idx, double value) { pointAdd(idx, value - rangeSum(idx, idx)); }
    void rangeAdd(std::size_t l, std::size_t r, double delta) { check(l, r); addRec(1, 0, (long)n_ - 1, (long)l, (long)r, delta); }
    double rangeSum(std::size_t l, std::size_t r) { check(l, r); return sumRec(1, 0, (long)n_ - 1, (long)l, (long)r); }
    double rangeMax(std::size_t l, std::size_t r) { check(l, r); return maxRec(1, 0, (long)n_ - 1, (long)l, (long)r); }
    std::size_t size() const { return n_; }

private:
    std::size_t n_ = 0;
    DynamicArray<double> sum_, max_, lazy_;

    void check(std::size_t l, std::size_t r) const { if (l > r || r >= n_) throw std::out_of_range("SegmentTree: bad range"); }

    void buildRec(const DynamicArray<double>& v, std::size_t node, long lo, long hi) {
        if (lo == hi) { sum_[node] = max_[node] = v[(std::size_t)lo]; return; }
        long mid = (lo + hi) / 2;
        buildRec(v, 2 * node, lo, mid);
        buildRec(v, 2 * node + 1, mid + 1, hi);
        pull(node);
    }
    void pull(std::size_t node) {
        sum_[node] = sum_[2 * node] + sum_[2 * node + 1];
        max_[node] = max_[2 * node] > max_[2 * node + 1] ? max_[2 * node] : max_[2 * node + 1];
    }
    void apply(std::size_t node, long lo, long hi, double d) {
        sum_[node] += d * (double)(hi - lo + 1);
        max_[node] += d;
        lazy_[node] += d;
    }
    void push(std::size_t node, long lo, long hi) {
        if (lazy_[node] == 0 || lo == hi) return;
        long mid = (lo + hi) / 2;
        apply(2 * node, lo, mid, lazy_[node]);
        apply(2 * node + 1, mid + 1, hi, lazy_[node]);
        lazy_[node] = 0;
    }
    void addRec(std::size_t node, long lo, long hi, long l, long r, double d) {
        if (r < lo || hi < l) return;
        if (l <= lo && hi <= r) { apply(node, lo, hi, d); return; }
        push(node, lo, hi);
        long mid = (lo + hi) / 2;
        addRec(2 * node, lo, mid, l, r, d);
        addRec(2 * node + 1, mid + 1, hi, l, r, d);
        pull(node);
    }
    double sumRec(std::size_t node, long lo, long hi, long l, long r) {
        if (r < lo || hi < l) return 0;
        if (l <= lo && hi <= r) return sum_[node];
        push(node, lo, hi);
        long mid = (lo + hi) / 2;
        return sumRec(2 * node, lo, mid, l, r) + sumRec(2 * node + 1, mid + 1, hi, l, r);
    }
    double maxRec(std::size_t node, long lo, long hi, long l, long r) {
        if (r < lo || hi < l) return -1e300;
        if (l <= lo && hi <= r) return max_[node];
        push(node, lo, hi);
        long mid = (lo + hi) / 2;
        double a = maxRec(2 * node, lo, mid, l, r), b = maxRec(2 * node + 1, mid + 1, hi, l, r);
        return a > b ? a : b;
    }
};

} // namespace nexa::ds
