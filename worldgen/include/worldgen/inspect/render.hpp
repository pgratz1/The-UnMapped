#pragma once

// Layer rendering for worldgen-cli render (spec P0-04 section 6).
//
// The contract is the RGB pixel buffer returned by render_rgb, not the PNG
// bytes (which depend on the image writer's version): the same grid,
// palette and scale give the same buffer on every platform (P0-04 10).

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "worldgen/db/layer_blob.hpp"

namespace worldgen::inspect {

// A palette (P0-04 6). P0 has only gray; P1 adds per-layer palettes.
enum class Palette : std::uint8_t {
    gray = 0,
};

// The --palette text of a palette: "gray".
[[nodiscard]] std::string_view palette_name(Palette palette) noexcept;

// The palette for a --palette value other than "auto" ("gray"), or
// std::nullopt for an unknown name (P0-04 6).
[[nodiscard]] std::optional<Palette> palette_from_name(std::string_view name) noexcept;

// The palette registered for a layer, used by --palette auto (P0-04 6). In
// P0 every layer's palette is gray.
[[nodiscard]] Palette registered_palette(std::string_view layer_name) noexcept;

// Allowed --scale values (P0-04 6).
inline constexpr std::uint32_t MIN_RENDER_SCALE = 1U;
inline constexpr std::uint32_t MAX_RENDER_SCALE = 16U;

// The minimum and maximum cell value of a grid, compared as values of its
// element type (P0-04 6 gray, 7 min/max). Unsigned and signed alternatives
// are reported separately so that the full u64 and i64 ranges are exact.
struct ValueRange {
    bool is_signed = false;
    std::int64_t min_signed = 0;    // set when is_signed
    std::int64_t max_signed = 0;
    std::uint64_t min_unsigned = 0;  // set when !is_signed
    std::uint64_t max_unsigned = 0;
};

// Min and max over all cells of a valid, non-empty grid (P0-04 6, 7).
// FatalError if the grid is invalid (db::is_valid_layer_grid).
[[nodiscard]] ValueRange value_range(const db::LayerGrid& grid);

// The gray level of value v for a layer whose values span [min, max], on the
// unsigned distance d = v - min and range r = max - min (both exact as
// uint64 for every element type): 0 if r = 0, else
// floor((510 d + r) / (2 r)), i.e. d * 255 / r rounded half up, computed on
// 128-bit integers (P0-04 6). FatalError if d > r.
[[nodiscard]] std::uint8_t gray_level(std::uint64_t d, std::uint64_t r);

// The 8-bit RGB pixel buffer of a layer (P0-04 6): (width * scale) x
// (height * scale) pixels, 3 bytes each (R, G, B), rows top (y = 0, north)
// to bottom, each row left (x = 0, west) to right; cell (x, y) fills the
// scale x scale block at pixel (scale x, scale y) (nearest neighbour). With
// Palette::gray every pixel is (g, g, g), g = gray_level over value_range.
// FatalError if the grid is invalid, scale is outside 1..16, or the buffer
// size does not fit in std::size_t.
[[nodiscard]] std::vector<std::uint8_t> render_rgb(const db::LayerGrid& grid, Palette palette,
                                                   std::uint32_t scale);

}  // namespace worldgen::inspect
