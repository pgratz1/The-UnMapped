// The production kind registry (P0-03 section 7.1): the one source file that
// lists every kind schema the generator loads from the data directory.
//
// P1 registers the fixed-id kind `worldgen` (P1-00 section 6.1, P0-03
// amendment 16) with the ids of the first pass, `elevation` (P1-01 section
// 4.2) and `hydrology` (P1-02 section 4), and of the second pass so far,
// `climate` (P1-03 section 4), `biomes` (P1-04 section 4.2), `resources`
// (P1-05 section 4.2) and `regions` (P1-06 section 4).
//
// P3-01 registers the history kinds `race`, `culture` and `history`; their
// schemas are in kinds_history.cpp.

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data/text.hpp"
#include "worldgen/data/content.hpp"
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

// P1-01 4.3 named check 4.
std::optional<std::string> check_pass_masks(const Record& r) {
    if (r.get_fx("pass_region_mask") > r.get_fx("pass_core_mask")) {
        return std::string("pass_region_mask must not exceed pass_core_mask");
    }
    return std::nullopt;
}

// P1-01 4.3 named check 5.
std::optional<std::string> check_pass_saddle_clamp(const Record& r) {
    if (r.get_fx("pass_min_col_m") > r.get_fx("pass_saddle_max_m")) {
        return std::string("pass_min_col_m must not exceed pass_saddle_max_m");
    }
    return std::nullopt;
}

// P1-03 4 named check wind_weights_positive.
std::optional<std::string> check_wind_weights_positive(const Record& r) {
    std::int64_t sum = 0;
    for (const Value& v : r.get_array("wind_weights")) {
        sum += v.as_int();  // 8 entries of at most 1000: no overflow
    }
    if (sum < 1) {
        return std::string("wind_weights must not all be 0");
    }
    return std::nullopt;
}

// P1-05 4.2: a named check "<lo> must be below <hi>" for two fx or two int
// fields of one kind.
std::optional<std::string> check_fx_below(const Record& r, const char* lo, const char* hi) {
    if (r.get_fx(lo) < r.get_fx(hi)) {
        return std::nullopt;
    }
    return std::string(lo) + " must be below " + hi;
}

std::optional<std::string> check_int_below(const Record& r, const char* lo, const char* hi) {
    if (r.get_int(lo) < r.get_int(hi)) {
        return std::nullopt;
    }
    return std::string(lo) + " must be below " + hi;
}

// P1-05 4.2 named check hab_weights_positive.
std::optional<std::string> check_hab_weights_positive(const Record& r) {
    // Four fields of at most 1000: no overflow.
    const std::int64_t sum = r.get_int("hab_weight_fertility") + r.get_int("hab_weight_forage") +
                             r.get_int("hab_weight_timber") + r.get_int("hab_weight_ore");
    if (sum < 1) {
        return std::string("the hab_weight fields must not all be 0");
    }
    return std::nullopt;
}

// P1-06 4 named check regions_tile_fits_spacing.
std::optional<std::string> check_regions_tile_fits_spacing(const Record& r) {
    // Both fields are at most 32768: no overflow.
    if (r.get_int("seed_tile_m") > 2 * r.get_int("seed_spacing_m")) {
        return std::string("seed_tile_m must be at most twice seed_spacing_m");
    }
    return std::nullopt;
}

// --- biome (P1-04 4.1) -------------------------------------------------------------

// Named check 2.
std::optional<std::string> check_biome_water_body(const Record& r) {
    const bool body = r.get_string("water_body") != "none";
    if (r.get_bool("water") != body) {
        return std::string("water must be true exactly when water_body is not \"none\"");
    }
    return std::nullopt;
}

// Named check 5.
std::optional<std::string> check_biome_water_rules(const Record& r) {
    if (r.get_string("water_body") != "none" && (r.get_int("priority") != 0 || !r.get_array("rules").empty())) {
        return std::string("water biomes take no rules and priority 0");
    }
    return std::nullopt;
}

// Named check 7, as a kind-level check so that it can report one error per
// failing pair: rules in array order, pairs in the order temp, rain, elev.
std::vector<KindCheckFailure> check_biome_rule_ranges(std::span<const Definition> defs) {
    std::vector<KindCheckFailure> out;
    static constexpr std::array<std::array<const char*, 2>, 3> PAIRS = {{
        {"temp_min_c", "temp_max_c"},
        {"rain_min_mm", "rain_max_mm"},
        {"elev_min_m", "elev_max_m"},
    }};
    for (std::size_t d = 0; d < defs.size(); ++d) {
        const std::vector<Value>& rules = defs[d].record.get_array("rules");
        for (std::size_t i = 0; i < rules.size(); ++i) {
            const Record& rule = rules[i].as_record();
            for (const auto& [lo, hi] : PAIRS) {
                const Value& a = rule.get(lo);
                const Value& b = rule.get(hi);
                const bool bad = a.kind() == ValueKind::fixed ? a.as_fx() > b.as_fx() : a.as_int() > b.as_int();
                if (bad) {
                    out.push_back(KindCheckFailure{d, "rules[" + detail::uint_text(i) + "]: " + std::string(lo) +
                                                          " must not exceed " + std::string(hi)});
                }
            }
        }
    }
    return out;
}

bool is_land_biome(const Definition& d) {
    return d.record.get_string("water_body") == "none";
}

std::string earlier(const Definition& d) {
    return "'" + std::string(BIOME_KIND) + "." + d.id + "'";
}

// Kind-level check 1: each later definition sharing a code is reported at
// its header, naming the earliest (id order).
std::vector<KindCheckFailure> check_biome_code_unique(std::span<const Definition> defs) {
    std::vector<KindCheckFailure> out;
    std::map<std::int64_t, std::size_t> first;
    for (std::size_t i = 0; i < defs.size(); ++i) {
        const std::int64_t code = defs[i].record.get_int("code");
        const auto [it, inserted] = first.try_emplace(code, i);
        if (!inserted) {
            out.push_back(
                KindCheckFailure{i, "code " + detail::int_text(code) + " is also used by " + earlier(defs[it->second])});
        }
    }
    return out;
}

// Kind-level check 3: exactly one "ocean" and exactly one "lake"; none is
// reported against the directory, each later one at its header.
std::vector<KindCheckFailure> check_biome_water_unique(std::span<const Definition> defs) {
    std::vector<KindCheckFailure> out;
    static constexpr std::array<std::string_view, 2> BODIES = {"ocean", "lake"};
    for (const std::string_view body : BODIES) {
        std::vector<std::size_t> found;
        for (std::size_t i = 0; i < defs.size(); ++i) {
            if (defs[i].record.get_string("water_body") == body) {
                found.push_back(i);
            }
        }
        const std::string quoted = "\"" + std::string(body) + "\"";
        if (found.empty()) {
            out.push_back(KindCheckFailure{std::nullopt, "no biome has water_body " + quoted});
        }
        for (std::size_t k = 1; k < found.size(); ++k) {
            out.push_back(
                KindCheckFailure{found[k], "water_body " + quoted + " is also used by " + earlier(defs[found[0]])});
        }
    }
    return out;
}

// Kind-level check 4: exactly one fallback, and it is a land biome.
std::vector<KindCheckFailure> check_biome_fallback(std::span<const Definition> defs) {
    std::vector<std::size_t> found;
    for (std::size_t i = 0; i < defs.size(); ++i) {
        if (defs[i].record.get_bool("fallback")) {
            found.push_back(i);
        }
    }
    std::vector<KindCheckFailure> out;
    if (found.empty()) {
        out.push_back(KindCheckFailure{std::nullopt, "no biome has fallback = true"});
    }
    for (std::size_t k = 1; k < found.size(); ++k) {
        out.push_back(KindCheckFailure{found[k], "fallback = true is also set by " + earlier(defs[found[0]])});
    }
    for (const std::size_t i : found) {
        if (!is_land_biome(defs[i])) {
            out.push_back(KindCheckFailure{i, "the fallback biome must have water_body \"none\""});
        }
    }
    return out;
}

// Kind-level check 6: land biomes with priority > 0 have distinct
// priorities; each later one is reported at its header, naming the earliest.
std::vector<KindCheckFailure> check_biome_priority_unique(std::span<const Definition> defs) {
    std::vector<KindCheckFailure> out;
    std::map<std::int64_t, std::size_t> first;
    for (std::size_t i = 0; i < defs.size(); ++i) {
        const std::int64_t p = defs[i].record.get_int("priority");
        if (!is_land_biome(defs[i]) || p == 0) {
            continue;
        }
        const auto [it, inserted] = first.try_emplace(p, i);
        if (!inserted) {
            out.push_back(
                KindCheckFailure{i, "priority " + detail::int_text(p) + " is also used by " + earlier(defs[it->second])});
        }
    }
    return out;
}

Schema biome_rule_schema() {
    return Schema(std::vector<Field>{
        Field::required("temp_min_c", Type::fixed("-60", "50")),
        Field::required("temp_max_c", Type::fixed("-60", "50")),
        Field::required("rain_min_mm", Type::integer(0, 10000)),
        Field::required("rain_max_mm", Type::integer(0, 10000)),
        Field::required("elev_min_m", Type::integer(0, 3000)),
        Field::required("elev_max_m", Type::integer(0, 3000)),
        Field::required("slope_max", Type::fixed("0", "100")),
        Field::required("hand_max_m", Type::fixed("0", "3000")),
        Field::required("coast_max_m", Type::integer(0, 32767)),
        Field::required("river_dist_max_m", Type::integer(0, 32767)),
        Field::required("wet_frac_min", Type::fixed("0", "1")),
    });
}

}  // namespace

Schema biome_schema() {
    // P1-04 4.1, in declaration order (this order fixes the canonical
    // encoding, P0-03 8).
    std::vector<Field> fields{
        Field::required("code", Type::integer(1, 254)),
        Field::required("name", Type::string(64)),
        Field::required("colour", Type::integer(0, 16777215)),
        Field::required("water", Type::boolean()),
        Field::required("water_body", Type::enumeration({"none", "lake", "ocean"})),
        Field::required("priority", Type::integer(0, 255)),
        Field::required("fallback", Type::boolean()),
        Field::required("rules", Type::array(Type::record(biome_rule_schema()), 0, 16)),
        Field::required("fertility", Type::fixed("0", "1")),
        Field::required("timber", Type::fixed("0", "1")),
        Field::required("forage", Type::fixed("0", "1")),  // P1-05 4.1, generator 0.10.0
        Field::required("region_noun", Type::string(32)),
    };
    std::vector<Check> checks{
        Check{"biome_water_body", [](const Record& r) { return check_biome_water_body(r); }},
        Check{"biome_water_rules", [](const Record& r) { return check_biome_water_rules(r); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

std::vector<KindCheck> biome_kind_checks() {
    std::vector<KindCheck> out;
    out.push_back(KindCheck{"biome_code_unique", [](std::span<const Definition> d) { return check_biome_code_unique(d); }});
    out.push_back(KindCheck{"biome_water_unique", [](std::span<const Definition> d) { return check_biome_water_unique(d); }});
    out.push_back(KindCheck{"biome_fallback", [](std::span<const Definition> d) { return check_biome_fallback(d); }});
    out.push_back(
        KindCheck{"biome_priority_unique", [](std::span<const Definition> d) { return check_biome_priority_unique(d); }});
    out.push_back(KindCheck{"biome_rule_ranges", [](std::span<const Definition> d) { return check_biome_rule_ranges(d); }});
    return out;
}

Schema worldgen_biomes_schema() {
    // P1-04 4.2, in declaration order (this order fixes the canonical
    // encoding, P0-03 8).
    return Schema(std::vector<Field>{
        Field::required("jitter_noise", geo::fbm_m_type()),
        Field::required("temp_jitter_c", Type::fixed("0", "10")),
        Field::required("rain_jitter", Type::fixed("0", "1")),
        Field::required("rain_scale_512", Type::fixed("0.25", "4")),
        Field::required("rain_scale_1024", Type::fixed("0.25", "4")),
        Field::required("rain_scale_2048", Type::fixed("0.25", "4")),
        Field::required("drain_river_class_min", Type::integer(1, 15)),
        Field::required("river_dist_class_min", Type::integer(1, 15)),
        Field::required("wet_window_m", Type::integer(0, 2000)),
        Field::required("wet_hand_m", Type::fixed("0", "100")),
        Field::required("wet_scale_512", Type::fixed("0.25", "4")),
        Field::required("wet_scale_1024", Type::fixed("0.25", "4")),
        Field::required("wet_scale_2048", Type::fixed("0.25", "4")),
    });
}

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
        Field::required("pass_count_max", Type::integer(0, 3)),
        Field::required("pass_core_mask", Type::fixed("0.01", "1")),
        Field::required("pass_crest_frac", Type::fixed("0", "1")),
        Field::required("pass_min_length_m", Type::integer(0, 100000)),
        Field::required("pass_spacing_m", Type::integer(500, 100000)),
        Field::required("pass_size_ref_n", Type::integer(16, 2048)),
        Field::required("pass_region_mask", Type::fixed("0.005", "1")),
        Field::required("pass_body_m", Type::fixed("0", "2900")),
        Field::required("pass_basin_depth_m", Type::fixed("1", "1000")),
        Field::required("pass_tarn_min_cells", Type::integer(0, 1000000)),
        Field::required("pass_divide_reach_m", Type::fixed("0", "20000")),
        Field::required("pass_saddle_window_m", Type::fixed("0", "20000")),
        Field::required("pass_saddle_rise_m", Type::fixed("0", "2900")),
        Field::required("pass_saddle_rise_reach_m", Type::fixed("0", "20000")),
        Field::required("pass_min_spacing_m", Type::fixed("0", "100000")),
        Field::required("pass_coast_clear_m", Type::fixed("0", "20000")),
        Field::required("pass_barrier_m", Type::fixed("0", "2900")),
        Field::required("pass_side_max_m", Type::fixed("0", "100000")),
        Field::required("pass_col_tarn_m", Type::fixed("0", "5000")),
        Field::required("pass_grade", Type::fixed("0", "2")),
        Field::required("pass_grade_jitter", Type::fixed("0", "1")),
        Field::required("pass_min_col_m", Type::fixed("0", "2900")),
        Field::required("pass_saddle_max_m", Type::fixed("0", "2900")),
        Field::required("pass_max_cut_m", Type::fixed("1", "2900")),
        Field::required("pass_peak_rise_m", Type::fixed("0", "2900")),
        Field::required("pass_halfwidth_m", Type::fixed("0", "5000")),
        Field::required("pass_width_jitter", Type::fixed("0", "0.9")),
        Field::required("pass_width_var", Type::fixed("0", "0.9")),
        Field::required("pass_walk_grade", Type::fixed("0.01", "4")),
        Field::required("pass_texture_keep", Type::fixed("0", "1")),
        Field::required("pass_drain_step_m", Type::fixed("0.01", "10")),
        Field::required("final_talus_iterations", Type::integer(0, 1000)),
        Field::required("final_talus_slope", Type::fixed("0.05", "16")),
        Field::required("final_talus_rate", Type::fixed("0", "0.125")),
    };
    std::vector<Check> checks{
        Check{"edge_radial_span", [](const Record& r) { return check_edge_radial_span(r); }},
        Check{"mountain_ranks", [](const Record& r) { return check_mountain_ranks(r); }},
        Check{"lake_areas", [](const Record& r) { return check_lake_areas(r); }},
        Check{"pass_masks", [](const Record& r) { return check_pass_masks(r); }},
        Check{"pass_saddle_clamp", [](const Record& r) { return check_pass_saddle_clamp(r); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

Schema worldgen_hydrology_schema() {
    return Schema(std::vector<Field>{
        Field::required("river_min_area_m2", Type::integer(256, 1000000000)),
        Field::required("flat_noise", geo::fbm_m_type()),
    });
}

Schema worldgen_climate_schema() {
    // P1-03 4, in declaration order (this order fixes the canonical
    // encoding, P0-03 8).
    std::vector<Field> fields{
        Field::required("temp_north_c", Type::fixed("-30", "40")),
        Field::required("temp_south_c", Type::fixed("-30", "40")),
        Field::required("lapse_c_per_km", Type::fixed("0", "15")),
        Field::required("temp_noise", geo::fbm_m_type()),
        Field::required("temp_noise_amp_c", Type::fixed("0", "5")),
        Field::required("wind_weights", Type::array(Type::integer(0, 1000), 8, 8)),
        Field::required("smooth_radius_m", Type::integer(0, CLIMATE_SMOOTH_RADIUS_MAX_M)),
        Field::required("rain_base_mm", Type::integer(0, 5000)),
        Field::required("rain_oro_mm", Type::integer(0, 100000)),
        Field::required("moisture_path_mm_km", Type::integer(100, 2000000)),
        Field::required("loss_max", Type::fixed("0", "0.9")),
        Field::required("ocean_recharge_m", Type::integer(16, 100000)),
        Field::required("coast_rain_mm", Type::integer(0, 5000)),
        Field::required("coast_rain_reach_m", Type::integer(16, 32767)),
        Field::required("rain_noise", geo::fbm_m_type()),
        Field::required("rain_noise_amp", Type::fixed("0", "1")),
        Field::required("rain_min_mm", Type::integer(0, 5000)),
        Field::required("temp_offset_max_c", Type::fixed("0", "5")),
    };
    std::vector<Check> checks{
        Check{"wind_weights_positive", [](const Record& r) { return check_wind_weights_positive(r); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

Schema worldgen_resources_schema() {
    // P1-05 4.2, in declaration order (this order fixes the canonical
    // encoding, P0-03 8).
    std::vector<Field> fields{
        Field::required("fresh_reach_m", Type::integer(16, 10000)),
        Field::required("fresh_rain_weight", Type::fixed("0", "1")),
        Field::required("fresh_rain_lo_mm", Type::integer(0, 10000)),
        Field::required("fresh_rain_hi_mm", Type::integer(0, 10000)),
        Field::required("fert_slope_lo", Type::fixed("0", "10")),
        Field::required("fert_slope_hi", Type::fixed("0", "10")),
        Field::required("alluvial_bonus", Type::fixed("0", "4")),
        Field::required("alluvial_hand_m", Type::fixed("0.1", "100")),
        Field::required("timber_slope_lo", Type::fixed("0", "10")),
        Field::required("timber_slope_hi", Type::fixed("0", "10")),
        Field::required("fish_river_class_min", Type::integer(1, 15)),
        Field::required("fish_weight", Type::fixed("0", "1")),
        Field::required("fish_reach_m", Type::integer(16, 10000)),
        Field::required("ore_noise", geo::fbm_m_type()),
        Field::required("ore_vein_lo", Type::fixed("-1", "1")),
        Field::required("ore_vein_hi", Type::fixed("-1", "1")),
        Field::required("ore_elev_lo_m", Type::integer(0, 3000)),
        Field::required("ore_elev_hi_m", Type::integer(0, 3000)),
        Field::required("ore_lowland", Type::fixed("0", "1")),
        Field::required("ore_slope_base", Type::fixed("0", "1")),
        Field::required("ore_slope_lo", Type::fixed("0", "10")),
        Field::required("ore_slope_hi", Type::fixed("0", "10")),
        Field::required("walk_slope_max", Type::fixed("0.01", "10")),
        Field::required("walk_min_area_m2", Type::integer(16384, 1000000000)),
        Field::required("hab_slope_lo", Type::fixed("0", "10")),
        Field::required("hab_slope_hi", Type::fixed("0", "10")),
        Field::required("hab_weight_fertility", Type::integer(0, 1000)),
        Field::required("hab_weight_forage", Type::integer(0, 1000)),
        Field::required("hab_weight_timber", Type::integer(0, 1000)),
        Field::required("hab_weight_ore", Type::integer(0, 1000)),
        Field::required("hab_water_min", Type::fixed("0", "1")),
        Field::required("hab_flood_hand_m", Type::fixed("0", "100")),
        Field::required("hab_flood_factor", Type::fixed("0", "1")),
        Field::required("hab_pocket_factor", Type::fixed("0", "1")),
    };
    // P1-05 4.2 named checks, in registration order.
    std::vector<Check> checks{
        Check{"fresh_rain_ordered",
              [](const Record& r) { return check_int_below(r, "fresh_rain_lo_mm", "fresh_rain_hi_mm"); }},
        Check{"fert_slope_ordered", [](const Record& r) { return check_fx_below(r, "fert_slope_lo", "fert_slope_hi"); }},
        Check{"timber_slope_ordered",
              [](const Record& r) { return check_fx_below(r, "timber_slope_lo", "timber_slope_hi"); }},
        Check{"ore_vein_ordered", [](const Record& r) { return check_fx_below(r, "ore_vein_lo", "ore_vein_hi"); }},
        Check{"ore_elev_ordered", [](const Record& r) { return check_int_below(r, "ore_elev_lo_m", "ore_elev_hi_m"); }},
        Check{"ore_slope_ordered", [](const Record& r) { return check_fx_below(r, "ore_slope_lo", "ore_slope_hi"); }},
        Check{"hab_slope_ordered", [](const Record& r) { return check_fx_below(r, "hab_slope_lo", "hab_slope_hi"); }},
        Check{"hab_weights_positive", [](const Record& r) { return check_hab_weights_positive(r); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

Schema region_names_schema() {
    // P1-06 4, record region_names, in declaration order.
    std::vector<Field> fields;
    for (const char* name : {"central", "north", "northeast", "east", "southeast", "south", "southwest", "west",
                             "northwest", "isle", "upland"}) {
        fields.push_back(Field::required(name, Type::string(32, word_grammar())));
    }
    return Schema(std::move(fields));
}

Schema region_name_pools_schema() {
    // P1-06 4 (r3), record name_pools: the [common, land, trait] weights of
    // each word list, in declaration order.
    std::vector<Field> fields;
    for (const char* name :
         {"nouns", "suffixes", "prefixes", "adjectives", "features", "possessors", "places", "objects"}) {
        fields.push_back(Field::required(name, Type::array(Type::integer(0, 1000), 3, 3)));
    }
    return Schema(std::move(fields));
}

Schema region_name_traits_schema() {
    // P1-06 4 (r3), record name_traits, in declaration order.
    return Schema(std::vector<Field>{
        Field::required("coast_cells", Type::integer(0, 100000000)),
        Field::required("river_class", Type::integer(1, 16)),
        Field::required("lake_permille", Type::integer(1, 1000)),
        Field::required("high_mean_m", Type::integer(0, 10000)),
        Field::required("ore_rich_cells", Type::integer(0, 100000000)),
        Field::required("barren_hab_max", Type::integer(-1, 255)),
        Field::required("fertile_min", Type::integer(0, 256)),
    });
}

Schema worldgen_regions_schema() {
    // P1-06 4, in declaration order (this order fixes the canonical
    // encoding, P0-03 8).
    std::vector<Field> fields{
        Field::required("seed_tile_m", Type::integer(256, 32768)),
        Field::required("seed_spacing_m", Type::integer(160, 32768)),
        Field::required("hand_ref_m", Type::integer(1, 10000)),
        Field::required("cost_noise", geo::fbm_m_type()),
        Field::required("cost_noise_amp", Type::fixed("0", "0.9")),
        Field::required("lake_cost_factor", Type::fixed("1", "10")),
        Field::required("river_border_class", Type::integer(1, 16)),
        Field::required("river_cost_m", Type::integer(0, 100000)),
        Field::required("region_min_area_m2", Type::integer(0, 100000000)),
        Field::required("ore_rich_min", Type::integer(1, 255)),
        Field::required("ore_reach_m", Type::integer(0, 10000)),
        Field::required("name_central_frac", Type::fixed("0", "0.5")),
        Field::required("names", Type::record(region_names_schema())),
        Field::required("upland_own_noun", Type::array(Type::reference(std::string(BIOME_KIND)), 0, 16)),  // r2
        Field::required("name_pool_weights", Type::record(region_name_pools_schema())),  // r3
        Field::required("name_traits", Type::record(region_name_traits_schema())),       // r3
    };
    std::vector<Check> checks{
        Check{"regions_tile_fits_spacing", [](const Record& r) { return check_regions_tile_fits_spacing(r); }},
    };
    return Schema(std::move(fields), std::move(checks));
}

KindRegistry builtin_kinds() {
    KindRegistry registry;
    // P1-00 6.1: kind `worldgen`, directory `worldgen`, required, fixed ids.
    registry.add(Kind::with_fixed_ids(std::string(WORLDGEN_KIND), std::string(WORLDGEN_DIRECTORY), true,
                                      {
                                          IdSchema{std::string(WORLDGEN_ELEVATION_ID), worldgen_elevation_schema()},
                                          IdSchema{std::string(WORLDGEN_HYDROLOGY_ID), worldgen_hydrology_schema()},
                                          IdSchema{std::string(WORLDGEN_CLIMATE_ID), worldgen_climate_schema()},
                                          IdSchema{std::string(WORLDGEN_BIOMES_ID), worldgen_biomes_schema()},
                                          IdSchema{std::string(WORLDGEN_RESOURCES_ID), worldgen_resources_schema()},
                                          IdSchema{std::string(WORLDGEN_REGIONS_ID), worldgen_regions_schema()},
                                      }));
    // P1-00 6.6: kind `biome`, directory `biomes`, required, open ids, with
    // the kind-level checks of P1-04 4.1.
    Kind biome{std::string(BIOME_KIND), std::string(BIOME_DIRECTORY), true, biome_schema()};
    biome.kind_checks = biome_kind_checks();
    registry.add(std::move(biome));
    // P3-01 3: kind `race`, directory `races`, required, with its
    // kind-level checks.
    Kind race{std::string(RACE_KIND), std::string(RACE_DIRECTORY), true, race_schema()};
    race.kind_checks = race_kind_checks();
    registry.add(std::move(race));
    // P3-01 4: kind `culture`, directory `cultures`, required; the naming
    // record of P3-02 3.
    registry.add(Kind{std::string(CULTURE_KIND), std::string(CULTURE_DIRECTORY), true, culture_schema()});
    // P3-01 5: kind `history`, directory `history`, required, fixed ids;
    // later P3 specs add their ids.
    Kind history = Kind::with_fixed_ids(std::string(HISTORY_KIND), std::string(HISTORY_DIRECTORY), true,
                                        {
                                            IdSchema{std::string(HISTORY_CHRONOLOGY_ID), history_chronology_schema()},
                                            IdSchema{std::string(HISTORY_CONFLICT_ID), history_conflict_schema()},
                                            IdSchema{std::string(HISTORY_ENGINE_ID), history_engine_schema()},
                                            IdSchema{std::string(HISTORY_MONSTERS_ID), history_monsters_schema()},
                                            IdSchema{std::string(HISTORY_PEOPLES_ID), history_peoples_schema()},
                                            IdSchema{std::string(HISTORY_RELATIONS_ID), history_relations_schema()},
                                            IdSchema{std::string(HISTORY_RENAMING_ID), history_renaming_schema()},
                                            IdSchema{std::string(HISTORY_REPORT_ID), history_report_schema()},
                                        });
    history.kind_checks = history_kind_checks();  // P3-04 4.2: report_events
    registry.add(std::move(history));
    // P3-08 (generator 0.17.0): kind `placename` (P1-06 r3 4.1) and kind
    // `name_filter` (P3-02 r5 11.2), both required, open ids; their schemas
    // are in kinds_names.cpp.
    Kind placename{std::string(PLACENAME_KIND), std::string(PLACENAME_DIRECTORY), true, placename_schema()};
    placename.kind_checks = placename_kind_checks();
    registry.add(std::move(placename));
    Kind name_filter{std::string(NAME_FILTER_KIND), std::string(NAME_FILTER_DIRECTORY), true, name_filter_schema()};
    name_filter.kind_checks = name_filter_kind_checks();
    registry.add(std::move(name_filter));
    // P5-02 4.1, 4.2 (generator 0.23.0): kinds beast and monster, required,
    // open ids.
    registry.add(Kind{std::string(BEAST_KIND), std::string(BEAST_DIRECTORY), true, beast_schema()});
    registry.add(Kind{std::string(MONSTER_KIND), std::string(MONSTER_DIRECTORY), true, monster_schema()});
    return registry;
}

}  // namespace worldgen::data
