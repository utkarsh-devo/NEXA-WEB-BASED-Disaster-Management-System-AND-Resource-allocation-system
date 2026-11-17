#pragma once
// ============================================================================
// NEXA :: Queue<T>  (circular buffer with automatic growth)
// Application: ordinary FIFO emergency-request intake. (Urgency-based
// processing uses PriorityQueue instead — the spec requires both.)
// enqueue: O(1) amortised   dequeue / front / rear: O(1)
// ============================================================================
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace nexa::ds {

template <typename T>
class Queue {
public:
    explicit Queue(std::size_t initialCapacity = 8)
        : buf_(new T[initialCapacity ? initialCapacity : 1]), cap_(initialCapacity ? initialCapacity : 1) {}
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    ~Queue() { delete[] buf_; }

    void enqueue(const T& v) {
        if (size_ == cap_) grow();
        buf_[(head_ + size_) % cap_] = v;
        ++size_;
    }
    T dequeue() {
        if (isEmpty()) throw std::underflow_error("Queue underflow");
        T v = std::move(buf_[head_]);
        head_ = (head_ + 1) % cap_;
        --size_;
        return v;
    }
    const T& front() const {
        if (isEmpty()) throw std::underflow_error("Queue is empty");
        return buf_[head_];
    }
    const T& rear() const {
        if (isEmpty()) throw std::underflow_error("Queue is empty");
        return buf_[(head_ + size_ - 1) % cap_];
    }
    bool isEmpty() const { return size_ == 0; }
    std::size_t size() const { return size_; }

    /// Display: front -> rear
    template <typename Fn> void forEach(Fn fn) const {
        for (std::size_t i = 0; i < size_; ++i) fn(buf_[(head_ + i) % cap_]);
    }

    /// Remove first element matching pred (used when a request is cancelled). O(n)
    template <typename Pred> bool removeFirst(Pred pred) {
        for (std::size_t i = 0; i < size_; ++i) {
            if (pred(buf_[(head_ + i) % cap_])) {
                for (std::size_t j = i; j + 1 < size_; ++j)
                    buf_[(head_ + j) % cap_] = std::move(buf_[(head_ + j + 1) % cap_]);
                --size_;
                return true;
            }
        }
        return false;
    }
    void clear() { head_ = size_ = 0; }

private:
    T* buf_;
    std::size_t cap_;
    std::size_t head_ = 0;
    std::size_t size_ = 0;

    void grow() {
        std::size_t nc = cap_ * 2;
        T* nb = new T[nc];
        for (std::size_t i = 0; i < size_; ++i) nb[i] = std::move(buf_[(head_ + i) % cap_]);
        delete[] buf_;
        buf_ = nb; cap_ = nc; head_ = 0;
    }
};

} // namespace nexa::ds
