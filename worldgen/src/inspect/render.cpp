// Layer rendering (spec P0-04 section 6).

#include "worldgen/inspect/render.hpp"

#include <cstddef>
#include <limits>
#include <type_traits>
#include <variant>

#include "worldgen/core/error.hpp"
#include "worldgen/core/wide.hpp"

namespace worldgen::inspect {

std::string_view palette_name(Palette palette) noexcept {
    switch (palette) {
        case Palette::gray: return "gray";
    }
    return "gray";
}

std::optional<Palette> palette_from_name(std::string_view name) noexcept {
    if (name == "gray") {
        return Palette::gray;
    }
    return std::nullopt;
}

Palette registered_palette(std::string_view /*layer_name*/) noexcept {
    return Palette::gray;  // P0: every layer (P0-04 6)
}

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

std::vector<std::uint8_t> render_rgb(const db::LayerGrid& grid, Palette palette, std::uint32_t scale) {
    require_valid(grid, "render_rgb: invalid layer grid");
    if (scale < MIN_RENDER_SCALE || scale > MAX_RENDER_SCALE) {
        fatal("render_rgb: scale must be 1 to 16");
    }
    if (palette != Palette::gray) {
        fatal("render_rgb: unknown palette");
    }
    // At most 65536 * 16 = 2^20 pixels per side, so the pixel count is at
    // most 2^40 and the byte count 3 * 2^40: fits a 64-bit size_t.
    const std::uint64_t out_w = static_cast<std::uint64_t>(grid.width) * scale;
    const std::uint64_t out_h = static_cast<std::uint64_t>(grid.height) * scale;
    const std::uint64_t bytes = out_w * out_h * 3U;
    if (bytes > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        fatal("render_rgb: image too large");
    }
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(bytes));
    const std::size_t row_bytes = static_cast<std::size_t>(out_w) * 3U;

    std::visit(
        [&](const auto& cells) {
            using T = typename std::decay_t<decltype(cells)>::value_type;
            T lo{};
            T hi{};
            min_max(cells, lo, hi);
            const std::uint64_t r = distance(hi, lo);
            const std::size_t w = grid.width;
            const std::size_t h = grid.height;
            const std::size_t s = scale;
            for (std::size_t y = 0; y < h; ++y) {
                // Fill the first pixel row of this cell row, then copy it
                // to the other scale - 1 rows.
                std::uint8_t* row = rgb.data() + (y * s) * row_bytes;
                for (std::size_t x = 0; x < w; ++x) {
                    const std::uint8_t g = gray_level(distance(cells[y * w + x], lo), r);
                    std::uint8_t* px = row + x * s * 3U;
                    for (std::size_t i = 0; i < s; ++i) {
                        px[3U * i] = g;
                        px[3U * i + 1U] = g;
                        px[3U * i + 2U] = g;
                    }
                }
                for (std::size_t j = 1; j < s; ++j) {
                    std::uint8_t* dst = row + j * row_bytes;
                    for (std::size_t b = 0; b < row_bytes; ++b) {
                        dst[b] = row[b];
                    }
                }
            }
        },
        grid.values);
    return rgb;
}

}  // namespace worldgen::inspect
