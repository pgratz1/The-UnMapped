#pragma once

// G6 row blocks and parallel per-block work (spec P1-00 sections 7.6 and 8,
// rules D1, D2, D8, D10).
//
// Parallel per-cell work is split into blocks of 64 rows: block b covers
// rows 64b to min(64b + 64, H) - 1. The split depends only on the grid
// height, never on the thread count. for_each_row_block and
// map_row_blocks run a per-block function on a caller-supplied number of
// threads through worldgen/core/parallel.hpp; the thread count never changes
// a result.

#include <cstdint>
#include <utility>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/core/parallel.hpp"

namespace worldgen::geo {

// Rows per G6 block (P1-00 7.6).
inline constexpr std::int32_t ROW_BLOCK_ROWS = 64;

// One G6 block: rows y_begin to y_end - 1 (P1-00 7.6).
struct RowBlock {
    std::uint32_t index = 0;   // b
    std::int32_t y_begin = 0;  // 64b
    std::int32_t y_end = 0;    // min(64b + 64, height), exclusive

    friend constexpr bool operator==(const RowBlock&, const RowBlock&) = default;
};

// Number of G6 blocks of a grid of `height` rows: ceil(height / 64). 0 for
// height 0. Fatal if height < 0.
[[nodiscard]] std::uint32_t row_block_count(std::int32_t height);

// Block b of a grid of `height` rows (P1-00 7.6). Fatal unless
// b < row_block_count(height).
[[nodiscard]] RowBlock row_block(std::int32_t height, std::uint32_t b);

// Calls fn(row_block(height, b)) for every block b on up to `threads`
// threads (run_blocks of worldgen/core/parallel.hpp: inline in ascending
// order with one thread; the lowest failing block's exception is rethrown;
// fatal if threads is 0 or above MAX_THREADS). For D1 and D2 steps: fn
// writes only the cells of its own rows and reads only inputs fixed before
// the call.
template <typename Fn>
void for_each_row_block(std::int32_t height, std::uint32_t threads, Fn&& fn) {
    const std::uint32_t count = row_block_count(height);
    run_blocks(count, threads, [height, &fn](std::uint32_t b) { fn(row_block(height, b)); });
}

// result[b] = fn(row_block(height, b)) for every block, computed as
// for_each_row_block does (P1-00 D8). The caller combines the results in
// ascending block order. R as for map_blocks (not bool).
template <typename R, typename Fn>
[[nodiscard]] std::vector<R> map_row_blocks(std::int32_t height, std::uint32_t threads, Fn&& fn) {
    const std::uint32_t count = row_block_count(height);
    return map_blocks<R>(count, threads, [height, &fn](std::uint32_t b) { return fn(row_block(height, b)); });
}

}  // namespace worldgen::geo
