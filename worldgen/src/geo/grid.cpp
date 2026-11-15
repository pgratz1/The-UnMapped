// Grid checks (P1-00 2.1) and G6 row blocks (P1-00 7.6).

#include <string>

#include "worldgen/core/error.hpp"
#include "worldgen/geo/blocks.hpp"
#include "worldgen/geo/grid.hpp"

namespace worldgen::geo {

void check_grid(Grid grid) {
    if (grid.width < 1 || grid.height < 1) {
        fatal("grid: width and height must be at least 1");
    }
    const std::uint64_t cells = static_cast<std::uint64_t>(grid.width) * static_cast<std::uint64_t>(grid.height);
    if (cells > MAX_GRID_CELLS) {
        fatal("grid: more than 2^24 cells");
    }
}

void check_grid_array(Grid grid, std::size_t size, const char* what) {
    check_grid(grid);
    if (static_cast<std::uint64_t>(size) != static_cast<std::uint64_t>(grid.cells())) {
        const std::string message = std::string(what) + ": array length is not width * height";
        fatal(message.c_str());
    }
}

std::uint32_t row_block_count(std::int32_t height) {
    if (height < 0) {
        fatal("row_block_count: negative height");
    }
    return static_cast<std::uint32_t>((std::int64_t{height} + (ROW_BLOCK_ROWS - 1)) / ROW_BLOCK_ROWS);
}

RowBlock row_block(std::int32_t height, std::uint32_t b) {
    if (b >= row_block_count(height)) {
        fatal("row_block: block index out of range");
    }
    RowBlock out;
    out.index = b;
    out.y_begin = static_cast<std::int32_t>(b) * ROW_BLOCK_ROWS;
    out.y_end = out.y_begin + ROW_BLOCK_ROWS < height ? out.y_begin + ROW_BLOCK_ROWS : height;
    return out;
}

}  // namespace worldgen::geo
