#pragma once
// ============================================================================
// NEXA :: ThreadedBinaryTree<K> — double-threaded binary search tree.
// Null left pointers are replaced by threads to the in-order predecessor
// and null right pointers by threads to the in-order successor, so in-order
// traversal needs NO stack and NO recursion.
// Application (DSA Lab): space-efficient ordered traversal demonstration;
// conceptually, an audit trail walked forward/backward without extra memory.
//
//  insert / search ...... O(h)
//  inorder traversal .... O(n) time, O(1) extra space
//  successor ............ O(1) amortised via threads
// ============================================================================
#include <cstddef>
#include "DynamicArray.hpp"

namespace nexa::ds {

template <typename K>
class ThreadedBinaryTree {
public:
    struct Node {
        K key;
        Node* left = nullptr; Node* right = nullptr;
        bool leftThread = true, rightThread = true;   // true => pointer is a thread (or null)
    };

    ThreadedBinaryTree() = default;
    ThreadedBinaryTree(const ThreadedBinaryTree&) = delete;
    ThreadedBinaryTree& operator=(const ThreadedBinaryTree&) = delete;
    ~ThreadedBinaryTree() {
        // free nodes using the threaded walk itself
        Node* n = leftmost(root_);
        while (n) { Node* next = successor(n); delete n; n = next; }
    }

    bool insert(const K& key) {
        Node* parent = nullptr;
        Node* cur = root_;
        while (cur) {
            if (key == cur->key) return false;
            parent = cur;
            if (key < cur->key) { if (cur->leftThread) break; cur = cur->left; }
            else { if (cur->rightThread) break; cur = cur->right; }
        }
        Node* n = new Node{key};
        if (!parent) { root_ = n; }
        else if (key < parent->key) {
            n->left = parent->left;      // inherit predecessor thread
            n->right = parent;           // successor is parent
            parent->leftThread = false;
            parent->left = n;
        } else {
            n->right = parent->right;    // inherit successor thread
            n->left = parent;            // predecessor is parent
            parent->rightThread = false;
            parent->right = n;
        }
        ++size_;
        return true;
    }

    bool contains(const K& key) const {
        Node* cur = root_;
        while (cur) {
            if (key == cur->key) return true;
            if (key < cur->key) { if (cur->leftThread) return false; cur = cur->left; }
            else { if (cur->rightThread) return false; cur = cur->right; }
        }
        return false;
    }

    /// Stackless in-order traversal.
    template <typename Fn> void inorder(Fn fn) const {
        for (Node* n = leftmost(root_); n; n = successor(n)) fn(n->key);
    }
    /// Reverse in-order using predecessor threads.
    template <typename Fn> void reverseInorder(Fn fn) const {
        Node* n = root_;
        while (n && !n->rightThread) n = n->right;
        for (; n; n = predecessor(n)) fn(n->key);
    }

    std::size_t size() const { return size_; }
    const Node* root() const { return root_; }

private:
    Node* root_ = nullptr;
    std::size_t size_ = 0;

    static Node* leftmost(Node* n) { while (n && !n->leftThread) n = n->left; return n; }
    static Node* successor(Node* n) { if (n->rightThread) return n->right; return leftmost(n->right); }
    static Node* predecessor(Node* n) {
        if (n->leftThread) return n->left;
        Node* x = n->left;
        while (!x->rightThread) x = x->right;
        return x;
    }
};

} // namespace nexa::ds
