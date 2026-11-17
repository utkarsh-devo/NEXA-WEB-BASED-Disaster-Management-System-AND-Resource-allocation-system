#pragma once
// ============================================================================
// NEXA :: Trie — prefix tree with first-child / next-sibling children
// (children kept sorted, so prefix listing is alphabetical).
// Each terminal node stores the set of record ids that contain that word.
// Application: autocomplete of incident titles / codes / locality names in
// the search bar ("gha" -> ghaziabad, ghazipur ...).
//
//  insert / search / remove ... O(L * sigma)  (L = word length, sigma = siblings scanned)
//  prefix listing ............. O(P * sigma + size of subtree output)
// Keys are lower-cased; any byte is allowed.
// ============================================================================
#include <cstddef>
#include <string>
#include "DynamicArray.hpp"

namespace nexa::ds {

class Trie {
    struct Node {
        char ch;
        bool terminal = false;
        DynamicArray<long> ids;
        Node* child = nullptr;     // first child
        Node* sibling = nullptr;   // next sibling (sorted by ch)
        explicit Node(char c) : ch(c) {}
    };
public:
    struct Match { std::string word; DynamicArray<long> ids; };

    Trie() : root_(new Node('\0')) {}
    Trie(const Trie&) = delete;
    Trie& operator=(const Trie&) = delete;
    ~Trie() { destroy(root_); }

    static std::string normalise(const std::string& s) {
        std::string out;
        for (char c : s) out += (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
        return out;
    }

    void insert(const std::string& rawWord, long id) {
        std::string w = normalise(rawWord);
        if (w.empty()) return;
        Node* n = root_;
        for (char c : w) n = childFor(n, c, true);
        if (!n->terminal) { n->terminal = true; ++words_; }
        if (n->ids.findIndex([id](long x) { return x == id; }) < 0) n->ids.pushBack(id);
    }

    const DynamicArray<long>* search(const std::string& rawWord) const {
        Node* n = walk(normalise(rawWord));
        return (n && n->terminal) ? &n->ids : nullptr;
    }

    bool startsWith(const std::string& prefix) const { return walk(normalise(prefix)) != nullptr; }

    /// Up to `limit` words beginning with prefix (alphabetical).
    DynamicArray<Match> withPrefix(const std::string& rawPrefix, std::size_t limit = 10) const {
        DynamicArray<Match> out;
        std::string p = normalise(rawPrefix);
        Node* n = walk(p);
        if (!n) return out;
        std::string buf = p;
        if (n->terminal) out.pushBack(Match{buf, n->ids});
        collect(n->child, buf, out, limit);
        return out;
    }

    /// Removes id from word; prunes nodes that become useless.
    bool remove(const std::string& rawWord, long id) {
        std::string w = normalise(rawWord);
        bool removed = false;
        removeRec(root_, w, 0, id, removed);
        return removed;
    }

    std::size_t wordCount() const { return words_; }
    void clear() { destroy(root_); root_ = new Node('\0'); words_ = 0; }

private:
    Node* root_;
    std::size_t words_ = 0;

    static Node* childFor(Node* n, char c, bool create) {
        Node** pp = &n->child;
        while (*pp && (*pp)->ch < c) pp = &(*pp)->sibling;
        if (*pp && (*pp)->ch == c) return *pp;
        if (!create) return nullptr;
        Node* x = new Node(c);
        x->sibling = *pp;
        *pp = x;
        return x;
    }
    Node* walk(const std::string& w) const {
        Node* n = root_;
        for (char c : w) { n = childFor(n, c, false); if (!n) return nullptr; }
        return n;
    }
    static void collect(Node* n, std::string& buf, DynamicArray<Match>& out, std::size_t limit) {
        for (; n && out.size() < limit; n = n->sibling) {
            buf.push_back(n->ch);
            if (n->terminal) out.pushBack(Match{buf, n->ids});
            collect(n->child, buf, out, limit);
            buf.pop_back();
        }
    }
    // returns true if node n can be deleted by its parent
    bool removeRec(Node* n, const std::string& w, std::size_t i, long id, bool& removed) {
        if (i == w.size()) {
            if (!n->terminal) return false;
            removed = n->ids.removeIf([id](long x) { return x == id; }) > 0;
            if (n->ids.empty()) { n->terminal = false; --words_; }
            return !n->terminal && !n->child;
        }
        Node** pp = &n->child;
        while (*pp && (*pp)->ch != w[i]) pp = &(*pp)->sibling;
        if (!*pp) return false;
        if (removeRec(*pp, w, i + 1, id, removed)) { Node* d = *pp; *pp = d->sibling; delete d; }
        return n != root_ && !n->terminal && !n->child;
    }
    static void destroy(Node* n) {
        while (n) { destroy(n->child); Node* s = n->sibling; delete n; n = s; }
    }
};

} // namespace nexa::ds
