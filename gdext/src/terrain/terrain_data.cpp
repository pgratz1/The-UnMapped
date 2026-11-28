#include "terrain/terrain_data.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

#include "worldgen/inspect/layer_catalogue.hpp"

namespace unmapped::terrain {

namespace {

// Grid coordinates u, v of (X, Z), clamped to [0, n - 1], and the lower
// corner i, j of the cell square holding them (6).
struct GridPoint {
    float u = 0.0f;
    float v = 0.0f;
    int i = 0;
    int j = 0;
};

GridPoint grid_point(const Layout& layout, float X, float Z) {
    const float half = static_cast<float>(layout.n / 2);
    const float top = static_cast<float>(layout.n - 1);
    GridPoint p;
    p.u = std::clamp(X / layout.spacing + half, 0.0f, top);
    p.v = std::clamp(Z / layout.spacing + half, 0.0f, top);
    p.i = std::min(static_cast<int>(std::floor(p.u)), layout.n - 2);
    p.j = std::min(static_cast<int>(std::floor(p.v)), layout.n - 2);
    return p;
}

std::size_t index(const Layout& layout, int x, int y) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(layout.n) + static_cast<std::size_t>(x);
}

}  // namespace

std::optional<Layout> layout_for(int width, int height, std::string& problem) {
    if (width != height || (width != 512 && width != 1024 && width != 2048)) {
        problem = "elevation is " + std::to_string(width) + "x" + std::to_string(height) +
                  "; the 3D preview needs a square map of 512, 1024 or 2048 cells.";
        return std::nullopt;
    }
    problem.clear();
    Layout layout;
    layout.n = width;
    layout.spacing = CELL_M;
    layout.region_size = REGION_SIZE;
    layout.region_min = -(width / 512);
    layout.region_count = width / REGION_SIZE;
    layout.origin = -8.0f * static_cast<float>(width);
    return layout;
}

std::vector<float> build_heights(const Layout& layout, std::span<const std::int16_t> elevation,
                                 std::span<const std::uint16_t> water_depth) {
    const std::size_t cells = static_cast<std::size_t>(layout.n) * static_cast<std::size_t>(layout.n);
    if (elevation.size() != cells) {
        throw std::invalid_argument("build_heights: elevation has " + std::to_string(elevation.size()) +
                                    " cells, expected " + std::to_string(cells));
    }
    if (!water_depth.empty() && water_depth.size() != cells) {
        throw std::invalid_argument("build_heights: water_depth has " + std::to_string(water_depth.size()) +
                                    " cells, expected " + std::to_string(cells));
    }
    std::vector<float> heights(cells);
    for (std::size_t k = 0; k < cells; ++k) {
        const std::int32_t e = elevation[k];
        const std::int32_t d = water_depth.empty() ? 0 : static_cast<std::int32_t>(water_depth[k]);
        const std::int32_t dm = (e > 0 && d > 0) ? e + d : e;  // a lake's surface, else land or sea bed
        heights[k] = static_cast<float>(dm) / 10.0f;
    }
    return heights;
}

std::vector<std::uint8_t> build_colours(const Layout& layout, std::span<const std::uint8_t> biome,
                                        std::span<const worldgen::inspect::BiomeRecord> records) {
    const std::size_t cells = static_cast<std::size_t>(layout.n) * static_cast<std::size_t>(layout.n);
    if (!biome.empty() && biome.size() != cells) {
        throw std::invalid_argument("build_colours: biome has " + std::to_string(biome.size()) +
                                    " cells, expected " + std::to_string(cells));
    }
    std::vector<std::uint8_t> out(cells * 4);
    if (biome.empty() || records.empty()) {
        for (std::size_t k = 0; k < cells; ++k) {
            out[4 * k + 0] = 128;
            out[4 * k + 1] = 128;
            out[4 * k + 2] = 128;
            out[4 * k + 3] = A_NEUTRAL;
        }
        return out;
    }
    // One RGB per code; magenta where there is no record (P1-00 10.2).
    std::uint8_t table[256][3];
    for (auto& rgb : table) {
        rgb[0] = 255;
        rgb[1] = 0;
        rgb[2] = 255;
    }
    for (const auto& record : records) {
        if (record.id >= 0 && record.id <= 255) {
            table[record.id][0] = static_cast<std::uint8_t>((record.colour >> 16) & 0xFF);
            table[record.id][1] = static_cast<std::uint8_t>((record.colour >> 8) & 0xFF);
            table[record.id][2] = static_cast<std::uint8_t>(record.colour & 0xFF);
        }
    }
    for (std::size_t k = 0; k < cells; ++k) {
        const std::uint8_t* rgb = table[biome[k]];
        out[4 * k + 0] = rgb[0];
        out[4 * k + 1] = rgb[1];
        out[4 * k + 2] = rgb[2];
        out[4 * k + 3] = A_NEUTRAL;
    }
    return out;
}

std::size_t unknown_biome_cells(std::span<const std::uint8_t> biome,
                                std::span<const worldgen::inspect::BiomeRecord> records) {
    if (biome.empty() || records.empty()) {
        return 0;
    }
    bool known[256] = {};
    for (const auto& record : records) {
        if (record.id >= 0 && record.id <= 255) {
            known[record.id] = true;
        }
    }
    std::size_t count = 0;
    for (const std::uint8_t code : biome) {
        count += known[code] ? 0 : 1;
    }
    return count;
}

std::uint32_t sea_colour(std::span<const worldgen::inspect::BiomeRecord> records) {
    for (const auto& record : records) {
        if (record.key == "ocean") {
            return static_cast<std::uint32_t>(record.colour & 0xFFFFFF);
        }
    }
    return 0x183878;
}

Vec2 cell_to_world(const Layout& layout, int x, int y) {
    const int half = layout.n / 2;
    return {layout.spacing * static_cast<float>(x - half), layout.spacing * static_cast<float>(y - half)};
}

CellHit world_to_cell(const Layout& layout, float X, float Z) {
    const float half = static_cast<float>(layout.n / 2);
    CellHit hit;
    hit.x = static_cast<int>(std::floor(X / layout.spacing + half + 0.5f));
    hit.y = static_cast<int>(std::floor(Z / layout.spacing + half + 0.5f));
    hit.inside = hit.x >= 0 && hit.x < layout.n && hit.y >= 0 && hit.y < layout.n;
    return hit;
}

float ground_clamp(const Layout& layout, std::span<const float> heights, float X, float Z) {
    const GridPoint p = grid_point(layout, X, Z);
    return std::max(std::max(heights[index(layout, p.i, p.j)], heights[index(layout, p.i + 1, p.j)]),
                    std::max(heights[index(layout, p.i, p.j + 1)], heights[index(layout, p.i + 1, p.j + 1)]));
}

float sample_bilinear(const Layout& layout, std::span<const float> heights, float X, float Z) {
    const GridPoint p = grid_point(layout, X, Z);
    const float fu = p.u - static_cast<float>(p.i);
    const float fv = p.v - static_cast<float>(p.j);
    const float h00 = heights[index(layout, p.i, p.j)];
    const float h10 = heights[index(layout, p.i + 1, p.j)];
    const float h01 = heights[index(layout, p.i, p.j + 1)];
    const float h11 = heights[index(layout, p.i + 1, p.j + 1)];
    const float top = h00 + (h10 - h00) * fu;
    const float bottom = h01 + (h11 - h01) * fu;
    return top + (bottom - top) * fv;
}

float ground_low(const Layout& layout, std::span<const float> heights, float X, float Z) {
    const GridPoint p = grid_point(layout, X, Z);
    const float fu = p.u - static_cast<float>(p.i);
    const float fv = p.v - static_cast<float>(p.j);
    const float h00 = heights[index(layout, p.i, p.j)];
    const float h10 = heights[index(layout, p.i + 1, p.j)];
    const float h01 = heights[index(layout, p.i, p.j + 1)];
    const float h11 = heights[index(layout, p.i + 1, p.j + 1)];
    // Diagonal 00-11.
    const float a = fu >= fv ? h00 + fu * (h10 - h00) + fv * (h11 - h10) : h00 + fv * (h01 - h00) + fu * (h11 - h01);
    // Diagonal 10-01.
    const float b = fu + fv <= 1.0f ? h00 + fu * (h10 - h00) + fv * (h01 - h00)
                                    : h11 + (1.0f - fu) * (h01 - h11) + (1.0f - fv) * (h10 - h11);
    return std::min(a, b);
}

std::string one_decimal(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.1f", value);
    std::string text = buffer;
    if (text == "-0.0") {
        text = "0.0";
    }
    return text;
}

std::string hud_line(const HudWorld& world, float X, float Y, float Z, float speed, int fps) {
    const Layout& layout = *world.layout;
    const CellHit hit = world_to_cell(layout, X, Z);
    const std::string sep = "  |  ";
    std::string text;
    if (hit.inside) {
        text = "cell (" + std::to_string(hit.x) + ", " + std::to_string(hit.y) + ")";
    } else {
        text = "outside the map";
    }
    text += sep + std::to_string(std::lround(X)) + " m E, " + std::to_string(std::lround(Z)) + " m S";
    const float ground = sample_bilinear(layout, world.heights, X, Z);
    text += sep + "altitude " + one_decimal(static_cast<double>(Y)) + " m, " + one_decimal(static_cast<double>(Y - ground)) +
            " above ground";
    if (hit.inside) {
        const std::size_t k = index(layout, hit.x, hit.y);
        if (!world.elevation.empty()) {
            const std::int64_t e = world.elevation[k];
            text += sep + "ground " + worldgen::inspect::format_layer_value("elevation", e);
            const std::int64_t d = world.water_depth.empty() ? 0 : world.water_depth[k];
            if (e > 0 && d > 0) {
                text += ", lake surface " + one_decimal(static_cast<double>(e + d) / 10.0) + " m";
            }
        }
        if (!world.biome.empty() && world.biome_name) {
            const std::string name = world.biome_name(world.biome[k]);
            if (!name.empty()) {
                text += sep + name;
            }
        }
        if (!world.region.empty() && world.region_name) {
            const std::string name = world.region_name(world.region[k]);
            if (!name.empty()) {
                text += sep + name;
            }
        }
    }
    text += sep + std::to_string(std::lround(speed)) + " m/s";
    text += sep + std::to_string(fps) + " fps";
    return text;
}

}  // namespace unmapped::terrain
