#pragma once
// ============================================================================
// NEXA :: OpCounter
// Lightweight instrumentation used by the DSA Laboratory and benchmarks.
// Every algorithm that accepts an OpCounter* records comparisons / swaps /
// generic operations, so the frontend can display *measured* work, not just
// theoretical Big-O.
// ============================================================================
#include <cstddef>

namespace nexa::algo {

struct OpCounter {
    std::size_t comparisons = 0;
    std::size_t swaps       = 0;   // swaps or element moves
    std::size_t operations  = 0;   // other primitive steps (visits, relaxations ...)

    void reset() { comparisons = swaps = operations = 0; }
};

inline void cmp(OpCounter* c)  { if (c) ++c->comparisons; }
inline void swp(OpCounter* c)  { if (c) ++c->swaps; }
inline void op(OpCounter* c)   { if (c) ++c->operations; }

} // namespace nexa::algo
