#pragma once
// ============================================================================
// NEXA :: SuffixArray (+ LCP array, Kasai)
// Construction by prefix doubling: rank pairs are re-sorted each round with
// a stable radix pass (counting sort on second key, then first key).
//
//  build ............... O(n log n)  (log n rounds x O(n) radix passes)
//  LCP (Kasai) ......... O(n)
//  find occurrences .... O(m log n + occ)
//  space ............... O(n)
// Application: substring search inside incident descriptions
// ("trapped", "gas leak") across the whole corpus. Multiple documents are
// concatenated with a '\x01' separator; positions map back to document ids
// by binary search over document start offsets.
// ============================================================================
#include <cstddef>
#include <string>
#include "DynamicArray.hpp"

namespace nexa::ds {

class SuffixArray {
public:
    struct Occurrence { long docId; long offset; };

    void clearDocuments() { text_.clear(); docStart_.clear(); docIds_.clear(); sa_.clear(); lcp_.clear(); }

    void addDocument(long docId, const std::string& raw) {
        docStart_.pushBack((long)text_.size());
        docIds_.pushBack(docId);
        for (char c : raw) text_ += (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : (c == '\x01' ? ' ' : c);
        text_ += '\x01';
    }

    void build() {
        long n = (long)text_.size();
        sa_.clear(); lcp_.clear();
        if (n == 0) return;
        DynamicArray<long> rank, tmp, sa2;
        for (long i = 0; i < n; ++i) { sa_.pushBack(i); rank.pushBack((unsigned char)text_[i]); tmp.pushBack(0); sa2.pushBack(0); }
        long classes = 256;
        for (long k = 1; ; k <<= 1) {
            // sort by (rank[i], rank[i+k]) with two stable counting passes
            auto second = [&](long i) { return i + k < n ? rank[i + k] + 1 : 0; };
            countingPass(sa_, sa2, second, classes + 1);
            countingPass(sa2, sa_, [&](long i) { return rank[i]; }, classes + 1);
            tmp[sa_[0]] = 0;
            for (long i = 1; i < n; ++i) {
                long a = sa_[i - 1], b = sa_[i];
                bool same = rank[a] == rank[b] && second(a) == second(b);
                tmp[b] = tmp[a] + (same ? 0 : 1);
            }
            for (long i = 0; i < n; ++i) rank[i] = tmp[i];
            classes = rank[sa_[n - 1]] + 1;
            if (classes == n) break;          // all suffixes distinct
            if (k > n) break;
        }
        buildLcp(rank);
    }

    /// All occurrences of pattern (case-insensitive).
    DynamicArray<Occurrence> find(const std::string& rawPattern, std::size_t* comparisons = nullptr) const {
        DynamicArray<Occurrence> out;
        std::string p;
        for (char c : rawPattern) p += (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
        if (p.empty() || sa_.empty()) return out;
        std::size_t cmps = 0;
        long lo = 0, hi = (long)sa_.size();
        while (lo < hi) { long mid = (lo + hi) / 2; ++cmps; if (text_.compare(sa_[mid], p.size(), p) < 0) lo = mid + 1; else hi = mid; }
        long first = lo; hi = (long)sa_.size();
        while (lo < hi) { long mid = (lo + hi) / 2; ++cmps; if (text_.compare(sa_[mid], p.size(), p) <= 0) lo = mid + 1; else hi = mid; }
        for (long i = first; i < lo; ++i) out.pushBack(toOccurrence(sa_[i]));
        if (comparisons) *comparisons = cmps;
        return out;
    }

    const DynamicArray<long>& suffixArray() const { return sa_; }
    const DynamicArray<long>& lcp() const { return lcp_; }
    const std::string& text() const { return text_; }
    std::size_t documentCount() const { return docIds_.size(); }

private:
    std::string text_;
    DynamicArray<long> docStart_, docIds_, sa_, lcp_;

    template <typename KeyFn>
    static void countingPass(const DynamicArray<long>& in, DynamicArray<long>& out, KeyFn key, long range) {
        DynamicArray<long> cnt((std::size_t)range + 1);
        for (long i = 0; i <= range; ++i) cnt.pushBack(0);
        for (long v : in) ++cnt[key(v)];
        long sum = 0;
        for (long i = 0; i <= range; ++i) { long c = cnt[i]; cnt[i] = sum; sum += c; }
        for (long v : in) out[cnt[key(v)]++] = v;
    }

    void buildLcp(const DynamicArray<long>& rank) {
        long n = (long)sa_.size();
        for (long i = 0; i < n; ++i) lcp_.pushBack(0);
        long h = 0;
        for (long i = 0; i < n; ++i) {
            if (rank[i] > 0) {
                long j = sa_[rank[i] - 1];
                while (i + h < n && j + h < n && text_[i + h] == text_[j + h]) ++h;
                lcp_[rank[i]] = h;
                if (h > 0) --h;
            } else h = 0;
        }
    }

    Occurrence toOccurrence(long pos) const {
        long lo = 0, hi = (long)docStart_.size() - 1;
        while (lo < hi) { long mid = (lo + hi + 1) / 2; if (docStart_[mid] <= pos) lo = mid; else hi = mid - 1; }
        return Occurrence{docIds_[lo], pos - docStart_[lo]};
    }
};

} // namespace nexa::ds
