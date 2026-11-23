#pragma once

// Civilization colours and the year-dependent image `territory` (spec P4-01
// 5.3, 5.5, 5.6). Integer arithmetic only; the RGB buffer is part of the
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

// The base colour of a race key (5.5), compared as bytes: elf (52, 150, 70),
// goblin (190, 160, 50), dwarf (160, 82, 45), faerie (150, 90, 200), human
// (50, 105, 200), orc (200, 40, 35); any other key: h = hash_words(TAG_COLOUR,
// encode_bytes(key)), channel j (0 R, 1 G, 2 B) = 48 + floor(((h >> 8j) &
// 0xFF) 160 / 255).
[[nodiscard]] Rgb race_base_colour(std::string_view race_key);

// The shade of a base colour for ordinal k (5.5): s = k mod 4; s 0 base, 1
// light c + floor((255 - c) 8 / 16), 2 dark floor(c 10 / 16), 3 deep
// floor(c 7 / 16). FatalError when ordinal is negative.
[[nodiscard]] Rgb civilization_shade(Rgb base, std::int64_t ordinal);

// The colour of each civilization, index id - 1: civilization_shade of its
// race's base colour with ordinal k = the number of civilizations of the same
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

// The year-independent part of the territory image (5.6), built once: per
// cell at scale 1 the pixel B of the composite regions, the hillshade factor
// and the region value when the cell is fillable (else 0).
class TerritoryBase {
public:
    // FatalError when composite_regions_problem(...) reports a problem.
    [[nodiscard]] static TerritoryBase build(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                             const db::LayerGrid* river, const db::LayerGrid* flow_dir,
                                             const db::LayerGrid& biome, const db::LayerGrid& region,
                                             std::span<const BiomeColour> biome_table);
    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }

    struct Cell {
        Rgb base;                 // B(x, y)
        std::uint16_t factor = 0;  // hillshade factor (160..352) when fillable, else 0
        std::uint16_t region = 0;  // the region value when fillable, else 0
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

// The territory image (5.6), laid out as render_rgb: per cell, on a fillable
// cell whose region v < fill.size() has fill[v] = C, shade_colour(C, factor);
// otherwise B. FatalError: scale outside 1..16, size overflow, out.size() !=
// territory_rgb_size(base, scale).
[[nodiscard]] std::vector<std::uint8_t> render_territory(const TerritoryBase& base,
                                                         std::span<const std::optional<Rgb>> fill,
                                                         std::uint32_t scale);
void render_territory(const TerritoryBase& base, std::span<const std::optional<Rgb>> fill, std::uint32_t scale,
                      std::span<std::uint8_t> out);

}  // namespace worldgen::inspect
