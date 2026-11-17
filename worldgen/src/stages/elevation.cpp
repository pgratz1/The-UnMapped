// Stage P1-01 elevation (docs/specs/p1-01-elevation.md, design v16). Steps
// E1 to E11, each named with its determinism rule of P1-01 section 7.

#include "worldgen/stages/elevation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
#include <tuple>
#include <utility>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/math.hpp"
#include "worldgen/core/parallel.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/geo/blocks.hpp"
#include "worldgen/geo/components.hpp"
#include "worldgen/geo/flow.hpp"
#include "worldgen/geo/noise_schema.hpp"
#include "worldgen/geo/select.hpp"

namespace worldgen::stages {

namespace {

using geo::CellIndex;
using geo::Direction;
using geo::Grid;

constexpr std::int64_t TWO_POW_32 = std::int64_t{1} << 32U;
constexpr std::int64_t TWO_POW_31 = std::int64_t{1} << 31U;
constexpr std::int64_t FINE_UNIT = std::int64_t{1} << ELEVATION_FINE_SHIFT;  // 2^16
constexpr std::int64_t MAX_AREA_M2 = 1000000000;
constexpr std::int64_t MAX_LAND_CELLS = std::int64_t{1} << 24U;

void check_n(std::int32_t n, const char* what) {
    if (n < ELEVATION_MIN_N || n > ELEVATION_MAX_N) {
        fatal(what);
    }
}

void check_threads(std::uint32_t threads) {
    if (threads < 1U || threads > MAX_THREADS) {
        fatal("elevation: thread count outside [1, 256]");
    }
}

void check_size(Grid grid, std::size_t size, const char* what) {
    geo::check_grid_array(grid, size, what);
}

std::int32_t to_i32(std::int64_t v, std::int64_t lo, std::int64_t hi, const char* what) {
    if (v < lo || v > hi) {
        fatal(what);
    }
    return static_cast<std::int32_t>(v);
}

// The cells of one G6 block of a grid: indices [first, last).
struct BlockCells {
    CellIndex first;
    CellIndex last;
};

BlockCells block_cells(Grid grid, const geo::RowBlock& block) {
    return BlockCells{grid.index(0, block.y_begin),
                      grid.index(0, block.y_end - 1) + static_cast<CellIndex>(grid.width)};
}

// The keys of P1-01 section 2.
struct Keys {
    std::uint64_t continent_warp_x;
    std::uint64_t continent_warp_y;
    std::uint64_t continent;
    std::uint64_t belt;
    std::uint64_t coast;
    std::uint64_t segment;
    std::uint64_t relief_warp_x;
    std::uint64_t relief_warp_y;
    std::uint64_t ridge;
    std::uint64_t hills;
    std::uint64_t roughness;
    std::uint64_t basin;
    std::uint64_t detail;
    std::uint64_t fill;
};

Keys stage_keys(std::uint64_t seed) {
    return Keys{
        stream_key(seed, streams::geo_elevation_continent_warp_x),
        stream_key(seed, streams::geo_elevation_continent_warp_y),
        stream_key(seed, streams::geo_elevation_continent),
        stream_key(seed, streams::geo_elevation_belt),
        stream_key(seed, streams::geo_elevation_coast),
        stream_key(seed, streams::geo_elevation_segment),
        stream_key(seed, streams::geo_elevation_relief_warp_x),
        stream_key(seed, streams::geo_elevation_relief_warp_y),
        stream_key(seed, streams::geo_elevation_ridge),
        stream_key(seed, streams::geo_elevation_hills),
        stream_key(seed, streams::geo_elevation_roughness),
        stream_key(seed, streams::geo_elevation_basin),
        stream_key(seed, streams::geo_elevation_detail),
        stream_key(seed, streams::geo_elevation_fill),
    };
}

std::int64_t ceil_cells(std::int64_t area_m2, const char* what) {
    if (area_m2 < 0 || area_m2 > MAX_AREA_M2) {
        fatal(what);
    }
    return (area_m2 + 255) / 256;
}

}  // namespace

// --- Tuning (4.2) ---------------------------------------------------------------------

ElevationTuning elevation_tuning_from(const data::Record& r) {
    ElevationTuning t;
    t.land_fraction = r.get_fx("land_fraction");
    t.edge_margin_frac = r.get_fx("edge_margin_frac");
    t.continent_warp = geo::fbm_map_from(r.get("continent_warp"));
    t.continent_warp_amp_frac = r.get_fx("continent_warp_amp_frac");
    t.continent_noise = geo::fbm_map_from(r.get("continent_noise"));
    t.continent_noise_amp = r.get_fx("continent_noise_amp");
    t.continent_center_jitter = r.get_fx("continent_center_jitter");
    t.edge_falloff_frac = r.get_fx("edge_falloff_frac");
    t.edge_falloff_depth = r.get_fx("edge_falloff_depth");
    t.edge_noise_amp = r.get_fx("edge_noise_amp");
    t.edge_radial_start = r.get_fx("edge_radial_start");
    t.edge_radial_end = r.get_fx("edge_radial_end");
    t.coast_noise = geo::fbm_m_from(r.get("coast_noise"));
    t.coast_noise_amp = r.get_fx("coast_noise_amp");
    t.island_min_area_m2 = r.get_int("island_min_area_m2");
    t.belt_noise = geo::fbm_map_from(r.get("belt_noise"));
    t.belt_halfwidth_m = r.get_fx("belt_halfwidth_m");
    t.belt_halfwidth_frac = r.get_fx("belt_halfwidth_frac");
    t.segment_noise = geo::fbm_map_from(r.get("segment_noise"));
    t.segment_contrast = r.get_fx("segment_contrast");
    t.segment_bias = r.get_fx("segment_bias");
    t.segment_on_share = r.get_fx("segment_on_share");
    t.segment_floor_mask = r.get_fx("segment_floor_mask");
    t.belt_offset_steps =
        to_i32(r.get_int("belt_offset_steps"), 0, 8, "elevation: belt_offset_steps outside [0, 8]");
    t.floor_inland_gm = r.get_fx("floor_inland_gm");
    t.mountain_coast_t = r.get_fx("mountain_coast_t");
    t.relief_warp = geo::fbm_m_from(r.get("relief_warp"));
    t.relief_warp_amp_m = r.get_fx("relief_warp_amp_m");
    t.ridge_noise = geo::fbm_m_from(r.get("ridge_noise"));
    t.ridge_weight = r.get_fx("ridge_weight");
    t.foothill_fraction = r.get_fx("foothill_fraction");
    t.mountain_height_m = r.get_fx("mountain_height_m");
    t.mountain_peak_rank = r.get_fx("mountain_peak_rank");
    t.mountain_cover_height_m = r.get_fx("mountain_cover_height_m");
    t.mountain_cover_rank = r.get_fx("mountain_cover_rank");
    t.lowland_height_m = r.get_fx("lowland_height_m");
    t.basin_noise = geo::fbm_m_from(r.get("basin_noise"));
    t.basin_amp_m = r.get_fx("basin_amp_m");
    t.hill_noise = geo::fbm_m_from(r.get("hill_noise"));
    t.roughness_noise = geo::fbm_m_from(r.get("roughness_noise"));
    t.roughness_min = r.get_fx("roughness_min");
    t.hill_height_m = r.get_fx("hill_height_m");
    t.hill_coast_min = r.get_fx("hill_coast_min");
    t.detail_noise = geo::fbm_m_from(r.get("detail_noise"));
    t.detail_amp_m = r.get_fx("detail_amp_m");
    t.ocean_depth_m = r.get_fx("ocean_depth_m");
    t.ocean_shelf_m = r.get_fx("ocean_shelf_m");
    t.erosion_iterations =
        to_i32(r.get_int("erosion_iterations"), 0, 200, "elevation: erosion_iterations outside [0, 200]");
    t.erosion_route_interval =
        to_i32(r.get_int("erosion_route_interval"), 1, 64, "elevation: erosion_route_interval outside [1, 64]");
    t.erosion_k = r.get_fx("erosion_k");
    t.erosion_diffusion = r.get_fx("erosion_diffusion");
    t.talus_slope = r.get_fx("talus_slope");
    t.talus_rate = r.get_fx("talus_rate");
    t.land_floor_m = r.get_fx("land_floor_m");
    t.lake_min_area_m2 = r.get_int("lake_min_area_m2");
    t.lake_max_area_m2 = r.get_int("lake_max_area_m2");
    t.lake_area_ref_n =
        to_i32(r.get_int("lake_area_ref_n"), 16, 2048, "elevation: lake_area_ref_n outside [16, 2048]");
    t.lake_min_depth_m = r.get_fx("lake_min_depth_m");
    t.lake_max_share = r.get_fx("lake_max_share");
    t.fill_slope_dm = r.get_fx("fill_slope_dm");
    t.fill_noise = geo::fbm_m_from(r.get("fill_noise"));
    t.fill_cost_amp = r.get_fx("fill_cost_amp");
    t.fill_detail_m = r.get_fx("fill_detail_m");
    t.pass_count_max = to_i32(r.get_int("pass_count_max"), 0, 3, "elevation: pass_count_max outside [0, 3]");
    t.pass_core_mask = r.get_fx("pass_core_mask");
    t.pass_crest_frac = r.get_fx("pass_crest_frac");
    t.pass_min_length_m = r.get_int("pass_min_length_m");
    t.pass_spacing_m = r.get_int("pass_spacing_m");
    t.pass_size_ref_n = to_i32(r.get_int("pass_size_ref_n"), 16, 2048, "elevation: pass_size_ref_n outside [16, 2048]");
    t.pass_region_mask = r.get_fx("pass_region_mask");
    t.pass_body_m = r.get_fx("pass_body_m");
    t.pass_basin_depth_m = r.get_fx("pass_basin_depth_m");
    t.pass_tarn_min_cells = r.get_int("pass_tarn_min_cells");
    t.pass_divide_reach_m = r.get_fx("pass_divide_reach_m");
    t.pass_saddle_window_m = r.get_fx("pass_saddle_window_m");
    t.pass_saddle_rise_m = r.get_fx("pass_saddle_rise_m");
    t.pass_saddle_rise_reach_m = r.get_fx("pass_saddle_rise_reach_m");
    t.pass_min_spacing_m = r.get_fx("pass_min_spacing_m");
    t.pass_coast_clear_m = r.get_fx("pass_coast_clear_m");
    t.pass_barrier_m = r.get_fx("pass_barrier_m");
    t.pass_side_max_m = r.get_fx("pass_side_max_m");
    t.pass_col_tarn_m = r.get_fx("pass_col_tarn_m");
    t.pass_grade = r.get_fx("pass_grade");
    t.pass_grade_jitter = r.get_fx("pass_grade_jitter");
    t.pass_min_col_m = r.get_fx("pass_min_col_m");
    t.pass_saddle_max_m = r.get_fx("pass_saddle_max_m");
    t.pass_max_cut_m = r.get_fx("pass_max_cut_m");
    t.pass_peak_rise_m = r.get_fx("pass_peak_rise_m");
    t.pass_halfwidth_m = r.get_fx("pass_halfwidth_m");
    t.pass_width_jitter = r.get_fx("pass_width_jitter");
    t.pass_width_var = r.get_fx("pass_width_var");
    t.pass_walk_grade = r.get_fx("pass_walk_grade");
    t.pass_texture_keep = r.get_fx("pass_texture_keep");
    t.pass_drain_step_m = r.get_fx("pass_drain_step_m");
    t.final_talus_iterations =
        to_i32(r.get_int("final_talus_iterations"), 0, 1000, "elevation: final_talus_iterations outside [0, 1000]");
    t.final_talus_slope = r.get_fx("final_talus_slope");
    t.final_talus_rate = r.get_fx("final_talus_rate");
    return t;
}

ElevationTuning elevation_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::WORLDGEN_KIND, data::WORLDGEN_ELEVATION_ID);
    if (d == nullptr) {
        fatal("elevation: the content has no definition worldgen.elevation");
    }
    return elevation_tuning_from(d->record);
}

// --- Derived integers (4.5) -------------------------------------------------------------

std::int32_t elevation_margin(Fx edge_margin_frac, std::int32_t n) {
    if (n < 1 || n > ELEVATION_MAX_N || edge_margin_frac < Fx() || edge_margin_frac > Fx::ONE) {
        fatal("elevation_margin: N outside [1, 2048] or edge_margin_frac outside [0, 1]");
    }
    const std::int64_t m = (edge_margin_frac.raw() * std::int64_t{n}) >> 32U;  // < 2^43
    return static_cast<std::int32_t>(std::max<std::int64_t>(1, m));
}

std::int64_t island_min_cells(std::int64_t island_min_area_m2) {
    return ceil_cells(island_min_area_m2, "island_min_cells: area outside [0, 10^9]");
}

std::int64_t lake_min_cells(std::int64_t lake_min_area_m2) {
    return ceil_cells(lake_min_area_m2, "lake_min_cells: area outside [0, 10^9]");
}

std::int64_t lake_max_cells(std::int64_t lake_max_area_m2, std::int32_t lake_area_ref_n, std::int32_t n) {
    if (lake_max_area_m2 < 0 || lake_max_area_m2 > MAX_AREA_M2 || lake_area_ref_n < 16 || lake_area_ref_n > 2048 ||
        n < 1 || n > ELEVATION_MAX_N) {
        fatal("lake_max_cells: area outside [0, 10^9], reference side outside [16, 2048] or N outside [1, 2048]");
    }
    const std::int64_t nn = std::int64_t{n} * std::int64_t{n};
    const std::int64_t ref = std::int64_t{lake_area_ref_n} * std::int64_t{lake_area_ref_n};
    const std::int64_t a = (lake_max_area_m2 * nn) / ref;  // <= 10^9 x 2^22 < 2^52
    return (a + 255) / 256;
}

std::int64_t lake_min_depth_dm(Fx lake_min_depth_m) {
    if (lake_min_depth_m < Fx() || lake_min_depth_m > Fx::from_int(100)) {
        fatal("lake_min_depth_dm: depth outside [0, 100] m");
    }
    return mul_div(lake_min_depth_m.raw(), 10, TWO_POW_32);
}

std::int64_t fill_fine_slope(Fx fill_slope_dm) {
    if (fill_slope_dm < Fx() || fill_slope_dm > Fx::from_int(10)) {
        fatal("fill_fine_slope: fill_slope_dm outside [0, 10]");
    }
    return mul_div(fill_slope_dm.raw(), FINE_UNIT, TWO_POW_32);
}

Fx belt_halfwidth(Fx belt_halfwidth_m, Fx belt_halfwidth_frac, std::int32_t n) {
    check_n(n, "belt_halfwidth: N outside [16, 2048]");
    return min(belt_halfwidth_m, mul(belt_halfwidth_frac, Fx::from_int(geo::CELL_M * n)));
}

std::int64_t mountain_floor_target(std::int64_t land_cells, Fx segment_on_share) {
    if (land_cells < 0 || land_cells > MAX_LAND_CELLS) {
        fatal("mountain_floor_target: land cell count outside [0, 2^24]");
    }
    return mul(Fx::from_int(land_cells), segment_on_share).round_int();
}

std::int64_t lake_max_share_cells(std::int64_t land_cells, Fx lake_max_share) {
    if (land_cells < 0 || land_cells > MAX_LAND_CELLS) {
        fatal("lake_max_share_cells: land cell count outside [0, 2^24]");
    }
    return std::max<std::int64_t>(1, mul(Fx::from_int(land_cells), lake_max_share).round_int());
}

// --- E1 ------------------------------------------------------------------------------------

Fx elevation_map_fraction(std::int32_t k, std::int32_t n) {
    return Fx::from_ratio(2 * std::int64_t{k} + 1, 2 * std::int64_t{n});
}

Fx elevation_metres(std::int32_t k) {
    return Fx::from_int(geo::CELL_M * k + geo::CELL_M / 2);
}

Fx continent_centre_coordinate(std::uint64_t hv, Fx continent_center_jitter) {
    if (hv >= (std::uint64_t{1} << 32U)) {
        fatal("continent_centre_coordinate: hv outside [0, 2^32)");
    }
    const Fx unit = Fx::from_raw((static_cast<std::int64_t>(hv) - TWO_POW_31) * 2);
    return Fx::HALF + mul(continent_center_jitter, unit);
}

ContinentCentre continent_centre(std::uint64_t seed, Fx continent_center_jitter) {
    const std::uint64_t key = stream_key(seed, streams::geo_elevation_continent);
    return ContinentCentre{continent_centre_coordinate(hash_at(key, 0, 0) >> 32U, continent_center_jitter),
                           continent_centre_coordinate(hash_at(key, 1, 0) >> 32U, continent_center_jitter)};
}

// --- E2, E3 ----------------------------------------------------------------------------------

std::vector<std::int64_t> belt_gradient(Grid grid, std::span<const std::int64_t> nb, std::uint32_t threads) {
    check_size(grid, nb.size(), "belt_gradient: nb");
    check_threads(threads);
    std::vector<std::int64_t> g(grid.cells(), 0);
    const std::int32_t w = grid.width;
    const std::int32_t hgt = grid.height;
    const auto at = [&](std::int32_t x, std::int32_t y) { return Fx::from_raw(nb[grid.index(x, y)]); };
    geo::for_each_row_block(hgt, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            for (std::int32_t x = 0; x < w; ++x) {
                const std::int32_t x0 = std::max(x - 1, 0);
                const std::int32_t x1 = std::min(x + 1, w - 1);
                const std::int32_t y0 = std::max(y - 1, 0);
                const std::int32_t y1 = std::min(y + 1, hgt - 1);
                // A side of one cell has x1 = x0: the difference is 0 (div_int by 1 avoids 0).
                const Fx gx = div_int(at(x1, y) - at(x0, y), std::max(x1 - x0, 1));
                const Fx gy = div_int(at(x, y1) - at(x, y0), std::max(y1 - y0, 1));
                g[grid.index(x, y)] = hypot(gx, gy).raw();
            }
        }
    });
    return g;
}

Fx belt_width(Fx g_med, Fx hw) {
    return max(Fx::from_raw(1), div_int(mul(g_med, hw), geo::CELL_M));
}

ElevationFields elevation_fields(std::uint64_t seed, std::int32_t n, const ElevationTuning& t, std::uint32_t threads) {
    check_n(n, "elevation_fields: N outside [16, 2048]");
    check_threads(threads);
    const Keys keys = stage_keys(seed);
    const Grid grid = geo::square_grid(n);
    const std::size_t cells = grid.cells();
    ElevationFields out;
    out.n = n;
    out.c.assign(cells, 0);
    out.nb.assign(cells, 0);
    out.ns.assign(cells, 0);
    out.centre = continent_centre(seed, t.continent_center_jitter);  // E1 (D1)
    const Fx cx = out.centre.cx;
    const Fx cy = out.centre.cy;
    // E2: pass A (D1, G6 row blocks).
    geo::for_each_row_block(n, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            const Fx v = elevation_map_fraction(y, n);
            const Fx ym = elevation_metres(y);
            for (std::int32_t x = 0; x < n; ++x) {
                const Fx u = elevation_map_fraction(x, n);
                const Fx xm = elevation_metres(x);
                const Fx wx = geo::fbm_map(keys.continent_warp_x, u, v, t.continent_warp);
                const Fx wy = geo::fbm_map(keys.continent_warp_y, u, v, t.continent_warp);
                const Fx uw = u + mul(t.continent_warp_amp_frac, wx);
                const Fx vw = v + mul(t.continent_warp_amp_frac, wy);
                const Fx d = mul_int(hypot(uw - cx, vw - cy), 2);
                const Fx nc = geo::fbm_map(keys.continent, uw, vw, t.continent_noise);
                const Fx uf = u + div_int(uw - u, 2);
                const Fx vf = v + div_int(vw - v, 2);
                const Fx ed = min(min(uf, vf), min(Fx::ONE - uf, Fx::ONE - vf));
                const Fx edn = ed + mul(t.edge_noise_amp, nc);
                const Fx fe = fade(clamp(div(edn, t.edge_falloff_frac), Fx(), Fx::ONE));
                const Fx dm = mul_int(hypot(uf - Fx::HALF, vf - Fx::HALF), 2) - mul(t.edge_noise_amp, nc);
                const Fx fr = fade(clamp(div(dm - t.edge_radial_start, t.edge_radial_end - t.edge_radial_start),
                                         Fx(), Fx::ONE));
                const Fx edge = mul(t.edge_falloff_depth, max(Fx::ONE - fe, fr));
                const Fx cd = geo::fbm_m(keys.coast, xm, ym, t.coast_noise);
                const Fx c =
                    (((Fx::ONE - d) + mul(t.continent_noise_amp, nc)) - edge) + mul(t.coast_noise_amp, cd);
                const Fx nb = geo::fbm_map(keys.belt, uw, vw, t.belt_noise);
                const Fx ns = geo::fbm_map(keys.segment, uw, vw, t.segment_noise);
                const CellIndex i = grid.index(x, y);
                out.c[i] = c.raw();
                out.nb[i] = nb.raw();
                out.ns[i] = ns.raw();
            }
        }
    });
    // E3.1 coast threshold (D7, G1).
    out.c_sea = Fx::from_raw(geo::quantile(out.c, Fx::ONE - t.land_fraction));
    // E3.2, E3.3 belt-noise gradient and its median (D2, then G1; the
    // gradient array is selected in place and then dropped).
    {
        std::vector<std::int64_t> g = belt_gradient(grid, out.nb, threads);
        out.g_med = Fx::from_raw(std::max<std::int64_t>(1, geo::kth_in_place(g, geo::quantile_rank(Fx::HALF, cells))));
    }
    // E3.4, E3.5 belt half-width.
    out.hw = belt_halfwidth(t.belt_halfwidth_m, t.belt_halfwidth_frac, n);
    out.w_b = belt_width(out.g_med, out.hw);
    // E3.6 land scale (G1 over a multiset built in index order).
    {
        std::vector<std::int64_t> above;
        const std::int64_t sea = out.c_sea.raw();
        for (const std::int64_t c : out.c) {
            if (c > sea) {
                above.push_back(c - sea);
            }
        }
        if (above.empty()) {
            fatal("elevation: no land");
        }
        out.s_land = Fx::from_raw(
            std::max<std::int64_t>(1, geo::kth_in_place(above, geo::quantile_rank(Fx::HALF, above.size()))));
    }
    return out;
}

// --- E4 --------------------------------------------------------------------------------------

std::vector<std::uint8_t> elevation_ocean_from_provisional(Grid grid, std::span<const std::uint8_t> provisional,
                                                           std::int64_t island_min) {
    check_size(grid, provisional.size(), "elevation_ocean_from_provisional: provisional");
    if (island_min < 0) {
        fatal("elevation_ocean_from_provisional: negative island_min");
    }
    const std::uint32_t cells = grid.cells();
    // Step 2 (D5, G2): the components of P (4-connected) touching the edge.
    const geo::Components pc = geo::label_components(grid, provisional, geo::Connectivity::four);
    std::vector<std::uint8_t> edge_label(static_cast<std::size_t>(pc.count) + 1U, 0);
    for (std::int32_t y = 0; y < grid.height; ++y) {
        for (std::int32_t x = 0; x < grid.width; ++x) {
            if (x == 0 || y == 0 || x == grid.width - 1 || y == grid.height - 1) {
                edge_label[pc.label[grid.index(x, y)]] = 1;
            }
        }
    }
    edge_label[0] = 0;  // label 0: not in P
    std::vector<std::uint8_t> ocean(cells, 0);
    for (CellIndex i = 0; i < cells; ++i) {
        ocean[i] = edge_label[pc.label[i]];
    }
    // Step 3 (D5, G2): sink 8-connected land components smaller than I_min.
    std::vector<std::uint8_t> land(cells, 0);
    for (CellIndex i = 0; i < cells; ++i) {
        land[i] = static_cast<std::uint8_t>(ocean[i] != 0U ? 0U : 1U);
    }
    const geo::Components lc = geo::label_components(grid, land, geo::Connectivity::eight);
    bool any_land = false;
    for (std::uint32_t k = 1; k <= lc.count; ++k) {
        if (std::int64_t{lc.size(k)} < island_min) {
            for (std::uint32_t p = lc.start[k - 1U]; p < lc.start[k]; ++p) {
                ocean[lc.order[p]] = 1;
            }
        } else {
            any_land = true;
        }
    }
    if (!any_land) {
        fatal("elevation: no land");
    }
    return ocean;
}

std::vector<std::uint8_t> elevation_classify(const ElevationFields& f, const ElevationTuning& tuning) {
    const std::int32_t n = f.n;
    check_n(n, "elevation_classify: N outside [16, 2048]");
    const Grid grid = geo::square_grid(n);
    check_size(grid, f.c.size(), "elevation_classify: c");
    const std::int64_t sea = f.c_sea.raw();
    const std::int32_t m = elevation_margin(tuning.edge_margin_frac, n);
    std::vector<std::uint8_t> p(grid.cells(), 0);
    for (std::int32_t y = 0; y < n; ++y) {
        for (std::int32_t x = 0; x < n; ++x) {
            const CellIndex i = grid.index(x, y);
            const std::int32_t edge = std::min(std::min(x, y), std::min(n - 1 - x, n - 1 - y));
            p[i] = static_cast<std::uint8_t>((f.c[i] <= sea || edge < m) ? 1U : 0U);
        }
    }
    return elevation_ocean_from_provisional(grid, p, island_min_cells(tuning.island_min_area_m2));
}

// --- E5 --------------------------------------------------------------------------------------

Fx ratio_capped(Fx a, Fx b) {
    if (a >= mul_int(b, ELEVATION_RATIO_CAP)) {
        return Fx::from_int(ELEVATION_RATIO_CAP);
    }
    return div(a, b);
}

LandProfile land_profile(Fx s, Fx s_land, Fx mountain_coast_t) {
    const Fx tl = ratio_capped(abs(s), s_land);
    const Fx gm = fade(clamp(div(tl, mountain_coast_t), Fx(), Fx::ONE));
    const Fx co = div(tl, Fx::ONE + tl);
    return LandProfile{tl, gm, co};
}

Fx belt_profile(Fx nb, Fx offset, Fx w_b) {
    const Fx a = abs(nb - offset);
    return a >= w_b ? Fx() : fade(Fx::ONE - div(a, w_b));
}

Fx segment_factor(Fx ns, Fx bias, Fx segment_contrast) {
    return fade(clamp((Fx::HALF + bias) + mul(segment_contrast, ns), Fx(), Fx::ONE));
}

Fx mountain_mask(Fx nb, Fx ns, Fx gm, Fx offset, Fx bias, Fx w_b, Fx segment_contrast) {
    return mul(mul(belt_profile(nb, offset, w_b), segment_factor(ns, bias, segment_contrast)), gm);
}

namespace {

// The land cells that can reach the core at one belt offset o: those with
// mul(b_o, gm) >= lev, which is necessary for mask(o, beta) >= lev for every
// beta (sg <= ONE and mul is monotone in each argument). core and inl count
// over this list, so they equal the counts over all land cells.
class FloorCandidates {
public:
    FloorCandidates(std::span<const FloorCell> cells, Fx w_b, const ElevationTuning& t)
        : cells_(cells), w_b_(w_b), t_(t) {}

    void build(Fx offset) {
        b_.clear();
        gm_.clear();
        cns_.clear();
        for (const FloorCell& cell : cells_) {
            const Fx b = belt_profile(cell.nb, offset, w_b_);
            if (mul(b, cell.gm) >= t_.segment_floor_mask) {
                b_.push_back(b);
                gm_.push_back(cell.gm);
                cns_.push_back(mul(t_.segment_contrast, cell.ns));
            }
        }
    }

    // core(o, bias) (inland_only false) or inl(o, bias) (true) for the
    // offset of the last build.
    [[nodiscard]] std::int64_t count(Fx bias, bool inland_only) const {
        std::int64_t k = 0;
        for (std::size_t j = 0; j < b_.size(); ++j) {
            if (inland_only && gm_[j] < t_.floor_inland_gm) {
                continue;
            }
            const Fx sg = fade(clamp((Fx::HALF + bias) + cns_[j], Fx(), Fx::ONE));
            if (mul(mul(b_[j], sg), gm_[j]) >= t_.segment_floor_mask) {
                ++k;
            }
        }
        return k;
    }

private:
    std::span<const FloorCell> cells_;
    Fx w_b_;
    const ElevationTuning& t_;
    std::vector<Fx> b_;
    std::vector<Fx> gm_;
    std::vector<Fx> cns_;  // mul(segment_contrast, ns)
};

}  // namespace

MountainFloor mountain_floor(std::span<const FloorCell> cells, Fx w_b, const ElevationTuning& t) {
    if (w_b <= Fx()) {
        fatal("mountain_floor: w_b must be positive");
    }
    MountainFloor out;
    out.target = mountain_floor_target(static_cast<std::int64_t>(cells.size()), t.segment_on_share);
    out.offset = Fx();
    out.bias = t.segment_bias;
    FloorCandidates cand(cells, w_b, t);
    cand.build(Fx());
    if (cand.count(t.segment_bias, false) >= out.target) {
        return out;  // the world has enough mountain core
    }
    out.triggered = true;
    const Fx two = Fx::from_int(2);
    // Offset search: 0, +d, -d, +2d, -2d, ..., +Kd, -Kd; a later offset
    // replaces the best only if its inland reach is strictly larger; stop at
    // the first whose reach meets the target.
    const Fx d = div_int(w_b, 2);
    Fx best_o;
    std::int64_t best_r = cand.count(two, true);
    if (best_r < out.target) {
        bool done = false;
        for (std::int32_t s = 1; s <= t.belt_offset_steps && !done; ++s) {
            for (const std::int32_t sign : {1, -1}) {
                const Fx o = mul_int(d, std::int64_t{sign} * s);
                cand.build(o);
                const std::int64_t r = cand.count(two, true);
                if (r > best_r) {
                    best_r = r;
                    best_o = o;
                }
                if (r >= out.target) {
                    done = true;
                    break;
                }
            }
        }
    }
    out.offset = best_o;
    // Bias: 32-step bisection on the full core at o* (D3, no tolerance).
    cand.build(out.offset);
    Fx lo = t.segment_bias;
    Fx hi = two;
    if (cand.count(lo, false) >= out.target) {
        hi = lo;
    } else {
        for (std::int32_t step = 0; step < ELEVATION_BISECTION_STEPS; ++step) {
            const Fx mid = Fx::from_raw((lo.raw() + hi.raw()) >> 1U);  // arithmetic shift (floor)
            out.bisection.push_back(mid);
            if (cand.count(mid, false) >= out.target) {
                hi = mid;
            } else {
                lo = mid;
            }
        }
    }
    out.bias = hi;
    return out;
}

// --- E5.3 ------------------------------------------------------------------------------------

std::vector<std::int64_t> chamfer_distance(Grid grid, std::span<const std::uint8_t> source) {
    check_size(grid, source.size(), "chamfer_distance: source");
    constexpr std::int64_t INF = std::int64_t{1} << 40U;
    const std::int32_t w = grid.width;
    const std::int32_t hgt = grid.height;
    std::vector<std::int64_t> d(grid.cells(), 0);
    for (CellIndex i = 0; i < grid.cells(); ++i) {
        d[i] = source[i] != 0U ? 0 : INF;
    }
    const auto at = [&](std::int32_t x, std::int32_t y) -> std::int64_t& { return d[grid.index(x, y)]; };
    // Forward pass in index order: W, NW, N, NE with weights 5, 7, 5, 7.
    for (std::int32_t y = 0; y < hgt; ++y) {
        for (std::int32_t x = 0; x < w; ++x) {
            std::int64_t& v = at(x, y);
            if (x > 0) {
                v = std::min(v, at(x - 1, y) + 5);
            }
            if (x > 0 && y > 0) {
                v = std::min(v, at(x - 1, y - 1) + 7);
            }
            if (y > 0) {
                v = std::min(v, at(x, y - 1) + 5);
            }
            if (x + 1 < w && y > 0) {
                v = std::min(v, at(x + 1, y - 1) + 7);
            }
        }
    }
    // Backward pass in reverse index order: E, SE, S, SW.
    for (std::int32_t y = hgt - 1; y >= 0; --y) {
        for (std::int32_t x = w - 1; x >= 0; --x) {
            std::int64_t& v = at(x, y);
            if (x + 1 < w) {
                v = std::min(v, at(x + 1, y) + 5);
            }
            if (x + 1 < w && y + 1 < hgt) {
                v = std::min(v, at(x + 1, y + 1) + 7);
            }
            if (y + 1 < hgt) {
                v = std::min(v, at(x, y + 1) + 5);
            }
            if (x > 0 && y + 1 < hgt) {
                v = std::min(v, at(x - 1, y + 1) + 7);
            }
        }
    }
    return d;
}

Fx ocean_depth(std::int64_t dl, Fx ocean_shelf_m, Fx ocean_depth_m) {
    if (dl < 1) {
        fatal("ocean_depth: distance below 1");
    }
    const Fx dist_m = div_int(Fx::from_int(geo::CELL_M * std::min<std::int64_t>(dl, std::int64_t{1} << 20U)), 5);
    const Fx q = div(dist_m, dist_m + ocean_shelf_m);
    return -(Fx::HALF + mul(ocean_depth_m, q));
}

// --- E6 --------------------------------------------------------------------------------------

MountainScale mountain_scale(std::span<const std::int64_t> land_units, const ElevationTuning& t) {
    if (land_units.empty()) {
        fatal("mountain_scale: no land");
    }
    MountainScale out;
    out.q = Fx::from_raw(geo::quantile(land_units, t.mountain_peak_rank));
    out.qc = Fx::from_raw(geo::quantile(land_units, t.mountain_cover_rank));
    const Fx cap = mul_int(t.mountain_height_m, 3);
    const Fx ps = (mul_int(out.q, 3) > Fx::ONE) ? div(t.mountain_height_m, out.q) : cap;
    out.scale = (out.qc > Fx() && mul(ps, out.qc) > t.mountain_cover_height_m) ? div(t.mountain_cover_height_m, out.qc)
                                                                                : ps;
    return out;
}

// --- E5.1, E5.2, E5.3, E6, E7 -----------------------------------------------------------------

namespace {

// E5 to E7. If owned is not null it is &f, and its grids c and ns are
// released after E5.2, their last reader (P1-01 8.1).
ElevationHeights heights_impl(std::uint64_t seed, const ElevationFields& f, std::span<const std::uint8_t> ocean,
                              const ElevationTuning& t, std::uint32_t threads, ElevationFields* owned) {
    const std::int32_t n = f.n;
    check_n(n, "elevation_heights: N outside [16, 2048]");
    check_threads(threads);
    const Grid grid = geo::square_grid(n);
    check_size(grid, ocean.size(), "elevation_heights: ocean");
    check_size(grid, f.c.size(), "elevation_heights: c");
    check_size(grid, f.nb.size(), "elevation_heights: nb");
    check_size(grid, f.ns.size(), "elevation_heights: ns");
    const Keys keys = stage_keys(seed);
    const std::uint32_t cells = grid.cells();
    ElevationHeights out;
    out.h.assign(cells, 0);
    out.unit.assign(cells, 0);
    out.mask.assign(cells, 0);
    out.profile.assign(cells, 0);

    // E5.1 mountain floor (D8 counts, fixed search order, D3 bisection).
    {
        std::vector<FloorCell> land;
        for (CellIndex i = 0; i < cells; ++i) {
            if (ocean[i] == 0U) {
                const LandProfile p = land_profile(Fx::from_raw(f.c[i]) - f.c_sea, f.s_land, t.mountain_coast_t);
                land.push_back(FloorCell{Fx::from_raw(f.nb[i]), Fx::from_raw(f.ns[i]), p.gm});
            }
        }
        out.land_cells = static_cast<std::int64_t>(land.size());
        out.floor = mountain_floor(land, f.w_b, t);
    }
    const Fx o_star = out.floor.offset;
    const Fx b_star = out.floor.bias;

    // E5.2 land heights h0 and unit (D1, G6 row blocks).
    geo::for_each_row_block(n, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            const Fx ym = elevation_metres(y);
            for (std::int32_t x = 0; x < n; ++x) {
                const CellIndex i = grid.index(x, y);
                if (ocean[i] != 0U) {
                    continue;
                }
                const Fx xm = elevation_metres(x);
                const LandProfile p = land_profile(Fx::from_raw(f.c[i]) - f.c_sea, f.s_land, t.mountain_coast_t);
                const Fx un = geo::fbm_m(keys.basin, xm, ym, t.basin_noise);
                const Fx base = mul(t.lowland_height_m, p.co) + mul(mul(t.basin_amp_m, un), p.co);
                const Fx mask = mountain_mask(Fx::from_raw(f.nb[i]), Fx::from_raw(f.ns[i]), p.gm, o_star, b_star,
                                              f.w_b, t.segment_contrast);
                const Fx qx = xm + mul(t.relief_warp_amp_m, geo::fbm_m(keys.relief_warp_x, xm, ym, t.relief_warp));
                const Fx qy = ym + mul(t.relief_warp_amp_m, geo::fbm_m(keys.relief_warp_y, xm, ym, t.relief_warp));
                Fx unit;
                if (mask > Fx()) {
                    const Fx ridge = geo::ridged_m(keys.ridge, qx, qy, t.ridge_noise, t.ridge_weight);
                    const Fx profile = t.foothill_fraction + mul(Fx::ONE - t.foothill_fraction, ridge);
                    unit = mul(mask, profile);
                    out.profile[i] = profile.raw();
                }
                const Fx hh = geo::fbm_m(keys.hills, qx, qy, t.hill_noise);
                const Fx rn = geo::fbm_m(keys.roughness, xm, ym, t.roughness_noise);
                const Fx rho = t.roughness_min + mul(Fx::ONE - t.roughness_min, fade(clamp(Fx::HALF + rn, Fx(), Fx::ONE)));
                const Fx hf = t.hill_coast_min + mul(Fx::ONE - t.hill_coast_min, p.co);
                const Fx hills = mul(t.hill_height_m, mul(mul(hh, rho), hf));
                const Fx det = mul(t.detail_amp_m, geo::fbm_m(keys.detail, xm, ym, t.detail_noise));
                out.h[i] = ((base + hills) + det).raw();  // h0
                out.unit[i] = unit.raw();
                out.mask[i] = mask.raw();
            }
        }
    });
    if (owned != nullptr) {
        std::vector<std::int64_t>().swap(owned->c);
        std::vector<std::int64_t>().swap(owned->ns);
    }

    // E5.3 ocean depth (D4 chamfer from the land, then D1).
    {
        std::vector<std::uint8_t> land(cells, 0);
        for (CellIndex i = 0; i < cells; ++i) {
            land[i] = static_cast<std::uint8_t>(ocean[i] == 0U ? 1U : 0U);
        }
        const std::vector<std::int64_t> dl = chamfer_distance(grid, land);
        geo::for_each_row_block(n, threads, [&](const geo::RowBlock& block) {
            const BlockCells bc = block_cells(grid, block);
            for (CellIndex i = bc.first; i < bc.last; ++i) {
                if (ocean[i] != 0U) {
                    out.h[i] = ocean_depth(dl[i], t.ocean_shelf_m, t.ocean_depth_m).raw();
                }
            }
        });
    }

    // E6 mountain scale (D7, G1) over the land units, zeros included.
    {
        std::vector<std::int64_t> units;
        units.reserve(static_cast<std::size_t>(out.land_cells));
        for (CellIndex i = 0; i < cells; ++i) {
            if (ocean[i] == 0U) {
                units.push_back(out.unit[i]);
            }
        }
        const MountainScale ms = mountain_scale(units, t);
        out.q = ms.q;
        out.qc = ms.qc;
        out.scale = ms.scale;
    }

    // E6.5 pass layout (single-threaded; reads h0 on land, never the ocean depth).
    {
        PassInputs in;
        in.grid = grid;
        in.h0 = out.h;
        in.unit = out.unit;
        in.mask = out.mask;
        in.profile = out.profile;
        in.nb = f.nb;
        in.ocean = ocean;
        in.scale = out.scale;
        in.offset = o_star;
        in.w_b = f.w_b;
        out.passes = pass_layout(seed, in, t);
    }

    // E7 compose (D1, G6 row blocks).
    const Fx h_max = Fx::from_int(ELEVATION_H_MAX_M);
    const Fx scale = out.scale;
    const std::vector<std::int64_t>& factor = out.passes.factor;
    const std::vector<std::int64_t>& adj = out.passes.adj;
    geo::for_each_row_block(n, threads, [&](const geo::RowBlock& block) {
        const BlockCells bc = block_cells(grid, block);
        for (CellIndex i = bc.first; i < bc.last; ++i) {
            if (ocean[i] != 0U) {
                continue;
            }
            const Fx natural = mul(scale, Fx::from_raw(out.unit[i]));
            Fx m = natural;
            if (factor[i] < Fx::ONE.raw()) {  // the lowered envelope
                m = pass_lowered(scale, Fx::from_raw(out.mask[i]), Fx::from_raw(out.profile[i]),
                                 Fx::from_raw(factor[i]), t.foothill_fraction, t.pass_texture_keep);
            }
            if (adj[i] != 0) {  // floor drainage, never above natural
                m = min(m - Fx::from_raw(adj[i]), natural);
            }
            out.h[i] = clamp(Fx::from_raw(out.h[i]) + m, t.land_floor_m, h_max).raw();
        }
    });
    return out;
}

}  // namespace

ElevationHeights elevation_heights(std::uint64_t seed, const ElevationFields& f, std::span<const std::uint8_t> ocean,
                                   const ElevationTuning& t, std::uint32_t threads) {
    return heights_impl(seed, f, ocean, t, threads, nullptr);
}

// --- E8 --------------------------------------------------------------------------------------

ErosionParams erosion_params(const ElevationTuning& t, std::uint64_t seed) {
    ErosionParams p;
    p.iterations = t.erosion_iterations;
    p.route_interval = t.erosion_route_interval;
    p.k = t.erosion_k;
    p.diffusion = t.erosion_diffusion;
    p.floor = t.land_floor_m;
    p.talus_drop_o = mul_int(t.talus_slope, geo::CELL_M);
    p.talus_drop_d = mul(p.talus_drop_o, Fx::SQRT2);
    p.talus_rate = t.talus_rate;
    p.key = stream_key(seed, streams::geo_elevation_erosion);
    return p;
}

void erosion_random_receivers(Grid grid, std::span<const std::int64_t> level, std::span<const std::uint8_t> ocean,
                              std::uint64_t key, std::int32_t it, std::span<Direction> rcv, std::uint32_t threads) {
    check_size(grid, level.size(), "erosion_random_receivers: level");
    check_size(grid, ocean.size(), "erosion_random_receivers: ocean");
    check_size(grid, rcv.size(), "erosion_random_receivers: rcv");
    check_threads(threads);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        const BlockCells bc = block_cells(grid, block);
        for (CellIndex c = bc.first; c < bc.last; ++c) {
            if (ocean[c] != 0U) {
                continue;
            }
            std::array<std::uint64_t, 8> w{};
            std::uint64_t total = 0;
            for (const Direction code : geo::NEIGHBOURS_8) {
                CellIndex m = 0;
                if (!geo::neighbour(grid, c, code, m) || level[m] >= level[c]) {
                    continue;
                }
                const std::uint64_t q = static_cast<std::uint64_t>(level[c] - level[m]) >> 16U;
                w[code] = geo::is_diagonal(code) ? q * q : 2U * q * q;
                total += w[code];
            }
            if (total == 0U) {
                continue;  // keeps G4's receiver
            }
            const std::uint64_t r = hash_at(key, static_cast<std::int64_t>(c), it) % total;
            std::uint64_t acc = 0;
            for (const Direction code : geo::NEIGHBOURS_8) {
                acc += w[code];
                if (w[code] != 0U && acc > r) {
                    rcv[c] = code;
                    break;
                }
            }
        }
    });
}

void erosion_factors(Grid grid, std::span<const std::uint32_t> accum, std::span<const Direction> rcv,
                     std::span<const std::uint8_t> ocean, Fx k, std::span<std::int64_t> f, std::uint32_t threads) {
    check_size(grid, accum.size(), "erosion_factors: accum");
    check_size(grid, rcv.size(), "erosion_factors: rcv");
    check_size(grid, ocean.size(), "erosion_factors: ocean");
    check_size(grid, f.size(), "erosion_factors: f");
    check_threads(threads);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        const BlockCells bc = block_cells(grid, block);
        for (CellIndex c = bc.first; c < bc.last; ++c) {
            if (ocean[c] != 0U) {
                f[c] = 0;
                continue;
            }
            if (rcv[c] > 7U) {
                fatal("erosion_factors: a land cell has no receiver");
            }
            // A <= N^2 <= 2^22 on the map (P1-01 7: A << 32 <= 2^54, root << 16 <= 2^43);
            // on any test grid A <= MAX_GRID_CELLS = 2^24, still A << 32 < 2^57.
            const std::uint64_t root = isqrt_u64(std::uint64_t{accum[c]} << 32U);
            Fx fc = mul(k, Fx::from_raw(static_cast<std::int64_t>(root << 16U)));
            if (geo::is_diagonal(rcv[c])) {
                fc = mul(fc, Fx::INV_SQRT2);
            }
            f[c] = fc.raw();
        }
    });
}

void erosion_solve(Grid grid, std::span<const std::int64_t> h, std::span<const std::uint8_t> ocean,
                   std::span<const Direction> rcv, std::span<const std::int64_t> f, std::span<const CellIndex> order,
                   Fx floor, std::span<std::int64_t> hn) {
    check_size(grid, h.size(), "erosion_solve: h");
    check_size(grid, ocean.size(), "erosion_solve: ocean");
    check_size(grid, rcv.size(), "erosion_solve: rcv");
    check_size(grid, f.size(), "erosion_solve: f");
    check_size(grid, order.size(), "erosion_solve: order");
    check_size(grid, hn.size(), "erosion_solve: hn");
    for (const CellIndex c : order) {  // D4: every receiver before its donors
        if (c >= grid.cells()) {
            fatal("erosion_solve: order holds a cell outside the grid");
        }
        if (ocean[c] != 0U) {
            hn[c] = h[c];
            continue;
        }
        CellIndex m = 0;
        if (rcv[c] > 7U || !geo::neighbour(grid, c, rcv[c], m)) {
            fatal("erosion_solve: a land cell's receiver is not a neighbour inside the grid");
        }
        const Fx base = ocean[m] != 0U ? Fx() : Fx::from_raw(hn[m]);
        const Fx hc = Fx::from_raw(h[c]);
        const Fx v = hc > base ? base + div(hc - base, Fx::ONE + Fx::from_raw(f[c])) : hc;
        hn[c] = max(v, floor).raw();
    }
}

void erosion_diffuse(Grid grid, std::span<const std::int64_t> hn, std::span<const std::uint8_t> ocean, Fx diffusion,
                     Fx floor, std::span<std::int64_t> h, std::uint32_t threads) {
    check_size(grid, hn.size(), "erosion_diffuse: hn");
    check_size(grid, ocean.size(), "erosion_diffuse: ocean");
    check_size(grid, h.size(), "erosion_diffuse: h");
    check_threads(threads);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {  // D2: reads hn, writes h
        const BlockCells bc = block_cells(grid, block);
        for (CellIndex c = bc.first; c < bc.last; ++c) {
            if (ocean[c] != 0U) {
                h[c] = hn[c];
                continue;
            }
            const Fx hc = Fx::from_raw(hn[c]);
            Fx s4;
            for (const Direction code : geo::NEIGHBOURS_4) {
                CellIndex m = 0;
                if (!geo::neighbour(grid, c, code, m)) {
                    s4 = s4 + hc;  // no flux across the grid edge (test grids only)
                } else if (ocean[m] == 0U) {
                    s4 = s4 + Fx::from_raw(hn[m]);
                }  // an ocean neighbour adds 0
            }
            h[c] = max(floor, hc + mul(diffusion, s4 - mul_int(hc, 4))).raw();
        }
    });
}

void erosion_talus(Grid grid, std::span<const std::int64_t> h, std::span<const std::uint8_t> ocean, Fx drop_o,
                   Fx drop_d, Fx rate, Fx floor, std::span<std::int64_t> t, std::uint32_t threads) {
    check_size(grid, h.size(), "erosion_talus: h");
    check_size(grid, ocean.size(), "erosion_talus: ocean");
    check_size(grid, t.size(), "erosion_talus: t");
    check_threads(threads);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {  // D2: reads h, writes t
        const BlockCells bc = block_cells(grid, block);
        for (CellIndex c = bc.first; c < bc.last; ++c) {
            if (ocean[c] != 0U) {
                t[c] = h[c];
                continue;
            }
            const Fx hc = Fx::from_raw(h[c]);
            Fx delta;
            for (const Direction code : geo::NEIGHBOURS_8) {
                CellIndex m = 0;
                if (!geo::neighbour(grid, c, code, m)) {
                    continue;
                }
                const bool land = ocean[m] == 0U;
                const Fx hm = land ? Fx::from_raw(h[m]) : Fx();
                const Fx dk = geo::is_diagonal(code) ? drop_d : drop_o;
                const Fx x = hc - hm;
                if (x > dk) {
                    delta = delta - mul(rate, x - dk);
                } else if (land && -x > dk) {
                    delta = delta + mul(rate, -x - dk);
                }
            }
            t[c] = max(floor, hc + delta).raw();
        }
    });
}

void erode(Grid grid, std::span<std::int64_t> h, std::span<const std::uint8_t> ocean, const ErosionParams& p,
           std::uint32_t threads) {
    check_size(grid, h.size(), "erode: h");
    check_size(grid, ocean.size(), "erode: ocean");
    check_threads(threads);
    if (p.iterations < 0 || p.route_interval < 1) {
        fatal("erode: iterations < 0 or route_interval < 1");
    }
    if (p.iterations == 0) {
        return;
    }
    const std::size_t cells = grid.cells();
    geo::RoutingFlood flood;
    geo::RoutingFloodScratch scratch;
    std::vector<Direction> rcv(cells, geo::NO_DIRECTION);
    std::vector<std::uint32_t> accum;
    std::vector<std::int64_t> f(cells, 0);
    std::vector<std::int64_t> hn(cells, 0);
    for (std::int32_t it = 0; it < p.iterations; ++it) {
        if (it % p.route_interval == 0) {
            geo::routing_flood(grid, h, ocean, flood, scratch);                          // E8.1 G3, D6
            geo::receivers(grid, flood.level, flood.parent, ocean, rcv, threads);         // E8.1 G4, D1
            erosion_random_receivers(grid, flood.level, ocean, p.key, it, rcv, threads);  // E8.2, D1
            accum = geo::accumulate(grid, rcv, ocean, flood.order);                       // E8.3 G5, D4
            erosion_factors(grid, accum, rcv, ocean, p.k, f, threads);                    // E8.3, D1
        }
        erosion_solve(grid, h, ocean, rcv, f, flood.order, p.floor, hn);                             // E8.4, D4
        erosion_diffuse(grid, hn, ocean, p.diffusion, p.floor, h, threads);                          // E8.5, D2
        erosion_talus(grid, h, ocean, p.talus_drop_o, p.talus_drop_d, p.talus_rate, p.floor, hn, threads);  // E8.6, D2
        std::copy(hn.begin(), hn.end(), h.begin());
    }
}

// E8.7 (D3 of D2 steps). Each step evaluates the active set (step 0: every
// land cell; then the land cells that are, or are 8-adjacent to, a cell
// whose delta was non-zero), in index order, in fixed blocks of that list
// that read h and write only their own slots; the new values are applied
// after the whole set is evaluated (Jacobi). A cell outside the set has
// delta 0 and keeps its value, so this equals the full-grid step (E8.7).
void final_talus(Grid grid, std::span<std::int64_t> h, std::span<const std::uint8_t> ocean, Fx drop_o, Fx drop_d,
                 Fx rate, Fx floor, std::int32_t iterations, std::uint32_t threads) {
    check_size(grid, h.size(), "final_talus: h");
    check_size(grid, ocean.size(), "final_talus: ocean");
    check_threads(threads);
    if (iterations < 0) {
        fatal("final_talus: negative iteration count");
    }
    if (iterations == 0) {
        return;
    }
    constexpr std::size_t BLOCK = 4096;
    const std::uint32_t cells = grid.cells();
    std::vector<CellIndex> active;
    for (CellIndex c = 0; c < cells; ++c) {
        if (ocean[c] == 0U) {
            active.push_back(c);
        }
    }
    std::vector<std::int64_t> next_h;
    std::vector<std::uint8_t> changed;
    std::vector<std::uint8_t> flag(cells, 0);
    std::vector<CellIndex> next;
    for (std::int32_t it = 0; it < iterations && !active.empty(); ++it) {
        next_h.assign(active.size(), 0);
        changed.assign(active.size(), 0);
        const auto blocks = static_cast<std::uint32_t>((active.size() + BLOCK - 1U) / BLOCK);
        run_blocks(blocks, threads, [&](std::uint32_t b) {
            const std::size_t first = std::size_t{b} * BLOCK;
            const std::size_t last = std::min(active.size(), first + BLOCK);
            for (std::size_t k = first; k < last; ++k) {
                const CellIndex c = active[k];
                const Fx hc = Fx::from_raw(h[c]);
                Fx delta;
                for (const Direction code : geo::NEIGHBOURS_8) {
                    CellIndex m = 0;
                    if (!geo::neighbour(grid, c, code, m)) {
                        continue;
                    }
                    const bool land = ocean[m] == 0U;
                    const Fx hm = land ? Fx::from_raw(h[m]) : Fx();
                    const Fx dk = geo::is_diagonal(code) ? drop_d : drop_o;
                    const Fx x = hc - hm;
                    if (x > dk) {
                        delta = delta - mul(rate, x - dk);
                    } else if (land && -x > dk) {
                        delta = delta + mul(rate, -x - dk);
                    }
                }
                if (delta != Fx()) {
                    changed[k] = 1;
                    next_h[k] = max(floor, hc + delta).raw();
                }
            }
        });
        next.clear();
        const auto mark = [&](CellIndex c) {
            if (flag[c] == 0U) {
                flag[c] = 1;
                next.push_back(c);
            }
        };
        for (std::size_t k = 0; k < active.size(); ++k) {
            if (changed[k] == 0U) {
                continue;
            }
            const CellIndex c = active[k];
            h[c] = next_h[k];
            mark(c);
            for (const Direction code : geo::NEIGHBOURS_8) {
                CellIndex m = 0;
                if (geo::neighbour(grid, c, code, m) && ocean[m] == 0U) {
                    mark(m);
                }
            }
        }
        for (const CellIndex c : next) {
            flag[c] = 0;
        }
        std::sort(next.begin(), next.end());  // determinism-lint: allow(unstable-sort) -- unique cell indices
        active.swap(next);
    }
}

// --- E9 --------------------------------------------------------------------------------------

std::int16_t quantize_elevation_cell(std::int64_t h_raw, bool ocean) {
    const std::int64_t q = mul_div(h_raw, 10, TWO_POW_32);
    const std::int64_t e = ocean ? std::min<std::int64_t>(-1, q) : std::max<std::int64_t>(1, q);
    if (e < std::numeric_limits<std::int16_t>::min() || e > std::numeric_limits<std::int16_t>::max()) {
        fatal("quantize_elevation: value does not fit i16");
    }
    return static_cast<std::int16_t>(e);
}

std::vector<std::int16_t> quantize_elevation(Grid grid, std::span<const std::int64_t> h,
                                             std::span<const std::uint8_t> ocean, std::uint32_t threads) {
    check_size(grid, h.size(), "quantize_elevation: h");
    check_size(grid, ocean.size(), "quantize_elevation: ocean");
    check_threads(threads);
    std::vector<std::int16_t> e(grid.cells(), 0);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        const BlockCells bc = block_cells(grid, block);
        for (CellIndex c = bc.first; c < bc.last; ++c) {
            e[c] = quantize_elevation_cell(h[c], ocean[c] != 0U);
        }
    });
    return e;
}

// --- E10 -------------------------------------------------------------------------------------

Fx fill_cost(Fx nz, Fx fill_cost_amp) {
    return Fx::ONE + mul(fill_cost_amp, clamp(Fx::HALF + nz, Fx(), Fx::ONE));
}

std::int64_t fill_bump(Fx dn, Fx fill_detail_m) {
    const Fx bv = mul(fill_detail_m, max(Fx(), dn));
    return mul_div(bv.raw(), 10 * FINE_UNIT, TWO_POW_32);
}

std::int64_t fill_slope_eps(std::int64_t fine_slope, Fx cost, bool diagonal) {
    const Fx ce = diagonal ? mul(cost, Fx::SQRT2) : cost;
    return std::max<std::int64_t>(1, mul_div(fine_slope, ce.raw(), TWO_POW_32));
}

FillGrids fill_grids(std::uint64_t seed, std::int32_t n, std::span<const std::uint8_t> ocean,
                     const ElevationTuning& t, std::uint32_t threads) {
    check_n(n, "fill_grids: N outside [16, 2048]");
    check_threads(threads);
    const Grid grid = geo::square_grid(n);
    check_size(grid, ocean.size(), "fill_grids: ocean");
    const Keys keys = stage_keys(seed);
    FillGrids out;
    out.cost.assign(grid.cells(), Fx::ONE.raw());
    out.bump.assign(grid.cells(), 0);
    geo::for_each_row_block(n, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            const Fx ym = elevation_metres(y);
            for (std::int32_t x = 0; x < n; ++x) {
                const CellIndex i = grid.index(x, y);
                if (ocean[i] != 0U) {
                    continue;
                }
                const Fx xm = elevation_metres(x);
                out.cost[i] = fill_cost(geo::fbm_m(keys.fill, xm, ym, t.fill_noise), t.fill_cost_amp).raw();
                out.bump[i] = fill_bump(geo::fbm_m(keys.detail, xm, ym, t.detail_noise), t.fill_detail_m);
            }
        }
    });
    return out;
}

std::vector<std::uint32_t> select_lakes(std::span<const DepressionComponent> components, const ConditionParams& p) {
    if (p.max_share_cells < 1) {
        fatal("select_lakes: max_share_cells below 1");
    }
    std::vector<DepressionComponent> cand;
    for (const DepressionComponent& c : components) {
        if (c.size >= p.lake_min_cells && c.size <= p.lake_max_cells && c.depth >= p.lake_min_depth_dm) {
            cand.push_back(c);
        }
    }
    // D7: (volume descending, label ascending), a strict total order for the
    // unique labels of E10.2. stable_sort keeps the input order of equal
    // elements, so the result is defined even for a caller's duplicate labels.
    std::stable_sort(cand.begin(), cand.end(), [](const DepressionComponent& a, const DepressionComponent& b) {
        return a.volume != b.volume ? a.volume > b.volume : a.label < b.label;
    });
    std::vector<std::uint32_t> kept;
    std::int64_t total = 0;
    for (const DepressionComponent& c : cand) {
        if (total + c.size > p.max_share_cells) {
            continue;  // a later, smaller candidate may still fit
        }
        kept.push_back(c.label);
        total += c.size;
    }
    return kept;
}

Conditioned condition_depressions(Grid grid, std::span<const std::int16_t> e, std::span<const std::uint8_t> ocean,
                                  std::span<const std::int64_t> cost, std::span<const std::int64_t> bump,
                                  const ConditionParams& p) {
    check_size(grid, e.size(), "condition_depressions: e");
    check_size(grid, ocean.size(), "condition_depressions: ocean");
    check_size(grid, cost.size(), "condition_depressions: cost");
    check_size(grid, bump.size(), "condition_depressions: bump");
    if (p.fine_slope < 0 || p.max_share_cells < 1) {
        fatal("condition_depressions: negative fine_slope or max_share_cells below 1");
    }
    const std::uint32_t cells = grid.cells();
    const std::vector<std::int64_t> k(e.begin(), e.end());
    Conditioned out;

    // E10.1 flood levels (D6, G3).
    std::vector<std::uint8_t> depressed(cells, 0);
    std::vector<std::uint8_t> kept(cells, 0);
    {
        const geo::RoutingFlood flood = geo::routing_flood(grid, k, ocean);
        for (CellIndex i = 0; i < cells; ++i) {
            depressed[i] = static_cast<std::uint8_t>((ocean[i] == 0U && flood.level[i] > k[i]) ? 1U : 0U);
        }
        // E10.2 depression components (D5, G2: 8-connected, joined at equal L).
        const geo::Components comp = geo::label_components(grid, depressed, geo::Connectivity::eight, flood.level);
        out.components.reserve(comp.count);
        for (std::uint32_t label = 1; label <= comp.count; ++label) {
            DepressionComponent dc;
            dc.label = label;
            dc.size = std::int64_t{comp.size(label)};
            for (std::uint32_t q = comp.start[label - 1U]; q < comp.start[label]; ++q) {
                const CellIndex c = comp.order[q];
                const std::int64_t below = flood.level[c] - k[c];
                dc.depth = std::max(dc.depth, below);
                dc.volume += below;
            }
            out.components.push_back(dc);
        }
        // E10.3 lake selection (D7).
        out.kept = select_lakes(out.components, p);
        for (const std::uint32_t label : out.kept) {
            for (std::uint32_t q = comp.start[label - 1U]; q < comp.start[label]; ++q) {
                kept[comp.order[q]] = 1;
            }
        }
    }

    // E10.4 noisy priority flood (D6): pops in ascending (P, seq).
    using Entry = std::tuple<std::int64_t, std::uint64_t, CellIndex>;
    // determinism-lint: allow(heap-order) -- entries compare as tuples (P, seq, cell) with seq the global push counter, unique per entry, so std::greater is a strict total order ending in the push sequence number (D6)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
    std::vector<std::int64_t> level(cells, 0);  // P
    std::vector<std::int64_t> kf(cells, 0);     // fine key
    std::vector<std::uint8_t> visited(cells, 0);
    std::uint64_t seq = 0;
    for (CellIndex i = 0; i < cells; ++i) {
        if (ocean[i] != 0U) {
            visited[i] = 1;
            queue.emplace(0, seq++, i);
        }
    }
    if (seq == 0U) {
        fatal("condition_depressions: no ocean cell");
    }
    while (!queue.empty()) {
        const auto [lv, sq, c] = queue.top();
        queue.pop();
        static_cast<void>(sq);
        for (const Direction code : geo::NEIGHBOURS_8) {
            CellIndex m = 0;
            if (!geo::neighbour(grid, c, code, m) || visited[m] != 0U) {
                continue;
            }
            visited[m] = 1;
            const std::int64_t base = checked_mul(k[m], FINE_UNIT);
            if (kept[m] != 0U) {
                kf[m] = base;
                level[m] = std::max(base, lv);
            } else {
                std::int64_t eps =
                    depressed[m] != 0U ? fill_slope_eps(p.fine_slope, Fx::from_raw(cost[m]), geo::is_diagonal(code)) : 1;
                if (depressed[m] != 0U || (k[m] == 1 && base <= lv)) {
                    eps = checked_add(eps, bump[m]);
                }
                kf[m] = std::max(base, checked_add(lv, eps));
                level[m] = kf[m];
            }
            queue.emplace(level[m], seq++, m);
        }
    }

    // E10.5 output.
    out.e.assign(e.begin(), e.end());
    for (CellIndex i = 0; i < cells; ++i) {
        if (ocean[i] == 0U && kept[i] == 0U) {
            out.e[i] = static_cast<std::int16_t>(std::min(ELEVATION_KEY_MAX, kf[i] >> ELEVATION_FINE_SHIFT));
        }
    }
    return out;
}

// --- The whole stage ---------------------------------------------------------------------------

ElevationResult elevation_run(std::uint64_t seed, std::int32_t n, const ElevationTuning& tuning,
                              std::uint32_t threads) {
    check_n(n, "elevation_run: N outside [16, 2048]");
    check_threads(threads);
    const Grid grid = geo::square_grid(n);
    ElevationResult result;
    std::vector<std::uint8_t> ocean;
    std::vector<std::int64_t> h;
    std::int64_t land_cells = 0;
    {
        ElevationFields fields = elevation_fields(seed, n, tuning, threads);  // E1 to E3
        ocean = elevation_classify(fields, tuning);                           // E4
        // E5 to E7; c and ns are released after E5.2 (P1-01 8.1).
        ElevationHeights heights = heights_impl(seed, fields, ocean, tuning, threads, &fields);
        h = std::move(heights.h);
        land_cells = heights.land_cells;
        result.sites = std::move(heights.passes.sites);
        result.h7.reserve(result.sites.size());
        for (const PassSite& site : result.sites) {
            result.h7.push_back(h[site.col]);
        }
    }  // nb, unit, mask, profile, P and adj are released here
    const FillGrids fill = fill_grids(seed, n, ocean, tuning, threads);  // E10.0
    erode(grid, h, ocean, erosion_params(tuning, seed), threads);         // E8
    {
        const Fx drop_o = mul_int(tuning.final_talus_slope, geo::CELL_M);  // E8.7
        final_talus(grid, h, ocean, drop_o, mul(drop_o, Fx::SQRT2), tuning.final_talus_rate, tuning.land_floor_m,
                    tuning.final_talus_iterations, threads);
    }
    const std::vector<std::int16_t> e = quantize_elevation(grid, h, ocean, threads);  // E9
    std::vector<std::int64_t>().swap(h);
    ConditionParams cp;
    cp.lake_min_cells = lake_min_cells(tuning.lake_min_area_m2);
    cp.lake_max_cells = lake_max_cells(tuning.lake_max_area_m2, tuning.lake_area_ref_n, n);
    cp.lake_min_depth_dm = lake_min_depth_dm(tuning.lake_min_depth_m);
    cp.max_share_cells = lake_max_share_cells(land_cells, tuning.lake_max_share);
    cp.fine_slope = fill_fine_slope(tuning.fill_slope_dm);
    result.e = condition_depressions(grid, e, ocean, fill.cost, fill.bump, cp).e;  // E10
    return result;
}

std::vector<std::int16_t> elevation_grid(std::uint64_t seed, std::int32_t n, const ElevationTuning& tuning,
                                         std::uint32_t threads) {
    return elevation_run(seed, n, tuning, threads).e;
}

void run_elevation(StageContext& context) {
    const ElevationTuning tuning = elevation_tuning_from(context.content());
    const std::int64_t map_size = context.map_size();
    if (map_size < ELEVATION_MIN_N || map_size > ELEVATION_MAX_N) {
        fatal("elevation: map size outside [16, 2048]");
    }
    const auto n = static_cast<std::int32_t>(map_size);
    ElevationResult result = elevation_run(context.seed(), n, tuning, context.threads());
    // E11: the stage's layer (P1-01 9), then the pass rows in site order (9.1).
    db::LayerGrid grid;
    grid.width = static_cast<std::uint32_t>(n);
    grid.height = static_cast<std::uint32_t>(n);
    grid.values = std::move(result.e);
    context.write_layer(ELEVATION_LAYER, std::move(grid));
    const Grid g = geo::square_grid(n);
    for (std::size_t s = 0; s < result.sites.size(); ++s) {
        const PassSite& site = result.sites[s];
        db::PassRow row;
        row.id = static_cast<std::int64_t>(s) + 1;
        row.range_id = site.range;
        row.kind = site.natural_gap ? 0 : 1;
        row.capped = site.capped ? 1 : 0;
        row.col_x = g.x_of(site.col);
        row.col_y = g.y_of(site.col);
        row.saddle_dm = mul_div(site.dh, 10, FINE_UNIT);
        row.col_dm = mul_div(result.h7[s], 10, TWO_POW_32);
        row.path.reserve(site.axis.size() * 4U);
        for (const CellIndex c : site.axis) {
            for (std::uint32_t b = 0; b < 4U; ++b) {
                row.path.push_back(static_cast<std::uint8_t>((c >> (8U * b)) & 0xFFU));
            }
        }
        context.write_pass(row);
    }
}

}  // namespace worldgen::stages
