#pragma once
// ============================================================================
// NEXA :: Searching algorithms
//
//  linearSearch ...... O(n)        works on unsorted data
//  binarySearch ...... O(log n)    requires data sorted ascending by key
//  lowerBound ........ O(log n)    first index with key >= target
//  quickSelect ....... O(n) avg, O(n^2) worst
//  medianOfMedians ... O(n) worst  (deterministic selection)
//  medianSearch ...... selection-based median search (see note)
//
// NOTE on "Median Search" (spec §Module 6):
//  The spec asks for the syllabus-defined variant and forbids passing off
//  binary search under that name. This implementation treats Median Search as
//  *selection of the median / k-th order statistic* using the
//  median-of-medians pivot rule (Blum-Floyd-Pratt-Rivest-Tarjan), which is
//  the most common syllabus meaning. NEXA uses it to find the median
//  distance / suitability of candidate resources without fully sorting them.
//  If your course defines Median Search differently, replace only
//  `medianSearch` below — callers depend on its signature, not its internals.
// ============================================================================
#include <cstddef>
#include <stdexcept>
#include "OpCounter.hpp"
#include "../data_structures/DynamicArray.hpp"

namespace nexa::algo {

using ds::DynamicArray;

template <typename T, typename Pred>
long linearSearch(const DynamicArray<T>& a, Pred matches, OpCounter* c = nullptr) {
    for (std::size_t i = 0; i < a.size(); ++i) { cmp(c); if (matches(a[i])) return (long)i; }
    return -1;
}

/// Collects every matching element (linear filter).
template <typename T, typename Pred>
DynamicArray<T> linearSearchAll(const DynamicArray<T>& a, Pred matches, OpCounter* c = nullptr) {
    DynamicArray<T> out;
    for (std::size_t i = 0; i < a.size(); ++i) { cmp(c); if (matches(a[i])) out.pushBack(a[i]); }
    return out;
}

/// Iterative binary search. `key(x)` extracts the sort key.
template <typename T, typename K, typename KeyFn>
long binarySearch(const DynamicArray<T>& a, const K& target, KeyFn key, OpCounter* c = nullptr) {
    long lo = 0, hi = (long)a.size() - 1;
    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2;        // avoids overflow of (lo+hi)
        cmp(c);
        const K k = key(a[mid]);
        if (k == target) return mid;
        if (k < target) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

template <typename T, typename K, typename KeyFn>
long lowerBound(const DynamicArray<T>& a, const K& target, KeyFn key, OpCounter* c = nullptr) {
    long lo = 0, hi = (long)a.size();
    while (lo < hi) {
        long mid = lo + (hi - lo) / 2;
        cmp(c);
        if (key(a[mid]) < target) lo = mid + 1; else hi = mid;
    }
    return lo;
}

namespace detail {
template <typename T, typename Less>
void sel_swap(DynamicArray<T>& a, long i, long j) { T t = a[i]; a[i] = a[j]; a[j] = t; }

template <typename T, typename Less>
void smallInsertion(DynamicArray<T>& a, long lo, long hi, Less& less, OpCounter* c) {
    for (long i = lo + 1; i <= hi; ++i) {
        T k = a[i]; long j = i - 1;
        while (j >= lo) { cmp(c); if (!less(k, a[j])) break; a[j + 1] = a[j]; --j; }
        a[j + 1] = k;
    }
}

/// Partitions a[lo..hi] around the value `pivot`; returns final pivot index.
template <typename T, typename Less>
long partitionAround(DynamicArray<T>& a, long lo, long hi, const T& pivot, Less& less, OpCounter* c) {
    // move one pivot-equal element to hi
    for (long i = lo; i <= hi; ++i) { cmp(c); if (!less(a[i], pivot) && !less(pivot, a[i])) { sel_swap<T, Less>(a, i, hi); break; } }
    long store = lo;
    for (long i = lo; i < hi; ++i) { cmp(c); if (less(a[i], a[hi])) sel_swap<T, Less>(a, i, store++); }
    sel_swap<T, Less>(a, store, hi);
    return store;
}

/// Median-of-medians pivot: groups of 5, median of each, recurse on medians.
template <typename T, typename Less>
T pivotMoM(DynamicArray<T>& a, long lo, long hi, Less& less, OpCounter* c);

template <typename T, typename Less>
T selectMoM(DynamicArray<T>& a, long lo, long hi, long k, Less& less, OpCounter* c) {
    while (true) {
        if (hi - lo < 5) { smallInsertion(a, lo, hi, less, c); return a[lo + k]; }
        T pivot = pivotMoM(a, lo, hi, less, c);
        long p = partitionAround(a, lo, hi, pivot, less, c);
        long rank = p - lo;
        if (k == rank) return a[p];
        if (k < rank) hi = p - 1;
        else { k -= rank + 1; lo = p + 1; }
    }
}

template <typename T, typename Less>
T pivotMoM(DynamicArray<T>& a, long lo, long hi, Less& less, OpCounter* c) {
    DynamicArray<T> medians;
    for (long g = lo; g <= hi; g += 5) {
        long gEnd = g + 4 > hi ? hi : g + 4;
        smallInsertion(a, g, gEnd, less, c);
        medians.pushBack(a[g + (gEnd - g) / 2]);
    }
    return selectMoM(medians, 0, (long)medians.size() - 1, (long)medians.size() / 2, less, c);
}
} // namespace detail

/// k-th smallest (0-based), deterministic O(n). Works on a copy.
template <typename T, typename Less>
T medianOfMediansSelect(const DynamicArray<T>& input, long k, Less less, OpCounter* c = nullptr) {
    if (input.empty() || k < 0 || k >= (long)input.size()) throw std::out_of_range("select: k out of range");
    DynamicArray<T> a = input;
    return detail::selectMoM(a, 0, (long)a.size() - 1, k, less, c);
}

/// Randomised-free quickselect (middle pivot). O(n) average. Works on a copy.
template <typename T, typename Less>
T quickSelect(const DynamicArray<T>& input, long k, Less less, OpCounter* c = nullptr) {
    if (input.empty() || k < 0 || k >= (long)input.size()) throw std::out_of_range("select: k out of range");
    DynamicArray<T> a = input;
    long lo = 0, hi = (long)a.size() - 1;
    while (lo < hi) {
        T pivot = a[lo + (hi - lo) / 2];
        long p = detail::partitionAround(a, lo, hi, pivot, less, c);
        if (k == p) return a[p];
        if (k < p) hi = p - 1; else lo = p + 1;
    }
    return a[lo];
}

/// Median Search: returns the (lower) median element using median-of-medians.
template <typename T, typename Less>
T medianSearch(const DynamicArray<T>& a, Less less, OpCounter* c = nullptr) {
    return medianOfMediansSelect(a, ((long)a.size() - 1) / 2, less, c);
}

} // namespace nexa::algo
