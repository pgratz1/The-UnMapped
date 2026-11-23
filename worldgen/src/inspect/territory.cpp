// Civilization colours and the territory image (spec P4-01 5.5, 5.6; P4-03
// 7, 8 and 9.2).

#include "worldgen/inspect/territory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/db/schema.hpp"
#include "render_internal.hpp"

namespace worldgen::inspect {

namespace {

// The four colours of each shipped race key (P4-03 7.2), entry k mod 4 for
// the civilization of ordinal k; entry 0 is the race's base colour.
struct KeyColours {
    std::string_view key;
    std::array<Rgb, 4> colours;
};
constexpr std::array<KeyColours, 6> RACE_COLOURS = {{
    {"elf", {{{40, 190, 70}, {150, 230, 150}, {0, 140, 40}, {130, 205, 40}}}},
    {"goblin", {{{230, 195, 20}, {250, 235, 110}, {170, 125, 0}, {205, 160, 60}}}},
    {"dwarf", {{{220, 105, 25}, {245, 165, 105}, {150, 65, 15}, {195, 80, 55}}}},
    {"faerie", {{{175, 70, 225}, {215, 160, 250}, {115, 35, 165}, {200, 110, 200}}}},
    {"human", {{{0, 190, 195}, {125, 235, 230}, {0, 120, 130}, {50, 215, 175}}}},
    {"orc", {{{205, 30, 30}, {240, 120, 110}, {135, 15, 20}, {230, 50, 80}}}},
}};

// The row of a race key, compared as bytes (no locale), or null.
const KeyColours* race_row(std::string_view race_key) noexcept {
    for (const KeyColours& k : RACE_COLOURS) {
        if (k.key == race_key) {
            return &k;
        }
    }
    return nullptr;
}

// The largest region value the u16 layer region can hold.
constexpr std::int64_t MAX_LAYER_REGION = 65535;

std::uint8_t hashed_channel(std::uint64_t h, unsigned j) noexcept {
    const std::uint64_t byte = (h >> (8U * j)) & 0xFFU;
    return static_cast<std::uint8_t>(48U + byte * 160U / 255U);
}

std::uint8_t light(std::uint8_t c) noexcept {
    const unsigned v = c;
    return static_cast<std::uint8_t>(v + (255U - v) * 8U / 16U);
}
std::uint8_t dark(std::uint8_t c) noexcept {
    return static_cast<std::uint8_t>(static_cast<unsigned>(c) * 10U / 16U);
}
std::uint8_t deep(std::uint8_t c) noexcept {
    return static_cast<std::uint8_t>(static_cast<unsigned>(c) * 7U / 16U);
}

// dim(C) of P4-03 8.2: each channel times 12 / 16, floored.
Rgb dim(Rgb c) noexcept {
    const auto d = [](std::uint8_t v) { return static_cast<std::uint8_t>(static_cast<unsigned>(v) * 12U / 16U); };
    return Rgb{d(c.r), d(c.g), d(c.b)};
}

// P4-01 5.5: the base colour of a key without a row.
Rgb hashed_base(std::string_view race_key) {
    const std::vector<std::uint64_t> words = encode_bytes(race_key);
    const std::uint64_t h = hash_words(TAG_COLOUR, words);
    return Rgb{hashed_channel(h, 0U), hashed_channel(h, 1U), hashed_channel(h, 2U)};
}

// The size of the fill and holder vectors (P4-01 5.3): min(largest region
// id, 65535) + 1, after the year check with the timeline's message.
std::size_t region_vector_size(const history::Timeline& timeline, std::int64_t year) {
    if (!timeline.valid_year(year)) {  // also with no regions; the timeline's message
        throw FatalError("timeline: year " + db::format_i64(year) + " outside 0 to " +
                         db::format_i64(timeline.end_year()));
    }
    std::int64_t largest = 0;
    for (const std::int64_t r : timeline.region_ids()) {
        if (r > largest && r <= MAX_LAYER_REGION) {
            largest = r;
        }
    }
    return static_cast<std::size_t>(largest) + 1U;
}

// holders[v], 0 when v is past the end (P4-03 8.2).
std::int64_t holder_of(std::span<const std::int64_t> holders, std::uint16_t v) noexcept {
    return v < holders.size() ? holders[v] : 0;
}

// The pixel of one cell (P4-01 5.6, P4-03 8.2).
Rgb territory_pixel(const TerritoryBase::Cell& cell, std::span<const std::optional<Rgb>> fill,
                    std::span<const std::int64_t> holders) noexcept {
    if (cell.kind == TerritoryBase::KIND_PLAIN || cell.region >= fill.size()) {
        return cell.base;
    }
    const std::optional<Rgb>& colour = fill[cell.region];
    if (!colour) {
        return cell.base;
    }
    if (cell.kind == TerritoryBase::KIND_FILLABLE) {
        return shade_colour(*colour, cell.factor);
    }
    // Inner border candidate: every side held by the same civilization.
    const std::int64_t c = holder_of(holders, cell.region);
    if (c == 0 || (cell.east != 0U && holder_of(holders, cell.east) != c) ||
        (cell.south != 0U && holder_of(holders, cell.south) != c)) {
        return cell.base;
    }
    return shade_colour(dim(*colour), cell.factor);
}

}  // namespace

Rgb race_base_colour(std::string_view race_key) {
    if (const KeyColours* row = race_row(race_key)) {
        return row->colours[0];
    }
    return hashed_base(race_key);
}

Rgb civilization_shade(Rgb base, std::int64_t ordinal) {
    if (ordinal < 0) {
        fatal("civilization_shade: negative ordinal");
    }
    switch (ordinal % 4) {
        case 1: return Rgb{light(base.r), light(base.g), light(base.b)};
        case 2: return Rgb{dark(base.r), dark(base.g), dark(base.b)};
        case 3: return Rgb{deep(base.r), deep(base.g), deep(base.b)};
        default: return base;
    }
}

Rgb race_civilization_colour(std::string_view race_key, std::int64_t ordinal) {
    if (ordinal < 0) {
        fatal("race_civilization_colour: negative ordinal");
    }
    if (const KeyColours* row = race_row(race_key)) {
        return row->colours[static_cast<std::size_t>(ordinal % 4)];
    }
    return civilization_shade(hashed_base(race_key), ordinal);
}

std::vector<Rgb> civilization_colours(const history::HistoryTables& tables) {
    std::vector<Rgb> out;
    out.reserve(tables.civilizations.size());
    std::map<std::int64_t, std::int64_t> seen;  // race id -> civilizations so far (ordered map)
    for (const db::CivilizationRow& c : tables.civilizations) {
        const db::RaceRow* race = nullptr;
        for (const db::RaceRow& r : tables.races) {
            if (r.id == c.race) {
                race = &r;
                break;
            }
        }
        if (race == nullptr) {
            throw FatalError("civilization_colours: civilization " + db::format_i64(c.id) + ": race " +
                             db::format_i64(c.race) + " is not a race");
        }
        std::int64_t& k = seen[c.race];
        out.push_back(race_civilization_colour(race->key, k));
        ++k;
    }
    return out;
}

std::vector<std::optional<Rgb>> territory_fill(const history::Timeline& timeline, std::span<const Rgb> colours,
                                               std::int64_t year) {
    std::vector<std::optional<Rgb>> fill(region_vector_size(timeline, year));
    for (const std::int64_t r : timeline.region_ids()) {
        const history::RegionHolding h = timeline.region_holder(r, year);
        if (h.civ == 0 || r < 1 || r > MAX_LAYER_REGION) {
            continue;
        }
        if (h.civ < 1 || static_cast<std::uint64_t>(h.civ) > static_cast<std::uint64_t>(colours.size())) {
            throw FatalError("territory_fill: no colour for civilization " + db::format_i64(h.civ));
        }
        fill[static_cast<std::size_t>(r)] = colours[static_cast<std::size_t>(h.civ - 1)];
    }
    return fill;
}

std::vector<std::int64_t> territory_holders(const history::Timeline& timeline, std::int64_t year) {
    std::vector<std::int64_t> holders(region_vector_size(timeline, year), 0);
    for (const std::int64_t r : timeline.region_ids()) {
        if (r < 1 || r > MAX_LAYER_REGION) {
            continue;
        }
        holders[static_cast<std::size_t>(r)] = timeline.region_holder(r, year).civ;
    }
    return holders;
}

TerritoryBase TerritoryBase::build(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                   const db::LayerGrid* river, const db::LayerGrid* flow_dir,
                                   const db::LayerGrid& biome, const db::LayerGrid& region,
                                   std::span<const BiomeColour> biome_table) {
    if (composite_regions_problem(elevation, water, river, flow_dir, biome, region)) {
        fatal("TerritoryBase::build: unsuitable layers");
    }
    const detail::RegionsCells cells =
        detail::regions_composite_cells(elevation, water, river, flow_dir, biome, region, biome_table);
    const std::vector<std::int16_t>& e = *elevation.get_if<std::int16_t>();
    const std::vector<std::uint16_t>& ids = *region.get_if<std::uint16_t>();
    TerritoryBase base;
    base.width_ = elevation.width;
    base.height_ = elevation.height;
    base.cells_.resize(e.size());
    const std::uint32_t w = elevation.width;
    for (std::size_t i = 0; i < e.size(); ++i) {
        Cell& c = base.cells_[i];
        c.base = cells.colour[i];
        // P4-01 5.6 item 1 and P4-03 8.2: v > 0, e >= 0 (shaded land), not
        // drawn by step 3 or 4; fillable when not a border cell, an inner
        // border candidate when it is one.
        if (ids[i] == 0U || e[i] < 0 || cells.drawn[i] != 0U) {
            continue;
        }
        const auto x = static_cast<std::uint32_t>(i % w);
        const auto y = static_cast<std::uint32_t>(i / w);
        c.factor = static_cast<std::uint16_t>(hillshade_factor(e, w, elevation.height, x, y));
        c.region = ids[i];
        if (cells.border[i] == 0U) {
            c.kind = KIND_FILLABLE;
        } else {
            c.kind = KIND_INNER_BORDER;
            c.east = cells.east[i];
            c.south = cells.south[i];
        }
    }
    return base;
}

std::size_t territory_rgb_size(const TerritoryBase& base, std::uint32_t scale) {
    return detail::composite_buffer_size(base.width(), base.height(), scale,
                                         "render_territory: scale must be 1 to 16");
}

void render_territory(const TerritoryBase& base, std::span<const std::optional<Rgb>> fill,
                      std::span<const std::int64_t> holders, std::uint32_t scale, std::span<std::uint8_t> out) {
    if (out.size() != territory_rgb_size(base, scale)) {
        fatal("render_territory: output buffer of the wrong size");
    }
    const std::span<const TerritoryBase::Cell> cells = base.cells();
    detail::fill_buffer(out, base.width(), base.height(), scale,
                        [&](std::size_t i) { return territory_pixel(cells[i], fill, holders); });
}

std::vector<std::uint8_t> render_territory(const TerritoryBase& base, std::span<const std::optional<Rgb>> fill,
                                           std::span<const std::int64_t> holders, std::uint32_t scale) {
    std::vector<std::uint8_t> out(territory_rgb_size(base, scale));
    render_territory(base, fill, holders, scale, out);
    return out;
}

}  // namespace worldgen::inspect
