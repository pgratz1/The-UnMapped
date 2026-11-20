#!/usr/bin/env python3
"""Independent exact-integer reference of P3-05, peoples.

Written by the test writer from docs/specs/p3-05-peoples.md (r2) alone,
with the engine seam of docs/specs/p3-04-simulation-engine.md (r6: 6, 7,
8, 16.6), without reading any implementation and without the spec
writer's prototype. It reproduces the history stage of generator 0.15.0
(schema 7), which is at 0.16.0 the peoples-only configuration (P3-06 adds
conflict to the shipped stage; p3_conflict_reference.py extends this file;
the catalogue is P3-06's 30 kinds): the peoples awakening handler (7) and the peoples subsystem
(steps T1, T2, T4, Q1, Q3, Q4; sections 4 to 10) on the P3-04 engine of
p3_engine_reference.py (scheduler SC1-SC6, event writer, quiet eras,
bookkeeping, catalogue, template grammar, renderer), and writes every row
of the thirteen history tables.

Libraries reused, each written independently from its own spec:
core_reference.py (P0-01), p3_data_reference.py (P0-03/P3-01, extended
for history.peoples, density_permille and the 0.15.0 race data),
p3_names_reference.py (P3-02: keyed draws, chance, pick, make_name),
p3_chronology_reference.py (P3-03), p3_engine_reference.py (P3-04),
p0_world_check.py (layer blobs).

Readings of the spec where it leaves a choice open are marked READING
below and listed in SPEC_GAPS (printed by `gaps`).

Python 3 standard library only, arbitrary-precision integers, no floating
point.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p3_peoples_reference.py check [--inputs DIR] [--quick]
        Self-checks: hand-made unit cases of 16.1, the invariants of 14 on
        the synthetic geographies and the world cases, the worked example of
        15.2 (seed 42 at 1024: first 60 sentences and the end state), and
        the committed golden file against a fresh computation (skipped with
        --quick). Exit 1 on any disagreement.

    python3 -B worldgen/tests/reference/p3_peoples_reference.py golden [PATH|-] [--inputs DIR]
        Writes worldgen/tests/golden/p3-05-peoples.txt. World cases read
        their geography from DIR (default
        ~/.cache/the-unmapped/out/p3-04-tests/inputs: one <case>.db per
        case, any world whose stages include regions; example-42-1024.db is
        the input of hist-peoples-42-1024).

    python3 -B worldgen/tests/reference/p3_peoples_reference.py world DB [--data ROOT] [--span E] [--races R]
                [--detail D] [--civs C] [--intensity I]
        Recomputes the peoples history of a world from its stored geography
        (the chronology recomputed for the given settings, default the
        world's meta) and prints the sigma/C table, the homes, the table
        hashes, the [history] lines and every sentence.

    python3 -B worldgen/tests/reference/p3_peoples_reference.py check-world DB [--data ROOT]
        Checks a schema-7 world written by the C++ (validation sweep):
        recomputes the five chronology tables and the thirteen history
        tables from the stored geography and meta, compares them table by
        table (naming the first differing row), re-checks the invariants of
        14 from the stored rows, and the [history] keys of 15.4. Exit 1 on
        a difference.

    python3 -B worldgen/tests/reference/p3_peoples_reference.py synthetic SEED
        Prints the synthetic geography of a seed (the C++ property tests
        build the same one) and its history.

    python3 -B worldgen/tests/reference/p3_peoples_reference.py gaps
        Prints the spec gaps and the readings taken.

Never regenerate the golden file to make a C++ test pass.
"""

import copy
import os
import sqlite3
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import core_reference as cr  # noqa: E402
import p0_world_check as pwc  # noqa: E402
import p3_chronology_reference as pchr  # noqa: E402
import p3_data_reference as pdr  # noqa: E402
import p3_engine_reference as pe  # noqa: E402
import p3_names_reference as pnr  # noqa: E402

FatalError = cr.FatalError
REPO = pe.REPO
GOLDEN = os.path.join(REPO, "worldgen", "tests", "golden", "p3-05-peoples.txt")
DEFAULT_INPUTS = pe.DEFAULT_INPUTS
GENERATOR_VERSION = "0.17.0"  # P3-08: P3-05 r6 rules, kind 31, P3-04 r10 quiet eras
ONE = 1 << 32

# ---------------------------------------------------------------------------
# P3-05 r6 (generator 0.17.0, docs/specs/p3-08-followups.md) and P3-04 r10's
# quiet eras: each rule change behind a flag, so that with every flag off
# (legacy()) and the 0.16.0 data, names and engine the reference reproduces
# the 0.16.0 worlds and goldens. The data-only changes of r6 (faerie
# weights, famine_ppm 1250, plague_spread_permille 100, templates) come from
# the data root (p3_data_reference.py).
# ---------------------------------------------------------------------------
FLAGS = dict(
    cell=True,          # 6.2: the per-race cell term, no seed-cell rule for a race with one
    fortress_cell=True,  # 8.4: a founding with a cell term on a cell with walk_area 0 is a fortress
    last=True,          # 8.5 (a): no chance abandonment of a civilization's last standing settlement
    causes=True,        # 8.5: the abandonment's causes gain the decline start
    dest=True,          # 8.5 item 1: kind 31 settlement_abandoned_to when a survivor reaches a destination
    cap=True,           # 9.1: the target T_s = min(C_s, kind_cap(s))
    plague_sal=True,    # 10.2: the outbreak's salience by its planned total toll
    crisis_sal=True,    # 10.3: the split test first; a crisis that does not split has crisis_small_salience
    quiet=True,         # P3-04 r10 6.6: a civilization's quiet era is not closed by its own events
)


def legacy(on=True):
    """Every r6 / r10 flag off (the 0.16.0 rules) or on (0.17.0)."""
    for k in FLAGS:
        FLAGS[k] = not on

RACE, GOD, CIV, SETTLEMENT, REGION = pe.RACE, pe.GOD, pe.CIV, pe.SETTLEMENT, pe.REGION
OPENING, QUIET, FLARE, FINAL = pe.OPENING, pe.QUIET, pe.FLARE, pe.FINAL
HAMLET, VILLAGE, TOWN, CITY, FORTRESS = 1, 2, 3, 4, 5
KIND_KEY = {1: "hamlet", 2: "village", 3: "town", 4: "city", 5: "fortress"}
KIND_CODE = {v: k for k, v in KIND_KEY.items()}
THRIVING, DECLINING, RUINED, FADED = 1, 2, 3, 4


def fatal(msg):
    raise FatalError(msg)


SPEC_GAPS = (
    ("14 item 2 against 6.2", "RULED (lead, ruling 1): 6.2 rules, as checked here. 'No two settlements share a cell' contradicts 6.2, where a faded settlement does "
                              "not block and the seed cell or the best cell may be the faded one's own cell (the "
                              "stress cases reach it). The reference follows 6.2 and checks invariant 2 as: no two "
                              "settlements share a cell unless the earlier one faded before the later was founded."),
    ("4.2 S", "Sum first, one floor division: S = (sum_k w_k m_k) * 1000 div (255 * sum_k w_k); the formula has "
              "no parentheses, B's has."),
    ("7.3 item 3 'kind by 8.4'", "A capital that is not a fortress gets ladder(r, founding_population) (a "
                                 "village at 300 for races that build villages), as 9.3 item 9 does for colonies."),
    ("11 civ_ended settlement_use 1", "civ_ended carries the abandoned settlement (8.5 item 6) or the settlement "
                                      "ruined last (9.1) as its settlement; the region is that settlement's."),
    ("10.2 spread 'owned by s's owner'", "RULED (lead, ruling 6): the owner after the outbreak's strike; an "
                                         "outbreak that abandons s spreads nowhere (SPREAD_OWNER = after)."),
    ("10 order in T4", "Famine and plague over the settlements first (10.2), then the crises over the "
                       "civilizations (10.3), in the order of the sections."),
    ("10.3 crisis loss and 8.5 (a)", "The capital's crisis loss is not a strike: it records `before`, sets "
                                     "declining and the last reduction event, but does not apply 8.5 (a)."),
    ("10.3 d_cap start", "d_cap starts at the capital's region whether or not c holds it, and the search runs "
                         "over c's held regions only."),
    ("10.3 new civilization", "culture = the parent's culture; home_region = the new capital's region."),
    ("8.6 / P3-04 9.2 faded rows", "RULED (lead, ruling 3): fading clears ruined_year and ruin_event; invariant 7 "
                                   "dates the fading against the ruin_faded event's cause."),
    ("9.2 recovery in Q1", "RULED (lead, ruling 4): invariant 9 governs; 8.3's recovery runs each year inside Q1."),
    ("6.6 W9 'quiet step'", "The quiet-event budget and the 'charged in this step' rule of the quiet eras "
                            "count over the whole quiet step (Q1 to Q6), as P3-04 6.6 counts per quiet step."),
    ("8.5 (b) and 8.2 in Q1", "Abandonment (b) and kind changes at the end of Q1 run over the settlements in "
                              "ascending id, each needing its events' budget, else waiting; T1's civilization "
                              "ending (9.1 first paragraph) is not applied in Q1."),
    ("9.3 item 3 outward site", "When the best outward region has no free site there is no outward target "
                                "(the next region is not tried), as the text says."),
    ("9.3 item 3 reach origin", "hops are counted from the region of c's capital settlement over every open "
                                "border (not only held regions)."),
    ("5.1 bound guard message", "Not given by the spec; implemented in P3-04 6.1's bound form 'history: bound: "
                                "capacity of region <R> for race <r> exceeds 2147483647' (the reference and tests follow)."),
    ("6.2 fatal site", "'history: peoples: no settlement site in region <id>' for an awakening; a colony with no "
                       "site has no target."),
    ("12.2 array positions", "Changed biome weights keep their place in the race file; the added entries (goblin "
                             "temperate_forest, wet_woodland; faerie heath) are appended at the end of `biomes`. "
                             "This decides the content hash (p3-01-data.txt), not any world table."),
    ("11 report.toml", "The fourteen new entries are appended after the five of P3-04 in kind id order."),
    ("15.4 regions_held_<race>", "Counted from the open region_control rows (to_year NULL) by the race of their "
                                 "civ."),
)


# ---------------------------------------------------------------------------
# 11: the catalogue, kinds 6-19 appended to P3-04's five
# ---------------------------------------------------------------------------

def _k(kid, key, caused, roles, values):
    return pe.kind_def(kid, key, caused, 1, 1, roles, values)


# Lead's ruling 4 (2026-11-19): recovery per year inside Q1. False gives the literal 9.2 reading.
QUIET_RECOVER_EACH_YEAR = True
# Lead's ruling 6 (2026-11-19): "s's owner" (10.2) is the owner after the outbreak's strike, so an outbreak
# that abandons s spreads nowhere. "outbreak" (the owner before the strike) is kept for comparison only.
SPREAD_OWNER = "after"

SET_CIV = (("settlement", SETTLEMENT, 1, 1), ("civilization", CIV, 1, 1))
PEOPLES_KINDS = (
    _k(6, "colony_founded", 0, (("settlement", SETTLEMENT, 1, 1), ("founder", CIV, 1, 1),
                                ("origin", SETTLEMENT, 1, 1)), ("population",)),
    _k(7, "became_village", 1, SET_CIV, ("population",)),
    _k(8, "became_town", 1, SET_CIV, ("population",)),
    _k(9, "became_city", 1, SET_CIV, ("population",)),
    _k(10, "settlement_dwindled", 0, SET_CIV, ("population",)),
    _k(11, "famine", 0, SET_CIV, ("dead", "population")),
    _k(12, "plague", 0, SET_CIV, ("dead", "population")),
    _k(13, "plague_spread", 1, SET_CIV, ("dead", "population")),
    _k(14, "succession_crisis", 0, (("civilization", CIV, 1, 1),), ()),
    _k(15, "civ_seceded", 1, (("civilization", CIV, 1, 1), ("from", CIV, 1, 1)), ("settlements",)),
    _k(16, "settlement_abandoned", 1, SET_CIV, ("survivors",)),
    _k(17, "capital_moved", 1, (("civilization", CIV, 1, 1), ("settlement", SETTLEMENT, 1, 1)), ()),
    _k(18, "civ_ended", 1, (("civilization", CIV, 1, 1),), ()),
    _k(19, "ruin_faded", 1, (("settlement", SETTLEMENT, 1, 1),), ()),
)
# P3-05 r6 11: kind 31, appended after P3-06's kinds 20-30 (generator 0.17.0).
KIND31 = pe.kind_def(31, "settlement_abandoned_to", 1, 1, 1,
                     (("settlement", SETTLEMENT, 1, 1), ("civilization", CIV, 1, 1),
                      ("destination", SETTLEMENT, 1, 1)), ("settled", "lost"))
# p3_engine_reference.py carries the catalogue (kinds 1-30 at 0.16.0; 31 from 0.17.0 once its own
# transcription has it); this file's transcription of section 11 must equal its kinds 6-19 and 31.
assert list(pe.COMPILED[5:19]) == list(PEOPLES_KINDS) and len(pe.COMPILED) in (30, 31)
assert len(pe.COMPILED) == 30 or pe.COMPILED[30] == KIND31, "engine reference kind 31 differs from P3-05 r6 11"
COMPILED_016 = tuple(pe.COMPILED[:30])
COMPILED_017 = COMPILED_016 + (KIND31,)
# COMPILED (every kind id a row may hold) is the 0.17.0 list; the catalogue a run writes is
# catalogue_kinds(), which drops kind 31 when FLAGS["dest"] is off (the 0.16.0 catalogue).
COMPILED = COMPILED_017
KIND_ID = {k["key"]: k["id"] for k in COMPILED}


def catalogue_kinds():
    return COMPILED_017 if FLAGS["dest"] else COMPILED_016
# Quiet-step kinds (14 item 8): 6-10, 16-19 (and P3-04's quiet_era).
QUIET_KINDS = {6, 7, 8, 9, 10, 16, 17, 18, 19, 31}   # 31: r6, an abandonment of 9.2 in Q1

SPEC_REPORT = (
    ("colony_founded", 15, "{founder} sent settlers from {origin} to found {settlement} in {@region}."),
    ("became_village", 10, "{settlement} grew into a village of {population}."),
    ("became_town", 18, "{settlement} grew into a town of {population}."),
    ("became_city", 45, "{settlement} grew into a city of {population}."),
    ("settlement_dwindled", 15, "{settlement} dwindled to {population} souls."),
    ("famine", 25, "Famine struck {settlement}; {dead} died."),
    ("plague", 45, "Plague broke out in {settlement}; {dead} died."),
    ("plague_spread", 25, "The plague spread to {settlement}; {dead} died."),
    ("succession_crisis", 40, "A succession crisis at {@settlement} shook {civilization}."),   # r6
    ("civ_seceded", 70, "{civilization} broke away from {from}, taking {settlements} "
                        "{settlements?settlement|settlements} with it."),
    ("settlement_abandoned", 55, "{settlement} was abandoned; {survivors?no one survived|its one survivor was "
                                 "scattered|its # survivors were scattered}."),   # r6
    ("capital_moved", 50, "{civilization} moved its seat to {settlement}."),
    ("civ_ended", 85, "{civilization} came to an end."),
    ("ruin_faded", 10, "The ruins of {settlement} faded from memory."),
)
# The 0.16.0 texts of the two templates r6 changes (kinds 14 and 16).
SPEC_REPORT_016 = {
    "succession_crisis": "A succession crisis shook {civilization}.",
    "settlement_abandoned": "{settlement} was abandoned; {survivors} {survivors?survivor|survivors} left.",
}
# P3-05 r6 11: kind 31's entry (appended after kind 30's).
SPEC_REPORT_31 = ("settlement_abandoned_to", 55, "{settlement} was abandoned; {settled} {settled?survivor|survivors} "
                  "went to {destination}{lost?|, and one was lost|, and # were lost}.")


def compiled_catalogue():
    return copy.deepcopy(list(catalogue_kinds()))


def event_catalogue(report):
    cat = compiled_catalogue()
    by = {k: (s, t) for k, s, t in report}
    for k in cat:
        if k["key"] in by:
            k["salience"], k["templ"] = by[k["key"]]
    return cat


def report_events_messages(report):
    """P3-04 4.2 check report_events against the 0.15.0 catalogue."""
    saved = pe.COMPILED
    pe.COMPILED = catalogue_kinds()
    try:
        return pe.report_events_messages(report)
    finally:
        pe.COMPILED = saved


# ---------------------------------------------------------------------------
# 3.3 streams and helpers
# ---------------------------------------------------------------------------

STREAMS = ("hist.peoples.home", "hist.peoples.civs", "hist.peoples.found", "hist.peoples.outward",
           "hist.peoples.famine", "hist.peoples.famine_toll", "hist.peoples.plague", "hist.peoples.plague_toll",
           "hist.peoples.plague_spread", "hist.peoples.abandon", "hist.peoples.crisis", "hist.peoples.split")


def civs_param(text):
    """--civs-per-race text -> P3-03 params value (None = auto)."""
    return None if text in (None, "auto") else int(text)


def chance_pm(u, p):
    return pnr.chance(u, min(ONE, p * ONE // 1000))


def chance_ppm(u, p):
    return pnr.chance(u, min(ONE, p * ONE // 1000000))


# ---------------------------------------------------------------------------
# Data: P3-04's view plus history.peoples and the race fields of 3.1
# ---------------------------------------------------------------------------

class Data:
    def __init__(self, root):
        self.root = root
        self.base = pe.Data(root)
        self.chron = self.base.chron
        self.engine = self.base.engine
        self.report = self.base.report
        self.naming = self.base.naming
        content = pdr.load_root(root, kinds=("history", "race"))
        hist = dict(content["history"])
        self.peoples = hist["peoples"]
        self.race = {}
        for key, t in content["race"]:
            rid = self.chron.race_by_id[key]["order"]
            self.race[rid] = dict(key=key, site=t["site_weights"], upland=t["upland"], coast=t["coast"],
                                  growth=t["growth_permille"], expansion=t["expansion"],
                                  kinds=[KIND_CODE[k] for k in t["settlement_kinds"]],
                                  density=t["density_permille"], generation=t["generation_years"],
                                  culture=t["culture"],
                                  # P3-01 r8 / P3-05 r6 6.2 (0.17.0): absent in a 0.16.0 root, read as 0, 0
                                  cell=dict(t.get("cell_weights", dict(ore=0, height=0))))


_DATA = {}


def data(root="game/data"):
    if root not in _DATA:
        _DATA[root] = Data(root)
    return _DATA[root]


def edited(dat, peoples=None, races=None, engine=None):
    """A copy of dat with history.peoples fields, race fields (by race id) or
    history.engine fields replaced (hand-made cases)."""
    d = copy.copy(dat)
    d.peoples = dict(dat.peoples)
    d.peoples.update(peoples or {})
    d.race = copy.deepcopy(dat.race)
    for rid, fields in (races or {}).items():
        d.race[rid].update(fields)
    d.engine = copy.deepcopy(dat.engine)
    d.engine.update(engine or {})
    return d


# ---------------------------------------------------------------------------
# Inputs (P3-04 3.3 with P3-05 3.2)
# ---------------------------------------------------------------------------

REGION_COLS = ("id", "name", "x_min", "y_min", "x_max", "y_max", "seed_x", "seed_y", "cells", "lake_cells",
               "walk_cells", "habitability_mean", "coast_cells", "fertility_mean", "forage_mean", "timber_mean",
               "fresh_water_mean", "ore_reach_cells", "elev_max_dm")   # elev_max_dm: r6 6.2
BORDER_COLS = ("a", "b", "length_m", "walk_pairs", "river_pairs", "pass_id")
LAYERS = ("region", "water", "habitability", "river", "flow_dir", "elevation", "ore", "walk_area")   # r6 3.2


class Inputs(pe.Inputs):
    def __init__(self, seed, map_size, params, regions, races, race_biomes, cultures, gods, ages, borders,
                 region_biomes, biomes, layers):
        super().__init__(seed, map_size, params, regions, races, cultures, gods, ages, None)
        self.race_biomes = race_biomes      # (race, biome code, weight)
        self.borders = borders              # [dict] (a, b) order
        self.region_biomes = region_biomes  # (region, biome, cells)
        self.biomes = biomes                # {code: water 0/1}
        self.peoples_layers = layers        # callable () -> {name: values row-major}


def world_geo(conn):
    regions = [dict(zip(REGION_COLS, r)) for r in conn.execute(
        "SELECT %s FROM region ORDER BY id" % ", ".join(REGION_COLS)).fetchall()]
    borders = [dict(zip(BORDER_COLS, r)) for r in conn.execute(
        "SELECT %s FROM region_border ORDER BY a, b" % ", ".join(BORDER_COLS)).fetchall()]
    rb = [tuple(r) for r in conn.execute("SELECT region, biome, cells FROM region_biome ORDER BY region, biome")]
    biomes = {r[0]: r[1] for r in conn.execute("SELECT id, water FROM biome ORDER BY id")}
    blobs = {}
    for name in LAYERS:
        row = conn.execute("SELECT data FROM layer WHERE name = ?", (name,)).fetchone()
        blobs[name] = row[0] if row else None
    cache = LazyLayers(blobs)

    def layers():
        return cache
    return regions, borders, rb, biomes, layers


class LazyLayers(dict):
    """Layer values decoded on first use (the r6 layers elevation, ore and walk_area are read only for a
    race with a cell term)."""

    def __init__(self, blobs):
        super().__init__()
        self.blobs = blobs

    def __missing__(self, name):
        blob = self.blobs.get(name)
        v = pwc.decode_blob(blob)[4] if blob is not None else None
        self[name] = v
        return v


def inputs_for(dat, conn, p, seed, size):
    """Inputs from a stored geography and the reference's own chronology."""
    regions, borders, rb, biomes, layers = world_geo(conn)
    chron = pchr.chronology(dat.chron, p, seed, [r["name"] for r in regions])
    return Inputs(seed, size, p, regions, chron["race"], chron["race_biome"], chron["culture"], chron["god"],
                  chron["age"], borders, rb, biomes, layers), chron


# ---------------------------------------------------------------------------
# 4, 5, 6.1: the geography view
# ---------------------------------------------------------------------------

class Geo:
    def __init__(self, inp, dat):
        self.inp = inp
        self.dat = dat
        self.t = dat.peoples
        self.n = inp.map_size
        self.reg = {r["id"]: r for r in inp.regions}
        self.ids = [r["id"] for r in inp.regions]
        islet_max = dat.engine["islet_max_dry_cells"]
        t = self.t
        self.q = {}
        for r in inp.regions:
            dry = r["cells"] - r["lake_cells"]
            q = dict(dry=dry, islet=dry < islet_max, upland=4 * r["walk_cells"] < r["cells"])
            land = sum(c for (rg, b, c) in inp.region_biomes if rg == r["id"] and inp.biomes.get(b, 1) == 0)
            q["land"] = land
            q["share"] = {b: (c * 1000 // land if land > 0 else 0) for (rg, b, c) in inp.region_biomes
                          if rg == r["id"]}
            q["rough"] = (dry - r["walk_cells"]) * 1000 // dry if dry > 0 else 0
            q["ore_m"] = min(255, r["ore_reach_cells"] * 255 // t["ore_reach_full"])
            q["coast_m"] = min(1000, r["coast_cells"] * 1000 // t["coast_full"])
            self.q[r["id"]] = q
        self.nb = {rid: [] for rid in self.ids}
        for b in inp.borders:
            if b["walk_pairs"] > 0 or b["pass_id"] > 0:
                cost = 1 if b["walk_pairs"] > 0 else t["pass_cost"]
                if b["length_m"] > 0 and b["river_pairs"] * 16 * 1000 >= t["river_border_share_permille"] * b["length_m"]:
                    cost += t["river_cost"]
                self.nb[b["a"]].append((b["b"], cost))
                self.nb[b["b"]].append((b["a"], cost))
        for rid in self.ids:
            self.nb[rid].sort()
        self.rbw = {}
        for (race, b, w) in inp.race_biomes:
            self.rbw.setdefault(race, {})[b] = w
        self._sigma = {}
        self._cap = {}
        self._hops = {}
        self._cand = {}
        self._cells = None
        self.lay = None

    # 4.1, 4.2
    def parts(self, rid, race):
        r = self.reg[rid]
        q = self.q[rid]
        rd = self.dat.race[race]
        sw = rd["site"]
        m = dict(fertility=r["fertility_mean"], forage=r["forage_mean"], timber=r["timber_mean"], ore=q["ore_m"],
                 fresh_water=r["fresh_water_mean"])
        keys = ("fertility", "forage", "timber", "ore", "fresh_water")
        wsum = sum(sw[k] for k in keys)
        s_ = sum(sw[k] * m[k] for k in keys) * 1000 // (255 * wsum)   # READING (SPEC_GAPS 4.2 S)
        b_ = sum(w * q["share"].get(b, 0) for b, w in self.rbw.get(race, {}).items()) // 1000
        u_ = rd["upland"] * q["rough"] // 1000
        c_ = rd["coast"] * q["coast_m"] // 1000
        return s_, b_, u_, c_

    def sigma(self, rid, race):
        key = (rid, race)
        if key not in self._sigma:
            t = self.t
            s_, b_, u_, c_ = self.parts(rid, race)
            self._sigma[key] = (t["score_site"] * s_ + t["score_biome"] * b_ + t["score_upland"] * u_ +
                                t["score_coast"] * c_) // 1000
        return self._sigma[key]

    def fit(self, rid, race):
        return min(1000, max(0, self.sigma(rid, race)))

    # 5.1
    def capacity(self, rid, race):
        key = (rid, race)
        if key not in self._cap:
            t = self.t
            rd = self.dat.race[race]
            r = self.reg[rid]
            dry = self.q[rid]["dry"]
            rough_r = t["rough_base"] + max(0, rd["upland"]) * t["rough_upland"] // 1000
            area = r["walk_cells"] + (dry - r["walk_cells"]) * rough_r // 1000
            prod = area * self.fit(rid, race) * rd["density"] * t["people_per_kcell"]
            c = max(t["capacity_floor"], prod // 10 ** 9)
            if c > (1 << 31) - 1:
                fatal("history: bound: capacity of region %d for race %d exceeds %d" % (rid, race, (1 << 31) - 1))
            self._cap[key] = c
        return self._cap[key]

    def islet(self, rid):
        return self.q[rid]["islet"]

    def upland(self, rid):
        return self.q[rid]["upland"]

    # 4.4
    def hops(self, r0):
        if r0 not in self._hops:
            d = {r0: 0}
            fifo = [r0]
            i = 0
            while i < len(fifo):
                x = fifo[i]
                i += 1
                for y, _c in self.nb.get(x, ()):
                    if y not in d:
                        d[y] = d[x] + 1
                        fifo.append(y)
            self._hops[r0] = d
        return self._hops[r0]

    # 6.1, 6.2: candidate cells of a region in site order for a race's coast
    def layers(self):
        if self.lay is None:
            self.lay = self.inp.peoples_layers()
            n = self.n
            wat = self.lay["water"]
            riv = self.lay["river"]
            fd = self.lay["flow_dir"]
            t = self.t
            rad = t["port_radius"]
            D8 = ((1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1))
            mouth_rows = [0] * n
            self.mouths = []
            for i in range(n * n):
                if wat[i] == 1 and riv[i] >= t["mouth_class_min"] and 0 <= fd[i] <= 7:
                    x, y = i % n, i // n
                    dx, dy = D8[fd[i]]
                    tx, ty = x + dx, y + dy
                    if 0 <= tx < n and 0 <= ty < n and wat[ty * n + tx] == 3:
                        mouth_rows[y] |= 1 << x
                        self.mouths.append(i)
            sea_rows = []
            table = bytes(49 if v == 3 else 48 for v in range(256))
            for y in range(n):
                s = bytes(wat[y * n:(y + 1) * n]).translate(table)[::-1]
                sea_rows.append(int(s, 2))
            self.near_mouth = self.dilate(mouth_rows, rad)
            self.near_sea = self.dilate(sea_rows, rad)
        return self.lay

    def dilate(self, rows, rad):
        n = self.n
        full = (1 << n) - 1
        h = []
        for v in rows:
            d = v
            for k in range(1, rad + 1):
                d |= (v << k) | (v >> k)
            h.append(d & full)
        out = []
        for y in range(n):
            d = 0
            for yy in range(max(0, y - rad), min(n, y + rad + 1)):
                d |= h[yy]
            out.append(d)
        return out

    def port(self, i, race):
        n = self.n
        x, y = i % n, i // n
        t = self.t
        if (self.near_mouth[y] >> x) & 1:
            b = t["port_mouth_bonus"]
        elif (self.near_sea[y] >> x) & 1:
            b = t["port_coast_bonus"]
        else:
            b = 0
        return b * (1000 + self.dat.race[race]["coast"]) // 1000

    def cell_weights(self, race):
        """(cell_ore_r, cell_height_r) of P3-05 r6 6.2; (0, 0) with FLAGS["cell"] off (0.16.0)."""
        if not FLAGS["cell"]:
            return 0, 0
        cw = self.dat.race[race]["cell"]
        return cw["ore"], cw["height"]

    def has_cell_term(self, race):
        return self.cell_weights(race) != (0, 0)

    def hgt(self, i, rid):
        """r6 6.2: the cell's height in its region, 0-255."""
        e = self.layers()["elevation"][i]
        return min(255, max(0, e) * 255 // max(1, self.reg[rid]["elev_max_dm"]))

    def value(self, i, rid, race):
        """r6 6.2: v(c, r) = habitability(c) + port(c, r) + (cell_ore_r ore(c) + cell_height_r hgt(c)) div 1000."""
        lay = self.layers()
        v = lay["habitability"][i] + self.port(i, race)
        co, ch = self.cell_weights(race)
        if (co, ch) != (0, 0):
            v += (co * lay["ore"][i] + ch * self.hgt(i, rid)) // 1000
        return v

    def candidates(self, rid, race):
        """[(value, index)] of the region's candidates, value descending, index ascending."""
        coast = self.dat.race[race]["coast"]
        key = (rid, coast, self.cell_weights(race))
        if key not in self._cand:
            lay = self.layers()
            n = self.n
            reg, wat = lay["region"], lay["water"]
            r = self.reg[rid]
            out = []
            for y in range(r["y_min"], r["y_max"] + 1):
                for x in range(r["x_min"], r["x_max"] + 1):
                    i = y * n + x
                    if reg[i] == rid and wat[i] == 0:
                        out.append((-self.value(i, rid, race), i))
            out.sort()
            self._cand[key] = [(-v, i) for v, i in out]
        return self._cand[key]

    def walkable(self, x, y):
        """r6 8.4: walk_area(c) > 0."""
        return self.layers()["walk_area"][y * self.n + x] > 0

    def is_candidate(self, rid, x, y):
        lay = self.layers()
        n = self.n
        if not (0 <= x < n and 0 <= y < n):
            return False
        i = y * n + x
        return lay["region"][i] == rid and lay["water"][i] == 0


# ---------------------------------------------------------------------------
# The world: P3-04's World with the peoples handler and subsystem
# ---------------------------------------------------------------------------

class World(pe.World):
    def __init__(self, dat, inp, cat, extra_draw=False):
        super().__init__(dat.base, inp, cat, dat.engine)
        self.pd = dat
        self.t = dat.peoples
        self.geo = Geo(inp, dat)
        self.serial = 0
        self.year = 0
        self.years = 1
        self.phase = 0
        self.quiet_used = None
        self.charged = set()
        self.homes_log = []        # (race, year, [homes], n requested)
        self.site_how = {}         # settlement -> seed / best / relaxed
        self.hunger = {}           # civ -> years
        self.hunger_max = 0
        self.before = {}           # settlement -> population before the first blow while thriving
        self.last_red = {}         # settlement -> last reduction event
        self.ruined_from = {}      # settlement -> civilization that lost it
        self.decline_start = {}    # r6 8.5: settlement -> the event that last turned it from thriving to declining
        self.plague_plans = []     # r6 10.2: (outbreak event, planned total, actual total) for the checks
        self.extra_draw = extra_draw
        self.keys = {s: cr.stream_key(inp.seed, s) for s in STREAMS}
        self.intensity = self.t["intensity"][inp.params["intensity"]]
        self.race_key = {r[0]: r[1] for r in inp.races}
        self.checks = []           # in-run invariant failures (14 items 3, 4, 5)

    # --- draws (3.3)
    def draw(self, stream, a, b, k):
        if self.extra_draw:   # 14 item 10 / Y3: a test-only extra draw before every decision
            pnr.draw(self.keys[stream], self.year, a + 1000003, b, k + 7)
        return pnr.draw(self.keys[stream], self.year, a, b, k)

    # --- the event writer (6.6) with W9 and salience overrides
    def append(self, kind_key, year=None, end_year=None, region=0, settlement=0, participants=(), values=(),
               causes=(), phase=None, salience=None, quiet_step=None):
        year = self.year if year is None else year
        end_year = year if end_year is None else end_year
        if self.quiet_used is not None and kind_key != "quiet_era":
            ch = 0
            for (_role, typ, x) in participants:
                if typ == CIV:
                    ch = x
                    break
            if self.quiet_events_left(ch) <= 0:
                fatal("history: event %d (%s): W9 quiet budget of civilization %d exhausted" %
                      (len(self.events) + 1, kind_key, ch))
            self.quiet_used[ch] = self.quiet_used.get(ch, 0) + 1
            self.charged.add(ch)
        k = self.cat_by_key[kind_key]
        if k["region_use"] == 1:
            assert region in self.geo.reg, (kind_key, region)
        if k["settlement_use"] == 1:
            assert 1 <= settlement <= len(self.settlements) + 1, (kind_key, settlement)
        eid = super().append(kind_key, year, end_year, region=region, settlement=settlement,
                             participants=participants, values=values, causes=causes,
                             phase=self.phase if phase is None else phase)
        if salience is not None:
            self.events[-1]["salience"] = salience
        return eid

    def quiet_events_left(self, civ):
        if self.quiet_used is None:
            return self.eng["quiet_events_max"]
        return self.eng["quiet_events_max"] - self.quiet_used.get(civ, 0)

    # --- steps and visibility (6.2)
    def begin(self):
        self.serial += 1

    def visible(self, e):
        return e["created"] < self.serial

    # --- State (6.1)
    def civ(self, c):
        return self.civs[c - 1]

    def stl(self, s):
        return self.settlements[s - 1]

    def standing(self, s):
        return s["status"] in (THRIVING, DECLINING)

    def found_civilization(self, race, culture, home, name, event):
        c = len(self.civs) + 1
        self.civs.append(dict(id=c, name=name, race=race, culture=culture, home_region=home, capital=0,
                              founded_year=self.year, founding_event=event, ended_year=None, ending_event=0,
                              created=self.serial))
        self.hunger[c] = 0
        return c

    def found_settlement(self, x, y, region, civ, kind, pop, name, event):
        s = len(self.settlements) + 1
        if self.inp.peoples_layers()["region"][y * self.inp.map_size + x] != region:
            fatal("history: state: found_settlement: cell (%d, %d) not in region %d" % (x, y, region))
        self.settlements.append(dict(id=s, name=name, x=x, y=y, region=region, founder_civ=civ,
                                     founded_year=self.year, founding_event=event, owner_civ=civ, kind=kind,
                                     status=THRIVING, population=pop, ruined_year=None, ruin_event=0,
                                     created=self.serial))
        return s

    def set_owner(self, rid, civ, event):
        self.owner[rid] = (civ, event)

    def owner_of(self, rid):
        return self.owner[rid][0]

    def race_of(self, s):
        return self.civ(s["owner_civ"])["race"]

    def standing_of(self, c, include_new=True):
        return [s for s in self.settlements if s["owner_civ"] == c and self.standing(s)
                and (include_new or self.visible(s))]

    def held(self, c):
        return [rid for rid in self.geo.ids if self.owner[rid][0] == c]

    # --- 8.1 kinds
    def ladder(self, race, p):
        t = self.t
        mins = {HAMLET: 0, VILLAGE: t["village_min"], TOWN: t["town_min"], CITY: t["city_min"]}
        built = [k for k in self.pd.race[race]["kinds"] if k != FORTRESS]
        if not built:
            return FORTRESS
        at_or_below = [k for k in built if mins[k] <= p]
        return max(at_or_below) if at_or_below else min(built)

    def kmin(self, k):
        t = self.t
        return {HAMLET: 0, VILLAGE: t["village_min"], TOWN: t["town_min"], CITY: t["city_min"]}[k]

    def kind_cap(self, s):
        t = self.t
        if s["kind"] == FORTRESS:
            return t["fortress_max"]
        built = [k for k in self.pd.race[self.race_of(s)]["kinds"] if k != FORTRESS]
        top = max(built) if built else FORTRESS
        return {HAMLET: t["village_min"] - 1, VILLAGE: t["town_min"] - 1, CITY: t["city_max"],
                TOWN: t["city_min"] - 1, FORTRESS: t["fortress_max"]}[top]

    def founding_kind(self, race, rid, pop, x, y):
        if FORTRESS in self.pd.race[race]["kinds"]:
            if self.geo.upland(rid):
                return FORTRESS
            # r6 8.4: a race with a cell term founding on a cell off the walkable ground
            if FLAGS["fortress_cell"] and self.geo.has_cell_term(race) and not self.geo.walkable(x, y):
                return FORTRESS
        return self.ladder(race, pop)   # READING (SPEC_GAPS 7.3) for capitals

    # --- 5.2 shares
    def shares(self):
        wsum = {}
        w = {}
        for s in self.settlements:
            if not self.standing(s):
                continue
            c = self.civ(s["owner_civ"])
            w[s["id"]] = self.t["capital_weight"] if c["capital"] == s["id"] else 1000
            wsum[s["region"]] = wsum.get(s["region"], 0) + w[s["id"]]
        out = {}
        for sid, ws in w.items():
            s = self.stl(sid)
            out[sid] = max(1, self.geo.capacity(s["region"], self.race_of(s)) * ws // wsum[s["region"]])
        return out

    # --- 6.2 sites
    def site(self, rid, race, relaxed):
        t = self.t
        sp2 = t["site_spacing"] ** 2
        block = [(s["x"], s["y"]) for s in self.settlements if s["status"] in (THRIVING, DECLINING, RUINED)]
        occupied = {(s["x"], s["y"]) for s in self.settlements}
        n = self.inp.map_size

        def free(x, y):
            for bx, by in block:
                if (bx - x) ** 2 + (by - y) ** 2 < sp2:
                    return False
            return True
        r = self.geo.reg[rid]
        # r6 6.2 item 1: the seed cell only for a race whose cell_weights are both 0
        if not self.geo.has_cell_term(race) and self.geo.is_candidate(rid, r["seed_x"], r["seed_y"]) and \
                free(r["seed_x"], r["seed_y"]):
            return r["seed_x"], r["seed_y"], "seed"
        cands = self.geo.candidates(rid, race)
        for _v, i in cands:
            if free(i % n, i // n):
                return i % n, i // n, "best"
        if relaxed:
            for _v, i in cands:
                if (i % n, i // n) not in occupied:
                    return i % n, i // n, "relaxed"
        return None

    # --- 7 the awakening handler
    def order_regions(self, race):
        y = self.year
        return sorted(self.geo.ids, key=lambda rid: (-self.geo.sigma(rid, race),
                                                      self.draw("hist.peoples.home", race, rid, 0), rid))

    def eligible(self, rid, race):
        return (not self.geo.islet(rid) and self.owner[rid][0] == 0 and
                self.geo.capacity(rid, race) >= self.t["home_min_capacity"])

    def homes(self, race):
        """awakening_place: every home of this awakening (no state change)."""
        order = self.order_regions(race)
        el = [rid for rid in order if self.eligible(rid, race)]
        if el:
            first = el[0]
        else:
            pools = ([rid for rid in order if not self.geo.islet(rid) and self.owner[rid][0] == 0],
                     [rid for rid in order if not self.geo.islet(rid)], order)
            first = next(p[0] for p in pools if p)
        alive = sum(1 for c in self.civs if c["ended_year"] is None)
        setting = self.inp.params["civs"]       # None = auto (P3-03 params)
        if setting is None:
            n = 1 + (1 if chance_pm(self.draw("hist.peoples.civs", race, 0, 0), self.t["civs_two_permille"]) else 0)
            if len(el) < self.t["regions_per_civ"] * alive + 2:
                n = 1
        else:
            n = setting
        homes = [first]
        while len(homes) < n:
            nxt = None
            for rid in el:
                if rid in homes:
                    continue
                if all(self.geo.hops(h).get(rid, None) is None or self.geo.hops(h)[rid] >= self.t["home_min_hops"]
                       for h in homes):
                    nxt = rid
                    break
            if nxt is None:
                break
            homes.append(nxt)
        return homes, n

    def awaken(self, eng_run, k, year):
        self.year, self.years, self.phase = year, 1, eng_run.phase_id
        self.begin()
        race_id = self.age_race[k]
        race = self.race[race_id]
        homes, n = self.homes(race_id)
        self.homes_log.append((race_id, year, homes, n))
        e_aw = self.append("awakening", region=homes[0],
                           participants=(("race", RACE, race_id), ("creator", GOD, race[11])))
        cnum = race[8]
        cid = self.culture[cnum][1]
        naming = self.dat.naming[cid]
        for h in homes:
            c = len(self.civs) + 1
            n_c, t_c = pnr.make_name(naming, cid, "civilization", self.inp.seed, cnum, c, self.registry)
            e_civ = self.append("civ_founded", region=h, participants=(("civilization", CIV, c), ("race", RACE, race_id)),
                                causes=(e_aw,))
            self.found_civilization(race_id, cnum, h, n_c, e_civ)
            if self.owner[h][0] == 0:
                self.set_owner(h, c, e_civ)
            st = self.site(h, race_id, relaxed=True)
            if st is None:
                fatal("history: peoples: no settlement site in region %d" % h)
            x, y, how = st
            s = len(self.settlements) + 1
            n_s, t_s = pnr.make_name(naming, cid, "settlement", self.inp.seed, cnum, s, self.registry)
            pop = self.t["founding_population"]
            e_set = self.append("settlement_founded", region=h, settlement=s,
                                participants=(("settlement", SETTLEMENT, s), ("founder", CIV, c)), values=(pop,),
                                causes=(e_civ,))
            self.found_settlement(x, y, h, c, self.founding_kind(race_id, h, pop, x, y), pop, n_s, e_set)
            self.civ(c)["capital"] = s
            self.site_how[s] = how
            self.name_attempts.append((c, t_c, s, t_s, how))
        return e_aw

    def flare_start(self, eng_run, year, length, trigger):
        self.year, self.years, self.phase = year, 1, eng_run.phase_id
        self.begin()
        super().flare_start(eng_run, year, length, trigger)

    # --- 9.1 growth (one year) and 8.3 recovery
    def grow_once(self, recover=False):
        sh = self.shares()
        for s in self.settlements:
            if not self.standing(s) or not self.visible(s):
                continue
            s["population"] = self.grown(s, sh[s["id"]])
            if recover:
                self.recover(s)

    def grown(self, s, cs):
        """9.1: one year of growth or decline of s with share cs (r6: the target T_s = min(C_s, kind_cap))."""
        p = s["population"]
        g = self.pd.race[self.race_of(s)]["growth"]
        cap = self.kind_cap(s)
        if FLAGS["cap"] and p > cap:
            # 14 item 3 (r6): a settlement above its kind cap at the start of a year loses at least 1 in it
            p2 = p - max(1, (p - min(cs, cap)) * self.t["decline_permille"] // 1000)
            if not p2 < p:
                self.checks.append("14 item 3 (r6): settlement %d above its cap %d kept %d in %d" %
                                   (s["id"], cap, p, self.year))
        if FLAGS["cap"]:
            ts = min(cs, cap)
            if p < ts:
                d = (p * g * (cs - p) + 500 * cs) // (1000 * cs)
                if g > 0:
                    d = max(1, d)
                return min(p + d, cap)
            if p > ts:
                return p - max(1, (p - ts) * self.t["decline_permille"] // 1000)
            return p
        if p < cs:
            d = (p * g * (cs - p) + 500 * cs) // (1000 * cs)
            if g > 0:
                d = max(1, d)
            return min(p + d, max(p, cap))
        if p > cs:
            return p - max(1, (p - cs) * self.t["decline_permille"] // 1000)
        return p

    def recover(self, s):
        if s["status"] == DECLINING and s["population"] * 1000 >= self.t["recover_permille"] * self.before.get(s["id"], 0):
            s["status"] = THRIVING

    def t1(self):
        self.begin()
        # 9.1 first paragraph: civilizations with no standing settlement end
        for cid in range(1, len(self.civs) + 1):
            c = self.civ(cid)
            if c["ended_year"] is not None or self.standing_of(cid):
                continue
            lost = [s for s in self.settlements if self.ruined_from.get(s["id"]) == cid]
            last = max(lost, key=lambda s: (s["ruined_year"], s["id"]))
            e = self.append("civ_ended", region=last["region"], settlement=last["id"],
                            participants=(("civilization", CIV, cid),), causes=(last["ruin_event"],))
            self.end_civ(cid, e)
        sh = self.shares()
        for s in self.settlements:
            if not self.standing(s) or not self.visible(s):
                continue
            s["population"] = self.grown(s, sh[s["id"]])
            self.recover(s)
        self.abandon_b()
        self.kind_changes()
        self.check_floor()

    def q1(self):
        self.begin()
        # Lead's ruling 4 (invariant 9 governs 9.2): 8.3's recovery is applied each year inside Q1,
        # after each settlement's growth, exactly as in T1.
        for _ in range(self.years):
            self.grow_once(recover=QUIET_RECOVER_EACH_YEAR)
        if not QUIET_RECOVER_EACH_YEAR:
            for s in self.settlements:
                if self.standing(s) and self.visible(s):
                    self.recover(s)
        self.abandon_b()
        self.kind_changes()
        self.check_floor()

    def check_floor(self):
        for s in self.settlements:
            if self.standing(s) and s["population"] < self.t["abandon_min"] and self.visible(s):
                if self.quiet_used is None:
                    self.checks.append("14 item 3: settlement %d below abandon_min after T1 in %d" % (s["id"], self.year))

    def abandon_b(self):
        for s in self.settlements:
            if not self.standing(s) or not self.visible(s) or s["population"] >= self.t["abandon_min"]:
                continue
            cause = self.last_red.get(s["id"], s["founding_event"])
            if not self.budget_for_abandon(s):
                continue
            self.abandon(s["id"], cause)

    def budget_for_abandon(self, s):
        if self.quiet_used is None:
            return True
        c = s["owner_civ"]
        others = [t for t in self.standing_of(c) if t["id"] != s["id"]]
        need = 1 + (1 if (self.civ(c)["capital"] == s["id"] and others) else 0) + (0 if others else 1)
        return self.quiet_events_left(c) >= need

    def kind_changes(self):
        for s in self.settlements:
            if not self.standing(s) or not self.visible(s) or s["kind"] == FORTRESS:
                continue
            k = s["kind"]
            p = s["population"]
            k2 = self.ladder(self.race_of(s), p)
            c = s["owner_civ"]
            if k2 > k:
                if self.quiet_used is not None and self.quiet_events_left(c) <= 0:
                    continue
                s["kind"] = k2
                self.append("became_" + KIND_KEY[k2], region=s["region"], settlement=s["id"],
                            participants=(("settlement", SETTLEMENT, s["id"]), ("civilization", CIV, c)),
                            values=(p,), causes=(s["founding_event"],))
            elif k2 < k and p * 1000 < self.kmin(k) * self.t["kind_shrink_permille"]:
                if self.quiet_used is not None and self.quiet_events_left(c) <= 0:
                    continue
                s["kind"] = k2
                lr = self.last_red.get(s["id"])
                self.append("settlement_dwindled", region=s["region"], settlement=s["id"],
                            participants=(("settlement", SETTLEMENT, s["id"]), ("civilization", CIV, c)),
                            values=(p,), causes=((lr,) if lr else ()))

    # --- 8.5 abandonment and the ruin procedure
    def ruin(self, sid, e):
        """8.5 items 2-5 with e as the ruin event (r1); not item 6."""
        s = self.stl(sid)
        c = s["owner_civ"]
        race = self.civ(c)["race"]
        p = s["population"]
        others = [t for t in self.standing_of(c) if t["id"] != sid]
        if others:
            t = min(others, key=lambda t: ((t["x"] - s["x"]) ** 2 + (t["y"] - s["y"]) ** 2, t["id"]))
            room = max(0, 2 * self.geo.capacity(t["region"], race) - t["population"])
            t["population"] += min(p, room)
        s["population"] = 0
        s["status"] = RUINED
        s["ruined_year"] = self.year
        s["ruin_event"] = e
        s["owner_civ"] = 0
        self.ruined_from[sid] = c
        rid = s["region"]
        if self.owner[rid][0] == c and not any(t["region"] == rid for t in others):
            self.set_owner(rid, 0, e)
        if self.civ(c)["capital"] == sid and others:
            nc = min(others, key=lambda t: (-t["population"], t["id"]))
            self.append("capital_moved", region=nc["region"], settlement=nc["id"],
                        participants=(("civilization", CIV, c), ("settlement", SETTLEMENT, nc["id"])), causes=(e,))
            self.civ(c)["capital"] = nc["id"]

    def destination(self, s):
        """8.5 item 1 (r6): (t, settled) for s's survivors: t the nearest other standing settlement of its owner
        (squared cell distance, then id) or None, settled = min(P, max(0, 2 C(R_t, r) - P_t))."""
        c = s["owner_civ"]
        race = self.civ(c)["race"]
        others = [t for t in self.standing_of(c) if t["id"] != s["id"]]
        if not others:
            return None, 0
        t = min(others, key=lambda t: ((t["x"] - s["x"]) ** 2 + (t["y"] - s["y"]) ** 2, t["id"]))
        room = max(0, 2 * self.geo.capacity(t["region"], race) - t["population"])
        return t, min(s["population"], room)

    def check_destination(self, s, t, settled):
        """14 item 12 (r6), in the run: the destination recomputed by a second scan."""
        c = s["owner_civ"]
        best = None
        for u in self.settlements:
            if u["id"] == s["id"] or u["owner_civ"] != c or u["status"] not in (THRIVING, DECLINING):
                continue
            k = ((u["x"] - s["x"]) ** 2 + (u["y"] - s["y"]) ** 2, u["id"])
            if best is None or k < best[0]:
                best = (k, u)
        if best is None:
            ok = t is None and settled == 0
        else:
            u = best[1]
            room = 2 * self.geo.capacity(u["region"], self.civ(c)["race"]) - u["population"]
            ok = t is u and settled == max(0, min(s["population"], room))
        if not ok:
            self.checks.append("14 item 12 (r6): destination of settlement %d in %d" % (s["id"], self.year))

    def abandon(self, sid, cause):
        s = self.stl(sid)
        c = s["owner_civ"]
        causes = (cause,)
        if FLAGS["causes"] and s["status"] == DECLINING and sid in self.decline_start:
            # r6 8.5 causes: the blow (or last reduction / founding) and the decline start, ascending, no repeats
            causes = tuple(sorted({cause, self.decline_start[sid]}))
        t, settled = self.destination(s) if FLAGS["dest"] else (None, 0)
        p = s["population"]
        if FLAGS["dest"]:
            self.check_destination(s, t, settled)
        if FLAGS["causes"] and s["status"] == DECLINING and sid in self.decline_start and \
                self.decline_start[sid] not in causes:
            self.checks.append("14 item 12 (r6): abandonment of %d without its decline start" % sid)
        if settled >= 1:
            e = self.append("settlement_abandoned_to", region=s["region"], settlement=sid,
                            participants=(("settlement", SETTLEMENT, sid), ("civilization", CIV, c),
                                          ("destination", SETTLEMENT, t["id"])),
                            values=(settled, p - settled), causes=causes)
        else:
            e = self.append("settlement_abandoned", region=s["region"], settlement=sid,
                            participants=(("settlement", SETTLEMENT, sid), ("civilization", CIV, c)),
                            values=(p,), causes=causes)
        self.ruin(sid, e)
        if not self.standing_of(c):
            e2 = self.append("civ_ended", region=s["region"], settlement=sid,
                             participants=(("civilization", CIV, c),), causes=(e,))
            self.end_civ(c, e2)
        return e

    def end_civ(self, c, e):
        civ = self.civ(c)
        civ["ended_year"] = self.year
        civ["ending_event"] = e
        for rid in self.held(c):
            self.set_owner(rid, 0, e)

    # --- 9.3 foundings
    def foundings(self, quiet):
        self.begin()
        t = self.t
        sh = self.shares()
        ncivs = len(self.civs)
        for cid in range(1, ncivs + 1):
            c = self.civ(cid)
            if c["ended_year"] is not None or not self.visible(c):
                continue
            race = c["race"]
            rd = self.pd.race[race]
            mine = [s for s in self.standing_of(cid, include_new=False)]
            press = {}
            for s in mine:
                p = s["population"]
                press[s["id"]] = max(p * 1000 // sh[s["id"]], p * 1000 // self.kind_cap(s))
            ready = [s for s in mine if press[s["id"]] >= t["found_pressure_permille"]]
            if not ready:
                continue
            src = max(ready, key=lambda s: (c["capital"] != s["id"], press[s["id"]], s["population"], -s["id"]))
            inside = None
            r_src = src["region"]
            if self.owner[r_src][0] == cid:
                in_r = sum(1 for s in self.settlements if self.standing(s) and s["region"] == r_src)
                if in_r < t["region_slots_max"]:
                    st = self.site(r_src, race, relaxed=False)
                    if st is not None:
                        inside = (r_src, st)
            outward = None
            held = self.held(cid)
            cap_region = self.stl(c["capital"])["region"]
            hops = self.geo.hops(cap_region)
            reach = t["reach_base"] + rd["expansion"] * t["reach_per_expansion"] // 1000
            smin = max(t["expand_min_score"], self.geo.sigma(c["home_region"], race) * t["expand_relative_permille"] // 1000)
            best_k = {}
            for r0 in held:
                for r1, k in self.geo.nb[r0]:
                    if self.owner[r1][0] != 0 or self.geo.islet(r1):
                        continue
                    if r1 not in hops or hops[r1] > reach:
                        continue
                    if self.geo.sigma(r1, race) < smin or self.geo.capacity(r1, race) < t["expand_min_capacity"]:
                        continue
                    if r1 not in best_k or k < best_k[r1]:
                        best_k[r1] = k
            if best_k:
                r1 = min(best_k, key=lambda r: (-(self.geo.sigma(r, race) - t["cost_penalty"] * best_k[r]), r))
                st = self.site(r1, race, relaxed=False)
                if st is not None:
                    outward = (r1, st)
            if inside is None and outward is None:
                self.hunger[cid] = self.hunger.get(cid, 0) + self.years
                self.hunger_max = max(self.hunger_max, self.hunger[cid])
                continue
            self.hunger[cid] = 0
            p = t["found_rate_permille"] * rd["expansion"] // 1000 * self.years
            if quiet:
                p = p * t["quiet_found_permille"] // 1000
            if not chance_pm(self.draw("hist.peoples.found", cid, 0, 0), min(1000, p)):
                continue
            if quiet and self.quiet_events_left(cid) == 0:
                continue
            n = max(t["colonists_min"], src["population"] * t["colonist_permille"] // 1000)
            if src["population"] - n < 2 * t["colonists_min"]:
                continue
            if inside is not None and outward is not None:
                target = outward if chance_pm(self.draw("hist.peoples.outward", cid, 0, 0), rd["expansion"]) else inside
            else:
                target = inside if inside is not None else outward
            rid, (x, y, how) = target
            n = min(n, self.geo.capacity(rid, race))
            src["population"] -= n
            s = len(self.settlements) + 1
            cnum = c["culture"]
            cul = self.culture[cnum][1]
            n_s, _t = pnr.make_name(self.dat.naming[cul], cul, "settlement", self.inp.seed, cnum, s, self.registry)
            was_free = self.owner[rid][0] == 0
            e = self.append("colony_founded", region=rid, settlement=s,
                            participants=(("settlement", SETTLEMENT, s), ("founder", CIV, cid),
                                          ("origin", SETTLEMENT, src["id"])), values=(n,),
                            salience=(t["colony_new_region_salience"] if was_free else None))
            self.found_settlement(x, y, rid, cid, self.founding_kind(race, rid, n, x, y), n, n_s, e)
            self.site_how[s] = how
            if was_free:
                self.set_owner(rid, cid, e)

    # --- 10 pressures
    def strike(self, s, kind, toll, causes, salience=None):
        p = s["population"]
        dead = p * toll // 1000
        was_declining = s["status"] == DECLINING
        if s["status"] == THRIVING:
            self.before[s["id"]] = p
        s["population"] = p - dead
        c = s["owner_civ"]
        e = self.append(kind, region=s["region"], settlement=s["id"],
                        participants=(("settlement", SETTLEMENT, s["id"]), ("civilization", CIV, c)),
                        values=(dead, s["population"]), causes=causes, salience=salience)
        if not was_declining:
            self.decline_start[s["id"]] = e      # r6 8.5: the blow that found it thriving
        s["status"] = DECLINING
        self.last_red[s["id"]] = e
        # 8.5 (a)
        t = self.t
        p2 = s["population"]
        if p2 < t["abandon_min"]:
            self.abandon(s["id"], e)
        elif was_declining and p2 < t["abandon_frail_max"] and self.chance_abandon_allowed(s):
            if self.abandon_draw(s):
                self.abandon(s["id"], e)
        return e

    def chance_abandon_allowed(self, s):
        """r6 8.5 (a): the chance is not drawn for a civilization's last standing settlement (the settlements
        created earlier in the step count)."""
        if not FLAGS["last"]:
            return True
        return any(t["id"] != s["id"] for t in self.standing_of(s["owner_civ"]))

    def abandon_draw(self, s):
        return chance_pm(self.draw("hist.peoples.abandon", s["id"], 0, 0), self.t["abandon_permille"][KIND_KEY[s["kind"]]])

    def t4(self):
        self.begin()
        t = self.t
        I = self.intensity
        sh = self.shares()
        struck = set()
        nset = len(self.settlements)
        tr_f = t["famine_toll_max"] - t["famine_toll_min"] + 1
        tr_p = t["plague_toll_max"] - t["plague_toll_min"] + 1
        for sid in range(1, nset + 1):
            s = self.stl(sid)
            if not self.standing(s) or not self.visible(s):
                continue
            k = s["population"] * 1000 // sh[sid]
            if k >= t["famine_crowding_permille"] and \
                    chance_ppm(self.draw("hist.peoples.famine", sid, 0, 0), t["famine_ppm"] * I // 1000):
                toll = t["famine_toll_min"] + pnr.pick(self.draw("hist.peoples.famine_toll", sid, 0, 0), tr_f)
                self.strike(s, "famine", toll, ())
                struck.add(sid)
            if not self.standing(s):
                continue
            if chance_ppm(self.draw("hist.peoples.plague", sid, 0, 0), t["plague_ppm"][KIND_KEY[s["kind"]]] * I // 1000):
                owner = s["owner_civ"]          # READING (SPEC_GAPS 10.2)
                small = s["kind"] in (HAMLET, VILLAGE)
                toll = t["plague_toll_min"] + pnr.pick(self.draw("hist.peoples.plague_toll", sid, 0, 0), tr_p)
                if FLAGS["plague_sal"]:
                    planned = self.plague_plan(s, toll, struck, nset, tr_p)
                    sal = None if planned >= t["plague_major_dead"] else t["plague_small_salience"]
                else:
                    sal = t["plague_small_salience"] if small else None
                o = self.strike(s, "plague", toll, (), salience=sal)
                actual = self.events[o - 1]["values"][1]
                if SPREAD_OWNER == "after":
                    owner = s["owner_civ"]
                struck.add(sid)
                area = {s["region"]} | {r for r, _c in self.geo.nb[s["region"]]}
                for uid in range(1, nset + 1):
                    u = self.stl(uid)
                    if uid == sid or uid in struck or not self.standing(u) or not self.visible(u):
                        continue
                    if u["owner_civ"] != owner or u["region"] not in area:
                        continue
                    if chance_pm(self.draw("hist.peoples.plague_spread", sid, uid, 0), t["plague_spread_permille"]):
                        toll = (t["plague_toll_min"] + pnr.pick(self.draw("hist.peoples.plague_toll", uid, 1, 0), tr_p)) \
                            * t["plague_spread_toll_permille"] // 1000
                        e_u = self.strike(u, "plague_spread", toll, (o,))
                        actual += self.events[e_u - 1]["values"][1]
                        struck.add(uid)
                if FLAGS["plague_sal"]:
                    self.plague_plans.append((o, planned, actual))
        ncivs = len(self.civs)
        for cid in range(1, ncivs + 1):
            c = self.civ(cid)
            if c["ended_year"] is not None or not self.visible(c):
                continue
            rate = t["crisis_per_generation_permille"] * 1000 // self.pd.race[c["race"]]["generation"] * I // 1000
            if not chance_ppm(self.draw("hist.peoples.crisis", cid, 0, 0), rate):
                continue
            cap = self.stl(c["capital"])
            if FLAGS["crisis_sal"]:
                # r6 10.3: the split test first (its conditions and draw do not depend on e)
                plan = self.split_plan(cid)
                e = self.append("succession_crisis", region=cap["region"], settlement=cap["id"],
                                participants=(("civilization", CIV, cid),),
                                salience=(None if plan is not None else t["crisis_small_salience"]))
                split = plan is not None and self.do_split(cid, e, plan)
            else:
                e = self.append("succession_crisis", region=cap["region"], settlement=cap["id"],
                                participants=(("civilization", CIV, cid),))
                split = self.try_split(cid, e)
            if split:
                continue
            p = cap["population"]
            if cap["status"] == THRIVING:
                self.before[cap["id"]] = p
                self.decline_start[cap["id"]] = e     # r6 8.5: a crisis loss that found it thriving
            cap["population"] = p - p * t["crisis_loss_permille"] // 1000
            cap["status"] = DECLINING
            self.last_red[cap["id"]] = e

    def plague_plan(self, s, toll, struck, nset, tr_p):
        """r6 10.2: the outbreak planned before it is written; returns total = dead_0 + sum of dead_u over the
        planned spreads (none if s is emptied). No state changes."""
        t = self.t
        p = s["population"]
        dead0 = p * toll // 1000
        rest = p - dead0
        emptied = rest < t["abandon_min"]
        if not emptied and s["status"] == DECLINING and rest < t["abandon_frail_max"] and \
                self.chance_abandon_allowed(s):
            emptied = self.abandon_draw(s)
        total = dead0
        if emptied:
            return total
        owner = s["owner_civ"]
        sid = s["id"]
        area = {s["region"]} | {r for r, _c in self.geo.nb[s["region"]]}
        for uid in range(1, nset + 1):
            u = self.stl(uid)
            if uid == sid or uid in struck or not self.standing(u) or not self.visible(u):
                continue
            if u["owner_civ"] != owner or u["region"] not in area:
                continue
            if chance_pm(self.draw("hist.peoples.plague_spread", sid, uid, 0), t["plague_spread_permille"]):
                tu = (t["plague_toll_min"] + pnr.pick(self.draw("hist.peoples.plague_toll", uid, 1, 0), tr_p)) \
                    * t["plague_spread_toll_permille"] // 1000
                total += u["population"] * tu // 1000
        return total

    def try_split(self, cid, e):
        plan = self.split_plan(cid)
        return plan is not None and self.do_split(cid, e, plan)

    def split_plan(self, cid):
        """10.3 split test: (mine, held, part, leaving) when the crisis splits the realm, else None."""
        t = self.t
        c = self.civ(cid)
        mine = self.standing_of(cid, include_new=False)
        held = self.held(cid)
        if len(mine) < t["split_min_settlements"] or len(held) < t["split_min_regions"]:
            return None
        if not chance_pm(self.draw("hist.peoples.split", cid, 0, 0), t["split_permille"]):
            return None
        hs = set(held)
        INF = 1 << 62

        def dist(r0):
            d = {r0: 0}
            fifo = [r0]
            i = 0
            while i < len(fifo):
                x = fifo[i]
                i += 1
                for y, _k in self.geo.nb[x]:
                    if y in hs and y not in d:
                        d[y] = d[x] + 1
                        fifo.append(y)
            return d
        cap = self.stl(c["capital"])
        dcap = dist(cap["region"])
        popin = {r: sum(s["population"] for s in mine if s["region"] == r) for r in held}
        far = max(held, key=lambda r: (dcap.get(r, INF), popin[r], -r))
        dfar = dist(far)
        part = [r for r in held if dfar.get(r, INF) < dcap.get(r, INF)]
        leaving = [s for s in mine if s["region"] in part]
        if not leaving or len(leaving) == len(mine):
            return None
        return part, leaving

    def do_split(self, cid, e, plan):
        c = self.civ(cid)
        part, leaving = plan
        newcap = min(leaving, key=lambda s: (-s["population"], s["id"]))
        c2 = len(self.civs) + 1
        cnum = c["culture"]
        cul = self.culture[cnum][1]
        name, _t = pnr.make_name(self.dat.naming[cul], cul, "civilization", self.inp.seed, cnum, c2, self.registry)
        e2 = self.append("civ_seceded", region=newcap["region"], settlement=newcap["id"],
                         participants=(("civilization", CIV, c2), ("from", CIV, cid)), values=(len(leaving),),
                         causes=(e,))
        self.found_civilization(c["race"], cnum, newcap["region"], name, e2)
        self.civ(c2)["capital"] = newcap["id"]
        for s in leaving:
            s["owner_civ"] = c2
        for r in part:
            self.set_owner(r, c2, e2)
        return True

    # --- 8.6 Q3
    def q3(self):
        self.begin()
        for s in self.settlements:
            if s["status"] != RUINED or self.year - s["ruined_year"] < self.t["ruin_fade_years"]:
                continue
            if self.quiet_events_left(0) <= 0:
                break
            self.append("ruin_faded", region=s["region"], settlement=s["id"],
                        participants=(("settlement", SETTLEMENT, s["id"]),), causes=(s["ruin_event"],))
            s["status"] = FADED
            # Lead's ruling 3: fading clears ruined_year and ruin_event (P3-04 State).
            s["ruined_year"] = None
            s["ruin_event"] = 0

    # --- the engine hooks
    def tick(self, eng_run, year, phase_end):
        self.year, self.years, self.phase = year, 1, eng_run.phase_id
        self.t1()
        self.foundings(quiet=False)
        self.begin()          # T3
        self.t4()
        self.begin()          # T5
        self.begin()          # T6
        self.begin()          # T7
        self.bookkeeping(year, phase_end)
        self.check_state()

    def quiet_step(self, eng_run, start, length, phase_end, row_start):
        self.year, self.years, self.phase = start, length, eng_run.phase_id
        last = start + length - 1
        if start == row_start:
            self.open_era = {}
        self.quiet_used = {}
        self.charged = set()
        self.q1()
        self.begin()          # Q2
        self.q3()
        self.foundings(quiet=True)
        self.begin()          # Q5
        self.begin()          # Q6
        self.quiet_used = None
        self.quiet_eras(start, last)
        self.bookkeeping(start, phase_end, last)
        if phase_end:
            self.open_era = {}
        self.check_state()

    def quiet_eras(self, start, last):
        """P3-04 6.6 (Q6). r10 (FLAGS["quiet"]): an event charged to the civilization does not close its era,
        so it has one era per quiet phase row; before r10 a charged civilization's era was closed."""
        for civ in self.civs:
            if civ["ended_year"] is not None:
                continue
            if civ["id"] in self.charged and not FLAGS["quiet"]:
                self.open_era.pop(civ["id"], None)
                continue
            idx = self.open_era.get(civ["id"])
            if idx is not None:
                self.events[idx]["end_year"] = last
            else:
                eid = self.append("quiet_era", start, last, region=civ["home_region"],
                                  participants=(("civilization", CIV, civ["id"]),), values=(0, 0))
                idx = eid - 1
                self.open_era[civ["id"]] = idx
            ev = self.events[idx]
            ev["values"] = {1: ev["end_year"] - ev["year"] + 1, 2: self.generations(civ, ev["year"], ev["end_year"])}

    # in-run invariants (14 items 3, 4, 5)
    def check_state(self):
        for s in self.settlements:
            if self.standing(s):
                cap = self.geo.capacity(s["region"], self.race_of(s))
                if s["population"] > 2 * cap:
                    self.checks.append("14 item 3: settlement %d population %d above 2 C %d in %d" %
                                       (s["id"], s["population"], 2 * cap, self.year))
        for rid in self.geo.ids:
            o = self.owner[rid][0]
            if o != 0 and not any(s["owner_civ"] == o and s["region"] == rid and self.standing(s)
                                  for s in self.settlements):
                self.checks.append("14 item 4: region %d held by %d without a settlement in %d" % (rid, o, self.year))
        for c in self.civs:
            if c["ended_year"] is None:
                cap = self.stl(c["capital"])
                if cap["owner_civ"] != c["id"] or not self.standing(cap):
                    self.checks.append("14 item 5: civilization %d capital %d in %d" % (c["id"], cap["id"], self.year))


class Hooks(pe.StubHooks):
    pass


def history_stage(dat, inp, tension=None, extra_draw=False):
    """3.3 history_stage with peoples installed (P3-05 13; 0.15.0)."""
    msg = pe.entry_checks(dat.base, inp, dat.engine, dat.report, True) if False else None
    msg = entry_checks(dat, inp)
    if msg is not None:
        fatal(msg)
    msg = pe.chronology_problem(inp)
    if msg is not None:
        fatal(msg)
    cat = event_catalogue(dat.report)
    w = World(dat, inp, cat, extra_draw)
    starts = [a[2] for a in inp.ages]
    en = pe.Engine(starts, [a[3] for a in inp.ages], inp.params["span"], dat.engine, inp.params["detail"], tension,
                   Hooks(w))
    en.run()
    res = dict(
        event_kind=[(k["id"], k["key"], k["caused"], k["region_use"], k["settlement_use"], k["salience"], k["templ"])
                    for k in cat],
        event_kind_role=[(k["id"], r["seq"], r["role"], r["type"], r["min"], r["max"]) for k in cat
                         for r in k["roles"]],
        event_kind_value=[(k["id"], i + 1, v) for k in cat for i, v in enumerate(k["values"])],
        phase=[tuple(r) for r in en.phases],
        event=[(e["id"], e["year"], e["end_year"], e["kind"], e["phase"], e["region"], e["settlement"],
                e["salience"]) for e in w.events],
        event_participant=[(e["id"],) + p for e in w.events for p in e["participants"]],
        event_cause=[(e["id"], c) for e in w.events for c in e["causes"]],
        event_value=[(e["id"], s, v) for e in w.events for s, v in sorted(e["values"].items())],
        civilization=[(c["id"], c["name"], c["race"], c["culture"], c["home_region"], c["capital"],
                       c["founded_year"], c["founding_event"], c["ended_year"], c["ending_event"]) for c in w.civs],
        settlement=[(s["id"], s["name"], s["x"], s["y"], s["region"], s["founder_civ"], s["founded_year"],
                     s["founding_event"], s["owner_civ"], s["kind"], s["status"], s["population"], s["ruined_year"],
                     s["ruin_event"]) for s in w.settlements],
        settlement_census=sorted(w.census),
        region_control=sorted(tuple(r) for r in w.region_control),
        relation=[],
    )
    res["_world"] = w
    res["_engine"] = en
    res["_catalogue"] = cat
    return res


def entry_checks(dat, inp):
    """P3-04 3.4 (the report check against the 0.15.0 catalogue)."""
    saved = pe.COMPILED
    pe.COMPILED = catalogue_kinds()
    try:
        return pe.entry_checks(dat.base, inp, dat.engine, dat.report, True)
    finally:
        pe.COMPILED = saved


# ---------------------------------------------------------------------------
# Output: hashes, [history] (11.1 + 15.4), sentences
# ---------------------------------------------------------------------------

TABLES = pe.TABLES
COLUMNS = pe.COLUMNS


def table_hashes(res):
    return pe.table_hashes(res)


def report_lines(res, inp):
    """P3-04 11.1 [history] with the keys of 15.4 appended (p3_engine_reference.report_lines)."""
    return pe.report_lines(res, inp.params["span"], inp.races)


def stats_extra(res):
    """15.4: the three [stats] keys after the [history] ones."""
    alive = [c[0] for c in res["civilization"] if c[8] is None]
    per = {c: sum(1 for s in res["settlement"] if s[8] == c and s[10] in (1, 2)) for c in alive}
    kind_of = {e[0]: e[3] for e in res["event"]}
    return ["civ_settlements_max = %d" % (max(per.values()) if per else 0),
            "settlements_colonies = %d" % sum(1 for s in res["settlement"] if kind_of[s[7]] == KIND_ID["colony_founded"]),
            "civilizations_seceded = %d" % sum(1 for c in res["civilization"]
                                               if kind_of[c[7]] == KIND_ID["civ_seceded"])]


def sentences(res, inp):
    view = pe.view_from_rows(res, [(r["id"], r["name"]) for r in inp.regions], inp.races, inp.gods)
    return [pe.render(view, e[0]) for e in res["event"]]


def end_state(res, inp):
    """The end-state listing of 15.2."""
    race_plural = {r[0]: r[3] for r in inp.races}
    rname = {r["id"]: r["name"] for r in inp.regions}
    st = {s[0]: s for s in res["settlement"]}
    out = []
    for c in res["civilization"]:
        mine = [s for s in res["settlement"] if s[8] == c[0] and s[10] in (1, 2)]
        held = sum(1 for r in res["region_control"] if r[2] is None and r[3] == c[0])
        cap = st[c[5]]
        out.append("c%d %s (%s), home %s, founded %d, %s; capital %s (%s %d); %d settlements, %d regions, "
                   "population %d" % (c[0], c[1], race_plural[c[2]], rname[c[4]], c[6],
                                      "alive" if c[8] is None else "ended %d" % c[8], cap[1], KIND_KEY[cap[9]],
                                      cap[11], len(mine), held, sum(s[11] for s in mine)))
    by = {1: 0, 2: 0, 3: 0, 4: 0}
    for s in res["settlement"]:
        by[s[10]] += 1
    out.append("settlements standing %d ruined %d faded %d" % (by[1] + by[2], by[3], by[4]))
    return out


def score_lines(name, w, inp):
    o = []
    for rid in w.geo.ids:
        for r in inp.races:
            o.append("%s score rg%d ra%d %d %d" % (name, rid, r[0], w.geo.sigma(rid, r[0]), w.geo.capacity(rid, r[0])))
    return o


def homes_lines(name, w):
    return ["%s homes ra%d y%d n%d %s" % (name, race, year, n, ",".join(map(str, h)))
            for race, year, h, n in w.homes_log]


# ---------------------------------------------------------------------------
# Synthetic geography (16.2 item 17), built identically by the C++ tests
# (history_peoples_support.hpp, make_synthetic): a test-owned splitmix64
# sequence, not the project's generator.
# ---------------------------------------------------------------------------

M64 = (1 << 64) - 1


class Mix:
    def __init__(self, s):
        self.s = s & M64

    def next(self):
        self.s = (self.s + 0x9E3779B97F4A7C15) & M64
        z = self.s
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
        return z ^ (z >> 31)

    def inr(self, lo, hi):
        return lo + self.next() % (hi - lo + 1)


SYN_N = 192


def make_synthetic(seed):
    """A map of SYN_N x SYN_N cells: ocean in the first SYN_OCEAN columns,
    then a grid of gx x gy rectangular regions (2 to 60), some islets
    (lakes or small tiles), some with no walkable cell, borders between
    neighbouring tiles of every type (walkable, pass, barrier, river),
    rivers draining west to the ocean along the tile rows."""
    m = Mix(0x5EED0000 + seed)
    n = SYN_N
    ocean = 8
    gx = m.inr(1, 8)
    gy = m.inr(2, 8) if gx == 1 else m.inr(1, 8)
    if gx * gy > 60:
        gy = 60 // gx
    # column and row cuts
    xs = [ocean]
    for i in range(1, gx):
        xs.append(ocean + (n - ocean) * i // gx + m.inr(0, 3))
    xs.append(n)
    ys = [0]
    for j in range(1, gy):
        ys.append(n * j // gy + m.inr(0, 3))
    ys.append(n)
    region = [0] * (n * n)
    water = [0] * (n * n)
    hab = [0] * (n * n)
    river = [0] * (n * n)
    flow = [8] * (n * n)
    for y in range(n):
        for x in range(ocean):
            water[y * n + x] = 3
    regions = []
    biomes_rows = []
    rid = 0
    for j in range(gy):
        for i in range(gx):
            rid += 1
            x0, x1, y0, y1 = xs[i], xs[i + 1] - 1, ys[j], ys[j + 1] - 1
            mode = m.inr(0, 9)       # 0: lake-filled islet, 1: no walkable cell, else ordinary
            cells = 0
            lake = 0
            for y in range(y0, y1 + 1):
                for x in range(x0, x1 + 1):
                    c = y * n + x
                    region[c] = rid
                    cells += 1
                    is_lake = (mode == 0 and (x - x0) > 2) or m.next() % 97 == 0
                    if is_lake and not (x == x0 and y == y0):
                        water[c] = 2
                        lake += 1
                    hab[c] = m.inr(0, 120) + (60 if (x + y) % 7 == 0 else 0)
            dry = cells - lake
            walk = 0 if mode == 1 else m.inr(dry // 5, dry)
            # a river along the tile's middle row, flowing west, only in the first column of tiles
            if i == 0 and mode != 0:
                ry = (y0 + y1) // 2
                for x in range(x0, x1 + 1):
                    c = ry * n + x
                    if water[c] == 0:
                        water[c] = 1
                        river[c] = m.inr(3, 6)
                        flow[c] = 4
            coast = (y1 - y0 + 1) if i == 0 else 0
            reg = dict(id=rid, name="Synthetic %d" % rid, x_min=x0, y_min=y0, x_max=x1, y_max=y1, seed_x=x0 + 1,
                       seed_y=y0 + 1, cells=cells, lake_cells=lake, walk_cells=walk, habitability_mean=m.inr(10, 120),
                       coast_cells=coast, fertility_mean=m.inr(0, 255), forage_mean=m.inr(0, 255),
                       timber_mean=m.inr(0, 255), fresh_water_mean=m.inr(0, 255), ore_reach_cells=m.inr(0, 400))
            regions.append(reg)
            # biome counts: three land biomes from 3..12 except 8, and the lake count
            left = dry
            used = set()
            for _ in range(3):
                b = (3, 4, 5, 6, 7, 9, 10, 11, 12)[m.inr(0, 8)]
                if b in used:
                    continue
                used.add(b)
                cnt = m.inr(0, left)
                left -= cnt
                biomes_rows.append((rid, b, cnt))
            if left > 0:
                b = 4 if 4 not in used else 3
                if b in used:
                    b = 12
                    if b in used:
                        b = None
                if b is not None:
                    biomes_rows.append((rid, b, left))
                    used.add(b)
            if lake > 0:
                biomes_rows.append((rid, 2, lake))
    # merge duplicate (region, biome) rows and sort
    agg = {}
    for (r, b, c) in biomes_rows:
        agg[(r, b)] = agg.get((r, b), 0) + c
    biomes_rows = sorted((r, b, c) for (r, b), c in agg.items())
    borders = []
    for j in range(gy):
        for i in range(gx):
            a = j * gx + i + 1
            for (di, dj) in ((1, 0), (0, 1)):
                if i + di >= gx or j + dj >= gy:
                    continue
                b = (j + dj) * gx + (i + di) + 1
                kind = m.inr(0, 9)
                length = 16 * (ys[j + 1] - ys[j] if di else xs[i + 1] - xs[i])
                walk_pairs = m.inr(1, 50) if kind <= 5 else 0
                pass_id = m.inr(1, 9) if kind in (6, 7) else 0
                river_pairs = (length // 16 // 2 if kind in (3, 7) else (length // 32 - 1 if kind == 4 else 0))
                borders.append(dict(a=min(a, b), b=max(a, b), length_m=(0 if kind == 5 else length),
                                    walk_pairs=walk_pairs, river_pairs=max(0, river_pairs), pass_id=pass_id))
    borders.sort(key=lambda r: (r["a"], r["b"]))
    biomes = {1: 1, 2: 1, 3: 0, 4: 0, 5: 0, 6: 0, 7: 0, 8: 0, 9: 0, 10: 0, 11: 0, 12: 0}
    # r6 (generator 0.17.0): the layers elevation, ore and walk_area and the region column elev_max_dm,
    # from a second sequence so that every earlier value of the geography is unchanged: one draw per cell
    # in index order; ocean cells elevation -1 - v % 500, ore 0, walk_area 0; other cells elevation
    # 1 + v % 3000 (decimetres), ore (v >> 16) % 256, walk_area 0 when (v >> 32) % 3 == 0, else
    # 1 + (v >> 40) % 1000; elev_max_dm the largest elevation over the region's cells with water 0 (0 if
    # it has none).
    m2 = Mix(0x5EED1000 + seed)
    elev = [0] * (n * n)
    ore = [0] * (n * n)
    walk_area = [0] * (n * n)
    emax = {}
    for i in range(n * n):
        v = m2.next()
        if water[i] == 3:
            elev[i] = -1 - v % 500
            continue
        elev[i] = 1 + v % 3000
        ore[i] = (v >> 16) % 256
        walk_area[i] = 0 if (v >> 32) % 3 == 0 else 1 + (v >> 40) % 1000
        if water[i] == 0:
            emax[region[i]] = max(emax.get(region[i], 0), elev[i])
    for reg in regions:
        reg["elev_max_dm"] = emax.get(reg["id"], 0)
    lay = dict(region=region, water=water, habitability=hab, river=river, flow_dir=flow, elevation=elev, ore=ore,
               walk_area=walk_area)
    return dict(n=n, regions=regions, borders=borders, region_biomes=biomes_rows, biomes=biomes, layers=lay)


# Stress tuning (fixture data, not shipped): pressures strong enough that
# abandonments, capital moves, civilization endings, secessions and fading
# ruins happen on small maps (8.5 item 6 and 8.6 are otherwise rare, 15.3).
STRESS = dict(famine_ppm=60000, famine_crowding_permille=0,
              plague_ppm=dict(hamlet=20000, village=20000, town=20000, city=20000, fortress=20000),
              abandon_permille=dict(hamlet=1000, village=1000, town=500, city=200, fortress=200),
              abandon_frail_max=100000, ruin_fade_years=200, split_min_settlements=2, split_min_regions=2,
              split_permille=1000, crisis_per_generation_permille=600, found_rate_permille=1000)
STRESS_SEEDS = (0, 1, 2, 3, 4, 5, 6, 7, 12, 14)
SYN_RACES = ("all", "elf", "dwarf,faerie", "goblin,human,orc")
SYN_CIVS = ("auto", "1", "4")
SYN_INTENSITY = ("normal", "calm", "turbulent")


def synthetic_case(seed):
    """The settings of synthetic case <seed> (16.2 item 17's grid, cycled)."""
    span = (2000, 6000)[seed % 2]
    races = SYN_RACES[seed % 4]
    civs = SYN_CIVS[(seed // 2) % 3]
    intensity = SYN_INTENSITY[(seed // 3) % 3]
    return span, races, civs, intensity


def synthetic_inputs(seed, dat=None):
    dat = dat or data()
    g = make_synthetic(seed)
    span, races, civs, intensity = synthetic_case(seed)
    p = pchr.params(span=span, races=pchr.races_of(races, dat.chron), civs=civs_param(civs), intensity=intensity)
    chron = pchr.chronology(dat.chron, p, seed, [r["name"] for r in g["regions"]])
    inp = Inputs(seed, g["n"], p, g["regions"], chron["race"], chron["race_biome"], chron["culture"], chron["god"],
                 chron["age"], g["borders"], g["region_biomes"], g["biomes"], lambda: g["layers"])
    return inp, g


# ---------------------------------------------------------------------------
# Invariants of 14 on rows (used on reference rows and on stored worlds)
# ---------------------------------------------------------------------------

def check_invariants(c, res, inp, w, label):
    t = w.t
    ev = {e[0]: e for e in res["event"]}
    kind = {e[0]: e[3] for e in res["event"]}
    causes = {}
    for (e, cc) in res["event_cause"]:
        causes.setdefault(e, []).append(cc)
    parts = {}
    for p in res["event_participant"]:
        parts.setdefault(p[0], []).append(p)
    lay = inp.peoples_layers()
    n = inp.map_size
    civs = {x[0]: x for x in res["civilization"]}
    sts = {x[0]: x for x in res["settlement"]}
    ph = {r[0]: r for r in res["phase"]}
    # 1
    setting = inp.params["civs"]
    for a in inp.ages:
        race = a[1]
        founded = [x for x in res["civilization"] if x[2] == race and kind[x[7]] == KIND_ID["civ_founded"]]
        c.true("%s inv 1 race %d has a civilization" % (label, race), len(founded) >= 1)
        c.true("%s inv 1 race %d at most its setting" % (label, race),
             len(founded) <= (2 if setting is None else setting))
        for x in founded:
            c.true("%s inv 1 founded at the awakening" % label, x[6] == a[2] and kind[causes[x[7]][0]] == 1)
    # 2
    cells = {}
    faded_at = {}
    for e in res["event"]:
        if e[3] == KIND_ID["ruin_faded"]:
            faded_at[e[6]] = e[0]
    for s in res["settlement"]:
        c.true("%s inv 2 s%d founding kind" % (label, s[0]), kind[s[7]] in (KIND_ID["settlement_founded"],
                                                                          KIND_ID["colony_founded"]))
        i = s[3] * n + s[2]
        c.true("%s inv 2 s%d cell in region, dry" % (label, s[0]), lay["region"][i] == s[4] and lay["water"][i] == 0)
        # READING (SPEC_GAPS 14 item 2 / 6.2): a faded settlement does not block (6.2), so a later
        # settlement may stand on its cell; any other shared cell is a defect.
        prev = cells.get((s[2], s[3]))
        c.true("%s inv 2 s%d cell unique" % (label, s[0]), prev is None or (prev in faded_at and
                                                                            faded_at[prev] < s[7]))
        cells[(s[2], s[3])] = s[0]
    sp2 = t["site_spacing"] ** 2
    ss = res["settlement"]
    ruined_by = {}   # settlement -> its settlement_abandoned event (a faded row no longer names it)
    for e in res["event"]:
        if e[3] in (KIND_ID["settlement_abandoned"], KIND_ID["settlement_abandoned_to"]):
            ruined_by[e[6]] = e[0]
    for i in range(len(ss)):
        for j in range(i + 1, len(ss)):
            a, b = ss[i], ss[j]
            # a founded first; it stood at b's founding unless ruined before b's founding event
            if a[0] in ruined_by and ruined_by[a[0]] < b[7]:
                continue
            if w.site_how.get(b[0]) == "relaxed":
                continue
            d2 = (a[2] - b[2]) ** 2 + (a[3] - b[3]) ** 2
            c.true("%s inv 2 spacing s%d s%d" % (label, a[0], b[0]), d2 >= sp2)
    # 3 (end state; in-run checks in w.checks)
    for s in ss:
        if s[10] in (1, 2):
            race = civs[s[8]][2]
            c.true("%s inv 3 s%d P <= 2 C" % (label, s[0]), s[11] <= 2 * w.geo.capacity(s[4], race))
    c.eq("%s in-run invariants 3-5" % label, w.checks, [])
    # 4
    for r in res["region_control"]:
        if r[2] is None:
            c.true("%s inv 4 region %d held with a settlement" % (label, r[0]),
                 any(s[8] == r[3] and s[4] == r[0] and s[10] in (1, 2) for s in ss))
    # 5
    for x in res["civilization"]:
        mine = [s for s in ss if s[8] == x[0] and s[10] in (1, 2)]
        if x[8] is None:
            c.true("%s inv 5 c%d capital standing and its own" % (label, x[0]),
                 x[5] in sts and sts[x[5]][8] == x[0] and sts[x[5]][10] in (1, 2))
        else:
            c.true("%s inv 5 ended c%d empty" % (label, x[0]), not mine and not any(
                r[2] is None and r[3] == x[0] for r in res["region_control"]))
    # 6: kinds through the census (r6, the P3-06 validation's D1: a kind built by a race that has owned it)
    cen = {}
    for r in res["settlement_census"]:
        cen.setdefault(r[0], []).append(r)
    for s in ss:
        ks = [r[3] for r in cen[s[0]]]
        if ks[0] == FORTRESS:
            c.true("%s inv 6 fortress s%d stays" % (label, s[0]), all(k == FORTRESS for k in ks))
    check_kinds_built(c, res, w, label)
    for e in res["event"]:
        key = COMPILED[e[3] - 1]["key"]
        if key.startswith("became_") or key == "settlement_dwindled":
            s = e[6]
            before = [r for r in cen[s] if r[1] < e[1]]
            at = [r for r in cen[s] if r[1] == e[1]]
            if before and at:
                up = at[-1][3] > before[-1][3]
                c.true("%s inv 6 e%d %s direction" % (label, e[0], key), up == key.startswith("became_"))
    # 7 (r6: kind 31 is a ruin event too)
    ruin_kinds = (KIND_ID["settlement_abandoned"], KIND_ID["settlement_abandoned_to"])
    for s in ss:
        if s[10] == RUINED:
            c.true("%s inv 7 s%d ruin event" % (label, s[0]), kind[s[13]] in ruin_kinds)
        if s[10] == FADED:
            fades = [e for e in res["event"] if e[3] == KIND_ID["ruin_faded"] and e[6] == s[0]]
            ok = len(fades) == 1 and len(causes.get(fades[0][0], [])) == 1
            if ok:
                ruin = ev[causes[fades[0][0]][0]]
                ok = (ruin[3] in ruin_kinds and ruin[6] == s[0] and
                      fades[0][1] >= ruin[1] + t["ruin_fade_years"] and s[12] is None and s[13] == 0)
            c.true("%s inv 7 s%d faded (cause its ruin event, cleared row)" % (label, s[0]), ok)
    # 8
    blows = ("famine", "plague", "plague_spread", "succession_crisis")
    ab = ("settlement_abandoned", "settlement_abandoned_to")
    want_cause = {
        "became_village": None, "became_town": None, "became_city": None, "plague_spread": ("plague",),
        "civ_seceded": ("succession_crisis",), "settlement_abandoned": blows + ("settlement_founded", "colony_founded"),
        "settlement_abandoned_to": blows + ("settlement_founded", "colony_founded"),
        "capital_moved": ab, "civ_ended": ab, "ruin_faded": ab, "settlement_dwindled": blows,
    }
    keys = {k["id"]: k["key"] for k in COMPILED}
    for e in res["event"]:
        key = keys[e[3]]
        cs = causes.get(e[0], [])
        if key in want_cause:
            if key.startswith("became_"):
                c.true("%s inv 8 e%d cause" % (label, e[0]), cs == [sts[e[6]][7]])
            else:
                c.true("%s inv 8 e%d cause kinds" % (label, e[0]),
                     all(keys[kind[x]] in want_cause[key] for x in cs) and (cs or key == "settlement_dwindled"))
        if e[3] >= 6 and ph[e[4]][2] == QUIET:
            c.true("%s inv 8 e%d %s not in a quiet step" % (label, e[0], key), e[3] in QUIET_KINDS)
    check_r6_invariants(c, res, inp, w, label)


def races_of_owners(res):
    """{settlement: [(year, race)] of every civilization that has owned it up to a year}: the founder at
    its founding and every census row's owner_civ (0 for a ruin: the owner before it stays)."""
    civ_race = {x[0]: x[2] for x in res["civilization"]}
    out = {}
    for s in res["settlement"]:
        out[s[0]] = [(s[6], civ_race[s[5]])]
    for r in res["settlement_census"]:
        if r[2]:
            out[r[0]].append((r[1], civ_race[r[2]]))
    return out


def check_kinds_built(c, res, w, label):
    """14 item 6 as r6 reads it (the P3-06 validation's D1): every census row's kind is a kind built by a
    race that has owned the settlement by that year (the founder's race or the race of any later owner;
    for a ruined or faded row, the owners before the ruin)."""
    owners = races_of_owners(res)
    for r in res["settlement_census"]:
        races = {race for (y, race) in owners[r[0]] if y <= r[1]}
        built = set()
        for race in races:
            built |= set(w.pd.race[race]["kinds"])
        c.true("%s inv 6 s%d kind %d in %d built by an owner's race %s" % (label, r[0], r[3], r[1], sorted(races)),
               r[3] in built)


def check_r6_invariants(c, res, inp, w, label):
    """P3-05 14 r6 items: 6 (a dwarven settlement off the walkable ground is a fortress), 11, 12 (rows; the
    rest is checked in the run, w.checks), 13. Only with the r6 flags on."""
    t = w.t
    kind = {e[0]: e[3] for e in res["event"]}
    ev = {e[0]: e for e in res["event"]}
    vals = {}
    for v in res["event_value"]:
        vals.setdefault(v[0], {})[v[1]] = v[2]
    parts = {}
    for p in res["event_participant"]:
        parts.setdefault(p[0], {})[p[2]] = p[4]
    causes = {}
    for (e, cc) in res["event_cause"]:
        causes.setdefault(e, []).append(cc)
    civs = {x[0]: x for x in res["civilization"]}
    cen = {}
    for r in res["settlement_census"]:
        cen.setdefault(r[0], []).append(r)
    base = {k[0]: k[5] for k in res["event_kind"]}
    k16, k31 = KIND_ID["settlement_abandoned"], KIND_ID["settlement_abandoned_to"]
    # 6 (r6): a settlement founded by a race with a cell term (and fortress among its kinds) on a cell with
    # walk_area 0 is a fortress
    if FLAGS["fortress_cell"] and FLAGS["cell"]:
        lay = inp.peoples_layers()
        n = inp.map_size
        for s in res["settlement"]:
            race = civs[s[5]][2]
            if w.geo.has_cell_term(race) and FORTRESS in w.pd.race[race]["kinds"] and \
                    lay["walk_area"][s[3] * n + s[2]] == 0:
                c.true("%s inv 6 (r6) s%d off the walkable ground is a fortress" % (label, s[0]),
                       all(r[3] == FORTRESS for r in cen[s[0]]))
    # 11: a chance abandonment (population >= abandon_min) never ends its civilization
    if FLAGS["last"]:
        ended_by = {}
        for e in res["event"]:
            if e[3] == KIND_ID["civ_ended"]:
                for x in causes.get(e[0], []):
                    ended_by[x] = e[0]
        for e in res["event"]:
            if e[3] in (k16, k31):
                pop = vals[e[0]][1] if e[3] == k16 else vals[e[0]][1] + vals[e[0]][2]
                if pop >= t["abandon_min"]:
                    c.true("%s inv 11 e%d a chance abandonment ends no civilization" % (label, e[0]),
                           e[0] not in ended_by)
    # 12 (rows): settled >= 1, lost >= 0, the destination another settlement owned by the civilization
    if FLAGS["dest"]:
        for e in res["event"]:
            if e[3] != k31:
                continue
            d = parts[e[0]].get(3)
            civ = parts[e[0]].get(2)
            ok = vals[e[0]][1] >= 1 and vals[e[0]][2] >= 0 and d is not None and d != e[6]
            if ok:
                rows = [r for r in cen[d] if r[1] <= e[1]]
                prev = [r for r in rows if r[1] < e[1]]
                st = [r for r in (rows[-1:] + prev[-1:])]
                ok = any(r[2] == civ and r[4] in (THRIVING, DECLINING) for r in st) or \
                    (not prev and res["settlement"][d - 1][5] == civ)
            c.true("%s inv 12 e%d settlement_abandoned_to destination" % (label, e[0]), ok)
    # 13: saliences
    if FLAGS["plague_sal"]:
        spread = {}
        for e in res["event"]:
            if e[3] == KIND_ID["plague_spread"]:
                for x in causes.get(e[0], []):
                    spread[x] = spread.get(x, 0) + vals[e[0]][1]
        for e in res["event"]:
            if e[3] == KIND_ID["plague"]:
                total = vals[e[0]][1] + spread.get(e[0], 0)
                want = base[KIND_ID["plague"]] if total >= t["plague_major_dead"] else t["plague_small_salience"]
                c.true("%s inv 13 e%d plague salience %d (total dead %d)" % (label, e[0], e[7], total), e[7] == want)
    if FLAGS["crisis_sal"]:
        split = {x for e in res["event"] if e[3] == KIND_ID["civ_seceded"] for x in causes.get(e[0], [])}
        for e in res["event"]:
            if e[3] == KIND_ID["succession_crisis"]:
                want = base[KIND_ID["succession_crisis"]] if e[0] in split else t["crisis_small_salience"]
                c.true("%s inv 13 e%d crisis salience" % (label, e[0]), e[7] == want)


# ---------------------------------------------------------------------------
# Golden cases (16.3)
# ---------------------------------------------------------------------------

WORLD_CASES = tuple(pe.WORLD_CASES) + (("hist-peoples-42-1024", 42, 1024, 6000, "all", "normal"),)
INPUT_OF = {"hist-peoples-42-1024": "example-42-1024"}
SYN_SEEDS = tuple(range(16))


def world_case(name, seed, size, span, races, detail, inputs_dir, dat=None):
    dat = dat or data()
    path = os.path.join(inputs_dir, INPUT_OF.get(name, name) + ".db")
    conn = pchr.open_world(path)
    h, _rows = pchr.world_table_hash(conn, "region")
    if name in pe.P1_06_CASE:
        want = pchr.p1_06_region_hash(pe.P1_06_CASE[name])
        if h != want:
            fatal("%s: table region %s differs from p1-06-regions.txt %s" % (path, h, want))
    meta = dict(conn.execute("SELECT key, value FROM meta").fetchall())
    if int(meta["seed"]) != seed or int(meta["param.map_size"]) != size:
        fatal("%s: seed or map size differs from the case" % path)
    p = pchr.params(span=span, races=pchr.races_of(races, dat.chron), detail=detail)
    inp, chron = inputs_for(dat, conn, p, seed, size)
    inp.peoples_layers()
    conn.close()
    res = history_stage(dat, inp)
    res["_inputs"] = inp
    res["_region_hash"] = h
    return res


def case_lines(name, res, all_sentences):
    inp = res["_inputs"]
    w = res["_world"]
    o = []
    if "_region_hash" in res:
        o.append("%s input_region %s" % (name, res["_region_hash"]))
    o += score_lines(name, w, inp)
    o += homes_lines(name, w)
    h = table_hashes(res)
    for t in TABLES:
        o.append("%s table %s %s" % (name, t, h[t]))
    for line in report_lines(res, inp)[1:]:
        o.append("%s report %s" % (name, line))
    for line in stats_extra(res):
        o.append("%s stats %s" % (name, line))
    sents = sentences(res, inp)
    ids = list(range(len(sents)))
    if not all_sentences:
        ids = ids[:300] + [i for i in ids[-50:] if i >= 300]
    for i in ids:
        o.append("%s e%d %s" % (name, i + 1, sents[i]))
    return o


HEADER = """\
# P3-05 peoples golden vectors for generator 0.16.0 (schema 7), spec
# docs/specs/p3-05-peoples.md r4, generated by
# worldgen/tests/reference/p3_peoples_reference.py golden, an independent
# exact-integer reference written from the spec and the P3-04 engine seam
# alone (with core_reference.py, p3_names_reference.py, p3_data_reference.py,
# p3_chronology_reference.py and p3_engine_reference.py as libraries).
# Since 0.16.0 the shipped history stage is peoples and conflict (P3-06): the
# cases below are the PEOPLES-ONLY configuration (run_engine with
# PeoplesAwakening and PeoplesSubsystem, no conflict subsystem, no tension
# source) on the 0.16.0 catalogue of 30 kinds; only the event_kind tables and
# the [history] lines events_<kind> of kinds 20-30 (all 0) differ from the
# 0.15.0 file. The shipped worlds are in p3-06-conflict.txt.
# Data: the shipped game/data with the 0.16.0 race data and the report
# entries of kinds 1-30 as listed in P3-05 and P3-06 (p3_data_reference.py's
# listings while the disk holds older files). Do not edit by hand; never
# regenerate to make a C++ test pass.
#
# Format: ASCII, LF, tokens separated by one space; a sentence runs to the
# end of the line.
#   world <case> <seed> <map size> <span> <races> <detail>
#       a world case (16.3 item 21): the four P1 golden cases, the three
#       hist-* cases and hist-peoples-42-1024 (16.3 item 22, the sample of
#       15.2): the peoples-only history of the world generated with the
#       shipped data and these settings up to chronology, other settings
#       default (civs auto, intensity normal)
#   synthetic <case> <seed> <span> <races> <civs> <intensity>
#       a synthetic geography (16.2 item 17; make_synthetic(seed) in the
#       reference and in history_peoples_support.hpp), map 192, detail
#       normal, the chronology of the shipped data for the case's seed
#   stress <case> <seed> <span> <races> <civs> <intensity>
#       the synthetic geography of <seed> with the shipped data but
#       history.peoples edited past the loader to the reference's STRESS
#       values (famine_ppm 60000, famine_crowding_permille 0, plague_ppm
#       20000 for every kind, abandon_permille {1000, 1000, 500, 200, 200},
#       abandon_frail_max 100000, ruin_fade_years 200, split_min_settlements
#       2, split_min_regions 2, split_permille 1000,
#       crisis_per_generation_permille 600, found_rate_permille 1000)
#   <case> input_region <hex>        P0-04 8 hash of the input's table region
#   <case> score rg<R> ra<r> <sigma> <C>
#                                    4.2 and 5.1 for every region and race
#   <case> homes ra<r> y<year> n<n> <R1,R2,...>
#                                    per awakening: the race, year, the count
#                                    n of 7.2 before the land limit, and the
#                                    homes founded, in order
#   <case> table <table> <hex>       the thirteen history tables (P0-04 8)
#   <case> report <line>             [history] after its header (P3-04 11.1
#                                    with the keys of P3-05 15.4)
#   <case> stats <line>              the three [stats]-only keys of 15.4
#   <case> e<id> <sentence>          render_event: every event of the 512 and
#                                    synthetic cases, the first 300 and the
#                                    last 50 of the 1024 cases
"""


def golden_text(inputs):
    dat = data()
    o = [HEADER.rstrip("\n")]
    for name, seed, size, span, races, detail in WORLD_CASES:
        res = world_case(name, seed, size, span, races, detail, inputs, dat)
        o.append("world %s %d %d %d %s %s" % (name, seed, size, span, races, detail))
        o += case_lines(name, res, size <= 512)
    for seed in SYN_SEEDS:
        span, races, civs, intensity = synthetic_case(seed)
        inp, _g = synthetic_inputs(seed, dat)
        res = history_stage(dat, inp)
        res["_inputs"] = inp
        name = "syn-%d" % seed
        o.append("synthetic %s %d %d %s %s %s" % (name, seed, span, races, civs, intensity))
        o += case_lines(name, res, True)
    sdat = edited(dat, peoples=STRESS)
    for seed in STRESS_SEEDS:
        span, races, civs, intensity = synthetic_case(seed)
        inp, _g = synthetic_inputs(seed, sdat)
        res = history_stage(sdat, inp)
        res["_inputs"] = inp
        name = "stress-%d" % seed
        o.append("stress %s %d %d %s %s %s" % (name, seed, span, races, civs, intensity))
        o += case_lines(name, res, True)
    return "\n".join(o) + "\n"


# ---------------------------------------------------------------------------
# check: the worked example, hand-made units, invariants, golden
# ---------------------------------------------------------------------------

Checker = pchr.Checker

SAMPLE_FIRST_60 = """\
0 e1 awakening/100: Fielie awakened the Elves in West Wood III.
0 e2 civ_founded/80: The Elves founded Finen in West Wood III. [causes e1]
0 e3 settlement_founded/40: Finen founded Voeledell in West Wood III. [causes e2]
0 e4 civ_founded/80: The Elves founded The Realm of Ithieglie in Southeast Isle. [causes e1]
0 e5 settlement_founded/40: The Realm of Ithieglie founded Mofaesi in Southeast Isle. [causes e4]
150-699 e6 quiet_era/20: Finen passed 550 quiet years (3 generations), from 150 to 699.
150-874 e7 quiet_era/20: The Realm of Ithieglie passed 725 quiet years (4 generations), from 150 to 874.
700 e8 became_town/18: Voeledell grew into a town of 1520. [causes e3]
725-1249 e9 quiet_era/20: Finen passed 525 quiet years (4 generations), from 725 to 1249.
875 e10 colony_founded/15: The Realm of Ithieglie sent settlers from Mofaesi to found Saelo in Southeast Isle.
900-949 e11 quiet_era/20: The Realm of Ithieglie passed 50 quiet years (0 generations), from 900 to 949.
950 e12 colony_founded/15: The Realm of Ithieglie sent settlers from Mofaesi to found Raenme in Southeast Isle.
975-999 e13 quiet_era/20: The Realm of Ithieglie passed 25 quiet years (0 generations), from 975 to 999.
1000 e14 became_village/10: Saelo grew into a village of 269. [causes e10]
1025-1049 e15 quiet_era/20: The Realm of Ithieglie passed 25 quiet years (1 generation), from 1025 to 1049.
1050 e16 colony_founded/15: The Realm of Ithieglie sent settlers from Saelo to found Glalivedell in Southeast Isle.
1075 e17 became_village/10: Raenme grew into a village of 256. [causes e12]
1100-1249 e18 quiet_era/20: The Realm of Ithieglie passed 150 quiet years (1 generation), from 1100 to 1249.
1250 e19 became_village/10: Glalivedell grew into a village of 256. [causes e16]
1250 e20 colony_founded/15: Finen sent settlers from Voeledell to found Minno in West Wood III.
1275-1424 e21 quiet_era/20: Finen passed 150 quiet years (1 generation), from 1275 to 1424.
1275-2073 e22 quiet_era/20: The Realm of Ithieglie passed 799 quiet years (5 generations), from 1275 to 2073.
1425 e23 colony_founded/15: Finen sent settlers from Voeledell to found Iennmo in West Wood III.
1450 e24 colony_founded/15: Finen sent settlers from Minno to found Rienigla in West Wood III.
1475 e25 colony_founded/30: Finen sent settlers from Minno to found Glathenddell in Southwest Wood.
1500-1574 e26 quiet_era/20: Finen passed 75 quiet years (0 generations), from 1500 to 1574.
1575 e27 colony_founded/30: Finen sent settlers from Minno to found Rannve in West Wood II.
1600-1649 e28 quiet_era/20: Finen passed 50 quiet years (1 generation), from 1600 to 1649.
1650 e29 became_village/10: Rienigla grew into a village of 265. [causes e24]
1675 e30 became_village/10: Glathenddell grew into a village of 260. [causes e25]
1700-1774 e31 quiet_era/20: Finen passed 75 quiet years (0 generations), from 1700 to 1774.
1775 e32 became_village/10: Rannve grew into a village of 265. [causes e27]
1800-2073 e33 quiet_era/20: Finen passed 274 quiet years (1 generation), from 1800 to 2073.
2074 e34 awakening/100: Krok awakened the Goblins in Southwest Steppe.
2074 e35 civ_founded/80: The Goblins founded The Grork Horde in Southwest Steppe. [causes e34]
2074 e36 settlement_founded/40: The Grork Horde founded Zugnob Hole in Southwest Steppe. [causes e35]
2122 e37 famine/25: Famine struck Mofaesi; 66 died.
2160 e38 became_town/18: Zugnob Hole grew into a town of 1513. [causes e36]
2195 e39 famine/25: Famine struck Raenme; 40 died.
2200 e40 plague/25: Plague broke out in Rienigla; 81 died.
2200 e41 plague_spread/25: The plague spread to Voeledell; 158 died. [causes e40]
2224-2373 e42 quiet_era/20: Finen passed 150 quiet years (1 generation), from 2224 to 2373.
2224-3241 e43 quiet_era/20: The Realm of Ithieglie passed 1018 quiet years (7 generations), from 2224 to 3241.
2224-2248 e44 quiet_era/20: The Grork Horde passed 25 quiet years (1 generation), from 2224 to 2248.
2249 e45 colony_founded/15: The Grork Horde sent settlers from Zugnob Hole to found Snozz-Skink in Southwest Steppe.
2274 e46 colony_founded/30: The Grork Horde sent settlers from Zugnob Hole to found Beedruzz in South Downs.
2299 e47 became_town/18: Snozz-Skink grew into a town of 1577. [causes e45]
2299 e48 colony_founded/30: The Grork Horde sent settlers from Zugnob Hole to found Skrotch in Southwest Downs II.
2324 e49 became_town/18: Beedruzz grew into a town of 2020. [causes e46]
2324 e50 colony_founded/15: The Grork Horde sent settlers from Zugnob Hole to found Niskreerk in Southwest Steppe.
2349 e51 became_town/18: Skrotch grew into a town of 1683. [causes e48]
2349 e52 colony_founded/30: The Grork Horde sent settlers from Snozz-Skink to found Snugg in Central Wood.
2374 e53 became_town/18: Glathenddell grew into a town of 1559. [causes e25]
2374 e54 colony_founded/30: The Grork Horde sent settlers from Snozz-Skink to found Gruz-Keegg in South Wood.
2399 e55 became_town/18: Niskreerk grew into a town of 1768. [causes e50]
2399 e56 colony_founded/30: The Grork Horde sent settlers from Snozz-Skink to found Groz in West Downs II.
2399-2498 e57 quiet_era/20: Finen passed 100 quiet years (1 generation), from 2399 to 2498.
2424 e58 became_village/10: Groz grew into a village of 395. [causes e56]
2424 e59 colony_founded/15: The Grork Horde sent settlers from Snozz-Skink to found Drirk Pit in Southwest Steppe.
2449 e60 became_town/18: Snugg grew into a town of 1519. [causes e52]
"""

SAMPLE_END = """\
c1 Finen (Elves), home West Wood III, founded 0, alive; capital Voeledell (town 1316); 11 settlements, 3 regions, population 11624
c2 The Realm of Ithieglie (Elves), home Southeast Isle, founded 0, alive; capital Mofaesi (village 659); 4 settlements, 1 regions, population 1411
c3 The Grork Horde (Goblins), home Southwest Steppe, founded 2074, alive; capital Zugnob Hole (town 6260); 12 settlements, 3 regions, population 22321
c4 The Hold of Dulimin (Dwarves), home South Wood II, founded 3242, alive; capital Grirnbra (town 6139); 4 settlements, 1 regions, population 13898
c5 The Narinthri Throne (Dwarves), home Northwest Peaks, founded 3242, alive; capital Kholikhu (fortress 3287); 4 settlements, 1 regions, population 6572
c6 The Teeshti Court (Faeries), home East Wood III, founded 4243, alive; capital Matia Glade (village 1499); 8 settlements, 5 regions, population 6182
c7 The Warrens of Dribnark (Goblins), home South Wood, founded 4392, alive; capital Sneezskrub Hole (town 4595); 4 settlements, 1 regions, population 9088
c8 The Kingdom of Stard (Humans), home Northeast Downs II, founded 4969, alive; capital Meadstead (city 12212); 17 settlements, 8 regions, population 108290
c9 The Duchy of Dant (Humans), home Northwest Downs II, founded 4969, alive; capital Bildheldham (town 5006); 5 settlements, 2 regions, population 20319
c10 The Zak-Neez Mob (Goblins), home South Downs, founded 5097, alive; capital Beedruzz (town 3693); 12 settlements, 3 regions, population 30892
c11 The Ghuughzuz Horde (Orcs), home Northwest Downs III, founded 5596, alive; capital Aa'azuugh Fort (town 4801); 4 settlements, 1 regions, population 9139
c12 The Slig Mob (Goblins), home West Downs II, founded 5637, alive; capital Groz (town 3657); 5 settlements, 2 regions, population 9913
settlements standing 90 ruined 1 faded 0
"""


def chronicle_lines(res, inp):
    """15.2's line form: <span> e<id> <kind>/<salience>: <sentence> [causes ...]."""
    keys = {k["id"]: k["key"] for k in COMPILED}
    causes = {}
    for (e, c) in res["event_cause"]:
        causes.setdefault(e, []).append(c)
    out = []
    for e, text in zip(res["event"], sentences(res, inp)):
        span = "%d" % e[1] if e[1] == e[2] else "%d-%d" % (e[1], e[2])
        line = "%s e%d %s/%d: %s" % (span, e[0], keys[e[3]], e[7], text)
        if e[0] in causes:
            line += " [causes %s]" % ", ".join("e%d" % c for c in causes[e[0]])
        out.append(line)
    return out


def kind_counts(res):
    keys = {k["id"]: k["key"] for k in COMPILED}
    out = {}
    for e in res["event"]:
        out[keys[e[3]]] = out.get(keys[e[3]], 0) + 1
    return out


def check_example(c, inputs):
    res = world_case("hist-peoples-42-1024", 42, 1024, 6000, "all", "normal", inputs)
    inp = res["_inputs"]
    got = chronicle_lines(res, inp)
    want = SAMPLE_FIRST_60.splitlines()
    first_diff = None
    for i, (a, b) in enumerate(zip(got, want)):
        if a != b:
            first_diff = (i, a, b)
            break
    c.true("15.2 first 60 events%s" % ("" if first_diff is None else ": first difference at line %d\n    ref:  %s\n"
                                                                   "    spec: %s" % first_diff),
         first_diff is None and len(got) >= 60)
    end = end_state(res, inp)
    want_end = SAMPLE_END.splitlines()
    for a, b in zip(end, want_end):
        c.eq("15.2 end state", a, b)
    c.eq("15.2 civilizations", len(end) - 1, len(want_end) - 1)
    c.eq("15.2 events", len(res["event"]), 530)
    w = res["_world"]
    check_invariants(c, res, inp, w, "hist-peoples-42-1024")
    return res


def unit_geo(regions=None, borders=(), region_biomes=(), races=("elf",), dat=None, n=64, layers=None,
             peoples=None, race_fields=None, civs="auto", intensity="normal", span=6000, starts=None, seed=0):
    """Hand-made inputs (16.1): regions as dicts (missing keys default)."""
    dat = edited(dat or data(), peoples=peoples, races=race_fields)
    regs = []
    for i, r in enumerate(regions or [dict()]):
        d = dict(id=i + 1, name="Unit %d" % (i + 1), x_min=0, y_min=0, x_max=n - 1, y_max=n - 1, seed_x=1,
                 seed_y=1, cells=4000, lake_cells=0, walk_cells=3000, habitability_mean=50, coast_cells=0,
                 fertility_mean=100, forage_mean=100, timber_mean=100, fresh_water_mean=100, ore_reach_cells=0,
                 elev_max_dm=0)
        d.update(r)
        regs.append(d)
    p = pchr.params(span=span, races=pchr.races_of(",".join(races), dat.chron), civs=civs_param(civs),
                    intensity=intensity)
    chron = pchr.chronology(dat.chron, p, seed, [r["name"] for r in regs])
    if layers is None:
        reg = [0] * (n * n)
        for r in regs:
            for y in range(r["y_min"], r["y_max"] + 1):
                for x in range(r["x_min"], r["x_max"] + 1):
                    reg[y * n + x] = r["id"]
        layers = dict(region=reg, water=[0] * (n * n), habitability=[0] * (n * n), river=[0] * (n * n),
                      flow_dir=[8] * (n * n))
    # r6: elevation 1 dm, ore 0, walkable everywhere unless the case gives them
    layers.setdefault("elevation", [1] * (n * n))
    layers.setdefault("ore", [0] * (n * n))
    layers.setdefault("walk_area", [1] * (n * n))
    ages = chron["age"]
    races_rows = chron["race"]
    if starts is not None:
        ends = [starts[i + 1] - 1 for i in range(len(starts) - 1)] + [span - 1]
        ages = [(a[0], a[1], starts[i], ends[i], a[4]) for i, a in enumerate(ages)]
        ys = {a[1]: a[2] for a in ages}
        races_rows = [r[:10] + (ys.get(r[0]),) + r[11:] for r in races_rows]
    biomes = {1: 1, 2: 1, 3: 0, 4: 0, 5: 0, 6: 0, 7: 0, 8: 0, 9: 0, 10: 0, 11: 0, 12: 0}
    inp = Inputs(seed, n, p, regs, races_rows, chron["race_biome"], chron["culture"], chron["god"], ages,
                 [dict(dict(length_m=0, walk_pairs=0, river_pairs=0, pass_id=0), **b) for b in borders],
                 list(region_biomes), biomes, lambda: layers)
    return dat, inp


def check_units(c):
    dat0 = data()
    ELF, GOBLIN, DWARF, FAERIE, HUMAN, ORC = 1, 2, 3, 4, 5, 6
    # 16.1 item 1: each term alone
    dat, inp = unit_geo([dict(cells=4000, walk_cells=3000)], region_biomes=[(1, 12, 4000)], races=("faerie",))
    g = Geo(inp, dat)
    s_, b_, u_, c_ = g.parts(1, FAERIE)
    c.eq("16.1.1 fen-only region for faeries: B", b_, 800)          # weight 800 x share 1000 div 1000
    c.eq("16.1.1 S (means 100, ore 0)", s_, (60 * 100 + 20 * 100 + 20 * 100) * 1000 // (255 * 100))
    c.eq("16.1.1 U (rough 250, upland 200)", u_, 200 * 250 // 1000)
    dat, inp = unit_geo([dict(cells=4000, walk_cells=0, fertility_mean=0, forage_mean=0, timber_mean=0,
                              fresh_water_mean=0)], races=("dwarf",))
    g = Geo(inp, dat)
    c.eq("16.1.1 upland rough 1000 no ore: parts", g.parts(1, DWARF), (0, 0, 800, -0))
    c.eq("16.1.1 sigma", g.sigma(1, DWARF), 800)
    for cc, want in ((0, 0), (100, 500), (200, 1000), (400, 1000)):
        dat, inp = unit_geo([dict(coast_cells=cc)], races=("human",))
        g = Geo(inp, dat)
        c.eq("16.1.1 coast_m at coast_cells %d" % cc, g.q[1]["coast_m"], want)
        c.eq("16.1.1 C term human at %d" % cc, g.parts(1, HUMAN)[3], 200 * want // 1000)
    # floor division: a B of -1 must be -1 (elf grassland 0, heath -200: share 5 -> -1)
    dat, inp = unit_geo([dict(fertility_mean=0, forage_mean=0, timber_mean=0, fresh_water_mean=0, walk_cells=4000)],
                        region_biomes=[(1, 9, 5), (1, 4, 995)], races=("elf",))
    g = Geo(inp, dat)
    c.eq("16.1.1 B = -1 by floor division", g.parts(1, ELF)[1], -1)
    c.eq("16.1.1 sigma -1 by floor division", g.sigma(1, ELF), -1)
    c.eq("16.1.1 fit clamps at 0", g.fit(1, ELF), 0)
    c.eq("16.1.1 capacity floor", g.capacity(1, ELF), 400)
    # capacity with full fit: human, walk 3000 of 4000, rough 1000 at 150 permille
    dat, inp = unit_geo([dict(fertility_mean=255, forage_mean=255, timber_mean=255, fresh_water_mean=255,
                              ore_reach_cells=200)], region_biomes=[(1, 4, 4000)], races=("human",))
    g = Geo(inp, dat)
    c.eq("16.1.1 human S full", g.parts(1, HUMAN)[0], 1000)
    c.eq("16.1.1 fit clamps at 1000", g.fit(1, HUMAN), 1000)
    c.eq("16.1.1 capacity", g.capacity(1, HUMAN), (3000 + 1000 * 150 // 1000) * 1000 * 1000 * 2500 // 10 ** 9)
    dat, inp = unit_geo([dict(cells=100000, walk_cells=100000, fertility_mean=255, forage_mean=255,
                              timber_mean=255, fresh_water_mean=255, ore_reach_cells=200)],
                        region_biomes=[(1, 4, 100000)], races=("human",), peoples=dict(people_per_kcell=10 ** 8))
    g = Geo(inp, dat)
    try:
        g.capacity(1, HUMAN)
        c.true("16.1.1 bound guard", False)
    except FatalError as e:
        c.true("16.1.1 bound guard: %s" % e, str(e).startswith("history: bound: capacity of region 1"))
    # 16.1 item 2: borders
    dat, inp = unit_geo([dict(x_max=9), dict(x_min=10, x_max=19), dict(x_min=20, x_max=29), dict(x_min=30, x_max=39),
                         dict(x_min=40, x_max=49)],
                        borders=[dict(a=1, b=2, walk_pairs=3, length_m=1000),
                                 dict(a=2, b=3, pass_id=1, length_m=1000),
                                 dict(a=3, b=4, length_m=1000),
                                 dict(a=4, b=5, walk_pairs=1, river_pairs=32, length_m=1024),
                                 dict(a=1, b=5, walk_pairs=1, river_pairs=5, length_m=0)])
    g = Geo(inp, dat)
    c.eq("16.1.2 neighbours of 1", g.nb[1], [(2, 1), (5, 1)])
    c.eq("16.1.2 pass-only", g.nb[3], [(2, 3)])
    c.eq("16.1.2 river share exactly 500 permille", g.nb[4], [(5, 1 + 2)])
    c.eq("16.1.2 length 0: no river cost", g.nb[5], [(1, 1), (4, 3)])
    c.eq("16.1.2 barrier: 4 unreachable from 1 but through 5", g.hops(1), {1: 0, 2: 1, 5: 1, 3: 2, 4: 2})
    check_unit_sites(c)
    check_unit_homes(c)
    check_unit_growth(c)


def check_unit_sites(c):
    ELF, HUMAN, GOBLIN = 1, 5, 2
    n = 64
    # 16.1 item 4: mouths; item 3: sites
    reg = [1] * (n * n)
    wat = [0] * (n * n)
    hab = [0] * (n * n)
    riv = [0] * (n * n)
    fd = [8] * (n * n)
    for y in range(n):
        for x in range(4):
            wat[y * n + x] = 3
            reg[y * n + x] = 0
    # a class-4 river cell at (4, 10) draining west into the ocean: a mouth
    wat[10 * n + 4], riv[10 * n + 4], fd[10 * n + 4] = 1, 4, 4
    # class 3 at (4, 40): not a mouth
    wat[40 * n + 4], riv[40 * n + 4], fd[40 * n + 4] = 1, 3, 4
    # class 5 at (63, 50) pointing east off the grid: not a mouth
    wat[50 * n + 63], riv[50 * n + 63], fd[50 * n + 63] = 1, 5, 0
    for i in range(n * n):
        hab[i] = 10
    hab[60 * n + 10] = 69   # the inland best cell: value 69, outside the spacing of (40, 40)
    lay = dict(region=reg, water=wat, habitability=hab, river=riv, flow_dir=fd)
    for coast, want_mouth, want_sea in ((-1000, 0, 0), (0, 60, 20), (1000, 120, 40)):
        dat, inp = unit_geo([dict(x_min=4, seed_x=40, seed_y=40)], races=("elf",), layers=lay,
                            race_fields={ELF: dict(coast=coast)})
        g = Geo(inp, dat)
        g.layers()
        c.eq("16.1.4 mouths", g.mouths, [10 * n + 4])
        c.eq("16.1.3 port near a mouth, coast %d" % coast, g.port(13 * n + 7, ELF), want_mouth)
        c.eq("16.1.3 port near the sea, coast %d" % coast, g.port(30 * n + 6, ELF), want_sea)
        c.eq("16.1.3 no port at distance 4, coast %d" % coast, g.port(30 * n + 7, ELF), 0)
    dat, inp = unit_geo([dict(x_min=4, seed_x=40, seed_y=40)], races=("elf",), layers=lay,
                        race_fields={ELF: dict(coast=1000)})
    w = World(dat, inp, event_catalogue(dat.report))
    c.eq("16.1.3 seed cell taken", w.site(1, ELF, False), (40, 40, "seed"))
    w.settlements.append(dict(id=1, x=40, y=40 + 31, status=THRIVING))
    c.eq("16.1.3 seed blocked at spacing 31: best value (mouth bonus 120 + 10 beats 69)", w.site(1, ELF, False),
         (4, 7, "best"))
    w.settlements[0]["y"] = 40 + 32
    c.eq("16.1.3 spacing 32 does not block", w.site(1, ELF, False), (40, 40, "seed"))
    w.settlements[0].update(y=40, status=FADED)
    c.eq("16.1.3 faded does not block", w.site(1, ELF, False), (40, 40, "seed"))
    w.settlements[0]["status"] = RUINED
    c.eq("16.1.3 a ruin blocks", w.site(1, ELF, False)[2], "best")
    dat2, inp2 = unit_geo([dict(x_min=4, seed_x=40, seed_y=40)], races=("elf",), layers=lay,
                          race_fields={ELF: dict(coast=-1000)})
    w2 = World(dat2, inp2, event_catalogue(dat2.report))
    w2.settlements.append(dict(id=1, x=40, y=40, status=THRIVING))
    c.eq("16.1.3 coast -1000: no bonus, the inland cell wins", w2.site(1, ELF, False), (10, 60, "best"))
    # ties to the smaller index: two cells of value 69
    hab2 = list(hab)
    hab2[5 * n + 60] = 69
    lay2 = dict(lay, habitability=hab2)
    dat3, inp3 = unit_geo([dict(x_min=4, seed_x=40, seed_y=40)], races=("elf",), layers=lay2,
                          race_fields={ELF: dict(coast=-1000)})
    w3 = World(dat3, inp3, event_catalogue(dat3.report))
    w3.settlements.append(dict(id=1, x=40, y=40, status=THRIVING))
    c.eq("16.1.3 tie by cell index", w3.site(1, ELF, False), (60, 5, "best"))
    # relaxed and no site: a region of 3 x 1 cells
    reg4 = [0] * (n * n)
    for x in (10, 11, 12):
        reg4[5 * n + x] = 1
    lay4 = dict(region=reg4, water=[0] * (n * n), habitability=[0] * (n * n), river=[0] * (n * n),
                flow_dir=[8] * (n * n))
    dat4, inp4 = unit_geo([dict(x_min=10, x_max=12, y_min=5, y_max=5, seed_x=10, seed_y=5)], races=("elf",),
                          layers=lay4)
    w4 = World(dat4, inp4, event_catalogue(dat4.report))
    w4.settlements.append(dict(id=1, x=10, y=5, status=THRIVING))
    c.eq("16.1.3 no free site (not relaxed)", w4.site(1, ELF, False), None)
    c.eq("16.1.3 relaxed site", w4.site(1, ELF, True), (11, 5, "relaxed"))
    for x in (11, 12):
        w4.settlements.append(dict(id=len(w4.settlements) + 1, x=x, y=5, status=FADED))
    c.eq("16.1.3 relaxed: no cell without a settlement of any status", w4.site(1, ELF, True), None)


def check_unit_homes(c):
    ELF = 1
    # 16.1 item 5: ties by the keyed draw then id; capacity floor of homes; fallbacks; auto and explicit counts
    rs = [dict(x_min=8 * i, x_max=8 * i + 7, seed_x=8 * i, seed_y=0, cells=20000, walk_cells=20000)
          for i in range(8)]
    dat, inp = unit_geo(rs, races=("elf",))
    w = World(dat, inp, event_catalogue(dat.report))
    order = w.order_regions(ELF)
    draws = {r: pnr.draw(cr.stream_key(0, "hist.peoples.home"), 0, ELF, r, 0) for r in range(1, 9)}
    c.eq("16.1.5 equal sigma: order by draw then id", order, sorted(range(1, 9), key=lambda r: (draws[r], r)))
    # a high-sigma region below home_min_capacity is skipped
    rs2 = [dict(x_max=7, walk_cells=100, cells=1200, fertility_mean=255, forage_mean=255, timber_mean=255),
           dict(x_min=8, x_max=15, seed_x=8, cells=20000, walk_cells=20000)]
    dat, inp = unit_geo(rs2, races=("elf",), region_biomes=[(1, 3, 1200), (2, 4, 4000)])
    w = World(dat, inp, event_catalogue(dat.report))
    c.true("16.1.5 region 1 scores higher", w.geo.sigma(1, ELF) > w.geo.sigma(2, ELF))
    c.true("16.1.5 region 1 below home_min_capacity", w.geo.capacity(1, ELF) < 1500)
    c.eq("16.1.5 home skips it", w.homes(ELF)[0], [2])
    dat, inp = unit_geo(rs2, races=("elf",), region_biomes=[(1, 3, 1200), (2, 4, 4000)],
                        peoples=dict(home_min_capacity=1000000))
    w = World(dat, inp, event_catalogue(dat.report))
    c.eq("16.1.5 nothing eligible: first free non-islet region of any capacity", w.homes(ELF)[0], [1])
    w.owner[1] = (9, 1)
    w.owner[2] = (9, 1)
    c.eq("16.1.5 all held: first non-islet region", w.homes(ELF)[0], [1])
    dat, inp = unit_geo([dict(cells=1000), dict(cells=1100, x_min=8, seed_x=8)], races=("elf",))
    w = World(dat, inp, event_catalogue(dat.report))
    c.eq("16.1.5 only islets: the first region in order", w.homes(ELF)[0][0], w.order_regions(ELF)[0])
    # auto reduced by regions_per_civ: 8 eligible regions, one alive civ: 8 < 4 x 1 + 2 is false -> may be 2;
    # with 5 alive civs 8 < 22 -> 1
    dat, inp = unit_geo(rs, races=("elf",), civs="auto")
    w = World(dat, inp, event_catalogue(dat.report))
    for i in range(5):
        w.civs.append(dict(id=i + 1, ended_year=None))
    c.eq("16.1.5 auto reduced to 1 by regions_per_civ", len(w.homes(ELF)[0]), 1)
    # explicit 4 with two regions far apart (a barrier between, unreachable = far)
    dat, inp = unit_geo(rs[:2], races=("elf",), civs="4")
    w = World(dat, inp, event_catalogue(dat.report))
    c.eq("16.1.5 explicit 4 on two separated regions: two homes", len(w.homes(ELF)[0]), 2)
    # home_min_hops: a chain 1-2-3-4 with walkable borders: second home at 3 hops at least
    dat, inp = unit_geo(rs[:4], races=("elf",), civs="2",
                        borders=[dict(a=1, b=2, walk_pairs=1), dict(a=2, b=3, walk_pairs=1),
                                 dict(a=3, b=4, walk_pairs=1)])
    w = World(dat, inp, event_catalogue(dat.report))
    h, n = w.homes(ELF)
    c.true("16.1.5 homes at least home_min_hops apart: %s" % h, len(h) == 1 or abs(h[0] - h[1]) >= 3)


def check_unit_growth(c):
    ELF, HUMAN = 1, 5
    # 16.1 item 6: 9.1's formula
    dat, inp = unit_geo([dict()], races=("human",))
    w = World(dat, inp, event_catalogue(dat.report))
    g = 15

    def step(p, cs, cap=7999, gg=g):
        if p < cs:
            d = (p * gg * (cs - p) + 500 * cs) // (1000 * cs)
            if gg > 0:
                d = max(1, d)
            return min(p + d, max(p, cap))
        if p > cs:
            return p - max(1, (p - cs) * 100 // 1000)
        return p
    c.eq("16.1.6 P < C_s", step(300, 2000), 300 + (300 * 15 * 1700 + 500 * 2000) // 2000000)
    c.eq("16.1.6 P = C_s", step(2000, 2000), 2000)
    c.eq("16.1.6 P > C_s", step(3000, 2000), 2900)
    c.eq("16.1.6 P just above C_s: at least 1", step(2001, 2000), 2000)
    c.eq("16.1.6 g = 0", step(300, 2000, gg=0), 300 + (500 * 2000) // 2000000)
    c.eq("16.1.6 max(1, d) for slow races", step(60, 100000, gg=3), 61)
    c.eq("16.1.6 kind cap", step(1499, 100000, cap=1499), 1499)
    c.eq("16.1.6 above the cap: no growth, no loss", step(1600, 100000, cap=1499), 1600)
    # 16.1 item 8: ladder and caps
    c.eq("16.1.8 ladder humans 300", w.ladder(HUMAN, 300), VILLAGE)
    c.eq("16.1.8 ladder humans 249", w.ladder(HUMAN, 249), HAMLET)
    c.eq("16.1.8 ladder humans 8000", w.ladder(HUMAN, 8000), CITY)
    c.eq("16.1.8 ladder faeries 9000", w.ladder(4, 9000), VILLAGE)
    dat2 = edited(dat, races={HUMAN: dict(kinds=[FORTRESS])})
    w2 = World(dat2, inp, event_catalogue(dat.report))
    c.eq("16.1.8 a race with only fortress", w2.ladder(HUMAN, 5000), FORTRESS)
    dat3 = edited(dat, races={HUMAN: dict(kinds=[TOWN, CITY])})
    w3 = World(dat3, inp, event_catalogue(dat.report))
    c.eq("16.1.8 none at or below P: the lowest ladder kind", w3.ladder(HUMAN, 100), TOWN)
    c.eq("16.1.8 hysteresis edge: 1200 x 1000 = 1500 x 800 does not downgrade", 1200 * 1000 < 1500 * 800, False)


def check_quiet_equals_ticks(c):
    """14 item 9 / 16.1 item 6: Q1 of L years = L T1 years without pressures (one settlement, no events)."""
    dat, inp = unit_geo([dict()], races=("human",))
    for L in (1, 7, 25):
        res = []
        for quiet in (False, True):
            w = World(dat, inp, event_catalogue(dat.report))
            w.civs.append(dict(id=1, name="C", race=5, culture=5, home_region=1, capital=1, founded_year=0,
                               founding_event=1, ended_year=None, ending_event=0, created=0))
            w.settlements.append(dict(id=1, name="S", x=1, y=1, region=1, founder_civ=1, founded_year=0,
                                      founding_event=1, owner_civ=1, kind=HAMLET, status=THRIVING, population=60,
                                      ruined_year=None, ruin_event=0, created=0))
            w.serial = 5
            if quiet:
                w.years = L
                for _ in range(L):
                    w.grow_once()
            else:
                for _ in range(L):
                    w.serial += 1
                    w.grow_once()
            res.append(w.settlements[0]["population"])
        c.eq("14 item 9: Q1 of %d years = %d T1 years" % (L, L), res[0], res[1])


def check_properties(c, seeds=SYN_SEEDS):
    dat = data()
    sdat = edited(dat, peoples=STRESS)
    seen = {}
    for seed in STRESS_SEEDS:
        inp, _g = synthetic_inputs(seed, sdat)
        res = history_stage(sdat, inp)
        check_invariants(c, res, inp, res["_world"], "stress-%d" % seed)
        for k, v in kind_counts(res).items():
            seen[k] = seen.get(k, 0) + v
    for k in ("settlement_abandoned", "capital_moved", "civ_ended", "ruin_faded", "civ_seceded"):
        c.true("stress cases reach %s" % k, seen.get(k, 0) > 0)
    for seed in seeds:
        inp, _g = synthetic_inputs(seed, dat)
        res = history_stage(dat, inp)
        check_invariants(c, res, inp, res["_world"], "syn-%d" % seed)
        # 14 item 10: an extra draw on every stream before each decision changes nothing
        if seed < 4:
            res2 = history_stage(dat, inp, extra_draw=True)
            c.eq("syn-%d 14 item 10 draw independence" % seed, table_hashes(res2), table_hashes(res))


def cmd_check(argv):
    inputs = argv[argv.index("--inputs") + 1] if "--inputs" in argv else DEFAULT_INPUTS
    quick = "--quick" in argv
    c = Checker()
    dat = data()
    c.eq("history.peoples = the spec listing", dat.peoples, pdr.parsed_spec("history")["peoples"])
    c.eq("report entries 6-19 = section 11", [e for e in dat.report[5:19]], list(SPEC_REPORT))
    c.eq("report_events on the 0.15.0 report", report_events_messages(dat.report), [])
    for k in COMPILED:
        c.eq("template of %s parses" % k["key"], pe.template_problem(k, dict((e[0], e[2]) for e in dat.report)[k["key"]]),
             None)
    c.eq("12 streams", len(STREAMS), 12)
    check_units(c)
    check_quiet_equals_ticks(c)
    check_properties(c, SYN_SEEDS[:6] if quick else SYN_SEEDS)
    if os.path.isdir(inputs):
        res = check_example(c, inputs)
        print("15.2 reference: %d events, kinds %s" % (len(res["event"]), sorted(kind_counts(res).items())))
        if not quick:
            fresh = golden_text(inputs)
            if os.path.exists(GOLDEN):
                with open(GOLDEN, "rb") as f:
                    c.eq("committed golden file is current", f.read().decode(), fresh)
            else:
                c.true("golden file present", False)
    else:
        print("note: %s missing; the world cases and the golden comparison are skipped" % inputs)
    if pdr.PRE_015_USED:
        print("note: the disk holds 0.14.0 race/history files; the P3-05 listings stand in for them")
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def cmd_golden(argv):
    inputs = argv[argv.index("--inputs") + 1] if "--inputs" in argv else DEFAULT_INPUTS
    pos = [a for i, a in enumerate(argv) if not a.startswith("--") and (i == 0 or argv[i - 1] != "--inputs")]
    text = golden_text(inputs)
    if pos and pos[0] == "-":
        sys.stdout.write(text)
    else:
        with open(pos[0] if pos else GOLDEN, "wb") as f:
            f.write(text.encode())
    return 0


def opt(argv, name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


def cmd_world(argv):
    path = argv[0]
    dat = data(opt(argv, "--data", "game/data"))
    conn = pchr.open_world(path)
    meta = dict(conn.execute("SELECT key, value FROM meta").fetchall())
    p0 = pchr.params_from_meta(meta)
    races = opt(argv, "--races")
    p = pchr.params(span=int(opt(argv, "--span", p0["span"])),
                    races=pchr.races_of(races, dat.chron) if races else p0["races"],
                    detail=opt(argv, "--detail", p0["detail"]),
                    civs=civs_param(opt(argv, "--civs")) if "--civs" in argv else p0["civs"],
                    intensity=opt(argv, "--intensity", p0["intensity"]))
    inp, _chron = inputs_for(dat, conn, p, int(meta["seed"]), int(meta["param.map_size"]))
    res = history_stage(dat, inp)
    w = res["_world"]
    for line in score_lines("w", w, inp) + homes_lines("w", w):
        print(line)
    for k, v in sorted(table_hashes(res).items()):
        print("table %s %s" % (k, v))
    for line in report_lines(res, inp) + stats_extra(res):
        print(line)
    for line in chronicle_lines(res, inp):
        print(line)
    for line in end_state(res, inp):
        print(line)
    print("kinds %s" % sorted(kind_counts(res).items()))
    print("hunger max %d" % w.hunger_max)
    return 0


def stored_rows(conn, table):
    _h, rows = pchr.world_table_hash(conn, table)
    return [tuple(r) for r in rows]


def cmd_check_world(argv):
    global SPREAD_OWNER, QUIET_RECOVER_EACH_YEAR
    path = argv[0]
    SPREAD_OWNER = opt(argv, "--spread-owner", SPREAD_OWNER)        # after (ruling 6, default) or outbreak
    QUIET_RECOVER_EACH_YEAR = opt(argv, "--quiet-recovery", "each-year") == "each-year"   # or end
    dat = data(opt(argv, "--data", "game/data"))
    conn = pchr.open_world(path)
    meta = dict(conn.execute("SELECT key, value FROM meta").fetchall())
    c = Checker()
    if meta.get("schema_version") != "7":
        print("%s: schema version %s, check-world reads 7" % (path, meta.get("schema_version")))
        return 1
    c.eq("generator_version", meta.get("generator_version"), GENERATOR_VERSION if FLAGS["dest"] else "0.16.0")
    stages = meta["stages"].split(",") if meta["stages"] else []
    p = pchr.params_from_meta(meta)
    inp, chron = inputs_for(dat, conn, p, int(meta["seed"]), int(meta["param.map_size"]))
    if "chronology" in stages:
        for t in pchr.TABLES:
            got, _ = pchr.world_table_hash(conn, t)
            c.eq("table %s" % t, got, pchr.table_hash(pchr.COLUMNS[t], chron[t]))
    if "history" in stages:
        res = history_stage(dat, inp)
        want = table_hashes(res)
        stored = {t: stored_rows(conn, t) for t in TABLES}
        for t in TABLES:
            got = pchr.table_hash(COLUMNS[t], stored[t])
            c.eq("table %s" % t, got, want[t])
            if got != want[t]:
                for a, b in zip(stored[t], res[t]):
                    if a != b:
                        print("  first difference in %s: stored %r, reference %r" % (t, a, b))
                        break
                else:
                    print("  %s: %d stored rows, %d reference rows" % (t, len(stored[t]), len(res[t])))
        st = dict(stored)
        check_invariants(c, st, inp, res["_world"], os.path.basename(path))
        print("\n".join(report_lines(st, inp)))
    conn.close()
    print("%s: %d checks, %d failures" % (path, c.count, c.failures))
    return 1 if c.failures else 0


def cmd_synthetic(argv):
    seed = int(argv[0])
    inp, g = synthetic_inputs(seed)
    print("regions %d borders %d region_biome %d" % (len(g["regions"]), len(g["borders"]), len(g["region_biomes"])))
    for r in g["regions"]:
        print(" ".join("%s=%s" % (k, r[k]) for k in REGION_COLS))
    for b in g["borders"]:
        print(" ".join("%s=%s" % (k, b[k]) for k in BORDER_COLS))
    res = history_stage(data(), inp)
    res["_inputs"] = inp
    for line in case_lines("syn-%d" % seed, res, True):
        print(line)
    return 0


def main(argv):
    # --legacy: every P3-08 rule flag off (the 0.16.0 rules; with 0.16.0 data and names the 0.16.0 worlds)
    if "--legacy" in argv:
        argv = [a for a in argv if a != "--legacy"]
        legacy()
    if not argv:
        print(__doc__)
        return 2
    cmd = argv[0]
    if cmd == "check":
        return cmd_check(argv[1:])
    if cmd == "golden":
        return cmd_golden(argv[1:])
    if cmd == "world":
        return cmd_world(argv[1:])
    if cmd == "check-world":
        return cmd_check_world(argv[1:])
    if cmd == "synthetic":
        return cmd_synthetic(argv[1:])
    if cmd == "gaps":
        for clause, reading in SPEC_GAPS:
            print("- %s: %s" % (clause, reading))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
