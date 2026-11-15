#pragma once

// Grids, D8 directions and neighbourhoods shared by every geography stage
// (spec P1-00 sections 2.1 and 2.3).
//
// Every shared algorithm takes plain arrays of width * height cells with an
// explicit Grid, so it runs on any W x H grid (the specs' unit tests use
// small non-square grids), not only the N x N map. Cell (x, y) has index
// i = y * width + x (P1-00 2.1, P0-02 5.6); x is the column (0 at the west
// edge), y the row (0 at the north edge).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace worldgen::geo {

// Cell size in metres (P1-00 2.1): a constant of the code, not tuning.
inline constexpr std::int64_t CELL_M = 16;

// Largest number of cells a Grid may have. Indices are stored as uint32 and
// accumulations as uint32 (P1-00 5.2 needs N^2 <= 2^22; this leaves room
// for test grids). 2^24 = 4096 x 4096.
inline constexpr std::uint64_t MAX_GRID_CELLS = std::uint64_t{1} << 24U;

// A cell index, y * width + x (P1-00 2.1).
using CellIndex = std::uint32_t;

// The shape of a grid. Every function taking a Grid raises FatalError
// (P0-01 4.1) unless width >= 1, height >= 1 and width * height <=
// MAX_GRID_CELLS, and every array argument must have exactly cells()
// elements (also fatal otherwise).
struct Grid {
    std::int32_t width = 0;
    std::int32_t height = 0;

    // width * height (valid grids only).
    [[nodiscard]] constexpr std::uint32_t cells() const noexcept {
        return static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height);
    }
    // y * width + x for 0 <= x < width, 0 <= y < height.
    [[nodiscard]] constexpr CellIndex index(std::int32_t x, std::int32_t y) const noexcept {
        return static_cast<CellIndex>(y) * static_cast<CellIndex>(width) + static_cast<CellIndex>(x);
    }
    [[nodiscard]] constexpr std::int32_t x_of(CellIndex i) const noexcept {
        return static_cast<std::int32_t>(i % static_cast<CellIndex>(width));
    }
    [[nodiscard]] constexpr std::int32_t y_of(CellIndex i) const noexcept {
        return static_cast<std::int32_t>(i / static_cast<CellIndex>(width));
    }
    [[nodiscard]] constexpr bool contains(std::int32_t x, std::int32_t y) const noexcept {
        return x >= 0 && y >= 0 && x < width && y < height;
    }

    friend constexpr bool operator==(const Grid&, const Grid&) = default;
};

// The square N x N map grid.
[[nodiscard]] constexpr Grid square_grid(std::int32_t n) noexcept {
    return Grid{n, n};
}

// FatalError unless the grid is valid (see Grid).
void check_grid(Grid grid);
// FatalError unless the grid is valid and size == grid.cells(); `what`
// names the caller and argument in the message.
void check_grid_array(Grid grid, std::size_t size, const char* what);

// --- D8 directions (P1-00 2.3) --------------------------------------------------

// A D8 direction code, 0 to 7 (P1-00 2.3), or NO_DIRECTION.
using Direction = std::uint8_t;

// "No downstream" / "no parent" (P1-00 4: flow_dir 8 on ocean; G3 parent 8
// on ocean; G2 parent 8 on component roots and cells outside the predicate).
inline constexpr Direction NO_DIRECTION = 8;

// Code:  0  1   2  3   4  5   6  7
// Name:  E  SE  S  SW  W  NW  N  NE
// dx:    1  1   0  -1  -1 -1  0  1
// dy:    0  1   1  1   0  -1  -1 -1      (y grows to the south)
inline constexpr std::array<std::int32_t, 8> D8_DX = {1, 1, 0, -1, -1, -1, 0, 1};
inline constexpr std::array<std::int32_t, 8> D8_DY = {0, 1, 1, 1, 0, -1, -1, -1};

// The 8-neighbour order: codes 0, 1, ..., 7 (P1-00 2.3).
inline constexpr std::array<Direction, 8> NEIGHBOURS_8 = {0, 1, 2, 3, 4, 5, 6, 7};
// The 4-neighbour order: codes 0, 2, 4, 6 = E, S, W, N (P1-00 2.3).
inline constexpr std::array<Direction, 4> NEIGHBOURS_4 = {0, 2, 4, 6};

// The opposite direction, (code + 4) mod 8 (P1-00 2.3). code must be 0..7.
[[nodiscard]] constexpr Direction opposite(Direction code) noexcept {
    return static_cast<Direction>((code + 4U) & 7U);
}
// True for the odd (diagonal, 16 * sqrt(2) m) codes (P1-00 2.3).
[[nodiscard]] constexpr bool is_diagonal(Direction code) noexcept {
    return (code & 1U) != 0U;
}

// Connectivity of a neighbourhood (P1-00 2.3): land components are
// 8-connected, ocean 4-connected.
enum class Connectivity : std::uint8_t {
    four = 4,   // NEIGHBOURS_4
    eight = 8,  // NEIGHBOURS_8
};

// The neighbour codes of a connectivity, in the order of P1-00 2.3.
[[nodiscard]] constexpr std::span<const Direction> neighbour_order(Connectivity c) noexcept {
    return c == Connectivity::four ? std::span<const Direction>(NEIGHBOURS_4) : std::span<const Direction>(NEIGHBOURS_8);
}

// The neighbour of cell i in direction code, if it is inside the grid: a
// neighbour outside the map does not exist (never wrapped or clamped,
// P1-00 2.3). Returns true and sets out when it exists. code must be 0..7.
[[nodiscard]] constexpr bool neighbour(Grid grid, CellIndex i, Direction code, CellIndex& out) noexcept {
    const std::int32_t x = grid.x_of(i) + D8_DX[code];
    const std::int32_t y = grid.y_of(i) + D8_DY[code];
    if (!grid.contains(x, y)) {
        return false;
    }
    out = grid.index(x, y);
    return true;
}

}  // namespace worldgen::geo
