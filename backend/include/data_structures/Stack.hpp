#pragma once
// ============================================================================
// NEXA :: Stack<T>  (linked implementation, optional capacity bound)
// Application: operational action history -> "undo last operator action"
// and the Rat-in-a-Maze backtracking path.
// push / pop / peek: O(1)   space: O(n)
// ============================================================================
#include <cstddef>
#include <stdexcept>

namespace nexa::ds {

template <typename T>
class Stack {
    struct Node { T value; Node* next; };
public:
    explicit Stack(std::size_t maxCapacity = 0) : maxCapacity_(maxCapacity) {}
    Stack(const Stack&) = delete;
    Stack& operator=(const Stack&) = delete;
    ~Stack() { clear(); }

    void push(const T& v) {
        if (isFull()) throw std::overflow_error("Stack overflow");
        top_ = new Node{v, top_};
        ++size_;
    }
    T pop() {
        if (isEmpty()) throw std::underflow_error("Stack underflow");
        Node* n = top_;
        T v = n->value;
        top_ = n->next;
        delete n;
        --size_;
        return v;
    }
    const T& peek() const {
        if (isEmpty()) throw std::underflow_error("Stack is empty");
        return top_->value;
    }
    bool isEmpty() const { return top_ == nullptr; }
    bool isFull() const  { return maxCapacity_ != 0 && size_ >= maxCapacity_; }
    std::size_t size() const { return size_; }

    /// Display / traversal from top to bottom.
    template <typename Fn> void forEach(Fn fn) const {
        for (Node* n = top_; n; n = n->next) fn(n->value);
    }
    void clear() { while (!isEmpty()) pop(); }

private:
    Node* top_ = nullptr;
    std::size_t size_ = 0;
    std::size_t maxCapacity_;
};

} // namespace nexa::ds
