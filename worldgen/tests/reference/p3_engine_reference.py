#!/usr/bin/env python3
"""Independent exact-integer reference of P3-04, the simulation engine.

Written by the test writer from docs/specs/p3-04-simulation-engine.md alone
(sections 3.4 entry checks, 4 tuning and catalogue data, 5 the scheduler
SC1-SC6 with E1-E10, 6 the engine: awakening and flare-up steps, quiet
eras, bookkeeping, 7 the stub subsystems S1-S4, 8 the event catalogue, the
template grammar and the rendering algorithm, 9 schema 7, 11.1 the
[history] report section) and the binding parts of P3-00 (H1-H8, 4.6, 6.2,
12.3), without reading any implementation and without the spec writer's
prototype. The template grammar (8.4) and the rendering algorithm (8.5) are
implemented here from the spec text; nothing is ported from C++.

Libraries reused, each written independently from its own spec:
core_reference.py (P0-01: sha256, stream_key, phrase_seed),
p3_data_reference.py (P0-03/P3-01: data roots with tomllib),
p3_names_reference.py (P3-02: make_name with the registry),
p3_chronology_reference.py (P3-03: the schedule, the five chronology
tables, table hashes, worlds read with sqlite3), p0_world_check.py (P0-02
layer blob decoding).

The listings of spec 4.1 and 4.2 are transcribed below (ENGINE_TOML,
REPORT_TOML); `check` compares the data files on disk (read through
p3_data_reference, which holds the kind schemas) with them.

Python 3 standard library only, arbitrary-precision integers, no floating
point.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p3_engine_reference.py check
        Self-checks against the worked example of 17 (phases, events,
        homes, names, region_control, census, sentences, the [history]
        lines of 11.1, both ForcedTension variants), the edge cases
        E1-E10, the guards, the template grammar, a scheduler property
        sweep, and the committed golden file against a fresh computation.
        Exit 1 on any disagreement.

    python3 -B worldgen/tests/reference/p3_engine_reference.py golden [PATH|-] [--inputs DIR]
        Writes worldgen/tests/golden/p3-04-engine.txt ("-": stdout). The
        world cases read their geography from DIR (default
        ~/.cache/the-unmapped/out/p3-04-tests/inputs), one <case>.db per
        world case (any generator whose table `region` has the hash of
        p1-06-regions.txt for the four P1 cases).

    python3 -B worldgen/tests/reference/p3_engine_reference.py schedule SEED SPAN [DETAIL] [RACES] [--tension PERIOD,VALUE,TRIGGER]
        Prints the phase table of one schedule on the shipped data.

    python3 -B worldgen/tests/reference/p3_engine_reference.py world DB [--data ROOT] [--tension PERIOD,VALUE,TRIGGER]
        Recomputes the stub history of a world whose geography and
        chronology tables are stored (any schema >= 6, any stages from
        chronology on) and prints the table hashes, the [history] lines
        and every sentence.

    python3 -B worldgen/tests/reference/p3_engine_reference.py check-world DB [--data ROOT]
        Checks a schema-7 world written by the C++ (validation sweep):
        recomputes the five chronology tables (P3-03 reference) and the
        thirteen history tables from the stored geography, chronology
        tables and meta, compares all eighteen table by table, re-checks
        the invariants of 12 (1-6) from the stored rows, and checks that
        every stored template parses. Exit 1 on a difference.

Never regenerate the golden file to make a C++ test pass.
"""

import hashlib
import itertools
import os
import sqlite3
import sys
import tomllib

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import core_reference as cr  # noqa: E402
import p0_world_check as pwc  # noqa: E402
import p3_chronology_reference as pchr  # noqa: E402
import p3_data_reference as pdr  # noqa: E402
import p3_names_reference as pnr  # noqa: E402

FatalError = cr.FatalError
M64 = (1 << 64) - 1
REPO = os.path.normpath(os.path.join(_HERE, "..", "..", ".."))
GOLDEN = os.path.join(REPO, "worldgen", "tests", "golden", "p3-04-engine.txt")
DEFAULT_INPUTS = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p3-08-tests", "inputs")
GENERATOR_VERSION = "0.20.0"  # P3-11: kinds 39, 40, number words (P3-10: kinds 37, 38, quiet stretches, {@causes:...}, separators (P3-09: 36 kinds)
SCHEMA_VERSION = 7


def fatal(msg):
    raise FatalError(msg)


# ---------------------------------------------------------------------------
# 4.1, 4.2: the two new history definitions, transcribed from the spec
# ---------------------------------------------------------------------------

ENGINE_TOML = """\
[history.engine]
quiet_step_years = 25
quiet_events_max = 3
simulated_min_permille = 100
simulated_max_permille = 450
islet_max_dry_cells = 1172
sparse = { active_years = 75, flare_years = 30, final_years = 250, flare_threshold = 1500 }
normal = { active_years = 150, flare_years = 50, final_years = 300, flare_threshold = 1000 }
dense = { active_years = 250, flare_years = 75, final_years = 400, flare_threshold = 700 }
"""

REPORT_TOML = """\
[history.report]
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
  { kind = "civ_seceded", salience = 70, template = "{civilization} broke away from {from}, taking {settlements} \
{settlements?settlement|settlements} with it." },
  { kind = "settlement_abandoned", salience = 55, template = "{settlement} was abandoned; {survivors} \
{survivors?survivor|survivors} left." },
  { kind = "capital_moved", salience = 50, template = "{civilization} moved its seat to {settlement}." },
  { kind = "civ_ended", salience = 85, template = "{civilization} came to an end." },
  { kind = "ruin_faded", salience = 10, template = "The ruins of {settlement} faded from memory." },
  { kind = "claim", salience = 45, template = "{claimant} laid claim to {@region}, held by {holder}." },
  { kind = "war_declared", salience = 70, template = "{attacker} declared war on {defender} over {@region}." },
  { kind = "battle", salience = 30, template = "{winner} defeated {loser} in {@region}; {loser_dead} of the defeated \
and {winner_dead} of the victors fell." },
  { kind = "battle_at_pass", salience = 35, template = "{winner} defeated {loser} at the pass into {@region}; \
{loser_dead} of the defeated and {winner_dead} of the victors fell." },
  { kind = "siege", salience = 25, template = "{attacker} besieged {@settlement} of {defender}; it {fell?fell|held}." },
  { kind = "sacked", salience = 75, template = "{attacker} sacked {@settlement} of {defender}; {dead} died and \
{survivors} fled." },
  { kind = "settlement_captured", salience = 60, template = "{attacker} took {@settlement} from {defender}; {dead} \
died and {fled} fled." },
  { kind = "peace", salience = 60, template = "{loser} sued {winner} for peace after {years} {years?year|years} of war." },
  { kind = "truce", salience = 50, template = "{parties:all} made peace after {years} {years?year|years} of war, \
neither the victor." },
  { kind = "region_ceded", salience = 55, template = "{from} ceded {@region} to {to}." },
  { kind = "civ_conquered", salience = 90, template = "{civilization} fell to {conqueror}." },
]
"""

# Schema ranges of 4.1 (the loader's; the stage's seam guards repeat them,
# 3.4 item 2). The kind schemas themselves are p3_data_reference's.
ENGINE_RANGES = (("quiet_step_years", 1, 200), ("quiet_events_max", 1, 100), ("simulated_min_permille", 100, 500),
                 ("simulated_max_permille", 100, 500), ("islet_max_dry_cells", 0, 1000000))
DETAIL_RANGES = (("active_years", 1, 2000), ("flare_years", 1, 1000), ("final_years", 1, 3000),
                 ("flare_threshold", 1, 1000000))
DETAILS = ("sparse", "normal", "dense")
FALLBACK_USED = set()  # kept for the notes of cmd_check; the data files exist since the integration


class Data:
    """What the history stage reads from the content: chronology's view
    (P3-03 reference Data) plus settlement_kinds, history.engine and
    history.report."""

    def __init__(self, root):
        self.root = root
        self.chron = pchr.data_from_root(root)
        content = pdr.load_root(root, kinds=("history", "race"))
        races = dict(content["race"])
        self.settlement_kinds = {rid: list(t["settlement_kinds"]) for rid, t in races.items()}
        hist = dict(content["history"])
        self.engine = hist["engine"]
        self.report = [(e["kind"], e["salience"], e["template"]) for e in hist["report"]["events"]]
        self.naming = {cid: c["naming"] for cid, c in self.chron.cultures.items()}


_DATA = {}


def data(root="game/data"):
    if root not in _DATA:
        _DATA[root] = Data(root)
    return _DATA[root]


def spec_engine():
    """4.1's listing; from 0.19.0 (r15) with quiet_min_years = 50 after quiet_events_max."""
    t = ENGINE_TOML
    if QUIET_R15:
        t = t.replace("quiet_events_max = 3\n", "quiet_events_max = 3\nquiet_min_years = 50\n")
    return tomllib.loads(t)["history"]["engine"]


def spec_report():
    """The history.report listing of the active version: 0.16.0's is REPORT_TOML (and
    p3_data_reference's 0.16.0 listing); from 0.17.0 the data reference's listing,
    derived from it by the template edits of P3-04 r10 4.2, P3-05 r6 11 and P3-06 r7 11."""
    if at_least("0.17.0"):
        return [(e["kind"], e["salience"], e["template"]) for e in pdr.parsed_spec("history")["report"]["events"]]
    return [(e["kind"], e["salience"], e["template"]) for e in tomllib.loads(REPORT_TOML)["history"]["report"]["events"]]


# ---------------------------------------------------------------------------
# 8.2 the compiled catalogue
# ---------------------------------------------------------------------------

RACE, GOD, CIV, SETTLEMENT, REGION = 1, 2, 3, 4, 5      # 6.1 EntityType / 9.2 codes
OPENING, QUIET, FLARE, FINAL = 1, 2, 3, 4                # 6.1 PhaseKind
SETTLEMENT_KIND_CODE = {"hamlet": 1, "village": 2, "town": 3, "city": 4, "fortress": 5}
THRIVING = 1


def kind_def(kid, key, caused, region_use, settlement_use, roles, values):
    return dict(id=kid, key=key, caused=caused, region_use=region_use, settlement_use=settlement_use,
                roles=[dict(seq=i + 1, role=r, type=t, min=lo, max=hi) for i, (r, t, lo, hi) in enumerate(roles)],
                values=list(values), salience=0, templ="")


COMPILED = (
    kind_def(1, "awakening", 0, 1, 0, (("race", RACE, 1, 1), ("creator", GOD, 1, 1)), ()),
    kind_def(2, "civ_founded", 1, 1, 0, (("civilization", CIV, 1, 1), ("race", RACE, 1, 1)), ()),
    kind_def(3, "settlement_founded", 0, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("founder", CIV, 1, 1)),
             ("population",)),
    kind_def(4, "quiet_era", 0, 1, 0, (("civilization", CIV, 1, 1),), ("years", "generations")),
    kind_def(5, "flare_up", 1, 0, 0, (), ("years",)),
    # P3-05 11 (generator 0.15.0): kinds 6-19, region_use and settlement_use 1, every count 1-1.
    kind_def(6, "colony_founded", 0, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("founder", CIV, 1, 1),
                                            ("origin", SETTLEMENT, 1, 1)), ("population",)),
    kind_def(7, "became_village", 1, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("civilization", CIV, 1, 1)),
             ("population",)),
    kind_def(8, "became_town", 1, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("civilization", CIV, 1, 1)),
             ("population",)),
    kind_def(9, "became_city", 1, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("civilization", CIV, 1, 1)),
             ("population",)),
    kind_def(10, "settlement_dwindled", 0, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("civilization", CIV, 1, 1)),
             ("population",)),
    kind_def(11, "famine", 0, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("civilization", CIV, 1, 1)),
             ("dead", "population")),
    kind_def(12, "plague", 0, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("civilization", CIV, 1, 1)),
             ("dead", "population")),
    kind_def(13, "plague_spread", 1, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("civilization", CIV, 1, 1)),
             ("dead", "population")),
    kind_def(14, "succession_crisis", 0, 1, 1, (("civilization", CIV, 1, 1),), ()),
    kind_def(15, "civ_seceded", 1, 1, 1, (("civilization", CIV, 1, 1), ("from", CIV, 1, 1)), ("settlements",)),
    kind_def(16, "settlement_abandoned", 1, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("civilization", CIV, 1, 1)),
             ("survivors",)),
    kind_def(17, "capital_moved", 1, 1, 1, (("civilization", CIV, 1, 1), ("settlement", SETTLEMENT, 1, 1)), ()),
    kind_def(18, "civ_ended", 1, 1, 1, (("civilization", CIV, 1, 1),), ()),
    kind_def(19, "ruin_faded", 1, 1, 1, (("settlement", SETTLEMENT, 1, 1),), ()),
    # P3-06 11 (generator 0.16.0): kinds 20-30, every kind caused, region_use and settlement_use as the
    # table's columns, every count 1-1 except parties 2-2.
    kind_def(20, "claim", 1, 1, 0, (("claimant", CIV, 1, 1), ("holder", CIV, 1, 1)), ()),
    kind_def(21, "war_declared", 1, 1, 0, (("attacker", CIV, 1, 1), ("defender", CIV, 1, 1)), ()),
    kind_def(22, "battle", 1, 1, 0, (("winner", CIV, 1, 1), ("loser", CIV, 1, 1)), ("winner_dead", "loser_dead")),
    kind_def(23, "battle_at_pass", 1, 1, 0, (("winner", CIV, 1, 1), ("loser", CIV, 1, 1)),
             ("winner_dead", "loser_dead", "pass")),
    kind_def(24, "siege", 1, 1, 1, (("attacker", CIV, 1, 1), ("defender", CIV, 1, 1)), ("fell",)),
    kind_def(25, "sacked", 1, 1, 1, (("attacker", CIV, 1, 1), ("defender", CIV, 1, 1)), ("dead", "survivors")),
    kind_def(26, "settlement_captured", 1, 1, 1, (("attacker", CIV, 1, 1), ("defender", CIV, 1, 1)),
             ("dead", "fled")),
    kind_def(27, "peace", 1, 0, 0, (("winner", CIV, 1, 1), ("loser", CIV, 1, 1)), ("years",)),
    kind_def(28, "truce", 1, 0, 0, (("parties", CIV, 2, 2),), ("years",)),
    kind_def(29, "region_ceded", 1, 1, 0, (("from", CIV, 1, 1), ("to", CIV, 1, 1)), ()),
    kind_def(30, "civ_conquered", 1, 1, 0, (("civilization", CIV, 1, 1), ("conqueror", CIV, 1, 1)), ()),
)
# P3-05 r6 11 (generator 0.17.0): kind 31, caused, region_use and
# settlement_use 1, every count 1-1.
KIND_31 = kind_def(31, "settlement_abandoned_to", 1, 1, 1, (("settlement", SETTLEMENT, 1, 1),
                                                            ("civilization", CIV, 1, 1),
                                                            ("destination", SETTLEMENT, 1, 1)), ("settled", "lost"))
# P3-06 r9 20.9 (generator 0.18.0, P3-09): kinds 32 and 33, caused, every count 1-1.
KIND_32 = kind_def(32, "refugees_settled", 1, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("founder", CIV, 1, 1),
                                                     ("origin", SETTLEMENT, 1, 1)), ("population",))
KIND_33 = kind_def(33, "tribute", 1, 0, 0, (("payer", CIV, 1, 1), ("payee", CIV, 1, 1)), ("people",))
# P3-06 r10 20.15, 20.17, 20.22 (P3-09 r3): kinds 34-36, caused, every count 1-1.
KIND_34 = kind_def(34, "war_won", 1, 0, 0, (("winner", CIV, 1, 1), ("loser", CIV, 1, 1)), ("years",))
KIND_35 = kind_def(35, "search_abandoned", 1, 1, 0, (("searcher", CIV, 1, 1), ("hidden", CIV, 1, 1)), ("years",))
KIND_36 = kind_def(36, "refuge_held", 1, 1, 1, (("attacker", CIV, 1, 1), ("defender", CIV, 1, 1)), ("reason",))
COMPILED_016 = COMPILED
COMPILED_017 = COMPILED + (KIND_31,)
COMPILED_018 = COMPILED_017 + (KIND_32, KIND_33, KIND_34, KIND_35, KIND_36)
# P3-04 r12 20.4 (generator 0.19.0, P3-10): kind 37 plague_outbreak (P3-05 r12 22.1) and kind 38 quiet_stretch
# (20.1; role realm 1-0, unbounded). Kinds 4 and 12 keep their rows and are no longer written.
KIND_37 = kind_def(37, "plague_outbreak", 0, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("civilization", CIV, 1, 1)),
                   ("dead", "population", "total", "reached"))
KIND_38 = kind_def(38, "quiet_stretch", 0, 0, 0, (("realm", CIV, 1, 0),), ("years", "realms"))
COMPILED_019 = COMPILED_018 + (KIND_37, KIND_38)
# P3-04 r17 21.2 (generator 0.20.0, P3-11): kinds 39 claim_settlements and 40 settlements_ceded, caused, with a
# region, no settlement column, roles (claimant | from, holder | to, civilization 1-1; settlement, settlement 1-0
# unbounded), no value.
KIND_39 = kind_def(39, "claim_settlements", 1, 1, 0, (("claimant", CIV, 1, 1), ("holder", CIV, 1, 1),
                                                      ("settlement", SETTLEMENT, 1, 0)), ())
KIND_40 = kind_def(40, "settlements_ceded", 1, 1, 0, (("from", CIV, 1, 1), ("to", CIV, 1, 1),
                                                      ("settlement", SETTLEMENT, 1, 0)), ())
COMPILED_020 = COMPILED_019 + (KIND_39, KIND_40)

# P3-04 r10 (generator 0.17.0, P3-08), each switchable so that the 0.16.0
# goldens can still be reproduced (use_version("0.16.0")):
#   RENDER_R10      8.4/8.5: the three-word value form {v?w0|w1|w2} with #,
#                   and a leading "The " of a name lower-cased mid-sentence;
#   QUIET_ERA_R10   6.6: events charged to a civilization do not close its
#                   era (one era per civilization and quiet phase row). The
#                   stub engine charges no event, so only the subsystems'
#                   quiet_step (P3-05, P3-06 references) read it.
RENDER_R10 = True
QUIET_ERA_R10 = True
# P3-04 r12 (generator 0.19.0, P3-10):
#   RENDER_R12      20.2 the placeholder {@causes:...} and 20.3 thousands separators;
#   QUIET_R12       20.1 one world-level quiet_stretch per quiet phase row in place of the quiet_era of
#                   every civilization (stub, and quiet_stretch_step() for the peoples and conflict
#                   references).
RENDER_R12 = True
QUIET_R12 = True
MAX_PARTICIPANTS = 64   # P3-04 r13 20.1: the event writer's bound on a quiet_stretch's participants
# P3-04 r15 (P3-10 second round, generator 0.19.0):
#   QUIET_R15       20.1 item 1a: no quiet_stretch when the stretch's assured length A (Engine.assured) is below
#                   history.engine.quiet_min_years (default 0 for hand-made tuning without the key); the row is
#                   then decided without a stretch (open_era[-1]); entry check engine_quiet_min.
QUIET_R15 = True
# P3-04 r17 (P3-11, generator 0.20.0):
#   RENDER_R17      21.1 values 1 to 9 render as words in {value} and in the # of a chosen three-word form.
RENDER_R17 = True
NUMBER_WORDS = ("one", "two", "three", "four", "five", "six", "seven", "eight", "nine")
ENGINE_VERSION = None


VERSIONS = ("0.16.0", "0.17.0", "0.18.0", "0.19.0", "0.20.0")


def at_least(v):
    """True when the served generator version is v or later."""
    return VERSIONS.index(ENGINE_VERSION) >= VERSIONS.index(v)


def use_version(v):
    """Serve generator v's rules and data: "0.18.0" (default; P3-09: the
    catalogue of 33 kinds and the 0.18.0 data), "0.17.0" (P3-08: 31 kinds,
    r10 rendering and quiet eras, P3-02 r5 names) or "0.16.0" (the catalogue
    of 30 kinds, r9 rendering and quiet eras, the 0.16.0 data and P3-02 r4
    names). Clears the cached data roots."""
    global COMPILED, RENDER_R10, QUIET_ERA_R10, RENDER_R12, QUIET_R12, QUIET_R15, GENERATOR_VERSION, ENGINE_VERSION
    global RENDER_R17
    if v not in VERSIONS:
        raise ValueError(v)
    new = v != "0.16.0"
    COMPILED = {"0.16.0": COMPILED_016, "0.17.0": COMPILED_017, "0.18.0": COMPILED_018, "0.19.0": COMPILED_019,
                "0.20.0": COMPILED_020}[v]
    RENDER_R10 = QUIET_ERA_R10 = new
    RENDER_R12 = QUIET_R12 = QUIET_R15 = v in ("0.19.0", "0.20.0")
    RENDER_R17 = v == "0.20.0"
    GENERATOR_VERSION = v
    ENGINE_VERSION = v
    pdr.use_version(v)
    # P3-09 item 8: the word list of the version (0.18.0 on disk; before, P3-08's, from git)
    pnr.FILTER_COMMIT = None if v in ("0.18.0", "0.19.0", "0.20.0") else pdr.NAME_FILTERS_017_COMMIT
    if hasattr(pnr, "use_version"):   # P3-10: the names reference serves the filter of the version (F1)
        pnr.use_version(v)
    pnr._SHIPPED_FILTER = None
    pnr.use_rules("r5" if new else "r4")
    pchr._SHIPPED = None
    _DATA.clear()


P3_06_KEYS = ['claim', 'war_declared', 'battle', 'battle_at_pass', 'siege', 'sacked', 'settlement_captured',
              'peace', 'truce', 'region_ceded', 'civ_conquered']
P3_05_KEYS = ['colony_founded', 'became_village', 'became_town', 'became_city', 'settlement_dwindled', 'famine', 'plague', 'plague_spread', 'succession_crisis', 'civ_seceded', 'settlement_abandoned', 'capital_moved', 'civ_ended', 'ruin_faded']


def compiled_catalogue():
    import copy
    return copy.deepcopy(list(COMPILED))


def event_catalogue(report):
    """compiled + history.report (8.1): salience and template per kind."""
    cat = compiled_catalogue()
    by = {k: (s, t) for k, s, t in report}
    for k in cat:
        if k["key"] in by:
            k["salience"], k["templ"] = by[k["key"]]
    return cat


def find_kind(cat, key):
    for k in cat:
        if k["key"] == key:
            return k
    return None


# ---------------------------------------------------------------------------
# 8.4 the template grammar: template_problem
# ---------------------------------------------------------------------------

FORMS = ("name", "plural", "adjective", "all")


def _is_word(w):
    return all(c not in "{}|?" for c in w)


CAUSE_KEY_FIRST = set("abcdefghijklmnopqrstuvwxyz")
CAUSE_KEY_REST = CAUSE_KEY_FIRST | set("0123456789_")


def causes_entries(body):
    """P3-04 r12 20.2: [(key, cword)], word1, word2 of a body "@causes:..." matching the grammar, or None.
    Words follow 8.4's word rule (no {, }, |, ?); a word may be empty, a cword may not."""
    rest = body[len("@causes:"):]
    parts = rest.split("|")
    if len(parts) != 3:
        return None
    ents, w1, w2 = parts
    if not (_is_word(w1) and _is_word(w2)):
        return None
    out, seen = [], set()
    for ent in ents.split(","):
        if ent.count("=") != 1:
            return None
        key, cw = ent.split("=")
        if not key or key[0] not in CAUSE_KEY_FIRST or any(ch not in CAUSE_KEY_REST for ch in key):
            return None
        if not cw or any(ch in "{}|?,=" for ch in cw):
            return None
        if key in seen:
            return None
        seen.add(key)
        out.append((key, cw))
    return out, w1, w2


def fmt_value(v):
    """P3-04 r12 20.3: a comma between groups of three digits from 1,000 (after the sign); as before
    without RENDER_R12."""
    if RENDER_R17 and 1 <= v <= 9:   # r17 21.1: one to nine as lower-case words
        return NUMBER_WORDS[v - 1]
    if not RENDER_R12 or abs(v) < 1000:
        return str(v)
    d = str(abs(v))
    groups = []
    while d:
        groups.insert(0, d[-3:])
        d = d[:-3]
    return ("-" if v < 0 else "") + ",".join(groups)


def placeholder_problem(kind, body, at):
    """The problem of one placeholder body, or None (8.4)."""
    roles = {r["role"]: r for r in kind["roles"]}
    values = set(kind["values"])
    unknown = "unknown placeholder '{%s}' at byte %d" % (body, at)
    if body.startswith("@"):
        if body in ("@year", "@end_year"):
            return None
        if RENDER_R12 and body.startswith("@causes:"):   # r12 20.2, any kind, caused or not
            return None if causes_entries(body) is not None else unknown
        if body == "@region":
            return None if kind["region_use"] == 1 else "{@region} needs a kind with a region at byte %d" % at
        if body == "@settlement":
            return (None if kind["settlement_use"] == 1
                    else "{@settlement} needs a kind with a settlement at byte %d" % at)
        return unknown
    if "?" in body:
        v, rest = body.split("?", 1)
        bars = rest.count("|")
        if v not in values or not (bars == 1 or (bars == 2 and RENDER_R10)):
            return unknown
        if not all(_is_word(w) for w in rest.split("|")):
            return unknown
        return None
    if "|" in body:
        return unknown
    if ":" in body:
        role, form = body.split(":", 1)
        if role not in roles or form not in FORMS:
            return unknown
        r = roles[role]
        if form in ("plural", "adjective") and r["type"] != RACE:
            return "form '%s' needs a race role at byte %d" % (form, at)
        if form != "all" and r["min"] == 0:
            return "role '%s' may have no participant at byte %d" % (role, at)
        return None
    if body in roles:
        if roles[body]["min"] == 0:
            return "role '%s' may have no participant at byte %d" % (body, at)
        return None
    if body in values:
        return None
    return unknown


def template_problem(kind, text):
    """8.4: the first problem scanning left to right, or None. `text` is
    bytes or str. Reading: a non-ASCII byte is reported where the scan
    meets it, also inside a placeholder body (the body is judged when its
    closing brace is reached)."""
    b = text.encode("utf-8") if isinstance(text, str) else bytes(text)
    n = len(b)
    i = 0
    while i < n:
        c = b[i]
        if c >= 0x80:
            return "non-ASCII byte at byte %d" % i
        if c == 0x7B:  # {
            if i + 1 < n and b[i + 1] == 0x7B:
                i += 2
                continue
            j = i + 1
            while j < n and b[j] not in (0x7B, 0x7D):
                if b[j] >= 0x80:
                    return "non-ASCII byte at byte %d" % j
                j += 1
            if j >= n or b[j] == 0x7B:
                return "unclosed placeholder at byte %d" % i
            p = placeholder_problem(kind, b[i + 1:j].decode("ascii"), i)
            if p is not None:
                return p
            i = j + 1
            continue
        if c == 0x7D:  # }
            if i + 1 < n and b[i + 1] == 0x7D:
                i += 2
                continue
            return "unmatched } at byte %d" % i
        i += 1
    if n == 0:
        return "the template is empty"
    return None


def report_events_messages(report):
    """4.2 named check report_events: every message, in order."""
    out = []
    first = {}
    keys = {k["key"] for k in COMPILED}
    for i, (key, _sal, templ) in enumerate(report):
        if key not in keys:
            out.append("events[%d]: kind '%s' is not an event kind" % (i, key))
            continue
        if key in first:
            out.append("events[%d]: kind '%s' is already listed at events[%d]" % (i, key, first[key]))
            continue
        first[key] = i
        p = template_problem(find_kind(COMPILED, key), templ)
        if p is not None:
            out.append("events[%d]: template: %s" % (i, p))
    for k in COMPILED:
        if k["key"] not in first:
            out.append("event kind '%s' has no entry" % k["key"])
    return out


# ---------------------------------------------------------------------------
# 8.5 rendering
# ---------------------------------------------------------------------------

def join_all(names):
    if not names:
        return ""
    if len(names) == 1:
        return names[0]
    return ", ".join(names[:-1]) + " and " + names[-1]


def render(view, eid):
    """render_event(view, id). view: dict with catalogue (by id), events (by
    id: year, end_year, kind, region, settlement, participants [(seq, role
    seq, type, id)], values {seq: v}) and names: name(type, id, form)."""
    if eid not in view["events"]:
        fatal("render_event: event %d: no such event" % eid)
    ev = view["events"][eid]
    kind = view["catalogue"][ev["kind"]]
    templ = kind["templ"]
    p = template_problem(kind, templ)
    if p is not None:
        fatal("render_event: event %d: %s" % (eid, p))
    role_seq = {r["role"]: r["seq"] for r in kind["roles"]}
    val_seq = {k: i + 1 for i, k in enumerate(kind["values"])}
    parts = sorted(ev["participants"])

    def of_role(role):
        return [(t, x) for (_s, rs, t, x) in parts if rs == role_seq[role]]

    out = []

    def put_name(nm):
        # 8.5 r10: a leading "The " of a name appended mid-sentence becomes "the ".
        if RENDER_R10 and nm.startswith("The "):
            so_far = "".join(out)
            if not (so_far == "" or so_far.endswith(". ")):
                nm = "the " + nm[4:]
        out.append(nm)

    b = templ
    i = 0
    n = len(b)
    while i < n:
        c = b[i]
        if c == "{" and b[i + 1] == "{":
            out.append("{")
            i += 2
            continue
        if c == "}" and b[i + 1] == "}":
            out.append("}")
            i += 2
            continue
        if c != "{":
            out.append(c)
            i += 1
            continue
        j = b.index("}", i)
        body = b[i + 1:j]
        i = j + 1
        if body == "@year":
            out.append(str(ev["year"]))
        elif body == "@end_year":
            out.append(str(ev["end_year"]))
        elif body == "@region":
            put_name(view["name"](REGION, ev["region"], "name"))
        elif body == "@settlement":
            put_name(view["name"](SETTLEMENT, ev["settlement"], "name"))
        elif RENDER_R12 and body.startswith("@causes:"):
            # r12 20.2: L = the entries' cwords of the causes' kind keys in ascending cause id, first
            # occurrence kept; nothing when L is empty; else word1, L joined as {role:all}, word2.
            ents, w1, w2 = causes_entries(body)
            first = {}
            for key, cw in ents:
                first.setdefault(key, cw)
            lst = []
            for cc in sorted(ev.get("causes", ())):
                key = view["catalogue"][view["events"][cc]["kind"]]["key"]
                cw = first.get(key)
                if cw is not None and cw not in lst:
                    lst.append(cw)
            if lst:
                out.append(w1 + join_all(lst) + w2)
        elif "?" in body:
            v, rest = body.split("?", 1)
            ws = rest.split("|")
            x = ev["values"][val_seq[v]]
            if len(ws) == 3:   # r10: w0 for 0, w1 for 1, w2 otherwise; # is the value in the chosen word
                out.append((ws[0] if x == 0 else ws[1] if x == 1 else ws[2]).replace("#", fmt_value(x)))
            else:
                out.append(ws[0] if x == 1 else ws[1])
        else:
            role, form = (body.split(":", 1) + ["name"])[:2] if ":" in body else (body, None)
            if form is None:
                if body in role_seq:
                    form = "name"
                else:
                    out.append(fmt_value(ev["values"][val_seq[body]]))
                    continue
            ps = of_role(role)
            if form == "all":
                # each name judged at the offset where it is appended (8.5 r10)
                for k, (t, x) in enumerate(ps):
                    if k > 0:
                        out.append(" and " if k == len(ps) - 1 else ", ")
                    put_name(view["name"](t, x, "name"))
            elif form == "name":
                t, x = ps[0]
                put_name(view["name"](t, x, form))
            else:
                t, x = ps[0]
                out.append(view["name"](t, x, form))
    return "".join(out)


# ---------------------------------------------------------------------------
# 5 the scheduler
# ---------------------------------------------------------------------------

def scale(v, e):
    return max(1, (v * e + 3000) // 6000)


def lengths(e, eng, detail):
    d = eng[detail]
    return dict(A=scale(d["active_years"], e), F=scale(d["flare_years"], e), L=scale(d["final_years"], e),
                Q=eng["quiet_step_years"],
                # lead's ruling (spec defect 5.1/5.4): the floor is clamped to the cap
                floor=min((eng["simulated_min_permille"] * e + 999) // 1000, (eng["simulated_max_permille"] * e) // 1000),
                cap=(eng["simulated_max_permille"] * e) // 1000, threshold=d["flare_threshold"])


def base_schedule(starts, e, eng, detail):
    """SC1-SC4. starts: y_1 = 0 < ... < y_P. Returns dict with the lengths,
    openings [(y_k, e_k)], S_f, sched."""
    ln = lengths(e, eng, detail)
    a, big_l = ln["A"], ln["L"]
    p = len(starts)
    y = list(starts)
    ends = []
    for k in range(p - 1):
        ends.append(min(y[k] + a, y[k + 1]))
    if e - y[p - 1] >= 2:
        ends.append(min(y[p - 1] + a, e - 1))
        s_f = max(e - big_l, ends[p - 1])
    else:
        ends.append(e)
        s_f = e
    o = sum(ends[k] - y[k] for k in range(p))
    sched = o + (e - s_f)
    dd = ln["floor"] - sched
    if dd > 0 and s_f < e:
        s2 = max(ends[p - 1], s_f - dd)
        dd -= s_f - s2
        s_f = s2
    for k in range(p - 1):
        if dd <= 0:
            break
        e2 = min(y[k + 1], ends[k] + dd)
        dd -= e2 - ends[k]
        ends[k] = e2
    sched = sum(ends[k] - y[k] for k in range(p)) + (e - s_f)
    if sched > ln["cap"]:
        fatal("history: schedule: scheduled active years %d above the cap %d" % (sched, ln["cap"]))
    return dict(ln, openings=list(zip(y, ends)), S_f=s_f, sched=sched, E=e, D=dd)


def plan(base):
    """The base schedule as segments in time order: ('opening', k, s, e),
    ('quiet', k, s, e) when non-empty, ('final', s, e) (half-open)."""
    segs = []
    op = base["openings"]
    p = len(op)
    for k, (yk, ek) in enumerate(op):
        segs.append(("opening", k, yk, ek))
        qe = op[k + 1][0] if k + 1 < p else base["S_f"]
        if ek < qe:
            segs.append(("quiet", k, ek, qe))
    if base["S_f"] < base["E"]:
        segs.append(("final", p - 1, base["S_f"], base["E"]))
    return segs


class ForcedTension:
    """7.4 S4: `value` when the quiet step's end year (start + length) is a
    multiple of `period`, else 0; trigger the fixed event id."""

    def __init__(self, period, value, trigger):
        self.period, self.value, self.trigger = period, value, trigger

    def __call__(self, start, length):
        return (self.value, self.trigger) if (start + length) % self.period == 0 else (0, self.trigger)

    def label(self):
        return "ForcedTension(%d, %d, %d)" % (self.period, self.value, self.trigger)


class Hooks:
    """Schedule-only run: no events."""

    def __init__(self):
        self.events = []

    def awakening(self, eng, k, year):
        return 0

    def flare_start(self, eng, year, length, trigger):
        pass

    def tick(self, eng, year, phase_end):
        pass

    def quiet_step(self, eng, start, length, phase_end, row_start):
        pass

    def last_event_id(self):
        return None


class Engine:
    """5.5 SC5/SC6 and the phase rows of 5.7, driving the hooks of one run."""

    def __init__(self, starts, ages_end, e, eng, detail, tension=None, hooks=None):
        self.base = base_schedule(starts, e, eng, detail)
        self.starts = list(starts)
        self.age_ends = list(ages_end)
        self.E = e
        self.tension = tension
        self.hooks = hooks or Hooks()
        self.phases = []   # [id, age, kind, start, end, trigger]
        self.flare_total = 0
        self.phase_id = 0
        self.phase_kind = 0

    def age_of(self, year):
        for i, (s, en) in enumerate(zip(self.starts, self.age_ends)):
            if s <= year <= en:
                return i + 1
        fatal("history: schedule: year %d in no age" % year)

    def open_phase(self, kind, start):
        self.phase_id = len(self.phases) + 1
        self.phase_kind = kind
        self.phases.append([self.phase_id, self.age_of(start), kind, start, None, 0])
        return self.phases[-1]

    def close_phase(self, row, end):
        if end < row[3]:
            fatal("history: schedule: phase %d ends before it starts" % row[0])
        row[4] = end

    def run_active(self, kind, s, e, awaken_k=None, flare_trigger=None):
        row = self.open_phase(kind, s)
        if kind == FLARE:
            row[5] = flare_trigger
        for year in range(s, e):
            if awaken_k is not None and year == s:
                row[5] = self.hooks.awakening(self, awaken_k, year)
            if kind == FLARE and year == s:
                self.hooks.flare_start(self, year, e - s, flare_trigger)
            self.hooks.tick(self, year, year == e - 1)
        self.close_phase(row, e - 1)

    def assured(self, s, ln, closes):
        """P3-04 r15 20.1 item 1a: the assured length A of the stretch from the step [s, s + ln) of the quiet
        interval ending at q_e (self.q_end): ln when the step is the row's last (closes), else ln + min(Q, q_e -
        (s + ln))."""
        return ln if closes else ln + min(self.base["Q"], self.q_end - (s + ln))

    def run_quiet(self, qs, qe):
        b = self.base
        self.q_end = qe
        cur = qs
        while cur < qe:
            row = self.open_phase(QUIET, cur)
            s = cur
            while True:
                ln = min(b["Q"], qe - s)
                if ln < 1:
                    fatal("history: schedule: quiet step length %d below 1" % ln)
                end = s + ln
                flare = 0
                trig = 0
                if end != qe and self.tension is not None:   # Q5: not asked at the interval's end (SC6)
                    tv, trig = self.tension(s, ln)
                    if tv < 0:
                        fatal("history: tension: value %d below 0" % tv)
                    if tv >= b["threshold"]:
                        last = self.hooks.last_event_id()
                        if last is not None and not (1 <= trig <= last):
                            fatal("history: tension: trigger %d outside 1 to %d" % (trig, last))
                        flare = min(b["F"], qe - end, b["cap"] - b["sched"] - self.flare_total)
                closes = end == qe or flare >= 1
                self.hooks.quiet_step(self, s, ln, closes, row[3])
                s = end
                if s == qe:
                    self.close_phase(row, s - 1)
                    cur = qe
                    break
                if flare >= 1:
                    if not (1 <= flare <= qe - s):
                        fatal("history: schedule: flare length %d outside 1 to %d" % (flare, qe - s))
                    self.close_phase(row, s - 1)
                    self.run_active(FLARE, s, s + flare, flare_trigger=trig)
                    self.flare_total += flare
                    cur = s + flare
                    break

    def run(self):
        for seg in plan(self.base):
            if seg[0] == "opening":
                self.run_active(OPENING, seg[2], seg[3], awaken_k=seg[1])
            elif seg[0] == "quiet":
                self.run_quiet(seg[2], seg[3])
            else:
                self.run_active(FINAL, seg[2], seg[3])
        self.check_tiling()
        return self

    def simulated(self):
        return sum(r[4] - r[3] + 1 for r in self.phases if r[2] in (OPENING, FLARE, FINAL))

    def check_tiling(self):
        prev = -1
        for r in self.phases:
            if r[3] != prev + 1 or r[4] < r[3]:
                fatal("history: schedule: phase %d does not tile" % r[0])
            prev = r[4]
        if prev != self.E - 1:
            fatal("history: schedule: the phases end at %d" % prev)
        sim = self.simulated()
        if not (self.base["floor"] <= sim <= self.base["cap"]):
            fatal("history: schedule: simulated years %d outside %d to %d" % (sim, self.base["floor"],
                                                                                 self.base["cap"]))


def age_ends(starts, e):
    return [starts[i + 1] - 1 for i in range(len(starts) - 1)] + [e - 1]


def schedule_phases(starts, e, eng, detail, tension=None):
    en = Engine(starts, age_ends(starts, e), e, eng, detail, tension).run()
    return en


def fmt_phases(phases, trigger=False):
    if trigger:
        return "|".join("%d:%d:%d:%d:%d:%d" % tuple(r) for r in phases)
    return "|".join("%d:%d:%d:%d:%d" % tuple(r[:5]) for r in phases)


# ---------------------------------------------------------------------------
# 6, 7: the engine with the stub subsystems
# ---------------------------------------------------------------------------

class Inputs:
    """HistoryInputs (3.3) in the reference's terms."""

    def __init__(self, seed, map_size, params, regions, races, cultures, gods, ages, layers=None):
        self.seed = seed
        self.map_size = map_size
        self.params = params          # P3-03 reference params dict
        self.regions = regions        # [dict] id order: id name x_min y_min x_max y_max seed_x seed_y cells
                                      # lake_cells walk_cells habitability_mean
        self.races = races            # race rows (P3-03 reference tuples, 12 columns)
        self.cultures = cultures      # culture rows (id, key, name, race)
        self.gods = gods              # god rows (id, name, camp)
        self.ages = ages              # age rows (id, race, start, end, name)
        self.layers = layers          # callable () -> (region values, water values), row-major


def entry_checks(dat, inp, eng=None, report=None, stage_entry=True):
    """3.4 in order; returns the first message or None."""
    eng = eng if eng is not None else dat.engine
    report = report if report is not None else dat.report
    if stage_entry:
        if not inp.regions:
            return "history: input precondition: no region rows"
        if not inp.ages:
            return "history: input precondition: no age rows"
    for f, lo, hi in ENGINE_RANGES + ((("quiet_min_years", 0, 400),) if QUIET_R15 and "quiet_min_years" in eng
                                      else ()):
        if not (lo <= eng[f] <= hi):
            return "history: tuning: %s outside its schema range" % f
    for lvl in DETAILS:
        for f, lo, hi in DETAIL_RANGES:
            if not (lo <= eng[lvl][f] <= hi):
                return "history: tuning: %s.%s outside its schema range" % (lvl, f)
    if eng["simulated_min_permille"] > eng["simulated_max_permille"]:
        return "history: tuning: simulated_min_permille must not be above simulated_max_permille"
    if QUIET_R15 and eng.get("quiet_min_years", 0) > 2 * eng["quiet_step_years"]:   # r15 engine_quiet_min
        return "history: tuning: quiet_min_years must not be above 2 x quiet_step_years"
    for lvl in DETAILS:
        d = eng[lvl]
        if 6 * d["active_years"] + d["final_years"] > 6 * eng["simulated_max_permille"] - 30:
            return ("history: tuning: %s: 6 x active_years + final_years must not be above "
                    "6 x simulated_max_permille - 30" % lvl)
    msgs = report_events_messages(report)
    if msgs:
        return "history: report: " + msgs[0]
    e = inp.params["span"]
    if not (2000 <= e <= 12000):
        return "history: settings: history_span %d outside 2000 to 12000" % e
    present = [r[1] for r in inp.races if r[9] == 1]
    if list(inp.params["races"]) != present:
        return "history: settings: param.races does not match the present race rows"
    return None


def chronology_problem(inp):
    """3.4 item 5; the reason text is not fixed by the spec (only the prefix)."""
    e = inp.params["span"]
    ages = inp.ages
    if [a[0] for a in ages] != list(range(1, len(ages) + 1)):
        return "history: chronology: age ids"
    if ages[0][2] != 0:
        return "history: chronology: first start_year"
    for a, b in zip(ages, ages[1:]):
        if b[2] != a[3] + 1:
            return "history: chronology: start_year after end_year"
    if ages[-1][3] != e - 1:
        return "history: chronology: last end_year"
    present = {r[0]: r for r in inp.races if r[9] == 1}
    for a in ages:
        if a[1] not in present or present[a[1]][10] != a[2]:
            return "history: chronology: age race"
    gods = {g[0] for g in inp.gods}
    for r in present.values():
        if r[11] not in gods:
            return "history: chronology: god row"
    return None


class World:
    """The in-memory state (6.1) and the log, filled by the stub (7) and the
    engine's own steps (6.3, 6.6, 6.7)."""

    def __init__(self, dat, inp, cat, eng, tension=None, extra_draw=False):
        self.dat = dat
        self.inp = inp
        self.cat = cat
        self.cat_by_key = {k["key"]: k for k in cat}
        self.eng = eng
        self.events = []
        self.civs = []
        self.settlements = []
        self.owner = {r["id"]: (0, 0) for r in inp.regions}   # region -> (civ, event)
        self.region = {r["id"]: r for r in inp.regions}
        self.race = {r[0]: r for r in inp.races}
        self.culture = {c[0]: c for c in inp.cultures}
        self.god = {g[0]: g for g in inp.gods}
        self.age_race = [a[1] for a in inp.ages]
        self.registry = set(r["name"] for r in inp.regions) | set(a[4] for a in inp.ages) | \
            set(g[1] for g in inp.gods)
        self.registry_initial = set(self.registry)
        self.quiet_skipped = []       # r15 20.1 item 1a: (step start, A, phase) of every row decided without one
        self.cores = set()            # P3-02 r10 12.2 (0.19.0): K, the cores of the stage's civilization and
                                      # settlement names (the stub's too: it is the history stage)
        self.region_control = []      # [region, from, to, civ, event]
        self.rc_open = {}             # region -> index
        self.recorded_owner = {r["id"]: 0 for r in inp.regions}
        self.census = []              # (settlement, year, owner, kind, status, population)
        self.census_last = {}
        self.open_era = {}            # civ -> event index
        self.layers = None
        self.name_attempts = []
        # 6.1 relations and 6.7 item 2 (r9): the open relations, the calls since the last bookkeeping
        # in call order, and the rows
        self.relations = {}        # RelationKey -> opening event
        self.rel_log = []          # ("open" | "close", key, event)
        self.rel_rows = []         # [a_type, a_id, b_type, b_id, kind, since, until, since_event, until_event]
        self.rel_open_row = {}     # key -> index of its open row
        self.extra_draw = extra_draw

    # --- the event writer (6.6), with the checks the stub can reach
    def append(self, kind_key, year, end_year, region=0, settlement=0, participants=(), values=(), causes=(),
               phase=0, quiet_step=None):
        k = self.cat_by_key.get(kind_key)
        eid = len(self.events) + 1
        if k is None:
            fatal("history: event %d (%s): not in the catalogue" % (eid, kind_key))
        if self.events and year < self.events[-1]["year"]:
            fatal("history: event %d (%s): year below the previous event's" % (eid, kind_key))
        assert year <= end_year <= self.inp.params["span"] - 1
        assert len(values) == len(k["values"])
        assert all(1 <= c < eid for c in causes) and list(causes) == sorted(set(causes))
        assert (not k["caused"]) or causes
        parts = []
        for i, (role, t, x) in enumerate(participants):
            rs = [r for r in k["roles"] if r["role"] == role][0]
            assert rs["type"] == t
            parts.append((i + 1, rs["seq"], t, x))
        ev = dict(id=eid, year=year, end_year=end_year, kind=k["id"], phase=phase, region=region,
                  settlement=settlement, salience=k["salience"], participants=parts,
                  values={i + 1: v for i, v in enumerate(values)}, causes=list(causes), key=kind_key)
        self.events.append(ev)
        return eid

    # --- S1, S2
    def islet(self, r):
        return r["cells"] - r["lake_cells"] < self.eng["islet_max_dry_cells"]

    def awakening_place(self, race_id):
        regs = self.inp.regions
        for pool in ([r for r in regs if not self.islet(r) and self.owner[r["id"]][0] == 0],
                     [r for r in regs if not self.islet(r)], list(regs)):
            if pool:
                return min(pool, key=lambda r: (-r["habitability_mean"], -r["walk_cells"], r["id"]))["id"]
        fatal("history: stub: no region")

    def site(self, rid):
        r = self.region[rid]
        taken = {(s["x"], s["y"]) for s in self.settlements}
        if (r["seed_x"], r["seed_y"]) not in taken:
            return r["seed_x"], r["seed_y"], "seed"
        if self.layers is None:
            self.layers = self.inp.layers()
        reg, wat = self.layers
        n = self.inp.map_size
        for y in range(r["y_min"], r["y_max"] + 1):
            for x in range(r["x_min"], r["x_max"] + 1):
                if reg[y * n + x] == rid and wat[y * n + x] != 2 and (x, y) not in taken:
                    return x, y, "row-major"
        fatal("history: stub: no settlement site in region %d" % rid)

    def awaken(self, eng_run, k, year):
        race_id = self.age_race[k]
        race = self.race[race_id]
        place = self.awakening_place(race_id)
        phase = eng_run.phase_id
        e_aw = self.append("awakening", year, year, region=place,
                           participants=(("race", RACE, race_id), ("creator", GOD, race[11])), phase=phase)
        cnum = race[8]
        cid = self.culture[cnum][1]
        naming = self.dat.naming[cid]
        c = len(self.civs) + 1
        if self.extra_draw:  # Y3: a test-only keyed draw before the names changes nothing
            pnr.draw(cr.stream_key(self.inp.seed, "test.engine.extra"), year, c, 0, 0)
        kw = dict(cores=self.cores) if QUIET_R12 else {}   # P3-02 r10 12.2 (0.19.0): K in the history stage
        n_c, t_c = pnr.make_name(naming, cid, "civilization", self.inp.seed, cnum, c, self.registry, **kw)
        e_civ = self.append("civ_founded", year, year, region=place,
                            participants=(("civilization", CIV, c), ("race", RACE, race_id)), causes=(e_aw,),
                            phase=phase)
        self.civs.append(dict(id=c, name=n_c, race=race_id, culture=cnum, home_region=place, capital=0,
                              founded_year=year, founding_event=e_civ, ended_year=None, ending_event=0))
        if self.owner[place][0] == 0:
            self.owner[place] = (c, e_civ)
        x, y, how = self.site(place)
        s = len(self.settlements) + 1
        n_s, t_s = pnr.make_name(naming, cid, "settlement", self.inp.seed, cnum, s, self.registry, **kw)
        e_set = self.append("settlement_founded", year, year, region=place, settlement=s,
                            participants=(("settlement", SETTLEMENT, s), ("founder", CIV, c)), values=(200,),
                            causes=(e_civ,), phase=phase)
        kinds = self.dat.settlement_kinds[race[1]]
        self.settlements.append(dict(id=s, name=n_s, x=x, y=y, region=place, founder_civ=c, founded_year=year,
                                     founding_event=e_set, owner_civ=c, kind=SETTLEMENT_KIND_CODE[kinds[0]],
                                     status=THRIVING, population=200, ruined_year=None, ruin_event=0))
        self.civs[c - 1]["capital"] = s
        self.name_attempts.append((c, t_c, s, t_s, how))
        return e_aw

    def flare_start(self, eng_run, year, length, trigger):
        self.append("flare_up", year, year, values=(length,), causes=(trigger,), phase=eng_run.phase_id)

    # --- 6.6 quiet eras and 6.7 bookkeeping
    def generations(self, civ, y0, y1):
        gy = self.race[civ["race"]][7]
        return (y1 + 1 - civ["founded_year"]) // gy - (y0 - civ["founded_year"]) // gy

    def quiet_stretch(self, start, last, phase=None, assured=None):
        """r12 20.1 (Q6, after the subsystems): one world-level quiet_stretch per quiet phase row, kept in
        open_era under key 0 (no civilization has id 0), so that every reset of the eras resets it. Opened at
        the row's first step with a living civilization: participants every civilization not ended at this
        step, ascending id, as `realm`; values years and realms. Later steps of the row lengthen it; its
        participants and `realms` never change. No living civilization: nothing."""
        idx = self.open_era.get(0)
        if idx is None:
            if self.open_era.get(-1):
                return   # r15 20.1 item 1a: the row is decided without a stretch
            alive = [c["id"] for c in sorted(self.civs, key=lambda c: c["id"]) if c["ended_year"] is None]
            if not alive:
                return
            if QUIET_R15:
                assert assured is not None
                if assured < self.eng.get("quiet_min_years", 0):
                    self.open_era[-1] = True
                    self.quiet_skipped.append((start, assured, phase))
                    return
            parts = alive
            if len(alive) > MAX_PARTICIPANTS:
                # r13: the MAX_PARTICIPANTS realms with the most people (the sum of their standing settlements'
                # populations at this step), ties to the smaller id, listed in ascending id; realms counts all
                pop = {x: 0 for x in alive}
                for st in self.settlements:
                    if st["status"] in (1, 2) and st["owner_civ"] in pop:
                        pop[st["owner_civ"]] += st["population"]
                parts = sorted(sorted(alive, key=lambda x: (-pop[x], x))[:MAX_PARTICIPANTS])
            kw = {} if phase is None else dict(phase=phase)
            eid = self.append("quiet_stretch", start, last, participants=tuple(("realm", CIV, x) for x in parts),
                              values=(last - start + 1, len(alive)), **kw)
            self.open_era[0] = eid - 1
            return
        ev = self.events[idx]
        ev["end_year"] = last
        ev["values"] = {1: last - ev["year"] + 1, 2: ev["values"][2]}

    def quiet_step(self, eng_run, start, length, phase_end, row_start):
        last = start + length - 1
        if start == row_start:
            self.open_era = {}
        if QUIET_R12:
            self.quiet_stretch(start, last, eng_run.phase_id, eng_run.assured(start, length, phase_end))
            self.bookkeeping(start, phase_end, last)
            if phase_end:
                self.open_era = {}
            return
        for civ in self.civs:   # none ends in the stub; none is charged an event
            if civ["ended_year"] is not None:
                continue
            idx = self.open_era.get(civ["id"])
            if idx is not None:
                ev = self.events[idx]
                ev["end_year"] = last
            else:
                eid = self.append("quiet_era", start, last, region=civ["home_region"],
                                  participants=(("civilization", CIV, civ["id"]),), values=(0, 0),
                                  phase=eng_run.phase_id)
                idx = eid - 1
                self.open_era[civ["id"]] = idx
            ev = self.events[idx]
            ev["values"] = {1: ev["end_year"] - ev["year"] + 1, 2: self.generations(civ, ev["year"], ev["end_year"])}
        self.bookkeeping(start, phase_end, last)
        if phase_end:
            self.open_era = {}

    def tick(self, eng_run, year, phase_end):
        self.bookkeeping(year, phase_end)

    # --- 6.1 open_relation / close_relation (the stub never calls them; P3-06 does)
    def open_relation(self, key, e):
        if key in self.relations:
            fatal("history: state: open_relation: the relation is open")
        if any(op == "open" and k == key for op, k, _e in self.rel_log):
            fatal("history: state: relation '%s' of %d %d and %d %d opened twice before one bookkeeping" %
                  (key[4], key[0], key[1], key[2], key[3]))
        self.relations[key] = e
        self.rel_log.append(("open", key, e))

    def close_relation(self, key, e):
        if key not in self.relations:
            fatal("history: state: close_relation: the relation is not open")
        del self.relations[key]
        self.rel_log.append(("close", key, e))

    def relation_bookkeeping(self, y):
        """6.7 item 2 (r9): walk the calls since the last bookkeeping in call order."""
        for op, key, e in self.rel_log:
            if op == "open":
                self.rel_open_row[key] = len(self.rel_rows)
                self.rel_rows.append([key[0], key[1], key[2], key[3], key[4], y, None, e, 0])
            else:
                if key not in self.rel_open_row:
                    fatal("history: engine: a relation was closed that has no row")
                row = self.rel_rows[self.rel_open_row.pop(key)]
                row[6] = y
                row[8] = e
        self.rel_log = []
        if set(self.rel_open_row) != set(self.relations):
            fatal("history: engine: a relation vanished without a closing event")

    def relation_table(self):
        """The rows stable-sorted by the primary key (9.3)."""
        return sorted((tuple(r) for r in self.rel_rows), key=lambda r: (r[0], r[1], r[2], r[3], r[4].encode(), r[5]))

    def bookkeeping(self, y, phase_end, end_year=None):
        """6.7 at year y; item 4's phase-end rows are dated at the phase's end_year (the step's last year
        in a quiet step: fixed at the P3-05 integration, the stub worlds never reached it)."""
        end_year = y if end_year is None else end_year
        for rid in sorted(self.owner):
            own, ev = self.owner[rid]
            if own != self.recorded_owner[rid]:
                if rid in self.rc_open:
                    self.region_control[self.rc_open.pop(rid)][2] = y
                if own != 0:
                    self.rc_open[rid] = len(self.region_control)
                    self.region_control.append([rid, y, None, own, ev])
                self.recorded_owner[rid] = own
        self.relation_bookkeeping(y)
        written = set()
        for s in self.settlements:
            cur = (s["owner_civ"], s["kind"], s["status"], s["population"])
            last = self.census_last.get(s["id"])
            if last is None or last[:3] != cur[:3]:
                self.census.append((s["id"], y) + cur)
                self.census_last[s["id"]] = cur
                written.add(s["id"])
        if phase_end:
            for s in self.settlements:
                cur = (s["owner_civ"], s["kind"], s["status"], s["population"])
                if self.census_last.get(s["id"]) != cur and not (s["id"] in written and end_year == y):
                    self.census.append((s["id"], end_year) + cur)
                    self.census_last[s["id"]] = cur

    def last_event_id(self):
        return len(self.events)


class StubHooks(Hooks):
    def __init__(self, world):
        super().__init__()
        self.w = world

    def awakening(self, eng, k, year):
        return self.w.awaken(eng, k, year)

    def flare_start(self, eng, year, length, trigger):
        self.w.flare_start(eng, year, length, trigger)

    def tick(self, eng, year, phase_end):
        self.w.tick(eng, year, phase_end)

    def quiet_step(self, eng, start, length, phase_end, row_start):
        self.w.quiet_step(eng, start, length, phase_end, row_start)

    def last_event_id(self):
        return self.w.last_event_id()


def history_stage(dat, inp, tension=None, stage_entry=True, eng=None, report=None, extra_draw=False):
    """3.3 history_stage (C1 items 2 to 5, then 5 to 7). Returns the rows of
    the thirteen tables in write order plus the run's details."""
    eng = eng if eng is not None else dat.engine
    report = report if report is not None else dat.report
    msg = entry_checks(dat, inp, eng, report, stage_entry)
    if msg is not None:
        fatal(msg)
    msg = chronology_problem(inp)
    if msg is not None:
        fatal(msg)
    cat = event_catalogue(report)
    w = World(dat, inp, cat, eng, tension, extra_draw)
    starts = [a[2] for a in inp.ages]
    en = Engine(starts, [a[3] for a in inp.ages], inp.params["span"], eng, inp.params["detail"], tension,
                StubHooks(w))
    en.run()
    for ev in w.events:  # 6.6 W2: end_year never past E - 1
        assert ev["end_year"] <= inp.params["span"] - 1
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
        relation=w.relation_table(),
    )
    res["_world"] = w
    res["_engine"] = en
    res["_catalogue"] = cat
    return res


def check_relation_bookkeeping(c):
    """16.1 item 6 (r9): the worked row, closed and reopened, opened twice, a close without a row."""
    inp = synthetic_inputs([0], 2000)
    dat = data()
    w = World(dat, inp, event_catalogue(dat.report), dat.engine)
    k = (CIV, 1, CIV, 2, "peace")
    w.open_relation(k, 13)
    w.close_relation(k, 14)
    w.relation_bookkeeping(7)
    c.eq("6.7 item 2 r9: opened and closed in year 7", w.relation_table(), [(3, 1, 3, 2, "peace", 7, 7, 13, 14)])
    w.open_relation(k, 15)
    w.relation_bookkeeping(8)
    w.close_relation(k, 16)
    w.open_relation(k, 17)
    w.relation_bookkeeping(9)
    c.eq("6.7 item 2 r9: closed and reopened in one bookkeeping: two rows", w.relation_table(),
         [(3, 1, 3, 2, "peace", 7, 7, 13, 14), (3, 1, 3, 2, "peace", 8, 9, 15, 16),
          (3, 1, 3, 2, "peace", 9, None, 17, 0)])
    w.close_relation(k, 18)

    def twice():
        w.open_relation(k, 19)
        w.close_relation(k, 20)
        w.open_relation(k, 21)
    expect_fatal(c, "6.7 item 2 r9: opened twice", twice,
                 "history: state: relation 'peace' of 3 1 and 3 2 opened twice before one bookkeeping")
    w2 = World(dat, inp, event_catalogue(dat.report), dat.engine)
    w2.relations[k] = 1
    w2.rel_log.append(("close", k, 2))
    del w2.relations[k]
    expect_fatal(c, "6.7 item 2 r9: a close without a row", lambda: w2.relation_bookkeeping(1),
                 "history: engine: a relation was closed that has no row")


# ---------------------------------------------------------------------------
# Tables: hashes (P0-04 8), the [history] report (11.1), rendering views
# ---------------------------------------------------------------------------

COLUMNS = {
    "event_kind": 7, "event_kind_role": 6, "event_kind_value": 3, "phase": 6, "event": 8, "event_participant": 5,
    "event_cause": 2, "event_value": 3, "civilization": 10, "settlement": 14, "settlement_census": 6,
    "region_control": 5, "relation": 9,
}
DDL_ORDER = ("event_kind", "event_kind_role", "event_kind_value", "phase", "event", "event_participant",
             "event_cause", "event_value", "civilization", "settlement", "settlement_census", "region_control",
             "relation")
TABLES = tuple(sorted(COLUMNS, key=lambda t: t.encode()))  # hash --detail order

DDL = (
    "CREATE TABLE event_kind (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, caused INTEGER NOT NULL, region_use "
    "INTEGER NOT NULL, settlement_use INTEGER NOT NULL, salience INTEGER NOT NULL, template TEXT NOT NULL) STRICT",
    "CREATE TABLE event_kind_role (kind INTEGER NOT NULL, seq INTEGER NOT NULL, role TEXT NOT NULL, entity_type "
    "INTEGER NOT NULL, min_count INTEGER NOT NULL, max_count INTEGER NOT NULL, PRIMARY KEY (kind, seq)) STRICT, "
    "WITHOUT ROWID",
    "CREATE TABLE event_kind_value (kind INTEGER NOT NULL, seq INTEGER NOT NULL, key TEXT NOT NULL, PRIMARY KEY "
    "(kind, seq)) STRICT, WITHOUT ROWID",
    "CREATE TABLE phase (id INTEGER PRIMARY KEY, age INTEGER NOT NULL, kind INTEGER NOT NULL, start_year INTEGER NOT "
    "NULL, end_year INTEGER NOT NULL, trigger_event INTEGER NOT NULL) STRICT",
    "CREATE TABLE event (id INTEGER PRIMARY KEY, year INTEGER NOT NULL, end_year INTEGER NOT NULL, kind INTEGER NOT "
    "NULL, phase INTEGER NOT NULL, region INTEGER NOT NULL, settlement INTEGER NOT NULL, salience INTEGER NOT NULL) "
    "STRICT",
    "CREATE TABLE event_participant (event INTEGER NOT NULL, seq INTEGER NOT NULL, role INTEGER NOT NULL, "
    "entity_type INTEGER NOT NULL, entity_id INTEGER NOT NULL, PRIMARY KEY (event, seq)) STRICT, WITHOUT ROWID",
    "CREATE TABLE event_cause (event INTEGER NOT NULL, cause INTEGER NOT NULL, PRIMARY KEY (event, cause)) STRICT, "
    "WITHOUT ROWID",
    "CREATE TABLE event_value (event INTEGER NOT NULL, seq INTEGER NOT NULL, value INTEGER NOT NULL, PRIMARY KEY "
    "(event, seq)) STRICT, WITHOUT ROWID",
    "CREATE TABLE civilization (id INTEGER PRIMARY KEY, name TEXT NOT NULL, race INTEGER NOT NULL, culture INTEGER "
    "NOT NULL, home_region INTEGER NOT NULL, capital INTEGER NOT NULL, founded_year INTEGER NOT NULL, founding_event "
    "INTEGER NOT NULL, ended_year INTEGER, ending_event INTEGER NOT NULL) STRICT",
    "CREATE TABLE settlement (id INTEGER PRIMARY KEY, name TEXT NOT NULL, x INTEGER NOT NULL, y INTEGER NOT NULL, "
    "region INTEGER NOT NULL, founder_civ INTEGER NOT NULL, founded_year INTEGER NOT NULL, founding_event INTEGER "
    "NOT NULL, owner_civ INTEGER NOT NULL, kind INTEGER NOT NULL, status INTEGER NOT NULL, population INTEGER NOT "
    "NULL, ruined_year INTEGER, ruin_event INTEGER NOT NULL) STRICT",
    "CREATE TABLE settlement_census (settlement INTEGER NOT NULL, year INTEGER NOT NULL, owner_civ INTEGER NOT NULL, "
    "kind INTEGER NOT NULL, status INTEGER NOT NULL, population INTEGER NOT NULL, PRIMARY KEY (settlement, year)) "
    "STRICT, WITHOUT ROWID",
    "CREATE TABLE region_control (region INTEGER NOT NULL, from_year INTEGER NOT NULL, to_year INTEGER, civ INTEGER "
    "NOT NULL, event INTEGER NOT NULL, PRIMARY KEY (region, from_year)) STRICT, WITHOUT ROWID",
    "CREATE TABLE relation (a_type INTEGER NOT NULL, a_id INTEGER NOT NULL, b_type INTEGER NOT NULL, b_id INTEGER "
    "NOT NULL, kind TEXT NOT NULL, since INTEGER NOT NULL, until INTEGER, since_event INTEGER NOT NULL, until_event "
    "INTEGER NOT NULL, PRIMARY KEY (a_type, a_id, b_type, b_id, kind, since)) STRICT, WITHOUT ROWID",
)


def table_hashes(res):
    return {t: pchr.table_hash(COLUMNS[t], res[t]) for t in TABLES}


def report_lines(res, end_year, races=None):
    """11.1: the [history] section, or [] when `phase` is empty; with the race rows, the keys P3-05 15.4
    appends after `relations` (every world from 0.15.0 with phases prints them)."""
    ph = res["phase"]
    if not ph:
        return []
    sim = sum(r[4] - r[3] + 1 for r in ph if r[2] in (OPENING, FLARE, FINAL))
    o = ["[history]", "end_year = %d" % end_year]
    for k, name in ((1, "opening"), (2, "quiet"), (3, "flare"), (4, "final")):
        o.append("phases_%s = %d" % (name, sum(1 for r in ph if r[2] == k)))
    o.append("simulated_years = %d" % sim)
    o.append("simulated_permille = %d" % (sim * 1000 // end_year))
    o.append("events = %d" % len(res["event"]))
    for k in res["event_kind"]:
        o.append("events_%s = %d" % (k[1], sum(1 for e in res["event"] if e[3] == k[0])))
    o.append("civilizations = %d" % len(res["civilization"]))
    o.append("civilizations_alive = %d" % sum(1 for c in res["civilization"] if c[8] is None))
    o.append("settlements = %d" % len(res["settlement"]))
    o.append("settlements_standing = %d" % sum(1 for s in res["settlement"] if s[10] in (1, 2)))
    o.append("regions_held = %d" % sum(1 for r in res["region_control"] if r[2] is None))
    o.append("census_rows = %d" % len(res["settlement_census"]))
    o.append("relations = %d" % len(res["relation"]))
    if races is not None:
        civ_race = {c[0]: c[2] for c in res["civilization"]}  # .get: a standing settlement may have no owner (0)
        standing = [x for x in res["settlement"] if x[10] in (1, 2)]
        o.append("population = %d" % sum(x[11] for x in standing))
        for rid, key in ((r[0], r[1]) for r in races):
            o.append("population_%s = %d" % (key, sum(x[11] for x in standing if civ_race.get(x[8]) == rid)))
        for rid, key in ((r[0], r[1]) for r in races):
            o.append("settlements_standing_%s = %d" % (key, sum(1 for x in standing if civ_race.get(x[8]) == rid)))
        for rid, key in ((r[0], r[1]) for r in races):
            o.append("regions_held_%s = %d" % (key, sum(1 for x in res["region_control"]
                                                          if x[2] is None and civ_race[x[3]] == rid)))
    return o


def view_from_rows(res, regions, races, gods):
    """A HistoryView over rows (as stored): the catalogue from the three
    tables, names from region, race, god, civilization, settlement."""
    cat = {}
    for k in res["event_kind"]:
        cat[k[0]] = dict(id=k[0], key=k[1], caused=k[2], region_use=k[3], settlement_use=k[4], salience=k[5],
                         templ=k[6], roles=[], values=[])
    for r in res["event_kind_role"]:
        cat[r[0]]["roles"].append(dict(seq=r[1], role=r[2], type=r[3], min=r[4], max=r[5]))
    for v in res["event_kind_value"]:
        cat[v[0]]["values"].append(v[2])
    evs = {}
    for e in res["event"]:
        evs[e[0]] = dict(year=e[1], end_year=e[2], kind=e[3], region=e[5], settlement=e[6], participants=[],
                         values={})
    for p in res["event_participant"]:
        evs[p[0]]["participants"].append(tuple(p[1:]))
    for e in evs.values():
        e["causes"] = []
    for x, cc in res.get("event_cause", ()):   # r12 20.2: {@causes:...} reads the causes
        evs[x]["causes"].append(cc)
    for v in res["event_value"]:
        evs[v[0]]["values"][v[1]] = v[2]
    names = {REGION: {r[0]: r[1] for r in regions}, GOD: {g[0]: g[1] for g in gods},
             CIV: {c[0]: c[1] for c in res["civilization"]}, SETTLEMENT: {s[0]: s[1] for s in res["settlement"]}}
    race_forms = {r[0]: dict(name=r[2], plural=r[3], adjective=r[4]) for r in races}

    def name(t, x, form):
        if t == RACE:
            return race_forms[x][form]
        return names[t][x]

    return dict(catalogue=cat, events=evs, name=name)


def sentences(res, inp):
    view = view_from_rows(res, [(r["id"], r["name"]) for r in inp.regions], inp.races, inp.gods)
    return [render(view, e[0]) for e in res["event"]]


# ---------------------------------------------------------------------------
# Worlds
# ---------------------------------------------------------------------------

REGION_COLS = ("id", "name", "x_min", "y_min", "x_max", "y_max", "seed_x", "seed_y", "cells", "lake_cells",
               "walk_cells", "habitability_mean")


def world_regions(conn):
    rows = conn.execute("SELECT %s FROM region ORDER BY id" % ", ".join(REGION_COLS)).fetchall()
    return [dict(zip(REGION_COLS, r)) for r in rows]


def world_layers(conn):
    def load():
        out = []
        for name in ("region", "water"):
            blob = conn.execute("SELECT data FROM layer WHERE name = ?", (name,)).fetchone()[0]
            out.append(pwc.decode_blob(blob)[4])
        return tuple(out)
    return load


def inputs_from_world(conn, chron_rows=None):
    """HistoryInputs from a stored world: meta, regions, layers; chronology
    rows from the world unless given (the reference's own P3-03 rows)."""
    meta = dict(conn.execute("SELECT key, value FROM meta").fetchall())
    p = pchr.params_from_meta(meta)
    if chron_rows is None:
        q = lambda sql: [tuple(r) for r in conn.execute(sql).fetchall()]  # noqa: E731
        chron_rows = dict(race=q("SELECT * FROM race ORDER BY id"), culture=q("SELECT * FROM culture ORDER BY id"),
                          god=q("SELECT * FROM god ORDER BY id"), age=q("SELECT * FROM age ORDER BY id"))
    return Inputs(int(meta["seed"]), int(meta["param.map_size"]), p, world_regions(conn), chron_rows["race"],
                  chron_rows["culture"], chron_rows["god"], chron_rows["age"], world_layers(conn))


def synthetic_inputs(starts, e, regions=None, detail="normal", seed=0, races=None, dat=None):
    """Hand-made inputs for the seam (unit cases): ages at `starts`, present
    races in race order, region rows as given (default two plain regions)."""
    dat = dat or data()
    ids = races or [r["id"] for r in dat.chron.races][:len(starts)]
    p = pchr.params(span=e, races=ids, detail=detail)
    res = pchr.stage_with_regions(dat.chron, p, seed, [r["name"] for r in (regions or [])])
    ends = age_ends(starts, e)
    present_order = [dat.chron.race_by_id[i]["order"] for i in ids]
    year_of = dict(zip(present_order, starts))
    race_rows = [r[:10] + (year_of.get(r[0]),) + r[11:] for r in res["race"]]
    ages = [(i + 1, present_order[i], starts[i], ends[i], res["age"][i][4]) for i in range(len(starts))]
    if regions is None:
        regions = [plain_region(1, "Alpha", 0, 0, 3, 3, 2000, 100, 50), plain_region(2, "Beta", 4, 0, 7, 3, 2000, 100, 40)]
    return Inputs(seed, 1024, p, regions, race_rows, res["culture"], res["god"], ages, None)


def plain_region(rid, name, x0, y0, x1, y1, cells, walk, hab, lake=0, seed_xy=None):
    sx, sy = seed_xy or (x0, y0)
    return dict(id=rid, name=name, x_min=x0, y_min=y0, x_max=x1, y_max=y1, seed_x=sx, seed_y=sy, cells=cells,
                lake_cells=lake, walk_cells=walk, habitability_mean=hab)


# ---------------------------------------------------------------------------
# Golden cases (16.4)
# ---------------------------------------------------------------------------

SEAM_SEEDS = pchr.SEAM_SEEDS   # 0, 1, 42, 2^64 - 1, phrase_seed("the unmapped")
SEAM_SPANS = (2000, 6000, 12000)
SEAM_RACES = ("all", "elf,orc", "orc", "goblin,dwarf,human")
EDGE_SPAN_SEEDS = (0, 1, 42)
EDGE_SPANS = (2001, 11999)
TENSIONS = (("ft500", ForcedTension(500, 1000, 1)), ("ft1", ForcedTension(1, 5000, 1)))

# World cases (16.4): the four P1 golden cases and the three hist-* cases.
WORLD_CASES = (
    ("geo-seed-0-512", 0, 512, 6000, "all", "normal"),
    ("geo-seed-42-512", 42, 512, 6000, "all", "normal"),
    ("geo-phrase-512", 15629206466951235176, 512, 6000, "all", "normal"),
    ("geo-seed-1-1024", 1, 1024, 6000, "all", "normal"),
    ("hist-span-2000-512", 7, 512, 2000, "all", "normal"),
    ("hist-races-elf-orc-1024", 3, 1024, 6000, "elf,orc", "normal"),
    ("hist-dense-1024", 11, 1024, 6000, "all", "dense"),
)
P1_06_CASE = pchr.P1_06_CASE

# Hand-made schedules for E1-E10 (5.6) on the shipped tuning: (name, span,
# detail, starts, tension or None). Expected tables are also written in
# check_edges below and in the C++ tests.
EDGE_CASES = (
    ("E1", 6000, "normal", (0, 100), None),
    ("E2", 6000, "normal", (0, 160, 3000), None),
    ("E3", 6000, "normal", (0, 520, 3000), ForcedTension(500, 1000, 1)),
    ("E3-final", 6000, "normal", (0, 4000), ForcedTension(5675, 1000, 1)),
    ("E3-end", 6000, "normal", (0, 525), ForcedTension(525, 1000, 1)),
    ("E4", 6000, "normal", (0, 5600), None),
    ("E5", 6000, "normal", (0, 5999), None),
    ("E6", 6000, "normal", (0, 5900), None),
    ("E6-2", 6000, "normal", (0, 5998), None),
    ("E7", 6000, "normal", (0, 5602), None),
    ("E8", 6000, "normal", (0, 3000), ForcedTension(25, 5000, 1)),
    ("E9", 6000, "normal", (0, 3000), ForcedTension(25, 5000, 1)),
    ("E10", 6000, "normal", (0,), None),
    ("E10-sparse-2000", 2000, "sparse", (0,), None),
    ("E10-dense-12000", 12000, "dense", (0,), None),
)


def races_of(text):
    return pchr.races_of(text, data().chron)


def starts_of(seed, span, races):
    dat = data().chron
    ids = races_of(races)
    present = [(dat.race_by_id[i]["order"], dat.race_by_id[i]["share"]) for i in ids]
    return [x["year"] for x in pchr.schedule(seed, span, present, dat.tuning["jitter"], dat.tuning["gap"])]


def sched_lines(name, starts, span, detail, tension=None):
    en = schedule_phases(starts, span, data().engine, detail, tension)
    b = en.base
    return ["%s starts %s" % (name, ",".join(map(str, starts))),
            "%s lengths %d %d %d %d %d %d" % (name, b["A"], b["F"], b["L"], b["Q"], b["floor"], b["cap"]),
            "%s phases %s" % (name, fmt_phases(en.phases)), "%s simulated %d" % (name, en.simulated())]


def world_case(name, seed, size, span, races, detail, inputs_dir, tension=None):
    dat = data()
    path = os.path.join(inputs_dir, name + ".db")
    conn = pchr.open_world(path)
    h, _rows = pchr.world_table_hash(conn, "region")
    regions = pchr.world_region_names(conn)
    if name in P1_06_CASE:
        want = pchr.p1_06_region_hash(P1_06_CASE[name])
        if h != want:
            fatal("%s: table region %s differs from p1-06-regions.txt %s" % (path, h, want))
    p = pchr.params(span=span, races=races_of(races), detail=detail)
    chron = pchr.chronology(dat.chron, p, seed, regions)
    meta = dict(conn.execute("SELECT key, value FROM meta").fetchall())
    inp = Inputs(seed, size, p, world_regions(conn), chron["race"], chron["culture"], chron["god"], chron["age"],
                 world_layers(conn))
    if int(meta["seed"]) != seed or int(meta["param.map_size"]) != size:
        fatal("%s: seed or map size differs from the case" % path)
    res = history_stage(dat, inp, tension)
    res["_inputs"] = inp
    res["_chron"] = chron
    res["_region_hash"] = h
    conn.close()
    return res


def world_lines(name, res):
    o = ["%s input_region %s" % (name, res["_region_hash"]), "%s regions %d" % (name, len(res["_inputs"].regions))]
    h = table_hashes(res)
    for t in TABLES:
        o.append("%s table %s %s" % (name, t, h[t]))
    for line in report_lines(res, res["_inputs"].params["span"], res["_inputs"].races)[1:]:
        o.append("%s report %s" % (name, line))
    o.append("%s phases %s" % (name, fmt_phases(res["phase"], trigger=True)))
    w = res["_world"]
    for c, tc, s, ts, how in w.name_attempts:
        o.append("%s names c%d:%s s%d:%s site:%s" % (name, c, tc, s, ts, how))
    for e, text in zip(res["event"], sentences(res, res["_inputs"])):
        o.append("%s e%d %s" % (name, e[0], text))
    return o


# Template grammar cases (16.4): (kind key or "test_optional", text). The
# test kind test_optional (id 99, not in the catalogue): caused 0,
# region_use 2, settlement_use 2, roles 1 `actor` civilization 0-0,
# 2 `people` race 1-1, 3 `place` region 1-2; values 1 `count`.
TEST_KIND = kind_def(99, "test_optional", 0, 2, 2, (("actor", CIV, 0, 0), ("people", RACE, 1, 1),
                                                   ("place", REGION, 1, 2)), ("count",))

TEMPLATE_CASES = (
    ("awakening", "{creator} awakened the {race:plural} in {@region}."),
    ("awakening", "Text with }} and {{ braces"),
    ("awakening", "{{race}}"),
    ("awakening", "a } b"),
    ("awakening", "}"),
    ("awakening", "{race"),
    ("awakening", "x {race{creator}"),
    ("awakening", "{}"),
    ("awakening", "{nobody}"),
    ("awakening", "{race:shout}"),
    ("awakening", "{race:}"),
    ("awakening", "{@nowhere}"),
    ("awakening", "{@settlement}"),
    ("awakening", "{creator:plural}"),
    ("awakening", "{creator:adjective}"),
    ("awakening", "{race:adjective} {race:name} {race:all} {creator:all} {creator:name}"),
    ("awakening", "{@year}-{@end_year}"),
    ("awakening", ""),
    ("awakening", "caf\xc3\xa9"),
    ("awakening", "{race}\xff"),
    ("flare_up", "{@region}"),
    ("flare_up", "{years?year|years}{years}"),
    ("flare_up", "{years?year}"),
    ("flare_up", "{years?a|b|c}"),
    ("flare_up", "{years?|}"),
    ("flare_up", "{nothing?a|b}"),
    ("flare_up", "{years:name}"),
    ("settlement_founded", "{@settlement} {settlement} {founder:all}"),
    ("settlement_founded", "{population?soul|souls}"),
    ("settlement_founded", "{founder:plural}"),
    ("quiet_era", "{civilization} {years} {generations?generation|generations}"),
    ("quiet_era", "ok}} {bad"),
    ("quiet_era", "{bad} }"),
    ("test_optional", "{actor}"),
    ("test_optional", "{actor:name}"),
    ("test_optional", "{actor:all} {people:plural} {place:all} {place}"),
    ("test_optional", "{@region}"),
    ("test_optional", "{@settlement}"),
    ("test_optional", "{actor:plural}"),
)
# r10 (P3-04 16.1 item 10): the three-word form is valid, four words are not;
# # is an ordinary byte. Appended only from 0.17.0, so 0.16.0 stays reproducible.
TEMPLATE_CASES_R10 = (
    ("flare_up", "{years?|one|#}"),
    ("flare_up", "{years?none|one|# of them|}"),
    ("flare_up", "{years?a|b|c|d}"),
    ("flare_up", "{years?a#|b#|c#}"),
    ("flare_up", "{years?x{y|b|c}"),
    ("quiet_era", "{civilization} passed {years} quiet {years?year|years}, from {@year} to {@end_year}"
                  "{generations?, under one generation of rulers|, in which one new generation of rulers came to power"
                  "|, in which # new generations of rulers came to power}."),
    ("quiet_era", "{nothing?a|b|c}"),
)


# r12 (P3-04 20.2, 20.6 item 2): {@causes:...} valid for any kind, and every malformed body of 20.2.
# Appended only from 0.19.0, so 0.18.0 stays reproducible.
TEMPLATE_CASES_R12 = (
    ("settlement_dwindled", "{@causes:famine=famine,plague=plague|Weakened by |, }{settlement} dwindled."),
    ("awakening", "{@causes:famine=famine|a|b}{race:name}"),          # an uncaused kind: allowed
    ("awakening", "{@causes:nokind=word|a|b}x"),                       # a key that is no kind: allowed
    ("awakening", "{@causes:a=x|||}"),                                 # three words
    ("awakening", "{@causes:a=x|w}"),                                  # one word
    ("awakening", "{@causes:a=x}"),                                    # no word
    ("awakening", "{@causes:|a|b}"),                                   # no entry
    ("awakening", "{@causes:a=|a|b}"),                                 # empty cword
    ("awakening", "{@causes:a=x,a=y|a|b}"),                            # the same key twice
    ("awakening", "{@causes:Ab=x|a|b}"),                               # key syntax
    ("awakening", "{@causes:1a=x|a|b}"),                               # key syntax
    ("awakening", "{@causes:a-b=x|a|b}"),                              # key syntax
    ("awakening", "{@causes:a=x=y|a|b}"),                              # '=' in a cword
    ("awakening", "{@causes:a=x,|a|b}"),                               # empty entry
    ("awakening", "{@causes:a=x?|a|b}"),                               # '?' in a cword
    ("awakening", "{@causes:a=x|a?|b}"),                               # '?' in a word
    ("awakening", "{@causes:a=x y,b_2=z|, w=|.}"),                     # ',' and '=' in words, space in cword
    ("awakening", "{@causes}"),
    ("awakening", "{@causes:}"),
    ("quiet_stretch", "For {years} {years?year|years}, from {@year} to {@end_year}, the land was quiet"
                      "{realms?|; one realm held it|, and # realms kept the peace}."),
    ("quiet_stretch", "{realm:all} {realm}"),
    ("plague_outbreak", "Plague broke out in {settlement}{reached?| and spread to one other settlement| and spread "
                        "to # other settlements}; {total} died{reached?| in all| in all}."),
)


# r17 (P3-04 21.3 item 3, generator 0.20.0): the templates of kinds 39 and 40, {settlement} (min count 1: the
# first participant) accepted, {@settlement} refused (settlement_use 0).
TEMPLATE_CASES_R17 = (
    ("claim_settlements", "{claimant} laid claim to {settlement:all} of {holder} in {@region}."),
    ("settlements_ceded", "{from} gave up {settlement:all} in {@region} to {to}."),
    ("claim_settlements", "{settlement} of {holder}."),
    ("settlements_ceded", "{settlement} to {to}."),
    ("claim_settlements", "{claimant} laid claim to {@settlement}."),
    ("settlements_ceded", "{from} gave up {@settlement}."),
)


def template_cases():
    return (TEMPLATE_CASES + (TEMPLATE_CASES_R10 if at_least("0.17.0") else ())
            + (TEMPLATE_CASES_R12 if at_least("0.19.0") else ())
            + (TEMPLATE_CASES_R17 if at_least("0.20.0") else ()))


def template_kind(key):
    return TEST_KIND if key == "test_optional" else find_kind(COMPILED, key)


HEADER_020 = """\
# P3-04 engine golden vectors for generator 0.20.0 (schema 7; P3-11): the stub engine (run_engine
# with the stub configuration) on the 0.20.0 catalogue of 40 kinds (P3-04 r17 21.2: kinds 39
# claim_settlements and 40 settlements_ceded appended), values one to nine as words in rendered
# sentences (21.1), P3-02 r15 names (faerie lists, no repeated syllable, tard) and otherwise as at
# 0.19.0: one quiet_stretch per quiet phase row (20.1), separators (20.3), {@causes:...} (20.2),
# P1-06 r7 region names (the input worlds' table region). Spec"""
HEADER_019 = """\
# P3-04 engine golden vectors for generator 0.19.0 (schema 7; P3-10): the stub engine (run_engine
# with the stub configuration) on the 0.19.0 catalogue of 38 kinds (P3-04 r12 20.4: kinds 37
# plague_outbreak and 38 quiet_stretch appended, kinds 4 and 12 kept and not written), one world-level
# quiet_stretch per quiet phase row (20.1), thousands separators in rendered values (20.3) and the
# placeholder {@causes:...} (20.2, template cases), with P3-02 r10 names and P1-06 r7 region names
# (the input worlds' table region). Spec"""
HEADER_018 = """\
# P3-04 engine golden vectors for generator 0.18.0 (schema 7; P3-09): the stub engine (run_engine
# with the stub configuration) on the 0.18.0 catalogue of 36 kinds (P3-05 11 and r6, P3-06 11, r9
# 20.9 and r10: kinds 32-36; claim salience 35; P3-02 r8's word list), with P3-04 r10 (names
# inside a sentence, the three-word value form, the quiet_era template of 4.2, one quiet era per
# civilization and quiet row), P3-02 r5 names (filter, god patterns, the elven House form) and
# P1-06 r3 region names (the input worlds' table region). Spec"""
HEADER_017 = """\
# P3-04 engine golden vectors for generator 0.17.0 (schema 7; P3-08): the stub engine (run_engine
# with the stub configuration) on the 0.17.0 catalogue of 31 kinds (P3-05 11 and r6, P3-06 11),
# with P3-04 r10 (names inside a sentence, the three-word value form, the quiet_era template of
# 4.2, one quiet era per civilization and quiet row), P3-02 r5 names (filter, god patterns, the
# elven House form) and P1-06 r3 region names (the input worlds' table region). Spec"""
HEADER = """\
# P3-04 engine golden vectors for generator 0.16.0 (schema 7): the stub engine (run_engine with
# the stub configuration) on the 0.16.0 catalogue of 30 kinds (P3-05 11, P3-06 11). Spec
# docs/specs/p3-04-simulation-engine.md approved 2026-11-19, generated by
# worldgen/tests/reference/p3_engine_reference.py golden, an independent
# exact-integer reference written from the spec and P3-00 alone (with
# core_reference.py, p3_names_reference.py, p3_data_reference.py and
# p3_chronology_reference.py as libraries). The template grammar and the
# renderer are the reference's own implementation of spec 8.4 and 8.5.
# Data: the shipped game/data; history.engine and history.report as listed
# in spec 4.1 and 4.2 (the golden depends on the shipped engine.toml and
# report.toml holding exactly those values).
# Do not edit by hand; never regenerate to make a C++ test pass.
#
# Format: ASCII, LF, tokens separated by one space; a sentence or a message
# runs to the end of the line.
#   sched <case> <seed> <span> <detail> <races> <tension>
#       a schedule case (16.4): the age start years of the shipped races
#       (P3-03 awakening_years, races "all" = the six ids), span, detail,
#       tension "none" or ForcedTension(period,value,trigger) (7.4)
#   edge <case> <span> <detail> <starts> <tension>
#       a hand-made schedule (5.6 E1-E10): starts comma separated
#   <case> starts <y_1,...,y_P>                 the age start years
#   <case> lengths <A> <F> <L> <Q> <floor_years> <cap_years>      SC1
#   <case> phases <id:age:kind:start:end|...>   phase rows (5.7), trigger
#                                               omitted
#   <case> simulated <n>                        years of kinds 1, 3, 4
#   world <case> <seed> <map size> <span> <races> <detail> <tension>
#       a world case: the stub history of a world generated with the
#       shipped data and these settings; geography from a world whose
#       table region has the hash "input_region" (for the four P1 cases
#       table_region of p1-06-regions.txt)
#   <case> input_region <hex>, <case> regions <count>
#   <case> table <table> <hex>     P0-04 8 logical hash, as the line
#                                  "table <table> <hex>" of hash --detail,
#                                  for the thirteen tables of 9.1
#   <case> report <line>           the [history] lines of 11.1 and P3-05 15.4 (after the
#                                  header line)
#   <case> phases <id:age:kind:start:end:trigger|...>
#   <case> names c<id>:<attempt> s<id>:<attempt> site:<seed|row-major>
#                                  per awakening: the P3-02 attempt of the
#                                  civilization and settlement names ("F":
#                                  numeral fallback) and how S2 chose the
#                                  site
#   <case> e<id> <sentence>        render_event of every event, byte for
#                                  byte (16.4; the sentences are a
#                                  cross-platform contract)
#   template <kind> <hex> <result>
#       template_problem(kind, bytes) (8.4): <hex> the template bytes in
#       lower-case hex ("-" for empty), <result> "ok" or the problem text.
#       Kind test_optional is a test kind (not in the catalogue): id 99,
#       caused 0, region_use 2, settlement_use 2, roles 1 actor
#       civilization 0-0, 2 people race 1-1, 3 place region 1-2, value 1
#       count.
#   report_events <message>
#       the messages of the named check report_events (4.2) for the edited
#       report data of the case "report-check" (see the reference)
#
# Where the worked example of spec 17 appears (unchanged):
#   schedule, simulated 1154     sched sched-42-6000-normal-all (P3-03's age
#                                starts for seed 42; the 1024 world has the
#                                same starts, they do not depend on the map)
#   ForcedTension(500, 1000, 1)  sched sched-42-6000-normal-all-ft500 (20
#                                phases, simulated 1354)
#   ForcedTension(1, 5000, 1)    sched sched-42-6000-normal-all-ft1 (75
#                                phases, 32 flare-ups, simulated 2700)
#   the world of 17              world example-42-1024 (seed 42, 1024: the
#                                33 events, homes, names, sites, the
#                                sentences of 8.5 and the [history] lines
#                                of 11.1), with the two tension variants
#                                as example-42-1024-ft500 (41 events) and
#                                example-42-1024-ft1 (run_engine with
#                                ForcedTension, 7.4)
#   E1-E10                       edge E1 ... E10 (E7 is elf,orc seed 3)
"""


def golden_text(inputs):
    dat = data()
    head = HEADER.rstrip("\n")
    if ENGINE_VERSION == "0.20.0":
        head = HEADER_020 + "\n" + "\n".join(head.splitlines()[2:])
    elif ENGINE_VERSION == "0.19.0":
        head = HEADER_019 + "\n" + "\n".join(head.splitlines()[2:])
    elif ENGINE_VERSION == "0.18.0":
        head = HEADER_018 + "\n" + "\n".join(head.splitlines()[2:])
    elif ENGINE_VERSION == "0.17.0":
        head = HEADER_017 + "\n" + "\n".join(head.splitlines()[2:])
    o = [head]
    for seed in SEAM_SEEDS:
        for span in SEAM_SPANS:
            for detail in DETAILS:
                for races in SEAM_RACES:
                    name = "sched-%d-%d-%s-%s" % (seed, span, detail, races)
                    starts = starts_of(seed, span, races)
                    o.append("sched %s %d %d %s %s none" % (name, seed, span, detail, races))
                    o += sched_lines(name, starts, span, detail)
                    if detail == "normal":
                        for tag, t in TENSIONS:
                            o.append("sched %s-%s %d %d %s %s %s" % (name, tag, seed, span, detail, races,
                                                                    t.label().replace(" ", "")))
                            o += sched_lines(name + "-" + tag, starts, span, detail, t)
    for seed in EDGE_SPAN_SEEDS:
        for span in EDGE_SPANS:
            for detail in DETAILS:
                for races in ("all", "orc"):
                    name = "sched-%d-%d-%s-%s" % (seed, span, detail, races)
                    o.append("sched %s %d %d %s %s none" % (name, seed, span, detail, races))
                    o += sched_lines(name, starts_of(seed, span, races), span, detail)
    for name, span, detail, starts, t in EDGE_CASES:
        o.append("edge %s %d %s %s %s" % (name, span, detail, ",".join(map(str, starts)),
                                          t.label().replace(" ", "") if t else "none"))
        o += sched_lines(name, list(starts), span, detail, t)
    cases = list(WORLD_CASES) + [("example-42-1024", 42, 1024, 6000, "all", "normal")]
    for name, seed, size, span, races, detail in cases:
        res = world_case(name, seed, size, span, races, detail, inputs)
        o.append("world %s %d %d %d %s %s none" % (name, seed, size, span, races, detail))
        o += world_lines(name, res)
    for tag, t in TENSIONS:
        name = "example-42-1024-" + tag
        res = world_case("example-42-1024", 42, 1024, 6000, "all", "normal", inputs, t)
        o.append("world %s 42 1024 6000 all normal %s" % (name, t.label().replace(" ", "")))
        o += world_lines(name, res)
    for key, text in template_cases():
        b = text.encode("latin-1")
        p = template_problem(template_kind(key), b)
        o.append("template %s %s %s" % (key, b.hex() if b else "-", p if p is not None else "ok"))
    for m in report_events_messages(REPORT_CHECK_CASE):
        o.append("report_events %s" % m)
    return "\n".join(o) + "\n"


# 16.1 item 11 in the reference: an edited report data set that reaches
# every message of report_events once.
REPORT_CHECK_CASE = (
    ("awakening", 100, "{creator} awakened the {race:plural} in {@region}."),
    ("duel", 10, "x"),          # an unknown kind ("battle" until 0.15.0; a kind of P3-06 since 0.16.0)
    ("awakening", 100, "again"),
    ("civ_founded", 80, "{race:plural"),
    ("settlement_founded", 40, ""),
    ("quiet_era", 20, "{civilization}"),
)


# ---------------------------------------------------------------------------
# check
# ---------------------------------------------------------------------------

Checker = pchr.Checker
expect_fatal = pchr.expect_fatal


def check_data(c):
    dat = data()
    c.eq("engine listing", spec_engine(), dict(
        quiet_step_years=25, quiet_events_max=3, **(dict(quiet_min_years=50) if QUIET_R15 else {}),
        simulated_min_permille=100, simulated_max_permille=450,
        islet_max_dry_cells=1172,
        sparse=dict(active_years=75, flare_years=30, final_years=250, flare_threshold=1500),
        normal=dict(active_years=150, flare_years=50, final_years=300, flare_threshold=1000),
        dense=dict(active_years=250, flare_years=75, final_years=400, flare_threshold=700)))
    c.eq("data engine = listing", dat.engine, spec_engine())
    c.eq("data report = listing", dat.report, spec_report())
    c.eq("report_events on the shipped report", report_events_messages(dat.report), [])
    c.eq("shipped templates parse", [template_problem(find_kind(COMPILED, k), t) for k, _s, t in dat.report],
         [None] * len(COMPILED))
    c.eq("settlement kind hamlet for every race", sorted(set(v[0] for v in dat.settlement_kinds.values())),
         ["hamlet"])
    for root in ("worldgen/tests/data/geo", "worldgen/tests/data/geo-small"):
        d = data(root)
        c.eq("%s engine" % root, d.engine, dat.engine)
        c.eq("%s report" % root, d.report, dat.report)
    # engine_phase_budget figures (4.1 "Shipped: 700 / 1200 / 1900 against 2670")
    c.eq("phase budget", [6 * dat.engine[lv]["active_years"] + dat.engine[lv]["final_years"] for lv in DETAILS],
         [700, 1200, 1900])
    c.eq("budget bound", 6 * dat.engine["simulated_max_permille"] - 30, 2670)
    # Catalogue (8.2): keys distinct and of the id grammar within a kind
    for k in COMPILED:
        keys = [r["role"] for r in k["roles"]] + k["values"]
        c.eq("kind %s keys distinct" % k["key"], len(set(keys)), len(keys))
        c.true("kind %s keys grammar" % k["key"], all(pchr.is_def_id(x) for x in keys + [k["key"]]))


def check_scale(c):
    eng = data().engine
    # 5.5 table: (A, F, L) at E = 2000, 6000, 12000
    want = {("sparse", 2000): (25, 10, 83), ("sparse", 6000): (75, 30, 250), ("sparse", 12000): (150, 60, 500),
            ("normal", 2000): (50, 17, 100), ("normal", 6000): (150, 50, 300), ("normal", 12000): (300, 100, 600),
            ("dense", 2000): (83, 25, 133), ("dense", 6000): (250, 75, 400), ("dense", 12000): (500, 150, 800)}
    for (lv, e), afl in want.items():
        ln = lengths(e, eng, lv)
        c.eq("5.5 A F L %s %d" % (lv, e), (ln["A"], ln["F"], ln["L"]), afl)
    # 16.1 item 1 at E = 2001, 11999 (hand computed: (v*E + 3000) div 6000)
    c.eq("scale 75 2001", scale(75, 2001), 25)      # 153075 div 6000 = 25
    c.eq("scale 250 2001", scale(250, 2001), 83)    # 503250 div 6000 = 83
    c.eq("scale 30 2001", scale(30, 2001), 10)      # 63030 div 6000 = 10
    c.eq("scale 50 2001", scale(50, 2001), 17)      # 103050 div 6000 = 17
    c.eq("scale 75 11999", scale(75, 11999), 150)   # 902925 div 6000 = 150
    c.eq("scale 250 11999", scale(250, 11999), 500)
    c.eq("scale 30 11999", scale(30, 11999), 60)    # 362970 div 6000 = 60
    c.eq("scale 1 2000", scale(1, 2000), 1)         # 5000 div 6000 = 0 -> 1
    c.eq("scale 3 2000", scale(3, 2000), 1)         # 9000 div 6000 = 1
    c.eq("scale 9 2000", scale(9, 2000), 3)         # 21000 div 6000 = 3 (half up: 3.0)
    c.eq("scale 15 2000", scale(15, 2000), 5)
    c.eq("floor cap 6000", (lengths(6000, eng, "normal")["floor"], lengths(6000, eng, "normal")["cap"]), (600, 2700))
    c.eq("floor cap 2001", (lengths(2001, eng, "normal")["floor"], lengths(2001, eng, "normal")["cap"]), (201, 900))
    c.eq("floor cap 11999", (lengths(11999, eng, "normal")["floor"], lengths(11999, eng, "normal")["cap"]),
         (1200, 5399))


def ph(*rows):
    return [list(r) for r in rows]


def check_edges(c):
    eng = data().engine
    run = lambda st, e=6000, lv="normal", t=None: schedule_phases(list(st), e, eng, lv, t)  # noqa: E731
    # E1: starts 0, 100, A = 150: the elves' opening ends at 100; no quiet between. The floor (550 -> 600)
    # moves the final phase back by 50 years.
    en = run((0, 100))
    c.eq("E1", en.phases, ph((1, 1, 1, 0, 99, 0), (2, 2, 1, 100, 249, 0), (3, 2, 2, 250, 5649, 0),
                             (4, 2, 4, 5650, 5999, 0)))
    # E2: a quiet interval [150, 160) shorter than Q is one row (one step of 10).
    en = run((0, 160, 3000))
    c.eq("E2", en.phases[:3], ph((1, 1, 1, 0, 149, 0), (2, 1, 2, 150, 159, 0), (3, 2, 1, 160, 309, 0)))
    # E3: ForcedTension(500): the step [475, 500) asks; flare min(50, 520 - 500, ...) = 20, cut at 520.
    en = run((0, 520, 3000), t=ForcedTension(500, 1000, 1))
    c.eq("E3", en.phases[:4], ph((1, 1, 1, 0, 149, 0), (2, 1, 2, 150, 499, 0), (3, 1, 3, 500, 519, 1),
                                 (4, 2, 1, 520, 669, 0)))
    # E3 at the final phase: interval [150, 5700); the step ending 5675 asks; the flare is cut at 5700.
    en = run((0, 4000), t=ForcedTension(5675, 1000, 1))
    c.eq("E3 final", [r for r in en.phases if r[2] == 3], ph((5, 2, 3, 5675, 5699, 1)))
    # E3 at the interval's end: starts 0, 525 -> quiet [150, 525); the step [500, 525) ends it: no flare.
    en = run((0, 525), t=ForcedTension(525, 1000, 1))
    c.eq("E3 end", [r for r in en.phases if r[2] == 3 and r[3] == 525], [])
    # E4: orcs at 5600 (three races, so the floor does not act): e_3 = 5750 > E - L = 5700, the final phase
    # starts at 5750 and is 250 years.
    en = run((0, 3000, 5600))
    c.eq("E4", en.phases, ph((1, 1, 1, 0, 149, 0), (2, 1, 2, 150, 2999, 0), (3, 2, 1, 3000, 3149, 0),
                             (4, 2, 2, 3150, 5599, 0), (5, 3, 1, 5600, 5749, 0), (6, 3, 4, 5750, 5999, 0)))
    # E5: last awakening at E - 1: that one year, no final phase; the floor (151 -> 600) has no final phase
    # to lengthen and lengthens opening 1 by 449 years.
    en = run((0, 5999))
    c.eq("E5", en.phases, ph((1, 1, 1, 0, 598, 0), (2, 1, 2, 599, 5998, 0), (3, 2, 1, 5999, 5999, 0)))
    c.eq("E5 simulated", en.simulated(), 600)
    # E6: E - y_P = 100 <= A: the last opening runs to E - 1 (5900-5998), the final phase is 5999 alone; the
    # final phase cannot move back past e_P, so the floor lengthens opening 1 (250 -> 600).
    en = run((0, 5900))
    c.eq("E6", en.phases, ph((1, 1, 1, 0, 499, 0), (2, 1, 2, 500, 5899, 0), (3, 2, 1, 5900, 5998, 0),
                             (4, 2, 4, 5999, 5999, 0)))
    en = run((0, 5998))
    c.eq("E6 two years", en.phases, ph((1, 1, 1, 0, 597, 0), (2, 1, 2, 598, 5997, 0), (3, 2, 1, 5998, 5998, 0),
                                       (4, 2, 4, 5999, 5999, 0)))
    # E7: elf,orc seed 3 (starts 0, 5602): 548 -> 600 by lengthening the elves' opening to 202 years.
    en = run((0, 5602))
    c.eq("E7", en.phases, ph((1, 1, 1, 0, 201, 0), (2, 1, 2, 202, 5601, 0), (3, 2, 1, 5602, 5751, 0),
                             (4, 2, 4, 5752, 5999, 0)))
    c.eq("E7 simulated", en.simulated(), 600)
    c.eq("E7 from P3-03", starts_of(3, 6000, "elf,orc"), [0, 5602])
    # E8 / E9: tension after every step; flare-ups until the cap 2700.
    en = run((0, 3000), t=ForcedTension(25, 5000, 1))
    c.eq("E8 simulated = cap", en.simulated(), 2700)
    fl = [r for r in en.phases if r[2] == 3]
    c.eq("E9 flare after one step", en.phases[1:4], ph((2, 1, 2, 150, 174, 0), (3, 1, 3, 175, 224, 1),
                                                         (4, 1, 2, 225, 249, 0)))
    c.true("E8 last flare shortened or skipped", sum(r[4] - r[3] + 1 for r in fl) == 2700 - 600)
    # E10: one race: one opening, one quiet interval, the final phase lengthened by the floor.
    en = run((0,))
    c.eq("E10", en.phases, ph((1, 1, 1, 0, 149, 0), (2, 1, 2, 150, 5549, 0), (3, 1, 4, 5550, 5999, 0)))
    c.eq("E10 simulated", en.simulated(), 600)
    en = run((0,), 2000, "sparse")
    c.eq("E10 sparse 2000", en.phases, ph((1, 1, 1, 0, 24, 0), (2, 1, 2, 25, 1824, 0), (3, 1, 4, 1825, 1999, 0)))
    # Guard: a tuning with 6A + L above the cap passed past the loader (5 x 500 + 500 + 300 = 3300).
    import copy
    bad = copy.deepcopy(eng)
    bad["normal"]["active_years"] = 500
    expect_fatal(c, "cap guard", lambda: schedule_phases([0, 1000, 2000, 3000, 4000, 5000], 6000, bad, "normal"),
                 "history: schedule: scheduled active years 3300 above the cap 2700")


def check_example(c, inputs):
    """17: seed 42, 1024, span 6000, normal, all races."""
    eng = data().engine
    starts = starts_of(42, 6000, "all")
    c.eq("17 starts", starts, [0, 2074, 3242, 4243, 4969, 5596])
    b = base_schedule(starts, 6000, eng, "normal")
    c.eq("17 SC1", (b["A"], b["F"], b["L"], b["Q"], b["floor"], b["cap"]), (150, 50, 300, 25, 600, 2700))
    c.eq("17 SC2", [e for _y, e in b["openings"]], [150, 2224, 3392, 4393, 5119, 5746])
    c.eq("17 SC3", b["S_f"], 5746)
    c.eq("17 SC4", b["sched"], 1154)
    en = schedule_phases(starts, 6000, eng, "normal")
    want = ph((1, 1, 1, 0, 149), (2, 1, 2, 150, 2073), (3, 2, 1, 2074, 2223), (4, 2, 2, 2224, 3241),
              (5, 3, 1, 3242, 3391), (6, 3, 2, 3392, 4242), (7, 4, 1, 4243, 4392), (8, 4, 2, 4393, 4968),
              (9, 5, 1, 4969, 5118), (10, 5, 2, 5119, 5595), (11, 6, 1, 5596, 5745), (12, 6, 4, 5746, 5999))
    c.eq("17 phases", [r[:5] for r in en.phases], want)
    steps = []
    for r in en.phases:
        if r[2] == 2:
            n = r[4] - r[3] + 1
            steps.append((n + 24) // 25)
    c.eq("17 steps", steps, [77, 41, 35, 24, 20])
    en = schedule_phases(starts, 6000, eng, "normal", ForcedTension(500, 1000, 1))
    c.eq("17 ft500 count", len(en.phases), 20)
    c.eq("17 ft500 simulated", en.simulated(), 1354)
    c.eq("17 ft500 first interval", [r[2:5] for r in en.phases[1:10]],
         [[2, 150, 499], [3, 500, 549], [2, 550, 999], [3, 1000, 1049], [2, 1050, 1499], [3, 1500, 1549],
          [2, 1550, 1999], [3, 2000, 2049], [2, 2050, 2073]])
    en = schedule_phases(starts, 6000, eng, "normal", ForcedTension(1, 5000, 1))
    c.eq("17 ft1", (len(en.phases), sum(1 for r in en.phases if r[2] == 3), en.simulated()), (75, 32, 2700))
    path = os.path.join(inputs, "example-42-1024.db")
    if not os.path.exists(path):
        print("note: %s missing; the world part of 17 is not checked" % path)
        return
    res = world_case("example-42-1024", 42, 1024, 6000, "all", "normal", inputs)
    w = res["_world"]
    c.eq("17 homes", [x["home_region"] for x in w.civs], [12, 10, 14, 30, 18, 4])
    regname = {r["id"]: r["name"] for r in res["_inputs"].regions}
    c.eq("17 home names", [regname[x] for x in (12, 10, 14, 30, 18)],
         ["Northeast Downs IV", "Northeast Downs II", "Northeast Wood", "West Downs", "Northeast Downs V"])
    c.eq("17 home name orcs", regname[4], "Northwest Downs II")
    c.eq("17 civ names", [x["name"] for x in w.civs], ["Finen", "The Skib Horde", "The Kaindnimm Throne",
                                                       "The Circle of Fysoli", "The Kingdom of Alird",
                                                       "The Warband of Baaldkrok"])
    c.eq("17 settlements", [(s["name"], s["x"], s["y"]) for s in w.settlements],
         [("Voeledell", 755, 293), ("Skrisligg Pit", 808, 251), ("Daidmin", 722, 376), ("Lowhou", 304, 534),
          ("Aledsealdbury", 678, 426), ("Uld'kag", 241, 201)])
    c.eq("17 events", len(res["event"]), 33)
    ev = res["event"]
    c.eq("17 e1-e4", ev[:4], [(1, 0, 0, 1, 1, 12, 0, 100), (2, 0, 0, 2, 1, 12, 0, 80), (3, 0, 0, 3, 1, 12, 1, 40),
                              (4, 150, 2073, 4, 2, 12, 0, 20)])
    vals = {e: [v for (x, s, v) in res["event_value"] if x == e] for e in range(1, 34)}
    c.eq("17 e4 values", vals[4], [1924, 12])
    c.eq("17 e8 e9", (ev[7][1:3], ev[8][1:3], vals[8], vals[9], ev[7][5], ev[8][5]),
         ((2224, 3241), (2224, 3241), [1018, 7], [1018, 67], 12, 10))
    c.eq("17 e13-e15 gens", [vals[i][1] for i in (13, 14, 15)], [6, 57, 12])
    c.eq("17 e13 years", vals[13][0], 851)
    c.eq("17 e19-e22 gens", [vals[i][1] for i in (19, 20, 21, 22)], [4, 39, 8, 6])
    c.eq("17 e26-e30 gens", [vals[i][1] for i in range(26, 31)], [3, 31, 7, 5, 19])
    c.eq("17 e26 years", vals[26][0], 477)
    c.eq("17 triggers", [r[5] for r in res["phase"] if r[2] == 1], [1, 5, 10, 16, 23, 31])
    c.eq("17 causes", res["event_cause"][:6], [(2, 1), (3, 2), (6, 5), (7, 6), (11, 10), (12, 11)])
    c.eq("17 region_control", res["region_control"], [(4, 5596, None, 6, 32), (10, 2074, None, 2, 6),
                                                      (12, 0, None, 1, 2), (14, 3242, None, 3, 11),
                                                      (18, 4969, None, 5, 24), (30, 4243, None, 4, 17)])
    c.eq("17 census", res["settlement_census"], [(1, 0, 1, 1, 1, 200), (2, 2074, 2, 1, 1, 200),
                                                 (3, 3242, 3, 1, 1, 200), (4, 4243, 4, 1, 1, 200),
                                                 (5, 4969, 5, 1, 1, 200), (6, 5596, 6, 1, 1, 200)])
    s = sentences(res, res["_inputs"])
    c.eq("8.5 first sentences", s[:4], ["Fielie awakened the Elves in Northeast Downs IV.",
                                        "The Elves founded Finen in Northeast Downs IV.",
                                        "Finen founded Voeledell in Northeast Downs IV.",
                                        "Finen passed 1924 quiet years (12 generations), from 150 to 2073."])
    c.eq("11.1 example", report_lines(res, 6000), [
        "[history]", "end_year = 6000", "phases_opening = 6", "phases_quiet = 5", "phases_flare = 0",
        "phases_final = 1", "simulated_years = 1154", "simulated_permille = 192", "events = 33",
        "events_awakening = 6", "events_civ_founded = 6", "events_settlement_founded = 6", "events_quiet_era = 15",
        "events_flare_up = 0"] + ["events_%s = 0" % k for k in P3_05_KEYS + P3_06_KEYS] + ["civilizations = 6", "civilizations_alive = 6", "settlements = 6",
        "settlements_standing = 6", "regions_held = 6", "census_rows = 6", "relations = 0"])
    res = world_case("example-42-1024", 42, 1024, 6000, "all", "normal", inputs, ForcedTension(500, 1000, 1))
    ev = res["event"]
    vals = {e: [v for (x, s_, v) in res["event_value"] if x == e] for e in range(1, len(ev) + 1)}
    c.eq("17 ft500 events", len(ev), 41)
    c.eq("17 ft500 e4", (ev[3][1], ev[3][2], ev[3][3]), (150, 499, 4))
    c.eq("17 ft500 e5", (ev[4][1], ev[4][2], ev[4][3], ev[4][5], vals[5]), (500, 500, 5, 0, [50]))
    c.eq("17 ft500 e5 cause", [x for x in res["event_cause"] if x[0] == 5], [(5, 1)])
    c.eq("17 ft500 e6", (ev[5][1], ev[5][2]), (550, 999))
    c.eq("17 ft500 e12", (ev[11][1], ev[11][2], vals[12]), (2050, 2073, [24, 0]))
    c.eq("17 ft500 e13", ev[12][3], 1)
    c.eq("17 ft500 flare triggers", [r[5] for r in res["phase"] if r[2] == 3], [1, 1, 1, 1])
    st = sentences(res, res["_inputs"])
    c.eq("17 ft500 flare sentence", st[4], "In 500 tensions flared into 50 years of upheaval.")


OLD_INPUTS = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p3-04-tests", "inputs")


def check_example_r10(c, inputs):
    """17 and 8.5 at 0.17.0 (r10; P1-06 r3 region names, P3-02 r5 names):
    seed 42 at 1024. ASSUMPTION (spec contradiction): P3-04 r10 8.5's example
    keeps the elven realm `Finen`, but P3-02 r5 11.4 gives the elves' third
    civilization form as `The House of {root}` (the form index is kept, 11.4:
    `Nife` -> `The House of Nife`), so the realm is `The House of Finen` and
    the example's second to fourth sentences read with it; the reference
    follows P3-02 r5 and the r10 renderer (`founded the House of Finen`)."""
    path = os.path.join(inputs, "example-42-1024.db")
    if not os.path.exists(path):
        print("note: %s missing; the r10 world part of 17 is not checked" % path)
        return
    res = world_case("example-42-1024", 42, 1024, 6000, "all", "normal", inputs)
    w = res["_world"]
    c.eq("r10 17 homes", [x["home_region"] for x in w.civs], [12, 10, 14, 30, 18, 4])
    regname = {r["id"]: r["name"] for r in res["_inputs"].regions}
    c.eq("r10 region 12 (P1-06 r3, P3-04 r10 8.5)", regname[12], "Witchgate Barrows")
    c.eq("r10 region 10 (P1-06 5.1 Knelldown)", regname[10], "Knelldown")
    c.eq("r10 civ names (P3-02 r5: The House of)", [x["name"] for x in w.civs],
         ["The House of Finen", "The Skib Horde", "The Kaindnimm Throne", "The Circle of Fysoli",
          "The Kingdom of Alird", "The Warband of Baaldkrok"])
    c.eq("r10 17 events", len(res["event"]), 33)
    st = sentences(res, res["_inputs"])
    c.eq("r10 8.5 first sentences", st[:4], [
        "Fielie awakened the Elves in Witchgate Barrows.",
        "The Elves founded the House of Finen in Witchgate Barrows.",
        "The House of Finen founded Voeledell in Witchgate Barrows.",
        "The House of Finen passed 1924 quiet years, from 150 to 2073, in which 12 new generations of rulers came to "
        "power."])
    c.eq("r10 11.1 example", report_lines(res, 6000)[12:14], ["events_quiet_era = 15", "events_flare_up = 0"])
    c.eq("r10 11.1 kind 31 key", "events_settlement_abandoned_to = 0" in report_lines(res, 6000), True)


P3_08_INPUTS = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p3-08-tests", "inputs")


def check_example_r12(c, inputs):
    """P3-04 r12 20.1's example (illustrative there; re-told from seed 42): the first quiet row of seed 42 at 1024,
    the elves alone, as one quiet_stretch."""
    path = os.path.join(inputs, "example-42-1024.db")
    if not os.path.exists(path):
        print("note: %s missing; the r12 example is not checked" % path)
        return
    res = world_case("example-42-1024", 42, 1024, 6000, "all", "normal", inputs)
    st = sentences(res, res["_inputs"])
    c.eq("r12 20.1 the elves' first row", st[3], "For 1,924 years, from 150 to 2073, the land was quiet; one realm held it.")
    rl = report_lines(res, 6000)
    c.eq("r12 events_quiet_era 0", "events_quiet_era = 0" in rl, True)
    c.eq("r12 the two new lines follow events_refuge_held",
         (lambda ks: ks[ks.index("events_refuge_held"):ks.index("events_refuge_held") + 3])(
             [l.split(" = ")[0] for l in rl if l.startswith("events_")]),
         ["events_refuge_held", "events_plague_outbreak", "events_quiet_stretch"])
    if at_least("0.20.0"):   # P3-07 r14 23 item 1: kinds 39 and 40 after events_quiet_stretch, by kind id
        c.eq("r17 the kind 39 and 40 lines follow events_quiet_stretch",
             [l.split(" = ")[0] for l in rl if l.startswith("events_")][-3:],
             ["events_quiet_stretch", "events_claim_settlements", "events_settlements_ceded"])


class no_r17:
    """The renderer without P3-04 r17 21.1 (the checks written for r10's grammar and the 0.16.0 examples)."""

    def __enter__(self):
        global RENDER_R17
        self.saved, RENDER_R17 = RENDER_R17, False

    def __exit__(self, *a):
        global RENDER_R17
        RENDER_R17 = self.saved
        return False


def check_render_r10(c):
    """P3-04 16.1 item 10 (r10) in the reference: the three-word form and
    names inside a sentence, on hand-made views (the examples of 8.5 and of
    P1-06 r3 14.7)."""
    names = {}

    def view_of(kind_key, templ, parts, values=None, region=0, settlement=0, kind=None):
        k = dict(kind or find_kind(COMPILED, kind_key), templ=templ)
        return dict(catalogue={k["id"]: k}, name=lambda t, x, form: names[(t, x)],
                    events={1: dict(year=10, end_year=12, kind=k["id"], region=region, settlement=settlement,
                                    participants=parts, values=values or {})})

    def r(*a, **kw):
        return render(view_of(*a, **kw), 1)
    # three-word form: 0, 1, 2, -1; # replaced in the chosen word only; # kept in the two-word form
    for v, want in ((0, "x."), (1, "x and one fled."), (2, "x and 2 fled."), (-1, "x and -1 fled."),
                    (12, "x and 12 fled.")):
        c.eq("r10 three-word %d" % v, r("flare_up", "x{years?| and one fled| and # fled}.", [], {1: v}), want)
    c.eq("r10 # in w0 and w1 replaced too", [r("flare_up", "{years?#a|#b|#c}", [], {1: v}) for v in (0, 1, 5)],
         ["0a", "1b", "5c"])
    c.eq("r10 # kept in the two-word form", [r("flare_up", "{years?#|##}", [], {1: v}) for v in (1, 3)], ["#", "##"])
    c.eq("r10 four words unknown", template_problem(find_kind(COMPILED, "flare_up"), "{years?a|b|c|d}"),
         "unknown placeholder '{years?a|b|c|d}' at byte 0")
    # names inside a sentence
    names.update({(GOD, 1): "Fielie", (RACE, 1): None, (REGION, 5): "The Scarred Downs", (CIV, 1): "The Skib Horde",
                  (CIV, 2): "The Hold of Durgarin", (REGION, 6): "The Grove of the Hunt", (CIV, 3): "The Court of Saene",
                  (CIV, 4): "The Warrens of Bonk-Gitch", (REGION, 7): "West Wood", (CIV, 5): "Clan Durgarin",
                  (SETTLEMENT, 8): "Makmoudin Deep", (REGION, 9): "The Ashen Downs", (CIV, 6): "Thebes",
                  (CIV, 7): "There Hollow", (CIV, 8): "The", (CIV, 9): "THE Mob", (CIV, 10): "The A",
                  (CIV, 11): "The B", (CIV, 12): "The C"})
    racef = dict(name="Elf", plural="Elves", adjective="Elven")
    gob = dict(name="Goblin", plural="Goblins", adjective="Goblin")

    def nm(t, x, form):
        if t == RACE:
            return (racef if x == 1 else gob)[form]
        return names[(t, x)]

    def rr(kind_key, templ, parts, values=None, region=0, settlement=0, kind=None):
        v = view_of(kind_key, templ, parts, values, region, settlement, kind)
        v["name"] = nm
        return render(v, 1)
    c.eq("r10 8.5 awakening", rr("awakening", "{creator} awakened the {race:plural} in {@region}.",
                                 [(1, 1, RACE, 1), (2, 2, GOD, 1)], region=5),
         "Fielie awakened the Elves in the Scarred Downs.")
    c.eq("r10 8.5 war", rr("war_declared", "{attacker} declared war on {defender} over {@region}.",
                           [(1, 1, CIV, 1), (2, 2, CIV, 2)], region=6),
         "The Skib Horde declared war on the Hold of Durgarin over the Grove of the Hunt.")
    c.eq("r10 8.5 literal The", rr("civ_founded", "The {race:plural} founded {civilization} in {@region}.",
                                   [(1, 1, CIV, 4), (2, 2, RACE, 2)], region=7),
         "The Goblins founded the Warrens of Bonk-Gitch in West Wood.")
    c.eq("r10 8.5 truce", rr("truce", "{parties:all} made peace after {years} {years?year|years} of war, neither "
                                      "the victor.", [(1, 1, CIV, 1), (2, 1, CIV, 3)], {1: 12}),
         "The Skib Horde and the Court of Saene made peace after 12 years of war, neither the victor.")
    c.eq("r10 14.7 sentence start", rr("civ_ended", "{civilization} came to an end.", [(1, 1, CIV, 1)]),
         "The Skib Horde came to an end.")
    c.eq("r10 14.7 siege", rr("siege", "{attacker} besieged {@settlement} of {defender}; it {fell?fell|held}.",
                              [(1, 1, CIV, 5), (2, 2, CIV, 4)], {1: 1}, settlement=8),
         "Clan Durgarin besieged Makmoudin Deep of the Warrens of Bonk-Gitch; it fell.")
    c.eq("r10 14.7 claim", rr("claim", "{claimant} laid claim to {@region}, held by {holder}.",
                              [(1, 1, CIV, 5), (2, 2, CIV, 3)], region=9),
         "Clan Durgarin laid claim to the Ashen Downs, held by the Court of Saene.")
    for cid, want in ((6, "x Thebes"), (7, "x There Hollow"), (8, "x The"), (9, "x THE Mob")):
        c.eq("r10 unchanged %s" % want, rr("civ_ended", "x {civilization}", [(1, 1, CIV, cid)]), want)
    c.eq("r10 after '. ' keeps the capital", rr("civ_ended", "It ended. {civilization} was no more.",
                                               [(1, 1, CIV, 1)]), "It ended. The Skib Horde was no more.")
    for sep in (": ", "; ", "(", ", ", "x and "):
        c.eq("r10 after %r lower-cased" % sep, rr("civ_ended", "a" + sep + "{civilization}", [(1, 1, CIV, 1)]),
             "a" + sep + "the Skib Horde")
    tk = dict(find_kind(COMPILED, "truce"), roles=[dict(seq=1, role="parties", type=CIV, min=0, max=9)])
    c.eq("r10 :all at the start", rr("truce", "{parties:all} met.", [(1, 1, CIV, 10), (2, 1, CIV, 11), (3, 1, CIV, 12)],
                                     {1: 1}, kind=tk), "The A, the B and the C met.")
    c.eq("r10 :all mid-sentence", rr("truce", "Met: {parties:all}.", [(1, 1, CIV, 10), (2, 1, CIV, 11),
                                                                     (3, 1, CIV, 12)], {1: 1}, kind=tk),
         "Met: the A, the B and the C.")
    c.eq("r10 stored names unchanged", names[(CIV, 1)], "The Skib Horde")


def check_render_r17(c):
    """P3-04 r17 21.3 items 1 to 3 in the reference, on hand-made views and the shipped report data."""
    names = {(SETTLEMENT, 1): "Drutnot Hole", (SETTLEMENT, 2): "Nutskrank", (SETTLEMENT, 3): "The Grey Fort",
             (CIV, 1): "The Snat Horde", (CIV, 2): "The Stinrild League", (REGION, 7): "King's Fold",
             (CIV, 3): "Clan Durgarin"}

    def nm(t, x, form):
        return names[(t, x)]

    shipped = event_catalogue(spec_report())

    def view(kind_key, templ, parts, values=None, year=5, end_year=5):
        cat = {k["id"]: dict(k) for k in shipped}
        kid = find_kind(shipped, kind_key)["id"]
        if templ is not None:
            cat[kid] = dict(cat[kid], templ=templ)
        evs = {100: dict(year=year, end_year=end_year, kind=kid, region=7, settlement=1, participants=list(parts),
                         values=values or {}, causes=[])}
        return dict(catalogue=cat, events=evs, name=nm)

    dw = ((1, 1, SETTLEMENT, 1), (2, 2, CIV, 1))   # settlement_dwindled: settlement, civilization; value population
    for v, w in ((0, "0"), (1, "one"), (2, "two"), (9, "nine"), (10, "10"), (11, "11"), (999, "999"),
                 (1000, "1,000"), (-3, "-3"), (3, "three"), (5, "five")):
        c.eq("r17 {value} %d" % v, render(view("settlement_dwindled", "{population}", dw, {1: v}), 100), w)
        c.eq("r17 # of a three-word form %d" % v,
             render(view("settlement_dwindled", "{population?zero|single|#}", dw, {1: v}), 100),
             "zero" if v == 0 else "single" if v == 1 else w)
    c.eq("r17 {@year} 5", render(view("settlement_dwindled", "{@year}-{@end_year}", dw, {1: 1}), 100), "5-5")
    c.eq("r17 two-word form n=1", render(view("settlement_dwindled", "{population?one|#}", dw, {1: 1}), 100), "one")
    c.eq("r17 two-word form n=4: # is a byte", render(view("settlement_dwindled", "{population?one|#}", dw, {1: 4}),
                                                       100), "#")
    c.eq("r17 {n?|one|#} n=4", render(view("settlement_dwindled", "{population?|one|#}", dw, {1: 4}), 100), "four")
    c.eq("r17 {n?|one|#} n=1: the template's word", render(view("settlement_dwindled", "{population?|one|#}", dw,
                                                                 {1: 1}), 100), "one")
    c.eq("r17 never capitalized", render(view("settlement_dwindled", "{population} died.", dw, {1: 7}), 100),
         "seven died.")
    # the six rows of 21.1's table, with the shipped templates
    rows = (
        ("civ_seceded", ((1, 1, CIV, 2), (2, 2, CIV, 1)), {1: 1}, "with one settlement."),
        ("plague_outbreak", ((1, 1, SETTLEMENT, 1), (2, 2, CIV, 1)), {1: 30, 2: 100, 3: 1084, 4: 2},
         "spread to two other settlements; 1,084 died in all."),
        ("quiet_stretch", ((1, 1, CIV, 1), (2, 1, CIV, 2), (3, 1, CIV, 3), (4, 1, CIV, 3)), {1: 30, 2: 4},
         ", and four realms kept the peace."),
        ("peace", ((1, 1, CIV, 1), (2, 2, CIV, 2)), {1: 3}, "after three years of war."),
        ("war_won", ((1, 1, CIV, 1), (2, 2, CIV, 2)), {1: 3}, "after three years of war."),
        ("truce", ((1, 1, CIV, 1), (2, 2, CIV, 2)), {1: 3}, "after three years of war"),
        ("battle", ((1, 1, CIV, 1), (2, 2, CIV, 2)), {"loser_dead": 7, "winner_dead": 1},
         "; seven of the defeated and one of the victors fell."),
        ("famine", ((1, 1, SETTLEMENT, 1), (2, 2, CIV, 1)), {1: 8}, "; eight died."),
        ("settlement_captured", ((1, 1, SETTLEMENT, 1), (2, 2, CIV, 1), (3, 3, CIV, 2)), {1: 8}, "eight"),
    )
    for key, parts, vals, want in rows:
        k = find_kind(shipped, key)
        if any(isinstance(x, str) for x in vals):
            vals = {k["values"].index(x) + 1: v for x, v in vals.items()}
        if len(vals) != len(k["values"]):
            vals = {i + 1: vals.get(i + 1, 2) for i in range(len(k["values"]))}
        got = render(view(key, None, parts, vals), 100)
        c.eq("r17 21.1 row %s holds %r: %s" % (key, want, got), want in got, True)
    # 21.3 item 2: no shipped template puts {value} or a three-word value body at byte 0 or after ". "
    bad = []
    for k in shipped:
        t = k["templ"]
        vals = set(k["values"])
        for at in [0] + [i + 2 for i in range(len(t) - 1) if t[i:i + 2] == ". "]:
            if t[at:at + 1] == "{" and not t[at:at + 2] == "{{":
                body = t[at + 1:t.index("}", at)]
                v = body.split("?", 1)[0]
                if v in vals and (("?" not in body) or body.count("|") == 2):
                    bad.append((k["key"], at))
    c.eq("r17 21.3 item 2: no shipped template starts a sentence with a value", bad, [])
    # 21.3 item 3: kinds 39 and 40
    for key in ("claim_settlements", "settlements_ceded"):
        k = find_kind(shipped, key)
        c.eq("r17 %s role and value keys distinct" % key, len(set([r["role"] for r in k["roles"]] + k["values"])),
             len(k["roles"]) + len(k["values"]))
        c.eq("r17 %s shipped template accepted" % key, template_problem(k, k["templ"]), None)
        c.eq("r17 %s {settlement} accepted" % key, template_problem(k, "{settlement}."), None)
        c.eq("r17 %s {@settlement} refused" % key, template_problem(k, "{@settlement}") is not None, True)
    for n, want in ((1, "The Snat Horde laid claim to Drutnot Hole of the Stinrild League in King's Fold."),
                    (2, "The Snat Horde laid claim to Drutnot Hole and Nutskrank of the Stinrild League in King's Fold."),
                    (3, "The Snat Horde laid claim to Drutnot Hole, Nutskrank and the Grey Fort of the Stinrild League "
                        "in King's Fold.")):
        parts = [(1, 1, CIV, 1), (2, 2, CIV, 2)] + [(3 + i, 3, SETTLEMENT, i + 1) for i in range(n)]
        c.eq("r17 kind 39 with %d settlements" % n, render(view("claim_settlements", None, parts), 100), want)
    for n, want in ((1, "The Stinrild League gave up Drutnot Hole in King's Fold to the Snat Horde."),
                    (2, "The Stinrild League gave up Drutnot Hole and Nutskrank in King's Fold to the Snat Horde."),
                    (3, "The Stinrild League gave up Drutnot Hole, Nutskrank and the Grey Fort in King's Fold to the "
                        "Snat Horde.")):
        parts = [(1, 1, CIV, 2), (2, 2, CIV, 1)] + [(3 + i, 3, SETTLEMENT, i + 1) for i in range(n)]
        c.eq("r17 kind 40 with %d settlements" % n, render(view("settlements_ceded", None, parts), 100), want)


def check_render_r12(c):
    """P3-04 r12 20.6 items 2 and 3 in the reference, on hand-made views: {@causes:...} and separators."""
    names = {(SETTLEMENT, 1): "Glathenddell", (CIV, 1): "The Warrens of Zeez", (CIV, 2): "Clan Durgarin"}

    def nm(t, x, form):
        return names[(t, x)]

    def view(templ, cause_keys, kind_key="settlement_dwindled", parts=((1, 1, SETTLEMENT, 1), (2, 2, CIV, 1)),
             values=None):
        """Event 100 of kind_key with causes 1..n of the given kind keys (ascending id)."""
        cat = {k["id"]: dict(k) for k in COMPILED}
        kid = find_kind(COMPILED, kind_key)["id"]
        cat[kid] = dict(cat[kid], templ=templ)
        evs = {}
        for i, key in enumerate(cause_keys):
            evs[i + 1] = dict(year=1, end_year=1, kind=find_kind(COMPILED, key)["id"], region=0, settlement=0,
                              participants=[], values={}, causes=[])
        evs[100] = dict(year=5, end_year=5, kind=kid, region=0, settlement=1, participants=list(parts),
                        values=values or {1: 412}, causes=list(range(1, len(cause_keys) + 1)))
        return dict(catalogue=cat, events=evs, name=nm)

    tm = "{@causes:famine=famine,plague=plague,plague_spread=plague,battle=war,tribute=tribute|Weakened by |, }" \
         "{settlement} dwindled to {population} souls."
    r = lambda keys, t=tm, **kw: render(view(t, keys, **kw), 100)   # noqa: E731
    c.eq("r12 causes famine, plague", r(["famine", "plague"]),
         "Weakened by famine and plague, Glathenddell dwindled to 412 souls.")
    c.eq("r12 causes plague, plague_spread one word", r(["plague", "plague_spread"]),
         "Weakened by plague, Glathenddell dwindled to 412 souls.")
    c.eq("r12 causes three words", r(["battle", "famine", "tribute"]),
         "Weakened by war, famine and tribute, Glathenddell dwindled to 412 souls.")
    c.eq("r12 causes in ascending cause id, not entry order", r(["tribute", "famine"]),
         "Weakened by tribute and famine, Glathenddell dwindled to 412 souls.")
    c.eq("r12 causes unmapped only: empty", r(["colony_founded", "settlement_founded"]),
         "Glathenddell dwindled to 412 souls.")
    c.eq("r12 causes none: empty", r([]), "Glathenddell dwindled to 412 souls.")
    c.eq("r12 first entry of a key wins over a repeated cword", r(["battle", "battle_at_pass", "battle"],
         t="{@causes:battle=war,battle_at_pass=war|W |, }x"), "W war, x")
    # names: after the second word mid-sentence, after an empty placeholder at the sentence start
    t2 = "{@causes:battle=war|Weakened by |, }{civilization} moved."
    kw = dict(kind_key="capital_moved", parts=((1, 1, CIV, 1), (2, 2, SETTLEMENT, 1)), values={})
    c.eq("r12 the name after ', ' is mid-sentence", r(["battle"], t=t2, **kw), "Weakened by war, the Warrens of Zeez moved.")
    c.eq("r12 the name after an empty placeholder is at the start", r(["famine"], t=t2, **kw),
         "The Warrens of Zeez moved.")
    # the cwords and words are literals (no name rule)
    c.eq("r12 words are literals", r(["battle"], t="{@causes:battle=The War|The |: }x"), "The The War: x")
    # every malformed body of 20.2 is unknown placeholder
    k = find_kind(COMPILED, "settlement_dwindled")
    for body in ("@causes:a=x|||", "@causes:a=x|w", "@causes:a=x", "@causes:|a|b", "@causes:a=|a|b",
                 "@causes:a=x,a=y|a|b", "@causes:Ab=x|a|b", "@causes:1a=x|a|b", "@causes:a=x,|a|b"):
        c.eq("r12 malformed {%s}" % body, template_problem(k, "ab{%s}" % body),
             "unknown placeholder '{%s}' at byte 2" % body)
    c.eq("r12 causes allowed for an uncaused kind", template_problem(find_kind(COMPILED, "awakening"),
                                                                     "{@causes:x=y|a|b}"), None)
    # 20.3 separators in {value} and #
    want = {0: "0", 999: "999", 1000: "1,000", 1155: "1,155", 25000: "25,000", 1000000: "1,000,000",
            -1000: "-1,000", -999: "-999", 52691: "52,691", -12000: "-12,000"}
    for v, w in want.items():
        c.eq("r12 {value} %d" % v, r([], t="{population}", values={1: v}), w)
        c.eq("r12 # %d" % v, r([], t="{population?a|b|# x}", values={1: v}), w + " x" if v not in (0, 1) else "a")
    c.eq("r12 years without separators", render(dict(view("{@year}-{@end_year}", []),
                                                     events={100: dict(year=4393, end_year=12000, kind=k["id"],
                                                                       region=0, settlement=1, participants=[],
                                                                       values={1: 0}, causes=[])}), 100),
         "4393-12000")
    c.eq("r12 the choice depends on the value, not its text",
         r([], t="{population?one|many}", values={1: 1000}), "many")


def check_quiet_r12(c, inputs):
    """P3-04 r12 20.6 item 1 on the stub world cases and the ForcedTension cases: one quiet_stretch per quiet
    phase row with a living civilization, participants every civilization alive at the row's first step in
    ascending id, realms their number, years = end_year - year + 1 = the row's length; no quiet_era."""
    cases = [(n, sd, sz, sp, rc, dt, None) for n, sd, sz, sp, rc, dt in
             list(WORLD_CASES) + [("example-42-1024", 42, 1024, 6000, "all", "normal")]]
    cases += [("example-42-1024", 42, 1024, 6000, "all", "normal", t) for _tag, t in TENSIONS]
    for name, seed, size, span, races, detail, t in cases:
        if not os.path.exists(os.path.join(inputs, name + ".db")):
            print("note: %s missing; quiet stretches not checked on it" % name)
            continue
        res = world_case(name, seed, size, span, races, detail, inputs, t)
        w = res["_world"]
        lab = name + ("-" + t.label() if t else "")
        qid = find_kind(COMPILED, "quiet_stretch")["id"]
        c.eq("r12 %s no quiet_era" % lab, [e for e in w.events if e["key"] == "quiet_era"], [])
        st = [e for e in w.events if e["key"] == "quiet_stretch"]
        rows = [r for r in res["phase"] if r[2] == QUIET]
        bad = []
        qmin = w.eng.get("quiet_min_years", 0) if QUIET_R15 else 0
        for r in rows:
            alive = sorted(x["id"] for x in w.civs if x["founded_year"] <= r[3])
            mine = [e for e in st if e["phase"] == r[0]]
            if not alive:
                if mine:
                    bad.append(("stretch without a civilization", r))
                continue
            if r[4] - r[3] + 1 < qmin:   # r15 20.1 item 1a: a row shorter than quiet_min_years has none
                if mine:
                    bad.append(("stretch on a short row", r, mine))
                continue
            if len(mine) != 1:
                bad.append(("count", r, len(mine)))
                continue
            e = mine[0]
            if (e["year"], e["end_year"]) != (r[3], r[4]) or e["values"] != {1: r[4] - r[3] + 1, 2: len(alive)} \
                    or [p[3] for p in e["participants"]] != alive or e["region"] or e["settlement"] or e["causes"]:
                bad.append(("row", r, e))
        c.eq("r12 %s one quiet_stretch per quiet row" % lab, bad, [])
        c.eq("r12 %s kind id" % lab, all(e["kind"] == qid for e in st), True)
    # a civilization ended within the row stays a participant (hand-made: a world state driven directly)
    w = World.__new__(World)
    w.events, w.civs, w.open_era = [], [dict(id=1, ended_year=None), dict(id=2, ended_year=None)], {}
    w.cat_by_key = {k["key"]: k for k in COMPILED}
    w.inp = Inputs(0, 64, dict(span=6000), [], [], [], [], [])
    w.eng, w.quiet_skipped = {}, []   # hand-made tuning: quiet_min_years 0 (r15 20.1 item 1a off)
    w.quiet_stretch(100, 124, 7, 50)
    w.civs[1]["ended_year"] = 130
    w.quiet_stretch(125, 149, 7, 25)
    e = w.events[0]
    c.eq("r12 an ended civilization stays a participant", (len(w.events), e["year"], e["end_year"],
                                                           [p[3] for p in e["participants"]], e["values"]),
         (1, 100, 149, [1, 2], {1: 50, 2: 2}))
    w.settlements = []
    # r13 20.6 item 0: 70 living civilizations; populations so that ties occur at the cut
    w70 = World.__new__(World)
    w70.events, w70.open_era, w70.cat_by_key, w70.inp, w70.eng, w70.quiet_skipped = [], {}, w.cat_by_key, w.inp, {}, []
    w70.civs = [dict(id=i, ended_year=None) for i in range(1, 71)]
    w70.settlements = []
    for i in range(1, 71):
        w70.settlements.append(dict(id=i, owner_civ=i, status=1, population=(1000 - i if i < 60 else 5)))
    w70.settlements.append(dict(id=71, owner_civ=3, status=3, population=0))      # a ruin counts nothing
    w70.settlements.append(dict(id=72, owner_civ=5, status=2, population=10000))  # declining counts
    w70.quiet_stretch(100, 124, 7, 25)
    pop = {i: (1000 - i if i < 60 else 5) + (10000 if i == 5 else 0) for i in range(1, 71)}
    want = sorted(sorted(range(1, 71), key=lambda x: (-pop[x], x))[:64])
    e70 = w70.events[0]
    c.eq("r13 20.6 item 0: realms 70, 64 participants, the most populous (ties by id), ascending",
         (e70["values"][2], [p[3] for p in e70["participants"]]), (70, want))
    c.eq("r13 20.6 item 0: ids 1-59 and, of the eleven tied at 5, the five smallest", want, list(range(1, 65)))
    w2 = World.__new__(World)
    w2.events, w2.civs, w2.open_era = [], [dict(id=1, ended_year=5)], {}
    w2.cat_by_key, w2.inp, w2.eng, w2.quiet_skipped = w.cat_by_key, w.inp, {}, []
    w2.quiet_stretch(100, 124, 7, 25)
    c.eq("r12 no living civilization: no quiet line", w2.events, [])


class QuietProbe(Hooks):
    """r15 20.6 item 00: a schedule-only run whose quiet steps drive World.quiet_stretch on a hand-made world with
    two living civilizations (no subsystem, no other event)."""

    def __init__(self, eng_tuning):
        super().__init__()
        w = World.__new__(World)
        w.events, w.civs, w.open_era = [], [dict(id=1, ended_year=None), dict(id=2, ended_year=None)], {}
        w.cat_by_key = {k["key"]: k for k in COMPILED}
        w.inp = Inputs(0, 64, dict(span=6000), [], [], [], [], [])
        w.eng, w.quiet_skipped, w.settlements = eng_tuning, [], []
        self.w = w
        self.appended_at = []   # (step start, stretch year) of every append

    def quiet_step(self, eng, start, length, phase_end, row_start):
        w = self.w
        if start == row_start:
            w.open_era = {}
        n = len(w.events)
        w.quiet_stretch(start, start + length - 1, eng.phase_id, eng.assured(start, length, phase_end))
        if len(w.events) > n:
            self.appended_at.append((start, w.events[-1]["year"]))
        if phase_end:
            w.open_era = {}

    def last_event_id(self):
        return 1   # the ForcedTension trigger 1 is valid


def quiet_probe(starts, q, qmin, tension=None, e=6000, detail="normal"):
    """(quiet rows [(start, end)], stretches [(year, end_year, years)], appended_at) of a probe run."""
    tun = dict(data().engine, quiet_step_years=q, quiet_min_years=qmin)
    h = QuietProbe(tun)
    en = Engine(starts, age_ends(starts, e), e, tun, detail, tension, h).run()
    rows = [(r[3], r[4]) for r in en.phases if r[2] == QUIET]
    st = [(x["year"], x["end_year"], x["values"][1]) for x in h.w.events]
    return rows, st, h.appended_at, en


def check_quiet_r15(c, inputs):
    """P3-04 r15 20.1 item 1a and 20.6 item 00."""
    eng = data().engine
    c.eq("r15 shipped quiet_min_years", eng.get("quiet_min_years"), 50)
    # a row of one 25-year step followed by a flare-up (Q 25): no stretch; quiet_min_years 0: one per row
    rows, st, _, _ = quiet_probe((0, 3000), 25, 50, ForcedTension(25, 5000, 1))
    c.eq("r15 Q25 rows of one step then a flare-up are 25 years", sorted(set(b - a + 1 for a, b in rows[:-1])), [25])
    c.eq("r15 Q25 one 25-year step then a flare-up: no quiet_stretch",
         [x for x in st if x[0] < rows[-1][0]], [])
    rows0, st0, _, _ = quiet_probe((0, 3000), 25, 0, ForcedTension(25, 5000, 1))
    c.eq("r15 quiet_min_years 0: every quiet row has one (as at r14)", [(a, b, b - a + 1) for a, b in rows0], st0)
    # one full step and one of 10 years cut by an awakening (35 years): none
    rows, st, _, _ = quiet_probe((0, 185), 25, 50)
    c.eq("r15 Q25 the row 150-184 (35 years)", rows[0], (150, 184))
    c.eq("r15 Q25 a 35-year row cut by an awakening: no quiet_stretch", [x for x in st if x[0] == 150], [])
    # rows of 49 and 50 years
    rows, st, app, _ = quiet_probe((0, 199), 25, 50)
    c.eq("r15 Q25 the row 150-198 (49 years)", rows[0], (150, 198))
    c.eq("r15 Q25 a 49-year row: no quiet_stretch", [x for x in st if x[0] == 150], [])
    rows, st, app, _ = quiet_probe((0, 200), 25, 50)
    c.eq("r15 Q25 the row 150-199 (50 years)", rows[0], (150, 199))
    c.eq("r15 Q25 two full steps (50 years): one quiet_stretch", [x for x in st if x[0] == 150], [(150, 199, 50)])
    c.eq("r15 Q25 ... appended in its first step", [a for a in app if a[1] == 150], [(150, 150)])
    # Q 50
    rows, st, _, _ = quiet_probe((0, 175), 50, 50)
    c.eq("r15 Q50 a 25-year row (one step, the interval's end): none", (rows[0], [x for x in st if x[0] == 150]),
         ((150, 174), []))
    rows, st, _, _ = quiet_probe((0, 185), 50, 50)
    c.eq("r15 Q50 a 35-year row: none", (rows[0], [x for x in st if x[0] == 150]), ((150, 184), []))
    rows, st, _, _ = quiet_probe((0, 199), 50, 50)
    c.eq("r15 Q50 a 49-year row: none", (rows[0], [x for x in st if x[0] == 150]), ((150, 198), []))
    rows, st, app, _ = quiet_probe((0, 200), 50, 50)
    c.eq("r15 Q50 a 50-year row (one step): one", [x for x in st if x[0] == 150], [(150, 199, 50)])
    rows, st, _, _ = quiet_probe((0, 249), 50, 100)
    c.eq("r15 Q50 quiet_min 100: a 99-year row has none", (rows[0], [x for x in st if x[0] == 150]),
         ((150, 248), []))
    rows, st, app, _ = quiet_probe((0, 250), 50, 100)
    c.eq("r15 Q50 quiet_min 100: a 100-year row has one, from its first step", ([x for x in st if x[0] == 150],
                                                                                 [a for a in app if a[1] == 150]),
         ([(150, 249, 100)], [(150, 150)]))
    # exactness (20.1 item 1a): over many schedules, a stretch exactly when the row is long enough, and never
    # moved; every quiet_stretch has years >= quiet_min_years
    bad = []
    for q in (25, 50):
        for qmin in sorted(set(x for x in (0, 25, 26, 49, 50, 51, 99, 2 * q) if x <= 2 * q)):
            for a2 in range(152, 280, 7):
                for t in (None, ForcedTension(25, 5000, 1), ForcedTension(100, 5000, 1), ForcedTension(35, 5000, 1)):
                    if t is not None and q == 50 and t.period == 35:
                        continue
                    rows, st, app, en = quiet_probe((0, a2, 3000), q, qmin, t)
                    want = [(x, y, y - x + 1) for x, y in rows if y - x + 1 >= qmin]
                    if st != want or any(z[2] < qmin for z in st) or any(p != y for p, y in app):
                        bad.append((q, qmin, a2, t and t.label(), rows[:4], st[:4]))
    c.eq("r15 exactness: a stretch for exactly the rows at least quiet_min_years long, from their first step", bad,
         [])
    # engine_quiet_min
    inp0 = None
    for tun, want in ((dict(eng, quiet_min_years=51, quiet_step_years=25),
                       "history: tuning: quiet_min_years must not be above 2 x quiet_step_years"),
                      (dict(eng, quiet_min_years=50, quiet_step_years=25), None),
                      (dict(eng, quiet_min_years=401), "history: tuning: quiet_min_years outside its schema range")):
        got = entry_checks(data(), Inputs(0, 64, dict(span=6000, races=[]), [dict(id=1)], [], [], [], [(1, 1, 0, 5999, "A")]),
                           eng=tun)
        c.eq("r15 entry check quiet_min_years %d, Q %d" % (tun["quiet_min_years"], tun["quiet_step_years"]),
             got if got is None or "quiet_min" in got else "other: " + got, want)


def check_templates(c):
    k = find_kind(COMPILED, "awakening")
    c.eq("t ok", template_problem(k, "{creator} awakened the {race:plural} in {@region}."), None)
    c.eq("t unmatched", template_problem(k, "a } b"), "unmatched } at byte 2")
    c.eq("t unclosed", template_problem(k, "ab {race"), "unclosed placeholder at byte 3")
    c.eq("t unclosed brace", template_problem(k, "{race{creator}"), "unclosed placeholder at byte 0")
    c.eq("t unknown", template_problem(k, "x{nobody}"), "unknown placeholder '{nobody}' at byte 1")
    c.eq("t form", template_problem(k, "{creator:plural}"), "form 'plural' needs a race role at byte 0")
    c.eq("t settlement", template_problem(k, "{@settlement}"), "{@settlement} needs a kind with a settlement at byte 0")
    c.eq("t region", template_problem(find_kind(COMPILED, "flare_up"), "{@region}"),
         "{@region} needs a kind with a region at byte 0")
    c.eq("t empty", template_problem(k, ""), "the template is empty")
    c.eq("t ascii", template_problem(k, "caf\xc3\xa9".encode("latin-1")), "non-ASCII byte at byte 3")
    c.eq("t optional", template_problem(TEST_KIND, "x {actor}"), "role 'actor' may have no participant at byte 2")
    c.eq("t optional all", template_problem(TEST_KIND, "{actor:all}"), None)
    c.eq("t braces", template_problem(k, "{{}}"), None)
    c.eq("join", [join_all([]), join_all(["A"]), join_all(["A", "B"]), join_all(["A", "B", "C"])],
         ["", "A", "A and B", "A, B and C"])
    c.eq("report_events messages", report_events_messages(REPORT_CHECK_CASE), [
        "events[1]: kind 'duel' is not an event kind",
        "events[2]: kind 'awakening' is already listed at events[0]",
        "events[3]: template: unclosed placeholder at byte 0",
        "events[4]: template: the template is empty",
        "event kind 'flare_up' has no entry"] + ["event kind '%s' has no entry" % k for k in P3_05_KEYS + P3_06_KEYS
                                                 + (["settlement_abandoned_to"] if at_least("0.17.0") else [])
                                                 + (["refugees_settled", "tribute", "war_won", "search_abandoned",
                                                     "refuge_held"] if at_least("0.18.0") else [])
                                                 + (["plague_outbreak", "quiet_stretch"] if at_least("0.19.0") else [])
                                                 + (["claim_settlements", "settlements_ceded"]
                                                    if at_least("0.20.0") else [])])
    # rendering on a hand-made view: :all with 0..3, ? with 0, 1, 2
    cat = {99: dict(TEST_KIND, region_use=1, templ="[{actor:all}] {people:plural}/{people:adjective}/{people} at {@region}: "
                                     "{count} {count?thing|things} {{ok}} {@year}-{@end_year}")}
    names = {CIV: {1: "Ash", 2: "Birch", 3: "Cedar"}, REGION: {7: "Vale"}}
    forms = dict(name="Elf", plural="Elves", adjective="Elven")

    def nm(t, x, form):
        return forms[form] if t == RACE else names[t][x]

    out = []
    for nact, cnt in ((0, 0), (1, 1), (2, 2), (3, -5)):
        parts = [(i + 1, 1, CIV, i + 1) for i in range(nact)] + [(nact + 1, 2, RACE, 1), (nact + 2, 3, REGION, 7)]
        view = dict(catalogue=cat, name=nm, events={1: dict(year=10, end_year=12, kind=99, region=7, settlement=0,
                                                            participants=parts, values={1: cnt})})
        out.append(render(view, 1))
    c.eq("render all and ?", out, [
        "[] Elves/Elven/Elf at Vale: 0 things {ok} 10-12",
        "[Ash] Elves/Elven/Elf at Vale: 1 thing {ok} 10-12",
        "[Ash and Birch] Elves/Elven/Elf at Vale: 2 things {ok} 10-12",
        "[Ash, Birch and Cedar] Elves/Elven/Elf at Vale: -5 things {ok} 10-12"])


def check_entry(c):
    dat = data()
    import copy
    inp = synthetic_inputs([0, 2074], 6000)
    c.eq("entry ok", entry_checks(dat, inp), None)
    eng = copy.deepcopy(dat.engine)
    eng["quiet_step_years"] = 0
    c.eq("tuning range", entry_checks(dat, inp, eng=eng), "history: tuning: quiet_step_years outside its schema range")
    eng = copy.deepcopy(dat.engine)
    eng["dense"]["flare_years"] = 1001
    c.eq("tuning level range", entry_checks(dat, inp, eng=eng),
         "history: tuning: dense.flare_years outside its schema range")
    eng = copy.deepcopy(dat.engine)
    eng["simulated_min_permille"] = 460
    c.eq("tuning bounds", entry_checks(dat, inp, eng=eng),
         "history: tuning: simulated_min_permille must not be above simulated_max_permille")
    eng = copy.deepcopy(dat.engine)
    eng["normal"]["active_years"] = 400
    c.eq("tuning budget", entry_checks(dat, inp, eng=eng),
         "history: tuning: normal: 6 x active_years + final_years must not be above 6 x "
         "simulated_max_permille - 30")
    c.eq("report", entry_checks(dat, inp, report=REPORT_CHECK_CASE),
         "history: report: events[1]: kind 'duel' is not an event kind")
    inp2 = synthetic_inputs([0, 2074], 6000)
    inp2.params = dict(inp2.params, span=1999)
    c.eq("span", entry_checks(dat, inp2), "history: settings: history_span 1999 outside 2000 to 12000")
    inp2 = synthetic_inputs([0, 2074], 6000)
    inp2.params = dict(inp2.params, races=["elf", "orc"])
    c.eq("races", entry_checks(dat, inp2), "history: settings: param.races does not match the present race rows")
    inp2 = synthetic_inputs([0, 2074], 6000)
    inp2.regions = []
    c.eq("no regions", entry_checks(dat, inp2), "history: input precondition: no region rows")
    inp2 = synthetic_inputs([0, 2074], 6000)
    inp2.ages = []
    c.eq("no ages", entry_checks(dat, inp2), "history: input precondition: no age rows")


def check_stub_sets(c):
    """S1 sets 2 and 3 and the S2 row-major fallback on a hand-made geography
    (16.2 item 17 in small): three races, one non-islet region."""
    dat = data()
    regs = [plain_region(1, "Isle", 0, 0, 1, 1, 4, 4, 90),            # islet (4 dry cells)
            plain_region(2, "Main", 2, 0, 5, 3, 2000, 10, 50, seed_xy=(3, 1))]
    n = 8
    reg = [0] * (n * n)
    wat = [0] * (n * n)
    for y in range(0, 2):
        for x in range(0, 2):
            reg[y * n + x] = 1
    for y in range(0, 4):
        for x in range(2, 6):
            reg[y * n + x] = 2
    wat[0 * n + 2] = 2  # a lake cell first in row-major order of region 2's box
    inp = synthetic_inputs([0, 2074, 3242], 6000, regions=regs, races=["elf", "goblin", "dwarf"])
    inp.map_size = n
    inp.layers = lambda: (reg, wat)
    res = history_stage(dat, inp)
    w = res["_world"]
    c.eq("S1 sets", [x["home_region"] for x in w.civs], [2, 2, 2])
    c.eq("S2 sites", [(s["x"], s["y"]) for s in w.settlements], [(3, 1), (3, 0), (4, 0)])
    c.eq("one region_control row", res["region_control"], [(2, 0, None, 1, 2)])
    regs = [plain_region(1, "Isle", 0, 0, 1, 1, 4, 4, 90), plain_region(2, "Rock", 2, 0, 3, 0, 2, 2, 95)]
    inp = synthetic_inputs([0, 2074], 6000, regions=regs, races=["elf", "goblin"])
    inp.map_size = 4
    inp.layers = lambda: ([1, 1, 2, 2] + [1, 1, 0, 0] + [0] * 8, [0] * 16)
    res = history_stage(dat, inp)
    c.eq("S1 set 3", [x["home_region"] for x in res["_world"].civs], [2, 2])
    c.eq("S1 set 3 sites", [(s["x"], s["y"]) for s in res["_world"].settlements], [(2, 0), (3, 0)])
    c.eq("S1 set 3 one region_control row", res["region_control"], [(2, 0, None, 1, 2)])


def check_properties(c, seeds=40):
    """16.2 item 15 in small: invariant 1 over seeds x spans x levels x the
    63 subsets, with and without ForcedTension(25k, 5000, 1)."""
    eng = data().engine
    chron = data().chron
    subsets = [s for k in range(1, 7) for s in itertools.combinations([r["id"] for r in chron.races], k)]
    bad = 0
    count = 0
    for seed in range(seeds):
        for span in (2000, 2001, 6000, 11999, 12000):
            for lv in DETAILS:
                for sub in subsets[:: (7 if seed % 2 else 5)]:
                    starts = starts_of(seed, span, ",".join(sub))
                    for t in (None, ForcedTension(25, 5000, 1), ForcedTension(75, 5000, 1),
                              ForcedTension(175, 5000, 1)):
                        en = schedule_phases(starts, span, eng, lv, t)
                        count += 1
                        ok = invariant_1(en.phases, starts, span, en.base, en.simulated())
                        bad += 0 if ok else 1
    c.eq("invariant 1 over %d schedules" % count, bad, 0)


def invariant_1(phases, starts, span, base, sim):
    prev = -1
    for r in phases:
        if r[3] != prev + 1 or r[4] < r[3]:
            return False
        prev = r[4]
    if prev != span - 1:
        return False
    op = [r for r in phases if r[2] == 1]
    if [r[3] for r in op] != list(starts):
        return False
    fin = [r for r in phases if r[2] == 4]
    if len(fin) > 1 or (fin and fin[0] is not phases[-1]):
        return False
    for i, r in enumerate(phases):
        if r[2] == 3:
            if i == 0 or phases[i - 1][2] != 2:
                return False
    return base["floor"] <= sim <= base["cap"]


def check_golden(c, inputs):
    if os.path.exists(GOLDEN) and os.path.isdir(inputs):
        with open(GOLDEN, "rb") as f:
            c.eq("golden file current", f.read().decode(), golden_text(inputs))
    else:
        print("note: golden comparison skipped (no golden file or no input worlds)")


def cmd_check(argv):
    inputs = DEFAULT_INPUTS
    if "--inputs" in argv:
        inputs = argv[argv.index("--inputs") + 1]
    c = Checker()
    check_data(c)
    check_scale(c)
    check_edges(c)
    # 17 as written (generator 0.14.0 to 0.16.0): the 0.16.0 rules on the 0.16.0 inputs.
    saved = ENGINE_VERSION
    use_version("0.16.0")
    try:
        check_example(c, OLD_INPUTS)
        check_templates(c)
    finally:
        use_version(saved)
    if at_least("0.19.0"):   # r10's example is 0.18.0's (quiet eras, P1-06 r3 names): on the 0.18.0 inputs
        use_version("0.18.0")
        try:
            check_example_r10(c, P3_08_INPUTS)
        finally:
            use_version(saved)
        check_example_r12(c, inputs)
        check_render_r12(c)
        if RENDER_R17:
            check_render_r17(c)
        check_quiet_r12(c, inputs)
        if QUIET_R15:
            check_quiet_r15(c, inputs)
    else:
        check_example_r10(c, inputs)
    with no_r17():   # r10's grammar cases and 8.5's examples were written with digits (r17 21.1 in check_render_r17)
        check_templates(c)
        check_render_r10(c)
    check_relation_bookkeeping(c)
    check_entry(c)
    check_stub_sets(c)
    check_properties(c)
    if "--no-golden" not in argv:
        check_golden(c, inputs)
    for root, name in sorted(FALLBACK_USED):
        print("note: %s has no history/%s; the listing of spec 4.1/4.2 stands in" % (root, name))
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def cmd_golden(argv):
    out = GOLDEN
    inputs = DEFAULT_INPUTS
    rest = list(argv)
    if "--inputs" in rest:
        i = rest.index("--inputs")
        inputs = rest[i + 1]
        del rest[i:i + 2]
    if rest:
        out = rest[0]
    text = golden_text(inputs)
    if out == "-":
        sys.stdout.write(text)
    else:
        with open(out, "w", newline="\n") as f:
            f.write(text)
    return 0


def parse_tension(argv):
    if "--tension" in argv:
        p, v, t = (int(x) for x in argv[argv.index("--tension") + 1].split(","))
        return ForcedTension(p, v, t)
    return None


def cmd_schedule(argv):
    pos = [a for a in argv if not a.startswith("--")]
    t = parse_tension(argv)
    if t is not None:
        pos = [a for a in pos if a != argv[argv.index("--tension") + 1]]
    seed, span = int(pos[0], 0), int(pos[1])
    detail = pos[2] if len(pos) > 2 else "normal"
    races = pos[3] if len(pos) > 3 else "all"
    starts = starts_of(seed, span, races)
    print("starts %s" % " ".join(map(str, starts)))
    for line in sched_lines("s", starts, span, detail, t):
        print(line)
    return 0


def cmd_world(argv):
    path = argv[0]
    root = argv[argv.index("--data") + 1] if "--data" in argv else "game/data"
    t = parse_tension(argv)
    dat = data(root)
    conn = pchr.open_world(path)
    inp = inputs_from_world(conn)
    res = history_stage(dat, inp, t)
    for k, v in sorted(table_hashes(res).items()):
        print("table %s %s" % (k, v))
    for line in report_lines(res, inp.params["span"]):
        print(line)
    for e, text in zip(res["event"], sentences(res, inp)):
        print("e%d %d %s" % (e[0], e[1], text))
    return 0


def stored_rows(conn, table):
    _h, rows = pchr.world_table_hash(conn, table)
    return [tuple(r) for r in rows]


def check_stored_invariants(c, conn, res_db, e):
    """12 invariants 1, 2, 4, 5 on the stored rows."""
    ph = res_db["phase"]
    prev = -1
    ok = True
    for r in ph:
        ok = ok and r[3] == prev + 1 and r[4] >= r[3]
        prev = r[4]
    c.true("inv 1 tiling", ok and prev == e - 1)
    ev = res_db["event"]
    c.eq("inv 2 ids", [x[0] for x in ev], list(range(1, len(ev) + 1)))
    c.true("inv 2 years non-decreasing", all(a[1] <= b[1] for a, b in zip(ev, ev[1:])))
    c.true("inv 2 end_year", all(x[2] >= x[1] for x in ev))
    c.true("inv 2 causes below", all(cc < x for x, cc in res_db["event_cause"]))
    phase_of = {}
    for r in ph:
        phase_of[r[0]] = (r[3], r[4])
    c.true("inv 2 event phase", all(phase_of[x[4]][0] <= x[1] <= phase_of[x[4]][1] for x in ev))
    rc = res_db["region_control"]
    by = {}
    for r in rc:
        by.setdefault(r[0], []).append(r)
    okr = True
    for rows in by.values():
        rows.sort(key=lambda r: r[1])
        okr = okr and sum(1 for r in rows if r[2] is None) <= 1
        for a, b in zip(rows, rows[1:]):
            okr = okr and a[2] is not None and a[2] <= b[1]
        okr = okr and all(r[2] is None or r[2] > r[1] for r in rows)
    c.true("inv 4 territory", okr)
    cs = res_db["settlement_census"]
    st = {s[0]: s for s in res_db["settlement"]}
    first = {}
    last = {}
    for r in cs:
        first.setdefault(r[0], r)
        last[r[0]] = r
    c.true("inv 5 first census at founding", all(first[s][1] == st[s][6] for s in st))
    c.true("inv 5 last census = end state", all(last[s][2:] == (st[s][8], st[s][9], st[s][10], st[s][11])
                                                for s in st))


def cmd_check_world(argv):
    path = argv[0]
    root = argv[argv.index("--data") + 1] if "--data" in argv else "game/data"
    dat = data(root)
    conn = pchr.open_world(path)
    meta = dict(conn.execute("SELECT key, value FROM meta").fetchall())
    c = Checker()
    if meta.get("schema_version") != str(SCHEMA_VERSION):
        print("%s: schema version %s, check-world reads %d" % (path, meta.get("schema_version"), SCHEMA_VERSION))
        return 1
    c.eq("history_end_year", meta.get("history_end_year"), meta.get("param.history_span"))
    stages = meta["stages"].split(",") if meta["stages"] else []
    p = pchr.params_from_meta(meta)
    if "chronology" in stages:
        chron = pchr.chronology(dat.chron, p, int(meta["seed"]), pchr.world_region_names(conn))
        for t in pchr.TABLES:
            got, _ = pchr.world_table_hash(conn, t)
            c.eq("table %s" % t, got, pchr.table_hash(pchr.COLUMNS[t], chron[t]))
    if "history" in stages:
        inp = inputs_from_world(conn)
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
        check_stored_invariants(c, conn, stored, inp.params["span"])
        view = view_from_rows(stored, [(r["id"], r["name"]) for r in inp.regions], inp.races, inp.gods)
        for k in view["catalogue"].values():
            c.eq("template of %s parses" % k["key"], template_problem(k, k["templ"]), None)
        c.eq("sentences", [render(view, e[0]) for e in stored["event"]], sentences(res, inp))
        c.true("one awakening per present race", sum(1 for e in stored["event"] if e[3] == 1) == len(inp.ages))
    else:
        for t in TABLES:
            c.eq("table %s empty" % t, conn.execute('SELECT count(*) FROM "%s"' % t).fetchone()[0], 0)
    conn.close()
    print("%s: %d checks, %d failures" % (path, c.count, c.failures))
    return 1 if c.failures else 0


use_version("0.20.0")


def main(argv):
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
    if cmd == "schedule":
        return cmd_schedule(argv[1:])
    if cmd == "world":
        return cmd_world(argv[1:])
    if cmd == "check-world":
        return cmd_check_world(argv[1:])
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
