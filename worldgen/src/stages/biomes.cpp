// Stage P1-04 biomes (docs/specs/p1-04-biomes.md). Steps B1 to B6, each
// named with its determinism rule of P1-04 section 7.

#include "worldgen/stages/biomes.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>

#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/parallel.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/layer_blob.hpp"
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

constexpr const char* PRECONDITION = "biomes: input precondition: ";

void check_threads(std::uint32_t threads) {
    if (threads < 1U || threads > MAX_THREADS) {
        fatal("biomes: thread count outside [1, 256]");
    }
}

// Metres of a cell centre along one axis: from_int(16 k + 8) (P1-04 5).
Fx cell_centre_m(std::int32_t k) {
    return Fx::from_int(geo::CELL_M * k + geo::CELL_M / 2);
}

WaterBody water_body_from(const std::string& text) {
    if (text == "none") return WaterBody::none;
    if (text == "lake") return WaterBody::lake;
    if (text == "ocean") return WaterBody::ocean;
    fatal("biomes: water_body is not none, lake or ocean");
}

BiomeRule rule_from(const data::Record& r) {
    BiomeRule rule;
    rule.temp_min_c = r.get_fx("temp_min_c");
    rule.temp_max_c = r.get_fx("temp_max_c");
    rule.rain_min_mm = r.get_int("rain_min_mm");
    rule.rain_max_mm = r.get_int("rain_max_mm");
    rule.elev_min_m = r.get_int("elev_min_m");
    rule.elev_max_m = r.get_int("elev_max_m");
    rule.slope_max = r.get_fx("slope_max");
    rule.hand_max_m = r.get_fx("hand_max_m");
    rule.coast_max_m = r.get_int("coast_max_m");
    return rule;
}

BiomeDef biome_from(const data::Definition& d) {
    const data::Record& r = d.record;
    BiomeDef b;
    b.key = d.id;
    b.code = r.get_int("code");
    b.name = r.get_string("name");
    b.colour = r.get_int("colour");
    b.water = r.get_bool("water");
    b.water_body = water_body_from(r.get_string("water_body"));
    b.priority = r.get_int("priority");
    b.fallback = r.get_bool("fallback");
    for (const data::Value& v : r.get_array("rules")) {
        b.rules.push_back(rule_from(v.as_record()));
    }
    b.fertility = r.get_fx("fertility");
    b.timber = r.get_fx("timber");
    b.region_noun = r.get_string("region_noun");
    return b;
}

// A rule with its bounds in the units B4 compares, computed once per world
// (the scaled rainfall bounds, the S2 limit of G8, the hand limit).
struct CompiledRule {
    Fx temp_lo;
    Fx temp_hi;
    Fx rain_lo;
    Fx rain_hi;
    std::int64_t elev_lo_dm = 0;
    std::int64_t elev_hi_dm = 0;
    std::int64_t s2_max = 0;
    Fx hand_max_dm;
    std::int64_t coast_max = 0;
};

// A bound at the extreme of its schema range means "no limit on that side"
// (P1-04 4.1, as amended): the comparison is skipped, so a jittered value
// beyond the stored range (rq above 10000, tq below -60 or above 50) or an
// extreme cell value still matches. Skipping is done by widening the
// compiled bound to the type's own extreme.
// Fatal ("biomes: rule: <field> outside its schema range") unless every
// field of a rule lies in its P1-04 4.1 schema range, so that no compiled
// bound can overflow whatever a caller of a public seam builds by hand
// (determinism audit S1).
void check_rule_ranges(const BiomeRule& r) {
    const auto fx_in = [](Fx v, std::int64_t lo, std::int64_t hi) {
        return Fx::from_int(lo) <= v && v <= Fx::from_int(hi);
    };
    const auto int_in = [](std::int64_t v, std::int64_t lo, std::int64_t hi) { return lo <= v && v <= hi; };
    const char* bad = nullptr;
    if (!fx_in(r.temp_min_c, BIOME_TEMP_MIN_C, BIOME_TEMP_MAX_C)) bad = "temp_min_c";
    else if (!fx_in(r.temp_max_c, BIOME_TEMP_MIN_C, BIOME_TEMP_MAX_C)) bad = "temp_max_c";
    else if (!int_in(r.rain_min_mm, 0, BIOME_RAIN_UNBOUNDED_MM)) bad = "rain_min_mm";
    else if (!int_in(r.rain_max_mm, 0, BIOME_RAIN_UNBOUNDED_MM)) bad = "rain_max_mm";
    else if (!int_in(r.elev_min_m, 0, BIOME_ELEV_MAX_M)) bad = "elev_min_m";
    else if (!int_in(r.elev_max_m, 0, BIOME_ELEV_MAX_M)) bad = "elev_max_m";
    else if (!fx_in(r.slope_max, 0, BIOME_SLOPE_MAX)) bad = "slope_max";
    else if (!fx_in(r.hand_max_m, 0, BIOME_HAND_MAX_M)) bad = "hand_max_m";
    else if (!int_in(r.coast_max_m, 0, BIOME_COAST_MAX_M)) bad = "coast_max_m";
    if (bad != nullptr) {
        throw FatalError(std::string("biomes: rule: ") + bad + " outside its schema range");
    }
}

CompiledRule compile_rule(const BiomeRule& r, Fx scale) {
    constexpr std::int64_t I_MIN = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t I_MAX = std::numeric_limits<std::int64_t>::max();
    check_rule_ranges(r);
    if (scale < Fx::from_ratio(1, 4) || scale > Fx::from_int(4)) {
        fatal("biomes: rule: the rain scale is outside [0.25, 4]");
    }
    CompiledRule c;
    c.temp_lo = r.temp_min_c == Fx::from_int(BIOME_TEMP_MIN_C) ? Fx::MIN : r.temp_min_c;
    c.temp_hi = r.temp_max_c == Fx::from_int(BIOME_TEMP_MAX_C) ? Fx::MAX : r.temp_max_c;
    c.rain_lo = r.rain_min_mm == 0 ? Fx::MIN : biome_rain_bound(scale, r.rain_min_mm);
    c.rain_hi = r.rain_max_mm == BIOME_RAIN_UNBOUNDED_MM ? Fx::MAX : biome_rain_bound(scale, r.rain_max_mm);
    c.elev_lo_dm = r.elev_min_m == 0 ? I_MIN : 10 * r.elev_min_m;
    c.elev_hi_dm = r.elev_max_m == BIOME_ELEV_MAX_M ? I_MAX : 10 * r.elev_max_m;
    // S2 2^64 <= (320 t.raw)^2 <=> S2 <= slope_s2_limit(t) (P1-00 G8).
    c.s2_max = r.slope_max == Fx::from_int(BIOME_SLOPE_MAX) ? I_MAX : geo::slope_s2_limit(r.slope_max);
    c.hand_max_dm = r.hand_max_m == Fx::from_int(BIOME_HAND_MAX_M) ? Fx::MAX : mul_int(r.hand_max_m, 10);
    c.coast_max = r.coast_max_m == BIOME_COAST_MAX_M ? I_MAX : r.coast_max_m;
    return c;
}

bool compiled_matches(const CompiledRule& c, Fx tq, Fx rq, std::int64_t elevation_dm, std::int64_t s2,
                      std::int64_t hand_dm, std::int64_t coast_dist) {
    return c.temp_lo <= tq && tq <= c.temp_hi && c.rain_lo <= rq && rq <= c.rain_hi && c.elev_lo_dm <= elevation_dm &&
           elevation_dm <= c.elev_hi_dm && s2 <= c.s2_max && Fx::from_int(hand_dm) <= c.hand_max_dm &&
           coast_dist <= c.coast_max;
}

// The land definitions of B4 in descending priority, with compiled rules.
struct CompiledLookup {
    struct Entry {
        std::uint8_t code = 0;
        std::vector<CompiledRule> rules;
    };
    std::vector<Entry> entries;
    std::uint8_t fallback = 0;
};

CompiledLookup compile_lookup(const BiomeDefinitions& defs, Fx scale) {
    CompiledLookup out;
    for (const std::size_t k : defs.land_order) {
        const BiomeDef& b = defs.biomes.at(k);
        CompiledLookup::Entry e;
        e.code = static_cast<std::uint8_t>(b.code);
        for (const BiomeRule& r : b.rules) {
            e.rules.push_back(compile_rule(r, scale));
        }
        out.entries.push_back(std::move(e));
    }
    out.fallback = defs.fallback_code;
    return out;
}

std::uint8_t compiled_lookup(const CompiledLookup& lookup, Fx tq, Fx rq, std::int64_t elevation_dm, std::int64_t s2,
                             std::int64_t hand_dm, std::int64_t coast_dist) {
    for (const CompiledLookup::Entry& e : lookup.entries) {
        for (const CompiledRule& r : e.rules) {
            if (compiled_matches(r, tq, rq, elevation_dm, s2, hand_dm, coast_dist)) {
                return e.code;
            }
        }
    }
    return lookup.fallback;
}

// B5 for one river cell: the most common B4 code of its existing dry
// 8-neighbours, ties to the smallest code; `own` if it has none.
std::uint8_t river_cell_biome(Grid grid, CellIndex c, std::span<const std::uint8_t> water,
                              std::span<const std::uint8_t> biome, std::uint8_t own) {
    std::array<std::uint8_t, 8> codes{};
    std::size_t count = 0;
    for (const geo::Direction code : geo::NEIGHBOURS_8) {
        CellIndex m = 0;
        if (geo::neighbour(grid, c, code, m) && water[m] == 0U) {
            codes[count] = biome[m];
            ++count;
        }
    }
    if (count == 0U) {
        return own;
    }
    std::uint8_t best = 0;
    std::size_t best_count = 0;
    for (std::size_t a = 0; a < count; ++a) {
        std::size_t n = 0;
        for (std::size_t b = 0; b < count; ++b) {
            if (codes[b] == codes[a]) ++n;
        }
        if (n > best_count || (n == best_count && codes[a] < best)) {
            best = codes[a];
            best_count = n;
        }
    }
    return best;
}

[[noreturn]] void fatal_text(const std::string& message) {
    throw FatalError(message);
}

}  // namespace

// --- Definitions (4.1) and tuning (4.2) --------------------------------------------------------

BiomeDefinitions biome_definitions_from(std::span<const data::Definition> definitions) {
    BiomeDefinitions out;
    for (const data::Definition& d : definitions) {
        out.biomes.push_back(biome_from(d));
    }
    std::stable_sort(out.biomes.begin(), out.biomes.end(),
                     [](const BiomeDef& a, const BiomeDef& b) { return a.code < b.code; });
    std::size_t oceans = 0;
    std::size_t lakes = 0;
    std::size_t fallbacks = 0;
    for (std::size_t i = 0; i < out.biomes.size(); ++i) {
        const BiomeDef& b = out.biomes[i];
        if (b.code < 1 || b.code > 254 || (i > 0 && out.biomes[i - 1].code == b.code)) {
            fatal("biomes: biome codes must be distinct and in 1..254");
        }
        if (b.water_body == WaterBody::ocean) {
            ++oceans;
            out.ocean_code = static_cast<std::uint8_t>(b.code);
        } else if (b.water_body == WaterBody::lake) {
            ++lakes;
            out.lake_code = static_cast<std::uint8_t>(b.code);
        } else if (b.priority > 0) {
            out.land_order.push_back(i);
        }
        if (b.fallback) {
            ++fallbacks;
            out.fallback_code = static_cast<std::uint8_t>(b.code);
            if (b.water_body != WaterBody::none) {
                fatal("biomes: the fallback biome must be a land biome");
            }
        }
    }
    if (oceans != 1U || lakes != 1U || fallbacks != 1U) {
        fatal("biomes: exactly one ocean, one lake and one fallback biome are required");
    }
    std::stable_sort(out.land_order.begin(), out.land_order.end(), [&out](std::size_t a, std::size_t b) {
        return out.biomes[a].priority > out.biomes[b].priority;
    });
    for (std::size_t i = 1; i < out.land_order.size(); ++i) {
        if (out.biomes[out.land_order[i - 1]].priority == out.biomes[out.land_order[i]].priority) {
            fatal("biomes: land biome priorities must be distinct");
        }
    }
    return out;
}

BiomeDefinitions biome_definitions_from(const data::ContentSet& content) {
    const data::KindContent* kind = content.find_kind(data::BIOME_KIND);
    if (kind == nullptr) {
        fatal("biomes: the content has no kind biome");
    }
    return biome_definitions_from(kind->definitions);
}

BiomeTuning biome_tuning_from(const data::Record& record) {
    BiomeTuning t;
    t.jitter_noise = geo::fbm_m_from(record.get("jitter_noise"));
    t.temp_jitter_c = record.get_fx("temp_jitter_c");
    t.rain_jitter = record.get_fx("rain_jitter");
    t.rain_scale_512 = record.get_fx("rain_scale_512");
    t.rain_scale_1024 = record.get_fx("rain_scale_1024");
    t.rain_scale_2048 = record.get_fx("rain_scale_2048");
    t.drain_river_class_min = record.get_int("drain_river_class_min");
    return t;
}

BiomeTuning biome_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::WORLDGEN_KIND, data::WORLDGEN_BIOMES_ID);
    if (d == nullptr) {
        fatal("biomes: the content has no definition worldgen.biomes");
    }
    return biome_tuning_from(d->record);
}

Fx biome_rain_scale(const BiomeTuning& tuning, std::int64_t n) {
    if (n < 1) {
        fatal("biome_rain_scale: map size below 1");
    }
    if (n <= 512) return tuning.rain_scale_512;
    if (n <= 1024) return tuning.rain_scale_1024;
    return tuning.rain_scale_2048;
}

Fx biome_rain_bound(Fx scale, std::int64_t bound_mm) {
    if (bound_mm == BIOME_RAIN_UNBOUNDED_MM) {
        return Fx::from_int(BIOME_RAIN_UNBOUNDED_MM);
    }
    return mul_int(scale, bound_mm);  // exact: |raw| <= 4 x 2^32 x 10^4 < 2^48 (P1-04 7)
}

// --- B3 (D1) -------------------------------------------------------------------------------------

BiomeClimate biome_jitter_cell(std::int64_t temperature, std::int64_t rainfall, Fx nt, Fx nr,
                               const BiomeTuning& tuning) {
    BiomeClimate c;
    c.tq = Fx::from_ratio(temperature, 10) + mul(tuning.temp_jitter_c, nt);
    c.rq = mul(Fx::from_int(rainfall), max(Fx{}, Fx::ONE + mul(tuning.rain_jitter, nr)));
    return c;
}

BiomeNoise biome_noise(std::uint64_t seed, std::int32_t x, std::int32_t y, const BiomeTuning& tuning) {
    const Fx xm = cell_centre_m(x);
    const Fx ym = cell_centre_m(y);
    return BiomeNoise{geo::fbm_m(stream_key(seed, streams::geo_biomes_temp_jitter), xm, ym, tuning.jitter_noise),
                      geo::fbm_m(stream_key(seed, streams::geo_biomes_rain_jitter), xm, ym, tuning.jitter_noise)};
}

// --- B4 ------------------------------------------------------------------------------------------

bool biome_rule_matches(const BiomeRule& rule, Fx scale, Fx tq, Fx rq, std::int64_t elevation_dm, std::int64_t s2,
                        std::int64_t hand_dm, std::int64_t coast_dist) {
    if (s2 < 0) {
        fatal("biome_rule_matches: negative S2");
    }
    return compiled_matches(compile_rule(rule, scale), tq, rq, elevation_dm, s2, hand_dm, coast_dist);
}

std::uint8_t biome_lookup(const BiomeDefinitions& definitions, Fx scale, Fx tq, Fx rq, std::int64_t elevation_dm,
                          std::int64_t s2, std::int64_t hand_dm, std::int64_t coast_dist) {
    if (s2 < 0) {
        fatal("biome_lookup: negative S2");
    }
    return compiled_lookup(compile_lookup(definitions, scale), tq, rq, elevation_dm, s2, hand_dm, coast_dist);
}

// --- B5 (D1 on the finished B4 grid, G6 row blocks) ---------------------------------------------

std::vector<std::uint8_t> biome_river_cells(Grid grid, std::span<const std::uint8_t> water,
                                            std::span<const std::uint8_t> biome, std::uint32_t threads) {
    check_threads(threads);
    geo::check_grid_array(grid, water.size(), "biome_river_cells: water");
    geo::check_grid_array(grid, biome.size(), "biome_river_cells: biome");
    std::vector<std::uint8_t> out(biome.begin(), biome.end());
    // Reads only the B4 values of dry cells (never written here), writes
    // only river cells of its own rows.
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            for (std::int32_t x = 0; x < grid.width; ++x) {
                const CellIndex c = grid.index(x, y);
                if (water[c] == WATER_RIVER) {
                    out[c] = river_cell_biome(grid, c, water, biome, biome[c]);
                }
            }
        }
    });
    return out;
}

// --- The whole stage ------------------------------------------------------------------------------

std::vector<std::uint8_t> biome_layer(const BiomeInputs& in, const BiomeDefinitions& definitions,
                                      const BiomeTuning& tuning, std::uint64_t seed, std::uint32_t threads) {
    check_threads(threads);
    const Grid grid = in.grid;
    geo::check_grid(grid);
    const std::size_t n = grid.cells();
    if (in.elevation.size() != n || in.water.size() != n || in.water_depth.size() != n || in.flow_dir.size() != n ||
        in.river.size() != n || in.coast_dist.size() != n || in.temperature.size() != n || in.rainfall.size() != n) {
        fatal("biomes: input precondition: an input layer does not have one value per cell");
    }
    // Precondition 2 (P1-04 section 2), every cell, before G7.
    for (std::size_t i = 0; i < n; ++i) {
        if (in.water[i] > WATER_OCEAN) {
            fatal("biomes: input precondition: a water value is outside 0..3");
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (in.river[i] > geo::RIVER_CLASS_MAX) {
            fatal("biomes: input precondition: a river value is outside 0..15");
        }
    }
    // B2: drainage height (G7 with k = drain_river_class_min; precondition 3).
    std::vector<std::int64_t> hand;
    {
        const std::vector<std::int64_t> surface = geo::water_surface(grid, in.elevation, in.water_depth);
        hand = geo::height_above_drainage(grid, surface, in.water, in.river, in.flow_dir,
                                          tuning.drain_river_class_min, "biomes: input precondition: G7: ");
    }
    // B4's scale, once per world.
    const CompiledLookup lookup = compile_lookup(definitions, biome_rain_scale(tuning, std::max(grid.width, grid.height)));
    const std::uint64_t k_tj = stream_key(seed, streams::geo_biomes_temp_jitter);
    const std::uint64_t k_rj = stream_key(seed, streams::geo_biomes_rain_jitter);

    // B3 and B4 (D1, G6 row blocks); G8 per cell.
    std::vector<std::uint8_t> b4(n, 0U);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            const Fx ym = cell_centre_m(y);
            for (std::int32_t x = 0; x < grid.width; ++x) {
                const CellIndex i = grid.index(x, y);
                const std::uint8_t w = in.water[i];
                if (w == WATER_OCEAN) {
                    b4[i] = definitions.ocean_code;
                    continue;
                }
                if (w == WATER_LAKE) {
                    b4[i] = definitions.lake_code;
                    continue;
                }
                const Fx xm = cell_centre_m(x);
                const Fx nt = geo::fbm_m(k_tj, xm, ym, tuning.jitter_noise);
                const Fx nr = geo::fbm_m(k_rj, xm, ym, tuning.jitter_noise);
                const BiomeClimate c = biome_jitter_cell(in.temperature[i], in.rainfall[i], nt, nr, tuning);
                const std::int64_t s2 = geo::cell_slope_s2(grid, in.elevation, x, y);
                b4[i] = compiled_lookup(lookup, c.tq, c.rq, in.elevation[i], s2, hand[i], in.coast_dist[i]);
            }
        }
    });
    hand.clear();
    hand.shrink_to_fit();
    // B5, after all of B4.
    return biome_river_cells(grid, in.water, b4, threads);
}

std::vector<db::BiomeRow> biome_table_rows(const BiomeDefinitions& definitions) {
    std::vector<db::BiomeRow> rows;
    for (const BiomeDef& b : definitions.biomes) {  // ascending code
        db::BiomeRow row;
        row.id = b.code;
        row.key = b.key;
        row.name = b.name;
        row.colour = b.colour;
        row.water = b.water ? 1 : 0;
        rows.push_back(std::move(row));
    }
    return rows;
}

namespace {

// A stored input layer of the catalogue type T and N x N cells; fatal
// otherwise (P1-04 section 2 precondition 1).
template <class T>
std::span<const T> input_layer(const StageContext& context, std::string_view name, std::int64_t n) {
    const db::LayerGrid* grid = context.find_layer(name);
    if (grid == nullptr) {
        fatal_text(std::string(PRECONDITION) + "layer '" + std::string(name) + "' is missing");
    }
    const std::vector<T>* values = grid->get_if<T>();
    if (values == nullptr) {
        fatal_text(std::string(PRECONDITION) + "layer '" + std::string(name) + "' does not have its catalogue type");
    }
    if (grid->width != static_cast<std::uint32_t>(n) || grid->height != static_cast<std::uint32_t>(n)) {
        fatal_text(std::string(PRECONDITION) + "layer '" + std::string(name) + "' is not N x N");
    }
    return *values;
}

}  // namespace

void run_biomes(StageContext& context) {
    const BiomeDefinitions definitions = biome_definitions_from(context.content());
    const BiomeTuning tuning = biome_tuning_from(context.content());
    const std::int64_t n = context.map_size();
    if (n < ELEVATION_MIN_N || n > ELEVATION_MAX_N) {
        fatal("biomes: map size outside [16, 2048]");
    }
    // Precondition 1: the eight input layers, in the order of section 2.
    BiomeInputs in;
    in.grid = geo::square_grid(static_cast<std::int32_t>(n));
    in.elevation = input_layer<std::int16_t>(context, ELEVATION_LAYER, n);
    in.water = input_layer<std::uint8_t>(context, WATER_LAYER, n);
    in.water_depth = input_layer<std::uint16_t>(context, WATER_DEPTH_LAYER, n);
    in.flow_dir = input_layer<std::uint8_t>(context, FLOW_DIR_LAYER, n);
    in.river = input_layer<std::uint8_t>(context, RIVER_LAYER, n);
    in.coast_dist = input_layer<std::int16_t>(context, COAST_DIST_LAYER, n);
    in.temperature = input_layer<std::int16_t>(context, TEMPERATURE_LAYER, n);
    in.rainfall = input_layer<std::uint16_t>(context, RAINFALL_LAYER, n);
    std::vector<std::uint8_t> biome = biome_layer(in, definitions, tuning, context.seed(), context.threads());
    // B6: one transaction (the pipeline's), the layer then the table rows.
    db::LayerGrid grid;
    grid.width = static_cast<std::uint32_t>(n);
    grid.height = static_cast<std::uint32_t>(n);
    grid.values = std::move(biome);
    context.write_layer(BIOME_LAYER, std::move(grid));
    for (const db::BiomeRow& row : biome_table_rows(definitions)) {
        context.write_biome(row);
    }
}

}  // namespace worldgen::stages
