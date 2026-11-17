#pragma once
// ============================================================================
// NEXA :: KaryTree<T> — general rooted tree where each node has at most K
// children (K = 0 means unbounded). Nodes carry an integer id, a label and a
// payload.
// Application: the disaster-category taxonomy used to classify incidents
//   ROOT -> Natural -> {Earthquake, Flood, Cyclone, Landslide, Heatwave}
//        -> Technological -> {Industrial Fire, Gas Leak, Building Collapse ...}
//        -> Societal -> {Stampede, Epidemic ...}
// so classification can report the full path (e.g. Natural > Flood) and
// analytics can roll counts up to the parent category.
//
//  addChild ........ O(1) after O(n) parent lookup (O(1) with the id index)
//  find / traverse . O(n)       pathToRoot ... O(depth)
// ============================================================================
#include <cstddef>
#include <string>
#include <stdexcept>
#include "DynamicArray.hpp"
#include "HashTable.hpp"
#include "Queue.hpp"

namespace nexa::ds {

template <typename T>
class KaryTree {
public:
    struct Node {
        int id; std::string label; T value;
        Node* parent = nullptr;
        DynamicArray<Node*> children;
        int depth = 0;
    };

    explicit KaryTree(std::size_t k = 0) : k_(k) {}
    KaryTree(const KaryTree&) = delete;
    KaryTree& operator=(const KaryTree&) = delete;
    ~KaryTree() { destroy(root_); }

    Node* setRoot(int id, const std::string& label, const T& value = T{}) {
        destroy(root_); index_.clear(); size_ = 0;
        root_ = new Node{id, label, value, nullptr, {}, 0};
        index_.insert(id, root_);
        size_ = 1;
        return root_;
    }

    Node* addChild(int parentId, int id, const std::string& label, const T& value = T{}) {
        Node** p = index_.find(parentId);
        if (!p) throw std::invalid_argument("KaryTree::addChild unknown parent");
        if (index_.contains(id)) throw std::invalid_argument("KaryTree::addChild duplicate id");
        if (k_ && (*p)->children.size() >= k_) throw std::overflow_error("KaryTree: node already has K children");
        Node* n = new Node{id, label, value, *p, {}, 0};
        n->depth = (*p)->depth + 1;
        (*p)->children.pushBack(n);
        index_.insert(id, n);
        ++size_;
        return n;
    }

    Node* find(int id) const { Node* const* p = index_.find(id); return p ? *p : nullptr; }

    /// Case-insensitive label lookup, BFS order.
    Node* findByLabel(const std::string& label) const {
        Node* hit = nullptr;
        levelOrder([&](Node* n) { if (!hit && equalsIgnoreCase(n->label, label)) hit = n; });
        return hit;
    }

    /// Root ... node labels.
    DynamicArray<Node*> pathToRoot(int id) const {
        DynamicArray<Node*> rev, out;
        for (Node* n = find(id); n; n = n->parent) rev.pushBack(n);
        for (long i = (long)rev.size() - 1; i >= 0; --i) out.pushBack(rev[i]);
        return out;
    }

    bool removeSubtree(int id) {
        Node* n = find(id);
        if (!n) return false;
        if (n == root_) { destroy(root_); root_ = nullptr; index_.clear(); size_ = 0; return true; }
        n->parent->children.removeIf([n](Node* c) { return c == n; });
        unindex(n);
        destroy(n);
        return true;
    }

    template <typename Fn> void preorder(Fn fn) const { preRec(root_, fn); }
    template <typename Fn> void levelOrder(Fn fn) const {
        if (!root_) return;
        Queue<Node*> q; q.enqueue(root_);
        while (!q.isEmpty()) { Node* n = q.dequeue(); fn(n); for (Node* c : n->children) q.enqueue(c); }
    }
    int height() const { return heightRec(root_); }
    std::size_t size() const { return size_; }
    Node* root() const { return root_; }

private:
    Node* root_ = nullptr;
    std::size_t k_;
    std::size_t size_ = 0;
    HashTable<int, Node*> index_{CollisionStrategy::SeparateChaining, 32};

    static bool equalsIgnoreCase(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            char x = a[i], y = b[i];
            if (x >= 'A' && x <= 'Z') x = char(x - 'A' + 'a');
            if (y >= 'A' && y <= 'Z') y = char(y - 'A' + 'a');
            if (x != y) return false;
        }
        return true;
    }
    template <typename Fn> static void preRec(Node* n, Fn& fn) { if (!n) return; fn(n); for (Node* c : n->children) preRec(c, fn); }
    static int heightRec(Node* n) { if (!n) return 0; int b = 0; for (Node* c : n->children) { int h = heightRec(c); if (h > b) b = h; } return 1 + b; }
    void unindex(Node* n) { index_.erase(n->id); --size_; for (Node* c : n->children) unindex(c); }
    static void destroy(Node* n) { if (!n) return; for (Node* c : n->children) destroy(c); delete n; }
};

} // namespace nexa::ds
