#pragma once

// G2: connected components, and the FIFO breadth-first search they are
// built from (spec P1-00 section 7.2, rule D5).

#include <cstdint>
#include <span>
#include <vector>

#include "worldgen/geo/grid.hpp"

namespace worldgen::geo {

// The result of label_components (P1-00 7.2).
struct Components {
    // Per cell: 0 if the cell is not in P, else its component label 1 .. count.
    // Labels are in order of each component's minimum index.
    std::vector<std::uint32_t> label;
    // Per cell: for a P-cell reached by the search from another cell, the D8
    // code pointing from the cell back to the cell it was reached from (the
    // BFS parent: reached from c through code k gives (k + 4) mod 8);
    // NO_DIRECTION (8) for each component's first cell and for cells not in P.
    std::vector<Direction> parent;
    // Every P-cell in visit order: component 1 in its BFS order (starting
    // with its minimum-index cell), then component 2, and so on.
    std::vector<CellIndex> order;
    // start[k - 1] is the offset in `order` of component k's first cell;
    // start[count] = order.size(). So component k's cells are
    // order[start[k - 1] .. start[k] - 1] and its size is start[k] - start[k - 1].
    std::vector<std::uint32_t> start;
    // Number of components.
    std::uint32_t count = 0;

    // Size of component k (1 <= k <= count). Fatal otherwise.
    [[nodiscard]] std::uint32_t size(std::uint32_t k) const;
};

// G2 (P1-00 7.2). Input: the predicate P as a mask (P(i) is mask[i] != 0),
// a connectivity, and optionally an equivalence E on adjacent cells given by
// keys: when `equal_key` is not empty, adjacent cells u, v belong together
// only if equal_key[u] == equal_key[v] (E10 of P1-01 and H3 of P1-02 use
// the flood levels L as the keys); when it is empty, E is always true.
//
// A component is a maximal set of P-cells connected by steps between
// adjacent P-cells u, v with E(u, v). Cells are scanned in index order; each
// P-cell not yet labelled starts a new component with the next label (1, 2,
// ...), whose cells are collected by FIFO breadth-first search from it,
// visiting neighbours in the 4- or 8-neighbour order of P1-00 2.3. So labels
// follow each component's minimum index and the BFS visit order and parents
// are unique. Fatal (FatalError) if the grid is invalid or an array has the
// wrong length (equal_key may also be empty).
[[nodiscard]] Components label_components(Grid grid, std::span<const std::uint8_t> mask, Connectivity connectivity,
                                          std::span<const std::int64_t> equal_key = {});

// The FIFO breadth-first search of G2 from one seed cell, over the cells
// whose `region` value equals region[seed] (P1-00 7.2, rule D5; P1-02 H4
// step 2 runs it over one lake from its outlet). Neighbours are visited in
// the 4- or 8-neighbour order; a cell is reached at most once. Appends the
// reached cells, seed first, to `order`, and for each reached cell other
// than the seed sets parent[cell] to the code pointing back to the cell it
// was reached from ((k + 4) mod 8 when reached through code k); parent[seed]
// is not changed. `visited` is caller-owned scratch of grid.cells() bytes
// whose entries for the region's cells must be 0 on entry; they are set to
// 1 for every reached cell. Fatal if the grid or an array size is invalid,
// seed is outside the grid, or visited[seed] is already set.
void bfs_region(Grid grid, std::span<const std::uint32_t> region, CellIndex seed, Connectivity connectivity,
                std::span<Direction> parent, std::span<std::uint8_t> visited, std::vector<CellIndex>& order);

}  // namespace worldgen::geo
