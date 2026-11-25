#!/usr/bin/env python3
"""Reference of P5-02 "Beasts and monsters" (docs/specs/p5-02-beasts-monsters.md r0), written from the spec alone,
without the engineer's branch.

The monsters subsystem of sections 5 and 6 runs inside p3_conflict_reference.py's replay, installed after renaming
(P5-00 4.1): World.monsters runs in T1, T4, T6, Q1 and Q4 and is the second tension source, so every history golden
of the conflict reference carries it. The peoples seams of section 7 (WildView, shunned regions) and the ghost rule
of 8.2 are in p3_peoples_reference.py; the catalogue, schema 9 and the rendering of 10.3 in p3_engine_reference.py;
monster and outpost names (9.1) in p3_names_reference.py. This module holds the subsystem, the lair names of 9.2,
the invariants of section 14 and the checks of 11.4 on rows (check-world mode), the hand-made units of 19.1 on a
stub world, invariant I0 (7.5) and the golden file worldgen/tests/golden/p5-02-monsters.txt.

Commands (from the repository root):
  python3 -B worldgen/tests/reference/p5_02_monsters_reference.py check [--quick] [--inputs DIR]
  python3 -B worldgen/tests/reference/p5_02_monsters_reference.py units          the stub-world units of 19.1
  python3 -B worldgen/tests/reference/p5_02_monsters_reference.py golden [PATH|-] [--inputs DIR]
  python3 -B worldgen/tests/reference/p5_02_monsters_reference.py check-world DB [DB ...]
  python3 -B worldgen/tests/reference/p5_02_monsters_reference.py i0 [--inputs DIR]   I0 on fixtures and inputs

--inputs DIR: the world inputs (default ~/.cache/the-unmapped/out/p5-02-tests/inputs: geography of the P5-12
inputs; only the geography tables and layers are read).

Recording after the engineer's calibration (17 step 9): the shipped rates are read from the data root, so once
game/data and both fixture roots hold the calibrated values, `golden` re-records this file in one command (and
p3_data_reference.py's listings BEASTS_023, MONSTERS_KINDS_023, MONSTERS_TUNING_023 are the one place to edit).
"""

import copy
import hashlib
import os
import sqlite3
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import core_reference as cr  # noqa: E402
import p3_engine_reference as pe  # noqa: E402
import p3_names_reference as pnr  # noqa: E402
import p3_peoples_reference as pp  # noqa: E402
import p3_data_reference as pdr  # noqa: E402

GOLDEN = os.path.join(pe.REPO, "worldgen", "tests", "golden", "p5-02-monsters.txt")
DEFAULT_INPUTS = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p5-02-tests", "inputs")
# 3: appended after hist.renaming.name, in this order (the two name streams are drawn by p3_names_reference)
STREAMS = ("hist.beasts.bold", "hist.monsters.wake", "hist.monsters.rise", "hist.monsters.sleep",
           "hist.monsters.rewake", "hist.monsters.raid", "hist.monsters.raid_toll", "hist.monsters.overrun",
           "hist.monsters.hunt", "hist.monsters.hunt_outcome", "hist.monsters.lair_name", "hist.sites.outpost",
           "hist.peoples.ghost", "hist.names.monster", "hist.names.site", "hist.monsters.raid_target")
# r1 (6.4.1): hist.monsters.raid_target is appended after hist.names.site.
SIM_STREAMS = STREAMS[:12] + ("hist.monsters.raid_target",)
# P5-03 3: appended after hist.monsters.raid_target (the part-B streams of this module)
P503_STREAMS = ("hist.beasts.bold_kind", "hist.monsters.fear")


def p503():
    """P5-03 part B (9, 12, 13, 14) is on with the peoples flag of 0.24.0."""
    return pp.FLAGS.get("p503", False)


def p503a():
    """P5-03a 6 (the undead threshold by world population) is on with the conflict flag of 0.25.0."""
    import p3_conflict_reference as pcr
    return pcr.FLAGS.get("p503a", False)


def riders():
    """P5-06 25.3 and 7.8 are on with the conflict flag riders of 0.27.0."""
    import p3_conflict_reference as pcr
    return pcr.FLAGS.get("riders", False)


def occupies(s):
    """P5-02 6.3.2's "occupied" test of a site at a ruin: kind != 3 at 0.26.0; P5-06 7.8: a lair or an outpost."""
    return s["kind"] in (LAIR, OUTPOST) if riders() else s["kind"] != MINE


def undead_threshold_027(min_dead, permille, P_W, M, R, regions_ref):
    """P5-06 25.3: Tpop = P_W x undead_dead_permille div 1000; x regions_ref div max(regions_ref, R) when
    regions_ref > 0; T = max(undead_min_dead, Tpop); S(r) >= T x 1000 div max(1000, M)."""
    tpop = P_W * permille // 1000
    if regions_ref > 0:
        tpop = tpop * regions_ref // max(regions_ref, R)
    T = max(min_dead, tpop)
    return T * 1000 // max(1000, M)


def undead_threshold_025(min_dead, permille, P_W, M):
    """P5-03a 6: T = max(undead_min_dead, P_W x undead_dead_permille div 1000); S(r) >= T x 1000 div max(1, M)."""
    T = max(min_dead, P_W * permille // 1000)
    return T * 1000 // max(1, M)
CIV, SETTLEMENT, REGION, MONSTER, SITE = pe.CIV, pe.SETTLEMENT, pe.REGION, pe.MONSTER, pe.SITE
AWAKE, ASLEEP, SLAIN, DIED = 1, 2, 3, 4
LAIR, OUTPOST = 1, 2
MINE = 3   # P5-04 9.2: written by the alliances subsystem through the site seam (7.16); every loop here skips it
# 11.2 site.ending
OPEN, END_SLAIN, END_DIED, END_MOVED, END_LOST, END_FELL, END_QUIET = 0, 1, 2, 3, 4, 5, 6
BEAST_MAX = 1000000
MONSTERS_MAX = 100000
SITES_MAX = 1000000

READINGS = """\
B1 (6.2.1) H(r, s) = (sum over the region_biome rows of cells x w) div 1000: one floor of the sum (the spacing of
   the formula puts `div 1000` after the sum); H_k(r) of 6.3.1 likewise.
B2 (6.2.2) the rows of a phase change carry the last year of the previous update (T1: its year; Q1: Y + L - 1); the
   first update's rows its last year; finish's rows E - 1. A row is written only when B differs from the last row
   of (r, s) (no row: 0).
B3 (6.2.3) menace(r) is the value of the last T1 or Q1 (with that step's P); the toll reads the live P_t of T4.
G1 (6.4.1, 6.4.2, 6.3, 6.6.2) GeoView neighbours are every region_border row (P3-04 6.1: ascending id, with the
   border row index), not peoples' open borders (P3-05 4.4); hop distances by BFS over them.
M1 (6.4 item 2) an asleep monster ends its year at item 2 whether it wakes or not.
M2 (6.4.1, 6.2.3) note_reduction is called for every raid and boldness event, also when dead = 0 (the text says
   `set_population ... when dead > 0; note_reduction(t, ...)`).
M3 (6.4.3) an overrun target's `other standing settlement` counts those created earlier in the step (P3-05 8.5
   item 2's rule, as stated); `no open lair of another monster lies in t's region`: m's own lair may lie there.
M4 (6.3.2) the lair at a ruined sacked settlement uses that settlement's cell even when its region differs from r
   (it never does: a sack's region is its settlement's); the event's settlement is that settlement.
N1 (9.2) the parts of V8 for a lair name are the form's literal runs after the possessive rule and the root.
N2 (9.1) a monster's or an outpost's name is P3-02 4.3 with the world's registry and no core test (cores are
   civilization and settlement roots only, 12.2; the spec says roots are not added to K, and K is not read).
N3 (9.1) D of a monster is its naming culture's data from the data root; F its kind's name_forms; the fused-tail
   rule of P3-02 16.1 reads T_K from F.
E1 (8.3) a faded settlement's ruin_event is 0 (P3-04 9.2 r3), so its ending is derived from the cause of its
   ruin_faded event (the event that ruined it).
Q1 (6.4.4, 6.3.1) a quiet step's death of age and appearance are dated Y (the step's start) and charged to
   civilization 0; an outpost left in Q4 to its founder (W9).
T1 (6.5, P3-04 6.5) the step's tension is the larger of conflict's and the monsters' values; on a tie conflict's
   (the first source) trigger.
F1 (7.4) conflict's forced declaration at a flare-up's first T2 happens only when the phase row's trigger_event is
   a `claim` or `claim_settlements`.
"""


def chance_pm(u, p):
    return pp.chance_pm(u, p)


def chance_ppm(u, p):
    return pp.chance_ppm(u, p)


def chance_ppb(u, p):
    """3: true iff (u >> 32) < min(2^32, floor(max(0, p) x 2^32 / 10^9))."""
    t = min(1 << 32, (max(0, p) << 32) // 1000000000)
    return (u >> 32) < t


def checked_mul(a, b):
    v = a * b
    if v >= (1 << 63) or v < -(1 << 63):
        pe.fatal("history: bound: product exceeds int64")
    return v


def fill_form(form, slot, value):
    """A form with its slot replaced, with 21.2's possessive rule: when the value ends in s or S and the literal
    run right after the slot starts with 's, that literal's s is dropped. Returns (text, parts): parts are the
    literal runs (after the rule) and the value in form order (V8, N1)."""
    a, b = form.split(slot, 1)
    if value[-1:] in ("s", "S") and b.startswith("'s"):
        b = "'" + b[2:]
    parts = [x for x in (a, value, b) if x]
    return a + value + b, parts


def roman(k):
    return pnr.roman(k)


class GeoNb:
    """GeoView neighbours (G1): every region_border row, ascending id."""

    def __init__(self, inp):
        self.ids = [r["id"] for r in inp.regions]
        self.nb = {r: [] for r in self.ids}
        for b in inp.borders:
            if b["b"] not in self.nb[b["a"]]:
                self.nb[b["a"]].append(b["b"])
                self.nb[b["b"]].append(b["a"])
        for r in self.ids:
            self.nb[r].sort()
        self._hops = {}

    def hops(self, r0):
        if r0 not in self._hops:
            d = {r0: 0}
            fifo = [r0]
            i = 0
            while i < len(fifo):
                x = fifo[i]
                i += 1
                for y in self.nb[x]:
                    if y not in d:
                        d[y] = d[x] + 1
                        fifo.append(y)
            self._hops[r0] = d
        return self._hops[r0]


class WildView:
    """7.1: growth_permille(region) and shunned(region) as the subsystem holds them."""

    def __init__(self, m):
        self.m = m

    def growth_permille(self, region):
        return self.m.g.get(region, 1000)

    def shunned(self, region):
        return self.m.shunned(region)


class Monsters:
    """MonstersSubsystem (5, 6) over a p3_conflict_reference (or p3_peoples_reference) World w."""

    def __init__(self, w, base, inp, tuning=None, kinds=None, beasts=None):
        self.w = w
        self.inp = inp
        self.t = tuning if tuning is not None else base.monsters_tuning
        self.kinds = kinds if kinds is not None else list(base.monsters)     # [(key, def)] canonical order
        self.beasts = beasts if beasts is not None else list(base.beasts)
        self.naming = base.naming
        self.threshold = base.engine[inp.params["detail"]]["flare_threshold"]
        self.M = self.t["pressure"][inp.params.get("pressure", "normal")]
        for s in SIM_STREAMS + P503_STREAMS:
            w.keys[s] = cr.stream_key(inp.seed, s)
        self.epithets = set()   # P5-03 12.1: the epithets of the world's monster names so far
        self.geo = GeoNb(inp)
        self.regions = list(self.geo.ids)
        self.R = len(self.regions)
        self.reg = {r["id"]: r for r in inp.regions}
        bk = getattr(inp, "biome_keys", None) or pe_biome_keys()
        self.rb = {}
        for (rg, b, cells) in inp.region_biomes:
            self.rb.setdefault(rg, []).append((bk.get(b, ""), cells))
        self.culture_key = {c[0]: c[1] for c in inp.cultures}
        self.culture_num = {c[1]: c[0] for c in inp.cultures}
        # beasts (5)
        self.started = False
        self.H, self.K0, self.B = {}, {}, {}
        self.d, self.g, self.menace, self.bold = {}, {}, {}, {}
        self.rows = []
        self.last_row = {}
        self.last_phase = None
        self.last_end = None
        self.first_done = False
        # monsters, sites
        self.monsters = []
        self.sites = []
        self.W = {}
        self.cursor = 1
        self.heroes_slain_now = set()   # P5-06 reading F-G1
        self.pending_flare = 0
        self.created_this_step = False
        # logs (not state), for checks and statistics
        self.log = []          # (year, what, ids...)
        self.tension_log = []

    # ---------------------------------------------------------------- helpers
    def kind(self, k):
        return self.kinds[k - 1][1]

    def kind_key(self, k):
        return self.kinds[k - 1][0]

    def habitat(self, rid, weights):
        """B1: sum of cells x weight over the region's biome rows, div 1000."""
        wmap = {x["biome"]: x["weight"] for x in weights}
        return sum(cells * wmap.get(key, 0) for key, cells in self.rb.get(rid, ())) // 1000

    def living(self):
        return [m for m in self.monsters if m["status"] in (AWAKE, ASLEEP)]

    def L_world(self):
        return max(1, self.R * self.t["living_permille"] // 1000)

    def open_lair_in(self, rid, but=None):
        return any(s["kind"] == LAIR and s["ending"] == OPEN and s["region"] == rid and s["monster"] != but
                   for s in self.sites)

    def shunned(self, rid):
        """7.2: an open lair whose settlement != 0 lies in r."""
        return any(s["kind"] == LAIR and s["ending"] == OPEN and s["settlement"] != 0 and s["region"] == rid
                   for s in self.sites)

    def caps_allow(self, k, rid):
        kd = self.kind(k)
        live = self.living()
        return (len(live) < self.L_world() and sum(1 for m in live if m["kind"] == k) < kd["max_alive"]
                and not self.open_lair_in(rid))

    def region_pop(self):
        """P(r) of 6.2.2: standing settlements of any owner, visible or not."""
        P = {r: 0 for r in self.regions}
        for s in self.w.settlements:
            if s["status"] in (1, 2):
                P[s["region"]] += s["population"]
        return P

    def site(self, sid):
        return self.sites[sid - 1]

    def lair_region(self, m):
        return self.site(m["lair"])["region"]

    def taken(self):
        t = set((s["x"], s["y"]) for s in self.w.settlements)
        t |= set((s["x"], s["y"]) for s in self.sites if s["ending"] == OPEN and s["kind"] != MINE)   # P5-04 7.16
        return t

    def cells_of(self, rid):
        lay = self.inp.peoples_layers()
        n = self.inp.map_size
        reg, wat, hab, ele = lay["region"], lay["water"], lay["habitability"], lay["elevation"]
        r = self.reg[rid]
        tk = self.taken()
        for y in range(r["y_min"], r["y_max"] + 1):
            for x in range(r["x_min"], r["x_max"] + 1):
                i = y * n + x
                if reg[i] == rid and wat[i] == 0 and (x, y) not in tk:
                    yield x, y, hab[i], ele[i]

    def wild_cell(self, rid):
        """6.6.1: the smallest habitability, then the largest elevation, then smaller y, x; else the seed cell."""
        best = None
        for x, y, h, e in self.cells_of(rid):
            k = (h, -e, y, x)
            if best is None or k < best[0]:
                best = (k, (x, y))
        return best[1] if best is not None else (self.reg[rid]["seed_x"], self.reg[rid]["seed_y"])

    def height(self, rid):
        """6.6.1: the largest elevation, then smaller y, x; None: no outpost."""
        best = None
        for x, y, _h, e in self.cells_of(rid):
            k = (-e, y, x)
            if best is None or k < best[0]:
                best = (k, (x, y))
        return best[1] if best is not None else None

    # ---------------------------------------------------------------- names (9)
    def monster_name(self, k, m, place=None):
        key, kd = self.kinds[k - 1]
        cul = kd["naming_culture"]
        if p503() and kd["wake"] == "dead":   # P5-03 12.2: a host named for its place
            return self.place_name(kd, m, place), place
        forms = kd["name_forms"]
        if p503():   # 12.1: F' without the forms whose epithet an earlier monster has
            fs = [f for f in forms if not (epithet(f) and epithet(f) in self.epithets)]
            if not fs:
                fs = ["{root}"] if "{root}" in forms else list(forms)
            forms = fs
        d = pnr.with_forms(self.naming[cul], "monster", forms)
        name, _t = pnr.make_name(d, cul, "monster", self.inp.seed, self.culture_num[cul], m, self.w.registry)
        root = realm_root(name, kd["name_forms"])
        if p503():
            f = matched_form(name, kd["name_forms"])
            if f is not None and epithet(f):
                self.epithets.add(epithet(f))
        return name, root

    def place_name(self, kd, m, place):
        """12.2: start = pick(D(hist.names.monster; m, 0, 0), |F|); the first form from start, cyclic, whose name is
        at most 48 bytes and not in the registry; none: the start form's name and the smallest free numeral."""
        forms = kd["name_forms"]
        n = len(forms)
        start = pnr.pick(pnr.draw(cr.stream_key(self.inp.seed, "hist.names.monster"), self.w.year, m, 0, 0), n)
        first = None
        for j in range(n):
            nm = forms[(start + j) % n].replace("{place}", place)
            if first is None:
                first = nm
            if len(nm.encode()) <= 48 and nm not in self.w.registry:
                self.w.registry.add(nm)
                return nm
        kk = 2
        while first + " " + roman(kk) in self.w.registry:
            kk += 1
        nm = first + " " + roman(kk)
        if len(nm.encode()) > 64:
            pe.fatal("names: a name longer than 64 bytes")
        self.w.registry.add(nm)
        return nm

    def place_of(self, rid, at_settlement):
        """12.2: the settlement's name at a ruin, else the region's name in force at Y; a leading The is the."""
        if at_settlement:
            nm = self.w.stl(at_settlement)["name"]
        else:
            nm = self.reg[rid]["name"]
            rn = getattr(self.w, "renaming", None)
            if rn is not None:
                for r in sorted(rn.rows):
                    if r[0] == rid and r[1] <= self.w.year:
                        nm = r[2]
        return "the " + nm[4:] if nm.startswith("The ") else nm

    def lair_name(self, k, s, root, trace=None):
        """9.2: cycle the kind's lair_forms from start = pick(D(lair_name; s, 0, 0), n)."""
        forms = self.kind(k)["lair_forms"]
        n = len(forms)
        start = pnr.pick(self.w.draw("hist.monsters.lair_name", s, 0, 0), n)
        first = None
        for j in range(n):
            f = forms[(start + j) % n]
            nm, parts = fill_form(f, "{root}", root)
            if first is None:
                first = nm
            prob = name_problem(nm, parts)
            if prob is None and nm in self.w.registry:
                prob = "registry"
            if trace is not None:
                trace.append(((start + j) % n, nm, prob))
            if prob is None:
                self.w.registry.add(nm)
                return nm
        kk = 2
        while first + " " + roman(kk) in self.w.registry:
            kk += 1
        nm = first + " " + roman(kk)
        if len(nm.encode()) > 64:
            pe.fatal("names: a name longer than 64 bytes")
        self.w.registry.add(nm)
        return nm

    def outpost_name(self, c, s):
        cul = self.culture_key[self.w.civ(c)["culture"]]
        name, _t = pnr.make_name(self.naming[cul], cul, "site", self.inp.seed, self.culture_num[cul], s,
                                 self.w.registry)
        return name

    # ---------------------------------------------------------------- 6.2 beasts
    def init_beasts(self):
        t = self.t
        for r in self.regions:
            for j, (_key, b) in enumerate(self.beasts):
                s = j + 1
                h = self.habitat(r, b["habitat"])
                F = self.M if b["menace"] > 0 else 1000
                k0 = checked_mul(checked_mul(h, b["density"]), F) // (10000 * 1000)
                if k0 > BEAST_MAX:
                    pe.fatal("history: bound: beast count exceeds 1000000")
                self.H[(r, s)], self.K0[(r, s)], self.B[(r, s)] = h, k0, k0
            self.d[r] = 0
            self.bold[r] = False
        self.started = True

    def write_rows(self, year):
        for r in self.regions:
            for s in range(1, len(self.beasts) + 1):
                b = self.B[(r, s)]
                if b != self.last_row.get((r, s), 0):
                    self.rows.append((r, s, year, b))
                    self.last_row[(r, s)] = b

    def beast_update(self, L):
        w, t = self.w, self.t
        if not self.started:
            self.init_beasts()
        if self.last_phase is not None and w.phase != self.last_phase:
            self.write_rows(self.last_end)
        P = self.region_pop()
        for r in self.regions:
            self.d[r] = min(t["displace_max_permille"], P[r] * t["displace_per_kpeople"] // 1000)
        for _ in range(L):
            for r in self.regions:
                for j, (_key, b) in enumerate(self.beasts):
                    s = j + 1
                    K = self.K0[(r, s)] * (1000 - self.d[r]) // 1000
                    B = self.B[(r, s)]
                    if B < K:
                        B2 = 1 if B == 0 else B + max(1, checked_mul(checked_mul(B, b["growth_permille"]), K - B)
                                                      // (1000 * K))
                        B2 = min(B2, K)
                    elif B > K:
                        B2 = B - max(1, (B - K) * t["beast_decline_permille"] // 1000)
                    else:
                        B2 = B
                    if B2 > BEAST_MAX:
                        pe.fatal("history: bound: beast count exceeds 1000000")
                    self.B[(r, s)] = B2
        for r in self.regions:
            Pd = max(P[r], t["people_floor"])
            pred = sum(self.B[(r, j + 1)] * b["menace"] for j, (_k, b) in enumerate(self.beasts))
            game = sum(self.B[(r, j + 1)] * b["food"] for j, (_k, b) in enumerate(self.beasts))
            drag = min(t["drag_max_permille"], pred * t["drag_k"] // Pd)
            feed = min(t["feed_max_permille"], game * t["feed_k"] // Pd)
            self.g[r] = 1000 - drag + feed
            self.menace[r] = pred * 1000 // Pd
        self.last_end = w.year + L - 1
        if not self.first_done:
            self.write_rows(self.last_end)
            self.first_done = True
        self.last_phase = w.phase

    def finish(self, E):
        if self.started:
            self.write_rows(E - 1)

    def beasts_bold(self):
        """6.2.3 (T4)."""
        w, t = self.w, self.t
        for r in self.regions:
            sts = [s for s in w.settlements if s["region"] == r and s["status"] in (1, 2) and w.visible(s)]
            if not sts:
                continue
            mn = self.menace.get(r, 0)
            if self.bold[r] and mn < t["bold_off_permille"]:
                self.bold[r] = False
            elif not self.bold[r] and mn >= t["bold_on_permille"] and \
                    chance_pm(w.draw("hist.beasts.bold", r, 0, 0), t["bold_permille"]):
                tt = min(sts, key=lambda s: (-s["population"], s["id"]))
                P = tt["population"]
                dead = min(P - 1, max(1, P * t["bold_toll_permille"] // 1000))
                if p503():   # P5-03 13: a weighted draw over B x menace
                    ws = [self.B[(r, s)] * self.beasts[s - 1][1]["menace"] for s in range(1, len(self.beasts) + 1)]
                    b = 1 + pnr.weighted(w.draw("hist.beasts.bold_kind", r, 0, 0), ws)
                else:
                    b = min(range(1, len(self.beasts) + 1),
                            key=lambda s: (-self.B[(r, s)] * self.beasts[s - 1][1]["menace"], s))
                e = w.append("beasts_bold", region=r, settlement=tt["id"],
                             participants=(("settlement", SETTLEMENT, tt["id"]), ("civilization", CIV, tt["owner_civ"])),
                             values=(b, dead))
                if dead > 0:
                    tt["population"] = P - dead
                w.last_red[tt["id"]] = e    # note_reduction (M2)
                self.bold[r] = True
                self.log.append((w.year, "bold", r, tt["id"], b, dead))

    # ---------------------------------------------------------------- 6.3 appearances
    def appear(self, k, rid, key="monster_appeared", causes=(), at_settlement=0, quiet=False):
        w = self.w
        m = len(self.monsters) + 1
        s = len(self.sites) + 1
        if m > MONSTERS_MAX:
            pe.fatal("history: bound: monsters exceed 100000")
        if s > SITES_MAX:
            pe.fatal("history: bound: sites exceed 1000000")
        name, root = self.monster_name(k, m, self.place_of(rid, at_settlement)
                                       if p503() and self.kind(k)["wake"] == "dead" else None)
        if at_settlement:
            x0 = w.stl(at_settlement)
            cell = (x0["x"], x0["y"])
        else:
            cell = self.wild_cell(rid)
        lname = self.lair_name(k, s, root)
        e = w.append(key, region=rid, settlement=at_settlement if key == "undead_rose" else 0,
                     participants=(("monster", MONSTER, m), ("lair", SITE, s)), causes=tuple(causes))
        self.sites.append(dict(id=s, name=lname, kind=LAIR, region=rid, x=cell[0], y=cell[1],
                               settlement=at_settlement, founder=0, monster=m, origin_year=w.year, origin_event=e,
                               ended_year=None, ending_event=0, ending=OPEN, hostile=None))
        self.monsters.append(dict(id=m, kind=k, name=name, root=root, status=AWAKE, born=w.year, born_event=e,
                                  lair=s, waking=e, sleeping=0, drive_until=0, hoard=0,
                                  # holds: set only by an overrun (6.4.3; the lead's ruling on integration D1):
                                  # a host risen at a ruin (6.3.2) may still overrun once
                                  holds=False, raids={}, died=None, death_event=0, slayer=0,
                                  overrun=0, raid_years={}, place_raids={},
                                  dead_kind=self.kind(k)["wake"] == "dead"))
        self.created_this_step = True
        if quiet and self.kind(k)["flare"]:
            self.pending_flare = e
        self.log.append((w.year, key, m, rid))
        return m

    def wild_appearance(self, quiet):
        """6.3.1 (T4, Q4): at most one creation per step."""
        w, t = self.w, self.t
        L = w.years if quiet else 1
        for k in range(1, len(self.kinds) + 1):
            kd = self.kind(k)
            if kd["wake"] != "wild":
                continue
            for r in self.regions:
                if self.created_this_step:
                    return
                if w.geo.islet(r):
                    continue
                if not self.caps_allow(k, r):
                    continue
                hk = self.habitat(r, kd["biomes"])
                if hk <= 0:
                    continue
                p = checked_mul(kd["wake_rate"], hk) // 10 * self.M // 1000 * (1000 - self.d.get(r, 0)) // 1000 * L
                if quiet and w.quiet_events_left(0) <= 0:
                    continue
                if chance_ppb(w.draw("hist.monsters.wake", r, k, 0), min(p, 1000000000)):
                    self.appear(k, r, quiet=quiet)
                    return

    def read_log(self):
        """6.3.2's read: battles and sacks into W[r]; then the window."""
        w, t = self.w, self.t
        last = len(w.events)
        for e in range(self.cursor, last + 1):
            ev = w.events[e - 1]
            key = ev["key"] if "key" in ev else None
            r = ev["region"]
            if r == 0:
                continue
            if key in ("battle", "battle_at_pass", "allied_battle", "battle_led"):   # P5-04, P5-06 11.1
                dead = ev["values"][1] + ev["values"][2]
            elif key == "sacked":
                dead = ev["values"][1]
            else:
                continue
            self.W.setdefault(r, []).append((ev["year"], dead, e))
        self.cursor = last + 1
        Y = w.year
        for r in list(self.W):
            self.W[r] = [x for x in self.W[r] if x[0] > Y - t["undead_window_years"]]

    def undead(self):
        w, t = self.w, self.t
        # P5-03a 6: P_W, the summed population of the standing settlements of State (hidden ones included)
        P_W = sum(s["population"] for s in w.settlements if w.standing(s)) if p503a() else 0
        for r in self.regions:
            ent = self.W.get(r, [])
            if riders():   # P5-06 25.3: the population term scaled by undead_regions_ref; no rise below normal
                need = undead_threshold_027(t["undead_min_dead"], t["undead_dead_permille"], P_W, self.M,
                                            self.R, t.get("undead_regions_ref", 0))
            elif p503a():
                need = undead_threshold_025(t["undead_min_dead"], t["undead_dead_permille"], P_W, self.M)
            else:
                need = t["undead_min_dead"] * 1000 // max(1, self.M) if p503() else t["undead_min_dead"]   # P5-03 14.2
            if sum(x[1] for x in ent) < need:
                continue
            for k in range(1, len(self.kinds) + 1):
                if self.created_this_step:
                    return
                kd = self.kind(k)
                if kd["wake"] != "dead" or not self.caps_allow(k, r):
                    continue
                if not chance_ppm(w.draw("hist.monsters.rise", r, k, 0), kd["wake_rate"] * self.M // 1000):
                    continue
                top = sorted(ent, key=lambda x: (-x[1], x[2]))
                causes = sorted(x[2] for x in top[:8])
                at = 0
                big = top[0]
                bev = w.events[big[2] - 1]
                if bev["key"] == "sacked":
                    x = bev["settlement"]
                    if x and w.stl(x)["status"] in (3, 4) and not any(
                            s["settlement"] == x and s["ending"] == OPEN and occupies(s) for s in self.sites):
                        at = x
                self.appear(k, r, key="undead_rose", causes=causes, at_settlement=at)
                self.W[r] = []
                return

    # ---------------------------------------------------------------- 6.4 the monsters' year
    def end_site(self, sid, e, ending, year=None):
        s = self.site(sid)
        s["ended_year"], s["ending_event"], s["ending"] = self.w.year if year is None else year, e, ending

    def die_of_age(self, m):
        w = self.w
        e = w.append("monster_died", region=self.lair_region(m), participants=(("monster", MONSTER, m["id"]),),
                     causes=(m["born_event"],))
        m["status"], m["died"], m["death_event"] = DIED, w.year, e
        self.end_site(m["lair"], e, END_DIED)
        self.log.append((w.year, "died", m["id"]))

    def monsters_year(self):
        w, t = self.w, self.t
        Y = w.year
        n = len(self.monsters)
        for mid in range(1, n + 1):
            m = self.monsters[mid - 1]
            if m["status"] in (SLAIN, DIED):
                continue
            kd = self.kind(m["kind"])
            if kd["lifespan_years"] > 0 and Y >= m["born"] + kd["lifespan_years"]:
                self.die_of_age(m)
                continue
            if m["status"] == ASLEEP:
                if Y >= m["drive_until"] and chance_ppm(w.draw("hist.monsters.rewake", mid, 0, 0),
                                                        kd["rewake_ppm"] * self.M // 1000):
                    e = w.append("monster_woke", region=self.lair_region(m),
                                 participants=(("monster", MONSTER, mid),), causes=(m["sleeping"],))
                    m["status"], m["waking"] = AWAKE, e
                    self.log.append((Y, "woke", mid))
                continue
            if chance_pm(w.draw("hist.monsters.sleep", mid, 0, 0), max(1, 1000 // kd["active_years"])):
                e = w.append("monster_slept", region=self.lair_region(m),
                             participants=(("monster", MONSTER, mid), ("lair", SITE, m["lair"])),
                             causes=(m["waking"],))
                m["status"], m["sleeping"] = ASLEEP, e
                self.log.append((Y, "slept", mid))
                continue
            if chance_pm(w.draw("hist.monsters.raid", mid, 0, 0), min(1000, kd["raid_permille"] * self.M // 1000)):
                self.raid(m, kd)
            if m["status"] == AWAKE:
                self.hunts(m, kd)

    def prey_weight(self, kd, kind, h, capital):
        """6.4.1 (r1): w(u), floored factor by factor: base (with might_f for towns, cities and fortresses), hop_f
        iterated with a floor per hop, w = base x hop_f div 1000, then the capital factor."""
        t = self.t
        might_f = min(t["prey_might_max_permille"], checked_mul(kd["might"], 1000) // t["prey_might_ref"])
        base = t["prey_weight"][pp.KIND_KEY[kind]]
        if kind >= 3:
            base = checked_mul(base, might_f) // 1000
        hop_f = 1000
        for _ in range(h):
            hop_f = checked_mul(hop_f, t["prey_hop_permille"]) // 1000
        wt = checked_mul(base, hop_f) // 1000
        if capital:
            wt = checked_mul(wt, t["prey_capital_permille"]) // 1000
        return wt

    def raid_target(self, m, kd, hops):
        """6.4.1 (r1): the candidates in ascending settlement id, their weights and the weighted draw on
        hist.monsters.raid_target; None (and no draw) with no candidate or W = 0."""
        w = self.w
        cands = [s for s in sorted(w.settlements, key=lambda x: x["id"])
                 if s["status"] in (1, 2) and w.visible(s) and s["owner_civ"] >= 1
                 and s["region"] in hops and hops[s["region"]] <= kd["range_hops"]]
        if not cands:
            return None
        ws = [self.prey_weight(kd, s["kind"], hops[s["region"]], w.civ(s["owner_civ"])["capital"] == s["id"])
              for s in cands]
        if sum(ws) == 0:
            return None
        return cands[pnr.weighted(w.draw("hist.monsters.raid_target", m["id"], 0, 0), ws)]

    def raid(self, m, kd):
        w = self.w
        hops = self.geo.hops(self.lair_region(m))
        tt = self.raid_target(m, kd, hops)
        if tt is None:
            return
        P = tt["population"]
        c = tt["owner_civ"]
        tau = kd["toll_min_permille"] + pnr.pick(w.draw("hist.monsters.raid_toll", m["id"], 0, 0),
                                                 kd["toll_max_permille"] - kd["toll_min_permille"] + 1)
        causes = (m["waking"],)
        if p503():   # 9.2 the toll above kill_max; 9.1 the last raid on the same place within raid_chain_years
            t = self.t
            raw = P * tau // 1000
            dead = raw if raw <= kd["kill_max"] else kd["kill_max"] + (raw - kd["kill_max"]) * t["toll_over_permille"] // 1000
            dead = max(0, min(dead, P - 1))
            keep = max(t["raid_chain_years"], t["fear_years"])
            lst = [x for x in m["place_raids"].get(tt["id"], []) if w.year - x[0] < keep]
            prev = [x for x in lst if w.year - x[0] < t["raid_chain_years"]]
            if prev:
                causes = tuple(sorted({m["waking"], prev[-1][1]}))
        else:
            dead = max(0, min(kd["kill_max"], P * tau // 1000, P - 1))
        plunder = P * kd["hoard_permille"] // 1000
        e = w.append("monster_raid", region=tt["region"], settlement=tt["id"],
                     participants=(("monster", MONSTER, m["id"]), ("civilization", CIV, c)), values=(dead, plunder),
                     causes=causes)
        if p503():
            m["place_raids"][tt["id"]] = lst + [(w.year, e)]
            m["raid_years"].setdefault(c, []).append(w.year)
        if dead > 0:
            tt["population"] = P - dead
        w.last_red[tt["id"]] = e    # note_reduction (M2)
        m["hoard"] += plunder
        m["raids"][c] = (w.year, e)
        self.log.append((w.year, "raid", m["id"], tt["id"], dead, hops[tt["region"]]))
        self.overrun(m, kd, tt, c, e)
        if p503() and tt["status"] in (1, 2):
            self.fear(m, tt, c)

    def fear(self, m, tt, c):
        """P5-03 9.4: the flight for fear after a raid and its overrun test."""
        w, t = self.w, self.t
        recent = [x for x in m["place_raids"].get(tt["id"], []) if x[0] > w.year - t["fear_years"]]
        if len(recent) < t["fear_raids"] or pp.KIND_KEY[tt["kind"]] not in ("hamlet", "village"):
            return
        if w.civ(c)["capital"] == tt["id"] or not any(x["id"] != tt["id"] for x in w.standing_of(c)):
            return
        if not chance_pm(w.draw("hist.monsters.fear", m["id"], tt["id"], 0), t["fear_permille"]):
            return
        e = w.append("settlement_fled", region=tt["region"], settlement=tt["id"],
                     participants=(("monster", MONSTER, m["id"]), ("civilization", CIV, c)),
                     causes=tuple(sorted(x[1] for x in recent[-4:])))
        w.abandon(tt["id"], e, causes_given=(e,))
        self.log.append((w.year, "fled", m["id"], tt["id"], c, e))

    def overrun(self, m, kd, tt, c, raid_e):
        w = self.w
        if m["holds"] or pp.KIND_KEY[tt["kind"]] not in kd["overruns"] or tt["status"] not in (1, 2):
            return
        if w.civ(c)["capital"] == tt["id"]:
            return
        if not any(x["id"] != tt["id"] for x in w.standing_of(c)):
            return
        if self.open_lair_in(tt["region"], but=m["id"]):
            return
        p_ov = kd["overrun_permille"]
        if p503():   # P5-03 14.1, 14.2: scaled by M and by the target's population after the raid
            fP = min(1000, self.t["overrun_people_ref"] * 1000 // max(1, tt["population"]))
            p_ov = min(1000, p_ov * self.M // 1000) * fP // 1000
        if not chance_pm(w.draw("hist.monsters.overrun", m["id"], tt["id"], 0), p_ov):
            return
        s2 = len(self.sites) + 1
        lname = self.lair_name(m["kind"], s2, m["root"])
        e = w.append("settlement_overrun", region=tt["region"], settlement=tt["id"],
                     participants=(("monster", MONSTER, m["id"]), ("civilization", CIV, c), ("lair", SITE, s2)),
                     values=(tt["population"],), causes=(raid_e,))
        self.sites.append(dict(id=s2, name=lname, kind=LAIR, region=tt["region"], x=tt["x"], y=tt["y"],
                               settlement=tt["id"], founder=0, monster=m["id"], origin_year=w.year, origin_event=e,
                               ended_year=None, ending_event=0, ending=OPEN, hostile=None))
        w.ruin(tt["id"], e)
        self.end_site(m["lair"], e, END_MOVED)
        m["lair"], m["holds"], m["overrun"] = s2, True, e
        self.log.append((w.year, "overrun", m["id"], tt["id"], c))

    def hunts(self, m, kd):
        w, t = self.w, self.t
        Y = w.year
        lr = self.lair_region(m)
        hops = self.geo.hops(lr)
        for c in sorted(m["raids"]):
            year, raid_e = m["raids"][c]
            if Y - year > t["hunt_memory_years"]:
                continue
            civ = w.civ(c)
            if civ["ended_year"] is not None or not w.visible(civ):
                continue
            H = sum(u["population"] * t["hunt_levy_permille"] // 1000 for u in w.settlements
                    if u["owner_civ"] == c and u["status"] in (1, 2) and w.visible(u)
                    and u["region"] in hops and hops[u["region"]] <= t["hunt_hops"])
            hero = self.hero_of(c)   # P5-06 7.7: (figure, hunt_permille, might_permille); (0, 0, 0) none
            S = H * self.prowess(civ["race"]) // 1000 * (1000 + hero[2]) // 1000
            if S == 0:
                continue
            p_h = t["hunt_permille"]
            if p503():   # P5-03 9.3 (reading H1: R_c counts the raids with Y - year <= hunt_memory_years)
                R = sum(1 for yr in m["raid_years"].get(c, []) if Y - yr <= t["hunt_memory_years"])
                p_h = min(t["hunt_max_permille"] + hero[1], t["hunt_permille"] + (max(1, R) - 1) *
                          t["hunt_raid_permille"] + hero[1])
            elif hero[0]:
                p_h = p_h + hero[1]
            if not chance_pm(w.draw("hist.monsters.hunt", m["id"], c, 0), p_h):
                continue
            odds = min(t["slay_max_permille"], S * 1000 // (S + kd["might"]))
            u = pnr.pick(w.draw("hist.monsters.hunt_outcome", m["id"], c, 0), 1000)
            if u < odds:
                if hero[0]:   # P5-06 7.7: the hero's slaying
                    e = w.append("monster_slain_by_hero", region=lr,
                                 participants=(("monster", MONSTER, m["id"]), ("slayer", CIV, c),
                                               ("hero", pe.FIGURE, hero[0])), values=(H,), causes=(raid_e,))
                else:
                    e = w.append("monster_slain", region=lr, participants=(("monster", MONSTER, m["id"]),
                                                                           ("slayer", CIV, c)),
                                 values=(H,), causes=(raid_e,))
                m["status"], m["died"], m["death_event"], m["slayer"] = SLAIN, Y, e, c
                self.end_site(m["lair"], e, END_SLAIN)
                self.log.append((Y, "slain", m["id"], c, H, S, odds, u))
                return
            if u < odds + (1000 - odds) * t["drive_permille"] // 1000:
                e = w.append("monster_driven_off", region=lr, participants=(("monster", MONSTER, m["id"]),
                                                                            ("civilization", CIV, c)),
                             causes=(raid_e,))
                m["status"], m["sleeping"], m["drive_until"] = ASLEEP, e, Y + t["drive_years"]
                self.log.append((Y, "driven", m["id"], c, H, S, odds, u))
                return
            self.log.append((Y, "hunt failed", m["id"], c, H, S, odds, u))
            if hero[0] and chance_pm(w.draw("hist.monsters.hero_fall", m["id"], c, 0),
                                     w.figures.t["hero_fall_permille"]):
                # P5-06 7.7: a failed hunt led by a hero may kill the hero; either ends the hunts of the year
                w.append("hero_slain", region=lr, participants=(("figure", pe.FIGURE, hero[0]),
                                                                ("civilization", CIV, c),
                                                                ("monster", MONSTER, m["id"])), causes=(raid_e,))
                self.heroes_slain_now.add(hero[0])
                self.log.append((Y, "hero slain", m["id"], c, hero[0]))
                return

    def prowess(self, race):
        return self.w.pd.race[race]["prowess"]

    def hero_of(self, c):
        """P5-06 7.7: FigureView::hero_of(c) as (figure, hunt_permille, might_permille); (0, 0, 0) without the view
        or a hero. Reading F-G1 (spec gap): a hero that a hero_slain of this T4 named is no longer the realm's hero for
        the rest of the step (the figures subsystem reads its death only in its own T4, after the monsters)."""
        fv = getattr(self.w, "figures", None)
        if fv is None:
            return (0, 0, 0)
        h = fv.hero_of(c)
        if h[0] == 0 or h[0] in self.heroes_slain_now:
            return (0, 0, 0)
        return h

    # ---------------------------------------------------------------- 6.6 outposts
    def at_war(self, a, b):
        rel = self.w.relations
        return (CIV, a, CIV, b, "war") in rel or (CIV, b, CIV, a, "war") in rel

    def war_event(self, a, b):
        rel = self.w.relations
        return rel.get((CIV, a, CIV, b, "war")) or rel.get((CIV, b, CIV, a, "war"))

    def hostile(self, a, b):
        return self.at_war(a, b) or (CIV, min(a, b), CIV, max(a, b), "enmity") in self.w.relations

    def open_outposts(self, c=None):
        return [s for s in self.sites if s["kind"] == OUTPOST and s["ending"] == OPEN
                and (c is None or s["founder"] == c)]

    def outposts_raised(self):
        w, t = self.w, self.t
        for c in range(1, len(w.civs) + 1):
            civ = w.civ(c)
            if civ["ended_year"] is not None or not w.visible(civ):
                continue
            if len(self.open_outposts(c)) >= t["outpost_max"]:
                continue
            E = [e for e in range(1, len(w.civs) + 1) if e != c and w.civ(e)["ended_year"] is None
                 and self.at_war(c, e)]
            if not E:
                continue
            for R in self.regions:
                if w.owner[R][0] != c or w.geo.islet(R):
                    continue
                if any(s["region"] == R for s in self.open_outposts()):
                    continue
                foes = [w.owner[x][0] for x in self.geo.nb[R] if w.owner[x][0] in E]
                if not foes:
                    continue
                if not chance_pm(w.draw("hist.sites.outpost", c, R, 0), t["outpost_permille"]):
                    continue
                cell = self.height(R)
                if cell is None:
                    continue
                s = len(self.sites) + 1
                nm = self.outpost_name(c, s)
                e = min(foes)
                ev = w.append("outpost_built", region=R, participants=(("civilization", CIV, c), ("enemy", CIV, e),
                                                                       ("site", SITE, s)),
                              causes=(self.war_event(c, e),))
                self.sites.append(dict(id=s, name=nm, kind=OUTPOST, region=R, x=cell[0], y=cell[1], settlement=0,
                                       founder=c, monster=0, origin_year=w.year, origin_event=ev, ended_year=None,
                                       ending_event=0, ending=OPEN, hostile=w.year))
                self.log.append((w.year, "outpost", s, c, R, e))
                break

    def outposts_left(self, quiet):
        w, t = self.w, self.t
        Y = w.year
        for s in list(self.open_outposts()):
            c, R = s["founder"], s["region"]
            if any(w.owner[x][0] and self.hostile(c, w.owner[x][0]) for x in self.geo.nb[R]):
                s["hostile"] = Y
            civ = w.civ(c)
            if civ["ended_year"] is not None:
                reason, cause, ending = 2, civ["ending_event"], END_FELL
            elif w.owner[R][0] != c:
                reason, cause, ending = 1, w.owner[R][1], END_LOST
            elif Y - s["hostile"] >= t["outpost_quiet_years"]:
                reason, cause, ending = 0, 0, END_QUIET
            else:
                continue
            if quiet and w.quiet_events_left(c) <= 0:
                continue
            e = w.append("outpost_abandoned", region=R, participants=(("civilization", CIV, c), ("site", SITE, s["id"])),
                         values=(reason,), causes=((cause,) if cause else ()))
            self.end_site(s["id"], e, ending)
            self.log.append((Y, "outpost left", s["id"], reason))

    # ---------------------------------------------------------------- the steps (6.1)
    def t1(self):
        self.created_this_step = False
        self.beast_update(1)

    def q1(self):
        self.created_this_step = False
        self.beast_update(self.w.years)

    def t4(self):
        self.created_this_step = False
        self.heroes_slain_now = set()
        self.read_log()
        self.beasts_bold()
        self.monsters_year()
        self.undead()
        if not self.created_this_step:
            self.wild_appearance(quiet=False)

    def t6(self):
        self.outposts_left(quiet=False)
        self.outposts_raised()

    def q4(self):
        w = self.w
        self.created_this_step = False
        self.pending_flare = 0
        Y, L = w.year, w.years
        for m in self.monsters:
            if m["status"] not in (AWAKE, ASLEEP):
                continue
            kd = self.kind(m["kind"])
            if kd["lifespan_years"] > 0 and m["born"] + kd["lifespan_years"] <= Y + L - 1 and \
                    w.quiet_events_left(0) > 0:
                self.die_of_age(m)
        self.wild_appearance(quiet=True)
        self.outposts_left(quiet=True)

    def tension(self, start, length):
        """6.5: pure."""
        v = (self.threshold, self.pending_flare) if self.pending_flare else (0, 0)
        self.tension_log.append((start, v[0], v[1]))
        return v

    # ---------------------------------------------------------------- rows (11.2)
    def monster_rows(self):
        return [(m["id"], m["name"], m["kind"], m["lair"], m["born"], m["born_event"], m["died"], m["death_event"],
                 m["slayer"], m["status"], m["hoard"]) for m in self.monsters]

    def site_rows(self):
        return [(s["id"], s["name"], s["kind"], s["region"], s["x"], s["y"], s["settlement"], s["founder"],
                 s["monster"], s["origin_year"], s["origin_event"], s["ended_year"], s["ending_event"], s["ending"])
                for s in self.sites]

    def beast_population_rows(self):
        return sorted(self.rows)

    def monster_name_of(self, mid):
        """MonstersSubsystem::monster_name(id) (9.3): the monster's name."""
        return self.monsters[mid - 1]["name"]

    def monster_root_of(self, mid):
        m = self.monsters[mid - 1]
        if p503() and m.get("dead_kind"):   # P5-03 12.2: a host's root is its place; monster_root is empty
            return ""
        return m["root"]


def pe_biome_keys():
    import p3_conflict_reference as pcr
    return dict(pcr.BIOME_KEYS)


def epithet(form):
    """P5-03 12.1."""
    return form.replace("{root}", "").strip(" ")


def matched_form(name, forms):
    """The form A{root}B a name's root is matched by (P5-02 9.2, list order), else None."""
    for f in forms:
        if "{root}" not in f:
            continue
        a, b = f.split("{root}", 1)
        if name.startswith(a) and name.endswith(b) and len(name) > len(a) + len(b):
            return f
    return None


def realm_root(name, forms):
    import p5_12_renaming_reference as p512
    return p512.realm_root(name, forms)


def name_problem(nm, parts):
    import p5_12_renaming_reference as p512
    return p512.name_problem(nm, parts)


# ---------------------------------------------------------------------------
# Fixture data (19.3): monsters-small and zero (I0). In-memory edits of a data set, as the conflict reference's
# stress data (READINGS X1: the "hand-made root" of 19.3 is these edits applied to the fixture root geo).
# ---------------------------------------------------------------------------

MONSTERS_SMALL_KINDS = dict(wake_rate_x=2000, overrun_permille=1000)
MONSTERS_SMALL_TUNING = dict(living_permille=1000)
ZERO_TUNING = dict(pressure=dict(low=0, normal=0, high=0), drag_max_permille=0, feed_max_permille=0, bold_permille=0,
                   outpost_permille=0)


def with_monsters(dat, tuning=None, kinds=None, ghost=None):
    """A copy of a conflict-reference Data whose base holds edited monster data."""
    import p3_conflict_reference as pcr
    d = pcr.edited(dat)
    d.base = copy.copy(dat.base)
    d.base.monsters_tuning = dict(copy.deepcopy(dat.base.monsters_tuning), **(tuning or {}))
    d.base.monsters = [(k, dict(copy.deepcopy(v), **(kinds(k, v) if kinds else {}))) for k, v in dat.base.monsters]
    if ghost is not None:
        d.peoples = dict(d.peoples, **ghost)
    return d


def monsters_small_data(dat=None):
    """19.3 monsters-small: the conflict stress data with every monster wake_rate raised (x 2000, at most 10^7),
    living_permille 1000 and overrun_permille 1000."""
    import p3_conflict_reference as pcr
    base = pcr.stress_data(dat)
    return with_monsters(base, MONSTERS_SMALL_TUNING, lambda k, v: dict(
        wake_rate=min(10000000, v["wake_rate"] * MONSTERS_SMALL_KINDS["wake_rate_x"]),
        overrun_permille=MONSTERS_SMALL_KINDS["overrun_permille"]))


def zero_data(dat=None):
    """7.5 the fixture tuning `zero`: every pressure factor 0, drag and feed caps 0, bold_permille 0,
    outpost_permille 0, peoples' ghost_permille 0."""
    import p3_conflict_reference as pcr
    return with_monsters(dat or pcr.data(), ZERO_TUNING, ghost=dict(ghost_permille=0))


# ---------------------------------------------------------------------------
# 14: invariants on rows (res, or a world file's tables)
# ---------------------------------------------------------------------------

def rows_from_db(path):
    """{table: rows in primary-key order} of a world file, plus meta and the geography the checks read."""
    import p3_chronology_reference as pchr
    conn = pchr.open_world(path)
    out = {}
    for t in pe.TABLES + ("region_border", "region"):
        try:
            out[t] = pchr.world_table_hash(conn, t)[1]
        except sqlite3.Error:
            out[t] = []
    out["meta"] = dict(conn.execute("SELECT key, value FROM meta").fetchall())
    conn.close()
    return out


def toll_dead(P, tau, kill_max, over_permille):
    """P5-03 9.2: raw = P x tau div 1000; above kill_max only toll_over_permille of the rest; at most P - 1."""
    raw = P * tau // 1000
    dead = raw if raw <= kill_max else kill_max + (raw - kill_max) * over_permille // 1000
    return max(0, min(dead, P - 1))


class Rows:
    """Indexes over a world's history rows (res-style tuples)."""

    def __init__(self, res, borders=None):
        self.res = res
        self.key = {k[0]: k[1] for k in res["event_kind"]}
        self.kid = {k[1]: k[0] for k in res["event_kind"]}
        self.ev = {e[0]: e for e in res["event"]}
        self.parts = {}
        for p in res["event_participant"]:
            self.parts.setdefault(p[0], []).append(p[1:])
        self.causes = {}
        for e, c in res["event_cause"]:
            self.causes.setdefault(e, []).append(c)
        self.vals = {}
        for e, seq, v in res["event_value"]:
            self.vals.setdefault(e, {})[seq] = v
        self.vkeys = {}
        for k, seq, key in res["event_kind_value"]:
            self.vkeys.setdefault(k, {})[key] = seq
        self.rkeys = {}
        for k, seq, role, _t, _lo, _hi in res["event_kind_role"]:
            self.rkeys.setdefault(k, {})[role] = seq
        self.phase = {p[0]: p for p in res["phase"]}
        self.stl = {s[0]: s for s in res["settlement"]}
        self.civ = {c[0]: c for c in res["civilization"]}
        self.monsters = list(res.get("monster", []))
        self.sites = list(res.get("site", []))
        self.census = {}
        for r in res.get("settlement_census", []):
            self.census.setdefault(r[0], []).append(r)
        nb = {}
        for b in (borders or []):
            nb.setdefault(b[0], set()).add(b[1])
            nb.setdefault(b[1], set()).add(b[0])
        self.nb = {r: sorted(v) for r, v in nb.items()}

    def kind_of(self, e):
        return self.key[self.ev[e][3]]

    def value(self, e, key):
        return self.vals.get(e, {}).get(self.vkeys[self.ev[e][3]][key])

    def role(self, e, role):
        rs = self.rkeys[self.ev[e][3]].get(role)
        return [x[3] for x in sorted(self.parts.get(e, [])) if x[1] == rs]

    def events_of(self, *keys):
        ids = set(self.kid[k] for k in keys if k in self.kid)
        return [e for e in self.res["event"] if e[3] in ids]

    def hops(self, r0):
        d = {r0: 0}
        fifo = [r0]
        i = 0
        while i < len(fifo):
            x = fifo[i]
            i += 1
            for y in self.nb.get(x, ()):
                if y not in d:
                    d[y] = d[x] + 1
                    fifo.append(y)
        return d

    def lair_at(self, m, e):
        """The lair site of monster m in force at event e (its origin_event <= e < ending_event, or open)."""
        for s in self.sites:
            if s[2] == LAIR and s[8] == m and s[10] <= e and (s[12] == 0 or e < s[12]):
                return s
        return None


MONSTER_EVENT_KINDS = ("monster_appeared", "undead_rose", "monster_woke", "monster_slept", "monster_raid",
                       "settlement_overrun", "monster_slain", "monster_driven_off", "monster_died",
                       "monster_slain_by_hero", "hero_rose", "hero_slain")   # P5-06 11.2: kinds naming a monster


def invariant_violations(res, kinds, tuning, E, borders, R=None, quiet_max=3, quarter=True):
    """Section 14's invariants 1, 3 to 11 on rows (2, 12 and 13 are checked in the run and elsewhere); kinds:
    [(key, def)] of the monster definitions (max_alive, range_hops, kill_max, overruns are data, not rows)."""
    x = Rows(res, borders)
    out = []
    R = R if R is not None else len(res.get("region", [])) or len({b for bb in borders for b in bb[:2]})
    Lw = max(1, R * tuning["living_permille"] // 1000)
    # 1
    last = {}
    for r, b, y, cnt in res.get("beast_population", []):
        if not 0 <= cnt <= BEAST_MAX:
            out.append("1: count %d of (%d, %d, %d)" % (cnt, r, b, y))
        if last.get((r, b)) == cnt:
            out.append("1: row (%d, %d, %d) repeats the count" % (r, b, y))
        if (r, b) not in last and cnt == 0:
            out.append("1: first row (%d, %d, %d) is 0" % (r, b, y))
        last[(r, b)] = cnt
    mon = {m[0]: m for m in x.monsters}
    # 3: at every event id, open lairs per region, living monsters, per kind
    marks = sorted(set([s[10] for s in x.sites] + [s[12] for s in x.sites if s[12]]))
    for e in marks:
        lairs = [s for s in x.sites if s[2] == LAIR and s[10] <= e and (s[12] == 0 or e < s[12])]
        regs = [s[3] for s in lairs]
        if len(regs) != len(set(regs)):
            out.append("3: two open lairs in one region at e%d" % e)
        alive = [m for m in x.monsters if m[5] <= e and (m[7] == 0 or e < m[7])]
        if len(alive) > Lw:
            out.append("3: %d living monsters at e%d above %d" % (len(alive), e, Lw))
        for k, (_key, kd) in enumerate(kinds, 1):
            if sum(1 for m in alive if m[2] == k) > kd["max_alive"]:
                out.append("3: kind %d above max_alive at e%d" % (k, e))
    # 4
    by_m = {}
    for e in x.events_of(*MONSTER_EVENT_KINDS):
        for mid in x.role(e[0], "monster"):
            by_m.setdefault(mid, []).append(e)
    for m in x.monsters:
        evs = by_m.get(m[0], [])
        app = [e for e in evs if x.key[e[3]] in ("monster_appeared", "undead_rose")]
        if [e[0] for e in app] != [m[5]]:
            out.append("4: monster %d appearances %s, born_event %d" % (m[0], [e[0] for e in app], m[5]))
        for e in evs:
            if e[0] < m[5] or e[1] < m[4] or (m[6] is not None and (e[1] > m[6] or e[0] > m[7])):
                out.append("4: monster %d event e%d outside its life" % (m[0], e[0]))
        if sum(1 for e in evs if x.key[e[3]] == "settlement_overrun") > 1:
            out.append("4: monster %d overran twice" % m[0])
        sl = [e for e in evs if x.key[e[3]] in ("monster_slain", "monster_slain_by_hero")]   # P5-06 11.2
        di = [e for e in evs if x.key[e[3]] == "monster_died"]
        if (m[9] == SLAIN) != (len(sl) == 1) or len(sl) > 1 or (m[9] == DIED) != (len(di) == 1) or len(di) > 1:
            out.append("4: monster %d status %d with %d slain, %d died" % (m[0], m[9], len(sl), len(di)))
    # 5
    for e in x.events_of("monster_raid"):
        mid = x.role(e[0], "monster")[0]
        kd = kinds[mon[mid][2] - 1][1]
        lair = x.lair_at(mid, e[0])
        if lair is None:
            out.append("5: raid e%d of monster %d without a lair" % (e[0], mid))
            continue
        h = x.hops(lair[3]).get(e[5])
        if h is None or h > kd["range_hops"]:
            out.append("5: raid e%d at %s hops" % (e[0], h))
        dead = x.value(e[0], "dead")
        if tuning.get("toll_over_permille") is None:   # P5-02: at most kill_max
            if not 0 <= dead <= kd["kill_max"]:
                out.append("5: raid e%d dead %d" % (e[0], dead))
        elif dead < 0:   # P5-03 9.2 (P5-03a 12 item 4): above kill_max only at toll_over_permille of the rest
            out.append("5: raid e%d dead %d" % (e[0], dead))
        # (above kill_max: the toll of 9.2 needs the settlement's population at the raid, which the rows do not keep
        # (a census row is the state at a step's end): reading M5, only dead >= 0 is checked from rows; the run's own
        # tolls are checked against 9.2 in the replay)
    # 6
    for m in x.monsters:
        lair = x.sites[m[3] - 1]
        if lair[2] != LAIR or lair[8] != m[0]:
            out.append("6: monster %d lair_site %d is not its lair" % (m[0], m[3]))
        if m[9] in (AWAKE, ASLEEP) and lair[13] != OPEN:
            out.append("6: living monster %d with an ended lair" % m[0])
        if m[9] in (SLAIN, DIED) and (lair[13] == OPEN or lair[11] != m[6]):
            out.append("6: ended monster %d lair ended %s, died %s" % (m[0], lair[11], m[6]))
    for s in x.sites:
        # P5-04 9.2, P5-06 7.8: a mine's settlement is its town, a library's its city; rule 6 reads lairs and outposts
        if s[6] and s[2] in (LAIR, OUTPOST):
            k = x.kind_of(s[10])
            st = x.stl.get(s[6])
            if s[2] != LAIR or k not in ("settlement_overrun", "undead_rose"):
                out.append("6: site %d holds settlement %d from a %s" % (s[0], s[6], k))
            if st is None or st[10] not in (3, 4):
                out.append("6: site %d settlement %d not ruined or faded" % (s[0], s[6]))
            elif st[10] == 3 and st[12] is not None and st[12] > s[9]:
                out.append("6: site %d settlement %d ruined after the site's origin" % (s[0], s[6]))
    # 7
    ov = x.events_of("settlement_overrun")
    for e in ov:
        sid = e[6]
        if x.stl[sid][13] != e[0] and x.stl[sid][10] == 3:
            out.append("7: overrun e%d is not settlement %d's ruin event" % (e[0], sid))
    if quarter and 4 * len(ov) > len(res["settlement"]):
        out.append("7: %d overruns above a quarter of %d settlements" % (len(ov), len(res["settlement"])))
    # 8: no colony into a shunned region (an open lair with a settlement there at the colony's event)
    for e in x.events_of("colony_founded"):
        if any(s[2] == LAIR and s[6] and s[3] == e[5] and s[10] < e[0] and (s[12] == 0 or e[0] < s[12])
               for s in x.sites):
            out.append("8: colony e%d into shunned region %d" % (e[0], e[5]))
    # 9
    ctrl = res["region_control"]

    def owner_at(r, y):
        for row in ctrl:
            if row[0] == r and row[1] <= y and (row[2] is None or y < row[2]):
                return row[3]
        return 0
    for s in x.sites:
        if s[2] != OUTPOST:
            continue
        if owner_at(s[3], s[9]) != s[7]:
            out.append("9: outpost %d in region %d not owned by %d at %d" % (s[0], s[3], s[7], s[9]))
        if s[13] == OPEN and (x.civ[s[7]][8] is not None or owner_at(s[3], E - 1) != s[7]):
            out.append("9: open outpost %d's founder %d dead or not the owner at E" % (s[0], s[7]))
    # 10
    want = pe.endings_from_rows(res)
    for st in res["settlement"]:
        if len(st) < 15 or st[14] != want[st[0]]:
            out.append("10: settlement %d ending %s, derived %d" % (st[0], st[14] if len(st) > 14 else None,
                                                                  want[st[0]]))
    # 11: causes (10.2) and W9
    cause_kinds = {"undead_rose": ("battle", "battle_at_pass", "sacked", "allied_battle", "battle_led"),
                   "monster_slain_by_hero": ("monster_raid",), "hero_slain": ("monster_raid",),
                   "monster_woke": ("monster_slept",
                   "monster_driven_off"), "monster_slept": ("monster_appeared", "undead_rose", "monster_woke"),
                   "settlement_overrun": ("monster_raid",), "monster_slain": ("monster_raid",),
                   "monster_driven_off": ("monster_raid",), "monster_died": ("monster_appeared", "undead_rose"),
                   "outpost_built": ("war_declared", "ally_joined_war")}   # P5-04 7.6: a joined war row's event
    for k, allowed in cause_kinds.items():
        for e in x.events_of(k):
            cs = x.causes.get(e[0], [])
            if not cs or any(x.kind_of(c) not in allowed for c in cs) or (k != "undead_rose" and len(cs) != 1):
                out.append("11: %s e%d causes %s" % (k, e[0], [x.kind_of(c) for c in cs]))
    # P5-03 9.1 (P5-03a 12 item 4): a raid cites its monster's waking event and at most one earlier raid of the
    # monster on the same settlement within raid_chain_years (before P5-03: the waking event alone)
    waking = {}
    for e in x.events_of("monster_appeared", "undead_rose", "monster_woke"):
        for mid in x.role(e[0], "monster"):
            waking.setdefault(mid, set()).add(e[0])
    chain = tuning.get("raid_chain_years")
    for e in x.events_of("monster_raid"):
        cs = x.causes.get(e[0], [])
        mid = x.role(e[0], "monster")[0]
        wk = [c for c in cs if c in waking.get(mid, set())]
        rd = [c for c in cs if x.kind_of(c) == "monster_raid"]
        bad = len(wk) != 1 or len(wk) + len(rd) != len(cs) or len(rd) > (0 if chain is None else 1)
        for c in rd:
            ce = x.ev[c]
            if ce[6] != e[6] or x.role(c, "monster")[0] != mid or c >= e[0] or e[1] - ce[1] >= chain:
                bad = True
        if bad:
            out.append("11: monster_raid e%d causes %s" % (e[0], [x.kind_of(c) for c in cs]))
    for e in x.events_of("monster_appeared", "beasts_bold"):
        if x.causes.get(e[0]):
            out.append("11: %s e%d has causes" % (x.key[e[3]], e[0]))
    for e in x.events_of("undead_rose"):
        if len(x.causes.get(e[0], [])) > 8:
            out.append("11: undead_rose e%d has %d causes" % (e[0], len(x.causes[e[0]])))
    # W9: per quiet step (the events of a quiet phase row and year), per charged civilization, at most quiet_max
    charged = {}
    for e in res["event"]:
        ph = x.phase.get(e[4])
        if ph is None or ph[2] != 2 or x.key[e[3]] in ("quiet_era", "quiet_stretch"):
            continue
        civs = [p[3] for p in sorted(x.parts.get(e[0], [])) if p[2] == CIV]
        ch = civs[0] if civs else 0
        charged[(e[1], ch)] = charged.get((e[1], ch), 0) + 1
    for (y, ch), n in charged.items():
        if n > quiet_max:
            out.append("11 W9: %d quiet events of civilization %d in the step of %d" % (n, ch, y))
    return out


def res_violations(res, w, quarter=True):
    """invariant_violations on a run's res (the reference's own world), and the in-run invariant 2. quarter=False
    leaves out invariant 7's quarter bound (P5-00 8.3), a calibration target no rule enforces: the stress fixture
    monsters-small (overrun_permille 1000 and r1's prey on small places) exceeds it by design (26.2)."""
    m = w.monsters
    borders = [(b["a"], b["b"]) for b in res["_inputs"].borders] if "_inputs" in res else \
        [(b["a"], b["b"]) for b in w.inp.borders]
    out = invariant_violations(res, m.kinds, m.t, w.inp.params["span"], borders, R=m.R,
                               quiet_max=w.eng["quiet_events_max"], quarter=quarter)
    lo, hi = 1000 - m.t["drag_max_permille"], 1000 + m.t["feed_max_permille"]
    for r, g in m.g.items():
        if not lo <= g <= hi:
            out.append("2: g(%d) = %d outside %d to %d" % (r, g, lo, hi))
    return out


# ---------------------------------------------------------------------------
# 19.1: hand-made units on a stub world (p3_conflict_reference.unit_world: regions in a row, 20 x 64 cells each,
# grassland, walkable borders 1-2, 2-3, ...)
# ---------------------------------------------------------------------------

class Checker:
    def __init__(self):
        self.count = 0
        self.failures = 0

    def ok(self, what, cond):
        self.count += 1
        if not cond:
            self.failures += 1
            print("FAIL: %s" % what)

    def eq(self, what, got, want):
        self.count += 1
        if got != want:
            self.failures += 1
            print("FAIL: %s: got %r, want %r" % (what, got, want))


def stub(regions=4, tuning=None, kinds=None, biomes=None, races=("elf", "goblin")):
    """A unit world with a Monsters subsystem: (w, m). The world's own subsystem is replaced by one with the
    given tuning and kinds edits ({key: {field: value}})."""
    import p3_conflict_reference as pcr
    w = pcr.unit_world(regions=regions, races=races, biomes=biomes)
    base = w.dat if hasattr(w.dat, "monsters_tuning") else pcr.data().base
    t = dict(copy.deepcopy(base.monsters_tuning), **(tuning or {}))
    ks = [(k, dict(copy.deepcopy(v), **((kinds or {}).get(k, {})))) for k, v in base.monsters]
    m = Monsters(w, base, w.inp, tuning=t, kinds=ks)
    w.monsters = m
    w.set_wild(WildView(m))
    w.year = 1000
    w.years = 1
    w.phase = 1
    return w, m


def kind_id(m, key):
    return [i + 1 for i, (k, _v) in enumerate(m.kinds) if k == key][0]


def check_units(c):
    import p3_conflict_reference as pcr
    TOWN, VILLAGE, HAMLET = 3, 2, 1
    # --- 3: chance_ppb
    c.eq("chance_ppb p = 0 is never", [chance_ppb(u, 0) for u in (0, (1 << 64) - 1)], [False, False])
    c.eq("chance_ppb p = 10^9 is always", [chance_ppb(u, 10 ** 9) for u in (0, (1 << 64) - 1)], [True, True])
    c.eq("chance_ppb p = 1: threshold floor(2^32 / 10^9) = 4", [chance_ppb(u << 32, 1) for u in (3, 4)], [True, False])
    c.eq("chance_ppb p > 10^9 clamps to 2^32", chance_ppb(((1 << 32) - 1) << 32, 2 * 10 ** 9), True)
    c.eq("chance_ppb p < 0 is never", chance_ppb(0, -5), False)
    # --- 6.2.1 habitat and capacity: 1280 grassland cells per region
    w, m = stub(regions=2)
    m.init_beasts()
    wolf, deer, boar = 5, 3, 2
    c.eq("H(wolf) = 1280 x 400 div 1000", m.H[(1, wolf)], 512)
    c.eq("H(deer) = 1280 x 800 div 1000", m.H[(1, deer)], 1024)
    c.eq("K0(wolf), a predator: 512 x 20 x M(1000) div 10^7", m.K0[(1, wolf)], 1)
    c.eq("K0(deer), prey: 1024 x 100 x 1000 div 10^7", m.K0[(1, deer)], 10)
    c.eq("B starts at K0", m.B[(1, deer)], 10)
    w2, m2 = stub(regions=2, tuning=dict(pressure=dict(low=500, normal=3000, high=1600)))
    m2.init_beasts()
    c.eq("F = M for a predator (M 3000): K0(wolf) = 512 x 20 x 3000 div 10^7", m2.K0[(1, wolf)], 3)
    c.eq("F = 1000 for prey whatever M", m2.K0[(1, deer)], 10)
    c.eq("B1: H sums the rows then floors", Monsters.habitat(m, 1, [dict(biome="grassland", weight=1)]), 1)
    # --- 6.2.2 one year
    w, m = stub(regions=1)
    m.init_beasts()
    m.K0[(1, deer)] = 1000
    m.B[(1, deer)] = 100
    m.beast_update(1)
    c.eq("B < K: 100 + 100 x 250 x 900 div 10^6 = 122", m.B[(1, deer)], 122)
    m.B[(1, deer)] = 0
    m.beast_update(1)
    c.eq("B = 0 -> 1", m.B[(1, deer)], 1)
    m.B[(1, deer)] = 1500
    m.beast_update(1)
    c.eq("B > K: 1500 - 500 x 200 div 1000 = 1400", m.B[(1, deer)], 1400)
    m.B[(1, deer)] = 1001
    m.beast_update(1)
    c.eq("B > K by 1: at least 1 lost", m.B[(1, deer)], 1000)
    m.beast_update(1)
    c.eq("B = K stays", m.B[(1, deer)], 1000)
    m.B[(1, deer)] = 999
    m.beast_update(1)
    c.eq("B just below K: +1 at least, capped at K", m.B[(1, deer)], 1000)
    # displacement: 5000 people in region 1 -> d = min(900, 500) = 500, K = 500
    s = pcr.add_stl(w, 0, 1, TOWN, 5000)
    m.beast_update(1)
    c.eq("d(r) = 5000 x 100 div 1000", m.d[1], 500)
    c.eq("B above the displaced K falls by 20% of the excess", m.B[(1, deer)], 1000 - 500 * 200 // 1000)
    w.stl(s)["population"] = 20000
    m.beast_update(1)
    c.eq("d(r) capped at displace_max_permille", m.d[1], 900)
    w.stl(s)["status"] = 3
    m.beast_update(1)
    c.eq("a ruined settlement displaces nothing", m.d[1], 0)
    # g(r) at the caps
    w, m = stub(regions=1)
    m.init_beasts()
    m.B[(1, wolf)], m.B[(1, deer)], m.B[(1, boar)] = 0, 0, 0
    for s2 in range(1, 6):
        m.B[(1, s2)] = 0
        m.K0[(1, s2)] = 0
    m.beast_update(1)
    c.eq("no beasts: g = 1000", m.g[1], 1000)
    m.K0[(1, wolf)] = m.B[(1, wolf)] = 100000
    m.beast_update(1)
    c.eq("many predators, Pd = people_floor: g = 1000 - drag_max", m.g[1], 1000 - m.t["drag_max_permille"])
    c.eq("menace(r) = 100000 x 10 x 1000 div 100", m.menace[1], 10000000)
    m.K0[(1, wolf)] = m.B[(1, wolf)] = 0
    m.K0[(1, deer)] = m.B[(1, deer)] = 100000
    m.beast_update(1)
    c.eq("much game: g = 1000 + feed_max", m.g[1], 1100)
    m.K0[(1, deer)] = m.B[(1, deer)] = 30
    m.beast_update(1)
    c.eq("feed = 30 x 60 x 2 div 100 = 36", m.g[1], 1036)
    c.eq("WildView growth_permille", WildView(m).growth_permille(1), 1036)
    c.eq("WildView of an unknown region: 1000", WildView(m).growth_permille(99), 1000)
    # Q1 equals L years of T1 with P fixed
    wa, ma = stub(regions=2)
    wb, mb = stub(regions=2)
    for ww in (wa, wb):
        pcr.add_stl(ww, 0, 1, TOWN, 3000)
    ma.init_beasts()
    mb.init_beasts()
    for mm in (ma, mb):
        mm.K0[(1, deer)] = 500
        mm.B[(1, deer)] = 3
    wa.years = 25
    ma.beast_update(25)
    for _ in range(25):
        mb.beast_update(1)
    c.eq("Q1 of 25 years = 25 T1 with P fixed", ma.B, mb.B)
    # rows: year 0, a phase change, finish; none for an unchanged count
    w, m = stub(regions=1)
    w.year, w.phase = 0, 1
    m.beast_update(1)
    first = list(m.rows)
    c.ok("rows after the first update dated its year", first and all(r[2] == 0 for r in first))
    c.ok("no row for a species with K0 = 0", all(r[3] > 0 for r in first))
    w.year = 1
    m.beast_update(1)
    c.eq("no row inside a phase", m.rows, first)
    w.year, w.phase = 2, 2
    m.K0[(1, deer)] = 1000
    m.beast_update(1)
    c.eq("a phase change writes nothing when no count changed since the last row", m.rows, first)
    b2 = m.B[(1, deer)]
    c.ok("the year-2 update grew the deer (K0 raised)", b2 > first[[r[1] for r in first].index(deer)][3])
    w.year, w.phase = 3, 3
    m.beast_update(1)
    c.eq("the next phase change writes the changed count dated the previous update's year",
         [r for r in m.rows if r not in first], [(1, deer, 2, b2)])
    m.finish(6000)
    c.eq("finish writes at E - 1", [r[2] for r in m.rows if r[1] == deer][-1], 5999)
    # --- 6.2.3 boldness: on at 1500, stays bold at 900, off at 799
    w, m = stub(regions=1, tuning=dict(bold_permille=1000))
    m.init_beasts()
    s = pcr.add_stl(w, 1, 1, TOWN, 1000)
    pcr.add_civ(w, 1, s)
    w.stl(s)["owner_civ"] = 1
    m.B[(1, wolf)] = 150   # Pred = 1500, Pd = 1000 -> 1500
    m.menace[1] = 1500
    n0 = len(w.events)
    m.beasts_bold()
    c.eq("bold on at 1500: one event", [e["key"] for e in w.events[n0:]], ["beasts_bold"])
    c.eq("the toll: max(1, 1000 x 10 div 1000) = 10", w.stl(s)["population"], 990)
    c.eq("the species of the largest B x menace", w.events[-1]["values"][1], wolf)
    m.menace[1] = 900
    m.beasts_bold()
    c.eq("bold at 900 stays bold, no event", (m.bold[1], len(w.events)), (True, n0 + 1))
    m.menace[1] = 799
    m.beasts_bold()
    c.eq("off at 799", m.bold[1], False)
    w.stl(s)["population"] = 1
    m.menace[1] = 2000
    m.beasts_bold()
    c.eq("the toll's cap at P - 1: dead 0", (w.events[-1]["values"][2], w.stl(s)["population"]), (0, 1))
    # --- 6.3: caps and appearances
    w, m = stub(regions=4, kinds={"troll": dict(biomes=[dict(biome="grassland", weight=1000)], wake_rate=10 ** 7)},
                tuning=dict(living_permille=1000))
    m.init_beasts()
    troll = kind_id(m, "troll")
    c.eq("L_world = max(1, R x living_permille div 1000)", m.L_world(), 4)
    m.created_this_step = False
    m.wild_appearance(quiet=False)
    c.eq("a wild appearance in the first region whose chance hits", [x["region"] for x in m.sites], [1])
    c.eq("one creation per step", (len(m.monsters), m.created_this_step), (1, True))
    m.wild_appearance(quiet=False)
    c.eq("no second creation in the step", len(m.monsters), 1)
    m.created_this_step = False
    m.wild_appearance(quiet=False)
    c.eq("one lair per region: the next in region 2", [x["region"] for x in m.sites], [1, 2])
    m.created_this_step = False
    m.wild_appearance(quiet=False)
    m.created_this_step = False
    m.wild_appearance(quiet=False)
    c.eq("max_alive 3 trolls (then the ogres and others need habitat)",
         sum(1 for x in m.monsters if x["kind"] == troll), 3)
    c.eq("appearance event and participants", [(e["key"], [p[2] for p in e["participants"]]) for e in w.events
                                               if e["key"] == "monster_appeared"][0], ("monster_appeared", [8, 9]))
    # quiet: pending_flare only for a flare kind, cleared at the next Q4
    w, m = stub(regions=2, kinds={"dragon": dict(biomes=[dict(biome="grassland", weight=1000)], wake_rate=10 ** 7)})
    m.init_beasts()
    w.years = 25
    w.quiet_used = {}
    m.q4()
    c.ok("a dragon appears in Q4 and sets pending_flare", m.pending_flare == len(w.events) and
         w.events[-1]["key"] == "monster_appeared")
    c.eq("tension: {flare_threshold, pending_flare}", m.tension(1000, 25), (m.threshold, m.pending_flare))
    c.eq("tension is pure (twice the same)", m.tension(1000, 25), (m.threshold, m.pending_flare))
    m.q4()
    c.eq("pending_flare cleared at the next Q4", m.pending_flare, 0)
    c.eq("tension 0 without pending", m.tension(1000, 25), (0, 0))
    w, m = stub(regions=2, kinds={"dragon": dict(biomes=[dict(biome="grassland", weight=1000)], wake_rate=10 ** 7)})
    m.init_beasts()
    w.years = 25
    w.quiet_used = {0: 3}
    m.q4()
    c.eq("no appearance in Q4 without civilization 0's budget", len(m.monsters), 0)
    m.created_this_step = False
    w.quiet_used = None
    m.t4()
    c.eq("a tick appearance never sets pending_flare", (len(m.monsters), m.pending_flare), (1, 0))
    # islets are skipped
    w, m = stub(regions=2, kinds={"troll": dict(biomes=[dict(biome="grassland", weight=1000)], wake_rate=10 ** 7)})
    m.init_beasts()
    w.geo.q[1]["islet"] = True
    m.created_this_step = False
    m.wild_appearance(quiet=False)
    c.eq("an islet region gets no wild appearance", [x["region"] for x in m.sites], [2])
    # --- 6.3.2 the undead: window and magnitudes
    w, m = stub(regions=2, kinds={"undead": dict(wake_rate=1000000)})
    m.init_beasts()
    a = pcr.add_civ(w, 1, 1)
    b = pcr.add_civ(w, 2, 1)
    w.year = 900
    e1 = w.append("battle", region=1, participants=(("winner", CIV, a), ("loser", CIV, b)), values=(1000, 1500), causes=(1,))
    w.year = 901
    e2 = w.append("battle", region=1, participants=(("winner", CIV, a), ("loser", CIV, b)), values=(700, 800), causes=(1,))
    w.year = 1000
    m.read_log()
    c.eq("an entry at Y - window drops, at Y - window + 1 stays", [x[2] for x in m.W[1]], [e2])
    c.eq("a battle's dead = winner_dead + loser_dead", m.W[1][0][1], 1500)
    # --- 6.4.1 (r1) raid targeting: the weights factor by factor, the weighted draw, and the toll
    w, m = stub(regions=4, kinds={"troll": dict(raid_permille=1000, active_years=100000, overrun_permille=0)})
    m.init_beasts()
    c1 = pcr.add_civ(w, 1, 1)
    s1 = pcr.add_stl(w, c1, 1, TOWN, 3000)
    s2 = pcr.add_stl(w, c1, 2, TOWN, 9000)
    s3 = pcr.add_stl(w, c1, 2, VILLAGE, 900)
    w.civ(c1)["capital"] = s1
    w.year = 1000
    tk = m.kind(kind_id(m, "troll"))
    dk = m.kind(kind_id(m, "dragon"))
    ok = m.kind(kind_id(m, "ogre"))
    c.eq("might_f: troll 300 -> 200, so a town at hop 0 weighs 150 x 200 div 1000", m.prey_weight(tk, TOWN, 0, False), 30)
    c.eq("might_f capped at prey_might_max_permille: a dragon's town 150 x 4000 div 1000", m.prey_weight(dk, TOWN, 0, False),
         600)
    c.eq("might_f of an ogre (200 -> 133): city 40 x 133 div 1000, fortress 20 x 133 div 1000",
         (m.prey_weight(ok, 4, 0, False), m.prey_weight(ok, 5, 0, False)), (5, 2))
    c.eq("might_f not applied to hamlets and villages", (m.prey_weight(dk, HAMLET, 0, False),
                                                        m.prey_weight(dk, VILLAGE, 0, False)), (1000, 600))
    c.eq("hop_f at 0, 1 and 2 hops: 1000, 500, 250", [m.prey_weight(tk, VILLAGE, h, False) for h in (0, 1, 2)],
         [600, 300, 150])
    c.eq("hop_f floors per hop (prey_hop_permille 333: 1000, 333, 110)",
         [Monsters.prey_weight(type("X", (), {"t": dict(m.t, prey_hop_permille=333)})(), tk, HAMLET, h, False)
          for h in (0, 1, 2)], [1000, 333, 110])
    c.eq("the capital factor after the hop factor: 30 x 500 div 1000 = 15, x 250 div 1000 = 3",
         m.prey_weight(tk, TOWN, 1, True), 3)
    mid = m.appear(kind_id(m, "troll"), 2)
    mm = m.monsters[mid - 1]
    hops = m.geo.hops(2)
    u = w.draw("hist.monsters.raid_target", mid, 0, 0)
    want = [s1, s2, s3][pnr.weighted(u, [3, 30, 600])]
    m.raid(mm, m.kind(mm["kind"]))
    c.eq("raid target: weighted([3, 30, 600]) on hist.monsters.raid_target (m, 0, 0), ascending id",
         w.events[-1]["settlement"], want)
    c.eq("raid_target returns that candidate", m.raid_target(mm, tk, hops)["id"], want)
    P = w.stl(want)["population"]
    tau = w.events[-1]["values"][1]
    c.ok("toll within [toll_min, toll_max] and at most kill_max", 0 < tau <= 60)
    c.eq("the raid's cause is the waking event", w.events[-1]["causes"], [mm["waking"]])
    c.eq("hoard grows by the plunder", mm["hoard"], w.events[-1]["values"][2])
    # the weighted boundaries: a candidate of weight 0 is never chosen; the first and last index at u's extremes
    c.eq("weighted at u = 0 and u = 2^64 - 1", (pnr.weighted(0, [3, 30, 600]), pnr.weighted((1 << 64) - 1, [3, 30, 600])),
         (0, 2))
    c.eq("weight 0 is never chosen", sorted({pnr.weighted(x << 32, [0, 5, 0]) for x in (0, 1 << 31, (1 << 32) - 1)}), [1])
    # no candidate, or W = 0: no raid and no draw
    for label, edit in (("W = 0", lambda: m.t.update(prey_weight=dict(hamlet=0, village=0, town=0, city=0, fortress=0))),
                        ("no candidate in range", lambda: [w.stl(x).update(status=3) for x in (s1, s2, s3)]),
                        ("a settlement without an owner is never a candidate",
                         lambda: [w.stl(x).update(owner_civ=0) for x in (s1, s2, s3)])):
        w, m = stub(regions=4, kinds={"troll": dict(raid_permille=1000, active_years=100000, overrun_permille=0)})
        m.init_beasts()
        c1 = pcr.add_civ(w, 1, 1)
        s1 = pcr.add_stl(w, c1, 1, TOWN, 3000)
        s2 = pcr.add_stl(w, c1, 2, TOWN, 9000)
        s3 = pcr.add_stl(w, c1, 2, VILLAGE, 900)
        w.civ(c1)["capital"] = s1
        w.year = 1000
        mid = m.appear(kind_id(m, "troll"), 2)
        mm = m.monsters[mid - 1]
        edit()
        n0 = len(w.events)
        draws = []
        real = w.draw
        w.draw = lambda st, *a, real=real: (draws.append(st), real(st, *a))[1]
        m.raid(mm, m.kind(mm["kind"]))
        w.draw = real
        c.eq("%s: no raid event" % label, len(w.events), n0)
        c.eq("%s: neither the target nor the toll is drawn" % label, draws, [])
    # --- 6.4.2 hunt odds and outcomes
    c.eq("odds at S = 0: no hunt (S = 0 skips)", min(900, 0 * 1000 // (0 + 300)), 0)
    c.eq("odds at S = might: 500", min(900, 300 * 1000 // (300 + 300)), 500)
    c.eq("odds capped at slay_max_permille", min(900, 10 ** 9 * 1000 // (10 ** 9 + 300)), 900)
    # --- 6.4.3 overrun: every condition
    for label, setup, want in (
            ("a village is overrun", lambda w, m, ids: None, True),
            ("not the capital", lambda w, m, ids: w.civ(ids[0]).update(capital=ids[2]), False),
            ("not a kind of overruns", lambda w, m, ids: w.stl(ids[2]).update(kind=TOWN), False),
            ("not the owner's last settlement", lambda w, m, ids: w.stl(ids[1]).update(status=3), False),
            ("not where another monster's lair lies", lambda w, m, ids: m.sites.append(dict(
                id=len(m.sites) + 1, name="X", kind=LAIR, region=2, x=0, y=0, settlement=0, founder=0,
                monster=99, origin_year=1, origin_event=1, ended_year=None, ending_event=0, ending=OPEN,
                hostile=None)), False)):
        w, m = stub(regions=3, kinds={"troll": dict(raid_permille=1000, overrun_permille=1000, active_years=100000)})
        m.init_beasts()
        cc = pcr.add_civ(w, 1, 1)
        sa = pcr.add_stl(w, cc, 1, TOWN, 3000)
        sb = pcr.add_stl(w, cc, 2, VILLAGE, 800)
        w.civ(cc)["capital"] = sa
        mid = m.appear(kind_id(m, "troll"), 3)
        setup(w, m, (cc, sa, sb))
        mm = m.monsters[mid - 1]
        n0 = len(w.events)
        m.raid(mm, m.kind(mm["kind"]))
        got = any(e["key"] == "settlement_overrun" for e in w.events[n0:])
        c.eq("overrun: %s" % label, got, want)
        if label == "a village is overrun":
            c.eq("overrun: the old lair ends with ending 3", m.sites[0]["ending"], END_MOVED)
            c.eq("overrun: the new lair at the settlement", (m.sites[1]["settlement"], m.sites[1]["x"],
                                                             m.sites[1]["y"]), (sb, w.stl(sb)["x"], w.stl(sb)["y"]))
            c.eq("overrun: ruin (status 3, owner 0)", (w.stl(sb)["status"], w.stl(sb)["owner_civ"]), (3, 0))
            c.eq("overrun: the region is released", w.owner[2][0], 0)
            c.ok("overrun: the region is shunned", WildView(m).shunned(2))
            n1 = len(w.events)
            m.raid(mm, m.kind(mm["kind"]))
            c.eq("a monster overruns once", sum(1 for e in w.events[n1:] if e["key"] == "settlement_overrun"), 0)
            m.die_of_age(mm)
            c.ok("the shun is lifted when the monster dies", not WildView(m).shunned(2))
    # --- 6.6 outposts: raised at war on the height, one per civilization per year, outpost_max; endings
    w, m = stub(regions=3, tuning=dict(outpost_permille=1000))
    m.init_beasts()
    ca = pcr.add_civ(w, 1, 1)
    cb = pcr.add_civ(w, 2, 1)
    sa = pcr.add_stl(w, ca, 1, TOWN, 3000)
    sb = pcr.add_stl(w, ca, 2, TOWN, 3000)
    sc = pcr.add_stl(w, cb, 3, TOWN, 3000)
    w.civ(ca)["capital"], w.civ(cb)["capital"] = sa, sc
    w.year = 1000
    m.outposts_raised()
    c.eq("no outpost without a war", m.sites, [])
    w.relations[(CIV, cb, CIV, ca, "war")] = 2
    m.outposts_raised()
    c.eq("one outpost per civilization per year, in the frontier region", [(x["founder"], x["region"])
                                                                           for x in m.sites], [(ca, 2), (cb, 3)])
    c.eq("outpost_built cause = the war's since_event", w.events[-1]["causes"], [2])
    w.year = 1001
    m.outposts_raised()
    c.eq("no second outpost in a region; region 1 has no hostile neighbour", len(m.sites), 2)
    del w.relations[(CIV, cb, CIV, ca, "war")]
    w.year = 1000 + 200
    m.outposts_left(quiet=False)
    c.eq("left when the frontier was quiet for outpost_quiet_years", [x["ending"] for x in m.sites],
         [END_QUIET, END_QUIET])
    c.eq("reason 0 and no cause", (w.events[-1]["values"][1], w.events[-1]["causes"]), (0, []))


# ---------------------------------------------------------------------------
# 7.5 I0: the zero tuning against the subsystem not installed (no view set)
# ---------------------------------------------------------------------------

I0_TABLES = ("event", "event_participant", "event_cause", "event_value", "phase", "civilization", "settlement",
             "settlement_census", "region_control", "relation", "region_name")


def without_monsters(fn):
    import p3_conflict_reference as pcr
    saved = pcr.FLAGS["monsters"]
    pcr.FLAGS["monsters"] = False
    try:
        return fn()
    finally:
        pcr.FLAGS["monsters"] = saved


def i0_differences(run):
    """run(dat) -> res. The tables of I0 that differ between the zero tuning with the subsystem and the shipped
    data without it (the settlement rows compared whole: both carry `ending`)."""
    import p3_conflict_reference as pcr
    zero = run(zero_data(pcr.data()))
    off_dat = pcr.edited(pcr.data(), peoples=dict(ghost_permille=0))
    off = without_monsters(lambda: run(off_dat))
    return [t for t in I0_TABLES if zero.get(t, []) != off.get(t, [])], zero


def i0_cases(inputs, quick=False):
    """(label, run) for 19.2's I0 cases: the stress fixtures and seeds 0-4 at 512 (inputs geo-seed-0-512 etc. are
    the 512 inputs present); with quick only the fixtures."""
    import p3_conflict_reference as pcr
    for seed in (0, 1, 2, 3):
        def run(dat, seed=seed):
            case = pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
            inp, _g = pcr.synthetic_inputs(seed, dat, case)
            return pcr.history_stage(dat, inp)
        yield "synthetic-%d" % seed, run
    if quick or not os.path.isdir(inputs):
        return
    for name, seed, size in (("geo-seed-0-512", 0, 512), ("geo-seed-42-512", 42, 512), ("geo-phrase-512", None, 512),
                             ("hist-span-2000-512", None, 512)):
        path = os.path.join(inputs, name + ".db")
        if not os.path.exists(path):
            continue
        case = [c for c in pcr.world_cases() if c[0] == name]
        if not case:
            continue
        n, sd, sz, span, races, detail, civs, intensity = case[0]

        def run(dat, n=n, sd=sd, sz=sz, span=span, races=races, detail=detail, civs=civs, intensity=intensity):
            return pcr.world_case(n, sd, sz, span, races, detail, inputs, dat, civs, intensity)
        yield n, run


# ---------------------------------------------------------------------------
# The golden file (18): per world case and fixture, the hashes of beast_population, monster, site and
# settlement.ending, and the counts of 16's M and G figures
# ---------------------------------------------------------------------------

HEADER = """\
# P5-02 monsters golden values (docs/specs/p5-02-beasts-monsters.md r0 sections 6, 8, 11, 16, 18), generator
# 0.23.0 (schema 9), written by worldgen/tests/reference/p5_02_monsters_reference.py golden, an independent
# reference written from the spec (the subsystem runs inside p3_conflict_reference.py's replay). Do not edit by
# hand; never regenerate to make a C++ test pass. The shipped rates are interim (17 step 9): after the
# calibration, re-record with `p5_02_monsters_reference.py golden` once the data roots hold the new values.
#
# Format: ASCII, LF, tokens separated by one space.
#   world <case> <seed> <map size> <span> <races> <detail> <civs> <intensity> <pressure>   a world case of
#       p3-06-conflict.txt (geography from the input world, history the shipped configuration), and
#       p5-monsters-high-1024 (world-hashes.sh's new case, --monster-pressure high)
#   fixture <case> <seed> <span> <races> <civs> <intensity> <data>   a synthetic geography (make_synthetic) with
#       data monsters-small (19.3: the conflict stress data, wake_rate x 2000, living_permille 1000,
#       overrun_permille 1000) or zero (7.5)
#   <case> table <beast_population|monster|site> <rows> <hash>   P0-04 8's table hash of the rows in key order
#   <case> ending <hash>      the table hash of the two columns (settlement id, ending), ascending id
#   <case> M monsters <n> alive <a> slain <s> died <d> raids <r> killed <k> overruns <o> risen <u> lair_settlement_open <l>
#   <case> G ghost <n> endings <0>:<n>,<1>:<n>,... standing <n> population <n> shunned <n>
#   <case> S outposts <n> ended <n> bold <n> monster_flares <n>
#   <case> m<id> <kind> <status> born <year> e<event> died <year|-> hoard <h> | <name> | lair <site name>
#   <case> si<id> <lair|outpost> rg<region> <x> <y> s<settlement> c<founder> m<monster> <origin_year> e<event> <ended|-> <ending> | <name>
#   kinds_seen <key>,...    over the monsters-small fixtures: the monster kinds that appeared
#   events_seen <key>,...   over the monsters-small fixtures: the event kinds of 44-55 written at least once
"""

WORLD_CASE_HIGH = ("p5-monsters-high-1024", 7, 1024, 6000, "all", "normal", "auto", "normal")
SMALL_SEEDS = (0, 1, 2, 3, 5, 6, 7, 9, 12, 13, 14)


def table_hash(ncols, rows):
    import p3_chronology_reference as pchr
    return pchr.table_hash(ncols, rows)


def case_lines(name, res):
    w = res["_world"]
    m = w.monsters
    o = []
    for t, n in (("beast_population", 4), ("monster", 11), ("site", 14)):
        o.append("%s table %s %d %s" % (name, t, len(res[t]), table_hash(n, res[t])))
    o.append("%s ending %s" % (name, table_hash(2, [(s[0], s[14]) for s in res["settlement"]])))
    keys = {}
    for e in w.events:
        keys[e["key"]] = keys.get(e["key"], 0) + 1
    raids = [e for e in w.events if e["key"] == "monster_raid"]
    st = [s for s in res["settlement"] if s[10] in (1, 2)]
    endings = {}
    for s in res["settlement"]:
        endings[s[14]] = endings.get(s[14], 0) + 1
    o.append("%s M monsters %d alive %d slain %d died %d raids %d killed %d overruns %d risen %d "
             "lair_settlement_open %d" % (
                 name, len(m.monsters), sum(1 for x in m.monsters if x["status"] in (AWAKE, ASLEEP)),
                 sum(1 for x in m.monsters if x["status"] == SLAIN), sum(1 for x in m.monsters if x["status"] == DIED),
                 len(raids), sum(e["values"][1] for e in raids), keys.get("settlement_overrun", 0),
                 keys.get("undead_rose", 0),
                 sum(1 for s in m.sites if s["kind"] == LAIR and s["ending"] == OPEN and s["settlement"])))
    o.append("%s G ghost %d endings %s standing %d population %d shunned %d" % (
        name, sum(endings.get(k, 0) for k in (3, 4, 5)), ",".join("%d:%d" % kv for kv in sorted(endings.items())),
        len(st), sum(s[11] for s in st), sum(1 for r in m.regions if m.shunned(r))))
    flares = [p for p in res["phase"] if p[2] == 3 and p[5] and w.events[p[5] - 1]["key"] == "monster_appeared"]
    o.append("%s S outposts %d ended %d bold %d monster_flares %d" % (
        name, sum(1 for s in m.sites if s["kind"] == OUTPOST),
        sum(1 for s in m.sites if s["kind"] == OUTPOST and s["ending"] != OPEN), keys.get("beasts_bold", 0),
        len(flares)))
    for x in m.monsters:
        o.append("%s m%d %s %d born %d e%d died %s hoard %d | %s | lair %s" % (
            name, x["id"], m.kind_key(x["kind"]), x["status"], x["born"], x["born_event"],
            "-" if x["died"] is None else str(x["died"]), x["hoard"], x["name"], m.site(x["lair"])["name"]))
    for s in m.sites:
        o.append("%s si%d %s rg%d %d %d s%d c%d m%d %d e%d %s %d | %s" % (
            name, s["id"], "lair" if s["kind"] == LAIR else "outpost", s["region"], s["x"], s["y"], s["settlement"],
            s["founder"], s["monster"], s["origin_year"], s["origin_event"],
            "-" if s["ended_year"] is None else str(s["ended_year"]), s["ending"], s["name"]))
    return o


def world_case_high(inputs, dat):
    import p3_conflict_reference as pcr
    import p3_chronology_reference as pchr
    name, seed, size, span, races, detail, civs, intensity = WORLD_CASE_HIGH
    conn = pchr.open_world(os.path.join(inputs, name + ".db"))
    p = pchr.params(span=span, races=pchr.races_of(races, dat.chron), detail=detail, civs=pp.civs_param(civs),
                    intensity=intensity, pressure="high")
    inp, _chron = pcr.inputs_for(dat, conn, p, seed, size)
    inp.peoples_layers()
    conn.close()
    return pcr.history_stage(dat, inp)


def all_cases(inputs, quick=False):
    import p3_conflict_reference as pcr
    dat = pcr.data()
    if os.path.isdir(inputs):
        for name, seed, size, span, races, detail, civs, intensity in pcr.world_cases():
            if quick and size > 512:
                continue
            res = pcr.world_case(name, seed, size, span, races, detail, inputs, dat, civs, intensity)
            yield ("world %s %d %d %d %s %s %s %s normal" % (name, seed, size, span, races, detail, civs, intensity),
                   name, res, "world")
        if not quick and os.path.exists(os.path.join(inputs, WORLD_CASE_HIGH[0] + ".db")):
            res = world_case_high(inputs, dat)
            yield ("world %s %d %d %d %s %s %s %s high" % WORLD_CASE_HIGH, WORLD_CASE_HIGH[0], res, "world")
    small = monsters_small_data(dat)
    for seed in (SMALL_SEEDS[:4] if quick else SMALL_SEEDS):
        case = pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
        inp, _g = pcr.synthetic_inputs(seed, small, case)
        res = pcr.history_stage(small, inp)
        res["_inputs"] = inp
        yield ("fixture monsters-small-%d %d %d %s %s %s monsters-small" % ((seed, seed) + case),
               "monsters-small-%d" % seed, res, "small")
    zero = zero_data(dat)
    for seed in (0, 1):
        case = pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
        inp, _g = pcr.synthetic_inputs(seed, zero, case)
        res = pcr.history_stage(zero, inp)
        res["_inputs"] = inp
        yield "fixture zero-%d %d %d %s %s %s zero" % ((seed, seed) + case), "zero-%d" % seed, res, "zero"


def golden_text(inputs, quick=False):
    o = [HEADER.rstrip("\n")]
    kinds_seen, events_seen = set(), set()
    for head, name, res, what in all_cases(inputs, quick):
        o.append(head)
        o += case_lines(name, res)
        if what == "small":
            w = res["_world"]
            kinds_seen.update(w.monsters.kind_key(x["kind"]) for x in w.monsters.monsters)
            events_seen.update(e["key"] for e in w.events if e["kind"] >= 44)
    o.append("kinds_seen %s" % ",".join(sorted(kinds_seen)))
    o.append("events_seen %s" % ",".join(k["key"] for k in pe.KINDS_023 if k["key"] in events_seen))
    return "\n".join(o) + "\n"


# ---------------------------------------------------------------------------
# check-world: a world file's rows (invariants of 14 and the reader's own checks on the stored tables)
# ---------------------------------------------------------------------------

def cmd_check_world(paths, data_root="game/data"):
    import p3_conflict_reference as pcr
    dat = pcr.data(data_root)
    bad = 0
    for path in paths:
        rows = rows_from_db(path)
        meta = rows["meta"]
        c = Checker()
        c.eq("%s schema_version" % path, meta.get("schema_version"), str(pe.SCHEMA_VERSION))   # 10 at 0.24.0
        E = int(meta.get("history_end_year", meta.get("param.history_span", "6000")))
        borders = [(r[0], r[1]) for r in rows["region_border"]]
        for v in invariant_violations(rows, list(dat.base.monsters), dat.base.monsters_tuning, E, borders,
                                      R=len(rows["region"]), quiet_max=dat.engine["quiet_events_max"]):
            c.ok("%s invariant %s" % (path, v), False)
        n = len(rows.get("monster", []))
        print("%s: %d monsters, %d sites, %d beast rows, %d checks, %d failures" % (
            path, n, len(rows.get("site", [])), len(rows.get("beast_population", [])), c.count, c.failures))
        bad += c.failures
    return 1 if bad else 0


def cmd_check(argv):
    import p3_conflict_reference as pcr
    quick = "--quick" in argv
    inputs = argv[argv.index("--inputs") + 1] if "--inputs" in argv else DEFAULT_INPUTS
    c = Checker()
    check_units(c)
    for head, name, res, what in all_cases(inputs, quick=True):
        if "_inputs" not in res:
            res["_inputs"] = res["_world"].inp
        for v in res_violations(res, res["_world"], quarter=not name.startswith("monsters-small")):
            c.ok("%s invariant %s" % (name, v), False)
    for label, run in i0_cases(inputs, quick):
        diff, _ = i0_differences(run)
        c.eq("I0 %s: the history tables equal those without the subsystem" % label, diff, [])
    if os.path.exists(GOLDEN) and not quick:
        with open(GOLDEN, "rb") as f:
            c.eq("committed golden file is current", f.read().decode(), golden_text(inputs))
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def main(argv):
    import p3_conflict_reference as pcr
    if not argv:
        print(__doc__)
        return 2
    version = "0.27.0"   # P5-06 phase 2 (--version 0.26.0: P5-04's file); P5-04; --version 0.25.0 the P5-03a file, 0.24.0 serves the P5-03 file, 0.23.0 the committed P5-02 file
    if "--version" in argv:
        i = argv.index("--version")
        version = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    pcr.use_version(version)
    inputs = argv[argv.index("--inputs") + 1] if "--inputs" in argv else DEFAULT_INPUTS
    if argv[0] == "check":
        return cmd_check(argv[1:])
    if argv[0] == "units":
        c = Checker()
        check_units(c)
        print("%d checks, %d failures" % (c.count, c.failures))
        return 1 if c.failures else 0
    if argv[0] == "i0":
        c = Checker()
        for label, run in i0_cases(inputs, "--quick" in argv):
            diff, _ = i0_differences(run)
            c.eq("I0 %s" % label, diff, [])
        print("%d checks, %d failures" % (c.count, c.failures))
        return 1 if c.failures else 0
    if argv[0] == "golden":
        rest = [a for a in argv[1:] if not a.startswith("--") and a != inputs]
        text = golden_text(inputs, "--quick" in argv)
        path = rest[0] if rest else GOLDEN
        if path == "-":
            sys.stdout.write(text)
        else:
            with open(path, "w", newline="\n") as f:
                f.write(text)
        return 0
    if argv[0] == "check-world":
        return cmd_check_world([a for a in argv[1:] if not a.startswith("--")])
    if argv[0] == "readings":
        print(READINGS)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
