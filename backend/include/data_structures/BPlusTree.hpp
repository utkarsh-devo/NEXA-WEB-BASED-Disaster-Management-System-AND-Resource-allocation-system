#pragma once
// ============================================================================
// NEXA :: BPlusTree<K,V> — all values live in leaves; leaves are chained
// left-to-right so range scans are sequential.
// Order `maxKeys`: a node splits when it exceeds maxKeys keys.
//   leaf split    -> separator is COPIED up (stays in right leaf)
//   internal split -> separator is MOVED up
// Application (DSA Lab): models MySQL/InnoDB clustered & secondary indexes;
// range scan demo "all incidents registered between t1 and t2".
//
//  search ....... O(log n)       insert ...... O(log n)
//  range scan ... O(log n + m)
//  remove ....... O(log n) — LAZY: the key is removed from its leaf without
//                 rebalancing/merging. Searches stay correct; occupancy may
//                 drop below half. (Documented simplification.)
// ============================================================================
#include <cstddef>
#include "DynamicArray.hpp"

namespace nexa::ds {

template <typename K, typename V>
class BPlusTree {
public:
    struct Node {
        bool leaf = true;
        DynamicArray<K> keys;
        DynamicArray<V> values;        // leaves only
        DynamicArray<Node*> kids;      // internal only
        Node* next = nullptr;          // leaf chain
    };

    explicit BPlusTree(std::size_t maxKeys = 4) : maxKeys_(maxKeys < 3 ? 3 : maxKeys) { root_ = new Node; }
    BPlusTree(const BPlusTree&) = delete;
    BPlusTree& operator=(const BPlusTree&) = delete;
    ~BPlusTree() { destroy(root_); }

    V* search(const K& k) const {
        Node* leaf = findLeaf(k);
        for (std::size_t i = 0; i < leaf->keys.size(); ++i) if (!(leaf->keys[i] < k) && !(k < leaf->keys[i])) return &leaf->values[i];
        return nullptr;
    }

    void insert(const K& k, const V& v) {
        K upKey{}; Node* newChild = nullptr;
        insertRec(root_, k, v, upKey, newChild);
        if (newChild) {
            Node* r = new Node; r->leaf = false;
            r->keys.pushBack(upKey);
            r->kids.pushBack(root_); r->kids.pushBack(newChild);
            root_ = r;
        }
    }

    bool remove(const K& k) {
        Node* leaf = findLeaf(k);
        for (std::size_t i = 0; i < leaf->keys.size(); ++i)
            if (!(leaf->keys[i] < k) && !(k < leaf->keys[i])) { leaf->keys.removeAt(i); leaf->values.removeAt(i); --size_; return true; }
        return false;
    }

    /// fn(key, value) for lo <= key <= hi, ascending, via the leaf chain.
    template <typename Fn> void range(const K& lo, const K& hi, Fn fn) const {
        for (Node* n = findLeaf(lo); n; n = n->next)
            for (std::size_t i = 0; i < n->keys.size(); ++i) {
                if (n->keys[i] < lo) continue;
                if (hi < n->keys[i]) return;
                fn(n->keys[i], n->values[i]);
            }
    }

    std::size_t size() const { return size_; }
    int height() const { int h = 1; for (Node* n = root_; !n->leaf; n = n->kids[0]) ++h; return h; }
    const Node* root() const { return root_; }

private:
    Node* root_;
    std::size_t maxKeys_;
    std::size_t size_ = 0;

    Node* findLeaf(const K& k) const {
        Node* n = root_;
        while (!n->leaf) {
            std::size_t i = 0;
            while (i < n->keys.size() && !(k < n->keys[i])) ++i;   // keys[i] <= k -> go right
            n = n->kids[i];
        }
        return n;
    }

    void insertRec(Node* n, const K& k, const V& v, K& upKey, Node*& newChild) {
        newChild = nullptr;
        if (n->leaf) {
            std::size_t i = 0;
            while (i < n->keys.size() && n->keys[i] < k) ++i;
            if (i < n->keys.size() && !(k < n->keys[i])) { n->values[i] = v; return; }   // overwrite
            n->keys.insertAt(i, k); n->values.insertAt(i, v); ++size_;
            if (n->keys.size() <= maxKeys_) return;
            // split leaf
            Node* r = new Node;
            std::size_t mid = n->keys.size() / 2;
            for (std::size_t j = mid; j < n->keys.size(); ++j) { r->keys.pushBack(n->keys[j]); r->values.pushBack(n->values[j]); }
            while (n->keys.size() > mid) { n->keys.popBack(); n->values.popBack(); }
            r->next = n->next; n->next = r;
            upKey = r->keys[0];                       // copy up
            newChild = r;
            return;
        }
        std::size_t i = 0;
        while (i < n->keys.size() && !(k < n->keys[i])) ++i;
        K childUp{}; Node* childNew = nullptr;
        insertRec(n->kids[i], k, v, childUp, childNew);
        if (!childNew) return;
        n->keys.insertAt(i, childUp);
        n->kids.insertAt(i + 1, childNew);
        if (n->keys.size() <= maxKeys_) return;
        // split internal
        Node* r = new Node; r->leaf = false;
        std::size_t mid = n->keys.size() / 2;
        upKey = n->keys[mid];                         // move up
        for (std::size_t j = mid + 1; j < n->keys.size(); ++j) r->keys.pushBack(n->keys[j]);
        for (std::size_t j = mid + 1; j < n->kids.size(); ++j) r->kids.pushBack(n->kids[j]);
        while (n->keys.size() > mid) n->keys.popBack();
        while (n->kids.size() > mid + 1) n->kids.popBack();
        newChild = r;
    }

    static void destroy(Node* n) { if (!n) return; if (!n->leaf) for (Node* c : n->kids) destroy(c); delete n; }
};

} // namespace nexa::ds
