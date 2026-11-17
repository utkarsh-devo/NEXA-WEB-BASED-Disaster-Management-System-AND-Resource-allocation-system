#pragma once
// ============================================================================
// NEXA :: BTree<K> — B-Tree of minimum degree t (CLRS ch.18)
// Every node except the root holds between t-1 and 2t-1 keys; all leaves
// are at the same depth.
// Application (DSA Lab): disk-oriented indexing — the same structure MySQL
// InnoDB uses (as a B+Tree) for the indexes on our tables. Shown to explain
// why `CREATE INDEX` makes lookups O(log_t n) page reads.
//
//  search / insert / remove ... O(t log_t n) CPU, O(log_t n) node visits
// ============================================================================
#include <cstddef>
#include "DynamicArray.hpp"

namespace nexa::ds {

template <typename K>
class BTree {
public:
    struct Node {
        DynamicArray<K> keys;
        DynamicArray<Node*> kids;
        bool leaf = true;
    };

    explicit BTree(int minDegree = 3) : t_(minDegree < 2 ? 2 : minDegree) {}
    BTree(const BTree&) = delete;
    BTree& operator=(const BTree&) = delete;
    ~BTree() { destroy(root_); }

    bool search(const K& k, std::size_t* nodesVisited = nullptr) const {
        std::size_t v = 0;
        Node* n = root_;
        while (n) {
            ++v;
            std::size_t i = 0;
            while (i < n->keys.size() && n->keys[i] < k) ++i;
            if (i < n->keys.size() && !(k < n->keys[i])) { if (nodesVisited) *nodesVisited = v; return true; }
            if (n->leaf) break;
            n = n->kids[i];
        }
        if (nodesVisited) *nodesVisited = v;
        return false;
    }

    void insert(const K& k) {
        if (search(k)) return;                       // set semantics
        if (!root_) { root_ = new Node; root_->keys.pushBack(k); ++size_; return; }
        if ((int)root_->keys.size() == 2 * t_ - 1) {   // grow in height only at the root
            Node* s = new Node; s->leaf = false; s->kids.pushBack(root_);
            splitChild(s, 0);
            root_ = s;
        }
        insertNonFull(root_, k);
        ++size_;
    }

    bool remove(const K& k) {
        if (!root_ || !search(k)) return false;
        removeRec(root_, k);
        if (root_->keys.empty()) {                   // shrink height
            Node* old = root_;
            root_ = root_->leaf ? nullptr : root_->kids[0];
            old->kids.clear();
            delete old;
        }
        --size_;
        return true;
    }

    template <typename Fn> void inorder(Fn fn) const { inRec(root_, fn); }
    int height() const { int h = 0; for (Node* n = root_; n; n = n->leaf ? nullptr : n->kids[0]) ++h; return h; }
    std::size_t size() const { return size_; }
    const Node* root() const { return root_; }
    int minDegree() const { return t_; }

private:
    Node* root_ = nullptr;
    int t_;
    std::size_t size_ = 0;

    void splitChild(Node* parent, std::size_t i) {
        Node* y = parent->kids[i];
        Node* z = new Node; z->leaf = y->leaf;
        K median = y->keys[t_ - 1];
        for (int j = t_; j < 2 * t_ - 1; ++j) z->keys.pushBack(y->keys[j]);
        if (!y->leaf) for (int j = t_; j < 2 * t_; ++j) z->kids.pushBack(y->kids[j]);
        while ((int)y->keys.size() > t_ - 1) y->keys.popBack();
        if (!y->leaf) while ((int)y->kids.size() > t_) y->kids.popBack();
        parent->kids.insertAt(i + 1, z);
        parent->keys.insertAt(i, median);
    }

    void insertNonFull(Node* n, const K& k) {
        while (true) {
            std::size_t i = 0;
            while (i < n->keys.size() && n->keys[i] < k) ++i;
            if (n->leaf) { n->keys.insertAt(i, k); return; }
            if ((int)n->kids[i]->keys.size() == 2 * t_ - 1) {
                splitChild(n, i);
                if (n->keys[i] < k) ++i;
            }
            n = n->kids[i];
        }
    }

    // CLRS deletion: guarantees the child we descend into has >= t keys
    void removeRec(Node* n, const K& k) {
        std::size_t i = 0;
        while (i < n->keys.size() && n->keys[i] < k) ++i;
        bool here = i < n->keys.size() && !(k < n->keys[i]);
        if (here) {
            if (n->leaf) { n->keys.removeAt(i); return; }
            Node* pre = n->kids[i]; Node* suc = n->kids[i + 1];
            if ((int)pre->keys.size() >= t_) {                     // case 2a
                Node* p = pre; while (!p->leaf) p = p->kids[p->kids.size() - 1];
                K pk = p->keys[p->keys.size() - 1];
                n->keys[i] = pk; removeRec(pre, pk);
            } else if ((int)suc->keys.size() >= t_) {              // case 2b
                Node* s = suc; while (!s->leaf) s = s->kids[0];
                K sk = s->keys[0];
                n->keys[i] = sk; removeRec(suc, sk);
            } else {                                               // case 2c
                merge(n, i);
                removeRec(pre, k);
            }
            return;
        }
        if (n->leaf) return;                                       // not present
        bool last = (i == n->keys.size());
        if ((int)n->kids[i]->keys.size() < t_) fill(n, i);         // case 3
        if (last && i > n->keys.size()) removeRec(n->kids[i - 1], k);
        else removeRec(n->kids[i], k);
    }

    void fill(Node* n, std::size_t i) {
        if (i > 0 && (int)n->kids[i - 1]->keys.size() >= t_) borrowPrev(n, i);
        else if (i < n->keys.size() && (int)n->kids[i + 1]->keys.size() >= t_) borrowNext(n, i);
        else if (i < n->keys.size()) merge(n, i);
        else merge(n, i - 1);
    }
    void borrowPrev(Node* n, std::size_t i) {
        Node* c = n->kids[i]; Node* s = n->kids[i - 1];
        c->keys.insertAt(0, n->keys[i - 1]);
        if (!c->leaf) { c->kids.insertAt(0, s->kids[s->kids.size() - 1]); s->kids.popBack(); }
        n->keys[i - 1] = s->keys[s->keys.size() - 1];
        s->keys.popBack();
    }
    void borrowNext(Node* n, std::size_t i) {
        Node* c = n->kids[i]; Node* s = n->kids[i + 1];
        c->keys.pushBack(n->keys[i]);
        if (!c->leaf) { c->kids.pushBack(s->kids[0]); s->kids.removeAt(0); }
        n->keys[i] = s->keys[0];
        s->keys.removeAt(0);
    }
    void merge(Node* n, std::size_t i) {
        Node* c = n->kids[i]; Node* s = n->kids[i + 1];
        c->keys.pushBack(n->keys[i]);
        for (const K& k : s->keys) c->keys.pushBack(k);
        if (!c->leaf) for (Node* x : s->kids) c->kids.pushBack(x);
        n->keys.removeAt(i);
        n->kids.removeAt(i + 1);
        s->kids.clear();
        delete s;
    }

    template <typename Fn> static void inRec(Node* n, Fn& fn) {
        if (!n) return;
        for (std::size_t i = 0; i < n->keys.size(); ++i) {
            if (!n->leaf) inRec(n->kids[i], fn);
            fn(n->keys[i]);
        }
        if (!n->leaf) inRec(n->kids[n->kids.size() - 1], fn);
    }
    static void destroy(Node* n) { if (!n) return; for (Node* c : n->kids) destroy(c); delete n; }
};

} // namespace nexa::ds
