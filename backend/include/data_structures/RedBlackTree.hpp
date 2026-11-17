#pragma once
// ============================================================================
// NEXA :: RedBlackTree<K,V> (CLRS ch.13, with a shared black sentinel NIL)
// Properties: root black; red nodes have black children; every root-to-leaf
// path has the same number of black nodes => height <= 2 log2(n+1).
// Application (DSA Lab): alternative balanced index compared against AVL —
// fewer rotations on insert/delete, slightly taller trees.
//
//  insert / remove / search ... O(log n) worst case
//  rotations: insert <= 2, delete <= 3
// ============================================================================
#include <cstddef>

namespace nexa::ds {

template <typename K, typename V>
class RedBlackTree {
public:
    enum Color : unsigned char { RED, BLACK };
    struct Node { K key{}; V value{}; Color color = BLACK; Node* left; Node* right; Node* parent; };

    RedBlackTree() {
        nil_ = new Node; nil_->color = BLACK;
        nil_->left = nil_->right = nil_->parent = nil_;
        root_ = nil_;
    }
    RedBlackTree(const RedBlackTree&) = delete;
    RedBlackTree& operator=(const RedBlackTree&) = delete;
    ~RedBlackTree() { destroy(root_); delete nil_; }

    bool insert(const K& k, const V& v) {
        Node* y = nil_; Node* x = root_;
        while (x != nil_) {
            y = x;
            if (k < x->key) x = x->left;
            else if (x->key < k) x = x->right;
            else { x->value = v; return false; }
        }
        Node* z = new Node{k, v, RED, nil_, nil_, y};
        if (y == nil_) root_ = z;
        else if (k < y->key) y->left = z; else y->right = z;
        insertFixup(z);
        ++size_;
        return true;
    }

    V* search(const K& k) const { Node* n = find(k); return n == nil_ ? nullptr : &n->value; }

    bool remove(const K& k) {
        Node* z = find(k);
        if (z == nil_) return false;
        Node* y = z; Node* x;
        Color yOrig = y->color;
        if (z->left == nil_) { x = z->right; transplant(z, z->right); }
        else if (z->right == nil_) { x = z->left; transplant(z, z->left); }
        else {
            y = minimum(z->right); yOrig = y->color; x = y->right;
            if (y->parent == z) x->parent = y;
            else { transplant(y, y->right); y->right = z->right; y->right->parent = y; }
            transplant(z, y);
            y->left = z->left; y->left->parent = y; y->color = z->color;
        }
        delete z;
        if (yOrig == BLACK) deleteFixup(x);
        --size_;
        return true;
    }

    template <typename Fn> void inorder(Fn fn) const { inRec(root_, fn); }
    std::size_t size() const { return size_; }
    std::size_t rotations() const { return rotations_; }
    int height() const { return heightRec(root_); }
    const Node* root() const { return root_; }
    const Node* nil() const { return nil_; }

    /// Verifies all RB properties; returns black-height or -1 if violated.
    int validate() const { if (root_->color != BLACK) return -1; return blackHeight(root_); }

private:
    Node* nil_;
    Node* root_;
    std::size_t size_ = 0, rotations_ = 0;

    Node* find(const K& k) const {
        Node* x = root_;
        while (x != nil_) { if (k < x->key) x = x->left; else if (x->key < k) x = x->right; else return x; }
        return nil_;
    }
    Node* minimum(Node* x) const { while (x->left != nil_) x = x->left; return x; }

    void rotateLeft(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left != nil_) y->left->parent = x;
        y->parent = x->parent;
        if (x->parent == nil_) root_ = y;
        else if (x == x->parent->left) x->parent->left = y; else x->parent->right = y;
        y->left = x; x->parent = y;
        ++rotations_;
    }
    void rotateRight(Node* x) {
        Node* y = x->left;
        x->left = y->right;
        if (y->right != nil_) y->right->parent = x;
        y->parent = x->parent;
        if (x->parent == nil_) root_ = y;
        else if (x == x->parent->right) x->parent->right = y; else x->parent->left = y;
        y->right = x; x->parent = y;
        ++rotations_;
    }
    void insertFixup(Node* z) {
        while (z->parent->color == RED) {
            Node* gp = z->parent->parent;
            if (z->parent == gp->left) {
                Node* u = gp->right;
                if (u->color == RED) { z->parent->color = BLACK; u->color = BLACK; gp->color = RED; z = gp; }
                else {
                    if (z == z->parent->right) { z = z->parent; rotateLeft(z); }
                    z->parent->color = BLACK; gp->color = RED; rotateRight(gp);
                }
            } else {
                Node* u = gp->left;
                if (u->color == RED) { z->parent->color = BLACK; u->color = BLACK; gp->color = RED; z = gp; }
                else {
                    if (z == z->parent->left) { z = z->parent; rotateRight(z); }
                    z->parent->color = BLACK; gp->color = RED; rotateLeft(gp);
                }
            }
        }
        root_->color = BLACK;
    }
    void transplant(Node* u, Node* v) {
        if (u->parent == nil_) root_ = v;
        else if (u == u->parent->left) u->parent->left = v; else u->parent->right = v;
        v->parent = u->parent;
    }
    void deleteFixup(Node* x) {
        while (x != root_ && x->color == BLACK) {
            if (x == x->parent->left) {
                Node* w = x->parent->right;
                if (w->color == RED) { w->color = BLACK; x->parent->color = RED; rotateLeft(x->parent); w = x->parent->right; }
                if (w->left->color == BLACK && w->right->color == BLACK) { w->color = RED; x = x->parent; }
                else {
                    if (w->right->color == BLACK) { w->left->color = BLACK; w->color = RED; rotateRight(w); w = x->parent->right; }
                    w->color = x->parent->color; x->parent->color = BLACK; w->right->color = BLACK;
                    rotateLeft(x->parent); x = root_;
                }
            } else {
                Node* w = x->parent->left;
                if (w->color == RED) { w->color = BLACK; x->parent->color = RED; rotateRight(x->parent); w = x->parent->left; }
                if (w->right->color == BLACK && w->left->color == BLACK) { w->color = RED; x = x->parent; }
                else {
                    if (w->left->color == BLACK) { w->right->color = BLACK; w->color = RED; rotateLeft(w); w = x->parent->left; }
                    w->color = x->parent->color; x->parent->color = BLACK; w->left->color = BLACK;
                    rotateRight(x->parent); x = root_;
                }
            }
        }
        x->color = BLACK;
    }
    template <typename Fn> void inRec(Node* n, Fn& fn) const { if (n == nil_) return; inRec(n->left, fn); fn(n->key, n->value); inRec(n->right, fn); }
    int heightRec(Node* n) const { if (n == nil_) return 0; int a = heightRec(n->left), b = heightRec(n->right); return 1 + (a > b ? a : b); }
    int blackHeight(Node* n) const {
        if (n == nil_) return 1;
        if (n->color == RED && (n->left->color == RED || n->right->color == RED)) return -1;
        int l = blackHeight(n->left), r = blackHeight(n->right);
        if (l < 0 || r < 0 || l != r) return -1;
        return l + (n->color == BLACK ? 1 : 0);
    }
    void destroy(Node* n) { if (n == nil_) return; destroy(n->left); destroy(n->right); delete n; }
};

} // namespace nexa::ds
