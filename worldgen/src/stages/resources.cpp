// Stage P1-05 resources and habitability
// (docs/specs/p1-05-resources-habitability.md). Steps R1 to R7, each named
// with its determinism rule of P1-05 section 7.

#include "worldgen/stages/resources.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <utility>

#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/parallel.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/core/wide.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/geo/blocks.hpp"
#include "worldgen/geo/noise_schema.hpp"
#include "worldgen/geo/terrain.hpp"
#include "worldgen/stages/climate.hpp"
#include "worldgen/stages/elevation.hpp"
#include "worldgen/stages/hydrology.hpp"

namespace worldgen::stages {

namespace {

using geo::CellIndex;
using geo::Grid;

constexpr const char* PRECONDITION = "resources: input precondition: ";

[[noreturn]] void fatal_text(const std::string& message) {
    throw FatalError(message);
}

void check_threads(std::uint32_t threads) {
    if (threads < 1U || threads > MAX_THREADS) {
        fatal("resources: thread count outside [1, 256]");
    }
}

// Metres of a cell centre along one axis: from_int(16 k + 8) (P1-05 5).
Fx cell_centre_m(std::int32_t k) {
    return Fx::from_int(geo::CELL_M * k + geo::CELL_M / 2);
}

// --- Tuning checks ---------------------------------------------------------------------------

void tuning_field(bool ok, const char* field) {
    if (!ok) {
        fatal_text(std::string("resources: tuning: ") + field + " outside its schema range");
    }
}

bool fx_in(Fx v, Fx lo, Fx hi) {
    return lo <= v && v <= hi;
}

bool int_in(std::int64_t v, std::int64_t lo, std::int64_t hi) {
    return lo <= v && v <= hi;
}

void tuning_ordered(bool ok, const char* message) {
    if (!ok) {
        fatal_text(std::string("resources: tuning: ") + message);
    }
}

// The tuning's derived constants, once per world.
struct Compiled {
    Fx flood_limit_dm;   // mul_int(hab_flood_hand_m, 10)
    Fx rain_lo;          // from_int(fresh_rain_lo_mm)
    Fx rain_hi;          // from_int(fresh_rain_hi_mm)
    Fx fish_reach;       // from_int(fish_reach_m)
    Fx ore_elev_lo;      // from_int(ore_elev_lo_m)
    Fx ore_elev_hi;      // from_int(ore_elev_hi_m)
    std::int64_t weight_sum = 0;
};

Compiled compile(const ResourceTuning& t) {
    Compiled c;
    c.flood_limit_dm = mul_int(t.hab_flood_hand_m, 10);
    c.rain_lo = Fx::from_int(t.fresh_rain_lo_mm);
    c.rain_hi = Fx::from_int(t.fresh_rain_hi_mm);
    c.fish_reach = Fx::from_int(t.fish_reach_m);
    c.ore_elev_lo = Fx::from_int(t.ore_elev_lo_m);
    c.ore_elev_hi = Fx::from_int(t.ore_elev_hi_m);
    c.weight_sum = t.hab_weight_fertility + t.hab_weight_forage + t.hab_weight_timber + t.hab_weight_ore;
    return c;
}

// --- The per-cell functions on the tangent (R3, R4, R6) ----------------------------------------

std::uint8_t fresh_water_of(std::int64_t df, std::int64_t rainfall, const ResourceTuning& t, const Compiled& c) {
    const Fx fs = df >= t.fresh_reach_m ? Fx{} : Fx::from_ratio(t.fresh_reach_m - df, t.fresh_reach_m);
    const Fx fr = mul(t.fresh_rain_weight, resource_ramp(Fx::from_int(rainfall), c.rain_lo, c.rain_hi));
    return resource_q255(max(fs, fr));
}

std::uint8_t fertility_of(Fx fb, Fx tan, std::int64_t hand, const ResourceTuning& t) {
    const Fx sf = Fx::ONE - resource_ramp(tan, t.fert_slope_lo, t.fert_slope_hi);
    const Fx hm = Fx::from_ratio(hand, 10);
    const Fx al = Fx::ONE + mul(t.alluvial_bonus, Fx::ONE - resource_ramp(hm, Fx{}, t.alluvial_hand_m));
    return resource_q255(min(Fx::ONE, mul(mul(fb, sf), al)));
}

std::uint8_t timber_of(Fx tb, Fx tan, const ResourceTuning& t) {
    const Fx st = Fx::ONE - resource_ramp(tan, t.timber_slope_lo, t.timber_slope_hi);
    return resource_q255(mul(tb, st));
}

std::uint8_t forage_of(Fx gb, std::int64_t dg, const ResourceTuning& t, const Compiled& c) {
    const Fx fish = mul(t.fish_weight, Fx::ONE - resource_ramp(Fx::from_int(dg), Fx{}, c.fish_reach));
    return resource_q255(min(Fx::ONE, gb + fish));
}

std::uint8_t ore_of(std::int64_t elevation, Fx tan, Fx noise, const ResourceTuning& t, const Compiled& c) {
    const Fx em = Fx::from_ratio(elevation, 10);
    const Fx eu = max(t.ore_lowland, resource_ramp(em, c.ore_elev_lo, c.ore_elev_hi));
    const Fx su = t.ore_slope_base + mul(Fx::ONE - t.ore_slope_base, resource_ramp(tan, t.ore_slope_lo, t.ore_slope_hi));
    const Fx vn = resource_ramp(noise, t.ore_vein_lo, t.ore_vein_hi);
    return resource_q255(mul(mul(eu, su), vn));
}

std::uint8_t habitability_of(const std::array<std::int64_t, 5>& fwgto, Fx tan, std::int64_t hand,
                             std::int64_t walk_area, const ResourceTuning& t, const Compiled& c) {
    const std::int64_t sc = t.hab_weight_fertility * fwgto[0] + t.hab_weight_forage * fwgto[2] +
                            t.hab_weight_timber * fwgto[3] + t.hab_weight_ore * fwgto[4];
    const Fx b = Fx::from_ratio(sc, 255 * c.weight_sum);
    const Fx wat = t.hab_water_min + mul(Fx::ONE - t.hab_water_min, Fx::from_ratio(fwgto[1], 255));
    Fx h = mul(b, wat);
    h = mul(h, Fx::ONE - resource_ramp(tan, t.hab_slope_lo, t.hab_slope_hi));
    if (Fx::from_int(hand) < c.flood_limit_dm) {
        h = mul(h, t.hab_flood_factor);
    }
    if (walk_area == 0) {
        h = mul(h, t.hab_pocket_factor);
    }
    return resource_q255(min(Fx::ONE, h));
}

// --- Argument checks of the seams ---------------------------------------------------------------

Fx checked_tangent(const char* seam, std::int64_t s2) {
    if (s2 < 0 || s2 >= (std::int64_t{1} << 34U)) {
        fatal_text(std::string(seam) + ": S2 outside [0, 2^34)");
    }
    return geo::slope_tangent(s2);
}

void check_non_negative(const char* seam, const char* what, std::int64_t v) {
    if (v < 0) {
        fatal_text(std::string(seam) + ": negative " + what);
    }
}

void check_unit(const char* seam, Fx v) {
    if (v < Fx{} || v > Fx::ONE) {
        fatal_text(std::string(seam) + ": biome value outside [0, 1]");
    }
}

// --- Walk areas (R5) ------------------------------------------------------------------------------

// The link limits of R5 in S2 and (Delta S)^2 terms: walkable iff S2 <=
// s2_max; linked iff (Delta S)^2 <= step_orth (orthogonal) or step_diag
// (diagonal). x 2^64 <= X <=> x <= X.hi for integers x >= 0.
struct WalkLimits {
    std::int64_t s2_max = 0;
    std::uint64_t step_orth = 0;
    std::uint64_t step_diag = 0;
};

WalkLimits walk_limits(Fx walk_slope_max) {
    WalkLimits l;
    l.s2_max = geo::slope_s2_limit(walk_slope_max);
    // 160 x raw <= 160 x 10 x 2^32 < 2^43: the square is below 2^86 and twice it below 2^87.
    const std::uint64_t scaled = 160U * static_cast<std::uint64_t>(walk_slope_max.raw());
    const U128 sq = mul_u64_wide(scaled, scaled);
    l.step_orth = sq.hi;
    l.step_diag = (sq + sq).hi;
    return l;
}

// --- Stage input helpers ------------------------------------------------------------------------

std::string cell_at_text(Grid grid, std::size_t i) {
    const auto c = static_cast<CellIndex>(i);
    return "(" + db::format_i64(grid.x_of(c)) + ", " + db::format_i64(grid.y_of(c)) + ")";
}

// The per-code biome values of R4 and whether a definition has the code.
struct BiomeValues {
    std::array<Fx, 256> fertility{};
    std::array<Fx, 256> timber{};
    std::array<Fx, 256> forage{};
    std::array<std::uint8_t, 256> defined{};
};

BiomeValues biome_values(const BiomeDefinitions& definitions) {
    BiomeValues v;
    for (const BiomeDef& b : definitions.biomes) {
        const bool unit = fx_in(b.fertility, Fx{}, Fx::ONE) && fx_in(b.timber, Fx{}, Fx::ONE) &&
                          fx_in(b.forage, Fx{}, Fx::ONE);
        if (!unit) {
            fatal_text("resources: biome '" + b.key + "' has a value outside [0, 1]");
        }
        if (b.code < 0 || b.code > 255) {
            fatal_text("resources: biome '" + b.key + "' has a code outside 0..255");
        }
        const auto k = static_cast<std::size_t>(b.code);
        v.fertility[k] = b.fertility;
        v.timber[k] = b.timber;
        v.forage[k] = b.forage;
        v.defined[k] = 1U;
    }
    return v;
}

// Section 2: lengths, water, river and biome values, in this order.
void check_inputs(const ResourceInputs& in, const BiomeValues& values) {
    const Grid grid = in.grid;
    try {
        geo::check_grid(grid);
    } catch (const FatalError& e) {
        fatal_text(std::string(PRECONDITION) + e.what());
    }
    const std::size_t n = grid.cells();
    const std::array<std::pair<const char*, std::size_t>, 7> sizes = {{
        {"elevation", in.elevation.size()},
        {"water", in.water.size()},
        {"water_depth", in.water_depth.size()},
        {"flow_dir", in.flow_dir.size()},
        {"river", in.river.size()},
        {"rainfall", in.rainfall.size()},
        {"biome", in.biome.size()},
    }};
    for (const auto& [name, size] : sizes) {
        if (size != n) {
            fatal_text(std::string(PRECONDITION) + name + " length is not width * height");
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (in.water[i] > WATER_OCEAN) {
            fatal_text(std::string(PRECONDITION) + "water value " + db::format_u64(in.water[i]) + " at " +
                       cell_at_text(grid, i));
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (in.river[i] > geo::RIVER_CLASS_MAX) {
            fatal_text(std::string(PRECONDITION) + "river value " + db::format_u64(in.river[i]) + " at " +
                       cell_at_text(grid, i));
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (values.defined[in.biome[i]] == 0U) {
            fatal_text(std::string(PRECONDITION) + "biome value " + db::format_u64(in.biome[i]) + " at " +
                       cell_at_text(grid, i) + " has no definition");
        }
    }
}

bool is_land(std::uint8_t water) {
    return water == WATER_DRY || water == WATER_RIVER;
}

}  // namespace

// --- Tuning (4.2) ------------------------------------------------------------------------------------

ResourceTuning resource_tuning_from(const data::Record& r) {
    ResourceTuning t;
    t.fresh_reach_m = r.get_int("fresh_reach_m");
    t.fresh_rain_weight = r.get_fx("fresh_rain_weight");
    t.fresh_rain_lo_mm = r.get_int("fresh_rain_lo_mm");
    t.fresh_rain_hi_mm = r.get_int("fresh_rain_hi_mm");
    t.fert_slope_lo = r.get_fx("fert_slope_lo");
    t.fert_slope_hi = r.get_fx("fert_slope_hi");
    t.alluvial_bonus = r.get_fx("alluvial_bonus");
    t.alluvial_hand_m = r.get_fx("alluvial_hand_m");
    t.timber_slope_lo = r.get_fx("timber_slope_lo");
    t.timber_slope_hi = r.get_fx("timber_slope_hi");
    t.fish_river_class_min = r.get_int("fish_river_class_min");
    t.fish_weight = r.get_fx("fish_weight");
    t.fish_reach_m = r.get_int("fish_reach_m");
    t.ore_noise = geo::fbm_m_from(r.get("ore_noise"));
    t.ore_vein_lo = r.get_fx("ore_vein_lo");
    t.ore_vein_hi = r.get_fx("ore_vein_hi");
    t.ore_elev_lo_m = r.get_int("ore_elev_lo_m");
    t.ore_elev_hi_m = r.get_int("ore_elev_hi_m");
    t.ore_lowland = r.get_fx("ore_lowland");
    t.ore_slope_base = r.get_fx("ore_slope_base");
    t.ore_slope_lo = r.get_fx("ore_slope_lo");
    t.ore_slope_hi = r.get_fx("ore_slope_hi");
    t.walk_slope_max = r.get_fx("walk_slope_max");
    t.walk_min_area_m2 = r.get_int("walk_min_area_m2");
    t.hab_slope_lo = r.get_fx("hab_slope_lo");
    t.hab_slope_hi = r.get_fx("hab_slope_hi");
    t.hab_weight_fertility = r.get_int("hab_weight_fertility");
    t.hab_weight_forage = r.get_int("hab_weight_forage");
    t.hab_weight_timber = r.get_int("hab_weight_timber");
    t.hab_weight_ore = r.get_int("hab_weight_ore");
    t.hab_water_min = r.get_fx("hab_water_min");
    t.hab_flood_hand_m = r.get_fx("hab_flood_hand_m");
    t.hab_flood_factor = r.get_fx("hab_flood_factor");
    t.hab_pocket_factor = r.get_fx("hab_pocket_factor");
    return t;
}

ResourceTuning resource_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::WORLDGEN_KIND, data::WORLDGEN_RESOURCES_ID);
    if (d == nullptr) {
        fatal("resources: the content has no definition worldgen.resources");
    }
    return resource_tuning_from(d->record);
}

void check_resource_tuning(const ResourceTuning& t) {
    const Fx zero{};
    const Fx one = Fx::ONE;
    const Fx ten = Fx::from_int(10);
    tuning_field(int_in(t.fresh_reach_m, 16, 10000), "fresh_reach_m");
    tuning_field(fx_in(t.fresh_rain_weight, zero, one), "fresh_rain_weight");
    tuning_field(int_in(t.fresh_rain_lo_mm, 0, 10000), "fresh_rain_lo_mm");
    tuning_field(int_in(t.fresh_rain_hi_mm, 0, 10000), "fresh_rain_hi_mm");
    tuning_field(fx_in(t.fert_slope_lo, zero, ten), "fert_slope_lo");
    tuning_field(fx_in(t.fert_slope_hi, zero, ten), "fert_slope_hi");
    tuning_field(fx_in(t.alluvial_bonus, zero, Fx::from_int(4)), "alluvial_bonus");
    tuning_field(fx_in(t.alluvial_hand_m, Fx::from_ratio(1, 10), Fx::from_int(100)), "alluvial_hand_m");
    tuning_field(fx_in(t.timber_slope_lo, zero, ten), "timber_slope_lo");
    tuning_field(fx_in(t.timber_slope_hi, zero, ten), "timber_slope_hi");
    tuning_field(int_in(t.fish_river_class_min, 1, geo::RIVER_CLASS_MAX), "fish_river_class_min");
    tuning_field(fx_in(t.fish_weight, zero, one), "fish_weight");
    tuning_field(int_in(t.fish_reach_m, 16, 10000), "fish_reach_m");
    tuning_field(int_in(t.ore_noise.wavelength_m, 64, 65536) && int_in(t.ore_noise.octaves, 1, 10) &&
                     fx_in(t.ore_noise.gain, Fx::from_ratio(1, 16), one),
                 "ore_noise");
    tuning_field(fx_in(t.ore_vein_lo, -one, one), "ore_vein_lo");
    tuning_field(fx_in(t.ore_vein_hi, -one, one), "ore_vein_hi");
    tuning_field(int_in(t.ore_elev_lo_m, 0, 3000), "ore_elev_lo_m");
    tuning_field(int_in(t.ore_elev_hi_m, 0, 3000), "ore_elev_hi_m");
    tuning_field(fx_in(t.ore_lowland, zero, one), "ore_lowland");
    tuning_field(fx_in(t.ore_slope_base, zero, one), "ore_slope_base");
    tuning_field(fx_in(t.ore_slope_lo, zero, ten), "ore_slope_lo");
    tuning_field(fx_in(t.ore_slope_hi, zero, ten), "ore_slope_hi");
    tuning_field(fx_in(t.walk_slope_max, Fx::from_ratio(1, 100), ten), "walk_slope_max");
    tuning_field(int_in(t.walk_min_area_m2, 16384, 1000000000), "walk_min_area_m2");
    tuning_field(fx_in(t.hab_slope_lo, zero, ten), "hab_slope_lo");
    tuning_field(fx_in(t.hab_slope_hi, zero, ten), "hab_slope_hi");
    tuning_field(int_in(t.hab_weight_fertility, 0, 1000), "hab_weight_fertility");
    tuning_field(int_in(t.hab_weight_forage, 0, 1000), "hab_weight_forage");
    tuning_field(int_in(t.hab_weight_timber, 0, 1000), "hab_weight_timber");
    tuning_field(int_in(t.hab_weight_ore, 0, 1000), "hab_weight_ore");
    tuning_field(fx_in(t.hab_water_min, zero, one), "hab_water_min");
    tuning_field(fx_in(t.hab_flood_hand_m, zero, Fx::from_int(100)), "hab_flood_hand_m");
    tuning_field(fx_in(t.hab_flood_factor, zero, one), "hab_flood_factor");
    tuning_field(fx_in(t.hab_pocket_factor, zero, one), "hab_pocket_factor");
    // The named checks of 4.2, in registration order.
    tuning_ordered(t.fresh_rain_lo_mm < t.fresh_rain_hi_mm, "fresh_rain_lo_mm must be below fresh_rain_hi_mm");
    tuning_ordered(t.fert_slope_lo < t.fert_slope_hi, "fert_slope_lo must be below fert_slope_hi");
    tuning_ordered(t.timber_slope_lo < t.timber_slope_hi, "timber_slope_lo must be below timber_slope_hi");
    tuning_ordered(t.ore_vein_lo < t.ore_vein_hi, "ore_vein_lo must be below ore_vein_hi");
    tuning_ordered(t.ore_elev_lo_m < t.ore_elev_hi_m, "ore_elev_lo_m must be below ore_elev_hi_m");
    tuning_ordered(t.ore_slope_lo < t.ore_slope_hi, "ore_slope_lo must be below ore_slope_hi");
    tuning_ordered(t.hab_slope_lo < t.hab_slope_hi, "hab_slope_lo must be below hab_slope_hi");
    tuning_ordered(t.hab_weight_fertility + t.hab_weight_forage + t.hab_weight_timber + t.hab_weight_ore >= 1,
                   "the hab_weight fields must not all be 0");
}

std::int64_t resource_walk_min_cells(const ResourceTuning& tuning) {
    check_resource_tuning(tuning);
    return (tuning.walk_min_area_m2 + 255) / 256;
}

// --- Helpers (5) -------------------------------------------------------------------------------------

Fx resource_ramp(Fx x, Fx a, Fx b) {
    if (!(a < b)) {
        fatal("resource_ramp: a must be below b");
    }
    if (x <= a) return Fx{};
    if (x >= b) return Fx::ONE;
    return div(x - a, b - a);
}

std::uint8_t resource_q255(Fx s) {
    const std::int64_t v = mul_int(clamp(s, Fx{}, Fx::ONE), 255).round_int();
    return static_cast<std::uint8_t>(std::clamp<std::int64_t>(v, 0, 255));
}

std::vector<std::int64_t> chamfer_metres(Grid grid, std::span<const std::uint8_t> source) {
    geo::check_grid_array(grid, source.size(), "chamfer_metres: source");
    std::vector<std::int64_t> d = chamfer_distance(grid, source);  // P1-02 H7 (D4)
    for (std::int64_t& v : d) {
        // d <= 2^40 + 7: 32 d + 5 < 2^46.
        v = std::min(RESOURCE_DISTANCE_MAX_M, (32 * v + 5) / 10);
    }
    return d;
}

// --- Per-cell seams --------------------------------------------------------------------------------

std::uint8_t fresh_water_cell(std::int64_t df, std::int64_t rainfall, const ResourceTuning& tuning) {
    check_resource_tuning(tuning);
    check_non_negative("fresh_water_cell", "df", df);
    if (rainfall < 0 || rainfall > 65535) {
        fatal("fresh_water_cell: rainfall outside 0..65535");
    }
    return fresh_water_of(df, rainfall, tuning, compile(tuning));
}

std::uint8_t fertility_cell(Fx fb, std::int64_t s2, std::int64_t hand, const ResourceTuning& tuning) {
    check_resource_tuning(tuning);
    check_unit("fertility_cell", fb);
    check_non_negative("fertility_cell", "hand", hand);
    if (hand > 65535) {
        fatal("fertility_cell: hand above 65535");
    }
    return fertility_of(fb, checked_tangent("fertility_cell", s2), hand, tuning);
}

std::uint8_t timber_cell(Fx tb, std::int64_t s2, const ResourceTuning& tuning) {
    check_resource_tuning(tuning);
    check_unit("timber_cell", tb);
    return timber_of(tb, checked_tangent("timber_cell", s2), tuning);
}

std::uint8_t forage_cell(Fx gb, std::int64_t dg, const ResourceTuning& tuning) {
    check_resource_tuning(tuning);
    check_unit("forage_cell", gb);
    check_non_negative("forage_cell", "dg", dg);
    if (dg > RESOURCE_DISTANCE_MAX_M) {
        fatal("forage_cell: dg above 32767");
    }
    return forage_of(gb, dg, tuning, compile(tuning));
}

std::uint8_t ore_cell(std::int64_t elevation, std::int64_t s2, Fx noise, const ResourceTuning& tuning) {
    check_resource_tuning(tuning);
    if (elevation < -32768 || elevation > 32767) {
        fatal("ore_cell: elevation outside the i16 range");
    }
    return ore_of(elevation, checked_tangent("ore_cell", s2), noise, tuning, compile(tuning));
}

Fx resource_ore_noise(std::uint64_t seed, std::int32_t x, std::int32_t y, const ResourceTuning& tuning) {
    return geo::fbm_m(stream_key(seed, streams::geo_resources_ore), cell_centre_m(x), cell_centre_m(y),
                      tuning.ore_noise);
}

std::uint8_t habitability_cell(std::int64_t fertility, std::int64_t fresh_water, std::int64_t forage,
                               std::int64_t timber, std::int64_t ore, std::int64_t s2, std::int64_t hand,
                               std::int64_t walk_area, const ResourceTuning& tuning) {
    check_resource_tuning(tuning);
    const std::array<std::int64_t, 5> scores = {fertility, fresh_water, forage, timber, ore};
    for (const std::int64_t v : scores) {
        if (v < 0 || v > 255) {
            fatal("habitability_cell: a score outside 0..255");
        }
    }
    check_non_negative("habitability_cell", "hand", hand);
    if (hand > 65535) {
        fatal("habitability_cell: hand above 65535");
    }
    check_non_negative("habitability_cell", "walk_area", walk_area);
    return habitability_of(scores, checked_tangent("habitability_cell", s2), hand, walk_area, tuning,
                           compile(tuning));
}

// --- R5 (D5) -------------------------------------------------------------------------------------------

WalkAreas walk_areas(Grid grid, std::span<const std::int64_t> surface, std::span<const std::uint8_t> water,
                     std::span<const std::int64_t> s2, Fx walk_slope_max, std::int64_t walk_min_cells) {
    geo::check_grid_array(grid, surface.size(), "walk_areas: surface");
    geo::check_grid_array(grid, water.size(), "walk_areas: water");
    geo::check_grid_array(grid, s2.size(), "walk_areas: s2");
    if (walk_slope_max < Fx::from_ratio(1, 100) || walk_slope_max > Fx::from_int(10)) {
        fatal("walk_areas: walk_slope_max outside [0.01, 10]");
    }
    if (walk_min_cells < 1) {
        fatal("walk_areas: walk_min_cells below 1");
    }
    const std::uint32_t n = grid.cells();
    for (std::uint32_t i = 0; i < n; ++i) {
        if (water[i] > WATER_OCEAN) {
            fatal("walk_areas: a water value is outside 0..3");
        }
        if (s2[i] < 0) {
            fatal("walk_areas: negative S2");
        }
        // S = elevation + water_depth lies in [-32768, 98302] on stored
        // layers; this bound keeps (Delta S)^2 below 2^36.
        if (surface[i] < -65536 || surface[i] > 131072) {
            fatal("walk_areas: surface outside [-65536, 131072]");
        }
    }
    const WalkLimits limits = walk_limits(walk_slope_max);
    std::vector<std::uint8_t> walkable(n, 0U);
    for (std::uint32_t i = 0; i < n; ++i) {
        walkable[i] = (is_land(water[i]) && s2[i] <= limits.s2_max) ? 1U : 0U;
    }
    // G2 labelling under `linked` (FIFO BFS, seeds in index order, 8-neighbour order).
    std::vector<std::uint32_t> label(n, 0U);
    std::vector<std::uint32_t> sizes(1, 0U);  // sizes[k] = cells of component k
    std::vector<CellIndex> queue;
    queue.reserve(n);
    std::uint32_t components = 0;
    for (CellIndex seed = 0; seed < n; ++seed) {
        if (walkable[seed] == 0U || label[seed] != 0U) {
            continue;
        }
        ++components;
        label[seed] = components;
        queue.clear();
        queue.push_back(seed);
        for (std::size_t head = 0; head < queue.size(); ++head) {
            const CellIndex c = queue[head];
            for (const geo::Direction code : geo::NEIGHBOURS_8) {
                CellIndex m = 0;
                if (!geo::neighbour(grid, c, code, m) || walkable[m] == 0U || label[m] != 0U) {
                    continue;
                }
                const std::int64_t ds = surface[c] - surface[m];  // |ds| < 2^18
                const auto ds2 = static_cast<std::uint64_t>(ds * ds);
                if (ds2 > (geo::is_diagonal(code) ? limits.step_diag : limits.step_orth)) {
                    continue;
                }
                label[m] = components;
                queue.push_back(m);
            }
        }
        sizes.push_back(static_cast<std::uint32_t>(queue.size()));
    }
    // Kept components, renumbered in label order (= order of smallest index).
    std::vector<std::uint32_t> id_of(sizes.size(), 0U);
    std::int64_t kept = 0;
    for (std::size_t k = 1; k < sizes.size(); ++k) {
        if (static_cast<std::int64_t>(sizes[k]) >= walk_min_cells) {
            ++kept;
            if (kept > WALK_AREA_MAX_ID) {
                fatal("resources: more than 65535 walk areas");
            }
            id_of[k] = static_cast<std::uint32_t>(kept);
        }
    }
    WalkAreas out;
    out.count = kept;
    out.ids.assign(n, 0U);
    for (std::uint32_t i = 0; i < n; ++i) {
        out.ids[i] = static_cast<std::uint16_t>(id_of[label[i]]);
    }
    return out;
}

// --- The whole stage ------------------------------------------------------------------------------------

ResourceLayers resources_layers(const ResourceInputs& in, const BiomeDefinitions& definitions,
                                const ResourceTuning& tuning, std::uint64_t seed, std::uint32_t threads) {
    check_threads(threads);
    check_resource_tuning(tuning);
    const BiomeValues values = biome_values(definitions);
    // R1: section 2.
    check_inputs(in, values);
    const Grid grid = in.grid;
    const std::uint32_t n = grid.cells();
    const Compiled compiled = compile(tuning);

    // R2: G7 with k = 1 (D5), then G8 (D1, G6 row blocks).
    const std::vector<std::int64_t> surface = geo::water_surface(grid, in.elevation, in.water_depth);
    const std::vector<std::int64_t> hand = geo::height_above_drainage(
        grid, surface, in.water, in.river, in.flow_dir, 1, std::string(PRECONDITION) + "G7: ");
    std::vector<std::int64_t> s2(n, 0);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            for (std::int32_t x = 0; x < grid.width; ++x) {
                s2[grid.index(x, y)] = geo::cell_slope_s2(grid, in.elevation, x, y);
            }
        }
    });

    // R3: the two chamfers (D4, sequential).
    std::vector<std::uint8_t> source(n, 0U);
    for (std::uint32_t i = 0; i < n; ++i) {
        source[i] = (in.water[i] == WATER_RIVER || in.water[i] == WATER_LAKE) ? 1U : 0U;
    }
    const std::vector<std::int64_t> df = chamfer_metres(grid, source);
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint8_t w = in.water[i];
        source[i] = (w == WATER_LAKE || w == WATER_OCEAN ||
                     (w == WATER_RIVER && std::int64_t{in.river[i]} >= tuning.fish_river_class_min))
                        ? 1U
                        : 0U;
    }
    const std::vector<std::int64_t> dg = chamfer_metres(grid, source);
    source.clear();
    source.shrink_to_fit();

    ResourceLayers out;
    out.fertility.assign(n, 0U);
    out.ore.assign(n, 0U);
    out.timber.assign(n, 0U);
    out.fresh_water.assign(n, 0U);
    out.forage.assign(n, 0U);
    out.habitability.assign(n, 0U);
    std::vector<Fx> tangent(n, Fx{});

    // R3 rest and R4 (D1, G6 row blocks; location-keyed noise on land cells only).
    const std::uint64_t k_ore = stream_key(seed, streams::geo_resources_ore);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            const Fx ym = cell_centre_m(y);
            for (std::int32_t x = 0; x < grid.width; ++x) {
                const CellIndex i = grid.index(x, y);
                const std::uint8_t w = in.water[i];
                if (w == WATER_LAKE) {
                    out.fresh_water[i] = 255U;
                    continue;
                }
                if (w == WATER_OCEAN) {
                    continue;
                }
                const Fx tan = geo::slope_tangent(s2[i]);
                tangent[i] = tan;
                const std::size_t b = in.biome[i];
                out.fresh_water[i] = fresh_water_of(df[i], in.rainfall[i], tuning, compiled);
                out.fertility[i] = fertility_of(values.fertility[b], tan, hand[i], tuning);
                out.timber[i] = timber_of(values.timber[b], tan, tuning);
                out.forage[i] = forage_of(values.forage[b], dg[i], tuning, compiled);
                const Fx noise = geo::fbm_m(k_ore, cell_centre_m(x), ym, tuning.ore_noise);
                out.ore[i] = ore_of(in.elevation[i], tan, noise, tuning, compiled);
            }
        }
    });

    // R5 (D5, sequential), after R2.
    WalkAreas walk = walk_areas(grid, surface, in.water, s2, tuning.walk_slope_max, resource_walk_min_cells(tuning));
    out.walk_area = std::move(walk.ids);

    // R6 (D1, G6 row blocks), after R3, R4 and R5, from the stored u8 values.
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            for (std::int32_t x = 0; x < grid.width; ++x) {
                const CellIndex i = grid.index(x, y);
                if (!is_land(in.water[i])) {
                    continue;
                }
                const std::array<std::int64_t, 5> scores = {out.fertility[i], out.fresh_water[i], out.forage[i],
                                                            out.timber[i], out.ore[i]};
                out.habitability[i] = habitability_of(scores, tangent[i], hand[i], out.walk_area[i], tuning, compiled);
            }
        }
    });
    return out;
}

namespace {

// A stored input layer of the catalogue type T and N x N cells; fatal
// otherwise (P1-05 section 2).
template <class T>
std::span<const T> input_layer(const StageContext& context, std::string_view name, std::int64_t n) {
    const db::LayerGrid* grid = context.find_layer(name);
    if (grid == nullptr) {
        fatal_text(std::string(PRECONDITION) + "missing layer '" + std::string(name) + "'");
    }
    const std::vector<T>* values = grid->get_if<T>();
    if (values == nullptr) {
        fatal_text(std::string(PRECONDITION) + "layer '" + std::string(name) + "' does not have its catalogue type");
    }
    if (grid->width != static_cast<std::uint32_t>(n) || grid->height != static_cast<std::uint32_t>(n)) {
        fatal_text(std::string(PRECONDITION) + std::string(name) + " length is not width * height");
    }
    return *values;
}

template <class T>
db::LayerGrid square_layer(std::int64_t n, std::vector<T> values) {
    db::LayerGrid grid;
    grid.width = static_cast<std::uint32_t>(n);
    grid.height = static_cast<std::uint32_t>(n);
    grid.values = std::move(values);
    return grid;
}

}  // namespace

void run_resources(StageContext& context) {
    const BiomeDefinitions definitions = biome_definitions_from(context.content());
    const ResourceTuning tuning = resource_tuning_from(context.content());
    const std::int64_t n = context.map_size();
    if (n < ELEVATION_MIN_N || n > ELEVATION_MAX_N) {
        fatal("resources: map size outside [16, 2048]");
    }
    // Section 2: every input layer present, in the order of the inputs table.
    for (const std::string_view name : {ELEVATION_LAYER, WATER_LAYER, WATER_DEPTH_LAYER, FLOW_DIR_LAYER,
                                        RIVER_LAYER, RAINFALL_LAYER, BIOME_LAYER}) {
        if (context.find_layer(name) == nullptr) {
            fatal_text(std::string(PRECONDITION) + "missing layer '" + std::string(name) + "'");
        }
    }
    ResourceInputs in;
    in.grid = geo::square_grid(static_cast<std::int32_t>(n));
    in.elevation = input_layer<std::int16_t>(context, ELEVATION_LAYER, n);
    in.water = input_layer<std::uint8_t>(context, WATER_LAYER, n);
    in.water_depth = input_layer<std::uint16_t>(context, WATER_DEPTH_LAYER, n);
    in.flow_dir = input_layer<std::uint8_t>(context, FLOW_DIR_LAYER, n);
    in.river = input_layer<std::uint8_t>(context, RIVER_LAYER, n);
    in.rainfall = input_layer<std::uint16_t>(context, RAINFALL_LAYER, n);
    in.biome = input_layer<std::uint8_t>(context, BIOME_LAYER, n);
    ResourceLayers layers = resources_layers(in, definitions, tuning, context.seed(), context.threads());
    // R7: one transaction (the pipeline's), in the order of section 3.
    context.write_layer(FERTILITY_LAYER, square_layer(n, std::move(layers.fertility)));
    context.write_layer(ORE_LAYER, square_layer(n, std::move(layers.ore)));
    context.write_layer(TIMBER_LAYER, square_layer(n, std::move(layers.timber)));
    context.write_layer(FRESH_WATER_LAYER, square_layer(n, std::move(layers.fresh_water)));
    context.write_layer(FORAGE_LAYER, square_layer(n, std::move(layers.forage)));
    context.write_layer(HABITABILITY_LAYER, square_layer(n, std::move(layers.habitability)));
    context.write_layer(WALK_AREA_LAYER, square_layer(n, std::move(layers.walk_area)));
}

}  // namespace worldgen::stages
