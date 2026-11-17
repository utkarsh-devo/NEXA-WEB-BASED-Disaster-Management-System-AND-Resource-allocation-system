#pragma once
// ============================================================================
// NEXA :: HashTable<K,V>
// One table, four collision strategies (chosen at construction):
//   SeparateChaining  — each bucket is a singly linked chain
//   LinearProbing     — probe i: (h + i)            mod m
//   QuadraticProbing  — probe i: (h + i(i+1)/2)     mod m  (m = power of 2,
//                       triangular offsets visit every slot)
//   DoubleHashing     — probe i: (h1 + i*h2)        mod m  (h2 forced odd)
// Open-addressing deletion uses tombstones. The table rehashes (doubles)
// when the load factor exceeds maxLoad (0.75 chaining / 0.6 open addressing).
//
// Application: O(1) average lookup of incidents, resources, hospitals and
// deployments by id; also the position-map inside PriorityQueue.
//   insert / find / erase ... O(1) average, O(n) worst case
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include "DynamicArray.hpp"
#include "../algorithms/OpCounter.hpp"

namespace nexa::ds {

enum class CollisionStrategy { SeparateChaining, LinearProbing, QuadraticProbing, DoubleHashing };

inline const char* toString(CollisionStrategy s) {
    switch (s) {
        case CollisionStrategy::SeparateChaining: return "separate_chaining";
        case CollisionStrategy::LinearProbing:    return "linear_probing";
        case CollisionStrategy::QuadraticProbing: return "quadratic_probing";
        case CollisionStrategy::DoubleHashing:    return "double_hashing";
    }
    return "unknown";
}

// ---- hash functions --------------------------------------------------------
template <typename K, typename Enable = void> struct Hasher;

/// Integers: 64-bit mix (splitmix64 finaliser) — spreads sequential ids.
template <typename K>
struct Hasher<K, typename std::enable_if<std::is_integral<K>::value>::type> {
    static std::uint64_t primary(K key) {
        std::uint64_t x = static_cast<std::uint64_t>(key) + 0x9E3779B97F4A7C15ULL;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
        return x ^ (x >> 31);
    }
    static std::uint64_t secondary(K key) {          // independent of primary
        std::uint64_t x = static_cast<std::uint64_t>(key);
        return (x * 0xC2B2AE3D27D4EB4FULL) ^ (x >> 17);
    }
};

/// Strings: FNV-1a (primary) and polynomial rolling hash, base 131 (secondary).
template <>
struct Hasher<std::string> {
    static std::uint64_t primary(const std::string& s) {
        std::uint64_t h = 1469598103934665603ULL;
        for (unsigned char ch : s) { h ^= ch; h *= 1099511628211ULL; }
        return h;
    }
    static std::uint64_t secondary(const std::string& s) {
        std::uint64_t h = 0;
        for (unsigned char ch : s) h = h * 131 + ch;
        return h;
    }
};

template <typename K, typename V>
class HashTable {
    struct ChainNode { K key; V value; ChainNode* next; };
    enum class SlotState : unsigned char { Empty, Occupied, Deleted };
    struct Slot { K key{}; V value{}; SlotState state = SlotState::Empty; };

public:
    explicit HashTable(CollisionStrategy strategy = CollisionStrategy::SeparateChaining,
                       std::size_t initialCapacity = 16)
        : strategy_(strategy) {
        capacity_ = 8;
        while (capacity_ < initialCapacity) capacity_ <<= 1;   // power of two
        allocate();
    }
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;
    ~HashTable() { deallocate(); }

    /// Inserts or overwrites. Returns true when a new key was added.
    bool insert(const K& key, const V& value) {
        if ((double)(size_ + tombstones_ + 1) / (double)capacity_ > maxLoad()) rehash(capacity_ * 2);
        return chaining() ? chainInsert(key, value) : openInsert(key, value);
    }

    V* find(const K& key) {
        if (chaining()) {
            for (ChainNode* n = buckets_[index(key)]; n; n = n->next) { algo::op(&stats_); if (n->key == key) return &n->value; }
            return nullptr;
        }
        long slot = openLocate(key);
        return slot < 0 ? nullptr : &slots_[slot].value;
    }
    const V* find(const K& key) const { return const_cast<HashTable*>(this)->find(key); }
    bool contains(const K& key) const { return find(key) != nullptr; }

    bool erase(const K& key) {
        if (chaining()) {
            for (ChainNode** pp = &buckets_[index(key)]; *pp; pp = &(*pp)->next) {
                if ((*pp)->key == key) { ChainNode* d = *pp; *pp = d->next; delete d; --size_; return true; }
            }
            return false;
        }
        long slot = openLocate(key);
        if (slot < 0) return false;
        slots_[slot].state = SlotState::Deleted;
        --size_; ++tombstones_;
        return true;
    }

    template <typename Fn> void forEach(Fn fn) const {
        if (chaining()) {
            for (std::size_t b = 0; b < capacity_; ++b)
                for (ChainNode* n = buckets_[b]; n; n = n->next) fn(n->key, n->value);
        } else {
            for (std::size_t i = 0; i < capacity_; ++i)
                if (slots_[i].state == SlotState::Occupied) fn(slots_[i].key, slots_[i].value);
        }
    }

    void clear() { deallocate(); allocate(); size_ = tombstones_ = 0; }

    /// Visualisation: fn(slotIndex, keysInSlot, state) with state 0=empty 1=occupied 2=deleted (tombstone).
    template <typename Fn> void forEachBucket(Fn fn) const {
        for (std::size_t i = 0; i < capacity_; ++i) {
            DynamicArray<K> keys;
            if (chaining()) {
                for (ChainNode* n = buckets_[i]; n; n = n->next) keys.pushBack(n->key);
                fn(i, keys, keys.empty() ? 0 : 1);
            } else {
                if (slots_[i].state == SlotState::Occupied) keys.pushBack(slots_[i].key);
                fn(i, keys, slots_[i].state == SlotState::Empty ? 0 : (slots_[i].state == SlotState::Occupied ? 1 : 2));
            }
        }
    }

    // ---- diagnostics (DSA Lab / benchmark) ------------------------------------
    std::size_t size() const { return size_; }
    std::size_t capacity() const { return capacity_; }
    double loadFactor() const { return (double)size_ / (double)capacity_; }
    std::size_t collisions() const { return collisions_; }        // probes/chain steps beyond first slot on insert
    std::size_t probes() const { return stats_.operations; }      // total probe steps on lookups
    std::size_t rehashCount() const { return rehashes_; }
    CollisionStrategy strategy() const { return strategy_; }
    /// Longest chain (chaining) or longest probe cluster (open addressing).
    std::size_t longestCluster() const {
        std::size_t best = 0, cur = 0;
        if (chaining()) {
            for (std::size_t b = 0; b < capacity_; ++b) {
                cur = 0; for (ChainNode* n = buckets_[b]; n; n = n->next) ++cur;
                if (cur > best) best = cur;
            }
        } else {
            for (std::size_t i = 0; i < capacity_; ++i) {
                cur = slots_[i].state == SlotState::Empty ? 0 : cur + 1;
                if (cur > best) best = cur;
            }
        }
        return best;
    }

private:
    CollisionStrategy strategy_;
    std::size_t capacity_ = 0, size_ = 0, tombstones_ = 0, collisions_ = 0, rehashes_ = 0;
    ChainNode** buckets_ = nullptr;
    Slot* slots_ = nullptr;
    algo::OpCounter stats_;

    bool chaining() const { return strategy_ == CollisionStrategy::SeparateChaining; }
    double maxLoad() const { return chaining() ? 0.75 : 0.6; }
    std::size_t index(const K& key) const { return Hasher<K>::primary(key) & (capacity_ - 1); }

    std::size_t probe(const K& key, std::size_t h, std::size_t i) const {
        switch (strategy_) {
            case CollisionStrategy::LinearProbing:    return (h + i) & (capacity_ - 1);
            case CollisionStrategy::QuadraticProbing: return (h + i * (i + 1) / 2) & (capacity_ - 1);
            case CollisionStrategy::DoubleHashing: {
                std::size_t step = (Hasher<K>::secondary(key) | 1ULL);  // odd => coprime with 2^k
                return (h + i * step) & (capacity_ - 1);
            }
            default: return h;
        }
    }

    void allocate() {
        if (chaining()) { buckets_ = new ChainNode*[capacity_]; for (std::size_t i = 0; i < capacity_; ++i) buckets_[i] = nullptr; }
        else slots_ = new Slot[capacity_];
    }
    void deallocate() {
        if (buckets_) {
            for (std::size_t b = 0; b < capacity_; ++b) { ChainNode* n = buckets_[b]; while (n) { ChainNode* x = n->next; delete n; n = x; } }
            delete[] buckets_; buckets_ = nullptr;
        }
        delete[] slots_; slots_ = nullptr;
    }

    bool chainInsert(const K& key, const V& value) {
        std::size_t b = index(key);
        for (ChainNode* n = buckets_[b]; n; n = n->next) if (n->key == key) { n->value = value; return false; }
        if (buckets_[b]) ++collisions_;
        buckets_[b] = new ChainNode{key, value, buckets_[b]};
        ++size_;
        return true;
    }

    long openLocate(const K& key) {
        std::size_t h = index(key);
        for (std::size_t i = 0; i < capacity_; ++i) {
            std::size_t s = probe(key, h, i);
            algo::op(&stats_);
            if (slots_[s].state == SlotState::Empty) return -1;
            if (slots_[s].state == SlotState::Occupied && slots_[s].key == key) return (long)s;
        }
        return -1;
    }

    bool openInsert(const K& key, const V& value) {
        long existing = openLocate(key);
        if (existing >= 0) { slots_[existing].value = value; return false; }
        std::size_t h = index(key);
        for (std::size_t i = 0; i < capacity_; ++i) {
            std::size_t s = probe(key, h, i);
            if (slots_[s].state != SlotState::Occupied) {
                if (slots_[s].state == SlotState::Deleted) --tombstones_;
                slots_[s].key = key; slots_[s].value = value; slots_[s].state = SlotState::Occupied;
                ++size_;
                return true;
            }
            ++collisions_;
        }
        rehash(capacity_ * 2);          // should not happen with load factor <= 0.6
        return openInsert(key, value);
    }

    void rehash(std::size_t newCap) {
        ChainNode** oldBuckets = buckets_;
        Slot* oldSlots = slots_;
        std::size_t oldCap = capacity_;
        buckets_ = nullptr; slots_ = nullptr;
        capacity_ = newCap; size_ = 0; tombstones_ = 0;
        allocate();
        ++rehashes_;
        if (oldBuckets) {
            for (std::size_t b = 0; b < oldCap; ++b) {
                ChainNode* n = oldBuckets[b];
                while (n) { chainInsert(n->key, n->value); ChainNode* x = n->next; delete n; n = x; }
            }
            delete[] oldBuckets;
        }
        if (oldSlots) {
            for (std::size_t i = 0; i < oldCap; ++i)
                if (oldSlots[i].state == SlotState::Occupied) openInsert(oldSlots[i].key, oldSlots[i].value);
            delete[] oldSlots;
        }
    }
};

} // namespace nexa::ds
