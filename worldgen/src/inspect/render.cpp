// Layer rendering (spec P0-04 section 6, P1-00 section 10.2).

#include "worldgen/inspect/render.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <variant>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/math.hpp"
#include "worldgen/core/wide.hpp"
#include "render_internal.hpp"

namespace worldgen::inspect {

namespace {

constexpr Rgb MAGENTA{255, 0, 255};

bool is_unsigned_type(db::ElementType t) noexcept {
    return t == db::ElementType::u8 || t == db::ElementType::u16 || t == db::ElementType::u32 ||
           t == db::ElementType::u64;
}

}  // namespace

// --- Palette names and suitability ---------------------------------------------

std::string_view palette_name(Palette palette) noexcept {
    switch (palette) {
        case Palette::gray: return "gray";
        case Palette::hypsometric: return "hypsometric";
        case Palette::water: return "water";
        case Palette::river: return "river";
        case Palette::log2: return "log2";
        case Palette::direction: return "direction";
        case Palette::ids: return "ids";
        case Palette::signed_distance: return "signed";
        case Palette::temperature: return "temperature";
        case Palette::rainfall: return "rainfall";
        case Palette::biome: return "biome";
        case Palette::score: return "score";
    }
    return "gray";
}

std::optional<Palette> palette_from_name(std::string_view name) noexcept {
    for (const Palette p : ALL_PALETTES) {
        if (name == palette_name(p)) {
            return p;
        }
    }
    return std::nullopt;
}

Palette registered_palette(std::string_view layer_name) noexcept {
    if (layer_name == "elevation") return Palette::hypsometric;
    if (layer_name == "water") return Palette::water;
    if (layer_name == "river") return Palette::river;
    if (layer_name == "flow_accum") return Palette::log2;
    if (layer_name == "flow_dir") return Palette::direction;
    if (layer_name == "lake") return Palette::ids;
    if (layer_name == "coast_dist") return Palette::signed_distance;
    if (layer_name == "temperature") return Palette::temperature;  // P1-03 9
    if (layer_name == "rainfall") return Palette::rainfall;        // P1-03 9
    if (layer_name == "biome") return Palette::biome;              // P1-04 9
    if (layer_name == "fertility" || layer_name == "ore" || layer_name == "timber" || layer_name == "fresh_water" ||
        layer_name == "forage" || layer_name == "habitability") {
        return Palette::score;  // P1-05 9
    }
    if (layer_name == "walk_area") return Palette::ids;  // P1-05 9
    if (layer_name == "region") return Palette::ids;     // P1-06 9
    return Palette::gray;  // P1-00 10.2: every other layer, water_depth included
}

bool palette_suits(Palette palette, const db::LayerGrid& grid) noexcept {
    if (!db::is_valid_layer_grid(grid)) {
        return false;
    }
    const db::ElementType t = grid.type();
    switch (palette) {
        case Palette::gray: return true;
        case Palette::hypsometric: return t == db::ElementType::i16;
        case Palette::water: return t == db::ElementType::u8;
        case Palette::direction: return t == db::ElementType::u8;
        case Palette::river: {
            const std::vector<std::uint8_t>* cells = grid.get_if<std::uint8_t>();
            if (cells == nullptr) {
                return false;
            }
            for (const std::uint8_t v : *cells) {
                if (v > 15U) {
                    return false;
                }
            }
            return true;
        }
        case Palette::log2:
        case Palette::ids: return is_unsigned_type(t);
        case Palette::signed_distance: return !is_unsigned_type(t);
        case Palette::temperature: return t == db::ElementType::i16;
        case Palette::rainfall: return t == db::ElementType::u16;
        case Palette::biome: return t == db::ElementType::u8;
        case Palette::score: return t == db::ElementType::u8;
    }
    return false;
}

std::string_view palette_requirement(Palette palette) noexcept {
    switch (palette) {
        case Palette::gray: return "any layer";
        case Palette::hypsometric: return "an i16 layer";
        case Palette::water: return "a u8 layer";
        case Palette::river: return "a u8 layer with values 0 to 15";
        case Palette::log2: return "a layer of an unsigned type (u8, u16, u32, u64)";
        case Palette::direction: return "a u8 layer";
        case Palette::ids: return "a layer of an unsigned type (u8, u16, u32, u64)";
        case Palette::signed_distance: return "a layer of a signed type (i8, i16, i32, i64)";
        case Palette::temperature: return "an i16 layer";
        case Palette::rainfall: return "a u16 layer";
        case Palette::biome: return "a u8 layer";
        case Palette::score: return "a u8 layer";
    }
    return "any layer";
}

Palette auto_palette(std::string_view layer_name, const db::LayerGrid& grid) noexcept {
    const Palette p = registered_palette(layer_name);
    return palette_suits(p, grid) ? p : Palette::gray;
}

// --- Colour functions ------------------------------------------------------------

namespace {

// The interpolation of ramp_colour on stops already known to be valid.
Rgb ramp_unchecked(std::span<const RampStop> stops, std::int64_t t) {
    if (t < stops.front().t) {
        return stops.front().colour;
    }
    if (t >= stops.back().t) {
        return stops.back().colour;
    }
    std::size_t k = 0;
    while (!(t < stops[k + 1].t)) {
        ++k;
    }
    const std::int64_t span = stops[k + 1].t - stops[k].t;  // 1 .. 2^40 - 1
    const std::int64_t off = t - stops[k].t;                 // 0 .. span - 1
    // |2 dc off + span| < 511 * 2^40 < 2^49: no overflow.
    const auto channel = [span, off](std::uint8_t c0, std::uint8_t c1) {
        const std::int64_t dc = static_cast<std::int64_t>(c1) - static_cast<std::int64_t>(c0);
        const std::int64_t step = floor_div(2 * dc * off + span, 2 * span);
        return static_cast<std::uint8_t>(static_cast<std::int64_t>(c0) + step);
    };
    const Rgb a = stops[k].colour;
    const Rgb b = stops[k + 1].colour;
    return Rgb{channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b)};
}

}  // namespace

Rgb ramp_colour(std::span<const RampStop> stops, std::int64_t t) {
    if (stops.empty()) {
        fatal("ramp_colour: no stops");
    }
    constexpr std::int64_t MAX_STOP_GAP = std::int64_t{1} << 40;
    for (std::size_t k = 1; k < stops.size(); ++k) {
        if (stops[k].t <= stops[k - 1].t) {
            fatal("ramp_colour: stops not strictly ascending");
        }
        if (checked_sub(stops[k].t, stops[k - 1].t) >= MAX_STOP_GAP) {
            fatal("ramp_colour: stops too far apart");
        }
    }
    return ramp_unchecked(stops, t);
}

// The two hypsometric ramps are valid constants (ascending, gaps below
// 2^40), so they skip ramp_colour's per-call validation.
Rgb hypsometric_land_colour(std::int64_t t) {
    return ramp_unchecked(HYPSOMETRIC_LAND_STOPS, t);
}

Rgb hypsometric_ocean_colour(std::int64_t depth) {
    return ramp_unchecked(HYPSOMETRIC_OCEAN_STOPS, depth);
}

// The climate ramps are valid constants too.
Rgb temperature_colour(std::int64_t t) {
    return ramp_unchecked(TEMPERATURE_STOPS, t);
}

Rgb rainfall_colour(std::int64_t t) {
    return ramp_unchecked(RAINFALL_STOPS, t);
}

Rgb score_colour(std::int64_t t) {
    return ramp_unchecked(SCORE_STOPS, t);
}

Rgb hypsometric_colour(std::int64_t e) {
    if (e > 0) {
        return hypsometric_land_colour(e);
    }
    // -e overflows only for INT64_MIN, which is far beyond the last stop.
    const std::int64_t depth = e == std::numeric_limits<std::int64_t>::min() ? std::numeric_limits<std::int64_t>::max()
                                                                             : -e;
    return hypsometric_ocean_colour(depth);
}

Rgb water_colour(std::uint64_t v) noexcept {
    switch (v) {
        case 0U: return Rgb{214, 200, 160};
        case 1U: return Rgb{52, 100, 200};
        case 2U: return Rgb{64, 128, 204};
        case 3U: return Rgb{24, 56, 120};
        default: return MAGENTA;
    }
}

Rgb river_colour(std::uint64_t v) noexcept {
    switch (v) {
        case 0U: return Rgb{16, 16, 16};
        case 1U: return Rgb{120, 170, 230};
        case 2U: return Rgb{100, 150, 225};
        case 3U: return Rgb{80, 130, 220};
        case 4U: return Rgb{60, 110, 210};
        case 5U: return Rgb{45, 90, 200};
        default: break;
    }
    if (v <= 15U) {
        return Rgb{30, 70, 190};
    }
    return MAGENTA;  // above 15: not defined by P1-00 10.2 (reading chosen)
}

Rgb log2_colour(std::uint64_t v) noexcept {
    // bit_length (P1-00 amendment A1) is 0 .. 64.
    const std::uint32_t n = 11U * static_cast<std::uint32_t>(bit_length(v));
    const auto g = static_cast<std::uint8_t>(n > 255U ? 255U : n);
    return Rgb{g, g, g};
}

Rgb direction_colour(std::uint64_t v) noexcept {
    switch (v) {
        case 0U: return Rgb{230, 25, 75};
        case 1U: return Rgb{245, 130, 48};
        case 2U: return Rgb{255, 225, 25};
        case 3U: return Rgb{210, 245, 60};
        case 4U: return Rgb{60, 180, 75};
        case 5U: return Rgb{70, 240, 240};
        case 6U: return Rgb{0, 130, 200};
        case 7U: return Rgb{145, 30, 180};
        case 8U: return Rgb{0, 0, 0};
        default: return Rgb{255, 255, 255};
    }
}

Rgb ids_colour(std::uint64_t v) noexcept {
    if (v == 0U) {
        return Rgb{0, 0, 0};
    }
    const std::uint64_t h = mix64(v);
    const auto channel = [h](unsigned int j) {
        const std::uint64_t byte = (h >> (8U * j)) & 0xFFU;
        return static_cast<std::uint8_t>(64U + byte * 191U / 255U);
    };
    return Rgb{channel(0U), channel(1U), channel(2U)};
}

Rgb biome_colour(std::span<const BiomeColour> table, std::uint64_t v) noexcept {
    for (const BiomeColour& b : table) {
        if (b.id >= 0 && static_cast<std::uint64_t>(b.id) == v) {
            const auto c = static_cast<std::uint64_t>(b.colour);
            return Rgb{static_cast<std::uint8_t>((c >> 16U) & 0xFFU), static_cast<std::uint8_t>((c >> 8U) & 0xFFU),
                       static_cast<std::uint8_t>(c & 0xFFU)};
        }
    }
    return MAGENTA;
}

SignedScale signed_scale(const db::LayerGrid& grid) {
    if (!db::is_valid_layer_grid(grid) || is_unsigned_type(grid.type())) {
        fatal("signed_scale: not a valid grid of a signed type");
    }
    SignedScale s;
    std::visit(
        [&s](const auto& cells) {
            using T = typename std::decay_t<decltype(cells)>::value_type;
            if constexpr (std::is_signed_v<T>) {
                for (const T v : cells) {
                    const auto w = static_cast<std::int64_t>(v);
                    if (w > 0) {
                        const auto pos = static_cast<std::uint64_t>(w);
                        if (pos > s.p) s.p = pos;
                    } else if (w < 0) {
                        // -w as uint64, exact for INT64_MIN too.
                        const std::uint64_t neg = 0U - static_cast<std::uint64_t>(w);
                        if (neg > s.m) s.m = neg;
                    }
                }
            }
        },
        grid.values);
    return s;
}

Rgb signed_colour(std::int64_t v, SignedScale scale) {
    if (v >= 0) {
        const std::uint8_t g = gray_level(static_cast<std::uint64_t>(v), scale.p);
        return Rgb{g, g, g};
    }
    const std::uint64_t neg = 0U - static_cast<std::uint64_t>(v);
    const std::uint32_t level = gray_level(neg, scale.m);
    return Rgb{0, 0, static_cast<std::uint8_t>(255U - (3U * level) / 4U)};
}

// --- Gray and the buffer ---------------------------------------------------------

namespace {

void require_valid(const db::LayerGrid& grid, const char* what) {
    if (!db::is_valid_layer_grid(grid)) {
        fatal(what);
    }
}

// v - min as uint64: exact for every element type when min <= v, because
// the difference of two values of a 64-bit-or-narrower type is below 2^64.
template <class T>
std::uint64_t distance(T v, T lo) noexcept {
    if constexpr (std::is_signed_v<T>) {
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(v)) -
               static_cast<std::uint64_t>(static_cast<std::int64_t>(lo));
    } else {
        return static_cast<std::uint64_t>(v) - static_cast<std::uint64_t>(lo);
    }
}

template <class T>
void min_max(const std::vector<T>& cells, T& lo, T& hi) noexcept {
    lo = cells.front();
    hi = cells.front();
    for (const T v : cells) {
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
}

// The byte size of the RGB buffer of a w x h grid at the given scale.
// FatalError if scale is outside 1..16 or the size does not fit in
// std::size_t.
std::size_t buffer_size(std::uint32_t w, std::uint32_t h, std::uint32_t scale, const char* what) {
    if (scale < MIN_RENDER_SCALE || scale > MAX_RENDER_SCALE) {
        fatal(what);
    }
    // At most 65536 * 16 = 2^20 pixels per side, so the pixel count is at
    // most 2^40 and the byte count 3 * 2^40: fits a 64-bit size_t.
    const std::uint64_t out_w = static_cast<std::uint64_t>(w) * scale;
    const std::uint64_t out_h = static_cast<std::uint64_t>(h) * scale;
    const std::uint64_t bytes = out_w * out_h * 3U;
    if (bytes > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        fatal("render: image too large");
    }
    return static_cast<std::size_t>(bytes);
}

// Fills rgb (of buffer_size bytes) with colour_of(index) for each cell,
// each cell a scale x scale block, in index order. Every byte is written.
template <class ColourOf>
void fill_buffer(std::span<std::uint8_t> rgb, std::uint32_t width, std::uint32_t height, std::uint32_t scale,
                 const ColourOf& colour_of) {
    const std::size_t w = width;
    const std::size_t h = height;
    const std::size_t s = scale;
    const std::size_t row_bytes = w * s * 3U;
    for (std::size_t y = 0; y < h; ++y) {
        // Fill the first pixel row of this cell row, then copy it to the
        // other scale - 1 rows.
        std::uint8_t* row = rgb.data() + (y * s) * row_bytes;
        for (std::size_t x = 0; x < w; ++x) {
            const Rgb c = colour_of(y * w + x);
            std::uint8_t* px = row + x * s * 3U;
            for (std::size_t i = 0; i < s; ++i) {
                px[3U * i] = c.r;
                px[3U * i + 1U] = c.g;
                px[3U * i + 2U] = c.b;
            }
        }
        for (std::size_t j = 1; j < s; ++j) {
            std::uint8_t* dst = row + j * row_bytes;
            for (std::size_t b = 0; b < row_bytes; ++b) {
                dst[b] = row[b];
            }
        }
    }
}

// A cell value as uint64 for the per-value palettes (unsigned types only).
template <class T>
std::uint64_t as_u64(T v) noexcept {
    return static_cast<std::uint64_t>(v);
}

}  // namespace

ValueRange value_range(const db::LayerGrid& grid) {
    require_valid(grid, "value_range: invalid layer grid");
    ValueRange range;
    std::visit(
        [&range](const auto& cells) {
            using T = typename std::decay_t<decltype(cells)>::value_type;
            T lo{};
            T hi{};
            min_max(cells, lo, hi);
            if constexpr (std::is_signed_v<T>) {
                range.is_signed = true;
                range.min_signed = static_cast<std::int64_t>(lo);
                range.max_signed = static_cast<std::int64_t>(hi);
            } else {
                range.is_signed = false;
                range.min_unsigned = static_cast<std::uint64_t>(lo);
                range.max_unsigned = static_cast<std::uint64_t>(hi);
            }
        },
        grid.values);
    return range;
}

std::uint8_t gray_level(std::uint64_t d, std::uint64_t r) {
    if (d > r) {
        fatal("gray_level: value above the layer maximum");
    }
    if (r == 0) {
        return 0;
    }
    // floor((510 d + r) / (2 r)) = floor(floor((510 d + r) / r) / 2). The
    // numerator is at most 511 (2^64 - 1) < 2^73, so it fits in 128 bits,
    // and the inner quotient is at most 511.
    const U128 numerator = mul_u64_wide(510U, d) + u128_from_u64(r);
    const U128DivMod q = divmod_u128_u64(numerator, r);
    return static_cast<std::uint8_t>(q.quotient.lo >> 1U);
}

std::size_t render_rgb_size(const db::LayerGrid& grid, std::uint32_t scale) {
    require_valid(grid, "render_rgb_size: invalid layer grid");
    return buffer_size(grid.width, grid.height, scale, "render_rgb_size: scale must be 1 to 16");
}

void render_rgb(const db::LayerGrid& grid, Palette palette, std::uint32_t scale, std::span<std::uint8_t> rgb) {
    render_rgb(grid, palette, scale, std::span<const BiomeColour>{}, rgb);
}

void render_rgb(const db::LayerGrid& grid, Palette palette, std::uint32_t scale,
                std::span<const BiomeColour> biome_table, std::span<std::uint8_t> rgb) {
    require_valid(grid, "render_rgb: invalid layer grid");
    if (scale < MIN_RENDER_SCALE || scale > MAX_RENDER_SCALE) {
        fatal("render_rgb: scale must be 1 to 16");
    }
    if (!palette_suits(palette, grid)) {
        fatal("render_rgb: the palette does not suit the layer");
    }
    if (rgb.size() != buffer_size(grid.width, grid.height, scale, "render_rgb: scale must be 1 to 16")) {
        fatal("render_rgb: output buffer has the wrong size");
    }

    std::visit(
        [&](const auto& cells) {
            using T = typename std::decay_t<decltype(cells)>::value_type;
            switch (palette) {
                case Palette::gray: {
                    T lo{};
                    T hi{};
                    min_max(cells, lo, hi);
                    const std::uint64_t r = distance(hi, lo);
                    fill_buffer(rgb, grid.width, grid.height, scale, [&](std::size_t i) {
                        const std::uint8_t g = gray_level(distance(cells[i], lo), r);
                        return Rgb{g, g, g};
                    });
                    return;
                }
                case Palette::signed_distance: {
                    if constexpr (std::is_signed_v<T>) {
                        const SignedScale s = signed_scale(grid);
                        fill_buffer(rgb, grid.width, grid.height, scale, [&](std::size_t i) {
                            return signed_colour(static_cast<std::int64_t>(cells[i]), s);
                        });
                    }
                    return;
                }
                case Palette::hypsometric: {
                    if constexpr (std::is_signed_v<T>) {
                        fill_buffer(rgb, grid.width, grid.height, scale, [&](std::size_t i) {
                            return hypsometric_colour(static_cast<std::int64_t>(cells[i]));
                        });
                    }
                    return;
                }
                case Palette::temperature: {
                    if constexpr (std::is_signed_v<T>) {
                        fill_buffer(rgb, grid.width, grid.height, scale, [&](std::size_t i) {
                            return temperature_colour(static_cast<std::int64_t>(cells[i]));
                        });
                    }
                    return;
                }
                case Palette::rainfall: {
                    if constexpr (!std::is_signed_v<T>) {
                        fill_buffer(rgb, grid.width, grid.height, scale, [&](std::size_t i) {
                            return rainfall_colour(static_cast<std::int64_t>(cells[i]));
                        });
                    }
                    return;
                }
                case Palette::score: {
                    if constexpr (!std::is_signed_v<T>) {
                        // One colour per value, then per cell (u8 only, palette_suits).
                        std::array<Rgb, 256> colours{};
                        for (std::size_t v = 0; v < colours.size(); ++v) {
                            colours[v] = score_colour(static_cast<std::int64_t>(v));
                        }
                        fill_buffer(rgb, grid.width, grid.height, scale, [&](std::size_t i) {
                            const std::uint64_t v = as_u64(cells[i]);
                            return v < colours.size() ? colours[v] : MAGENTA;
                        });
                    }
                    return;
                }
                case Palette::biome: {
                    if constexpr (!std::is_signed_v<T>) {
                        // One colour per code, then per cell.
                        std::array<Rgb, 256> colours{};
                        for (std::size_t v = 0; v < colours.size(); ++v) {
                            colours[v] = biome_colour(biome_table, v);
                        }
                        fill_buffer(rgb, grid.width, grid.height, scale, [&](std::size_t i) {
                            const std::uint64_t v = as_u64(cells[i]);
                            return v < colours.size() ? colours[v] : MAGENTA;
                        });
                    }
                    return;
                }
                case Palette::water:
                case Palette::river:
                case Palette::log2:
                case Palette::direction:
                case Palette::ids: {
                    if constexpr (!std::is_signed_v<T>) {
                        // A switch per cell rather than a function pointer, so
                        // no compiler has to reason about pointers to noexcept
                        // functions.
                        const auto colour = [palette](std::uint64_t v) {
                            switch (palette) {
                                case Palette::river: return river_colour(v);
                                case Palette::log2: return log2_colour(v);
                                case Palette::direction: return direction_colour(v);
                                case Palette::ids: return ids_colour(v);
                                default: return water_colour(v);
                            }
                        };
                        fill_buffer(rgb, grid.width, grid.height, scale,
                                    [&](std::size_t i) { return colour(as_u64(cells[i])); });
                    }
                    return;
                }
            }
        },
        grid.values);
}

std::vector<std::uint8_t> render_rgb(const db::LayerGrid& grid, Palette palette, std::uint32_t scale) {
    std::vector<std::uint8_t> rgb(render_rgb_size(grid, scale));
    render_rgb(grid, palette, scale, rgb);
    return rgb;
}

std::vector<std::uint8_t> render_rgb(const db::LayerGrid& grid, Palette palette, std::uint32_t scale,
                                     std::span<const BiomeColour> biome_table) {
    std::vector<std::uint8_t> rgb(render_rgb_size(grid, scale));
    render_rgb(grid, palette, scale, biome_table, rgb);
    return rgb;
}

// --- Composite map ---------------------------------------------------------------

std::string_view composite_name(Composite composite) noexcept {
    switch (composite) {
        case Composite::map: return "map";
        case Composite::biomes: return "biomes";
        case Composite::regions: return "regions";
    }
    return "map";
}

std::optional<Composite> composite_from_name(std::string_view name) noexcept {
    for (const Composite c : ALL_COMPOSITES) {
        if (name == composite_name(c)) {
            return c;
        }
    }
    return std::nullopt;
}

std::uint32_t hillshade_factor(std::span<const std::int16_t> cells, std::uint32_t w, std::uint32_t h,
                               std::uint32_t x, std::uint32_t y) {
    if (static_cast<std::uint64_t>(cells.size()) != static_cast<std::uint64_t>(w) * h || x >= w || y >= h) {
        fatal("hillshade_factor: cell outside the grid");
    }
    const auto e = [&](std::uint32_t cx, std::uint32_t cy) {
        const std::int64_t v = cells[static_cast<std::size_t>(cy) * w + cx];
        return v > 0 ? v : std::int64_t{0};
    };
    const std::uint32_t xl = x == 0U ? 0U : x - 1U;
    const std::uint32_t xr = x + 1U >= w ? w - 1U : x + 1U;
    const std::uint32_t yu = y == 0U ? 0U : y - 1U;
    const std::uint32_t yd = y + 1U >= h ? h - 1U : y + 1U;
    const std::int64_t gx = e(xr, y) - e(xl, y);
    const std::int64_t gy = e(x, yd) - e(x, yu);
    std::int64_t f = 256 + floor_div(gx + gy, 2);
    if (f < 160) f = 160;
    if (f > 352) f = 352;
    return static_cast<std::uint32_t>(f);
}

Rgb shade_colour(Rgb c, std::uint32_t f) noexcept {
    const auto channel = [f](std::uint8_t v) {
        const std::uint64_t s = (static_cast<std::uint64_t>(v) * f + 128U) >> 8U;
        return static_cast<std::uint8_t>(s > 255U ? 255U : s);
    };
    return Rgb{channel(c.r), channel(c.g), channel(c.b)};
}

namespace {

std::optional<std::string> u8_companion_problem(const db::LayerGrid* grid, std::string_view name,
                                                const db::LayerGrid& elevation) {
    if (grid == nullptr) {
        return std::nullopt;
    }
    if (!db::is_valid_layer_grid(*grid)) {
        return "layer '" + std::string(name) + "' is not a valid grid";
    }
    if (grid->type() != db::ElementType::u8) {
        return "layer '" + std::string(name) + "' is " + std::string(db::element_type_name(grid->type())) +
               ", expected u8";
    }
    if (grid->width != elevation.width || grid->height != elevation.height) {
        return "layer '" + std::string(name) + "' is not the size of layer 'elevation'";
    }
    return std::nullopt;
}

}  // namespace

std::optional<std::string> composite_map_problem(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                                 const db::LayerGrid* river) {
    return composite_map_problem(elevation, water, river, nullptr);
}

std::optional<std::string> composite_map_problem(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                                 const db::LayerGrid* river, const db::LayerGrid* flow_dir) {
    if (!db::is_valid_layer_grid(elevation)) {
        return std::string("layer 'elevation' is not a valid grid");
    }
    if (elevation.type() != db::ElementType::i16) {
        return "layer 'elevation' is " + std::string(db::element_type_name(elevation.type())) + ", expected i16";
    }
    if (std::optional<std::string> p = u8_companion_problem(water, "water", elevation)) {
        return p;
    }
    if (std::optional<std::string> p = u8_companion_problem(river, "river", elevation)) {
        return p;
    }
    return u8_companion_problem(flow_dir, "flow_dir", elevation);
}

Rgb composite_river_colour(std::uint64_t k) noexcept {
    switch (k) {
        case 0U: return MAGENTA;
        case 1U: return Rgb{70, 125, 215};
        case 2U: return Rgb{55, 110, 210};
        case 3U: return Rgb{40, 95, 200};
        case 4U: return Rgb{30, 80, 190};
        case 5U: return Rgb{25, 65, 175};
        default: return Rgb{20, 50, 160};
    }
}

namespace {

// One entry of the river paint list of P1-00 10.2 composite step 4.
struct RiverPaint {
    std::int64_t x;
    std::int64_t y;
    std::uint8_t k;
};

// D8 x offsets (P1-00 2.3): 0 E, 1 SE, 2 S, 3 SW, 4 W, 5 NW, 6 N, 7 NE.
constexpr std::int64_t RIVER_D8_DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};

// Step 4 of the composite map: rivers drawn over the cell colours of steps
// 1 to 3.
// When drawn is given, each painted cell is marked 1 in it.
void paint_rivers(std::vector<Rgb>& colour, std::uint32_t w, std::uint32_t h, const std::vector<std::uint8_t>& wc,
                  const std::vector<std::uint8_t>& rc, const std::vector<std::uint8_t>* fd,
                  std::vector<std::uint8_t>* drawn) {
    // The paint list, built by scanning the river cells in index order.
    std::vector<RiverPaint> list;
    for (std::size_t i = 0; i < wc.size(); ++i) {
        if (wc[i] != 1U) {
            continue;
        }
        const auto x = static_cast<std::int64_t>(i % w);
        const auto y = static_cast<std::int64_t>(i / w);
        const std::uint8_t k = rc[i];
        if (k == 0U) {
            continue;  // class 0 (a hand-made layer only): no entry, not painted
        }
        list.push_back(RiverPaint{x, y, k});
        if (fd != nullptr) {
            const std::uint8_t d = (*fd)[i];
            // Odd codes 1, 3, 5, 7 are diagonal steps; 8 (or an invalid
            // code above 7) is not.
            if (d <= 7U && (d & 1U) != 0U) {
                list.push_back(RiverPaint{x + RIVER_D8_DX[d], y, k});
            }
        }
        if (k >= 4U) {
            list.push_back(RiverPaint{x + 1, y, k});
            list.push_back(RiverPaint{x, y + 1, k});
        }
    }
    // Ascending k; equal k in list order (stable), a later entry overwrites.
    std::stable_sort(list.begin(), list.end(), [](const RiverPaint& a, const RiverPaint& b) { return a.k < b.k; });
    const auto wi = static_cast<std::int64_t>(w);
    const auto hi = static_cast<std::int64_t>(h);
    for (const RiverPaint& p : list) {
        if (p.x < 0 || p.y < 0 || p.x >= wi || p.y >= hi) {
            continue;  // never off the map
        }
        const auto j = static_cast<std::size_t>(p.y * wi + p.x);
        if (wc[j] > 1U) {
            continue;  // never over a lake or ocean cell
        }
        colour[j] = composite_river_colour(p.k);
        if (drawn != nullptr) {
            (*drawn)[j] = 1U;
        }
    }
}

}  // namespace

std::vector<std::uint8_t> render_composite_map(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                               const db::LayerGrid* river, std::uint32_t scale) {
    return render_composite_map(elevation, water, river, nullptr, scale);
}

namespace {

// Steps 1 to 4 of the composite map (P1-00 10.2), with the land colour of
// step 2 given by land_colour(i, e) before shading (hypsometric for the
// composite map, the biome colour for the composite biomes, P1-04 9). When
// drawn is given it is resized to the cell count and marks 1 every cell that
// step 3 or step 4 drew (P4-01 5.6 item 1), 0 every other cell.
template <class LandColour>
std::vector<Rgb> shaded_colours(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                const db::LayerGrid* river, const db::LayerGrid* flow_dir,
                                const LandColour& land_colour, std::vector<std::uint8_t>* drawn = nullptr) {
    const std::vector<std::int16_t>& e = *elevation.get_if<std::int16_t>();
    const std::vector<std::uint8_t>* wcells = water == nullptr ? nullptr : water->get_if<std::uint8_t>();
    const std::vector<std::uint8_t>* rcells = river == nullptr ? nullptr : river->get_if<std::uint8_t>();
    const std::vector<std::uint8_t>* fcells = flow_dir == nullptr ? nullptr : flow_dir->get_if<std::uint8_t>();
    const std::uint32_t w = elevation.width;
    const std::uint32_t h = elevation.height;
    // Steps 1 to 3 per cell (and step 4 without `river`), then step 4.
    std::vector<Rgb> colour(e.size());
    if (drawn != nullptr) {
        drawn->assign(e.size(), 0U);
    }
    for (std::size_t i = 0; i < e.size(); ++i) {
        if (wcells != nullptr) {
            const std::uint8_t cat = (*wcells)[i];
            if (cat == 2U) {
                colour[i] = Rgb{64, 128, 204};
                if (drawn != nullptr) (*drawn)[i] = 1U;
                continue;
            }
            if (cat == 1U && rcells == nullptr) {
                colour[i] = Rgb{52, 100, 200};
                if (drawn != nullptr) (*drawn)[i] = 1U;
                continue;
            }
        }
        const std::int64_t v = e[i];
        if (v < 0) {
            colour[i] = hypsometric_ocean_colour(-v);
            continue;
        }
        const auto x = static_cast<std::uint32_t>(i % w);
        const auto y = static_cast<std::uint32_t>(i / w);
        colour[i] = shade_colour(land_colour(i, v), hillshade_factor(e, w, h, x, y));
    }
    if (wcells != nullptr && rcells != nullptr) {
        paint_rivers(colour, w, h, *wcells, *rcells, fcells, drawn);
    }
    return colour;
}

template <class LandColour>
std::vector<std::uint8_t> render_shaded(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                        const db::LayerGrid* river, const db::LayerGrid* flow_dir, std::uint32_t scale,
                                        const LandColour& land_colour, const char* scale_problem) {
    std::vector<std::uint8_t> rgb(buffer_size(elevation.width, elevation.height, scale, scale_problem));
    const std::vector<Rgb> colour = shaded_colours(elevation, water, river, flow_dir, land_colour);
    fill_buffer(rgb, elevation.width, elevation.height, scale, [&](std::size_t i) { return colour[i]; });
    return rgb;
}

}  // namespace

std::vector<std::uint8_t> render_composite_map(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                               const db::LayerGrid* river, const db::LayerGrid* flow_dir,
                                               std::uint32_t scale) {
    if (composite_map_problem(elevation, water, river, flow_dir)) {
        fatal("render_composite_map: unsuitable layers");
    }
    return render_shaded(
        elevation, water, river, flow_dir, scale,
        [](std::size_t, std::int64_t v) { return hypsometric_land_colour(v); },
        "render_composite_map: scale must be 1 to 16");
}

std::optional<std::string> composite_biomes_problem(const db::LayerGrid& elevation, const db::LayerGrid& biome,
                                                    const db::LayerGrid* water, const db::LayerGrid* river,
                                                    const db::LayerGrid* flow_dir) {
    if (std::optional<std::string> p = composite_map_problem(elevation, water, river, flow_dir)) {
        return p;
    }
    return u8_companion_problem(&biome, "biome", elevation);
}

std::vector<std::uint8_t> render_composite_biomes(const db::LayerGrid& elevation, const db::LayerGrid& biome,
                                                  std::span<const BiomeColour> biome_table,
                                                  const db::LayerGrid* water, const db::LayerGrid* river,
                                                  const db::LayerGrid* flow_dir, std::uint32_t scale) {
    if (composite_biomes_problem(elevation, biome, water, river, flow_dir)) {
        fatal("render_composite_biomes: unsuitable layers");
    }
    std::array<Rgb, 256> colours{};
    for (std::size_t v = 0; v < colours.size(); ++v) {
        colours[v] = biome_colour(biome_table, v);
    }
    const std::vector<std::uint8_t>& codes = *biome.get_if<std::uint8_t>();
    return render_shaded(
        elevation, water, river, flow_dir, scale, [&](std::size_t i, std::int64_t) { return colours[codes[i]]; },
        "render_composite_biomes: scale must be 1 to 16");
}

std::optional<std::string> composite_regions_problem(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                                     const db::LayerGrid* river, const db::LayerGrid* flow_dir,
                                                     const db::LayerGrid& biome, const db::LayerGrid& region) {
    if (std::optional<std::string> p = composite_biomes_problem(elevation, biome, water, river, flow_dir)) {
        return p;
    }
    if (!db::is_valid_layer_grid(region)) {
        return std::string("layer 'region' is not a valid grid");
    }
    if (region.type() != db::ElementType::u16) {
        return "layer 'region' is " + std::string(db::element_type_name(region.type())) + ", expected u16";
    }
    if (region.width != elevation.width || region.height != elevation.height) {
        return std::string("layer 'region' is not the size of layer 'elevation'");
    }
    return std::nullopt;
}

namespace detail {

std::size_t composite_buffer_size(std::uint32_t w, std::uint32_t h, std::uint32_t scale, const char* what) {
    return buffer_size(w, h, scale, what);
}

RegionsCells regions_composite_cells(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                     const db::LayerGrid* river, const db::LayerGrid* flow_dir,
                                     const db::LayerGrid& biome, const db::LayerGrid& region,
                                     std::span<const BiomeColour> biome_table) {
    std::array<Rgb, 256> colours{};
    for (std::size_t v = 0; v < colours.size(); ++v) {
        colours[v] = biome_colour(biome_table, v);
    }
    const std::vector<std::uint8_t>& codes = *biome.get_if<std::uint8_t>();
    RegionsCells out;
    out.colour = shaded_colours(
        elevation, water, river, flow_dir, [&](std::size_t i, std::int64_t) { return colours[codes[i]]; },
        &out.drawn);
    out.border.assign(out.colour.size(), 0U);
    // P0-04 amendment 18 (e): a cell with region v > 0 whose E or S
    // neighbour has a region w > 0, w != v, becomes black.
    const std::vector<std::uint16_t>& ids = *region.get_if<std::uint16_t>();
    const std::size_t w = elevation.width;
    const std::size_t h = elevation.height;
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            const std::size_t i = y * w + x;
            const std::uint16_t v = ids[i];
            if (v == 0U) {
                continue;
            }
            const bool east = x + 1U < w && ids[i + 1U] != 0U && ids[i + 1U] != v;
            const bool south = y + 1U < h && ids[i + w] != 0U && ids[i + w] != v;
            if (east || south) {
                out.colour[i] = Rgb{0, 0, 0};
                out.border[i] = 1U;
            }
        }
    }
    return out;
}

}  // namespace detail

std::vector<std::uint8_t> render_composite_regions(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                                   const db::LayerGrid* river, const db::LayerGrid* flow_dir,
                                                   const db::LayerGrid& biome, const db::LayerGrid& region,
                                                   std::uint32_t scale, std::span<const BiomeColour> biome_table) {
    if (composite_regions_problem(elevation, water, river, flow_dir, biome, region)) {
        fatal("render_composite_regions: unsuitable layers");
    }
    std::vector<std::uint8_t> rgb(
        buffer_size(elevation.width, elevation.height, scale, "render_composite_regions: scale must be 1 to 16"));
    const detail::RegionsCells cells =
        detail::regions_composite_cells(elevation, water, river, flow_dir, biome, region, biome_table);
    fill_buffer(rgb, elevation.width, elevation.height, scale, [&](std::size_t i) { return cells.colour[i]; });
    return rgb;
}

}  // namespace worldgen::inspect
