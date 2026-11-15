#pragma once

// G3 routing flood, G4 receivers and G5 accumulation (spec P1-00 sections
// 7.3 to 7.5, rules D6, D1 and D4). Worked example: P1-02 section 5.1.

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "worldgen/geo/grid.hpp"

namespace worldgen::geo {

// --- G3: routing flood (priority flood), P1-00 7.3 --------------------------------

// The result of the routing flood (P1-00 7.3).
struct RoutingFlood {
    // Flood level L per cell: 0 on ocean; for every other cell the minimum,
    // over all paths from it to an ocean cell, of the maximum key on the
    // path (ocean counting as 0), so L[i] >= keys[i] there.
    std::vector<std::int64_t> level;
    // Flood parent per cell: the D8 code from the cell to the cell that
    // pushed it, (code + 4) mod 8 when pushed from c through `code`;
    // NO_DIRECTION (8) on ocean.
    std::vector<Direction> parent;
    // Every cell, in pop order: ascending (key, seq), seq the global push
    // counter. Ocean cells first, in index order (they are all pushed with
    // key 0 before anything else).
    std::vector<CellIndex> order;
};

// Reusable working storage for routing_flood, so that a caller running the
// flood many times (erosion, P1-01 E8) does not reallocate. It holds only
// capacity: its contents never affect a result. Not thread-safe; use one
// per thread.
class RoutingFloodScratch {
public:
    RoutingFloodScratch();
    ~RoutingFloodScratch();
    RoutingFloodScratch(RoutingFloodScratch&&) noexcept;
    RoutingFloodScratch& operator=(RoutingFloodScratch&&) noexcept;
    RoutingFloodScratch(const RoutingFloodScratch&) = delete;
    RoutingFloodScratch& operator=(const RoutingFloodScratch&) = delete;

    struct Impl;

private:
    friend void routing_flood(Grid, std::span<const std::int64_t>, std::span<const std::uint8_t>, RoutingFlood&,
                              RoutingFloodScratch&);
    std::unique_ptr<Impl> impl_;
};

// G3 (P1-00 7.3) on keys h (int64 per cell: heights above sea level in the
// stage's unit, decimetres or Fx raw) and an ocean mask (ocean[i] != 0):
//
//     seq = 0; Q = empty
//     for i in 0 .. n-1 (index order):
//         if ocean[i]: L[i] = 0; parent[i] = 8; visited[i] = true; push (0, seq, i); seq += 1
//     while Q not empty:
//         pop the entry (k, s, c) with the smallest (k, s); append c to order
//         for code in 0 .. 7:
//             m = neighbour of c in direction code; skip if outside the map or visited[m]
//             visited[m] = true; L[m] = max(h[m], L[c]); parent[m] = (code + 4) mod 8
//             push (L[m], seq, m); seq += 1
//
// Keys of ocean cells are ignored. Fatal (FatalError) if the grid or an
// array length is invalid, no cell is ocean, or a non-ocean key is below 1.
//
// Implementation: a radix heap on the key with FIFO buckets, which pops in
// exactly ascending (key, seq) order (every bucket holds its entries in push
// order); see src/geo/flow.cpp.
[[nodiscard]] RoutingFlood routing_flood(Grid grid, std::span<const std::int64_t> keys,
                                         std::span<const std::uint8_t> ocean);

// As above, writing into `out` (its vectors are resized) and using
// `scratch` for the queue. Same result.
void routing_flood(Grid grid, std::span<const std::int64_t> keys, std::span<const std::uint8_t> ocean,
                   RoutingFlood& out, RoutingFloodScratch& scratch);

// --- G4: receivers, P1-00 7.4 ---------------------------------------------------------

// G4 (P1-00 7.4): rcv[c] for every cell. Ocean cells get NO_DIRECTION (8).
// For a non-ocean cell c, among its existing neighbours m in the
// 8-neighbour order with level[m] < level[c], the one with the steepest
// slope: with drop d = level[c] - level[m], score w * d^2 with w = 2 for an
// orthogonal and 1 for a diagonal neighbour, compared exactly (in U128); a
// candidate replaces the best only if its score is strictly greater, so ties
// go to the lowest code. With integer drops an exact tie between an
// orthogonal and a diagonal neighbour cannot occur (2 d_o^2 = d_d^2 has no
// solution in positive integers, since sqrt(2) is irrational), so the
// lowest-code rule only decides among neighbours of the same kind. If no neighbour has a strictly smaller level,
// rcv[c] = parent[c]. A per-cell function (rule D1), computed in G6 row
// blocks on `threads` threads; the result does not depend on threads.
// Fatal if the grid or an array length is invalid, threads is outside
// [1, MAX_THREADS], a level is negative, or a non-ocean cell needing its
// parent has parent NO_DIRECTION.
[[nodiscard]] std::vector<Direction> receivers(Grid grid, std::span<const std::int64_t> level,
                                               std::span<const Direction> parent, std::span<const std::uint8_t> ocean,
                                               std::uint32_t threads = 1);

// As above, writing into rcv (grid.cells() entries).
void receivers(Grid grid, std::span<const std::int64_t> level, std::span<const Direction> parent,
               std::span<const std::uint8_t> ocean, std::span<Direction> rcv, std::uint32_t threads = 1);

// --- G5: accumulation, P1-00 7.5 ------------------------------------------------------

// G5 (P1-00 7.5) with G3's order: A = 1 on non-ocean cells, 0 on ocean;
// then for c in reverse `order`, if c is not ocean and its receiver m (the
// neighbour in direction rcv[c]) is not ocean, A[m] += A[c]. A[c] is the
// number of non-ocean cells whose downstream path passes through c, c
// included. Fatal if the grid or an array length is invalid, `order` is not
// a permutation of the cells, a non-ocean cell's receiver is not a code
// 0..7 pointing inside the grid, or `order` does not list every non-ocean
// receiver before its donors (then the loop above would not compute A).
[[nodiscard]] std::vector<std::uint32_t> accumulate(Grid grid, std::span<const Direction> rcv,
                                                    std::span<const std::uint8_t> ocean,
                                                    std::span<const CellIndex> order);

// G5 in another topological order, for receivers changed after G4 (P1-02
// H4 step 2, H5): Kahn's method. indeg[c] = number of non-ocean cells whose
// receiver is c; a FIFO queue starts with every non-ocean cell of indeg 0
// in index order; popping u, if its receiver m is not ocean: A[m] += A[u],
// indeg[m] -= 1, and m is pushed when indeg[m] reaches 0. Same values as
// the definition (any topological order gives them). Fatal as accumulate,
// and if the receivers of non-ocean cells contain a cycle.
[[nodiscard]] std::vector<std::uint32_t> accumulate_topological(Grid grid, std::span<const Direction> rcv,
                                                                std::span<const std::uint8_t> ocean);

}  // namespace worldgen::geo
