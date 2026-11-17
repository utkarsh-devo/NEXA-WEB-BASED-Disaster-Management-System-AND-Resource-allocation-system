#pragma once
// ============================================================================
// NEXA :: Module 12 — DSA Laboratory & §8.4 Benchmarking
// Runs the SAME C++ structures/algorithms used by the operational modules
// on user-supplied input and returns structure snapshots, animation frames,
// measured operation counts and theoretical complexity. The frontend only
// draws what this service returns.
// ============================================================================
#include <string>
#include "../data_structures/DynamicArray.hpp"

namespace nexa::svc {

using ds::DynamicArray;

struct LabStat { std::string key, value; };
struct LabNode { std::string id, parent, label, color, edge; int depth = 0; };
struct LabFrame { DynamicArray<long> values; long i = -1, j = -1; std::string action; };

struct LabOutput {
    std::string structure, operation;
    DynamicArray<LabNode> nodes;               // tree / graph snapshot
    DynamicArray<LabFrame> frames;             // array animation frames
    DynamicArray<long> result;                 // final sequence (sorted array, traversal ...)
    DynamicArray<std::string> log;             // human-readable steps
    DynamicArray<LabStat> stats;               // measured
    DynamicArray<LabStat> complexity;          // theoretical
    DynamicArray<DynamicArray<std::string>> table;   // tabular snapshots (hash slots, matrix, suffix array)
};

struct LabOp { std::string op; long value = 0; long position = -1; };
struct LabEdge { long u = 0, v = 0; double w = 1; };
struct BenchmarkRow { std::string variant; long n = 0; double microseconds = 0; double comparisons = 0; double operations = 0; double memoryBytes = 0; std::string note; };
struct CatalogEntry { std::string name, group, application, file; DynamicArray<LabStat> complexity; };

class DsaLabService {
public:
    LabOutput linear(const std::string& structure, const DynamicArray<LabOp>& ops);
    LabOutput sort(const std::string& algorithm, const DynamicArray<long>& values);
    LabOutput search(const std::string& algorithm, const DynamicArray<long>& values, long target);
    LabOutput tree(const std::string& type, const DynamicArray<long>& inserts, const DynamicArray<long>& deletes, long searchKey, int order);
    LabOutput heap(const std::string& mode, const DynamicArray<long>& values, int extractCount);
    LabOutput hash(const std::string& strategy, const DynamicArray<long>& keys, int capacity, const DynamicArray<long>& deletes);
    LabOutput graph(int vertices, const DynamicArray<LabEdge>& edges, const std::string& algorithm, long source, long target);
    LabOutput spatial(const std::string& structure, int points, unsigned seed, double qLat, double qLon, double radiusKm, int k);
    LabOutput text(const std::string& structure, const std::string& corpus, const std::string& pattern);
    LabOutput intervals(const DynamicArray<long>& flatPairs, long qFrom, long qTo);
    LabOutput segment(const DynamicArray<long>& values, long l, long r, long addL, long addR, long addValue);
    LabOutput backtracking(const std::string& problem, int n, const DynamicArray<long>& grid, int rows, int cols);
    LabOutput sparse(const DynamicArray<long>& triplets, long rows, long cols);
    LabOutput multilist(const DynamicArray<long>& pairs);

    DynamicArray<BenchmarkRow> benchmark(const std::string& suite, long n, int repeats);
    DynamicArray<CatalogEntry> catalog();
};

} // namespace nexa::svc
