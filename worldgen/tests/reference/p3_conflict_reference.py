#!/usr/bin/env python3
"""Independent exact-integer reference of P3-06, expansion and conflict.

Written by the test writer from docs/specs/p3-06-expansion-conflict.md (r4)
alone, with the seams of docs/specs/p3-04-simulation-engine.md (r7: 6.1,
6.4, 6.5, 6.7, 6.9, W1-W9) and docs/specs/p3-05-peoples.md (r4: 8.5 `ruin`,
13), without reading any implementation and without the spec writer's
prototype. It reproduces the history stage of generator 0.16.0 (schema 7):
the peoples of p3_peoples_reference.py with the conflict subsystem installed
after them (steps T2, T6, Q4; sections 4 to 8) and as the one tension
source (5.4, asked by the engine in Q5, SC5/SC6), and writes every row of
the thirteen history tables, `relation` included (P3-04 6.7 item 2).

Libraries reused, each written independently from its own spec:
core_reference.py (P0-01), p3_data_reference.py (P0-03/P3-01, extended for
history.conflict, prowess_permille and the 0.16.0 race data),
p3_names_reference.py (P3-02), p3_chronology_reference.py (P3-03),
p3_engine_reference.py (P3-04, the 0.16.0 catalogue of 30 kinds),
p3_peoples_reference.py (P3-05).

Readings of the spec where it leaves a choice open are marked READING below
and listed in SPEC_GAPS (printed by `gaps`).

Python 3 standard library only, arbitrary-precision integers, no floating
point.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p3_conflict_reference.py check [--inputs DIR] [--quick] [--write]
        Self-checks: the data (history.conflict = the listing, the race
        changes, the catalogue of kinds 20-30 and the report entries), the
        worked example W1 of 16.1 and hand-made units, the invariants of 14
        and the hard bounds on the synthetic and stress cases and the world
        cases, draw independence (14 item 9), the sample of 15.2 (seed 42 at
        1024: the war list and the sixty summary lines), and the committed
        golden file against a fresh computation (skipped with --quick, which
        also runs fewer synthetic seeds and only one 1024 world; --write
        writes the golden file from the run instead of comparing). Exit 1
        on any disagreement.

    python3 -B worldgen/tests/reference/p3_conflict_reference.py golden [PATH|-] [--inputs DIR]
        Writes worldgen/tests/golden/p3-06-conflict.txt. World cases read
        their geography from DIR (default
        ~/.cache/the-unmapped/out/p3-06-tests/inputs: one <case>.db per
        case, any world whose stages include regions; example-42-1024.db is
        the input of hist-conflict-42-1024).

    python3 -B worldgen/tests/reference/p3_conflict_reference.py world DB [--data ROOT] [--span E] [--races R]
                [--detail D] [--civs C] [--intensity I]
        Recomputes the history of a world from its stored geography (the
        chronology recomputed for the given settings, default the world's
        meta) and prints the table hashes, [history] and [stats], the wars,
        the tensions, every event in 15.2's line form, the end state and the
        counts of 15.3.

    python3 -B worldgen/tests/reference/p3_conflict_reference.py check-world DB [--data ROOT]
        Checks a schema-7 world written by the C++ (validation sweep):
        recomputes the five chronology tables and the thirteen history tables
        from the stored geography and meta and compares them table by table
        (naming the first differing row), and re-checks every invariant of 14
        (and P3-05's) and the hard bounds of 15.5 from the stored rows.
        Exit 1 on a difference.

    python3 -B worldgen/tests/reference/p3_conflict_reference.py synthetic SEED [--stress]
        Prints the history of a synthetic case (the C++ builds the same).

    python3 -B worldgen/tests/reference/p3_conflict_reference.py gaps
        Prints the spec gaps and the readings taken.

Never regenerate the golden file to make a C++ test pass.
"""

import copy
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import core_reference as cr  # noqa: E402
import p3_chronology_reference as pchr  # noqa: E402
import p3_data_reference as pdr  # noqa: E402
import p3_engine_reference as pe  # noqa: E402
import p3_names_reference as pnr  # noqa: E402
import p3_peoples_reference as pp  # noqa: E402

FatalError = cr.FatalError
REPO = pe.REPO
GOLDEN = os.path.join(REPO, "worldgen", "tests", "golden", "p3-06-conflict.txt")
DEFAULT_INPUTS = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p3-06-tests", "inputs")
GENERATOR_VERSION = "0.18.0"

# P3-06 r7 (generator 0.17.0) and r9 section 20 (generator 0.18.0, P3-09): each rule change behind a
# flag, as pp.FLAGS for P3-05; use_version(v) sets the flags of generator v in both files (and the data,
# names and catalogue of the engine reference); legacy() is use_version("0.16.0").
FLAGS = dict(
    loss=True,      # r7 7.3 item 4: dead_W = min(F_W, F_L) x loss_winner div 1000
    # r9 (P3-09)
    flight=True,    # 20.1: a fallen last refuge flees into hiding (kind 32), hidden_until
    goals=True,     # 20.2: allowed goals (no capital region unless alone), the declaration's re-choice
    cost=True,      # 20.3: a lost war costs the loser another region or a tribute (kind 33)
    weary=True,     # 20.4: war weariness per (defender, region)
    revenge=True,   # 20.5: wronged regions, effective aggression g'
    muster=True,    # 20.6 item 1: muster in force and in the exhaustion test
    levies=True,    # 20.6 item 2: local force in battles
    odds=True,      # 20.6 item 3: odds weighted by the goal's defence, floor, minimum (and tension skips)
    truce=True,     # 20.6 item 4: the directional truce
    conceal=True,   # 20.7 item 1: concealment
    home=True,      # 20.7 item 2: home defence in def and garrison
    enmity=True,    # 20.8 item 1: enmity_hops for a pair with an enmity row
    # r10 (P3-09 r3)
    score=True,     # 20.15: the score of dead and people taken, the exhausted leader wins, the last battle, war_won
    kill=True,      # 20.16: dead_L bounded by F_W x kill_max_permille
    search=True,    # 20.17: concealed settlements are present but must be found (replaces 20.7 item 1's hiding)
    floor=True,     # 20.20 item 1: casualties never take a race's last refuge below abandon_min
    held=True,      # 20.22: refuge_held (kind 36) instead of the siege when the guard holds
    weary_open=True,  # 20.23 (r3, as the lead stated it; not yet in the spec text here): futile + open >= weary
)
FLAGS_017 = ("loss",)


def use_version(v):
    """Generator v's rules, data, names and catalogue: "0.18.0" (default), "0.17.0" or "0.16.0"."""
    global GENERATOR_VERSION
    pp.use_version(v)
    for k in FLAGS:
        FLAGS[k] = v == "0.18.0" or (v == "0.17.0" and k in FLAGS_017)
    GENERATOR_VERSION = v
    _DATA.clear()


def legacy(on=True):
    use_version("0.16.0" if on else "0.18.0")


class rules:
    """with rules("0.17.0"): the conflict and peoples rule flags of another version (the data stay)."""

    def __init__(self, v):
        self.v = v

    def __enter__(self):
        self.saved = (dict(FLAGS), dict(pp.FLAGS))
        for k in FLAGS:
            FLAGS[k] = self.v == "0.18.0" or (self.v == "0.17.0" and k in FLAGS_017)
        for k in pp.FLAGS:
            pp.FLAGS[k] = self.v != "0.16.0" and (self.v == "0.18.0" or k not in pp.FLAGS_018)
        return self

    def __exit__(self, *a):
        FLAGS.clear()
        FLAGS.update(self.saved[0])
        pp.FLAGS.clear()
        pp.FLAGS.update(self.saved[1])

RACE, GOD, CIV, SETTLEMENT, REGION = pe.RACE, pe.GOD, pe.CIV, pe.SETTLEMENT, pe.REGION
OPENING, QUIET, FLARE, FINAL = pe.OPENING, pe.QUIET, pe.FLARE, pe.FINAL
HAMLET, VILLAGE, TOWN, CITY, FORTRESS = 1, 2, 3, 4, 5
THRIVING, DECLINING, RUINED, FADED = 1, 2, 3, 4
KIND_KEY = pp.KIND_KEY
GOOD, NEUTRAL, EVIL = 0, 1, 2
CAMP_FIELD = {(0, 0): "good_good", (0, 1): "good_neutral", (0, 2): "good_evil", (1, 1): "neutral_neutral",
              (1, 2): "neutral_evil", (2, 2): "evil_evil"}
BIOME_KEYS = {1: "ocean", 2: "lake", 3: "temperate_forest", 4: "grassland", 5: "marsh", 6: "highland_moor",
              7: "alpine", 8: "blighted_waste", 9: "heath", 10: "steppe", 11: "wet_woodland", 12: "fen"}
COVER_BIOMES = ("temperate_forest", "wet_woodland", "fen", "marsh")


def fatal(msg):
    raise FatalError(msg)


SPEC_GAPS = (
    ("7.2 item 2 / item 3", "When a = a* but b* is not a candidate (sheltered, out of contact, ...), item 3 "
                            "applies to a as to every other civilization ('Otherwise' read per civilization). The "
                            "flare pair is computed once per T2, after the strain update and the claims, before "
                            "the first declaration; a best value of 0 (no claimed pair) means no flare pair."),
    ("7.1 pop0", "pop0 = PeoplesView::population at the declaration: the sum over the side's standing settlements, "
                 "including a colony founded by peoples earlier in the same T2 (P3-05 13 defines population "
                 "without a visibility filter; 8.3 item 4 does not list pop0 among what invisible colonies miss)."),
    ("7.3 item 5 values", "winner_dead and loser_dead are step 4's dead_W and dead_L as computed, before the "
                          "min(dead, T) of 7.4 (never binding with valid force; equal on every case here)."),
    ("7.3 item 7 / 7.5 'standing settlements in R'", "The besieged town, the aim test of 7.7 item 2 and the goal "
                                                     "re-choice of 7.2 read standing VISIBLE settlements (as "
                                                     "presence does); in T6 every settlement is visible."),
    ("7.7 cession refugees", "The excess of a ceded settlement goes to L's nearest other standing settlement at that "
                             "moment, which may be another settlement of L in G not yet ceded (P3-05 8.5 item 2 "
                             "applied literally, settlements of G in ascending id)."),
    ("7.5 item 3 capture capital", "If the captured t was y's capital and y has another standing settlement, the "
                                   "capital moves (most populous, then smaller id) with `capital_moved` caused by "
                                   "the capture; with none left, 7.6 ends y and no capital_moved is appended."),
    ("7.6 claims and strain", "A conquest drops every claim involving y and closes every war of y (wars(c) of "
                              "its other sides fall); no strain, grudge or last_peace changes."),
    ("8.1 item 1 order", "Relations closed in housekeeping and in a conquest are taken in RelationKey order "
                         "(a_type, a_id, b_type, b_id, kind as bytes); wars ended with a side 'are over' without an "
                         "event and leave their war row to that closing."),
    ("5.4 'past their truce'", "Tested at the quiet step's start year Y (the year of Q4)."),
    ("4.2 decay in ticks", "years x grudge_decay_per_century div 100 with years = 1 is 0 at the shipped 50: grudges "
                           "never decay in tick years (literal; 12 per quiet step of 25 years)."),
    ("16.3 (c) tension lines", "One line per quiet step whose tension the engine asks (every quiet step but the "
                               "one ending its interval, SC6), value 0 and trigger e0 included."),
    ("16.3 (b) war lines", "Format of the reference: `<case> war e<w> <start>-<end|open> c<a> c<b> rg<goal> "
                           "claim e<claim> battles <n> sacked <n> taken <n> <how> <outcome>`, how one of "
                           "exhaustion aim no-front length quiet conquest ended open, outcome won-c<W>[-ceded] "
                           "truce or -."),
    ("15.4 settlements_sacked", "Counted from current rows: a sacked ruin that has faded no longer has a ruin_event "
                                "(P3-05 ruling 3), so it is not counted."),
    ("15.4 war_years_total E", "E = param.history_span (meta.history_end_year)."),
    ("16.2 item 18a no claim", "A world with no claim equals the peoples-only world in every history table but "
                               "the three catalogue tables AND relation: conflict's housekeeping opens enmity rows "
                               "without any claim (hist-races-elf-orc-1024 has them)."),
    ("14 item 4 conqueror alive", "Alive at the conquest: ended_year NULL, or later than the year, or the same year "
                                  "with an ending event after the conquest."),
    # r9 (P3-09, generator 0.18.0)
    ("r9 20.1 presence", "'c has a hidden settlement' in R counts every standing settlement of c, a colony founded "
                         "earlier in the step included (concealment belongs to the place); the prototype counted "
                         "visible settlements only."),
    ("r9 20.1 h_x cap", "h_x(D) is capped at march_hops + 1 for every pair, also an enmity pair whose fronts use "
                        "enmity_hops (the text names march_hops)."),
    ("r9 20.1 hidden where", "A hidden settlement is no siege target and no claim cause; the aim test of 7.7, 7.7's "
                             "and 20.3's transfer of 'each standing visible settlement' and the casualties of 7.4 "
                             "(battles, tributes) still include hidden settlements (the text changes only presence, "
                             "targets and the tribute's n)."),
    ("r9 20.2 dropped claim", "'the claim is dropped' removes a's claim on b only (the prototype dropped both "
                              "directions); strains are kept."),
    ("r9 20.3 front", "The cession candidates of 20.3 are read from front(W, L) as it stands after the peace, with "
                      "the pair's hops and hidden settlements out of presence; weary regions are not excluded."),
    ("r9 20.3 quiet wait", "In Q4 an end waits for the loser's budget when 7.7's cession, 20.3's cession or a "
                           "tribute of n > 0 would follow (no wait when L is a last refuge or n = 0)."),
    ("r9 20.4 moment", "The futile count is updated at the peace or truce, before any cession (equivalent for reads)."),
    ("r9 20.6 odds region", "The odds weigh the defence of the claim region G (the claim's region, before 20.2's "
                            "re-choice); 1000 when G is not in front(a, b)."),
    ("r9 20.7 race", "A settlement's race (concealment, home defence) is its owner's race."),
    ("r9 20.13 item 2", "Closed by r10 20.20 (the finding of stress cases 1, 12, 14 at r2); checked as an ordinary "
                        "invariant again."),
    # r10 (P3-09 r3)
    ("r10 20.22 refuge_flight 0", "With refuge_flight = 0 (the guard of 0.16.0) the held siege stays a `siege` with "
                                  "fell 0: refuge_held's reasons (no destination, too few) presuppose the flight rule."),
    ("r10 20.17 who searches", "x is the side of 7.3's battle (the war's initiative, or the other side when it has no "
                               "front); y the side whose settlements at R are concealed; one search per war per T6, "
                               "key (w, R); a year with no search (a battlefield needing none) leaves `unfound` as it "
                               "is."),
    ("r10 20.15 taken", "taken(c) adds the settlement's P at the fall before any toll: for a sack or capture before "
                        "7.5's toll, for a flight before the sack toll of 20.1 item 1."),
    ("r10 20.20 item 1 P", "P is the refuge's population (its one standing visible settlement) at the casualty; the "
                           "floor applies whatever last_refuge says."),
    ("r10 T3 from rows", "Region owners at year y are the region_control rows with from_year <= y < to_year (or "
                         "open); the site test blocks every settlement founded before the event and not faded by it."),
)


# ---------------------------------------------------------------------------
# 11: the catalogue (kinds 20-30, transcribed; must equal the engine reference's)
# ---------------------------------------------------------------------------

def _k(kid, key, region_use, settlement_use, roles, values):
    return pe.kind_def(kid, key, 1, region_use, settlement_use, roles, values)


CONFLICT_KINDS = (
    _k(20, "claim", 1, 0, (("claimant", CIV, 1, 1), ("holder", CIV, 1, 1)), ()),
    _k(21, "war_declared", 1, 0, (("attacker", CIV, 1, 1), ("defender", CIV, 1, 1)), ()),
    _k(22, "battle", 1, 0, (("winner", CIV, 1, 1), ("loser", CIV, 1, 1)), ("winner_dead", "loser_dead")),
    _k(23, "battle_at_pass", 1, 0, (("winner", CIV, 1, 1), ("loser", CIV, 1, 1)), ("winner_dead", "loser_dead", "pass")),
    _k(24, "siege", 1, 1, (("attacker", CIV, 1, 1), ("defender", CIV, 1, 1)), ("fell",)),
    _k(25, "sacked", 1, 1, (("attacker", CIV, 1, 1), ("defender", CIV, 1, 1)), ("dead", "survivors")),
    _k(26, "settlement_captured", 1, 1, (("attacker", CIV, 1, 1), ("defender", CIV, 1, 1)), ("dead", "fled")),
    _k(27, "peace", 0, 0, (("winner", CIV, 1, 1), ("loser", CIV, 1, 1)), ("years",)),
    _k(28, "truce", 0, 0, (("parties", CIV, 2, 2),), ("years",)),
    _k(29, "region_ceded", 1, 0, (("from", CIV, 1, 1), ("to", CIV, 1, 1)), ()),
    _k(30, "civ_conquered", 1, 0, (("civilization", CIV, 1, 1), ("conqueror", CIV, 1, 1)), ()),
)
# r9 20.9 (generator 0.18.0): kinds 32 and 33, transcribed (must equal the engine reference's)
CONFLICT_KINDS_018 = (
    pe.kind_def(32, "refugees_settled", 1, 1, 1, (("settlement", SETTLEMENT, 1, 1), ("founder", CIV, 1, 1),
                                                  ("origin", SETTLEMENT, 1, 1)), ("population",)),
    pe.kind_def(33, "tribute", 1, 0, 0, (("payer", CIV, 1, 1), ("payee", CIV, 1, 1)), ("people",)),
    # r10 20.15, 20.17, 20.22
    pe.kind_def(34, "war_won", 1, 0, 0, (("winner", CIV, 1, 1), ("loser", CIV, 1, 1)), ("years",)),
    pe.kind_def(35, "search_abandoned", 1, 1, 0, (("searcher", CIV, 1, 1), ("hidden", CIV, 1, 1)), ("years",)),
    pe.kind_def(36, "refuge_held", 1, 1, 1, (("attacker", CIV, 1, 1), ("defender", CIV, 1, 1)), ("reason",)),
)
COMPILED = pp.COMPILED      # kinds 1-33 (P3-05 r6 appends 31 after P3-06's 20-30; r9 32 and 33)
assert list(COMPILED[19:30]) == list(CONFLICT_KINDS) and len(COMPILED) == 36
assert list(COMPILED[31:36]) == list(CONFLICT_KINDS_018)
KIND_ID = {k["key"]: k["id"] for k in COMPILED}
KEY_OF = {k["id"]: k["key"] for k in COMPILED}
K = KIND_ID

SPEC_REPORT = (
    ("claim", 45, "{claimant} laid claim to {@region}, held by {holder}."),
    ("war_declared", 70, "{attacker} declared war on {defender} over {@region}."),
    ("battle", 30, "{winner} defeated {loser} in {@region}; {loser_dead} of the defeated and {winner_dead} of the "
                   "victors fell."),
    ("battle_at_pass", 35, "{winner} defeated {loser} at the pass into {@region}; {loser_dead} of the defeated and "
                           "{winner_dead} of the victors fell."),
    ("siege", 25, "{attacker} besieged {@settlement} of {defender}; it {fell?fell|held}."),
    ("sacked", 75, "{attacker} sacked {@settlement} of {defender}; {dead} died and {survivors} fled."),
    ("settlement_captured", 60, "{attacker} took {@settlement} from {defender}; {dead} died{fled?| and one fled| "
                                "and # fled}."),   # r7 (0.16.0: "...; {dead} died and {fled} fled.")
    ("peace", 60, "{loser} sued {winner} for peace after {years} {years?year|years} of war."),
    ("truce", 50, "{parties:all} made peace after {years} {years?year|years} of war, neither the victor."),
    ("region_ceded", 55, "{from} ceded {@region} to {to}."),
    ("civ_conquered", 90, "{civilization} fell to {conqueror}."),
)

STREAMS = ("hist.conflict.declare", "hist.conflict.battle", "hist.conflict.battle_win", "hist.conflict.siege",
           "hist.conflict.sack", "hist.conflict.search")   # search: r10 20.17 (0.18.0)

chance_pm = pp.chance_pm


def floor_div(a, b):
    return a // b  # Python's // is floor division


# ---------------------------------------------------------------------------
# Data: the peoples' view plus history.conflict and the race fields of 12.2
# ---------------------------------------------------------------------------

class Data(pp.Data):
    def __init__(self, root):
        super().__init__(root)
        content = pdr.load_root(root, kinds=("history", "race"))
        hist = dict(content["history"])
        self.conflict = hist.get("conflict")
        for key, t in content["race"]:
            rid = self.chron.race_by_id[key]["order"]
            self.race[rid]["aggression"] = t["aggression"]
            self.race[rid]["prowess"] = t["prowess_permille"]
            # P3-01 r9 3.2 (0.18.0); a 0.17.0 root has none (and no rule reads them then)
            self.race[rid]["muster"] = t.get("muster_permille", 1000)
            self.race[rid]["conceal"] = t.get("conceal_min", 1001)
            self.race[rid]["home"] = t.get("home_defense_permille", 0)


_DATA = {}


def data(root="game/data"):
    if root not in _DATA:
        _DATA[root] = Data(root)
    return _DATA[root]


def edited(dat, conflict=None, peoples=None, races=None, engine=None):
    d = pp.edited(dat, peoples=peoples, races=races, engine=engine)
    d.conflict = copy.deepcopy(dat.conflict)
    d.conflict.update(conflict or {})
    return d


def tuning_problem(t):
    """16.4 tuning messages at configuration, in the order of 12.1."""
    if t is None:
        return "history: tuning: the content has no definition history.conflict"
    for n, lo, hi in pdr.conflict_ranges():
        v = t[n]
        if isinstance(v, dict):
            sch = pdr.CONFLICT_RECORDS[n][1]
            for f, _typ in sch:
                if not (lo <= v[f] <= hi):
                    return "history: tuning: conflict.%s.%s outside its schema range" % (n, f)
        elif not (lo <= v <= hi):
            return "history: tuning: conflict.%s outside its schema range" % n
    msgs = pdr.conflict_check_messages(t)
    if msgs:
        return "history: tuning: " + msgs[0]
    return None


# ---------------------------------------------------------------------------
# The engine with Q5 between Q4 and Q6 (P3-04 6.2: Q1 ... Q4, Q5 tension, Q6)
# ---------------------------------------------------------------------------

class Engine(pe.Engine):
    """pe.Engine asks the tension before the quiet step's hook (its stub
    tension depends on nothing); here the step runs Q1-Q4, then the engine
    asks the tension (Q5, not at the interval's end, SC6), then Q6 with the
    phase-end flag that the tension decides."""

    def run_quiet(self, qs, qe):
        b = self.base
        cur = qs
        while cur < qe:
            row = self.open_phase(QUIET, cur)
            s = cur
            while True:
                ln = min(b["Q"], qe - s)
                if ln < 1:
                    fatal("history: schedule: quiet step length %d below 1" % ln)
                end = s + ln
                self.hooks.quiet_a(self, s, ln, row[3])
                flare = 0
                trig = 0
                if end != qe and self.tension is not None:
                    tv, trig = self.tension(s, ln)
                    if tv < 0:
                        fatal("history: tension: value %d below 0" % tv)
                    if tv >= b["threshold"]:
                        last = self.hooks.last_event_id()
                        if last is not None and not (1 <= trig <= last):
                            fatal("history: tension: trigger %d outside 1 to %d" % (trig, last))
                        flare = min(b["F"], qe - end, b["cap"] - b["sched"] - self.flare_total)
                closes = end == qe or flare >= 1
                self.hooks.quiet_b(self, s, ln, closes, row[3])
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


class Hooks(pe.StubHooks):
    def quiet_a(self, eng, start, length, row_start):
        self.w.quiet_a(eng, start, length, row_start)

    def quiet_b(self, eng, start, length, phase_end, row_start):
        self.w.quiet_b(eng, start, length, phase_end, row_start)


# ---------------------------------------------------------------------------
# The world: peoples with conflict installed after them
# ---------------------------------------------------------------------------

def rel_key(a, b, kind):
    return (CIV, a, CIV, b, kind)


def sym_key(a, b, kind):
    """N4: the one helper for symmetric kinds (lower civilization id first)."""
    return (CIV, min(a, b), CIV, max(a, b), kind)


def key_order(k):
    return (k[0], k[1], k[2], k[3], k[4].encode())


class World(pp.World):
    def __init__(self, dat, inp, cat, extra_draw=False, conflict=True):
        super().__init__(dat, inp, cat, extra_draw)
        self.ct = dat.conflict
        self.conflict_on = conflict
        for s in STREAMS:
            self.keys[s] = cr.stream_key(inp.seed, s)
        self.strain = {}           # (a, b) -> milli-units
        self.claim = {}            # (a, b) -> (event, region)
        self.grudge = {}           # (min, max) -> grudge
        self.last_peace = {}       # (min, max) -> (year, event)
        self.wars = []             # in declaration order
        self.enmity_opened = set()
        self.seen = set()
        self.last_t2_phase = 0
        self.phase_kind = 0
        self.contact = {}          # (a, b) -> k, as computed in the last strain update
        self.version = 0           # bumped by every owner or status change (front cache)
        self._fcache = {}
        self.tension_log = []      # (step start, value, trigger)
        self.flare_values = {}     # flare phase start -> (value, trigger, pair)
        self.biome_code = {v: k for k, v in getattr(inp, "biome_keys", BIOME_KEYS).items()}
        self.border = {}
        for b in inp.borders:
            self.border[(b["a"], b["b"])] = b
        self.n_land = sum(1 for rid in self.geo.ids if not self.geo.islet(rid))
        self.checks6 = []          # in-run conflict invariants
        self.refuge_saves = 0      # sieges that fell but were held by last_refuge (7.3 item 7)
        self.shelter_blocks = 0    # claimed candidates left out by shelter_years (7.2 item 1)
        self.wanted_override = None  # unit tests: a fixed wanted_held
        # r9 members (20.1, 20.4, 20.5), keyed by ids
        self.hidden_until = {}     # settlement -> year it is found (20.1)
        self.futile = {}           # (defender, region) -> (count, owner) (20.4)
        self.wronged = {}          # (a, b) -> set of regions (20.5)
        self._bterm = {}           # (region, race) -> B (P3-05 4.2)
        # r9 counters for the checks and the statistics (not state)
        self.flights = 0           # 20.1 flights
        self.guard_no_dest = 0     # sieges of a last refuge held: no destination
        self.guard_too_few = 0     # ... held: a destination, survivors below abandon_min
        self.guard_with_dest = 0   # ... held although a destination with enough survivors exists (T3: 0)
        self.weary_checks = []     # (step year, kind, defender, region) of every claim and declaration (20.13.5)
        self.checks9 = []          # in-run r9 invariant failures
        self.kills = []            # r10: (F_W, dead_L) of every battle (20.19 item 9)
        self.search_log = []       # r10: (year, war, region, defender, found) of every search (items 10, 11)
        self.held_log = []         # r10: (refuge_held event, settlement, reason) (20.22, T3)

    # --- engine phase kind
    def tick(self, eng_run, year, phase_end):
        self.year, self.years, self.phase = year, 1, eng_run.phase_id
        self.phase_kind = eng_run.phase_kind
        self.t1()
        self.foundings(quiet=False)          # T2: peoples
        if self.conflict_on:
            self.c_t2()                      # T2: conflict (same step)
        self.begin()                         # T3
        self.t4()                            # T4
        self.begin()                         # T5
        self.begin()                         # T6
        if self.conflict_on:
            self.c_t6()
        self.begin()                         # T7
        self.bookkeeping(year, phase_end)
        self.check_state()

    def quiet_a(self, eng_run, start, length, row_start):
        self.year, self.years, self.phase = start, length, eng_run.phase_id
        self.phase_kind = eng_run.phase_kind
        if start == row_start:
            self.open_era = {}
        self.quiet_used = {}
        self.charged = set()
        self.q1()
        self.begin()          # Q2
        self.q3()
        self.foundings(quiet=True)          # Q4: peoples
        if self.conflict_on:
            self.c_q4()                     # Q4: conflict (same step)

    def quiet_b(self, eng_run, start, length, phase_end, row_start):
        last = start + length - 1
        self.begin()          # Q5
        self.begin()          # Q6
        self.quiet_used = None
        self.quiet_eras(start, last)        # P3-04 6.6 (r10 behind pp.FLAGS["quiet"])
        self.bookkeeping(start, phase_end, last)
        if phase_end:
            self.open_era = {}
        self.check_state()

    def quiet_step(self, eng_run, start, length, phase_end, row_start):   # not used by this engine
        raise AssertionError("quiet_step")

    # --- State helpers
    def set_owner(self, rid, civ, event):
        super().set_owner(rid, civ, event)
        self.version += 1

    def open_rel(self, key, e):
        self.open_relation(key, e)      # P3-04 State (6.7 item 2, r9: calls replayed in order)

    def close_rel(self, key, e):
        self.close_relation(key, e)

    def alive(self, c):
        return self.civ(c)["ended_year"] is None

    def avc(self):
        """Alive visible civilizations, ascending."""
        return [c["id"] for c in self.civs if c["ended_year"] is None and self.visible(c)]

    def race_c(self, c):
        return self.civ(c)["race"]

    def camp(self, a, b):
        ka, kb = self.race[self.race_c(a)][5], self.race[self.race_c(b)][5]
        return self.ct["camp"][CAMP_FIELD[(min(ka, kb), max(ka, kb))]]

    def disp(self, a, b):
        return self.camp(a, b) - self.grudge.get((min(a, b), max(a, b)), 0)

    def hostility(self, a, b):
        return max(0, -self.disp(a, b))

    def svis(self, c):
        """c's standing visible settlements, ascending."""
        return [s for s in self.settlements if s["owner_civ"] == c and self.standing(s) and self.visible(s)]

    def population(self, c):
        """PeoplesView::population (READING SPEC_GAPS 7.1 pop0: every standing settlement)."""
        return sum(s["population"] for s in self.settlements if s["owner_civ"] == c and self.standing(s))

    # --- r9 20.1, 20.7: hidden settlements
    def bterm(self, rid, race):
        """B(R, r) of P3-05 4.2 (the biome term of sigma)."""
        k = (rid, race)
        if k not in self._bterm:
            self._bterm[k] = self.geo.parts(rid, race)[1]
        return self._bterm[k]

    def concealed(self, s):
        """20.7 item 1: a standing settlement of race r (its owner's) in R with B(R, r) >= conceal_min(r)."""
        if not FLAGS["conceal"] or not self.standing(s) or s["owner_civ"] == 0:
            return False
        race = self.race_of(s)
        return self.bterm(s["region"], race) >= self.pd.race[race]["conceal"]

    def hidden(self, s):
        """20.1: Y < hidden_until[s] (a fled refuge); before r10 also concealed (20.7). From r10 (20.17) a
        concealed settlement is in presence and must be found."""
        fled = FLAGS["flight"] and self.year < self.hidden_until.get(s["id"], -1)
        return fled or (not FLAGS["search"] and self.concealed(s))

    def svis_seen(self, c):
        """c's standing visible settlements that are not hidden (the targets an army can see)."""
        return [s for s in self.svis(c) if not self.hidden(s)]

    def presence(self, c):
        if not (FLAGS["flight"] or (FLAGS["conceal"] and not FLAGS["search"])):
            return sorted(set(self.held(c)) | {s["region"] for s in self.svis(c)})
        # 20.1: the regions of c's standing visible settlements that are not hidden, with the regions c
        # holds in which c has no hidden settlement (READING: every standing settlement of c counts for
        # "has a hidden settlement", a colony founded earlier in the step included: concealment is a
        # property of the place, SPEC_GAPS r9 20.1 presence)
        hid = {s["region"] for s in self.standing_of(c) if self.hidden(s)}
        return sorted({r for r in self.held(c) if r not in hid} | {s["region"] for s in self.svis_seen(c)})

    def pair_hops(self, x, y):
        """20.8 item 1: enmity_hops for a pair with an enmity row (opened or closed), else march_hops."""
        if FLAGS["enmity"] and (min(x, y), max(x, y)) in self.enmity_opened:
            return self.ct["enmity_hops"]
        return self.ct["march_hops"]

    def reach_dist(self, x, hops=None):
        """(presence(x), d_{H-1}) for x, cached per step, year, state version and H."""
        hops = self.ct["march_hops"] if hops is None else hops
        key = (self.serial, self.year, self.version, x, hops)
        if key in self._fcache:
            return self._fcache[key]
        if len(self._fcache) > 4096:
            self._fcache = {}
        pres = self.presence(x)
        d = {r: 0 for r in pres}
        for _h in range(hops - 1):
            nd = dict(d)
            for u in sorted(d):
                du = d[u]
                for v, c in self.geo.nb[u]:
                    if v not in nd or du + c < nd[v]:
                        nd[v] = du + c
            d = nd
        self._fcache[key] = (pres, d)
        return pres, d

    def front(self, x, y):
        """5.1: [(R, cost, R_from)] over presence(y) ascending (r9 20.8: the pair's hops)."""
        px, d = self.reach_dist(x, self.pair_hops(x, y))
        spx = set(px)
        py, _ = self.reach_dist(y)
        out = []
        for r in py:
            if r in spx:
                out.append((r, 1, r))
                continue
            best = None
            for u, c in self.geo.nb[r]:
                if u in d:
                    cand = (d[u] + c, u)
                    if best is None or cand < best:
                        best = cand
            if best is not None:
                out.append((r, best[0], best[1]))
        return out

    def share(self, c):
        if self.n_land == 0:
            return 0
        return sum(1 for rid in self.held(c) if not self.geo.islet(rid)) * 1000 // self.n_land

    def fear(self, b):
        t = self.ct
        return t["fear_weight"] * max(0, self.share(b) - t["fear_share"]) // (1000 - t["fear_share"])

    def wanted_held(self, c):
        """PeoplesView::wanted_held (P3-05 20): held by another, every outward test but 'free'."""
        if self.wanted_override is not None:
            return list(self.wanted_override.get(c, []))
        t = self.t
        civ = self.civ(c)
        race = civ["race"]
        rd = self.pd.race[race]
        held = self.held(c)
        hops = self.geo.hops(self.stl(civ["capital"])["region"])
        reach = t["reach_base"] + rd["expansion"] * t["reach_per_expansion"] // 1000
        smin = max(t["expand_min_score"], self.geo.sigma(civ["home_region"], race) * t["expand_relative_permille"] // 1000)
        out = set()
        for r0 in held:
            for r1, _k in self.geo.nb[r0]:
                o = self.owner[r1][0]
                if o == 0 or o == c or self.geo.islet(r1):
                    continue
                if r1 not in hops or hops[r1] > reach:
                    continue
                if self.geo.sigma(r1, race) < smin or self.geo.capacity(r1, race) < t["expand_min_capacity"]:
                    continue
                out.add(r1)
        return sorted(out)

    def muster(self, c):
        return self.pd.race[self.race_c(c)]["muster"] if FLAGS["muster"] else 1000

    def force(self, c):
        lv = self.ct["levy_permille"]
        levy = sum(s["population"] * lv[KIND_KEY[s["kind"]]] // 1000 for s in self.svis(c))
        f = levy * self.pd.race[self.race_c(c)]["prowess"] // 1000
        if FLAGS["muster"]:
            f = f * self.muster(c) // 1000         # r9 20.6 item 1
        if f > (1 << 40):
            fatal("history: bound: conflict force of civilization %d exceeds 2^40" % c)
        return f

    def local_force(self, c, r):
        """r9 20.6 item 2: the levy of c's standing visible settlements within levy_hops of R, times prowess
        and muster (not yet divided by the open wars)."""
        lv = self.ct["levy_permille"]
        hops = self.geo.hops(r)
        lim = self.ct["levy_hops"]
        levy = sum(s["population"] * lv[KIND_KEY[s["kind"]]] // 1000 for s in self.svis(c)
                   if s["region"] in hops and hops[s["region"]] <= lim)
        return levy * self.pd.race[self.race_c(c)]["prowess"] // 1000 * self.muster(c) // 1000

    def wars_of(self, c):
        return sum(1 for w in self.wars if w["open"] and c in (w["a"], w["b"]))

    def at_war(self, a, b):
        return any(w["open"] and {w["a"], w["b"]} == {a, b} for w in self.wars)

    def truce_length(self, a):
        """r9 20.6 item 4: a's truce after a peace, shorter for a people more warlike than
        truce_break_aggression (the raw aggression, not g')."""
        t = self.ct
        if not FLAGS["truce"]:
            return t["truce_years"]
        g = self.pd.race[self.race_c(a)]["aggression"]
        return t["truce_years"] * min(1000, (1000 - g) * 1000 // (1000 - t["truce_break_aggression"])) // 1000

    def past_truce(self, a, b, y=None):
        """Whether a is past its truce with b (directional from 0.18.0)."""
        y = self.year if y is None else y
        lp = self.last_peace.get((min(a, b), max(a, b)))
        return lp is None or y >= lp[0] + self.truce_length(a)

    # --- r9 20.5 revenge, 20.4 weariness, 20.2 goals
    def wrong(self, a, b, r):
        """20.5: b took r from a (a sack, a capture, a cession)."""
        if FLAGS["revenge"]:
            self.wronged.setdefault((a, b), set()).add(r)

    def aggression(self, a, b):
        """g'(a, b) of 20.5 (the race's aggression without revenge)."""
        g = self.pd.race[self.race_c(a)]["aggression"]
        if FLAGS["revenge"] and self.wronged.get((a, b)) and self.grudge.get((min(a, b), max(a, b)), 0) > 0:
            return max(g, self.ct["revenge_aggression"])
        return g

    def weary(self, b, r):
        """20.4: b is weary of r (count read >= weary_wars; a count of another owner reads 0)."""
        ww = self.ct["weary_wars"] if FLAGS["weary"] else 0
        if ww == 0:
            return False
        k = self.futile.get((b, r))
        n = k[0] if k is not None and k[1] == self.owner[r][0] else 0
        if FLAGS["weary_open"]:
            # 20.23: the wars now open against b with goal r count, by any attacker
            n += sum(1 for w in self.wars if w["open"] and w["b"] == b and w["goal"] == r)
        return n >= ww

    def count_futile(self, war):
        """20.4: a war ended by peace or truce whose goal kept the owner it had at the declaration."""
        if not FLAGS["weary"]:
            return
        b, g = war["b"], war["goal"]
        o = self.owner[g][0]
        if o != war["owner0"]:
            return
        k = self.futile.get((b, g))
        n = k[0] if k is not None and k[1] == o else 0
        self.futile[(b, g)] = (n + 1, o)

    def capital_region(self, c):
        return self.stl(self.civ(c)["capital"])["region"]

    def allowed(self, a, b, fr):
        """20.2: the entries of front(a, b) without b's weary regions and without the region of b's capital
        unless that leaves none."""
        ents = [e for e in fr if not self.weary(b, e[0])]
        if FLAGS["goals"]:
            cap = self.capital_region(b)
            other = [e for e in ents if e[0] != cap]
            if other:
                ents = other
        return ents

    def last_refuge_of(self, y):
        """20.1 / 7.3 item 7: y has one standing settlement and is the only alive civilization of its race."""
        race = self.race_c(y)
        alive_same = [c["id"] for c in self.civs if c["ended_year"] is None and c["race"] == race]
        return alive_same == [y] and len(self.standing_of(y)) == 1

    def cover(self, r):
        q = self.geo.q[r]
        v = q["rough"]
        for key in COVER_BIOMES:
            code = self.biome_code.get(key)
            if code is not None:
                v += q["share"].get(code, 0)
        return min(1000, v)

    def large_river(self, b):
        p = self.t
        return b["length_m"] > 0 and b["river_pairs"] * 16 * 1000 >= p["river_border_share_permille"] * b["length_m"]

    def defence(self, r, r_from, y):
        """6.3: (def, pass id or 0)."""
        t = self.ct
        d = 1000 + t["terrain_defense"] * self.cover(r) // 1000
        pass_id = 0
        if r_from != r:
            b = self.border[(min(r, r_from), max(r, r_from))]
            if b["walk_pairs"] == 0 and b["pass_id"] > 0:
                d += t["pass_defense"]
                pass_id = b["pass_id"]
            if self.large_river(b):
                d += t["river_defense"]
        if any(s["region"] == r and s["kind"] == FORTRESS for s in self.svis(y)):
            d += t["fort_defense"]
        if FLAGS["home"]:      # r9 20.7 item 2: the defender's own preferred land
            ry = self.race_c(y)
            d += self.pd.race[ry]["home"] * max(0, self.bterm(r, ry)) // 1000
        return d, pass_id

    def target_key(self, x, e):
        r, cost, _f = e
        return (-(self.geo.sigma(r, self.race_c(x)) - self.ct["cost_penalty"] * cost), r)

    def add_grudge(self, a, b, v):
        k = (min(a, b), max(a, b))
        self.grudge[k] = self.grudge.get(k, 0) + v

    def drop_claims(self, a, b):
        self.claim.pop((a, b), None)
        self.claim.pop((b, a), None)

    def zero_strain(self, a, b):
        self.strain.pop((a, b), None)
        self.strain.pop((b, a), None)

    # --- 8.1 housekeeping
    def housekeeping(self):
        ended = lambda c: self.civ(c)["ended_year"] is not None  # noqa: E731
        for k in sorted(list(self.relations), key=key_order):
            a, b = k[1], k[3]
            ea, eb = ended(a), ended(b)
            if ea or eb:
                side = min(a, b) if (ea and eb) else (a if ea else b)
                self.close_rel(k, self.civ(side)["ending_event"])
        for w in self.wars:
            if w["open"] and (ended(w["a"]) or ended(w["b"])):
                self.close_war(w, "ended", None)
        for k in sorted(self.claim):
            if ended(k[0]) or ended(k[1]):
                del self.claim[k]
        al = self.avc()
        for c in al:
            if c in self.seen:
                continue
            self.seen.add(c)
            fe = self.events[self.civ(c)["founding_event"] - 1]
            if fe["kind"] == K["civ_seceded"]:
                p = [x for (_i, seq, _t, x) in fe["participants"] if seq == 2][0]
                self.add_grudge(p, c, self.ct["secession_grudge"])
        for i, a in enumerate(al):
            for b in al[i + 1:]:
                if (a, b) in self.enmity_opened or self.camp(a, b) > self.ct["enmity_max"]:
                    continue
                self.open_rel(sym_key(a, b, "enmity"), self.civ(b)["founding_event"])
                self.enmity_opened.add((a, b))

    def decay(self, years):
        dv = years * self.ct["grudge_decay_per_century"] // 100
        for k in sorted(self.grudge):
            self.grudge[k] = max(0, self.grudge[k] - dv)

    # --- 5.2, 5.3
    def strain_update(self, years, permille, quiet):
        t = self.ct
        al = self.avc()
        wanted = {c: set(self.wanted_held(c)) for c in al}           # 5.3 snapshot (8.4 item 7)
        held = {c: set(self.held(c)) for c in al}
        hunger = {c: self.hunger.get(c, 0) for c in al}
        self.contact = {}
        fronts = {}
        for a in al:
            for b in al:
                if a == b:
                    continue
                fr = self.front(a, b)
                fronts[(a, b)] = fr
                if fr:
                    self.contact[(a, b)] = min(e[1] for e in fr)
        I = self.intensity
        for a in al:
            for b in al:
                if a == b or (a, b) not in self.contact:
                    continue
                if self.at_war(a, b) or not self.past_truce(a, b):
                    continue
                g = self.aggression(a, b)     # r9 20.5: g' (g without revenge)
                k = self.contact[(a, b)]
                want = 0
                if wanted[a] & held[b]:
                    want = t["want_weight"] * min(hunger[a], t["hunger_full"]) // t["hunger_full"]
                drive = max(0, t["border_friction"] - self.disp(a, b) + want + self.fear(b))
                rate = g * g // 1000 * drive // 1000 * (1000 // k) // 1000
                v = self.strain.get((a, b), 0) + rate * years * permille * I // 1000
                v = min(v, t["strain_max"] * 1000)
                if v > (1 << 40):
                    fatal("history: bound: conflict strain of civilization %d against %d exceeds 2^40" % (a, b))
                if v:
                    self.strain[(a, b)] = v
        # 5.3 claims
        for a in al:
            for b in al:
                if a == b or (a, b) not in self.contact:
                    continue
                if (a, b) in self.claim or self.at_war(a, b) or not self.past_truce(a, b):
                    continue
                if self.strain.get((a, b), 0) < t["claim_strain"] * 1000:
                    continue
                if quiet and self.quiet_events_left(a) <= 0:
                    continue
                ra = self.race_c(a)
                fr = fronts[(a, b)]
                if FLAGS["goals"] or FLAGS["weary"]:
                    fr = self.allowed(a, b, fr)      # r9 20.2 item 1, 20.4
                    if not fr:
                        continue
                wr = self.wronged.get((a, b), set())
                r, cost, _f = min(fr, key=lambda e: (0 if (e[0] in wr and self.owner[e[0]][0] == b) else 1,
                                                    0 if (e[0] in wanted[a] and self.owner[e[0]][0] == b) else 1,
                                                    -(self.geo.sigma(e[0], ra) - t["cost_penalty"] * e[1]), e[0]))
                self.weary_checks.append((self.year, "claim", b, r))
                if self.owner[r][0] == b:
                    cause = self.owner[r][1]
                else:
                    ss = [s for s in self.svis_seen(b) if s["region"] == r]
                    cause = min(ss, key=lambda s: (-s["population"], s["id"]))["founding_event"]
                e = self.append("claim", region=r, participants=(("claimant", CIV, a), ("holder", CIV, b)),
                                causes=(cause,))
                self.claim[(a, b)] = (e, r)

    def best_claim(self, y):
        """5.4 over the stored strain, claims and contact: (value, trigger, pair)."""
        best = (0, 0, None)
        for (a, b) in sorted(self.claim):
            if not (self.alive(a) and self.alive(b)) or (a, b) not in self.contact:
                continue
            if self.at_war(a, b) or not self.past_truce(a, b, y):
                continue
            if FLAGS["odds"] and self.odds(a, b) < self.ct["odds_min"]:
                continue                     # r9 20.6 item 3: a quarrel a people cannot pursue does not flare
            v = self.strain.get((a, b), 0) // 1000
            if v > best[0]:
                best = (v, self.claim[(a, b)][0], (a, b))
        return best

    def tension(self, start, length):
        """The TensionSource (5.4), asked in Q5; read-only."""
        before = (dict(self.strain), dict(self.claim), dict(self.contact), len(self.events))
        v, e, pair = self.best_claim(start)
        assert before == (dict(self.strain), dict(self.claim), dict(self.contact), len(self.events))
        self.tension_log.append((start, v, e, pair))
        return v, e

    # --- 7.2
    def declare(self, a, b, draw_free):
        t = self.ct
        ce, g0 = self.claim[(a, b)]
        goal = g0
        if FLAGS["goals"] or FLAGS["weary"]:
            # r9 20.2 item 2: the claim region if still an allowed goal that b holds or has a standing visible
            # settlement in; else the allowed goal with the smallest target key; none: the claim is dropped
            al = self.allowed(a, b, self.front(a, b))
            if g0 not in [e[0] for e in al] or (self.owner[g0][0] != b and
                                                 not any(s["region"] == g0 for s in self.svis(b))):
                if not al:
                    del self.claim[(a, b)]
                    return None
                goal = min(al, key=lambda e: self.target_key(a, e))[0]
        elif self.owner[g0][0] != b and not any(s["region"] == g0 for s in self.svis(b)):
            goal = min(self.front(a, b), key=lambda e: self.target_key(a, e))[0]
        self.weary_checks.append((self.year, "war_declared", b, goal))
        causes = {ce}
        lp = self.last_peace.get((min(a, b), max(a, b)))
        if lp is not None:
            causes.add(lp[1])
        e = self.append("war_declared", region=goal, participants=(("attacker", CIV, a), ("defender", CIV, b)),
                        causes=tuple(sorted(causes)))
        pk = sym_key(a, b, "peace")
        if pk in self.relations:
            self.close_rel(pk, e)
        self.open_rel(rel_key(a, b, "war"), e)
        self.zero_strain(a, b)
        self.drop_claims(a, b)
        self.wars.append(dict(w=e, a=a, b=b, goal=goal, start=self.year, last=e, init=a, claim=ce,
                              won={a: 0, b: 0}, taken={a: 0, b: 0}, dead={a: 0, b: 0},
                              pop0={a: self.population(a), b: self.population(b)}, open=True, end=None,
                              battles=0, sacked=0, captured=0, flare=draw_free, ceded=False,
                              owner0=self.owner[goal][0], cost=None, fled=0))
        return e

    def c_t2(self):
        t = self.ct
        self.housekeeping()
        self.decay(1)
        self.strain_update(1, t["tick_strain_permille"], False)
        first_flare = self.phase != self.last_t2_phase and self.phase_kind == FLARE
        flare_pair = self.best_claim(self.year)[2] if first_flare else None
        if first_flare:
            self.flare_values[self.year] = self.best_claim(self.year)
        for a in self.avc():
            if any(w["open"] and w["a"] == a for w in self.wars):
                continue
            cands = []
            for b in self.avc():
                if b == a or (a, b) not in self.claim or (a, b) not in self.contact:
                    continue
                if self.at_war(a, b) or not self.past_truce(a, b):
                    continue
                if self.year < self.civ(b)["founded_year"] + t["shelter_years"]:
                    self.shelter_blocks += 1
                    continue
                cands.append(b)
            if flare_pair is not None and flare_pair[0] == a and flare_pair[1] in cands:
                self.declare(a, flare_pair[1], True)
                continue
            if not cands:
                continue
            b = min(cands, key=lambda b: (-self.strain.get((a, b), 0), b))
            p = self.declare_p(a, b)
            if p is None:
                continue
            if chance_pm(self.draw("hist.conflict.declare", a, b, 0), p):
                self.declare(a, b, False)
        self.last_t2_phase = self.phase

    def declare_p(self, a, b):
        """7.2 item 3: the yearly chance of a's declaration on b, or None below declare_min (no draw)."""
        t = self.ct
        s = self.strain.get((a, b), 0) // 1000
        if s < t["declare_min"]:
            return None
        p = t["declare_permille"] * min(1000, (s - t["declare_min"]) * 1000 // t["declare_span"]) // 1000
        odds = self.odds(a, b)
        if FLAGS["odds"] and odds < t["odds_min"]:
            return None                       # r9 20.6 item 3: no declaration and no draw
        return p * odds // 1000

    def odds(self, a, b):
        """7.2 item 3's odds; r9 20.6 item 3: the target's force weighted by the defence of the claimed region
        (1000 if it is not in front(a, b)), the aggression term floored at odds_floor; g' of 20.5."""
        t = self.ct
        g = self.aggression(a, b)
        fb = self.force(b)
        if FLAGS["odds"]:
            g0 = self.claim[(a, b)][1]
            ent = [e for e in self.front(a, b) if e[0] == g0]
            dv = self.defence(g0, ent[0][2], b)[0] if ent else 1000
            fb = fb * dv // 1000
            m = max(1000 - g, t["odds_floor"])
        else:
            m = 1000 - g
        return min(1000, self.force(a) * 1000 // max(1, fb * t["declare_ratio"] // 1000 * m // 1000))

    def battle_p(self, A, D, dv):
        D2 = D * dv // 1000
        return A * 1000 // (A + D2) if A + D2 > 0 else 500

    def garrison(self, ts, y):
        t = self.ct
        walls = t["walls_permille"][KIND_KEY[ts["kind"]]] + (t["capital_walls"] if self.civ(y)["capital"] == ts["id"]
                                                             else 0)
        g = ts["population"] * t["garrison_permille"] // 1000 * self.pd.race[self.race_c(y)]["prowess"] // 1000 \
            * walls // 1000
        if FLAGS["home"]:      # r9 20.7 item 2, after the walls, left to right
            ry = self.race_of(ts)
            g = g * (1000 + self.pd.race[ry]["home"] * max(0, self.bterm(ts["region"], ry)) // 1000) // 1000
        return g

    def siege_p(self, A2, G):
        return A2 * 1000 // (A2 + G) if A2 + G > 0 else 0

    # --- 7.4
    def casualties(self, c, dead, e, war):
        ss = self.svis(c)
        tot = sum(s["population"] for s in ss)
        if FLAGS["floor"] and self.last_refuge_of(c):
            # r10 20.20 item 1: a race's last refuge is never taken below abandon_min
            dead = min(dead, max(0, tot - self.t["abandon_min"]))
        if tot == 0 or dead == 0:
            return 0
        dead = min(dead, tot)
        took = 0
        for s in ss:
            loss = dead * s["population"] // tot
            s["population"] -= loss
            took += loss
        rem = dead - took
        if rem:
            m = min(ss, key=lambda s: (-s["population"], s["id"]))
            m["population"] -= min(rem, m["population"])
        for s in ss:
            self.last_red[s["id"]] = e        # note_reduction
        if war is not None:
            war["dead"][c] += dead
        return dead

    def refugees(self, c, s, n):
        """P3-05 8.5 item 2: n people of c leave s for c's nearest other standing settlement, room permitting."""
        if n <= 0:
            return
        race = self.civ(c)["race"]
        others = [t for t in self.standing_of(c) if t["id"] != s["id"]]
        if not others:
            return
        t = min(others, key=lambda t: ((t["x"] - s["x"]) ** 2 + (t["y"] - s["y"]) ** 2, t["id"]))
        room = max(0, 2 * self.geo.capacity(t["region"], race) - t["population"])
        t["population"] += min(n, room)

    def move_capital(self, c, e):
        others = self.standing_of(c)
        nc = min(others, key=lambda t: (-t["population"], t["id"]))
        self.append("capital_moved", region=nc["region"], settlement=nc["id"],
                    participants=(("civilization", CIV, c), ("settlement", SETTLEMENT, nc["id"])), causes=(e,))
        self.civ(c)["capital"] = nc["id"]

    # --- 7.5, 7.6
    def fall(self, war, t_s, x, y, r, siege_e):
        t = self.ct
        war.setdefault("taken_pop", {war["a"]: 0, war["b"]: 0})[x] += t_s["population"]   # r10 20.15: P before toll
        sack_p = min(1000, t["sack_base"] + self.hostility(x, y) * t["sack_hostility"] // 1000)
        capital = self.civ(y)["capital"] == t_s["id"]
        if chance_pm(self.draw("hist.conflict.sack", war["w"], t_s["id"], 0), sack_p):
            dead = t_s["population"] * t["sack_toll_permille"] // 1000
            t_s["population"] -= dead
            e = self.append("sacked", region=r, settlement=t_s["id"],
                            participants=(("attacker", CIV, x), ("defender", CIV, y)),
                            values=(dead, t_s["population"]), causes=(siege_e,),
                            salience=(t["capital_sacked_salience"] if capital else None))
            if not self.standing(t_s) or t_s["owner_civ"] == 0:
                fatal("history: conflict: ruin of settlement %d, which is not standing" % t_s["id"])
            self.ruin(t_s["id"], e)
            self.version += 1
            war["sacked"] += 1
            self.wrong(y, x, r)            # r9 20.5
        else:
            dead = t_s["population"] * t["capture_toll_permille"] // 1000
            t_s["population"] -= dead
            excess = max(0, t_s["population"] - 2 * self.geo.capacity(r, self.race_c(x)))
            e = self.append("settlement_captured", region=r, settlement=t_s["id"],
                            participants=(("attacker", CIV, x), ("defender", CIV, y)), values=(dead, excess),
                            causes=(siege_e,), salience=(t["capital_captured_salience"] if capital else None))
            t_s["population"] -= excess
            self.refugees(y, t_s, excess)
            t_s["owner_civ"] = x
            self.version += 1
            if self.owner[r][0] == y and not any(s["region"] == r for s in self.standing_of(y)):
                self.set_owner(r, x, e)
            if capital and self.standing_of(y):
                self.move_capital(y, e)
            war["captured"] += 1
            self.wrong(y, x, r)            # r9 20.5
        war["taken"][x] += 1
        self.add_grudge(x, y, t["capture_grudge"])
        war["last"] = e
        if not self.standing_of(y):
            self.conquer(y, x, e, r)

    def conquer(self, y, x, e, r):
        e2 = self.append("civ_conquered", region=r, participants=(("civilization", CIV, y), ("conqueror", CIV, x)),
                         causes=(e,))
        self.end_civ(y, e2)
        self.version += 1
        for k in sorted([k for k in self.relations if y in (k[1], k[3])], key=key_order):
            self.close_rel(k, e2)
        for w in self.wars:
            if w["open"] and y in (w["a"], w["b"]):
                self.close_war(w, "conquest", w["a"] if w["b"] == y else w["b"])
        for k in sorted(self.claim):
            if y in k:
                del self.claim[k]

    def close_war(self, w, how, winner):
        w["open"] = False
        w["end"] = (self.year, how, winner)

    # --- 7.3, 7.7
    def c_t6(self):
        t = self.ct
        self.housekeeping()
        for war in [w for w in self.wars if w["open"]]:
            if not war["open"] or war["start"] == self.year:
                continue
            self.fight(war)
            if war["open"]:
                self.end_check(war)

    def fight(self, war):
        t = self.ct
        x = war["init"]
        y = war["b"] if x == war["a"] else war["a"]
        fr = self.front(x, y)
        if not fr:
            x, y = y, x
            fr = self.front(x, y)
        war["no_front"] = not fr
        if not fr:
            return
        ent = None
        if x == war["a"]:
            for e in fr:
                if e[0] == war["goal"]:
                    ent = e
        if ent is None:
            ent = min(fr, key=lambda e: self.target_key(x, e))
        r, _cost, r_from = ent
        w = war["w"]
        if FLAGS["search"] and self.conceal_search(war, x, y, r):
            return
        if not chance_pm(self.draw("hist.conflict.battle", w, 0, 0), t["battle_permille"]):
            return
        if FLAGS["levies"]:     # r9 20.6 item 2: each side's local force at R
            A = self.local_force(x, r) // self.wars_of(x)
            D = self.local_force(y, r) // self.wars_of(y)
        else:
            A = self.force(x) // self.wars_of(x)
            D = self.force(y) // self.wars_of(y)
        dv, pass_id = self.defence(r, r_from, y)
        p = self.battle_p(A, D, dv)
        xwin = chance_pm(self.draw("hist.conflict.battle_win", w, 0, 0), p)
        W, L, FW, FL = (x, y, A, D) if xwin else (y, x, D, A)
        dead_l = FL * t["loss_loser_permille"] // 1000
        if FLAGS["kill"]:      # r10 20.16: at most kill_max of the victors' (local) force
            dead_l = min(dead_l, FW * t["kill_max_permille"] // 1000)
            self.kills.append((FW, dead_l))
        # r7 7.3 item 4: the victors lose loss_winner of the smaller force (before r7, of their own force)
        dead_w = (min(FW, FL) if FLAGS["loss"] else FW) * t["loss_winner_permille"] // 1000
        if pass_id:
            e = self.append("battle_at_pass", region=r, participants=(("winner", CIV, W), ("loser", CIV, L)),
                            values=(dead_w, dead_l, pass_id), causes=(w,))
        else:
            e = self.append("battle", region=r, participants=(("winner", CIV, W), ("loser", CIV, L)),
                            values=(dead_w, dead_l), causes=(w,))
        self.casualties(W, dead_w, e, war)
        self.casualties(L, dead_l, e, war)
        war["won"][W] += 1
        war["battles"] += 1
        war["last"] = e
        war["init"] = W
        war["last_battle_winner"] = W     # r10 20.15 item 3
        if not xwin:
            return
        ss = [s for s in self.svis_seen(y) if s["region"] == r]   # r9 20.1: a hidden settlement is no target
        if not ss:
            return
        ts = min(ss, key=lambda s: (-s["population"], s["id"]))
        G = self.garrison(ts, y)
        A2 = max(0, A - dead_w)
        ps = self.siege_p(A2, G)
        fell = chance_pm(self.draw("hist.conflict.siege", w, ts["id"], 0), ps)
        flight = None
        if t["last_refuge"] == 1 and fell:
            race = self.race_c(y)
            alive_same = [c["id"] for c in self.civs if c["ended_year"] is None and c["race"] == race]
            if alive_same == [y] and [s["id"] for s in self.standing_of(y)] == [ts["id"]]:
                if FLAGS["flight"] and t["refuge_flight"] == 1:
                    # r9 20.1: flight when a destination exists and the survivors of the sack are enough
                    dest = self.flight_destination(x, y, ts)
                    if dest is None:
                        self.guard_no_dest += 1
                    else:
                        left = ts["population"] - ts["population"] * t["sack_toll_permille"] // 1000
                        n = min(left, 2 * self.geo.capacity(dest[0], race))
                        if n >= self.t["abandon_min"]:
                            flight = dest
                        else:
                            self.guard_too_few += 1
                if flight is None:
                    fell = False
                    self.refuge_saves += 1
                    if FLAGS["held"] and FLAGS["flight"] and t["refuge_flight"] == 1:
                        # r10 20.22: the guard holds and says why (1 no destination, 2 too few survivors)
                        reason = 1 if dest is None else 2
                        eh = self.append("refuge_held", region=r, settlement=ts["id"],
                                         participants=(("attacker", CIV, x), ("defender", CIV, y)), values=(reason,),
                                         causes=(e,))
                        war["last"] = eh
                        self.held_log.append((eh, ts["id"], reason))
                        return
        es = self.append("siege", region=r, settlement=ts["id"], participants=(("attacker", CIV, x),
                                                                               ("defender", CIV, y)),
                         values=(1 if fell else 0,), causes=(e,))
        war["last"] = es
        if flight is not None:
            self.flee(war, ts, x, y, r, es, flight)
        elif fell:
            self.fall(war, ts, x, y, r, es)

    # --- r10 20.17: the search for a concealed people
    def conceal_search(self, war, x, y, r):
        """Before the battle draw: when every standing visible settlement of y in R is concealed, x searches.
        Returns True when the year's fighting stops here (not found, or the war given up)."""
        ys = [s for s in self.svis(y) if s["region"] == r]
        if not ys or not all(self.concealed(s) for s in ys):
            return False
        found = chance_pm(self.draw("hist.conflict.search", war["w"], r, 0), self.ct["find_permille"])
        self.search_log.append((self.year, war["w"], r, y, found))
        if found:
            war["unfound"] = 0
            return False
        war["unfound"] = war.get("unfound", 0) + 1
        if war["unfound"] >= self.ct["search_years"]:
            self.give_up(war, x, y, r)
        return True

    def give_up(self, war, x, y, r):
        """20.17 item 2: search_abandoned, then the truce bookkeeping of 7.7, with no cost."""
        t = self.ct
        a, b = war["a"], war["b"]
        e = self.append("search_abandoned", region=r, participants=(("searcher", CIV, x), ("hidden", CIV, y)),
                        values=(self.year - war["start"],), causes=(war["last"],))
        self.count_futile(war)
        self.close_rel(rel_key(a, b, "war"), e)
        self.open_rel(sym_key(a, b, "peace"), e)
        self.last_peace[(min(a, b), max(a, b))] = (self.year, e)
        self.add_grudge(a, b, t["war_grudge"])
        self.zero_strain(a, b)
        self.drop_claims(a, b)
        self.close_war(war, "unfound", None)

    # --- r9 20.1: the flight of a last refuge
    def flight_destination(self, x, y, ts):
        """(D, (x, y, how)) or None: the free non-islet regions within flight_hops of R_t with capacity, ordered
        by (-h_x(D), -sigma(D, r_y), D), the first with a site of P3-05 6.2 (not relaxed)."""
        t = self.ct
        race = self.race_c(y)
        rt = ts["region"]
        hops_t = self.geo.hops(rt)
        cap = t["march_hops"] + 1
        px = self.presence(x)
        cands = []
        for d in self.geo.ids:
            if self.owner[d][0] != 0 or self.geo.islet(d) or d == rt:
                continue
            if d not in hops_t or hops_t[d] > t["flight_hops"]:
                continue
            if self.geo.capacity(d, race) < self.t["expand_min_capacity"]:
                continue
            h = cap
            for p_ in px:
                hp = self.geo.hops(p_)
                if d in hp:
                    h = min(h, hp[d])
            cands.append((-min(h, cap), -self.geo.sigma(d, race), d))
        for _h, _s, d in sorted(cands):
            st = self.site(d, race, relaxed=False)
            if st is not None:
                return d, st
        return None

    def flee(self, war, ts, x, y, r, siege_e, dest):
        """20.1's flight procedure (T6, right after the siege event)."""
        t = self.ct
        d, (sx, sy, how) = dest
        race = self.race_c(y)
        war.setdefault("taken_pop", {war["a"]: 0, war["b"]: 0})[x] += ts["population"]   # r10 20.15
        # 1 the sack (no sack draw: a refuge that falls is always sacked)
        dead = ts["population"] * t["sack_toll_permille"] // 1000
        ts["population"] -= dead
        e = self.append("sacked", region=r, settlement=ts["id"], participants=(("attacker", CIV, x), ("defender", CIV, y)),
                        values=(dead, ts["population"]), causes=(siege_e,), salience=t["capital_sacked_salience"])
        # 2 the refuge
        n = min(ts["population"], 2 * self.geo.capacity(d, race))
        h = len(self.settlements) + 1
        civ = self.civ(y)
        cnum = civ["culture"]
        cul = self.culture[cnum][1]
        name, _t = pnr.make_name(self.dat.naming[cul], cul, "settlement", self.inp.seed, cnum, h, self.registry)
        e2 = self.append("refugees_settled", region=d, settlement=h,
                         participants=(("settlement", SETTLEMENT, h), ("founder", CIV, y), ("origin", SETTLEMENT, ts["id"])),
                         values=(n,), causes=(e,))
        got = self.found_settlement(sx, sy, d, y, self.founding_kind(race, d, n, sx, sy), n, name, e2)
        assert got == h
        self.site_how[h] = how
        self.set_owner(d, y, e2)
        self.hidden_until[h] = self.year + t["hidden_years"]
        # 3 the remainder, the ruin (destination h; the capital moves to h)
        ts["population"] -= n
        if not self.standing(ts) or ts["owner_civ"] == 0:
            fatal("history: conflict: ruin of settlement %d, which is not standing" % ts["id"])
        self.ruin(ts["id"], e)
        self.version += 1
        # 4 the war
        war["sacked"] += 1
        war["fled"] += 1
        war["taken"][x] += 1
        self.add_grudge(x, y, t["capture_grudge"])
        self.wrong(y, x, r)
        war["last"] = e2
        self.flights += 1
        if not self.standing_of(y):
            fatal("history: conflict: a flight ended civilization %d" % y)

    def score(self, war):
        a, b = war["a"], war["b"]
        if FLAGS["score"]:   # r10 20.15 item 1: dead inflicted and people taken
            tp = war.get("taken_pop", {a: 0, b: 0})
            return (war["dead"][b] - war["dead"][a]) + (tp[a] - tp[b])
        return (war["won"][a] - war["won"][b]) + 2 * (war["taken"][a] - war["taken"][b])

    def decided(self, war, winner):
        """r10 20.15 item 3: an ending decided by the score whose loser won the war's last battle is a truce."""
        if FLAGS["score"] and winner is not None:
            L = war["b"] if winner == war["a"] else war["a"]
            if war.get("last_battle_winner") == L:
                return None
        return winner

    def by_score(self, war):
        s = self.score(war)
        return war["a"] if s > 0 else (war["b"] if s < 0 else None)

    def end_check(self, war):
        t = self.ct
        a, b = war["a"], war["b"]
        # r9 20.6 item 1: pop0 x muster div 1000 for each side
        ex = [c for c in (a, b) if war["dead"][c] * 1000 >= t["exhaustion_permille"] *
              max(1, war["pop0"][c] * self.muster(c) // 1000)]
        if ex:
            s = self.score(war)
            fav = a if s > 0 else (b if s < 0 else None)
            if len(ex) == 2 or s == 0 or ex[0] != fav:
                # the loser (if any) is exhausted: it sues for peace (r10 20.15 item 4)
                self.end_war(war, self.decided(war, self.by_score(war)), "exhaustion", "peace")
            elif FLAGS["score"]:
                # r10 20.15 item 2: the exhausted side that leads wins; the other has not yielded
                self.end_war(war, self.decided(war, ex[0]), "exhaustion", "war_won")
            else:
                self.end_war(war, None, "exhaustion")
            return
        g = war["goal"]
        if self.owner[g][0] != b and not any(s["region"] == g for s in self.svis(b)):
            self.end_war(war, a, "aim", "war_won")     # by aim: not decided by the score (item 3)
            return
        if war.get("no_front"):
            self.end_war(war, self.decided(war, self.by_score(war)), "no-front", "war_won")
            return
        if self.year - war["start"] >= t["war_max_years"]:
            self.end_war(war, self.decided(war, self.by_score(war)), "length", "war_won")

    def cession_due(self, war, winner):
        if winner != war["a"]:
            return False
        L = war["b"]
        g = war["goal"]
        return self.owner[g][0] == L and self.stl(self.civ(L)["capital"])["region"] != g

    def end_war(self, war, winner, how, kind="peace"):
        t = self.ct
        a, b = war["a"], war["b"]
        years = self.year - war["start"]
        if not FLAGS["score"]:
            kind = "peace"
        war["end_kind"] = kind if winner is not None else "truce"
        if winner is not None:
            L = b if winner == a else a
            e = self.append(kind, participants=(("winner", CIV, winner), ("loser", CIV, L)), values=(years,),
                            causes=(war["last"],))
        else:
            e = self.append("truce", participants=(("parties", CIV, min(a, b)), ("parties", CIV, max(a, b))),
                            values=(years,), causes=(war["last"],))
        self.count_futile(war)           # r9 20.4, at the end by peace or truce
        self.close_rel(rel_key(a, b, "war"), e)
        self.open_rel(sym_key(a, b, "peace"), e)
        self.last_peace[(min(a, b), max(a, b))] = (self.year, e)
        self.add_grudge(a, b, t["war_grudge"])
        self.zero_strain(a, b)
        self.drop_claims(a, b)
        self.close_war(war, how, winner)
        if winner is None:
            return
        plan = self.cost_plan(war, winner)
        if plan is None:
            return
        L = b if winner == a else a
        if plan[0] == "tribute":
            n = plan[1]
            et = self.append("tribute", participants=(("payer", CIV, L), ("payee", CIV, winner)), values=(n,),
                             causes=(e,))
            self.casualties(L, n, et, None)
            war["cost"] = "tribute"
            return
        g = plan[1]
        ec = self.append("region_ceded", region=g, participants=(("from", CIV, L), ("to", CIV, winner)),
                         causes=(e,))
        ids = [s["id"] for s in self.svis(L) if s["region"] == g]
        race_w = self.race_c(winner)
        for sid in ids:
            s = self.stl(sid)
            excess = max(0, s["population"] - 2 * self.geo.capacity(g, race_w))
            if excess:
                s["population"] -= excess
                self.refugees(L, s, excess)
            s["owner_civ"] = winner
            self.version += 1
        self.set_owner(g, winner, ec)
        self.wrong(L, winner, g)        # r9 20.5
        war["ceded"] = True
        war["cost"] = plan[0]

    def cost_plan(self, war, winner):
        """What the peace of a war won by `winner` costs the loser: ("goal", G) for 7.7's cession,
        r9 20.3's ("cede", G') or ("tribute", n), or None."""
        if self.cession_due(war, winner):
            return ("goal", war["goal"])
        if not FLAGS["cost"]:
            return None
        W = winner
        L = war["b"] if W == war["a"] else war["a"]
        cap = self.capital_region(L)
        ents = [e for e in self.front(W, L) if self.owner[e[0]][0] == L and e[0] != cap]
        if ents:
            return ("cede", min(ents, key=lambda e: self.target_key(W, e))[0])
        if self.last_refuge_of(L):
            war["no_cost"] = "refuge"
            return None                       # no tribute from a last refuge (P3-09 10)
        n = sum(s["population"] for s in self.svis_seen(L)) * self.ct["tribute_permille"] // 1000
        if n <= 0:
            war["no_cost"] = "empty"
        return ("tribute", n) if n > 0 else None

    # --- 7.8 Q4
    def c_q4(self):
        t = self.ct
        self.housekeeping()
        for war in [w for w in self.wars if w["open"]]:
            if not war["open"]:
                continue
            winner = self.decided(war, self.by_score(war))
            if winner is None:
                if self.quiet_events_left(min(war["a"], war["b"])) <= 0:
                    continue
            else:
                if self.quiet_events_left(winner) <= 0:
                    continue
                # 7.7 and r9 20.3: an end that cedes or takes tribute waits for the loser's budget too
                L = war["b"] if winner == war["a"] else war["a"]
                if self.cost_plan(war, winner) is not None and self.quiet_events_left(L) <= 0:
                    continue
            self.end_war(war, winner, "quiet", "war_won")
        self.decay(self.years)
        self.strain_update(self.years, t["quiet_strain_permille"], True)

    # in-run invariants (P3-05's, and 14 item 6 capacity after every step)
    def check_state(self):
        super().check_state()


# Cases where the 0.18.0 rules as the reference reads them let a race die out with last_refuge 1 (the stress
# tuning only): the last refuge holds its sieges but its battle losses bring it below abandon_min, and P3-05 8.5
# (b) abandons it. A gap of P3-06 r9 20.13 item 2 (20.1 and P3-09 10 guard flights and tributes, not battles),
# reported to the lead; checked as a finding, not a failure.
KNOWN_EXTINCT_018 = ()   # r10 20.20 closed it (stress-1, -12, -14 were the cases at r2)


def history_stage(dat, inp, extra_draw=False, conflict=True):
    """3.3 history_stage with peoples and conflict installed (0.16.0); conflict=False is the
    peoples-only configuration (p3_peoples_reference.history_stage at 0.16.0)."""
    msg = pp.entry_checks(dat, inp)
    if msg is not None:
        fatal(msg)
    msg = pe.chronology_problem(inp)
    if msg is not None:
        fatal(msg)
    if conflict:
        msg = tuning_problem(dat.conflict)
        if msg is not None:
            fatal(msg)
        for r in inp.races:
            if r[9] == 1 and r[0] not in dat.race:
                fatal("history: conflict: race '%s' is not in the content" % r[1])
    cat = pp.event_catalogue(dat.report)
    w = World(dat, inp, cat, extra_draw, conflict)
    starts = [a[2] for a in inp.ages]
    en = Engine(starts, [a[3] for a in inp.ages], inp.params["span"], dat.engine, inp.params["detail"],
                w.tension if conflict else None, Hooks(w))
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
        relation=w.relation_table(),
    )
    res["_world"] = w
    res["_engine"] = en
    res["_catalogue"] = cat
    res["_inputs"] = inp
    return res


TABLES = pe.TABLES
COLUMNS = pe.COLUMNS


def table_hashes(res):
    return pe.table_hashes(res)


# ---------------------------------------------------------------------------
# Output: [history], [stats] (P3-05 15.4 + 15.4 here), wars, tensions
# ---------------------------------------------------------------------------

def report_lines(res, inp):
    return pe.report_lines(res, inp.params["span"], inp.races)


def stats_lines(res, inp):
    """P3-05 15.4's three keys, then the keys of 15.4 in order."""
    o = list(pp.stats_extra(res))
    kind = {e[0]: e[3] for e in res["event"]}
    rel = res["relation"]
    E = inp.params["span"]
    o.append("relations_enmity = %d" % sum(1 for r in rel if r[4] == "enmity"))
    o.append("relations_peace = %d" % sum(1 for r in rel if r[4] == "peace"))
    yrs = [(r[6] if r[6] is not None else E) - r[5] for r in rel if r[4] == "war"]
    o.append("war_years_total = %d" % sum(yrs))
    o.append("war_years_max = %d" % (max(yrs) if yrs else 0))
    o.append("civilizations_conquered = %d" % sum(1 for c in res["civilization"]
                                                 if c[9] and kind[c[9]] == K["civ_conquered"]))
    o.append("settlements_sacked = %d" % sum(1 for s in res["settlement"] if s[13] and kind[s[13]] == K["sacked"]))
    o.append("settlements_captured = %d" % len({e[6] for e in res["event"] if e[3] == K["settlement_captured"]}))
    o.append("regions_conquered = %d" % sum(1 for r in res["region_control"]
                                           if kind[r[4]] in (K["settlement_captured"], K["region_ceded"])))
    o.append("races_alive = %d" % sum(1 for r in inp.races if any(c[2] == r[0] and c[8] is None
                                                                   for c in res["civilization"])))
    civ_race = {c[0]: c[2] for c in res["civilization"]}
    for r in inp.races:
        o.append("wars_attacker_%s = %d" % (r[1], sum(1 for x in rel if x[4] == "war" and civ_race[x[1]] == r[0])))
    if pe.at_least("0.18.0"):
        o += stats21_lines(res, inp)       # P3-07 r8 21.5
    return o


# ---------------------------------------------------------------------------
# P3-07 r8 21 (P3-09): the six [stats] keys, the war notes, units and the summary set, computed from
# the rows only (reference-only until P3-07 is implemented; no world byte depends on them)
# ---------------------------------------------------------------------------

def _index(res):
    kind = {e[0]: e[3] for e in res["event"]}
    ev = {e[0]: e for e in res["event"]}
    causes, kids = {}, {}
    for (e, c) in res["event_cause"]:
        causes.setdefault(e, []).append(c)
        kids.setdefault(c, []).append(e)
    parts = {}
    for p_ in res["event_participant"]:
        parts.setdefault(p_[0], []).append(p_)
    return kind, ev, causes, kids, parts


def war_events(res, d, idx=None):
    """21.5 "the war of d": the battles caused by d, their sieges, the falls caused by those, and the war row's
    until_event u with the cessions and tributes caused by u. Returns (set of event ids, u or None)."""
    kind, _ev, _causes, kids, _parts = idx or _index(res)
    row = [r for r in res["relation"] if r[4] == "war" and r[7] == d]
    out = set()
    for b in kids.get(d, []):
        if kind[b] not in (K["battle"], K["battle_at_pass"]):
            continue
        out.add(b)
        for sg in kids.get(b, []):
            if kind[sg] != K["siege"]:
                continue
            out.add(sg)
            for f in kids.get(sg, []):
                if kind[f] in (K["sacked"], K["settlement_captured"]):
                    out.add(f)
    u = None
    if row and row[0][6] is not None:
        u = row[0][8]
        out.add(u)
        for x in kids.get(u, []):
            if kind[x] in (K["region_ceded"], K["tribute"]):
                out.add(x)
    return out, u


def stats21_lines(res, inp):
    """P3-07 r8 21.5: the six keys appended after wars_attacker_<race>."""
    idx = _index(res)
    kind, ev, _causes, _kids, parts = idx
    E = inp.params["span"]
    decl = [e for e in res["event"] if e[3] == K["war_declared"]]
    hollow = unfound = 0
    for d in decl:
        evs, u = war_events(res, d[0], idx)
        if u is not None and kind[u] == K["search_abandoned"]:
            unfound += 1                 # r10 / P3-07 21.8 item 2: counted apart, neither hollow nor not
            continue
        changed = any(kind[x] in (K["sacked"], K["settlement_captured"], K["region_ceded"], K["tribute"]) for x in evs)
        if not changed and not (u is not None and kind[u] == K["civ_conquered"]):
            hollow += 1
    role = lambda e, seq: [p_[4] for p_ in parts.get(e, []) if p_[2] == seq][0]  # noqa: E731
    groups = {}
    for d in decl:
        groups.setdefault((role(d[0], 2), d[5]), []).append(d[0])
    span = {}
    for r in res["relation"]:
        if r[4] == "war":
            span[r[7]] = (r[5], r[6] if r[6] is not None else E)
    unchanged = 0
    for (b, g), ds in groups.items():
        changes = [y for rc in res["region_control"] if rc[0] == g for y in (rc[1], rc[2]) if y is not None]
        run = 0
        for d in sorted(ds):
            lo, hi = span[d]
            if any(lo <= y <= hi for y in changes):
                run = 0
            else:
                run += 1
                unchanged = max(unchanged, run)
    civ_race = {c[0]: c[2] for c in res["civilization"]}
    standing = [x for x in res["settlement"] if x[10] in (1, 2)]
    pop = sum(x[11] for x in standing)
    held = [x for x in res["region_control"] if x[2] is None]
    pshare = max([sum(x[11] for x in standing if civ_race[x[8]] == r[0]) * 1000 // pop for r in inp.races]
                 or [0]) if pop else 0
    rshare = max([sum(1 for x in held if civ_race[x[3]] == r[0]) * 1000 // len(held) for r in inp.races]
                 or [0]) if held else 0
    return ["wars_hollow = %d" % hollow,
            "wars_hollow_permille = %d" % (1000 * hollow // (len(decl) - unfound) if len(decl) > unfound else 0),
            "wars_same_goal_max = %d" % (max(len(v) for v in groups.values()) if groups else 0),
            "wars_same_goal_unchanged_max = %d" % unchanged,
            "race_population_share_max_permille = %d" % pshare,
            "race_regions_share_max_permille = %d" % rshare,
            "wars_unfound = %d" % unfound,
            "refuges_held_no_destination = %d" % sum(1 for e in res["event"] if e[3] == K["refuge_held"] and
                                                     any(v[0] == e[0] and v[1] == 1 and v[2] == 1
                                                         for v in res["event_value"]))]


ORDINALS = {2: "second", 3: "third", 4: "fourth", 5: "fifth", 6: "sixth", 7: "seventh", 8: "eighth", 9: "ninth",
            10: "tenth"}


def ordinal(n):
    """P3-07 r8 21.4 item 3."""
    if n in ORDINALS:
        return ORDINALS[n]
    if n % 100 in (11, 12, 13):
        return "%dth" % n
    return "%d%s" % (n, {1: "st", 2: "nd", 3: "rd"}.get(n % 10, "th"))


def mid(name):
    """A name in the middle of a sentence (P3-04 8.5 r10): a leading "The " lower-cased."""
    return "the " + name[4:] if name.startswith("The ") else name


def history_note(res, inp, eid, view=None, idx=None):
    """P3-07 r8 21.4: the note of event eid without its parentheses, or ""."""
    kind, ev, causes, _kids, parts = idx or _index(res)
    e = ev[eid]
    if e[3] == K["war_declared"]:
        cl = sorted(c for c in causes.get(eid, []) if kind[c] == K["claim"])
        if not cl:
            return ""
        yc = ev[cl[0]][1]
        note = "Claimed since %d." % yc if yc < e[1] else "Claimed the same year."
        pair = sorted(p_[4] for p_ in parts[eid] if p_[3] == CIV)
        n = 1 + sum(1 for x in res["event"] if x[3] == K["war_declared"] and x[0] < eid and
                    sorted(p_[4] for p_ in parts[x[0]] if p_[3] == CIV) == pair)
        if n >= 2:
            note += " The %s war between them." % ordinal(n)
        return note
    if e[3] == K["flare_up"]:
        cs = sorted(causes.get(eid, []))
        if not cs or kind[cs[0]] != K["claim"]:
            return ""
        c = cs[0]
        view = view or pe.view_from_rows(res, [(r["id"], r["name"]) for r in inp.regions], inp.races, inp.gods)
        a = [p_[4] for p_ in parts[c] if p_[2] == 1][0]
        b = [p_[4] for p_ in parts[c] if p_[2] == 2][0]
        return "%s pressed its claim to %s against %s." % (view["name"](CIV, a, "name"),
                                                          mid(view["name"](REGION, ev[c][5], "name")),
                                                          mid(view["name"](CIV, b, "name")))
    return ""


def note_lines(name, res, inp):
    idx = _index(res)
    view = pe.view_from_rows(res, [(r["id"], r["name"]) for r in inp.regions], inp.races, inp.gods)
    out = []
    for e in res["event"]:
        if e[3] in (K["war_declared"], K["flare_up"]):
            n = history_note(res, inp, e[0], view, idx)
            if n:
                out.append("%s note e%d %s" % (name, e[0], n))
    return out


SUMMARY_MIN_SALIENCE, SUMMARY_AGE_FLOOR, SUMMARY_BUDGET = 20, 20, 250


def unit(res, eid, idx=None):
    """P3-07 r8 21.2: e with its companions."""
    kind, ev, _causes, kids, _parts = idx or _index(res)
    k = kind[eid]
    out = {eid}
    if k == K["war_declared"]:
        row = [r for r in res["relation"] if r[4] == "war" and r[7] == eid]
        if row and row[0][6] is not None:
            u = row[0][8]
            out.add(u)
            out |= {x for x in kids.get(u, []) if kind[x] in (K["region_ceded"], K["tribute"])}
    elif k in (K["peace"], K["war_won"], K["truce"]):   # r10 / P3-07 21.8 item 1
        out |= {x for x in kids.get(eid, []) if kind[x] in (K["region_ceded"], K["tribute"])}
    elif k in (K["sacked"], K["settlement_captured"]):
        out |= {x for x in kids.get(eid, []) if kind[x] in (K["capital_moved"], K["refugees_settled"],
                                                             K["civ_conquered"])}
    return out


def summary_set(res, inp):
    """P3-07 r8 21.3: (T, the summary set, per-age counts of the set)."""
    idx = _index(res)
    ev = idx[1]
    cands = sorted((e for e in res["event"] if e[7] >= SUMMARY_MIN_SALIENCE), key=lambda e: (-e[7], e[0]))
    units = {e[0]: unit(res, e[0], idx) for e in cands}
    ages = [(a[0], a[2], a[3]) for a in inp.ages]
    floor = set()
    for _aid, y0, y1 in ages:
        for e in cands:
            if sum(1 for x in floor if y0 <= ev[x][1] <= y1) >= SUMMARY_AGE_FLOOR:
                break
            if not (y0 <= e[1] <= y1) or e[0] in floor:
                continue
            floor |= units[e[0]]
    T, shown = 100, None
    for t in range(SUMMARY_MIN_SALIENCE, 101):
        c_t = set()
        for e in cands:
            if e[7] >= t:
                c_t |= units[e[0]]
        if len(floor | c_t) <= SUMMARY_BUDGET:
            T, shown = t, floor | c_t
            break
    if shown is None:
        c_t = set()
        for e in cands:
            if e[7] >= 100:
                c_t |= units[e[0]]
        shown = floor | c_t
    per_age = [sum(1 for x in shown if y0 <= ev[x][1] <= y1) for _aid, y0, y1 in ages]
    return T, shown, per_age


def summary_lines21(name, res, inp):
    T, shown, per_age = summary_set(res, inp)
    return ["%s summary threshold %d shown %d ages %s" % (name, T, len(shown), ",".join(str(n) for n in per_age))]


def war_lines(name, res):
    w = res["_world"]
    o = []
    for war in w.wars:
        if war["open"]:
            span, how, out = "%d-open" % war["start"], "open", "-"
        else:
            y, how, win = war["end"]
            span = "%d-%d" % (war["start"], y)
            if how in ("conquest", "ended"):
                out = "-"
            elif how == "unfound":
                out = "unfound"           # r10 20.17: search_abandoned
            else:
                out = ("won-c%d" % win + ("-ceded" if war["ceded"] else "") +
                       ("-tribute" if war.get("cost") == "tribute" else "")) if win is not None else "truce"
        o.append("%s war e%d %s c%d c%d rg%d claim e%d battles %d sacked %d taken %d %s %s" % (
            name, war["w"], span, war["a"], war["b"], war["goal"], war["claim"], war["battles"], war["sacked"],
            war["captured"], how, out))
    return o


def tension_lines(name, res):
    return ["%s q%d %d e%d" % (name, s, v, e) for (s, v, e, _p) in res["_world"].tension_log]


def sentences(res, inp):
    view = pe.view_from_rows(res, [(r["id"], r["name"]) for r in inp.regions], inp.races, inp.gods)
    return [pe.render(view, e[0]) for e in res["event"]]


def chronicle_lines(res, inp):
    """15.2's line form with the participants: <span> e<id> <kind>/<salience>: <sentence> {...} [causes ...]."""
    causes = {}
    for (e, c) in res["event_cause"]:
        causes.setdefault(e, []).append(c)
    parts = {}
    for p in res["event_participant"]:
        parts.setdefault(p[0], []).append(p)
    vals = {}
    for v in res["event_value"]:
        vals.setdefault(v[0], []).append(v)
    roles = {(r[0], r[1]): r[2] for r in res["event_kind_role"]}
    vkeys = {(v[0], v[1]): v[2] for v in res["event_kind_value"]}
    pref = {CIV: "c", SETTLEMENT: "s", RACE: "ra", GOD: "g", REGION: "rg"}
    out = []
    for e, text in zip(res["event"], sentences(res, inp)):
        span = "%d" % e[1] if e[1] == e[2] else "%d-%d" % (e[1], e[2])
        line = "%s e%d %s/%d: %s" % (span, e[0], KEY_OF[e[3]], e[7], text)
        items = []
        if e[5]:
            items.append("@rg%d" % e[5])
        if e[6]:
            items.append("@s%d" % e[6])
        for p in parts.get(e[0], []):
            items.append("%s=%s%d" % (roles[(e[3], p[2])], pref[p[3]], p[4]))
        for v in vals.get(e[0], []):
            items.append("%s=%d" % (vkeys[(e[3], v[1])], v[2]))
        if items:
            line += " {%s}" % " ".join(items)
        if e[0] in causes:
            line += " [causes %s]" % ", ".join("e%d" % c for c in causes[e[0]])
        out.append(line)
    return out


def summary_lines(res, inp, limit=250):
    """P3-07 7.4 as 15.2 uses it: the events of salience >= the smallest threshold that shows at most
    `limit` events (READING: threshold t = the smallest integer such that #events with salience >= t is <= limit)."""
    thr = 0
    for t in range(0, 102):
        if sum(1 for e in res["event"] if e[7] >= t) <= limit:
            thr = t
            break
    lines = chronicle_lines(res, inp)
    return thr, [l for l, e in zip(lines, res["event"]) if e[7] >= thr]


def kind_counts(res):
    out = {}
    for e in res["event"]:
        out[KEY_OF[e[3]]] = out.get(KEY_OF[e[3]], 0) + 1
    return out


def spec_war_lines(res, inp):
    """The war list of 15.2 in its text form."""
    w = res["_world"]
    cname = {c[0]: c[1] for c in res["civilization"]}
    rname = {r["id"]: r["name"] for r in inp.regions}
    ev = {e[0]: e for e in res["event"]}
    out = []
    for war in w.wars:
        if war["open"]:
            end, how = "open", "open at the present"
            res_ = "no victor"
        else:
            y, h, win = war["end"]
            end = str(y)
            how = {"exhaustion": "exhaustion", "aim": "aim taken", "no-front": "no front", "length": "length",
                   "quiet": "end of the active phase", "conquest": "conquest of the defender"
                   if not w.alive(war["b"]) else "conquest of the attacker", "ended": "end of a side"}[h]
            res_ = ("won by %s" % cname[win] + (", ceded" if war["ceded"] else "")) if win is not None else "no victor"
        out.append("e%d, %d-%s: %s (c%d) on %s (c%d) over %s, claim e%d (%d); %d battles, %d sacked, %d taken; %s, %s." % (
            war["w"], war["start"], end, cname[war["a"]], war["a"], cname[war["b"]], war["b"], rname[war["goal"]],
            war["claim"], ev[war["claim"]][1], war["battles"], war["sacked"], war["captured"], how, res_))
    return out


def end_state(res, inp):
    rname = {r["id"]: r["name"] for r in inp.regions}
    race_plural = {r[0]: r[3] for r in inp.races}
    kind = {e[0]: e[3] for e in res["event"]}
    parts = {}
    for p in res["event_participant"]:
        parts.setdefault(p[0], []).append(p)
    out = []
    for c in res["civilization"]:
        mine = [s for s in res["settlement"] if s[8] == c[0] and s[10] in (1, 2)]
        held = [r[0] for r in res["region_control"] if r[2] is None and r[3] == c[0]]
        if c[8] is None:
            fate = "alive: %d regions, %d settlements, %d people" % (len(held), len(mine), sum(s[11] for s in mine))
        else:
            how = KEY_OF[kind[c[9]]]
            by = [p[4] for p in parts.get(c[9], []) if p[2] == 2]
            fate = "ended %d by %s%s" % (c[8], how, (" of c%d" % by[0]) if by else "")
        out.append("c%d %s (%s), founded %d: %s" % (c[0], c[1], race_plural[c[2]], c[6], fate))
    return out


def world_counts(res):
    kc = kind_counts(res)
    w = res["_world"]
    ruined = sum(1 for s in res["settlement"] if s[10] == RUINED)
    return dict(wars=len(w.wars), battles=kc.get("battle", 0) + kc.get("battle_at_pass", 0),
                sacked=kc.get("sacked", 0), taken=kc.get("settlement_captured", 0),
                ended=sum(1 for c in res["civilization"] if c[8] is not None),
                ruined=ruined, flares=sum(1 for r in res["phase"] if r[2] == FLARE),
                simulated=res["_engine"].simulated(), events=len(res["event"]),
                conquered=kc.get("civ_conquered", 0), claims=kc.get("claim", 0), passes=kc.get("battle_at_pass", 0),
                ceded=kc.get("region_ceded", 0), truces=kc.get("truce", 0), peaces=kc.get("peace", 0))


# ---------------------------------------------------------------------------
# Inputs: world cases, synthetic and stress cases (16.2 item 15)
# ---------------------------------------------------------------------------

def inputs_for(dat, conn, p, seed, size):
    inp, chron = pp.inputs_for(dat, conn, p, seed, size)
    inp.biome_keys = {r[0]: r[1] for r in conn.execute("SELECT id, key FROM biome ORDER BY id")}
    return inp, chron


def synthetic_inputs(seed, dat=None, case=None):
    dat = dat or data()
    g = pp.make_synthetic(seed)
    span, races, civs, intensity = case or synthetic_case(seed)
    p = pchr.params(span=span, races=pchr.races_of(races, dat.chron), civs=pp.civs_param(civs), intensity=intensity)
    chron = pchr.chronology(dat.chron, p, seed, [r["name"] for r in g["regions"]])
    inp = pp.Inputs(seed, g["n"], p, g["regions"], chron["race"], chron["race_biome"], chron["culture"], chron["god"],
                    chron["age"], g["borders"], g["region_biomes"], g["biomes"], lambda: g["layers"])
    inp.biome_keys = dict(BIOME_KEYS)
    return inp, g


# 16.2 item 15's grid, cycled over the seeds: spans 2000, 6000; races all, elf,goblin, elf,orc, dwarf,faerie;
# civs auto, 4; intensities normal, calm, turbulent.
SYN_RACES = ("all", "elf,goblin", "elf,orc", "dwarf,faerie")
SYN_CIVS = ("auto", "4")
SYN_INTENSITY = ("normal", "calm", "turbulent")
SYN_SEEDS = tuple(range(16))


def synthetic_case(seed):
    return ((2000, 6000)[seed % 2], SYN_RACES[(seed // 2) % 4], SYN_CIVS[(seed // 8) % 2],
            SYN_INTENSITY[seed % 3])


# Stress tuning (fixture data, not shipped): the peoples stress of P3-05 for growth and secessions, and a
# conflict tuning in which quarrels come fast, wars are long and bloody and towns fall, so that sieges,
# sacks, captures, conquests, cessions, truces, flare-ups, battles at passes, last_refuge and the shelter
# all happen on 192-cell maps.
STRESS_PEOPLES = dict(found_rate_permille=1000, split_min_settlements=2, split_min_regions=2, split_permille=1000,
                      crisis_per_generation_permille=300)
STRESS_CONFLICT = dict(border_friction=600, tick_strain_permille=400, quiet_strain_permille=100, claim_strain=100,
                       declare_min=100, declare_span=200, declare_permille=600, shelter_years=60, march_hops=3,
                       battle_permille=900, garrison_permille=200, exhaustion_permille=150, war_max_years=25,
                       truce_years=40)
STRESS_SEEDS = (0, 1, 2, 3, 4, 5, 6, 7, 9, 12, 13, 14)
STRESS_CASES = {s: ((2000, 6000)[s % 2], ("all", "elf,goblin", "goblin,human,orc", "all")[(s // 2) % 4],
                    ("auto", "4")[(s // 4) % 2], ("normal", "turbulent", "calm")[s % 3]) for s in STRESS_SEEDS}


def stress_data(dat=None):
    return edited(dat or data(), conflict=STRESS_CONFLICT, peoples=STRESS_PEOPLES)


# The guards' counterfactual (9, question 4): the stress tuning with last_refuge 0 and shelter_years 0.
NOREFUGE_SEEDS = (5, 12, 13)


def norefuge_data(dat=None):
    return edited(dat or data(), conflict=dict(STRESS_CONFLICT, last_refuge=0, shelter_years=0),
                  peoples=STRESS_PEOPLES)


# ---------------------------------------------------------------------------
# Invariants (14, with P3-05's as amended by 18.1) on rows
# ---------------------------------------------------------------------------

QUIET_KINDS = set(pp.QUIET_KINDS) | {K["claim"], K["peace"], K["truce"], K["region_ceded"], K["tribute"],
                                      K["war_won"]}  # r9 20.3, r10 20.15


def check_peoples_invariants(c, res, inp, w, label):
    """P3-05 14 items 1-8 as amended by P3-06 18.1 (sacked as a ruin event; battles as reduction causes;
    conflict kinds in quiet steps; captured settlements' kinds by owner)."""
    t = w.t
    ev = {e[0]: e for e in res["event"]}
    kind = {e[0]: e[3] for e in res["event"]}
    causes = {}
    for (e, cc) in res["event_cause"]:
        causes.setdefault(e, []).append(cc)
    lay = inp.peoples_layers()
    n = inp.map_size
    civs = {x[0]: x for x in res["civilization"]}
    sts = {x[0]: x for x in res["settlement"]}
    ph = {r[0]: r for r in res["phase"]}
    setting = inp.params["civs"]
    for a in inp.ages:
        founded = [x for x in res["civilization"] if x[2] == a[1] and kind[x[7]] == K["civ_founded"]]
        c.true("%s P3-05 inv 1 race %d" % (label, a[1]), 1 <= len(founded) <= (pp.auto_max(w) if setting is None
                                                                               else setting))
        for x in founded:
            c.true("%s P3-05 inv 1 founded at the awakening" % label, x[6] == a[2] and kind[causes[x[7]][0]] == 1)
    cells = {}
    faded_at = {e[6]: e[0] for e in res["event"] if e[3] == K["ruin_faded"]}
    for s in res["settlement"]:
        c.true("%s P3-05 inv 2 s%d founding kind" % (label, s[0]), kind[s[7]] in (K["settlement_founded"],
                                                                                  K["colony_founded"],
                                                                                  K["refugees_settled"]))
        i = s[3] * n + s[2]
        c.true("%s P3-05 inv 2 s%d cell" % (label, s[0]), lay["region"][i] == s[4] and lay["water"][i] == 0)
        prev = cells.get((s[2], s[3]))
        c.true("%s P3-05 inv 2 s%d cell unique" % (label, s[0]), prev is None or (prev in faded_at and
                                                                                  faded_at[prev] < s[7]))
        cells[(s[2], s[3])] = s[0]
    for s in res["settlement"]:
        if s[10] in (1, 2):
            c.true("%s inv 6 s%d P <= 2 C" % (label, s[0]), s[11] <= 2 * w.geo.capacity(s[4], civs[s[8]][2]))
    c.eq("%s in-run invariants (P3-05 3-5, P3-06 6)" % label, w.checks, [])
    for r in res["region_control"]:
        if r[2] is None:
            c.true("%s inv 5 region %d held with a settlement" % (label, r[0]),
                   any(s[8] == r[3] and s[4] == r[0] and s[10] in (1, 2) for s in res["settlement"]))
    for x in res["civilization"]:
        mine = [s for s in res["settlement"] if s[8] == x[0] and s[10] in (1, 2)]
        if x[8] is None:
            c.true("%s P3-05 inv 5 c%d capital" % (label, x[0]),
                   x[5] in sts and sts[x[5]][8] == x[0] and sts[x[5]][10] in (1, 2))
        else:
            c.true("%s P3-05 inv 5 ended c%d empty" % (label, x[0]), not mine and not any(
                r[2] is None and r[3] == x[0] for r in res["region_control"]))
    # P3-05 inv 6 as r6 reads it (the P3-06 validation's D1; the 0.16.0 check took the founder's and the
    # row's owner's races only, which fails for ruins and for settlements captured twice)
    pp.check_kinds_built(c, res, w, label + " P3-05")
    cen = {}
    for r in res["settlement_census"]:
        cen.setdefault(r[0], []).append(r)
    for s, rows in cen.items():
        if rows[0][3] == FORTRESS:
            c.true("%s P3-05 inv 6 fortress s%d stays" % (label, s), all(r[3] == FORTRESS for r in rows))
    for s in res["settlement"]:
        if s[10] == RUINED:
            c.true("%s P3-05 inv 7 s%d ruin event" % (label, s[0]), kind[s[13]] in (K["settlement_abandoned"],
                                                                                   K["settlement_abandoned_to"],
                                                                                   K["sacked"]))
        if s[10] == FADED:
            fades = [e for e in res["event"] if e[3] == K["ruin_faded"] and e[6] == s[0]]
            ok = len(fades) == 1 and len(causes.get(fades[0][0], [])) == 1
            if ok:
                ruin = ev[causes[fades[0][0]][0]]
                ok = (ruin[3] in (K["settlement_abandoned"], K["settlement_abandoned_to"], K["sacked"]) and ruin[6] == s[0] and
                      fades[0][1] >= ruin[1] + t["ruin_fade_years"] and s[12] is None and s[13] == 0)
            c.true("%s P3-05 inv 7 s%d faded" % (label, s[0]), ok)
    # r9 20.3: a tribute's people are taken through 7.4's casualties (note_reduction), a reduction like a battle
    red = ("famine", "plague", "plague_spread", "succession_crisis", "battle", "battle_at_pass", "tribute")
    want_cause = {
        "plague_spread": ("plague",), "civ_seceded": ("succession_crisis",),
        "settlement_abandoned": red + ("settlement_founded", "colony_founded"),
        "settlement_abandoned_to": red + ("settlement_founded", "colony_founded"),
        "capital_moved": ("settlement_abandoned", "settlement_abandoned_to", "sacked", "settlement_captured"),
        "civ_ended": ("settlement_abandoned", "settlement_abandoned_to"),
        "ruin_faded": ("settlement_abandoned", "settlement_abandoned_to", "sacked"),
        "settlement_dwindled": red,
    }
    for e in res["event"]:
        key = KEY_OF[e[3]]
        cs = causes.get(e[0], [])
        if key.startswith("became_"):
            c.true("%s P3-05 inv 8 e%d cause" % (label, e[0]), cs == [sts[e[6]][7]])
        elif key in want_cause:
            c.true("%s P3-05 inv 8 e%d %s cause kinds %s" % (label, e[0], key, [KEY_OF[kind[x]] for x in cs]),
                   all(KEY_OF[kind[x]] in want_cause[key] for x in cs) and (cs or key == "settlement_dwindled"))
        if e[3] >= 6 and ph[e[4]][2] == QUIET:
            c.true("%s inv 8/14.2 e%d %s not in a quiet step" % (label, e[0], key), e[3] in QUIET_KINDS)
    pp.check_r6_invariants(c, res, inp, w, label + " P3-05")


def check_invariants(c, res, inp, w, label, last_refuge=True, threshold=None):
    """14 items 1-8 from rows (9 is draw independence, checked by the caller)."""
    check_peoples_invariants(c, res, inp, w, label)
    ev = {e[0]: e for e in res["event"]}
    kind = {e[0]: e[3] for e in res["event"]}
    causes = {}
    for (e, cc) in res["event_cause"]:
        causes.setdefault(e, []).append(cc)
    parts = {}
    for p in res["event_participant"]:
        parts.setdefault(p[0], []).append(p)
    vals = {}
    for v in res["event_value"]:
        vals.setdefault(v[0], {})[v[1]] = v[2]

    def role(e, seq):
        return [p[4] for p in parts.get(e, []) if p[2] == seq]
    civs = {x[0]: x for x in res["civilization"]}
    sts = {x[0]: x for x in res["settlement"]}
    ph = {r[0]: r for r in res["phase"]}
    E = inp.params["span"]
    INF = 1 << 60
    # 1 relations
    rel = res["relation"]
    seen_enmity = set()
    for r in rel:
        lab = "%s inv 1 relation %s" % (label, r)
        c.true(lab + " types", r[0] == CIV and r[2] == CIV)
        c.true(lab + " kind", r[4] in ("enmity", "war", "peace"))
        if r[4] in ("enmity", "peace"):
            c.true(lab + " a < b", r[1] < r[3])
        sk = KEY_OF[kind[r[7]]]
        uk = KEY_OF[kind[r[8]]] if r[6] is not None else None
        if r[4] == "enmity":
            c.true(lab + " one enmity per pair", (r[1], r[3]) not in seen_enmity)
            seen_enmity.add((r[1], r[3]))
            c.true(lab + " since_event", sk in ("civ_founded", "civ_seceded"))
            c.true(lab + " since = younger's founded_year", r[5] == civs[max(r[1], r[3])][6] and
                   r[7] == civs[max(r[1], r[3])][7])
            if uk is not None:
                c.true(lab + " until_event", uk in ("civ_ended", "civ_conquered"))
        elif r[4] == "war":
            c.true(lab + " since_event", sk == "war_declared" and role(r[7], 1) == [r[1]] and role(r[7], 2) == [r[3]])
            c.true(lab + " since = declaration year", ev[r[7]][1] == r[5])
            if uk is not None:
                c.true(lab + " until_event", uk in ("peace", "war_won", "truce", "search_abandoned", "civ_ended",
                                                    "civ_conquered"))
        else:
            c.true(lab + " since_event", sk in ("peace", "war_won", "truce", "search_abandoned"))
            if uk is not None:
                c.true(lab + " until_event", uk in ("war_declared", "civ_ended", "civ_conquered"))
    pairs = {}
    for r in rel:
        if r[4] in ("war", "peace"):
            pairs.setdefault((min(r[1], r[3]), max(r[1], r[3])), []).append(r)
    for pr, rows in pairs.items():
        iv = sorted((r[5], r[6] if r[6] is not None else INF, r[4]) for r in rows)
        for x, y in zip(iv, iv[1:]):
            c.true("%s inv 1 pair %s rows overlap %s %s" % (label, pr, x, y), x[1] <= y[0])
    # N5: every war_declared leaves exactly one war row; ends of a war
    war_rows = {r[7]: r for r in rel if r[4] == "war"}
    for e in res["event"]:
        if e[3] != K["war_declared"]:
            continue
        lab = "%s inv 2 war e%d" % (label, e[0])
        a, b = role(e[0], 1)[0], role(e[0], 2)[0]
        c.true(lab + " leaves a war row (N5)", e[0] in war_rows)
        cl = [x for x in causes.get(e[0], []) if kind[x] == K["claim"]]
        c.true(lab + " caused by a claim of the attacker on the defender",
               any(role(x, 1) == [a] and role(x, 2) == [b] and ev[x][1] <= e[1] for x in cl))
        r = war_rows.get(e[0])
        if r is not None and r[6] is not None:
            u = r[8]
            uk = KEY_OF[kind[u]]
            if uk in ("peace", "war_won", "truce", "search_abandoned"):
                ps = sorted(p[4] for p in parts[u])
                c.true(lab + " ended by a peace of its sides", ps == sorted((a, b)))
                c.true(lab + " peace dated at the row's until", ev[u][1] == r[6])
            else:
                ended = role(u, 1)[0]
                c.true(lab + " ended by the end of a side", ended in (a, b) and civs[ended][9] == u)
            # 7.3: no war ends in its declaration year by peace or truce; a side conquered by another war in
            # that year's T6 closes it in the same year (8.3 item 7, P3-04 6.7 r9: since = until)
            c.true(lab + " no war ends in its declaration year by peace (7.3)",
                   r[6] > r[5] or KEY_OF[kind[r[8]]] in ("civ_conquered", "civ_ended"))
    for e in res["event"]:
        key = KEY_OF[e[3]]
        if key in ("battle", "battle_at_pass", "siege", "sacked", "settlement_captured", "refuge_held",
                   "search_abandoned"):
            c.true("%s inv 2 e%d %s not in a quiet phase" % (label, e[0], key), ph[e[4]][2] != QUIET)
        if key in ("peace", "war_won", "truce", "region_ceded", "tribute") and ph[e[4]][2] == QUIET:
            row = ph[e[4]]
            c.true("%s inv 2 e%d %s at a quiet step start" % (label, e[0], key),
                   (e[1] - row[3]) % w.eng["quiet_step_years"] == 0)
        if key in ("peace", "war_won", "truce", "search_abandoned"):
            cs = causes.get(e[0], [])
            c.true("%s inv 2 e%d caused by the war's last event" % (label, e[0]),
                   len(cs) == 1 and KEY_OF[kind[cs[0]]] in ("war_declared", "battle", "battle_at_pass", "siege",
                                                           "sacked", "settlement_captured", "refugees_settled",
                                                           "refuge_held"))
        if key == "region_ceded":
            cs = causes.get(e[0], [])
            c.true("%s 11 e%d cession caused by a peace" % (label, e[0]), len(cs) == 1 and
                   kind[cs[0]] in (K["peace"], K["war_won"]))
        if key in ("battle", "battle_at_pass"):
            cs = causes.get(e[0], [])
            c.true("%s 11 e%d caused by its declaration" % (label, e[0]), len(cs) == 1 and
                   kind[cs[0]] == K["war_declared"])
        if key == "claim":
            cs = causes.get(e[0], [])
            # 8.3 item 4: a colony of the same step is no claim cause, unless it is the region's
            # owner_event (its ownership is live state)
            c.true("%s 8.3 item 4 e%d claim cause not a colony of the same step" % (label, e[0]),
                   not any(kind[x] == K["colony_founded"] and ev[x][1] == e[1] and
                           not any(r[0] == e[5] and r[4] == x for r in res["region_control"]) for x in cs))
            c.true("%s 11 e%d claim cause" % (label, e[0]), len(cs) == 1 and KEY_OF[kind[cs[0]]] in (
                "civ_founded", "settlement_founded", "colony_founded", "civ_seceded", "settlement_captured",
                "region_ceded", "refugees_settled"))
    # r7 7.3 item 4: with loss_winner <= loss_loser the victors never lose more than the defeated
    if FLAGS["loss"] and w.ct["loss_winner_permille"] <= w.ct["loss_loser_permille"]:
        for e in res["event"]:
            if e[3] in (K["battle"], K["battle_at_pass"]):
                c.true("%s r7 e%d the victors lose no more than the defeated" % (label, e[0]),
                       vals[e[0]][1] <= vals[e[0]][2])
    # 3 falls
    for e in res["event"]:
        if e[3] not in (K["sacked"], K["settlement_captured"]):
            continue
        lab = "%s inv 3 e%d" % (label, e[0])
        cs = causes.get(e[0], [])
        ok = len(cs) == 1 and kind[cs[0]] == K["siege"]
        if ok:
            sg = ev[cs[0]]
            ok = sg[6] == e[6] and sg[1] == e[1] and vals[sg[0]][1] == 1 and role(sg[0], 1) == role(e[0], 1)
            bc = causes.get(sg[0], [])
            ok = ok and len(bc) == 1 and kind[bc[0]] in (K["battle"], K["battle_at_pass"]) and \
                ev[bc[0]][1] == e[1] and role(bc[0], 1) == role(sg[0], 1)
        c.true(lab + " caused by a fallen siege after a won battle", ok)
    for e in res["event"]:
        if e[3] == K["siege"]:
            bc = causes.get(e[0], [])
            c.true("%s inv 3 siege e%d after a battle won by its attacker" % (label, e[0]),
                   len(bc) == 1 and kind[bc[0]] in (K["battle"], K["battle_at_pass"]) and
                   role(bc[0], 1) == role(e[0], 1) and ev[bc[0]][1] == e[1])
    # 4 ends
    for e in res["event"]:
        if e[3] != K["civ_conquered"]:
            continue
        lab = "%s inv 4 e%d" % (label, e[0])
        y, x = role(e[0], 1)[0], role(e[0], 2)[0]
        c.true(lab + " ends its civilization", civs[y][9] == e[0] and civs[y][8] == e[1])
        cs = causes.get(e[0], [])
        c.true(lab + " caused by a fall of its last settlement", len(cs) == 1 and
               kind[cs[0]] in (K["sacked"], K["settlement_captured"]) and role(cs[0], 2) == [y])
        cx = civs[x]
        c.true(lab + " conqueror alive", cx[8] is None or cx[8] > e[1] or (cx[8] == e[1] and cx[9] > e[0]))
    for x in res["civilization"]:
        if x[8] is not None:
            c.true("%s inv 4 ended c%d no open relation" % (label, x[0]),
                   not any(r[6] is None and x[0] in (r[1], r[3]) for r in rel))
    # 5 territory: every owner change by P3-06 is a capture or a cession
    for r in res["region_control"]:
        c.true("%s inv 5 region_control %s event kind" % (label, r), KEY_OF[kind[r[4]]] in (
            "civ_founded", "colony_founded", "civ_seceded", "settlement_captured", "region_ceded",
            "refugees_settled"))
    # 7 races
    if last_refuge:
        for a in inp.ages:
            c.true("%s inv 7 race %d alive at the present" % (label, a[1]),
                   any(x[2] == a[1] and x[8] is None for x in res["civilization"]))
    # 8 tension
    thr = threshold if threshold is not None else w.eng[inp.params["detail"]]["flare_threshold"]
    log = {s: (v, e) for (s, v, e, _p) in w.tension_log}
    for r in res["phase"]:
        if r[2] != FLARE:
            continue
        c.true("%s inv 8 flare-up %d trigger is a claim" % (label, r[0]), kind.get(r[5]) == K["claim"])
        prev = [s for s in log if s < r[3]]
        c.true("%s inv 8 flare-up %d strain >= threshold" % (label, r[0]),
               bool(prev) and log[max(prev)][0] >= thr and log[max(prev)][1] == r[5])
    c.eq("%s in-run conflict checks" % label, w.checks6, [])
    if pe.at_least("0.18.0"):
        check_invariants_r9(c, res, inp, w, label)


def concealed_at(w, rid, civ_race):
    """20.7 item 1 from the inputs: a settlement of race r in R is concealed when B(R, r) >= conceal_min(r)."""
    return w.bterm(rid, civ_race) >= w.pd.race[civ_race]["conceal"]


def weary_violations(res, ww, E, count_open=False):
    """20.13 item 5 from the rows (sound, conservative): per (defender, region) walk the wars in id order; a war
    ended by peace or truce during whose span [declaration, end] the region's owner never changed (no
    region_control row of the region starts or ends in the span, nor between the run's wars) adds one; any change
    restarts the run. A claim or declaration naming (defender, region) after `ww` such wars ended, with no change
    since, is a violation. Returns the list of (event id, defender, region)."""
    kind, ev, _causes, _kids, parts = _index(res)
    if ww == 0:
        return []
    role = lambda e, seq: [p_[4] for p_ in parts.get(e, []) if p_[2] == seq][0]  # noqa: E731
    rows = {r[7]: r for r in res["relation"] if r[4] == "war"}
    changes = {}
    for rc in res["region_control"]:
        for y in (rc[1], rc[2]):
            if y is not None:
                changes.setdefault(rc[0], []).append(y)
    ends = {}   # (b, g) -> [(end event id, start year, end year)]
    for e in res["event"]:
        if e[3] != K["war_declared"]:
            continue
        r = rows.get(e[0])
        if r is None or r[6] is None or kind[r[8]] not in (K["peace"], K["war_won"], K["truce"],
                                                            K["search_abandoned"]):
            continue
        ends.setdefault((role(e[0], 2), e[5]), []).append((r[8], e[1], r[6]))
    out = []
    for e in res["event"]:
        if e[3] == K["claim"]:
            b = role(e[0], 2)
        elif e[3] == K["war_declared"]:
            b = role(e[0], 2)
        else:
            continue
        g = e[5]
        done = sorted(x for x in ends.get((b, g), []) if x[0] < e[0])
        ch = changes.get(g, [])
        run = 0
        last_end = None
        for (_u, y0, y1) in done:
            if last_end is not None and any(last_end <= y <= y0 for y in ch):
                run = 0
            if any(y0 <= y <= y1 for y in ch):
                run = 0
                last_end = y1
                continue
            run += 1
            last_end = y1
        if count_open:   # 20.23: the wars over (b, g) open at e, by any attacker
            opened = sum(1 for r in res["relation"] if r[4] == "war" and r[7] < e[0] and
                         ev[r[7]][5] == g and role(r[7], 2) == b and (r[6] is None or r[8] > e[0]))
            now = run if (last_end is not None and not any(last_end <= y <= e[1] for y in ch)) else 0
            if now + opened >= ww:
                out.append((e[0], b, g))
            continue
        if run >= ww and last_end is not None and not any(last_end <= y <= e[1] for y in ch):
            out.append((e[0], b, g))
    return out


def check_invariants_r9(c, res, inp, w, label):
    """P3-06 r9 20.13 items 3-6 from the rows (items 1 and 2: check_invariants), with the in-run counters."""
    kind, ev, causes, kids, parts = _index(res)
    vals = {}
    for v in res["event_value"]:
        vals.setdefault(v[0], {})[v[1]] = v[2]
    role = lambda e, seq: [p_[4] for p_ in parts.get(e, []) if p_[2] == seq]  # noqa: E731
    civs = {x[0]: x for x in res["civilization"]}
    sts = {x[0]: x for x in res["settlement"]}
    t = w.ct
    hy = t["hidden_years"]
    # 3 refugees_settled
    for e in res["event"]:
        if e[3] != K["refugees_settled"]:
            continue
        lab = "%s r9 20.13.3 e%d" % (label, e[0])
        h, y, o = role(e[0], 1)[0], role(e[0], 2)[0], role(e[0], 3)[0]
        c.true(lab + " population >= abandon_min", vals[e[0]][1] >= w.t["abandon_min"])
        cs = causes.get(e[0], [])
        ok = len(cs) == 1 and kind[cs[0]] == K["sacked"] and ev[cs[0]][1] == e[1] and ev[cs[0]][6] == o and \
            role(cs[0], 2) == [y]
        c.true(lab + " caused by the sack of its origin, by its founder's enemy, the same year", ok)
        c.true(lab + " the settlement is the event's and founded by it", e[6] == h and sts[h][7] == e[0] and
               sts[h][5] == y and sts[h][4] == e[5])
        race = civs[y][2]
        others = [x for x in res["civilization"] if x[2] == race and x[0] != y and x[6] <= e[1] and
                  (x[8] is None or x[8] > e[1] or (x[8] == e[1] and x[9] > e[0]))]
        c.true(lab + " the founder was its race's only alive civilization", not others)
        c.true(lab + " the origin was the founder's capital (sacked at capital_sacked_salience)",
               ev[cs[0]][7] == t["capital_sacked_salience"])
        c.true(lab + " no kind 31 caused by the sack", not any(kind[x] == K["settlement_abandoned_to"]
                                                                  for x in kids.get(cs[0], [])))
        y_end = e[1] + hy
        for x in res["event"]:
            if not (e[1] <= x[1] < y_end) or x[0] <= e[0]:
                continue
            if x[3] == K["siege"]:
                c.true(lab + " hidden: never besieged (e%d)" % x[0], x[6] != h)
            if x[5] != e[5] or x[3] not in (K["battle"], K["battle_at_pass"], K["claim"], K["war_declared"]):
                continue
            against = (y in role(x[0], 1) + role(x[0], 2)) if x[3] in (K["battle"], K["battle_at_pass"]) else \
                role(x[0], 2) == [y]
            if not against:
                continue
            # only while h is the founder's only settlement there: skip when another of its settlements may be
            alone = not any(s_[0] != h and s_[4] == e[5] and s_[6] <= x[1] for s_ in res["settlement"]
                            if s_[5] == y or s_[8] == y)
            if alone:
                c.true(lab + " hidden: no %s in its region against its founder (e%d)" % (KEY_OF[x[3]], x[0]), False)
    # 4 tribute
    for e in res["event"]:
        if e[3] == K["tribute"]:
            cs = causes.get(e[0], [])
            ok = len(cs) == 1 and kind[cs[0]] in (K["peace"], K["war_won"]) and role(cs[0], 2) == role(e[0], 1) and \
                role(cs[0], 1) == role(e[0], 2)
            c.true("%s r9 20.13.4 e%d tribute caused by the peace its payer lost" % (label, e[0]), ok)
            c.true("%s r9 20.3 e%d tribute people >= 1" % (label, e[0]), vals[e[0]][1] >= 1)
        if e[3] in (K["peace"], K["war_won"]):
            costs = [x for x in kids.get(e[0], []) if kind[x] in (K["region_ceded"], K["tribute"])]
            c.true("%s r9 20.13.4 e%d at most one cession or tribute per peace" % (label, e[0]), len(costs) <= 1)
        if e[3] in (K["truce"], K["search_abandoned"]):
            c.true("%s r9 20.3 e%d a truce (or a war given up, r10 20.17) costs nothing" % (label, e[0]),
                   not any(kind[x] in (K["region_ceded"], K["tribute"]) for x in kids.get(e[0], [])))
    # every won war has a cession, a tribute or the stated exception (in-run: w.cost_skips)
    for war in w.wars:
        if war["end"] is None or war["end"][1] in ("conquest", "ended", "unfound") or war["end"][2] is None:
            continue
        if war.get("cost") is None:
            L = war["b"] if war["end"][2] == war["a"] else war["a"]
            c.true("%s r9 20.3 war e%d won without a cost: only when the loser was a last refuge or had no people "
                   "to take" % (label, war["w"]), war.get("no_cost") in ("refuge", "empty"))
    # 5 weariness, from the rows
    c.eq("%s r9 20.13.5 no claim or declaration over a region its defender is weary of" % label,
         weary_violations(res, t["weary_wars"], inp.params["span"], FLAGS["weary_open"]), [])
    # 6 concealment (r9): from r10 (20.17) a concealed settlement is besieged once found; item 11 of 20.19
    # replaces it (check_invariants_r10)
    if not FLAGS["search"]:
        for e in res["event"]:
            if e[3] == K["siege"]:
                y = role(e[0], 2)[0]
                c.true("%s r9 20.13.6 e%d siege of a settlement not concealed" % (label, e[0]),
                       not concealed_at(w, sts[e[6]][4], civs[y][2]))
    # T3 (P3-09 5): no last refuge holds a siege while a destination with enough survivors exists
    c.eq("%s T3 guard holds with a destination" % label, w.guard_with_dest, 0)
    check_invariants_r10(c, res, inp, w, label, kind, ev, causes, kids, parts, vals)
    c.eq("%s in-run r9 checks" % label, w.checks9, [])


def last_battle_winner(res, d, idx):
    """The winner of the war of d's most recent battle (by id), or None."""
    kind, _ev, _causes, kids, parts = idx
    bs = [b for b in kids.get(d, []) if kind[b] in (K["battle"], K["battle_at_pass"])]
    if not bs:
        return None
    return [p_[4] for p_ in parts[max(bs)] if p_[2] == 1][0]


def t3_destinations(res, inp, w, e_held, y):
    """20.22 from the rows: the flight destinations of the refuge_held event (the region owners of year y from
    region_control, the capacity and the site rule of P3-05 6.2 with the settlements of the rows standing then)."""
    ev = {e[0]: e for e in res["event"]}
    e = ev[e_held]
    st = {s[0]: s for s in res["settlement"]}
    rt = e[5]
    defender = [p_[4] for p_ in res["event_participant"] if p_[0] == e_held and p_[2] == 2][0]
    race = [c[2] for c in res["civilization"] if c[0] == defender][0]
    owned = {rc[0] for rc in res["region_control"] if rc[1] <= y and (rc[2] is None or rc[2] > y)}
    hops = w.geo.hops(rt)
    out = []
    for d in w.geo.ids:
        if d in owned or w.geo.islet(d) or d == rt or d not in hops or hops[d] > w.ct["flight_hops"]:
            continue
        if w.geo.capacity(d, race) < w.t["expand_min_capacity"]:
            continue
        out.append(d)
    return out


def check_invariants_r10(c, res, inp, w, label, kind, ev, causes, kids, parts, vals):
    """P3-06 r10 20.19 items 7-11, 20.22 (T3) and 20.20 from the rows and the in-run logs."""
    idx = (kind, ev, causes, kids, parts)
    t = w.ct
    role = lambda e, seq: [p_[4] for p_ in parts.get(e, []) if p_[2] == seq]  # noqa: E731
    war_of_end = {r[8]: r[7] for r in res["relation"] if r[4] == "war" and r[6] is not None}
    ends = {war["w"]: war for war in w.wars}
    for e in res["event"]:
        k = e[3]
        if k in (K["peace"], K["war_won"]):
            d = war_of_end.get(e[0])
            c.true("%s r10 e%d %s closes a war row" % (label, e[0], KEY_OF[k]), d is not None)
            if d is None:
                continue
            war = ends[d]
            how = war["end"][1]
            if k == K["peace"]:   # (7) a peace follows an ending by exhaustion with an exhausted loser
                L = role(e[0], 2)[0]
                exhausted = war["dead"][L] * 1000 >= t["exhaustion_permille"] * max(1, war["pop0"][L] * w.muster(L) // 1000)
                c.true("%s r10 20.19 (7) e%d peace after exhaustion, its loser exhausted" % (label, e[0]),
                       how == "exhaustion" and exhausted)
            # (8) T6: the loser did not win the war's last battle, unless the ending was by aim
            lb = last_battle_winner(res, d, idx)
            c.true("%s r10 20.19 (8) T6 e%d loser c%d is not the last battle's winner (end %s)" % (
                label, e[0], role(e[0], 2)[0], how), how == "aim" or lb != role(e[0], 2)[0])
        if k == K["search_abandoned"]:   # (10) and the truce bookkeeping, no cost
            d = war_of_end.get(e[0])
            war = ends.get(d)
            fails = [x for x in w.search_log if x[1] == d]
            tail = 0
            for x in fails:
                tail = 0 if x[4] else tail + 1
            c.true("%s r10 20.19 (10) e%d after search_years failed searches" % (label, e[0]),
                   war is not None and tail == t["search_years"] and not fails[-1][4])
            c.true("%s r10 20.17 e%d a war given up costs nothing" % (label, e[0]),
                   not any(kind[x] in (K["region_ceded"], K["tribute"]) for x in kids.get(e[0], [])))
            c.eq("%s r10 20.17 e%d years" % (label, e[0]), vals[e[0]][1], e[1] - (war["start"] if war else 0))
        if k == K["refuge_held"]:   # 20.22: caused by a battle its attacker won; never followed by a fall
            cs = causes.get(e[0], [])
            c.true("%s r10 20.22 e%d refuge_held after a battle won by its attacker" % (label, e[0]),
                   len(cs) == 1 and kind[cs[0]] in (K["battle"], K["battle_at_pass"]) and
                   role(cs[0], 1) == role(e[0], 1) and ev[cs[0]][1] == e[1])
            c.true("%s r10 20.22 e%d no fall follows a refuge_held" % (label, e[0]),
                   not any(kind[x] in (K["sacked"], K["settlement_captured"], K["refugees_settled"])
                           for x in kids.get(e[0], [])))
            c.true("%s r10 20.22 e%d reason 1 or 2" % (label, e[0]), vals[e[0]][1] in (1, 2))
            if vals[e[0]][1] == 1:   # T3 from the rows: no free region within reach with capacity and a site
                dests = t3_destinations(res, inp, w, e[0], e[1])
                with_site = [d for d in dests if site_from_rows(res, w, d, civ_race_of(res, role(e[0], 2)[0]), e[0])]
                c.eq("%s r10 T3 e%d reason 1: no destination by the rows" % (label, e[0]), with_site, [])
    # (9) T7 every battle's loser_dead <= F_W x kill_max div 1000 (the forces recomputed in the run)
    c.true("%s r10 20.19 (9) T7 every defeated's dead within the kill bound" % label,
           all(dl <= fw * t["kill_max_permille"] // 1000 for fw, dl in w.kills))
    # (11) no battle or siege against a region whose defender's settlements there were all concealed in a year
    # whose search found nothing (the searches replayed in the run)
    unfound = {(x[0], x[1], x[2]) for x in w.search_log if not x[4]}
    war_of_battle = {}
    for e in res["event"]:
        if e[3] in (K["battle"], K["battle_at_pass"]):
            war_of_battle[e[0]] = causes[e[0]][0]
    for e in res["event"]:
        if e[3] in (K["battle"], K["battle_at_pass"]):
            c.true("%s r10 20.19 (11) e%d battle in a year the search found nothing" % (label, e[0]),
                   (e[1], war_of_battle[e[0]], e[5]) not in unfound)
        if e[3] == K["siege"]:
            b = causes[e[0]][0]
            c.true("%s r10 20.19 (11) e%d siege in a year the search found nothing" % (label, e[0]),
                   (e[1], war_of_battle.get(b), e[5]) not in unfound)
    # 20.20 item 2 with 20.13 item 2: no race ends, whatever the tuning, while last_refuge is 1
    if t["last_refuge"] == 1:
        for a in inp.ages:
            c.true("%s r10 20.20 race %d alive at the present" % (label, a[1]),
                   any(x[2] == a[1] and x[8] is None for x in res["civilization"]))


def site_from_rows(res, w, rid, race, eid):
    """P3-05 6.2 (not relaxed) at event eid from the rows: the settlements founded before eid that had not faded
    by then block (thriving, declining or ruined)."""
    faded = {e[6]: e[0] for e in res["event"] if e[3] == K["ruin_faded"]}
    block = [(s[2], s[3]) for s in res["settlement"] if s[7] < eid and not (s[0] in faded and faded[s[0]] < eid)]
    sp2 = w.t["site_spacing"] ** 2
    n = w.inp.map_size

    def free(x, y):
        return all((bx - x) ** 2 + (by - y) ** 2 >= sp2 for bx, by in block)
    r = w.geo.reg[rid]
    if not w.geo.has_cell_term(race) and w.geo.is_candidate(rid, r["seed_x"], r["seed_y"]) and free(r["seed_x"], r["seed_y"]):
        return True
    return any(free(i % n, i // n) for _v, i in w.geo.candidates(rid, race))


def civ_race_of(res, civ):
    return [c[2] for c in res["civilization"] if c[0] == civ][0]


def hard_bounds(c, res, inp, label):
    """15.5 (and P3-05 15.5's not-empty bound): defects on any seed."""
    standing = sum(1 for s in res["settlement"] if s[10] in (1, 2))
    c.true("%s 15.5 not empty" % label, standing >= 1 and any(x[8] is None for x in res["civilization"]))
    sim = res["_engine"].simulated()
    c.true("%s 15.5 events per 1000 simulated years <= 200000" % label, len(res["event"]) * 1000 <= 200000 * sim)
    c.true("%s 15.5 simulated_permille <= 450" % label, sim * 1000 // inp.params["span"] <= 450)


def share_over(res, inp, w, limit_permille):
    """Years during which one civilization held more than limit of the non-islet land (P3-07 7.5 shares
    from region_control)."""
    land = [r["id"] for r in inp.regions if not w.geo.islet(r["id"])]
    n = len(land)
    if n == 0:
        return 0, 0
    E = inp.params["span"]
    years = set()
    for r in res["region_control"]:
        years.add(r[1])
        if r[2] is not None:
            years.add(r[2])
    years = sorted(years | {E})
    worst = 0
    over = 0
    sl = set(land)
    for y0, y1 in zip(years, years[1:]):
        held = {}
        for r in res["region_control"]:
            if r[0] in sl and r[1] <= y0 and (r[2] is None or r[2] > y0):
                held[r[3]] = held.get(r[3], 0) + 1
        m = max(held.values()) * 1000 // n if held else 0
        worst = max(worst, m)
        if m > limit_permille:
            over += y1 - y0
    return worst, over


# ---------------------------------------------------------------------------
# Golden cases (16.3)
# ---------------------------------------------------------------------------

# (name, seed, size, span, races, detail, civs, intensity): the cases of 16.3 item 19 and 20, and two
# 512 cases of the settings that P3-06 makes matter (--event-intensity, --civs-per-race; 16.2 item 15).
WORLD_CASES = tuple(c + ("auto", "normal") for c in pe.WORLD_CASES) + (
    ("hist-conflict-42-1024", 42, 1024, 6000, "all", "normal", "auto", "normal"),
    ("hist-conflict-turbulent-512", 42, 512, 6000, "all", "normal", "auto", "turbulent"),
    ("hist-conflict-civs4-512", 42, 512, 6000, "all", "normal", "4", "normal"),
)
# r9 20.14 item 8 (generator 0.18.0): the test writer's extra case, a world with flights and found refuges
# (seed 1 at 512, default settings: three flights, every refuge found before the present)
WORLD_CASES_018 = (("hist-conflict-flight", 1, 512, 6000, "all", "normal", "auto", "normal"),)
INPUT_OF = {"hist-conflict-42-1024": "example-42-1024", "hist-conflict-turbulent-512": "geo-seed-42-512",
            "hist-conflict-civs4-512": "geo-seed-42-512", "hist-conflict-flight": "sweep-1-512"}


def world_cases():
    return WORLD_CASES + (WORLD_CASES_018 if pe.at_least("0.18.0") else ())


def world_case(name, seed, size, span, races, detail, inputs_dir, dat=None, civs="auto", intensity="normal"):
    dat = dat or data()
    path = os.path.join(inputs_dir, INPUT_OF.get(name, name) + ".db")
    conn = pchr.open_world(path)
    h, _rows = pchr.world_table_hash(conn, "region")
    if name in pe.P1_06_CASE and pp.FLAGS["dest"]:   # the 0.16.0 inputs (--legacy) predate P1-06 r3
        want = pchr.p1_06_region_hash(pe.P1_06_CASE[name])
        if h != want:
            fatal("%s: table region %s differs from p1-06-regions.txt %s" % (path, h, want))
    meta = dict(conn.execute("SELECT key, value FROM meta").fetchall())
    if int(meta["seed"]) != seed or int(meta["param.map_size"]) != size:
        fatal("%s: seed or map size differs from the case" % path)
    p = pchr.params(span=span, races=pchr.races_of(races, dat.chron), detail=detail, civs=pp.civs_param(civs),
                    intensity=intensity)
    inp, _chron = inputs_for(dat, conn, p, seed, size)
    inp.peoples_layers()
    conn.close()
    res = history_stage(dat, inp)
    res["_region_hash"] = h
    return res


def case_lines(name, res, all_sentences):
    inp = res["_inputs"]
    o = []
    if "_region_hash" in res:
        o.append("%s input_region %s" % (name, res["_region_hash"]))
    h = table_hashes(res)
    for t in TABLES:
        o.append("%s table %s %s" % (name, t, h[t]))
    for line in report_lines(res, inp)[1:]:
        o.append("%s report %s" % (name, line))
    for line in stats_lines(res, inp):
        o.append("%s stats %s" % (name, line))
    o += war_lines(name, res)
    o += tension_lines(name, res)
    if pe.at_least("0.18.0") and all_sentences is not None:
        o += note_lines(name, res, inp)        # P3-07 r8 21.4
        o += summary_lines21(name, res, inp)   # P3-07 r8 21.3
    sents = sentences(res, inp) if all_sentences is not None else []
    for e, text in zip(res["event"], sents):
        if all_sentences or e[3] >= 20:
            o.append("%s e%d %s" % (name, e[0], text))
    return o


HEADER = """\
# P3-06 expansion and conflict golden vectors for generator 0.16.0 (schema 7),
# spec docs/specs/p3-06-expansion-conflict.md r4, generated by
# worldgen/tests/reference/p3_conflict_reference.py golden, an independent
# exact-integer reference written from the spec and the P3-04 and P3-05 seams
# alone (with core_reference.py, p3_names_reference.py, p3_data_reference.py,
# p3_chronology_reference.py, p3_engine_reference.py and
# p3_peoples_reference.py as libraries). The history is the shipped stage of
# 0.16.0: peoples, then conflict (steps T2, T6, Q4), conflict the one tension
# source. Data: the shipped game/data with history.conflict, prowess_permille,
# goblin aggression 600 and the report entries of kinds 20-30 as listed in
# the spec (p3_data_reference.py's listings while the disk holds older
# files). Do not edit by hand; never regenerate to make a C++ test pass.
#
# Format: ASCII, LF, tokens separated by one space; a sentence runs to the
# end of the line.
#   world <case> <seed> <map size> <span> <races> <detail> <civs> <intensity>
#       a world case (16.3 item 19): the four P1 golden cases, the three
#       hist-* cases, hist-conflict-42-1024 (16.3 item 20, the sample of
#       15.2), and hist-conflict-turbulent-512 and hist-conflict-civs4-512
#       (seed 42 at 512 with --event-intensity turbulent, --civs-per-race 4),
#       generated with the shipped data and these settings
#   synthetic <case> <seed> <span> <races> <civs> <intensity>
#       a synthetic geography (16.2 item 15; P3-05's make_synthetic(seed) in
#       p3_peoples_reference.py and p3_05_peoples_support.hpp), map 192,
#       detail normal, the chronology of the shipped data for the case's seed
#   stress <case> <seed> <span> <races> <civs> <intensity>
#       the synthetic geography of <seed> with the shipped data but
#       history.peoples and history.conflict edited past the loader to the
#       reference's STRESS_PEOPLES and STRESS_CONFLICT values:
#         peoples  found_rate_permille 1000, split_min_settlements 2,
#                  split_min_regions 2, split_permille 1000,
#                  crisis_per_generation_permille 300
#         conflict border_friction 600, tick_strain_permille 400,
#                  quiet_strain_permille 100, claim_strain 100, declare_min
#                  100, declare_span 200, declare_permille 600, shelter_years
#                  60, march_hops 3, battle_permille 900, garrison_permille
#                  200, exhaustion_permille 150, war_max_years 25,
#                  truce_years 40
#   norefuge <case> <seed> <span> <races> <civs> <intensity>
#       as stress, with last_refuge 0 and shelter_years 0 (the guards'
#       counterfactual of 9 and question 4)
#   <case> input_region <hex>        P0-04 8 hash of the input's table region
#   <case> table <table> <hex>       the thirteen history tables (P0-04 8)
#   <case> report <line>             [history] after its header (P3-04 11.1
#                                    with P3-05 15.4's keys)
#   <case> stats <line>              the [stats] keys of P3-05 15.4 and P3-06
#                                    15.4, in order
#   <case> war e<w> <start>-<end|open> c<a> c<b> rg<goal> claim e<claim>
#              battles <n> sacked <n> taken <n> <how> <outcome>
#                                    one line per war (16 (b)), declaration
#                                    order: how = exhaustion, aim, no-front,
#                                    length, quiet (7.8), conquest, ended (a
#                                    side ended by peoples), open; outcome =
#                                    won-c<W> (-ceded with a cession), truce,
#                                    or - (no peace event)
#   <case> q<start> <value> e<trigger>
#                                    the tension of every quiet step the
#                                    engine asks (5.4; 16.3 (c))
#   <case> e<id> <sentence>          render_event: every event of the 512 and
#                                    synthetic cases, the events of kinds
#                                    20-30 of the 1024 cases (none of the
#                                    stress cases: their tables carry them)
"""


# ---------------------------------------------------------------------------
# check
# ---------------------------------------------------------------------------

Checker = pchr.Checker


def spec_block(title_prefix):
    spec = open(os.path.join(REPO, "docs", "specs", "p3-06-expansion-conflict.md"), encoding="utf-8").read().split("\n")
    i = [k for k, l in enumerate(spec) if l.startswith(title_prefix)][0]
    a = spec.index("```", i) + 1
    b = spec.index("```", a)
    return spec[a:b]


def spec_war_list():
    spec = open(os.path.join(REPO, "docs", "specs", "p3-06-expansion-conflict.md"), encoding="utf-8").read().split("\n")
    i = [k for k, l in enumerate(spec) if l.startswith("**Every war**")][0]
    out = []
    for l in spec[i + 1:]:
        if l.startswith("- e"):
            out.append(l[2:].replace("–", "-"))
        elif out and not l.strip():
            break
    return out


SAMPLE_END = (
    "c1 Finen (Elves), founded 0: alive: 1 regions, 1 settlements, 1544 people",
    "c2 The Realm of Ithieglie (Elves), founded 0: alive: 1 regions, 4 settlements, 1411 people",
    "c3 The Grork Horde (Goblins), founded 2074: alive: 7 regions, 23 settlements, 53716 people",
    "c4 The Hold of Dulimin (Dwarves), founded 3242: alive: 1 regions, 4 settlements, 14500 people",
    "c5 The Narinthri Throne (Dwarves), founded 3242: alive: 1 regions, 4 settlements, 6267 people",
    "c6 The Teeshti Court (Faeries), founded 4243: alive: 1 regions, 2 settlements, 1615 people",
    "c7 The Warrens of Dribnark (Goblins), founded 4392: ended 4858 by civ_conquered of c3",
    "c8 The Kingdom of Stard (Humans), founded 4969: alive: 10 regions, 21 settlements, 132342 people",
    "c9 The Duchy of Dant (Humans), founded 4969: alive: 2 regions, 7 settlements, 18955 people",
    "c10 The Zak-Neez Mob (Goblins), founded 5097: ended 5656 by civ_conquered of c3",
    "c11 The Kritch-Gux Horde (Goblins), founded 5536: alive: 4 regions, 10 settlements, 11350 people",
    "c12 The Daald Horde (Orcs), founded 5596: alive: 1 regions, 4 settlements, 8920 people",
    "c13 The Warrens of Kug-Nuk (Goblins), founded 5637: alive: 0 regions, 1 settlements, 4535 people",
)


def check_example(c, res):
    """15.2: the sample of seed 42 at 1024 (spec text read from the spec file)."""
    inp = res["_inputs"]
    c.eq("15.2 events", len(res["event"]), 1030)
    c.eq("15.2 simulated years", res["_engine"].simulated(), 1404)
    c.eq("15.2 flare-ups (start, end, trigger)", [(r[3], r[4], r[5]) for r in res["phase"] if r[2] == FLARE],
         [(2949, 2998, 75), (3817, 3866, 150), (4818, 4867, 251), (5294, 5343, 430), (5519, 5568, 473)])
    log = {s_: (v, e) for (s_, v, e, _p) in res["_world"].tension_log}
    c.eq("15.2 strain of the first flare-up's trigger", log[2924], (1025, 75))
    mine = [l.replace(" no victor.", " no victor.") for l in spec_war_lines(res, inp)]
    c.eq("15.2 every war", mine, spec_war_list())
    c.eq("15.2 end state", end_state(res, inp), list(SAMPLE_END))
    chron = {l.split()[1]: l for l in chronicle_lines(res, inp)}
    for l in spec_block("**Sixty lines") + spec_block("**One war at the"):
        c.eq("15.2 chronicle %s" % l.split()[1], chron.get(l.split()[1]), l)
    thr, shown = summary_lines(res, inp)
    c.eq("15.2 summary threshold and events shown", (thr, len(shown)), (46, 155))
    sents = dict(zip((e[0] for e in res["event"]), sentences(res, inp)))
    for text in ("The Grork Horde laid claim to Southwest Wood, held by Finen.",
                 "Finen defeated The Grork Horde in Southwest Wood; 905 of the defeated and 111 of the victors fell.",
                 "The Grork Horde besieged Assill of Finen; it held.",
                 "Finen sacked Groz of The Grork Horde; 540 died and 1261 fled.",
                 "Finen ceded Southwest Wood to The Grork Horde.",
                 "The Grork Horde and The Hold of Dulimin made peace after 17 years of war, neither the victor.",
                 "The Grork Horde took Gob-Skreet from The Warrens of Dribnark; 132 died and 0 fled.",
                 "The Warrens of Dribnark fell to The Grork Horde."):
        c.true("11 rendered sample: %s" % text, text in sents.values())
    k = kind_counts(res)
    c.eq("15.2 two conquered", k.get("civ_conquered"), 2)
    rel = res["relation"]
    enm = [r for r in rel if r[4] == "enmity"]
    gob = [x for x in res["civilization"] if x[2] == 2 and x[7] == 35][0]
    c.true("9 goblins hostile to elves from their first year (enmity c1-c3 since 2074)",
           any(r[1] == 1 and r[3] == gob[0] and r[5] == 2074 for r in enm))


def unit_world(dat=None, regions=3, conflict=None, peoples=None, races=("elf", "goblin"), borders=None, biomes=None):
    """A hand-made World for 16.1: regions R1..Rn in a row (20 x 64 cells each), walkable borders."""
    dat = dat or data()
    regs = [dict(x_min=20 * i, x_max=20 * i + 19, seed_x=20 * i + 1, seed_y=1, cells=1280, walk_cells=1280,
                 lake_cells=0) for i in range(regions)]
    bs = borders if borders is not None else [dict(a=i, b=i + 1, walk_pairs=5, length_m=1000)
                                              for i in range(1, regions)]
    rb = biomes if biomes is not None else [(i + 1, 4, 1280) for i in range(regions)]
    d2 = edited(dat, conflict=conflict, peoples=peoples)
    d2, inp = pp.unit_geo(regs, borders=bs, region_biomes=rb, races=races, dat=d2, n=20 * regions)
    d2.conflict = edited(dat, conflict=conflict).conflict
    for rid in d2.race:
        for f in ("aggression", "prowess", "muster", "conceal", "home"):
            d2.race[rid][f] = dat.race[rid][f]
    inp.biome_keys = dict(BIOME_KEYS)
    w = World(d2, inp, pp.event_catalogue(d2.report))
    w.serial = 10
    w.year = 1000
    return w


def add_civ(w, race, capital, founded=0):
    c = len(w.civs) + 1
    w.events.append(dict(id=len(w.events) + 1, year=0, end_year=0, kind=K["civ_founded"], phase=1, region=1,
                         settlement=0, salience=80, participants=[(1, 1, CIV, c)], values={}, causes=[1], key="x"))
    w.civs.append(dict(id=c, name="C%d" % c, race=race, culture=race, home_region=1, capital=capital,
                       founded_year=founded, founding_event=len(w.events), ended_year=None, ending_event=0, created=0))
    w.hunger[c] = 0
    return c


def add_stl(w, civ, region, kind, pop, x=None):
    sid = len(w.settlements) + 1
    w.events.append(dict(id=len(w.events) + 1, year=0, end_year=0, kind=K["settlement_founded"], phase=1,
                         region=region, settlement=sid, salience=40, participants=[], values={}, causes=[], key="x"))
    w.settlements.append(dict(id=sid, name="S%d" % sid, x=(x if x is not None else 20 * (region - 1) + 2 + sid),
                              y=5, region=region, founder_civ=civ, founded_year=0, founding_event=len(w.events),
                              owner_civ=civ, kind=kind, status=THRIVING, population=pop, ruined_year=None,
                              ruin_event=0, created=0))
    if w.owner[region][0] == 0:
        w.set_owner(region, civ, len(w.events))
    return sid


def w1(conflict=None):
    """16.1's worked example W1."""
    w = unit_world(conflict=conflict)
    ELF, GOB = 1, 2
    add_civ(w, ELF, 1)
    add_civ(w, GOB, 3)
    add_stl(w, 1, 3, TOWN, 2000)
    add_stl(w, 1, 2, VILLAGE, 500)
    add_stl(w, 2, 1, TOWN, 6000)
    w.civ(1)["capital"] = 1
    w.civ(2)["capital"] = 3
    w.geo._sigma[(2, GOB)] = 500
    w.geo._sigma[(3, GOB)] = 600
    w.wanted_override = {1: [], 2: []}
    w.phase, w.phase_kind = 1, OPENING
    return w


def check_w1(c):
    w = w1()
    c.eq("W1 d(1, 2), h", (w.disp(1, 2), w.hostility(1, 2)), (-700, 700))
    c.eq("W1 share(1), fear(1)", (w.share(1), w.fear(1)), (666, 443))
    c.eq("W1 front(2, 1)", w.front(2, 1), [(2, 1, 1), (3, 2, 2)])
    c.eq("W1 force(2), force(1)", (w.force(2), w.force(1)), (480, 400))
    ys = []
    for y in range(1, 51):
        w.year = 1000 + y
        w.strain_update(1, w.ct["tick_strain_permille"], False)
        ys.append(w.strain.get((2, 1), 0))
        if y == 1:
            c.eq("W1 strain per tick year (rate 483 x 25)", w.strain[(2, 1)], 12075)
            c.eq("W1 strain 1 -> 2 (drive 900: rate 22 x 900 div 1000 = 19 ... )", w.strain[(1, 2)],
                 (150 * 150 // 1000 * 900 // 1000 * 1000 // 1000) * 25)
        if y == 24:
            c.true("W1 no claim before the 25th year", (2, 1) not in w.claim)
        if y == 25:
            c.true("W1 claim in the 25th tick year", (2, 1) in w.claim)
            e, r = w.claim[(2, 1)]
            c.eq("W1 claim region R2, cause R2's owner_event", (r, w.events[e - 1]["causes"]), (2, [w.owner[2][1]]))
    c.eq("W1 strain at year 50", w.strain[(2, 1)], 603750)
    c.eq("W1 declaration chance (p 75 permille at s 603, full odds)", w.declare_p(2, 1), 75)
    c.eq("W1 battle chance A 480 D 400 def 1000", w.battle_p(480, 400, w.defence(2, 1, 1)[0]), 545)
    c.eq("W1 cover 0 and no bonus: def 1000", w.defence(2, 1, 1), (1000, 0))
    # the declaration and the battle with forced draws (u = 0 succeeds whenever p > 0)
    for branch in ("sack", "capture", "elves win"):
        w = w1(conflict=dict(battle_permille=1000, sack_base=1000 if branch == "sack" else 0,
                             sack_hostility=0))
        for y in range(1, 51):
            w.year = 1000 + y
            w.strain_update(1, w.ct["tick_strain_permille"], False)
        w.year = 1050
        w.declare(2, 1, False)
        war = w.wars[0]
        c.eq("W1 %s: war goal R2, pop0" % branch, (war["goal"], war["pop0"]), (2, {1: 2500, 2: 6000}))
        c.true("W1 %s: strains reset, claims dropped" % branch, not w.strain.get((2, 1)) and not w.claim)
        w.c_t6()
        c.eq("W1 %s: no battle in the declaration year (7.3)" % branch, war["battles"], 0)
        w.year = 1051
        w.serial += 1
        real = w.draw
        if branch == "elves win":
            w.draw = lambda st, a, b, k: (1 << 64) - 1 if st == "hist.conflict.battle_win" else 0
        else:
            w.draw = lambda st, a, b, k: 0 if st != "hist.conflict.sack" or branch == "sack" else (1 << 64) - 1
        w.c_t6()
        w.draw = real
        ev = [e for e in w.events if e["kind"] >= 21]
        keys = [KEY_OF[e["kind"]] for e in ev]
        if branch == "elves win":
            b = ev[1]
            c.eq("W1 elves win: battle values (dead_W 400 x 80, dead_L 480 x 250)", (keys[:2], b["values"]),
                 (["war_declared", "battle"], {1: 32, 2: 120}))
            c.eq("W1 elves win: no siege", "siege" in keys, False)
            continue
        b = ev[1]
        dw = 32 if FLAGS["loss"] else 38     # r7: min(480, 400) x 80 div 1000 = 32 (0.16.0: 480 x 80 div 1000)
        c.eq("W1 %s: battle won by 2, dead %d and 100" % (branch, dw), ([p[3] for p in b["participants"]], b["values"]),
             ([2, 1], {1: dw, 2: 100}))
        c.eq("W1 %s: siege of s2 fell" % branch, (keys[2], ev[2]["settlement"], ev[2]["values"]),
             ("siege", 2, {1: 1}))
        c.eq("W1 %s: G 345, A2 %d, ps" % (branch, 480 - dw), (w.garrison(dict(w.stl(2), population=480), 1),
                                                               w.siege_p(480 - dw, 345)),
             (345, 564 if FLAGS["loss"] else 561))
        if branch == "sack":
            c.eq("W1 sack: values (144 dead, 336 survivors)", (keys[3], ev[3]["values"]), ("sacked", {1: 144, 2: 336}))
            c.eq("W1 sack: casualties 80 at s1 then 336 fled to s1 (room permitting)", w.stl(1)["population"],
                 min(1920 + 336, max(1920, 2 * w.geo.capacity(3, 1))))
            c.eq("W1 sack: s3 lost dead_W", w.stl(3)["population"], 6000 - dw)
            c.eq("W1 sack: s2 ruined by the sack, R2 released", (w.stl(2)["status"], w.stl(2)["ruin_event"],
                                                                  w.owner[2][0]), (RUINED, ev[3]["id"], 0))
        else:
            c.eq("W1 capture: values (48 dead, 0 fled)", (keys[3], ev[3]["values"]), ("settlement_captured",
                                                                                     {1: 48, 2: 0}))
            c.eq("W1 capture: s2 is 2's with 432, R2 too", (w.stl(2)["owner_civ"], w.stl(2)["population"],
                                                            w.owner[2]), (2, 432, (2, ev[3]["id"])))
        c.eq("W1 %s: the aim is achieved: peace, winner 2, no cession" % branch,
             (keys[4:], [p[3] for p in ev[4]["participants"]]), (["peace"], [2, 1]))
        c.eq("W1 %s: grudge 50 + 150, d -900" % branch, (w.grudge[(1, 2)], w.disp(1, 2)), (200, -900))
        c.eq("W1 %s: the enmity row (housekeeping) and the peace row" % branch, sorted(w.relations),
             [sym_key(1, 2, "enmity"), sym_key(1, 2, "peace")])


def check_units_r7(c):
    """16.1 (r7): the victors' dead from the smaller force; the capture sentence; P3-05 r6 17e's
    note_reduction never sets a decline start."""
    t = data().conflict
    lw, ll = t["loss_winner_permille"], t["loss_loser_permille"]
    for fw, fl, want in ((900, 300, (24, 75)), (300, 900, (24, 225)), (1200, 400, (400 * 80 // 1000, 100))):
        got = (min(fw, fl) * lw // 1000, fl * ll // 1000)
        c.eq("16.1 r7 F_W %d F_L %d: dead_W, dead_L" % (fw, fl), got, want)
    c.eq("16.1 r7 F_W = 3 F_L: dead_W = F_L x 80 div 1000", min(1200, 400) * lw // 1000, 400 * 80 // 1000)
    # the capture sentence with fled 0, 1, 12 (the shipped template through render)
    tmpl = [e for e in data().report if e[0] == "settlement_captured"][0][2]
    if FLAGS["loss"]:
        c.eq("11 r7 capture template (data)", tmpl, SPEC_REPORT[6][2])
    for fled, want in ((0, "The Grork Horde took Tyfie Glade from the Teeshti Court; 135 died."),
                       (1, "The Grork Horde took Tyfie Glade from the Teeshti Court; 135 died and one fled."),
                       (12, "The Grork Horde took Tyfie Glade from the Teeshti Court; 135 died and 12 fled.")):
        c.eq("16.1 r7 capture sentence, fled %d" % fled, capture_sentence(fled), want)
    # note_reduction (casualties) never sets the decline start (P3-05 r6 17e)
    w = w1()
    before = dict(w.decline_start)
    w.casualties(1, 100, len(w.events), None)
    c.eq("P3-05 r6 17e casualties set no decline start", w.decline_start, before)


def capture_sentence(fled):
    """A settlement_captured event (dead 135) of the Grork Horde against the Teeshti Court, rendered."""
    w = w1()
    w.civ(2)["name"] = "The Grork Horde"
    w.civ(1)["name"] = "The Teeshti Court"
    w.stl(2)["name"] = "Tyfie Glade"
    e = w.append("settlement_captured", region=2, settlement=2, participants=(("attacker", CIV, 2),
                                                                             ("defender", CIV, 1)),
                 values=(135, fled), causes=(1,))
    res = pp.rows_of(w, pp.event_catalogue(w.pd.report), [])
    view = pe.view_from_rows(res, [(r["id"], r["name"]) for r in w.inp.regions], w.inp.races, w.inp.gods)
    try:
        return pe.render(view, e)
    except FatalError as x:
        return "render failed: %s" % x


def check_units(c):
    if FLAGS["loss"]:
        check_units_r7(c)
    dat = data()
    # 12.1 tuning
    c.eq("12.1 history.conflict = the listing", dat.conflict, pdr.parsed_spec("history")["conflict"])
    if pe.at_least("0.18.0"):
        c.eq("20.9 conflict_enmity_hops message", tuning_problem(dict(dat.conflict, enmity_hops=1)),
             "history: tuning: enmity_hops must not be below march_hops")
        c.eq("20.9 range of a new field", tuning_problem(dict(dat.conflict, flight_hops=9)),
             "history: tuning: conflict.flight_hops outside its schema range")
    c.eq("12.1 shipped tuning passes", tuning_problem(dat.conflict), None)
    c.eq("16.4 no definition", tuning_problem(None), "history: tuning: the content has no definition history.conflict")
    c.eq("16.4 range", tuning_problem(dict(dat.conflict, march_hops=5)),
         "history: tuning: conflict.march_hops outside its schema range")
    c.eq("16.4 record range", tuning_problem(dict(dat.conflict, levy_permille=dict(dat.conflict["levy_permille"],
                                                                                    city=1001))),
         "history: tuning: conflict.levy_permille.city outside its schema range")
    c.eq("16.4 claim_strain", tuning_problem(dict(dat.conflict, claim_strain=301)),
         "history: tuning: claim_strain must not be above declare_min")
    c.eq("16.4 strain_max", tuning_problem(dict(dat.conflict, strain_max=899)),
         "history: tuning: declare_min + declare_span must not be above strain_max")
    # 11 catalogue and report
    want = [(k, 35 if (k == "claim" and pe.at_least("0.18.0")) else sal,   # r9 20.9: claim 45 -> 35
             "{attacker} took {@settlement} from {defender}; {dead} died and {fled} fled."
             if (k == "settlement_captured" and not FLAGS["loss"]) else tm) for k, sal, tm in SPEC_REPORT]
    c.eq("11 report entries 20-30", [e for e in dat.report[19:30]], want)
    c.eq("11 report_events on the 0.16.0 report", pp.report_events_messages(dat.report), [])
    for k in CONFLICT_KINDS:
        c.eq("11 template of %s parses" % k["key"], pe.template_problem(k, dict((e[0], e[2]) for e in dat.report)[k["key"]]),
             None)
    c.eq("3.3 the five streams and r10 20.17's hist.conflict.search", len(STREAMS), 6)
    # 12.2 race data
    c.eq("12.2 prowess", {dat.race[r]["key"]: dat.race[r]["prowess"] for r in dat.race},
         dict(elf=1600, goblin=800, dwarf=1400, faerie=1200, human=1000, orc=1300))
    c.eq("12.2 goblin aggression 600", dat.race[2]["aggression"], 600)
    # 4.1 camp table for every pair; 4.2 decay and its floor
    w = unit_world(races=("elf", "goblin", "dwarf", "faerie", "human", "orc"), regions=2)
    for r1 in range(1, 7):
        add_civ(w, r1, 0)
    camps = {r[0]: r[5] for r in w.inp.races}
    c.eq("4.1 race camps (P3-03 race.camp)", camps, {1: GOOD, 2: EVIL, 3: NEUTRAL, 4: GOOD, 5: NEUTRAL, 6: EVIL})
    vals = dict(good_good=400, good_neutral=-150, good_evil=-700, neutral_neutral=100, neutral_evil=-250,
                evil_evil=300)
    for a in range(1, 7):
        for b in range(1, 7):
            if a != b:
                c.eq("4.1 camp(%d, %d)" % (a, b), w.camp(a, b),
                     vals[CAMP_FIELD[(min(camps[a], camps[b]), max(camps[a], camps[b]))]])
    w.grudge[(1, 2)] = 30
    w.decay(25)
    c.eq("4.2 decay at L = 25 (12)", w.grudge[(1, 2)], 18)
    w.decay(1)
    c.eq("4.2 decay in a tick year (1 x 50 div 100 = 0)", w.grudge[(1, 2)], 18)
    w.decay(50)
    c.eq("4.2 floor at 0", w.grudge[(1, 2)], 0)
    # 5.1 front: a pass (cost 3) against a two-border walkable path (cost 2), march_hops 1 and 2, islets
    for hops, want in ((2, [(3, 2, 2)]), (1, [(3, 3, 1)])):
        w = unit_world(regions=3, conflict=dict(march_hops=hops),
                       borders=[dict(a=1, b=2, walk_pairs=5, length_m=1000), dict(a=2, b=3, walk_pairs=5, length_m=1000),
                                dict(a=1, b=3, walk_pairs=0, pass_id=7, length_m=1000)])
        add_civ(w, 1, 1)
        add_civ(w, 2, 2)
        add_stl(w, 1, 1, TOWN, 1000)
        add_stl(w, 2, 3, TOWN, 1000)
        c.eq("5.1 front pass vs path at march_hops %d" % hops, w.front(1, 2), want)
        dv, pid = w.defence(3, 1, 2)
        if hops == 1:
            c.eq("6.3 a pass adds pass_defense and gives the pass id", (dv, pid), (2000, 7))
    w = unit_world(regions=3, conflict=dict(march_hops=1))
    add_civ(w, 1, 1)
    add_civ(w, 2, 2)
    add_stl(w, 1, 1, TOWN, 1000)
    add_stl(w, 2, 3, TOWN, 1000)
    c.eq("5.1 march_hops 1: two borders away is out of contact", w.front(1, 2), [])
    w.ct["march_hops"] = 2
    w.version += 1
    c.eq("5.1 march_hops 2: through a free region", w.front(1, 2), [(3, 2, 2)])
    add_civ(w, 3, 4)
    add_stl(w, 3, 2, TOWN, 1000)
    w.version += 1
    c.eq("5.1 through a third civilization's region", w.front(1, 2), [(3, 2, 2)])
    add_stl(w, 1, 3, HAMLET, 100)
    w.version += 1
    c.eq("5.1 a shared region costs 1", w.front(1, 2)[0], (3, 1, 3))
    # ties of (cost, last region): two walkable paths of cost 2 into R4 from R1 through R2 and R3
    w = unit_world(regions=4, borders=[dict(a=1, b=2, walk_pairs=5, length_m=1000), dict(a=1, b=3, walk_pairs=5, length_m=1000),
                                       dict(a=2, b=4, walk_pairs=5, length_m=1000), dict(a=3, b=4, walk_pairs=5, length_m=1000)])
    add_civ(w, 1, 1)
    add_civ(w, 2, 2)
    add_stl(w, 1, 1, TOWN, 1000)
    add_stl(w, 2, 4, TOWN, 1000)
    c.eq("5.1 ties to the smaller last region", w.front(1, 2), [(4, 2, 2)])
    # 6.3 defence: cover, river, fortress
    w = unit_world(regions=2, biomes=[(1, 4, 1280), (2, 3, 640), (2, 4, 640)],
                   borders=[dict(a=1, b=2, walk_pairs=5, river_pairs=40, length_m=1000)])
    add_civ(w, 1, 1)
    add_civ(w, 3, 2)
    add_stl(w, 1, 1, TOWN, 1000)
    add_stl(w, 2, 2, FORTRESS, 1000)
    c.eq("6.3 cover of a half-forest region", w.cover(2), 500)
    c.eq("6.3 def with cover 500, a large river and a fortress", w.defence(2, 1, 2), (1000 + 500 + 500 + 500, 0))
    c.eq("6.3 a shared region has no crossing bonus", w.defence(2, 2, 2), (2000, 0))
    # 5.2 rate left to right, intensity, the cap
    w = w1()
    w.strain_update(1, 25, False)
    w2 = w1()
    w2.intensity = 2000
    w2.strain_update(1, 25, False)
    c.eq("5.2 turbulent doubles the strain", w2.strain[(2, 1)], 2 * w.strain[(2, 1)])
    w2.strain[(2, 1)] = 4999990
    w2.strain_update(1, 25, False)
    c.eq("5.2 strain capped at strain_max x 1000", w2.strain[(2, 1)], 5000000)
    w3 = w1()
    w3.grudge[(1, 2)] = 0
    w3.ct["camp"] = dict(w3.ct["camp"], good_evil=400)
    w3.wanted_override = {1: [], 2: []}
    w3.civs[1]["race"] = 2
    for x in w3.settlements:
        x["owner_civ"] = 2 if x["id"] == 3 else 1
    w3.fear = lambda b: 0
    w3.strain_update(1, 25, False)
    c.eq("5.2 friendly pair (d >= friction, no want or fear) builds no strain", w3.strain.get((2, 1), 0), 0)
    # 5.4 tension: largest strain among claimed pairs, ties by (a, b), read-only, 0 and e0 when none
    w = w1()
    c.eq("5.4 nothing claimed: (0, 0)", w.tension(1000, 25), (0, 0))
    w.contact = {(1, 2): 1, (2, 1): 1}
    w.claim = {(1, 2): (7, 3), (2, 1): (8, 2)}
    w.strain = {(1, 2): 1500000, (2, 1): 1500999}
    c.eq("5.4 tie by (a, b) on strain div 1000", w.tension(1000, 25), (1500, 7))
    w.last_peace[(1, 2)] = (900, 9)
    c.eq("5.4 in truce: ignored", w.tension(1000, 25), (0, 0))
    # 7.2 odds by aggression: full odds at force ratio (1000 - g) / 1000
    for race, g in ((1, 150), (2, 600), (6, 900)):
        w = unit_world(races=("elf", "goblin", "orc"), regions=2)
        add_civ(w, race, 1)
        add_civ(w, 1 if race != 1 else 2, 2)
        add_stl(w, 1, 1, TOWN, 10000)
        add_stl(w, 2, 2, TOWN, 10000)
        w.strain[(1, 2)] = 900000
        fa, fb = w.force(1), w.force(2)
        want = 150 * min(1000, fa * 1000 // max(1, fb * 1000 // 1000 * (1000 - g) // 1000)) // 1000
        c.eq("7.2 declaration chance at full strain, aggression %d" % g, w.declare_p(1, 2), want)
    # 7.4 casualties with a remainder
    w = w1()
    w.casualties(1, 7, 99, None)
    c.eq("7.4 casualties 7 over 2000 and 500: 5 and 1, the remainder 1 from the most populous",
         (w.stl(1)["population"], w.stl(2)["population"]), (2000 - 6, 499))
    c.eq("7.4 note_reduction on every settlement of the side", (w.last_red[1], w.last_red[2]), (99, 99))
    check_w1(c)


# ---------------------------------------------------------------------------
# 20.14: hand-made units of r9 (generator 0.18.0)
# ---------------------------------------------------------------------------

def r9_world(regions=6, races=("elf", "goblin"), conflict=None, peoples=None, biomes=None):
    w = unit_world(regions=regions, races=races, conflict=conflict, peoples=peoples, biomes=biomes)
    w.phase, w.phase_kind = 1, OPENING
    w.wanted_override = {}
    return w


def forced(w, losing=()):
    """Every draw 0 (each chance with p > 0 succeeds) except the streams in `losing` (u = 2^64 - 1)."""
    w.draw = lambda st, a, b, k: (1 << 64) - 1 if st in losing else 0


def claim_war(w, a, b, region, year=1050):
    w.year = year
    e = w.append("claim", region=region, participants=(("claimant", CIV, a), ("holder", CIV, b)), causes=(1,))
    w.claim[(a, b)] = (e, region)
    return w.declare(a, b, False)


def flight_case(pop=2000, cap4=None, owned=(), conflict=None):
    """Elves (c1, a town of 3000 in R1) against the goblins' last refuge (c2, a town in R3), six regions in a
    row; the battle, the siege and the fall forced."""
    w = r9_world(conflict=conflict)
    add_civ(w, 1, 1)
    add_civ(w, 2, 2)
    add_stl(w, 1, 1, TOWN, 3000)
    add_stl(w, 2, 3, TOWN, pop)
    w.civ(1)["capital"], w.civ(2)["capital"] = 1, 2
    for r in owned:
        add_stl(w, 1, r, HAMLET, 50)
    if cap4 is not None:
        w.geo._cap[(4, 2)] = cap4
    forced(w)
    claim_war(w, 1, 2, 3)
    w.c_t6()                       # no battle in the declaration year
    w.year = 1051
    w.serial += 1
    return w


def check_units_r9(c):
    """P3-06 r9 20.14 items 1-7 on hand-made worlds (items 8, 9: the cases)."""
    dat = data()
    t = dat.conflict
    c.eq("20.9 shipped r9 tuning", [(n, t[n]) for n, _v in pdr.CONFLICT_018_VALUES], list(pdr.CONFLICT_018_VALUES))
    c.eq("20.12 race fields", {dat.race[r]["key"]: (dat.race[r]["muster"], dat.race[r]["conceal"], dat.race[r]["home"])
                               for r in dat.race}, pdr.RACE_FIELDS_018_VALUES)
    # --- 1 flight: destination order (-h_x, -sigma, id) with the cap at march_hops + 1
    w = flight_case()
    ts = w.stl(2)
    c.eq("20.1 destination: R4 (h_x capped at 3 for R4, R5, R6; equal sigma; smaller id)",
         w.flight_destination(1, 2, ts)[0], 4)
    w.geo._sigma[(5, 2)] = w.geo.sigma(4, 2) + 1
    c.eq("20.1 destination: the better land among the equally safe", w.flight_destination(1, 2, ts)[0], 5)
    w.ct["march_hops"] = 4
    c.eq("20.1 destination: with march_hops 4 the cap is 5, R6 (h 5) is the farthest", w.flight_destination(1, 2, ts)[0], 6)
    w.ct["march_hops"] = 2
    w.ct["flight_hops"] = 1
    c.eq("20.1 destination: flight_hops 1 leaves R2 and R4", w.flight_destination(1, 2, ts)[0], 4)
    w.ct["flight_hops"] = 3
    w.geo._cap[(5, 2)] = 799
    w.geo._cap[(4, 2)] = 799
    w.geo._cap[(6, 2)] = 799
    c.eq("20.1 destination: capacity below expand_min_capacity excluded", w.flight_destination(1, 2, ts)[0], 2)
    w.geo._cap[(6, 2)] = 800
    c.eq("20.1 destination: capacity at expand_min_capacity allowed", w.flight_destination(1, 2, ts)[0], 6)
    w.geo.q[6]["islet"] = True
    c.eq("20.1 destination: an islet excluded", w.flight_destination(1, 2, ts)[0], 2)
    real_site = w.site
    w.site = lambda rid, race, relaxed: None if rid == 2 else real_site(rid, race, relaxed)
    c.eq("20.1 destination: a candidate without a site is skipped (none left)", w.flight_destination(1, 2, ts), None)
    # --- the flight procedure
    w = flight_case()
    ts = w.stl(2)
    war = w.wars[0]
    n0 = len(w.events)
    w.c_t6()
    ev = w.events[n0:]
    keys = [KEY_OF[e["kind"]] for e in ev]
    c.eq("20.1 flight: battle, siege (fell), sacked, refugees_settled, capital_moved, war_won (aim; r10 20.15 item 4)",
         keys, ["battle", "siege", "sacked", "refugees_settled", "capital_moved", "war_won"])
    c.eq("20.1 flight: battle values (dead_W min(480, 160) x 80, dead_L 160 x 250) and siege fell",
         (ev[0]["values"], ev[1]["values"]), ({1: 12, 2: 40}, {1: 1}))
    c.eq("20.1 flight: sacked dead 588 survivors 1372 (1960 after the battle), salience capital_sacked",
         (ev[2]["values"], ev[2]["salience"], ev[2]["causes"]), ({1: 588, 2: 1372}, 90, [ev[1]["id"]]))
    h = ev[3]["settlement"]
    c.eq("20.1 flight: refugees_settled in R4, settlement h, roles h / c2 / s2, population 1372, cause the sack",
         (ev[3]["region"], h, ev[3]["participants"], ev[3]["values"], ev[3]["causes"]),
         (4, 3, [(1, 1, SETTLEMENT, 3), (2, 2, CIV, 2), (3, 3, SETTLEMENT, 2)], {1: 1372}, [ev[2]["id"]]))
    c.eq("20.1 flight: the refuge (thriving, 1372, owner c2, kind ladder: a town), R4 owned by c2 by the event",
         (w.stl(h)["status"], w.stl(h)["population"], w.stl(h)["owner_civ"], w.stl(h)["kind"], w.owner[4]),
         (THRIVING, 1372, 2, w.ladder(2, 1372), (2, ev[3]["id"])))
    c.eq("20.1 flight: t ruined by the sack, R3 released, capital h, capital_moved caused by the sack",
         (ts["status"], ts["ruin_event"], ts["population"], w.owner[3][0], w.civ(2)["capital"], ev[4]["causes"],
          ev[4]["settlement"]), (RUINED, ev[2]["id"], 0, 0, h, [ev[2]["id"]], h))
    c.eq("20.1 flight: hidden until Y + 300", w.hidden_until, {h: 1351})
    c.eq("20.1 flight: no kind 31", any(KEY_OF[e["kind"]] == "settlement_abandoned_to" for e in w.events), False)
    c.eq("20.1 flight: taken, grudge 700 + 150 + 50? (grudge{1,2} = war_grudge + capture_grudge), wronged",
         (war["taken"][1], w.grudge[(1, 2)], w.wronged), (1, 200, {(2, 1): {3}}))
    c.eq("20.1 flight: the war ends by its aim; the loser, a last refuge, pays nothing",
         (war["end"][1], war["end"][2], war.get("cost"), war.get("no_cost")), ("aim", 1, None, "refuge"))
    c.eq("20.1 civilization 2 is not conquered", w.civ(2)["ended_year"], None)
    # hidden: no presence, no front; found at hidden_until
    w.serial += 1
    c.eq("20.1 hidden: presence(2) empty, front(1, 2) empty", (w.presence(2), w.front(1, 2)), ([], []))
    w.year = 1350
    w.serial += 1
    c.eq("20.1 hidden at Y = hidden_until - 1", w.presence(2), [])
    w.year = 1351
    w.serial += 1
    c.eq("20.1 found at hidden_until: R4 in presence again", w.presence(2), [4])
    # cap at 2C, the remainder lost; capacity at the threshold
    w = flight_case(pop=3000, cap4=800)
    n0 = len(w.events)
    w.c_t6()
    ev = w.events[n0:]
    c.eq("20.1 flight: n = min(2058, 2 x 800); the remainder 458 lost (the refuge is full)",
         ([KEY_OF[e["kind"]] for e in ev][:5], ev[2]["values"], ev[3]["values"], w.stl(3)["population"]),
         (["battle", "siege", "sacked", "refugees_settled", "capital_moved"], {1: 882, 2: 2058}, {1: 1600}, 1600))
    # guard: no destination (every region owned or out of reach)
    w = flight_case(owned=(2, 4, 5, 6))
    n0 = len(w.events)
    w.c_t6()
    ev = w.events[n0:]
    c.eq("20.1, r10 20.22: no destination -> the guard holds: refuge_held, reason 1", ([KEY_OF[e["kind"]] for e in ev][:2],
                                                                    ev[1]["values"], w.guard_no_dest),
         (["battle", "refuge_held"], {1: 1}, 1))
    # guard: survivors below abandon_min
    w = flight_case(pop=56)
    n0 = len(w.events)
    w.c_t6()
    ev = w.events[n0:]
    c.eq("20.1, r10 20.22: 56 - 1 dead = 55, sack 16, 39 < abandon_min 40 -> refuge_held, reason 2",
         ([KEY_OF[e["kind"]] for e in ev][:2], ev[1]["values"], w.guard_too_few), (["battle", "refuge_held"], {1: 2}, 1))
    w = flight_case(pop=57)
    n0 = len(w.events)
    w.c_t6()
    c.eq("20.1 guard: 57 - 1 = 56, sack 16, 40 = abandon_min -> flees",
         [KEY_OF[e["kind"]] for e in w.events[n0:]][:4], ["battle", "siege", "sacked", "refugees_settled"])
    w = flight_case(conflict=dict(refuge_flight=0))
    n0 = len(w.events)
    w.c_t6()
    c.eq("20.1 refuge_flight 0: the guard of 0.16.0", w.events[n0 + 1]["values"], {1: 0})
    check_units_r9_goals(c)
    check_units_r9_odds(c)


def check_units_r9_goals(c):
    # --- 2 goals: b's capital region excluded unless alone; re-choice; dropped
    w = r9_world(regions=4)
    add_civ(w, 2, 1)       # goblins c1 in R1
    add_civ(w, 1, 2)       # elves c2: capital in R2, a village in R3
    add_stl(w, 1, 1, TOWN, 3000)
    add_stl(w, 2, 2, TOWN, 300)
    add_stl(w, 2, 3, VILLAGE, 200)
    w.civ(1)["capital"], w.civ(2)["capital"] = 1, 2
    fr = w.front(1, 2)
    c.eq("20.2 front(1, 2) holds R2 (capital) and R3", [e[0] for e in fr], [2, 3])
    c.eq("20.2 allowed goals: the capital region excluded", [e[0] for e in w.allowed(1, 2, fr)], [3])
    c.eq("20.2 the capital region alone is allowed", [e[0] for e in w.allowed(1, 2, [e for e in fr if e[0] == 2])], [2])
    w.futile[(2, 3)] = (3, 2)
    c.eq("20.4 weary of R3 (count 3, owner unchanged): only the capital region is left", [e[0] for e in w.allowed(1, 2, fr)],
         [2])
    w.futile[(2, 2)] = (3, 2)
    c.eq("20.4 weary of both: no allowed goal", w.allowed(1, 2, fr), [])
    w.futile[(2, 3)] = (3, 1)
    c.eq("20.4 a count recorded for another owner reads 0", [e[0] for e in w.allowed(1, 2, fr)], [3])
    w.futile = {(2, 3): (2, 2)}
    c.eq("20.4 two futile wars are not weariness", [e[0] for e in w.allowed(1, 2, fr)], [3])
    w.ct["weary_wars"] = 0
    w.futile = {(2, 3): (5, 2)}
    c.eq("20.4 weary_wars 0: never weary", [e[0] for e in w.allowed(1, 2, fr)], [3])
    w.ct["weary_wars"] = 3
    w.futile = {}
    # claim key: wronged land still held by b first
    w.strain[(1, 2)] = 400000
    w.contact = {}
    w.year = 1001
    w.geo._sigma[(3, 2)] = 100
    w.geo._sigma[(2, 2)] = 900
    w.strain_update(1, 0, False)
    c.eq("20.2 claim: the allowed goal R3 (not the better capital region)", w.claim[(1, 2)][1], 3)
    # re-choice at the declaration: the claim region became weary
    w.futile[(2, 3)] = (3, 2)
    e = w.declare(1, 2, False)
    c.eq("20.2 declaration re-chooses the only allowed goal (the capital region)", w.events[e - 1]["region"], 2)
    w.wars = []
    w.bookkeeping(w.year, False)
    w.futile[(2, 2)] = (3, 2)
    w.claim[(1, 2)] = (5, 3)
    c.eq("20.2 no allowed goal: the claim is dropped, no war", (w.declare(1, 2, False), (1, 2) in w.claim), (None, False))
    # --- 4 weariness counts: per defender over two attackers, reset on a change of owner
    w = r9_world(regions=4, races=("elf", "goblin"))
    add_civ(w, 2, 1)
    add_civ(w, 2, 4)
    add_civ(w, 1, 2)
    add_stl(w, 1, 1, TOWN, 3000)
    add_stl(w, 2, 4, TOWN, 3000)
    add_stl(w, 3, 2, TOWN, 300)
    add_stl(w, 3, 3, VILLAGE, 200)
    w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 2, 3
    for k, a in enumerate((1, 2, 1)):
        claim_war(w, a, 3, 3, year=1100 + 10 * k)
        war = w.wars[-1]
        w.year += 5
        w.end_war(war, None, "length")
        w.bookkeeping(w.year, False)
        w.last_peace = {}
    c.eq("20.4 three truces over R3 by two attackers: count 3, owner c3", w.futile[(3, 3)], (3, 3))
    c.true("20.4 the defender is weary of R3", w.weary(3, 3))
    w.set_owner(3, 1, 1)
    c.true("20.4 a change of owner resets the count (reads 0)", not w.weary(3, 3))
    w.set_owner(3, 3, 1)
    c.true("20.4 the owner back: the old count reads again (recorded owner c3)", w.weary(3, 3))
    # --- 3 costs
    w = r9_world(regions=5)
    add_civ(w, 2, 1)       # goblins c1: R3
    add_civ(w, 1, 2)       # elves c2: capital R1, a village in R2, a village in R4 (held by c3)
    add_civ(w, 2, 5)       # goblins c3: R4, R5
    add_stl(w, 1, 3, TOWN, 3000)
    add_stl(w, 2, 1, TOWN, 400)
    add_stl(w, 2, 2, VILLAGE, 200)
    add_stl(w, 3, 4, HAMLET, 100, x=70)
    add_stl(w, 2, 4, VILLAGE, 200)
    add_stl(w, 3, 5, HAMLET, 100)
    w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 2, 4
    c.eq("20.2 the claim region R4 (not held by c2) is an allowed goal", claim_war(w, 1, 2, 4) is not None, True)
    war = w.wars[-1]
    c.eq("20.2 the war's goal R4", war["goal"], 4)
    w.year = 1060
    c.eq("20.3 attacker wins: no 7.7 cession (R4 is not c2's); cede the best non-capital region c2 holds in front (R2)",
         w.cost_plan(war, 1), ("cede", 2))
    n0 = len(w.events)
    w.end_war(war, 1, "length")
    ev = w.events[n0:]
    c.eq("20.3 peace then region_ceded R2 from c2 to c1, caused by the peace",
         ([KEY_OF[e["kind"]] for e in ev], ev[1]["region"], ev[1]["participants"], ev[1]["causes"]),
         (["peace", "region_ceded"], 2, [(1, 1, CIV, 2), (2, 2, CIV, 1)], [ev[0]["id"]]))
    c.eq("20.3 R2 and its village pass to c1; wronged(2, 1) gains R2", (w.owner[2][0], w.stl(3)["owner_civ"],
                                                                         w.wronged.get((2, 1))), (1, 1, {2}))
    # the defender wins: the attacker loses its best non-capital region in the defender's reach (R2)
    w.last_peace = {}
    w.bookkeeping(w.year, False)
    claim_war(w, 1, 2, 3, year=1300)
    war = w.wars[-1]
    w.year = 1310
    c.eq("20.3 defender wins: the attacker cedes R2 (in front(2, 1), not its capital region R3)", w.cost_plan(war, 2),
         ("cede", 2))
    # tribute when nothing can be ceded: elves with their capital only in reach
    w = r9_world(regions=3)
    add_civ(w, 2, 1)
    add_civ(w, 1, 3)
    add_civ(w, 1, 4)          # a second elven realm, far: c2 is not its race's last refuge
    add_stl(w, 1, 1, TOWN, 3000)
    add_stl(w, 2, 3, TOWN, 400)
    add_stl(w, 3, 3, HAMLET, 100)
    w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 2, 3
    w.owner[3] = (2, 1)
    claim_war(w, 1, 2, 3)
    war = w.wars[-1]
    w.year = 1060
    c.eq("20.3 nothing to cede: tribute of 400 x 100 div 1000", w.cost_plan(war, 1), ("tribute", 40))
    n0 = len(w.events)
    w.end_war(war, 1, "length")
    ev = w.events[n0:]
    c.eq("20.3 tribute event (no region; payer c2, payee c1; people 40; cause the peace) and the casualties",
         ([KEY_OF[e["kind"]] for e in ev], ev[1]["region"], ev[1]["participants"], ev[1]["values"], ev[1]["causes"],
          w.stl(2)["population"], w.last_red[2]),
         (["peace", "tribute"], 0, [(1, 1, CIV, 2), (2, 2, CIV, 1)], {1: 40}, [ev[0]["id"]], 360, ev[1]["id"]))
    # no tribute from a last refuge; a truce costs nothing
    w = r9_world(regions=3)
    add_civ(w, 2, 1)
    add_civ(w, 1, 3)
    add_stl(w, 1, 1, TOWN, 3000)
    add_stl(w, 2, 3, TOWN, 400)
    w.civ(1)["capital"], w.civ(2)["capital"] = 1, 2
    claim_war(w, 1, 2, 3)
    war = w.wars[-1]
    w.year = 1060
    c.eq("20.3 no tribute from a last refuge", (w.cost_plan(war, 1), war.get("no_cost")), (None, "refuge"))
    n0 = len(w.events)
    w.end_war(war, None, "length")
    c.eq("20.3 a truce costs nothing", [KEY_OF[e["kind"]] for e in w.events[n0:]], ["truce"])
    # the quiet step waits for the loser's budget
    w = r9_world(regions=3)
    add_civ(w, 2, 1)
    add_civ(w, 1, 3)
    add_civ(w, 1, 4)
    add_stl(w, 1, 1, TOWN, 3000)
    add_stl(w, 2, 3, TOWN, 400)
    add_stl(w, 3, 3, HAMLET, 100)
    w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 2, 3
    w.owner[3] = (2, 1)
    claim_war(w, 1, 2, 3)
    war = w.wars[-1]
    war["dead"][2] = 10                # c1 leads on the score (r10 20.15: dead inflicted)
    w.year, w.years = 1100, 25
    w.quiet_used = {2: w.eng["quiet_events_max"]}
    w.housekeeping = lambda: None
    w.strain_update = lambda *a: None
    w.c_q4()
    c.true("20.3 Q4: a tribute end waits while the loser has no budget", war["open"])
    w.quiet_used = {}
    w.c_q4()
    c.eq("20.3 Q4: with budget the war ends with its tribute", (war["open"], war.get("cost")), (False, "tribute"))


def check_units_r9_odds(c):
    # --- 6 muster and local force
    w = r9_world(regions=6, races=("elf", "goblin", "orc"))
    add_civ(w, 6, 1)       # orcs c1: towns in R1 and R5
    add_civ(w, 1, 2)
    add_stl(w, 1, 1, TOWN, 1000)
    add_stl(w, 1, 5, TOWN, 1000)
    add_stl(w, 2, 2, TOWN, 1000)
    c.eq("20.6 force(orc) = levy 200 x 1300 div 1000 x 4000 div 1000", w.force(1), 200 * 1300 // 1000 * 4000 // 1000)
    c.eq("20.6 local force at R2: R1 (1 hop) and R5 (3 hops = levy_hops) fight",
         w.local_force(1, 2), 200 * 1300 // 1000 * 4000 // 1000)
    c.eq("20.6 local force at R1: R5 is 4 hops away and stays home", w.local_force(1, 1), 100 * 1300 // 1000 * 4)
    c.eq("20.6 local force of the elves at R6 (4 hops): none", w.local_force(2, 6), 0)
    war = dict(a=1, b=2, dead={1: 0, 2: 0}, pop0={1: 2000, 2: 1000}, won={1: 0, 2: 0}, taken={1: 0, 2: 0})
    war["dead"][1] = 2000 * 4 * 80 // 1000 - 1
    c.true("20.6 exhaustion reads pop0 x muster: 639 dead of 2000 orcs is not exhaustion",
           not (war["dead"][1] * 1000 >= w.ct["exhaustion_permille"] * max(1, 2000 * w.muster(1) // 1000)))
    war["dead"][1] += 1
    c.true("20.6 ... 640 is", war["dead"][1] * 1000 >= w.ct["exhaustion_permille"] * max(1, 2000 * w.muster(1) // 1000))
    # --- odds: defence of the claimed region, the floor, odds_min (no draw below)
    w = r9_world(regions=2, races=("elf", "goblin", "orc"),
                 biomes=[(1, 4, 1280), (2, 3, 1280)])     # R2 all temperate forest
    add_civ(w, 2, 1)       # goblins c1
    add_civ(w, 1, 2)       # elves c2
    add_stl(w, 1, 1, TOWN, 10000)
    add_stl(w, 2, 2, TOWN, 10000)
    w.claim[(1, 2)] = (1, 2)
    fa, fb = w.force(1), w.force(2)
    dv = w.defence(2, 1, 2)[0]
    b_el = w.bterm(2, 1)
    c.eq("20.7 def(R2) = 1000 + terrain 1000 x cover 1000 + home 1500 x B(R2, elf) 500", (dv, b_el),
         (1000 + 1000 + 1500 * 500 // 1000, 500))
    want = min(1000, fa * 1000 // max(1, fb * dv // 1000 * 1000 // 1000 * max(400, 500) // 1000))
    c.eq("20.6 odds of goblins (g 600, floor 500) on forest elves", w.odds(1, 2), want)
    w.strain[(1, 2)] = 900000
    c.eq("20.6 odds below odds_min: no declaration chance (None, no draw)", w.declare_p(1, 2), None if want < 700 else
         150 * want // 1000)
    for om, ok in ((want, True), (want + 1, False)):
        w.ct["odds_min"] = om
        c.eq("20.6 odds_min %s the odds" % ("at" if ok else "above"), w.declare_p(1, 2) is not None, ok)
    w.ct["odds_min"] = 700
    # --- 5 revenge: g' in the rate and in the odds
    w = r9_world(regions=2, races=("elf", "goblin"))
    add_civ(w, 2, 1)
    add_civ(w, 1, 2)
    add_stl(w, 1, 1, TOWN, 10000)
    add_stl(w, 2, 2, TOWN, 1000)
    c.eq("20.5 g'(elves, goblins) without a wrong: 150", w.aggression(2, 1), 150)
    w.wronged[(2, 1)] = {3}
    c.eq("20.5 wronged but no grudge: still 150", w.aggression(2, 1), 150)
    w.grudge[(1, 2)] = 1
    c.eq("20.5 wronged and grudge > 0: 400", w.aggression(2, 1), 400)
    w.claim[(2, 1)] = (1, 1)
    c.eq("20.5 odds with g' 400: 160 x 1000 div (800 x 600 div 1000)", w.odds(2, 1), 160 * 1000 // 480)
    w.wronged = {}
    c.eq("20.5 odds with g 150: 160 x 1000 div (800 x 850 div 1000)", w.odds(2, 1), 160 * 1000 // 680)
    w.wronged[(2, 1)] = {3}
    w.strain[(2, 1)] = 0
    w.contact = {}
    w.wanted_override = {1: [], 2: []}
    w.strain_update(1, 25, False)
    s400 = w.strain.get((2, 1), 0)
    w.wronged = {}
    w.strain[(2, 1)] = 0
    w.strain_update(1, 25, False)
    c.true("20.5 g' 400 in the rate: more strain than g 150 (%d against %d)" % (s400, w.strain.get((2, 1), 0)),
           s400 > w.strain.get((2, 1), 0))
    # --- 6 directional truce: orc 50 years, goblin 200, elf 200
    w = r9_world(regions=2, races=("elf", "goblin", "orc"))
    add_civ(w, 6, 1)
    add_civ(w, 2, 2)
    add_civ(w, 1, 3)
    c.eq("20.6 truce lengths orc, goblin, elf", (w.truce_length(1), w.truce_length(2), w.truce_length(3)), (50, 200, 200))
    w.last_peace[(1, 2)] = (1000, 9)
    c.eq("20.6 directional: the orcs past it at 1050, the goblins not", (w.past_truce(1, 2, 1049), w.past_truce(1, 2, 1050),
                                                                        w.past_truce(2, 1, 1199), w.past_truce(2, 1, 1200)),
         (False, True, False, True))
    # --- 20.8 enmity hops in front for an enmity pair only
    w = r9_world(regions=4, races=("elf", "goblin", "human"))
    add_civ(w, 2, 1)
    add_civ(w, 1, 2)
    add_civ(w, 5, 3)
    add_stl(w, 1, 1, TOWN, 1000)
    add_stl(w, 2, 4, TOWN, 1000)
    add_stl(w, 3, 4, HAMLET, 100, x=70)
    c.eq("20.8 three borders apart, no enmity row: no front", w.front(1, 2), [])
    w.enmity_opened.add((1, 2))
    w.version += 1
    c.eq("20.8 with an enmity row: enmity_hops 3 reach R4", w.front(1, 2), [(4, 3, 3)])
    c.eq("20.8 a neutral pair keeps march_hops", w.front(1, 3), [])
    # --- 7 concealment at B 399 / 400, garrison with home defence, a concealed settlement never besieged
    w = r9_world(regions=2, races=("goblin", "faerie"), biomes=[(1, 4, 1280), (2, 3, 640), (2, 4, 640)])
    add_civ(w, 2, 1)
    add_civ(w, 4, 2)
    add_stl(w, 1, 1, TOWN, 1000)
    add_stl(w, 2, 2, VILLAGE, 500)
    w.civ(2)["capital"] = 2
    b_ = w.bterm(2, 4)
    c.eq("20.7 B(R2, faerie): 800 x 500 + (-300) x 500, div 1000", b_, 250)
    for cmin, hid in ((b_ + 1, False), (b_, True)):
        w.pd.race[4]["conceal"] = cmin
        w.version += 1
        # r10 20.17: a concealed settlement stays in presence (it must be found)
        c.eq("20.7 conceal_min %d: concealed %s, in presence" % (cmin, hid), (w.concealed(w.stl(2)), w.presence(2)),
             (hid, [2]))
    w.pd.race[4]["conceal"] = 400
    w.version += 1
    g0 = w.stl(2)["population"] * 300 // 1000 * 1200 // 1000 * (1500 + 2000) // 1000
    c.eq("20.7, P3-01 r10 3.3: faerie home defence 0, the garrison unchanged", w.garrison(w.stl(2), 2), g0)
    w.pd.race[4]["home"] = 2000
    c.eq("20.7 garrison x (1000 + home 2000 x B 250 div 1000) div 1000", w.garrison(w.stl(2), 2), g0 * 1500 // 1000)
    w.pd.race[4]["home"] = 0
    check_units_r10(c)
def r10_war(regions=3, goblin_pop=3000, elf_pop=3000, conflict=None, elf_biome=4):
    """Goblins c1 (a town in R1) at war with elves c2 (a town in R3, and a second elven realm c3 in R2 so that c2
    is not its race's last refuge); the war declared in 1050."""
    w = r9_world(regions=regions, conflict=conflict, biomes=[(i + 1, elf_biome if i == 2 else 4, 1280)
                                                             for i in range(regions)])
    add_civ(w, 2, 1)
    add_civ(w, 1, 2)
    add_civ(w, 1, 3)
    add_stl(w, 1, 1, TOWN, goblin_pop)
    add_stl(w, 2, 3, TOWN, elf_pop)
    add_stl(w, 3, 2, HAMLET, 100)
    w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 2, 3
    claim_war(w, 1, 2, 3)
    return w, w.wars[-1]


def check_units_r10(c):
    """P3-06 r10 20.19 tests: the score, the exhausted leader, the last battle, peace against war_won, the kill
    bound, the search, the last refuge's floor."""
    # --- 20.15 item 1: the score of dead and people taken
    w, war = r10_war()
    war["dead"] = {1: 300, 2: 500}
    war["taken_pop"] = {1: 0, 2: 150}
    c.eq("20.15 s = (dead(b) - dead(a)) + (taken(a) - taken(b)) = 200 - 150", w.score(war), 50)
    war["taken_pop"] = {1: 0, 2: 200}
    c.eq("20.15 s = 0: a truce", (w.score(war), w.by_score(war)), (0, None))
    war["taken_pop"] = {1: 0, 2: 0}
    war["won"] = {1: 0, 2: 9}
    c.eq("20.15 battles won do not count", w.by_score(war), 1)
    # --- items 2 and 4: the exhausted leader wins with war_won; an exhausted loser sues for peace
    w, war = r10_war()
    w.year = 1060
    war["pop0"] = {1: 3000, 2: 3000}
    war["dead"] = {1: 240, 2: 100}      # c1 exhausted (240 x 1000 >= 80 x 3000), behind on the score
    n0 = len(w.events)
    w.end_check(war)
    c.eq("20.15 item 4: the exhausted side behind sues for peace (peace, loser c1)",
         ([KEY_OF[e["kind"]] for e in w.events[n0:]][:1], w.events[n0]["participants"][1][3]), (["peace"], 1))
    w, war = r10_war()
    w.year = 1060
    war["pop0"] = {1: 3000, 2: 30000}
    war["dead"] = {1: 240, 2: 900}      # c1 exhausted but leads (900 - 240 > 0); c2 not exhausted
    n0 = len(w.events)
    w.end_check(war)
    c.eq("20.15 item 2: the exhausted leader wins: war_won, winner c1",
         ([KEY_OF[e["kind"]] for e in w.events[n0:]][:1], w.events[n0]["participants"][0][3]), (["war_won"], 1))
    # --- item 3: the last battle's winner does not lose an ending decided by the score
    war2 = None
    w, war2 = r10_war()
    w.year = 1060
    war2["pop0"] = {1: 3000, 2: 30000}
    war2["dead"] = {1: 240, 2: 900}
    war2["last_battle_winner"] = 2      # the score's loser c2 won the last battle
    n0 = len(w.events)
    w.end_check(war2)
    c.eq("20.15 item 3: the score's loser won the last battle: a truce", [KEY_OF[e["kind"]] for e in w.events[n0:]],
         ["truce"])
    w, war2 = r10_war()
    w.year = 1050 + w.ct["war_max_years"]
    war2["dead"] = {1: 0, 2: 10}
    war2["last_battle_winner"] = 2
    n0 = len(w.events)
    w.end_check(war2)
    c.eq("20.15 item 3 at the end by length: a truce", [KEY_OF[e["kind"]] for e in w.events[n0:]], ["truce"])
    w, war2 = r10_war()
    w.year = 1060
    war2["last_battle_winner"] = 2
    s2 = w.stl(2)
    w.ruin(s2["id"], len(w.events))     # the goal R3 lost: the aim is achieved whatever the last battle
    n0 = len(w.events)
    w.end_check(war2)
    c.eq("20.15 item 3: the aim exception (war_won, winner the attacker)",
         ([KEY_OF[e["kind"]] for e in w.events[n0:]][:1], w.events[n0]["participants"][0][3]), (["war_won"], 1))
    w, war2 = r10_war()
    w.year = 1050 + w.ct["war_max_years"]
    war2["dead"] = {1: 0, 2: 10}
    n0 = len(w.events)
    w.end_check(war2)
    c.eq("20.15 item 4: the end by length with a winner is a war_won", [KEY_OF[e["kind"]] for e in w.events[n0:]][:1],
         ["war_won"])
    # --- 20.16: the kill bound with F_W = F_L / 20
    w, war = r10_war(goblin_pop=60000, elf_pop=1000)
    forced(w, losing=("hist.conflict.battle_win",))   # the defenders (elves) win
    w.year = 1051
    w.serial += 1
    n0 = len(w.events)
    w.c_t6()
    b = w.events[n0]
    fw = 1000 * 100 // 1000 * 1600 // 1000      # elven local force at R3
    fl = 60000 * 100 // 1000 * 800 // 1000      # goblin local force (R1 is 2 hops from R3)
    c.eq("20.16 F_W = F_L / 20: dead_L = min(F_L x 250, F_W x 500) div 1000", (KEY_OF[b["kind"]], b["values"][2]),
         ("battle", min(fl * 250 // 1000, fw * 500 // 1000)))
    c.true("20.16 the bound binds here", fw * 500 // 1000 < fl * 250 // 1000)
    # --- 20.17: the search (elves in forest made concealable)
    for find, found in ((1000, True), (0, False)):
        w, war = r10_war(conflict=dict(find_permille=find), elf_biome=3)
        w.pd.race[1]["conceal"] = w.bterm(3, 1)
        w.year = 1051
        w.serial += 1
        forced(w)
        n0 = len(w.events)
        w.c_t6()
        keys = [KEY_OF[e["kind"]] for e in w.events[n0:]]
        c.eq("20.17 find_permille %d: found %s" % (find, found), ("battle" in keys, war.get("unfound", 0)),
             (found, 0 if found else 1))
    w, war = r10_war(conflict=dict(find_permille=0), elf_biome=3)
    w.pd.race[1]["conceal"] = w.bterm(3, 1)
    forced(w)
    for y in (1051, 1052):
        w.year = y
        w.serial += 1
        w.c_t6()
    c.eq("20.17 two failed searches: unfound 2, the war open", (war["unfound"], war["open"]), (2, True))
    w.ct["find_permille"] = 1000
    forced(w, losing=("hist.conflict.battle",))    # found, then no battle this year
    w.year = 1053
    w.serial += 1
    w.c_t6()
    c.eq("20.17 a find resets unfound", war.get("unfound"), 0)
    forced(w)
    w.ct["find_permille"] = 0
    for y in (1054, 1055):
        w.year = y
        w.serial += 1
        w.c_t6()
    c.true("20.17 still open after two more failures", war["open"])
    w.year = 1056
    w.serial += 1
    n0 = len(w.events)
    w.c_t6()
    ev = w.events[n0:]
    c.eq("20.17 the end at search_years: search_abandoned (R3, searcher c1, hidden c2, years 6), no cost",
         ([KEY_OF[e["kind"]] for e in ev], ev[0]["region"], [p_[3] for p_ in ev[0]["participants"]], ev[0]["values"],
          war["open"]), (["search_abandoned"], 3, [1, 2], {1: 6}, False))
    c.eq("20.17 the truce bookkeeping: war row closed, peace row, last_peace, grudge", (
        rel_key(1, 2, "war") in w.relations, sym_key(1, 2, "peace") in w.relations, w.last_peace[(1, 2)],
        w.grudge.get((1, 2))), (False, True, (1056, ev[0]["id"]), w.ct["war_grudge"]))
    w, war = r10_war(conflict=dict(find_permille=0), elf_biome=3)
    w.pd.race[1]["conceal"] = w.bterm(3, 1)
    add_stl(w, 2, 3, HAMLET, 100)
    w.stl(4)["owner_civ"] = 2
    w.pd.race[1]["conceal"] = w.bterm(3, 1)
    real = w.concealed
    w.concealed = lambda s_: False if s_["id"] == 4 else real(s_)
    forced(w)
    w.year = 1051
    w.serial += 1
    n0 = len(w.events)
    w.c_t6()
    c.true("20.17 a region with a settlement not concealed needs no search", "battle" in
           [KEY_OF[e["kind"]] for e in w.events[n0:]] and not w.search_log)
    # --- 20.20 item 1: the last refuge's floor
    w, war = r10_war()
    w.civs[2]["ended_year"] = 1000      # c2 is now its race's last refuge
    s2 = w.stl(2)
    s2["population"] = 100
    got = w.casualties(2, 90, len(w.events), war)
    c.eq("20.20 item 1: dead = min(90, 100 - abandon_min 40); the war's dead add 60", (got, s2["population"],
                                                                                   war["dead"][2]), (60, 40, 60))
    c.eq("20.20 item 1: below abandon_min nothing is taken", w.casualties(2, 10, len(w.events), war), 0)
    # --- P3-05 21 item 6: the last refuge is never abandoned below abandon_min
    s2["population"] = 30
    w.abandon_b()
    c.eq("P3-05 21 item 6: the refuge below abandon_min stays standing", s2["status"], THRIVING)


def check_peoples_only(c, dat):
    """At 0.16.0 the peoples-only configuration is p3_peoples_reference's run (its golden file)."""
    for seed in (0, 3, 5):
        inp, _g = pp.synthetic_inputs(seed, dat)
        a = pp.history_stage(dat, inp)
        inp2, _g = pp.synthetic_inputs(seed, dat)
        inp2.biome_keys = dict(BIOME_KEYS)
        b = history_stage(dat, inp2, conflict=False)
        c.eq("syn-%d peoples-only configuration = the P3-05 reference" % seed, table_hashes(b), table_hashes(a))


def check_prefix(c, res, pres, label):
    """16.2 item 18a (r7, the 0.16.0 validation's F1): below the first claim the log with conflict equals
    the peoples-only log row for row in event, event_participant, event_cause and event_value, except the
    quiet_era events open in the quiet step of the first claim, whose end_year, years and generations may be
    smaller. A world with no claim equals the peoples-only world in every history table but the three
    catalogue tables and relation (READING: conflict's housekeeping opens enmity rows without a claim).
    Returns the number of excepted eras."""
    claims = [e[0] for e in res["event"] if e[3] == K["claim"]]
    if not claims:
        for t in TABLES:
            if t in ("event_kind", "event_kind_role", "event_kind_value", "relation"):
                continue
            c.eq("%s 18a no claim: table %s equals the peoples-only one" % (label, t), res[t], pres[t])
        return 0
    first = claims[0]
    cy = res["event"][first - 1][1]

    def by(t, n):
        out = {}
        for r in t:
            if r[0] < n:
                out.setdefault(r[0], []).append(r)
        return out
    pe_ = {e[0]: e for e in pres["event"] if e[0] < first}
    ce_ = {e[0]: e for e in res["event"] if e[0] < first}
    excepted = set()
    ok_events = len(pe_) == len(ce_)
    for i, a in ce_.items():
        b = pe_.get(i)
        if a == b:
            continue
        if b is not None and a[3] == K["quiet_era"] and a[:2] == b[:2] and a[3:] == b[3:] and cy <= a[2] <= b[2]:
            excepted.add(i)
            continue
        ok_events = False
        print("  %s 18a first difference at e%d: %r against peoples-only %r" % (label, i, a, b))
        break
    c.true("%s 18a events below the first claim e%d" % (label, first), ok_events)
    for t in ("event_participant", "event_cause"):
        c.eq("%s 18a %s below the first claim" % (label, t), by(res[t], first), by(pres[t], first))
    va, vb = by(res["event_value"], first), by(pres["event_value"], first)
    for i in excepted:
        va.pop(i, None)
        vb.pop(i, None)
    c.eq("%s 18a event_value below the first claim (excepted eras aside)" % label, va, vb)
    return len(excepted)


def all_cases(inputs, quick=False):
    """(header line, name, res, all sentences) for every golden case, in file order."""
    dat = data()
    if os.path.isdir(inputs):
        for name, seed, size, span, races, detail, civs, intensity in world_cases():
            if quick and size > 512 and name != "hist-conflict-42-1024":
                continue
            res = world_case(name, seed, size, span, races, detail, inputs, dat, civs, intensity)
            yield ("world %s %d %d %d %s %s %s %s" % (name, seed, size, span, races, detail, civs, intensity), name, res,
                   size <= 512)
    for seed in (SYN_SEEDS[:6] if quick else SYN_SEEDS):
        span, races, civs, intensity = synthetic_case(seed)
        inp, _g = synthetic_inputs(seed, dat)
        res = history_stage(dat, inp)
        name = "syn-%d" % seed
        yield "synthetic %s %d %d %s %s %s" % (name, seed, span, races, civs, intensity), name, res, True
    sdat = stress_data(dat)
    for seed in (STRESS_SEEDS[:6] if quick else STRESS_SEEDS):
        span, races, civs, intensity = STRESS_CASES[seed]
        inp, _g = synthetic_inputs(seed, sdat, STRESS_CASES[seed])
        res = history_stage(sdat, inp)
        name = "stress-%d" % seed
        yield "stress %s %d %d %s %s %s" % (name, seed, span, races, civs, intensity), name, res, None
    ndat = norefuge_data(dat)
    for seed in (NOREFUGE_SEEDS[:1] if quick else NOREFUGE_SEEDS):
        span, races, civs, intensity = STRESS_CASES[seed]
        inp, _g = synthetic_inputs(seed, ndat, STRESS_CASES[seed])
        res = history_stage(ndat, inp)
        name = "norefuge-%d" % seed
        yield "norefuge %s %d %d %s %s %s" % (name, seed, span, races, civs, intensity), name, res, None


def header():
    if not pe.at_least("0.18.0"):
        return HEADER.rstrip("\n")
    old = HEADER.rstrip("\n").splitlines()
    return HEADER_018 + "#\n" + "\n".join(old[[i for i, l in enumerate(old) if l.startswith("# Format:")][0]:])


HEADER_018 = """\
# P3-06 expansion and conflict golden vectors for generator 0.18.0 (schema 7),
# spec docs/specs/p3-06-expansion-conflict.md r9 (section 20, P3-09 conflict
# balance, docs/specs/p3-09-conflict-balance.md r2) with r7, P3-05 r8 and
# P3-07 r8 21 (the stats, notes and summary lines), generated by
# worldgen/tests/reference/p3_conflict_reference.py golden, an independent
# exact-integer reference written from the specs alone (with core_reference.py,
# p3_names_reference.py, p3_data_reference.py, p3_chronology_reference.py,
# p3_engine_reference.py and p3_peoples_reference.py as libraries). The
# history is the shipped stage of 0.18.0: peoples, then conflict (steps T2,
# T6, Q4), conflict the one tension source. Data: the shipped game/data with
# the 0.18.0 race, history.conflict, history.peoples and history.report
# values (p3_data_reference.py's listings while the disk holds older files).
# Do not edit by hand; never regenerate to make a C++ test pass.
#
# 0.18.0 items: the world case hist-conflict-flight (seed 1 at 512, three
# flights; 20.14 item 8); war outcome won-c<W>-ceded for a cession of 7.7 or
# 20.3, won-c<W>-tribute for a tribute (20.3); [stats] gains the six keys of
# P3-07 r8 21.5 after wars_attacker_<race>; for the world and synthetic cases
#   <case> note e<id> <note>         history_note (P3-07 r8 21.4) of every
#                                    war_declared and flare_up with a note
#   <case> summary threshold <T> shown <n> ages <n1,...>
#                                    the summary set of P3-07 r8 21.3: T, its
#                                    size and the events shown per age
"""


def golden_text(inputs, quick=False):  # noqa: F811 (one pass over all_cases)
    o = [header()]
    for head, name, res, alls in all_cases(inputs, quick):
        o.append(head)
        o += case_lines(name, res, alls)
    return "\n".join(o) + "\n"


def cmd_check(argv):
    inputs = argv[argv.index("--inputs") + 1] if "--inputs" in argv else DEFAULT_INPUTS
    quick = "--quick" in argv
    c = Checker()
    dat = data()
    with rules("0.17.0" if pe.at_least("0.17.0") else "0.16.0"):
        check_units(c)          # 16.1 (r4, r7) on the rules they were written for
    if pe.at_least("0.18.0"):
        check_units_r9(c)       # 20.14
    check_peoples_only(c, dat)
    o = [header()]
    reach = {}
    guards = dict(refuge=0, shelter=0)
    stress = {}
    for head, name, res, alls in all_cases(inputs, quick):
        inp = res["_inputs"]
        w = res["_world"]
        known = pe.at_least("0.18.0") and name in KNOWN_EXTINCT_018
        check_invariants(c, res, inp, w, name, last_refuge=w.ct["last_refuge"] == 1 and not known)
        if known:
            extinct = [a[1] for a in inp.ages if not any(x[2] == a[1] and x[8] is None for x in res["civilization"])]
            print("finding: %s: race(s) %s extinct with last_refuge 1 (P3-06 r9 20.13 item 2; reported)" % (name, extinct))
        hard_bounds(c, res, inp, name)
        if name.startswith("norefuge"):
            extinct = [a[1] for a in inp.ages if not any(x[2] == a[1] and x[8] is None for x in res["civilization"])]
            guards["extinct"] = guards.get("extinct", 0) + (1 if extinct else 0)
        o.append(head)
        o += case_lines(name, res, alls)
        for k, v in kind_counts(res).items():
            reach[k] = reach.get(k, 0) + v
        if name.startswith("stress"):
            for k, v in kind_counts(res).items():
                stress[k] = stress.get(k, 0) + v
            guards["refuge"] += w.refuge_saves
            guards["shelter"] += w.shelter_blocks
        for k in ("flights", "guard_no_dest", "guard_too_few", "guard_with_dest"):
            guards[k] = guards.get(k, 0) + getattr(w, k)
        if name == "hist-conflict-flight":
            found = sum(1 for _h, y in w.hidden_until.items() if y < inp.params["span"])
            c.true("hist-conflict-flight: flights and found refuges (%d, %d)" % (w.flights, found),
                   w.flights >= 1 and found >= 1)
        for r in res["relation"]:
            if r[6] is not None and r[5] == r[6]:
                guards["same-year " + r[4]] = guards.get("same-year " + r[4], 0) + 1
                print("same-year row %s %s" % (name, r))
        for (oe, planned, actual) in w.plague_plans:
            if planned != actual:
                guards["plague plan != strikes"] = guards.get("plague plan != strikes", 0) + 1
                print("note: %s plague e%d planned %d dead, struck %d (10.2: a spread target changed by an earlier "
                      "strike of the same outbreak)" % (name, oe, planned, actual))
        if name.startswith("syn-") or head.startswith("world"):
            pres = history_stage(w.pd, inp, conflict=False)
            guards["18a eras"] = guards.get("18a eras", 0) + check_prefix(c, res, pres, name)
        if name.startswith("syn-") and int(name[4:]) < 3:
            res2 = history_stage(w.pd, inp, extra_draw=True)
            c.eq("%s 14 item 9 draw independence" % name, table_hashes(res2), table_hashes(res))
        if name == "hist-conflict-42-1024" and not FLAGS["loss"]:   # the sample of 15.2 is 0.16.0's
            check_example(c, res)
            pres = pp.world_case("hist-peoples-42-1024", 42, 1024, 6000, "all", "normal", inputs)
            c.eq("16.3 item 20: e1-e37 of hist-peoples-42-1024 unchanged with conflict",
                 [e for e in res["event"][:37]], [e for e in pres["event"][:37]])
            c.eq("16.3 item 20: the first conflict event is the claim e38 in 2148",
                 (res["event"][37][3], res["event"][37][1]), (K["claim"], 2148))
            print("15.2 reference: %d events, kinds %s" % (len(res["event"]), sorted(kind_counts(res).items())))
    for k in ("claim", "war_declared", "battle", "battle_at_pass", "siege", "sacked", "settlement_captured", "peace",
              "truce", "region_ceded", "civ_conquered", "flare_up", "capital_moved") + \
            (("refugees_settled", "tribute", "war_won", "refuge_held") if pe.at_least("0.18.0") else ()):
        c.true("the stress cases reach %s" % k, stress.get(k, 0) > 0)
    if pe.at_least("0.18.0"):
        c.true("some case reaches search_abandoned (r10 20.17)", reach.get("search_abandoned", 0) > 0)
        print("r10 reach: war_won %d, search_abandoned %d, refuge_held %d, peace %d" % tuple(
            reach.get(k, 0) for k in ("war_won", "search_abandoned", "refuge_held", "peace")))
    c.true("the stress cases reach last_refuge (a fallen siege held)", guards["refuge"] > 0)
    c.true("the stress cases reach shelter_years (a claimed neighbour sheltered)", guards["shelter"] > 0)
    c.true("without the guards a race dies out (9, question 4)", guards.get("extinct", 0) > 0)
    if not quick:
        c.true("a case holds a war row opened and closed in one year (8.3 item 7, P3-04 6.7 r9)",
               guards.get("same-year war", 0) > 0)
    print("stress reach: %s; refuge %d, shelter %d" % (sorted(stress.items()), guards["refuge"], guards["shelter"]))
    print("guards: %s" % sorted(guards.items()))
    if not quick and os.path.isdir(inputs) and "--write" in argv:
        with open(GOLDEN, "wb") as f:
            f.write(("\n".join(o) + "\n").encode())
        print("note: wrote %s (check --write: the golden is this run's text)" % GOLDEN)
    elif not quick and os.path.isdir(inputs):
        fresh = "\n".join(o) + "\n"
        if os.path.exists(GOLDEN):
            with open(GOLDEN, "rb") as f:
                c.eq("committed golden file is current", f.read().decode() == fresh, True)
        else:
            c.true("golden file present", False)
    elif not os.path.isdir(inputs):
        print("note: %s missing; the world cases and the golden comparison are skipped" % inputs)
    if pdr.PRE_015_USED:
        print("note: the disk holds pre-0.16.0 race/history files; the P3-06 listings stand in for them")
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


def print_world(res, inp, name="w"):
    for k, v in sorted(table_hashes(res).items()):
        print("table %s %s" % (k, v))
    for line in report_lines(res, inp) + ["[stats]"] + stats_lines(res, inp):
        print(line)
    for line in war_lines(name, res) + tension_lines(name, res):
        print(line)
    for line in chronicle_lines(res, inp):
        print(line)
    for line in spec_war_lines(res, inp):
        print(line)
    for line in end_state(res, inp):
        print(line)
    print("counts %s" % sorted(world_counts(res).items()))


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
                    civs=pp.civs_param(opt(argv, "--civs")) if "--civs" in argv else p0["civs"],
                    intensity=opt(argv, "--intensity", p0["intensity"]))
    inp, _chron = inputs_for(dat, conn, p, int(meta["seed"]), int(meta["param.map_size"]))
    res = history_stage(dat, inp)
    print_world(res, inp)
    return 0


def stored_rows(conn, table):
    _h, rows = pchr.world_table_hash(conn, table)
    return [tuple(r) for r in rows]


def cmd_check_world(argv):
    path = argv[0]
    dat = data(opt(argv, "--data", "game/data"))
    conn = pchr.open_world(path)
    meta = dict(conn.execute("SELECT key, value FROM meta").fetchall())
    c = Checker()
    if meta.get("schema_version") != "7":
        print("%s: schema version %s, check-world reads 7" % (path, meta.get("schema_version")))
        return 1
    c.eq("generator_version", meta.get("generator_version"), GENERATOR_VERSION)
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
        st["_inputs"] = inp
        st["_engine"] = res["_engine"]
        st["_world"] = res["_world"]
        w = res["_world"]
        check_invariants(c, st, inp, w, os.path.basename(path), last_refuge=w.ct["last_refuge"] == 1)
        hard_bounds(c, st, inp, os.path.basename(path))
        worst, over = share_over(st, inp, w, 800)
        c.true("15.5 runaway: share_over_800_years <= 1000 (%d years; largest share %d permille)" % (over, worst),
               over <= 1000)
        print("\n".join(report_lines(st, inp) + ["[stats]"] + stats_lines(st, inp)))
        print("\n".join(war_lines("w", res)))
    conn.close()
    print("%s: %d checks, %d failures" % (path, c.count, c.failures))
    return 1 if c.failures else 0


def cmd_synthetic(argv):
    seed = int(argv[0])
    stress = "--stress" in argv
    dat = stress_data() if stress else data()
    case = STRESS_CASES.get(seed, synthetic_case(seed)) if stress else synthetic_case(seed)
    inp, _g = synthetic_inputs(seed, dat, case)
    res = history_stage(dat, inp)
    print_world(res, inp, "%s-%d" % ("stress" if stress else "syn", seed))
    return 0


def main(argv):
    # --legacy: every P3-08 rule flag off (the 0.16.0 rules; with 0.16.0 data and names the 0.16.0 worlds)
    if "--legacy" in argv:
        argv = [a for a in argv if a != "--legacy"]
        legacy()
    # --version V: the rules, data, names and catalogue of generator V (0.16.0, 0.17.0 or 0.18.0)
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
