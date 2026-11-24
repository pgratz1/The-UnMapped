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
# P3-06 r15 21.12 item 2 (0.19.0): after conflict_enmity_hops.
CONFLICT_CHECKS_019 = CONFLICT_CHECKS_018 + (("conflict_quarrel_strain", "quarrel_strain must not be below claim_strain"),
                                             # P3-06 r17 22.7 (P3-10 r3, audit A3)
                                             ("conflict_quarrel_strain_max", "quarrel_strain must not be above strain_max"))


def conflict_ranges():
    """The history.conflict field ranges of the served data version."""
    if at_least("0.21.0"):
        return CONFLICT_RANGES_021
    if at_least("0.19.0"):
        return CONFLICT_RANGES_019
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
    if "quarrel_strain" in t and not (t["quarrel_strain"] >= t["claim_strain"]):
        out.append(CONFLICT_CHECKS_019[3][1])
    if "quarrel_strain" in t and not (t["quarrel_strain"] <= t["strain_max"]):
        out.append(CONFLICT_CHECKS_019[4][1])
    if "toll_spread_permille" in t:   # P3-06 r22 24.5 (P5-01)
        s = t["toll_spread_permille"]
        if not (t["loss_winner_permille"] * (1000 + s) <= min(t["loss_loser_permille"], t["kill_max_permille"])
                * (1000 - s)):
            out.append("toll_spread_permille lets the victors lose more than the defeated")
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


# ---------------------------------------------------------------------------
# The 0.19.0 listings (P3-10, docs/specs/p3-10-followups.md), derived from the
# 0.18.0 ones by the edits the specs state:
#   races     P3-01 r11 3.4: cell_weights gains inland, port, habitability
#             after height (dwarf port 0, habitability 500; faerie inland 400,
#             port 0; every other race neutral 0/1000/1000); faerie coast -500
#             (P3-05 r12 22.5). P3-01 r11 3.5: odds_min_permille (orc 500,
#             others 700) and awakening_quarrel (orc 1) after
#             home_defense_permille.
#   conflict  P3-01 r11 3.5 / P3-06 r14 21.7, 21.9: odds_min removed;
#             search_weary_years 500 and search_repeat_salience 15 after
#             search_years.
#   peoples   P3-05 r12 22.2, 22.5: cause_memory_years 50 and inland_cap 20
#             after civs_extra_max.
#   report    P3-05 r12 22.1, 22.3, 22.4; P3-06 r14 21.2; P3-04 r12 20.1, 20.4:
#             templates of kinds 10, 15, 16, 17, 22, 23, 31; the entries of
#             kinds 37 plague_outbreak (45) and 38 quiet_stretch (20).
#             ASSUMPTION (spec gap, as for kinds 31-36): appended after kind
#             36's entry in kind id order; the specs give the entries, not
#             their place in report.toml.
#   cultures  unchanged.
#   copied kinds (placename, name_filter): P1-06 r6 15.1-15.3 and P3-02 r10
#             12.1, applied to the 0.18.0 files read from disk (or from git
#             at P3_10_BASE_COMMIT once the disk holds 0.19.0 files), see
#             copied_files_019().
# ---------------------------------------------------------------------------

CELL_WEIGHTS_019 = {   # (ore, height, inland, port, habitability), P3-01 r11 3.4
    "elf": (0, 0, 0, 1000, 1000), "goblin": (0, 0, 0, 1000, 1000), "human": (0, 0, 0, 1000, 1000),
    "orc": (0, 0, 0, 1000, 1000), "dwarf": (400, 600, 0, 0, 500), "faerie": (0, 0, 400, 0, 1000),
}
CELL_WEIGHTS_NEUTRAL_019 = (0, 0, 0, 1000, 1000)
RACE_FIELDS_019_VALUES = {   # (odds_min_permille, awakening_quarrel), P3-01 r11 3.5
    "elf": (700, 0), "goblin": (700, 0), "dwarf": (700, 0), "faerie": (700, 0), "human": (700, 0), "orc": (500, 1),
}
# P3-10 second round (r3; P3-01 r13 3.6, P3-06 r17 22.2): race field hidden_years after awakening_quarrel,
# orc 100, others 300.
HIDDEN_YEARS_019 = {"elf": 300, "goblin": 300, "dwarf": 300, "faerie": 300, "human": 300, "orc": 100}
RACE_FIELDS_019 = (("odds_min_permille", 0, 1000), ("awakening_quarrel", 0, 1), ("hidden_years", 0, 10000))
CELL_FIELDS_019 = (("inland", 0, 1000), ("port", 0, 1000), ("habitability", 0, 1000))
RACE_CELL_WEIGHTS_019 = REC(fields("ore:I height:I inland:I port:I habitability:I"))
_i = [n for n, _ in RACE_018].index("home_defense_permille") + 1
RACE_019 = tuple((n, RACE_CELL_WEIGHTS_019 if n == "cell_weights" else t) for n, t in RACE_018[:_i]) + \
    (("odds_min_permille", INT), ("awakening_quarrel", INT), ("hidden_years", INT)) + RACE_018[_i:]
assert RACE_019[_i + 3][0] == "settlement_kinds" and len(RACE_019) == len(RACE_018) + 3


def _race_019(name, text):
    rid = name[:-5]
    o, h, i, p, a = CELL_WEIGHTS_019[rid]
    line = [l for l in text.splitlines(True) if l.startswith("cell_weights = ")][0]
    text = _edit(text, line, "cell_weights = { ore = %d, height = %d, inland = %d, port = %d, habitability = %d }\n"
                 % (o, h, i, p, a))
    if rid == "faerie":
        text = _edit(text, "\ncoast = 0\n", "\ncoast = -500\n")
    q, aq = RACE_FIELDS_019_VALUES[rid]
    line = [l for l in text.splitlines(True) if l.startswith("home_defense_permille = ")][0]
    return _edit(text, line, line + "odds_min_permille = %d\nawakening_quarrel = %d\nhidden_years = %d\n"
                 % (q, aq, HIDDEN_YEARS_019[rid]))


RACE_FILES_019 = {n: _race_019(n, t) for n, t in RACE_FILES_018.items()}

CONFLICT_NEW_019 = (("search_weary_years", 0, 10000), ("search_repeat_salience", 0, 100),
                    ("quarrel_strain", 0, 5000),   # P3-06 r15 21.12 item 2, P3-01 r12 3.5 (P3-10 r1)
                    ("quarrel_min_settlements", 0, 1000))   # P3-06 r17 22.1 item 4 (P3-10 r3)
# r3 (P3-06 r17 22.2): hidden_years leaves history.conflict for the race.
_rows = [r for r in CONFLICT_RANGES_018 if r[0] not in ("odds_min", "hidden_years")]
_i = [n for n, _, _ in _rows].index("search_years") + 1
CONFLICT_RANGES_019 = tuple(_rows[:_i]) + CONFLICT_NEW_019 + tuple(_rows[_i:])
CONFLICT_019 = tuple((n, CONFLICT_RECORDS.get(n, INT)) for n, _, _ in CONFLICT_RANGES_019)
assert len(CONFLICT_019) == 59 and CONFLICT_019[_i][0] == "search_weary_years"
_co = _edit(_edit(CONFLICT_FILE_018["conflict.toml"], "odds_min = 700\n", ""), "hidden_years = 300\n", "")
CONFLICT_FILE_019 = {"conflict.toml": _edit(_co, "search_years = 3\n",
                                            "search_years = 3\nsearch_weary_years = 500\nsearch_repeat_salience = 15\n"
                                            "quarrel_strain = 600\nquarrel_min_settlements = 2\n")}

PEOPLES_NEW_019 = (("cause_memory_years", 0, 100000), ("inland_cap", 1, 256))
_i = [n for n, _, _ in PEOPLES_RANGES_018].index("civs_extra_max") + 1
PEOPLES_RANGES_019 = PEOPLES_RANGES_018[:_i] + PEOPLES_NEW_019 + PEOPLES_RANGES_018[_i:]
PEOPLES_019 = tuple((n, PEOPLES_RECORDS.get(n, INT)) for n, _, _ in PEOPLES_RANGES_019)
assert len(PEOPLES_019) == 71 and PEOPLES_019[_i][0] == "cause_memory_years"
PEOPLES_FILE_019 = {"peoples.toml": _edit(PEOPLES_FILE_018["peoples.toml"], "civs_extra_max = 2\n",
                                          "civs_extra_max = 2\ncause_memory_years = 50\ninland_cap = 20\n")}

# P3-05 r12 22.3: the map M, written out in full in the data.
CAUSES_M_019 = ("famine=famine,plague=plague,plague_outbreak=plague,plague_spread=plague,succession_crisis=strife,"
                "battle=war,battle_at_pass=war,tribute=tribute")
TEMPLATES_019 = {   # kind key -> (0.18.0 template, 0.19.0 template)
    "settlement_dwindled": ("{settlement} dwindled to {population} souls.",
                            "{@causes:" + CAUSES_M_019 + ",colony_founded=emigration|Weakened by |, }"
                            "{settlement} dwindled to {population} souls."),
    "settlement_abandoned": ("{settlement} was abandoned; {survivors?no one survived|its one survivor was scattered"
                             "|its # survivors were scattered}.",
                             "{@causes:" + CAUSES_M_019 + "|Weakened by |, }{settlement} was abandoned; "
                             "{survivors?no one survived|its one survivor was scattered|its # survivors were scattered}."),
    "settlement_abandoned_to": ("{settlement} was abandoned; {settled} {settled?survivor|survivors} went to "
                                "{destination}{lost?|, and one was lost|, and # were lost}.",
                                "{@causes:" + CAUSES_M_019 + "|Weakened by |, }{settlement} was abandoned; {settled} "
                                "{settled?survivor|survivors} went to {destination}{lost?|, and one was lost|, and # were lost}."),
    "civ_seceded": ("{civilization} broke away from {from}, taking {settlements} {settlements?settlement|settlements} "
                    "with it.",
                    "{civilization} broke away from {from} with {settlements} {settlements?settlement|settlements}."),
    "capital_moved": ("{civilization} moved its seat to {settlement}.",
                      "The seat of {civilization} moved to {settlement}."),
    "battle": ("{winner} defeated {loser} in {@region}; {loser_dead} of the defeated and {winner_dead} of the victors "
               "fell.",
               "{winner} defeated {loser} in {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and "
               "none of the victors| and one of the victors fell| and # of the victors fell}."),
    "battle_at_pass": ("{winner} defeated {loser} at the pass into {@region}; {loser_dead} of the defeated and "
                       "{winner_dead} of the victors fell.",
                       "{winner} defeated {loser} at the pass into {@region}; {loser_dead?none|one|#} of the defeated"
                       "{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors "
                       "fell}."),
}
KIND37_ENTRY = ('  { kind = "plague_outbreak", salience = 45, template = "Plague broke out in {settlement}'
                '{reached?| and spread to one other settlement| and spread to # other settlements}; {total} died'
                '{reached?| in all| in all}." },\n')
KIND38_ENTRY = ('  { kind = "quiet_stretch", salience = 20, template = "For {years} {years?year|years}, from {@year} to '
                '{@end_year}, the land was quiet{realms?|; one realm held it|, and # realms kept the peace}." },\n')
_re = REPORT_FILE_018["report.toml"]
for _k, (_old, _new) in TEMPLATES_019.items():
    _re = _edit(_re, 'template = "%s" }' % _old, 'template = "%s" }' % _new)
_last = [l for l in _re.splitlines(True) if '"refuge_held"' in l][0]
_re = _edit(_re, _last, _last + KIND37_ENTRY + KIND38_ENTRY)
REPORT_FILE_019 = {"report.toml": _re}
# P3-04 r15 4.1 (P3-10 r3): history.engine gains quiet_min_years (int(0, 400), shipped 50) after quiet_events_max,
# as the table and the listing place it; named check engine_quiet_min between engine_simulated_bounds and
# engine_phase_budget.
ENGINE_019 = ENGINE[:2] + (("quiet_min_years", INT),) + ENGINE[2:]
assert ENGINE_019[1][0] == "quiet_events_max" and ENGINE_019[3][0] == "simulated_min_permille"
ENGINE_FILE_019 = {"engine.toml": _edit(ENGINE_FILE["engine.toml"], "quiet_events_max = 3\n",
                                        "quiet_events_max = 3\nquiet_min_years = 50\n")}
ENGINE_CHECKS_019 = (
    ("engine_simulated_bounds", "simulated_min_permille must not be above simulated_max_permille"),
    ("engine_quiet_min", "quiet_min_years must not be above 2 x quiet_step_years"),
)


def engine_check_messages(t):
    """The first two named checks of P3-04 4.1 (r15) that fail for a history.engine table (engine_phase_budget is
    the engine reference's)."""
    out = []
    if not (t["simulated_min_permille"] <= t["simulated_max_permille"]):
        out.append(ENGINE_CHECKS_019[0][1])
    if "quiet_min_years" in t and not (t["quiet_min_years"] <= 2 * t["quiet_step_years"]):
        out.append(ENGINE_CHECKS_019[1][1])
    return out + engine_flare_messages(t)   # P3-04 r19 22.3: engine_flare_min after engine_quiet_min


HISTORY_FILES_019 = dict(CHRONOLOGY_FILE, **CONFLICT_FILE_019, **ENGINE_FILE_019, **PEOPLES_FILE_019,
                         **REPORT_FILE_019)
SPEC_FILES_019 = {"race": RACE_FILES_019, "culture": CULTURE_FILES_017, "history": HISTORY_FILES_019}

KINDS_019 = dict(KINDS_018)
KINDS_019["history"] = ("history", {"chronology": CHRONOLOGY, "conflict": CONFLICT_019, "engine": ENGINE_019,
                                    "peoples": PEOPLES_019, "report": REPORT})
KINDS_019["race"] = ("races", RACE_019)

# --- copied kinds at 0.19.0 -------------------------------------------------
# The commit whose game/data holds the 0.18.0 copied-kind files (the P3-10 spec base, the validated P3-09
# state): read from git when the disk holds 0.19.0 files and an earlier version is served.
P3_10_BASE_COMMIT = "81908fa"

# P1-06 r6 15.1: church words out.
CHURCH_019 = {
    ("common", "possessors"): ["Abbess", "Anchorite", "Martyr", "Penitent", "Pilgrim", "Prophet", "Saint", "Sexton"],
    ("common", "objects"): ["Martyrs", "Penance", "Pilgrims", "Prayers", "Saints", "the Unshriven"],
    ("common", "prefixes"): ["Rood", "Tithe"],
    ("common", "places"): ["Penance", "Tithe"],
    ("trait_fertile", "objects"): ["Tithes"],
}
# P1-06 r6 15.2: the grim class (a set per list; the common list's order decides which stay).
GRIM_019 = {
    "prefixes": "Bale Bane Bier Charnel Dirge Dole Dread Drear Gallow Ghost Gibbet Grave Grief Grim Knell Lich Lorn "
                "Mourn Pall Pyre Shroud Sorrow Tomb Weep Wight Woe Wraith Wither",
    "adjectives": "Accursed Baleful Bereft Bloodless Cheerless Crumbling Cursed Desolate Dismal Doleful Dolorous Dread "
                  "Drear Dreary Dying Fallen Forgotten Forlorn Forsaken Friendless Ghostly Godless Grieving Grievous "
                  "Grim Grisly Haunted Joyless Lamenting Lost Moaning Mouldering Mourning Orphaned Pitiless Ruined "
                  "Shunned Sorrowing Spectral Stricken Unhallowed Wailing Weeping Widowed Woeful Wretched",
    "places": "Bane Dole Grave Grief Lament Ruin Shame Sorrow",
    "possessors": "Gravedigger Hangman Headsman Mourner Orphan Widow Widower",
    "objects": "Bones|Curses|Gallows|Ghosts|Gibbets|Graves|Grief|Mourners|Ruin|Shades|Sorrows|Tombs|Widows|Wights|"
               "Wraiths|the Damned|the Fallen|the Forgotten|the Forsaken|the Hanged|the Lost|the Slain|the Unburied|"
               "the Weeping",
}
GRIM_019 = {k: (v.split("|") if "|" in v else v.split()) for k, v in GRIM_019.items()}
GRIM_KEPT_019 = {   # the spec's own statement of what stays in common (checked against the rule)
    "prefixes": "Bale Charnel Dread Ghost Grief Lich Pall Sorrow Wight Wraith".split(),
    "adjectives": ("Accursed Bloodless Cursed Doleful Drear Fallen Forsaken Godless Grim Joyless Moaning Orphaned "
                   "Shunned Stricken Weeping Wretched").split(),
    "places": "Bane Grief Shame".split(), "possessors": "Gravedigger Mourner Widower".split(),
    "objects": ["Bones", "Ghosts", "Grief", "Shades", "Widows", "the Damned", "the Forsaken", "the Slain"],
}
GRIM_MOVED_COUNTS_019 = {"prefixes": 18, "adjectives": 30, "places": 5, "possessors": 4, "objects": 16}
PLACENAME_LISTS = ("nouns", "suffixes", "prefixes", "adjectives", "features", "possessors", "places", "objects",
                   "forbidden", "excluded")
# P3-02 r10 12.1 (data): (file, list) -> (added, removed).
NAME_FILTER_EDITS_019 = {
    ("crude", "anywhere"): (["shiet", "suck"], []),
    ("words", "anywhere"): (["dead", "flop", "gramma", "hack", "whop"], []),
    ("words", "at_start"): (["gord"], []),
    ("words", "whole"): ([], ["flop", "gord", "hack", "suck"]),
}
# P3-02 r13 13.1 (P3-10 second round, r3), applied after r10's edits: (file, list) -> (added, removed).
NAME_FILTER_EDITS_019_R13 = {
    ("crude", "anywhere"): (["cum", "fass", "pee", "poo", "tit"], ["poop", "tits", "titt"]),
    ("crude", "at_start"): ([], ["cum", "pee", "poo", "tit"]),
    ("slurs", "at_start"): ([], ["poof"]),
    ("words", "whole"): ([], ["teepee", "toupee"]),
}


def _byte_sorted(words):
    return sorted(words, key=lambda w: w.encode())


def _list_text(key, words):
    """A TOML array line block, wrapped at 78 columns (the values alone are hashed)."""
    if not words:
        return "%s = []\n" % key
    lines, cur = [], " "
    for w in words:
        item = ' "%s",' % w
        if len(cur) + len(item) > 78:
            lines.append(cur)
            cur = " "
        cur += item
    lines.append(cur)
    return "%s = [\n%s\n]\n" % (key, "\n".join(" " + l.strip() for l in lines))


def _set_list(text, table, key, words):
    """Replaces the array `key` inside [<kind>.<table>] of a TOML text written in the shipped style."""
    import re
    start = text.index("." + table + "]\n")
    nxt = text.find("\n[", start + 1)
    end = len(text) if nxt < 0 else nxt + 1
    block = text[start:end]
    m = re.search(r"(?m)^%s = \[(?:[^\]]|\n)*?\]\n" % re.escape(key), block)
    assert m, (table, key)
    block = block[:m.start()] + _list_text(key, words) + block[m.end():]
    return text[:start] + block + text[end:]


def _toml_lists(text, kind):
    return tomllib.loads(text)[kind]


def placenames_pre_019(files):
    """True when a placenames/ directory's files are older than 0.19.0 (the church words are still in common)."""
    for name, text in files:
        d = tomllib.loads(text).get("placename", {})
        if "common" in d:
            return "Abbess" in d["common"]["possessors"]
    return False


def name_filters_pre_019(files):
    for name, text in files:
        d = tomllib.loads(text).get("name_filter", {})
        if "words" in d:
            return "suck" in d["words"]["whole"]
    return False


def grim_split_019(common):
    """(kept, moved) per list of P1-06 r6 15.2: of the grim words of each common list, in list order, every third
    starting with the first stays."""
    kept, moved = {}, {}
    for f, cls in GRIM_019.items():
        grim = [w for w in common[f] if w in cls]
        kept[f] = grim[0::3]
        moved[f] = [w for j, w in enumerate(grim) if j % 3 != 0]
    return kept, moved


def placenames_019(files):
    """The 0.19.0 placenames/ files from the 0.18.0 ones (P1-06 r6 15.1 to 15.3)."""
    files = dict(files)
    owner = {}
    for name, text in files.items():
        for did in tomllib.loads(text).get("placename", {}):
            owner[did] = name
    defs = {did: dict(tomllib.loads(files[n])["placename"][did]) for did, n in owner.items()}
    new = {did: {f: list(t[f]) for f in PLACENAME_LISTS} for did, t in defs.items()}
    for (did, f), words in CHURCH_019.items():
        for w in words:
            new[did][f].remove(w)
    # 15.2 on the common lists after 15.1 (no church word is grim)
    kept, moved = grim_split_019(new["common"])
    for f in GRIM_019:
        assert kept[f] == GRIM_KEPT_019[f], (f, kept[f])
        assert len(moved[f]) == GRIM_MOVED_COUNTS_019[f], (f, moved[f])
        new["common"][f] = [w for w in new["common"][f] if w not in moved[f]]
        new["trait_barren"][f] = _byte_sorted(set(new["trait_barren"][f]) | set(moved[f]))
    # 15.3
    new["wood"]["suffixes"].remove("hanger")
    new["trait_high"]["adjectives"].remove("High")
    new["common"]["places"].remove("Cleft")
    new["common"]["places"].remove("Gap")
    new["trait_high"]["places"] = _byte_sorted(new["trait_high"]["places"] + ["Cleft"])
    out = dict(files)
    for did in new:
        for f in PLACENAME_LISTS:
            if new[did][f] != defs[did][f]:
                out[owner[did]] = _set_list(out[owner[did]], did, f, new[did][f])
    for did in new:   # the edit is exact: every list now holds the new words
        got = tomllib.loads(out[owner[did]])["placename"][did]
        assert all(got[f] == new[did][f] for f in PLACENAME_LISTS), did
    return sorted(out.items(), key=lambda kv: kv[0].encode())


def name_filters_r16(files):
    """True when a name_filters/ directory holds the 0.19.0 files of the first P3-10 round (r10 applied, r13 not:
    `pee` is still a crude.at_start entry)."""
    for name, text in files:
        d = tomllib.loads(text).get("name_filter", {})
        if "crude" in d:
            return "pee" in d["crude"]["at_start"]
    return False


def name_filters_019(files, edits=None):
    """The 0.19.0 name_filters/ files from the 0.18.0 ones (P3-02 r10 12.1, then r13 13.1); with `edits`, only
    those edits."""
    if edits is None:
        return name_filters_019(name_filters_019(files, NAME_FILTER_EDITS_019), NAME_FILTER_EDITS_019_R13)
    out = dict(files)
    for (did, f), (add, rem) in edits.items():
        name = did + ".toml"
        cur = tomllib.loads(out[name])["name_filter"][did][f]
        assert all(w not in cur for w in add) and all(w in cur for w in rem), (did, f)
        words = _byte_sorted([w for w in cur if w not in rem] + add)
        out[name] = _set_list(out[name], did, f, words)
        assert tomllib.loads(out[name])["name_filter"][did][f] == words
    return sorted(out.items(), key=lambda kv: kv[0].encode())


def git_dir_files(commit, path):
    """[(name, text)] of the *.toml files of a directory of the repository at a commit."""
    import subprocess
    rel = os.path.relpath(path, REPO).replace(os.sep, "/")
    names = subprocess.run(["git", "-C", REPO, "ls-tree", "--name-only", commit + ":" + rel],
                           check=True, capture_output=True, text=True).stdout.split()
    out = []
    for name in sorted((n for n in names if n.endswith(".toml")), key=lambda n: n.encode()):
        text = subprocess.run(["git", "-C", REPO, "show", "%s:%s/%s" % (commit, rel, name)],
                              check=True, capture_output=True).stdout.decode("utf-8")
        out.append((name, text))
    return out


def copied_files_019(path, directory, files):
    """The files of a copied kind directory for the served version, given what the disk holds (P3-10)."""
    if directory not in ("placenames", "name_filters"):
        return files
    if directory == "name_filters" and name_filters_024(files) and not at_least("0.24.0"):
        # P5-03 12.3: the disk holds 0.24.0 files; an older version reads them as at the P5-03 base
        if os.path.normpath(path).startswith(os.path.normpath(REPO)):
            return copied_files_019(path, directory, git_dir_files(P5_03_BASE_COMMIT, path))
        raise DataError("%s: 0.24.0 files, cannot serve %s" % (path, DATA_VERSION))
    if directory == "name_filters" and at_least("0.24.0") and not name_filters_024(files) and not pre21_any(files):
        PRE_015_USED.add(path)   # 0.23.0 files on disk, 0.24.0 served: 12.3's word
        return name_filters_add_024(files)
    pre21 = placenames_pre_021 if directory == "placenames" else name_filters_pre_021
    if not pre21(files):   # P5-01: the disk holds 0.21.0 files
        if at_least("0.21.0"):
            return files
        if os.path.normpath(path).startswith(os.path.normpath(REPO)):
            return copied_files_019(path, directory, git_dir_files(P5_01_BASE_COMMIT, path))
        raise DataError("%s: 0.21.0 files, cannot serve %s" % (path, DATA_VERSION))
    if at_least("0.21.0"):   # 0.20.0 files on disk, 0.21.0 served: the spec's edits
        PRE_015_USED.add(path)
        return placenames_021(files) if directory == "placenames" else name_filters_019(files, NAME_FILTER_EDITS_021)
    pre = placenames_pre_019 if directory == "placenames" else name_filters_pre_019
    if directory == "name_filters" and at_least("0.19.0") and not pre(files) and not name_filters_r16(files):
        # P3-11: the disk holds 0.19.0 or 0.20.0 files
        if DATA_VERSION == "0.20.0" and name_filters_pre_020(files):
            PRE_015_USED.add(path)
            return name_filters_019(files, NAME_FILTER_EDITS_020)
        if DATA_VERSION == "0.19.0" and not name_filters_pre_020(files):
            if os.path.normpath(path).startswith(os.path.normpath(REPO)):
                return git_dir_files(P3_11_BASE_COMMIT, path)
            raise DataError("%s: 0.20.0 files, cannot serve %s" % (path, DATA_VERSION))
        return files
    if DATA_VERSION == "0.20.0" and directory == "name_filters":
        raise DataError("%s: files older than 0.19.0, cannot serve %s" % (path, DATA_VERSION))
    if at_least("0.19.0"):
        if pre(files):
            PRE_015_USED.add(path)
            return placenames_019(files) if directory == "placenames" else name_filters_019(files)
        if directory == "name_filters" and name_filters_r16(files):   # P3-10 r3: the first round's files
            PRE_015_USED.add(path)
            return name_filters_019(files, NAME_FILTER_EDITS_019_R13)
        return files
    if not pre(files):   # an earlier version served over 0.19.0 files on disk
        if os.path.normpath(path).startswith(os.path.normpath(REPO)):
            return git_dir_files(P3_10_BASE_COMMIT, path)
        raise DataError("%s: 0.19.0 files, cannot serve %s" % (path, DATA_VERSION))
    return files


def write_root_019(dest):
    """Writes a scratch 0.19.0 data root (P3-10) into dest (outside the repository): game/data with the 0.19.0
    listings of races/, cultures/, history/ and the 0.19.0 copied kinds."""
    import shutil
    assert DATA_VERSION == "0.19.0"
    dest = os.path.abspath(dest)
    assert not dest.startswith(os.path.normpath(REPO) + os.sep), "refusing a scratch root inside the repository"
    if os.path.exists(dest):
        shutil.rmtree(dest)
    shutil.copytree(os.path.join(REPO, "game", "data"), dest)
    for kind in ("race", "culture", "history", "placename", "name_filter"):
        directory = KINDS[kind][0]
        p = os.path.join(dest, directory)
        files = read_kind_files("game/data", kind, False)
        shutil.rmtree(p)
        os.makedirs(p)
        for name, text in files:
            with open(os.path.join(p, name), "wb") as f:
                f.write(text.encode("utf-8"))

# ---------------------------------------------------------------------------
# The 0.20.0 listings (P3-11, docs/specs/p3-11-followups.md), derived from the
# 0.19.0 ones by the edits the specs state:
#   races     P3-01 r14 3.7 / P3-05 r16 23.2: awakening_population
#             int(0, 100000) after hidden_years, orc 1200, others 0.
#   cultures  P3-02 r15 14.1: the faerie onsets, nuclei, codas and forbidden
#             lists (every other line unchanged).
#   report    P3-06 r19 23.6 / P3-04 r17 21.2: the entries claim_settlements
#             (35) and settlements_ceded (55) appended after quiet_stretch,
#             in kind order (the spec states the place this time).
#   name_filter  P3-02 r15 14.3: slurs.anywhere +tard -retard; slurs.whole
#             -tard; crude.anywhere -bastard; words.whole +ova +van +via +von.
#   conflict, engine, peoples, chronology: unchanged.
# ---------------------------------------------------------------------------

AWAKENING_POPULATION_020 = {"elf": 0, "goblin": 0, "dwarf": 0, "faerie": 0, "human": 0, "orc": 1200}
RACE_FIELDS_020 = RACE_FIELDS_019 + (("awakening_population", 0, 100000),)
_i = [n for n, _ in RACE_019].index("hidden_years") + 1
RACE_020 = RACE_019[:_i] + (("awakening_population", INT),) + RACE_019[_i:]
assert RACE_020[_i + 1][0] == "settlement_kinds" and len(RACE_020) == len(RACE_019) + 1


def _race_020(name, text):
    rid = name[:-5]
    line = [l for l in text.splitlines(True) if l.startswith("hidden_years = ")][0]
    return _edit(text, line, line + "awakening_population = %d\n" % AWAKENING_POPULATION_020[rid])


RACE_FILES_020 = {n: _race_020(n, t) for n, t in RACE_FILES_019.items()}

FAERIE_LISTS_020 = (   # P3-02 r15 14.1, the four lines as listed
    ('onsets = ["", "p", "f", "w", "l", "s", "t", "b", "sh", "wh", "fl", "tw", "m"]',
     'onsets = ["", "v", "f", "w", "l", "s", "t", "gl", "sh", "wh", "fl", "tw", "m"]'),
    ('nuclei = ["i", "ee", "ie", "ia", "a", "y", "ae", "ou", "o"]',
     'nuclei = ["i", "i", "ee", "ie", "ie", "ia", "ia", "a", "a", "y", "y", "ae", "ae", "ou", "o", "o"]'),
    ('codas = ["", "", "ll", "n", "sh", "th", "p", "sk", "ss"]',
     'codas = ["", "", "ll", "n", "sh", "th", "sk", "ss"]'),
    ('forbidden = ["eee", "iee", "yy", "ouou", "shsh"]',
     'forbidden = ["eee", "iee", "yy", "ouou", "shsh", "ass"]'),
)
_fa = CULTURE_FILES_017["faerie.toml"]
for _old, _new in FAERIE_LISTS_020:
    _fa = _edit(_fa, _old, _new)
CULTURE_FILES_020 = dict(CULTURE_FILES_017, **{"faerie.toml": _fa})

KIND39_ENTRY = ('  { kind = "claim_settlements", salience = 35, template = "{claimant} laid claim to {settlement:all} of '
                '{holder} in {@region}." },\n')
KIND40_ENTRY = ('  { kind = "settlements_ceded", salience = 55, template = "{from} gave up {settlement:all} in {@region} '
                'to {to}." },\n')
_re = REPORT_FILE_019["report.toml"]
_last = [l for l in _re.splitlines(True) if '"quiet_stretch"' in l][0]
REPORT_FILE_020 = {"report.toml": _edit(_re, _last, _last + KIND39_ENTRY + KIND40_ENTRY)}
HISTORY_FILES_020 = dict(HISTORY_FILES_019, **REPORT_FILE_020)
SPEC_FILES_020 = {"race": RACE_FILES_020, "culture": CULTURE_FILES_020, "history": HISTORY_FILES_020}
KINDS_020 = dict(KINDS_019)
KINDS_020["race"] = ("races", RACE_020)

# P3-02 r15 14.3: (file, list) -> (added, removed), applied to the 0.19.0 files.
NAME_FILTER_EDITS_020 = {
    ("slurs", "anywhere"): (["tard"], ["retard"]),
    ("slurs", "whole"): ([], ["tard"]),
    ("crude", "anywhere"): ([], ["bastard"]),
    ("words", "whole"): (["ova", "van", "via", "von"], []),
}
# The commit whose game/data holds the 0.19.0 files (the P3-11 spec base, P3-10 merged): read from git when the
# disk holds 0.20.0 files and 0.19.0 is served.
P3_11_BASE_COMMIT = "dfa4e66"


def name_filters_pre_020(files):
    """True when a name_filters/ directory's files are older than 0.20.0 (`tard` still a slurs.whole entry)."""
    for name, text in files:
        d = tomllib.loads(text).get("name_filter", {})
        if "slurs" in d:
            return "tard" in d["slurs"]["whole"]
    return False


def pre_020(path, kind):
    """A race, culture or history directory older than 0.20.0 (P3-11): a race file without
    `awakening_population`, a faerie culture with the onset "p", or a report.toml without claim_settlements."""
    texts = {}
    for n in os.listdir(path):
        if n.endswith(".toml"):
            with open(os.path.join(path, n), encoding="utf-8") as f:
                texts[n] = f.read()
    if kind == "history":
        return "claim_settlements" not in texts.get("report.toml", "")
    if kind == "culture":
        return any('"p", "f"' in t for t in texts.values())
    return any("awakening_population" not in t for t in texts.values())


# ---------------------------------------------------------------------------
# The 0.21.0 listings (P5-01, docs/specs/p5-01-followups.md), derived from the
# 0.20.0 ones by the edits the specs state:
#   races     unchanged.
#   cultures  P3-02 r17 15.2: dwarven forbidden gains "didin", "ririn" (appended, in that order).
#   report    P3-05 r18 24: the template of became_city; P3-06 r22 24.6 / P3-04 r19 22.2: the entries
#             claim_dropped (15) and refuge_hidden (10) appended after settlements_ceded, in kind order.
#   conflict  P3-01 r15 3.8 / P3-06 r22 24.2, 24.5: quarrel_strength_permille 1000 and toll_spread_permille
#             200 after quarrel_min_settlements; check conflict_toll_spread after conflict_quarrel_strain_max.
#   engine    P3-01 r15 3.8 / P3-04 r19 22.3: flare_min_years 10 after quiet_min_years; check engine_flare_min
#             after engine_quiet_min.
#   name_filter  P3-02 r17 15.1: slurs.anywhere +fag -fagg; slurs.at_start -fag; famous.whole +shiva +hindi;
#             crude.whole +fannie.
#   placename P1-06 r10 16.1: 34 grim words move from common to trait_barren.
# ---------------------------------------------------------------------------

GRIM_KEPT_021 = {"prefixes": ["Ghost", "Wraith"], "adjectives": ["Accursed", "Cursed"], "places": [],
                 "possessors": [], "objects": ["Ghosts", "Shades"]}
GRIM_MOVED_021 = {   # P1-06 r10 16.1, as listed
    "prefixes": "Bale Charnel Dread Grief Lich Pall Sorrow Wight".split(),
    "adjectives": ("Bloodless Doleful Drear Fallen Forsaken Godless Grim Joyless Moaning Orphaned Shunned Stricken "
                   "Weeping Wretched").split(),
    "places": "Bane Grief Shame".split(),
    "possessors": "Gravedigger Mourner Widower".split(),
    "objects": ["Bones", "Grief", "Widows", "the Damned", "the Forsaken", "the Slain"],
}
assert sum(len(v) for v in GRIM_MOVED_021.values()) == 34
assert all(sorted(GRIM_KEPT_021[f] + GRIM_MOVED_021[f]) == sorted(GRIM_KEPT_019[f]) for f in GRIM_KEPT_019)
NAME_FILTER_EDITS_021 = {   # P3-02 r17 15.1: (file, list) -> (added, removed)
    ("slurs", "anywhere"): (["fag"], ["fagg"]),
    ("slurs", "at_start"): ([], ["fag"]),
    ("famous", "whole"): (["shiva", "hindi"], []),
    ("crude", "whole"): (["fannie"], []),
}
DWARVEN_FORBIDDEN_021 = (
    'forbidden = ["mmm", "rr", "dd", "kk", "rdt", "rkd", "rdd", "mmb", "kdr", "kthr", "dkh", "ndb", "rnk"]',
    'forbidden = ["mmm", "rr", "dd", "kk", "rdt", "rkd", "rdd", "mmb", "kdr", "kthr", "dkh", "ndb", "rnk", "didin", '
    '"ririn"]')
BECAME_CITY_021 = ("{settlement} grew into a city of {population}.", "{settlement} in {@region} grew into a city.")
KIND41_ENTRY = '  { kind = "claim_dropped", salience = 15, template = "{claimant} let the claim against {holder} over ' \
               '{@region} lapse." },\n'
KIND42_ENTRY = '  { kind = "refuge_hidden", salience = 10, template = "{@settlement} lay hidden from {@year} to ' \
               '{@end_year}." },\n'

CULTURE_FILES_021 = dict(CULTURE_FILES_020, **{"dwarven.toml": _edit(CULTURE_FILES_020["dwarven.toml"],
                                                                      *DWARVEN_FORBIDDEN_021)})
_re = _edit(REPORT_FILE_020["report.toml"], 'template = "%s" }' % BECAME_CITY_021[0],
            'template = "%s" }' % BECAME_CITY_021[1])
_last = [l for l in _re.splitlines(True) if '"settlements_ceded"' in l][0]
REPORT_FILE_021 = {"report.toml": _edit(_re, _last, _last + KIND41_ENTRY + KIND42_ENTRY)}
CONFLICT_NEW_021 = (("quarrel_strength_permille", 0, 1000), ("toll_spread_permille", 0, 500))
_i = [n for n, _, _ in CONFLICT_RANGES_019].index("quarrel_min_settlements") + 1
CONFLICT_RANGES_021 = CONFLICT_RANGES_019[:_i] + CONFLICT_NEW_021 + CONFLICT_RANGES_019[_i:]
CONFLICT_021 = tuple((n, CONFLICT_RECORDS.get(n, INT)) for n, _, _ in CONFLICT_RANGES_021)
assert len(CONFLICT_021) == 61 and CONFLICT_021[_i][0] == "quarrel_strength_permille" and \
    CONFLICT_021[_i + 2][0] == "capital_sacked_salience"
CONFLICT_FILE_021 = {"conflict.toml": _edit(CONFLICT_FILE_019["conflict.toml"], "quarrel_min_settlements = 2\n",
                                            "quarrel_min_settlements = 2\nquarrel_strength_permille = 1000\n"
                                            "toll_spread_permille = 200\n")}
CONFLICT_CHECKS_021 = CONFLICT_CHECKS_019 + (("conflict_toll_spread",
                                              "toll_spread_permille lets the victors lose more than the defeated"),)
_i = [n for n, _ in ENGINE_019].index("quiet_min_years") + 1
ENGINE_021 = ENGINE_019[:_i] + (("flare_min_years", INT),) + ENGINE_019[_i:]
assert ENGINE_021[_i + 1][0] == "simulated_min_permille"
ENGINE_FILE_021 = {"engine.toml": _edit(ENGINE_FILE_019["engine.toml"], "quiet_min_years = 50\n",
                                        "quiet_min_years = 50\nflare_min_years = 10\n")}
ENGINE_RANGES_021 = (("flare_min_years", 1, 1000),)
HISTORY_FILES_021 = dict(HISTORY_FILES_019, **CONFLICT_FILE_021, **ENGINE_FILE_021, **REPORT_FILE_021)
SPEC_FILES_021 = {"race": RACE_FILES_020, "culture": CULTURE_FILES_021, "history": HISTORY_FILES_021}
KINDS_021 = dict(KINDS_020)
KINDS_021["history"] = ("history", dict(KINDS_020["history"][1], conflict=CONFLICT_021, engine=ENGINE_021))
# The commit whose game/data holds the 0.20.0 files (the P5-01 spec base): read from git when the disk holds
# 0.21.0 files and an earlier version is served.
P5_01_BASE_COMMIT = "f29deec"


def engine_flare_messages(t):
    """P3-04 r19 22.3's named check engine_flare_min (levels in the order sparse, normal, dense)."""
    out = []
    if "flare_min_years" in t:
        for lv in ("sparse", "normal", "dense"):
            if not (t["flare_min_years"] <= t[lv]["flare_years"]):
                out.append("%s: flare_min_years must not be above flare_years" % lv)
    return out


def placenames_021(files):
    """The 0.21.0 placenames/ files from the 0.20.0 ones (P1-06 r10 16.1)."""
    files = dict(files)
    for name, text in list(files.items()):
        d = tomllib.loads(text).get("placename", {})
        for did, lists in (("common", None), ("trait_barren", None)):
            if did not in d:
                continue
            for f, words in GRIM_MOVED_021.items():
                cur = tomllib.loads(text)["placename"][did][f]
                if did == "common":
                    assert all(w in cur for w in words), (f, words)
                    new = [w for w in cur if w not in words]
                else:
                    new = _byte_sorted(set(cur) | set(words))
                text = _set_list(text, did, f, new)
        files[name] = text
    return sorted(files.items(), key=lambda kv: kv[0].encode())


def placenames_pre_021(files):
    """True when a placenames/ directory's files are older than 0.21.0 (`Sorrow` still a common prefix)."""
    for name, text in files:
        d = tomllib.loads(text).get("placename", {})
        if "common" in d:
            return "Sorrow" in d["common"]["prefixes"]
    return False


def name_filters_pre_021(files):
    """True when a name_filters/ directory's files are older than 0.21.0 (`fagg` still a slurs.anywhere entry)."""
    for name, text in files:
        d = tomllib.loads(text).get("name_filter", {})
        if "slurs" in d:
            return "fagg" in d["slurs"]["anywhere"]
    return False


def pre_021(path, kind):
    """A culture or history directory older than 0.21.0 (P5-01): no `didin` in dwarven.toml; no
    `flare_min_years` in engine.toml, no `toll_spread_permille` in conflict.toml or no claim_dropped entry."""
    texts = {}
    for n in os.listdir(path):
        if n.endswith(".toml"):
            with open(os.path.join(path, n), encoding="utf-8") as f:
                texts[n] = f.read()
    if kind == "history":
        return ("flare_min_years" not in texts.get("engine.toml", "")
                or "toll_spread_permille" not in texts.get("conflict.toml", "")
                or "claim_dropped" not in texts.get("report.toml", ""))
    if kind == "culture":
        return '"didin"' not in texts.get("dwarven.toml", "")
    return False


# The 0.22.0 listings (P5-12, docs/specs/p5-12-renamed-regions.md 4 and 4.1, P3-01 r16 5.4, P3-02 r19 16.2,
# P3-04 r21 23.2), derived from the 0.21.0 ones by the edits the specs state: the new fixed id
# history.renaming (renaming.toml), the report entry of kind 43 after refuge_hidden, human forbidden + ass.
RENAME_RULE = REC(fields("key:S"))[1] + (("kinds", ARR(STR)), ("values", ARR(STR))) + fields(
    "min_magnitude:I window_years:I min_salience:I capital:B chance_permille:I weight:I name_role:S namer_role:S") + \
    (("forms", ARR(STR)),)
RENAMING_022 = fields("region_max:I region_gap_years:I world_max_permille:I century_max_permille:I "
                      "century_years:I") + (("rules", ARR(REC(RENAME_RULE))),)
RENAMING_RANGES_022 = (("region_max", 0, 16), ("region_gap_years", 1, 100000), ("world_max_permille", 0, 1000),
                       ("century_max_permille", 0, 1000), ("century_years", 1, 100000))
RENAMING_HEAD_022 = dict(region_max=2, region_gap_years=300, world_max_permille=150, century_max_permille=50,
                         century_years=100)
# P5-12 4.1, transcribed column by column: key, kinds, values, min, window, min salience, capital, chance,
# weight, name_role, namer_role, forms.
RENAMING_RULES_022 = (
    ("conquest", ["civ_conquered"], [], 1, 1, 0, False, 1000, 100, "civilization", "conqueror",
     ["{name}'s Grave", "The Grave of {name}", "The End of {name}", "{name}'s Barrow"]),
    ("capital_sack", ["sacked"], [], 1, 1, 90, False, 700, 90, "settlement", "attacker",
     ["{name}'s Fall", "The Ashes of {name}", "The Ruin of {name}", "{name} Waste"]),
    ("great_sack", ["sacked"], ["dead"], 1500, 1, 0, False, 500, 80, "settlement", "attacker",
     ["{name}'s Pyre", "The Ashes of {name}", "{name} Waste", "The Burnt {noun}"]),
    ("slaughter", ["battle", "battle_at_pass"], ["winner_dead", "loser_dead"], 5000, 100, 0, False, 600, 70,
     "loser", "winner", ["The Red {noun}", "The Bloody {noun}", "{name}'s Bane", "The {noun} of the Slain"]),
    ("plague", ["plague_outbreak"], ["total"], 3000, 1, 0, False, 400, 60, "settlement", "civilization",
     ["The Emptied {noun}", "The Fever {noun}", "The Silent {noun}", "{name}'s Grave"]),
    ("heartland", ["became_city"], [], 1, 1, 0, True, 300, 50, "civilization", "civilization",
     ["{name}land", "The Heart of {name}", "The {name} Reach", "{name}'s Hearth"]),
)


def _toml_str_list(xs):
    return "[" + ", ".join('"%s"' % x for x in xs) + "]"


def renaming_rule_dicts(rows=RENAMING_RULES_022):
    names = [n for n, _ in RENAME_RULE]
    return [dict(zip(names, r)) for r in rows]


def _renaming_file_022():
    out = ["[history.renaming]\n"] + ["%s = %d\n" % kv for kv in RENAMING_HEAD_022.items()]
    for r in renaming_rule_dicts():
        out.append("\n[[history.renaming.rules]]\n")
        for n, v in r.items():
            if isinstance(v, bool):
                out.append("%s = %s\n" % (n, "true" if v else "false"))
            elif isinstance(v, int):
                out.append("%s = %d\n" % (n, v))
            elif isinstance(v, list):
                out.append("%s = %s\n" % (n, _toml_str_list(v)))
            else:
                out.append('%s = "%s"\n' % (n, v))
    return "".join(out)


KIND43_ENTRY = ('  { kind = "region_renamed", salience = 60, template = "{@causes:civ_conquered=the fall of a realm,'
                'sacked=the sack,battle=the slaughter,battle_at_pass=the slaughter,plague_outbreak=the plague,'
                'became_city=the rise of a city|After |, }{@region_former} became known as {@region}." },\n')
HUMAN_FORBIDDEN_022 = ('forbidden = ["eae", "aea", "ckc", "ldl", "ntd", "rdd", "ckb", "ckm"]',
                       'forbidden = ["eae", "aea", "ckc", "ldl", "ntd", "rdd", "ckb", "ckm", "ass"]')
CULTURE_FILES_022 = dict(CULTURE_FILES_021, **{"human.toml": _edit(CULTURE_FILES_021["human.toml"],
                                                                    *HUMAN_FORBIDDEN_022)})
_last = [l for l in REPORT_FILE_021["report.toml"].splitlines(True) if '"refuge_hidden"' in l][0]
REPORT_FILE_022 = {"report.toml": _edit(REPORT_FILE_021["report.toml"], _last, _last + KIND43_ENTRY)}
HISTORY_FILES_022 = dict(HISTORY_FILES_021, **REPORT_FILE_022, **{"renaming.toml": _renaming_file_022()})
SPEC_FILES_022 = {"race": RACE_FILES_020, "culture": CULTURE_FILES_022, "history": HISTORY_FILES_022}
KINDS_022 = dict(KINDS_021)
KINDS_022["history"] = ("history", dict(KINDS_021["history"][1], renaming=RENAMING_022))
# The commit whose game/data holds the 0.21.0 files (the P5-12 spec base, 0b78b31).
P5_12_BASE_COMMIT = "0b78b31"
RENAME_FORM_LETTERS = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ '")


def rename_form_ok(s):
    """Grammar rename_form (P5-12 4): letters, space, ', slots {name} and {noun} at most once each, a letter
    outside the slots, no leading, trailing or double space, no other brace; at most 48 bytes."""
    if len(s.encode()) > 48 or s.count("{name}") > 1 or s.count("{noun}") > 1:
        return False
    rest = s.replace("{name}", "").replace("{noun}", "")
    if any(ch not in RENAME_FORM_LETTERS for ch in rest):
        return False
    if not any(ch.isalpha() for ch in rest):
        return False
    return not (s.startswith(" ") or s.endswith(" ") or "  " in s)


def renaming_check_messages(t):
    """P3-01 r16 5.4's four named checks, in order; the messages of every failure (the loader reports the
    first)."""
    out = []
    rules = t["rules"]
    for check, field in (("renaming_rule_keys", "key"), ("renaming_rule_weights", "weight")):
        for j, r in enumerate(rules):
            for i in range(j):
                if rules[i][field] == r[field]:
                    out.append(("rules[%d] key '%s' repeats rules[%d]" if field == "key" else
                                "rules[%d] weight %d repeats rules[%d]") % (j, r[field], i))
                    break
    for i, r in enumerate(rules):
        for field in ("kinds", "values", "forms"):
            xs = r[field]
            for j, v in enumerate(xs):
                if v in xs[:j]:
                    out.append("rules[%d].%s[%d] '%s' repeats %s[%d]" % (i, field, j, v, field, xs.index(v)))
    for i, r in enumerate(rules):
        for j, f in enumerate(r["forms"]):
            if "{name}" in f and r["name_role"] == "":
                out.append("rules[%d].forms[%d] uses {name} but name_role is empty" % (i, j))
    return out


def pre_022(path, kind):
    """A culture or history directory older than 0.22.0 (P5-12): no renaming.toml or no region_renamed entry;
    human forbidden without `ass`."""
    names = os.listdir(path)
    if kind == "history":
        if "renaming.toml" not in names:
            return True
        with open(os.path.join(path, "report.toml"), encoding="utf-8") as f:
            return "region_renamed" not in f.read()
    if kind == "culture":
        with open(os.path.join(path, "human.toml"), encoding="utf-8") as f:
            return '"ckm", "ass"]' not in f.read()
    return False


def check_spec_022(c, races, cultures):
    """The 0.22.0 listings say what P5-12 4 and 4.1, P3-01 r16 5.4, P3-02 r19 16.2 and P3-04 r21 23.2 state."""
    h = parsed_spec("history")
    rn = h["renaming"]
    check_record(RENAMING_022, rn, "history.renaming")
    c.eq("history.renaming fields in order (P5-12 4)", list(rn), [n for n, _ in RENAMING_022])
    for n, lo, hi in RENAMING_RANGES_022:
        c.ok("history.renaming.%s inside int(%d, %d)" % (n, lo, hi), lo <= rn[n] <= hi)
    c.eq("history.renaming head values", {k: rn[k] for k in RENAMING_HEAD_022}, RENAMING_HEAD_022)
    c.eq("six rules in rule order", [r["key"] for r in rn["rules"]],
         ["conquest", "capital_sack", "great_sack", "slaughter", "plague", "heartland"])
    c.eq("rule fields in order (P5-12 4)", [list(r) for r in rn["rules"]], [[n for n, _ in RENAME_RULE]] * 6)
    c.eq("renaming passes its four named checks", renaming_check_messages(rn), [])
    for i, r in enumerate(rn["rules"]):
        c.ok("rules[%d] ranges" % i, 1 <= r["min_magnitude"] <= 10 ** 9 and 1 <= r["window_years"] <= 100000
             and 0 <= r["min_salience"] <= 100 and 0 <= r["chance_permille"] <= 1000 and 1 <= r["weight"] <= 1000
             and 1 <= len(r["kinds"]) <= 8 and len(r["values"]) <= 4 and 1 <= len(r["forms"]) <= 16)
        c.ok("rules[%d] forms by grammar rename_form" % i, all(rename_form_ok(f) for f in r["forms"]))
    bad = dict(rn, rules=[dict(r) for r in rn["rules"]])
    bad["rules"][3]["key"] = "conquest"
    bad["rules"][4]["weight"] = 90
    bad["rules"][2]["forms"] = bad["rules"][2]["forms"] + ["{name}'s Pyre"]
    bad["rules"][5]["name_role"] = ""
    c.eq("renaming checks' messages in check order", renaming_check_messages(bad),
         ["rules[3] key 'conquest' repeats rules[0]", "rules[4] weight 90 repeats rules[1]",
          "rules[2].forms[4] '{name}'s Pyre' repeats forms[0]",
          "rules[5].forms[0] uses {name} but name_role is empty",
          "rules[5].forms[1] uses {name} but name_role is empty",
          "rules[5].forms[2] uses {name} but name_role is empty",
          "rules[5].forms[3] uses {name} but name_role is empty"])
    for f, ok in (("{name}land", True), ("The Burnt {noun}", True), ("{name}{noun}", False), ("{name} ", False),
                  ("The  {noun}", False), ("{name}'s {name}", False), ("The {other}", False), ("A-B {noun}", False),
                  ("The Ashes of {name}", True), ("x" * 49, False), ("x" * 48, True)):
        c.eq("rename_form %r" % f, rename_form_ok(f), ok)
    evl = h["report"]["events"]
    c.eq("kind 43 entry last, after refuge_hidden (P3-04 r21 23.2, P5-12 7.1)",
         [(e["kind"], e["salience"]) for e in evl[-2:]], [("refuge_hidden", 10), ("region_renamed", 60)])
    c.eq("kind 43 template", evl[-1]["template"],
         "{@causes:civ_conquered=the fall of a realm,sacked=the sack,battle=the slaughter,battle_at_pass=the "
         "slaughter,plague_outbreak=the plague,became_city=the rise of a city|After |, }{@region_former} became "
         "known as {@region}.")
    hu = cultures["human"]["naming"]["forbidden"]
    c.eq("human forbidden gains ass, appended (P3-02 r19 16.2)", hu,
         ["eae", "aea", "ckc", "ldl", "ntd", "rdd", "ckb", "ckm", "ass"])
    old = {k: v for _, t in CULTURE_FILES_021.items() for k, v in tomllib.loads(t)["culture"].items()}
    c.eq("every other culture as at 0.21.0", {k: v for k, v in cultures.items() if k != "human"},
         {k: v for k, v in old.items() if k != "human"})



# The 0.23.0 listings (P5-02, docs/specs/p5-02-beasts-monsters.md 4.1 to 4.5, 9.3, 10.2, 21.2), derived from the
# 0.22.0 ones by the edits the spec states: the new required kinds beast (beasts/) and monster (monsters/), the
# fixed id history.monsters (monsters.toml), culture_naming.site_forms after settlement_forms, peoples' ghost_*
# after crisis_small_salience, the renaming forms of 21.2 and rule 7, the report entries of kinds 44 to 55 after
# region_renamed and kind 43's cause words. The shipped rates are interim (P5-02 17 step 9): after the engineer's
# calibration, BEASTS_023, MONSTERS_023 and MONSTERS_TUNING_023 are the one place to edit, then `golden`.
HABITAT = REC(fields("biome:S weight:I"))
BEAST = (("name", STR), ("plural", STR), ("habitat", ARR(HABITAT))) + fields(
    "density:I growth_permille:I menace:I food:I")
BEAST_RANGES = (("density", 0, 100000), ("growth_permille", 1, 1000), ("menace", 0, 1000), ("food", 0, 1000))
MONSTER = (("name", STR), ("noun", STR), ("grouped", BOOL), ("wake", STR), ("biomes", ARR(HABITAT)),
           ("wake_rate", INT), ("naming_culture", STR), ("name_forms", ARR(STR)), ("lair_forms", ARR(STR))) + fields(
    "might:I range_hops:I raid_permille:I toll_min_permille:I toll_max_permille:I kill_max:I") + (
    ("overruns", ARR(STR)),) + fields("overrun_permille:I active_years:I rewake_ppm:I lifespan_years:I max_alive:I") + (
    ("flare", BOOL),) + fields("hoard_permille:I")
MONSTER_RANGES = (("wake_rate", 0, 10000000), ("might", 1, 10000000), ("range_hops", 0, 4), ("raid_permille", 0, 1000),
                  ("toll_min_permille", 0, 1000), ("toll_max_permille", 0, 1000), ("kill_max", 0, 10000000),
                  ("overrun_permille", 0, 1000), ("active_years", 1, 100000), ("rewake_ppm", 0, 1000000),
                  ("lifespan_years", 0, 100000), ("max_alive", 0, 64), ("hoard_permille", 0, 1000))
PRESSURE = REC(fields("low:I normal:I high:I"))
MONSTERS_RANGES_023 = (
    ("living_permille", 0, 1000), ("undead_min_dead", 1, 10 ** 9), ("undead_window_years", 1, 100000),
    ("hunt_hops", 0, 8), ("hunt_levy_permille", 0, 1000), ("hunt_permille", 0, 1000), ("hunt_memory_years", 1, 100000),
    ("slay_max_permille", 0, 1000), ("drive_permille", 0, 1000), ("drive_years", 0, 100000),
    ("displace_per_kpeople", 0, 1000), ("displace_max_permille", 0, 1000), ("beast_decline_permille", 1, 1000),
    ("people_floor", 1, 100000), ("drag_k", 0, 10000), ("drag_max_permille", 0, 500), ("feed_k", 0, 10000),
    ("feed_max_permille", 0, 500), ("bold_on_permille", 1, 1000000), ("bold_off_permille", 0, 1000000),
    ("bold_permille", 0, 1000), ("bold_toll_permille", 0, 1000), ("outpost_permille", 0, 1000), ("outpost_max", 0, 16),
    ("outpost_quiet_years", 1, 100000))
# (r1, 6.4.1) the raid target's tuning, appended after outpost_quiet_years.
PREY_WEIGHT = REC(fields("hamlet:I village:I town:I city:I fortress:I"))
PREY_RANGES_023 = (("prey_capital_permille", 0, 1000), ("prey_hop_permille", 0, 1000), ("prey_might_ref", 1, 10000000),
                   ("prey_might_max_permille", 0, 10000))
MONSTERS_023 = ((("pressure", PRESSURE),) + tuple((n, INT) for n, _, _ in MONSTERS_RANGES_023) +
                (("prey_weight", PREY_WEIGHT),) + tuple((n, INT) for n, _, _ in PREY_RANGES_023))
# 4.3, the "Shipped" column.
MONSTERS_TUNING_023 = dict(
    pressure=dict(low=500, normal=1000, high=1600), living_permille=100, undead_min_dead=4000, undead_window_years=100,
    hunt_hops=2, hunt_levy_permille=50, hunt_permille=20, hunt_memory_years=30, slay_max_permille=900,
    drive_permille=300, drive_years=100, displace_per_kpeople=100, displace_max_permille=900,
    beast_decline_permille=200, people_floor=100, drag_k=40, drag_max_permille=100, feed_k=2, feed_max_permille=100,
    bold_on_permille=1500, bold_off_permille=800, bold_permille=100, bold_toll_permille=10, outpost_permille=30,
    outpost_max=2, outpost_quiet_years=200,
    prey_weight=dict(hamlet=1000, village=600, town=150, city=40, fortress=20), prey_capital_permille=250,
    prey_hop_permille=500, prey_might_ref=1500, prey_might_max_permille=4000)
# 4.1's table: id, name, plural, habitat (in the table's order), density, growth, menace, food.
BEASTS_023 = (
    ("bear", "Bear", "bears", (("temperate_forest", 900), ("wet_woodland", 700), ("highland_moor", 400), ("alpine", 500),
                               ("heath", 200)), 5, 60, 30, 30),
    ("boar", "Boar", "boar", (("temperate_forest", 800), ("wet_woodland", 900), ("grassland", 300), ("heath", 300),
                              ("marsh", 400), ("fen", 300)), 40, 250, 3, 100),
    ("deer", "Deer", "deer", (("temperate_forest", 900), ("wet_woodland", 600), ("grassland", 800), ("heath", 600),
                              ("steppe", 500), ("highland_moor", 500), ("alpine", 200), ("marsh", 200), ("fen", 200)),
     100, 250, 0, 60),
    ("giant_spider", "Giant spider", "giant spiders", (("wet_woodland", 800), ("temperate_forest", 400), ("marsh", 600),
                                                       ("fen", 500), ("blighted_waste", 900)), 10, 200, 40, 0),
    ("wolf", "Wolf", "wolves", (("temperate_forest", 700), ("wet_woodland", 500), ("grassland", 400), ("heath", 600),
                                ("steppe", 700), ("highland_moor", 800), ("alpine", 400), ("marsh", 100), ("fen", 100),
                                ("blighted_waste", 200)), 20, 150, 10, 0),
)
# 4.2's two tables joined: id, name, noun, grouped, wake, biomes, wake_rate, culture, name_forms, lair_forms, might,
# range, raid, toll min, toll max, kill_max, overruns, overrun, active, rewake, lifespan, max_alive, flare, hoard.
MONSTERS_KINDS_023 = (
    ("dragon", "Dragon", "a dragon of the old world", False, "wild",
     (("alpine", 800), ("highland_moor", 400), ("blighted_waste", 600)), 60, "elven",
     ("{root} the Black", "{root} the Pale", "{root}"), ("The Lair of {root}", "{root}'s Deep"),
     6000, 2, 60, 50, 200, 600, ("hamlet", "village", "town"), 600, 40, 2000, 0, 1, True, 40),
    ("giant", "Giant", "a giant of the mountains", False, "wild", (("alpine", 900), ("highland_moor", 500)), 25,
     "dwarven", ("{root} Stonehand", "{root}"), ("{root}'s Seat", "The Hall of {root}"),
     1200, 1, 100, 30, 100, 150, ("hamlet", "village"), 900, 150, 3000, 2500, 2, True, 8),
    ("ogre", "Ogre", "an ogre", False, "wild", (("grassland", 400), ("heath", 600), ("steppe", 700), ("highland_moor", 300)),
     40, "orcish", ("{root} the Glutton", "{root}"), ("{root}'s Larder", "The Pit of {root}"),
     200, 1, 150, 20, 60, 40, ("hamlet",), 900, 80, 8000, 1000, 3, False, 3),
    ("troll", "Troll", "a troll of the hills", False, "wild",
     (("highland_moor", 800), ("alpine", 600), ("temperate_forest", 400), ("heath", 300)), 50, "goblin",
     ("Old {root}", "{root}"), ("{root}'s Hollow", "The Den of {root}"),
     300, 1, 150, 20, 80, 60, ("hamlet", "village"), 900, 120, 5000, 2000, 3, False, 5),
    ("undead", "Undead", "a host of the restless dead", True, "dead", (), 15000, "human",
     ("The Host of {root}", "The Restless of {root}"), ("The Barrows of {root}", "{root}'s Howe"),
     1500, 1, 80, 30, 120, 200, ("hamlet", "village"), 900, 200, 3000, 1000, 2, False, 5),
    ("wyvern", "Wyvern", "a brood of wyverns", True, "wild", (("alpine", 700), ("heath", 500), ("highland_moor", 600)), 50,
     "goblin", ("The {root} Brood", "The Brood of {root}"), ("The Eyrie of {root}", "{root}'s Crag"),
     600, 2, 120, 20, 80, 50, ("hamlet",), 900, 60, 10000, 1000, 2, False, 2),
)
SITE_FORMS_023 = {   # 4.4
    "dwarven": ["{root} Watch", "The Tower of {root}"], "elven": ["{root} Watch", "The Spire of {root}"],
    "faerie": ["{root} Watch", "The Thorn of {root}"], "goblin": ["{root} Lookout", "{root} Stockade"],
    "human": ["{root} Watch", "{root} Tower"], "orcish": ["{root} Stockade", "The Watch of {root}"]}
GHOST_023 = (("ghost_permille", 0, 1000, 200), ("ghost_max_population", 0, 1000000, 600))   # 4.5 (r1: 600)
# 10.2: kinds 44 to 55 with salience and template. Kinds 48 and 49 read {@settlement} where 10.2 has {settlement}:
# neither kind has a role `settlement` (10.1), so 10.2's text fails template_problem (spec defect, READINGS D2).
REPORT_ENTRIES_023 = (
    ("monster_appeared", 45, "{monster}, {monster:kind}, made its lair in {@region}."),
    ("undead_rose", 50, "{@causes:battle=the slaughter,battle_at_pass=the slaughter,sacked=the sack|After |, }the dead "
                        "of {@region} rose as {monster}."),
    ("monster_woke", 25, "{monster} woke again in {@region}."),
    ("monster_slept", 10, "{monster} sank into sleep in {lair}."),
    ("monster_raid", 15, "{monster} fell upon {@settlement}; {dead?no one died|one died|# died}."),
    ("settlement_overrun", 50, "{monster} drove the people from {@settlement} and made it its lair; "
                               "{survivors?no one escaped|one escaped|# escaped}."),
    ("monster_slain", 55, "{slayer} hunted down and slew {monster}, {monster:kind}, in {@region}."),
    ("monster_driven_off", 30, "{civilization} drove {monster} back to its lair in {@region}."),
    ("monster_died", 30, "{monster}, {monster:kind}, died of great age in {@region}."),
    ("beasts_bold", 15, "In {@region} the {@beast} grew bold, and travellers from {settlement} went in fear; "
                        "{dead?none|one|#} died on the roads."),
    ("outpost_built", 12, "{civilization} raised {site} in {@region} against {enemy}."),
    ("outpost_abandoned", 10, "{civilization} abandoned {site}{reason?, the frontier having fallen quiet|, the land "
                              "being lost|, its realm having fallen}."),
)
KIND43_CAUSES_023 = ("became_city=the rise of a city|", "became_city=the rise of a realm's seat,"
                                                        "settlement_overrun=the ravage|")
# 21.2 and 9.3: the forms of rules 2 to 5 and rule 7.
RENAMING_FORMS_023 = {
    "capital_sack": ["{stem}'s Fall", "The Ashes of {name}", "The Ruin of {name}", "The Fallen {noun}"],
    "great_sack": ["{stem}'s Pyre", "The Ashes of {name}", "The Burnt {noun}", "The Cinder {noun}"],
    "slaughter": ["The Red {noun}", "The Bloody {noun}", "The Rout of {name}", "The {noun} of the Slain",
                  "The Weeping {noun}", "The Grim {noun}"],
    "plague": ["The Emptied {noun}", "The Fever {noun}", "The Silent {noun}", "{stem}'s Grave", "The Pale {noun}",
               "The Hollow {noun}"]}
RENAME_RULE_7 = ("ravage", ["settlement_overrun"], [], 1, 1, 0, False, 600, 75, "monster", "civilization",
                 ["The Haunt of {name}", "{name}'s Shadow", "The Forsaken {noun}", "The Hollow of {name}"])
RENAMING_RULES_023 = tuple(r[:11] + (RENAMING_FORMS_023.get(r[0], r[11]),) for r in RENAMING_RULES_022) + (RENAME_RULE_7,)


def _toml_value(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, int):
        return "%d" % v
    if isinstance(v, str):
        return '"%s"' % v
    if isinstance(v, dict):
        return "{ " + ", ".join("%s = %s" % (k, _toml_value(x)) for k, x in v.items()) + " }"
    return "[" + ", ".join(_toml_value(x) for x in v) + "]"


def _habitat_text(name, rows):
    if not rows:
        return "%s = []\n" % name
    return "%s = [\n" % name + "".join('  { biome = "%s", weight = %d },\n' % r for r in rows) + "]\n"


def beast_file_023(row):
    i, n, p, h, d, g, m, f = row
    return ("[beast.%s]\nname = \"%s\"\nplural = \"%s\"\n%sdensity = %d\ngrowth_permille = %d\nmenace = %d\nfood = %d\n"
            % (i, n, p, _habitat_text("habitat", h), d, g, m, f))


MONSTER_FIELD_NAMES = [n for n, _ in MONSTER]


def monster_dict_023(row):
    d = dict(zip(["id"] + MONSTER_FIELD_NAMES, row))
    d["biomes"] = [dict(biome=b, weight=w) for b, w in d["biomes"]]
    for k in ("name_forms", "lair_forms", "overruns"):
        d[k] = list(d[k])
    return d


def monster_file_023(row):
    d = monster_dict_023(row)
    out = ["[monster.%s]\n" % d["id"]]
    for n in MONSTER_FIELD_NAMES:
        if n == "biomes":
            out.append(_habitat_text("biomes", [(b["biome"], b["weight"]) for b in d["biomes"]]))
        else:
            out.append("%s = %s\n" % (n, _toml_value(d[n])))
    return "".join(out)


def monsters_file_023():
    return ("# Beasts and monsters (spec P5-02 4.3; P3-01, next revision). Generator 0.23.0; rates interim until the\n"
            "# calibration sweep (P5-02 17).\n[history.monsters]\n" +
            "".join("%s = %s\n" % (k, _toml_value(v)) for k, v in MONSTERS_TUNING_023.items()))


def _renaming_file_023():
    out = ["# Regions renamed by history (spec P5-12 section 4; P3-01 r16 5.4). Generator 0.22.0; forms of P5-02 "
           "21.2\n# and rule 7 (P5-02 9.3), generator 0.23.0.\n", "[history.renaming]\n"]
    out += ["%s = %d\n" % kv for kv in RENAMING_HEAD_022.items()]
    for r in renaming_rule_dicts(RENAMING_RULES_023):
        out.append("\n[[history.renaming.rules]]\n")
        out += ["%s = %s\n" % (n, _toml_value(v)) for n, v in r.items()]
    return "".join(out)


def _culture_023(cid, text):
    lines = text.split("\n")
    i = [k for k, l in enumerate(lines) if l.startswith("settlement_forms")]
    assert len(i) == 1
    lines.insert(i[0] + 1, "site_forms = %s" % _toml_value(SITE_FORMS_023[cid]))
    return "\n".join(lines)


CULTURE_FILES_023 = {n: _culture_023(n[:-5], t) for n, t in CULTURE_FILES_022.items()}
_pe = HISTORY_FILES_022["peoples.toml"]
assert _pe.endswith("crisis_small_salience = 25\n")
_re = _edit(REPORT_FILE_022["report.toml"], *KIND43_CAUSES_023)
assert _re.endswith("]\n")
_re = _re[:-2] + "".join('  { kind = "%s", salience = %d, template = "%s" },\n' % e for e in REPORT_ENTRIES_023) + "]\n"
HISTORY_FILES_023 = dict(HISTORY_FILES_022, **{
    "peoples.toml": _pe + "".join("%s = %d\n" % (n, v) for n, _, _, v in GHOST_023),
    "report.toml": _re, "renaming.toml": _renaming_file_023(), "monsters.toml": monsters_file_023()})
BEAST_FILES_023 = {r[0] + ".toml": beast_file_023(r) for r in BEASTS_023}
MONSTER_FILES_023 = {r[0] + ".toml": monster_file_023(r) for r in MONSTERS_KINDS_023}
SPEC_FILES_023 = {"race": RACE_FILES_020, "culture": CULTURE_FILES_023, "history": HISTORY_FILES_023,
                  "beast": BEAST_FILES_023, "monster": MONSTER_FILES_023}
CULTURE_NAMING_023 = REC(CULTURE_NAMING_017[1] + (("site_forms", ARR(STR)),))
PEOPLES_RANGES_023 = PEOPLES_RANGES_019 + tuple((n, lo, hi) for n, lo, hi, _ in GHOST_023)
PEOPLES_023 = tuple((n, PEOPLES_RECORDS.get(n, INT)) for n, _, _ in PEOPLES_RANGES_023)
KINDS_023 = dict(KINDS_022)
KINDS_023["culture"] = ("cultures", (("race", STR), ("name", STR), ("naming", CULTURE_NAMING_023)))
KINDS_023["history"] = ("history", dict(KINDS_022["history"][1], peoples=PEOPLES_023, monsters=MONSTERS_023))
KINDS_023["beast"] = ("beasts", BEAST)
KINDS_023["monster"] = ("monsters", MONSTER)
# The commit whose game/data holds the 0.22.0 files (the P5-02 base, p5/02-base at 73fc624).
P5_02_BASE_COMMIT = "73fc624"
BIOME_KEYS_023 = ("alpine", "blighted_waste", "fen", "grassland", "heath", "highland_moor", "lake", "marsh", "ocean",
                  "steppe", "temperate_forest", "wet_woodland")


def display_name_ok(s):
    """Grammar display_name (4.1): ASCII letters and space, starting with an upper-case letter; at most 32 bytes."""
    return 0 < len(s) <= 32 and "A" <= s[0] <= "Z" and all(ch == " " or ch.isascii() and ch.isalpha() for ch in s)


def beast_plural_ok(s):
    """Grammar beast_plural (4.1): lower-case ASCII letters and single spaces, starting and ending with a letter."""
    return (0 < len(s) <= 32 and all(ch == " " or "a" <= ch <= "z" for ch in s) and s[0] != " " and s[-1] != " "
            and "  " not in s)


def monster_noun_ok(s):
    """Grammar monster_noun (4.2): lower-case ASCII letters, space and ', starting with `a ` or `an `; 48 bytes."""
    return (len(s) <= 48 and all(ch in " '" or "a" <= ch <= "z" for ch in s)
            and (s.startswith("a ") or s.startswith("an ")))


def name_form_ok(s):
    """Grammar name_form (P3-02 3): ASCII letters, space, ' and -, exactly one {root}; no leading or trailing space,
    no two spaces in a row; at most 48 bytes."""
    if len(s.encode()) > 48 or s.count("{root}") != 1:
        return False
    rest = s.replace("{root}", "")
    return all(ch in " '-" or ch.isascii() and ch.isalpha() for ch in rest) and not (
        s.startswith(" ") or s.endswith(" ") or "  " in s)


def beast_check_messages(b, biomes=BIOME_KEYS_023):
    """beast_habitat_biomes (4.1): every message, in order (the loader reports the first)."""
    out = []
    seen = []
    for i, h in enumerate(b["habitat"]):
        if h["biome"] not in biomes:
            out.append("habitat[%d].biome '%s' is not a biome" % (i, h["biome"]))
        elif h["biome"] in seen:
            out.append("habitat[%d].biome '%s' is listed twice" % (i, h["biome"]))
        seen.append(h["biome"])
    return out


def monster_check_messages(m, biomes=BIOME_KEYS_023, cultures=tuple(SITE_FORMS_023)):
    """4.2's named checks in order: monster_biomes, monster_wild_habitat, monster_culture, monster_toll_ordered,
    monster_overruns_unique, monster_bare_form_last. Messages not given by the spec are this reference's reading
    (READINGS D1): the culture, toll and overruns checks."""
    out = []
    seen = []
    for i, h in enumerate(m["biomes"]):
        if h["biome"] not in biomes:
            out.append("biomes[%d].biome '%s' is not a biome" % (i, h["biome"]))
        elif h["biome"] in seen:
            out.append("biomes[%d].biome '%s' is listed twice" % (i, h["biome"]))
        seen.append(h["biome"])
    if m["wake"] == "wild" and not any(h["weight"] > 0 for h in m["biomes"]):
        out.append("a wild monster needs a biome with a weight above 0")
    if m["naming_culture"] not in cultures:
        out.append("naming_culture '%s' is not a culture" % m["naming_culture"])
    if m["toll_min_permille"] > m["toll_max_permille"]:
        out.append("toll_min_permille %d is above toll_max_permille %d" % (m["toll_min_permille"], m["toll_max_permille"]))
    for i, k in enumerate(m["overruns"]):
        if k in m["overruns"][:i]:
            out.append("overruns[%d] '%s' repeats overruns[%d]" % (i, k, m["overruns"].index(k)))
    for i, f in enumerate(m["name_forms"]):
        if f == "{root}" and i != len(m["name_forms"]) - 1:
            out.append("name_forms[%d] '{root}' must be the last form" % i)
    return out


def monsters_check_messages(t):
    """monsters_bold_ordered (4.3); the message is this reference's reading (READINGS D1)."""
    if t["bold_off_permille"] > t["bold_on_permille"]:
        return ["bold_off_permille %d is above bold_on_permille %d" % (t["bold_off_permille"], t["bold_on_permille"])]
    return []


def rename_form_ok_023(s):
    """P5-02 21.2: grammar rename_form gains the slot {stem}, at most once, not together with {name}."""
    if s.count("{stem}") > 1 or ("{stem}" in s and "{name}" in s):
        return False
    return rename_form_ok(s.replace("{stem}", "{name}"))


def pre_023(path, kind):
    """A culture or history directory older than 0.23.0 (P5-02): no site_forms in a culture file; no monsters.toml,
    no ghost_permille in peoples.toml, no monster_appeared entry or no ravage rule."""
    names = [n for n in os.listdir(path) if n.endswith(".toml")]
    texts = {}
    for n in names:
        with open(os.path.join(path, n), encoding="utf-8") as f:
            texts[n] = f.read()
    if kind == "history":
        return ("monsters.toml" not in names or "ghost_permille" not in texts.get("peoples.toml", "")
                or "monster_appeared" not in texts.get("report.toml", "")
                or '"ravage"' not in texts.get("renaming.toml", ""))
    if kind == "culture":
        return any("site_forms" not in t for t in texts.values())
    return False


def check_spec_023(c, races, cultures):
    """The 0.23.0 listings say what P5-02 4.1 to 4.5, 9.3, 10.2 and 21.2 state."""
    h = parsed_spec("history")
    mo = h["monsters"]
    check_record(MONSTERS_023, mo, "history.monsters")
    c.eq("history.monsters fields in order (4.3)", list(mo), [n for n, _ in MONSTERS_023])
    c.eq("history.monsters shipped values (4.3)", mo, MONSTERS_TUNING_023)
    for n, lo, hi in MONSTERS_RANGES_023 + PREY_RANGES_023:
        c.ok("history.monsters.%s inside int(%d, %d)" % (n, lo, hi), lo <= mo[n] <= hi)
    c.ok("prey_weight inside int(0, 1000) (r1 4.3)", all(0 <= v <= 1000 for v in mo["prey_weight"].values()))
    c.ok("pressure factors inside int(0, 10000)", all(0 <= v <= 10000 for v in mo["pressure"].values()))
    c.eq("history.monsters passes monsters_bold_ordered", monsters_check_messages(mo), [])
    c.eq("monsters_bold_ordered at off = on + 1", monsters_check_messages(dict(mo, bold_off_permille=1501)),
         ["bold_off_permille 1501 is above bold_on_permille 1500"])
    c.eq("monsters_bold_ordered passes at off = on", monsters_check_messages(dict(mo, bold_off_permille=1500)), [])
    be = parsed_spec("beast")
    c.eq("beast ids in canonical order (4.1: bear 1 ... wolf 5)", sorted(be, key=lambda k: k.encode()),
         ["bear", "boar", "deer", "giant_spider", "wolf"])
    for bid, b in be.items():
        check_record(BEAST, b, "beast." + bid)
        c.eq("beast.%s fields in order" % bid, list(b), [n for n, _ in BEAST])
        c.ok("beast.%s name by display_name" % bid, display_name_ok(b["name"]))
        c.ok("beast.%s plural by beast_plural" % bid, beast_plural_ok(b["plural"]))
        c.ok("beast.%s habitat 1 to 16 rows, weights 0 to 1000" % bid,
             1 <= len(b["habitat"]) <= 16 and all(0 <= x["weight"] <= 1000 for x in b["habitat"]))
        for n, lo, hi in BEAST_RANGES:
            c.ok("beast.%s.%s inside int(%d, %d)" % (bid, n, lo, hi), lo <= b[n] <= hi)
        c.eq("beast.%s passes beast_habitat_biomes" % bid, beast_check_messages(b), [])
    bad = dict(be["wolf"], habitat=be["wolf"]["habitat"] + [dict(biome="heath", weight=1), dict(biome="moon", weight=1)])
    c.eq("beast_habitat_biomes messages", beast_check_messages(bad),
         ["habitat[10].biome 'heath' is listed twice", "habitat[11].biome 'moon' is not a biome"])
    mk = parsed_spec("monster")
    c.eq("monster ids in canonical order (4.2: dragon 1 ... wyvern 6)", sorted(mk, key=lambda k: k.encode()),
         ["dragon", "giant", "ogre", "troll", "undead", "wyvern"])
    for mid, m in mk.items():
        check_record(MONSTER, m, "monster." + mid)
        c.eq("monster.%s fields in order" % mid, list(m), MONSTER_FIELD_NAMES)
        c.ok("monster.%s name by display_name" % mid, display_name_ok(m["name"]))
        c.ok("monster.%s noun by monster_noun" % mid, monster_noun_ok(m["noun"]))
        c.ok("monster.%s wake an enum value" % mid, m["wake"] in ("wild", "dead"))
        c.ok("monster.%s forms by name_form, 1 to 8" % mid,
             all(name_form_ok(f) for f in m["name_forms"] + m["lair_forms"])
             and 1 <= len(m["name_forms"]) <= 8 and 1 <= len(m["lair_forms"]) <= 8)
        c.ok("monster.%s overruns enum values, at most 3" % mid,
             len(m["overruns"]) <= 3 and all(k in ("hamlet", "village", "town") for k in m["overruns"]))
        for n, lo, hi in MONSTER_RANGES:
            c.ok("monster.%s.%s inside int(%d, %d)" % (mid, n, lo, hi), lo <= m[n] <= hi)
        c.eq("monster.%s passes its named checks" % mid, monster_check_messages(m), [])
    c.eq("two flare kinds, one dead kind", ([k for k, m in mk.items() if m["flare"]],
                                            [k for k, m in mk.items() if m["wake"] == "dead"]),
         (["dragon", "giant"], ["undead"]))
    d = mk["troll"]
    for edit, want in (
            (dict(biomes=d["biomes"] + [dict(biome="alpine", weight=1)]), ["biomes[4].biome 'alpine' is listed twice"]),
            (dict(biomes=[dict(biome="sky", weight=5)]),
             ["biomes[0].biome 'sky' is not a biome"]),
            (dict(biomes=[dict(biome="heath", weight=0)]), ["a wild monster needs a biome with a weight above 0"]),
            (dict(naming_culture="trollish"), ["naming_culture 'trollish' is not a culture"]),
            (dict(toll_min_permille=81), ["toll_min_permille 81 is above toll_max_permille 80"]),
            (dict(overruns=["hamlet", "hamlet"]), ["overruns[1] 'hamlet' repeats overruns[0]"]),
            (dict(name_forms=["{root}", "Old {root}"]), ["name_forms[0] '{root}' must be the last form"])):
        c.eq("troll with %s" % sorted(edit), monster_check_messages(dict(d, **edit)), want)
    c.eq("a dead kind needs no habitat", monster_check_messages(mk["undead"]), [])
    for cid, cu in cultures.items():
        nm = cu["naming"]
        c.eq("culture.%s site_forms after settlement_forms (4.4)" % cid, list(nm)[-2:], ["settlement_forms", "site_forms"])
        c.eq("culture.%s site_forms" % cid, nm["site_forms"], SITE_FORMS_023[cid])
        c.ok("culture.%s site_forms by name_form" % cid, all(name_form_ok(f) for f in nm["site_forms"]))
    old = {k: v for _, t in CULTURE_FILES_022.items() for k, v in tomllib.loads(t)["culture"].items()}
    c.eq("every culture as at 0.22.0 but for site_forms",
         {k: dict(v, naming={a: b for a, b in v["naming"].items() if a != "site_forms"}) for k, v in cultures.items()}, old)
    pe = h["peoples"]
    c.eq("peoples ghost_* appended after crisis_small_salience (4.5)", list(pe.items())[-3:],
         [("crisis_small_salience", 25), ("ghost_permille", 200), ("ghost_max_population", 600)])
    rn = h["renaming"]
    c.eq("seven rules in rule order", [r["key"] for r in rn["rules"]],
         ["conquest", "capital_sack", "great_sack", "slaughter", "plague", "heartland", "ravage"])
    c.eq("renaming passes its four named checks (0.23.0)", renaming_check_messages(rn), [])
    for i, r in enumerate(rn["rules"]):
        c.ok("rules[%d] forms by rename_form with {stem} (21.2)" % i, all(rename_form_ok_023(f) for f in r["forms"]))
    c.eq("rule 7 ravage (9.3)", [rn["rules"][6][n] for n, _ in RENAME_RULE], list(RENAME_RULE_7))
    for f, ok in (("{stem}'s Fall", True), ("{stem} {name}", False), ("{stem}{stem}", False), ("The {stem}", True)):
        c.eq("rename_form 0.23.0 %r" % f, rename_form_ok_023(f), ok)
    evl = h["report"]["events"]
    c.eq("kinds 44-55 entries after region_renamed (10.2)", [(e["kind"], e["salience"], e["template"]) for e in evl[-12:]],
         [tuple(e) for e in REPORT_ENTRIES_023])
    c.eq("report lists 55 kinds", len(evl), 55)
    c.ok("kind 43 cause words: a realm's seat, the ravage (21.2)",
         "became_city=the rise of a realm's seat,settlement_overrun=the ravage|After |, }" in evl[42]["template"])
    c.eq("races as at 0.22.0", races, {k: v for _, t in RACE_FILES_020.items() for k, v in tomllib.loads(t)["race"].items()})
    for k in ("chronology", "conflict", "engine"):
        c.eq("history.%s as at 0.22.0" % k, h[k], tomllib.loads(HISTORY_FILES_022[k + ".toml"])["history"][k])

# ---------------------------------------------------------------------------
# The 0.24.0 listings (P5-03, docs/specs/p5-03-race-relations.md r1: 4.1, 4.2, 8.2, 9.2, 9.4, 9.5, 11.1, 11.3, 12,
# 13), derived from the 0.23.0 ones by the edits the spec states, at the engineer's calibrated values (22 step 10,
# integration phase 2): RELATIONS_RANGES_024, MONSTERS_NEW_RANGES_024, PEOPLES_NEW_RANGES_024 and the tables below
# are the one place to edit, then `golden`.
# ---------------------------------------------------------------------------
# 4.1: race field stances after culture (the last field).
STANCE = REC(fields("race:S value:I"))
RACE_024 = RACE_020 + (("stances", ARR(STANCE)),)
assert RACE_020[-1][0] == "culture"
STANCES_024 = {"elf": [("faerie", 200)], "faerie": [("elf", 200)], "dwarf": [("goblin", -100)],
               "goblin": [("dwarf", -100)], "human": [], "orc": []}


def stances_text(rows):
    return "stances = [%s]" % ", ".join('{ race = "%s", value = %d }' % r for r in rows)


def _race_024(n, text):
    lines = text.rstrip("\n").split("\n")
    i = [k for k, l in enumerate(lines) if l.startswith("culture = ")]
    assert len(i) == 1
    lines.insert(i[0] + 1, stances_text(STANCES_024[n[:-5]]))
    return "\n".join(lines) + "\n"


RACE_FILES_024 = {n: _race_024(n, t) for n, t in RACE_FILES_020.items()}
# 4.2: history.relations, every field required, in this order (name, lo, hi, shipped).
RELATIONS_RANGES_024 = (
    ("hatred_max", -2000, 0, -500), ("friend_min", 0, 2000, 150), ("band_margin", 0, 500, 50),
    ("memory_min", -2000, 0, -500), ("memory_max", 0, 2000, 500), ("memory_decay_per_century", 0, 10000, 20),
    ("war_memory", 0, 1000, 75), ("conquest_memory", 0, 1000, 150), ("sack_memory", 0, 1000, 30),
    ("capture_memory", 0, 1000, 15), ("expel_memory", 0, 1000, 90), ("shelter_memory", 0, 1000, 45),
    ("meet_hops", -1, 8, 8), ("migrate_hops", 0, 8, 2), ("migrate_min", 1, 100000, 40),
    ("welcome_min", -2000, 2001, -200), ("subject_years", 0, 100000, 200),
    ("conquest_settlers_permille", 0, 1000, 100), ("drift_permille_per_century", 0, 1000, 100),
    ("fade_permille", 0, 1000, 20), ("mixed_min_permille", 1, 1001, 100), ("expel_permille", 0, 1000, 3),
    ("expel_share_max", 0, 1000, 500))
RELATIONS_024 = tuple((n, INT) for n, _, _, _ in RELATIONS_RANGES_024)
RELATIONS_TUNING_024 = {n: v for n, _, _, v in RELATIONS_RANGES_024}
# 4.2 / 7.4: the zero fixture tuning (with every race's stances empty).
RELATIONS_ZERO_024 = dict(RELATIONS_TUNING_024, war_memory=0, conquest_memory=0, sack_memory=0, capture_memory=0,
                          expel_memory=0, shelter_memory=0, meet_hops=-1, welcome_min=2001, subject_years=100000,
                          mixed_min_permille=1001, expel_permille=0)
# 9.4 (and 9.1, 9.2, 9.3, 14.1): history.monsters fields after prey_might_max_permille.
MONSTERS_NEW_RANGES_024 = (
    ("raid_chain_years", 1, 100000, 300), ("toll_over_permille", 0, 1000, 250), ("hunt_raid_permille", 0, 1000, 5),
    ("hunt_max_permille", 0, 1000, 200), ("fear_raids", 1, 100, 2), ("fear_years", 1, 100000, 50),
    ("fear_permille", 0, 1000, 400), ("overrun_people_ref", 1, 1000000, 800))
MONSTERS_024 = MONSTERS_023 + tuple((n, INT) for n, _, _, _ in MONSTERS_NEW_RANGES_024)
MONSTERS_TUNING_024 = dict(MONSTERS_TUNING_023, **{n: v for n, _, _, v in MONSTERS_NEW_RANGES_024})
# 11.1 and 11.2: history.peoples fields after ghost_max_population.
PEOPLES_NEW_RANGES_024 = (("abandon_reach_hops", 0, 16, 0), ("abandon_loss_min_permille", 0, 1000, 50),
                          ("abandon_loss_max_permille", 0, 1000, 200), ("ghost_fade_years", 0, 100000, 0))
PEOPLES_RANGES_024 = PEOPLES_RANGES_023 + tuple((n, lo, hi) for n, lo, hi, _ in PEOPLES_NEW_RANGES_024)
PEOPLES_024 = tuple((n, PEOPLES_RECORDS.get(n, INT)) for n, _, _ in PEOPLES_RANGES_024)
# 13: beast bold_phrases after food.
BEAST_024 = BEAST + (("bold_phrases", ARR(STR)),)
BOLD_PHRASES_024 = {
    "bear": ["broke into the stores of", "came down on the herds of"],
    "boar": ["trampled the fields of", "tore up the gardens of"],
    "deer": ["strayed into the fields of"],
    "giant_spider": ["spun their webs across the paths of", "lay in wait on the roads from"],
    "wolf": ["hunted the roads from", "grew bold and stalked the herds of"]}
# 9.5: monster raid_phrases after hoard_permille (the last field); 12.1, 12.2 name forms; 9.2 hoards of undead and
# wyvern 0; 12.2 the undead lair forms.
MONSTER_024 = MONSTER + (("raid_phrases", ARR(STR)),)
assert MONSTER[-1][0] == "hoard_permille"
RAID_PHRASES_024 = {
    "dragon": ["fell in fire upon", "burned the fields of", "came down in flame on"],
    "giant": ["came down from the heights upon", "hurled stones upon", "fell upon"],
    "ogre": ["fell upon", "plundered the stores of", "carried off the herds of"],
    "troll": ["came down by night on", "fell upon", "prowled the edges of"],
    "undead": ["came out of the barrows against", "walked by night in", "crept into"],
    "wyvern": ["stooped on", "swept down on", "came screaming down on"]}
NAME_FORMS_024 = {
    "dragon": ["{root} the Black", "{root} the Pale", "{root} the Red", "{root} the Old", "{root} Ashwing",
               "{root} the Gold-Eyed", "{root}"],
    "giant": ["{root} Stonehand", "{root} the Tall", "{root} Cliffbreaker", "{root} the Grey", "{root}"],
    "ogre": ["{root} the Glutton", "{root} the Gaunt", "{root} the Hungering", "{root} Bonebreaker", "{root} the Fat",
             "{root}"],
    "troll": ["Old {root}", "{root} the Grey", "{root} Stonehide", "{root} of the Fells", "Mossback {root}", "{root}"],
    "wyvern": ["The {root} Brood", "The Brood of {root}", "The Screamers of {root}"],
    "undead": ["The Host of {place}", "The Restless of {place}", "The Dead of {place}"]}
LAIR_FORMS_024 = {"undead": ["The Barrows of {root}", "The Howes of {root}"]}
HOARD_024 = {"undead": 0, "wyvern": 0}


def _monster_row_024(row):
    d = monster_dict_023(row)
    k = d["id"]
    d["name_forms"] = list(NAME_FORMS_024[k])
    d["lair_forms"] = list(LAIR_FORMS_024.get(k, d["lair_forms"]))
    d["hoard_permille"] = HOARD_024.get(k, d["hoard_permille"])
    d["raid_phrases"] = list(RAID_PHRASES_024[k])
    return d


def monster_file_024(row):
    d = _monster_row_024(row)
    out = ["[monster.%s]\n" % d["id"]]
    for n, _ in MONSTER_024:
        if n == "biomes":
            out.append(_habitat_text("biomes", [(b["biome"], b["weight"]) for b in d["biomes"]]))
        else:
            out.append("%s = %s\n" % (n, _toml_value(d[n])))
    return "".join(out)


def beast_file_024(row):
    return beast_file_023(row) + "bold_phrases = %s\n" % _toml_value(BOLD_PHRASES_024[row[0]])


# 8.2, 9.5, 11.3, 13: the report entries of kinds 56 to 62 after outpost_abandoned; kinds 48 and 53 reworded.
REPORT_ENTRIES_024 = (
    ("settlement_fled", 45, "For fear of {monster}, the people of {@settlement} left their homes."),
    ("peoples_met", 25, "The {race_a:plural} first met the {race_b:plural} in {@region}{band?, and hated them from the "
                        "first|, and were wary of them|, and were glad of them}."),
    ("disposition_turned", 55, "{@causes:war_won=the wars,peace=the wars,truce=the wars,civ_conquered=the conquests,"
                               "sacked=the sacks,settlement_captured=the conquests,people_expelled=the expulsions,"
                               "refugees_arrived=years of shelter given|After |, }the {race_a:plural} and the "
                               "{race_b:plural} {band?came to hate one another|grew wary of one another|became friends}."),
    ("refugees_arrived", 25, "{@causes:sacked=the sack,settlement_captured=the fall,settlement_overrun=the ravage,"
                             "people_expelled=the expulsion|After |, }{people} {race:plural} from {origin} found shelter "
                             "in {settlement}{standing?, held as subjects by|, tolerated by|, welcomed by|, among their "
                             "own kin of} {civilization}."),
    ("settlement_mixed", 25, "{@causes:settlement_captured=the conquest,settlements_ceded=the cession,region_ceded=the "
                             "cession|After |, }the {race:plural} and the {ruling:plural} lived side by side in "
                             "{settlement}."),
    ("standing_changed", 15, "In {settlement}, {civilization} {standing?held the|tolerated the|accepted the} "
                             "{race:plural}{standing? as subjects|| as equals}."),
    ("people_expelled", 40, "{civilization} drove the {race:plural} out of {settlement}; {people} left."),
)
KIND48_TEMPLATE_024 = "{monster} {monster:raid} {@settlement}; {dead?no one died|one died|# died}."
KIND53_TEMPLATE_024 = "In {@region} the {@beast} {@beast:bold} {settlement}; {dead?no one|one|#} died."
# 12.3: rule 7's form The Hollow of {name} becomes {name}'s Waste.
RENAME_RULE_7_024 = RENAME_RULE_7[:11] + (["The Haunt of {name}", "{name}'s Shadow", "The Forsaken {noun}",
                                           "{name}'s Waste"],)
RENAMING_RULES_024 = RENAMING_RULES_023[:6] + (RENAME_RULE_7_024,)


def _renaming_file_024():
    out = ["# Regions renamed by history (spec P5-12 section 4; P3-01 r16 5.4). Generator 0.22.0; forms of P5-02 "
           "21.2\n# and rule 7 (P5-02 9.3), generator 0.23.0; rule 7's {name}'s Waste (P5-03 12.3), generator 0.24.0.\n",
           "[history.renaming]\n"]
    out += ["%s = %d\n" % kv for kv in RENAMING_HEAD_022.items()]
    for r in renaming_rule_dicts(RENAMING_RULES_024):
        out.append("\n[[history.renaming.rules]]\n")
        out += ["%s = %s\n" % (n, _toml_value(v)) for n, v in r.items()]
    return "".join(out)


def relations_file_024(t=None):
    t = RELATIONS_TUNING_024 if t is None else t
    return ("# Race relations, mixed settlements and migrations (spec P5-03 4.2). Generator 0.24.0; values of the\n"
            "# calibration sweep (P5-03 22 step 10).\n[history.relations]\n" +
            "".join("%s = %d\n" % (n, t[n]) for n, _ in RELATIONS_024))


def monsters_file_024():
    return ("# Beasts and monsters (spec P5-02 4.3; P5-03 9.4). Generator 0.24.0; rates of the calibration sweep\n"
            "# (P5-03 22 step 10).\n[history.monsters]\n" +
            "".join("%s = %s\n" % (k, _toml_value(v)) for k, v in MONSTERS_TUNING_024.items()))


def _report_024(text):
    old48 = REPORT_ENTRIES_023[5 - 1][2]
    old53 = REPORT_ENTRIES_023[10 - 1][2]
    assert old48.startswith("{monster} fell upon") and old53.startswith("In {@region} the {@beast} grew bold")
    text = _edit(text, old48, KIND48_TEMPLATE_024)
    text = _edit(text, old53, KIND53_TEMPLATE_024)
    assert text.endswith("]\n")
    return text[:-2] + "".join('  { kind = "%s", salience = %d, template = "%s" },\n' % e
                               for e in REPORT_ENTRIES_024) + "]\n"


HISTORY_FILES_024 = dict(HISTORY_FILES_023, **{
    "peoples.toml": HISTORY_FILES_023["peoples.toml"] + "".join("%s = %d\n" % (n, v)
                                                                 for n, _, _, v in PEOPLES_NEW_RANGES_024),
    "report.toml": _report_024(HISTORY_FILES_023["report.toml"]), "renaming.toml": _renaming_file_024(),
    "monsters.toml": monsters_file_024(), "relations.toml": relations_file_024()})
BEAST_FILES_024 = {r[0] + ".toml": beast_file_024(r) for r in BEASTS_023}
MONSTER_FILES_024 = {r[0] + ".toml": monster_file_024(r) for r in MONSTERS_KINDS_023}
SPEC_FILES_024 = {"race": RACE_FILES_024, "culture": CULTURE_FILES_023, "history": HISTORY_FILES_024,
                  "beast": BEAST_FILES_024, "monster": MONSTER_FILES_024}
KINDS_024 = dict(KINDS_023)
KINDS_024["race"] = ("races", RACE_024)
KINDS_024["history"] = ("history", dict(KINDS_023["history"][1], peoples=PEOPLES_024, monsters=MONSTERS_024,
                                        relations=RELATIONS_024))
KINDS_024["beast"] = ("beasts", BEAST_024)
KINDS_024["monster"] = ("monsters", MONSTER_024)
# 12.3: bird in words.toml's whole list.
FILTER_WORDS_024 = ("bird",)
# The commit whose game/data holds the 0.23.0 files (origin/p5/03-spec, main at 0.23.0 plus the spec).
P5_03_BASE_COMMIT = "87fbda9"


def phrase_ok(s):
    """Grammar phrase (9.5, 13): lower-case ASCII letters, single spaces, ' and -, starting and ending with a
    letter; string(48)."""
    return (0 < len(s.encode()) <= 48 and all(ch in " '-" or "a" <= ch <= "z" for ch in s)
            and "a" <= s[0] <= "z" and "a" <= s[-1] <= "z" and "  " not in s)


def monster_form_ok_024(s, dead):
    """name_form (P3-02 4) with {place} for monster forms (12.2): a form of a dead kind holds {place} exactly once
    and no {root}; a wild kind's forms as before."""
    if "{place}" in s:
        return s.count("{place}") == 1 and "{root}" not in s and name_form_ok(s.replace("{place}", "{root}"))
    return name_form_ok(s)


def race_stance_messages(rid, r, races):
    """race_stances (4.1): the first failing entry only (the loader's error path)."""
    seen = []
    for i, s in enumerate(r["stances"]):
        if s["race"] not in races:
            return ["stances[%d].race '%s' is not a race" % (i, s["race"])]
        if s["race"] == rid:
            return ["stances[%d].race '%s' is the race itself" % (i, s["race"])]
        if s["race"] in seen:
            return ["stances[%d].race '%s' is listed twice" % (i, s["race"])]
        seen.append(s["race"])
    return []


def race_stance_error(rid, r, races):
    """The loader's text (4.1, P5-02 25 item 1's error path)."""
    m = race_stance_messages(rid, r, races)
    return "check 'race_stances' failed for 'race.%s': %s" % (rid, m[0]) if m else None


def relations_check_messages(t):
    """relations_bands (4.2); relations_memory holds by the types."""
    out = []
    if not (t["hatred_max"] + t["band_margin"] < t["friend_min"] - t["band_margin"]):
        out.append("bands overlap: hatred_max %d and friend_min %d with band_margin %d"
                   % (t["hatred_max"], t["friend_min"], t["band_margin"]))
    return out


def monsters_check_messages_024(t):
    out = monsters_check_messages(t)
    if t["hunt_permille"] > t["hunt_max_permille"]:
        out.append("hunt_permille %d is above hunt_max_permille %d" % (t["hunt_permille"], t["hunt_max_permille"]))
    return out


def peoples_check_messages_024(t):
    if t["abandon_loss_min_permille"] > t["abandon_loss_max_permille"]:
        return ["abandon_loss_min_permille %d is above abandon_loss_max_permille %d"
                % (t["abandon_loss_min_permille"], t["abandon_loss_max_permille"])]
    return []


def monster_place_messages(m):
    """monster_place_forms (12.2), every message in order."""
    out = []
    for i, f in enumerate(m["name_forms"]):
        if m["wake"] == "dead" and ("{place}" not in f or "{root}" in f):
            out.append("name_forms[%d] '%s' of a dead kind needs {place} and no {root}" % (i, f))
        elif m["wake"] == "wild" and "{place}" in f:
            out.append("name_forms[%d] '%s' holds {place}, which only a dead kind may" % (i, f))
    return out


def monster_check_messages_024(m, biomes=BIOME_KEYS_023, cultures=tuple(SITE_FORMS_023)):
    """4.2's named checks with monster_bare_form_last, then monster_place_forms (12.2)."""
    return monster_check_messages(m, biomes, cultures) + monster_place_messages(m)


def epithet(form):
    """12.1: the form with {root} removed and outer spaces trimmed."""
    return form.replace("{root}", "").strip(" ")


def pre_024(path, kind):
    """A race, history, beast or monster directory older than 0.24.0 (P5-03): no stances in a race file; no
    relations.toml; no bold_phrases or raid_phrases."""
    names = [n for n in os.listdir(path) if n.endswith(".toml")]
    texts = {}
    for n in names:
        with open(os.path.join(path, n), encoding="utf-8") as f:
            texts[n] = f.read()
    if kind == "race":
        return any("stances" not in t for t in texts.values())
    if kind == "history":
        return "relations.toml" not in names
    if kind == "beast":
        return any("bold_phrases" not in t for t in texts.values())
    if kind == "monster":
        return any("raid_phrases" not in t for t in texts.values())
    return True


def name_filters_024(files):
    """True when a name_filters/ directory's files hold 12.3's word (0.24.0)."""
    for name, text in files:
        d = tomllib.loads(text).get("name_filter", {})
        if "words" in d:
            return all(w in d["words"]["whole"] for w in FILTER_WORDS_024)
    return False


NAME_FILTER_EDITS_024 = {("words", "whole"): (list(FILTER_WORDS_024), [])}


def name_filters_add_024(files):
    return name_filters_019(files, NAME_FILTER_EDITS_024)


def pre21_any(files):
    return name_filters_pre_021(files)


def check_spec_024(c, races, cultures):
    """The 0.24.0 listings say what P5-03 4.1, 4.2, 8.2, 9.2, 9.4, 9.5, 11.1, 12 and 13 state."""
    h = parsed_spec("history")
    for rid, r in races.items():
        check_record(RACE_024, r, "race." + rid)
        c.eq("race.%s stances last (4.1)" % rid, list(r)[-2:], ["culture", "stances"])
        c.eq("race.%s stances (4.1)" % rid, [(s["race"], s["value"]) for s in r["stances"]], STANCES_024[rid])
        c.ok("race.%s 0 to 8 stances, values inside int(-500, 500)" % rid,
             len(r["stances"]) <= 8 and all(-500 <= s["value"] <= 500 for s in r["stances"]))
        c.eq("race.%s passes race_stances" % rid, race_stance_messages(rid, r, races), [])
    old = {k: v for _, t in RACE_FILES_020.items() for k, v in tomllib.loads(t)["race"].items()}
    c.eq("races as at 0.23.0 but for stances", {k: {a: b for a, b in v.items() if a != "stances"}
                                                for k, v in races.items()}, old)
    el = races["elf"]
    for edit, want in (([dict(race="troll", value=1)], "stances[0].race 'troll' is not a race"),
                       ([dict(race="faerie", value=1), dict(race="elf", value=2)], "stances[1].race 'elf' is the race itself"),
                       ([dict(race="orc", value=1), dict(race="human", value=1), dict(race="orc", value=3)],
                        "stances[2].race 'orc' is listed twice"),
                       ([dict(race="elf", value=1), dict(race="troll", value=1)],
                        "stances[0].race 'elf' is the race itself")):
        c.eq("race_stances on %r" % edit, race_stance_error("elf", dict(el, stances=edit), races),
             "check 'race_stances' failed for 'race.elf': " + want)
    rl = h["relations"]
    check_record(RELATIONS_024, rl, "history.relations")
    c.eq("history.relations fields in order (4.2)", list(rl), [n for n, _ in RELATIONS_024])
    c.eq("history.relations shipped values (4.2)", rl, RELATIONS_TUNING_024)
    for n, lo, hi, _ in RELATIONS_RANGES_024:
        c.ok("history.relations.%s inside int(%d, %d)" % (n, lo, hi), lo <= rl[n] <= hi)
    c.eq("relations_bands passes", relations_check_messages(rl), [])
    c.eq("relations_bands at the boundary (h + m = f - m)", relations_check_messages(dict(rl, friend_min=-400)),
         ["bands overlap: hatred_max -500 and friend_min -400 with band_margin 50"])
    c.eq("relations_bands passes one above", relations_check_messages(dict(rl, friend_min=-399)), [])
    c.eq("zero tuning passes relations_bands", relations_check_messages(RELATIONS_ZERO_024), [])
    c.eq("the shipped hatred_max equals enmity_max (4.2, 7.1)", rl["hatred_max"], h["conflict"]["enmity_max"])
    c.eq("the shipped migrate_min equals abandon_min (4.2)", rl["migrate_min"], h["peoples"]["abandon_min"])
    mo = h["monsters"]
    check_record(MONSTERS_024, mo, "history.monsters")
    c.eq("history.monsters fields in order (9.4)", list(mo), [n for n, _ in MONSTERS_024])
    c.eq("history.monsters as at 0.23.0 before the new fields", {k: mo[k] for k in MONSTERS_TUNING_023},
         MONSTERS_TUNING_023)
    for n, lo, hi, v in MONSTERS_NEW_RANGES_024:
        c.ok("history.monsters.%s inside int(%d, %d)" % (n, lo, hi), lo <= mo[n] <= hi)
        c.eq("history.monsters.%s shipped" % n, mo[n], v)
    c.eq("monsters_hunt_ordered passes", monsters_check_messages_024(mo), [])
    c.eq("monsters_hunt_ordered message", monsters_check_messages_024(dict(mo, hunt_max_permille=19)),
         ["hunt_permille 20 is above hunt_max_permille 19"])
    c.eq("monsters_hunt_ordered at equality", monsters_check_messages_024(dict(mo, hunt_max_permille=20)), [])
    pe_ = h["peoples"]
    c.eq("history.peoples fields in order (11.1)", list(pe_), [n for n, _ in PEOPLES_024])
    for n, lo, hi, v in PEOPLES_NEW_RANGES_024:
        c.ok("history.peoples.%s inside int(%d, %d)" % (n, lo, hi), lo <= pe_[n] <= hi)
        c.eq("history.peoples.%s shipped" % n, pe_[n], v)
    c.eq("peoples_abandon_loss_ordered passes", peoples_check_messages_024(pe_), [])
    c.eq("peoples_abandon_loss_ordered message", peoples_check_messages_024(dict(pe_, abandon_loss_min_permille=201)),
         ["abandon_loss_min_permille 201 is above abandon_loss_max_permille 200"])
    be = parsed_spec("beast")
    for bid, b in be.items():
        check_record(BEAST_024, b, "beast." + bid)
        c.eq("beast.%s bold_phrases after food (13)" % bid, list(b)[-2:], ["food", "bold_phrases"])
        c.eq("beast.%s bold_phrases (13)" % bid, b["bold_phrases"], BOLD_PHRASES_024[bid])
        c.ok("beast.%s 1 to 4 phrases by phrase" % bid,
             1 <= len(b["bold_phrases"]) <= 4 and all(phrase_ok(p) for p in b["bold_phrases"]))
    mk = parsed_spec("monster")
    for mid, m in mk.items():
        check_record(MONSTER_024, m, "monster." + mid)
        c.eq("monster.%s raid_phrases after hoard_permille (9.5)" % mid, list(m)[-2:], ["hoard_permille", "raid_phrases"])
        c.eq("monster.%s raid_phrases (9.5)" % mid, m["raid_phrases"], RAID_PHRASES_024[mid])
        c.ok("monster.%s 1 to 4 phrases by phrase" % mid,
             1 <= len(m["raid_phrases"]) <= 4 and all(phrase_ok(p) for p in m["raid_phrases"]))
        c.eq("monster.%s name_forms (12.1, 12.2)" % mid, m["name_forms"], NAME_FORMS_024[mid])
        c.ok("monster.%s forms by name_form with {place} (12.2)" % mid,
             all(monster_form_ok_024(f, m["wake"] == "dead") for f in m["name_forms"])
             and all(name_form_ok(f) for f in m["lair_forms"]) and 1 <= len(m["name_forms"]) <= 8)
        c.eq("monster.%s passes its named checks (0.24.0)" % mid, monster_check_messages_024(m), [])
        eps = [epithet(f) for f in m["name_forms"] if "{root}" in f]
        c.ok("monster.%s epithets distinct (12.1)" % mid, len(eps) == len(set(eps)))
    c.eq("hoards of undead and wyvern 0 (9.2)", (mk["undead"]["hoard_permille"], mk["wyvern"]["hoard_permille"]), (0, 0))
    c.eq("undead lair_forms (12.2)", mk["undead"]["lair_forms"], ["The Barrows of {root}", "The Howes of {root}"])
    c.eq("monster_place_forms on a dead kind with {root}",
         monster_place_messages(dict(mk["undead"], name_forms=["The Host of {place}", "The Host of {root}"])),
         ["name_forms[1] 'The Host of {root}' of a dead kind needs {place} and no {root}"])
    c.eq("monster_place_forms on a wild kind with {place}",
         monster_place_messages(dict(mk["troll"], name_forms=["Old {root}", "The Troll of {place}"])),
         ["name_forms[1] 'The Troll of {place}' holds {place}, which only a dead kind may"])
    for p, ok in (("fell upon", True), ("Fell upon", False), ("fell  upon", False), ("fell upon ", False),
                  ("o'er the hills", True), ("half-starved", True), ("-x", False), ("x" * 48, True), ("x" * 49, False)):
        c.eq("phrase grammar %r" % p, phrase_ok(p), ok)
    evl = h["report"]["events"]
    c.eq("kinds 56-62 entries after outpost_abandoned (8.2, 11.3)",
         [(e["kind"], e["salience"], e["template"]) for e in evl[-7:]], [tuple(e) for e in REPORT_ENTRIES_024])
    c.eq("report lists 62 kinds", len(evl), 62)
    c.eq("kind 48 template (9.5)", evl[47]["template"], KIND48_TEMPLATE_024)
    c.eq("kind 53 template (13)", evl[52]["template"], KIND53_TEMPLATE_024)
    rn = h["renaming"]
    c.eq("rule 7's forms (12.3)", rn["rules"][6]["forms"], RENAME_RULE_7_024[11])
    c.eq("renaming passes its four named checks (0.24.0)", renaming_check_messages(rn), [])
    for k in ("chronology", "conflict", "engine"):
        c.eq("history.%s as at 0.23.0" % k, h[k], tomllib.loads(HISTORY_FILES_023[k + ".toml"])["history"][k])
    c.eq("cultures as at 0.23.0", cultures,
         {k: v for _, t in CULTURE_FILES_023.items() for k, v in tomllib.loads(t)["culture"].items()})


# The data version this module serves: "0.18.0" (default, P3-09), "0.17.0"
# (P3-08) or "0.16.0" (use_version), so that the references can still
# reproduce the older worlds and goldens.
DATA_VERSION = None
VERSIONS = ("0.16.0", "0.17.0", "0.18.0", "0.19.0", "0.20.0", "0.21.0", "0.22.0", "0.23.0", "0.24.0")


def at_least(v):
    """True when the served data version is v or later."""
    return VERSIONS.index(DATA_VERSION) >= VERSIONS.index(v)


def use_version(v):
    global DATA_VERSION, KINDS, SPEC_FILES, NEW_KINDS
    NEW_KINDS = ("beast", "culture", "history", "monster", "race") if v in ("0.23.0", "0.24.0") else ("culture", "history", "race")
    if v == "0.24.0":
        KINDS, SPEC_FILES = KINDS_024, SPEC_FILES_024
    elif v == "0.23.0":
        KINDS, SPEC_FILES = KINDS_023, SPEC_FILES_023
    elif v == "0.22.0":
        KINDS, SPEC_FILES = KINDS_022, SPEC_FILES_022
    elif v == "0.21.0":
        KINDS, SPEC_FILES = KINDS_021, SPEC_FILES_021
    elif v == "0.20.0":
        KINDS, SPEC_FILES = KINDS_020, SPEC_FILES_020
    elif v == "0.19.0":
        KINDS, SPEC_FILES = KINDS_019, SPEC_FILES_019
    elif v == "0.18.0":
        KINDS, SPEC_FILES = KINDS_018, SPEC_FILES_018
    elif v == "0.17.0":
        KINDS, SPEC_FILES = KINDS_017, SPEC_FILES_017
    elif v == "0.16.0":
        KINDS, SPEC_FILES = KINDS_016, SPEC_FILES_016
    else:
        raise ValueError(v)
    DATA_VERSION = v


use_version("0.24.0")

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
    if directory == "name_filters" and not at_least("0.18.0") and os.path.isdir(path) and \
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
    return copied_files_019(path, directory, out)


def disk_version(path, kind):
    """The data version of a race, culture or history directory on disk."""
    if kind in ("beast", "monster"):   # new at 0.23.0 (P5-02 4.1, 4.2); phrases at 0.24.0 (P5-03 9.5, 13)
        return "0.23.0" if pre_024(path, kind) else "0.24.0"
    if kind in ("race", "history") and not pre_024(path, kind):
        return "0.24.0"
    if kind in ("race", "history") and pre_015(path, kind):
        return "0.15.0"
    if pre_017(path, kind):
        return "0.16.0"
    if kind != "culture" and pre_018(path, kind):
        return "0.17.0"
    if kind != "culture" and pre_019(path, kind):
        return "0.18.0"
    if kind != "culture" and pre_019_r17(path, kind):
        return "0.19.0-r16"   # the first P3-10 round's files: matches no served version
    if pre_020(path, kind):
        return "0.19.0"
    if pre_021(path, kind):
        return "0.20.0"
    if kind != "race" and pre_022(path, kind):
        return "0.21.0"
    if kind != "race" and pre_023(path, kind):
        return "0.22.0"
    return "0.23.0"


def same_version(kind, a, b):
    """Whether two data versions have the same files of a kind (cultures did not change at 0.18.0)."""
    if kind == "culture":   # cultures did not change at 0.18.0 and 0.19.0 (0.20.0: the faerie lists)
        a, b = ("0.17.0" if x in ("0.18.0", "0.19.0") else x for x in (a, b))
    if kind == "race":   # races did not change at 0.21.0, 0.22.0 and 0.23.0
        a, b = ("0.20.0" if x in ("0.21.0", "0.22.0", "0.23.0") else x for x in (a, b))
    if kind == "culture":   # cultures did not change at 0.24.0
        a, b = ("0.23.0" if x == "0.24.0" else x for x in (a, b))
    return a == b


def pre_019(path, kind):
    """A race or history directory older than 0.19.0 (P3-10): no `odds_min_permille` in a race file, no
    `search_weary_years` in conflict.toml, no `inland_cap` in peoples.toml or no kind 38 entry in report.toml."""
    texts = {}
    for n in os.listdir(path):
        if n.endswith(".toml"):
            with open(os.path.join(path, n), encoding="utf-8") as f:
                texts[n] = f.read()
    if kind == "history":
        return ("search_weary_years" not in texts.get("conflict.toml", "")
                or "quarrel_strain" not in texts.get("conflict.toml", "")   # P3-10 r1 (P3-06 r15 21.12)
                or "inland_cap" not in texts.get("peoples.toml", "")
                or "quiet_stretch" not in texts.get("report.toml", ""))
    return any("odds_min_permille" not in t for t in texts.values())


def pre_019_r17(path, kind):
    """A race or history directory of the first P3-10 round (P3-10 r3 replaces it): no `hidden_years` in a race
    file, `hidden_years` still in conflict.toml, no `quarrel_min_settlements` or no `quiet_min_years`."""
    texts = {}
    for n in os.listdir(path):
        if n.endswith(".toml"):
            with open(os.path.join(path, n), encoding="utf-8") as f:
                texts[n] = f.read()
    if kind == "history":
        co = texts.get("conflict.toml", "")
        return ("quarrel_min_settlements" not in co or "\nhidden_years" in co
                or "quiet_min_years" not in texts.get("engine.toml", ""))
    return any("hidden_years" not in t for t in texts.values())


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
    """At 0.24.0: every check of the 0.23.0 listings on them, then check_spec_024 on the 0.24.0 listings."""
    if at_least("0.24.0"):
        use_version("0.23.0")
        try:
            check_spec_invariants(c)
        finally:
            use_version("0.24.0")
        check_spec_024(c, parsed_spec("race"), parsed_spec("culture"))
        c.eq("history ids (0.24.0)", sorted(parsed_spec("history")),
             ["chronology", "conflict", "engine", "monsters", "peoples", "relations", "renaming", "report"])
        return None
    if at_least("0.23.0"):
        use_version("0.22.0")
        try:
            check_spec_invariants_022(c)
        finally:
            use_version("0.23.0")
        check_spec_023(c, parsed_spec("race"), parsed_spec("culture"))
        c.eq("history ids (0.23.0)", sorted(parsed_spec("history")),
             ["chronology", "conflict", "engine", "monsters", "peoples", "renaming", "report"])
        return None
    return check_spec_invariants_022(c)


def check_spec_invariants_022(c):
    """At 0.22.0: every check of the 0.21.0 listings on them (the 0.22.0 listings are derived from them by
    P5-12's edits), then check_spec_022 on the 0.22.0 listings."""
    if not at_least("0.22.0"):
        return check_spec_invariants_upto_021(c)
    use_version("0.21.0")
    try:
        check_spec_invariants_upto_021(c)
    finally:
        use_version("0.22.0")
    check_spec_022(c, parsed_spec("race"), parsed_spec("culture"))
    h = parsed_spec("history")
    c.eq("history ids (0.22.0)", sorted(h), ["chronology", "conflict", "engine", "peoples", "renaming", "report"])
    for k in ("chronology", "conflict", "engine", "peoples"):
        c.eq("history.%s as at 0.21.0" % k, h[k], tomllib.loads(HISTORY_FILES_021[k + ".toml"])["history"][k])


def check_spec_invariants_upto_021(c):
    """P3-01 3: the shipped data passes every named and kind-level check and
    the three cross-kind conditions (the spec states none can fail)."""
    races = parsed_spec("race")
    cultures = parsed_spec("culture")
    for rid, r in races.items():
        check_record(KINDS["race"][1], r, "race." + rid)
    for cid, cu in cultures.items():
        check_record(KINDS["culture"][1], cu, "culture." + cid)
    check_record(CHRONOLOGY, parsed_spec("history")["chronology"], "history.chronology")
    check_record(KINDS["history"][1]["engine"], parsed_spec("history")["engine"], "history.engine")
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
    for n, lo, hi in (PEOPLES_RANGES_019 if at_least("0.19.0") else PEOPLES_RANGES_018 if at_least("0.18.0") else
                      PEOPLES_RANGES_017 if at_least("0.17.0") else PEOPLES_RANGES):
        vals = list(pe[n].values()) if isinstance(pe[n], dict) else [pe[n]]
        c.ok("history.peoples.%s inside its range" % n, all(lo <= v <= hi for v in vals))
    kinds30 = ["awakening", "civ_founded", "settlement_founded", "quiet_era", "flare_up", "colony_founded",
               "became_village", "became_town", "became_city", "settlement_dwindled", "famine", "plague",
               "plague_spread", "succession_crisis", "civ_seceded", "settlement_abandoned", "capital_moved",
               "civ_ended", "ruin_faded", "claim", "war_declared", "battle", "battle_at_pass", "siege", "sacked",
               "settlement_captured", "peace", "truce", "region_ceded", "civ_conquered"]
    later = (["settlement_abandoned_to"] if at_least("0.17.0") else []) + \
        (["refugees_settled", "tribute", "war_won", "search_abandoned", "refuge_held"] if at_least("0.18.0") else []) + \
        (["plague_outbreak", "quiet_stretch"] if at_least("0.19.0") else []) + \
        (["claim_settlements", "settlements_ceded"] if at_least("0.20.0") else []) + \
        (["claim_dropped", "refuge_hidden"] if at_least("0.21.0") else [])
    c.eq("report lists kinds 1-%d in id order (P3-05 11, P3-06 11%s)"
         % (30 + len(later), (", P3-05 r6 11" if at_least("0.17.0") else "") +
            (", P3-06 r9 20.9" if at_least("0.18.0") else "")),
         [e["kind"] for e in parsed_spec("history")["report"]["events"]], kinds30 + later)
    if at_least("0.17.0"):
        check_spec_017(c, races, cultures)
    if DATA_VERSION == "0.18.0":
        check_spec_018(c, races)
    if at_least("0.19.0"):
        check_spec_019(c, races)
    if at_least("0.20.0"):
        check_spec_020(c, races, cultures)
    if at_least("0.21.0"):
        check_spec_021(c, races, cultures)
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


def check_spec_019(c, races):
    """The 0.19.0 listings say what P3-01 r11 3.4 and 3.5, P3-05 r12 22.1-22.5, P3-06 r14 21.2, 21.7, 21.9 and
    P3-04 r12 20.1, 20.4 state."""
    c.eq("race fields of P3-06 r9 20.12 kept (muster, conceal_min, home defence)",
         {k: (v["muster_permille"], v["conceal_min"], v["home_defense_permille"]) for k, v in races.items()},
         RACE_FIELDS_018_VALUES)
    c.eq("cell_weights (P3-01 r11 3.4): ore, height, inland, port, habitability",
         {k: tuple(v["cell_weights"][f] for f in ("ore", "height", "inland", "port", "habitability"))
          for k, v in races.items()}, CELL_WEIGHTS_019)
    c.eq("cell_weights field order (3.4: the three after height)",
         [list(v["cell_weights"]) for v in races.values()], [["ore", "height", "inland", "port", "habitability"]] * 6)
    for k, v in races.items():
        for n, lo, hi in CELL_FIELDS_019:
            c.ok("race %s.cell_weights.%s inside int(%d, %d)" % (k, n, lo, hi), lo <= v["cell_weights"][n] <= hi)
        for n, lo, hi in RACE_FIELDS_019:
            c.ok("race %s.%s inside int(%d, %d)" % (k, n, lo, hi), lo <= v[n] <= hi)
    c.eq("races with a cell term (3.4: record differs from the neutral one)",
         sorted(k for k, v in races.items() if tuple(v["cell_weights"][f] for f in
                ("ore", "height", "inland", "port", "habitability")) != CELL_WEIGHTS_NEUTRAL_019), ["dwarf", "faerie"])
    c.eq("coast (P3-05 r12 22.5: faerie -500)", {k: v["coast"] for k, v in races.items()},
         dict(elf=0, goblin=-100, dwarf=-200, faerie=-500, human=200, orc=-100) | {
             k: v["coast"] for k, v in races.items() if k != "faerie"})
    c.eq("odds_min_permille, awakening_quarrel (P3-01 r11 3.5)",
         {k: (v["odds_min_permille"], v["awakening_quarrel"]) for k, v in races.items()}, RACE_FIELDS_019_VALUES)
    c.eq("hidden_years (P3-01 r13 3.6: orc 100, others 300)", {k: v["hidden_years"] for k, v in races.items()},
         HIDDEN_YEARS_019)
    c.eq("the three race fields follow home_defense_permille in this order (3.5, 3.6)",
         [list(v)[list(v).index("home_defense_permille") + 1:list(v).index("home_defense_permille") + 4]
          for v in races.values()], [["odds_min_permille", "awakening_quarrel", "hidden_years"]] * 6)
    co = {k: v for k, v in parsed_spec("history")["conflict"].items() if k not in ("quarrel_strength_permille", "toll_spread_permille")}   # 0.21.0: check_spec_021
    c.ok("history.conflict has no odds_min (3.5)", "odds_min" not in co)
    c.ok("history.conflict has no hidden_years (3.6)", "hidden_years" not in co)
    i = list(co).index("search_years")
    c.eq("history.conflict: search_weary_years 500, search_repeat_salience 15 (P3-06 r14 21.7), quarrel_strain 600 "
         "(r15 21.12), quarrel_min_settlements 2 (r17 22.1) after search_years",
         [(n, co[n]) for n in list(co)[i + 1:i + 5]],
         [("search_weary_years", 500), ("search_repeat_salience", 15), ("quarrel_strain", 600),
          ("quarrel_min_settlements", 2)])
    c.eq("history.conflict: capital_sacked_salience follows quarrel_min_settlements", list(co)[i + 5],
         "capital_sacked_salience")
    c.eq("history.conflict has 59 fields (r17: one out, one in)", len(co), 59)
    c.eq("conflict_quarrel_strain fails below claim_strain (r15 21.12)",
         conflict_check_messages(dict(co, quarrel_strain=co["claim_strain"] - 1)),
         ["quarrel_strain must not be below claim_strain"])
    c.eq("conflict_quarrel_strain passes at claim_strain", conflict_check_messages(dict(co, quarrel_strain=co["claim_strain"])), [])
    c.eq("conflict_quarrel_strain_max fails with strain_max 500, quarrel_strain 600 (r17 22.7)",
         conflict_check_messages(dict(co, strain_max=500, quarrel_strain=600, claim_strain=100, declare_min=100,
                                      declare_span=400)),
         ["quarrel_strain must not be above strain_max"])
    c.eq("conflict_quarrel_strain_max passes at 600 and 600",
         conflict_check_messages(dict(co, strain_max=600, quarrel_strain=600, claim_strain=100, declare_min=100,
                                      declare_span=500)), [])
    c.eq("history.conflict without odds_min equals 0.18.0's but for the new fields",
         {k: v for k, v in co.items() if k not in ("search_weary_years", "search_repeat_salience", "quarrel_strain",
                                                   "quarrel_min_settlements")},
         {k: v for k, v in tomllib.loads(CONFLICT_FILE_018["conflict.toml"])["history"]["conflict"].items()
          if k not in ("odds_min", "hidden_years")})
    en = {k: v for k, v in parsed_spec("history")["engine"].items() if k != "flare_min_years"}   # check_spec_021
    c.eq("history.engine: quiet_min_years 50 after quiet_events_max (P3-04 r15 4.1)",
         list(en.items())[1:3], [("quiet_events_max", 3), ("quiet_min_years", 50)])
    c.eq("history.engine passes its named checks", engine_check_messages(en), [])
    c.eq("engine_quiet_min passes at 2 x quiet_step_years", engine_check_messages(dict(en, quiet_min_years=50)), [])
    c.eq("engine_quiet_min fails at 51 with quiet_step_years 25 (P3-04 r15 20.6 item 00)",
         engine_check_messages(dict(en, quiet_min_years=51)), ["quiet_min_years must not be above 2 x quiet_step_years"])
    c.eq("history.engine equals 0.18.0's but for quiet_min_years",
         {k: v for k, v in en.items() if k != "quiet_min_years"},
         tomllib.loads(ENGINE_FILE["engine.toml"])["history"]["engine"])
    pe = parsed_spec("history")["peoples"]
    j = list(pe).index("civs_extra_max")
    c.eq("history.peoples: cause_memory_years 50 and inland_cap 20 after civs_extra_max (P3-05 r12 22.2, 22.5)",
         [(n, pe[n]) for n in list(pe)[j + 1:j + 3]], [("cause_memory_years", 50), ("inland_cap", 20)])
    c.eq("history.peoples has 71 fields", len(pe), 71)
    ev = {e["kind"]: e for e in parsed_spec("history")["report"]["events"]}
    c.eq("kind 37 entry (P3-05 r12 22.1)", (ev["plague_outbreak"]["salience"], ev["plague_outbreak"]["template"]),
         (45, "Plague broke out in {settlement}{reached?| and spread to one other settlement| and spread to # other "
              "settlements}; {total} died{reached?| in all| in all}."))
    c.eq("kind 38 entry (P3-04 r12 20.1)", (ev["quiet_stretch"]["salience"], ev["quiet_stretch"]["template"]),
         (20, "For {years} {years?year|years}, from {@year} to {@end_year}, the land was quiet{realms?|; one realm "
              "held it|, and # realms kept the peace}."))
    for k, (_old, new) in TEMPLATES_019.items():
        c.eq("template of %s (P3-10)" % k, ev[k]["template"], new)
    c.eq("dwindle template (P3-05 r12 22.3, M written out)", ev["settlement_dwindled"]["template"],
         "{@causes:famine=famine,plague=plague,plague_outbreak=plague,plague_spread=plague,succession_crisis=strife,"
         "battle=war,battle_at_pass=war,tribute=tribute,colony_founded=emigration|Weakened by |, }{settlement} "
         "dwindled to {population} souls.")
    old = {e["kind"]: e for e in tomllib.loads(REPORT_FILE_018["report.toml"])["history"]["report"]["events"]}
    c.eq("every other report entry as at 0.18.0 (saliences unchanged)",
         [e for e in parsed_spec("history")["report"]["events"]
          if e["kind"] not in TEMPLATES_019 and e["kind"] not in ("plague_outbreak", "quiet_stretch",
                                                                   "claim_settlements", "settlements_ceded",
                                                                   "claim_dropped", "refuge_hidden", "became_city")],
         [e for k, e in old.items() if k not in TEMPLATES_019 and k != "became_city"])
    # P3-04 r13 4.2: report_event.template is string(512) from 0.19.0 (256 before); the cause-list templates are
    # why (this reference checks types only; the bound is the loader's, tested in data_history_kinds_test.cpp)
    lens = {k: len(e["template"].encode()) for k, e in ev.items()}
    c.ok("every 0.19.0 template within 512 bytes (P3-04 r13 4.2)", max(lens.values()) <= 512)
    c.eq("templates above 256 bytes are the cause-list ones",
         sorted(k for k, n in lens.items() if n > 256), ["settlement_abandoned", "settlement_abandoned_to"])
    c.eq("saliences of the changed kinds unchanged", {k: ev[k]["salience"] for k in TEMPLATES_019},
         {k: old[k]["salience"] for k in TEMPLATES_019})


def check_spec_020(c, races, cultures):
    """The 0.20.0 listings say what P3-01 r14 3.7, P3-02 r15 14.1, P3-06 r19 23.6 and P3-04 r17 21.2 state."""
    c.eq("awakening_population (P3-01 r14 3.7: orc 1200, others 0)",
         {k: v["awakening_population"] for k, v in races.items()}, AWAKENING_POPULATION_020)
    c.eq("awakening_population follows hidden_years", [list(v).index("awakening_population") - list(v).index(
        "hidden_years") for v in races.values()], [1] * 6)
    for k, v in races.items():
        c.ok("race %s.awakening_population inside int(0, 100000)" % k, 0 <= v["awakening_population"] <= 100000)
    old = {}
    for _, text in RACE_FILES_019.items():
        old.update(tomllib.loads(text)["race"])
    c.eq("every other race field as at 0.19.0", {k: {f: x for f, x in v.items() if f != "awakening_population"}
                                                for k, v in races.items()}, old)
    fa = cultures["faerie"]["naming"]
    c.eq("faerie lists (P3-02 r15 14.1)", {f: fa[f] for f in ("onsets", "nuclei", "codas", "forbidden")},
         {f: tomllib.loads("x = " + new.split(" = ", 1)[1])["x"] for f, (_, new) in
          zip(("onsets", "nuclei", "codas", "forbidden"), FAERIE_LISTS_020)})
    c.eq("faerie list sizes (13 onsets, 16 nuclei, 8 codas, 6 forbidden)",
         [len(fa[f]) for f in ("onsets", "nuclei", "codas", "forbidden")], [13, 16, 8, 6])
    oldc = {}
    for _, text in CULTURE_FILES_017.items():
        oldc.update(tomllib.loads(text)["culture"])
    if at_least("0.21.0"):   # check_spec_021 has the dwarven forbidden list
        cultures = dict(cultures, dwarven=dict(cultures["dwarven"], naming=dict(
            cultures["dwarven"]["naming"], forbidden=oldc["dwarven"]["naming"]["forbidden"])))
    c.eq("every other culture and faerie field as at 0.19.0",
         {k: dict(v, naming={f: x for f, x in v["naming"].items() if k != "faerie" or f not in (
             "onsets", "nuclei", "codas", "forbidden")}) for k, v in cultures.items()},
         {k: dict(v, naming={f: x for f, x in v["naming"].items() if k != "faerie" or f not in (
             "onsets", "nuclei", "codas", "forbidden")}) for k, v in oldc.items()})
    ev = [e for e in parsed_spec("history")["report"]["events"] if e["kind"] not in ("claim_dropped", "refuge_hidden")]
    if at_least("0.21.0"):   # check_spec_021 has became_city's template
        ev = [dict(e, template=BECAME_CITY_021[0]) if e["kind"] == "became_city" else e for e in ev]
    c.eq("kinds 39, 40 entries after quiet_stretch (P3-06 r19 23.6, P3-04 r17 21.2)",
         [(e["kind"], e["salience"], e["template"]) for e in ev[-3:]],
         [("quiet_stretch", 20, ev[-3]["template"]),
          ("claim_settlements", 35, "{claimant} laid claim to {settlement:all} of {holder} in {@region}."),
          ("settlements_ceded", 55, "{from} gave up {settlement:all} in {@region} to {to}.")])
    c.eq("every other report entry as at 0.19.0", ev[:-2],
         tomllib.loads(REPORT_FILE_019["report.toml"])["history"]["report"]["events"])


def check_spec_021(c, races, cultures):
    """The 0.21.0 listings say what P3-01 r15 3.8, P3-02 r17 15.2, P3-05 r18 24 and P3-06 r22 24.6 state."""
    co = parsed_spec("history")["conflict"]
    i = list(co).index("quarrel_min_settlements")
    c.eq("history.conflict: quarrel_strength_permille 1000, toll_spread_permille 200 after quarrel_min_settlements "
         "(P3-01 r15 3.8)", [(n, co[n]) for n in list(co)[i + 1:i + 4]],
         [("quarrel_strength_permille", 1000), ("toll_spread_permille", 200), ("capital_sacked_salience", 90)])
    c.eq("history.conflict has 61 fields", len(co), 61)
    c.eq("conflict_toll_spread passes for the shipped values (80 x 1200 <= 250 x 800)", conflict_check_messages(co), [])
    c.eq("conflict_toll_spread fails for loss_winner 200, loss_loser 250, spread 200 (P3-01 r15 3.8)",
         conflict_check_messages(dict(co, loss_winner_permille=200, loss_loser_permille=250, toll_spread_permille=200)),
         ["toll_spread_permille lets the victors lose more than the defeated"])
    c.eq("conflict_toll_spread reads min(loss_loser, kill_max): kill_max 100 fails at spread 200",
         conflict_check_messages(dict(co, kill_max_permille=100)),
         ["toll_spread_permille lets the victors lose more than the defeated"])
    c.eq("conflict_toll_spread passes at spread 0 with loss_winner = loss_loser",
         conflict_check_messages(dict(co, loss_winner_permille=250, toll_spread_permille=0)), [])
    old = tomllib.loads(CONFLICT_FILE_019["conflict.toml"])["history"]["conflict"]
    c.eq("history.conflict equals 0.20.0's but for the two fields",
         {k: v for k, v in co.items() if k not in ("quarrel_strength_permille", "toll_spread_permille")}, old)
    en = parsed_spec("history")["engine"]
    c.eq("history.engine: flare_min_years 10 after quiet_min_years (P3-04 r19 22.3)",
         list(en.items())[2:4], [("quiet_min_years", 50), ("flare_min_years", 10)])
    c.eq("history.engine passes engine_flare_min", engine_check_messages(en), [])
    c.eq("engine_flare_min fails for 31 against sparse 30 (P3-01 r15 3.8)",
         engine_check_messages(dict(en, flare_min_years=31)), ["sparse: flare_min_years must not be above flare_years"])
    c.eq("engine_flare_min names every failing level in order",
         engine_check_messages(dict(en, flare_min_years=80)),
         ["%s: flare_min_years must not be above flare_years" % lv for lv in ("sparse", "normal", "dense")])
    c.eq("engine_flare_min passes at 30 (= sparse flare_years)", engine_check_messages(dict(en, flare_min_years=30)),
         [])
    c.eq("history.engine equals 0.20.0's but for flare_min_years",
         {k: v for k, v in en.items() if k != "flare_min_years"},
         tomllib.loads(ENGINE_FILE_019["engine.toml"])["history"]["engine"])
    for n, lo, hi in ENGINE_RANGES_021:
        c.ok("history.engine.%s inside int(%d, %d)" % (n, lo, hi), lo <= en[n] <= hi)
    dw = cultures["dwarven"]["naming"]["forbidden"]
    c.eq("dwarven forbidden gains didin, ririn appended (P3-02 r17 15.2)", dw[-2:], ["didin", "ririn"])
    c.eq("dwarven forbidden has 15 entries", len(dw), 15)
    evl = [e for e in parsed_spec("history")["report"]["events"] if e["kind"] != "region_renamed"]   # 0.22.0: 022
    ev = {e["kind"]: e for e in evl}
    c.eq("became_city (P3-05 r18 24)", (ev["became_city"]["salience"], ev["became_city"]["template"]),
         (45, "{settlement} in {@region} grew into a city."))
    c.eq("kinds 41, 42 entries after settlements_ceded (P3-06 r22 24.6, P3-04 r19 22.2)",
         [(e["kind"], e["salience"], e["template"]) for e in evl[-3:]],
         [("settlements_ceded", 55, ev["settlements_ceded"]["template"]),
          ("claim_dropped", 15, "{claimant} let the claim against {holder} over {@region} lapse."),
          ("refuge_hidden", 10, "{@settlement} lay hidden from {@year} to {@end_year}.")])
    oldr = tomllib.loads(REPORT_FILE_020["report.toml"])["history"]["report"]["events"]
    c.eq("every other report entry as at 0.20.0",
         [e for e in evl[:-2] if e["kind"] != "became_city"], [e for e in oldr if e["kind"] != "became_city"])
    c.eq("races as at 0.20.0", races, {k: v for _, t in RACE_FILES_020.items() for k, v in tomllib.loads(t)["race"].items()})


def check_placenames_021(c, pn):
    """P1-06 r10 16.1 and 16.3 in the served placename lists."""
    co, ba = pn["common"], pn["trait_barren"]
    grim_common = {f: [w for w in co[f] if w in GRIM_019[f]] for f in GRIM_019}
    c.eq("16.1 common holds no grim word but the six", grim_common, GRIM_KEPT_021)
    for f, words in GRIM_MOVED_021.items():
        c.eq("16.1 the moved words are absent from common.%s" % f, [w for w in words if w in co[f]], [])
        c.eq("16.1 the moved words are in trait_barren.%s" % f, [w for w in words if w not in ba[f]], [])
        c.ok("16.1 trait_barren.%s sorted without repeats" % f, ba[f] == sorted(set(ba[f]), key=lambda w: w.encode()))
    c.ok("16.1 carr.adjectives keeps Weeping", "Weeping" in pn["carr"]["adjectives"])
    c.ok("16.1 steppe.objects keeps Bones", "Bones" in pn["steppe"]["objects"])
    for w, f in (("Black", "prefixes"), ("Ashen", "adjectives"), ("Gloom", "prefixes"), ("Murk", "prefixes")):
        c.ok("16.1 %s stays in common.%s" % (w, f), w in co[f])


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
        if not (at_least("0.19.0") and k in TEMPLATES_019):   # P3-10 rewrites some; check_spec_019 has them
            c.eq("report template of %s (0.17.0)" % k, ev[k]["template"], new)
    if not at_least("0.19.0"):
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
    if at_least("0.21.0"):   # P3-02 r17 15.1: slurs 33 / 17 / 5, crude 71 / 12 / 15, famous 0 / 0 / 103 (15.1 says 105)
        n24 = len(FILTER_WORDS_024) if at_least("0.24.0") else 0   # P5-03 12.3: bird in words.whole
        sizes = dict(slurs=(33, 17, 5), crude=(71, 12, 15), words=(5, 1, 391 + n24), famous=(0, 0, 103))
        effs = [109, 30, 514 + n24]
    elif at_least("0.20.0"):   # P3-02 r15 14.3: slurs 33 / 18 / 5, crude 71 / 12 / 14, words 5 / 1 / 391
        sizes = dict(slurs=(33, 18, 5), crude=(71, 12, 14), words=(5, 1, 391), famous=(0, 0, 101))
        effs = [109, 31, 511]
    elif at_least("0.19.0"):   # P3-02 r10 12.1, r13 13.1: crude 72 / 12 / 14, slurs 33 / 18 / 6, words 5 / 1 / 387
        sizes = dict(slurs=(33, 18, 6), crude=(72, 12, 14), words=(5, 1, 387), famous=(0, 0, 101))
        effs = [110, 31, 508]
    else:
        sizes = dict(slurs=(33, 19, 6), crude=(68, 16, 14), words=(0, 0, 393 if at_least("0.18.0") else 647),
                     famous=(0, 0, 101))   # P3-02 r8 11.11 (0.18.0): the short word list, 393 (647 before)
        effs = [101, 35, 514 if at_least("0.18.0") else 768]
    c.eq("name_filter sizes (P3-02 r5 11.2, r10 12.1)",
         {d: tuple(len(t[f]) for f in ("anywhere", "at_start", "whole")) for d, t in content}, sizes)
    eff = [len(set(w for _, t in content for w in t[f])) for f in ("anywhere", "at_start", "whole")]
    c.eq("effective list sizes (P3-02 r5 11.2, r8 11.11, r10 12.1)", eff, effs)
    if at_least("0.19.0"):
        check_placenames_019(c)
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


def check_placenames_019(c):
    """P1-06 r6 15.1-15.3 in the served placename lists."""
    pn = {d: t for d, t in load_root("game/data", ["placename"])["placename"]}
    co, ba, hi = pn["common"], pn["trait_barren"], pn["trait_high"]
    for (did, f), words in CHURCH_019.items():
        c.eq("15.1 %s.%s holds no church word" % (did, f), [w for w in words if w in pn[did][f]], [])
    for w in ("Hallow", "Hermit", "Bellringer"):
        c.ok("15.1 %s stays" % w, w in co["prefixes"] + co["possessors"])
    for f in GRIM_019:
        c.eq("15.2 grim words kept in common.%s" % f, [w for w in co[f] if w in GRIM_019[f]],
             GRIM_KEPT_021[f] if at_least("0.21.0") else GRIM_KEPT_019[f])
        moved = [w for w in GRIM_019[f] if w not in GRIM_KEPT_019[f]]
        c.ok("15.2 moved grim words of %s are in trait_barren" % f, all(w in ba[f] for w in moved))
    for f in PLACENAME_LISTS:
        if f == "excluded":
            continue   # grouped (one-word entries, then multi-word ones), not one sorted run
        for did, t in pn.items():
            c.ok("%s.%s sorted without repeats" % (did, f), t[f] == sorted(set(t[f]), key=lambda w: w.encode()))
    c.ok("15.3 wood.suffixes without hanger", "hanger" not in pn["wood"]["suffixes"])
    c.ok("15.3 trait_high.adjectives without High", "High" not in hi["adjectives"])
    c.ok("15.3 common.places without Cleft and Gap", "Cleft" not in co["places"] and "Gap" not in co["places"])
    c.ok("15.3 trait_high.places with Cleft", "Cleft" in hi["places"])
    c.ok("15.3 trait_pass.places holds Gap", "Gap" in pn["trait_pass"]["places"])
    c.eq("15.2 trait_barren.places (Dole not repeated)", ba["places"],
         sorted(set(["Dole", "Hunger", "Want", "Dole", "Grave", "Lament", "Ruin", "Sorrow"]
                    + (GRIM_MOVED_021["places"] if at_least("0.21.0") else [])), key=lambda w: w.encode()))
    if at_least("0.21.0"):
        check_placenames_021(c, pn)


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
    if os.path.exists(GOLDEN) and not at_least("0.24.0"):   # the file of an older version is its base commit's
        import subprocess
        base = P5_03_BASE_COMMIT if DATA_VERSION == "0.23.0" else P5_02_BASE_COMMIT if DATA_VERSION == "0.22.0" else P5_12_BASE_COMMIT if DATA_VERSION == "0.21.0" else P5_01_BASE_COMMIT if DATA_VERSION == "0.20.0" else \
            P3_11_BASE_COMMIT if DATA_VERSION == "0.19.0" else P3_10_BASE_COMMIT
        committed = subprocess.run(["git", "-C", REPO, "show", base + ":worldgen/tests/golden/p3-01-data.txt"],
                                   check=True, capture_output=True, text=True).stdout
        c.eq("golden file of %s (git %s) is current" % (DATA_VERSION, base), committed, fresh)
    elif os.path.exists(GOLDEN):
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
    if args[0] == "write-root":   # P3-10: a scratch 0.19.0 data root, outside the repository
        write_root_019(args[1])
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
