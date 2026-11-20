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

    python3 -B worldgen/tests/reference/p3_conflict_reference.py check [--inputs DIR] [--quick]
        Self-checks: the data (history.conflict = the listing, the race
        changes, the catalogue of kinds 20-30 and the report entries), the
        worked example W1 of 16.1 and hand-made units, the invariants of 14
        and the hard bounds on the synthetic and stress cases and the world
        cases, draw independence (14 item 9), the sample of 15.2 (seed 42 at
        1024: the war list and the sixty summary lines), and the committed
        golden file against a fresh computation (skipped with --quick, which
        also runs fewer synthetic seeds). Exit 1 on any disagreement.

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
GENERATOR_VERSION = "0.16.0"

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
    ("14 item 4 conqueror alive", "Alive at the conquest: ended_year NULL, or later than the year, or the same year "
                                  "with an ending event after the conquest."),
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
COMPILED = tuple(pe.COMPILED)
assert list(COMPILED[19:]) == list(CONFLICT_KINDS) and len(COMPILED) == 30
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
    ("settlement_captured", 60, "{attacker} took {@settlement} from {defender}; {dead} died and {fled} fled."),
    ("peace", 60, "{loser} sued {winner} for peace after {years} {years?year|years} of war."),
    ("truce", 50, "{parties:all} made peace after {years} {years?year|years} of war, neither the victor."),
    ("region_ceded", 55, "{from} ceded {@region} to {to}."),
    ("civ_conquered", 90, "{civilization} fell to {conqueror}."),
)

STREAMS = ("hist.conflict.declare", "hist.conflict.battle", "hist.conflict.battle_win", "hist.conflict.siege",
           "hist.conflict.sack")

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
    for n, lo, hi in pdr.CONFLICT_RANGES:
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
        self.relations = {}        # RelationKey -> opening event (P3-04 State)
        self.rel_closed = {}       # RelationKey -> closing event since the last bookkeeping
        self.rel_rows = []         # [a_type, a_id, b_type, b_id, kind, since, until, since_event, until_event]
        self.rel_open_row = {}     # RelationKey -> index of its open row
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
        for civ in self.civs:
            if civ["ended_year"] is not None:
                continue
            if civ["id"] in self.charged:
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
        if key in self.relations:
            fatal("history: state: open_relation: the relation is open")
        self.relations[key] = e

    def close_rel(self, key, e):
        if key not in self.relations:
            fatal("history: state: close_relation: the relation is not open")
        del self.relations[key]
        self.rel_closed[key] = e

    def bookkeeping(self, y, phase_end, end_year=None):
        super().bookkeeping(y, phase_end, end_year)
        # 6.7 item 2: relations, in RelationKey order over the union of the recorded and current keys
        keys = set(self.relations) | set(self.rel_open_row)
        for k in sorted(keys, key=key_order):
            now, rec = k in self.relations, k in self.rel_open_row
            if now and not rec:
                self.rel_open_row[k] = len(self.rel_rows)
                self.rel_rows.append([k[0], k[1], k[2], k[3], k[4], y, None, self.relations[k], 0])
            elif rec and not now:
                row = self.rel_rows[self.rel_open_row.pop(k)]
                row[6] = y
                row[8] = self.rel_closed[k]
        self.rel_closed = {}

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

    def presence(self, c):
        return sorted(set(self.held(c)) | {s["region"] for s in self.svis(c)})

    def reach_dist(self, x):
        """(presence(x), d_{H-1}) for x, cached per step and state version."""
        key = (self.serial, self.version, x)
        if key in self._fcache:
            return self._fcache[key]
        if len(self._fcache) > 4096:
            self._fcache = {}
        pres = self.presence(x)
        d = {r: 0 for r in pres}
        for _h in range(self.ct["march_hops"] - 1):
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
        """5.1: [(R, cost, R_from)] over presence(y) ascending."""
        px, d = self.reach_dist(x)
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

    def force(self, c):
        lv = self.ct["levy_permille"]
        levy = sum(s["population"] * lv[KIND_KEY[s["kind"]]] // 1000 for s in self.svis(c))
        f = levy * self.pd.race[self.race_c(c)]["prowess"] // 1000
        if f > (1 << 40):
            fatal("history: bound: conflict force of civilization %d exceeds 2^40" % c)
        return f

    def wars_of(self, c):
        return sum(1 for w in self.wars if w["open"] and c in (w["a"], w["b"]))

    def at_war(self, a, b):
        return any(w["open"] and {w["a"], w["b"]} == {a, b} for w in self.wars)

    def past_truce(self, a, b, y=None):
        y = self.year if y is None else y
        lp = self.last_peace.get((min(a, b), max(a, b)))
        return lp is None or y >= lp[0] + self.ct["truce_years"]

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
            g = self.pd.race[self.race_c(a)]["aggression"]
            for b in al:
                if a == b or (a, b) not in self.contact:
                    continue
                if self.at_war(a, b) or not self.past_truce(a, b):
                    continue
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
                r, cost, _f = min(fr, key=lambda e: (0 if (e[0] in wanted[a] and self.owner[e[0]][0] == b) else 1,
                                                    -(self.geo.sigma(e[0], ra) - t["cost_penalty"] * e[1]), e[0]))
                if self.owner[r][0] == b:
                    cause = self.owner[r][1]
                else:
                    ss = [s for s in self.svis(b) if s["region"] == r]
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
        if self.owner[g0][0] != b and not any(s["region"] == g0 for s in self.svis(b)):
            goal = min(self.front(a, b), key=lambda e: self.target_key(a, e))[0]
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
                              battles=0, sacked=0, captured=0, flare=draw_free, ceded=False))

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
        g = self.pd.race[self.race_c(a)]["aggression"]
        p = t["declare_permille"] * min(1000, (s - t["declare_min"]) * 1000 // t["declare_span"]) // 1000
        odds = min(1000, self.force(a) * 1000 // max(1, self.force(b) * t["declare_ratio"] // 1000 *
                                                      (1000 - g) // 1000))
        return p * odds // 1000

    def battle_p(self, A, D, dv):
        D2 = D * dv // 1000
        return A * 1000 // (A + D2) if A + D2 > 0 else 500

    def garrison(self, ts, y):
        t = self.ct
        walls = t["walls_permille"][KIND_KEY[ts["kind"]]] + (t["capital_walls"] if self.civ(y)["capital"] == ts["id"]
                                                             else 0)
        return ts["population"] * t["garrison_permille"] // 1000 * self.pd.race[self.race_c(y)]["prowess"] // 1000 \
            * walls // 1000

    def siege_p(self, A2, G):
        return A2 * 1000 // (A2 + G) if A2 + G > 0 else 0

    # --- 7.4
    def casualties(self, c, dead, e, war):
        ss = self.svis(c)
        tot = sum(s["population"] for s in ss)
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
        if not chance_pm(self.draw("hist.conflict.battle", w, 0, 0), t["battle_permille"]):
            return
        A = self.force(x) // self.wars_of(x)
        D = self.force(y) // self.wars_of(y)
        dv, pass_id = self.defence(r, r_from, y)
        p = self.battle_p(A, D, dv)
        xwin = chance_pm(self.draw("hist.conflict.battle_win", w, 0, 0), p)
        W, L, FW, FL = (x, y, A, D) if xwin else (y, x, D, A)
        dead_l = FL * t["loss_loser_permille"] // 1000
        dead_w = FW * t["loss_winner_permille"] // 1000
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
        if not xwin:
            return
        ss = [s for s in self.svis(y) if s["region"] == r]
        if not ss:
            return
        ts = min(ss, key=lambda s: (-s["population"], s["id"]))
        G = self.garrison(ts, y)
        A2 = max(0, A - dead_w)
        ps = self.siege_p(A2, G)
        fell = chance_pm(self.draw("hist.conflict.siege", w, ts["id"], 0), ps)
        if t["last_refuge"] == 1 and fell:
            race = self.race_c(y)
            alive_same = [c["id"] for c in self.civs if c["ended_year"] is None and c["race"] == race]
            if alive_same == [y] and [s["id"] for s in self.standing_of(y)] == [ts["id"]]:
                fell = False
                self.refuge_saves += 1
        es = self.append("siege", region=r, settlement=ts["id"], participants=(("attacker", CIV, x),
                                                                               ("defender", CIV, y)),
                         values=(1 if fell else 0,), causes=(e,))
        war["last"] = es
        if fell:
            self.fall(war, ts, x, y, r, es)

    def score(self, war):
        a, b = war["a"], war["b"]
        return (war["won"][a] - war["won"][b]) + 2 * (war["taken"][a] - war["taken"][b])

    def by_score(self, war):
        s = self.score(war)
        return war["a"] if s > 0 else (war["b"] if s < 0 else None)

    def end_check(self, war):
        t = self.ct
        a, b = war["a"], war["b"]
        ex = [c for c in (a, b) if war["dead"][c] * 1000 >= t["exhaustion_permille"] * max(1, war["pop0"][c])]
        if ex:
            s = self.score(war)
            fav = a if s > 0 else (b if s < 0 else None)
            if len(ex) == 2 or s == 0 or ex[0] != fav:
                self.end_war(war, self.by_score(war), "exhaustion")
            else:
                self.end_war(war, None, "exhaustion")
            return
        g = war["goal"]
        if self.owner[g][0] != b and not any(s["region"] == g for s in self.svis(b)):
            self.end_war(war, a, "aim")
            return
        if war.get("no_front"):
            self.end_war(war, self.by_score(war), "no-front")
            return
        if self.year - war["start"] >= t["war_max_years"]:
            self.end_war(war, self.by_score(war), "length")

    def cession_due(self, war, winner):
        if winner != war["a"]:
            return False
        L = war["b"]
        g = war["goal"]
        return self.owner[g][0] == L and self.stl(self.civ(L)["capital"])["region"] != g

    def end_war(self, war, winner, how):
        t = self.ct
        a, b = war["a"], war["b"]
        years = self.year - war["start"]
        if winner is not None:
            L = b if winner == a else a
            e = self.append("peace", participants=(("winner", CIV, winner), ("loser", CIV, L)), values=(years,),
                            causes=(war["last"],))
        else:
            e = self.append("truce", participants=(("parties", CIV, min(a, b)), ("parties", CIV, max(a, b))),
                            values=(years,), causes=(war["last"],))
        self.close_rel(rel_key(a, b, "war"), e)
        self.open_rel(sym_key(a, b, "peace"), e)
        self.last_peace[(min(a, b), max(a, b))] = (self.year, e)
        self.add_grudge(a, b, t["war_grudge"])
        self.zero_strain(a, b)
        self.drop_claims(a, b)
        self.close_war(war, how, winner)
        if winner is not None and self.cession_due(war, winner):
            L = b
            g = war["goal"]
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
            war["ceded"] = True

    # --- 7.8 Q4
    def c_q4(self):
        t = self.ct
        self.housekeeping()
        for war in [w for w in self.wars if w["open"]]:
            if not war["open"]:
                continue
            winner = self.by_score(war)
            if winner is None:
                if self.quiet_events_left(min(war["a"], war["b"])) <= 0:
                    continue
            else:
                if self.quiet_events_left(winner) <= 0:
                    continue
                if self.cession_due(war, winner) and self.quiet_events_left(war["b"]) <= 0:
                    continue
            self.end_war(war, winner, "quiet")
        self.decay(self.years)
        self.strain_update(self.years, t["quiet_strain_permille"], True)

    # in-run invariants (P3-05's, and 14 item 6 capacity after every step)
    def check_state(self):
        super().check_state()


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
        relation=sorted((tuple(r) for r in w.rel_rows), key=lambda r: (r[0], r[1], r[2], r[3], r[4].encode(), r[5])),
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
    return o


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
            else:
                out = ("won-c%d" % win + ("-ceded" if war["ceded"] else "")) if win is not None else "truce"
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
                       battle_permille=900, garrison_permille=40, walls_permille=dict(hamlet=1000, village=1000,
                                                                                     town=1200, city=1500,
                                                                                     fortress=2000),
                       capital_walls=500, exhaustion_permille=400, war_max_years=25, truce_years=40)
STRESS_SEEDS = (0, 1, 2, 3, 4, 5, 6, 7, 9, 12, 13, 14)
STRESS_CASES = {s: ((2000, 6000)[s % 2], ("all", "elf,goblin", "goblin,human,orc", "all")[(s // 2) % 4],
                    ("auto", "4")[(s // 4) % 2], ("normal", "turbulent", "calm")[s % 3]) for s in STRESS_SEEDS}


def stress_data(dat=None):
    return edited(dat or data(), conflict=STRESS_CONFLICT, peoples=STRESS_PEOPLES)


# ---------------------------------------------------------------------------
# Invariants (14, with P3-05's as amended by 18.1) on rows
# ---------------------------------------------------------------------------

QUIET_KINDS = set(pp.QUIET_KINDS) | {K["claim"], K["peace"], K["truce"], K["region_ceded"]}


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
        c.true("%s P3-05 inv 1 race %d" % (label, a[1]), 1 <= len(founded) <= (2 if setting is None else setting))
        for x in founded:
            c.true("%s P3-05 inv 1 founded at the awakening" % label, x[6] == a[2] and kind[causes[x[7]][0]] == 1)
    cells = {}
    faded_at = {e[6]: e[0] for e in res["event"] if e[3] == K["ruin_faded"]}
    for s in res["settlement"]:
        c.true("%s P3-05 inv 2 s%d founding kind" % (label, s[0]), kind[s[7]] in (K["settlement_founded"],
                                                                                  K["colony_founded"]))
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
    for r in res["settlement_census"]:
        s = sts[r[0]]
        built = set(w.pd.race[civs[s[5]][2]]["kinds"])
        if r[2]:
            built |= set(w.pd.race[civs[r[2]][2]]["kinds"])
        c.true("%s P3-05 inv 6 s%d kind %d built" % (label, r[0], r[3]), r[3] in built)
    cen = {}
    for r in res["settlement_census"]:
        cen.setdefault(r[0], []).append(r)
    for s, rows in cen.items():
        if rows[0][3] == FORTRESS:
            c.true("%s P3-05 inv 6 fortress s%d stays" % (label, s), all(r[3] == FORTRESS for r in rows))
    for s in res["settlement"]:
        if s[10] == RUINED:
            c.true("%s P3-05 inv 7 s%d ruin event" % (label, s[0]), kind[s[13]] in (K["settlement_abandoned"],
                                                                                   K["sacked"]))
        if s[10] == FADED:
            fades = [e for e in res["event"] if e[3] == K["ruin_faded"] and e[6] == s[0]]
            ok = len(fades) == 1 and len(causes.get(fades[0][0], [])) == 1
            if ok:
                ruin = ev[causes[fades[0][0]][0]]
                ok = (ruin[3] in (K["settlement_abandoned"], K["sacked"]) and ruin[6] == s[0] and
                      fades[0][1] >= ruin[1] + t["ruin_fade_years"] and s[12] is None and s[13] == 0)
            c.true("%s P3-05 inv 7 s%d faded" % (label, s[0]), ok)
    red = ("famine", "plague", "plague_spread", "succession_crisis", "battle", "battle_at_pass")
    want_cause = {
        "plague_spread": ("plague",), "civ_seceded": ("succession_crisis",),
        "settlement_abandoned": red + ("settlement_founded", "colony_founded"),
        "capital_moved": ("settlement_abandoned", "sacked", "settlement_captured"),
        "civ_ended": ("settlement_abandoned",), "ruin_faded": ("settlement_abandoned", "sacked"),
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
                c.true(lab + " until_event", uk in ("peace", "truce", "civ_ended", "civ_conquered"))
        else:
            c.true(lab + " since_event", sk in ("peace", "truce"))
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
            if uk in ("peace", "truce"):
                ps = sorted(p[4] for p in parts[u])
                c.true(lab + " ended by a peace of its sides", ps == sorted((a, b)))
                c.true(lab + " peace dated at the row's until", ev[u][1] == r[6])
            else:
                ended = role(u, 1)[0]
                c.true(lab + " ended by the end of a side", ended in (a, b) and civs[ended][9] == u)
            c.true(lab + " no war ends in its declaration year (7.3)", r[6] > r[5])
    for e in res["event"]:
        key = KEY_OF[e[3]]
        if key in ("battle", "battle_at_pass", "siege", "sacked", "settlement_captured"):
            c.true("%s inv 2 e%d %s not in a quiet phase" % (label, e[0], key), ph[e[4]][2] != QUIET)
        if key in ("peace", "truce", "region_ceded") and ph[e[4]][2] == QUIET:
            row = ph[e[4]]
            c.true("%s inv 2 e%d %s at a quiet step start" % (label, e[0], key),
                   (e[1] - row[3]) % w.eng["quiet_step_years"] == 0)
        if key in ("peace", "truce"):
            cs = causes.get(e[0], [])
            c.true("%s inv 2 e%d caused by the war's last event" % (label, e[0]),
                   len(cs) == 1 and KEY_OF[kind[cs[0]]] in ("war_declared", "battle", "battle_at_pass", "siege",
                                                           "sacked", "settlement_captured"))
        if key == "region_ceded":
            cs = causes.get(e[0], [])
            c.true("%s 11 e%d cession caused by a peace" % (label, e[0]), len(cs) == 1 and kind[cs[0]] == K["peace"])
        if key in ("battle", "battle_at_pass"):
            cs = causes.get(e[0], [])
            c.true("%s 11 e%d caused by its declaration" % (label, e[0]), len(cs) == 1 and
                   kind[cs[0]] == K["war_declared"])
        if key == "claim":
            cs = causes.get(e[0], [])
            c.true("%s 11 e%d claim cause" % (label, e[0]), len(cs) == 1 and KEY_OF[kind[cs[0]]] in (
                "civ_founded", "settlement_founded", "colony_founded", "civ_seceded", "settlement_captured",
                "region_ceded"))
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
            "civ_founded", "colony_founded", "civ_seceded", "settlement_captured", "region_ceded"))
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

WORLD_CASES = tuple(pe.WORLD_CASES) + (("hist-conflict-42-1024", 42, 1024, 6000, "all", "normal"),)
INPUT_OF = {"hist-conflict-42-1024": "example-42-1024"}


def world_case(name, seed, size, span, races, detail, inputs_dir, dat=None, civs=None, intensity="normal"):
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
    p = pchr.params(span=span, races=pchr.races_of(races, dat.chron), detail=detail, civs=civs, intensity=intensity)
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
    sents = sentences(res, inp)
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
#   world <case> <seed> <map size> <span> <races> <detail>
#       a world case (16.3 item 19): the four P1 golden cases, the three
#       hist-* cases and hist-conflict-42-1024 (16.3 item 20, the sample of
#       15.2), generated with the shipped data and these settings, other
#       settings default (civs auto, intensity normal)
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
#                  40, walls_permille {1000, 1000, 1200, 1500, 2000},
#                  capital_walls 500, exhaustion_permille 400, war_max_years
#                  25, truce_years 40
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
#   <case> e<id> <sentence>          render_event: every event of the 512,
#                                    synthetic and stress cases, the events
#                                    of kinds 20-30 of the 1024 cases
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
        d2.race[rid]["aggression"] = dat.race[rid]["aggression"]
        d2.race[rid]["prowess"] = dat.race[rid]["prowess"]
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
        c.eq("W1 %s: battle won by 2, dead 38 and 100" % branch, ([p[3] for p in b["participants"]], b["values"]),
             ([2, 1], {1: 38, 2: 100}))
        c.eq("W1 %s: siege of s2 fell" % branch, (keys[2], ev[2]["settlement"], ev[2]["values"]),
             ("siege", 2, {1: 1}))
        c.eq("W1 %s: G 345, A2 442, ps 561" % branch, (w.garrison(dict(w.stl(2), population=480), 1),
                                                       w.siege_p(442, 345)), (345, 561))
        if branch == "sack":
            c.eq("W1 sack: values (144 dead, 336 survivors)", (keys[3], ev[3]["values"]), ("sacked", {1: 144, 2: 336}))
            c.eq("W1 sack: casualties 80 at s1 then 336 fled to s1 (room permitting)", w.stl(1)["population"],
                 min(1920 + 336, max(1920, 2 * w.geo.capacity(3, 1))))
            c.eq("W1 sack: s3 lost 38", w.stl(3)["population"], 5962)
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


def check_units(c):
    dat = data()
    # 12.1 tuning
    c.eq("12.1 history.conflict = the listing", dat.conflict, pdr.parsed_spec("history")["conflict"])
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
    c.eq("11 report entries 20-30", [e for e in dat.report[19:]], list(SPEC_REPORT))
    c.eq("11 report_events on the 0.16.0 report", pp.report_events_messages(dat.report), [])
    for k in CONFLICT_KINDS:
        c.eq("11 template of %s parses" % k["key"], pe.template_problem(k, dict((e[0], e[2]) for e in dat.report)[k["key"]]),
             None)
    c.eq("3.3 five streams", len(STREAMS), 5)
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


def check_peoples_only(c, dat):
    """At 0.16.0 the peoples-only configuration is p3_peoples_reference's run (its golden file)."""
    for seed in (0, 3, 5):
        inp, _g = pp.synthetic_inputs(seed, dat)
        a = pp.history_stage(dat, inp)
        inp2, _g = pp.synthetic_inputs(seed, dat)
        inp2.biome_keys = dict(BIOME_KEYS)
        b = history_stage(dat, inp2, conflict=False)
        c.eq("syn-%d peoples-only configuration = the P3-05 reference" % seed, table_hashes(b), table_hashes(a))


def all_cases(inputs, quick=False):
    """(header line, name, res, all sentences) for every golden case, in file order."""
    dat = data()
    if os.path.isdir(inputs):
        for name, seed, size, span, races, detail in WORLD_CASES:
            if quick and size > 512 and name != "hist-conflict-42-1024":
                continue
            res = world_case(name, seed, size, span, races, detail, inputs, dat)
            yield "world %s %d %d %d %s %s" % (name, seed, size, span, races, detail), name, res, size <= 512
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
        yield "stress %s %d %d %s %s %s" % (name, seed, span, races, civs, intensity), name, res, True


def golden_text(inputs, quick=False):  # noqa: F811 (one pass over all_cases)
    o = [HEADER.rstrip("\n")]
    for head, name, res, alls in all_cases(inputs, quick):
        o.append(head)
        o += case_lines(name, res, alls)
    return "\n".join(o) + "\n"


def cmd_check(argv):
    inputs = argv[argv.index("--inputs") + 1] if "--inputs" in argv else DEFAULT_INPUTS
    quick = "--quick" in argv
    c = Checker()
    dat = data()
    check_units(c)
    check_peoples_only(c, dat)
    o = [HEADER.rstrip("\n")]
    reach = {}
    guards = dict(refuge=0, shelter=0)
    stress = {}
    for head, name, res, alls in all_cases(inputs, quick):
        inp = res["_inputs"]
        w = res["_world"]
        check_invariants(c, res, inp, w, name, last_refuge=w.ct["last_refuge"] == 1)
        hard_bounds(c, res, inp, name)
        o.append(head)
        o += case_lines(name, res, alls)
        for k, v in kind_counts(res).items():
            reach[k] = reach.get(k, 0) + v
        if name.startswith("stress"):
            for k, v in kind_counts(res).items():
                stress[k] = stress.get(k, 0) + v
            guards["refuge"] += w.refuge_saves
            guards["shelter"] += w.shelter_blocks
        if name.startswith("syn-") and int(name[4:]) < 3:
            res2 = history_stage(w.pd, inp, extra_draw=True)
            c.eq("%s 14 item 9 draw independence" % name, table_hashes(res2), table_hashes(res))
        if name == "hist-conflict-42-1024":
            check_example(c, res)
            pres = pp.world_case("hist-peoples-42-1024", 42, 1024, 6000, "all", "normal", inputs)
            c.eq("16.3 item 20: e1-e37 of hist-peoples-42-1024 unchanged with conflict",
                 [e for e in res["event"][:37]], [e for e in pres["event"][:37]])
            c.eq("16.3 item 20: the first conflict event is the claim e38 in 2148",
                 (res["event"][37][3], res["event"][37][1]), (K["claim"], 2148))
            print("15.2 reference: %d events, kinds %s" % (len(res["event"]), sorted(kind_counts(res).items())))
    for k in ("claim", "war_declared", "battle", "battle_at_pass", "siege", "sacked", "settlement_captured", "peace",
              "truce", "region_ceded", "civ_conquered", "flare_up", "capital_moved"):
        c.true("the stress cases reach %s" % k, stress.get(k, 0) > 0)
    c.true("the stress cases reach last_refuge (a fallen siege held)", guards["refuge"] > 0)
    c.true("the stress cases reach shelter_years (a claimed neighbour sheltered)", guards["shelter"] > 0)
    print("stress reach: %s; refuge %d, shelter %d" % (sorted(stress.items()), guards["refuge"], guards["shelter"]))
    if not quick and os.path.isdir(inputs):
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
