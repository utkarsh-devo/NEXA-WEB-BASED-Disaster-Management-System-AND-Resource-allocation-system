#pragma once
// ============================================================================
// NEXA :: IntervalTree — AVL-balanced BST keyed by interval start, each node
// augmented with maxEnd of its subtree (CLRS §14.3).
// Application: time windows when a resource is committed / a hospital bed is
// reserved. "Is resource R free during [t1,t2]?" = no overlapping interval.
//
//  insert / remove ........ O(log n)
//  anyOverlap ............. O(log n)
//  allOverlaps ............ O(min(n, k log n))  (k = reported)
//  pointQuery(t) .......... same as allOverlaps with [t,t]
// Intervals are closed: [start, end].
// ============================================================================
#include <cstddef>
#include <climits>
#include <stdexcept>
#include "DynamicArray.hpp"

namespace nexa::ds {

struct Interval { long start = 0; long end = 0; long id = 0; };

class IntervalTree {
    struct Node { Interval iv; long maxEnd; int height = 1; Node* left = nullptr; Node* right = nullptr; };
public:
    IntervalTree() = default;
    IntervalTree(const IntervalTree&) = delete;
    IntervalTree& operator=(const IntervalTree&) = delete;
    ~IntervalTree() { destroy(root_); }

    void insert(const Interval& iv) {
        if (iv.end < iv.start) throw std::invalid_argument("IntervalTree: end < start");
        root_ = ins(root_, iv); ++size_;
    }
    bool remove(long id, long start) {
        bool removed = false;
        root_ = rem(root_, id, start, removed);
        if (removed) --size_;
        return removed;
    }
    bool anyOverlap(long s, long e) const {
        Node* n = root_;
        while (n) {
            if (n->iv.start <= e && s <= n->iv.end) return true;
            if (n->left && n->left->maxEnd >= s) n = n->left; else n = n->right;
        }
        return false;
    }
    DynamicArray<Interval> allOverlaps(long s, long e) const { DynamicArray<Interval> out; collect(root_, s, e, out); return out; }
    DynamicArray<Interval> pointQuery(long t) const { return allOverlaps(t, t); }
    std::size_t size() const { return size_; }
    template <typename Fn> void inorder(Fn fn) const { inRec(root_, fn); }

private:
    Node* root_ = nullptr;
    std::size_t size_ = 0;

    static int h(Node* n) { return n ? n->height : 0; }
    static long mx(Node* n) { return n ? n->maxEnd : LONG_MIN; }
    static void upd(Node* n) {
        n->height = 1 + (h(n->left) > h(n->right) ? h(n->left) : h(n->right));
        long m = n->iv.end;
        if (mx(n->left) > m) m = mx(n->left);
        if (mx(n->right) > m) m = mx(n->right);
        n->maxEnd = m;
    }
    static Node* rotR(Node* y) { Node* x = y->left; y->left = x->right; x->right = y; upd(y); upd(x); return x; }
    static Node* rotL(Node* x) { Node* y = x->right; x->right = y->left; y->left = x; upd(x); upd(y); return y; }
    static Node* bal(Node* n) {
        upd(n);
        int bf = h(n->left) - h(n->right);
        if (bf > 1)  { if (h(n->left->left) < h(n->left->right)) n->left = rotL(n->left); return rotR(n); }
        if (bf < -1) { if (h(n->right->right) < h(n->right->left)) n->right = rotR(n->right); return rotL(n); }
        return n;
    }
    static bool lessKey(const Interval& a, long id, long start) { return a.start < start || (a.start == start && a.id < id); }

    static Node* ins(Node* n, const Interval& iv) {
        if (!n) { Node* x = new Node{iv, iv.end}; return x; }
        if (iv.start < n->iv.start || (iv.start == n->iv.start && iv.id < n->iv.id)) n->left = ins(n->left, iv);
        else n->right = ins(n->right, iv);
        return bal(n);
    }
    static Node* rem(Node* n, long id, long start, bool& removed) {
        if (!n) return nullptr;
        if (n->iv.id == id && n->iv.start == start) {
            removed = true;
            if (!n->left || !n->right) { Node* c = n->left ? n->left : n->right; delete n; return c; }
            Node* s = n->right; while (s->left) s = s->left;
            n->iv = s->iv;
            bool d = false;
            n->right = rem(n->right, s->iv.id, s->iv.start, d);
        } else if (lessKey(n->iv, id, start)) n->right = rem(n->right, id, start, removed);
        else n->left = rem(n->left, id, start, removed);
        return bal(n);
    }
    static void collect(Node* n, long s, long e, DynamicArray<Interval>& out) {
        if (!n || n->maxEnd < s) return;          // nothing in this subtree ends after s
        collect(n->left, s, e, out);
        if (n->iv.start <= e && s <= n->iv.end) out.pushBack(n->iv);
        if (n->iv.start <= e) collect(n->right, s, e, out);   // right subtree starts later
    }
    template <typename Fn> static void inRec(Node* n, Fn& fn) { if (!n) return; inRec(n->left, fn); fn(n->iv); inRec(n->right, fn); }
    static void destroy(Node* n) { if (!n) return; destroy(n->left); destroy(n->right); delete n; }
};

} // namespace nexa::ds
