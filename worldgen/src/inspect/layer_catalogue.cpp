// The layer catalogue in the library (spec P4-02).

#include "worldgen/inspect/layer_catalogue.hpp"

#include <array>
#include <limits>
#include <type_traits>
#include <variant>

#include "worldgen/db/schema.hpp"

namespace worldgen::inspect {

namespace {

// --- The table (P4-02 4.2, P1-00 4) ------------------------------------------------

constexpr std::int64_t U32_MAX = 4294967295;

constexpr std::array<SpecialValue, 4> WATER_SPECIAL = {{
    {0, "dry land"},
    {1, "river"},
    {2, "lake"},
    {3, "ocean"},
}};
constexpr std::array<SpecialValue, 1> WATER_DEPTH_SPECIAL = {{{0, "no standing water"}}};
constexpr std::array<SpecialValue, 1> FLOW_DIR_SPECIAL = {{{8, "none (ocean)"}}};
constexpr std::array<SpecialValue, 1> FLOW_ACCUM_SPECIAL = {{{0, "ocean"}}};
constexpr std::array<SpecialValue, 1> RIVER_SPECIAL = {{{0, "not a river"}}};
constexpr std::array<SpecialValue, 1> NONE_SPECIAL = {{{0, "none"}}};
constexpr std::array<SpecialValue, 1> REGION_SPECIAL = {{{0, "none (ocean)"}}};

constexpr std::string_view SCORE_UNIT = "score 0 to 255";

// Layer ids 1 to 19 of P1-00 3. unit strings are the viewer's legend titles,
// byte for byte (P4-02 4.2).
constexpr std::array<LayerSpec, 19> CATALOGUE = {{
    {"elevation", LayerGroup::terrain, db::ElementType::i16, -30000, 30000, false, 10, "m",
     "decimetres above sea level",
     "Height of the ground above sea level in decimetres; below 0 is ocean, above 0 is land, never 0.",
     {}, Palette::hypsometric},
    {"water", LayerGroup::water, db::ElementType::u8, 0, 3, false, 1, "",
     "category: 0 dry land, 1 river, 2 lake, 3 ocean",
     "Water category of the cell: 0 dry land, 1 river, 2 lake, 3 ocean.",
     WATER_SPECIAL, Palette::water},
    {"water_depth", LayerGroup::water, db::ElementType::u16, 0, 32768, false, 10, "m",
     "decimetres of standing water",
     "Depth of standing water in decimetres, below the sea or the lake level; 0 on land and river cells.",
     WATER_DEPTH_SPECIAL, Palette::gray},
    {"flow_dir", LayerGroup::water, db::ElementType::u8, 0, 8, false, 1, "",
     "D8 code of the downstream neighbour",
     "D8 code of the downstream neighbour, 0 east clockwise to 7 north-east; 8 for no downstream, on ocean cells "
     "only.",
     FLOW_DIR_SPECIAL, Palette::direction},
    {"flow_accum", LayerGroup::water, db::ElementType::u32, 0, U32_MAX, true, 1, "",
     "cells draining through the cell (x 256 m2)",
     "Cells draining through the cell, itself included, each 256 m2 of upstream area; 0 on ocean, at least 1 "
     "elsewhere.",
     FLOW_ACCUM_SPECIAL, Palette::log2},
    {"river", LayerGroup::water, db::ElementType::u8, 0, 15, false, 1, "",
     "river size class, 0 none",
     "River size class, each class doubling the upstream cells from the river threshold; 0 not a river, 15 "
     "open-ended.",
     RIVER_SPECIAL, Palette::river},
    {"lake", LayerGroup::water, db::ElementType::u32, 0, U32_MAX, false, 1, "",
     "lake id, 0 none",
     "Lake id, 1, 2, ... in the order of each lake's first cell in index order; 0 not a lake.",
     NONE_SPECIAL, Palette::ids},
    {"coast_dist", LayerGroup::water, db::ElementType::i16, -32767, 32767, false, 1, "m",
     "metres to the coast, < 0 at sea",
     "Chamfer distance to the sea coast in metres, positive on land and lakes, negative at sea; saturates at "
     "32767.",
     {}, Palette::signed_distance},
    {"temperature", LayerGroup::climate, db::ElementType::i16, -600, 500, false, 10, "°C",
     "annual mean, tenths of a degree Celsius",
     "Annual mean temperature at the water or land surface, in tenths of a degree Celsius (°C).",
     {}, Palette::temperature},
    {"rainfall", LayerGroup::climate, db::ElementType::u16, 0, 10000, false, 1, "mm/yr",
     "annual precipitation, millimetres",
     "Annual precipitation in millimetres, defined on the ocean too.",
     {}, Palette::rainfall},
    {"biome", LayerGroup::biomes, db::ElementType::u8, 1, 254, false, 1, "",
     "biome code of table biome",
     "Biome code of a row of table biome; 0 never appears in a finished layer.",
     {}, Palette::biome},
    {"fertility", LayerGroup::resources, db::ElementType::u8, 0, 255, false, 1, "/ 255", SCORE_UNIT,
     "Fertility score on an absolute scale, 0 none to 255 the best; 0 on ocean and lakes.",
     NONE_SPECIAL, Palette::score},
    {"ore", LayerGroup::resources, db::ElementType::u8, 0, 255, false, 1, "/ 255", SCORE_UNIT,
     "Ore score on an absolute scale, 0 none to 255 the best; 0 on ocean and lakes.",
     NONE_SPECIAL, Palette::score},
    {"timber", LayerGroup::resources, db::ElementType::u8, 0, 255, false, 1, "/ 255", SCORE_UNIT,
     "Timber score on an absolute scale, 0 none to 255 the best; 0 on ocean and lakes.",
     NONE_SPECIAL, Palette::score},
    {"fresh_water", LayerGroup::resources, db::ElementType::u8, 0, 255, false, 1, "/ 255", SCORE_UNIT,
     "Fresh water score on an absolute scale, 0 none to 255 the best; 0 on ocean, 255 on lakes.",
     NONE_SPECIAL, Palette::score},
    {"forage", LayerGroup::resources, db::ElementType::u8, 0, 255, false, 1, "/ 255", SCORE_UNIT,
     "Wild food score (fish, fowl, game, reeds), 0 none to 255 the best; 0 on ocean and lakes.",
     NONE_SPECIAL, Palette::score},
    {"habitability", LayerGroup::resources, db::ElementType::u8, 0, 255, false, 1, "/ 255", SCORE_UNIT,
     "Habitability score from the other scores, fresh water, slope, flooding and walk area; 0 on ocean and "
     "lakes.",
     NONE_SPECIAL, Palette::score},
    {"walk_area", LayerGroup::resources, db::ElementType::u16, 0, 65535, false, 1, "",
     "walkable area id, 0 none",
     "Id of the connected walkable area; 0 on water, on steep cells and in areas below the minimum size.",
     NONE_SPECIAL, Palette::ids},
    {"region", LayerGroup::regions, db::ElementType::u16, 0, 65535, false, 1, "",
     "region id of table region, 0 ocean",
     "Region id, a row of table region; 0 on ocean only, lakes belong to regions.",
     REGION_SPECIAL, Palette::ids},
}};

// --- Text helpers (P4-02 4.3 notation) ---------------------------------------------

std::uint32_t pack(Rgb c) noexcept {
    return (static_cast<std::uint32_t>(c.r) << 16U) | (static_cast<std::uint32_t>(c.g) << 8U) |
           static_cast<std::uint32_t>(c.b);
}

std::uint32_t gray(std::uint8_t g) noexcept {
    return pack(Rgb{g, g, g});
}

// |v| as uint64, exact for INT64_MIN.
std::uint64_t magnitude(std::int64_t v) noexcept {
    return v < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(v) : static_cast<std::uint64_t>(v);
}

// T(v): v / 10 with one decimal, sign first ("-0.5", "0.0").
std::string tenths(std::int64_t v) {
    const std::uint64_t m = magnitude(v);
    return (v < 0 ? std::string("-") : std::string()) + db::format_u64(m / 10U) + "." + db::format_u64(m % 10U);
}

// H(u): u / 100 with two decimals.
std::string hundredths(std::uint64_t u) {
    const std::uint64_t frac = u % 100U;
    return db::format_u64(u / 100U) + (frac < 10U ? ".0" : ".") + db::format_u64(frac);
}

// A(n) = H((256 n + 5000) / 10000): upstream area of n cells of 256 m2 in km2,
// round half up. With n = 10000 q + r the quotient is 256 q + (256 r + 5000)
// / 10000 exactly, which cannot overflow for any uint64 n.
std::string upstream_km2(std::uint64_t n) {
    const std::uint64_t q = n / 10000U;
    const std::uint64_t r = n % 10000U;
    return hundredths(256U * q + (256U * r + 5000U) / 10000U);
}

constexpr std::array<std::string_view, 8> D8_NAMES = {"E", "SE", "S", "SW", "W", "NW", "N", "NE"};

std::string direction_text(std::int64_t v) {
    if (v >= 0 && v < 8) {
        return std::string(D8_NAMES[static_cast<std::size_t>(v)]);
    }
    if (v == 8) {
        return "none (ocean)";
    }
    return "code " + db::format_i64(v);
}

std::string water_text(std::int64_t v) {
    switch (v) {
        case 0: return "dry land";
        case 1: return "river";
        case 2: return "lake";
        case 3: return "ocean";
        default: return "category " + db::format_i64(v);
    }
}

bool is_score(std::string_view name) noexcept {
    const LayerSpec* spec = find_layer_spec(name);
    return spec != nullptr && spec->group == LayerGroup::resources && spec->palette == Palette::score;
}

// --- Legend helpers -------------------------------------------------------------------

template <std::size_t N>
std::vector<std::uint32_t> stop_colours(const std::array<RampStop, N>& stops) {
    std::vector<std::uint32_t> colours;
    colours.reserve(N);
    for (const RampStop& stop : stops) {
        colours.push_back(pack(stop.colour));
    }
    return colours;
}

template <std::size_t N, class Label>
void add_stops(std::vector<LegendEntry>& out, const std::array<RampStop, N>& stops, Label label) {
    for (const RampStop& stop : stops) {
        out.push_back(LegendEntry{label(stop.t), {pack(stop.colour)}});
    }
}

void add_hypsometric_land(std::vector<LegendEntry>& out) {
    add_stops(out, HYPSOMETRIC_LAND_STOPS, [](std::int64_t t) { return tenths(t) + " m"; });
}

void add_biome_rows(std::vector<LegendEntry>& out, std::span<const BiomeRecord> biomes, bool land_only) {
    for (const BiomeRecord& record : biomes) {
        if (land_only && record.water != 0) {
            continue;
        }
        out.push_back(LegendEntry{record.name.empty() ? record.key : record.name,
                                  {static_cast<std::uint32_t>(record.colour & 0xFFFFFF)}});
    }
}

// W of P4-02 4.4: the water overlays of every composite.
void add_water_overlays(std::vector<LegendEntry>& out) {
    out.push_back(LegendEntry{"ocean, 0 to 3000 m deep", stop_colours(HYPSOMETRIC_OCEAN_STOPS)});
    out.push_back(LegendEntry{"lake", {pack(Rgb{64, 128, 204})}});
    std::vector<std::uint32_t> rivers;
    for (std::uint64_t k = 1; k <= 6; ++k) {
        rivers.push_back(pack(composite_river_colour(k)));
    }
    out.push_back(LegendEntry{"river, class 1 to 6+", rivers});
}

void add_signed_rows(std::vector<LegendEntry>& out, std::string_view name, const db::LayerGrid& grid) {
    if (!db::is_valid_layer_grid(grid)) {
        return;
    }
    const db::ElementType t = grid.type();
    if (t != db::ElementType::i8 && t != db::ElementType::i16 && t != db::ElementType::i32 &&
        t != db::ElementType::i64) {
        return;  // signed_scale would raise (P4-02 5)
    }
    const SignedScale scale = signed_scale(grid);
    // M is at most 2^63 (INT64_MIN), P at most INT64_MAX. -M and -M/2 (C++
    // truncation toward zero, i.e. -(M / 2)) are formed without negating an
    // int64 that could be INT64_MIN.
    constexpr std::uint64_t I64_MAX = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    const std::int64_t neg_m = scale.m > I64_MAX ? std::numeric_limits<std::int64_t>::min()
                                                 : -static_cast<std::int64_t>(scale.m);
    const std::int64_t neg_half_m = -static_cast<std::int64_t>(scale.m / 2U);
    const auto p = static_cast<std::int64_t>(scale.p);
    const std::array<std::int64_t, 6> values = {neg_m, neg_half_m, std::int64_t{-1}, std::int64_t{0}, p / 2, p};
    for (const std::int64_t v : values) {
        if (v < neg_m || v > p) {
            continue;
        }
        out.push_back(LegendEntry{format_layer_value(name, v), {pack(signed_colour(v, scale))}});
    }
}

void add_gray_rows(std::vector<LegendEntry>& out, std::string_view name, const db::LayerGrid& grid) {
    if (!db::is_valid_layer_grid(grid)) {
        return;
    }
    const ValueRange range = value_range(grid);
    if (range.is_signed) {
        const std::uint64_t r =
            static_cast<std::uint64_t>(range.max_signed) - static_cast<std::uint64_t>(range.min_signed);
        out.push_back(LegendEntry{format_layer_value(name, range.min_signed) + " (min)", {gray(0)}});
        out.push_back(LegendEntry{"", {gray(0), gray(128), gray(255)}});
        out.push_back(LegendEntry{format_layer_value(name, range.max_signed) + " (max)", {gray(gray_level(r, r))}});
        return;
    }
    const std::uint64_t r = range.max_unsigned - range.min_unsigned;
    constexpr std::uint64_t I64_MAX = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    const auto text = [name](std::uint64_t v) {
        return v > I64_MAX ? db::format_u64(v) : format_layer_value(name, static_cast<std::int64_t>(v));
    };
    out.push_back(LegendEntry{text(range.min_unsigned) + " (min)", {gray(0)}});
    out.push_back(LegendEntry{"", {gray(0), gray(128), gray(255)}});
    out.push_back(LegendEntry{text(range.max_unsigned) + " (max)", {gray(gray_level(r, r))}});
}

void add_log2_rows(std::vector<LegendEntry>& out, std::string_view name) {
    constexpr std::array<std::uint64_t, 7> VALUES = {0, 1, 16, 256, 4096, 65536, 1048576};
    for (const std::uint64_t v : VALUES) {
        const std::string text = name == "flow_accum" && v > 0U
                                     ? db::format_u64(v) + " cells (" + upstream_km2(v) + " km2)"
                                     : db::format_u64(v);
        out.push_back(LegendEntry{text, {pack(log2_colour(v))}});
    }
}

bool has_layer(std::span<const db::LayerInfo> layers, std::string_view name) noexcept {
    for (const db::LayerInfo& info : layers) {
        if (info.name == name) {
            return true;
        }
    }
    return false;
}

}  // namespace

// --- Groups ---------------------------------------------------------------------------

std::string_view layer_group_title(LayerGroup group) noexcept {
    switch (group) {
        case LayerGroup::composites: return "Composites";
        case LayerGroup::terrain: return "Terrain";
        case LayerGroup::water: return "Water";
        case LayerGroup::climate: return "Climate";
        case LayerGroup::biomes: return "Biomes";
        case LayerGroup::resources: return "Resources";
        case LayerGroup::regions: return "Regions";
        case LayerGroup::other: return "Other";
    }
    return "Other";
}

// --- Catalogue ------------------------------------------------------------------------

std::span<const LayerSpec> layer_catalogue() noexcept {
    return CATALOGUE;
}

const LayerSpec* find_layer_spec(std::string_view name) noexcept {
    for (const LayerSpec& spec : CATALOGUE) {
        if (spec.name == name) {
            return &spec;
        }
    }
    return nullptr;
}

LayerGroup layer_group(std::string_view name) noexcept {
    const LayerSpec* spec = find_layer_spec(name);
    return spec == nullptr ? LayerGroup::other : spec->group;
}

std::string_view layer_unit(std::string_view name) noexcept {
    const LayerSpec* spec = find_layer_spec(name);
    return spec == nullptr ? std::string_view() : spec->unit;
}

// --- Values -----------------------------------------------------------------------------

bool cell_value(const db::LayerGrid& grid, std::uint32_t x, std::uint32_t y, std::int64_t& value) noexcept {
    if (x >= grid.width || y >= grid.height) {
        return false;
    }
    // At most (2^32 - 1)^2 + 2^32 - 1 < 2^64: exact in uint64.
    const std::uint64_t index = static_cast<std::uint64_t>(y) * grid.width + x;
    return std::visit(
        [&value, index](const auto& values) -> bool {
            using T = typename std::decay_t<decltype(values)>::value_type;
            if (index >= static_cast<std::uint64_t>(values.size())) {
                return false;
            }
            const T v = values[static_cast<std::size_t>(index)];
            if constexpr (std::is_same_v<T, std::uint64_t>) {
                if (v > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                    return false;
                }
            }
            value = static_cast<std::int64_t>(v);
            return true;
        },
        grid.values);
}

std::string format_layer_value(std::string_view name, std::int64_t v) {
    if (name == "elevation") {
        return tenths(v) + " m" + (v < 0 ? " (sea)" : "");
    }
    if (name == "water") {
        return water_text(v);
    }
    if (name == "water_depth") {
        return v == 0 ? std::string("0 m (no standing water)") : tenths(v) + " m deep";
    }
    if (name == "flow_dir") {
        return direction_text(v);
    }
    if (name == "flow_accum") {
        if (v <= 0) {
            return "0 (ocean)";
        }
        const auto n = static_cast<std::uint64_t>(v);
        return db::format_u64(n) + " cells, " + upstream_km2(n) + " km2 upstream";
    }
    if (name == "river") {
        return v == 0 ? std::string("not a river") : "class " + db::format_i64(v);
    }
    if (name == "lake" || name == "walk_area") {
        return v == 0 ? std::string("none") : "id " + db::format_i64(v);
    }
    if (name == "coast_dist") {
        if (v < 0) {
            return db::format_u64(magnitude(v)) + " m offshore";
        }
        return db::format_i64(v) + " m from the sea";
    }
    if (name == "temperature") {
        return tenths(v) + " °C";
    }
    if (name == "rainfall") {
        return db::format_i64(v) + " mm/yr";
    }
    if (is_score(name)) {
        return db::format_i64(v) + " / 255";
    }
    if (name == "biome") {
        return "code " + db::format_i64(v);
    }
    if (name == "region") {
        return v == 0 ? std::string("none (ocean)") : "id " + db::format_i64(v);
    }
    return db::format_i64(v);
}

// --- Legends ----------------------------------------------------------------------------

std::vector<LegendEntry> layer_legend(std::string_view name, Palette palette, const db::LayerGrid& grid,
                                      std::span<const BiomeRecord> biomes) {
    std::vector<LegendEntry> out;
    switch (palette) {
        case Palette::hypsometric:
            add_hypsometric_land(out);
            add_stops(out, HYPSOMETRIC_OCEAN_STOPS, [](std::int64_t t) { return "sea, " + tenths(t) + " m deep"; });
            break;
        case Palette::temperature:
            add_stops(out, TEMPERATURE_STOPS, [](std::int64_t t) { return tenths(t) + " °C"; });
            break;
        case Palette::rainfall:
            add_stops(out, RAINFALL_STOPS, [](std::int64_t t) { return db::format_i64(t) + " mm/yr"; });
            break;
        case Palette::score:
            add_stops(out, SCORE_STOPS, [](std::int64_t t) {
                return db::format_i64(t) + (t == 0 ? " / 255 (none)" : t == 255 ? " / 255 (best)" : " / 255");
            });
            break;
        case Palette::water:
            for (std::int64_t v = 0; v <= 3; ++v) {
                out.push_back(LegendEntry{format_layer_value("water", v),
                                          {pack(water_colour(static_cast<std::uint64_t>(v)))}});
            }
            break;
        case Palette::river:
            out.push_back(LegendEntry{"0 not a river", {pack(river_colour(0))}});
            for (std::uint64_t k = 1; k <= 5; ++k) {
                out.push_back(LegendEntry{"class " + db::format_u64(k), {pack(river_colour(k))}});
            }
            out.push_back(LegendEntry{"class 6 to 15", {pack(river_colour(6))}});
            break;
        case Palette::direction:
            for (std::uint64_t d = 0; d <= 8; ++d) {
                out.push_back(LegendEntry{db::format_u64(d) + " " + direction_text(static_cast<std::int64_t>(d)),
                                          {pack(direction_colour(d))}});
            }
            break;
        case Palette::log2:
            add_log2_rows(out, name);
            break;
        case Palette::ids:
            out.push_back(LegendEntry{"0: none", {pack(ids_colour(0))}});
            out.push_back(LegendEntry{"each id its own colour",
                                      {pack(ids_colour(1)), pack(ids_colour(2)), pack(ids_colour(3)),
                                       pack(ids_colour(4)), pack(ids_colour(5))}});
            break;
        case Palette::signed_distance:
            add_signed_rows(out, name, grid);
            break;
        case Palette::biome:
            add_biome_rows(out, biomes, false);
            break;
        case Palette::gray:
            add_gray_rows(out, name, grid);
            break;
    }
    return out;
}

std::vector<LegendEntry> composite_legend(Composite composite, std::span<const BiomeRecord> biomes) {
    std::vector<LegendEntry> out;
    switch (composite) {
        case Composite::map:
            add_hypsometric_land(out);
            add_water_overlays(out);
            break;
        case Composite::biomes:
            add_biome_rows(out, biomes, true);
            add_water_overlays(out);
            break;
        case Composite::regions:
            out.push_back(LegendEntry{"region border", {0x000000U}});
            add_biome_rows(out, biomes, true);
            add_water_overlays(out);
            break;
    }
    return out;
}

std::string_view composite_description(Composite composite) noexcept {
    switch (composite) {
        case Composite::map: return "relief shaded by slope";
        case Composite::biomes: return "biome colours shaded by slope";
        case Composite::regions: return "biome colours shaded by slope, region borders";
    }
    return {};
}

// --- Composites available ----------------------------------------------------------------

std::vector<Composite> available_composites(std::span<const db::LayerInfo> layers, bool biome_table_usable) {
    std::vector<Composite> result;
    if (!has_layer(layers, "elevation")) {
        return result;
    }
    const bool biome_ok = biome_table_usable && has_layer(layers, "biome");
    const bool region_ok = biome_ok && has_layer(layers, "region");
    for (const Composite composite : ALL_COMPOSITES) {
        if (composite == Composite::biomes && !biome_ok) {
            continue;
        }
        if (composite == Composite::regions && !region_ok) {
            continue;
        }
        result.push_back(composite);
    }
    return result;
}

// --- Region names --------------------------------------------------------------------------

std::string_view region_name(std::span<const RegionRecord> records, std::int64_t id) noexcept {
    // Plain binary search on [lo, hi); on unordered records it still ends
    // within bounds, so the result is some record's name or empty.
    if (id == 0) {
        return {};  // 0 is the ocean, never a region (P1-06 3.2)
    }
    std::size_t lo = 0;
    std::size_t hi = records.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2U;
        if (records[mid].id < id) {
            lo = mid + 1U;
        } else {
            hi = mid;
        }
    }
    if (lo < records.size() && records[lo].id == id) {
        return records[lo].name;
    }
    return {};
}

}  // namespace worldgen::inspect
