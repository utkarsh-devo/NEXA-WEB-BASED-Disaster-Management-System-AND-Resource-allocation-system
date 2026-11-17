#pragma once
// ============================================================================
// NEXA :: SparseMatrix<T>  (Coordinate / triplet representation, row-major
// sorted, so lookups use binary search).
// Application: incident x resource allocation matrix used by analytics
// (e.g. units committed, utilisation). Only non-zero cells are stored.
//
//   get(r,c) ............ O(log nnz)
//   set(r,c,v) .......... O(nnz)  (sorted insert / erase)
//   rowSum / colSum ..... O(nnz)
//   transpose ........... O(nnz log nnz)
//   space ............... O(nnz)  vs O(rows*cols) for a dense matrix
// ============================================================================
#include <cstddef>
#include "DynamicArray.hpp"

namespace nexa::ds {

template <typename T>
class SparseMatrix {
public:
    struct Entry { long row = 0; long col = 0; T value{}; };

    SparseMatrix(long rows = 0, long cols = 0) : rows_(rows), cols_(cols) {}

    long rows() const { return rows_; }
    long cols() const { return cols_; }
    std::size_t nonZeroCount() const { return data_.size(); }

    void set(long r, long c, const T& v) {
        if (r >= rows_) rows_ = r + 1;
        if (c >= cols_) cols_ = c + 1;
        long idx = lowerBound(r, c);
        bool exists = idx < (long)data_.size() && data_[idx].row == r && data_[idx].col == c;
        if (v == T{}) { if (exists) data_.removeAt(idx); return; }   // keep only non-zeros
        if (exists) data_[idx].value = v;
        else data_.insertAt(idx, Entry{r, c, v});
    }

    T get(long r, long c) const {
        long idx = lowerBound(r, c);
        if (idx < (long)data_.size() && data_[idx].row == r && data_[idx].col == c) return data_[idx].value;
        return T{};
    }

    void add(long r, long c, const T& delta) { set(r, c, get(r, c) + delta); }

    T rowSum(long r) const { T s{}; for (const auto& e : data_) if (e.row == r) s = s + e.value; return s; }
    T colSum(long c) const { T s{}; for (const auto& e : data_) if (e.col == c) s = s + e.value; return s; }

    SparseMatrix transpose() const {
        SparseMatrix t(cols_, rows_);
        for (const auto& e : data_) t.set(e.col, e.row, e.value);
        return t;
    }

    /// Density = nnz / (rows*cols); used to justify the sparse representation.
    double density() const {
        double cells = (double)rows_ * (double)cols_;
        return cells == 0 ? 0.0 : (double)data_.size() / cells;
    }

    const DynamicArray<Entry>& entries() const { return data_; }
    void clear() { data_.clear(); }

private:
    long rows_, cols_;
    DynamicArray<Entry> data_;

    // first index whose (row,col) >= (r,c) — binary search
    long lowerBound(long r, long c) const {
        long lo = 0, hi = (long)data_.size();
        while (lo < hi) {
            long mid = lo + (hi - lo) / 2;
            const Entry& e = data_[mid];
            bool less = e.row < r || (e.row == r && e.col < c);
            if (less) lo = mid + 1; else hi = mid;
        }
        return lo;
    }
};

} // namespace nexa::ds
