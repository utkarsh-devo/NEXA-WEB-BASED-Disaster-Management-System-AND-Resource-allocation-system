#pragma once
// ============================================================================
// NEXA :: Sorting algorithms (all operate on ds::DynamicArray in place)
//
//  Algorithm      Time (best/avg/worst)          Space     Stable  NEXA use
//  ------------   ----------------------------   -------   ------  ----------------------------
//  Merge Sort     n log n / n log n / n log n    O(n)      yes     final ranked resource list
//  Quick Sort     n log n / n log n / n^2        O(log n)  no      general ranking (median-of-3)
//  Heap Sort      n log n / n log n / n log n    O(1)      no      priority demonstration
//  Counting Sort  n + k                          O(n + k)  yes     severity levels (k = 5)
//  Radix Sort     d (n + b)                      O(n + b)  yes     numeric ids / capacities
//  Bucket Sort    n + k avg / n^2 worst          O(n + k)  yes*    distances & scores in ranges
//  Insertion Sort n / n^2 / n^2                  O(1)      yes     inside buckets, tiny inputs
//  (* stable because buckets are filled in order and sorted with insertion sort)
//
// `less(a,b)` defines ascending order; pass a reversed comparator for
// descending. Every function accepts an optional OpCounter and an optional
// Trace callback ("compare"/"swap"/"write", i, j) used by the DSA Lab to
// animate the real execution.
// ============================================================================
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include "OpCounter.hpp"
#include "../data_structures/DynamicArray.hpp"

namespace nexa::algo {

using ds::DynamicArray;
using Trace = std::function<void(const char* action, long i, long j)>;

namespace detail {
inline void tr(const Trace* t, const char* a, long i, long j) { if (t && *t) (*t)(a, i, j); }
}

// ---------------------------------------------------------------- insertion
template <typename T, typename Less>
void insertionSort(DynamicArray<T>& a, Less less, OpCounter* c = nullptr, const Trace* t = nullptr) {
    for (std::size_t i = 1; i < a.size(); ++i) {
        T key = a[i];
        long j = (long)i - 1;
        while (j >= 0) {
            cmp(c); detail::tr(t, "compare", j, j + 1);
            if (!less(key, a[j])) break;
            a[j + 1] = a[j]; swp(c); detail::tr(t, "write", j + 1, j);
            --j;
        }
        a[j + 1] = key; detail::tr(t, "write", j + 1, -1);
    }
}

// ---------------------------------------------------------------- merge sort
namespace detail {
template <typename T, typename Less>
void mergeRec(DynamicArray<T>& a, DynamicArray<T>& tmp, long lo, long hi, Less& less, OpCounter* c, const Trace* t) {
    if (hi - lo < 1) return;
    long mid = lo + (hi - lo) / 2;
    mergeRec(a, tmp, lo, mid, less, c, t);
    mergeRec(a, tmp, mid + 1, hi, less, c, t);
    long i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
        cmp(c); tr(t, "compare", i, j);
        // `<=` behaviour (take left on ties) keeps the sort stable
        if (less(a[j], a[i])) tmp[k++] = a[j++]; else tmp[k++] = a[i++];
    }
    while (i <= mid) tmp[k++] = a[i++];
    while (j <= hi)  tmp[k++] = a[j++];
    for (long x = lo; x <= hi; ++x) { a[x] = tmp[x]; swp(c); tr(t, "write", x, -1); }
}
} // namespace detail

template <typename T, typename Less>
void mergeSort(DynamicArray<T>& a, Less less, OpCounter* c = nullptr, const Trace* t = nullptr) {
    if (a.size() < 2) return;
    DynamicArray<T> tmp = a;
    detail::mergeRec(a, tmp, 0, (long)a.size() - 1, less, c, t);
}

// ---------------------------------------------------------------- quick sort
namespace detail {
template <typename T>
void swapAt(DynamicArray<T>& a, long i, long j, OpCounter* c, const Trace* t) {
    if (i == j) return;
    T x = a[i]; a[i] = a[j]; a[j] = x; swp(c); tr(t, "swap", i, j);
}
template <typename T, typename Less>
long partition(DynamicArray<T>& a, long lo, long hi, Less& less, OpCounter* c, const Trace* t) {
    // median-of-three pivot selection reduces the chance of O(n^2) on sorted input
    long mid = lo + (hi - lo) / 2;
    cmp(c); if (less(a[mid], a[lo])) swapAt(a, mid, lo, c, t);
    cmp(c); if (less(a[hi], a[lo]))  swapAt(a, hi, lo, c, t);
    cmp(c); if (less(a[mid], a[hi])) swapAt(a, mid, hi, c, t);   // pivot now at hi
    T pivot = a[hi];
    long i = lo - 1;
    for (long j = lo; j < hi; ++j) {
        cmp(c); tr(t, "compare", j, hi);
        if (less(a[j], pivot)) swapAt(a, ++i, j, c, t);
    }
    swapAt(a, i + 1, hi, c, t);
    return i + 1;
}
template <typename T, typename Less>
void quickRec(DynamicArray<T>& a, long lo, long hi, Less& less, OpCounter* c, const Trace* t) {
    while (lo < hi) {
        long p = partition(a, lo, hi, less, c, t);
        // recurse into the smaller side first -> O(log n) stack depth
        if (p - lo < hi - p) { quickRec(a, lo, p - 1, less, c, t); lo = p + 1; }
        else                 { quickRec(a, p + 1, hi, less, c, t); hi = p - 1; }
    }
}
} // namespace detail

template <typename T, typename Less>
void quickSort(DynamicArray<T>& a, Less less, OpCounter* c = nullptr, const Trace* t = nullptr) {
    if (a.size() < 2) return;
    detail::quickRec(a, 0, (long)a.size() - 1, less, c, t);
}

// ---------------------------------------------------------------- heap sort
template <typename T, typename Less>
void heapSort(DynamicArray<T>& a, Less less, OpCounter* c = nullptr, const Trace* t = nullptr) {
    long n = (long)a.size();
    auto sift = [&](long i, long size) {
        while (true) {
            long big = i, l = 2 * i + 1, r = 2 * i + 2;
            if (l < size) { cmp(c); detail::tr(t, "compare", l, big); if (less(a[big], a[l])) big = l; }
            if (r < size) { cmp(c); detail::tr(t, "compare", r, big); if (less(a[big], a[r])) big = r; }
            if (big == i) return;
            detail::swapAt(a, i, big, c, t);
            i = big;
        }
    };
    for (long i = n / 2 - 1; i >= 0; --i) sift(i, n);       // build max-heap O(n)
    for (long end = n - 1; end > 0; --end) {                 // move max to the end
        detail::swapAt(a, 0, end, c, t);
        sift(0, end);
    }
}

// ---------------------------------------------------------------- counting sort
/// Stable counting sort by an integer key in [minKey, maxKey].
template <typename T, typename KeyFn>
void countingSortBy(DynamicArray<T>& a, KeyFn key, long minKey, long maxKey,
                    OpCounter* c = nullptr, const Trace* t = nullptr) {
    if (a.size() < 2) return;
    long k = maxKey - minKey + 1;
    DynamicArray<long> count((std::size_t)k);
    for (long i = 0; i < k; ++i) count.pushBack(0);
    for (const T& x : a) { long v = key(x); if (v < minKey || v > maxKey) throw std::out_of_range("countingSort: key outside declared range"); ++count[v - minKey]; op(c); }
    for (long i = 1; i < k; ++i) count[i] += count[i - 1];          // prefix sums -> final positions
    DynamicArray<T> out = a;
    for (long i = (long)a.size() - 1; i >= 0; --i) {                 // right-to-left keeps it stable
        long v = key(a[i]) - minKey;
        out[--count[v]] = a[i]; swp(c);
    }
    for (std::size_t i = 0; i < a.size(); ++i) { a[i] = out[i]; detail::tr(t, "write", (long)i, -1); }
}

// ---------------------------------------------------------------- radix sort
/// LSD radix sort (base 10) for non-negative integer keys. Stable.
template <typename T, typename KeyFn>
void radixSortBy(DynamicArray<T>& a, KeyFn key, OpCounter* c = nullptr, const Trace* t = nullptr) {
    if (a.size() < 2) return;
    long maxKey = 0;
    for (const T& x : a) { long v = key(x); if (v < 0) throw std::invalid_argument("radixSort: negative key"); if (v > maxKey) maxKey = v; }
    DynamicArray<T> out = a;
    for (long exp = 1; maxKey / exp > 0; exp *= 10) {
        long count[10] = {0};
        for (const T& x : a) { ++count[(key(x) / exp) % 10]; op(c); }
        for (int d = 1; d < 10; ++d) count[d] += count[d - 1];
        for (long i = (long)a.size() - 1; i >= 0; --i) { out[--count[(key(a[i]) / exp) % 10]] = a[i]; swp(c); }
        for (std::size_t i = 0; i < a.size(); ++i) { a[i] = out[i]; detail::tr(t, "write", (long)i, -1); }
    }
}

// ---------------------------------------------------------------- bucket sort
/// Bucket sort for real-valued keys in [lo, hi]. Values outside are clamped
/// into the first/last bucket. Each bucket is insertion-sorted.
template <typename T, typename KeyFn>
void bucketSortBy(DynamicArray<T>& a, KeyFn key, double lo, double hi, std::size_t buckets = 10,
                  OpCounter* c = nullptr, const Trace* t = nullptr) {
    if (a.size() < 2) return;
    if (buckets == 0) buckets = 1;
    if (hi <= lo) hi = lo + 1;
    DynamicArray<DynamicArray<T>> b(buckets);
    for (std::size_t i = 0; i < buckets; ++i) b.pushBack(DynamicArray<T>());
    for (const T& x : a) {
        double r = (key(x) - lo) / (hi - lo);
        long idx = (long)(r * (double)buckets);
        if (idx < 0) idx = 0;
        if (idx >= (long)buckets) idx = (long)buckets - 1;
        b[(std::size_t)idx].pushBack(x); op(c);
    }
    std::size_t w = 0;
    auto lessByKey = [&](const T& x, const T& y) { return key(x) < key(y); };
    for (std::size_t i = 0; i < buckets; ++i) {
        insertionSort(b[i], lessByKey, c, nullptr);
        for (const T& x : b[i]) { a[w] = x; detail::tr(t, "write", (long)w, -1); ++w; }
    }
}

// ---------------------------------------------------------------- dispatch
enum class SortAlgorithm { Merge, Quick, Heap, Insertion };

inline SortAlgorithm parseSortAlgorithm(const std::string& s) {
    if (s == "quick") return SortAlgorithm::Quick;
    if (s == "heap") return SortAlgorithm::Heap;
    if (s == "insertion") return SortAlgorithm::Insertion;
    return SortAlgorithm::Merge;
}

template <typename T, typename Less>
void sortWith(SortAlgorithm alg, DynamicArray<T>& a, Less less, OpCounter* c = nullptr, const Trace* t = nullptr) {
    switch (alg) {
        case SortAlgorithm::Quick:     quickSort(a, less, c, t); break;
        case SortAlgorithm::Heap:      heapSort(a, less, c, t); break;
        case SortAlgorithm::Insertion: insertionSort(a, less, c, t); break;
        default:                       mergeSort(a, less, c, t); break;
    }
}

} // namespace nexa::algo
