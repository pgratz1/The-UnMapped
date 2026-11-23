#pragma once

// Civilization colours and the year-dependent image `territory` (spec P4-01
// 5.3, 5.5, 5.6, amended by P4-03 r4 7, 8 and 9.2, after P4-03a). Integer arithmetic only; the RGB buffer is part of the
// cross-platform contract, as every composite's (P0-04 10).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "worldgen/db/layer_blob.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/render.hpp"

namespace worldgen::inspect {

// The domain tag of a colour from a race key, "UNM:COLR" (P0-01 7.2 form).
inline constexpr std::uint64_t TAG_COLOUR = 0x524C4F433A4D4E55ULL;

// The base colour of a race key, compared as bytes: entry 0 of the table of
// P4-03 7.2 for the six shipped keys elf (40, 190, 70), goblin (230, 195,
// 20), dwarf (220, 105, 25), faerie (175, 70, 225), human (0, 190, 195) and
// orc (205, 30, 30); any other key (P4-01 5.5): h = hash_words(TAG_COLOUR,
// encode_bytes(key)), channel j (0 R, 1 G, 2 B) = 48 + floor(((h >> 8j) &
// 0xFF) 160 / 255).
[[nodiscard]] Rgb race_base_colour(std::string_view race_key);

// The shade of a base colour for ordinal k (P4-01 5.5; used for race keys
// without a row of P4-03 7.2): s = k mod 4; s 0 base, 1 light c +
// floor((255 - c) 8 / 16), 2 dark floor(c 10 / 16), 3 deep floor(c 7 / 16).
// FatalError when ordinal is negative.
[[nodiscard]] Rgb civilization_shade(Rgb base, std::int64_t ordinal);

// The colour of the civilization with ordinal k (>= 0) of race `race_key`:
// entry k mod 4 of P4-03 7.2 for the six shipped keys, else
// civilization_shade(race_base_colour(key), k). FatalError if k < 0.
[[nodiscard]] Rgb race_civilization_colour(std::string_view race_key, std::int64_t ordinal);

// The colour of each civilization, index id - 1: race_civilization_colour of
// its race's key with ordinal k = the number of civilizations of the same
// race and a smaller id. FatalError "civilization_colours: civilization <c>:
// race <r> is not a race" when the race row is missing (checked data never).
[[nodiscard]] std::vector<Rgb> civilization_colours(const history::HistoryTables& tables);

// The fill of the territory image at a year, indexed by region id: size =
// min(largest region id, 65535) + 1 (1 with no regions; a region id above
// 65535 or below 1 cannot appear in the u16 layer region and is left out);
// fill[r] = colours[c - 1] for each region r held by c at Y; [0] and free
// regions empty. FatalError as Timeline::region_holder for a bad year, and
// "territory_fill: no colour for civilization <c>" when colours is short.
[[nodiscard]] std::vector<std::optional<Rgb>> territory_fill(const history::Timeline& timeline,
                                                             std::span<const Rgb> colours, std::int64_t year);

// The holders at a year, indexed by region id with the size and id rules of
// territory_fill: entry the civilization holding the region at `year`, 0 when
// free; [0] = 0 (P4-03 9.2). FatalError for a year outside 0..E with the
// timeline's text.
[[nodiscard]] std::vector<std::int64_t> territory_holders(const history::Timeline& timeline, std::int64_t year);

// The year-independent part of the territory image (P4-01 5.6, P4-03 8.2),
// built once: per cell at scale 1 the pixel B of the composite regions, its
// kind, and for a fillable cell or an inner border candidate the hillshade
// factor, the region value and (candidate only) the neighbour values that
// make it a border cell.
class TerritoryBase {
public:
    // FatalError when composite_regions_problem(...) reports a problem.
    [[nodiscard]] static TerritoryBase build(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                             const db::LayerGrid* river, const db::LayerGrid* flow_dir,
                                             const db::LayerGrid& biome, const db::LayerGrid& region,
                                             std::span<const BiomeColour> biome_table);
    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }

    // Cell kinds.
    static constexpr std::uint8_t KIND_PLAIN = 0;         // drawn as B only
    static constexpr std::uint8_t KIND_FILLABLE = 1;      // filled when its region is held
    static constexpr std::uint8_t KIND_INNER_BORDER = 2;  // inner border candidate (P4-03 8.2)

    struct Cell {
        Rgb base;                  // B(x, y)
        Rgb half;                  // kind 2: half(U) of P4-03 r4 8.2 rule 3; else (0, 0, 0)
        std::uint8_t kind = 0;     // KIND_*
        std::uint16_t factor = 0;  // hillshade factor (160..352) when kind is 1 or 2, else 0
        std::uint16_t region = 0;  // v when kind is 1 or 2, else 0
        std::uint16_t east = 0;    // kind 2: the E neighbour's value when it makes the cell a border, else 0
        std::uint16_t south = 0;   // kind 2: the S neighbour's value when it makes the cell a border, else 0
    };
    [[nodiscard]] std::span<const Cell> cells() const noexcept { return cells_; }

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::vector<Cell> cells_;
};

// The byte size of the territory buffer at a scale: 3 (width scale)
// (height scale). FatalError when scale is outside 1..16 or the size
// overflows std::size_t.
[[nodiscard]] std::size_t territory_rgb_size(const TerritoryBase& base, std::uint32_t scale);

// The territory image (P4-01 5.6 with P4-03 r4 8.2), laid out as
// render_rgb. holders[v] is the holder of region v, 0 when free or v >=
// holders.size(); holders may be empty (then every inner border candidate is
// B, black). Per cell: on a fillable cell whose region v < fill.size() has
// fill[v] = C, shade_colour(C, factor); on an inner border candidate with a
// non-empty holders, when its region v and every other region w of it have
// the same holder c: c != 0 and fill[v] = C gives shade_colour(dim(C),
// factor), dim the channels times n / 16, floored, n = 10 when L(C) =
// floor((77 R + 150 G + 29 B) / 256) > 175 and 12 otherwise; c = 0 gives
// Cell::half; otherwise B. FatalError: scale outside 1..16, size overflow,
// out.size() != territory_rgb_size(base, scale).
[[nodiscard]] std::vector<std::uint8_t> render_territory(const TerritoryBase& base,
                                                         std::span<const std::optional<Rgb>> fill,
                                                         std::span<const std::int64_t> holders, std::uint32_t scale);
void render_territory(const TerritoryBase& base, std::span<const std::optional<Rgb>> fill,
                      std::span<const std::int64_t> holders, std::uint32_t scale, std::span<std::uint8_t> out);

// The territory image at a year from one timeline (P4-03 r4 9.2, audit A1):
// equal to render_territory(base, territory_fill(timeline, colours, year),
// territory_holders(timeline, year), scale[, out]), with the same FatalErrors
// in the same order (year, missing colour, then scale and size).
[[nodiscard]] std::vector<std::uint8_t> render_territory(const TerritoryBase& base, const history::Timeline& timeline,
                                                         std::span<const Rgb> colours, std::int64_t year,
                                                         std::uint32_t scale);
void render_territory(const TerritoryBase& base, const history::Timeline& timeline, std::span<const Rgb> colours,
                      std::int64_t year, std::uint32_t scale, std::span<std::uint8_t> out);

}  // namespace worldgen::inspect
