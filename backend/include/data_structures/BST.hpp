#pragma once
// ============================================================================
// NEXA :: BST<K,V>  — unbalanced Binary Search Tree
// Application: ordered incident index (by registration time / id) and the
// baseline for the BST-vs-AVL benchmark.
//   insert / search / remove ... O(h)  — O(log n) average, O(n) worst
//                                (sorted inserts degrade it to a list)
//   traversals ................. O(n)
// ============================================================================
#include <cstddef>
#include "../algorithms/OpCounter.hpp"

namespace nexa::ds {

template <typename K, typename V>
class BST {
public:
    struct Node { K key; V value; Node* left = nullptr; Node* right = nullptr; };

    BST() = default;
    BST(const BST&) = delete;
    BST& operator=(const BST&) = delete;
    ~BST() { destroy(root_); }

    /// Inserts or overwrites. Returns true if a new key was added.
    bool insert(const K& key, const V& value, algo::OpCounter* c = nullptr) {
        Node** cur = &root_;
        while (*cur) {
            algo::cmp(c);
            if (key < (*cur)->key) cur = &(*cur)->left;
            else if ((*cur)->key < key) cur = &(*cur)->right;
            else { (*cur)->value = value; return false; }
        }
        *cur = new Node{key, value};
        ++size_;
        return true;
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

    bool remove(const K& key) {
        bool removed = false;
        root_ = removeRec(root_, key, removed);
        if (removed) --size_;
        return removed;
    }

    template <typename Fn> void inorder(Fn fn) const   { inorderRec(root_, fn); }
    template <typename Fn> void preorder(Fn fn) const  { preRec(root_, fn); }
    template <typename Fn> void postorder(Fn fn) const { postRec(root_, fn); }

    /// Ordered range retrieval [lo, hi] — prunes subtrees outside the range.
    template <typename Fn> void range(const K& lo, const K& hi, Fn fn) const { rangeRec(root_, lo, hi, fn); }

    int height() const { return heightRec(root_); }
    std::size_t size() const { return size_; }
    const Node* root() const { return root_; }
    void clear() { destroy(root_); root_ = nullptr; size_ = 0; }

private:
    Node* root_ = nullptr;
    std::size_t size_ = 0;

    static Node* removeRec(Node* n, const K& key, bool& removed) {
        if (!n) return nullptr;
        if (key < n->key) n->left = removeRec(n->left, key, removed);
        else if (n->key < key) n->right = removeRec(n->right, key, removed);
        else {
            removed = true;
            if (!n->left) { Node* r = n->right; delete n; return r; }
            if (!n->right) { Node* l = n->left; delete n; return l; }
            // two children: copy inorder successor, then delete it from right subtree
            Node* s = n->right;
            while (s->left) s = s->left;
            n->key = s->key; n->value = s->value;
            bool dummy = false;
            n->right = removeRec(n->right, s->key, dummy);
        }
        return n;
    }
    template <typename Fn> static void inorderRec(Node* n, Fn& fn) { if (!n) return; inorderRec(n->left, fn); fn(n->key, n->value); inorderRec(n->right, fn); }
    template <typename Fn> static void preRec(Node* n, Fn& fn)     { if (!n) return; fn(n->key, n->value); preRec(n->left, fn); preRec(n->right, fn); }
    template <typename Fn> static void postRec(Node* n, Fn& fn)    { if (!n) return; postRec(n->left, fn); postRec(n->right, fn); fn(n->key, n->value); }
    template <typename Fn> static void rangeRec(Node* n, const K& lo, const K& hi, Fn& fn) {
        if (!n) return;
        if (lo < n->key) rangeRec(n->left, lo, hi, fn);
        if (!(n->key < lo) && !(hi < n->key)) fn(n->key, n->value);
        if (n->key < hi) rangeRec(n->right, lo, hi, fn);
    }
    static int heightRec(Node* n) { if (!n) return 0; int a = heightRec(n->left), b = heightRec(n->right); return 1 + (a > b ? a : b); }
    static void destroy(Node* n) { if (!n) return; destroy(n->left); destroy(n->right); delete n; }
};

} // namespace nexa::ds
