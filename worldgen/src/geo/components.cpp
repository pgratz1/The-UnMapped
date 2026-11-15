// G2: connected components and FIFO breadth-first search (P1-00 7.2).

#include "worldgen/geo/components.hpp"

#include <cstddef>

#include "worldgen/core/error.hpp"

namespace worldgen::geo {

std::uint32_t Components::size(std::uint32_t k) const {
    if (k < 1U || k > count) {
        fatal("Components::size: label out of range");
    }
    return start[k] - start[k - 1U];
}

namespace {

// FIFO BFS from `seed`, appending to `order` (which is the queue: entries
// from `head` on are still to be expanded). `joins(c, m)` says whether
// neighbour m belongs with c and is not yet reached; reaching m marks it.
template <typename Joins>
void bfs(Grid grid, CellIndex seed, Connectivity connectivity, std::span<Direction> parent,
         std::vector<CellIndex>& order, Joins&& joins) {
    const std::span<const Direction> codes = neighbour_order(connectivity);
    std::size_t head = order.size();
    order.push_back(seed);
    while (head < order.size()) {
        const CellIndex c = order[head];
        ++head;
        const std::int32_t x = grid.x_of(c);
        const std::int32_t y = grid.y_of(c);
        for (const Direction code : codes) {
            const std::int32_t mx = x + D8_DX[code];
            const std::int32_t my = y + D8_DY[code];
            if (!grid.contains(mx, my)) {
                continue;
            }
            const CellIndex m = grid.index(mx, my);
            if (!joins(c, m)) {
                continue;
            }
            parent[m] = opposite(code);
            order.push_back(m);
        }
    }
}

}  // namespace

Components label_components(Grid grid, std::span<const std::uint8_t> mask, Connectivity connectivity,
                            std::span<const std::int64_t> equal_key) {
    check_grid_array(grid, mask.size(), "label_components: mask");
    if (!equal_key.empty()) {
        check_grid_array(grid, equal_key.size(), "label_components: equal_key");
    }
    const std::uint32_t n = grid.cells();
    Components out;
    out.label.assign(n, 0U);
    out.parent.assign(n, NO_DIRECTION);
    out.order.reserve(n);
    out.start.push_back(0U);
    const bool keyed = !equal_key.empty();
    for (CellIndex i = 0; i < n; ++i) {
        if (mask[i] == 0U || out.label[i] != 0U) {
            continue;
        }
        const std::uint32_t k = ++out.count;
        out.label[i] = k;
        bfs(grid, i, connectivity, out.parent, out.order, [&](CellIndex c, CellIndex m) {
            if (mask[m] == 0U || out.label[m] != 0U || (keyed && equal_key[m] != equal_key[c])) {
                return false;
            }
            out.label[m] = k;
            return true;
        });
        out.start.push_back(static_cast<std::uint32_t>(out.order.size()));
    }
    return out;
}

void bfs_region(Grid grid, std::span<const std::uint32_t> region, CellIndex seed, Connectivity connectivity,
                std::span<Direction> parent, std::span<std::uint8_t> visited, std::vector<CellIndex>& order) {
    check_grid_array(grid, region.size(), "bfs_region: region");
    check_grid_array(grid, parent.size(), "bfs_region: parent");
    check_grid_array(grid, visited.size(), "bfs_region: visited");
    if (seed >= grid.cells()) {
        fatal("bfs_region: seed outside the grid");
    }
    if (visited[seed] != 0U) {
        fatal("bfs_region: seed already visited");
    }
    const std::uint32_t r = region[seed];
    visited[seed] = 1U;
    bfs(grid, seed, connectivity, parent, order, [&](CellIndex, CellIndex m) {
        if (region[m] != r || visited[m] != 0U) {
            return false;
        }
        visited[m] = 1U;
        return true;
    });
}

}  // namespace worldgen::geo
