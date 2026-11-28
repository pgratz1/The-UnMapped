#include "terrain/ground.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "worldgen/inspect/render.hpp"

namespace unmapped::terrain {

const std::array<GroundTypeInfo, GROUND_TYPE_COUNT> GROUND_TYPES = {{
    {"", 0xFF00FF, 1, 0, 0, 230, 1.0f / 16.0f},                   // 0 unknown
    {"grassland", 0x96AA5A, 8, 50, 2, 217, 1.0f / 8.0f},          // 1
    {"temperate_forest", 0x2E6034, 6, 80, 3, 230, 1.0f / 8.0f},   // 2 forest floor
    {"wet_woodland", 0x23505E, 6, 70, 2, 179, 1.0f / 8.0f},       // 3 carr
    {"marsh", 0x6AA396, 4, 60, 1, 140, 1.0f / 12.0f},             // 4
    {"fen", 0x8A8070, 4, 60, 1, 153, 1.0f / 12.0f},               // 5
    {"steppe", 0xE0D296, 4, 40, 1, 230, 1.0f / 12.0f},            // 6
    {"heath", 0x94664A, 8, 80, 3, 230, 1.0f / 8.0f},              // 7
    {"highland_moor", 0x7E6680, 6, 70, 2, 217, 1.0f / 10.0f},     // 8 moor
    {"alpine", 0xBEBEB4, 16, 90, 4, 204, 1.0f / 6.0f},            // 9 scree
    {"blighted_waste", 0x605446, 4, 100, 3, 242, 1.0f / 10.0f},   // 10 waste
    {"", 0x6E6A64, 4, 110, 6, 191, 1.0f / 24.0f},                 // 11 rock
    {"", 0xF0F2F5, 2, 25, 1, 115, 1.0f / 16.0f},                  // 12 snow
    {"lake", 0xFFFFFF, 8, 20, 1, 20, 1.0f / 8.0f},                // 13 water
    {"ocean", 0xB8A878, 16, 40, 2, 217, 1.0f / 6.0f},             // 14 shore
}};

GroundType ground_type_of_key(std::string_view key) {
    if (key.empty()) {
        return GroundType::unknown;
    }
    for (int id = 0; id < GROUND_TYPE_COUNT; ++id) {
        if (key == GROUND_TYPES[static_cast<std::size_t>(id)].key) {
            return static_cast<GroundType>(id);
        }
    }
    return GroundType::unknown;
}

std::uint32_t ground_type_colour(GroundType type, std::span<const worldgen::inspect::BiomeRecord> records) {
    const auto id = static_cast<std::size_t>(type);
    const GroundTypeInfo& info = GROUND_TYPES[id];
    if (id >= 1 && id <= 10) {
        for (const auto& record : records) {
            if (record.key == info.key) {
                return static_cast<std::uint32_t>(record.colour & 0xFFFFFF);
            }
        }
    }
    return info.fallback_rgb;
}

std::uint32_t hash32(std::uint32_t a, std::uint32_t b, std::uint32_t c) noexcept {
    std::uint32_t h = a * 0x8DA6B343u + b * 0xD8163841u + c * 0xCB1AB31Fu;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

std::int64_t isqrt64(std::int64_t v) noexcept {
    if (v <= 0) {
        return 0;
    }
    auto r = static_cast<std::int64_t>(std::sqrt(static_cast<double>(v)));
    while (r > 0 && r * r > v) {
        --r;
    }
    while ((r + 1) * (r + 1) <= v) {
        ++r;
    }
    return r;
}

namespace {

constexpr int TS = GROUND_TEXTURE_SIZE;

std::uint32_t smooth(std::uint32_t f) { return (f * f * (768u - 2u * f)) >> 16; }

// The height n(i, j) of 3.2 for every pixel, row-major.
std::vector<int> ground_height(int id) {
    const GroundTypeInfo& info = GROUND_TYPES[static_cast<std::size_t>(id)];
    std::vector<std::int64_t> sum(static_cast<std::size_t>(TS) * TS, 0);
    for (std::uint32_t o = 0; o < 4; ++o) {
        const std::uint32_t P = static_cast<std::uint32_t>(info.p0) << o;
        const std::uint32_t c = 16u * static_cast<std::uint32_t>(id) + o;
        const std::int64_t weight = std::int64_t{8} >> o;
        for (std::uint32_t j = 0; j < static_cast<std::uint32_t>(TS); ++j) {
            const std::uint32_t uy = j * P;
            const std::uint32_t yl = uy >> 8;
            const std::uint32_t fy = uy & 255u;
            const std::uint32_t y1 = (yl + 1) % P;
            const std::uint32_t sy = smooth(fy);
            for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(TS); ++i) {
                const std::uint32_t ux = i * P;
                const std::uint32_t xl = ux >> 8;
                const std::uint32_t fx = ux & 255u;
                const std::uint32_t x1 = (xl + 1) % P;
                const std::uint32_t sx = smooth(fx);
                auto v = [c](std::uint32_t a, std::uint32_t b) { return hash32(a, b, c) >> 24; };
                const std::uint32_t top = v(xl, yl) * (256u - sx) + v(x1, yl) * sx;
                const std::uint32_t bot = v(xl, y1) * (256u - sx) + v(x1, y1) * sx;
                const std::uint32_t q = (top * (256u - sy) + bot * sy) >> 8;
                sum[static_cast<std::size_t>(j) * TS + i] += weight * static_cast<std::int64_t>(q);
            }
        }
    }
    std::vector<int> n(sum.size());
    for (std::size_t k = 0; k < sum.size(); ++k) {
        n[k] = static_cast<int>((sum[k] + 1920) / 3840);
    }
    return n;
}

}  // namespace

GroundTexture build_ground_texture(GroundType type) {
    const int id = static_cast<int>(type);
    const GroundTypeInfo& info = GROUND_TYPES[static_cast<std::size_t>(id)];
    const std::vector<int> n = ground_height(id);
    auto at = [&n](int i, int j) {
        return n[static_cast<std::size_t>((j + TS) % TS) * TS + static_cast<std::size_t>((i + TS) % TS)];
    };
    GroundTexture tex;
    tex.albedo.resize(static_cast<std::size_t>(4) * TS * TS);
    tex.normal.resize(static_cast<std::size_t>(4) * TS * TS);
    std::int64_t grey_sum = 0;
    const std::int64_t G = info.grain;
    const std::int64_t S = info.normal_strength;
    for (int j = 0; j < TS; ++j) {
        for (int i = 0; i < TS; ++i) {
            const std::size_t k = 4 * (static_cast<std::size_t>(j) * TS + static_cast<std::size_t>(i));
            const std::int64_t h = at(i, j);
            const std::int64_t g = 255 - (G * (255 - h) + 127) / 255;
            grey_sum += g;
            tex.albedo[k + 0] = static_cast<std::uint8_t>(g);
            tex.albedo[k + 1] = static_cast<std::uint8_t>(g);
            tex.albedo[k + 2] = static_cast<std::uint8_t>(g);
            tex.albedo[k + 3] = static_cast<std::uint8_t>(h);
            const std::int64_t gx = at(i + 1, j) - at(i - 1, j);
            const std::int64_t gy = at(i, j + 1) - at(i, j - 1);
            const std::int64_t X = -S * gx;
            const std::int64_t Y = S * gy;
            const std::int64_t Z = 512;
            const std::int64_t L = isqrt64(X * X + Y * Y + Z * Z);
            tex.normal[k + 0] = static_cast<std::uint8_t>(128 + 127 * X / L);
            tex.normal[k + 1] = static_cast<std::uint8_t>(128 + 127 * Y / L);
            tex.normal[k + 2] = static_cast<std::uint8_t>(128 + 127 * Z / L);
            tex.normal[k + 3] = info.roughness;
        }
    }
    tex.mean_grey = static_cast<double>(grey_sum) / 65536.0;
    return tex;
}

namespace {

double lin(double u) { return u <= 0.04045 ? u / 12.92 : std::pow((u + 0.055) / 1.055, 2.4); }

}  // namespace

std::array<float, 3> albedo_colour(GroundType type, const GroundTexture& texture,
                                   std::span<const worldgen::inspect::BiomeRecord> records) {
    const std::uint32_t c = ground_type_colour(type, records);
    const double m = lin(texture.mean_grey / 255.0);
    auto channel = [m](std::uint32_t byte) { return static_cast<float>(lin(static_cast<double>(byte) / 255.0) / m); };
    return {channel((c >> 16) & 0xFF), channel((c >> 8) & 0xFF), channel(c & 0xFF)};
}

float srgb_encode(float u) {
    const double v = static_cast<double>(u);
    return static_cast<float>(v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055);
}

namespace {

std::size_t cells_of(const Layout& layout) {
    return static_cast<std::size_t>(layout.n) * static_cast<std::size_t>(layout.n);
}

template <class T>
void require_size(std::span<const T> span, std::size_t cells, const char* name) {
    if (!span.empty() && span.size() != cells) {
        throw std::invalid_argument(std::string("ground: ") + name + " has " + std::to_string(span.size()) +
                                    " cells, expected " + std::to_string(cells));
    }
}

struct Paint {
    int x;
    int y;
    std::uint8_t k;
};

// D8 x offsets: 0 E, 1 SE, 2 S, 3 SW, 4 W, 5 NW, 6 N, 7 NE.
constexpr int D8_DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};

}  // namespace

std::vector<std::uint8_t> river_paint(const Layout& layout, std::span<const std::uint8_t> water,
                                      std::span<const std::uint8_t> river, std::span<const std::uint8_t> flow_dir) {
    const std::size_t cells = cells_of(layout);
    require_size(water, cells, "water");
    require_size(river, cells, "river");
    require_size(flow_dir, cells, "flow_dir");
    std::vector<std::uint8_t> out(cells, 0);
    if (water.empty()) {
        return out;
    }
    if (river.empty()) {
        for (std::size_t k = 0; k < cells; ++k) {
            out[k] = static_cast<std::uint8_t>(water[k] == 1 ? 16 : 0);
        }
        return out;
    }
    const int n = layout.n;
    std::vector<Paint> list;
    for (std::size_t k = 0; k < cells; ++k) {
        if (water[k] != 1) {
            continue;
        }
        const int x = static_cast<int>(k % static_cast<std::size_t>(n));
        const int y = static_cast<int>(k / static_cast<std::size_t>(n));
        const std::uint8_t cls = river[k];
        if (cls == 0) {
            continue;
        }
        list.push_back({x, y, cls});
        if (!flow_dir.empty()) {
            const std::uint8_t d = flow_dir[k];
            if (d <= 7 && (d & 1) != 0) {
                list.push_back({x + D8_DX[d], y, cls});
            }
        }
        if (cls >= 4) {
            list.push_back({x + 1, y, cls});
            list.push_back({x, y + 1, cls});
        }
    }
    std::stable_sort(list.begin(), list.end(), [](const Paint& a, const Paint& b) { return a.k < b.k; });
    for (const Paint& p : list) {
        if (p.x < 0 || p.y < 0 || p.x >= n || p.y >= n) {
            continue;
        }
        const std::size_t j = static_cast<std::size_t>(p.y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(p.x);
        if (water[j] > 1) {
            continue;
        }
        out[j] = p.k;
    }
    return out;
}

GroundCells build_ground_cells(const GroundInputs& in) {
    const Layout& layout = *in.layout;
    const std::size_t cells = cells_of(layout);
    if (in.elevation.size() != cells) {
        throw std::invalid_argument("ground: elevation has " + std::to_string(in.elevation.size()) +
                                    " cells, expected " + std::to_string(cells));
    }
    require_size(in.biome, cells, "biome");
    require_size(in.temperature, cells, "temperature");
    GroundCells out;
    // Type of each biome code, by its record's key.
    std::uint8_t code_type[256] = {};
    for (const auto& record : in.records) {
        if (record.id >= 0 && record.id <= 255) {
            code_type[record.id] = static_cast<std::uint8_t>(ground_type_of_key(record.key));
        }
    }
    out.type.assign(cells, 0);
    if (!in.biome.empty()) {
        for (std::size_t k = 0; k < cells; ++k) {
            out.type[k] = code_type[in.biome[k]];
        }
    }
    out.painted = river_paint(layout, in.water, in.river, in.flow_dir);
    out.rock.assign(cells, 0);
    out.snow.assign(cells, 0);
    const int n = layout.n;
    auto E = [&in, n](int x, int y) {
        x = std::clamp(x, 0, n - 1);
        y = std::clamp(y, 0, n - 1);
        return static_cast<std::int64_t>(
            in.elevation[static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x)]);
    };
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const std::size_t k = static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x);
            const std::uint8_t type = out.type[k];
            if (!is_land_type(type)) {
                continue;
            }
            const std::int64_t dx = E(x + 1, y) - E(x - 1, y);
            const std::int64_t dy = E(x, y + 1) - E(x, y - 1);
            const std::int64_t g = isqrt64(dx * dx + dy * dy);
            const std::int64_t rock = g <= 224 ? 0 : g >= 320 ? 255 : ((g - 224) * 255 + 48) / 96;
            out.rock[k] = static_cast<std::uint8_t>(rock);
            if (type == static_cast<std::uint8_t>(GroundType::scree) && !in.temperature.empty()) {
                const std::int64_t T = in.temperature[k];
                const std::int64_t snow = T >= -20 ? 0 : T <= -60 ? 255 : ((-20 - T) * 255 + 20) / 40;
                out.snow[k] = static_cast<std::uint8_t>(snow);
            }
        }
    }
    return out;
}

std::vector<std::uint32_t> build_control(const Layout& layout, const GroundCells& cells) {
    const int n = layout.n;
    const std::size_t total = cells_of(layout);
    std::vector<std::uint32_t> out(total, 0);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const std::size_t k = static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x);
            const std::uint32_t type = cells.type[k];
            const bool painted = cells.painted[k] > 0;
            const bool land = is_land_type(cells.type[k]);
            const std::uint32_t base = painted ? 13u : type;
            std::uint32_t overlay = base;
            std::uint32_t blend = 0;
            if (land && !painted) {
                int count[11] = {};
                for (int wy = y - 2; wy <= y + 2; ++wy) {
                    if (wy < 0 || wy >= n) {
                        continue;
                    }
                    for (int wx = x - 2; wx <= x + 2; ++wx) {
                        if (wx < 0 || wx >= n) {
                            continue;
                        }
                        const std::uint8_t t =
                            cells.type[static_cast<std::size_t>(wy) * static_cast<std::size_t>(n) + static_cast<std::size_t>(wx)];
                        if (is_land_type(t)) {
                            ++count[t];
                        }
                    }
                }
                const int b = count[type];
                int o = 0;
                std::uint32_t ov = 0;
                for (std::uint32_t t = 0; t <= 10; ++t) {
                    if (t != type && count[t] > o) {
                        o = count[t];
                        ov = t;
                    }
                }
                const int border = o == 0 ? 0 : std::min(127, (255 * o + (b + o) / 2) / (b + o));
                // Candidates in order; the largest blend wins, ties to the earlier.
                const std::uint32_t ids[3] = {11u, 12u, ov};
                const int blends[3] = {cells.rock[k], cells.snow[k], border};
                int pick = 0;
                for (int c = 1; c < 3; ++c) {
                    if (blends[c] > blends[pick]) {
                        pick = c;
                    }
                }
                if (blends[pick] > 0) {
                    overlay = ids[pick];
                    blend = static_cast<std::uint32_t>(blends[pick]);
                }
            }
            out[k] = (base << 27) | (overlay << 22) | (blend << 14);
        }
    }
    return out;
}

std::vector<std::uint8_t> build_textured_colours(const Layout& layout, const GroundCells& cells,
                                                 std::span<const worldgen::inspect::BiomeRecord> records) {
    const std::size_t total = cells_of(layout);
    std::uint32_t lake = 0x4080CC;
    for (const auto& record : records) {
        if (record.key == "lake") {
            lake = static_cast<std::uint32_t>(record.colour & 0xFFFFFF);
            break;
        }
    }
    std::vector<std::uint8_t> out(total * 4);
    for (std::size_t k = 0; k < total; ++k) {
        std::uint8_t r = 255;
        std::uint8_t g = 255;
        std::uint8_t b = 255;
        const std::uint8_t p = cells.painted[k];
        if (p > 0) {
            if (p <= 15) {
                const worldgen::inspect::Rgb rgb = worldgen::inspect::composite_river_colour(p);
                r = rgb.r;
                g = rgb.g;
                b = rgb.b;
            } else {
                r = 52;
                g = 100;
                b = 200;
            }
        } else if (cells.type[k] == static_cast<std::uint8_t>(GroundType::water)) {
            r = static_cast<std::uint8_t>((lake >> 16) & 0xFF);
            g = static_cast<std::uint8_t>((lake >> 8) & 0xFF);
            b = static_cast<std::uint8_t>(lake & 0xFF);
        }
        out[4 * k + 0] = r;
        out[4 * k + 1] = g;
        out[4 * k + 2] = b;
        out[4 * k + 3] = A_NEUTRAL;
    }
    return out;
}

}  // namespace unmapped::terrain
