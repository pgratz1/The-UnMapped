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
GENERATOR_VERSION = "0.24.0"  # P5-03: reach, road losses, unfaded ruins, the ledger (P5-02: the ghost rule (8.2), the WildView seam (7.1, 7.2), settlement.ending (P5-12: P3-02 r19 names, kind 43 in the catalogue; no peoples rule (P5-01: P3-05 r18 24 (data), kinds 41, 42 (P3-11: P3-05 r16 23 (P3-10: P3-05 r12 22 (P3-09: r8 21; P3-08: r6 rules, kind 31, P3-04 r10 quiet eras)
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
    # P3-05 r8 21 (generator 0.18.0, P3-09)
    auto_size=True,     # 21 item 5: `auto` + min(civs_extra_max, N div regions_per_extra_civ)
    refuge_floor=True,  # 21 item 6 (r9, P3-09 r3): a race's last refuge is never abandoned
    # P3-05 r12 22 (generator 0.19.0, P3-10); P3-04 r12's quiet stretches are pe.QUIET_R12
    outbreak37=True,    # 22.1: every outbreak is kind 37 plague_outbreak with total and reached
    memory=True,        # 22.2: causes older than cause_memory_years are not cited; a dwindle cites its colony
    sites19=True,       # 22.5: the site value with inland, port, habitability; cell term = not neutral
    lie_low=True,       # 22.7a: no founding while Y < low_until[c]
    cores=True,         # P3-02 r10 12.2: civilization and settlement names avoid near cores (K)
    # P3-05 r16 23 (generator 0.20.0, P3-11)
    awaken_pop=True,    # 23.2: the awakening settlement starts at max(founding, min(awakening_population, C(H, r)))
    # P5-02 (generator 0.23.0)
    ghost=True,         # P5-02 8.2: a small place struck by famine or plague may be left standing (hist.peoples.ghost)
    # P5-03 (generator 0.24.0)
    p503=True,          # P5-03 11.1: a destination within abandon_reach_hops, the losses on the road; 11.2: ruins left
                        # standing do not fade; 7.2: the ledger calls of the ruin procedure; 7.3 owner_event; 9.4
                        # abandon public (with causes)
    # P5-06 (generator 0.27.0)
    nofade=True,        # P5-06 25.1 (a): no ruin fades (Q3 does nothing); ruin_fade_years and ghost_fade_years gone
)
FLAGS_018 = ("auto_size", "refuge_floor")
FLAGS_019 = ("outbreak37", "memory", "sites19", "lie_low", "cores")
FLAGS_020 = ("awaken_pop",)
FLAGS_023 = ("ghost",)
FLAGS_024 = ("p503",)
FLAGS_027 = ("nofade",)
VERSIONS = ("0.16.0", "0.17.0", "0.18.0", "0.19.0", "0.20.0", "0.21.0", "0.22.0", "0.23.0", "0.24.0", "0.25.0",
            "0.26.0", "0.27.0", "0.28.0", "0.29.0")   # 0.21.0, 0.22.0, 0.28.0, 0.29.0: no peoples rule


def road_loss_of(P, u, t):
    """P5-03 11.1: road = P x (min + pick(u, max - min + 1)) div 1000, at most P - 1 when P >= 1."""
    lo, hi = t["abandon_loss_min_permille"], t["abandon_loss_max_permille"]
    road = P * (lo + pnr.pick(u, hi - lo + 1)) // 1000
    return min(road, P - 1) if P >= 1 else road


def flags_for(v):
    """The FLAGS of generator v (for p3_conflict_reference.py, which sets this module's flags)."""
    if v not in VERSIONS:
        raise ValueError(v)
    i = VERSIONS.index(v)
    return {k: (i >= 11 if k in FLAGS_027 else i >= 8 if k in FLAGS_024 else i >= 7 if k in FLAGS_023 else i >= 4 if k in FLAGS_020 else i >= 3 if k in FLAGS_019 else i >= 2 if k in FLAGS_018 else i >= 1)
            for k in FLAGS}


def use_version(v):
    """Serve generator v: "0.19.0" (default; P3-10), "0.18.0" (P3-09), "0.17.0" (P3-08) or "0.16.0": the
    rule flags of the version and the data, names, renderer and catalogue of p3_engine_reference.use_version.
    Call it before the first data() (the data are cached)."""
    FLAGS.update(flags_for(v))
    if hasattr(pe, "use_version"):
        pe.use_version(v)
    else:
        pdr.use_version(v)
        pnr.use_rules("r4" if v == "0.16.0" else "r5")
    global TABLES, COLUMNS
    TABLES, COLUMNS = pe.TABLES, pe.COLUMNS
    _DATA.clear()


def legacy(on=True):
    """Every r6 / r10 / r8 flag off (the 0.16.0 rules), with the 0.16.0 data (p3_data_reference) and names
    (p3_names_reference r4) and the engine reference's 0.16.0 renderer, or everything 0.18.0. Call it
    before the first data() (the data are cached)."""
    use_version("0.16.0" if on else "0.27.0")

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
    ("10.2 r6 the plan", "RULED (lead, P3-08 integration): 'no state changes between the plan and the strikes' does not "
                         "hold (a spread that abandons its target sends its survivors to a later target); the plan "
                         "replays the strikes in order on a copy of the populations, with the 8.5 transfer."),
    ("14 item 11 r6", "Checked as: no civ_ended is caused by an abandonment of abandon_min or more people (the "
                      "'same step' wording would also forbid a second, unrelated abandonment ending the realm)."),
    ("14 item 12 r6", "From rows only settled >= 1 and a destination the civilization has owned; nearest-and-room "
                      "and the decline start are recomputed in the run (World.check_destination)."),
    ("8.4 r6 'such a race with a cell term'", "A race whose settlement_kinds include fortress and whose cell_weights "
                                              "are not both 0; the founding cell's walk_area is 0."),
    ("15.4 regions_held_<race>", "Counted from the open region_control rows (to_year NULL) by the race of their "
                                 "civ."),
    # P3-10 (generator 0.19.0): P3-05 r12 22 and P3-04 r12 20
    ("P3-05 r12 22.1 template (closed by r14)", "r12's {reached?|| in all} left 'in all' out at one spread; P3-05 r14 "
        "22.1 (P3-10 10d item 4) ships {reached?| in all| in all}, served from 0.19.0."),
    ("P3-05 r12 22.2 'Y - year(r)'", "year(r) is the cited event's year (its start); Y the year of the event being "
        "written (in a quiet step its start). A difference of exactly cause_memory_years is cited (<=)."),
    ("P3-05 r12 22.2 item 4 (b)", "An abandonment by 9.2 (b) whose last reduction is older than the memory cites "
        "its founding event in its place (as when it has none); the decline start is added only when recent."),
    ("P3-05 r12 22.2 item 3", "'its last colony_founded event with it as origin': kind 6 only; P3-06's "
        "refugees_settled (kind 32, also with an origin) is not a colony and is not cited."),
    ("P3-05 r12 22.5 sea_d", "The BFS runs over every cell (land, lake and sea) with unit 8-neighbour steps from "
        "every water-3 cell, so it equals the Chebyshev distance capped at inland_cap; computed as repeated one-cell "
        "dilations of the sea mask."),
    ("P3-05 r12 22.5 port(c, r)", "port(c, r) is 6.1's value including the race's coast factor "
        "(bonus x (1000 + coast) div 1000); cw_p multiplies that value (div 1000 after)."),
    ("P3-05 r12 22.7a 'Y < low_until'", "In a tick Y is the year; in a quiet step its start. The first founding "
        "chance after lie_low(c, U) is in the first step with Y >= U. A civilization never given lie_low is never "
        "skipped."),
    ("P3-02 r10 12.2 in the stub (P3-04 7.2)", "K is kept by the history stage beside the registry, so the stub "
        "configuration (P3-04's test seam, the same stage) applies the core rule too; no stub name of the engine "
        "golden's world cases changes by it (checked)."),
    ("P3-04 r13 20.1 the 64 cap", "A realm's people are the summed populations of its thriving and declining "
        "settlements when Q6 runs (after the step's subsystems); the 64 largest, ties to the smaller id, are listed "
        "in ascending id; realms counts every living civilization. No golden world has more than 64 realms."),
    ("P3-05 r13 hand-made tuning", "The reference's hand-made and stress tunings are the shipped tuning with "
        "edits, so they carry the shipped cause_memory_years 50 and inland_cap 20, as the C++ support's "
        "with_peoples does; a tuning table without the fields reads them as 100000 and 20 (rule off)."),
    ("P3-04 r12 20.1 participants", "'every civilization not ended at this step': ended_year unset when Q6 runs, "
        "so a civilization that ended earlier in the row's first step is not a participant; the event's phase is "
        "the quiet row's id; the stretch is kept per row and reset where the eras were."),
    ("P3-04 r12 20.2 words", "The two words follow 8.4's word rule (no '{', '}', '|', '?'), may be empty and may "
        "hold ',' and '=' (the shipped second word is ', '); a cword may hold a space. A body '@causes' without "
        "the colon, or '@causes:' with nothing after it, is an unknown placeholder."),
    ("P3-04 r12 20.6 item 4 / P3-10 5 'phase rows do not move'", "The phase rows' trigger_event column holds event "
        "ids, which are renumbered when the quiet lines collapse (an awakening's id is lower at 0.19.0), so the "
        "phase table hash and the 'phases' lines move in their trigger field; id, age, kind, start and end do not "
        "(checked against the 0.18.0 golden)."),
    ("P3-05 16.1a 17c (0.17.0 case)", "The case 'a last settlement below abandon_min: abandoned and the "
        "civilization ends' contradicts r9 21 item 6 (the race's last refuge is never abandoned) when the case's "
        "civilization is its race's only one; the reference case gains a second human civilization (found failing "
        "at 0.18.0 by the P3-10 tests)."),
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
assert list(pe.COMPILED[5:19]) == list(PEOPLES_KINDS) and len(pe.COMPILED) in (30, 31, 36, 38, 40, 42, 43, 55, 62, 78, 92, 96)
assert len(pe.COMPILED) == 30 or pe.COMPILED[30] == KIND31, "engine reference kind 31 differs from P3-05 r6 11"
assert getattr(pe, "KIND_31", KIND31) == KIND31, "engine reference KIND_31 differs from P3-05 r6 11"
COMPILED_016 = tuple(pe.COMPILED[:30])
COMPILED_017 = COMPILED_016 + (KIND31,)
# P3-06 r9 20.9 (generator 0.18.0): kinds 32 refugees_settled and 33 tribute, from the engine reference.
COMPILED_018 = COMPILED_017 + tuple(pe.COMPILED_018[31:36])
assert [k["key"] for k in COMPILED_018[31:]] == ["refugees_settled", "tribute", "war_won", "search_abandoned",
                                                 "refuge_held"]
# P3-04 r12 20.4 (generator 0.19.0): kinds 37 plague_outbreak (P3-05 r12 22.1) and 38 quiet_stretch.
COMPILED_019 = COMPILED_018 + (pe.KIND_37, pe.KIND_38)
assert pe.KIND_37["values"] == ["dead", "population", "total", "reached"], "kind 37 differs from P3-05 r12 22.1"
# COMPILED (every kind id a row may hold) is the 0.19.0 list; the catalogue a run writes is
# catalogue_kinds(): the engine reference's catalogue of the served version (30, 31, 36 or 38 kinds).
# P3-04 r17 21.2 (generator 0.20.0): kinds 39 claim_settlements and 40 settlements_ceded (P3-06 r19 23.2, 23.3).
COMPILED_020 = COMPILED_019 + (pe.KIND_39, pe.KIND_40)
# P3-04 r19 22.2 (generator 0.21.0): kinds 41 claim_dropped and 42 refuge_hidden (P3-06 r22 24.3, 24.4).
COMPILED_021 = COMPILED_020 + (pe.KIND_41, pe.KIND_42)
# P3-04 r21 23.2 (generator 0.22.0): kind 43 region_renamed (P5-12 7.1).
COMPILED_022 = COMPILED_021 + (pe.KIND_43,)
# P5-02 10.1 (generator 0.23.0): kinds 44 to 55.
COMPILED_023 = COMPILED_022 + pe.KINDS_023
# P5-03 11.3, 8.1 (generator 0.24.0): kinds 56 to 62.
COMPILED_024 = COMPILED_023 + pe.KINDS_024
COMPILED = COMPILED_024
KIND_ID = {k["key"]: k["id"] for k in COMPILED}


def catalogue_kinds():
    if len(pe.COMPILED) == 96:   # P5-07 8.1 (0.29.0): kinds 93 to 96
        return pe.COMPILED_029
    if len(pe.COMPILED) == 92 and getattr(pe, "RENDER_R28", False):   # P5-04a 14.4 (0.28.0): ten value rows
        return pe.COMPILED_028
    if len(pe.COMPILED) == 92:   # P5-06 8.1 (0.27.0): kinds 79 to 92
        return pe.COMPILED_027
    if len(pe.COMPILED) == 78:   # P5-04 8.1 (0.26.0): kinds 63 to 78
        return pe.COMPILED_026
    if len(pe.COMPILED) == 62 and pe.RENDER_R25:   # P5-03a 10.4: the five value rows
        return pe.COMPILED_025
    return {30: COMPILED_016, 31: COMPILED_017, 36: COMPILED_018, 38: COMPILED_019, 40: COMPILED_020,
            42: COMPILED_021, 43: COMPILED_022, 55: COMPILED_023, 62: COMPILED_024}[len(pe.COMPILED)]
# Quiet-step kinds (14 item 8): 6-10, 16-19 (and P3-04's quiet_era; from 0.19.0 its quiet_stretch, 38).
QUIET_KINDS = {6, 7, 8, 9, 10, 16, 17, 18, 19, 31, 38}   # 31: r6, an abandonment of 9.2 in Q1

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

STREAMS = ("hist.peoples.ghost", "hist.peoples.home", "hist.peoples.civs", "hist.peoples.found", "hist.peoples.outward",
           "hist.peoples.famine", "hist.peoples.famine_toll", "hist.peoples.plague", "hist.peoples.plague_toll",
           "hist.peoples.plague_spread", "hist.peoples.abandon", "hist.peoples.crisis", "hist.peoples.split",
           "hist.peoples.abandon_loss")   # P5-03 3 (0.24.0; keyed, so the older draws are unchanged)


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
        self.monsters = hist.get("monsters")   # P5-02 4.3 (0.23.0); an older root has none
        self.race = {}
        for key, t in content["race"]:
            rid = self.chron.race_by_id[key]["order"]
            self.race[rid] = dict(key=key, site=t["site_weights"], upland=t["upland"], coast=t["coast"],
                                  growth=t["growth_permille"], expansion=t["expansion"],
                                  kinds=[KIND_CODE[k] for k in t["settlement_kinds"]],
                                  density=t["density_permille"], generation=t["generation_years"],
                                  culture=t["culture"],
                                  # P3-01 r8 / P3-05 r6 6.2 (0.17.0): absent in a 0.16.0 root, read as 0, 0
                                  cell=dict(CELL_NEUTRAL, **t.get("cell_weights", {})),
                                  # P3-01 r14 3.7 (0.20.0): absent in an older root, read as 0 (the rule off)
                                  awakening=t.get("awakening_population", 0))


# P3-01 r11 3.4 (0.19.0): the neutral record; a 0.18.0 or older root has only ore and height, read with the
# three new fields neutral.
CELL_NEUTRAL = dict(ore=0, height=0, inland=0, port=1000, habitability=1000)
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
            self.sea_rows = sea_rows
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

    def cell_record(self, race):
        """r12 22.5 (0.19.0): (ore, height, inland, port, habitability); before 0.19.0 the r6 pair with the
        three new fields neutral."""
        co, ch = self.cell_weights(race)
        if not FLAGS["sites19"]:
            return co, ch, 0, 1000, 1000
        cw = dict(CELL_NEUTRAL, **self.dat.race[race]["cell"])
        return cw["ore"], cw["height"], cw["inland"], cw["port"], cw["habitability"]

    def has_cell_term(self, race):
        """r12 22.5 / P3-01 r11 3.4: the record differs from the neutral one (r6: ore or height not 0, the same
        for a record whose three new fields are neutral)."""
        return self.cell_record(race) != (0, 0, 0, 1000, 1000)

    def sea_d(self):
        """r12 22.5: per cell, the Chebyshev distance in cells to the nearest cell with water 3, capped at
        inland_cap: a multi-source breadth-first search over the 8-neighbour grid by unit steps, done here by
        repeated one-cell dilation of the sea mask (each dilation is one BFS level); a world without sea is
        inland_cap everywhere. Computed once, on the first use by a race with inland > 0."""
        if getattr(self, "_sea_d", None) is None:
            self.layers()
            n = self.n
            cap = self.t.get("inland_cap", 20)   # P3-05 r13: the hand-made default
            level = list(self.sea_rows)        # level k: the cells at distance <= k
            full = (1 << n) - 1
            count = [bytearray(n) for _ in range(n)]   # how many levels 0..cap-1 hold the cell
            for k in range(cap):
                if k > 0:
                    h = [(v | (v << 1) | (v >> 1)) & full for v in level]
                    level = [h[y] | (h[y - 1] if y > 0 else 0) | (h[y + 1] if y + 1 < n else 0) for y in range(n)]
                for y in range(n):
                    v = level[y]
                    if v:
                        row = count[y]
                        bits = bin(v)[2:][::-1]
                        for x, b in enumerate(bits):
                            if b == "1":
                                row[x] += 1
            self._sea_d = [cap - count[i // n][i % n] for i in range(n * n)]
        return self._sea_d

    def hgt(self, i, rid):
        """r6 6.2: the cell's height in its region, 0-255."""
        e = self.layers()["elevation"][i]
        return min(255, max(0, e) * 255 // max(1, self.reg[rid]["elev_max_dm"]))

    def value(self, i, rid, race):
        """r6 6.2: v(c, r) = habitability(c) + port(c, r) + (cell_ore_r ore(c) + cell_height_r hgt(c)) div 1000.
        r12 22.5 (0.19.0): v = hab x cw_a div 1000 + port x cw_p div 1000 + (cw_o ore + cw_h hgt) div 1000
        + cw_i x min(inland_cap, sea_d) x 255 div (inland_cap x 1000), each div a floor, in this order."""
        lay = self.layers()
        co, ch, ci, cp, ca = self.cell_record(race)
        v = lay["habitability"][i] * ca // 1000 + self.port(i, race) * cp // 1000
        if (co, ch) != (0, 0):
            v += (co * lay["ore"][i] + ch * self.hgt(i, rid)) // 1000
        if ci > 0:
            cap = self.t.get("inland_cap", 20)
            v += ci * min(cap, self.sea_d()[i]) * 255 // (cap * 1000)
        return v

    def candidates(self, rid, race):
        """[(value, index)] of the region's candidates, value descending, index ascending."""
        coast = self.dat.race[race]["coast"]
        key = (rid, coast, self.cell_record(race))
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
        self.homes_state = []      # (race, year, {region: owner}, [eligible regions in order]) at each awakening
        self.site_how = {}         # settlement -> seed / best / relaxed
        self.hunger = {}           # civ -> years
        self.hunger_max = 0
        self.before = {}           # settlement -> population before the first blow while thriving
        self.last_red = {}         # settlement -> last reduction event
        self.ruined_from = {}      # settlement -> civilization that lost it
        self.decline_start = {}    # r6 8.5: settlement -> the event that last turned it from thriving to declining
        self.plague_plans = []     # r6 10.2: (outbreak event, planned total, actual total) for the checks
        self.last_colony = {}      # r12 22.2 item 3: settlement -> its last colony_founded event as origin
        self.first_colony_pending = set()
        self.colony_log = []                # (colony event, founder, source, source pressure, founder pending)   # r17 23.2 item 2: civilizations whose first colony is freed from pressure
        self.low_until = {}        # r12 22.7a: civilization -> the year it stops lying low (latest call wins)
        self.low_refuge = {}       # r15 22.7a: civilization -> the refuge h of its latest lie_low call
        self.cores = set()         # P3-02 r10 12.2: K, the cores of the civilization and settlement names drawn
        self.extra_draw = extra_draw
        self.keys = {s: cr.stream_key(inp.seed, s) for s in STREAMS}
        self.intensity = self.t["intensity"][inp.params["intensity"]]
        self.race_key = {r[0]: r[1] for r in inp.races}
        self.checks = []           # in-run invariant failures (14 items 3, 4, 5)
        # P5-02 7.1, 7.2: the WildView (set_wild; None: growth 1000, nothing shunned) and the ghost log
        self.wild = None
        self.ghost_log = []        # (year, settlement, strike event, abandonment event)
        self.shun_log = []         # (year, civ, region) of every candidate left out because it is shunned
        # P5-03 7.2: the people ledger (set_ledger; None: no call); 6.4's records of the ruin procedure
        self.ledger = None
        self.road_log = []         # (year, settlement, P, road, settled, kind) of every abandonment (11.1)

    # --- P5-03 7.2, 11.1, 11.2
    def set_ledger(self, ledger):
        self.ledger = ledger

    def in_reach(self, s, t):
        """11.1: hops(region(s))[region(t)] <= abandon_reach_hops (always, before 0.24.0)."""
        if not FLAGS["p503"]:
            return True
        h = self.geo.hops(s["region"]).get(t["region"])
        return h is not None and h <= self.t["abandon_reach_hops"]

    def open_site_at(self, sid):
        """11.2 (b): an open site (any kind) has the settlement sid (through the monsters, when installed)."""
        m = getattr(self, "monsters", None)
        # P5-04 7.16: a mine (kind 3) does not hold back a ruin's fading
        return m is not None and any(x["settlement"] == sid and x["ending"] == 0 and x["kind"] != 3 for x in m.sites)

    def ending_now(self, s):
        """P5-02 8.3 (with 11.3's code 6) of a ruined settlement, from its ruin event."""
        e = s["ruin_event"]
        return pe.settlement_ending(s["status"], e, lambda x: self.events[x - 1]["key"],
                                    lambda x: self.events[x - 1]["causes"])

    # --- names (P3-02 4.3; r10 12.2 from 0.19.0: K, the cores of the history stage's names)
    def hist_name(self, naming, culture_id, kind, cnum, entity):
        if FLAGS["cores"]:
            return pnr.make_name(naming, culture_id, kind, self.inp.seed, cnum, entity, self.registry,
                                 cores=self.cores)
        return pnr.make_name(naming, culture_id, kind, self.inp.seed, cnum, entity, self.registry)

    # --- r12 22.7a: the hook P3-06's flight calls (beside note_reduction and ruin)
    def lie_low(self, civ, settlement, until_year):
        """PeoplesSubsystem::lie_low (r15 22.7a): records the latest (refuge, until) of civ (a later call replaces
        it)."""
        self.low_until[civ] = until_year
        self.low_refuge[civ] = settlement

    def lying_low(self, cid, year=None):
        """r15 22.7a, P3-06 r17 22.3: the one predicate. True when cid has a record, year < until, and the refuge
        h is standing and owned by cid (read in State at the call: changes of status and owner are immediate,
        P3-04 6.2). year: the step's year (a quiet step's start), default self.year."""
        if not FLAGS["lie_low"] or cid not in self.low_until:
            return False
        y = self.year if year is None else year
        if not (y < self.low_until[cid]):
            return False
        h = self.stl(self.low_refuge[cid])
        return self.standing(h) and h["owner_civ"] == cid

    # --- r12 22.2: the cause memory
    def recent(self, e):
        """An event id e (0 or None: none) cited by an event written now: only when Y - year(e) <=
        cause_memory_years (from 0.19.0; always before)."""
        if not e:
            return False
        # P3-05 r13: a hand-made tuning without the field has the rule off (100000)
        return not FLAGS["memory"] or self.year - self.events[e - 1]["year"] <= self.t.get("cause_memory_years", 100000)

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
        if self.quiet_used is not None and kind_key not in ("quiet_era", "quiet_stretch"):
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
                                     created=self.serial, owner_event=event))
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
            if FLAGS["auto_size"]:
                # P3-05 r8 21 item 5 (0.18.0): one more realm per regions_per_extra_civ non-islet regions,
                # at most civs_extra_max more, before the free-land check
                n_land = sum(1 for rid in self.geo.ids if not self.geo.islet(rid))
                n += min(self.t["civs_extra_max"], n_land // self.t["regions_per_extra_civ"])
            if len(el) < self.t["regions_per_civ"] * alive + 2:
                n = 1
        else:
            n = setting
        self.homes_state.append((race, self.year, {rid: self.owner[rid][0] for rid in self.geo.ids}, list(el)))
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
            n_c, t_c = self.hist_name(naming, cid, "civilization", cnum, c)
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
            n_s, t_s = self.hist_name(naming, cid, "settlement", cnum, s)
            pop = self.awakening_population(race_id, h)
            e_set = self.append("settlement_founded", region=h, settlement=s,
                                participants=(("settlement", SETTLEMENT, s), ("founder", CIV, c)), values=(pop,),
                                causes=(e_civ,))
            self.found_settlement(x, y, h, c, self.founding_kind(race_id, h, pop, x, y), pop, n_s, e_set)
            self.civ(c)["capital"] = s
            self.site_how[s] = how
            self.name_attempts.append((c, t_c, s, t_s, how))
            if FLAGS["awaken_pop"] and self.pd.race[race_id].get("awakening", 0) > 0:
                self.first_colony_pending.add(c)    # r17 23.2 item 2: the mark
        return e_aw

    def awakening_population(self, race_id, h):
        """P3-05 r16 23.2: P0 of the first settlement of a civilization placed at an awakening in home h."""
        fp = self.t["founding_population"]
        ap = self.pd.race[race_id].get("awakening", 0)
        if not FLAGS["awaken_pop"] or ap == 0:
            return fp
        return max(fp, min(ap, self.geo.capacity(h, race_id)))

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

    def set_wild(self, view):
        """P5-02 7.1: PeoplesSubsystem::set_wild(const WildView*)."""
        self.wild = view

    def wild_growth(self, rid):
        """G of P5-02 7.1: the view's growth_permille(region), 1000 without a view."""
        return self.wild.growth_permille(rid) if self.wild is not None else 1000

    def shunned(self, rid):
        return self.wild is not None and self.wild.shunned(rid)

    def grown(self, s, cs):
        """9.1: one year of growth or decline of s with share cs (r6: the target T_s = min(C_s, kind_cap)).
        P5-02 7.1: the rate times G / 1000, d = (P g G (C - P) + 500 1000 C) div (1000 1000 C)."""
        p = s["population"]
        g = self.pd.race[self.race_of(s)]["growth"]
        G = self.wild_growth(s["region"]) if self.wild is not None else 1000
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
                d = (p * g * G * (cs - p) + 500 * 1000 * cs) // (1000 * 1000 * cs)
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
            if self.standing(s) and s["population"] < self.t["abandon_min"] and self.visible(s) and \
                    not self.race_refuge(s):      # 21 item 6: a race's last refuge may stay below abandon_min
                if self.quiet_used is None:
                    self.checks.append("14 item 3: settlement %d below abandon_min after T1 in %d" % (s["id"], self.year))

    def abandon_b(self):
        for s in self.settlements:
            if not self.standing(s) or not self.visible(s) or s["population"] >= self.t["abandon_min"]:
                continue
            if self.race_refuge(s):
                continue                          # 21 item 6
            cause = self.last_red.get(s["id"], s["founding_event"])
            if FLAGS["memory"] and not self.recent(self.last_red.get(s["id"])):
                cause = s["founding_event"]       # r12 22.2 item 4 (b): an old reduction is none
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
                if FLAGS["memory"]:
                    # r12 22.2 item 3: the last reduction within the memory, else the last colony it sent
                    # within the memory, else no cause
                    if not self.recent(lr):
                        lr = self.last_colony.get(s["id"])
                        if not self.recent(lr):
                            lr = None
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
        reach = [t for t in others if self.in_reach(s, t)]   # P5-03 11.1
        settled = 0
        if reach:
            t = min(reach, key=lambda t: ((t["x"] - s["x"]) ** 2 + (t["y"] - s["y"]) ** 2, t["id"]))
            room = max(0, 2 * self.geo.capacity(t["region"], race) - t["population"])
            settled = min(p, room)
            t["population"] += settled
            if self.ledger is not None and settled >= 1:   # P5-03 7.2
                self.ledger.moved(sid, t["id"], settled)
        if self.ledger is not None and p - settled >= 1:
            self.ledger.displaced(sid, 0, p - settled, e)
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
        others = [t for t in self.standing_of(c) if t["id"] != s["id"] and self.in_reach(s, t)]   # P5-03 11.1
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
            if not self.in_reach(s, u):   # P5-03 11.1
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

    def abandon(self, sid, cause, causes_given=None):
        """8.5 (P5-03 9.4: public; causes_given in place of the blow's)."""
        s = self.stl(sid)
        c = s["owner_civ"]
        causes = (cause,) if causes_given is None else tuple(causes_given)
        cite_d = FLAGS["causes"] and s["status"] == DECLINING and sid in self.decline_start and \
            (not FLAGS["memory"] or self.recent(self.decline_start[sid]))   # r12 22.2 item 2
        if cite_d:
            # r6 8.5 causes: the blow (or last reduction / founding) and the decline start, ascending, no repeats
            causes = tuple(sorted(set(causes) | {self.decline_start[sid]}))
        p = s["population"]
        road = 0
        if FLAGS["p503"]:   # 11.1: the losses on the road, taken off before the ruin
            road = road_loss_of(p, self.draw("hist.peoples.abandon_loss", sid, 0, 0), self.t)
            s["population"] = p - road
        t, settled = self.destination(s) if FLAGS["dest"] else (None, 0)
        if FLAGS["dest"]:
            self.check_destination(s, t, settled)
        if cite_d and self.decline_start[sid] not in causes:
            self.checks.append("14 item 12 (r6): abandonment of %d without its decline start" % sid)
        if settled >= 1:
            e = self.append("settlement_abandoned_to", region=s["region"], settlement=sid,
                            participants=(("settlement", SETTLEMENT, sid), ("civilization", CIV, c),
                                          ("destination", SETTLEMENT, t["id"])),
                            values=(settled, p - settled), causes=causes)
        else:
            e = self.append("settlement_abandoned", region=s["region"], settlement=sid,
                            participants=(("settlement", SETTLEMENT, sid), ("civilization", CIV, c)),
                            values=(p - road,), causes=causes)
        if FLAGS["p503"]:
            self.road_log.append((self.year, sid, p, road, settled, "31" if settled >= 1 else "16"))
        self.ruin(sid, e)
        if not self.standing_of(c):
            e2 = self.append("civ_ended", region=s["region"], settlement=sid,
                             participants=(("civilization", CIV, c),), causes=(e,))
            self.end_civ(c, e2)
        return e

    def end_civ(self, c, e):
        self.first_colony_pending.discard(c)   # r17 23.2 item 2: the mark clears when the civilization ends
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
            if self.lying_low(cid):
                continue                          # r12 22.7a: before item 1; no draw, hunger unchanged
            race = c["race"]
            rd = self.pd.race[race]
            mine = [s for s in self.standing_of(cid, include_new=False)]
            press = {}
            for s in mine:
                p = s["population"]
                press[s["id"]] = max(p * 1000 // sh[s["id"]], p * 1000 // self.kind_cap(s))
            ready = [s for s in mine if press[s["id"]] >= t["found_pressure_permille"] or
                     (cid in self.first_colony_pending and s["id"] == c["capital"])]   # r17 23.2 item 2
            if not ready:
                continue
            src = max(ready, key=lambda s: (c["capital"] != s["id"], press[s["id"]], s["population"], -s["id"]))
            inside = None
            r_src = src["region"]
            if self.owner[r_src][0] == cid and self.shunned(r_src):
                self.shun_log.append((self.year, cid, r_src))   # P5-02 7.2: no inside target in a shunned region
            elif self.owner[r_src][0] == cid:
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
                    if self.shunned(r1):   # P5-02 7.2: no outward target R1 in a shunned region (READINGS P1)
                        self.shun_log.append((self.year, cid, r1))
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
            n_s, _t = self.hist_name(self.dat.naming[cul], cul, "settlement", cnum, s)
            was_free = self.owner[rid][0] == 0
            e = self.append("colony_founded", region=rid, settlement=s,
                            participants=(("settlement", SETTLEMENT, s), ("founder", CIV, cid),
                                          ("origin", SETTLEMENT, src["id"])), values=(n,),
                            salience=(t["colony_new_region_salience"] if was_free else None))
            self.found_settlement(x, y, rid, cid, self.founding_kind(race, rid, n, x, y), n, n_s, e)
            self.last_colony[src["id"]] = e       # r12 22.2 item 3
            was_pending = cid in self.first_colony_pending
            self.colony_log.append((e, cid, src["id"], press[src["id"]], was_pending))
            self.first_colony_pending.discard(cid)  # r17 23.2 item 2: the mark clears at the first colony
            self.site_how[s] = how
            if was_free:
                self.set_owner(rid, cid, e)

    # --- 10 pressures
    def strike(self, s, kind, toll, causes, salience=None, extra=None):
        """extra(dead): the values after (dead, population), for kind 37 (r12 22.1: total, reached)."""
        p = s["population"]
        dead = p * toll // 1000
        was_declining = s["status"] == DECLINING
        if s["status"] == THRIVING:
            self.before[s["id"]] = p
        s["population"] = p - dead
        c = s["owner_civ"]
        e = self.append(kind, region=s["region"], settlement=s["id"],
                        participants=(("settlement", SETTLEMENT, s["id"]), ("civilization", CIV, c)),
                        values=(dead, s["population"]) + (extra(dead) if extra else ()), causes=causes,
                        salience=salience)
        if not was_declining:
            self.decline_start[s["id"]] = e      # r6 8.5: the blow that found it thriving
        s["status"] = DECLINING
        self.last_red[s["id"]] = e
        # 8.5 (a)
        t = self.t
        p2 = s["population"]
        if self.race_refuge(s):
            pass                                  # 21 item 6: never abandoned, by the rule or by chance
        elif p2 < t["abandon_min"]:
            self.abandon(s["id"], e)
        elif was_declining and p2 < t["abandon_frail_max"] and self.chance_abandon_allowed(s):
            if self.abandon_draw(s):
                self.abandon(s["id"], e)
        if FLAGS["ghost"] and kind in ("famine", "plague_outbreak", "plague_spread") and self.standing(s) and \
                self.ghost_eligible(s, s["population"]) and self.ghost_draw(s):
            a = self.abandon(s["id"], e)   # P5-02 8.2: 8.5's procedure, causes the strike and its decline start
            self.ghost_log.append((self.year, s["id"], e, a))
        return e

    def ghost_eligible(self, s, pop, gone=()):
        """P5-02 8.2's conditions but the draw: kind hamlet or village, P < ghost_max_population, not its owner's
        capital, its owner has another standing settlement (those of the step included; `gone` the plan's)."""
        t = self.t
        c = s["owner_civ"]
        if s["kind"] not in (HAMLET, VILLAGE) or not pop < t.get("ghost_max_population", 0) or c == 0:
            return False
        if self.civ(c)["capital"] == s["id"]:
            return False
        return any(x["id"] != s["id"] and x["id"] not in gone for x in self.standing_of(c))

    def ghost_draw(self, s):
        return chance_pm(self.draw("hist.peoples.ghost", s["id"], 0, 0), self.t.get("ghost_permille", 0))

    def race_refuge(self, s):
        """21 item 6: s is its race's last refuge (its civilization's only standing settlement, and that
        civilization the only alive one of its race)."""
        if not FLAGS["refuge_floor"] or not self.standing(s) or s["owner_civ"] == 0:
            return False
        c = s["owner_civ"]
        return self.race_last_refuge(c) and [x["id"] for x in self.standing_of(c)] == [s["id"]]

    def race_last_refuge(self, civ):
        """r12 22.7 (P3-09 audit A2): the one last-refuge predicate: civ has exactly one standing settlement and
        no other civilization of its race is alive (ended_year from the live state)."""
        if civ == 0:
            return False
        race = self.civ(civ)["race"]
        return len(self.standing_of(civ)) == 1 and \
            [x["id"] for x in self.civs if x["ended_year"] is None and x["race"] == race] == [civ]

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
                toll = t["plague_toll_min"] + pnr.pick(self.draw("hist.peoples.plague_toll", sid, 0, 0), tr_p)
                self.outbreak(s, toll, struck, nset, tr_p)
        ncivs = len(self.civs)
        for cid in range(1, ncivs + 1):
            c = self.civ(cid)
            if c["ended_year"] is not None or not self.visible(c):
                continue
            rate = t["crisis_per_generation_permille"] * 1000 // self.pd.race[c["race"]]["generation"] * I // 1000
            if not chance_ppm(self.draw("hist.peoples.crisis", cid, 0, 0), rate):
                continue
            self.crisis(cid)

    def crisis(self, cid):
        """10.3 for civilization cid once the crisis chance has hit; returns the crisis event."""
        t = self.t
        c = self.civ(cid)
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
            return e
        p = cap["population"]
        if cap["status"] == THRIVING:
            self.before[cap["id"]] = p
            self.decline_start[cap["id"]] = e     # r6 8.5: a crisis loss that found it thriving
        cap["population"] = p - p * t["crisis_loss_permille"] // 1000
        cap["status"] = DECLINING
        self.last_red[cap["id"]] = e
        return e

    def outbreak(self, s, toll, struck, nset, tr_p):
        """10.2 item 2 from the outbreak's toll on: the strike of s (salience by r6's plan), then the spreads.
        Returns the outbreak's event."""
        t = self.t
        sid = s["id"]
        owner = s["owner_civ"]          # READING (SPEC_GAPS 10.2)
        small = s["kind"] in (HAMLET, VILLAGE)
        if FLAGS["plague_sal"]:
            planned = self.plague_plan(s, toll, struck, nset, tr_p)
            sal = None if planned >= t["plague_major_dead"] else t["plague_small_salience"]
        else:
            sal = t["plague_small_salience"] if small else None
        if FLAGS["outbreak37"]:
            # r12 22.1: kind 37 with the plan's total and the number of planned spreads
            reached = self._plan_reached
            o = self.strike(s, "plague_outbreak", toll, (), salience=sal,
                            extra=lambda dead: (planned, reached))
        else:
            o = self.strike(s, "plague", toll, (), salience=sal)
        actual = self.events[o - 1]["values"][1]
        n_spread = 0
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
                n_spread += 1
                struck.add(uid)
        if FLAGS["plague_sal"]:
            self.plague_plans.append((o, planned, actual))
        if FLAGS["outbreak37"] and n_spread != self._plan_reached:
            self.checks.append("22.1: outbreak e%d reached %d planned %d" % (o, n_spread, self._plan_reached))
        return o

    def plague_plan(self, s, toll, struck, nset, tr_p):
        """r6 10.2: the outbreak planned before it is written; returns total = dead_0 + sum of dead_u over the
        planned spreads (none if s is emptied). No state changes."""
        t = self.t
        p = s["population"]
        dead0 = p * toll // 1000
        rest = p - dead0
        self._plan_reached = 0
        emptied = rest < t["abandon_min"] and not self.race_refuge(s)
        if not emptied and not self.race_refuge(s) and s["status"] == DECLINING and rest < t["abandon_frail_max"] and \
                self.chance_abandon_allowed(s):
            emptied = self.abandon_draw(s)
        if not emptied and FLAGS["ghost"] and FLAGS["outbreak37"] and self.ghost_eligible(s, rest):
            emptied = self.ghost_draw(s)   # P5-02 8.2: the plan includes the ghost test with the same draw
        total = dead0
        if emptied:
            return total
        # The lead's ruling (P3-08 integration): "no state changes between the plan and the strikes" does not
        # hold: a spread strike can abandon its target (8.5 (a)), whose survivors join the owner's nearest
        # other standing settlement (8.5 item 2), which may be a later target. The plan replays the strikes
        # in order on a copy of the populations, with that transfer and the keyed draws the strikes use, so
        # the planned toll equals the real toll (14 item 13).
        owner = s["owner_civ"]
        sid = s["id"]
        race = self.civ(owner)["race"]
        pop = {u["id"]: u["population"] for u in self.settlements}
        pop[sid] = rest
        gone = set()
        area = {s["region"]} | {r for r, _c in self.geo.nb[s["region"]]}
        for uid in range(1, nset + 1):
            u = self.stl(uid)
            if uid == sid or uid in struck or uid in gone or not self.standing(u) or not self.visible(u):
                continue
            if u["owner_civ"] != owner or u["region"] not in area:
                continue
            if not chance_pm(self.draw("hist.peoples.plague_spread", sid, uid, 0), t["plague_spread_permille"]):
                continue
            tu = (t["plague_toll_min"] + pnr.pick(self.draw("hist.peoples.plague_toll", uid, 1, 0), tr_p)) \
                * t["plague_spread_toll_permille"] // 1000
            dead = pop[uid] * tu // 1000
            total += dead
            self._plan_reached += 1
            pop[uid] -= dead
            # 8.5 (a) for u (the origin stands, so u is never its owner's last settlement)
            left = pop[uid] < t["abandon_min"]
            if not left and u["status"] == DECLINING and pop[uid] < t["abandon_frail_max"]:
                left = self.abandon_draw(u)
            if not left and FLAGS["ghost"] and self.ghost_eligible(u, pop[uid], gone):
                left = self.ghost_draw(u)   # P5-02 8.2 in the plan
            if left:
                # P5-03 11.1 in the plan as in the abandonment (integration: the plan equals the real toll): the
                # destination within abandon_reach_hops and the survivors less the losses on the road
                others = [x for x in self.standing_of(owner) if x["id"] != uid and x["id"] not in gone
                          and self.in_reach(u, x)]
                if others:
                    d = min(others, key=lambda x: ((x["x"] - u["x"]) ** 2 + (x["y"] - u["y"]) ** 2, x["id"]))
                    room = max(0, 2 * self.geo.capacity(d["region"], race) - pop[d["id"]])
                    road = road_loss_of(pop[uid], self.draw("hist.peoples.abandon_loss", uid, 0, 0), t) \
                        if FLAGS["p503"] else 0
                    pop[d["id"]] += min(pop[uid] - road, room)
                pop[uid] = 0
                gone.add(uid)
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
        # P5-06 7.4: the split bonus of a short reign (FigureView::split_bonus_permille; none without the view)
        fv = getattr(self, "figures", None)
        bonus = fv.split_bonus_permille(cid) if fv is not None else 0
        if not chance_pm(self.draw("hist.peoples.split", cid, 0, 0), min(1000, t["split_permille"] + bonus)):
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
        name, _t = self.hist_name(self.dat.naming[cul], cul, "civilization", cnum, c2)
        e2 = self.append("civ_seceded", region=newcap["region"], settlement=newcap["id"],
                         participants=(("civilization", CIV, c2), ("from", CIV, cid)), values=(len(leaving),),
                         causes=(e,))
        self.found_civilization(c["race"], cnum, newcap["region"], name, e2)
        self.civ(c2)["capital"] = newcap["id"]
        for s in leaving:
            s["owner_civ"] = c2
            s["owner_event"] = e2   # P5-03 7.3
        for r in part:
            self.set_owner(r, c2, e2)
        return True

    # --- 8.6 Q3
    def q3(self):
        self.begin()
        if FLAGS.get("nofade"):   # P5-06 25.1 (a): 8.6 removed; Q3 does nothing
            return
        for s in self.settlements:
            if s["status"] != RUINED or self.year - s["ruined_year"] < self.t["ruin_fade_years"]:
                continue
            if FLAGS["p503"]:   # 11.2 (a) and (b)
                gf = self.t["ghost_fade_years"]
                if not (self.ending_now(s) == 1 or (gf > 0 and self.year - s["ruined_year"] >= gf)):
                    continue
                if self.open_site_at(s["id"]):
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
        self.quiet_eras(start, last, eng_run.assured(start, length, phase_end))
        self.bookkeeping(start, phase_end, last)
        if phase_end:
            self.open_era = {}
        self.check_state()

    def quiet_eras(self, start, last, assured=None):
        """P3-04 6.6 (Q6). r10 (FLAGS["quiet"]): an event charged to the civilization does not close its era,
        so it has one era per quiet phase row; before r10 a charged civilization's era was closed. From 0.19.0
        (P3-04 r12 20.1, pe.QUIET_R12) one world-level quiet_stretch per quiet phase row instead."""
        if pe.QUIET_R12:
            self.quiet_stretch(start, last, assured=assured)
            return
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
    res = rows_of(w, cat, en.phases)
    res["_world"] = w
    res["_engine"] = en
    res["_catalogue"] = cat
    return res


def rows_of(w, cat, phases):
    return rows_of_023(w, cat, phases)


def rows_of_022(w, cat, phases):
    """The thirteen history tables of a world's state (P3-04 9)."""
    return dict(
        event_kind=[(k["id"], k["key"], k["caused"], k["region_use"], k["settlement_use"], k["salience"], k["templ"])
                    for k in cat],
        event_kind_role=[(k["id"], r["seq"], r["role"], r["type"], r["min"], r["max"]) for k in cat
                         for r in k["roles"]],
        event_kind_value=[(k["id"], i + 1, v) for k in cat for i, v in enumerate(k["values"])],
        phase=[tuple(r) for r in phases],
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
    ) if True else None


def rows_of_023(w, cat, phases):
    """rows_of with P5-02's settlement.ending and the tables beast and monster_kind (11.2)."""
    res = rows_of_022(w, cat, phases)
    if pe.RENDER_R23:
        res["beast"], res["monster_kind"] = pe.beast_rows(w.dat), pe.monster_kind_rows(w.dat)
    return pe.with_ending(res)


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

def auto_max(w):
    """The most civilizations `auto` founds for one race (7.2; P3-05 r8 21 item 5 from 0.18.0)."""
    extra = 0
    if FLAGS["auto_size"]:
        n_land = sum(1 for rid in w.geo.ids if not w.geo.islet(rid))
        extra = min(w.t["civs_extra_max"], n_land // w.t["regions_per_extra_civ"])
    return 2 + extra


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
             len(founded) <= (auto_max(w) if setting is None else setting))
        for x in founded:
            c.true("%s inv 1 founded at the awakening" % label, x[6] == a[2] and kind[causes[x[7]][0]] == 1)
    # 2
    cells = {}
    faded_at = {}
    for e in res["event"]:
        if e[3] == KIND_ID["ruin_faded"]:
            faded_at[e[6]] = e[0]
    for s in res["settlement"]:
        # r8 21 item 3 (0.18.0): or refugees_settled (P3-06 kind 32)
        c.true("%s inv 2 s%d founding kind" % (label, s[0]), kind[s[7]] in (KIND_ID["settlement_founded"],
                                                                          KIND_ID["colony_founded"],
                                                                          KIND_ID["refugees_settled"]))
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
    blows = ("famine", "plague", "plague_spread", "succession_crisis", "plague_outbreak")
    ab = ("settlement_abandoned", "settlement_abandoned_to")
    want_cause = {
        "became_village": None, "became_town": None, "became_city": None,
        "plague_spread": ("plague", "plague_outbreak"),
        "civ_seceded": ("succession_crisis",), "settlement_abandoned": blows + ("settlement_founded", "colony_founded"),
        "settlement_abandoned_to": blows + ("settlement_founded", "colony_founded"),
        "capital_moved": ab, "civ_ended": ab, "ruin_faded": ab,
        "settlement_dwindled": blows + (("colony_founded",) if FLAGS["memory"] else ()),   # r12 22.2 item 3
    }
    keys = {k["id"]: k["key"] for k in tuple(COMPILED) + tuple(pe.KINDS_026)}   # P5-04: kinds 63 to 78 named
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
    if FLAGS["outbreak37"]:
        check_r12_invariants(c, res, w, label, kind, ev, vals, causes, base)
    if FLAGS["crisis_sal"]:
        split = {x for e in res["event"] if e[3] == KIND_ID["civ_seceded"] for x in causes.get(e[0], [])}
        for e in res["event"]:
            if e[3] == KIND_ID["succession_crisis"]:
                want = base[KIND_ID["succession_crisis"]] if e[0] in split else t["crisis_small_salience"]
                c.true("%s inv 13 e%d crisis salience" % (label, e[0]), e[7] == want)


def check_r12_invariants(c, res, w, label, kind, ev, vals, causes, base):
    """P3-05 r12 22 from the rows: no kind 12 or kind 4 event; kind 37's total equals its own dead plus its
    spreads' (invariant 13 with total), reached their number, salience by total; every cited reduction,
    decline start or colony of a dwindling or an abandonment within cause_memory_years (22.2; a founding
    event as the abandonment's fallback is exempt)."""
    t = w.t
    c.eq("%s 22.1 no kind 12 event" % label, [e[0] for e in res["event"] if e[3] == KIND_ID["plague"]], [])
    c.eq("%s 20.1 no kind 4 event" % label, [e[0] for e in res["event"] if e[3] == pe.find_kind(pe.COMPILED,
                                                                                                "quiet_era")["id"]], [])
    k37, k13 = KIND_ID["plague_outbreak"], KIND_ID["plague_spread"]
    spread, n = {}, {}
    for e in res["event"]:
        if e[3] == k13:
            for x in causes.get(e[0], []):
                spread[x] = spread.get(x, 0) + vals[e[0]][1]
                n[x] = n.get(x, 0) + 1
    for e in res["event"]:
        if e[3] == k37:
            total = vals[e[0]][1] + spread.get(e[0], 0)
            want = base[k37] if total >= t["plague_major_dead"] else t["plague_small_salience"]
            c.true("%s inv 13 e%d kind 37 total %d = %d, reached %d = %d, salience %d" %
                   (label, e[0], vals[e[0]][3], total, vals[e[0]][4], n.get(e[0], 0), e[7]),
                   vals[e[0]][3] == total and vals[e[0]][4] == n.get(e[0], 0) and e[7] == want)
    mem = t["cause_memory_years"]
    reds = {"famine", "plague", "plague_outbreak", "plague_spread", "succession_crisis", "battle", "battle_at_pass",
            "tribute", "colony_founded", "allied_battle",   # allied_battle: P5-04 11.1 (no older world has it)
            "battle_led", "blight"}   # P5-06 11.1, 6.8 (no older world has them)
    keys = {k["id"]: k["key"] for k in tuple(COMPILED) + tuple(pe.KINDS_026) + tuple(pe.KINDS_027)}   # P5-04, P5-06
    for e in res["event"]:
        if keys[e[3]] in ("settlement_dwindled", "settlement_abandoned", "settlement_abandoned_to"):
            for x in causes.get(e[0], []):
                if keys[ev[x][3]] in reds and x != e[0]:
                    c.true("%s 22.2 e%d cites e%d of %d years ago" % (label, e[0], x, e[1] - ev[x][1]),
                           e[1] - ev[x][1] <= mem)


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
    if name in pe.P1_06_CASE and FLAGS["dest"]:   # the 0.16.0 inputs (--legacy) predate P1-06 r3
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


def dwarf_kinds_line(name, res):
    """P3-10 4 item 2: the dwarven settlements of a case (founder's race dwarf) by their first census kind."""
    dwarf = [r[0] for r in res["_inputs"].races if r[1] == "dwarf"]
    civ_race = {c[0]: c[2] for c in res["civilization"]}
    first = {}
    for r in res["settlement_census"]:
        if r[0] not in first or r[1] < first[r[0]][0]:
            first[r[0]] = (r[1], r[3])
    forts = ladder = 0
    for x in res["settlement"]:
        if civ_race[x[5]] in dwarf:
            if first[x[0]][1] == FORTRESS:
                forts += 1
            else:
                ladder += 1
    return "%s dwarf_kinds forts %d ladder %d" % (name, forts, ladder)


HEADER_019 = """\
# Generator 0.19.0 (P3-10; P3-05 r12 22, P3-04 r12 20, P3-02 r10 12.2, P1-06 r7 region names of the input
# worlds): kind 37 plague_outbreak for every outbreak, the cause memory, the site value with inland, port
# and habitability, lie_low, one world-level quiet_stretch per quiet phase row, separators in sentences,
# the cause clause of kinds 10, 16 and 31, the name core rule; the catalogue of 38 kinds. New line:
#   <case> dwarf_kinds forts <n> ladder <m>   (hist-peoples-42-1024 only; P3-10 4 item 2) the dwarven
#       settlements (founder's race dwarf) by their first census kind: fortress, or any ladder kind
"""

HEADER_021 = """\
# Generator 0.21.0 (P5-01; P3-05 r18 24, P3-04 r19 22, P3-02 r17 15, P1-06 r10 16): became_city's template
# names the region and drops the figure ({settlement} in {@region} grew into a city.); the numbers of one
# clause of a template all words or all digits; no flare-up shorter than the scaled flare_min_years; the
# dwarven forbidden didin and ririn and the r17 filter entries in names; P1-06 r10 region names (the inputs);
# the catalogue of 42 kinds.
"""

HEADER_020 = """\
# Generator 0.20.0 (P3-11; P3-05 r16 23, P3-04 r17 21, P3-02 r15 14): the first settlement of a civilization
# placed at an awakening starts at max(founding_population, min(awakening_population, C(H, r))) (orc 1200,
# the other races 0: founding_population), and (r17 23.2 item 2) such a civilization's capital is ready for its
# first colony whatever its pressure; values one to nine as words in sentences; the faerie lists, the
# repeated-syllable rule and the r15 filter entries in names; the catalogue of 40 kinds.
"""

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


HEADER_022 = """\
# Generator 0.22.0 (P5-12, docs/specs/p5-12-renamed-regions.md): no peoples rule changes; the catalogue gains
# kind 43 region_renamed (P3-04 r21 23.2), table region_name (schema 8; always empty here: the renaming
# subsystem is installed after conflict, so the peoples-only configuration has none), P3-02 r19 names (no
# doubled place suffix, human forbidden ass), P1-06 r11 region names (V8; the inputs:
# ~/.cache/the-unmapped/out/p5-12-tests/inputs) and the history.renaming tuning in the data roots.
"""


def golden_text(inputs):
    dat = data()
    o = [HEADER.rstrip("\n")]
    if FLAGS["outbreak37"]:
        o.append(HEADER_019.rstrip("\n"))
    if FLAGS["awaken_pop"]:
        o.append(HEADER_020.rstrip("\n"))
    if pe.at_least("0.21.0"):
        o.append(HEADER_021.rstrip("\n"))
    if pe.at_least("0.22.0"):
        o.append(HEADER_022.rstrip("\n"))
    for name, seed, size, span, races, detail in WORLD_CASES:
        res = world_case(name, seed, size, span, races, detail, inputs, dat)
        o.append("world %s %d %d %d %s %s" % (name, seed, size, span, races, detail))
        o += case_lines(name, res, size <= 512)
        if FLAGS["outbreak37"] and name == "hist-peoples-42-1024":
            o.append(dwarf_kinds_line(name, res))
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
    keys = {k["id"]: k["key"] for k in tuple(COMPILED) + tuple(pe.KINDS_026)}   # P5-04: kinds 63 to 78 named
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
    keys = {k["id"]: k["key"] for k in tuple(COMPILED) + tuple(pe.KINDS_026)}   # P5-04: kinds 63 to 78 named
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
    # weight x share 1000 div 1000: fen 800 at 0.16.0, 300 from 0.17.0 (12.3, r6)
    c.eq("16.1.1 fen-only region for faeries: B", b_, 300 if FLAGS["dest"] else 800)
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
    if FLAGS["dest"]:
        check_units_r6(c)
    if FLAGS["outbreak37"]:
        check_units_r12(c)
    if FLAGS["awaken_pop"]:
        check_units_r16(c)


def awakening_firsts(res):
    """[(settlement id, region, population)] of the settlements founded at an awakening (the settlement_founded
    events caused by a civ_founded), in id order."""
    kind = {k[0]: k[1] for k in res["event_kind"]}
    ev = {e[0]: e for e in res["event"]}
    causes = {}
    for e, cc in res["event_cause"]:
        causes.setdefault(e, []).append(cc)
    vals = {}
    for e, sq, v in res["event_value"]:
        vals.setdefault(e, {})[sq] = v
    out = []
    for e in res["event"]:
        if kind[e[3]] == "settlement_founded" and any(kind[ev[x][3]] == "civ_founded" for x in causes.get(e[0], [])):
            out.append((e[6], e[5], vals[e[0]][1]))
    return out


def check_units_r16(c):
    """P3-05 r16 23.4 items 1 to 7 (the reference's own forms; the C++ tests are
    history/history_peoples_r16_test.cpp)."""
    ORC, ELF, GOBLIN = 6, 1, 2
    big = [dict(cells=4096, walk_cells=4000)]
    # items 1, 2, 4: the formula, with C(H, r) set by hand
    for cap, ap, fp, want in ((4700, 1200, 300, 1200), (900, 1200, 300, 900), (400, 1200, 300, 400),
                              (4700, 200, 300, 300), (4700, 0, 300, 300), (100000, 100000, 300, 100000),
                              (4700, 100000, 300, 4700), (300, 1200, 300, 300)):
        w = r6_world(races=("orc",), regions=big, peoples=dict(founding_population=fp),
                     race_fields={ORC: dict(awakening=ap)})
        w.geo._cap[(1, ORC)] = cap
        c.eq("23.2 P0 with C %d, awakening_population %d, founding %d" % (cap, ap, fp),
             w.awakening_population(ORC, 1), want)
    # r17 23.4 items 1 to 3, 6: the pending mark. One orc civilization, its capital a village of 1,200 alone in a
    # free region of C 4,700 with free neighbours (found_rate 1000: the chance always passes)
    regions = [dict(cells=4096, walk_cells=4000, x_min=0, x_max=31), dict(cells=4096, walk_cells=4000, x_min=32, x_max=63)]
    w = r6_world(races=("orc",), regions=regions, borders=[dict(a=1, b=2, walk_pairs=5, length_m=1000)],
                 race_fields={ORC: dict(awakening=1200)}, peoples=dict(found_rate_permille=1000))
    c.eq("23.4.1 ladder(orc, 1200) is a village", w.ladder(ORC, 1200), VILLAGE)
    civ = r6_civ(w, ORC)
    sid = r6_stl(w, civ, 1, VILLAGE, 1200, 10, 10)
    w.geo._cap[(1, ORC)] = 4700
    c.eq("23.4.1 (5.2) the orc village's kind cap is town's", w.kind_cap(w.stl(sid)), w.t["city_min"] - 1)
    sh = w.shares()[sid]
    press = max(1200 * 1000 // sh, 1200 * 1000 // w.kind_cap(w.stl(sid)))
    c.eq("23.4.1 pressure 255 (share 4,700), below 750", (sh, press), (4700, 255))
    draws = []
    real = w.draw
    w.draw = lambda stream, a, b, k, w=w, draws=draws, real=real: (draws.append((w.year, stream, a)) or
                                                                 real(stream, a, b, k))
    w.serial += 1
    w.foundings(quiet=False)
    c.eq("23.4.1 not pending: no found draw", [x for x in draws if x[1] == "hist.peoples.found"], [])
    w.first_colony_pending.add(civ)
    w.year += 1
    w.serial += 1
    w.foundings(quiet=False)
    col = [e for e in w.events if e["key"] == "colony_founded"]
    c.eq("23.4.1 pending: the found draw is taken and a colony of 144 founded",
         ([x[1] for x in draws if x[1] == "hist.peoples.found"], [e["values"] for e in col]),
         (["hist.peoples.found"], [{1: 144}]))
    c.eq("23.4.2 the mark clears at the colony", civ in w.first_colony_pending, False)
    c.eq("23.4.2 the capital keeps 1,056", w.stl(sid)["population"], 1056)
    n0 = len(draws)
    w.year += 1
    w.serial += 1
    w.foundings(quiet=False)
    c.eq("23.4.2 the next T2: not ready, no found draw", [x for x in draws[n0:] if x[1] == "hist.peoples.found"], [])
    # item 3: seed 1's case in miniature, P 1,219 with share 8,882: 152, ready only while pending
    c.eq("23.4.3 pressure 152", max(1219 * 1000 // 8882, 1219 * 1000 // (w.t["city_min"] - 1)), 152)
    # item 6: lying low comes first
    w2 = r6_world(races=("orc",), regions=regions, borders=[dict(a=1, b=2, walk_pairs=5, length_m=1000)],
                  race_fields={ORC: dict(awakening=1200)}, peoples=dict(found_rate_permille=1000))
    c2 = r6_civ(w2, ORC)
    s2 = r6_stl(w2, c2, 1, VILLAGE, 1200, 10, 10)
    w2.first_colony_pending.add(c2)
    w2.lie_low(c2, s2, w2.year + 100)
    w2.serial += 1
    w2.foundings(quiet=False)
    c.eq("23.4.6 lying low: skipped, still pending, no colony",
         (c2 in w2.first_colony_pending, [e for e in w2.events if e["key"] == "colony_founded"]), (True, []))
    # items 4, 5, 7 and the rule's reach on synthetic worlds: the field 0 gives the 0.19.0 rule byte for byte;
    # 1,200 moves only worlds with orcs; 100,000 keeps invariant 3 at every awakening
    dat = data()
    zero = edited(dat, races={r: dict(awakening=0) for r in dat.race})
    huge = edited(dat, races={ORC: dict(awakening=100000), ELF: dict(awakening=100000)})
    moved = 0
    for seed in SYN_SEEDS[:8]:
        inp, _g = synthetic_inputs(seed, dat)
        a = history_stage(dat, inp)
        b = history_stage(zero, inp)
        FLAGS["awaken_pop"] = False
        try:
            o = history_stage(dat, inp)
        finally:
            FLAGS["awaken_pop"] = True
        c.eq("23.4.4 syn-%d: every race at 0 = the 0.19.0 rule" % seed, table_hashes(b), table_hashes(o))
        orcs = any(r[1] == "orc" for r in inp.races) if inp.races and isinstance(inp.races[0], tuple) else None
        moved += table_hashes(a) != table_hashes(o)
        firsts = awakening_firsts(a)
        w = a["_world"]
        for sid, h, pop in firsts:
            race = w.civ(w.stl(sid)["founder_civ"])["race"]
            want = w.awakening_population(race, h)
            c.eq("23.2 syn-%d s%d P0" % (seed, sid), pop, want)
        for (e, cv, src, pr, pend) in w.colony_log:
            c.true("23.4.10 syn-%d e%d: ready by pressure or the first colony of a pending civilization" % (seed, e),
                   pr >= w.t["found_pressure_permille"] or pend)
        firsts_c = {}
        for (e, cv, src, pr, pend) in w.colony_log:
            firsts_c.setdefault(cv, (e, pend))
        c.true("23.4.10 syn-%d: only a civilization's first colony is pending" % seed,
               all(not pend or firsts_c[cv][0] == e for (e, cv, src, pr, pend) in w.colony_log))
        c.true("23.4.10 syn-%d: no civilization with a colony stays pending" % seed,
               not any(cv in w.first_colony_pending for (_e, cv, _s, _p, _pd) in w.colony_log))
        h = history_stage(huge, inp)
        hw = h["_world"]
        for sid, reg, pop in awakening_firsts(h):
            cap = hw.geo.capacity(reg, hw.civ(hw.stl(sid)["founder_civ"])["race"])
            c.true("23.4.7 syn-%d s%d: invariant 3 at the awakening (P %d <= 2 C %d)" % (seed, sid, pop, cap),
                   pop <= 2 * cap)
        _ = orcs
    c.true("23.2 the rule moves some synthetic world", moved > 0)


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
        if FLAGS["cap"]:          # r6 9.1: the target T_s = min(C_s, cap)
            ts = min(cs, cap)
            if p < ts:
                d = (p * gg * (cs - p) + 500 * cs) // (1000 * cs)
                if gg > 0:
                    d = max(1, d)
                return min(p + d, cap)
            if p > ts:
                return p - max(1, (p - ts) * 100 // 1000)
            return p
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
    if FLAGS["cap"]:
        c.eq("16.1.6 (r6) above the cap: drains toward it", step(1600, 100000, cap=1499), 1600 - 10)
    else:
        c.eq("16.1.6 above the cap: no growth, no loss", step(1600, 100000, cap=1499), 1600)
    sw = World(dat, inp, event_catalogue(dat.report))
    for p_, cs_ in ((300, 2000), (2000, 2000), (3000, 2000), (60, 100000)):
        sw.settlements = [dict(id=1, owner_civ=1, kind=VILLAGE, population=p_)]
        sw.civs = [dict(id=1, race=HUMAN)]
        c.eq("16.1.6 World.grown equals the formula (P %d, C_s %d)" % (p_, cs_), sw.grown(sw.settlements[0], cs_),
             step(p_, cs_, cap=sw.kind_cap(sw.settlements[0])))
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


# ---------------------------------------------------------------------------
# 16.1a: hand-made units of r6 (generator 0.17.0)
# ---------------------------------------------------------------------------

def r6_world(races=("elf",), regions=None, n=64, layers=None, peoples=None, race_fields=None, borders=()):
    """A hand-made World (16.1a): regions as dicts for unit_geo, serial and year set, no civilization."""
    dat, inp = unit_geo(regions or [dict()], borders=borders, races=races, n=n, layers=layers, peoples=peoples,
                        race_fields=race_fields)
    cat = event_catalogue(dat.report)
    w = World(dat, inp, cat)
    w.serial = 10
    w.year = 1000
    w.phase = 1
    w._cat = cat
    return w


def r6_civ(w, race, home=1):
    c = len(w.civs) + 1
    w.events.append(dict(id=len(w.events) + 1, year=0, end_year=0, kind=KIND_ID["civ_founded"], phase=1,
                         region=home, settlement=0, salience=80, participants=[(1, 1, CIV, c)], values={},
                         causes=[], key="civ_founded"))
    w.civs.append(dict(id=c, name="C%d" % c, race=race, culture=race, home_region=home, capital=0, founded_year=0,
                       founding_event=len(w.events), ended_year=None, ending_event=0, created=0))
    w.hunger[c] = 0
    return c


def r6_stl(w, civ, region, kind, pop, x, y, status=THRIVING, created=0, name=None):
    sid = len(w.settlements) + 1
    w.events.append(dict(id=len(w.events) + 1, year=0, end_year=0, kind=KIND_ID["settlement_founded"], phase=1,
                         region=region, settlement=sid, salience=40,
                         participants=[(1, 1, SETTLEMENT, sid), (2, 2, CIV, civ)], values={1: pop}, causes=[],
                         key="settlement_founded"))
    w.settlements.append(dict(id=sid, name=name or "S%d" % sid, x=x, y=y, region=region, founder_civ=civ,
                              founded_year=0, founding_event=len(w.events), owner_civ=civ, kind=kind, status=status,
                              population=pop, ruined_year=None, ruin_event=0, created=created))
    if w.owner[region][0] == 0:
        w.set_owner(region, civ, len(w.events))
    if w.civ(civ)["capital"] == 0:
        w.civ(civ)["capital"] = sid
    return sid


def r6_sentence(w, eid):
    res = rows_of(w, w._cat, [])
    view = pe.view_from_rows(res, [(r["id"], r["name"]) for r in w.inp.regions], w.inp.races, w.inp.gods)
    try:
        return pe.render(view, eid)
    except FatalError as e:      # an engine reference without P3-04 r10's three-word form
        return "render failed: %s" % e


def r6_vals(w, eid):
    e = w.events[eid - 1]
    return e["key"] if "key" in e else None, dict(e["values"])


def r6_kind(w, eid):
    return COMPILED[w.events[eid - 1]["kind"] - 1]["key"]


def check_units_r6(c):
    """16.1a items 17a-17i (P3-05 r6), with kind 12 for the outbreak (as written for 0.17.0 and 0.18.0)."""
    ELF, GOBLIN, DWARF, FAERIE, HUMAN = 1, 2, 3, 4, 5
    n = 64
    saved = dict(FLAGS)
    FLAGS["outbreak37"] = False
    FLAGS["sites19"] = False      # r6's site value (the shipped dwarf record of 0.19.0 has habitability 500)
    # --- 17a cell term (6.2)
    N = n * n
    lay = dict(region=[1] * N, water=[0] * N, habitability=[0] * N, river=[0] * N, flow_dir=[8] * N,
               elevation=[1] * N, ore=[0] * N, walk_area=[1] * N)
    ia = 1 * n + 40          # the low habitable cell (40, 1)
    ib = 40 * n + 1          # the high ore cell (1, 40)
    lay["habitability"][ia] = 200
    lay["ore"][ib] = 200
    lay["elevation"][ib] = 3000
    reg = [dict(seed_x=1, seed_y=1, elev_max_dm=3000)]
    for race, cw, want in ((ELF, (0, 0), [(1, 1, "seed"), (40, 1, "best")]),
                           (DWARF, (400, 600), [(1, 40, "best"), (40, 1, "best")])):
        w = r6_world(races=("elf", "dwarf"), regions=reg, layers=lay,
                     race_fields={race: dict(cell=dict(ore=cw[0], height=cw[1]))})
        got = []
        for _ in range(2):
            x, y, how = w.site(1, race, False)
            got.append((x, y, how))
            w.settlements.append(dict(id=len(w.settlements) + 1, x=x, y=y, status=THRIVING))
        c.eq("17a race %d cell_weights %s: sites" % (race, cw), got, want)
    w = r6_world(races=("dwarf",), regions=reg, layers=lay)
    c.eq("17a v of the high ore cell for 400, 600", w.geo.value(ib, 1, DWARF), (400 * 200 + 600 * 255) // 1000)
    c.eq("17a v = 233", w.geo.value(ib, 1, DWARF), 233)
    c.eq("17a v of the habitable cell", w.geo.value(ia, 1, DWARF), 200 + 600 * (1 * 255 // 3000) // 1000)
    w = r6_world(races=("dwarf",), regions=[dict(seed_x=1, seed_y=1, elev_max_dm=0)], layers=lay)
    c.eq("17a hgt at elev_max_dm 0 (the max(1, ...) guard)", w.geo.hgt(ib, 1), 255)
    c.eq("17a hgt of an elevation-1 cell at elev_max_dm 0", w.geo.hgt(0, 1), 255)
    lay_t = dict(lay, habitability=[0] * N, ore=[0] * N, elevation=[1] * N)
    w = r6_world(races=("dwarf",), regions=[dict(seed_x=1, seed_y=1, elev_max_dm=1)], layers=lay_t)
    c.eq("17a ties by cell index (every cell equal): the first cell", w.site(1, DWARF, False), (0, 0, "best"))
    FLAGS["cell"] = False
    c.eq("17a flag off: a dwarf takes the seed cell (0.16.0)",
         r6_world(races=("dwarf",), regions=reg, layers=lay).site(1, DWARF, False), (1, 1, "seed"))
    FLAGS["cell"] = True
    # --- 17b fortress cell rule (8.4)
    lay_b = dict(lay)
    lay_b["walk_area"] = [1] * N
    lay_b["walk_area"][5 * n + 5] = 0
    reg_b = [dict(cells=4000, walk_cells=3000)]          # not upland
    w = r6_world(races=("dwarf", "goblin"), regions=reg_b, layers=lay_b)
    c.true("17b region is not upland", not w.geo.upland(1))
    c.eq("17b dwarf on walk_area 0: fortress", w.founding_kind(DWARF, 1, 300, 5, 5), FORTRESS)
    c.eq("17b goblin (no cell term) on walk_area 0: the ladder", w.founding_kind(GOBLIN, 1, 300, 5, 5),
         w.ladder(GOBLIN, 300))
    c.true("17b the ladder is not a fortress", w.ladder(GOBLIN, 300) != FORTRESS)
    c.eq("17b dwarf on a walkable cell: the ladder", w.founding_kind(DWARF, 1, 300, 6, 5), w.ladder(DWARF, 300))
    # --- 17c last settlement (8.5 (a))
    big = [dict(cells=60000, walk_cells=60000, fertility_mean=255, forage_mean=255, timber_mean=255,
                fresh_water_mean=255)]
    pe_edit = dict(plague_toll_min=100, plague_toll_max=100, abandon_permille=dict(
        hamlet=1000, village=1000, town=1000, city=1000, fortress=1000))
    w = r6_world(races=("human",), regions=big, peoples=pe_edit)
    draws = []
    real = w.abandon_draw
    w.abandon_draw = lambda s: draws.append(s["id"]) or real(s)
    civ = r6_civ(w, HUMAN)
    s1 = r6_stl(w, civ, 1, VILLAGE, 500, 10, 10, status=DECLINING)
    w.strike(w.stl(s1), "plague", 100, ())
    c.eq("17c a last declining settlement: no draw", draws, [])
    c.eq("17c not abandoned", w.stl(s1)["status"], DECLINING)
    s2 = r6_stl(w, civ, 1, HAMLET, 100, 50, 50, created=w.serial)    # created earlier in the same step
    w.strike(w.stl(s1), "plague", 100, ())
    c.eq("17c with a second settlement of the step: the draw is made", draws, [s1])
    c.eq("17c and decides (abandon_permille 1000)", w.stl(s1)["status"], RUINED)
    w = r6_world(races=("human",), regions=big)
    civ = r6_civ(w, HUMAN)
    s1 = r6_stl(w, civ, 1, VILLAGE, 40, 10, 10)
    # P3-05 r9 21 item 6 (0.18.0): a race's last refuge is never abandoned, so another civilization of the race
    # stands elsewhere (the case as written for 0.17.0 had none; found failing at 0.18.0, P3-10 tests)
    r6_stl(w, r6_civ(w, HUMAN), 1, VILLAGE, 900, 50, 50)
    e = w.strike(w.stl(s1), "plague", 100, ())
    keys = [r6_kind(w, i) for i in range(e, len(w.events) + 1)]
    c.eq("17c a last settlement below abandon_min: abandoned and the civilization ends", keys,
         ["plague", "settlement_abandoned", "civ_ended"])
    # --- 17d destination (8.5 item 1)
    def dest_case(room, survivors=869, other=True, names=("Rannve", "Voeledell")):
        w = r6_world(races=("elf",), regions=[dict(cells=60000, walk_cells=60000)])
        civ = r6_civ(w, ELF)
        s = r6_stl(w, civ, 1, VILLAGE, survivors, 10, 10, name=names[0])
        t_id = None
        if other:
            cap = w.geo.capacity(1, ELF)
            t_id = r6_stl(w, civ, 1, VILLAGE, 2 * cap - room, 50, 50, name=names[1])
        e = w.abandon(s, w.stl(s)["founding_event"])
        return w, e, t_id
    w, e, t_id = dest_case(900)
    c.eq("17d room 900: kind 31, settled 869, lost 0", (r6_kind(w, e), w.events[e - 1]["values"]),
         ("settlement_abandoned_to", {1: 869, 2: 0}))
    c.eq("17d its destination", [p for p in w.events[e - 1]["participants"] if p[1] == 3][0][3], t_id)
    c.eq("17d sentence", r6_sentence(w, e), "Rannve was abandoned; 869 survivors went to Voeledell.")
    w, e, _ = dest_case(857)
    c.eq("17d room 857: settled 857, lost 12", w.events[e - 1]["values"], {1: 857, 2: 12})
    c.eq("17d sentence", r6_sentence(w, e), "Rannve was abandoned; 857 survivors went to Voeledell, and 12 were lost.")
    w, e, _ = dest_case(0)
    c.eq("17d room 0: kind 16, survivors 869", (r6_kind(w, e), w.events[e - 1]["values"]),
         ("settlement_abandoned", {1: 869}))
    c.eq("17d sentence", r6_sentence(w, e), "Rannve was abandoned; its 869 survivors were scattered.")
    w, e, _ = dest_case(0, survivors=1, other=False)
    c.eq("17d one survivor and no other settlement", r6_sentence(w, e),
         "Rannve was abandoned; its one survivor was scattered.")
    w, e, _ = dest_case(0, survivors=0, other=False)
    c.eq("17d no survivor", r6_sentence(w, e), "Rannve was abandoned; no one survived.")
    w, e, _ = dest_case(900, survivors=1)
    c.eq("17d one survivor reaching a destination", r6_sentence(w, e),
         "Rannve was abandoned; %s survivor went to Voeledell." % ("one" if pe.RENDER_R17 else "1"))
    w = r6_world(races=("elf",), regions=[dict(cells=60000, walk_cells=60000)])
    civ = r6_civ(w, ELF)
    s = r6_stl(w, civ, 1, VILLAGE, 300, 30, 30)
    a = r6_stl(w, civ, 1, VILLAGE, 300, 40, 30)       # distance^2 100
    b = r6_stl(w, civ, 1, VILLAGE, 300, 30, 40)       # distance^2 100, larger id
    far = r6_stl(w, civ, 1, VILLAGE, 300, 31, 20)     # distance^2 101
    c.eq("17d nearest by squared distance, then id", w.destination(w.stl(s))[0]["id"], a)
    w.stl(a)["x"] = 41
    c.eq("17d nearest by squared distance", w.destination(w.stl(s))[0]["id"], b)
    del far
    # --- 17e decline start (8.5 causes)
    pe_e = dict(abandon_permille=dict(hamlet=1000, village=1000, town=1000, city=1000, fortress=1000))
    w = r6_world(races=("human",), regions=big, peoples=pe_e)
    civ = r6_civ(w, HUMAN)
    s = r6_stl(w, civ, 1, VILLAGE, 900, 10, 10)
    r6_stl(w, civ, 1, VILLAGE, 900, 50, 50)
    ea = w.strike(w.stl(s), "famine", 100, ())
    c.eq("17e the famine found it thriving: no abandonment yet", w.stl(s)["status"], DECLINING)
    eb = w.strike(w.stl(s), "plague", 100, ())
    ab = len(w.events) - 0
    abandoned = [i for i in range(eb + 1, len(w.events) + 1) if r6_kind(w, i).startswith("settlement_abandoned")]
    c.eq("17e abandoned by chance with causes (a, b)", [w.events[i - 1]["causes"] for i in abandoned], [[ea, eb]])
    del ab
    w = r6_world(races=("human",), regions=big)
    civ = r6_civ(w, HUMAN)
    s = r6_stl(w, civ, 1, VILLAGE, 40, 10, 10)
    r6_stl(w, civ, 1, VILLAGE, 900, 50, 50)
    e1 = w.strike(w.stl(s), "famine", 100, ())
    ab = [i for i in range(e1 + 1, len(w.events) + 1) if r6_kind(w, i).startswith("settlement_abandoned")]
    c.eq("17e struck once while thriving below abandon_min: one cause", [w.events[i - 1]["causes"] for i in ab],
         [[e1]])
    w = r6_world(races=("human",), regions=big, peoples=pe_e)
    civ = r6_civ(w, HUMAN)
    s = r6_stl(w, civ, 1, VILLAGE, 900, 10, 10)
    r6_stl(w, civ, 1, VILLAGE, 900, 50, 50)
    ea = w.strike(w.stl(s), "famine", 100, ())
    w.stl(s)["status"] = THRIVING                     # recovered (8.3)
    eb = w.strike(w.stl(s), "famine", 100, ())
    ec = w.strike(w.stl(s), "plague", 100, ())
    ab = [i for i in range(ec + 1, len(w.events) + 1) if r6_kind(w, i).startswith("settlement_abandoned")]
    c.eq("17e recovery resets the decline start to the second blow", [w.events[i - 1]["causes"] for i in ab],
         [[eb, ec]])
    c.true("17e the first blow is not a cause", ea not in w.events[ab[0] - 1]["causes"])
    # --- 17f cap drain (9.1)
    w = r6_world(races=("faerie",), regions=big)
    civ = r6_civ(w, FAERIE)
    s = r6_stl(w, civ, 1, VILLAGE, 2277, 10, 10)
    c.eq("17f faerie village cap", w.kind_cap(w.stl(s)), 1499)
    c.eq("17f 2277 with share 3000 and cap 1499 loses 77", w.grown(w.stl(s), 3000), 2277 - max(1, 778 * 100 // 1000))
    c.eq("17f = 2200", w.grown(w.stl(s), 3000), 2200)
    w.stl(s)["population"] = 1499
    c.eq("17f at 1499 it stays", w.grown(w.stl(s), 3000), 1499)
    for p, cs in ((900, 1000), (1000, 1000), (1100, 1000), (300, 1499), (1499, 1200)):
        w.stl(s)["population"] = p
        new = w.grown(w.stl(s), cs)
        FLAGS["cap"] = False
        old = w.grown(w.stl(s), cs)
        FLAGS["cap"] = True
        c.eq("17f P %d <= cap, C_s %d: the r5 value" % (p, cs), new, old)
    # --- 17g plague salience (10.2)
    def plague_case(spreads, s_pop=6000, s_toll=100, plan=True):
        FLAGS["plague_sal"] = plan
        pe_g = dict(plague_spread_permille=1000, plague_toll_min=100, plague_toll_max=100,
                    plague_spread_toll_permille=500)
        w = r6_world(races=("human",), regions=big, peoples=pe_g)
        civ = r6_civ(w, HUMAN)
        s = r6_stl(w, civ, 1, CITY, s_pop, 10, 10)
        for i in range(spreads):
            r6_stl(w, civ, 1, CITY, 5000, 50 + 3 * i, 50)
        o = w.outbreak(w.stl(s), s_toll, set(), len(w.settlements), 1)
        FLAGS["plague_sal"] = True
        evs = [(r6_kind(w, i), w.events[i - 1]["values"], w.events[i - 1]["causes"]) for i in
               range(o, len(w.events) + 1)]
        return w.events[o - 1]["salience"], evs
    sal, evs = plague_case(2)
    c.eq("17g 600 + 250 + 250 = 1100: salience 45", (sal, [v[1][1] for v in evs]), (45, [600, 250, 250]))
    sal, evs = plague_case(1)
    c.eq("17g 600 + 250 = 850: salience 25", (sal, [v[1][1] for v in evs]), (25, [600, 250]))
    sal, evs = plague_case(2, s_pop=1100, s_toll=980)
    c.eq("17g an emptied outbreak: no spread, its own toll decides (1078)",
         (sal, [v[0] for v in evs][:2], evs[0][1][1]), (45, ["plague", "settlement_abandoned_to"], 1078))
    sal, evs = plague_case(2, s_pop=900, s_toll=980)
    c.eq("17g an emptied outbreak of 882 dead: 25", (sal, evs[0][1][1]), (25, 882))
    for k in (0, 1, 2):
        a = plague_case(k)[1]
        b = plague_case(k, plan=False)[1]
        c.eq("17g the plan and the strikes agree (%d spreads)" % k, a, b)
    # the lead's ruling: a spread that abandons its target sends survivors to a later target; the plan replays it
    pe_r = dict(plague_spread_permille=1000, plague_toll_min=100, plague_toll_max=100,
                plague_spread_toll_permille=1000)
    w = r6_world(races=("human",), regions=big, peoples=pe_r)
    civ = r6_civ(w, HUMAN)
    s = r6_stl(w, civ, 1, CITY, 8000, 10, 10)
    r6_stl(w, civ, 1, VILLAGE, 43, 40, 40)             # 43 - 4 = 39 < abandon_min: abandoned, 39 go to s3
    r6_stl(w, civ, 1, CITY, 1955, 41, 40)              # nearest to s2; struck after it at 1955 + 39 = 1994
    o = w.outbreak(w.stl(s), 100, set(), len(w.settlements), 1)
    dead = [w.events[i - 1]["values"][1] for i in range(o, len(w.events) + 1)
            if r6_kind(w, i) in ("plague", "plague_spread")]
    c.eq("17g ruling: the later target is struck with the refugees (1994 x 100 div 1000)", dead, [800, 4, 199])
    c.eq("17g ruling: planned toll = struck toll", w.plague_plans[-1][1:], (1003, 1003))
    c.eq("17g ruling: salience 45 by the replayed plan (from the populations before: 800 + 4 + 195 = 999, 25)",
         w.events[o - 1]["salience"], 45)
    # --- 17h crisis salience (10.3)
    regs3 = [dict(x_min=0, x_max=19, seed_x=1, cells=1280, walk_cells=1280),
             dict(x_min=20, x_max=39, seed_x=21, cells=1280, walk_cells=1280),
             dict(x_min=40, x_max=59, seed_x=41, cells=1280, walk_cells=1280)]
    pe_h = dict(split_min_settlements=2, split_min_regions=2, split_permille=1000)
    w = r6_world(races=("human",), regions=regs3, peoples=pe_h, n=60,
                 borders=[dict(a=1, b=2, walk_pairs=5, length_m=1000), dict(a=2, b=3, walk_pairs=5, length_m=1000)])
    civ = r6_civ(w, HUMAN)
    cap = r6_stl(w, civ, 1, TOWN, 3000, 5, 5, name="Meadstead")
    r6_stl(w, civ, 3, TOWN, 2000, 45, 5)
    e = w.crisis(civ)
    sec = [i for i in range(e + 1, len(w.events) + 1) if r6_kind(w, i) == "civ_seceded"]
    c.eq("17h a crisis that splits: salience 40", w.events[e - 1]["salience"], 40)
    c.eq("17h the civ_seceded names it", [w.events[i - 1]["causes"] for i in sec], [[e]])
    c.eq("17h template", r6_sentence(w, e), "A succession crisis at Meadstead shook C1.")
    w = r6_world(races=("human",), regions=regs3, peoples=pe_h, n=60)
    civ = r6_civ(w, HUMAN)
    r6_stl(w, civ, 1, TOWN, 3000, 5, 5, name="Meadstead")
    e = w.crisis(civ)
    c.eq("17h a crisis that does not split: 25", w.events[e - 1]["salience"], 25)
    c.eq("17h no secession", [r6_kind(w, i) for i in range(e, len(w.events) + 1)], ["succession_crisis"])
    del cap
    # --- 17i catalogue
    c.eq("17i kind 31", (KIND31["id"], KIND31["key"], KIND31["caused"], KIND31["region_use"], KIND31["settlement_use"],
                         [(r["seq"], r["role"], r["type"], r["min"], r["max"]) for r in KIND31["roles"]],
                         KIND31["values"]),
         (31, "settlement_abandoned_to", 1, 1, 1, [(1, "settlement", SETTLEMENT, 1, 1), (2, "civilization", CIV, 1, 1),
                                                   (3, "destination", SETTLEMENT, 1, 1)], ["settled", "lost"]))
    if len(pe.COMPILED) == 31:
        c.eq("17i the engine reference's kind 31", pe.COMPILED[30], KIND31)
    c.eq("17i its template passes template_problem", pe.template_problem(KIND31, SPEC_REPORT_31[2]), None)
    dat = data()
    c.eq("17i history.report lists it (data)", [e[:2] for e in dat.report if e[0] == "settlement_abandoned_to"],
         [SPEC_REPORT_31[:2]])
    FLAGS.clear()
    FLAGS.update(saved)


def r12_event(w, key, year, settlement=0, region=1, participants=(), values=None, causes=()):
    """A hand-appended event (as r6_civ does), dated `year`."""
    w.events.append(dict(id=len(w.events) + 1, year=year, end_year=year, kind=KIND_ID[key], phase=1, region=region,
                         settlement=settlement, salience=0, participants=list(participants),
                         values=dict(values or {}), causes=list(causes), key=key))
    return len(w.events)


def check_units_r12(c):
    """P3-05 r12 22.8 items 1, 2, 3, 4, 4a (generator 0.19.0)."""
    ELF, GOBLIN, DWARF, FAERIE, HUMAN = 1, 2, 3, 4, 5
    big = [dict(cells=60000, walk_cells=60000, fertility_mean=255, forage_mean=255, timber_mean=255,
                fresh_water_mean=255)]
    # --- 1. 22.1 kind 37
    pe_g = dict(plague_spread_permille=1000, plague_toll_min=100, plague_toll_max=100, plague_spread_toll_permille=500)

    def outbreak(pops, names=None):
        w = r6_world(races=("human",), regions=big, peoples=pe_g)
        civ = r6_civ(w, HUMAN)
        ids = [r6_stl(w, civ, 1, CITY, p, 10 + 3 * i, 10, name=(names[i] if names else None))
               for i, p in enumerate(pops)]
        o = w.outbreak(w.stl(ids[0]), 100, set(), len(w.settlements), 1)
        return w, o
    w, o = outbreak([6000, 6000, 2400], names=["Gork-Skreeg Hole", "B", "C"])
    e = w.events[o - 1]
    c.eq("22.8.1 kind 37 values dead 600, population 5400, total 1020, reached 2; salience 45",
         (e["key"], e["values"], e["salience"]), ("plague_outbreak", {1: 600, 2: 5400, 3: 1020, 4: 2}, 45))
    c.eq("22.8.1 the spreads: kind 13 caused by the outbreak", [(r6_kind(w, i), w.events[i - 1]["values"][1],
                                                                  w.events[i - 1]["causes"])
                                                                 for i in range(o + 1, len(w.events) + 1)],
         [("plague_spread", 300, [o]), ("plague_spread", 120, [o])])
    c.eq("22.8.1 sentence with two spreads", r6_sentence(w, o),
         "Plague broke out in Gork-Skreeg Hole and spread to %s other settlements; 1,020 died in all." %
         ("two" if pe.RENDER_R17 else "2"))
    w, o = outbreak([6000, 6000], names=["Raenme", "B"])
    c.eq("22.8.1 sentence with one spread (P3-05 r14 22.1: 'in all' whenever another settlement was reached)",
         r6_sentence(w, o), "Plague broke out in Raenme and spread to one other settlement; 900 died in all.")
    w, o = outbreak([1200], names=["Raenme"])
    e = w.events[o - 1]
    c.eq("22.8.1 no spread: reached 0, total 120, salience 25", (e["values"], e["salience"]),
         ({1: 120, 2: 1080, 3: 120, 4: 0}, 25))
    c.eq("22.8.1 sentence with no spread", r6_sentence(w, o), "Plague broke out in Raenme; 120 died.")
    w = r6_world(races=("human",), regions=big, peoples=pe_g)
    civ = r6_civ(w, HUMAN)
    s = r6_stl(w, civ, 1, CITY, 43, 10, 10)            # 43 - 4 = 39 < abandon_min 40: emptied
    r6_stl(w, civ, 1, CITY, 6000, 13, 10)
    o = w.outbreak(w.stl(s), 100, set(), len(w.settlements), 1)
    c.eq("22.8.1 an outbreak that empties s: reached 0, no spread", (w.events[o - 1]["values"][4],
                                                                    [r6_kind(w, i) for i in range(o, len(w.events) + 1)]),
         (0, ["plague_outbreak", "settlement_abandoned_to", "capital_moved"]))
    c.eq("22.8.1 the plan's total equals the real toll (invariant 13)", w.plague_plans[-1][1:], (4, 4))
    # --- 2. 22.2 the cause memory
    Y = 1000

    def dwindle_case(ago_battle=None, ago_colony=None, pop=1142):
        w = r6_world(races=("human",), regions=big)
        w.year = Y
        civ = r6_civ(w, HUMAN)
        s = r6_stl(w, civ, 1, CITY, pop, 10, 10, name="Glathenddell")
        if ago_battle is not None:
            w.last_red[s] = r12_event(w, "battle", Y - ago_battle, participants=[(1, 1, CIV, civ), (2, 2, CIV, civ)],
                                      values={1: 5, 2: 7}, causes=[1])
        if ago_colony is not None:
            w.last_colony[s] = r12_event(w, "colony_founded", Y - ago_colony, settlement=s,
                                         participants=[(1, 1, SETTLEMENT, s), (2, 2, CIV, civ), (3, 3, SETTLEMENT, s)],
                                         values={1: 60})
        n0 = len(w.events)
        w.kind_changes()
        ds = [i for i in range(n0 + 1, len(w.events) + 1) if r6_kind(w, i) == "settlement_dwindled"]
        return w, ds
    for ago, want in ((49, True), (50, True), (51, False)):
        w, ds = dwindle_case(ago_battle=ago)
        c.eq("22.8.2 a dwindling %d years after a battle: %s" % (ago, "the battle" if want else "no cause"),
             [w.events[i - 1]["causes"] for i in ds], [[w.last_red[1]]] if want else [[]])
    w, ds = dwindle_case(ago_battle=80, ago_colony=30)
    c.eq("22.8.2 a colony sent 30 years before and no recent reduction: the colony", [w.events[i - 1]["causes"]
                                                                                    for i in ds], [[w.last_colony[1]]])
    c.eq("22.8.2 sentence (22.3: emigration, separators)", r6_sentence(w, ds[0]),
         "Weakened by emigration, Glathenddell dwindled to 1,142 souls.")
    w, ds = dwindle_case(ago_battle=10, ago_colony=30, pop=412)
    c.eq("22.8.2 a recent reduction wins over the colony", [w.events[i - 1]["causes"] for i in ds],
         [[w.last_red[1]]])
    w.stl(1)["name"] = "Krux"
    c.eq("22.8.3 sentence Weakened by war", r6_sentence(w, ds[0]), "Weakened by war, Krux dwindled to 412 souls.")
    w, ds = dwindle_case(ago_colony=51)
    c.eq("22.8.2 a colony 51 years ago: no cause", [w.events[i - 1]["causes"] for i in ds], [[]])
    c.eq("22.8.3 no cause: the sentence starts with the name", r6_sentence(w, ds[0]),
         "Glathenddell dwindled to 1,142 souls.")
    # an abandonment whose decline start is 51 years old cites only the blow
    w = r6_world(races=("human",), regions=big)
    w.year = Y
    civ = r6_civ(w, HUMAN)
    s = r6_stl(w, civ, 1, VILLAGE, 44, 10, 10, status=DECLINING)
    r6_stl(w, civ, 1, VILLAGE, 900, 50, 50)
    w.decline_start[s] = r12_event(w, "famine", Y - 51, settlement=s, participants=[(1, 1, SETTLEMENT, s),
                                                                                   (2, 2, CIV, civ)],
                                   values={1: 5, 2: 44})
    eb = w.strike(w.stl(s), "famine", 100, ())
    ab = [i for i in range(eb + 1, len(w.events) + 1) if r6_kind(w, i).startswith("settlement_abandoned")]
    c.eq("22.8.2 an abandonment with a 51-year-old decline start cites only the blow",
         [w.events[i - 1]["causes"] for i in ab], [[eb]])
    w.decline_start[s] = 0
    w2 = r6_world(races=("human",), regions=big)
    w2.year = Y
    civ = r6_civ(w2, HUMAN)
    s = r6_stl(w2, civ, 1, VILLAGE, 44, 10, 10, status=DECLINING)
    r6_stl(w2, civ, 1, VILLAGE, 900, 50, 50)
    w2.decline_start[s] = r12_event(w2, "famine", Y - 50, settlement=s, participants=[(1, 1, SETTLEMENT, s),
                                                                                     (2, 2, CIV, civ)],
                                    values={1: 5, 2: 44})
    eb = w2.strike(w2.stl(s), "famine", 100, ())
    ab = [i for i in range(eb + 1, len(w2.events) + 1) if r6_kind(w2, i).startswith("settlement_abandoned")]
    c.eq("22.8.2 a 50-year-old decline start is cited", [w2.events[i - 1]["causes"] for i in ab],
         [[w2.decline_start[s], eb]])
    # 9.2's abandonment (b): an old last reduction is none, the founding event stands in
    w3 = r6_world(races=("human",), regions=big)
    w3.year = Y
    civ = r6_civ(w3, HUMAN)
    s = r6_stl(w3, civ, 1, VILLAGE, 30, 10, 10)
    r6_stl(w3, civ, 1, VILLAGE, 900, 50, 50)
    w3.last_red[s] = r12_event(w3, "famine", Y - 60, settlement=s, participants=[(1, 1, SETTLEMENT, s),
                                                                                (2, 2, CIV, civ)], values={1: 5, 2: 30})
    n0 = len(w3.events)
    w3.abandon_b()
    ab = [i for i in range(n0 + 1, len(w3.events) + 1) if r6_kind(w3, i).startswith("settlement_abandoned")]
    c.eq("22.8.2 abandonment (b): a 60-year-old reduction is not cited; the founding event", [w3.events[i - 1]["causes"]
                                                                                           for i in ab],
         [[w3.stl(s)["founding_event"]]])
    c.eq("22.8.3 an abandonment with only its founding event starts with the name", r6_sentence(w3, ab[0])[:3], "S1 ")
    # --- 3. 22.3, 22.4 templates: the illustrative examples
    w = r6_world(races=("elf",), regions=big)
    w.year = Y
    civ = r6_civ(w, ELF)
    s = r6_stl(w, civ, 1, TOWN, 1000, 10, 10, name="Ithononiedell")
    r6_stl(w, civ, 1, TOWN, 1000, 50, 50, name="Elaethaer")
    ef = w.strike(w.stl(s), "famine", 100, ())                          # 900, declining (decline start)
    w.stl(s)["population"] = 543
    ep = w.strike(w.stl(s), "plague_outbreak", 100, (), extra=lambda d: (d, 0))   # 543 - 54 = 489
    n0 = len(w.events)
    w.abandon(s, ep)
    ab = [i for i in range(n0 + 1, len(w.events) + 1) if r6_kind(w, i).startswith("settlement_abandoned")]
    c.eq("22.8.3 causes famine (decline start) and the outbreak", w.events[ab[0] - 1]["causes"], [ef, ep])
    c.eq("22.8.3 kind 31 sentence", r6_sentence(w, ab[0]),
         "Weakened by famine and plague, Ithononiedell was abandoned; 489 survivors went to Elaethaer.")
    w = r6_world(races=("goblin",), regions=big)
    civ = r6_civ(w, GOBLIN)
    w.civ(civ)["name"] = "The Warrens of Zeez"
    s = r6_stl(w, civ, 1, TOWN, 1000, 10, 10)
    r6_stl(w, civ, 1, TOWN, 3000, 50, 50, name="Gnog Pit")
    eb = r12_event(w, "famine", w.year, settlement=s, participants=[(1, 1, SETTLEMENT, s), (2, 2, CIV, civ)],
                   values={1: 1, 2: 1})
    n0 = len(w.events)
    w.ruin(s, eb)
    cm = [i for i in range(n0 + 1, len(w.events) + 1) if r6_kind(w, i) == "capital_moved"]
    c.eq("22.8.3 kind 17 without its", r6_sentence(w, cm[0]), "The seat of the Warrens of Zeez moved to Gnog Pit.")
    c2 = r6_civ(w, GOBLIN)
    w.civ(c2)["name"] = "The Warrens of Gnog"
    es = r12_event(w, "civ_seceded", w.year, settlement=2, participants=[(1, 1, CIV, c2), (2, 2, CIV, civ)],
                   values={1: 3}, causes=[eb])
    c.eq("22.8.3 kind 15 without its", r6_sentence(w, es),
         "The Warrens of Gnog broke away from the Warrens of Zeez with %s settlements." % ("three" if pe.RENDER_R17 else "3"))
    w.events[es - 1]["values"] = {1: 1}
    c.eq("22.8.3 kind 15 one settlement", r6_sentence(w, es),
         "The Warrens of Gnog broke away from the Warrens of Zeez with %s settlement." % ("one" if pe.RENDER_R17 else "1"))
    # --- 4. 22.5 the site terms
    n = 64
    N = n * n
    lay = dict(region=[1] * N, water=[0] * N, habitability=[(i * 7) % 256 for i in range(N)], river=[0] * N,
               flow_dir=[8] * N, elevation=[(i * 13) % 3001 for i in range(N)], ore=[(i * 5) % 256 for i in range(N)],
               walk_area=[1] * N)
    for i in range(0, N, 97):
        lay["water"][i] = 3                                   # some sea, so ports occur
    reg = [dict(seed_x=1, seed_y=1, elev_max_dm=3000)]
    for race in (ELF, DWARF):
        w19 = r6_world(races=("elf", "dwarf"), regions=reg, layers=lay,
                       race_fields={race: dict(cell=dict(CELL_NEUTRAL, ore=400, height=600) if race == DWARF
                                               else dict(CELL_NEUTRAL))})
        saved = dict(FLAGS)
        got19 = [w19.geo.value(i, 1, race) for i in range(N) if lay["water"][i] == 0]
        FLAGS["sites19"] = False
        w18 = r6_world(races=("elf", "dwarf"), regions=reg, layers=lay,
                       race_fields={race: dict(cell=dict(CELL_NEUTRAL, ore=400, height=600) if race == DWARF
                                               else dict(CELL_NEUTRAL))})
        got18 = [w18.geo.value(i, 1, race) for i in range(N) if lay["water"][i] == 0]
        FLAGS.clear()
        FLAGS.update(saved)
        c.eq("22.8.4 the r6 record with neutral new fields gives r6's v on every cell (race %d)" % race, got19, got18)
    # sea_d on a 5 x 5 grid with one sea cell at a corner
    n5 = 5
    lay5 = dict(region=[1] * 25, water=[0] * 25, habitability=[0] * 25, river=[0] * 25, flow_dir=[8] * 25,
                elevation=[1] * 25, ore=[0] * 25, walk_area=[1] * 25)
    lay5["water"][0] = 3
    for cap in (3, 20):
        w5 = r6_world(races=("faerie",), regions=[dict(x_max=4, y_max=4, seed_x=2, seed_y=2)], n=n5, layers=lay5,
                      peoples=dict(inland_cap=cap))
        c.eq("22.8.4 sea_d 5x5, sea at (0, 0), inland_cap %d" % cap, w5.geo.sea_d(),
             [min(cap, max(x, y)) for y in range(5) for x in range(5)])
    lay5b = dict(lay5, water=[0] * 25)
    w5 = r6_world(races=("faerie",), regions=[dict(x_max=4, y_max=4)], n=n5, layers=lay5b, peoples=dict(inland_cap=7))
    c.eq("22.8.4 a world without sea: inland_cap everywhere", w5.geo.sea_d(), [7] * 25)
    # a faerie site in a coastal region with an inland free cell chooses the inland cell
    layf = dict(region=[1] * N, water=[0] * N, habitability=[0] * N, river=[0] * N, flow_dir=[8] * N,
                elevation=[1] * N, ore=[0] * N, walk_area=[1] * N)
    for y in range(n):
        layf["water"][y * n] = 3                              # the sea is column 0
    for y in range(n):
        for x in range(1, n):
            layf["habitability"][y * n + x] = max(0, 200 - 4 * x)   # the shore is the most habitable
    wf = r6_world(races=("faerie",), regions=[dict(seed_x=1, seed_y=1)], layers=layf)
    x, y, how = wf.site(1, FAERIE, False)
    v_inl = wf.geo.value(y * n + x, 1, FAERIE)
    c.eq("22.8.4 faerie: not the seed cell, an inland cell (sea_d >= 20)", (how, x >= 20), ("best", True))
    c.eq("22.8.4 faerie v of the shore cell (1, 0): 196 + 0 + 400 x 1 x 255 div 20000",
         wf.geo.value(1, 1, FAERIE), 196 + 400 * 1 * 255 // 20000)
    c.eq("22.8.4 faerie v of the chosen cell", v_inl, max(0, 200 - 4 * x) + 400 * 20 * 255 // 20000)
    c.true("22.8.4 the faerie has a cell term (inland, port differ from neutral)", wf.geo.has_cell_term(FAERIE))
    # the dwarf: a hill cell of habitability 60 and height 255 beats a shore cell of habitability 200
    layd = dict(layf, habitability=[0] * N, elevation=[1] * N)
    hill, shore = 30 * n + 30, 5 * n + 1
    layd["habitability"][hill] = 60
    layd["elevation"][hill] = 3000
    layd["habitability"][shore] = 200
    wd = r6_world(races=("dwarf",), regions=[dict(seed_x=1, seed_y=5, elev_max_dm=3000)], layers=layd)
    c.eq("22.8.4 dwarf v hill = 30 + 153 = 183", wd.geo.value(hill, 1, DWARF), 183)
    c.eq("22.8.4 dwarf v shore = 100 + 0 (port 0) = 100", wd.geo.value(shore, 1, DWARF), 100)
    c.eq("22.8.4 the dwarf chooses the hill", wd.site(1, DWARF, False)[:2], (30, 30))
    c.true("22.8.4 the shore cell is in the port radius (its bonus is dropped by port 0)",
           wd.geo.port(shore, DWARF) > 0)
    # --- 4a. 22.7a lie_low
    w = r6_world(races=("human",), regions=big + [dict(x_min=0, x_max=0)], peoples=dict(found_rate_permille=1000))
    civ = r6_civ(w, HUMAN)
    s = r6_stl(w, civ, 1, CITY, 30000, 10, 10)
    draws = []
    real = w.draw
    w.draw = lambda stream, a, b, k: (draws.append((w.year, stream)) or real(stream, a, b, k))
    Yl = 1000
    w.year = Yl
    w.lie_low(civ, s, Yl + 300)
    hunger0 = w.hunger.get(civ)
    for yr in (Yl, Yl + 150, Yl + 299):
        w.year = yr
        w.serial += 1
        w.foundings(quiet=False)
        w.serial += 1
        w.years = 25
        w.foundings(quiet=True)
        w.years = 1
    c.eq("22.8.4a no founding and no found or outward draw while lying low",
         ([x for x in draws if x[1] in ("hist.peoples.found", "hist.peoples.outward")],
          len(w.settlements), w.hunger.get(civ)), ([], 1, hunger0))
    c.true("22.8.4a its pressure is above found_pressure_permille",
           w.stl(s)["population"] * 1000 // w.shares()[s] >= w.t["found_pressure_permille"] or
           w.stl(s)["population"] * 1000 // w.kind_cap(w.stl(s)) >= w.t["found_pressure_permille"])
    w.year = Yl + 300
    w.serial += 1
    w.foundings(quiet=False)
    c.eq("22.8.4a the first founding chance at Y + 300", [x for x in draws if x[1] == "hist.peoples.found"],
         [(Yl + 300, "hist.peoples.found")])
    w.lie_low(civ, s, Yl + 400)
    w.lie_low(civ, s, Yl + 350)
    c.eq("22.8.4a a later call replaces the year", w.low_until[civ], Yl + 350)
    # r15 (P3-06 r17 22.2, 22.3): an orc refuge's 100 years; the refuge ruined, or owned by another civilization,
    # before its year while the civilization stands elsewhere: foundings resume in the next step
    for how in ("orc", "ruined", "passed", "ruined-quiet"):
        races = ("orc", "human") if how == "orc" else ("human", "elf")
        rc = 6 if how == "orc" else HUMAN   # orc: race order 6
        w = r6_world(races=races, regions=big + [dict(x_min=0, x_max=0)], peoples=dict(found_rate_permille=1000))
        civ = r6_civ(w, rc)
        other = r6_civ(w, rc)
        home = r6_stl(w, civ, 1, CITY, 30000, 10, 10)     # stands elsewhere
        h = r6_stl(w, civ, 1, VILLAGE, 300, 40, 40)       # the refuge
        r6_stl(w, other, 1, VILLAGE, 300, 50, 50)
        draws = []
        real = w.draw
        w.draw = lambda stream, a, b, k, w=w, draws=draws, real=real: (draws.append((w.year, stream, a)) or
                                                                     real(stream, a, b, k))
        until = Yl + (100 if how == "orc" else 300)
        w.year = Yl
        w.lie_low(civ, h, until)
        found = lambda: [x for x in draws if x[1] == "hist.peoples.found" and x[2] == civ]  # noqa: E731
        w.year = Yl + 50
        w.serial += 1
        w.foundings(quiet=False)
        c.eq("22.8.4a r15 %s: lying low at Y + 50" % how, (w.lying_low(civ), found()), (True, []))
        if how == "orc":
            w.year = Yl + 99
            w.serial += 1
            w.foundings(quiet=False)
            c.eq("22.8.4a r15 orc: lying low at Y + 99", (w.lying_low(civ), found()), (True, []))
            w.year = Yl + 100
            w.serial += 1
            w.foundings(quiet=False)
            c.eq("22.8.4a r15 orc: the first founding chance at Y + 100", (w.lying_low(civ), found()),
                 (False, [(Yl + 100, "hist.peoples.found", civ)]))
            continue
        if how.startswith("ruined"):
            w.stl(h)["status"] = RUINED
            w.stl(h)["owner_civ"] = 0
        else:
            w.stl(h)["owner_civ"] = other
        c.eq("22.8.4a r15 %s: no longer lying low, though Y < until" % how, w.lying_low(civ), False)
        c.eq("22.8.4a r15 %s: the new owner does not lie low" % how, w.lying_low(other), False)
        w.year = Yl + 51
        w.serial += 1
        if how == "ruined-quiet":
            w.years = 25
        w.foundings(quiet=how == "ruined-quiet")
        w.years = 1
        c.eq("22.8.4a r15 %s: the founding chance is taken in the next step" % how, found(),
             [(Yl + 51, "hist.peoples.found", civ)])
        c.true("22.8.4a r15 %s: the record stays (the year and refuge)" % how,
               (w.low_until[civ], w.low_refuge[civ]) == (until, h))
        _ = home
    c.eq("22.8.4a r15 a civilization never given lie_low does not lie low", w.lying_low(99), False)
    # 22.7: the one predicate
    w = r6_world(races=("human", "elf"), regions=big)
    a = r6_civ(w, HUMAN)
    r6_stl(w, a, 1, CITY, 3000, 10, 10)
    c.eq("22.7 one settlement, the race's only civilization: the last refuge", w.race_last_refuge(a), True)
    b = r6_civ(w, HUMAN)
    r6_stl(w, b, 1, CITY, 3000, 30, 10)
    c.eq("22.7 another civilization of the race alive: not", w.race_last_refuge(a), False)
    w.civ(b)["ended_year"] = 500
    r6_stl(w, a, 1, CITY, 3000, 50, 10)
    c.eq("22.7 two standing settlements: not", w.race_last_refuge(a), False)


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
    want = [(k, sal, SPEC_REPORT_016.get(k, t) if not FLAGS["dest"] else t) for k, sal, t in SPEC_REPORT]
    if FLAGS["outbreak37"]:   # r12 22.3, 22.4: the templates of kinds 10, 15, 16, 17 (data reference's listing)
        want = [(k, sal, pdr.TEMPLATES_019[k][1] if k in pdr.TEMPLATES_019 else t) for k, sal, t in want]
    if pe.at_least("0.21.0"):   # P3-05 r18 24: became_city names the region, without the figure
        want = [(k, sal, pdr.BECAME_CITY_021[1] if k == "became_city" else t) for k, sal, t in want]
    c.eq("report entries 6-19 = section 11", [e for e in dat.report[5:19]], want)
    if FLAGS["dest"]:
        e31 = SPEC_REPORT_31 if not FLAGS["outbreak37"] else \
            SPEC_REPORT_31[:2] + (pdr.TEMPLATES_019["settlement_abandoned_to"][1],)
        c.eq("report entry 31 = r6 11", dat.report[30:31], [e31])
    if FLAGS["outbreak37"]:
        c.eq("report entries 37, 38 (P3-05 r12 22.1, P3-04 r12 20.1)", [e[:2] for e in dat.report[36:38]],
             [("plague_outbreak", 45), ("quiet_stretch", 20)])
    c.eq("report_events on the report", report_events_messages(dat.report), [])
    for k in catalogue_kinds():
        c.eq("template of %s parses" % k["key"], pe.template_problem(k, dict((e[0], e[2]) for e in dat.report)[k["key"]]),
             None)
    c.eq("12 streams (13 from 0.23.0: hist.peoples.ghost, P5-02 3)", len(STREAMS), 13)
    check_units(c)
    check_quiet_equals_ticks(c)
    check_properties(c, SYN_SEEDS[:6] if quick else SYN_SEEDS)
    if os.path.isdir(inputs) and not FLAGS["dest"]:      # the sample of 15.2 is 0.15.0/0.16.0's
        res = check_example(c, inputs)
        print("15.2 reference: %d events, kinds %s" % (len(res["event"]), sorted(kind_counts(res).items())))
    if os.path.isdir(inputs):
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


def cmd_compare_016(argv):
    """P3-08 5: against the 0.16.0 golden file OLD, the score lines move only for faeries (race 4) and the
    homes only for faeries or where a home of 0.16.0 is not eligible at that awakening in the 0.17.0 run
    (held by a civilization: the history before it differs). Recomputes every case; exit 1 otherwise."""
    old = open(argv[0], encoding="utf-8").read().splitlines()
    inputs = opt(argv, "--inputs", DEFAULT_INPUTS)
    new = open(GOLDEN, encoding="utf-8").read().splitlines()
    c = Checker()

    def sel(lines, kw):
        return {l for l in lines if (" %s " % kw) in l and not l.startswith("#")}
    for l in sorted(sel(old, "score") - sel(new, "score")):
        c.true("P3-08 5: a non-faerie score line moved: %s" % l, " ra4 " in l)
    changed = {}
    for l in sorted(sel(old, "homes") - sel(new, "homes")):
        w = l.split()
        if w[2] != "ra4":
            changed.setdefault(w[0], []).append((int(w[2][2:]), int(w[3][1:]), [int(x) for x in w[5].split(",")]))
    dat = data()
    for name, items in sorted(changed.items()):
        if name.startswith("syn-") or name.startswith("stress-"):
            seed = int(name.split("-")[1])
            d = edited(dat, peoples=STRESS) if name.startswith("stress-") else dat
            inp, _g = synthetic_inputs(seed, d)
            res = history_stage(d, inp)
        else:
            case = [x for x in WORLD_CASES if x[0] == name][0]
            res = world_case(*case, inputs)
        w = res["_world"]
        for race, year, homes in items:
            st = [x for x in w.homes_state if x[0] == race]
            now = [x for x in w.homes_log if x[0] == race]
            ok = bool(st) and bool(now)
            why = []
            if ok:
                owners, el = st[0][2], st[0][3]
                for h in homes:
                    if h in now[0][2]:
                        why.append("rg%d still a home" % h)
                    elif h not in el:
                        why.append("rg%d held by c%d at y%d" % (h, owners[h], st[0][1]))
                    else:
                        ok = False
                alive = sum(1 for x in w.civs if x["ended_year"] is None or x["ended_year"] > st[0][1])
                why.append("0.17.0 homes %s (n %d; %d eligible, %d alive)" % (now[0][2], now[0][3], len(el), alive))
            print("%s ra%d (0.16.0 y%d homes %s): %s" % (name, race, year, homes, "; ".join(why) or "not explained"))
            c.true("%s ra%d: every 0.16.0 home is a home again or not eligible at the 0.17.0 awakening" % (name, race),
                   ok)
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


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
    c.eq("generator_version", meta.get("generator_version"), pe.ENGINE_VERSION)
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
    # --version V: the rules, data, names and catalogue of generator V (0.16.0 to 0.19.0)
    if "--version" in argv:
        i = argv.index("--version")
        use_version(argv[i + 1])
        argv = argv[:i] + argv[i + 2:]
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
    if cmd == "compare-016":
        return cmd_compare_016(argv[1:])
    if cmd == "synthetic":
        return cmd_synthetic(argv[1:])
    if cmd == "gaps":
        for clause, reading in SPEC_GAPS:
            print("- %s: %s" % (clause, reading))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.modules.setdefault("p3_peoples_reference", sys.modules["__main__"])   # one module object when run as a script (P5-04a integration)
    sys.exit(main(sys.argv[1:]))
