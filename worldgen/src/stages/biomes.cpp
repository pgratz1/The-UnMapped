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
#include "worldgen/core/wide.hpp"
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
    rule.river_dist_max_m = r.get_int("river_dist_max_m");
    rule.wet_frac_min = r.get_fx("wet_frac_min");
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
    // 16 d34 <= 3 river_dist_max_m <=> d34 <= (3 river_dist_max_m) div 16
    // (d34 >= 0, integers); INT64_MAX when skipped.
    std::int64_t d34_max = 0;
    // need = mul(wet_frac_min, ws).raw, compared as need x total <= count x
    // 2^32; 0 when skipped (wet_frac_min 0; a need of 0 holds everywhere).
    std::uint64_t wet_need = 0;
};

// The per-cell values of B2b and B2c that B4 compares.
struct CellWet {
    std::int64_t d34 = BIOME_D34_FAR;
    std::int64_t count = 0;
    std::int64_t total = 1;
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
    else if (!int_in(r.river_dist_max_m, 0, BIOME_RIVER_DIST_MAX_M)) bad = "river_dist_max_m";
    else if (!fx_in(r.wet_frac_min, 0, 1)) bad = "wet_frac_min";
    if (bad != nullptr) {
        throw FatalError(std::string("biomes: rule: ") + bad + " outside its schema range");
    }
}

CompiledRule compile_rule(const BiomeRule& r, Fx scale, Fx wet_scale) {
    constexpr std::int64_t I_MIN = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t I_MAX = std::numeric_limits<std::int64_t>::max();
    check_rule_ranges(r);
    if (scale < Fx::from_ratio(1, 4) || scale > Fx::from_int(4)) {
        fatal("biomes: rule: the rain scale is outside [0.25, 4]");
    }
    if (wet_scale < Fx::from_ratio(1, 4) || wet_scale > Fx::from_int(4)) {
        fatal("biomes: rule: the wet scale is outside [0.25, 4]");
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
    c.d34_max = r.river_dist_max_m == BIOME_RIVER_DIST_MAX_M ? I_MAX : (3 * r.river_dist_max_m) / 16;
    // need <= mul(1, 4).raw = 2^34 (P1-04 7); a need of 0 is skipped as it holds on every cell.
    c.wet_need = r.wet_frac_min == Fx{} ? 0U : static_cast<std::uint64_t>(mul(r.wet_frac_min, wet_scale).raw());
    return c;
}

// need x total <= count x 2^32, exactly (128-bit products; count, total >= 0).
bool wet_share_holds(std::uint64_t need, std::int64_t count, std::int64_t total) {
    const U128 lhs = mul_u64_wide(need, static_cast<std::uint64_t>(total));
    const U128 rhs = U128{static_cast<std::uint64_t>(count) >> 32U, static_cast<std::uint64_t>(count) << 32U};
    return lhs <= rhs;
}

bool compiled_matches(const CompiledRule& c, Fx tq, Fx rq, std::int64_t elevation_dm, std::int64_t s2,
                      std::int64_t hand_dm, std::int64_t coast_dist, const CellWet& wet) {
    return c.temp_lo <= tq && tq <= c.temp_hi && c.rain_lo <= rq && rq <= c.rain_hi && c.elev_lo_dm <= elevation_dm &&
           elevation_dm <= c.elev_hi_dm && s2 <= c.s2_max && Fx::from_int(hand_dm) <= c.hand_max_dm &&
           coast_dist <= c.coast_max && wet.d34 <= c.d34_max &&
           (c.wet_need == 0U || wet_share_holds(c.wet_need, wet.count, wet.total));
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

CompiledLookup compile_lookup(const BiomeDefinitions& defs, Fx scale, Fx wet_scale) {
    CompiledLookup out;
    for (const std::size_t k : defs.land_order) {
        const BiomeDef& b = defs.biomes.at(k);
        CompiledLookup::Entry e;
        e.code = static_cast<std::uint8_t>(b.code);
        for (const BiomeRule& r : b.rules) {
            e.rules.push_back(compile_rule(r, scale, wet_scale));
        }
        out.entries.push_back(std::move(e));
    }
    out.fallback = defs.fallback_code;
    return out;
}

std::uint8_t compiled_lookup(const CompiledLookup& lookup, Fx tq, Fx rq, std::int64_t elevation_dm, std::int64_t s2,
                             std::int64_t hand_dm, std::int64_t coast_dist, const CellWet& wet) {
    for (const CompiledLookup::Entry& e : lookup.entries) {
        for (const CompiledRule& r : e.rules) {
            if (compiled_matches(r, tq, rq, elevation_dm, s2, hand_dm, coast_dist, wet)) {
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

// --- B2b: river distance (sequential, exact) -----------------------------------------------------

// The internal "no target" value of B2b: 2^30 instead of the seam's 2^40.
// Finite values are below 4 max(W, H) <= 2^26 on any valid grid (at most
// 2^24 cells), so FAR32 + 4 fits int32 and never lowers a value, and every
// comparison of B4 gives the same result as with 2^40 (16 x 2^30 > 3 x
// 32766; P1-04 8).
constexpr std::int32_t FAR32 = std::int32_t{1} << 30;
static_assert(std::int64_t{16} * FAR32 > 3 * (BIOME_RIVER_DIST_MAX_M - 1));
static_assert(std::int64_t{4} * (std::int64_t{1} << 24) + 4 < FAR32);

void relax(std::int32_t& here, std::int32_t candidate) {
    if (candidate < here) here = candidate;
}

// The two raster passes of B2b on int32 values (FAR32 for "none").
std::vector<std::int32_t> river_distance_cells(Grid grid, std::span<const std::uint8_t> water,
                                               std::span<const std::uint8_t> river, std::int64_t k) {
    const std::int32_t w = grid.width;
    const std::int32_t h = grid.height;
    const auto at = [w](std::int32_t x, std::int32_t y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
    };
    std::vector<std::int32_t> d(grid.cells(), FAR32);
    for (std::size_t i = 0; i < d.size(); ++i) {
        if (water[i] == WATER_RIVER && static_cast<std::int64_t>(river[i]) >= k) d[i] = 0;
    }
    // Forward: (x-1, y) +3, (x-1, y-1) +4, (x, y-1) +3, (x+1, y-1) +4.
    for (std::int32_t y = 0; y < h; ++y) {
        for (std::int32_t x = 0; x < w; ++x) {
            std::int32_t& c = d[at(x, y)];
            if (x > 0) relax(c, d[at(x - 1, y)] + 3);
            if (y > 0) {
                if (x > 0) relax(c, d[at(x - 1, y - 1)] + 4);
                relax(c, d[at(x, y - 1)] + 3);
                if (x + 1 < w) relax(c, d[at(x + 1, y - 1)] + 4);
            }
        }
    }
    // Backward: (x+1, y) +3, (x+1, y+1) +4, (x, y+1) +3, (x-1, y+1) +4.
    for (std::int32_t y = h - 1; y >= 0; --y) {
        for (std::int32_t x = w - 1; x >= 0; --x) {
            std::int32_t& c = d[at(x, y)];
            if (x + 1 < w) relax(c, d[at(x + 1, y)] + 3);
            if (y + 1 < h) {
                if (x + 1 < w) relax(c, d[at(x + 1, y + 1)] + 4);
                relax(c, d[at(x, y + 1)] + 3);
                if (x > 0) relax(c, d[at(x - 1, y + 1)] + 4);
            }
        }
    }
    return d;
}

bool any_rule_uses_river_distance(const BiomeDefinitions& defs) {
    for (const std::size_t k : defs.land_order) {
        for (const BiomeRule& r : defs.biomes.at(k).rules) {
            if (r.river_dist_max_m != BIOME_RIVER_DIST_MAX_M) return true;
        }
    }
    return false;
}

bool any_rule_uses_wet_share(const BiomeDefinitions& defs) {
    for (const std::size_t k : defs.land_order) {
        for (const BiomeRule& r : defs.biomes.at(k).rules) {
            if (r.wet_frac_min != Fx{}) return true;
        }
    }
    return false;
}

// --- B2c: wet share (D2, two exact box passes, G6 row blocks) ------------------------------------

// The stage can never trip the entry guard (P1-04 7): wet_window_m <= 2000
// gives r_cells <= 125, and the largest map, 2048 x 2048, is <= 2^31 cells.
static_assert(BIOME_WET_WINDOW_MAX_M / geo::CELL_M == BIOME_WET_R_CELLS_MAX);
static_assert(std::int64_t{2048} * 2048 <= BIOME_WET_GRID_CELLS_MAX);

// The entry guard of B2c, in the order of P1-04 7, before any summation or
// allocation.
void check_wet_share_entry(Grid grid, std::size_t water_size, std::size_t hand_size, std::int64_t r_cells) {
    if (r_cells < 0 || r_cells > BIOME_WET_R_CELLS_MAX) {
        fatal("biomes: input precondition: B2c window r_cells outside 0..125");
    }
    const std::int64_t cells = std::int64_t{grid.width} * std::int64_t{grid.height};
    if (grid.width < 1 || grid.height < 1 || cells > BIOME_WET_GRID_CELLS_MAX) {
        fatal("biomes: input precondition: B2c grid is empty or has more than 2^31 cells");
    }
    if (cells > static_cast<std::int64_t>(geo::MAX_GRID_CELLS)) {
        fatal("biomes: input precondition: B2c grid has more than 2^24 cells");
    }
    if (static_cast<std::int64_t>(water_size) != cells || static_cast<std::int64_t>(hand_size) != cells) {
        fatal("biomes: input precondition: B2c array length is not width * height");
    }
}

// The clipped window count along one axis of length n with half-width h:
// |[k - h, k + h] intersected with [0, n)|.
std::int64_t clipped_count(std::int64_t k, std::int64_t h, std::int64_t n) {
    return std::min(n - 1, k + h) - std::max(std::int64_t{0}, k - h) + 1;
}

// The triangle total along one axis: t[k] = the sum of clipped_count(j)
// over the clipped window of k. Box(Box(1, h), h) at (x, y) is tx[x] x
// ty[y] (the clipped 2D box is separable; exact).
std::vector<std::int64_t> axis_triangle_total(std::int64_t n, std::int64_t h) {
    std::vector<std::int64_t> t(static_cast<std::size_t>(n), 0);
    for (std::int64_t k = 0; k < n; ++k) {
        std::int64_t s = 0;
        for (std::int64_t j = std::max(std::int64_t{0}, k - h); j <= std::min(n - 1, k + h); ++j) {
            s += clipped_count(j, h, n);
        }
        t[static_cast<std::size_t>(k)] = s;
    }
    return t;
}

std::size_t cell_at(std::int32_t w, std::int32_t x, std::int32_t y) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
}

// One clipped box along x of half-width h on every row (rows in G6 blocks):
// out[x, y] = the sum of in[x', y] over x' in [x - h, x + h] in the grid.
// Every value is at most 125^4 < 2^28, so int32 holds it (P1-04 7, 8).
void box_rows(Grid grid, std::span<const std::int32_t> in, std::span<std::int32_t> out, std::int32_t h,
              std::uint32_t threads) {
    const std::int32_t w = grid.width;
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            std::int64_t s = 0;  // the clipped window of x, sliding
            for (std::int32_t x = 0; x <= std::min(w - 1, h); ++x) s += in[cell_at(w, x, y)];
            for (std::int32_t x = 0; x < w; ++x) {
                out[cell_at(w, x, y)] = static_cast<std::int32_t>(s);
                if (x + h + 1 < w) s += in[cell_at(w, x + h + 1, y)];
                if (x - h >= 0) s -= in[cell_at(w, x - h, y)];
            }
        }
    });
}

// One clipped box along y of half-width h (output rows in G6 blocks; each
// block starts its own running column sums, so the split changes nothing).
void box_columns(Grid grid, std::span<const std::int32_t> in, std::span<std::int32_t> out, std::int32_t h,
                 std::uint32_t threads) {
    const std::int32_t w = grid.width;
    const std::int32_t rows = grid.height;
    const auto add_row = [&](std::vector<std::int64_t>& s, std::int32_t y, std::int64_t sign) {
        for (std::int32_t x = 0; x < w; ++x) s[static_cast<std::size_t>(x)] += sign * in[cell_at(w, x, y)];
    };
    geo::for_each_row_block(rows, threads, [&](const geo::RowBlock& block) {
        std::vector<std::int64_t> s(static_cast<std::size_t>(w), 0);
        for (std::int32_t y = std::max(0, block.y_begin - h); y <= std::min(rows - 1, block.y_begin + h); ++y) {
            add_row(s, y, 1);
        }
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            for (std::int32_t x = 0; x < w; ++x) {
                out[cell_at(w, x, y)] = static_cast<std::int32_t>(s[static_cast<std::size_t>(x)]);
            }
            if (y + 1 >= block.y_end) break;
            if (y + h + 1 < rows) add_row(s, y + h + 1, 1);
            if (y - h >= 0) add_row(s, y - h, -1);
        }
    });
}

// wet_count of B2c, after the entry guard: Box(Box(wet, h), h) with h =
// r_cells div 2. The clipped 2D box is the y box of the x box, and boxes
// along x and along y commute, so the passes run x, x, y, y (exact
// integers: the order changes no sum).
std::vector<std::int32_t> wet_count_cells(Grid grid, std::span<const std::uint8_t> water,
                                          std::span<const std::int64_t> hand, Fx wet_hand_m, std::int64_t r_cells,
                                          std::uint32_t threads) {
    check_wet_share_entry(grid, water.size(), hand.size(), r_cells);
    const Fx limit_dm = mul_int(wet_hand_m, 10);
    const auto h = static_cast<std::int32_t>(r_cells / 2);
    std::vector<std::int32_t> a(grid.cells(), 0);
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = (water[i] != WATER_OCEAN && Fx::from_int(hand[i]) <= limit_dm) ? 1 : 0;
    }
    if (h == 0) {
        return a;
    }
    std::vector<std::int32_t> b(grid.cells(), 0);
    box_rows(grid, a, b, h, threads);
    box_rows(grid, b, a, h, threads);
    box_columns(grid, a, b, h, threads);
    box_columns(grid, b, a, h, threads);
    return a;
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
    t.river_dist_class_min = record.get_int("river_dist_class_min");
    t.wet_window_m = record.get_int("wet_window_m");
    t.wet_hand_m = record.get_fx("wet_hand_m");
    t.wet_scale_512 = record.get_fx("wet_scale_512");
    t.wet_scale_1024 = record.get_fx("wet_scale_1024");
    t.wet_scale_2048 = record.get_fx("wet_scale_2048");
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

Fx biome_wet_scale(const BiomeTuning& tuning, std::int64_t n) {
    if (n < 1) {
        fatal("biome_wet_scale: map size below 1");
    }
    if (n <= 512) return tuning.wet_scale_512;
    if (n <= 1024) return tuning.wet_scale_1024;
    return tuning.wet_scale_2048;
}

Fx biome_rain_bound(Fx scale, std::int64_t bound_mm) {
    if (bound_mm == BIOME_RAIN_UNBOUNDED_MM) {
        return Fx::from_int(BIOME_RAIN_UNBOUNDED_MM);
    }
    return mul_int(scale, bound_mm);  // exact: |raw| <= 4 x 2^32 x 10^4 < 2^48 (P1-04 7)
}

// --- B2b (sequential) and B2c (D2) ---------------------------------------------------------------

std::vector<std::int64_t> biome_river_distance(Grid grid, std::span<const std::uint8_t> water,
                                               std::span<const std::uint8_t> river, std::int64_t k) {
    if (k < 1 || k > geo::RIVER_CLASS_MAX) {
        fatal("biome_river_distance: river class k outside 1..15");
    }
    geo::check_grid_array(grid, water.size(), "biome_river_distance: water");
    geo::check_grid_array(grid, river.size(), "biome_river_distance: river");
    const std::vector<std::int32_t> d = river_distance_cells(grid, water, river, k);
    std::vector<std::int64_t> out(d.size(), 0);
    for (std::size_t i = 0; i < d.size(); ++i) {
        out[i] = d[i] == FAR32 ? BIOME_D34_FAR : std::int64_t{d[i]};
    }
    return out;
}

BiomeWetShare biome_wet_share(Grid grid, std::span<const std::uint8_t> water, std::span<const std::int64_t> hand,
                              Fx wet_hand_m, std::int64_t r_cells, std::uint32_t threads) {
    check_wet_share_entry(grid, water.size(), hand.size(), r_cells);
    check_threads(threads);
    const std::vector<std::int32_t> count = wet_count_cells(grid, water, hand, wet_hand_m, r_cells, threads);
    const std::int64_t h = r_cells / 2;
    const std::vector<std::int64_t> tx = axis_triangle_total(grid.width, h);
    const std::vector<std::int64_t> ty = axis_triangle_total(grid.height, h);
    BiomeWetShare out;
    out.count.assign(count.begin(), count.end());
    out.total.resize(count.size());
    for (std::int32_t y = 0; y < grid.height; ++y) {
        for (std::int32_t x = 0; x < grid.width; ++x) {
            out.total[cell_at(grid.width, x, y)] = tx[static_cast<std::size_t>(x)] * ty[static_cast<std::size_t>(y)];
        }
    }
    return out;
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

namespace {

// The cell arguments of the B4 seams: S2 >= 0, d34 >= 0, 0 <= count <= total, total >= 1.
CellWet checked_cell(const char* seam, std::int64_t s2, std::int64_t d34, std::int64_t wet_count,
                     std::int64_t wet_total) {
    if (s2 < 0) {
        fatal_text(std::string(seam) + ": negative S2");
    }
    if (d34 < 0) {
        fatal_text(std::string(seam) + ": negative d34");
    }
    if (wet_total < 1 || wet_count < 0 || wet_count > wet_total) {
        fatal_text(std::string(seam) + ": wet share counts outside 0 <= count <= total, total >= 1");
    }
    return CellWet{d34, wet_count, wet_total};
}

}  // namespace

bool biome_rule_matches(const BiomeRule& rule, Fx scale, Fx wet_scale, Fx tq, Fx rq, std::int64_t elevation_dm,
                        std::int64_t s2, std::int64_t hand_dm, std::int64_t coast_dist, std::int64_t d34,
                        std::int64_t wet_count, std::int64_t wet_total) {
    const CellWet wet = checked_cell("biome_rule_matches", s2, d34, wet_count, wet_total);
    return compiled_matches(compile_rule(rule, scale, wet_scale), tq, rq, elevation_dm, s2, hand_dm, coast_dist, wet);
}

std::uint8_t biome_lookup(const BiomeDefinitions& definitions, Fx scale, Fx wet_scale, Fx tq, Fx rq,
                          std::int64_t elevation_dm, std::int64_t s2, std::int64_t hand_dm, std::int64_t coast_dist,
                          std::int64_t d34, std::int64_t wet_count, std::int64_t wet_total) {
    const CellWet wet = checked_cell("biome_lookup", s2, d34, wet_count, wet_total);
    return compiled_lookup(compile_lookup(definitions, scale, wet_scale), tq, rq, elevation_dm, s2, hand_dm,
                           coast_dist, wet);
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
    // B4's scales and the rule checks of 4.1, once per world, before B2b and B2c.
    const std::int64_t side = std::max(grid.width, grid.height);
    const CompiledLookup lookup =
        compile_lookup(definitions, biome_rain_scale(tuning, side), biome_wet_scale(tuning, side));
    // B2b and B2c, each only when some land rule reads its result (P1-04
    // B2b, B2c: skipping changes nothing, the comparisons are skipped).
    std::vector<std::int32_t> d34;
    if (any_rule_uses_river_distance(definitions)) {
        if (tuning.river_dist_class_min < 1 || tuning.river_dist_class_min > geo::RIVER_CLASS_MAX) {
            fatal("biomes: river_dist_class_min outside 1..15");
        }
        d34 = river_distance_cells(grid, in.water, in.river, tuning.river_dist_class_min);
    }
    std::vector<std::int32_t> wet_count;
    std::vector<std::int64_t> wet_tx;
    std::vector<std::int64_t> wet_ty;
    if (any_rule_uses_wet_share(definitions)) {
        // wet_window_m outside 0..2000 gives r_cells outside 0..125: the entry guard is fatal.
        const std::int64_t r_cells = tuning.wet_window_m < 0 ? -1 : tuning.wet_window_m / geo::CELL_M;
        wet_count = wet_count_cells(grid, in.water, hand, tuning.wet_hand_m, r_cells, threads);
        wet_tx = axis_triangle_total(grid.width, r_cells / 2);
        wet_ty = axis_triangle_total(grid.height, r_cells / 2);
    }
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
                CellWet wet;
                if (!d34.empty()) {
                    wet.d34 = d34[i];  // FAR32 for "no target": the same comparisons as 2^40
                }
                if (!wet_count.empty()) {
                    wet.count = wet_count[i];
                    wet.total = wet_tx[static_cast<std::size_t>(x)] * wet_ty[static_cast<std::size_t>(y)];
                }
                b4[i] = compiled_lookup(lookup, c.tq, c.rq, in.elevation[i], s2, hand[i], in.coast_dist[i], wet);
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
