#pragma once
// ============================================================================
// NEXA :: AVLTree<K,V> — height-balanced BST
// Application: primary balanced index for incidents, resources and hospitals
// (id -> record position) and for ordered queries (e.g. by severity score).
// Invariant: |height(left) - height(right)| <= 1 for every node.
//   insert / search / remove ... O(log n) worst case
//   rotations per insert ....... <= 2 (one single or one double)
// ============================================================================
#include <cstddef>
#include "../algorithms/OpCounter.hpp"

namespace nexa::ds {

template <typename K, typename V>
class AVLTree {
public:
    struct Node { K key; V value; int height = 1; Node* left = nullptr; Node* right = nullptr; };

    AVLTree() = default;
    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;
    ~AVLTree() { destroy(root_); }

    bool insert(const K& key, const V& value, algo::OpCounter* c = nullptr) {
        bool added = false;
        root_ = insertRec(root_, key, value, added, c);
        if (added) ++size_;
        return added;
    }

    V* search(const K& key, algo::OpCounter* c = nullptr) const {
        Node* n = root_;
        while (n) {
            algo::cmp(c);
            if (key < n->key) n = n->left;
            else if (n->key < key) n = n->right;
            else return &n->value;
        }
        return nullptr;
    }
    bool contains(const K& key) const { return search(key) != nullptr; }

    bool remove(const K& key) {
        bool removed = false;
        root_ = removeRec(root_, key, removed);
        if (removed) --size_;
        return removed;
    }

    template <typename Fn> void inorder(Fn fn) const   { inRec(root_, fn); }
    template <typename Fn> void preorder(Fn fn) const  { preRec(root_, fn); }
    template <typename Fn> void postorder(Fn fn) const { postRec(root_, fn); }
    template <typename Fn> void range(const K& lo, const K& hi, Fn fn) const { rangeRec(root_, lo, hi, fn); }

    int height() const { return h(root_); }
    std::size_t size() const { return size_; }
    std::size_t rotations() const { return rotations_; }
    const Node* root() const { return root_; }
    static int balanceFactor(const Node* n) { return n ? h(n->left) - h(n->right) : 0; }
    void clear() { destroy(root_); root_ = nullptr; size_ = 0; rotations_ = 0; }

private:
    Node* root_ = nullptr;
    std::size_t size_ = 0;
    std::size_t rotations_ = 0;

    static int h(const Node* n) { return n ? n->height : 0; }
    static void update(Node* n) { int a = h(n->left), b = h(n->right); n->height = 1 + (a > b ? a : b); }

    Node* rotateRight(Node* y) {          // LL case
        Node* x = y->left;
        y->left = x->right;
        x->right = y;
        update(y); update(x);
        ++rotations_;
        return x;
    }
    Node* rotateLeft(Node* x) {           // RR case
        Node* y = x->right;
        x->right = y->left;
        y->left = x;
        update(x); update(y);
        ++rotations_;
        return y;
    }
    Node* rotateLeftRight(Node* n) { n->left = rotateLeft(n->left); return rotateRight(n); }   // LR
    Node* rotateRightLeft(Node* n) { n->right = rotateRight(n->right); return rotateLeft(n); } // RL

    Node* rebalance(Node* n) {
        update(n);
        int bf = balanceFactor(n);
        if (bf > 1)  return balanceFactor(n->left) >= 0 ? rotateRight(n) : rotateLeftRight(n);
        if (bf < -1) return balanceFactor(n->right) <= 0 ? rotateLeft(n) : rotateRightLeft(n);
        return n;
    }

    Node* insertRec(Node* n, const K& key, const V& value, bool& added, algo::OpCounter* c) {
        if (!n) { added = true; return new Node{key, value}; }
        algo::cmp(c);
        if (key < n->key) n->left = insertRec(n->left, key, value, added, c);
        else if (n->key < key) n->right = insertRec(n->right, key, value, added, c);
        else { n->value = value; return n; }
        return rebalance(n);
    }

    Node* removeRec(Node* n, const K& key, bool& removed) {
        if (!n) return nullptr;
        if (key < n->key) n->left = removeRec(n->left, key, removed);
        else if (n->key < key) n->right = removeRec(n->right, key, removed);
        else {
            removed = true;
            if (!n->left || !n->right) {
                Node* child = n->left ? n->left : n->right;
                delete n;
                return child;
            }
            Node* s = n->right;
            while (s->left) s = s->left;
            n->key = s->key; n->value = s->value;
            bool dummy = false;
            n->right = removeRec(n->right, s->key, dummy);
        }
        return rebalance(n);
    }

    template <typename Fn> static void inRec(Node* n, Fn& fn)   { if (!n) return; inRec(n->left, fn); fn(n->key, n->value); inRec(n->right, fn); }
    template <typename Fn> static void preRec(Node* n, Fn& fn)  { if (!n) return; fn(n->key, n->value); preRec(n->left, fn); preRec(n->right, fn); }
    template <typename Fn> static void postRec(Node* n, Fn& fn) { if (!n) return; postRec(n->left, fn); postRec(n->right, fn); fn(n->key, n->value); }
    template <typename Fn> static void rangeRec(Node* n, const K& lo, const K& hi, Fn& fn) {
        if (!n) return;
        if (lo < n->key) rangeRec(n->left, lo, hi, fn);
        if (!(n->key < lo) && !(hi < n->key)) fn(n->key, n->value);
        if (n->key < hi) rangeRec(n->right, lo, hi, fn);
    }
    static void destroy(Node* n) { if (!n) return; destroy(n->left); destroy(n->right); delete n; }
};

} // namespace nexa::ds
