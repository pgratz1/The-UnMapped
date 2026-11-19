// Stage P1-06 regions (docs/specs/p1-06-regions.md). Steps RG1 to RG14,
// each named with its determinism rule of P1-06 section 7.

#include "worldgen/stages/regions.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <queue>
#include <string>
#include <utility>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/parallel.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/geo/blocks.hpp"
#include "worldgen/geo/components.hpp"
#include "worldgen/geo/noise_schema.hpp"
#include "worldgen/geo/terrain.hpp"
#include "worldgen/stages/elevation.hpp"
#include "worldgen/stages/hydrology.hpp"
#include "worldgen/stages/resources.hpp"

namespace worldgen::stages {

namespace {

using geo::CellIndex;
using geo::Grid;

constexpr const char* PRECONDITION = "regions: input precondition: ";

[[noreturn]] void fatal_text(const std::string& message) {
    throw FatalError(message);
}

void check_threads(std::uint32_t threads) {
    if (threads < 1U || threads > MAX_THREADS) {
        fatal("regions: thread count outside [1, 256]");
    }
}

// Metres of a cell centre along one axis: from_int(16 k + 8) (P1-06 5).
Fx cell_centre_m(std::int32_t k) {
    return Fx::from_int(geo::CELL_M * k + geo::CELL_M / 2);
}

// RHA(a / b) for b > 0 (P0-01 5: round half away from zero).
std::int64_t rha(std::int64_t a, std::int64_t b) {
    return mul_div(a, 1, b);
}

bool is_land(std::uint8_t water) {
    return water != WATER_OCEAN;
}

bool is_dry(std::uint8_t water) {
    return water == WATER_DRY || water == WATER_RIVER;
}

std::string cell_at_text(Grid grid, std::size_t i) {
    const auto c = static_cast<CellIndex>(i);
    return "(" + db::format_i64(grid.x_of(c)) + ", " + db::format_i64(grid.y_of(c)) + ")";
}

// --- Tuning checks ---------------------------------------------------------------------------

void tuning_field(bool ok, const std::string& field) {
    if (!ok) {
        fatal_text("regions: tuning: " + field + " outside its schema range");
    }
}

bool fx_in(Fx v, Fx lo, Fx hi) {
    return lo <= v && v <= hi;
}

bool int_in(std::int64_t v, std::int64_t lo, std::int64_t hi) {
    return lo <= v && v <= hi;
}

// A word of record region_names or a biome region_noun (P1-06 4): the word
// grammar, 1 to 32 bytes.
bool is_region_word(std::string_view text) {
    return text.size() <= 32U && data::is_valid_word(text);
}

// The constants of RG7 derived from the tuning.
struct FloodCosts {
    std::int64_t river_dm = 0;  // 10 river_cost_m
    Fx lake_factor;
    Fx d160;                    // mul_int(SQRT2, 160)
};

FloodCosts flood_costs(const RegionTuning& t) {
    FloodCosts c;
    c.river_dm = 10 * t.river_cost_m;
    c.lake_factor = t.lake_cost_factor;
    c.d160 = mul_int(Fx::SQRT2, 160);
    return c;
}

// hz of RG7 for a step onto a cell of weight wt and water category w.
std::int64_t step_hz(Fx wt, bool diagonal, std::uint8_t w, const FloodCosts& c) {
    std::int64_t hz = diagonal ? mul(wt, c.d160).round_int() : mul_int(wt, 160).round_int();
    if (w == WATER_LAKE) {
        hz = mul_int(c.lake_factor, hz).round_int();
    }
    return hz;
}

}  // namespace

// --- Tuning (4) ------------------------------------------------------------------------------

RegionTuning region_tuning_from(const data::Record& r) {
    RegionTuning t;
    t.seed_tile_m = r.get_int("seed_tile_m");
    t.seed_spacing_m = r.get_int("seed_spacing_m");
    t.hand_ref_m = r.get_int("hand_ref_m");
    t.cost_noise = geo::fbm_m_from(r.get("cost_noise"));
    t.cost_noise_amp = r.get_fx("cost_noise_amp");
    t.lake_cost_factor = r.get_fx("lake_cost_factor");
    t.river_border_class = r.get_int("river_border_class");
    t.river_cost_m = r.get_int("river_cost_m");
    t.region_min_area_m2 = r.get_int("region_min_area_m2");
    t.ore_rich_min = r.get_int("ore_rich_min");
    t.ore_reach_m = r.get_int("ore_reach_m");
    t.name_central_frac = r.get_fx("name_central_frac");
    const data::Record& n = r.get_record("names");
    t.names.central = n.get_string("central");
    t.names.north = n.get_string("north");
    t.names.northeast = n.get_string("northeast");
    t.names.east = n.get_string("east");
    t.names.southeast = n.get_string("southeast");
    t.names.south = n.get_string("south");
    t.names.southwest = n.get_string("southwest");
    t.names.west = n.get_string("west");
    t.names.northwest = n.get_string("northwest");
    t.names.isle = n.get_string("isle");
    return t;
}

RegionTuning region_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::WORLDGEN_KIND, data::WORLDGEN_REGIONS_ID);
    if (d == nullptr) {
        fatal("regions: the content has no definition worldgen.regions");
    }
    return region_tuning_from(d->record);
}

std::array<std::string_view, 9> region_sector_words(const RegionNames& n) {
    return {n.central, n.north, n.northeast, n.east, n.southeast, n.south, n.southwest, n.west, n.northwest};
}

void check_region_tuning(const RegionTuning& t) {
    tuning_field(int_in(t.seed_tile_m, 256, 32768), "seed_tile_m");
    tuning_field(int_in(t.seed_spacing_m, 160, 32768), "seed_spacing_m");
    tuning_field(int_in(t.hand_ref_m, 1, 10000), "hand_ref_m");
    tuning_field(int_in(t.cost_noise.wavelength_m, 64, 65536) && int_in(t.cost_noise.octaves, 1, 10) &&
                     fx_in(t.cost_noise.gain, Fx::from_ratio(1, 16), Fx::ONE),
                 "cost_noise");
    tuning_field(fx_in(t.cost_noise_amp, Fx{}, Fx::from_ratio(9, 10)), "cost_noise_amp");
    tuning_field(fx_in(t.lake_cost_factor, Fx::ONE, Fx::from_int(10)), "lake_cost_factor");
    tuning_field(int_in(t.river_border_class, 1, 16), "river_border_class");
    tuning_field(int_in(t.river_cost_m, 0, 100000), "river_cost_m");
    tuning_field(int_in(t.region_min_area_m2, 0, 100000000), "region_min_area_m2");
    tuning_field(int_in(t.ore_rich_min, 1, 255), "ore_rich_min");
    tuning_field(int_in(t.ore_reach_m, 0, 10000), "ore_reach_m");
    tuning_field(fx_in(t.name_central_frac, Fx{}, Fx::HALF), "name_central_frac");
    const std::array<std::pair<const char*, const std::string*>, 10> words = {{
        {"central", &t.names.central},
        {"north", &t.names.north},
        {"northeast", &t.names.northeast},
        {"east", &t.names.east},
        {"southeast", &t.names.southeast},
        {"south", &t.names.south},
        {"southwest", &t.names.southwest},
        {"west", &t.names.west},
        {"northwest", &t.names.northwest},
        {"isle", &t.names.isle},
    }};
    for (const auto& [name, word] : words) {
        tuning_field(is_region_word(*word), std::string("names.") + name);
    }
    if (t.seed_tile_m > 2 * t.seed_spacing_m) {
        fatal("regions: tuning: seed_tile_m must be at most twice seed_spacing_m");
    }
}

std::int64_t region_min_cells(const RegionTuning& tuning) {
    check_region_tuning(tuning);
    return (tuning.region_min_area_m2 + 255) / 256;
}

// --- RG3 ---------------------------------------------------------------------------------------

Fx region_cost_noise(std::uint64_t seed, std::int32_t x, std::int32_t y, const RegionTuning& tuning) {
    check_region_tuning(tuning);
    return geo::fbm_m(stream_key(seed, streams::geo_regions_cost), cell_centre_m(x), cell_centre_m(y),
                      tuning.cost_noise);
}

namespace {

Fx step_weight_of(Fx nz, std::int64_t hand, const RegionTuning& t) {
    const Fx nf = Fx::ONE + mul(t.cost_noise_amp, nz);
    const Fx hf = Fx::ONE + Fx::from_ratio(hand, 10 * t.hand_ref_m);
    return mul(nf, hf);
}

}  // namespace

Fx region_step_weight(Fx nz, std::int64_t hand, const RegionTuning& tuning) {
    check_region_tuning(tuning);
    if (nz < -Fx::ONE || nz > Fx::ONE) {
        fatal("region_step_weight: noise outside [-1, 1]");
    }
    if (hand < 0 || hand > REGION_HAND_MAX_DM) {
        fatal("region_step_weight: hand outside [0, 131070]");
    }
    return step_weight_of(nz, hand, tuning);
}

// --- RG4 to RG6 ----------------------------------------------------------------------------------

RegionSite region_site(Grid grid, CellIndex cell, std::int64_t habitability, std::int64_t surface,
                       std::uint64_t seed) {
    geo::check_grid(grid);
    if (cell >= grid.cells()) {
        fatal("region_site: cell outside the grid");
    }
    const std::uint64_t key = stream_key(seed, streams::geo_regions_seed);
    return RegionSite{cell, habitability, surface, hash_at(key, grid.x_of(cell), grid.y_of(cell))};
}

bool region_site_better(const RegionSite& a, const RegionSite& b) noexcept {
    if (a.habitability != b.habitability) return a.habitability > b.habitability;
    if (a.surface != b.surface) return a.surface < b.surface;  // -S larger
    if (a.hash != b.hash) return a.hash > b.hash;
    return a.cell < b.cell;
}

std::vector<RegionSite> region_candidates(Grid grid, std::span<const std::uint8_t> water,
                                          std::span<const std::uint8_t> habitability,
                                          std::span<const std::int64_t> surface, std::uint64_t seed,
                                          const RegionTuning& tuning) {
    check_region_tuning(tuning);
    geo::check_grid_array(grid, water.size(), "region_candidates: water");
    geo::check_grid_array(grid, habitability.size(), "region_candidates: habitability");
    geo::check_grid_array(grid, surface.size(), "region_candidates: surface");
    const std::uint64_t key = stream_key(seed, streams::geo_regions_seed);
    const auto t = static_cast<std::int32_t>(tuning.seed_tile_m / geo::CELL_M);  // >= 16
    std::vector<RegionSite> out;
    for (std::int32_t ty = 0; ty * t < grid.height; ++ty) {
        for (std::int32_t tx = 0; tx * t < grid.width; ++tx) {
            bool found = false;
            RegionSite best;
            const std::int32_t y_end = std::min(grid.height, ty * t + t);
            const std::int32_t x_end = std::min(grid.width, tx * t + t);
            for (std::int32_t y = ty * t; y < y_end; ++y) {
                for (std::int32_t x = tx * t; x < x_end; ++x) {
                    const CellIndex i = grid.index(x, y);
                    if (water[i] != WATER_DRY) {
                        continue;
                    }
                    const RegionSite s{i, habitability[i], surface[i], hash_at(key, x, y)};
                    if (!found || region_site_better(s, best)) {
                        best = s;
                        found = true;
                    }
                }
            }
            if (found) {
                out.push_back(best);
            }
        }
    }
    return out;
}

std::vector<std::uint8_t> region_border_rivers(Grid grid, std::span<const std::uint8_t> water,
                                               std::span<const std::uint8_t> river,
                                               std::int64_t river_border_class) {
    geo::check_grid_array(grid, water.size(), "region_border_rivers: water");
    geo::check_grid_array(grid, river.size(), "region_border_rivers: river");
    if (river_border_class < 1 || river_border_class > 16) {
        fatal("region_border_rivers: river_border_class outside [1, 16]");
    }
    std::vector<std::uint8_t> out(grid.cells(), 0U);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = (water[i] == WATER_RIVER && std::int64_t{river[i]} >= river_border_class) ? 1U : 0U;
    }
    return out;
}

bool region_crosses(Grid grid, std::span<const std::uint8_t> border_river, CellIndex s, CellIndex c) {
    geo::check_grid_array(grid, border_river.size(), "region_crosses: border_river");
    if (s >= grid.cells() || c >= grid.cells()) {
        fatal("region_crosses: cell outside the grid");
    }
    const std::int64_t sx = grid.x_of(s);
    const std::int64_t sy = grid.y_of(s);
    const std::int64_t dx = std::int64_t{grid.x_of(c)} - sx;
    const std::int64_t dy = std::int64_t{grid.y_of(c)} - sy;
    const std::int64_t m = std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
    const auto br = [&](std::int64_t x, std::int64_t y) {
        return border_river[grid.index(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y))] != 0U;
    };
    std::int64_t px = sx;
    std::int64_t py = sy;
    for (std::int64_t j = 1; j <= m; ++j) {
        // |dx j| <= 2^24 x 2^24 at most: no overflow.
        const std::int64_t qx = sx + rha(dx * j, m);
        const std::int64_t qy = sy + rha(dy * j, m);
        if (br(qx, qy)) {
            return true;
        }
        if (qx != px && qy != py && br(qx, py) && br(px, qy)) {
            return true;  // a diagonal slip between two river cells
        }
        px = qx;
        py = qy;
    }
    return false;
}

std::vector<CellIndex> region_seeds(std::span<const RegionSite> candidates, Grid grid,
                                    std::span<const std::uint8_t> water, std::span<const std::uint8_t> river,
                                    const RegionTuning& tuning) {
    check_region_tuning(tuning);
    const std::vector<std::uint8_t> border = region_border_rivers(grid, water, river, tuning.river_border_class);
    std::vector<RegionSite> sorted(candidates.begin(), candidates.end());
    std::vector<std::uint8_t> listed(grid.cells(), 0U);
    for (const RegionSite& s : sorted) {
        if (s.cell >= grid.cells()) {
            fatal("region_seeds: a candidate outside the grid");
        }
        if (listed[s.cell] != 0U) {
            fatal("region_seeds: a candidate cell is listed twice");
        }
        listed[s.cell] = 1U;
    }
    // D7: a strict total order (cells are distinct), so the order is unique.
    // determinism-lint: allow(unstable-sort) -- region_site_better is a strict total order on distinct cells (ends in the cell index), so the sorted order is unique
    std::sort(sorted.begin(), sorted.end(), region_site_better);
    const std::int64_t spacing2 = tuning.seed_spacing_m * tuning.seed_spacing_m;
    std::vector<CellIndex> accepted;
    for (const RegionSite& c : sorted) {
        const std::int64_t cx = grid.x_of(c.cell);
        const std::int64_t cy = grid.y_of(c.cell);
        bool rejected = false;
        for (const CellIndex s : accepted) {  // acceptance order; stop at the first blocking seed
            const std::int64_t ddx = geo::CELL_M * (cx - grid.x_of(s));
            const std::int64_t ddy = geo::CELL_M * (cy - grid.y_of(s));
            if (ddx * ddx + ddy * ddy < spacing2 && !region_crosses(grid, border, s, c.cell)) {
                rejected = true;
                break;
            }
        }
        if (!rejected) {
            accepted.push_back(c.cell);
        }
    }
    return accepted;
}

RegionLandmasses region_landmasses(Grid grid, std::span<const std::uint8_t> water) {
    geo::check_grid_array(grid, water.size(), "region_landmasses: water");
    std::vector<std::uint8_t> mask(grid.cells(), 0U);
    for (std::size_t i = 0; i < mask.size(); ++i) {
        mask[i] = is_land(water[i]) ? 1U : 0U;
    }
    geo::Components comps = geo::label_components(grid, mask, geo::Connectivity::eight);
    return RegionLandmasses{std::move(comps.label), comps.count};
}

std::vector<CellIndex> region_landmass_seeds(Grid grid, std::span<const std::uint8_t> water,
                                             std::span<const std::uint8_t> habitability,
                                             std::span<const std::int64_t> surface,
                                             const RegionLandmasses& landmasses, std::vector<CellIndex> seeds,
                                             std::uint64_t seed) {
    geo::check_grid_array(grid, water.size(), "region_landmass_seeds: water");
    geo::check_grid_array(grid, habitability.size(), "region_landmass_seeds: habitability");
    geo::check_grid_array(grid, surface.size(), "region_landmass_seeds: surface");
    geo::check_grid_array(grid, landmasses.label.size(), "region_landmass_seeds: landmass labels");
    std::vector<std::uint8_t> seeded(std::size_t{landmasses.count} + 1U, 0U);
    for (const CellIndex s : seeds) {
        if (s >= grid.cells() || landmasses.label[s] == 0U || landmasses.label[s] > landmasses.count) {
            fatal("region_landmass_seeds: a seed outside the grid or not on land");
        }
        seeded[landmasses.label[s]] = 1U;
    }
    const std::uint64_t key = stream_key(seed, streams::geo_regions_seed);
    std::vector<RegionSite> best(std::size_t{landmasses.count} + 1U);
    std::vector<std::uint8_t> has_site(std::size_t{landmasses.count} + 1U, 0U);
    std::vector<CellIndex> first(std::size_t{landmasses.count} + 1U, 0U);
    std::vector<std::uint8_t> has_first(std::size_t{landmasses.count} + 1U, 0U);
    for (CellIndex i = 0; i < grid.cells(); ++i) {
        const std::uint32_t k = landmasses.label[i];
        if (k == 0U || k > landmasses.count || seeded[k] != 0U) {
            continue;
        }
        if (has_first[k] == 0U) {
            first[k] = i;
            has_first[k] = 1U;
        }
        if (water[i] != WATER_DRY) {
            continue;
        }
        const RegionSite s{i, habitability[i], surface[i], hash_at(key, grid.x_of(i), grid.y_of(i))};
        if (has_site[k] == 0U || region_site_better(s, best[k])) {
            best[k] = s;
            has_site[k] = 1U;
        }
    }
    for (std::uint32_t k = 1; k <= landmasses.count; ++k) {
        if (seeded[k] != 0U) {
            continue;
        }
        if (has_first[k] == 0U) {
            fatal("region_landmass_seeds: a landmass label with no cell");
        }
        seeds.push_back(has_site[k] != 0U ? best[k].cell : first[k]);
    }
    return seeds;
}

// --- RG7 ---------------------------------------------------------------------------------------------

std::int64_t region_step_cost(Grid grid, std::span<const Fx> wt, std::span<const std::uint8_t> water,
                              std::span<const std::uint8_t> border_river, CellIndex c, geo::Direction d,
                              const RegionTuning& tuning) {
    check_region_tuning(tuning);
    geo::check_grid_array(grid, wt.size(), "region_step_cost: wt");
    geo::check_grid_array(grid, water.size(), "region_step_cost: water");
    geo::check_grid_array(grid, border_river.size(), "region_step_cost: border_river");
    CellIndex m = 0;
    if (c >= grid.cells() || d > 7U || !geo::neighbour(grid, c, d, m)) {
        fatal("region_step_cost: no neighbour in that direction");
    }
    const FloodCosts costs = flood_costs(tuning);
    std::int64_t cost = step_hz(wt[m], geo::is_diagonal(d), water[m], costs);
    if (border_river[m] != 0U) {
        cost += costs.river_dm;
    }
    if (geo::is_diagonal(d)) {
        const CellIndex c1 = grid.index(grid.x_of(m), grid.y_of(c));
        const CellIndex c2 = grid.index(grid.x_of(c), grid.y_of(m));
        if (border_river[c1] != 0U && border_river[c2] != 0U) {
            cost += costs.river_dm;
        }
    }
    return cost;
}

namespace {

// One entry of the RG7 queue: popped by the smallest (k, q).
struct FloodEntry {
    std::int64_t k = 0;
    std::uint64_t q = 0;
    CellIndex c = 0;
};

// std::priority_queue keeps the "largest" first; this puts the smallest
// (k, q) on top. (k, q) is unique per entry (q is), so the pop order is the
// same with any heap implementation.
struct FloodLater {
    bool operator()(const FloodEntry& a, const FloodEntry& b) const noexcept {
        return a.k != b.k ? a.k > b.k : a.q > b.q;
    }
};

}  // namespace

RegionFlood region_flood(Grid grid, std::span<const Fx> wt, std::span<const std::uint8_t> water,
                         std::span<const std::uint8_t> river, std::span<const CellIndex> seeds,
                         const RegionTuning& tuning) {
    check_region_tuning(tuning);
    geo::check_grid_array(grid, wt.size(), "region_flood: wt");
    geo::check_grid_array(grid, water.size(), "region_flood: water");
    geo::check_grid_array(grid, river.size(), "region_flood: river");
    const std::uint32_t n = grid.cells();
    for (std::uint32_t i = 0; i < n; ++i) {
        if (water[i] > WATER_OCEAN || river[i] > geo::RIVER_CLASS_MAX) {
            fatal("region_flood: a water or river value is out of range");
        }
        if (is_land(water[i]) && wt[i] <= Fx{}) {
            fatal("region_flood: a land cell has a weight of 0 or below");
        }
        // wt < 2^15 ONE keeps every step below 2^28 (P1-06 7).
        if (is_land(water[i]) && wt[i] >= Fx::from_int(std::int64_t{1} << 15)) {
            fatal("region_flood: a land cell has a weight of 2^15 or above");
        }
    }
    const std::vector<std::uint8_t> border = region_border_rivers(grid, water, river, tuning.river_border_class);
    const FloodCosts costs = flood_costs(tuning);
    // Per land cell m: the orthogonal and diagonal cost of a step onto m
    // (hz with the lake factor, plus rv(m)); the corner term is added below.
    std::vector<std::int64_t> onto4(n, 0);
    std::vector<std::int64_t> onto8(n, 0);
    for (std::uint32_t i = 0; i < n; ++i) {
        if (!is_land(water[i])) {
            continue;
        }
        const std::int64_t rv = border[i] != 0U ? costs.river_dm : 0;
        onto4[i] = step_hz(wt[i], false, water[i], costs) + rv;
        onto8[i] = step_hz(wt[i], true, water[i], costs) + rv;
    }

    RegionFlood out;
    out.owner.assign(n, 0U);
    out.cost.assign(n, REGION_COST_INF);
    std::vector<std::uint8_t> done(n, 0U);
    // determinism-lint: allow(heap-order) -- FloodLater orders by (k, q) and q is the push sequence number, unique per entry: a strict total order ending in seq (P1-00 D6)
    std::priority_queue<FloodEntry, std::vector<FloodEntry>, FloodLater> queue;
    std::uint64_t seq = 0;
    for (std::size_t s = 0; s < seeds.size(); ++s) {
        const CellIndex c = seeds[s];
        if (c >= n || !is_land(water[c])) {
            fatal("region_flood: a seed outside the grid or on ocean");
        }
        if (out.owner[c] != 0U) {
            fatal("region_flood: a seed cell is listed twice");
        }
        out.cost[c] = 0;
        out.owner[c] = static_cast<std::uint32_t>(s + 1U);
        queue.push(FloodEntry{0, seq, c});
        ++seq;
    }
    while (!queue.empty()) {
        const FloodEntry e = queue.top();
        queue.pop();
        if (done[e.c] != 0U) {
            continue;
        }
        done[e.c] = 1U;
        const std::int32_t cx = grid.x_of(e.c);
        const std::int32_t cy = grid.y_of(e.c);
        for (const geo::Direction d : geo::NEIGHBOURS_8) {
            CellIndex m = 0;
            if (!geo::neighbour(grid, e.c, d, m) || !is_land(water[m]) || done[m] != 0U) {
                continue;
            }
            std::int64_t step = 0;
            if (geo::is_diagonal(d)) {
                step = onto8[m];
                const CellIndex c1 = grid.index(cx + geo::D8_DX[d], cy);
                const CellIndex c2 = grid.index(cx, cy + geo::D8_DY[d]);
                if (border[c1] != 0U && border[c2] != 0U) {
                    step += costs.river_dm;
                }
            } else {
                step = onto4[m];
            }
            const std::int64_t nk = e.k + step;  // < 2^50 (P1-06 7)
            if (nk < out.cost[m]) {
                out.cost[m] = nk;
                out.owner[m] = out.owner[e.c];
                queue.push(FloodEntry{nk, seq, m});
                ++seq;
            }
        }
    }
    return out;
}

// --- RG8, RG9 ------------------------------------------------------------------------------------------

namespace {

// Border pair counts between two regions (RG8): n, and nr = pairs where
// neither cell is a border river cell.
struct PairCounts {
    std::int64_t n = 0;
    std::int64_t nr = 0;
};

}  // namespace

std::vector<std::uint32_t> region_merge(Grid grid, std::span<const std::uint32_t> owner,
                                        std::span<const std::uint8_t> water, std::span<const std::uint8_t> river,
                                        std::uint32_t count, std::int64_t min_cells, const RegionTuning& tuning) {
    check_region_tuning(tuning);
    geo::check_grid_array(grid, owner.size(), "region_merge: owner");
    const std::vector<std::uint8_t> border = region_border_rivers(grid, water, river, tuning.river_border_class);
    const std::uint32_t n = grid.cells();
    std::vector<std::int64_t> cells(std::size_t{count} + 1U, 0);
    for (std::uint32_t i = 0; i < n; ++i) {
        const bool land = is_land(water[i]);
        if (land != (owner[i] != 0U) || owner[i] > count) {
            fatal("region_merge: an owner is 0 on land, above the count, or non-zero on ocean");
        }
        ++cells[owner[i]];
    }
    // Neighbour counts per region, keyed by the other region (ordered map:
    // iteration in ascending number).
    std::vector<std::map<std::uint32_t, PairCounts>> nb(std::size_t{count} + 1U);
    const auto add_pair = [&](CellIndex p, CellIndex q) {
        const std::uint32_t a = owner[p];
        const std::uint32_t b = owner[q];
        if (a == 0U || b == 0U || a == b) {
            return;
        }
        const bool river_pair = border[p] != 0U || border[q] != 0U;
        PairCounts& ab = nb[a][b];
        PairCounts& ba = nb[b][a];
        ++ab.n;
        ++ba.n;
        if (!river_pair) {
            ++ab.nr;
            ++ba.nr;
        }
    };
    for (std::int32_t y = 0; y < grid.height; ++y) {
        for (std::int32_t x = 0; x < grid.width; ++x) {
            const CellIndex i = grid.index(x, y);
            if (x + 1 < grid.width) add_pair(i, i + 1U);
            if (y + 1 < grid.height) add_pair(i, i + static_cast<CellIndex>(grid.width));
        }
    }
    std::vector<std::uint32_t> target(std::size_t{count} + 1U);
    for (std::uint32_t r = 0; r <= count; ++r) {
        target[r] = r;
    }
    std::vector<std::uint8_t> alive(std::size_t{count} + 1U, 1U);
    for (;;) {
        std::uint32_t r = 0;  // the smallest (cells, number) small region with a neighbour
        for (std::uint32_t k = 1; k <= count; ++k) {
            if (alive[k] == 0U || cells[k] >= min_cells || nb[k].empty()) {
                continue;
            }
            if (r == 0U || cells[k] < cells[r]) {
                r = k;
            }
        }
        if (r == 0U) {
            break;
        }
        std::uint32_t t = 0;  // the neighbour with the largest (nr, n), ties to the smaller number
        PairCounts best;
        for (const auto& [u, c] : nb[r]) {
            if (t == 0U || c.nr > best.nr || (c.nr == best.nr && c.n > best.n)) {
                t = u;
                best = c;
            }
        }
        cells[t] += cells[r];
        cells[r] = 0;
        for (const auto& [u, c] : nb[r]) {
            nb[u].erase(r);
            if (u == t) {
                continue;
            }
            PairCounts& tu = nb[t][u];
            PairCounts& ut = nb[u][t];
            tu.n += c.n;
            tu.nr += c.nr;
            ut.n += c.n;
            ut.nr += c.nr;
        }
        nb[r].clear();
        alive[r] = 0U;
        target[r] = t;
    }
    std::vector<std::uint32_t> root(std::size_t{count} + 1U);
    for (std::uint32_t k = 0; k <= count; ++k) {
        std::uint32_t r = k;
        while (target[r] != r) {
            r = target[r];
        }
        root[k] = r;
    }
    std::vector<std::uint32_t> out(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        out[i] = root[owner[i]];
    }
    return out;
}

RegionNumbering region_renumber(std::span<const std::uint32_t> owner) {
    std::map<std::uint32_t, std::uint16_t> id_of;
    RegionNumbering out;
    out.region.assign(owner.size(), 0U);
    for (std::size_t i = 0; i < owner.size(); ++i) {
        const std::uint32_t o = owner[i];
        if (o == 0U) {
            continue;
        }
        auto it = id_of.find(o);
        if (it == id_of.end()) {
            if (static_cast<std::int64_t>(out.old_of_id.size()) >= REGION_MAX_ID) {
                fatal("regions: more than 65535 regions");
            }
            out.old_of_id.push_back(o);
            it = id_of.emplace(o, static_cast<std::uint16_t>(out.old_of_id.size())).first;
        }
        out.region[i] = it->second;
    }
    return out;
}

// --- RG11 ---------------------------------------------------------------------------------------------

std::uint32_t region_sector(std::int64_t dx, std::int64_t dy, std::int64_t r_c) {
    constexpr std::int64_t LIMIT = std::int64_t{1} << 24;
    if (dx <= -LIMIT || dx >= LIMIT || dy <= -LIMIT || dy >= LIMIT || r_c < 0 || r_c >= LIMIT) {
        fatal("region_sector: offset or radius outside its range");
    }
    if (dx * dx + dy * dy < r_c * r_c) {
        return 0U;  // central
    }
    const std::int64_t ax = dx < 0 ? -dx : dx;
    const std::int64_t ay = dy < 0 ? -dy : dy;
    if (29 * ay <= 12 * ax) {
        return dx > 0 ? 3U : 7U;  // east, west
    }
    if (29 * ax <= 12 * ay) {
        return dy > 0 ? 5U : 1U;  // south, north
    }
    if (dy > 0) {
        return dx > 0 ? 4U : 6U;  // southeast, southwest
    }
    return dx > 0 ? 2U : 8U;  // northeast, northwest
}

std::string roman(std::int64_t k) {
    if (k < 1 || k > 100000) {
        fatal("roman: k outside [1, 100000]");
    }
    static constexpr std::array<std::pair<std::int64_t, const char*>, 13> PARTS = {{
        {1000, "M"},
        {900, "CM"},
        {500, "D"},
        {400, "CD"},
        {100, "C"},
        {90, "XC"},
        {50, "L"},
        {40, "XL"},
        {10, "X"},
        {9, "IX"},
        {5, "V"},
        {4, "IV"},
        {1, "I"},
    }};
    std::string out;
    for (const auto& [value, text] : PARTS) {
        while (k >= value) {
            out += text;
            k -= value;
        }
    }
    return out;
}

std::vector<std::string> region_names(std::span<const RegionNameInput> regions,
                                      std::span<const std::uint64_t> landmass_cells, std::uint64_t land_cells,
                                      std::uint64_t sum_x, std::uint64_t sum_y, std::int64_t n,
                                      const BiomeDefinitions& definitions, const RegionTuning& tuning) {
    check_region_tuning(tuning);
    if (land_cells == 0U) {
        fatal("region_names: no land cells");
    }
    if (n < 1 || n > 4096 || sum_x >= (std::uint64_t{1} << 62) || sum_y >= (std::uint64_t{1} << 62)) {
        fatal("region_names: map side or sums outside their range");
    }
    std::array<const BiomeDef*, 256> by_code{};
    for (const BiomeDef& b : definitions.biomes) {
        if (b.code >= 0 && b.code <= 255) {
            by_code[static_cast<std::size_t>(b.code)] = &b;
        }
    }
    // 1. Land centroid and central radius.
    const auto l = static_cast<std::int64_t>(land_cells);
    const std::int64_t cx = rha(static_cast<std::int64_t>(sum_x), l);
    const std::int64_t cy = rha(static_cast<std::int64_t>(sum_y), l);
    const std::int64_t r_c = mul_int(tuning.name_central_frac, n).round_int();
    // The largest landmass: most cells, ties to the smaller label.
    std::uint32_t largest = 0;
    for (std::size_t k = 0; k < landmass_cells.size(); ++k) {
        if (largest == 0U || landmass_cells[k] > landmass_cells[largest - 1U]) {
            largest = static_cast<std::uint32_t>(k + 1U);
        }
    }
    std::vector<std::uint32_t> regions_on(landmass_cells.size() + 1U, 0U);
    for (const RegionNameInput& r : regions) {
        if (r.landmass == 0U || r.landmass > landmass_cells.size()) {
            fatal("region_names: a landmass label out of range");
        }
        ++regions_on[r.landmass];
    }
    const std::array<std::string_view, 9> sectors = region_sector_words(tuning.names);
    std::map<std::string, std::int64_t> seen;
    std::vector<std::string> out;
    out.reserve(regions.size());
    for (const RegionNameInput& r : regions) {
        const BiomeDef* b = by_code[r.biome];
        if (b == nullptr) {
            fatal("region_names: a region's biome has no definition");
        }
        // 2. Sector; 3. noun.
        const std::uint32_t sector = region_sector(r.seed_x - cx, r.seed_y - cy, r_c);
        const bool isle = regions_on[r.landmass] == 1U && r.landmass != largest;
        std::string base = std::string(sectors[sector]) + " " + (isle ? tuning.names.isle : b->region_noun);
        // 4. Numbered in id order.
        const std::int64_t k = ++seen[base];
        out.push_back(k == 1 ? base : base + " " + roman(k));
    }
    return out;
}

// --- RG12 ----------------------------------------------------------------------------------------------

std::vector<db::RegionBorderRow> region_borders(Grid grid, std::span<const std::uint16_t> region,
                                                std::span<const std::int64_t> surface,
                                                std::span<const std::uint8_t> water,
                                                std::span<const std::uint8_t> river,
                                                std::span<const std::uint16_t> walk_area,
                                                const RegionTuning& tuning) {
    check_region_tuning(tuning);
    geo::check_grid_array(grid, region.size(), "region_borders: region");
    geo::check_grid_array(grid, surface.size(), "region_borders: surface");
    geo::check_grid_array(grid, water.size(), "region_borders: water");
    geo::check_grid_array(grid, river.size(), "region_borders: river");
    geo::check_grid_array(grid, walk_area.size(), "region_borders: walk_area");
    std::map<std::pair<std::uint16_t, std::uint16_t>, db::RegionBorderRow> rows;
    const auto pair = [&](CellIndex p, CellIndex q) {
        const std::uint16_t rp = region[p];
        const std::uint16_t rq = region[q];
        if (!is_land(water[p]) || !is_land(water[q]) || rp == 0U || rq == 0U || rp == rq) {
            return;
        }
        const std::pair<std::uint16_t, std::uint16_t> key{std::min(rp, rq), std::max(rp, rq)};
        auto [it, inserted] = rows.try_emplace(key);
        db::RegionBorderRow& row = it->second;
        const std::int64_t high = std::max(surface[p], surface[q]);
        if (inserted || high < row.low_dm) {
            const CellIndex at = surface[q] > surface[p] ? q : p;
            row.a = key.first;
            row.b = key.second;
            row.low_dm = high;
            row.low_x = grid.x_of(at);
            row.low_y = grid.y_of(at);
        }
        row.length_m += geo::CELL_M;
        if (walk_area[p] != 0U && walk_area[p] == walk_area[q]) {
            ++row.walk_pairs;
        }
        const bool bp = water[p] == WATER_RIVER && std::int64_t{river[p]} >= tuning.river_border_class;
        const bool bq = water[q] == WATER_RIVER && std::int64_t{river[q]} >= tuning.river_border_class;
        if (bp || bq) {
            ++row.river_pairs;
        }
        if (water[p] == WATER_RIVER) row.river_class_max = std::max<std::int64_t>(row.river_class_max, river[p]);
        if (water[q] == WATER_RIVER) row.river_class_max = std::max<std::int64_t>(row.river_class_max, river[q]);
    };
    for (std::int32_t y = 0; y < grid.height; ++y) {
        for (std::int32_t x = 0; x < grid.width; ++x) {
            const CellIndex i = grid.index(x, y);
            if (x + 1 < grid.width) pair(i, i + 1U);                                   // E
            if (y + 1 < grid.height) pair(i, i + static_cast<CellIndex>(grid.width));  // S
        }
    }
    std::vector<db::RegionBorderRow> out;
    out.reserve(rows.size());
    for (auto& [key, row] : rows) {
        out.push_back(row);
    }
    return out;
}

namespace {

// The cells of a pass path, or std::nullopt if it is not a chain of cells
// (P1-06 2 check 5): non-empty, a whole number of u32 little-endian cells,
// each below W x H, consecutive cells equal or 8-neighbours.
std::optional<std::vector<CellIndex>> pass_cells(Grid grid, const db::PassRow& row) {
    const std::vector<std::uint8_t>& b = row.path;
    if (b.empty() || b.size() % 4U != 0U) {
        return std::nullopt;
    }
    std::vector<CellIndex> cells;
    cells.reserve(b.size() / 4U);
    for (std::size_t i = 0; i < b.size(); i += 4U) {
        std::uint32_t c = 0;
        for (std::size_t k = 0; k < 4U; ++k) {
            c |= static_cast<std::uint32_t>(b[i + k]) << (8U * k);
        }
        if (c >= grid.cells()) {
            return std::nullopt;
        }
        if (!cells.empty()) {
            const std::int32_t dx = grid.x_of(c) - grid.x_of(cells.back());
            const std::int32_t dy = grid.y_of(c) - grid.y_of(cells.back());
            if (dx < -1 || dx > 1 || dy < -1 || dy > 1) {
                return std::nullopt;
            }
        }
        cells.push_back(c);
    }
    return cells;
}

void check_pass_paths(Grid grid, std::span<const db::PassRow> passes) {
    for (const db::PassRow& p : passes) {
        if (!pass_cells(grid, p)) {
            fatal_text(std::string(PRECONDITION) + "pass " + db::format_i64(p.id) + ": path is not a chain of cells");
        }
    }
}

}  // namespace

void region_pass_crossings(Grid grid, std::span<const std::uint16_t> region, std::span<const std::uint8_t> water,
                           std::span<const db::PassRow> passes, std::vector<db::RegionBorderRow>& borders) {
    geo::check_grid_array(grid, region.size(), "region_pass_crossings: region");
    geo::check_grid_array(grid, water.size(), "region_pass_crossings: water");
    check_pass_paths(grid, passes);
    std::map<std::pair<std::int64_t, std::int64_t>, std::size_t> row_of;
    for (std::size_t k = 0; k < borders.size(); ++k) {
        row_of.emplace(std::pair{borders[k].a, borders[k].b}, k);
    }
    for (const db::PassRow& p : passes) {
        const std::vector<CellIndex> path = *pass_cells(grid, p);
        // The step sequence: corners inserted between diagonal steps, equal
        // consecutive cells skipped.
        std::vector<CellIndex> seq;
        seq.reserve(2U * path.size());
        const auto append = [&seq](CellIndex c) {
            if (seq.empty() || seq.back() != c) {
                seq.push_back(c);
            }
        };
        for (std::size_t j = 0; j < path.size(); ++j) {
            if (j > 0U) {
                const CellIndex a = path[j - 1U];
                const CellIndex b = path[j];
                if (grid.x_of(a) != grid.x_of(b) && grid.y_of(a) != grid.y_of(b)) {
                    const CellIndex k1 = grid.index(grid.x_of(b), grid.y_of(a));
                    const CellIndex k2 = grid.index(grid.x_of(a), grid.y_of(b));
                    if (is_land(water[k1])) {
                        append(k1);
                    } else if (is_land(water[k2])) {
                        append(k2);
                    }
                }
            }
            append(path[j]);
        }
        std::vector<std::size_t> crossed;
        for (std::size_t j = 1; j < seq.size(); ++j) {
            const CellIndex u = seq[j - 1U];
            const CellIndex v = seq[j];
            const std::int32_t dx = grid.x_of(v) - grid.x_of(u);
            const std::int32_t dy = grid.y_of(v) - grid.y_of(u);
            const bool four = (dx == 0) != (dy == 0);
            if (!four || !is_land(water[u]) || !is_land(water[v]) || region[u] == 0U || region[v] == 0U ||
                region[u] == region[v]) {
                continue;
            }
            const std::pair<std::int64_t, std::int64_t> key{std::min(region[u], region[v]),
                                                            std::max(region[u], region[v])};
            const auto it = row_of.find(key);
            if (it == row_of.end()) {
                fatal("region_pass_crossings: a crossing with no border row");
            }
            if (std::find(crossed.begin(), crossed.end(), it->second) == crossed.end()) {
                crossed.push_back(it->second);
            }
        }
        for (const std::size_t k : crossed) {
            db::RegionBorderRow& row = borders[k];
            if (row.passes == 0 || p.id < row.pass_id) {
                row.pass_id = p.id;
            }
            ++row.passes;
        }
    }
}

// --- The whole stage ---------------------------------------------------------------------------------

namespace {

// Section 2 check 4: every biome's region_noun is one word.
void check_nouns(const BiomeDefinitions& definitions) {
    for (const BiomeDef& b : definitions.biomes) {  // ascending code
        if (!is_region_word(b.region_noun)) {
            fatal_text("regions: biome '" + b.key + "' has a region_noun that is not one word");
        }
    }
}

// Section 2 check 5 up to the passes (G7 follows in RG2).
void check_inputs(const RegionInputs& in, const BiomeDefinitions& definitions, std::span<const db::PassRow> passes) {
    const Grid grid = in.grid;
    try {
        geo::check_grid(grid);
    } catch (const FatalError& e) {
        fatal_text(std::string(PRECONDITION) + e.what());
    }
    const std::size_t n = grid.cells();
    const std::array<std::pair<const char*, std::size_t>, 13> sizes = {{
        {"elevation", in.elevation.size()},
        {"water", in.water.size()},
        {"water_depth", in.water_depth.size()},
        {"flow_dir", in.flow_dir.size()},
        {"river", in.river.size()},
        {"biome", in.biome.size()},
        {"fertility", in.fertility.size()},
        {"ore", in.ore.size()},
        {"timber", in.timber.size()},
        {"fresh_water", in.fresh_water.size()},
        {"forage", in.forage.size()},
        {"habitability", in.habitability.size()},
        {"walk_area", in.walk_area.size()},
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
    std::array<std::uint8_t, 256> defined{};
    for (const BiomeDef& b : definitions.biomes) {
        if (b.code >= 0 && b.code <= 255) {
            defined[static_cast<std::size_t>(b.code)] = 1U;
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (defined[in.biome[i]] == 0U) {
            fatal_text(std::string(PRECONDITION) + "biome value " + db::format_u64(in.biome[i]) + " at " +
                       cell_at_text(grid, i) + " has no definition");
        }
    }
    check_pass_paths(grid, passes);
}

// Per-region accumulators of RG10 and RG13.
struct RegionAcc {
    std::int64_t x_min = 0;
    std::int64_t y_min = 0;
    std::int64_t x_max = 0;
    std::int64_t y_max = 0;
    bool any = false;
    std::int64_t cells = 0;
    std::int64_t lake = 0;
    std::int64_t river = 0;
    std::int64_t river_class_max = 0;
    std::int64_t coast = 0;
    std::int64_t walk = 0;
    std::int64_t dry = 0;
    std::int64_t elev_sum = 0;
    std::int64_t elev_max = 0;
    std::array<std::int64_t, 6> score_sum{};  // fertility, forage, timber, ore, fresh_water, habitability
    std::int64_t ore_rich = 0;
    std::int64_t ore_reach = 0;
    std::vector<std::pair<std::uint16_t, std::int64_t>> walk_counts;  // (walk_area, cells), first-seen order
};

// The most common value of (value, count) pairs, ties to the smaller value;
// 0 if none.
std::int64_t most_common(const std::vector<std::pair<std::uint16_t, std::int64_t>>& counts) {
    std::int64_t best = 0;
    std::int64_t best_count = 0;
    for (const auto& [v, c] : counts) {
        if (c > best_count || (c == best_count && v < best)) {
            best = v;
            best_count = c;
        }
    }
    return best;
}

}  // namespace

RegionResult regions_stage(const RegionInputs& in, const BiomeDefinitions& definitions,
                           std::span<const db::PassRow> passes, const RegionTuning& tuning, std::uint64_t seed,
                           std::uint32_t threads) {
    // RG1: section 2 items 2 to 5.
    check_threads(threads);
    check_region_tuning(tuning);
    check_nouns(definitions);
    check_inputs(in, definitions, passes);
    const Grid grid = in.grid;
    const std::uint32_t n = grid.cells();
    RegionResult out;

    // RG2: G7 with k = 1 (D5).
    const std::vector<std::int64_t> surface = geo::water_surface(grid, in.elevation, in.water_depth);
    out.hand = geo::height_above_drainage(grid, surface, in.water, in.river, in.flow_dir, 1,
                                          std::string(PRECONDITION) + "G7: ");

    // RG3 (D1, G6 row blocks; location-keyed noise on land cells only).
    out.weight.assign(n, Fx{});
    const std::uint64_t k_cost = stream_key(seed, streams::geo_regions_cost);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            const Fx ym = cell_centre_m(y);
            for (std::int32_t x = 0; x < grid.width; ++x) {
                const CellIndex i = grid.index(x, y);
                if (!is_land(in.water[i])) {
                    continue;
                }
                const std::int64_t hand = out.hand[i];
                if (hand < 0 || hand > REGION_HAND_MAX_DM) {
                    fatal("regions: a height above drainage outside [0, 131070]");
                }
                const Fx nz = geo::fbm_m(k_cost, cell_centre_m(x), ym, tuning.cost_noise);
                out.weight[i] = step_weight_of(nz, hand, tuning);
            }
        }
    });

    // RG4, RG5 (D1 per tile, D7, D4), RG6 (D5).
    const std::vector<RegionSite> candidates =
        region_candidates(grid, in.water, in.habitability, surface, seed, tuning);
    std::vector<CellIndex> seeds = region_seeds(candidates, grid, in.water, in.river, tuning);
    const RegionLandmasses landmasses = region_landmasses(grid, in.water);
    out.seeds = region_landmass_seeds(grid, in.water, in.habitability, surface, landmasses, std::move(seeds), seed);

    // RG7 (D6).
    RegionFlood flood = region_flood(grid, out.weight, in.water, in.river, out.seeds, tuning);
    out.flood_owner = std::move(flood.owner);
    flood.cost.clear();
    flood.cost.shrink_to_fit();

    // RG8 (D4), RG9 (D7).
    const std::vector<std::uint32_t> merged =
        region_merge(grid, out.flood_owner, in.water, in.river, static_cast<std::uint32_t>(out.seeds.size()),
                     region_min_cells(tuning), tuning);
    RegionNumbering numbering = region_renumber(merged);
    out.region = std::move(numbering.region);
    const std::size_t r_count = numbering.old_of_id.size();

    // RG10 and RG13 (D8: exact integer sums, counts, minima, maxima).
    std::vector<std::uint8_t> walk_source(n, 0U);
    for (std::uint32_t i = 0; i < n; ++i) {
        walk_source[i] = in.walk_area[i] != 0U ? 1U : 0U;
    }
    const std::vector<std::int64_t> dw = chamfer_metres(grid, walk_source);  // P1-02 H7 (D4)
    std::vector<RegionAcc> acc(r_count + 1U);
    std::vector<std::uint32_t> biome_hist((r_count + 1U) * 256U, 0U);
    std::vector<std::uint32_t> hab_hist((r_count + 1U) * 256U, 0U);
    std::uint64_t land_cells = 0;
    std::uint64_t sum_x = 0;
    std::uint64_t sum_y = 0;
    for (std::int32_t y = 0; y < grid.height; ++y) {
        for (std::int32_t x = 0; x < grid.width; ++x) {
            const CellIndex i = grid.index(x, y);
            const std::uint8_t w = in.water[i];
            if (!is_land(w)) {
                continue;
            }
            ++land_cells;
            sum_x += static_cast<std::uint64_t>(x);
            sum_y += static_cast<std::uint64_t>(y);
            const std::size_t r = out.region[i];
            RegionAcc& a = acc[r];
            if (!a.any) {
                a.x_min = a.x_max = x;
                a.y_min = a.y_max = y;
                a.any = true;
            }
            a.x_min = std::min<std::int64_t>(a.x_min, x);
            a.x_max = std::max<std::int64_t>(a.x_max, x);
            a.y_min = std::min<std::int64_t>(a.y_min, y);
            a.y_max = std::max<std::int64_t>(a.y_max, y);
            ++a.cells;
            ++biome_hist[r * 256U + in.biome[i]];
            if (w == WATER_LAKE) ++a.lake;
            if (w == WATER_RIVER) {
                ++a.river;
                a.river_class_max = std::max<std::int64_t>(a.river_class_max, in.river[i]);
            }
            bool coast = false;
            for (const geo::Direction d : geo::NEIGHBOURS_4) {
                CellIndex m = 0;
                if (geo::neighbour(grid, i, d, m) && in.water[m] == WATER_OCEAN) {
                    coast = true;
                }
            }
            if (coast) ++a.coast;
            const std::uint16_t wa = in.walk_area[i];
            if (wa != 0U) {
                ++a.walk;
                auto it = std::find_if(a.walk_counts.begin(), a.walk_counts.end(),
                                       [wa](const auto& e) { return e.first == wa; });
                if (it == a.walk_counts.end()) {
                    a.walk_counts.emplace_back(wa, 1);
                } else {
                    ++it->second;
                }
            }
            if (!is_dry(w)) {
                continue;
            }
            const std::int64_t e = in.elevation[i];
            a.elev_max = a.dry == 0 ? e : std::max(a.elev_max, e);
            ++a.dry;
            a.elev_sum += e;
            a.score_sum[0] += in.fertility[i];
            a.score_sum[1] += in.forage[i];
            a.score_sum[2] += in.timber[i];
            a.score_sum[3] += in.ore[i];
            a.score_sum[4] += in.fresh_water[i];
            a.score_sum[5] += in.habitability[i];
            ++hab_hist[r * 256U + in.habitability[i]];
            if (std::int64_t{in.ore[i]} >= tuning.ore_rich_min) {
                ++a.ore_rich;
                if (dw[i] <= tuning.ore_reach_m) {
                    ++a.ore_reach;
                }
            }
        }
    }
    std::array<bool, 256> water_biome{};
    for (const BiomeDef& b : definitions.biomes) {
        if (b.code >= 0 && b.code <= 255) {
            water_biome[static_cast<std::size_t>(b.code)] = b.water;
        }
    }
    std::vector<std::uint64_t> landmass_cells(landmasses.count, 0U);
    for (std::uint32_t i = 0; i < n; ++i) {
        if (landmasses.label[i] != 0U) {
            ++landmass_cells[landmasses.label[i] - 1U];
        }
    }
    std::vector<RegionNameInput> name_inputs;
    out.regions.resize(r_count);
    for (std::size_t id = 1; id <= r_count; ++id) {
        const RegionAcc& a = acc[id];
        const CellIndex s = out.seeds[numbering.old_of_id[id - 1U] - 1U];
        if (a.dry == 0 || out.region[s] != id) {
            fatal("regions: a region without dry land or without its seed");
        }
        db::RegionRow& row = out.regions[id - 1U];
        row.id = static_cast<std::int64_t>(id);
        std::int64_t dominant = -1;
        std::uint32_t dominant_cells = 0;
        for (std::size_t code = 0; code < 256U; ++code) {
            const std::uint32_t c = biome_hist[id * 256U + code];
            if (c != 0U && !water_biome[code] && c > dominant_cells) {
                dominant = static_cast<std::int64_t>(code);
                dominant_cells = c;
            }
        }
        if (dominant < 0) {
            fatal("regions: a region without a land biome");
        }
        row.biome = dominant;
        row.x_min = a.x_min;
        row.y_min = a.y_min;
        row.x_max = a.x_max;
        row.y_max = a.y_max;
        row.seed_x = grid.x_of(s);
        row.seed_y = grid.y_of(s);
        row.landmass = landmasses.label[s];
        row.cells = a.cells;
        row.lake_cells = a.lake;
        row.river_cells = a.river;
        row.river_class_max = a.river_class_max;
        row.coast_cells = a.coast;
        row.walk_cells = a.walk;
        row.walk_area = most_common(a.walk_counts);
        row.elev_mean_dm = rha(a.elev_sum, a.dry);
        row.elev_max_dm = a.elev_max;
        row.fertility_mean = rha(a.score_sum[0], a.dry);
        row.forage_mean = rha(a.score_sum[1], a.dry);
        row.timber_mean = rha(a.score_sum[2], a.dry);
        row.ore_mean = rha(a.score_sum[3], a.dry);
        row.fresh_water_mean = rha(a.score_sum[4], a.dry);
        row.habitability_mean = rha(a.score_sum[5], a.dry);
        // G1 rank min(n - 1, 90 n div 100) on the dry habitability values.
        const auto dn = static_cast<std::uint64_t>(a.dry);
        const std::uint64_t rank = std::min(dn - 1U, (90U * dn) / 100U);
        std::uint64_t seen = 0;
        for (std::size_t v = 0; v < 256U; ++v) {
            seen += hab_hist[id * 256U + v];
            if (seen > rank) {
                row.habitability_p90 = static_cast<std::int64_t>(v);
                break;
            }
        }
        row.ore_rich_cells = a.ore_rich;
        row.ore_reach_cells = a.ore_reach;
        name_inputs.push_back(RegionNameInput{row.seed_x, row.seed_y, static_cast<std::uint8_t>(dominant),
                                              static_cast<std::uint32_t>(row.landmass)});
    }

    // RG11 (sequential, id order).
    const std::vector<std::string> names = region_names(name_inputs, landmass_cells, land_cells, sum_x, sum_y,
                                                        grid.width, definitions, tuning);
    for (std::size_t k = 0; k < r_count; ++k) {
        out.regions[k].name = names[k];
    }

    // RG12 (D8 per pair, sorted; passes sequential in id order).
    out.borders = region_borders(grid, out.region, surface, in.water, in.river, in.walk_area, tuning);
    region_pass_crossings(grid, out.region, in.water, passes, out.borders);

    // RG13: biome counts, sorted by (region, biome).
    for (std::size_t id = 1; id <= r_count; ++id) {
        for (std::size_t code = 0; code < 256U; ++code) {
            const std::uint32_t c = biome_hist[id * 256U + code];
            if (c != 0U) {
                out.biomes.push_back(db::RegionBiomeRow{static_cast<std::int64_t>(id), static_cast<std::int64_t>(code),
                                                        static_cast<std::int64_t>(c)});
            }
        }
    }
    return out;
}

namespace {

// The thirteen stored inputs with their catalogue types, in the order of the
// inputs table of P1-06 section 2.
struct StoredInput {
    std::string_view name;
    db::ElementType type;
};
constexpr std::array<StoredInput, 13> STORED_INPUTS = {{
    {ELEVATION_LAYER, db::ElementType::i16},
    {WATER_LAYER, db::ElementType::u8},
    {WATER_DEPTH_LAYER, db::ElementType::u16},
    {FLOW_DIR_LAYER, db::ElementType::u8},
    {RIVER_LAYER, db::ElementType::u8},
    {BIOME_LAYER, db::ElementType::u8},
    {FERTILITY_LAYER, db::ElementType::u8},
    {ORE_LAYER, db::ElementType::u8},
    {TIMBER_LAYER, db::ElementType::u8},
    {FRESH_WATER_LAYER, db::ElementType::u8},
    {FORAGE_LAYER, db::ElementType::u8},
    {HABITABILITY_LAYER, db::ElementType::u8},
    {WALK_AREA_LAYER, db::ElementType::u16},
}};

// Section 2 item 1, each check over all thirteen inputs in turn: present,
// of its catalogue type, of the size N x N.
void check_stored_inputs(const StageContext& context, std::int64_t n) {
    for (const StoredInput& s : STORED_INPUTS) {
        if (context.find_layer(s.name) == nullptr) {
            fatal_text(std::string(PRECONDITION) + "missing layer '" + std::string(s.name) + "'");
        }
    }
    for (const StoredInput& s : STORED_INPUTS) {
        if (context.find_layer(s.name)->type() != s.type) {
            fatal_text(std::string(PRECONDITION) + "layer '" + std::string(s.name) +
                       "' does not have its catalogue type");
        }
    }
    for (const StoredInput& s : STORED_INPUTS) {
        const db::LayerGrid* grid = context.find_layer(s.name);
        if (grid->width != static_cast<std::uint32_t>(n) || grid->height != static_cast<std::uint32_t>(n)) {
            fatal_text(std::string(PRECONDITION) + std::string(s.name) + " length is not width * height");
        }
    }
}

template <class T>
std::span<const T> input_layer(const StageContext& context, std::string_view name) {
    return *context.find_layer(name)->get_if<T>();
}

}  // namespace

void run_regions(StageContext& context) {
    const BiomeDefinitions definitions = biome_definitions_from(context.content());
    const RegionTuning tuning = region_tuning_from(context.content());
    const std::int64_t n = context.map_size();
    if (n < ELEVATION_MIN_N || n > ELEVATION_MAX_N) {
        fatal("regions: map size outside [16, 2048]");
    }
    check_stored_inputs(context, n);
    RegionInputs in;
    in.grid = geo::square_grid(static_cast<std::int32_t>(n));
    in.elevation = input_layer<std::int16_t>(context, ELEVATION_LAYER);
    in.water = input_layer<std::uint8_t>(context, WATER_LAYER);
    in.water_depth = input_layer<std::uint16_t>(context, WATER_DEPTH_LAYER);
    in.flow_dir = input_layer<std::uint8_t>(context, FLOW_DIR_LAYER);
    in.river = input_layer<std::uint8_t>(context, RIVER_LAYER);
    in.biome = input_layer<std::uint8_t>(context, BIOME_LAYER);
    in.fertility = input_layer<std::uint8_t>(context, FERTILITY_LAYER);
    in.ore = input_layer<std::uint8_t>(context, ORE_LAYER);
    in.timber = input_layer<std::uint8_t>(context, TIMBER_LAYER);
    in.fresh_water = input_layer<std::uint8_t>(context, FRESH_WATER_LAYER);
    in.forage = input_layer<std::uint8_t>(context, FORAGE_LAYER);
    in.habitability = input_layer<std::uint8_t>(context, HABITABILITY_LAYER);
    in.walk_area = input_layer<std::uint16_t>(context, WALK_AREA_LAYER);
    // Table pass, in id order (the pipeline keeps the rows as written).
    RegionResult result = regions_stage(in, definitions, context.passes(), tuning, context.seed(), context.threads());
    // RG14: one transaction (the pipeline's): the layer, then the three tables.
    db::LayerGrid grid;
    grid.width = static_cast<std::uint32_t>(n);
    grid.height = static_cast<std::uint32_t>(n);
    grid.values = std::move(result.region);
    context.write_layer(REGION_LAYER, std::move(grid));
    for (const db::RegionRow& row : result.regions) {
        context.write_region(row);
    }
    for (const db::RegionBorderRow& row : result.borders) {
        context.write_region_border(row);
    }
    for (const db::RegionBiomeRow& row : result.biomes) {
        context.write_region_biome(row);
    }
}

}  // namespace worldgen::stages
