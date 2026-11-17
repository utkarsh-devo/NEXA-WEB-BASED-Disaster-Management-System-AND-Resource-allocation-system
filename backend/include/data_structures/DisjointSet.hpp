#pragma once
// ============================================================================
// NEXA :: DisjointSet (Union-Find) with path compression + union by rank.
// Used by Kruskal's MST and for counting disconnected regions.
//   find / unite ... O(alpha(n)) amortised (practically constant)
// ============================================================================
#include <cstddef>
#include "DynamicArray.hpp"

namespace nexa::ds {

class DisjointSet {
public:
    explicit DisjointSet(int n = 0) { reset(n); }

    void reset(int n) {
        parent_.clear(); rank_.clear();
        for (int i = 0; i < n; ++i) { parent_.pushBack(i); rank_.pushBack(0); }
        sets_ = n;
    }
    int find(int x) {
        int root = x;
        while (parent_[root] != root) root = parent_[root];
        while (parent_[x] != root) { int next = parent_[x]; parent_[x] = root; x = next; }  // path compression
        return root;
    }
    bool unite(int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra == rb) return false;
        if (rank_[ra] < rank_[rb]) { int t = ra; ra = rb; rb = t; }
        parent_[rb] = ra;
        if (rank_[ra] == rank_[rb]) ++rank_[ra];
        --sets_;
        return true;
    }
    int setCount() const { return sets_; }

private:
    DynamicArray<int> parent_, rank_;
    int sets_ = 0;
};

} // namespace nexa::ds
