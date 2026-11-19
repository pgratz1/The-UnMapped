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
  history   P3-01 5.1 (fixed id `chronology`)
  race      P3-01 3
  worldgen  P1-01 4.2 (with the records of P1-01 4.1), P1-02 4, P1-03 4,
            P1-04 4.2, P1-05 4.2, P1-06 4 (fixed ids)
Kinds are encoded in ascending name: biome, culture, history, race, worldgen
(P3-01 7).

The spec listings of P3-01 3.1, 4 and 5.1 and P3-02 5 are transcribed below
(RACE_FILES, CULTURE_FILES, CHRONOLOGY_FILE). The golden file is computed
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

# P3-02 3: record culture_naming.
CULTURE_NAMING = REC((
    ("onsets", ARR(STR)), ("nuclei", ARR(STR)), ("codas", ARR(STR)), ("patterns", ARR(STR)),
    ("min_letters", INT), ("max_letters", INT), ("forbidden", ARR(STR)), ("god_forms", ARR(STR)),
    ("civilization_forms", ARR(STR)), ("settlement_forms", ARR(STR)),
))

# P3-01 4.
CULTURE = (("race", STR), ("name", STR), ("naming", CULTURE_NAMING))

# P3-01 5.1.
CHRONOLOGY = (("awakening_jitter_frac", FX), ("awakening_min_gap_years", INT), ("age_name_form", STR))

# P3-01 3.
RACE_SITE_WEIGHTS = REC(fields("fertility:I forage:I timber:I ore:I fresh_water:I"))
RACE_BIOME = REC(fields("biome:S weight:I"))
RACE = (
    ("order", INT), ("name", STR), ("plural", STR), ("adjective", STR), ("camp", STR), ("awakening_share", FX),
    ("lifespan_years", INT), ("generation_years", INT), ("growth_permille", INT),
    ("site_weights", RACE_SITE_WEIGHTS), ("biomes", ARR(RACE_BIOME)), ("upland", INT), ("coast", INT),
    ("expansion", INT), ("aggression", INT), ("settlement_kinds", ARR(STR)), ("culture", STR),
)

# Kinds: name -> (directory, schema). A schema is a tuple of fields (open
# kind) or a dict id -> fields (fixed-id kind).
KINDS = {
    "biome": ("biomes", BIOME),
    "culture": ("cultures", CULTURE),
    "history": ("history", {"chronology": CHRONOLOGY}),
    "race": ("races", RACE),
    "worldgen": ("worldgen", {
        "biomes": BIOMES_TUNING, "climate": CLIMATE, "elevation": ELEVATION, "hydrology": HYDROLOGY,
        "regions": REGIONS, "resources": RESOURCES,
    }),
}
NEW_KINDS = ("culture", "history", "race")

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
site_weights = { fertility = 40, forage = 20, timber = 10, ore = 30, fresh_water = 0 }
biomes = [
  { biome = "heath", weight = 200 },
  { biome = "highland_moor", weight = 200 },
  { biome = "steppe", weight = 150 },
  { biome = "grassland", weight = 100 },
  { biome = "marsh", weight = 100 },
  { biome = "alpine", weight = -400 },
]
upland = 100
coast = -100
expansion = 600
aggression = 700
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
site_weights = { fertility = 0, forage = 60, timber = 20, ore = 0, fresh_water = 20 }
biomes = [
  { biome = "fen", weight = 600 },
  { biome = "wet_woodland", weight = 600 },
  { biome = "marsh", weight = 400 },
  { biome = "temperate_forest", weight = 200 },
  { biome = "highland_moor", weight = 100 },
  { biome = "grassland", weight = -200 },
  { biome = "alpine", weight = -300 },
  { biome = "steppe", weight = -400 },
]
upland = 0
coast = 0
expansion = 100
aggression = 50
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

SPEC_FILES = {"race": RACE_FILES, "culture": CULTURE_FILES, "history": CHRONOLOGY_FILE}

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
        return None
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
        check_record(RACE, r, "race." + rid)
    for cid, cu in cultures.items():
        check_record(CULTURE, cu, "culture." + cid)
    check_record(CHRONOLOGY, parsed_spec("history")["chronology"], "history.chronology")
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
