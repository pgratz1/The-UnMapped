#!/usr/bin/env python3
"""Independent reference of the canonical encoding and content hash (spec
P0-03 section 8) for the data roots of spec P3-01 (history data and
settings), written from the specs alone, without the project's C++.

It reads the TOML files of a data root with the standard library's tomllib
(syntax only), validates every definition against the schemas transcribed
below from the specs, encodes the content set as P0-03 8 states and hashes
it with SHA-256 (core_reference.sha256_hex, cross-checked with hashlib).

Schemas, in declaration order (the order fixes the encoding):
  biome     P1-00 6.6 with P1-04 4.1 and P1-05 4.1 (`forage` after `timber`)
  culture   P3-01 4, record `culture_naming` of P3-02 3
  history   P3-01 5.1 (fixed id `chronology`); P3-04 4.1 and 4.2 (fixed
            ids `engine`, record `engine_detail`, and `report`, record
            `report_event`); P3-05 12.1 (`peoples`); P3-06 12.1 (`conflict`,
            generator 0.16.0)
  race      P3-01 3 with `density_permille` (P3-05 12.2) and
            `prowess_permille` after `aggression` (P3-06 12.2, 0.16.0)
  worldgen  P1-01 4.2 (with the records of P1-01 4.1), P1-02 4, P1-03 4,
            P1-04 4.2, P1-05 4.2, P1-06 4 (fixed ids)
Kinds are encoded in ascending name: biome, culture, history, race, worldgen
(P3-01 7).

The spec listings of P3-01 3.1, 4 and 5.1 and P3-02 5 are transcribed below
(RACE_FILES, CULTURE_FILES, HISTORY_FILES with the listings of P3-04 4.1
and 4.2, P3-05 12.1 and P3-06 12.1; the race listings carry the 0.16.0
values of P3-06 12.2: prowess_permille, goblin aggression 600; report.toml
carries the 30 entries of kinds 1-30 in kind id order). The golden file is computed
from the TOML files on disk; `--from-spec` uses the transcribed listings for
a root whose races/, cultures/ or history/ directory does not exist yet (the
data files are the generator engineer's deliverable). `check` verifies that
the files on disk, where they exist, hold exactly the listings' values.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p3_data_reference.py check
        Self-tests, the spec listings' own invariants (P3-01 3: the named
        and kind-level checks and the cross-kind conditions hold for the
        shipped data), the files on disk against the listings where they
        exist, and the committed golden file against a fresh computation.
        Exit 1 on any disagreement.

    python3 -B worldgen/tests/reference/p3_data_reference.py golden [-] [--from-spec]
        Writes worldgen/tests/golden/p3-01-data.txt (or standard output
        with "-"): one line "<root> <hex>" for game/data,
        worldgen/tests/data/geo and worldgen/tests/data/geo-small.

    python3 -B worldgen/tests/reference/p3_data_reference.py hash ROOT [--kinds a,b,...] [--from-spec]
        Prints the content hash of one data root, for the given kinds only
        (default: all five). `--kinds biome,worldgen` gives the hash of a
        generator 0.11.0 data root, which `worldgen-cli report` of a world
        generated from that root prints, so the transcription of the older
        schemas can be cross-checked against the 0.11.0 build.

Generator 0.17.0 (P3-08): the module serves the 0.17.0 schemas by default
(kinds name_filter and placename, race field cell_weights, culture field
god_patterns, the two history.peoples fields, the two worldgen.regions
records) and the 0.17.0 listings (SPEC_FILES_017, derived from the 0.16.0
ones by the edits the specs state). While a root on disk holds 0.16.0
files, the 0.17.0 listings stand in for races/, cultures/ and history/,
game/data's name_filters/ and placenames/ for a fixture root without them,
and P1-06 4's shipped name_pool_weights and name_traits for a regions.toml
without them; `check` prints a note for each. `--version 0.16.0` (or
use_version("0.16.0")) serves the 0.16.0 schemas and listings, and
reproduces the 0.16.0 golden file and the content_hash of a 0.16.0 world.

Never regenerate the golden file to make a C++ test pass.
"""

import hashlib
import os
import struct
import sys
import tomllib

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
import core_reference as ref  # noqa: E402

REPO = os.path.normpath(os.path.join(_HERE, "..", "..", ".."))
GOLDEN = os.path.join(REPO, "worldgen", "tests", "golden", "p3-01-data.txt")
ROOTS = ("game/data", "worldgen/tests/data/geo", "worldgen/tests/data/geo-small")

# ---------------------------------------------------------------------------
# Schema types
# ---------------------------------------------------------------------------

INT = ("int",)
FX = ("fx",)
BOOL = ("bool",)
STR = ("str",)  # string, enum and ref: all encoded as 0x04 str(bytes)


def ARR(t):
    return ("arr", t)


def REC(fields):
    return ("rec", tuple(fields))


def fields(spec):
    """'a:I b:F c:S' -> ((a, INT), (b, FX), (c, STR))."""
    out = []
    for tok in spec.split():
        name, t = tok.split(":")
        out.append((name, {"I": INT, "F": FX, "B": BOOL, "S": STR, "M": FBM_MAP, "W": FBM_M}[t]))
    return tuple(out)


# P1-01 4.1: the shared noise records.
FBM_MAP = REC((("cycles", FX), ("octaves", INT), ("gain", FX)))
FBM_M = REC((("wavelength_m", INT), ("octaves", INT), ("gain", FX)))

# P1-04 4.1 record biome_rule (eleven fields).
BIOME_RULE = REC(fields(
    "temp_min_c:F temp_max_c:F rain_min_mm:I rain_max_mm:I elev_min_m:I elev_max_m:I slope_max:F "
    "hand_max_m:F coast_max_m:I river_dist_max_m:I wet_frac_min:F"))

# P1-04 4.1 with P1-05 4.1: the biome kind.
BIOME = (
    ("code", INT), ("name", STR), ("colour", INT), ("water", BOOL), ("water_body", STR), ("priority", INT),
    ("fallback", BOOL), ("rules", ARR(BIOME_RULE)), ("fertility", FX), ("timber", FX), ("forage", FX),
    ("region_noun", STR),
)

# P1-01 4.2 (97 fields, table order).
ELEVATION = fields(
    "land_fraction:F edge_margin_frac:F continent_warp:M continent_warp_amp_frac:F continent_noise:M "
    "continent_noise_amp:F continent_center_jitter:F edge_falloff_frac:F edge_falloff_depth:F edge_noise_amp:F "
    "edge_radial_start:F edge_radial_end:F coast_noise:W coast_noise_amp:F island_min_area_m2:I belt_noise:M "
    "belt_halfwidth_m:F belt_halfwidth_frac:F segment_noise:M segment_contrast:F segment_bias:F "
    "segment_on_share:F segment_floor_mask:F belt_offset_steps:I floor_inland_gm:F mountain_coast_t:F "
    "relief_warp:W relief_warp_amp_m:F ridge_noise:W ridge_weight:F foothill_fraction:F mountain_height_m:F "
    "mountain_peak_rank:F mountain_cover_height_m:F mountain_cover_rank:F lowland_height_m:F basin_noise:W "
    "basin_amp_m:F hill_noise:W roughness_noise:W roughness_min:F hill_height_m:F hill_coast_min:F "
    "detail_noise:W detail_amp_m:F ocean_depth_m:F ocean_shelf_m:F erosion_iterations:I "
    "erosion_route_interval:I erosion_k:F erosion_diffusion:F talus_slope:F talus_rate:F land_floor_m:F "
    "lake_min_area_m2:I lake_max_area_m2:I lake_area_ref_n:I lake_min_depth_m:F lake_max_share:F "
    "fill_slope_dm:F fill_noise:W fill_cost_amp:F fill_detail_m:F pass_count_max:I pass_core_mask:F "
    "pass_crest_frac:F pass_min_length_m:I pass_spacing_m:I pass_size_ref_n:I pass_region_mask:F pass_body_m:F "
    "pass_basin_depth_m:F pass_tarn_min_cells:I pass_divide_reach_m:F pass_saddle_window_m:F "
    "pass_saddle_rise_m:F pass_saddle_rise_reach_m:F pass_min_spacing_m:F pass_coast_clear_m:F "
    "pass_barrier_m:F pass_side_max_m:F pass_col_tarn_m:F pass_grade:F pass_grade_jitter:F pass_min_col_m:F "
    "pass_saddle_max_m:F pass_max_cut_m:F pass_peak_rise_m:F pass_halfwidth_m:F pass_width_jitter:F "
    "pass_width_var:F pass_walk_grade:F pass_texture_keep:F pass_drain_step_m:F final_talus_iterations:I "
    "final_talus_slope:F final_talus_rate:F")
assert len(ELEVATION) == 97

# P1-02 4.
HYDROLOGY = fields("river_min_area_m2:I flat_noise:W")

# P1-03 4 (wind_weights is array(int(0, 1000), 8, 8)).
CLIMATE = (
    ("temp_north_c", FX), ("temp_south_c", FX), ("lapse_c_per_km", FX), ("temp_noise", FBM_M),
    ("temp_noise_amp_c", FX), ("wind_weights", ARR(INT)), ("smooth_radius_m", INT), ("rain_base_mm", INT),
    ("rain_oro_mm", INT), ("moisture_path_mm_km", INT), ("loss_max", FX), ("ocean_recharge_m", INT),
    ("coast_rain_mm", INT), ("coast_rain_reach_m", INT), ("rain_noise", FBM_M), ("rain_noise_amp", FX),
    ("rain_min_mm", INT), ("temp_offset_max_c", FX),
)

# P1-04 4.2.
BIOMES_TUNING = fields(
    "jitter_noise:W temp_jitter_c:F rain_jitter:F rain_scale_512:F rain_scale_1024:F rain_scale_2048:F "
    "drain_river_class_min:I river_dist_class_min:I wet_window_m:I wet_hand_m:F wet_scale_512:F "
    "wet_scale_1024:F wet_scale_2048:F")

# P1-05 4.2 (34 fields).
RESOURCES = fields(
    "fresh_reach_m:I fresh_rain_weight:F fresh_rain_lo_mm:I fresh_rain_hi_mm:I fert_slope_lo:F fert_slope_hi:F "
    "alluvial_bonus:F alluvial_hand_m:F timber_slope_lo:F timber_slope_hi:F fish_river_class_min:I "
    "fish_weight:F fish_reach_m:I ore_noise:W ore_vein_lo:F ore_vein_hi:F ore_elev_lo_m:I ore_elev_hi_m:I "
    "ore_lowland:F ore_slope_base:F ore_slope_lo:F ore_slope_hi:F walk_slope_max:F walk_min_area_m2:I "
    "hab_slope_lo:F hab_slope_hi:F hab_weight_fertility:I hab_weight_forage:I hab_weight_timber:I "
    "hab_weight_ore:I hab_water_min:F hab_flood_hand_m:F hab_flood_factor:F hab_pocket_factor:F")
assert len(RESOURCES) == 34

# P1-06 4 with record region_names (eleven words) and upland_own_noun.
REGION_NAMES = REC(fields(
    "central:S north:S northeast:S east:S southeast:S south:S southwest:S west:S northwest:S isle:S upland:S"))
REGIONS = fields(
    "seed_tile_m:I seed_spacing_m:I hand_ref_m:I cost_noise:W cost_noise_amp:F lake_cost_factor:F "
    "river_border_class:I river_cost_m:I region_min_area_m2:I ore_rich_min:I ore_reach_m:I "
    "name_central_frac:F") + (("names", REGION_NAMES), ("upland_own_noun", ARR(STR)))
# P1-06 r3 4 (generator 0.17.0, P3-08): records name_pools and name_traits
# after upland_own_noun, fields in their stated order.
NAME_POOLS = REC(tuple((n, ARR(INT)) for n in
                       ("nouns", "suffixes", "prefixes", "adjectives", "features", "possessors", "places", "objects")))
NAME_TRAITS = REC(fields("coast_cells:I river_class:I lake_permille:I high_mean_m:I ore_rich_cells:I "
                         "barren_hab_max:I fertile_min:I"))
REGIONS_017 = REGIONS + (("name_pool_weights", NAME_POOLS), ("name_traits", NAME_TRAITS))
# The shipped values of P1-06 4's block (r3), standing in for a regions.toml
# of 0.16.0 that does not have them yet.
REGIONS_017_VALUES = {
    "name_pool_weights": dict(nouns=[0, 1, 0], suffixes=[0, 4, 1], prefixes=[6, 3, 2], adjectives=[6, 3, 2],
                              features=[5, 1, 2], possessors=[8, 1, 1], places=[8, 1, 1], objects=[6, 3, 2]),
    "name_traits": dict(coast_cells=128, river_class=7, lake_permille=125, high_mean_m=400, ore_rich_cells=1000,
                        barren_hab_max=20, fertile_min=200),
}

# P3-02 3: record culture_naming.
CULTURE_NAMING = REC((
    ("onsets", ARR(STR)), ("nuclei", ARR(STR)), ("codas", ARR(STR)), ("patterns", ARR(STR)),
    ("min_letters", INT), ("max_letters", INT), ("forbidden", ARR(STR)), ("god_forms", ARR(STR)),
    ("civilization_forms", ARR(STR)), ("settlement_forms", ARR(STR)),
))
# P3-02 r5 11.4 (0.17.0): `god_patterns` directly after `patterns`.
CULTURE_NAMING_017 = REC(CULTURE_NAMING[1][:4] + (("god_patterns", ARR(STR)),) + CULTURE_NAMING[1][4:])

# P3-01 4.
CULTURE = (("race", STR), ("name", STR), ("naming", CULTURE_NAMING))
CULTURE_017 = (("race", STR), ("name", STR), ("naming", CULTURE_NAMING_017))

# P3-01 5.1.
CHRONOLOGY = (("awakening_jitter_frac", FX), ("awakening_min_gap_years", INT), ("age_name_form", STR))

# P3-04 4.1: history.engine, fields in table order; record engine_detail in
# its stated order (active_years, flare_years, final_years, flare_threshold).
ENGINE_DETAIL = REC(fields("active_years:I flare_years:I final_years:I flare_threshold:I"))
ENGINE = fields("quiet_step_years:I quiet_events_max:I simulated_min_permille:I simulated_max_permille:I "
                "islet_max_dry_cells:I") + (("sparse", ENGINE_DETAIL), ("normal", ENGINE_DETAIL),
                                            ("dense", ENGINE_DETAIL))

# P3-04 4.2: history.report, one field `events`, an array of record
# report_event (kind string, salience int, template string).
REPORT_EVENT = REC(fields("kind:S salience:I template:S"))
REPORT = (("events", ARR(REPORT_EVENT)),)

# P3-05 12.1: history.peoples, fields in table order (= the listing's order);
# record abandon_permille and plague_ppm {hamlet, village, town, city,
# fortress}, record intensity {calm, normal, turbulent}. The ranges are the
# table's (PEOPLES_RANGES, for the tests of 16.1 item 15).
PEOPLES_BY_KIND = REC(fields("hamlet:I village:I town:I city:I fortress:I"))
PEOPLES_INTENSITY = REC(fields("calm:I normal:I turbulent:I"))
PEOPLES_RANGES = (
    ("score_site", 0, 2000), ("score_biome", 0, 2000), ("score_upland", 0, 2000), ("score_coast", 0, 2000),
    ("ore_reach_full", 1, 100000), ("coast_full", 1, 100000), ("rough_base", 0, 1000), ("rough_upland", 0, 1000),
    ("people_per_kcell", 1, 100000), ("capacity_floor", 1, 100000), ("capital_weight", 1000, 10000),
    ("founding_population", 1, 10000), ("village_min", 1, 1000000), ("town_min", 1, 1000000),
    ("city_min", 1, 1000000), ("city_max", 1, 1000000), ("fortress_max", 1, 1000000),
    ("kind_shrink_permille", 0, 1000), ("site_spacing", 1, 1000), ("region_slots_max", 1, 64),
    ("mouth_class_min", 1, 15), ("port_radius", 0, 16), ("port_mouth_bonus", 0, 255), ("port_coast_bonus", 0, 255),
    ("decline_permille", 1, 1000), ("recover_permille", 0, 1000), ("colonist_permille", 1, 1000),
    ("colonists_min", 1, 10000), ("abandon_min", 1, 10000), ("abandon_frail_max", 1, 1000000),
    ("abandon_permille", 0, 1000), ("found_pressure_permille", 1, 2000), ("found_rate_permille", 0, 1000),
    ("quiet_found_permille", 0, 1000), ("reach_base", 0, 64), ("reach_per_expansion", 0, 64),
    ("expand_min_score", -3000, 4000), ("expand_relative_permille", 0, 1000), ("expand_min_capacity", 0, 1000000),
    ("cost_penalty", 0, 10000), ("pass_cost", 1, 100), ("river_cost", 0, 100),
    ("river_border_share_permille", 0, 1000), ("civs_two_permille", 0, 1000), ("regions_per_civ", 0, 100),
    ("home_min_hops", 1, 64), ("home_min_capacity", 0, 1000000), ("intensity", 0, 10000),
    ("famine_crowding_permille", 0, 2000), ("famine_ppm", 0, 1000000), ("famine_toll_min", 0, 1000),
    ("famine_toll_max", 0, 1000), ("plague_ppm", 0, 1000000), ("plague_toll_min", 0, 1000),
    ("plague_toll_max", 0, 1000), ("plague_spread_permille", 0, 1000), ("plague_spread_toll_permille", 0, 1000),
    ("crisis_per_generation_permille", 0, 1000), ("crisis_loss_permille", 0, 1000),
    ("split_min_settlements", 2, 1000), ("split_min_regions", 2, 1000), ("split_permille", 0, 1000),
    ("ruin_fade_years", 1, 100000), ("colony_new_region_salience", 0, 100), ("plague_small_salience", 0, 100),
)
# P3-05 r6 12.1 (0.17.0): two fields after plague_small_salience.
PEOPLES_RANGES_016 = PEOPLES_RANGES
PEOPLES_RANGES_017 = PEOPLES_RANGES + (("plague_major_dead", 0, 1000000), ("crisis_small_salience", 0, 100))
PEOPLES_RECORDS = {"abandon_permille": PEOPLES_BY_KIND, "plague_ppm": PEOPLES_BY_KIND,
                   "intensity": PEOPLES_INTENSITY}
PEOPLES = tuple((n, PEOPLES_RECORDS.get(n, INT)) for n, _, _ in PEOPLES_RANGES)
assert len(PEOPLES) == 65
PEOPLES_017 = tuple((n, PEOPLES_RECORDS.get(n, INT)) for n, _, _ in PEOPLES_RANGES_017)
assert len(PEOPLES_017) == 67
# P3-05 r8 21 item 5 (0.18.0, P3-09): two fields after regions_per_civ.
_i = [n for n, _, _ in PEOPLES_RANGES_017].index("regions_per_civ") + 1
PEOPLES_RANGES_018 = PEOPLES_RANGES_017[:_i] + (("regions_per_extra_civ", 1, 100000), ("civs_extra_max", 0, 3)) + \
    PEOPLES_RANGES_017[_i:]
PEOPLES_018 = tuple((n, PEOPLES_RECORDS.get(n, INT)) for n, _, _ in PEOPLES_RANGES_018)
assert len(PEOPLES_018) == 69 and PEOPLES_018[_i - 1][0] == "regions_per_civ"
# P3-05 12.1 named checks, in order: (name, message).
PEOPLES_CHECKS = (
    ("peoples_ladder", "village_min, town_min, city_min must increase and city_max must not be below city_min"),
    ("peoples_tolls", "a toll minimum must not be above its maximum"),
    ("peoples_floor", "capacity_floor must not be below founding_population"),
    ("peoples_colonists", "colonists_min must be above abandon_min"),
)


def peoples_check_messages(t):
    """The named checks of P3-05 12.1 that fail for a history.peoples table."""
    out = []
    if not (t["village_min"] < t["town_min"] < t["city_min"] <= t["city_max"]):
        out.append(PEOPLES_CHECKS[0][1])
    if not (t["famine_toll_min"] <= t["famine_toll_max"] and t["plague_toll_min"] <= t["plague_toll_max"]):
        out.append(PEOPLES_CHECKS[1][1])
    if not (t["capacity_floor"] >= t["founding_population"]):
        out.append(PEOPLES_CHECKS[2][1])
    if not (t["abandon_min"] < t["colonists_min"]):
        out.append(PEOPLES_CHECKS[3][1])
    return out

# P3-06 12.1 (generator 0.16.0): history.conflict, fields in table order (=
# the listing's order); records camp {good_good, good_neutral, good_evil,
# neutral_neutral, neutral_evil, evil_evil}, levy_permille and walls_permille
# {hamlet, village, town, city, fortress}. CONFLICT_RANGES are the table's.
CONFLICT_CAMP = REC(fields("good_good:I good_neutral:I good_evil:I neutral_neutral:I neutral_evil:I evil_evil:I"))
CONFLICT_RANGES = (
    ("camp", -1000, 1000), ("enmity_max", -1000, 1000), ("border_friction", 0, 2000), ("want_weight", 0, 2000),
    ("hunger_full", 1, 100000), ("fear_share", 0, 999), ("fear_weight", 0, 2000),
    ("tick_strain_permille", 0, 1000), ("quiet_strain_permille", 0, 1000), ("strain_max", 1, 1000000),
    ("claim_strain", 1, 1000000), ("declare_min", 0, 1000000), ("declare_span", 1, 1000000),
    ("declare_permille", 0, 1000), ("declare_ratio", 1, 10000), ("shelter_years", 0, 1000), ("march_hops", 1, 4),
    ("cost_penalty", 0, 10000), ("levy_permille", 0, 1000), ("terrain_defense", 0, 10000),
    ("pass_defense", 0, 10000), ("river_defense", 0, 10000), ("fort_defense", 0, 10000),
    ("battle_permille", 0, 1000), ("loss_loser_permille", 0, 1000), ("loss_winner_permille", 0, 1000),
    ("garrison_permille", 0, 1000), ("walls_permille", 1, 100000), ("capital_walls", 0, 100000),
    ("sack_base", 0, 1000), ("sack_hostility", 0, 2000), ("sack_toll_permille", 0, 1000),
    ("capture_toll_permille", 0, 1000), ("exhaustion_permille", 1, 1000), ("war_max_years", 1, 1000),
    ("truce_years", 0, 10000), ("war_grudge", 0, 2000), ("capture_grudge", 0, 2000), ("secession_grudge", 0, 2000),
    ("grudge_decay_per_century", 0, 10000), ("last_refuge", 0, 1), ("capital_sacked_salience", 0, 100),
    ("capital_captured_salience", 0, 100),
)
CONFLICT_RECORDS = {"camp": CONFLICT_CAMP, "levy_permille": PEOPLES_BY_KIND, "walls_permille": PEOPLES_BY_KIND}
CONFLICT = tuple((n, CONFLICT_RECORDS.get(n, INT)) for n, _, _ in CONFLICT_RANGES)
assert len(CONFLICT) == 43
# P3-06 12.1 named checks, in order: (name, message).
CONFLICT_CHECKS = (
    ("conflict_declare", "claim_strain must not be above declare_min"),
    ("conflict_strain_max", "declare_min + declare_span must not be above strain_max"),
)


CONFLICT_RANGES_016 = CONFLICT_RANGES
# P3-06 r9 20.9 (generator 0.18.0, P3-09): eleven fields after last_refuge, in this order.
CONFLICT_NEW_018 = (
    ("refuge_flight", 0, 1), ("flight_hops", 1, 8), ("hidden_years", 0, 10000), ("weary_wars", 0, 100),
    ("tribute_permille", 0, 1000), ("revenge_aggression", 0, 1000), ("levy_hops", 0, 16), ("odds_floor", 0, 1000),
    ("odds_min", 0, 1000), ("truce_break_aggression", 0, 999), ("enmity_hops", 1, 4),
    # r10 20.19 (P3-09 r3): after enmity_hops
    ("kill_max_permille", 1, 10000), ("find_permille", 0, 1000), ("search_years", 1, 100),
)
_i = [n for n, _, _ in CONFLICT_RANGES].index("last_refuge") + 1
CONFLICT_RANGES_018 = CONFLICT_RANGES[:_i] + CONFLICT_NEW_018 + CONFLICT_RANGES[_i:]
CONFLICT_018 = tuple((n, CONFLICT_RECORDS.get(n, INT)) for n, _, _ in CONFLICT_RANGES_018)
assert len(CONFLICT_018) == 57 and CONFLICT_018[_i][0] == "refuge_flight"
CONFLICT_CHECKS_018 = CONFLICT_CHECKS + (("conflict_enmity_hops", "enmity_hops must not be below march_hops"),)


def conflict_ranges():
    """The history.conflict field ranges of the served data version."""
    return CONFLICT_RANGES_018 if DATA_VERSION == "0.18.0" else CONFLICT_RANGES_016


def conflict_check_messages(t):
    """The named checks of P3-06 12.1 (and r9 20.9 from 0.18.0) that fail for a history.conflict table."""
    out = []
    if not (t["claim_strain"] <= t["declare_min"]):
        out.append(CONFLICT_CHECKS[0][1])
    if not (t["declare_min"] + t["declare_span"] <= t["strain_max"]):
        out.append(CONFLICT_CHECKS[1][1])
    if "enmity_hops" in t and not (t["enmity_hops"] >= t["march_hops"]):
        out.append(CONFLICT_CHECKS_018[2][1])
    return out

# P3-01 3.
RACE_SITE_WEIGHTS = REC(fields("fertility:I forage:I timber:I ore:I fresh_water:I"))
RACE_BIOME = REC(fields("biome:S weight:I"))
RACE = (
    ("order", INT), ("name", STR), ("plural", STR), ("adjective", STR), ("camp", STR), ("awakening_share", FX),
    ("lifespan_years", INT), ("generation_years", INT), ("growth_permille", INT),
    ("density_permille", INT),  # P3-05 12.2 item 1 / P3-01 r6: after growth_permille
    ("site_weights", RACE_SITE_WEIGHTS), ("biomes", ARR(RACE_BIOME)), ("upland", INT), ("coast", INT),
    ("expansion", INT), ("aggression", INT),
    ("prowess_permille", INT),  # P3-06 12.2 item 1 / P3-01 r7 (generator 0.16.0): after aggression
    ("settlement_kinds", ARR(STR)), ("culture", STR),
)
# P3-01 r8 (1) (0.17.0): record race_cell_weights {ore, height} after site_weights.
RACE_CELL_WEIGHTS = REC(fields("ore:I height:I"))
RACE_017 = RACE[:11] + (("cell_weights", RACE_CELL_WEIGHTS),) + RACE[11:]
assert RACE_017[10][0] == "site_weights" and RACE_017[12][0] == "biomes"
# P3-01 r9 3.2 (0.18.0, P3-09): three fields after prowess_permille, in this order.
_i = [n for n, _ in RACE_017].index("prowess_permille") + 1
RACE_018 = RACE_017[:_i] + (("muster_permille", INT), ("conceal_min", INT), ("home_defense_permille", INT)) + \
    RACE_017[_i:]
RACE_FIELDS_018 = (("muster_permille", 1, 10000), ("conceal_min", 0, 1001), ("home_defense_permille", 0, 10000))
assert RACE_018[_i + 3][0] == "settlement_kinds"

# P3-02 r5 11.2 (0.17.0): kind name_filter, three lists.
NAME_FILTER = (("anywhere", ARR(STR)), ("at_start", ARR(STR)), ("whole", ARR(STR)))
# P1-06 r3 4.1 (0.17.0): kind placename, record place_form {form, weight}.
PLACE_FORM = REC(fields("form:S weight:I"))
PLACENAME = (("role", STR), ("biomes", ARR(STR)), ("special", STR), ("trait", STR), ("forms", ARR(PLACE_FORM))) + tuple(
    (n, ARR(STR)) for n in ("nouns", "suffixes", "prefixes", "adjectives", "features", "possessors", "places",
                            "objects", "forbidden", "excluded"))

# Kinds: name -> (directory, schema). A schema is a tuple of fields (open
# kind) or a dict id -> fields (fixed-id kind).
KINDS = {
    "biome": ("biomes", BIOME),
    "culture": ("cultures", CULTURE),
    "history": ("history", {"chronology": CHRONOLOGY, "conflict": CONFLICT, "engine": ENGINE,
                             "peoples": PEOPLES, "report": REPORT}),
    "race": ("races", RACE),
    "worldgen": ("worldgen", {
        "biomes": BIOMES_TUNING, "climate": CLIMATE, "elevation": ELEVATION, "hydrology": HYDROLOGY,
        "regions": REGIONS, "resources": RESOURCES,
    }),
}
KINDS_016 = KINDS
KINDS_017 = {
    "biome": ("biomes", BIOME),
    "culture": ("cultures", CULTURE_017),
    "history": ("history", {"chronology": CHRONOLOGY, "conflict": CONFLICT, "engine": ENGINE,
                             "peoples": PEOPLES_017, "report": REPORT}),
    "name_filter": ("name_filters", NAME_FILTER),
    "placename": ("placenames", PLACENAME),
    "race": ("races", RACE_017),
    "worldgen": ("worldgen", {
        "biomes": BIOMES_TUNING, "climate": CLIMATE, "elevation": ELEVATION, "hydrology": HYDROLOGY,
        "regions": REGIONS_017, "resources": RESOURCES,
    }),
}
KINDS_018 = dict(KINDS_017)
KINDS_018["history"] = ("history", {"chronology": CHRONOLOGY, "conflict": CONFLICT_018, "engine": ENGINE,
                                    "peoples": PEOPLES_018, "report": REPORT})
KINDS_018["race"] = ("races", RACE_018)
NEW_KINDS = ("culture", "history", "race")
# Kinds whose shipped files are normative and committed (P1-06 r3 4.1,
# P3-02 r5 11.2): the fixture roots get identical copies with the
# implementation; game/data's files stand in for a root without them.
COPIED_KINDS = ("name_filter", "placename")

# ---------------------------------------------------------------------------
# Spec listings (P3-01 3.1, 4, 5.1; P3-02 5), transcribed
# ---------------------------------------------------------------------------

RACE_FILES = {
    "elf.toml": """[race.elf]
order = 1
name = "Elf"
plural = "Elves"
adjective = "Elven"
camp = "good"
awakening_share = 0
lifespan_years = 700
generation_years = 150
growth_permille = 3
density_permille = 300
site_weights = { fertility = 20, forage = 35, timber = 35, ore = 0, fresh_water = 10 }
biomes = [
  { biome = "temperate_forest", weight = 500 },
  { biome = "wet_woodland", weight = 300 },
  { biome = "grassland", weight = 0 },
  { biome = "heath", weight = -200 },
  { biome = "steppe", weight = -300 },
  { biome = "highland_moor", weight = -300 },
  { biome = "alpine", weight = -600 },
]
upland = -300
coast = 0
expansion = 200
aggression = 150
prowess_permille = 1600
settlement_kinds = ["hamlet", "village", "town", "city"]
culture = "elven"
""",
    "goblin.toml": """[race.goblin]
order = 2
name = "Goblin"
plural = "Goblins"
adjective = "Goblin"
camp = "evil"
awakening_share = "0.35"
lifespan_years = 40
generation_years = 15
growth_permille = 20
density_permille = 900
site_weights = { fertility = 40, forage = 20, timber = 10, ore = 30, fresh_water = 0 }
biomes = [
  { biome = "heath", weight = 200 },
  { biome = "highland_moor", weight = 200 },
  { biome = "steppe", weight = 150 },
  { biome = "grassland", weight = 100 },
  { biome = "marsh", weight = 100 },
  { biome = "alpine", weight = -400 },
  { biome = "temperate_forest", weight = -200 },
  { biome = "wet_woodland", weight = -300 },
]
upland = 100
coast = -100
expansion = 600
aggression = 600
prowess_permille = 800
settlement_kinds = ["hamlet", "village", "town", "fortress"]
culture = "goblin"
""",
    "dwarf.toml": """[race.dwarf]
order = 3
name = "Dwarf"
plural = "Dwarves"
adjective = "Dwarven"
camp = "neutral"
awakening_share = "0.55"
lifespan_years = 300
generation_years = 70
growth_permille = 5
density_permille = 700
site_weights = { fertility = 10, forage = 10, timber = 10, ore = 60, fresh_water = 10 }
biomes = [
  { biome = "alpine", weight = 300 },
  { biome = "highland_moor", weight = 300 },
  { biome = "heath", weight = 200 },
  { biome = "temperate_forest", weight = -100 },
  { biome = "wet_woodland", weight = -300 },
  { biome = "marsh", weight = -400 },
  { biome = "fen", weight = -400 },
]
upland = 800
coast = -200
expansion = 250
aggression = 300
prowess_permille = 1400
settlement_kinds = ["hamlet", "village", "town", "city", "fortress"]
culture = "dwarven"
""",
    "faerie.toml": """[race.faerie]
order = 4
name = "Faerie"
plural = "Faeries"
adjective = "Faerie"
camp = "good"
awakening_share = "0.70"
lifespan_years = 400
generation_years = 100
growth_permille = 4
density_permille = 250
site_weights = { fertility = 0, forage = 60, timber = 20, ore = 0, fresh_water = 20 }
biomes = [
  { biome = "fen", weight = 800 },
  { biome = "wet_woodland", weight = 800 },
  { biome = "marsh", weight = 600 },
  { biome = "temperate_forest", weight = 100 },
  { biome = "highland_moor", weight = 300 },
  { biome = "grassland", weight = -200 },
  { biome = "alpine", weight = -300 },
  { biome = "steppe", weight = -400 },
  { biome = "heath", weight = 100 },
]
upland = 200
coast = 0
expansion = 100
aggression = 50
prowess_permille = 1200
settlement_kinds = ["hamlet", "village"]
culture = "faerie"
""",
    "human.toml": """[race.human]
order = 5
name = "Human"
plural = "Humans"
adjective = "Human"
camp = "neutral"
awakening_share = "0.83"
lifespan_years = 70
generation_years = 25
growth_permille = 15
density_permille = 1000
site_weights = { fertility = 60, forage = 10, timber = 10, ore = 5, fresh_water = 15 }
biomes = [
  { biome = "grassland", weight = 300 },
  { biome = "steppe", weight = 100 },
  { biome = "temperate_forest", weight = 100 },
  { biome = "marsh", weight = -100 },
  { biome = "heath", weight = -100 },
  { biome = "fen", weight = -200 },
  { biome = "highland_moor", weight = -200 },
  { biome = "alpine", weight = -500 },
]
upland = -300
coast = 200
expansion = 700
aggression = 400
prowess_permille = 1000
settlement_kinds = ["hamlet", "village", "town", "city", "fortress"]
culture = "human"
""",
    "orc.toml": """[race.orc]
order = 6
name = "Orc"
plural = "Orcs"
adjective = "Orcish"
camp = "evil"
awakening_share = "0.93"
lifespan_years = 50
generation_years = 18
growth_permille = 18
density_permille = 800
site_weights = { fertility = 35, forage = 25, timber = 10, ore = 30, fresh_water = 0 }
biomes = [
  { biome = "steppe", weight = 400 },
  { biome = "heath", weight = 200 },
  { biome = "grassland", weight = 200 },
  { biome = "highland_moor", weight = 100 },
  { biome = "temperate_forest", weight = -100 },
  { biome = "marsh", weight = -100 },
  { biome = "fen", weight = -200 },
  { biome = "wet_woodland", weight = -200 },
  { biome = "alpine", weight = -300 },
]
upland = 100
coast = -100
expansion = 600
aggression = 900
prowess_permille = 1300
settlement_kinds = ["hamlet", "village", "town", "fortress"]
culture = "orcish"
""",
}

# P3-02 5, verbatim.
NAMING = {
    "dwarven": """onsets = ["d", "b", "th", "k", "g", "br", "dr", "gr", "t", "h", "kh", "n", "m", "thr"]
nuclei = ["a", "u", "o", "i", "ai", "ou"]
codas = ["r", "n", "m", "k", "d", "rn", "rd", "rk", "mm", "nd", "rin", "li", "din"]
patterns = ["ONC", "ONCONC", "ONCONC", "ONCON"]
min_letters = 4
max_letters = 10
forbidden = ["mmm", "rr", "dd", "kk", "rdt", "rkd", "rdd", "mmb", "kdr", "kthr", "dkh", "ndb", "rnk"]
god_forms = ["{root}"]
civilization_forms = ["The Hold of {root}", "Clan {root}", "The {root} Throne"]
settlement_forms = ["{root}", "{root} Deep", "{root} Hold", "{root}"]
""",
    "elven": """onsets = ["", "l", "th", "s", "f", "n", "m", "r", "v", "gl", "c", "el", "ith"]
nuclei = ["a", "e", "i", "ae", "ie", "o"]
codas = ["", "", "l", "n", "th", "s", "r", "nd", "ll", "nn"]
patterns = ["ONONC", "ONONC", "ONCON", "ONONON", "NCONC"]
min_letters = 4
max_letters = 11
forbidden = ["aea", "iei", "iae", "eae", "aa", "ii", "ee", "thth", "lll", "nnn", "ea", "ia"]
god_forms = ["{root}"]
civilization_forms = ["The Court of {root}", "The Realm of {root}", "{root}"]
settlement_forms = ["{root}", "{root}", "{root}", "{root}dell"]
""",
    "faerie": """onsets = ["", "p", "f", "w", "l", "s", "t", "b", "sh", "wh", "fl", "tw", "m"]
nuclei = ["i", "ee", "ie", "ia", "a", "y", "ae", "ou", "o"]
codas = ["", "", "ll", "n", "sh", "th", "p", "sk", "ss"]
patterns = ["ONON", "ONONON", "ONC", "ONONC", "ONCON"]
min_letters = 3
max_letters = 10
forbidden = ["eee", "iee", "yy", "ouou", "shsh"]
god_forms = ["{root}"]
civilization_forms = ["The {root} Ring", "The Circle of {root}", "The {root} Court"]
settlement_forms = ["{root}", "{root} Glade", "{root} Hollow", "{root}"]
""",
    "goblin": """onsets = ["g", "gr", "sn", "k", "kr", "z", "sk", "b", "n", "gn", "dr", "sl", "skr"]
nuclei = ["a", "i", "u", "o", "ee"]
codas = ["k", "g", "b", "z", "t", "x", "nk", "rk", "gg", "zz", "tch"]
patterns = ["ONC", "ONC", "ONCONC", "ONC-ONC", "ONONC"]
min_letters = 3
max_letters = 10
forbidden = ["eee", "ggg", "zzz", "xg", "xk"]
god_forms = ["{root}"]
civilization_forms = ["The {root} Horde", "The Warrens of {root}", "The {root} Mob"]
settlement_forms = ["{root}", "{root}", "{root} Pit", "{root} Hole"]
""",
    "human": """onsets = ["b", "c", "d", "g", "h", "m", "r", "s", "t", "w", "st", "br", "al", "ed", "ash"]
nuclei = ["a", "e", "i", "o", "u", "ea"]
codas = ["n", "d", "r", "s", "l", "rd", "ck", "ld", "m", "nt"]
patterns = ["ONC", "ONC", "ONCONC", "ONC"]
min_letters = 4
max_letters = 9
forbidden = ["eae", "aea", "ckc", "ldl", "ntd", "rdd", "ckb", "ckm"]
god_forms = ["{root}"]
civilization_forms = ["The Kingdom of {root}", "The {root} League", "The Duchy of {root}"]
settlement_forms = ["{root}", "{root}ford", "{root}ton", "{root}bury", "{root}wick", "{root}ham", "{root}stead"]
""",
    "orcish": """onsets = ["gh", "gr", "ug", "m", "k", "z", "sh", "b", "d", "th", "kr", "ur", "az"]
nuclei = ["a", "u", "o", "aa", "uu"]
codas = ["g", "sh", "k", "rz", "z", "gh", "rk", "l", "m", "", "kh", "ld"]
patterns = ["ONC", "ONC", "ONC'ONC", "ONCONC", "NC'ONC"]
min_letters = 3
max_letters = 10
forbidden = ["aaa", "uuu", "ghgh", "shsh"]
god_forms = ["{root}"]
civilization_forms = ["The {root} Horde", "Clan {root}", "The Warband of {root}"]
settlement_forms = ["{root}", "{root} Camp", "{root}", "{root} Fort"]
""",
}

# P3-01 4: (race, name) per culture id.
CULTURE_RACE_NAME = {
    "dwarven": ("dwarf", "Dwarven"), "elven": ("elf", "Elven"), "faerie": ("faerie", "Faerie"),
    "goblin": ("goblin", "Goblin"), "human": ("human", "Human"), "orcish": ("orc", "Orcish"),
}

CULTURE_FILES = {
    cid + ".toml": '[culture.%s]\nrace = "%s"\nname = "%s"\n\n[culture.%s.naming]\n%s'
    % (cid, race, name, cid, NAMING[cid])
    for cid, (race, name) in CULTURE_RACE_NAME.items()
}

CHRONOLOGY_FILE = {
    "chronology.toml": """[history.chronology]
awakening_jitter_frac = "0.01"
awakening_min_gap_years = 100
age_name_form = "The Age of {plural}"
""",
}

# P3-04 4.1 and 4.2, the two listings.
ENGINE_FILE = {
    "engine.toml": """[history.engine]
quiet_step_years = 25
quiet_events_max = 3
simulated_min_permille = 100
simulated_max_permille = 450
islet_max_dry_cells = 1172
sparse = { active_years = 75, flare_years = 30, final_years = 250, flare_threshold = 1500 }
normal = { active_years = 150, flare_years = 50, final_years = 300, flare_threshold = 1000 }
dense = { active_years = 250, flare_years = 75, final_years = 400, flare_threshold = 700 }
""",
}
REPORT_FILE = {
    "report.toml": """[history.report]
events = [
  { kind = "awakening", salience = 100, template = "{creator} awakened the {race:plural} in {@region}." },
  { kind = "civ_founded", salience = 80, template = "The {race:plural} founded {civilization} in {@region}." },
  { kind = "settlement_founded", salience = 40, template = "{founder} founded {settlement} in {@region}." },
  { kind = "quiet_era", salience = 20, template = "{civilization} passed {years} quiet {years?year|years} \
({generations} {generations?generation|generations}), from {@year} to {@end_year}." },
  { kind = "flare_up", salience = 50, template = "In {@year} tensions flared into {years} {years?year|years} \
of upheaval." },
  { kind = "colony_founded", salience = 15, template = "{founder} sent settlers from {origin} to found {settlement} in {@region}." },
  { kind = "became_village", salience = 10, template = "{settlement} grew into a village of {population}." },
  { kind = "became_town", salience = 18, template = "{settlement} grew into a town of {population}." },
  { kind = "became_city", salience = 45, template = "{settlement} grew into a city of {population}." },
  { kind = "settlement_dwindled", salience = 15, template = "{settlement} dwindled to {population} souls." },
  { kind = "famine", salience = 25, template = "Famine struck {settlement}; {dead} died." },
  { kind = "plague", salience = 45, template = "Plague broke out in {settlement}; {dead} died." },
  { kind = "plague_spread", salience = 25, template = "The plague spread to {settlement}; {dead} died." },
  { kind = "succession_crisis", salience = 40, template = "A succession crisis shook {civilization}." },
  { kind = "civ_seceded", salience = 70, template = "{civilization} broke away from {from}, taking {settlements} {settlements?settlement|settlements} with it." },
  { kind = "settlement_abandoned", salience = 55, template = "{settlement} was abandoned; {survivors} {survivors?survivor|survivors} left." },
  { kind = "capital_moved", salience = 50, template = "{civilization} moved its seat to {settlement}." },
  { kind = "civ_ended", salience = 85, template = "{civilization} came to an end." },
  { kind = "ruin_faded", salience = 10, template = "The ruins of {settlement} faded from memory." },
  { kind = "claim", salience = 45, template = "{claimant} laid claim to {@region}, held by {holder}." },
  { kind = "war_declared", salience = 70, template = "{attacker} declared war on {defender} over {@region}." },
  { kind = "battle", salience = 30, template = "{winner} defeated {loser} in {@region}; {loser_dead} of the defeated and {winner_dead} of the victors fell." },
  { kind = "battle_at_pass", salience = 35, template = "{winner} defeated {loser} at the pass into {@region}; {loser_dead} of the defeated and {winner_dead} of the victors fell." },
  { kind = "siege", salience = 25, template = "{attacker} besieged {@settlement} of {defender}; it {fell?fell|held}." },
  { kind = "sacked", salience = 75, template = "{attacker} sacked {@settlement} of {defender}; {dead} died and {survivors} fled." },
  { kind = "settlement_captured", salience = 60, template = "{attacker} took {@settlement} from {defender}; {dead} died and {fled} fled." },
  { kind = "peace", salience = 60, template = "{loser} sued {winner} for peace after {years} {years?year|years} of war." },
  { kind = "truce", salience = 50, template = "{parties:all} made peace after {years} {years?year|years} of war, neither the victor." },
  { kind = "region_ceded", salience = 55, template = "{from} ceded {@region} to {to}." },
  { kind = "civ_conquered", salience = 90, template = "{civilization} fell to {conqueror}." },
]
""",
}
# P3-05 12.1, the listing, verbatim from the spec.
PEOPLES_FILE = {
    "peoples.toml": """[history.peoples]
score_site = 1000
score_biome = 1000
score_upland = 1000
score_coast = 1000
ore_reach_full = 200
coast_full = 200
rough_base = 150
rough_upland = 600
people_per_kcell = 2500
capacity_floor = 400
capital_weight = 3000
founding_population = 300
village_min = 250
town_min = 1500
city_min = 8000
city_max = 25000
fortress_max = 6000
kind_shrink_permille = 800
site_spacing = 32
region_slots_max = 4
mouth_class_min = 4
port_radius = 3
port_mouth_bonus = 60
port_coast_bonus = 20
decline_permille = 100
recover_permille = 900
colonist_permille = 120
colonists_min = 60
abandon_min = 40
abandon_frail_max = 1200
abandon_permille = { hamlet = 300, village = 250, town = 50, city = 0, fortress = 0 }
found_pressure_permille = 750
found_rate_permille = 100
quiet_found_permille = 500
reach_base = 1
reach_per_expansion = 3
expand_min_score = 250
expand_relative_permille = 700
expand_min_capacity = 800
cost_penalty = 100
pass_cost = 3
river_cost = 2
river_border_share_permille = 500
civs_two_permille = 500
regions_per_civ = 4
home_min_hops = 3
home_min_capacity = 1500
intensity = { calm = 500, normal = 1000, turbulent = 2000 }
famine_crowding_permille = 900
famine_ppm = 2500
famine_toll_min = 50
famine_toll_max = 200
plague_ppm = { hamlet = 300, village = 600, town = 1500, city = 3000, fortress = 600 }
plague_toll_min = 100
plague_toll_max = 300
plague_spread_permille = 200
plague_spread_toll_permille = 500
crisis_per_generation_permille = 60
crisis_loss_permille = 50
split_min_settlements = 6
split_min_regions = 3
split_permille = 300
ruin_fade_years = 1500
colony_new_region_salience = 30
plague_small_salience = 25
""",
}
# P3-06 12.1, the listing, verbatim from the spec (generator 0.16.0).
CONFLICT_FILE = {
    "conflict.toml": """[history.conflict]
camp = { good_good = 400, good_neutral = -150, good_evil = -700, neutral_neutral = 100, neutral_evil = -250, evil_evil = 300 }
enmity_max = -500
border_friction = 200
want_weight = 500
hunger_full = 300
fear_share = 400
fear_weight = 1000
tick_strain_permille = 25
quiet_strain_permille = 6
strain_max = 5000
claim_strain = 300
declare_min = 300
declare_span = 600
declare_permille = 150
declare_ratio = 1000
shelter_years = 150
march_hops = 2
cost_penalty = 100
levy_permille = { hamlet = 100, village = 100, town = 100, city = 80, fortress = 250 }
terrain_defense = 1000
pass_defense = 1000
river_defense = 500
fort_defense = 500
battle_permille = 300
loss_loser_permille = 250
loss_winner_permille = 80
garrison_permille = 300
walls_permille = { hamlet = 1000, village = 1500, town = 2500, city = 4000, fortress = 6000 }
capital_walls = 2000
sack_base = 200
sack_hostility = 1000
sack_toll_permille = 300
capture_toll_permille = 100
exhaustion_permille = 80
war_max_years = 40
truce_years = 200
war_grudge = 150
capture_grudge = 50
secession_grudge = 300
grudge_decay_per_century = 50
last_refuge = 1
capital_sacked_salience = 90
capital_captured_salience = 80
""",
}
# P3-05 11: the fourteen report entries of kinds 6-19 (salience and
# template of the table; `\|` there is a plain `|`). ASSUMPTION (spec gap):
# the spec gives the entries but not their place in report.toml; they are
# appended after the five P3-04 entries in kind id order. The same for the
# eleven entries of kinds 20-30 of P3-06 11 (0.16.0): appended after
# ruin_faded in kind id order.
HISTORY_FILES = dict(CHRONOLOGY_FILE, **CONFLICT_FILE, **ENGINE_FILE, **PEOPLES_FILE, **REPORT_FILE)

SPEC_FILES = {"race": RACE_FILES, "culture": CULTURE_FILES, "history": HISTORY_FILES}
SPEC_FILES_016 = SPEC_FILES


# ---------------------------------------------------------------------------
# The 0.17.0 listings (P3-08), derived from the 0.16.0 ones by the edits the
# specs state, so that each edit is visible here:
#   races     P3-01 r8 (1): cell_weights after site_weights (dwarf 400/600,
#             every other race 0/0); r8 (2) and P3-05 r6 12.3: the faerie
#             biome weights of candidate A (nine rows, heath kept at 0).
#   cultures  P3-02 r5 11.4: god_patterns after patterns; the elven third
#             civilization form `The House of {root}`.
#   peoples   P3-05 r6 12.1: famine_ppm 1250, plague_spread_permille 100,
#             plague_major_dead 1000 and crisis_small_salience 25 after
#             plague_small_salience.
#   report    P3-04 r10 4.2 (quiet_era), P3-05 r6 11 (kinds 14, 16, 31),
#             P3-06 r7 11 (kind 26). ASSUMPTION (spec gap): kind 31's entry
#             is appended after civ_conquered (kind id order, as for the
#             earlier kinds); the specs give the entry, not its place.
# ---------------------------------------------------------------------------


def _edit(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


CELL_WEIGHTS_017 = {"dwarf": (400, 600)}
FAERIE_BIOMES_016 = """biomes = [
  { biome = "fen", weight = 800 },
  { biome = "wet_woodland", weight = 800 },
  { biome = "marsh", weight = 600 },
  { biome = "temperate_forest", weight = 100 },
  { biome = "highland_moor", weight = 300 },
  { biome = "grassland", weight = -200 },
  { biome = "alpine", weight = -300 },
  { biome = "steppe", weight = -400 },
  { biome = "heath", weight = 100 },
]
"""
FAERIE_BIOMES_017 = """biomes = [
  { biome = "temperate_forest", weight = 800 },
  { biome = "wet_woodland", weight = 800 },
  { biome = "fen", weight = 300 },
  { biome = "marsh", weight = 200 },
  { biome = "highland_moor", weight = 100 },
  { biome = "heath", weight = 0 },
  { biome = "grassland", weight = -300 },
  { biome = "alpine", weight = -400 },
  { biome = "steppe", weight = -400 },
]
"""


def _race_017(name, text):
    rid = name[:-5]
    ore, height = CELL_WEIGHTS_017.get(rid, (0, 0))
    line = [l for l in text.splitlines(True) if l.startswith("site_weights = ")][0]
    text = _edit(text, line, line + "cell_weights = { ore = %d, height = %d }\n" % (ore, height))
    if rid == "faerie":
        text = _edit(text, FAERIE_BIOMES_016, FAERIE_BIOMES_017)
    return text


RACE_FILES_017 = {n: _race_017(n, t) for n, t in RACE_FILES.items()}

# P3-02 r5 11.4, verbatim.
GOD_PATTERNS_017 = {
    "dwarven": '["ONCONC", "ONCONC", "ONCON"]',
    "elven": '["ONONC", "ONONC", "ONCON", "ONONON", "NCONC"]',
    "faerie": '["ONON", "ONONON", "ONONC", "ONCON"]',
    "goblin": '["ONCONC", "ONC-ONC", "ONONC"]',
    "human": '["ONCONC"]',
    "orcish": '["ONC\'ONC", "ONCONC", "NC\'ONC"]',
}


def _naming_017(cid, text):
    line = [l for l in text.splitlines(True) if l.startswith("patterns = ")][0]
    text = _edit(text, line, line + "god_patterns = %s\n" % GOD_PATTERNS_017[cid])
    if cid == "elven":
        text = _edit(text, 'civilization_forms = ["The Court of {root}", "The Realm of {root}", "{root}"]',
                     'civilization_forms = ["The Court of {root}", "The Realm of {root}", "The House of {root}"]')
    return text


NAMING_017 = {cid: _naming_017(cid, t) for cid, t in NAMING.items()}
CULTURE_FILES_017 = {
    cid + ".toml": '[culture.%s]\nrace = "%s"\nname = "%s"\n\n[culture.%s.naming]\n%s'
    % (cid, race, name, cid, NAMING_017[cid])
    for cid, (race, name) in CULTURE_RACE_NAME.items()
}

_pe = PEOPLES_FILE["peoples.toml"]
_pe = _edit(_pe, "famine_ppm = 2500\n", "famine_ppm = 1250\n")
_pe = _edit(_pe, "plague_spread_permille = 200\n", "plague_spread_permille = 100\n")
_pe = _edit(_pe, "plague_small_salience = 25\n",
            "plague_small_salience = 25\nplague_major_dead = 1000\ncrisis_small_salience = 25\n")
PEOPLES_FILE_017 = {"peoples.toml": _pe}

# The templates the 0.17.0 specs change (kind key -> (old, new)), and kind 31.
TEMPLATES_017 = {
    "quiet_era": ("{civilization} passed {years} quiet {years?year|years} "
                  "({generations} {generations?generation|generations}), from {@year} to {@end_year}.",
                  "{civilization} passed {years} quiet {years?year|years}, from {@year} to {@end_year}"
                  "{generations?, under one generation of rulers|, in which one new generation of rulers came to power"
                  "|, in which # new generations of rulers came to power}."),
    "succession_crisis": ("A succession crisis shook {civilization}.",
                          "A succession crisis at {@settlement} shook {civilization}."),
    "settlement_abandoned": ("{settlement} was abandoned; {survivors} {survivors?survivor|survivors} left.",
                             "{settlement} was abandoned; {survivors?no one survived|its one survivor was scattered"
                             "|its # survivors were scattered}."),
    "settlement_captured": ("{attacker} took {@settlement} from {defender}; {dead} died and {fled} fled.",
                            "{attacker} took {@settlement} from {defender}; {dead} died"
                            "{fled?| and one fled| and # fled}."),
}
KIND31_ENTRY = ('  { kind = "settlement_abandoned_to", salience = 55, template = "{settlement} was abandoned; '
                '{settled} {settled?survivor|survivors} went to {destination}'
                '{lost?|, and one was lost|, and # were lost}." },\n')
_re = REPORT_FILE["report.toml"]
for _k, (_old, _new) in TEMPLATES_017.items():
    _re = _edit(_re, _old, _new)
_last = [l for l in _re.splitlines(True) if '"civ_conquered"' in l][0]
_re = _edit(_re, _last, _last + KIND31_ENTRY)
REPORT_FILE_017 = {"report.toml": _re}
HISTORY_FILES_017 = dict(CHRONOLOGY_FILE, **CONFLICT_FILE, **ENGINE_FILE, **PEOPLES_FILE_017, **REPORT_FILE_017)
SPEC_FILES_017 = {"race": RACE_FILES_017, "culture": CULTURE_FILES_017, "history": HISTORY_FILES_017}


# ---------------------------------------------------------------------------
# The 0.18.0 listings (P3-09), derived from the 0.17.0 ones by the edits the
# specs state:
#   races     P3-01 r9 3.2 / P3-06 r9 20.12: muster_permille, conceal_min,
#             home_defense_permille after prowess_permille (orc muster 4000,
#             faerie conceal 400, elf home defence 1500, faerie 2000).
#   conflict  P3-06 r9 20.9: eleven fields after last_refuge, shipped values.
#   peoples   P3-05 r8 21 item 5: regions_per_extra_civ 90 and civs_extra_max 2
#             after regions_per_civ.
#   report    P3-06 r9 20.9: claim salience 45 -> 35 (P3-07 r8 21.6); the
#             entries of kinds 32 refugees_settled (65) and 33 tribute (50).
#             ASSUMPTION (spec gap, as for kind 31): appended after kind 31's
#             entry in kind id order; the spec gives the entries, not their
#             place in report.toml.
#   cultures  unchanged.
#   name filter: the short word list (P3-09 item 8) is data read from disk
#             (game/data/name_filters/), not a listing here.
# ---------------------------------------------------------------------------

RACE_FIELDS_018_VALUES = {
    "elf": (1000, 1001, 1500), "goblin": (1000, 1001, 0), "dwarf": (1000, 1001, 0),
    "faerie": (1000, 400, 0), "human": (1000, 1001, 0), "orc": (4000, 1001, 0),   # faerie home 0: P3-01 r10 3.3
}


def _race_018(name, text):
    m, cmin, hd = RACE_FIELDS_018_VALUES[name[:-5]]
    line = [l for l in text.splitlines(True) if l.startswith("prowess_permille = ")][0]
    return _edit(text, line, line + "muster_permille = %d\nconceal_min = %d\nhome_defense_permille = %d\n"
                 % (m, cmin, hd))


RACE_FILES_018 = {n: _race_018(n, t) for n, t in RACE_FILES_017.items()}
CONFLICT_018_VALUES = (("refuge_flight", 1), ("flight_hops", 3), ("hidden_years", 300), ("weary_wars", 3),
                       ("tribute_permille", 100), ("revenge_aggression", 400), ("levy_hops", 3),
                       ("odds_floor", 500), ("odds_min", 700), ("truce_break_aggression", 600),
                       ("enmity_hops", 3), ("kill_max_permille", 500), ("find_permille", 250), ("search_years", 3))
assert [n for n, _ in CONFLICT_018_VALUES] == [n for n, _, _ in CONFLICT_NEW_018]
CONFLICT_FILE_018 = {"conflict.toml": _edit(CONFLICT_FILE["conflict.toml"], "last_refuge = 1\n", "last_refuge = 1\n" +
                                            "".join("%s = %d\n" % kv for kv in CONFLICT_018_VALUES))}
PEOPLES_FILE_018 = {"peoples.toml": _edit(PEOPLES_FILE_017["peoples.toml"], "regions_per_civ = 4\n",
                                          "regions_per_civ = 4\nregions_per_extra_civ = 90\ncivs_extra_max = 2\n")}
KIND32_ENTRY = ('  { kind = "refugees_settled", salience = 65, template = "The survivors of {origin} fled into hiding '
                'and founded {@settlement} in {@region}." },\n')
KIND33_ENTRY = ('  { kind = "tribute", salience = 50, template = "{payer} paid tribute to {payee}; {people} '
                '{people?was|were} carried off." },\n')
_re = _edit(REPORT_FILE_017["report.toml"], '{ kind = "claim", salience = 45,', '{ kind = "claim", salience = 35,')
_last = [l for l in _re.splitlines(True) if '"settlement_abandoned_to"' in l][0]
# r10 20.15, 20.17, 20.22 (P3-09 r3): kinds 34-36, appended after 33 (20.21 item 9)
KIND34_ENTRY = ('  { kind = "war_won", salience = 60, template = "{winner} prevailed over {loser} after {years} '
                '{years?year|years} of war." },\n')
KIND35_ENTRY = ('  { kind = "search_abandoned", salience = 40, template = "{searcher} searched {@region} for {hidden} '
                'and found no one; the war ended after {years} {years?year|years}." },\n')
KIND36_ENTRY = ('  { kind = "refuge_held", salience = 25, template = "{attacker} besieged {@settlement} of {defender}; '
                'with {reason?nowhere to flee|too few to flee}, its people held on." },\n')
_re = _edit(_re, _last, _last + KIND32_ENTRY + KIND33_ENTRY + KIND34_ENTRY + KIND35_ENTRY + KIND36_ENTRY)
REPORT_FILE_018 = {"report.toml": _re}
HISTORY_FILES_018 = dict(CHRONOLOGY_FILE, **CONFLICT_FILE_018, **ENGINE_FILE, **PEOPLES_FILE_018, **REPORT_FILE_018)
SPEC_FILES_018 = {"race": RACE_FILES_018, "culture": CULTURE_FILES_017, "history": HISTORY_FILES_018}

# The data version this module serves: "0.18.0" (default, P3-09), "0.17.0"
# (P3-08) or "0.16.0" (use_version), so that the references can still
# reproduce the older worlds and goldens.
DATA_VERSION = None
VERSIONS = ("0.16.0", "0.17.0", "0.18.0")


def at_least(v):
    """True when the served data version is v or later."""
    return VERSIONS.index(DATA_VERSION) >= VERSIONS.index(v)


def use_version(v):
    global DATA_VERSION, KINDS, SPEC_FILES
    if v == "0.18.0":
        KINDS, SPEC_FILES = KINDS_018, SPEC_FILES_018
    elif v == "0.17.0":
        KINDS, SPEC_FILES = KINDS_017, SPEC_FILES_017
    elif v == "0.16.0":
        KINDS, SPEC_FILES = KINDS_016, SPEC_FILES_016
    else:
        raise ValueError(v)
    DATA_VERSION = v


use_version("0.18.0")

# ---------------------------------------------------------------------------
# Loading and validation (types only: what the encoding needs)
# ---------------------------------------------------------------------------


class DataError(Exception):
    pass


def valid_file_name(name):
    if not name.endswith(".toml") or len(name) <= 5:
        return False
    stem = name[:-5]
    return stem[0] in "abcdefghijklmnopqrstuvwxyz0123456789" and all(
        c in "abcdefghijklmnopqrstuvwxyz0123456789_-" for c in stem)


def read_kind_files(root, kind, from_spec):
    """[(file name, text)] of one kind directory in byte order (P0-03 7.2),
    or None when the directory does not exist."""
    directory = KINDS[kind][0]
    path = os.path.join(REPO, root, directory) if not os.path.isabs(root) else os.path.join(root, directory)
    if not os.path.isdir(path):
        if from_spec and kind in SPEC_FILES:
            return sorted(SPEC_FILES[kind].items(), key=lambda kv: kv[0].encode())
        if at_least("0.17.0") and kind in COPIED_KINDS and SPEC_FALLBACK:
            # P1-06 r3 4.1, P3-02 r5 11.2: the fixture roots get identical
            # copies of the shipped (normative, committed) files with the
            # implementation; until then game/data's files stand in.
            shipped = os.path.join(REPO, "game", "data", directory)
            if os.path.isdir(shipped) and os.path.normpath(shipped) != os.path.normpath(path):
                PRE_015_USED.add(os.path.join(root, directory))
                return read_dir_files(shipped, directory)
        return None
    if kind in SPEC_FILES and SPEC_FALLBACK and not same_version(kind, disk_version(path, kind), DATA_VERSION):
        # The race, culture and history files are the engineer's deliverable
        # (P3-06 for 0.16.0, P3-08 for 0.17.0, P3-09 for 0.18.0): while the
        # disk holds files of another version, the listings of the served
        # version stand in for them (noted by `check`).
        PRE_015_USED.add(os.path.join(root, directory))
        return sorted(SPEC_FILES[kind].items(), key=lambda kv: kv[0].encode())
    return read_dir_files(path, directory)


# P3-09 item 8 (0.18.0): the short word list replaces game/data/name_filters/words.toml (and its fixture copies).
# A version before 0.18.0 reads the name_filter files as they were at the P3-08 commit below (from git), so
# that the 0.17.0 goldens stay reproducible after the data change.
NAME_FILTERS_017_COMMIT = "d750d15"


def legacy_filter_files(path, directory):
    import subprocess
    rel = os.path.relpath(path, REPO).replace(os.sep, "/")
    names = subprocess.run(["git", "-C", REPO, "ls-tree", "--name-only", NAME_FILTERS_017_COMMIT + ":" + rel],
                           check=True, capture_output=True, text=True).stdout.split()
    out = []
    for name in sorted((n for n in names if n.endswith(".toml")), key=lambda n: n.encode()):
        text = subprocess.run(["git", "-C", REPO, "show", "%s:%s/%s" % (NAME_FILTERS_017_COMMIT, rel, name)],
                              check=True, capture_output=True).stdout.decode("utf-8")
        out.append((name, text))
    return out


def read_dir_files(path, directory):
    if directory == "name_filters" and DATA_VERSION != "0.18.0" and os.path.isdir(path) and \
            os.path.normpath(path).startswith(os.path.normpath(REPO)):
        return legacy_filter_files(path, directory)
    names = []
    for name in os.listdir(path):
        if name.startswith(".") or not name.endswith(".toml"):
            continue
        if not valid_file_name(name):
            raise DataError("%s/%s: bad data file name" % (directory, name))
        names.append(name)
    out = []
    for name in sorted(names, key=lambda n: n.encode()):
        with open(os.path.join(path, name), "rb") as f:
            out.append((name, f.read().decode("utf-8")))
    return out


def disk_version(path, kind):
    """The data version of a race, culture or history directory on disk."""
    if kind in ("race", "history") and pre_015(path, kind):
        return "0.15.0"
    if pre_017(path, kind):
        return "0.16.0"
    if kind != "culture" and pre_018(path, kind):
        return "0.17.0"
    return "0.18.0"


def same_version(kind, a, b):
    """Whether two data versions have the same files of a kind (cultures did not change at 0.18.0)."""
    if kind == "culture":
        a, b = ("0.17.0" if x == "0.18.0" else x for x in (a, b))
    return a == b


def pre_018(path, kind):
    """A race or history directory older than 0.18.0 (P3-09): no `muster_permille` in a race file, no
    `refuge_flight` in conflict.toml, no `regions_per_extra_civ` in peoples.toml or no kind 32 entry
    in report.toml."""
    texts = {}
    for n in os.listdir(path):
        if n.endswith(".toml"):
            with open(os.path.join(path, n), encoding="utf-8") as f:
                texts[n] = f.read()
    if kind == "history":
        return ("kill_max_permille" not in texts.get("conflict.toml", "")
                or "refuge_held" not in texts.get("report.toml", "")
                or "refuge_flight" not in texts.get("conflict.toml", "")
                or "regions_per_extra_civ" not in texts.get("peoples.toml", "")
                or "refugees_settled" not in texts.get("report.toml", ""))
    return any("muster_permille" not in t for t in texts.values())


def pre_017(path, kind):
    """A race, culture or history directory older than 0.17.0 (P3-08): no
    `cell_weights` in a race file, no `god_patterns` in a culture file, no
    kind 31 entry in report.toml or no `plague_major_dead` in peoples.toml."""
    names = [n for n in os.listdir(path) if n.endswith(".toml")]
    texts = {}
    for n in names:
        with open(os.path.join(path, n), encoding="utf-8") as f:
            texts[n] = f.read()
    if kind == "history":
        return ("settlement_abandoned_to" not in texts.get("report.toml", "")
                or "plague_major_dead" not in texts.get("peoples.toml", ""))
    word = {"race": "cell_weights", "culture": "god_patterns"}[kind]
    return any(word not in t for t in texts.values())


# A root whose races/ has no `prowess_permille` or whose history/ has no
# conflict.toml is a root older than 0.16.0 (P3-06 12.1, 12.2); with
# SPEC_FALLBACK the listings of this file replace it. (PRE_015_USED keeps
# its name for the callers.)
SPEC_FALLBACK = True
PRE_015_USED = set()


def pre_015(path, kind):
    names = [n for n in os.listdir(path) if n.endswith(".toml")]
    if kind == "history":
        return "conflict.toml" not in names
    for n in names:
        with open(os.path.join(path, n), encoding="utf-8") as f:
            if "prowess_permille" not in f.read():
                return True
    return False


def check_value(t, v, key):
    """Raises DataError unless v suits schema type t (TOML types only)."""
    if isinstance(v, float):
        raise DataError("floating-point value for '%s'" % key)
    kind = t[0]
    if kind == "int":
        if not isinstance(v, int) or isinstance(v, bool):
            raise DataError("'%s' is not an integer" % key)
    elif kind == "fx":
        if isinstance(v, bool) or not isinstance(v, (int, str)):
            raise DataError("'%s' is not an fx value" % key)
        if isinstance(v, str):
            try:
                ref.parse_decimal(v)
            except Exception as e:  # noqa: BLE001
                raise DataError("'%s' is not a decimal: %s" % (key, e))
    elif kind == "bool":
        if not isinstance(v, bool):
            raise DataError("'%s' is not a boolean" % key)
    elif kind == "str":
        if not isinstance(v, str):
            raise DataError("'%s' is not a string" % key)
    elif kind == "arr":
        if not isinstance(v, list):
            raise DataError("'%s' is not an array" % key)
        for i, e in enumerate(v):
            check_value(t[1], e, "%s[%d]" % (key, i))
    elif kind == "rec":
        check_record(t[1], v, key)
    else:
        raise AssertionError(kind)


def check_record(schema, table, key):
    if not isinstance(table, dict):
        raise DataError("'%s' is not a table" % key)
    names = [n for n, _ in schema]
    for k in table:
        if k not in names:
            raise DataError("unknown key '%s.%s'" % (key, k))
    for n, t in schema:
        if n not in table:
            raise DataError("missing required field '%s.%s'" % (key, n))  # no defaults in these kinds
        check_value(t, table[n], key + "." + n)


def load_root(root, kinds=None, from_spec=False):
    """{kind: [(id, table)] ascending id} for the given kinds (all by default)."""
    kinds = sorted(kinds if kinds is not None else KINDS)
    content = {}
    for kind in kinds:
        files = read_kind_files(root, kind, from_spec)
        if files is None:
            raise DataError("%s: the directory of kind '%s' is missing" % (root, kind))
        defs = {}
        for name, text in files:
            doc = tomllib.loads(text)
            for top in doc:
                if top != kind:
                    raise DataError("%s/%s: top-level key '%s'" % (KINDS[kind][0], name, top))
            for did, table in doc.get(kind, {}).items():
                if did in defs:
                    raise DataError("duplicate id '%s.%s'" % (kind, did))
                defs[did] = table
        schema = KINDS[kind][1]
        if (at_least("0.17.0") and kind == "worldgen" and SPEC_FALLBACK and "regions" in defs
                and not any(k in defs["regions"] for k in REGIONS_017_VALUES)):
            # P1-06 r3 4: the two records are written into regions.toml by the
            # engineer; the shipped values of the spec's block stand in.
            PRE_015_USED.add(os.path.join(root, "worldgen/regions"))
            defs["regions"] = dict(defs["regions"], **REGIONS_017_VALUES)
        if DATA_VERSION == "0.16.0" and kind == "worldgen" and "regions" in defs:
            # 0.17.0 regions.toml on disk: the two r3 records are not 0.16.0 fields.
            defs["regions"] = {k: v for k, v in defs["regions"].items() if k not in REGIONS_017_VALUES}
        for did, table in defs.items():
            if isinstance(schema, dict):
                if did not in schema:
                    raise DataError("unknown id '%s' for kind '%s'" % (did, kind))
                check_record(schema[did], table, kind + "." + did)
            else:
                check_record(schema, table, kind + "." + did)
        if isinstance(schema, dict):
            for did in schema:
                if did not in defs:
                    raise DataError("missing definition '%s.%s'" % (kind, did))
        content[kind] = sorted(defs.items(), key=lambda kv: kv[0].encode())
    return content


# ---------------------------------------------------------------------------
# Canonical encoding (P0-03 8)
# ---------------------------------------------------------------------------


def u64(v):
    return struct.pack("<Q", v)


def i64(v):
    return struct.pack("<q", v)


def enc_str(s):
    b = s.encode("utf-8")
    return u64(len(b)) + b


def enc_value(t, v):
    kind = t[0]
    if kind == "int":
        return b"\x01" + i64(v)
    if kind == "fx":
        raw = (v << 32) if isinstance(v, int) else ref.parse_decimal(v)
        return b"\x02" + i64(raw)
    if kind == "bool":
        return b"\x03" + (b"\x01" if v else b"\x00")
    if kind == "str":
        # P0-03 4: CR LF in a string becomes LF (none of these files has one).
        return b"\x04" + enc_str(v.replace("\r\n", "\n"))
    if kind == "arr":
        return b"\x05" + u64(len(v)) + b"".join(enc_value(t[1], e) for e in v)
    if kind == "rec":
        return b"\x06" + enc_record(t[1], v)
    raise AssertionError(kind)


def enc_record(schema, table):
    return b"".join(enc_str(n) + enc_value(t, table[n]) for n, t in schema)


def encode(content):
    out = bytearray()
    for kind in sorted(content, key=lambda k: k.encode()):
        defs = content[kind]
        schema = KINDS[kind][1]
        out += enc_str(kind) + u64(len(defs))
        for did, table in defs:
            out += enc_str(did) + enc_record(schema[did] if isinstance(schema, dict) else schema, table)
    return bytes(out)


def content_hash(root, kinds=None, from_spec=False):
    data = encode(load_root(root, kinds, from_spec))
    h = ref.sha256_hex(data)
    assert h == hashlib.sha256(data).hexdigest()
    return h


def golden_text(from_spec=False):
    return "".join("%s %s\n" % (r, content_hash(r, from_spec=from_spec)) for r in ROOTS)


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------


class Checker:
    def __init__(self):
        self.failures = 0
        self.count = 0

    def ok(self, what, cond):
        self.count += 1
        if not cond:
            self.failures += 1
            print("FAIL:", what)

    def eq(self, what, got, want):
        self.count += 1
        if got != want:
            self.failures += 1
            print("FAIL: %s: got %r, want %r" % (what, got, want))


def parsed_spec(kind):
    defs = {}
    for _, text in SPEC_FILES[kind].items():
        defs.update(tomllib.loads(text)[kind])
    return defs


def fx_raw(v):
    return (v << 32) if isinstance(v, int) else ref.parse_decimal(v)


def check_spec_invariants(c):
    """P3-01 3: the shipped data passes every named and kind-level check and
    the three cross-kind conditions (the spec states none can fail)."""
    races = parsed_spec("race")
    cultures = parsed_spec("culture")
    for rid, r in races.items():
        check_record(KINDS["race"][1], r, "race." + rid)
    for cid, cu in cultures.items():
        check_record(KINDS["culture"][1], cu, "culture." + cid)
    check_record(CHRONOLOGY, parsed_spec("history")["chronology"], "history.chronology")
    check_record(ENGINE, parsed_spec("history")["engine"], "history.engine")
    check_record(REPORT, parsed_spec("history")["report"], "history.report")
    check_record(KINDS["history"][1]["peoples"], parsed_spec("history")["peoples"], "history.peoples")
    check_record(KINDS["history"][1]["conflict"], parsed_spec("history")["conflict"], "history.conflict")
    c.eq("history ids", sorted(parsed_spec("history")), ["chronology", "conflict", "engine", "peoples", "report"])
    co = parsed_spec("history")["conflict"]
    c.eq("history.conflict passes its named checks (P3-06 12.1)", conflict_check_messages(co), [])
    for n, lo, hi in conflict_ranges():
        vals = list(co[n].values()) if isinstance(co[n], dict) else [co[n]]
        c.ok("history.conflict.%s inside its range" % n, all(lo <= v <= hi for v in vals))
    pe = parsed_spec("history")["peoples"]
    c.eq("history.peoples passes its named checks (P3-05 12.1)", peoples_check_messages(pe), [])
    for n, lo, hi in (PEOPLES_RANGES_018 if at_least("0.18.0") else
                      PEOPLES_RANGES_017 if at_least("0.17.0") else PEOPLES_RANGES):
        vals = list(pe[n].values()) if isinstance(pe[n], dict) else [pe[n]]
        c.ok("history.peoples.%s inside its range" % n, all(lo <= v <= hi for v in vals))
    kinds30 = ["awakening", "civ_founded", "settlement_founded", "quiet_era", "flare_up", "colony_founded",
               "became_village", "became_town", "became_city", "settlement_dwindled", "famine", "plague",
               "plague_spread", "succession_crisis", "civ_seceded", "settlement_abandoned", "capital_moved",
               "civ_ended", "ruin_faded", "claim", "war_declared", "battle", "battle_at_pass", "siege", "sacked",
               "settlement_captured", "peace", "truce", "region_ceded", "civ_conquered"]
    later = (["settlement_abandoned_to"] if at_least("0.17.0") else []) + \
        (["refugees_settled", "tribute", "war_won", "search_abandoned", "refuge_held"] if at_least("0.18.0") else [])
    c.eq("report lists kinds 1-%d in id order (P3-05 11, P3-06 11%s)"
         % (30 + len(later), (", P3-05 r6 11" if at_least("0.17.0") else "") +
            (", P3-06 r9 20.9" if at_least("0.18.0") else "")),
         [e["kind"] for e in parsed_spec("history")["report"]["events"]], kinds30 + later)
    if at_least("0.17.0"):
        check_spec_017(c, races, cultures)
    if at_least("0.18.0"):
        check_spec_018(c, races)
    c.eq("densities (P3-05 12.2)", {k: v["density_permille"] for k, v in races.items()},
         dict(elf=300, goblin=900, dwarf=700, faerie=250, human=1000, orc=800))
    c.eq("prowess_permille (P3-06 12.2)", {k: v["prowess_permille"] for k, v in races.items()},
         dict(elf=1600, goblin=800, dwarf=1400, faerie=1200, human=1000, orc=1300))
    c.ok("prowess_permille inside int(1, 4000)", all(1 <= v["prowess_permille"] <= 4000 for v in races.values()))
    c.eq("aggression (P3-06 12.2: goblins 700 -> 600)", {k: v["aggression"] for k, v in races.items()},
         dict(elf=150, goblin=600, dwarf=300, faerie=50, human=400, orc=900))
    c.eq("prowess_permille follows aggression in every listing (P3-01 r7)",
         [list(v).index("prowess_permille") - list(v).index("aggression") for v in races.values()], [1] * 6)
    c.eq("six races", sorted(races), ["dwarf", "elf", "faerie", "goblin", "human", "orc"])
    c.eq("six cultures", sorted(cultures), ["dwarven", "elven", "faerie", "goblin", "human", "orcish"])
    by_order = sorted(races.items(), key=lambda kv: kv[1]["order"])
    c.eq("order 1..6", [r["order"] for _, r in by_order], [1, 2, 3, 4, 5, 6])
    c.eq("design 5 awakening order", [rid for rid, _ in by_order], ["elf", "goblin", "dwarf", "faerie", "human", "orc"])
    shares = [fx_raw(r["awakening_share"]) for _, r in by_order]
    c.eq("order 1 share 0", shares[0], 0)
    c.ok("shares strictly increase", all(a < b for a, b in zip(shares, shares[1:])))
    land = set()
    for name, text in [(n, open(os.path.join(REPO, "game", "data", "biomes", n), encoding="utf-8").read())
                       for n in os.listdir(os.path.join(REPO, "game", "data", "biomes")) if n.endswith(".toml")]:
        for bid, b in tomllib.loads(text)["biome"].items():
            if not b["water"]:
                land.add(bid)
    for rid, r in races.items():
        c.ok("%s: generation below lifespan" % rid, r["generation_years"] < r["lifespan_years"])
        c.ok("%s: site weights sum >= 1" % rid, sum(r["site_weights"].values()) >= 1)
        bl = [b["biome"] for b in r["biomes"]]
        c.ok("%s: biomes unique" % rid, len(bl) == len(set(bl)))
        c.ok("%s: biomes are land biomes" % rid, all(b in land for b in bl))
        c.ok("%s: at most 16 biomes" % rid, len(bl) <= 16)
        sk = r["settlement_kinds"]
        c.ok("%s: settlement kinds unique" % rid, len(sk) == len(set(sk)) and 1 <= len(sk) <= 5)
        c.ok("%s: settlement kinds valid" % rid, set(sk) <= {"hamlet", "village", "town", "city", "fortress"})
        c.ok("%s: camp" % rid, r["camp"] in ("good", "neutral", "evil"))
        c.ok("%s: culture belongs to it" % rid, cultures[r["culture"]]["race"] == rid)
        for w in ("name", "plural", "adjective"):
            v = r[w]
            c.ok("%s.%s is a word" % (rid, w), 1 <= len(v) <= 32 and v[0].isupper() and v[0].isascii()
                 and all("a" <= ch <= "z" for ch in v[1:]))
    c.ok("every race has a culture", {cu["race"] for cu in cultures.values()} == set(races))
    form = parsed_spec("history")["chronology"]["age_name_form"]
    c.ok("age_name_form check", ("{plural}" in form or "{adjective}" in form)
         and form.replace("{plural}", "").replace("{adjective}", "").count("{") == 0)


def check_spec_018(c, races):
    """The 0.18.0 listings say what P3-01 r9 3.2, P3-05 r8 21 item 5 and P3-06 r9 20.9 and 20.12 state."""
    c.eq("race fields of P3-06 r9 20.12 (muster, conceal_min, home defence)",
         {k: (v["muster_permille"], v["conceal_min"], v["home_defense_permille"]) for k, v in races.items()},
         RACE_FIELDS_018_VALUES)
    c.eq("the three race fields follow prowess_permille in this order (P3-01 r9 3.2)",
         [list(v)[list(v).index("prowess_permille") + 1:list(v).index("prowess_permille") + 4] for v in races.values()],
         [["muster_permille", "conceal_min", "home_defense_permille"]] * 6)
    for k, v in races.items():
        for n, lo, hi in RACE_FIELDS_018:
            c.ok("race %s.%s inside int(%d, %d)" % (k, n, lo, hi), lo <= v[n] <= hi)
    co = parsed_spec("history")["conflict"]
    i = list(co).index("last_refuge")
    c.eq("history.conflict: the eleven r9 and three r10 fields after last_refuge, in order, shipped values",
         [(n, co[n]) for n in list(co)[i + 1:i + 15]], list(CONFLICT_018_VALUES))
    c.eq("history.conflict: capital_sacked_salience follows search_years", list(co)[i + 15], "capital_sacked_salience")
    c.eq("history.conflict has 57 fields", len(co), 57)
    pe = parsed_spec("history")["peoples"]
    j = list(pe).index("regions_per_civ")
    c.eq("history.peoples: regions_per_extra_civ 90 and civs_extra_max 2 after regions_per_civ (P3-05 r8 21 item 5)",
         [(n, pe[n]) for n in list(pe)[j + 1:j + 3]], [("regions_per_extra_civ", 90), ("civs_extra_max", 2)])
    ev = {e["kind"]: e for e in parsed_spec("history")["report"]["events"]}
    c.eq("claim salience 35 (P3-06 r9 20.9, P3-07 r8 21.6)", ev["claim"]["salience"], 35)
    c.eq("kind 32 entry (P3-06 r9 20.9)", (ev["refugees_settled"]["salience"], ev["refugees_settled"]["template"]),
         (65, "The survivors of {origin} fled into hiding and founded {@settlement} in {@region}."))
    c.eq("kind 33 entry (P3-06 r9 20.9)", (ev["tribute"]["salience"], ev["tribute"]["template"]),
         (50, "{payer} paid tribute to {payee}; {people} {people?was|were} carried off."))
    c.eq("every other report entry as at 0.17.0", [e for e in parsed_spec("history")["report"]["events"]
                                                    if e["kind"] not in ("claim", "refugees_settled", "tribute",
                                                                         "war_won", "search_abandoned",
                                                                         "refuge_held")],
         [e for e in tomllib.loads(REPORT_FILE_017["report.toml"])["history"]["report"]["events"]
          if e["kind"] != "claim"])
    # the named check conflict_enmity_hops, once failing and at its edge (20.9)
    c.eq("conflict_enmity_hops fails below march_hops", conflict_check_messages(dict(co, enmity_hops=1)),
         ["enmity_hops must not be below march_hops"])
    c.eq("conflict_enmity_hops passes at march_hops", conflict_check_messages(dict(co, enmity_hops=2)), [])


def check_spec_017(c, races, cultures):
    """The 0.17.0 listings say what P3-01 r8, P3-02 r5 11.4, P3-04 r10 4.2,
    P3-05 r6 11, 12.1, 12.3 and P3-06 r7 11 state."""
    c.eq("cell_weights (P3-01 r8 (1))", {k: (v["cell_weights"]["ore"], v["cell_weights"]["height"])
                                        for k, v in races.items()},
         dict(elf=(0, 0), goblin=(0, 0), dwarf=(400, 600), faerie=(0, 0), human=(0, 0), orc=(0, 0)))
    c.eq("cell_weights follows site_weights in every listing (P3-01 r8)",
         [list(v).index("cell_weights") - list(v).index("site_weights") for v in races.values()], [1] * 6)
    c.eq("faerie biomes (P3-05 r6 12.3, candidate A, nine rows in this order)",
         [(b["biome"], b["weight"]) for b in races["faerie"]["biomes"]],
         [("temperate_forest", 800), ("wet_woodland", 800), ("fen", 300), ("marsh", 200), ("highland_moor", 100),
          ("heath", 0), ("grassland", -300), ("alpine", -400), ("steppe", -400)])
    for cid, cu in cultures.items():
        n = cu["naming"]
        one_nucleus = [q for q in n["patterns"] if q.count("N") <= 1]
        c.eq("%s: god_patterns = patterns without the one-nucleus shapes, repeats kept (P3-02 r5 11.4)" % cid,
             n["god_patterns"], [q for q in n["patterns"] if q not in one_nucleus])
        c.eq("%s: god_patterns follows patterns" % cid, list(n).index("god_patterns") - list(n).index("patterns"), 1)
        c.ok("%s: naming_realm_forms (every civilization form holds a word besides {root})" % cid,
             all(" " in f for f in n["civilization_forms"]))
    c.eq("elven civilization forms (P3-02 r5 11.4)", cultures["elven"]["naming"]["civilization_forms"],
         ["The Court of {root}", "The Realm of {root}", "The House of {root}"])
    pe = parsed_spec("history")["peoples"]
    c.eq("history.peoples r6 values (P3-05 r6 12.1)",
         {k: pe[k] for k in ("famine_ppm", "plague_spread_permille", "plague_major_dead", "crisis_small_salience")},
         dict(famine_ppm=1250, plague_spread_permille=100, plague_major_dead=1000, crisis_small_salience=25))
    c.eq("history.peoples ends with the two r6 fields", list(pe)[-3:],
         ["plague_small_salience", "plague_major_dead", "crisis_small_salience"])
    ev = {e["kind"]: e for e in parsed_spec("history")["report"]["events"]}
    for k, (_, new) in TEMPLATES_017.items():
        c.eq("report template of %s (0.17.0)" % k, ev[k]["template"], new)
    c.eq("kind 31 entry (P3-05 r6 11)", (ev["settlement_abandoned_to"]["salience"],
                                         ev["settlement_abandoned_to"]["template"]),
         (55, "{settlement} was abandoned; {settled} {settled?survivor|survivors} went to {destination}"
              "{lost?|, and one was lost|, and # were lost}."))
    c.eq("crisis and abandonment saliences unchanged (P3-05 r6 11: bases 40, 55)",
         (ev["succession_crisis"]["salience"], ev["settlement_abandoned"]["salience"], ev["plague"]["salience"]),
         (40, 55, 45))


FILTER_WORD = set("abcdefghijklmnopqrstuvwxyz")


def name_filter_messages(defs):
    """The named and kind checks of P3-02 r5 11.2 on [(id, table)] ascending
    id: the first failing message of each check, in order, as
    "<check>: <message>" (the loader's prefix is not reproduced)."""
    out = []
    lists = ("anywhere", "at_start", "whole")
    for did, t in defs:
        for f in lists:
            for j, w in enumerate(t[f]):
                if not (2 <= len(w) <= 16 and set(w) <= FILTER_WORD):
                    out.append("grammar: %s.%s[%d] '%s'" % (did, f, j, w))
        msg = None
        for f in lists:
            seen = {}
            for j, w in enumerate(t[f]):
                if w in seen and msg is None:
                    msg = "%s[%d] '%s' repeats %s[%d]" % (f, j, w, f, seen[w])
                seen.setdefault(w, j)
        if msg:
            out.append("name_filter_unique: '%s': %s" % (did, msg))
    msg = None
    for i, (d1, t1) in enumerate(defs):
        for f in lists:
            for w in t1[f]:
                for d2, t2 in defs[i + 1:]:
                    if msg is None and w in t2[f]:
                        msg = "%s '%s' is in '%s' and '%s'" % (f, w, d1, d2)
    if msg:
        out.append("name_filter_distinct: " + msg)
    msg = None
    for d1, t1 in defs:
        for f in ("at_start", "whole"):
            for w in t1[f]:
                if msg is not None:
                    break
                cover = None
                for d2, t2 in defs:
                    for f2 in lists:
                        for w2 in t2[f2]:
                            if cover is None and (
                                    (f2 == "anywhere" and w2 in w) or (f == "whole" and f2 == "at_start"
                                                                        and w.startswith(w2))):
                                cover = (f2, w2, d2)
                if cover:
                    msg = "'%s' %s '%s' is already rejected by %s '%s' of '%s'" % (d1, f, w, cover[0], cover[1],
                                                                                  cover[2])
    if msg:
        out.append("name_filter_not_redundant: " + msg)
    return out


def check_copied_kinds(c):
    """P3-02 r5 11.2 and P1-06 r3 4.1: the shipped name_filter (and
    placename) files load; their fixture copies, where they exist, are
    byte-identical; the name_filter lists pass their checks, are sorted and
    have the sizes of 11.2."""
    for kind in COPIED_KINDS:
        directory = KINDS[kind][0]
        shipped = read_dir_files(os.path.join(REPO, "game", "data", directory), directory)
        c.ok("game/data/%s/ exists and is not empty" % directory, bool(shipped))
        for root in ROOTS[1:]:
            path = os.path.join(REPO, root, directory)
            if not os.path.isdir(path):
                print("note: %s/%s/ does not exist yet; game/data's files stand in for it" % (root, directory))
                continue
            c.eq("%s/%s/ is a byte-identical copy of game/data's" % (root, directory),
                 read_dir_files(path, directory), shipped)
    content = load_root("game/data", ["name_filter"])["name_filter"]
    c.eq("name_filter ids (P3-02 r5 11.2)", [d for d, _ in content], ["crude", "famous", "slurs", "words"])
    c.eq("name_filter sizes (P3-02 r5 11.2)",
         {d: tuple(len(t[f]) for f in ("anywhere", "at_start", "whole")) for d, t in content},
         dict(slurs=(33, 19, 6), crude=(68, 16, 14), words=(0, 0, 392 if at_least("0.18.0") else 647),
              famous=(0, 0, 101)))   # P3-02 r8 11.11 (0.18.0): the short word list, 392 (647 before)
    eff = [len(set(w for _, t in content for w in t[f])) for f in ("anywhere", "at_start", "whole")]
    c.eq("effective list sizes (P3-02 r5 11.2, r8 11.11)", eff, [101, 35, 513 if at_least("0.18.0") else 768])
    c.eq("shipped name_filter passes its checks", name_filter_messages(content), [])
    for d, t in content:
        for f in ("anywhere", "at_start", "whole"):
            c.ok("name_filter.%s.%s sorted (convention, 11.7 item 1)" % (d, f), t[f] == sorted(t[f]))
    # Each check fails once (P3-02 r5 11.7 item 2), on hand-made data.
    bad = [("a", dict(anywhere=["ab", "cd", "ab"], at_start=[], whole=[]))]
    c.eq("name_filter_unique message", name_filter_messages(bad),
         ["name_filter_unique: 'a': anywhere[2] 'ab' repeats anywhere[0]"])
    bad = [("a", dict(anywhere=["xy"], at_start=[], whole=[])), ("b", dict(anywhere=["xy"], at_start=[], whole=[]))]
    c.eq("name_filter_distinct message", name_filter_messages(bad),
         ["name_filter_distinct: anywhere 'xy' is in 'a' and 'b'"])
    bad = [("a", dict(anywhere=["ab"], at_start=["cab"], whole=[])), ("b", dict(anywhere=[], at_start=[], whole=[]))]
    c.eq("name_filter_not_redundant (anywhere inside at_start)", name_filter_messages(bad),
         ["name_filter_not_redundant: 'a' at_start 'cab' is already rejected by anywhere 'ab' of 'a'"])
    bad = [("a", dict(anywhere=[], at_start=[], whole=["cdx"])), ("b", dict(anywhere=[], at_start=["cd"], whole=[]))]
    c.eq("name_filter_not_redundant (whole starts with at_start)", name_filter_messages(bad),
         ["name_filter_not_redundant: 'a' whole 'cdx' is already rejected by at_start 'cd' of 'b'"])
    for w in ("Ni", "a", "abcdefghijklmnopq", "ab1"):
        c.eq("filter_word grammar refuses %r" % w,
             name_filter_messages([("a", dict(anywhere=[w], at_start=[], whole=[]))]),
             ["grammar: a.anywhere[0] '%s'" % w])


def check_disk(c):
    """Files on disk, where the new directories exist, hold exactly the
    listings' values (P3-01 2: identical copies in the fixture roots)."""
    present = 0
    for root in ROOTS:
        for kind in NEW_KINDS:
            files = read_kind_files(root, kind, False)
            if files is None:
                print("note: %s/%s/ does not exist yet; the listings stand in for it" % (root, KINDS[kind][0]))
                continue
            if os.path.join(root, KINDS[kind][0]) in PRE_015_USED:
                print("note: %s/%s/ holds files older than %s; the %s listings stand in for it"
                      % (root, KINDS[kind][0], DATA_VERSION, DATA_VERSION))
                continue
            present += 1
            defs = {}
            for _, text in files:
                defs.update(tomllib.loads(text).get(kind, {}))
            c.eq("%s %s values equal the spec listing" % (root, kind), defs, parsed_spec(kind))
    return present


def self_test(c):
    c.eq("empty stream", ref.sha256_hex(encode({})),
         "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")
    # A hand-made stream (P0-03 8): one kind, one definition, every value form.
    sch = (("i", INT), ("f", FX), ("b", BOOL), ("s", STR), ("a", ARR(INT)), ("r", REC((("x", INT),))))
    KINDS["zz_test"] = ("zz", sch)
    try:
        got = encode({"zz_test": [("d", {"i": -1, "f": "0.5", "b": True, "s": "x", "a": [7], "r": {"x": 2}})]})
    finally:
        del KINDS["zz_test"]
    want = (u64(7) + b"zz_test" + u64(1) + u64(1) + b"d"
            + u64(1) + b"i" + b"\x01" + b"\xff" * 8
            + u64(1) + b"f" + b"\x02" + struct.pack("<q", 1 << 31)
            + u64(1) + b"b" + b"\x03\x01"
            + u64(1) + b"s" + b"\x04" + u64(1) + b"x"
            + u64(1) + b"a" + b"\x05" + u64(1) + b"\x01" + struct.pack("<q", 7)
            + u64(1) + b"r" + b"\x06" + u64(1) + b"x" + b"\x01" + struct.pack("<q", 2))
    c.eq("hand-made stream", got, want)
    # fx as an integer and as "1.0" encode alike (P0-03 8).
    c.eq("fx 1 == '1.0'", enc_value(FX, 1), enc_value(FX, "1.0"))
    # Canonical order is ascending id whatever the file order (P0-03 7.4).
    races = load_root("game/data", ["race"], True)
    c.eq("race ids in canonical order", [d for d, _ in races["race"]],
         ["dwarf", "elf", "faerie", "goblin", "human", "orc"])
    # All six races in one file encode the same bytes (file boundaries do
    # not matter, P0-03 8).
    saved = SPEC_FILES["race"]
    SPEC_FILES["race"] = {"all.toml": "\n".join(saved[k] for k in sorted(saved, reverse=True))}
    try:
        merged = load_root("/nonexistent-root-for-self-test", ["race"], True)
    finally:
        SPEC_FILES["race"] = saved
    c.eq("one file or six", encode(merged), encode(races))


def cmd_check():
    c = Checker()
    self_test(c)
    check_spec_invariants(c)
    present = check_disk(c)
    if at_least("0.17.0"):
        check_copied_kinds(c)
    # The three roots differ only where the geo-small elevation tuning does
    # (P1-00 6.4); the new kinds are identical in all three.
    fresh = golden_text(from_spec=present < len(ROOTS) * len(NEW_KINDS))
    if os.path.exists(GOLDEN):
        with open(GOLDEN, "rb") as f:
            c.eq("committed golden file is current", f.read().decode(), fresh)
    else:
        c.ok("golden file present", False)
    hashes = [l.split()[1] for l in fresh.splitlines()]
    c.ok("game/data and geo agree (identical files)", hashes[0] == hashes[1])
    c.ok("geo-small differs (its elevation tuning)", hashes[2] != hashes[0])
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def main(argv):
    args = argv[1:]
    from_spec = "--from-spec" in args
    args = [a for a in args if a != "--from-spec"]
    if "--version" in args:
        i = args.index("--version")
        use_version(args[i + 1])
        del args[i:i + 2]
    if not args:
        print(__doc__)
        return 2
    if args[0] == "check":
        return cmd_check()
    if args[0] == "golden":
        text = golden_text(from_spec)
        if len(args) > 1 and args[1] == "-":
            sys.stdout.write(text)
        else:
            with open(GOLDEN, "wb") as f:
                f.write(text.encode())
        return 0
    if args[0] == "hash":
        kinds = None
        if "--kinds" in args:
            kinds = args[args.index("--kinds") + 1].split(",")
        print(content_hash(args[1], kinds, from_spec))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
