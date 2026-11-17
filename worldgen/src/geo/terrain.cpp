// G7 height above drainage and G8 cell slope (P1-00 7.7, 7.8).

#include "worldgen/geo/terrain.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>

#include "worldgen/core/error.hpp"
#include "worldgen/core/wide.hpp"

namespace worldgen::geo {

namespace {

[[noreturn]] void fail(std::string_view prefix, std::string_view what) {
    std::string message(prefix);
    message += what;
    throw FatalError(message);
}

// Decimal text of a non-negative coordinate (no locale, no C library).
std::string coordinate_text(std::int32_t v) {
    std::string digits;
    std::uint32_t u = static_cast<std::uint32_t>(v);
    do {
        digits.insert(digits.begin(), static_cast<char>('0' + u % 10U));
        u /= 10U;
    } while (u != 0U);
    return digits;
}

std::string cell_text(Grid grid, CellIndex i) {
    return "cell (" + coordinate_text(grid.x_of(i)) + ", " + coordinate_text(grid.y_of(i)) + ")";
}

constexpr std::uint32_t UNREACHED = std::numeric_limits<std::uint32_t>::max();

}  // namespace

std::vector<std::int64_t> water_surface(Grid grid, std::span<const std::int16_t> elevation,
                                        std::span<const std::uint16_t> water_depth) {
    check_grid_array(grid, elevation.size(), "water_surface: elevation");
    check_grid_array(grid, water_depth.size(), "water_surface: water_depth");
    std::vector<std::int64_t> s(grid.cells(), 0);
    for (std::size_t i = 0; i < s.size(); ++i) {
        s[i] = std::int64_t{elevation[i]} + std::int64_t{water_depth[i]};
    }
    return s;
}

std::vector<std::int64_t> height_above_drainage(Grid grid, std::span<const std::int64_t> surface,
                                                std::span<const std::uint8_t> water,
                                                std::span<const std::uint8_t> river,
                                                std::span<const std::uint8_t> flow_dir, std::int64_t k) {
    return height_above_drainage(grid, surface, water, river, flow_dir, k, "G7: ");
}

std::vector<std::int64_t> height_above_drainage(Grid grid, std::span<const std::int64_t> surface,
                                                std::span<const std::uint8_t> water,
                                                std::span<const std::uint8_t> river,
                                                std::span<const std::uint8_t> flow_dir, std::int64_t k,
                                                std::string_view message_prefix) {
    check_grid(grid);
    if (k < 1 || k > RIVER_CLASS_MAX) {
        fail(message_prefix, "the minimum river class is outside [1, 15]");
    }
    const std::uint32_t n = grid.cells();
    if (surface.size() != n || water.size() != n || river.size() != n || flow_dir.size() != n) {
        fail(message_prefix, "an input array does not have one value per cell");
    }
    // drain[c]: the drain cell c drains to, UNREACHED until it is known.
    std::vector<std::uint32_t> drain(n, UNREACHED);
    std::vector<CellIndex> queue;
    queue.reserve(n);
    for (CellIndex i = 0; i < n; ++i) {
        if (is_drain_cell(water[i], river[i], k)) {
            drain[i] = i;
            queue.push_back(i);
        }
    }
    // FIFO in push order (P1-00 7.7, D5): the queue vector is read from the
    // front while cells are appended at the back.
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const CellIndex c = queue[head];
        for (const Direction code : NEIGHBOURS_8) {
            CellIndex m = 0;
            if (!neighbour(grid, c, code, m)) {
                continue;
            }
            // m drains to c when its flow_dir points back along code.
            if (drain[m] != UNREACHED || flow_dir[m] != opposite(code)) {
                continue;
            }
            drain[m] = drain[c];
            queue.push_back(m);
        }
    }
    std::vector<std::int64_t> hand(n, 0);
    for (CellIndex i = 0; i < n; ++i) {
        if (drain[i] == UNREACHED) {
            fail(message_prefix, cell_text(grid, i) + " does not reach a drain cell");
        }
        const std::int64_t h = surface[i] - surface[drain[i]];
        if (h < 0) {
            fail(message_prefix, cell_text(grid, i) + " has a negative height above drainage");
        }
        hand[i] = h;
    }
    return hand;
}

std::int64_t cell_slope_s2(Grid grid, std::span<const std::int16_t> elevation, std::int32_t x, std::int32_t y) {
    check_grid_array(grid, elevation.size(), "cell_slope_s2: elevation");
    if (!grid.contains(x, y)) {
        fatal("cell_slope_s2: cell outside the grid");
    }
    const auto e = [&](std::int32_t cx, std::int32_t cy) {
        const std::int64_t v = elevation[grid.index(cx, cy)];
        return std::max<std::int64_t>(v, 0);
    };
    const std::int32_t xa = std::max(x - 1, 0);
    const std::int32_t xb = std::min(x + 1, grid.width - 1);
    const std::int32_t ya = std::max(y - 1, 0);
    const std::int32_t yb = std::min(y + 1, grid.height - 1);
    const std::int64_t gx = xb > xa ? (e(xb, y) - e(xa, y)) * (2 / (xb - xa)) : 0;
    const std::int64_t gy = yb > ya ? (e(x, yb) - e(x, ya)) * (2 / (yb - ya)) : 0;
    return gx * gx + gy * gy;
}

std::vector<std::int64_t> slope_s2(Grid grid, std::span<const std::int16_t> elevation) {
    check_grid_array(grid, elevation.size(), "slope_s2: elevation");
    std::vector<std::int64_t> out(grid.cells(), 0);
    for (std::int32_t y = 0; y < grid.height; ++y) {
        for (std::int32_t x = 0; x < grid.width; ++x) {
            out[grid.index(x, y)] = cell_slope_s2(grid, elevation, x, y);
        }
    }
    return out;
}

std::int64_t slope_s2_limit(Fx t) {
    if (t.raw() < 0) {
        fatal("slope_s2_limit: negative tangent");
    }
    const auto raw = static_cast<std::uint64_t>(t.raw());
    if (raw > std::numeric_limits<std::uint64_t>::max() / 320U) {
        fatal("slope_s2_limit: tangent too large");
    }
    const std::uint64_t scaled = 320U * raw;
    // S2 2^64 <= X  <=>  S2 <= floor(X / 2^64) = X.hi, for integer S2 >= 0.
    const U128 sq = mul_u64_wide(scaled, scaled);
    if (sq.hi > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return static_cast<std::int64_t>(sq.hi);
}

bool slope_not_steeper(std::int64_t s2, Fx t) {
    if (s2 < 0) {
        fatal("slope_not_steeper: negative S2");
    }
    return s2 <= slope_s2_limit(t);
}

}  // namespace worldgen::geo
