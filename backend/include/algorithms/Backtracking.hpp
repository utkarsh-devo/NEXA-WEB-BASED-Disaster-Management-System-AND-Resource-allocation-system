#pragma once
// ============================================================================
// NEXA :: Backtracking demonstrations (DSA Lab)
//
// Rat in a Maze — find a path from (0,0) to (n-1,m-1) through open cells
//   (moves D, L, R, U). Iterative: an explicit stack of frames (DynamicArray
//   used as a stack so the final path can be read bottom-to-top).
//   Disaster framing: a rescue robot navigating a collapsed-building grid.
//   Time O(4^(n*m)) worst case, typically far less due to visited marking;
//   with visited marking each cell is entered at most once -> O(n*m).
//
// N-Queens — place n queens so none attack each other. Column/diagonal
//   occupancy arrays make each safety check O(1). Time O(n!) worst case.
// ============================================================================
#include <cstddef>
#include "OpCounter.hpp"
#include "../data_structures/DynamicArray.hpp"

namespace nexa::algo {

using ds::DynamicArray;

struct MazeResult {
    bool found = false;
    DynamicArray<int> path;          // flattened cell indices r*cols+c in order
    std::size_t explored = 0;        // cells entered
    std::size_t backtracks = 0;
};

/// grid: row-major, 1 = open, 0 = wall.
inline MazeResult ratInMaze(const DynamicArray<int>& grid, int rows, int cols) {
    MazeResult res;
    if (rows <= 0 || cols <= 0 || (int)grid.size() != rows * cols || grid[0] == 0 || grid[(std::size_t)rows * cols - 1] == 0) return res;
    DynamicArray<bool> seen((std::size_t)rows * cols);
    for (int i = 0; i < rows * cols; ++i) seen.pushBack(false);
    const int dr[4] = {1, 0, 0, -1}, dc[4] = {0, -1, 1, 0};   // D L R U

    // explicit DFS with per-frame "next direction" counter (iterative backtracking)
    DynamicArray<int> cell, nextDir;
    cell.pushBack(0); nextDir.pushBack(0); seen[0] = true; res.explored = 1;
    while (!cell.empty()) {
        int cur = cell.back();
        if (cur == rows * cols - 1) { res.found = true; res.path = cell; return res; }
        int& d = nextDir.back();
        bool advanced = false;
        while (d < 4) {
            int r = cur / cols + dr[d], c = cur % cols + dc[d];
            ++d;
            if (r < 0 || c < 0 || r >= rows || c >= cols) continue;
            int idx = r * cols + c;
            if (grid[idx] == 0 || seen[idx]) continue;
            seen[idx] = true; ++res.explored;
            cell.pushBack(idx); nextDir.pushBack(0);
            advanced = true;
            break;
        }
        if (!advanced) { cell.popBack(); nextDir.popBack(); ++res.backtracks; }
    }
    return res;
}

struct NQueensResult {
    std::size_t solutionCount = 0;
    DynamicArray<int> firstSolution;    // firstSolution[row] = column
    std::size_t nodesVisited = 0;
    std::size_t backtracks = 0;
};

namespace detail {
inline void placeQueen(int row, int n, DynamicArray<int>& cols, DynamicArray<bool>& colUsed,
                       DynamicArray<bool>& d1, DynamicArray<bool>& d2, NQueensResult& r, std::size_t limit) {
    if (row == n) {
        if (r.solutionCount == 0) r.firstSolution = cols;
        ++r.solutionCount;
        return;
    }
    for (int c = 0; c < n; ++c) {
        if (limit && r.solutionCount >= limit) return;
        ++r.nodesVisited;
        if (colUsed[c] || d1[row + c] || d2[row - c + n - 1]) continue;
        colUsed[c] = d1[row + c] = d2[row - c + n - 1] = true;
        cols[row] = c;
        placeQueen(row + 1, n, cols, colUsed, d1, d2, r, limit);
        colUsed[c] = d1[row + c] = d2[row - c + n - 1] = false;   // undo = backtrack
        ++r.backtracks;
    }
}
} // namespace detail

/// limit = 0 counts all solutions; otherwise stops after `limit` solutions.
inline NQueensResult nQueens(int n, std::size_t limit = 0) {
    NQueensResult r;
    if (n <= 0 || n > 14) return r;
    DynamicArray<int> cols; DynamicArray<bool> colUsed, d1, d2;
    for (int i = 0; i < n; ++i) { cols.pushBack(-1); colUsed.pushBack(false); }
    for (int i = 0; i < 2 * n - 1; ++i) { d1.pushBack(false); d2.pushBack(false); }
    detail::placeQueen(0, n, cols, colUsed, d1, d2, r, limit);
    return r;
}

} // namespace nexa::algo
