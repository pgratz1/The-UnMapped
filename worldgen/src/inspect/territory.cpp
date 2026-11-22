// Civilization colours and the territory image (spec P4-01 5.5, 5.6).

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

struct KeyColour {
    std::string_view key;
    Rgb colour;
};

// The six bases of 5.5 (interim, P4-01 14 item 1).
constexpr std::array<KeyColour, 6> BASES = {{
    {"elf", {52, 150, 70}},
    {"goblin", {190, 160, 50}},
    {"dwarf", {160, 82, 45}},
    {"faerie", {150, 90, 200}},
    {"human", {50, 105, 200}},
    {"orc", {200, 40, 35}},
}};

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

}  // namespace

Rgb race_base_colour(std::string_view race_key) {
    for (const KeyColour& k : BASES) {
        if (k.key == race_key) {  // byte comparison, no locale
            return k.colour;
        }
    }
    const std::vector<std::uint64_t> words = encode_bytes(race_key);
    const std::uint64_t h = hash_words(TAG_COLOUR, words);
    return Rgb{hashed_channel(h, 0U), hashed_channel(h, 1U), hashed_channel(h, 2U)};
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
        out.push_back(civilization_shade(race_base_colour(race->key), k));
        ++k;
    }
    return out;
}

std::vector<std::optional<Rgb>> territory_fill(const history::Timeline& timeline, std::span<const Rgb> colours,
                                               std::int64_t year) {
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
    std::vector<std::optional<Rgb>> fill(static_cast<std::size_t>(largest) + 1U);
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
        // 5.6 item 1: v > 0, e >= 0 (shaded land), not drawn by step 3 or 4,
        // not a border cell.
        const bool fillable = ids[i] > 0U && e[i] >= 0 && cells.drawn[i] == 0U && cells.border[i] == 0U;
        if (fillable) {
            const auto x = static_cast<std::uint32_t>(i % w);
            const auto y = static_cast<std::uint32_t>(i / w);
            c.factor = static_cast<std::uint16_t>(hillshade_factor(e, w, elevation.height, x, y));
            c.region = ids[i];
        }
    }
    return base;
}

std::size_t territory_rgb_size(const TerritoryBase& base, std::uint32_t scale) {
    return detail::composite_buffer_size(base.width(), base.height(), scale,
                                         "render_territory: scale must be 1 to 16");
}

void render_territory(const TerritoryBase& base, std::span<const std::optional<Rgb>> fill, std::uint32_t scale,
                      std::span<std::uint8_t> out) {
    if (out.size() != territory_rgb_size(base, scale)) {
        fatal("render_territory: output buffer of the wrong size");
    }
    const std::span<const TerritoryBase::Cell> cells = base.cells();
    const std::size_t w = base.width();
    const std::size_t h = base.height();
    const std::size_t s = scale;
    const std::size_t row_bytes = w * s * 3U;
    for (std::size_t y = 0; y < h; ++y) {
        std::uint8_t* row = out.data() + (y * s) * row_bytes;
        for (std::size_t x = 0; x < w; ++x) {
            const TerritoryBase::Cell& cell = cells[y * w + x];
            Rgb c = cell.base;
            if (cell.region != 0U && cell.region < fill.size()) {
                if (const std::optional<Rgb>& holder = fill[cell.region]) {
                    c = shade_colour(*holder, cell.factor);
                }
            }
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

std::vector<std::uint8_t> render_territory(const TerritoryBase& base, std::span<const std::optional<Rgb>> fill,
                                           std::uint32_t scale) {
    std::vector<std::uint8_t> out(territory_rgb_size(base, scale));
    render_territory(base, fill, scale, out);
    return out;
}

}  // namespace worldgen::inspect
