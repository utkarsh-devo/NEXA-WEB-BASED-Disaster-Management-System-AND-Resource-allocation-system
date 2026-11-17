#pragma once
// ============================================================================
// NEXA :: SuffixTree — compressed suffix trie (edges labelled by [start,end)
// index ranges into the text, children as sorted sibling lists).
//
// Construction here is the straightforward insertion of every suffix with
// edge splitting: O(n^2) worst case time, O(n) nodes. Ukkonen's algorithm
// achieves O(n) but is substantially more complex; NEXA uses the suffix
// tree on modest text sizes (DSA Lab + comparison against SuffixArray),
// and uses the SuffixArray for the production description search.
//
//  search(pattern) ....... O(m * sigma)
//  occurrences ........... O(m * sigma + occ)
//  space ................. O(n) nodes (labels are index pairs, not copies)
// ============================================================================
#include <cstddef>
#include <string>
#include "DynamicArray.hpp"

namespace nexa::ds {

class SuffixTree {
    struct Node {
        long start = 0, end = 0;   // edge label text_[start, end)
        long suffixIndex = -1;     // leaf: starting position of the suffix
        Node* child = nullptr;
        Node* sibling = nullptr;
    };
public:
    SuffixTree() = default;
    SuffixTree(const SuffixTree&) = delete;
    SuffixTree& operator=(const SuffixTree&) = delete;
    ~SuffixTree() { destroy(root_); }

    void build(const std::string& raw) {
        destroy(root_);
        text_.clear();
        for (char c : raw) text_ += (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
        text_ += '\x02';                       // unique terminator -> every suffix ends at a leaf
        root_ = new Node;
        nodes_ = 1;
        for (long i = 0; i < (long)text_.size(); ++i) insertSuffix(i);
    }

    bool contains(const std::string& p) const { return locate(lower(p)) != nullptr || p.empty(); }

    /// Starting offsets of every occurrence of p.
    DynamicArray<long> occurrences(const std::string& rawP) const {
        DynamicArray<long> out;
        std::string p = lower(rawP);
        if (p.empty()) return out;
        Node* n = locate(p);
        if (n) collectLeaves(n, out);
        return out;
    }

    std::size_t nodeCount() const { return nodes_; }
    const std::string& text() const { return text_; }

private:
    Node* root_ = nullptr;
    std::string text_;
    std::size_t nodes_ = 0;

    static std::string lower(const std::string& s) { std::string o; for (char c : s) o += (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; return o; }

    Node* findChild(Node* n, char c) const {
        for (Node* x = n->child; x; x = x->sibling) if (text_[x->start] == c) return x;
        return nullptr;
    }
    void addChild(Node* parent, Node* c) {
        Node** pp = &parent->child;
        while (*pp && text_[(*pp)->start] < text_[c->start]) pp = &(*pp)->sibling;
        c->sibling = *pp;
        *pp = c;
    }

    void insertSuffix(long i) {
        Node* n = root_;
        long pos = i, len = (long)text_.size();
        while (pos < len) {
            Node* c = findChild(n, text_[pos]);
            if (!c) {
                Node* leaf = new Node{pos, len, i}; ++nodes_;
                addChild(n, leaf);
                return;
            }
            long k = 0, edgeLen = c->end - c->start;
            while (k < edgeLen && pos + k < len && text_[c->start + k] == text_[pos + k]) ++k;
            if (k == edgeLen) { n = c; pos += k; continue; }   // consumed whole edge
            // split edge c at k
            Node* mid = new Node{c->start, c->start + k, -1}; ++nodes_;
            // replace c by mid in n's child list
            for (Node** pp = &n->child; *pp; pp = &(*pp)->sibling)
                if (*pp == c) { mid->sibling = c->sibling; *pp = mid; break; }
            c->start += k;
            c->sibling = nullptr;
            addChild(mid, c);
            Node* leaf = new Node{pos + k, len, i}; ++nodes_;
            addChild(mid, leaf);
            return;
        }
    }

    // returns node whose path label starts with p (p may end mid-edge)
    Node* locate(const std::string& p) const {
        if (!root_) return nullptr;
        Node* n = root_;
        std::size_t i = 0;
        while (i < p.size()) {
            Node* c = findChild(n, p[i]);
            if (!c) return nullptr;
            long k = 0, edgeLen = c->end - c->start;
            while (k < edgeLen && i < p.size()) {
                if (text_[c->start + k] != p[i]) return nullptr;
                ++k; ++i;
            }
            n = c;
        }
        return n;
    }
    static void collectLeaves(Node* n, DynamicArray<long>& out) {
        if (!n->child) { out.pushBack(n->suffixIndex); return; }
        for (Node* c = n->child; c; c = c->sibling) collectLeaves(c, out);
    }
    static void destroy(Node* n) {
        if (!n) return;
        Node* c = n->child;
        while (c) { Node* s = c->sibling; destroy(c); c = s; }
        delete n;
    }
};

} // namespace nexa::ds
