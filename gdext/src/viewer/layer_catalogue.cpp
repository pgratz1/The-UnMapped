#include "viewer/layer_catalogue.hpp"

#include <array>
#include <limits>
#include <type_traits>
#include <variant>

namespace unmapped::catalogue {

namespace {

namespace inspect = worldgen::inspect;

struct LayerEntry {
    std::string_view name;
    Group group;
    std::string_view unit;
};

// P1-00 section 4, in catalogue order.
constexpr std::array<LayerEntry, 19> LAYERS = {{
    {"elevation", Group::terrain, "decimetres above sea level"},
    {"water", Group::water, "category: 0 dry land, 1 river, 2 lake, 3 ocean"},
    {"water_depth", Group::water, "decimetres of standing water"},
    {"flow_dir", Group::water, "D8 code of the downstream neighbour"},
    {"flow_accum", Group::water, "cells draining through the cell (x 256 m2)"},
    {"river", Group::water, "river size class, 0 none"},
    {"lake", Group::water, "lake id, 0 none"},
    {"coast_dist", Group::water, "metres to the coast, < 0 at sea"},
    {"temperature", Group::climate, "annual mean, tenths of a degree Celsius"},
    {"rainfall", Group::climate, "annual precipitation, millimetres"},
    {"biome", Group::biomes, "biome code of table biome"},
    {"fertility", Group::resources, "score 0 to 255"},
    {"ore", Group::resources, "score 0 to 255"},
    {"timber", Group::resources, "score 0 to 255"},
    {"fresh_water", Group::resources, "score 0 to 255"},
    {"forage", Group::resources, "score 0 to 255"},
    {"habitability", Group::resources, "score 0 to 255"},
    {"walk_area", Group::resources, "walkable area id, 0 none"},
    {"region", Group::regions, "region id of table region, 0 ocean"},
}};

const LayerEntry* find_entry(std::string_view name) noexcept {
    for (const auto& entry : LAYERS) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

std::uint32_t pack(inspect::Rgb c) noexcept {
    return (static_cast<std::uint32_t>(c.r) << 16U) | (static_cast<std::uint32_t>(c.g) << 8U) |
           static_cast<std::uint32_t>(c.b);
}

std::uint32_t gray(std::uint8_t g) noexcept {
    return pack(inspect::Rgb{g, g, g});
}

std::uint64_t magnitude(std::int64_t v) noexcept {
    return v < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(v) : static_cast<std::uint64_t>(v);
}

// v / 10 with one decimal: "-12.5", "0.0".
std::string tenths(std::int64_t v) {
    const std::uint64_t m = magnitude(v);
    return (v < 0 ? "-" : "") + std::to_string(m / 10U) + "." + std::to_string(m % 10U);
}

// v / 100 with two decimals.
std::string hundredths(std::uint64_t v) {
    const std::uint64_t frac = v % 100U;
    return std::to_string(v / 100U) + (frac < 10U ? ".0" : ".") + std::to_string(frac);
}

constexpr std::array<std::string_view, 8> D8_NAMES = {"E", "SE", "S", "SW", "W", "NW", "N", "NE"};

std::string direction_text(std::int64_t v) {
    if (v >= 0 && v < 8) {
        return std::string(D8_NAMES[static_cast<std::size_t>(v)]);
    }
    if (v == 8) {
        return "none (ocean)";
    }
    return "code " + std::to_string(v);
}

std::string water_text(std::int64_t v) {
    switch (v) {
        case 0:
            return "dry land";
        case 1:
            return "river";
        case 2:
            return "lake";
        case 3:
            return "ocean";
        default:
            return "category " + std::to_string(v);
    }
}

// Upstream area of n cells of 256 m2 in km2, two decimals (RHA as report).
std::string upstream_km2(std::uint64_t cells) {
    return hundredths((cells * 256U + 5000U) / 10000U);
}

bool is_score(std::string_view name) noexcept {
    return name == "fertility" || name == "ore" || name == "timber" || name == "fresh_water" || name == "forage" ||
           name == "habitability";
}

template <std::size_t N>
std::vector<std::uint32_t> stop_colours(const std::array<inspect::RampStop, N>& stops) {
    std::vector<std::uint32_t> colours;
    for (const auto& stop : stops) {
        colours.push_back(pack(stop.colour));
    }
    return colours;
}

template <std::size_t N, class Label>
void add_stops(std::vector<LegendEntry>& out, const std::array<inspect::RampStop, N>& stops, Label label) {
    for (const auto& stop : stops) {
        out.push_back(LegendEntry{label(stop.t), {pack(stop.colour)}});
    }
}

void add_biome_rows(std::vector<LegendEntry>& out, std::span<const inspect::BiomeRecord> biomes, bool land_only) {
    for (const auto& record : biomes) {
        if (land_only && record.water != 0) {
            continue;
        }
        out.push_back(LegendEntry{record.name.empty() ? record.key : record.name,
                                  {static_cast<std::uint32_t>(record.colour & 0xFFFFFF)}});
    }
}

void add_water_overlays(std::vector<LegendEntry>& out) {
    out.push_back(LegendEntry{"ocean, 0 to 3000 m deep", stop_colours(inspect::HYPSOMETRIC_OCEAN_STOPS)});
    out.push_back(LegendEntry{"lake", {pack(inspect::Rgb{64, 128, 204})}});
    std::vector<std::uint32_t> rivers;
    for (std::uint64_t k = 1; k <= 6; ++k) {
        rivers.push_back(pack(inspect::composite_river_colour(k)));
    }
    out.push_back(LegendEntry{"river, class 1 to 6+", rivers});
}

}  // namespace

std::string_view group_title(Group group) noexcept {
    switch (group) {
        case Group::composites:
            return "Composites";
        case Group::terrain:
            return "Terrain";
        case Group::water:
            return "Water";
        case Group::climate:
            return "Climate";
        case Group::biomes:
            return "Biomes";
        case Group::resources:
            return "Resources";
        case Group::regions:
            return "Regions";
        case Group::other:
            return "Other";
    }
    return "Other";
}

Group layer_group(std::string_view layer_name) noexcept {
    const LayerEntry* entry = find_entry(layer_name);
    return entry == nullptr ? Group::other : entry->group;
}

std::string_view layer_unit(std::string_view layer_name) noexcept {
    const LayerEntry* entry = find_entry(layer_name);
    return entry == nullptr ? std::string_view() : entry->unit;
}

std::string format_value(std::string_view name, std::int64_t v) {
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
        return std::to_string(v) + " cells, " + upstream_km2(static_cast<std::uint64_t>(v)) + " km2 upstream";
    }
    if (name == "river") {
        return v == 0 ? std::string("not a river") : "class " + std::to_string(v);
    }
    if (name == "lake" || name == "walk_area") {
        return v == 0 ? std::string("none") : "id " + std::to_string(v);
    }
    if (name == "coast_dist") {
        if (v < 0) {
            return std::to_string(magnitude(v)) + " m offshore";
        }
        return std::to_string(v) + " m from the sea";
    }
    if (name == "temperature") {
        return tenths(v) + " °C";
    }
    if (name == "rainfall") {
        return std::to_string(v) + " mm/yr";
    }
    if (is_score(name)) {
        return std::to_string(v) + " / 255";
    }
    if (name == "biome") {
        return "code " + std::to_string(v);
    }
    if (name == "region") {
        return v == 0 ? std::string("none (ocean)") : "id " + std::to_string(v);
    }
    return std::to_string(v);
}

bool cell_value(const worldgen::db::LayerGrid& grid, std::uint32_t x, std::uint32_t y, std::int64_t& value) noexcept {
    if (x >= grid.width || y >= grid.height) {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(y) * grid.width + x;
    return std::visit(
        [&](const auto& values) -> bool {
            using T = typename std::decay_t<decltype(values)>::value_type;
            if (index >= values.size()) {
                return false;
            }
            if constexpr (std::is_same_v<T, std::uint64_t>) {
                if (values[index] > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                    return false;
                }
            }
            value = static_cast<std::int64_t>(values[index]);
            return true;
        },
        grid.values);
}

std::vector<LegendEntry> layer_legend(std::string_view name, inspect::Palette palette,
                                      const worldgen::db::LayerGrid& grid,
                                      std::span<const inspect::BiomeRecord> biomes) {
    std::vector<LegendEntry> out;
    switch (palette) {
        case inspect::Palette::hypsometric:
            add_stops(out, inspect::HYPSOMETRIC_LAND_STOPS, [](std::int64_t t) { return tenths(t) + " m"; });
            add_stops(out, inspect::HYPSOMETRIC_OCEAN_STOPS,
                      [](std::int64_t t) { return "sea, " + tenths(t) + " m deep"; });
            break;
        case inspect::Palette::temperature:
            add_stops(out, inspect::TEMPERATURE_STOPS, [](std::int64_t t) { return tenths(t) + " °C"; });
            break;
        case inspect::Palette::rainfall:
            add_stops(out, inspect::RAINFALL_STOPS, [](std::int64_t t) { return std::to_string(t) + " mm/yr"; });
            break;
        case inspect::Palette::score:
            add_stops(out, inspect::SCORE_STOPS, [](std::int64_t t) {
                return std::to_string(t) + (t == 0 ? " / 255 (none)" : t == 255 ? " / 255 (best)" : " / 255");
            });
            break;
        case inspect::Palette::water:
            for (std::int64_t v = 0; v <= 3; ++v) {
                out.push_back(LegendEntry{water_text(v), {pack(inspect::water_colour(static_cast<std::uint64_t>(v)))}});
            }
            break;
        case inspect::Palette::river:
            out.push_back(LegendEntry{"0 not a river", {pack(inspect::river_colour(0))}});
            for (std::uint64_t k = 1; k <= 5; ++k) {
                out.push_back(LegendEntry{"class " + std::to_string(k), {pack(inspect::river_colour(k))}});
            }
            out.push_back(LegendEntry{"class 6 to 15", {pack(inspect::river_colour(6))}});
            break;
        case inspect::Palette::direction:
            for (std::uint64_t d = 0; d <= 8; ++d) {
                out.push_back(LegendEntry{std::to_string(d) + " " + direction_text(static_cast<std::int64_t>(d)),
                                          {pack(inspect::direction_colour(d))}});
            }
            break;
        case inspect::Palette::log2:
            for (std::uint64_t v : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{16}, std::uint64_t{256},
                                    std::uint64_t{4096}, std::uint64_t{65536}, std::uint64_t{1048576}}) {
                const std::string text =
                    name == "flow_accum" && v > 0 ? std::to_string(v) + " cells (" + upstream_km2(v) + " km2)"
                                                  : std::to_string(v);
                out.push_back(LegendEntry{text, {pack(inspect::log2_colour(v))}});
            }
            break;
        case inspect::Palette::ids:
            out.push_back(LegendEntry{"0: none", {pack(inspect::ids_colour(0))}});
            out.push_back(LegendEntry{"each id its own colour",
                                      {pack(inspect::ids_colour(1)), pack(inspect::ids_colour(2)),
                                       pack(inspect::ids_colour(3)), pack(inspect::ids_colour(4)),
                                       pack(inspect::ids_colour(5))}});
            break;
        case inspect::Palette::signed_distance: {
            const inspect::SignedScale scale = inspect::signed_scale(grid);
            const auto m = static_cast<std::int64_t>(scale.m);
            const auto p = static_cast<std::int64_t>(scale.p);
            for (std::int64_t v : {-m, -m / 2, std::int64_t{-1}, std::int64_t{0}, p / 2, p}) {
                if (v < -m || v > p) {
                    continue;
                }
                out.push_back(LegendEntry{format_value(name, v), {pack(inspect::signed_colour(v, scale))}});
            }
            break;
        }
        case inspect::Palette::biome:
            add_biome_rows(out, biomes, false);
            break;
        case inspect::Palette::gray: {
            if (!worldgen::db::is_valid_layer_grid(grid)) {
                break;
            }
            const inspect::ValueRange range = inspect::value_range(grid);
            if (range.is_signed) {
                const std::uint64_t r = static_cast<std::uint64_t>(range.max_signed) -
                                        static_cast<std::uint64_t>(range.min_signed);
                out.push_back(LegendEntry{format_value(name, range.min_signed) + " (min)", {gray(0)}});
                out.push_back(LegendEntry{"", {gray(0), gray(128), gray(255)}});
                out.push_back(LegendEntry{format_value(name, range.max_signed) + " (max)",
                                          {gray(inspect::gray_level(r, r))}});
            } else {
                const std::uint64_t r = range.max_unsigned - range.min_unsigned;
                const auto text = [&](std::uint64_t v) {
                    return v > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
                               ? std::to_string(v)
                               : format_value(name, static_cast<std::int64_t>(v));
                };
                out.push_back(LegendEntry{text(range.min_unsigned) + " (min)", {gray(0)}});
                out.push_back(LegendEntry{"", {gray(0), gray(128), gray(255)}});
                out.push_back(LegendEntry{text(range.max_unsigned) + " (max)", {gray(inspect::gray_level(r, r))}});
            }
            break;
        }
    }
    return out;
}

std::vector<LegendEntry> composite_legend(inspect::Composite composite,
                                          std::span<const inspect::BiomeRecord> biomes) {
    std::vector<LegendEntry> out;
    switch (composite) {
        case inspect::Composite::map:
            add_stops(out, inspect::HYPSOMETRIC_LAND_STOPS, [](std::int64_t t) { return tenths(t) + " m"; });
            add_water_overlays(out);
            break;
        case inspect::Composite::regions:
            out.push_back(LegendEntry{"region border", {0x000000U}});
            add_biome_rows(out, biomes, true);
            add_water_overlays(out);
            break;
        case inspect::Composite::biomes:
            add_biome_rows(out, biomes, true);
            add_water_overlays(out);
            break;
    }
    return out;
}

}  // namespace unmapped::catalogue
