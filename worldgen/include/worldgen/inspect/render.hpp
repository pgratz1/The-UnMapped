#pragma once

// Layer rendering for worldgen-cli render (spec P0-04 section 6, P1-00
// section 10.2, P0-04 amendment 16b).
//
// The contract is the RGB pixel buffer returned by render_rgb and
// render_composite_map, not the PNG bytes (which depend on the image
// writer's version): the same grids, palette or composite and scale give the
// same buffer on every platform (P0-04 10). Every palette is an exact integer
// function of the cell value (and, for gray and signed, of the layer's value
// range).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/db/layer_blob.hpp"

namespace worldgen::inspect {

// --- Palettes (P0-04 6, P1-00 10.2) ------------------------------------------

// A palette. The --palette name of each is given by palette_name.
enum class Palette : std::uint8_t {
    gray = 0,             // "gray": min-max gray (P0-04 6); suits every layer
    hypsometric = 1,      // "hypsometric": elevation in decimetres (P1-00 10.2)
    water = 2,            // "water": water category 0..3
    river = 3,            // "river": river class 0..15
    log2 = 4,             // "log2": gray by bit length (flow_accum)
    direction = 5,        // "direction": D8 code 0..8 (flow_dir)
    ids = 6,              // "ids": hashed colour per id (lake, later region)
    signed_distance = 7,  // "signed": signed distance (coast_dist)
};

// Every palette in enumerator order (for --help text and tests).
inline constexpr std::array<Palette, 8> ALL_PALETTES = {
    Palette::gray,      Palette::hypsometric, Palette::water, Palette::river,
    Palette::log2,      Palette::direction,   Palette::ids,   Palette::signed_distance,
};

// The --palette text of a palette: "gray", "hypsometric", "water", "river",
// "log2", "direction", "ids", "signed" (P1-00 10.2).
[[nodiscard]] std::string_view palette_name(Palette palette) noexcept;

// The palette for a --palette value other than "auto" (one of the names of
// palette_name, exact case), or std::nullopt for an unknown name. "biome"
// (P1-00 10.2, second pass) is not known yet: it needs table `biome`.
[[nodiscard]] std::optional<Palette> palette_from_name(std::string_view name) noexcept;

// The palette registered for a layer name, used by --palette auto (P1-00
// 10.2, P0-04 amendment 16b): elevation -> hypsometric, water -> water,
// river -> river, flow_accum -> log2, flow_dir -> direction, lake -> ids,
// coast_dist -> signed; every other layer (water_depth included) -> gray.
[[nodiscard]] Palette registered_palette(std::string_view layer_name) noexcept;

// Whether a palette suits a grid (P0-04 amendment 16b: "a named palette
// that does not suit the layer's element type or values is a usage error").
// Reading chosen for P1-00 10.2, which does not list the types:
//   gray        every element type and value
//   hypsometric i16
//   water       u8 (values outside 0..3 have their own colour)
//   river       u8 with every value at most 15
//   log2        any unsigned type (u8, u16, u32, u64)
//   direction   u8 (values outside 0..8 have their own colour)
//   ids         any unsigned type
//   signed      any signed type (i8, i16, i32, i64)
// The grid must be valid (db::is_valid_layer_grid); an invalid grid suits
// no palette.
[[nodiscard]] bool palette_suits(Palette palette, const db::LayerGrid& grid) noexcept;

// What a palette needs, for a usage message: e.g. "an i16 layer",
// "a u8 layer with values 0 to 15" (wording of palette_suits).
[[nodiscard]] std::string_view palette_requirement(Palette palette) noexcept;

// The palette --palette auto uses for a layer: registered_palette(name) if
// it suits the grid, otherwise gray (reading chosen: auto never fails, so a
// catalogue-named layer of an unexpected type still renders).
[[nodiscard]] Palette auto_palette(std::string_view layer_name, const db::LayerGrid& grid) noexcept;

// An 8-bit RGB colour.
struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    friend constexpr bool operator==(const Rgb&, const Rgb&) noexcept = default;
};

// One stop of a colour ramp: at value t, colour c.
struct RampStop {
    std::int64_t t = 0;
    Rgb colour;
};

// The interpolation rule of P1-00 10.2. For stops (t_k, c_k) with t
// strictly ascending and t_k <= t < t_{k+1}, each channel is
//   c_k + floor((2 (c_{k+1} - c_k)(t - t_k) + (t_{k+1} - t_k)) / (2 (t_{k+1} - t_k)))
// (round half up, floor division on a possibly negative numerator). Below
// the first stop: the first colour; at or above the last stop: the last
// colour. FatalError if stops is empty, t is not strictly ascending, or two
// adjacent stops are 2^40 or more apart (bound that keeps the numerator in
// int64).
[[nodiscard]] Rgb ramp_colour(std::span<const RampStop> stops, std::int64_t t);

// The land stops of hypsometric, on t = elevation in decimetres (P1-00 10.2).
inline constexpr std::array<RampStop, 8> HYPSOMETRIC_LAND_STOPS = {{
    {0, {70, 120, 64}},
    {500, {104, 146, 78}},
    {2000, {150, 168, 96}},
    {5000, {178, 160, 108}},
    {10000, {150, 120, 86}},
    {15000, {128, 106, 92}},
    {20000, {196, 192, 188}},
    {30000, {250, 250, 250}},
}};

// The ocean stops of hypsometric, on t = depth in decimetres (P1-00 10.2).
inline constexpr std::array<RampStop, 5> HYPSOMETRIC_OCEAN_STOPS = {{
    {0, {120, 170, 205}},
    {200, {88, 140, 190}},
    {1000, {56, 104, 168}},
    {5000, {32, 66, 130}},
    {30000, {16, 34, 82}},
}};

// ramp_colour(HYPSOMETRIC_LAND_STOPS, t) (P1-00 10.2).
[[nodiscard]] Rgb hypsometric_land_colour(std::int64_t t);
// ramp_colour(HYPSOMETRIC_OCEAN_STOPS, depth) (P1-00 10.2).
[[nodiscard]] Rgb hypsometric_ocean_colour(std::int64_t depth);
// hypsometric (P1-00 10.2): e > 0 -> land colour of t = e; e <= 0 -> ocean
// colour of t = -e (e = 0 is the ocean colour of depth 0). Any int64.
[[nodiscard]] Rgb hypsometric_colour(std::int64_t e);

// water (P1-00 10.2): 0 (214, 200, 160); 1 (52, 100, 200); 2 (64, 128, 204);
// 3 (24, 56, 120); any other value (255, 0, 255).
[[nodiscard]] Rgb water_colour(std::uint64_t v) noexcept;

// river (P1-00 10.2): 0 (16, 16, 16); 1 (120, 170, 230); 2 (100, 150, 225);
// 3 (80, 130, 220); 4 (60, 110, 210); 5 (45, 90, 200); 6 to 15 (30, 70, 190).
// Reading chosen for values above 15, which 10.2 does not define:
// (255, 0, 255), as in water (palette_suits rejects such a layer for a
// named --palette river).
[[nodiscard]] Rgb river_colour(std::uint64_t v) noexcept;

// log2 (P1-00 10.2): g = min(255, 11 * bit_length(v)), bit_length 0 for 0
// (P1-00 amendment A1); pixel (g, g, g).
[[nodiscard]] Rgb log2_colour(std::uint64_t v) noexcept;

// direction (P1-00 10.2): 0 (230, 25, 75); 1 (245, 130, 48); 2 (255, 225, 25);
// 3 (210, 245, 60); 4 (60, 180, 75); 5 (70, 240, 240); 6 (0, 130, 200);
// 7 (145, 30, 180); 8 (0, 0, 0); any other value (255, 255, 255).
[[nodiscard]] Rgb direction_colour(std::uint64_t v) noexcept;

// ids (P1-00 10.2): 0 -> (0, 0, 0); v >= 1: h = mix64(v), channel j
// (0 red, 1 green, 2 blue) = 64 + floor(((h >> 8j) & 0xFF) * 191 / 255).
[[nodiscard]] Rgb ids_colour(std::uint64_t v) noexcept;

// The scale of the signed palette for a layer (P1-00 10.2): P = max(1,
// largest positive value), M = max(1, -(smallest negative value)), both as
// uint64 so that every i64 value is exact.
struct SignedScale {
    std::uint64_t p = 1;
    std::uint64_t m = 1;
};

// The SignedScale of a valid grid of a signed element type. FatalError if
// the grid is invalid or of an unsigned type.
[[nodiscard]] SignedScale signed_scale(const db::LayerGrid& grid);

// signed (P1-00 10.2): v >= 0 -> (g, g, g), g = gray_level(v, P);
// v < 0 -> (0, 0, 255 - floor(3 gray_level(-v, M) / 4)). FatalError if
// v > P or -v > M.
[[nodiscard]] Rgb signed_colour(std::int64_t v, SignedScale scale);

// --- Gray (P0-04 6) and the render buffer ------------------------------------

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
// scale x scale block at pixel (scale x, scale y) (nearest neighbour). Each
// cell's colour is the palette's colour of its value: gray over
// value_range, signed over signed_scale, every other palette per value.
// FatalError if the grid is invalid, the palette does not suit it
// (palette_suits), scale is outside 1..16, or the buffer size does not fit
// in std::size_t.
[[nodiscard]] std::vector<std::uint8_t> render_rgb(const db::LayerGrid& grid, Palette palette,
                                                   std::uint32_t scale);

// The byte size of render_rgb's buffer: 3 (width scale) (height scale).
// FatalError if the grid is invalid, scale is outside 1..16, or the size
// does not fit in std::size_t.
[[nodiscard]] std::size_t render_rgb_size(const db::LayerGrid& grid, std::uint32_t scale);

// render_rgb into a caller-supplied buffer of exactly render_rgb_size(grid,
// scale) bytes, so that a caller holding its own image memory (the viewer)
// avoids a second copy. Every byte of out is written, with the same content
// as the returning overload. FatalError in the same cases, and if out.size()
// is not render_rgb_size(grid, scale).
void render_rgb(const db::LayerGrid& grid, Palette palette, std::uint32_t scale, std::span<std::uint8_t> out);

// --- Composites (P1-00 10.2, P0-04 amendment 16b) ----------------------------

// A composite image built from several layers.
enum class Composite : std::uint8_t {
    map = 0,  // "map": shaded hypsometric relief with water overlays
};

// The --composite text of a composite: "map".
[[nodiscard]] std::string_view composite_name(Composite composite) noexcept;

// The composite for a --composite value, or std::nullopt for an unknown name.
[[nodiscard]] std::optional<Composite> composite_from_name(std::string_view name) noexcept;

// The hillshade factor of the composite map at cell (x, y) of an i16
// elevation grid of width w and height h (P1-00 10.2 step 2): with
// e'(x, y) = max(e, 0) and coordinates clamped to the map,
// gx = e'(x+1, y) - e'(x-1, y), gy = e'(x, y+1) - e'(x, y-1),
// f = clamp(256 + floor_div(gx + gy, 2), 160, 352). FatalError if
// cells.size() != w * h or (x, y) is outside the map.
[[nodiscard]] std::uint32_t hillshade_factor(std::span<const std::int16_t> cells, std::uint32_t w,
                                             std::uint32_t h, std::uint32_t x, std::uint32_t y);

// A colour shaded by factor f (P1-00 10.2 step 2): each channel c becomes
// min(255, (c f + 128) >> 8).
[[nodiscard]] Rgb shade_colour(Rgb c, std::uint32_t f) noexcept;

// Why the layers of the composite map cannot be used, or std::nullopt if
// they can (P1-00 10.2): elevation must be a valid i16 grid; water and
// river, when given (non-null), valid u8 grids of the same width and
// height. The text names the layer, e.g. "layer 'water' is i16, expected
// u8".
[[nodiscard]] std::optional<std::string> composite_map_problem(const db::LayerGrid& elevation,
                                                               const db::LayerGrid* water,
                                                               const db::LayerGrid* river);

// The RGB buffer of the composite map (P1-00 10.2), laid out as render_rgb.
// Per cell, with e the elevation:
//   1. e < 0: hypsometric_ocean_colour(-e);
//   2. otherwise (e = 0 included): shade_colour(hypsometric_land_colour(e),
//      hillshade_factor(...));
//   3. if water is given: a cell with water 2 is (64, 128, 204); a cell with
//      water 1 is river_colour of its river class, or (52, 100, 200) when
//      river is not given. Step 3 replaces the colour of steps 1 and 2
//      whatever e is. river without water is not used.
// FatalError if composite_map_problem reports a problem or scale is outside
// 1..16 or the buffer size does not fit in std::size_t.
[[nodiscard]] std::vector<std::uint8_t> render_composite_map(const db::LayerGrid& elevation,
                                                             const db::LayerGrid* water,
                                                             const db::LayerGrid* river, std::uint32_t scale);

}  // namespace worldgen::inspect
