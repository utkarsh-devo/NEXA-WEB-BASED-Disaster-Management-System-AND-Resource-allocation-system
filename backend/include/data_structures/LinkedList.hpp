#pragma once
// ============================================================================
// NEXA :: LinkedList<T>  (doubly linked, with tail pointer)
// Application: chronological incident event timeline, resource history,
// request history, deployment history.
//   insertFront / insertBack ...... O(1)
//   insertAt / removeAt / search .. O(n)
// ============================================================================
#include <cstddef>
#include <stdexcept>

namespace nexa::ds {

template <typename T>
class LinkedList {
    struct Node { T value; Node* prev; Node* next; };
public:
    LinkedList() = default;
    LinkedList(const LinkedList& o) { o.forEach([this](const T& v) { insertBack(v); }); }
    LinkedList& operator=(const LinkedList& o) {
        if (this != &o) { clear(); o.forEach([this](const T& v) { insertBack(v); }); }
        return *this;
    }
    LinkedList(LinkedList&& o) noexcept : head_(o.head_), tail_(o.tail_), size_(o.size_) {
        o.head_ = o.tail_ = nullptr; o.size_ = 0;
    }
    ~LinkedList() { clear(); }

    void insertFront(const T& v) {
        Node* n = new Node{v, nullptr, head_};
        if (head_) head_->prev = n; else tail_ = n;
        head_ = n; ++size_;
    }
    void insertBack(const T& v) {
        Node* n = new Node{v, tail_, nullptr};
        if (tail_) tail_->next = n; else head_ = n;
        tail_ = n; ++size_;
    }
    void insertAt(std::size_t pos, const T& v) {
        if (pos > size_) throw std::out_of_range("LinkedList::insertAt out of range");
        if (pos == 0) return insertFront(v);
        if (pos == size_) return insertBack(v);
        Node* cur = nodeAt(pos);
        Node* n = new Node{v, cur->prev, cur};
        cur->prev->next = n;
        cur->prev = n;
        ++size_;
    }
    void removeAt(std::size_t pos) {
        if (pos >= size_) throw std::out_of_range("LinkedList::removeAt out of range");
        unlink(nodeAt(pos));
    }
    /// Remove first node matching pred. O(n)
    template <typename Pred> bool removeFirst(Pred pred) {
        for (Node* n = head_; n; n = n->next) if (pred(n->value)) { unlink(n); return true; }
        return false;
    }
    template <typename Pred> T* find(Pred pred) {
        for (Node* n = head_; n; n = n->next) if (pred(n->value)) return &n->value;
        return nullptr;
    }
    /// Update: apply fn to first match.
    template <typename Pred, typename Fn> bool update(Pred pred, Fn fn) {
        T* v = find(pred);
        if (!v) return false;
        fn(*v);
        return true;
    }
    T& get(std::size_t pos) {
        if (pos >= size_) throw std::out_of_range("LinkedList::get out of range");
        return nodeAt(pos)->value;
    }
    template <typename Fn> void forEach(Fn fn) const { for (Node* n = head_; n; n = n->next) fn(n->value); }
    template <typename Fn> void forEachReverse(Fn fn) const { for (Node* n = tail_; n; n = n->prev) fn(n->value); }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    void clear() { while (head_) unlink(head_); }

private:
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    std::size_t size_ = 0;

    Node* nodeAt(std::size_t pos) const {
        // walk from the nearer end
        if (pos < size_ / 2) { Node* n = head_; while (pos--) n = n->next; return n; }
        Node* n = tail_; for (std::size_t i = size_ - 1; i > pos; --i) n = n->prev; return n;
    }
    void unlink(Node* n) {
        if (n->prev) n->prev->next = n->next; else head_ = n->next;
        if (n->next) n->next->prev = n->prev; else tail_ = n->prev;
        delete n; --size_;
    }
};

} // namespace nexa::ds
