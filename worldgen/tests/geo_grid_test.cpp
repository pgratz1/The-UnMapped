// Grids, D8 neighbourhood tables and G6 row blocks (spec P1-00 sections 2.1,
// 2.3 and 7.6).
//
// Written from docs/specs/p1-00-geography-overview.md and the public headers
// worldgen/geo/grid.hpp and worldgen/geo/blocks.hpp only.

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

#include "core_test_util.hpp"
#include "worldgen/geo/blocks.hpp"
#include "worldgen/geo/grid.hpp"

namespace geo = worldgen::geo;

namespace {

// P1-00 2.3, written out from the table.
struct D8Row {
    int code;
    const char* name;
    std::int32_t dx;
    std::int32_t dy;
};
constexpr std::array<D8Row, 8> D8_TABLE = {{
    {0, "E", 1, 0},
    {1, "SE", 1, 1},
    {2, "S", 0, 1},
    {3, "SW", -1, 1},
    {4, "W", -1, 0},
    {5, "NW", -1, -1},
    {6, "N", 0, -1},
    {7, "NE", 1, -1},
}};

}  // namespace

TEST_CASE("P1-00 2.3: D8 codes, offsets and names") {
    for (const D8Row& row : D8_TABLE) {
        CAPTURE(row.name);
        const auto code = static_cast<std::size_t>(row.code);
        CHECK(geo::D8_DX[code] == row.dx);
        CHECK(geo::D8_DY[code] == row.dy);
    }
    CHECK(geo::NO_DIRECTION == 8);
    CHECK(geo::CELL_M == 16);
}

TEST_CASE("P1-00 2.3: opposite is (k + 4) mod 8 and reverses the offset") {
    for (int k = 0; k < 8; ++k) {
        CAPTURE(k);
        const auto code = static_cast<geo::Direction>(k);
        const geo::Direction o = geo::opposite(code);
        CHECK(static_cast<int>(o) == (k + 4) % 8);
        CHECK(geo::D8_DX[o] == -geo::D8_DX[code]);
        CHECK(geo::D8_DY[o] == -geo::D8_DY[code]);
        CHECK(geo::opposite(o) == code);
    }
}

TEST_CASE("P1-00 2.3: odd codes are diagonal, even codes orthogonal") {
    for (int k = 0; k < 8; ++k) {
        CAPTURE(k);
        const auto code = static_cast<geo::Direction>(k);
        const bool diagonal = geo::D8_DX[code] != 0 && geo::D8_DY[code] != 0;
        CHECK(geo::is_diagonal(code) == diagonal);
        CHECK(geo::is_diagonal(code) == (k % 2 == 1));
    }
}

TEST_CASE("P1-00 2.3: the 8-neighbour and 4-neighbour orders") {
    const std::array<geo::Direction, 8> eight = {0, 1, 2, 3, 4, 5, 6, 7};
    const std::array<geo::Direction, 4> four = {0, 2, 4, 6};  // E, S, W, N
    CHECK(geo::NEIGHBOURS_8 == eight);
    CHECK(geo::NEIGHBOURS_4 == four);
    const auto o8 = geo::neighbour_order(geo::Connectivity::eight);
    const auto o4 = geo::neighbour_order(geo::Connectivity::four);
    REQUIRE(o8.size() == 8U);
    REQUIRE(o4.size() == 4U);
    for (std::size_t i = 0; i < 8; ++i) {
        CHECK(o8[i] == eight[i]);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(o4[i] == four[i]);
    }
    CHECK(static_cast<int>(geo::Connectivity::four) == 4);
    CHECK(static_cast<int>(geo::Connectivity::eight) == 8);
}

TEST_CASE("P1-00 2.1: cell index i = y * W + x, on a non-square grid") {
    const geo::Grid g{7, 5};
    CHECK(g.cells() == 35U);
    for (std::int32_t y = 0; y < 5; ++y) {
        for (std::int32_t x = 0; x < 7; ++x) {
            const geo::CellIndex i = g.index(x, y);
            CHECK(i == static_cast<geo::CellIndex>(y * 7 + x));
            CHECK(g.x_of(i) == x);
            CHECK(g.y_of(i) == y);
            CHECK(g.contains(x, y));
        }
    }
    CHECK_FALSE(g.contains(-1, 0));
    CHECK_FALSE(g.contains(0, -1));
    CHECK_FALSE(g.contains(7, 0));
    CHECK_FALSE(g.contains(0, 5));
    CHECK(geo::square_grid(64) == geo::Grid{64, 64});
}

TEST_CASE("P1-00 2.3: a neighbour outside the map does not exist (no wrap, no clamp)") {
    // Every cell of several grids, including 1 x 1, 1 x N and N x 1 grids.
    for (const geo::Grid g : {geo::Grid{1, 1}, geo::Grid{1, 6}, geo::Grid{6, 1}, geo::Grid{2, 2}, geo::Grid{5, 3},
                              geo::Grid{3, 7}}) {
        CAPTURE(g.width);
        CAPTURE(g.height);
        for (geo::CellIndex i = 0; i < g.cells(); ++i) {
            const std::int32_t x = g.x_of(i);
            const std::int32_t y = g.y_of(i);
            std::set<geo::CellIndex> seen;
            for (int k = 0; k < 8; ++k) {
                const auto code = static_cast<geo::Direction>(k);
                const std::int32_t nx = x + D8_TABLE[static_cast<std::size_t>(k)].dx;
                const std::int32_t ny = y + D8_TABLE[static_cast<std::size_t>(k)].dy;
                const bool inside = nx >= 0 && ny >= 0 && nx < g.width && ny < g.height;
                geo::CellIndex out = 0xDEADBEEFU;
                const bool exists = geo::neighbour(g, i, code, out);
                CHECK(exists == inside);
                if (inside) {
                    CHECK(out == static_cast<geo::CellIndex>(ny * g.width + nx));
                    seen.insert(out);
                }
            }
            // The existing neighbours are distinct and never the cell itself.
            CHECK(seen.count(i) == 0U);
        }
    }
}

TEST_CASE("P1-00 2.3: neighbour counts at corners, edges and the interior") {
    const geo::Grid g{5, 4};
    const auto count = [&](std::int32_t x, std::int32_t y, geo::Connectivity c) {
        int n = 0;
        for (const geo::Direction code : geo::neighbour_order(c)) {
            geo::CellIndex out = 0;
            if (geo::neighbour(g, g.index(x, y), code, out)) {
                ++n;
            }
        }
        return n;
    };
    CHECK(count(0, 0, geo::Connectivity::eight) == 3);
    CHECK(count(4, 3, geo::Connectivity::eight) == 3);
    CHECK(count(2, 0, geo::Connectivity::eight) == 5);
    CHECK(count(0, 2, geo::Connectivity::eight) == 5);
    CHECK(count(2, 2, geo::Connectivity::eight) == 8);
    CHECK(count(0, 0, geo::Connectivity::four) == 2);
    CHECK(count(2, 0, geo::Connectivity::four) == 3);
    CHECK(count(2, 2, geo::Connectivity::four) == 4);
    // The east neighbour of the last cell of a row is not the first cell of
    // the next row.
    geo::CellIndex out = 0;
    CHECK_FALSE(geo::neighbour(g, g.index(4, 1), 0, out));
    CHECK_FALSE(geo::neighbour(g, g.index(0, 1), 4, out));
    CHECK_FALSE(geo::neighbour(g, g.index(4, 1), 1, out));
    CHECK_FALSE(geo::neighbour(g, g.index(0, 2), 3, out));
}

TEST_CASE("P1-00 2.1: grid validity checks") {
    geo::check_grid(geo::Grid{1, 1});
    geo::check_grid(geo::Grid{4096, 4096});  // exactly MAX_GRID_CELLS
    CHECK(geo::MAX_GRID_CELLS == (std::uint64_t{1} << 24U));
    CHECK_FATAL(geo::check_grid(geo::Grid{0, 5}));
    CHECK_FATAL(geo::check_grid(geo::Grid{5, 0}));
    CHECK_FATAL(geo::check_grid(geo::Grid{-1, 5}));
    CHECK_FATAL(geo::check_grid(geo::Grid{5, -3}));
    CHECK_FATAL(geo::check_grid(geo::Grid{4097, 4096}));
    CHECK_FATAL(geo::check_grid(geo::Grid{65536, 65536}));
    geo::check_grid_array(geo::Grid{3, 2}, 6U, "test");
    CHECK_FATAL(geo::check_grid_array(geo::Grid{3, 2}, 5U, "test"));
    CHECK_FATAL(geo::check_grid_array(geo::Grid{3, 2}, 7U, "test"));
    CHECK_FATAL(geo::check_grid_array(geo::Grid{0, 2}, 0U, "test"));
}

// --- G6 row blocks (P1-00 7.6) -------------------------------------------------

TEST_CASE("G6: block count is ceil(H / 64)") {
    CHECK(geo::ROW_BLOCK_ROWS == 64);
    CHECK(geo::row_block_count(0) == 0U);
    CHECK(geo::row_block_count(1) == 1U);
    CHECK(geo::row_block_count(63) == 1U);
    CHECK(geo::row_block_count(64) == 1U);
    CHECK(geo::row_block_count(65) == 2U);
    CHECK(geo::row_block_count(127) == 2U);
    CHECK(geo::row_block_count(128) == 2U);
    CHECK(geo::row_block_count(129) == 3U);
    CHECK(geo::row_block_count(512) == 8U);
    CHECK(geo::row_block_count(1024) == 16U);
    CHECK(geo::row_block_count(2048) == 32U);
    CHECK(geo::row_block_count(4096) == 64U);
    CHECK_FATAL(geo::row_block_count(-1));
}

TEST_CASE("G6: block b covers rows 64b to min(64b + 64, H) - 1") {
    for (const std::int32_t h : {1, 16, 63, 64, 65, 100, 128, 130, 191, 192, 193, 512, 1000, 2048}) {
        CAPTURE(h);
        const std::uint32_t n = geo::row_block_count(h);
        std::int32_t next_row = 0;
        for (std::uint32_t b = 0; b < n; ++b) {
            const geo::RowBlock blk = geo::row_block(h, b);
            CHECK(blk.index == b);
            CHECK(blk.y_begin == static_cast<std::int32_t>(64U * b));
            const std::int32_t end = static_cast<std::int32_t>(64U * b) + 64;
            CHECK(blk.y_end == (end < h ? end : h));
            CHECK(blk.y_begin == next_row);  // contiguous, no gap or overlap
            CHECK(blk.y_end > blk.y_begin);
            next_row = blk.y_end;
        }
        CHECK(next_row == h);
        CHECK_FATAL(geo::row_block(h, n));
        CHECK_FATAL(geo::row_block(h, n + 7U));
    }
    // Example: 130 rows = blocks [0, 64), [64, 128), [128, 130).
    CHECK(geo::row_block(130, 2) == geo::RowBlock{2U, 128, 130});
    CHECK_FATAL(geo::row_block(0, 0));
}

TEST_CASE("G6: the split depends only on the height, so a non-square grid splits by its rows") {
    // A 1000-wide, 70-high grid has two blocks; a 70-wide, 1000-high grid 16.
    CHECK(geo::row_block_count(geo::Grid{1000, 70}.height) == 2U);
    CHECK(geo::row_block_count(geo::Grid{70, 1000}.height) == 16U);
    CHECK(geo::row_block(70, 1) == geo::RowBlock{1U, 64, 70});
}
