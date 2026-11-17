#pragma once
// ============================================================================
// NEXA :: DynamicArray<T>
// A manually managed, growable array (replacement for std::vector).
// Application: registries of incidents, resources and hospitals; the general
// collection type returned by services and repositories.
//
// Complexity
//   access / update ......... O(1)
//   pushBack ................ O(1) amortised (capacity doubling)
//   insertAt / removeAt ..... O(n) (elements shifted)
//   linear find ............. O(n)
//   space ................... O(capacity)
// ============================================================================
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <initializer_list>

namespace nexa::ds {

template <typename T>
class DynamicArray {
public:
    DynamicArray() = default;
    explicit DynamicArray(std::size_t initialCapacity) { reserve(initialCapacity); }

    DynamicArray(std::initializer_list<T> init) {
        reserve(init.size());
        for (const T& v : init) pushBack(v);
    }

    DynamicArray(const DynamicArray& other) { copyFrom(other); }
    DynamicArray(DynamicArray&& other) noexcept { steal(other); }

    DynamicArray& operator=(const DynamicArray& other) {
        if (this != &other) { release(); copyFrom(other); }
        return *this;
    }
    DynamicArray& operator=(DynamicArray&& other) noexcept {
        if (this != &other) { release(); steal(other); }
        return *this;
    }
    ~DynamicArray() { release(); }

    // ---- capacity ---------------------------------------------------------
    std::size_t size() const     { return size_; }
    std::size_t capacity() const { return capacity_; }
    bool empty() const           { return size_ == 0; }

    void reserve(std::size_t newCap) {
        if (newCap <= capacity_) return;
        T* fresh = new T[newCap];
        for (std::size_t i = 0; i < size_; ++i) fresh[i] = std::move(data_[i]);
        delete[] data_;
        data_ = fresh;
        capacity_ = newCap;
    }

    // ---- element access -----------------------------------------------------
    T& operator[](std::size_t i)             { return data_[i]; }
    const T& operator[](std::size_t i) const { return data_[i]; }

    T& at(std::size_t i) {
        if (i >= size_) throw std::out_of_range("DynamicArray::at index out of range");
        return data_[i];
    }
    const T& at(std::size_t i) const {
        if (i >= size_) throw std::out_of_range("DynamicArray::at index out of range");
        return data_[i];
    }
    T& back()  { if (empty()) throw std::out_of_range("DynamicArray::back on empty"); return data_[size_ - 1]; }
    T& front() { if (empty()) throw std::out_of_range("DynamicArray::front on empty"); return data_[0]; }

    // ---- modifiers ----------------------------------------------------------
    void pushBack(const T& v) { grow(); data_[size_++] = v; }
    void pushBack(T&& v)      { grow(); data_[size_++] = std::move(v); }

    void popBack() {
        if (empty()) throw std::out_of_range("DynamicArray::popBack on empty");
        --size_;
    }

    void insertAt(std::size_t pos, const T& v) {
        if (pos > size_) throw std::out_of_range("DynamicArray::insertAt position out of range");
        grow();
        for (std::size_t i = size_; i > pos; --i) data_[i] = std::move(data_[i - 1]);
        data_[pos] = v;
        ++size_;
    }

    void removeAt(std::size_t pos) {
        if (pos >= size_) throw std::out_of_range("DynamicArray::removeAt position out of range");
        for (std::size_t i = pos; i + 1 < size_; ++i) data_[i] = std::move(data_[i + 1]);
        --size_;
    }

    /// Removes every element matching pred. Returns number removed. O(n)
    template <typename Pred>
    std::size_t removeIf(Pred pred) {
        std::size_t w = 0, removed = 0;
        for (std::size_t r = 0; r < size_; ++r) {
            if (pred(data_[r])) { ++removed; continue; }
            if (w != r) data_[w] = std::move(data_[r]);
            ++w;
        }
        size_ = w;
        return removed;
    }

    void clear() { size_ = 0; }

    // ---- search / traversal -------------------------------------------------
    /// Linear search. Returns index or -1.
    template <typename Pred>
    long findIndex(Pred pred) const {
        for (std::size_t i = 0; i < size_; ++i) if (pred(data_[i])) return static_cast<long>(i);
        return -1;
    }

    template <typename Pred>
    T* findFirst(Pred pred) {
        long i = findIndex(pred);
        return i < 0 ? nullptr : &data_[i];
    }

    template <typename Fn>
    void forEach(Fn fn) const { for (std::size_t i = 0; i < size_; ++i) fn(data_[i]); }

    template <typename Pred>
    DynamicArray filter(Pred pred) const {
        DynamicArray out;
        for (std::size_t i = 0; i < size_; ++i) if (pred(data_[i])) out.pushBack(data_[i]);
        return out;
    }

    // range-for support (raw pointer iterators)
    T* begin()             { return data_; }
    T* end()               { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const   { return data_ + size_; }

private:
    T* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;

    void grow() { if (size_ == capacity_) reserve(capacity_ == 0 ? 4 : capacity_ * 2); }

    void copyFrom(const DynamicArray& o) {
        data_ = o.capacity_ ? new T[o.capacity_] : nullptr;
        capacity_ = o.capacity_;
        size_ = o.size_;
        for (std::size_t i = 0; i < size_; ++i) data_[i] = o.data_[i];
    }
    void steal(DynamicArray& o) {
        data_ = o.data_; size_ = o.size_; capacity_ = o.capacity_;
        o.data_ = nullptr; o.size_ = o.capacity_ = 0;
    }
    void release() { delete[] data_; data_ = nullptr; size_ = capacity_ = 0; }
};

} // namespace nexa::ds
