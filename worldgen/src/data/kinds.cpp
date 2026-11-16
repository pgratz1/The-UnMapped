// The production kind registry (P0-03 section 7.1): the one source file that
// lists every kind schema the generator loads from the data directory.
//
// P1 registers the fixed-id kind `worldgen` (P1-00 section 6.1, P0-03
// amendment 16) with the ids of the first pass: `elevation` (P1-01 section
// 4.2) and `hydrology` (P1-02 section 4). The second-pass specs add their
// ids here.

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/data/schema.hpp"
#include "worldgen/geo/noise_schema.hpp"

namespace worldgen::data {

namespace {

// P1-01 4.3 named check 1.
std::optional<std::string> check_edge_radial_span(const Record& r) {
    const Fx span = r.get_fx("edge_radial_end") - r.get_fx("edge_radial_start");
    if (span < decimal_literal("0.01")) {
        return std::string("edge_radial_end - edge_radial_start must be at least 0.01");
    }
    return std::nullopt;
}

// P1-01 4.3 named check 2.
std::optional<std::string> check_mountain_ranks(const Record& r) {
    if (r.get_fx("mountain_cover_rank") > r.get_fx("mountain_peak_rank")) {
        return std::string("mountain_cover_rank must not exceed mountain_peak_rank");
    }
    return std::nullopt;
}

// P1-01 4.3 named check 3.
std::optional<std::string> check_lake_areas(const Record& r) {
    if (r.get_int("lake_min_area_m2") > r.get_int("lake_max_area_m2")) {
        return std::string("lake_min_area_m2 must not exceed lake_max_area_m2");
    }
    return std::nullopt;
}

}  // namespace

Schema worldgen_elevation_schema() {
    // P1-01 4.2, in declaration order (this order fixes the canonical
    // encoding, P0-03 8).
    std::vector<Field> fields{
        Field::required("land_fraction", Type::fixed("0.1", "0.8")),
        Field::required("edge_margin_frac", Type::fixed("0", "0.25")),
        Field::required("continent_warp", geo::fbm_map_type()),
        Field::required("continent_warp_amp_frac", Type::fixed("0", "0.5")),
        Field::required("continent_noise", geo::fbm_map_type()),
        Field::required("continent_noise_amp", Type::fixed("0", "4")),
        Field::required("continent_center_jitter", Type::fixed("0", "0.25")),
        Field::required("edge_falloff_frac", Type::fixed("0.01", "0.5")),
        Field::required("edge_falloff_depth", Type::fixed("0", "8")),
        Field::required("edge_noise_amp", Type::fixed("0", "0.25")),
        Field::required("edge_radial_start", Type::fixed("0", "1.99")),
        Field::required("edge_radial_end", Type::fixed("0.01", "2")),
        Field::required("coast_noise", geo::fbm_m_type()),
        Field::required("coast_noise_amp", Type::fixed("0", "2")),
        Field::required("island_min_area_m2", Type::integer(0, 1000000000)),
        Field::required("belt_noise", geo::fbm_map_type()),
        Field::required("belt_halfwidth_m", Type::fixed("100", "20000")),
        Field::required("belt_halfwidth_frac", Type::fixed("0.01", "1")),
        Field::required("segment_noise", geo::fbm_map_type()),
        Field::required("segment_contrast", Type::fixed("0", "16")),
        Field::required("segment_bias", Type::fixed("-1", "1")),
        Field::required("segment_on_share", Type::fixed("0", "1")),
        Field::required("segment_floor_mask", Type::fixed("0.01", "1")),
        Field::required("belt_offset_steps", Type::integer(0, 8)),
        Field::required("floor_inland_gm", Type::fixed("0", "1")),
        Field::required("mountain_coast_t", Type::fixed("0.01", "16")),
        Field::required("relief_warp", geo::fbm_m_type()),
        Field::required("relief_warp_amp_m", Type::fixed("0", "5000")),
        Field::required("ridge_noise", geo::fbm_m_type()),
        Field::required("ridge_weight", Type::fixed("0", "4")),
        Field::required("foothill_fraction", Type::fixed("0", "1")),
        Field::required("mountain_height_m", Type::fixed("0", "2900")),
        Field::required("mountain_peak_rank", Type::fixed("0.5", "1")),
        Field::required("mountain_cover_height_m", Type::fixed("0", "2900")),
        Field::required("mountain_cover_rank", Type::fixed("0.5", "1")),
        Field::required("lowland_height_m", Type::fixed("0", "2000")),
        Field::required("basin_noise", geo::fbm_m_type()),
        Field::required("basin_amp_m", Type::fixed("0", "1000")),
        Field::required("hill_noise", geo::fbm_m_type()),
        Field::required("roughness_noise", geo::fbm_m_type()),
        Field::required("roughness_min", Type::fixed("0", "1")),
        Field::required("hill_height_m", Type::fixed("0", "1000")),
        Field::required("hill_coast_min", Type::fixed("0", "1")),
        Field::required("detail_noise", geo::fbm_m_type()),
        Field::required("detail_amp_m", Type::fixed("0", "100")),
        Field::required("ocean_depth_m", Type::fixed("0", "2900")),
        Field::required("ocean_shelf_m", Type::fixed("0", "50000")),
        Field::required("erosion_iterations", Type::integer(0, 200)),
        Field::required("erosion_route_interval", Type::integer(1, 64)),
        Field::required("erosion_k", Type::fixed("0", "1")),
        Field::required("erosion_diffusion", Type::fixed("0", "0.25")),
        Field::required("talus_slope", Type::fixed("0.05", "4")),
        Field::required("talus_rate", Type::fixed("0", "0.0625")),
        Field::required("land_floor_m", Type::fixed("0.05", "10")),
        Field::required("lake_min_area_m2", Type::integer(0, 1000000000)),
        Field::required("lake_max_area_m2", Type::integer(0, 1000000000)),
        Field::required("lake_area_ref_n", Type::integer(16, 2048)),
        Field::required("lake_min_depth_m", Type::fixed("0", "100")),
        Field::required("lake_max_share", Type::fixed("0.001", "1")),
        Field::required("fill_slope_dm", Type::fixed("0", "10")),
        Field::required("fill_noise", geo::fbm_m_type()),
        Field::required("fill_cost_amp", Type::fixed("0", "16")),
        Field::required("fill_detail_m", Type::fixed("0", "10")),
    };
    std::vector<Check> checks{
        Check{"edge_radial_span", [](const Record& r) { return check_edge_radial_span(r); }},
        Check{"mountain_ranks", [](const Record& r) { return check_mountain_ranks(r); }},
        Check{"lake_areas", [](const Record& r) { return check_lake_areas(r); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

Schema worldgen_hydrology_schema() {
    return Schema(std::vector<Field>{
        Field::required("river_min_area_m2", Type::integer(256, 1000000000)),
        Field::required("flat_noise", geo::fbm_m_type()),
    });
}

KindRegistry builtin_kinds() {
    KindRegistry registry;
    // P1-00 6.1: kind `worldgen`, directory `worldgen`, required, fixed ids.
    registry.add(Kind::with_fixed_ids(std::string(WORLDGEN_KIND), std::string(WORLDGEN_DIRECTORY), true,
                                      {
                                          IdSchema{std::string(WORLDGEN_ELEVATION_ID), worldgen_elevation_schema()},
                                          IdSchema{std::string(WORLDGEN_HYDROLOGY_ID), worldgen_hydrology_schema()},
                                      }));
    return registry;
}

}  // namespace worldgen::data
