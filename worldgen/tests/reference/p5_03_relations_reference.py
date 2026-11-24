#!/usr/bin/env python3
"""Reference of P5-03 "Race relations, mixed settlements and refugee migrations; P5-02 follow-ups"
(docs/specs/p5-03-race-relations.md r1), written from the spec alone, without the engineer's branch.

The relations subsystem of sections 5 and 6 runs inside p3_conflict_reference.py's replay, installed after the
monsters (P5-00 4.1): World.relations runs in T1, T4, T6, Q1 and Q4; peoples and conflict call its ledger (7.2) and
conflict reads its dispositions (7.1). Part B's rules are in p5_02_monsters_reference.py (9, 10, 12.2, 13, 14) and
p3_peoples_reference.py (11.1, 11.2, the public abandon); the catalogue, schema 10 and the rendering of 8.2, 9.5
and 13 in p3_engine_reference.py; the data of 4 and the checks in p3_data_reference.py; the epithets of 12.1 in
p5_02_monsters_reference.py. This module holds the subsystem, the formulas of 6 as functions, the invariants of
19 and the checks of 16.4 on rows (check-world mode), the hand-made units of 24.1 on formulas and stub worlds,
invariant I0 (7.4), the fixture relations-small (24.3) and the golden file worldgen/tests/golden/p5-03-relations.txt.

Commands (from the repository root):
  python3 -B worldgen/tests/reference/p5_03_relations_reference.py check [--quick] [--inputs DIR]
  python3 -B worldgen/tests/reference/p5_03_relations_reference.py units
  python3 -B worldgen/tests/reference/p5_03_relations_reference.py golden [PATH|-] [--inputs DIR] [--fixtures-only]
  python3 -B worldgen/tests/reference/p5_03_relations_reference.py check-world DB [DB ...]
  python3 -B worldgen/tests/reference/p5_03_relations_reference.py i0 [--inputs DIR]
  python3 -B worldgen/tests/reference/p5_03_relations_reference.py readings

--inputs DIR: the world inputs (default ~/.cache/the-unmapped/out/p5-02-tests/inputs: geography only).
"""

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

GOLDEN = os.path.join(pe.REPO, "worldgen", "tests", "golden", "p5-03-relations.txt")
DEFAULT_INPUTS = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p5-02-tests", "inputs")
# 3: appended after hist.monsters.raid_target, in this order
STREAMS = ("hist.beasts.bold_kind", "hist.monsters.fear", "hist.peoples.abandon_loss", "hist.relations.expel")
RACE, CIV, SETTLEMENT, REGION, MONSTER = pe.RACE, pe.CIV, pe.SETTLEMENT, pe.REGION, pe.MONSTER
HATRED, WARINESS, FRIENDSHIP = 0, 1, 2
SUBJECT, TOLERATED, ACCEPTED, RULING = 0, 1, 2, 3
STANDING_WORDS = ("subject", "tolerated", "accepted", "ruling")
BAND_WORDS = ("hatred", "wariness", "friendship")
P503_KINDS = ("settlement_fled", "peoples_met", "disposition_turned", "refugees_arrived", "settlement_mixed",
              "standing_changed", "people_expelled")

READINGS = """\
R1 (6.4.2, 6.3.2) the event a move's new entries date from (`since_event`, "the arrival or the abandonment it came
   with") is the last event of the log at the ledger call: moved() has no cause argument, and every caller appends
   its event (the abandonment, sack, overrun, capture, cession or refugees_settled) right before it calls.
R2 (6.4.1) a `from` settlement the subsystem has not yet seen (created earlier in the step) has the entries of
   6.3.1 (its founder's race at 1000) at the call.
R3 (6.7) settlement_race rows of a quiet step are dated Y + L - 1 (the end of the step), those of a tick step Y.
R4 (6.7) the rows of "the previous phase's end" are dated Y - 1 (phases are contiguous); none at the first T1.
R5 (6.3.4) the first drift's Δ is Y - 0 (the history's first year is 0).
R6 (6.2.4) a race is awakened from the year of its `awakening` event; the meeting's presence is conflict's
   presence(c) (P3-06 5.1 with 20.1's hidden places), read live at the relations step.
R7 (7.2) the refugees of a cession (P3-06 7.7, 20.27, 22.5: the excess of a ceded settlement) go through the same
   `refugees` procedure as a capture's, so the ledger records them too, with the cession's event as the cause.
R8 (7.2, 11.1) the capture and cession refugees go to the nearest other standing settlement of the defender with
   no reach limit (11.1 amends P3-05 8.5 item 1 and the ruin procedure; 7.2 names "nearest").
R9 (6.4.3) "no open war relation between c_u and o" reads the relations of State at the relations step.
R10 (6.5) an expulsion's people n = P_s x share div 1000 is read before the expulsion; the remaining entries are
   scaled by 6.3.2's largest-remainder rule with weights their shares (as 6.3.3 item 2).
R11 (6.3.3 item 2) the other entries are scaled to fill 1000 - share_q: c_g = share_g x (1000 - share_q),
   T = 1000 - x (the sum of the others), shares c_g div T, leftovers by (c_g mod T descending, race ascending);
   when the old ruling race was the only other entry it takes all of 1000 - share_q.
R12 (6.6) a settlement ruined in the step keeps its entries but writes no row (6.7); its pending moves still mix
   into it (no row).
R13 (6.2.3) a pair's contributions are cleared at a turn and at the meeting; the causes of a turn are the
   contributions ascending (at most 8), else the last turn event, else the met event.
R14 (8.2) a turn by decay alone and a settlement_mixed caused by an arrival or an abandonment render with a
   lower-case "the" at the start (the template is rendered as written; spec defect D1).
X2 (24.3) relations-small adds conquest_settlers_permille 600 and subject_years 0
   to the spec's list (large *_memory, band_margin 0, expel_permille 1000, welcome_min -2000, abandon_reach_hops
   0), so that standing changes and expulsions happen on the small stress maps; the memory bounds are +-2000.
X3 (24.3) standing_changed happens on none of the ten relations-small maps (a hated minority is expelled before a
   phase start can change its standing, and conquests are between hated peoples); the stub-world units and H14
   cover it.
R17 (6.3.3) a standing settlement whose owner becomes 0 (no realm) changes o_s only.
R16 (17.2, 17.4) "mixed at E" needs mixed_min_permille, which the world does not store (spec defect D3): the
   reference reads it from the data root (the shipped 100 for world cases).
R15 (12.1) the epithets excluded are those of every earlier monster of the world (any kind), read from each
   name's matched form; a host of a dead kind has no epithet.
"""


# ---------------------------------------------------------------------------
# The formulas of section 6 (pure functions)
# ---------------------------------------------------------------------------

def floor_div(a, b):
    return a // b


def trunc_div(a, b):
    """C++'s integer division (truncation toward zero), b > 0."""
    q = abs(a) // b
    return q if a >= 0 else -q


def base_disposition(camp, ka, kb, stance_ab, stance_ba):
    """6.2.1 B(a, b) = camp_table[min][max] + floor_div(stance(a->b) + stance(b->a), 2); camp a dict by
    (min, max) camp codes."""
    return camp[(min(ka, kb), max(ka, kb))] + floor_div(stance_ab + stance_ba, 2)


def disposition(B, M):
    """6.2.1 D = B + (M truncated toward zero to whole units)."""
    return B + trunc_div(M, 1000)


def decay_step(M, years, per_century):
    """6.2.2 decay: d = years x rate x 1000 div 100 toward 0."""
    d = years * per_century * 1000 // 100
    if M > 0:
        return max(0, M - d)
    if M < 0:
        return min(0, M + d)
    return 0


def clamp_memory(M, t):
    return max(t["memory_min"] * 1000, min(t["memory_max"] * 1000, M))


def plain_band(D, t):
    if D <= t["hatred_max"]:
        return HATRED
    if D >= t["friend_min"]:
        return FRIENDSHIP
    return WARINESS


def band_step(band, D, t):
    """6.2.3: the band after one step's test, at most one change."""
    h, f, m = t["hatred_max"], t["friend_min"], t["band_margin"]
    if band == WARINESS:
        if D <= h:
            return HATRED
        if D >= f:
            return FRIENDSHIP
        return WARINESS
    if band == HATRED:
        return WARINESS if D > h + m else HATRED
    return WARINESS if D < f - m else FRIENDSHIP


def largest_remainder(weights, total, T):
    """{race: share}: c_g = weights[g], share_g = c_g div T, the total - sum left over one each by (c_g mod T
    descending, race ascending); races with share 0 dropped."""
    share = {g: c // T for g, c in weights.items()}
    left = total - sum(share.values())
    order = sorted(weights, key=lambda g: (-(weights[g] % T), g))
    for g in order[:left]:
        share[g] += 1
    return {g: v for g, v in share.items() if v > 0}


def mix_shares(S, G, n, Pb):
    """6.3.2 mix: S and G dicts race -> per mille, summing to 1000; n >= 1 people with shares G join a settlement
    of weight Pb."""
    races = sorted(set(S) | set(G))
    c = {g: S.get(g, 0) * Pb + G.get(g, 0) * n for g in races}
    T = Pb + n
    if T <= 0:
        return dict(S)
    return largest_remainder(c, 1000, T)


def conquest_shares(S, q, settlers_permille):
    """6.3.3 item 2: q's share x + (1000 - x) x settlers div 1000; the others scaled to fill the rest (R11)."""
    x = S.get(q, 0)
    sq = x + (1000 - x) * settlers_permille // 1000
    others = {g: v for g, v in S.items() if g != q}
    rest = 1000 - sq
    out = {q: sq}
    T = sum(others.values())
    if others and rest > 0:
        out.update(largest_remainder({g: v * rest for g, v in others.items()}, rest, T))
    return {g: v for g, v in out.items() if v > 0}


def drift_shares(S, q, permille_per_century, delta, fade_permille):
    """6.3.4: each minority loses min(share, share x rate x Δ div 100000) to q; then minorities below fade_permille
    pass their whole share to q."""
    out = dict(S)
    out.setdefault(q, 0)
    for g in sorted(out):
        if g == q:
            continue
        lost = min(out[g], out[g] * permille_per_century * delta // 100000)
        out[g] -= lost
        out[q] += lost
    for g in sorted(out):
        if g != q and out[g] < fade_permille:
            out[q] += out[g]
            out[g] = 0
    return {g: v for g, v in out.items() if v > 0}


def split_groups(n, shares, migrate_min):
    """6.4.3 item 1 (race 0): n_g = n x share div 1000, leftovers by (n x share mod 1000 descending, race); the
    groups of at least migrate_min, ascending race. Returns (kept, lost)."""
    ng = {g: n * s // 1000 for g, s in shares.items()}
    left = n - sum(ng.values())
    for g in sorted(shares, key=lambda g: (-(n * shares[g] % 1000), g))[:left]:
        ng[g] += 1
    kept = [(g, ng[g]) for g in sorted(ng) if ng[g] >= migrate_min]
    lost = [(g, ng[g]) for g in sorted(ng) if ng[g] < migrate_min]
    return kept, lost


def toll_dead(P, tau, kill_max, over_permille):
    """9.2."""
    raw = P * tau // 1000
    dead = raw if raw <= kill_max else kill_max + (raw - kill_max) * over_permille // 1000
    return max(0, min(dead, P - 1))


def hunt_permille(R, t):
    """9.3: min(hunt_max, hunt_permille + (R - 1) x hunt_raid_permille)."""
    return min(t["hunt_max_permille"], t["hunt_permille"] + (R - 1) * t["hunt_raid_permille"])


def overrun_chance(overrun_permille, M, P_t, ref):
    """14.1: min(1000, overrun x M div 1000) x f_P div 1000, f_P = min(1000, ref x 1000 div max(1, P_t))."""
    fP = min(1000, ref * 1000 // max(1, P_t))
    return min(1000, overrun_permille * M // 1000) * fP // 1000


def undead_threshold(min_dead, M):
    """14.2: S(r) >= undead_min_dead x 1000 div max(1, M)."""
    return min_dead * 1000 // max(1, M)


def road_loss(P, u, lo, hi):
    """11.1: road = P x (lo + pick(u, hi - lo + 1)) div 1000, at most P - 1 when P >= 1."""
    road = P * (lo + pnr.pick(u, hi - lo + 1)) // 1000
    return min(road, P - 1) if P >= 1 else road


# ---------------------------------------------------------------------------
# The subsystem (5, 6) over a p3_conflict_reference World
# ---------------------------------------------------------------------------

class Relations:
    """RelationsSubsystem: dispositions, shares and standing, the people ledger, expulsions."""

    def __init__(self, w, tuning, camp_table, stances):
        """camp_table: {(k_a, k_b): value}; stances: {race id: {race id: value}}."""
        self.w = w
        self.t = tuning
        w.keys["hist.relations.expel"] = cr.stream_key(w.inp.seed, "hist.relations.expel")
        self.camp = camp_table
        self.race_ids = sorted(r[0] for r in w.inp.races)
        self.camp_of = {r[0]: r[5] for r in w.inp.races}
        self.stances = stances
        self.pairs = {}
        for i, a in enumerate(self.race_ids):
            for b in self.race_ids[i + 1:]:
                B = base_disposition(self.camp, self.camp_of[a], self.camp_of[b], stances.get(a, {}).get(b, 0),
                                     stances.get(b, {}).get(a, 0))
                self.pairs[(a, b)] = dict(B=B, M=0, met=False, met_year=0, met_event=0, band=plain_band(B, tuning),
                                          turn_year=0, turn_event=0, hatred_event=0, contrib=[], last=None)
        self.rr_rows = {}          # (a, b, year) -> (disposition, band)
        self.sh = {}               # settlement -> dict(o, q, e={race: entry})
        self.known = 0
        self.sr_rows = {}          # (s, g, year) -> (share, standing)
        self.sr_last = {}          # (s, g) -> (share, standing) of the last row
        self.pending = []          # the ledger's records, in record order
        self.cursor = 1
        self.last_drift = None
        self.last_phase = None
        self.changed = set()
        self.awakened = set()
        self.aw_cursor = 1
        self.log = []              # (year, what, ...) for checks and statistics, not state

    # --- dispositions (6.2.1; the DispositionView of 7.1)
    def pair(self, a, b):
        return self.pairs[(min(a, b), max(a, b))]

    def disposition(self, a, b):
        if a == b:
            k = self.camp_of[a]
            return self.camp[(k, k)]
        p = self.pair(a, b)
        return disposition(p["B"], p["M"])

    def hatred_event(self, a, b):
        if a == b:
            return 0
        return self.pair(a, b)["hatred_event"]

    def band(self, a, b):
        """The band of a pair: its band when met, else the plain band of B (6.3.5)."""
        if a == b:
            return plain_band(self.disposition(a, a), self.t)
        p = self.pair(a, b)
        return p["band"] if p["met"] else plain_band(p["B"], self.t)

    # --- shares (6.3)
    def race_of_civ(self, c):
        return self.w.civ(c)["race"]

    def fresh_entries(self, s):
        """6.3.1's entries of a settlement (R2)."""
        x = self.w.stl(s)
        r = self.race_of_civ(x["founder_civ"])
        return {r: dict(share=1000, standing=RULING, since_year=x["founded_year"], since_event=x["founding_event"],
                        conquered=False, mixed_told=False)}

    def new_settlements(self):
        n = len(self.w.settlements)
        for s in range(self.known + 1, n + 1):
            x = self.w.stl(s)
            self.sh[s] = dict(o=x["founder_civ"], q=self.race_of_civ(x["founder_civ"]), e=self.fresh_entries(s))
            self.changed.add(s)
        self.known = n

    def entries_at_call(self, s):
        if s in self.sh:
            return {g: e["share"] for g, e in self.sh[s]["e"].items()}
        return {g: e["share"] for g, e in self.fresh_entries(s).items()}

    def target(self, s, g):
        st = self.sh[s]
        e = st["e"][g]
        if g == st["q"]:
            return RULING
        if e["conquered"] and self.w.year - e["since_year"] < self.t["subject_years"]:
            return SUBJECT
        return (SUBJECT, TOLERATED, ACCEPTED)[self.band(st["q"], g)]

    def target_new(self, s, g):
        """The target standing of a race new to s (6.3.2): not conquered."""
        st = self.sh[s]
        if g == st["q"]:
            return RULING
        return (SUBJECT, TOLERATED, ACCEPTED)[self.band(st["q"], g)]

    def set_shares(self, s, shares, event):
        """Replace s's shares; new races get the target standing, since Y and event; removed races go."""
        st = self.sh[s]
        old = st["e"]
        new = {}
        for g in sorted(shares):
            if g in old:
                e = dict(old[g], share=shares[g])
            else:
                e = dict(share=shares[g], standing=self.target_new(s, g), since_year=self.w.year, since_event=event,
                         conquered=False, mixed_told=False)
            new[g] = e
        st["e"] = new
        self.changed.add(s)

    def mix(self, s, G, n, Pb, event):
        S = {g: e["share"] for g, e in self.sh[s]["e"].items()}
        self.set_shares(s, mix_shares(S, G, n, Pb), event)

    # --- the ledger (6.4.1)
    def moved(self, frm, to, people):
        w = self.w
        self.pending.append(dict(kind="move", frm=frm, to=to, race=0, people=people, cause=len(w.events),
                                 entries=self.entries_at_call(frm), owner=w.stl(frm)["owner_civ"],
                                 P_to=w.stl(to)["population"]))

    def displaced(self, frm, race, people, cause):
        w = self.w
        self.pending.append(dict(kind="displaced", frm=frm, to=0, race=race, people=people, cause=cause,
                                 entries=self.entries_at_call(frm), owner=w.stl(frm)["owner_civ"], P_to=0))

    def pending_moves(self):
        rest = []
        for r in self.pending:
            if r["kind"] != "move":
                rest.append(r)
                continue
            self.mix(r["to"], r["entries"], r["people"], r["P_to"] - r["people"], r["cause"])
            self.log.append((self.w.year, "move", r["frm"], r["to"], r["people"]))
        self.pending = rest

    def at_war(self, a, b):
        rel = self.w.relations
        return (CIV, a, CIV, b, "war") in rel or (CIV, b, CIV, a, "war") in rel

    def pending_displacements(self, quiet):
        w, t = self.w, self.t
        recs = [r for r in self.pending if r["kind"] == "displaced"]
        self.pending = [r for r in self.pending if r["kind"] != "displaced"]
        for r in recs:
            frm = w.stl(r["frm"])
            if r["race"]:
                groups = [(r["race"], r["people"])] if r["people"] >= t["migrate_min"] else []
            else:
                groups, _lost = split_groups(r["people"], r["entries"], t["migrate_min"])
            hops = w.geo.hops(frm["region"])
            for g, ng in groups:
                best = None
                for u in w.settlements:
                    if u["id"] == frm["id"] or not w.standing(u) or not w.visible(u):
                        continue
                    cu = u["owner_civ"]
                    if cu == 0:
                        continue
                    civ = w.civ(cu)
                    if civ["ended_year"] is not None or not w.visible(civ) or w.lying_low(cu):
                        continue
                    if cu == r["owner"] or (r["owner"] and self.at_war(cu, r["owner"])):
                        continue
                    h = hops.get(u["region"])
                    if h is None or h > t["migrate_hops"]:
                        continue
                    rc = civ["race"]
                    D = self.disposition(rc, g)
                    if D < t["welcome_min"]:
                        continue
                    room = max(0, 2 * w.geo.capacity(u["region"], rc) - u["population"])
                    if room < t["migrate_min"]:
                        continue
                    key = (h, 0 if rc == g else 1, -D, (u["x"] - frm["x"]) ** 2 + (u["y"] - frm["y"]) ** 2, u["id"])
                    if best is None or key < best[0]:
                        best = (key, u, room)
                if best is None:
                    self.log.append((w.year, "lost", r["frm"], g, ng))
                    continue
                _k, u, room = best
                cu = u["owner_civ"]
                if quiet and w.quiet_events_left(cu) <= 0:
                    self.log.append((w.year, "lost-budget", r["frm"], g, ng))
                    continue
                taken = min(ng, room)
                st = self.target_new(u["id"], g) if u["id"] in self.sh else RULING
                e = w.append("refugees_arrived", region=u["region"], settlement=u["id"],
                             participants=(("settlement", SETTLEMENT, u["id"]), ("civilization", CIV, cu),
                                           ("origin", SETTLEMENT, r["frm"]), ("race", RACE, g)),
                             values=(taken, st), causes=(r["cause"],))
                Pu = u["population"]
                u["population"] = Pu + taken
                self.mix(u["id"], {g: 1000}, taken, Pu, e)
                self.log.append((w.year, "arrival", r["frm"], u["id"], g, taken, ng))

    # --- the log (6.2.2)
    def read_log(self):
        w, t = self.w, self.t
        last = len(w.events)
        delta = {"war_won": ("winner", "loser", t["war_memory"]), "peace": ("winner", "loser", t["war_memory"]),
                 "truce": ("parties", "parties", t["war_memory"]),
                 "civ_conquered": ("civilization", "conqueror", t["conquest_memory"]),
                 "sacked": ("attacker", "defender", t["sack_memory"]),
                 "settlement_captured": ("attacker", "defender", t["capture_memory"]),
                 "people_expelled": ("civilization", "race", t["expel_memory"]),
                 "refugees_arrived": ("civilization", "race", -t["shelter_memory"])}
        for e in range(self.cursor, last + 1):
            ev = w.events[e - 1]
            key = ev["key"]
            if key not in delta:
                continue
            rx, ry, d = delta[key]
            k = w.cat_by_key[key]
            seq = {r["role"]: r["seq"] for r in k["roles"]}
            parts = sorted(ev["participants"])
            px = [x for (_s, rs, _t, x) in parts if rs == seq[rx]]
            py = [x for (_s, rs, _t, x) in parts if rs == seq[ry]]
            if key == "truce":
                ra, rb = self.race_of_civ(px[0]), self.race_of_civ(px[1])
            elif ry == "race":
                ra, rb = self.race_of_civ(px[0]), py[0]
            else:
                ra, rb = self.race_of_civ(px[0]), self.race_of_civ(py[0])
            if ra == rb:
                continue
            p = self.pair(ra, rb)
            if not p["met"]:
                continue
            p["M"] = clamp_memory(p["M"] - d * 1000, t)
            p["contrib"] = (p["contrib"] + [e])[-8:]
        self.cursor = last + 1

    def decay(self, years):
        for k in sorted(self.pairs):
            p = self.pairs[k]
            p["M"] = decay_step(p["M"], years, self.t["memory_decay_per_century"])

    def rr_row(self, a, b, year):
        p = self.pairs[(a, b)]
        v = (disposition(p["B"], p["M"]), p["band"])
        if p["last"] != v:
            self.rr_rows[(a, b, year)] = v
            p["last"] = v

    def turns(self, quiet):
        w = self.w
        for (a, b) in sorted(self.pairs):
            p = self.pairs[(a, b)]
            if not p["met"]:
                continue
            D = disposition(p["B"], p["M"])
            nb = band_step(p["band"], D, self.t)
            if nb == p["band"]:
                continue
            if quiet and w.quiet_events_left(0) <= 0:
                continue
            causes = sorted(set(p["contrib"])) if p["contrib"] else [p["turn_event"] or p["met_event"]]
            e = w.append("disposition_turned", participants=(("race_a", RACE, a), ("race_b", RACE, b)),
                         values=(D, nb), causes=tuple(causes))
            p["band"], p["turn_year"], p["turn_event"], p["contrib"] = nb, w.year, e, []
            if nb == HATRED:
                p["hatred_event"] = e
            self.rr_row(a, b, w.year)
            self.log.append((w.year, "turn", a, b, D, nb))

    def note_awakenings(self):
        w = self.w
        for e in range(self.aw_cursor, len(w.events) + 1):
            ev = w.events[e - 1]
            if ev["key"] == "awakening":
                for (_s, _rs, t, x) in ev["participants"]:
                    if t == RACE:
                        self.awakened.add(x)
        self.aw_cursor = len(w.events) + 1

    def meetings(self, quiet):
        w, t = self.w, self.t
        self.note_awakenings()
        if t["meet_hops"] < 0:
            return
        civs = [c for c in w.civs if c["ended_year"] is None and w.visible(c)]
        pres = {c["id"]: w.presence(c["id"]) for c in civs}
        for (a, b) in sorted(self.pairs):
            p = self.pairs[(a, b)]
            if p["met"] or a not in self.awakened or b not in self.awakened:
                continue
            xs = [c for c in civs if c["race"] == a]
            ys = [c for c in civs if c["race"] == b]
            best = None
            for x in xs:
                for Rx in pres[x["id"]]:
                    hops = w.geo.hops(Rx)
                    for y in ys:
                        for Ry in pres[y["id"]]:
                            h = hops.get(Ry)
                            if h is None:
                                continue
                            key = (h, Rx, Ry, x["id"], y["id"])
                            if best is None or key < best:
                                best = key
            if best is None or best[0] > t["meet_hops"]:
                continue
            if quiet and w.quiet_events_left(0) <= 0:
                continue
            h, Rx, Ry, xi, yi = best
            cx, cy = w.civ(xi), w.civ(yi)
            young = cx if (cx["founded_year"], cx["id"]) > (cy["founded_year"], cy["id"]) else cy
            region = Rx if young is cx else Ry
            band = plain_band(p["B"], t)
            e = w.append("peoples_met", region=region, participants=(("race_a", RACE, a), ("race_b", RACE, b)),
                         values=(p["B"], band), causes=(young["founding_event"],))
            p.update(met=True, met_year=w.year, met_event=e, M=0, band=band, contrib=[])
            if band == HATRED:
                p["hatred_event"] = 0   # 7.1: only a disposition_turned into band 0 is a hatred event
            self.rr_row(a, b, w.year)
            self.log.append((w.year, "met", a, b, h, region))

    # --- owner changes, drift, standings, mixed (6.3.3 to 6.3.6)
    def owner_changes(self):
        w = self.w
        for s in range(1, self.known + 1):
            x = w.stl(s)
            if not w.standing(x):
                continue
            st = self.sh[s]
            c = x["owner_civ"]
            if c == st["o"]:
                continue
            st["o"] = c
            if c == 0:   # R17: a standing settlement left without an owner keeps its people and rulers
                continue
            q = self.race_of_civ(c)
            if q == st["q"]:
                continue
            ev = x.get("owner_event", 0)
            old = st["q"]
            S = {g: e["share"] for g, e in st["e"].items()}
            new = conquest_shares(S, q, self.t["conquest_settlers_permille"])
            ents = {}
            for g in sorted(new):
                e = dict(st["e"][g]) if g in st["e"] else dict(standing=0, since_year=w.year, since_event=ev,
                                                               conquered=False, mixed_told=False)
                e["share"] = new[g]
                ents[g] = e
            if old in ents:
                ents[old].update(standing=SUBJECT, conquered=True, since_year=w.year, since_event=ev)
            ents[q].update(standing=RULING, since_event=ev)
            if old in ents and old != q:
                pass
            st["e"], st["q"] = ents, q
            for g in sorted(ents):
                if g not in (q, old):
                    ents[g]["standing"] = self.target(s, g)
            self.changed.add(s)
            self.log.append((w.year, "owner", s, old, q))

    def drift(self):
        w, t = self.w, self.t
        last = self.last_drift if self.last_drift is not None else 0
        delta = w.year - last
        self.last_drift = w.year
        for s in range(1, self.known + 1):
            x = w.stl(s)
            st = self.sh[s]
            if not w.standing(x) or len(st["e"]) <= 1:
                continue
            S = {g: e["share"] for g, e in st["e"].items()}
            new = drift_shares(S, st["q"], t["drift_permille_per_century"], delta, t["fade_permille"])
            if new != S:
                st["e"] = {g: dict(st["e"][g], share=v) for g, v in new.items()}
                self.changed.add(s)

    def standings(self, quiet):
        w = self.w
        for s in range(1, self.known + 1):
            x = w.stl(s)
            if not w.standing(x):
                continue
            st = self.sh[s]
            for g in sorted(st["e"]):
                if g == st["q"]:
                    continue
                e = st["e"][g]
                tg = self.target(s, g)
                if tg == e["standing"]:
                    continue
                if quiet and w.quiet_events_left(st["o"]) <= 0:
                    continue
                p = self.pair(st["q"], g)
                cause = p["turn_event"] if p["turn_event"] > e["since_event"] else e["since_event"]
                ev = w.append("standing_changed", region=x["region"], settlement=s,
                              participants=(("settlement", SETTLEMENT, s), ("civilization", CIV, st["o"]),
                                            ("race", RACE, g)), values=(tg, e["share"]), causes=(cause,))
                by_conquest = e["conquered"] and w.year - e["since_year"] < self.t["subject_years"]
                e.update(standing=tg, since_event=ev, since_year=w.year)
                if not (tg == SUBJECT and by_conquest):
                    e["conquered"] = False
                self.changed.add(s)
                self.log.append((w.year, "standing", s, g, tg))

    def mixed(self, quiet):
        w, t = self.w, self.t
        for s in range(1, self.known + 1):
            x = w.stl(s)
            if not w.standing(x) or not w.visible(x):
                continue
            st = self.sh[s]
            for g in sorted(st["e"]):
                e = st["e"][g]
                if g == st["q"] or e["share"] < t["mixed_min_permille"] or e["mixed_told"]:
                    continue
                if quiet and w.quiet_events_left(st["o"]) <= 0:
                    continue
                w.append("settlement_mixed", region=x["region"], settlement=s,
                         participants=(("settlement", SETTLEMENT, s), ("civilization", CIV, st["o"]),
                                       ("race", RACE, g), ("ruling", RACE, st["q"])),
                         values=(e["share"],), causes=(e["since_event"],))
                e["mixed_told"] = True
                self.log.append((w.year, "mixed", s, g))

    # --- expulsions (6.5)
    def expulsions(self):
        w, t = self.w, self.t
        for s in range(1, self.known + 1):
            x = w.stl(s)
            if not w.standing(x) or not w.visible(x):
                continue
            st = self.sh[s]
            o = st["o"]
            if o == 0:
                continue
            civ = w.civ(o)
            if civ["ended_year"] is not None or not w.visible(civ) or w.lying_low(o):
                continue
            for g in sorted(st["e"]):
                e = st["e"][g]
                if g == st["q"] or e["standing"] > TOLERATED or self.band(st["q"], g) != HATRED:
                    continue
                if e["share"] > t["expel_share_max"]:
                    continue
                P = x["population"]
                n = P * e["share"] // 1000
                if n < t["migrate_min"]:
                    continue
                if not pp.chance_pm(w.draw("hist.relations.expel", s, g, 0), t["expel_permille"]):
                    continue
                p = self.pair(st["q"], g)
                causes = sorted({e["since_event"]} | ({p["turn_event"]} if p["turn_event"] > e["since_event"]
                                                      else set()))
                ev = w.append("people_expelled", region=x["region"], settlement=s,
                              participants=(("settlement", SETTLEMENT, s), ("civilization", CIV, o), ("race", RACE, g)),
                              values=(n,), causes=tuple(c for c in causes if c))
                x["population"] = P - n
                w.last_red[s] = ev   # note_reduction
                rest = {h: v["share"] for h, v in st["e"].items() if h != g}
                T = sum(rest.values())
                new = largest_remainder({h: v * 1000 for h, v in rest.items()}, 1000, T)
                st["e"] = {h: dict(st["e"][h], share=v) for h, v in new.items()}
                self.changed.add(s)
                self.displaced(s, g, n, ev)
                self.log.append((w.year, "expel", s, g, n))
                break   # at most one expulsion per settlement per year

    # --- rows (6.7)
    def row_year(self, quiet):
        return self.w.year + self.w.years - 1 if quiet else self.w.year

    def end_rows(self, quiet):
        w = self.w
        y = self.row_year(quiet)
        for s in sorted(self.changed):
            if not w.standing(w.stl(s)):
                continue
            self.sr_write(s, y)
        self.changed = set()

    def sr_write(self, s, y):
        st = self.sh[s]
        races = set(st["e"]) | {g for (s2, g), v in self.sr_last.items() if s2 == s and v[0] > 0}
        for g in sorted(races):
            e = st["e"].get(g)
            v = (e["share"], e["standing"]) if e is not None else (0, 0)
            if self.sr_last.get((s, g), (0, 0) if e is None else None) != v:
                self.sr_rows[(s, g, y)] = v
                self.sr_last[(s, g)] = v

    def phase_start(self, quiet):
        w = self.w
        if self.last_phase is not None and w.phase != self.last_phase:
            for (a, b) in sorted(self.pairs):
                if self.pairs[(a, b)]["met"]:
                    self.rr_row(a, b, w.year - 1)
        changed = self.last_phase is None or w.phase != self.last_phase
        self.last_phase = w.phase
        if changed:
            self.drift()
            self.standings(quiet)

    # --- the steps (6.1)
    def t1(self):
        self.new_settlements()
        self.pending_moves()
        self.phase_start(False)
        self.pending_displacements(False)
        self.mixed(False)
        self.end_rows(False)

    def q1(self):
        self.new_settlements()
        self.pending_moves()
        self.phase_start(True)
        self.pending_displacements(True)
        self.mixed(True)
        self.end_rows(True)

    def t4(self):
        self.new_settlements()
        self.pending_moves()
        self.pending_displacements(False)
        self.mixed(False)
        self.end_rows(False)

    def t6(self):
        self.new_settlements()
        self.pending_moves()
        self.read_log()
        self.owner_changes()
        self.expulsions()
        self.pending_displacements(False)
        self.decay(1)
        self.turns(False)
        self.meetings(False)
        self.mixed(False)
        self.end_rows(False)

    def q4(self):
        self.new_settlements()
        self.pending_moves()
        self.read_log()
        self.owner_changes()
        self.pending_displacements(True)
        self.decay(self.w.years)
        self.turns(True)
        self.meetings(True)
        self.mixed(True)
        self.end_rows(True)

    def finish(self, E):
        for (a, b) in sorted(self.pairs):
            if self.pairs[(a, b)]["met"]:
                self.rr_row(a, b, E - 1)
        w = self.w
        self.new_settlements()
        for s in range(1, self.known + 1):
            if w.standing(w.stl(s)):
                self.sr_write(s, E - 1)

    def race_relation_rows(self):
        return sorted((a, b, y) + v for (a, b, y), v in self.rr_rows.items())

    def settlement_race_rows(self):
        return sorted((s, g, y) + v for (s, g, y), v in self.sr_rows.items())


def relations_of(dat, w):
    """The subsystem for a conflict World w with the data's history.relations and stances."""
    t = dat.relations
    camp = {}
    import p3_conflict_reference as pcr
    for (ka, kb), f in pcr.CAMP_FIELD.items():
        camp[(ka, kb)] = dat.conflict["camp"][f]
    key_id = {dat.race[r]["key"]: r for r in dat.race}
    stances = {}
    for r, d in dat.race.items():
        stances[r] = {key_id[k]: v for k, v in d.get("stances", []) if k in key_id}
    return Relations(w, t, camp, stances)


# ---------------------------------------------------------------------------
# Fixtures (24.3): relations-small, monsters-small with flights, zero (7.4)
# ---------------------------------------------------------------------------

RELATIONS_SMALL = dict(war_memory=400, conquest_memory=600, sack_memory=300, capture_memory=200, expel_memory=300,
                       shelter_memory=300, memory_min=-2000, memory_max=2000, band_margin=0, expel_permille=1000,
                       welcome_min=-2000,
                       # reading X2: beyond 24.3's list, so that standings change and expulsions pass the majority
                       # guard on small maps: more settlers, a short subjection, a fast drift
                       conquest_settlers_permille=600, subject_years=0)
MONSTERS_SMALL_FEAR = dict(fear_raids=2, fear_permille=1000)


def relations_small_data(dat=None):
    """24.3 relations-small (reading X1 of P5-02: in-memory edits of the conflict stress data): large *_memory
    values (and memory bounds +-2000 so that they show), band_margin 0, expel_permille 1000, welcome_min -2000,
    peoples' abandon_reach_hops 0."""
    import p3_conflict_reference as pcr
    d = pcr.stress_data(dat)
    d = pcr.edited(d, peoples=dict(abandon_reach_hops=0))
    d.relations = dict(d.relations, **RELATIONS_SMALL)
    return d


def monsters_small_fear_data(dat=None):
    """24.3: monsters-small with fear_raids 2 and fear_permille 1000, so that flights happen."""
    import p5_02_monsters_reference as p502
    d = p502.monsters_small_data(dat)
    d.base = __import__("copy").copy(d.base)
    d.base.monsters_tuning = dict(d.base.monsters_tuning, **MONSTERS_SMALL_FEAR)
    return d


def zero_data(dat=None):
    """7.4: the zero tuning, every race's stances empty."""
    import copy
    import p3_conflict_reference as pcr
    d = pcr.edited(dat or pcr.data())
    d.relations = dict(pdr.RELATIONS_ZERO_024)
    d.race = copy.deepcopy(d.race)
    for r in d.race:
        d.race[r]["stances"] = []
    return d


def without_relations(fn):
    import p3_conflict_reference as pcr
    saved = pcr.FLAGS["relations"]
    pcr.FLAGS["relations"] = False
    try:
        return fn()
    finally:
        pcr.FLAGS["relations"] = saved


I0_TABLES = ("event", "event_participant", "event_cause", "event_value", "phase", "civilization", "settlement",
             "settlement_census", "region_control", "relation", "region_name", "beast_population", "monster", "site")


def i0_differences(run, base=None):
    """7.4: the tables that differ between the zero tuning with the subsystem and the same data without it (no
    ledger, no view)."""
    import p3_conflict_reference as pcr
    base = base or pcr.data()
    zero = run(zero_data(base))
    off = without_relations(lambda: run(zero_data(base)))
    return [t for t in I0_TABLES if zero.get(t, []) != off.get(t, [])], zero, off


def fixture_case(seed, dat):
    import p3_conflict_reference as pcr
    span, races, civs, intensity = pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
    inp, _g = pcr.synthetic_inputs(seed, dat, (span, races, civs, intensity))
    return pcr.history_stage(dat, inp)


# ---------------------------------------------------------------------------
# Invariants (19) and the reader checks (16.4) on rows
# ---------------------------------------------------------------------------

class Rows:
    """Indexes over a world's history rows (res-style tuples) with the peoples neighbours (open borders)."""

    def __init__(self, res, borders):
        self.res = res
        self.key = {k[0]: k[1] for k in res["event_kind"]}
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
        self.stl = {s[0]: s for s in res["settlement"]}
        self.civ = {c[0]: c for c in res["civilization"]}
        self.nb = {}
        for a, b in borders:
            self.nb.setdefault(a, set()).add(b)
            self.nb.setdefault(b, set()).add(a)
        self._hops = {}
        self.census = {}
        for r in res["settlement_census"]:
            self.census.setdefault(r[0], []).append(r)
        self.ruin_ev = pe.ruin_events_from_rows(res)

    def kind_of(self, e):
        return self.key[self.ev[e][3]]

    def value(self, e, key):
        return self.vals.get(e, {}).get(self.vkeys[self.ev[e][3]][key])

    def role(self, e, role):
        rs = self.rkeys[self.ev[e][3]].get(role)
        return [x[3] for x in sorted(self.parts.get(e, [])) if x[1] == rs]

    def events_of(self, *keys):
        return [e for e in self.res["event"] if self.key[e[3]] in keys]

    def hops(self, r0):
        if r0 not in self._hops:
            d = {r0: 0}
            fifo = [r0]
            i = 0
            while i < len(fifo):
                x = fifo[i]
                i += 1
                for y in sorted(self.nb.get(x, ())):
                    if y not in d:
                        d[y] = d[x] + 1
                        fifo.append(y)
            self._hops[r0] = d
        return self._hops[r0]

    def owner_at(self, s, y):
        """The owner of settlement s at the end of year y by its census rows (P4-01's reading), else its row."""
        best = None
        for r in self.census.get(s, ()):
            if r[1] <= y:
                best = r
        return best[2] if best is not None else self.stl[s][8]

    def ruined_year(self, s):
        e = self.ruin_ev.get(s, 0)
        return self.ev[e][1] if e else None


def open_borders(rows_region_border):
    """PeoplesView's neighbours (P3-05 4.4): the borders with walk pairs or a pass."""
    return [(b["a"], b["b"]) for b in rows_region_border if b["walk_pairs"] > 0 or b["pass_id"] > 0]


def shares_in_force(sr_rows):
    """{settlement: {race: [(year, share, standing)]}} in key order."""
    out = {}
    for s, g, y, sh, st in sr_rows:
        out.setdefault(s, {}).setdefault(g, []).append((y, sh, st))
    return out


def in_force(per_race, y):
    """{race: (share, standing)} of the rows in force at y (the last row per race with year <= y, share > 0)."""
    out = {}
    for g, lst in per_race.items():
        last = None
        for (yy, sh, st) in lst:
            if yy <= y:
                last = (sh, st)
        if last is not None and last[0] > 0:
            out[g] = last
    return out


def invariant_violations(res, t, E, borders, base_of, race_key, monster_kinds=None, ghost_fade_years=0,
                         log=None):
    """19's invariants 1 to 6, 8 to 10 on rows. base_of(a, b): B from the data; race_key: {race id: key};
    monster_kinds: [(key, def)] (8); log: the subsystem's run log (Relations.log), for what rows do not keep."""
    x = Rows(res, borders)
    out = []
    rr = sorted(res.get("race_relation", []))
    sr = sorted(res.get("settlement_race", []))
    civ_race = {c[0]: c[2] for c in res["civilization"]}
    met = {}
    for e in x.events_of("peoples_met"):
        a, b = x.role(e[0], "race_a")[0], x.role(e[0], "race_b")[0]
        if (a, b) in met:
            out.append("1: %d-%d met twice" % (a, b))
        met.setdefault((a, b), e)
    rows_of = {}
    for r in rr:
        rows_of.setdefault((r[0], r[1]), []).append(r)
    for k, lst in rows_of.items():
        if k not in met:
            out.append("1: pair %d-%d has rows and no meeting" % k)
            continue
        if lst[0][2] != met[k][1]:
            out.append("1: pair %d-%d's first row %d is not at its meeting %d" % (k + (lst[0][2], met[k][1])))
        B = x.value(met[k][0], "disposition")
        for r in lst:   # 2
            if not (B + t["memory_min"] <= r[3] <= B + t["memory_max"]):
                out.append("2: row %r outside B %d + memory bounds" % (r, B))
    band = {k: x.value(e[0], "band") for k, e in met.items()}
    for e in x.events_of("disposition_turned"):
        k = (x.role(e[0], "race_a")[0], x.role(e[0], "race_b")[0])
        if k not in met or met[k][0] > e[0]:
            out.append("1: e%d turns a pair before its meeting" % e[0])
            continue
        nb = x.value(e[0], "band")
        if abs(nb - band[k]) != 1:
            out.append("1: e%d turns %d-%d from band %d to %d" % (e[0], k[0], k[1], band[k], nb))
        band[k] = nb
    # 3
    per = shares_in_force(sr)
    for s, races in per.items():
        ys = sorted({y for lst in races.values() for (y, _sh, _st) in lst})
        ry = x.ruined_year(s)
        for y in ys:
            if ry is not None and ry <= y and not any(yy == y for lst in races.values() for (yy, a, b) in lst):
                continue
            f = in_force(races, y)
            if sum(v[0] for v in f.values()) != 1000:
                out.append("3: settlement %d shares at %d sum to %d" % (s, y, sum(v[0] for v in f.values())))
            rul = [g for g, v in f.items() if v[1] == RULING]
            if len(rul) != 1:
                out.append("3: settlement %d has %d ruling races at %d" % (s, len(rul), y))
            elif civ_race.get(x.owner_at(s, y)) != rul[0] and (ry is None or y < ry):
                out.append("3: settlement %d's ruling race %d at %d is not its owner's" % (s, rul[0], y))
    # 4
    arrivals_into = {}
    for e in x.events_of("refugees_arrived"):
        arrivals_into.setdefault(e[6], []).append(e[1])
    for s, races in per.items():
        y0 = min(yy for lst in races.values() for (yy, _a, _b) in lst)
        first = in_force(races, y0)
        fe = x.stl[s][7]
        fk = x.kind_of(fe) if fe in x.ev else ""
        if fk in ("colony_founded", "settlement_founded") and y0 not in arrivals_into.get(s, []):
            want = {civ_race[x.stl[s][5]]: (1000, RULING)}
            if first != want:
                out.append("4: colony %d's first rows %r are not its founder's race" % (s, first))
        if fk == "refugees_settled":
            o = x.role(fe, "origin")[0]
            if o in per:
                want = {g: v[0] for g, v in in_force(per[o], y0).items()}
                got = {g: v[0] for g, v in first.items()}
                if want and got != want:
                    out.append("4: refuge %d's first shares %r are not its origin %d's %r" % (s, got, o, want))
    # 5
    for e in x.events_of("refugees_arrived"):
        u, o = x.role(e[0], "settlement")[0], x.role(e[0], "origin")[0]
        h = x.hops(x.stl[o][4]).get(x.stl[u][4])
        if h is None or h > t["migrate_hops"]:
            out.append("5: e%d host %d is %s hops from origin %d" % (e[0], u, h, o))
        cu = x.role(e[0], "civilization")[0]
        cs = x.causes.get(e[0], [])
        oc = 0
        if cs:
            for role in ("defender", "civilization", "from"):
                r = x.rkeys[x.ev[cs[0]][3]].get(role)
                if r is not None:
                    oc = x.role(cs[0], role)[0]
                    break
        if oc and any(rel[4] == "war" and {rel[1], rel[3]} == {cu, oc} and rel[5] <= e[1] and
                      (rel[6] is None or rel[6] > e[1]) for rel in res["relation"]):
            out.append("5: e%d host owner %d at war with the origin's owner %d" % (e[0], cu, oc))
        if cu == oc:
            out.append("5: e%d host owner %d is the origin's owner" % (e[0], cu))
    # 6
    for e in x.events_of("people_expelled"):
        s, g = x.role(e[0], "settlement")[0], x.role(e[0], "race")[0]
        c = x.role(e[0], "civilization")[0]
        q = civ_race.get(c)
        k = (min(q, g), max(q, g))
        if k in met and met[k][1] < e[1]:
            bd = None
            for r in rows_of.get(k, []):
                if r[2] < e[1]:
                    bd = r[4]
            bd = band.get(k) if bd is None else bd
        else:
            bd = plain_band(base_of(q, g), t)
        if bd != HATRED:
            out.append("6: e%d expels race %d not in hatred with the ruling race %d" % (e[0], g, q))
        before = in_force(per.get(s, {}), e[1] - 1)
        if g in before and before[g][0] > t["expel_share_max"] and not any(
                yy == e[1] for lst in per.get(s, {}).values() for (yy, _a, _b) in lst):
            out.append("6: e%d expels a share %d above expel_share_max" % (e[0], before[g][0]))
    # 8
    if monster_kinds is not None:
        used = set()
        for m in sorted(res.get("monster", [])):
            key, kd = monster_kinds[m[2] - 1]
            if kd["wake"] == "dead":
                if not any(f.replace("{place}", "") in m[1] and len(m[1]) > len(f.replace("{place}", ""))
                           for f in kd["name_forms"] if "{place}" in f):
                    out.append("8: host %d's name %r has no place" % (m[0], m[1]))
                continue
            import p5_02_monsters_reference as p502
            f = p502.matched_form(m[1], kd["name_forms"])
            ep = p502.epithet(f) if f else ""
            if ep and ep in used:
                unused = [p502.epithet(g) for g in kd["name_forms"] if p502.epithet(g) and p502.epithet(g) not in used]
                if unused:
                    out.append("8: monster %d repeats the epithet %r while %r is unused" % (m[0], ep, unused[0]))
            if ep:
                used.add(ep)
    # 9
    waking = {}
    for e in x.events_of("monster_appeared", "undead_rose", "monster_woke"):
        waking.setdefault(x.role(e[0], "monster")[0], set()).add(e[0])
    for e in x.events_of("monster_raid"):
        m = x.role(e[0], "monster")[0]
        cs = x.causes.get(e[0], [])
        wk = [c for c in cs if c in waking.get(m, set())]
        rd = [c for c in cs if x.kind_of(c) == "monster_raid"]
        if len(wk) != 1 or len(rd) > 1 or len(wk) + len(rd) != len(cs):
            out.append("9: raid e%d's causes %r" % (e[0], cs))
        for c in rd:
            if x.ev[c][6] != e[6] or x.role(c, "monster")[0] != m:
                out.append("9: raid e%d cites e%d, not an earlier raid of its monster on its place" % (e[0], c))
    for e in x.events_of("settlement_fled"):
        s = e[6]
        nxt = [f for f in x.events_of("settlement_abandoned", "settlement_abandoned_to")
               if f[6] == s and f[1] == e[1] and e[0] in x.causes.get(f[0], [])]
        if not nxt:
            out.append("9: settlement_fled e%d not followed by its abandonment" % e[0])
        cen = [r for r in x.census.get(s, ()) if r[1] < e[1]]
        if cen and cen[-1][3] not in (1, 2):
            out.append("9: settlement_fled e%d: settlement %d was a %d" % (e[0], s, cen[-1][3]))
    # 10
    endings = pe.endings_from_rows(res)
    faded = {x.ev[e[0]][6]: e[1] for e in x.events_of("ruin_faded")}
    for s, fy in faded.items():
        if ghost_fade_years == 0 and endings.get(s) in (2, 3, 4, 5, 6):
            out.append("10: settlement %d of ending %d faded" % (s, endings[s]))
        for st in res.get("site", []):
            if st[6] == s and st[10] <= fy and (st[11] is None or st[11] > fy):
                out.append("10: settlement %d faded at %d while site %d was open" % (s, fy, st[0]))
    if log is not None:
        out += log_violations(res, t, log)
    return out


def log_violations(res, t, log):
    """5 and 6 on the run's log: the room, the population changes."""
    out = []
    for item in log:
        if item[1] == "arrival":
            _y, _w, frm, u, g, taken, ng = item
            if taken > ng:
                out.append("5: arrival into %d took %d of a group of %d" % (u, taken, ng))
    return out


def reader_problems(res, E):
    """16.4's checks of race_relation and settlement_race (the first failing row of each table) and the events
    and settlement.ending checks; returns the messages in order."""
    out = []
    races = {r[0] for r in res.get("race", [])} if res.get("race") else None
    x_key = {k[0]: k[1] for k in res["event_kind"]}
    met = {}
    parts = {}
    for p in res["event_participant"]:
        parts.setdefault(p[0], []).append(p)
    for e in res["event"]:
        if x_key[e[3]] == "peoples_met":
            ps = sorted(parts.get(e[0], []))
            k = (ps[0][4], ps[1][4])
            met.setdefault(k, []).append(e)
    prev = None
    last_pair = {}
    for r in list(res.get("race_relation", [])):   # as stored (key order in a world file)
        key = "(%d, %d, %d)" % r[:3]

        def bad(why):
            out.append("corrupt table 'race_relation': row %s: %s" % (key, why))
        if prev is not None and r[:3] <= prev[:3]:
            bad("not above the previous row")
            break
        if races is not None and (r[0] not in races or r[1] not in races):
            bad("unknown race %d" % (r[0] if r[0] not in races else r[1]))
            break
        if not r[0] < r[1]:
            bad("race_a %d is not below race_b %d" % (r[0], r[1]))
            break
        if not 0 <= r[2] <= E - 1:
            bad("year %d outside 0 to %d" % (r[2], E - 1))
            break
        if not -4000 <= r[3] <= 4000:
            bad("disposition %d outside -4000 to 4000" % r[3])
            break
        if not 0 <= r[4] <= 2:
            bad("band %d outside 0 to 2" % r[4])
            break
        lp = last_pair.get(r[:2])
        if lp is not None and lp[3:] == r[3:]:
            bad("equals the previous row of this pair")
            break
        if lp is None and not any(e[1] == r[2] for e in met.get(r[:2], [])):
            bad("first row is not at its meeting")
            break
        last_pair[r[:2]] = r
        prev = r
    stl = {s[0]: s for s in res["settlement"]}
    prev = None
    last = {}
    per = {}
    for r in list(res.get("settlement_race", [])):
        key = "(%d, %d, %d)" % r[:3]

        def bad(why):
            out.append("corrupt table 'settlement_race': row %s: %s" % (key, why))
        if prev is not None and r[:3] <= prev[:3]:
            bad("not above the previous row")
            break
        if r[0] not in stl:
            bad("unknown settlement %d" % r[0])
            break
        if races is not None and r[1] not in races:
            bad("unknown race %d" % r[1])
            break
        if not stl[r[0]][6] <= r[2] <= E - 1:
            bad("year %d outside %d to %d" % (r[2], stl[r[0]][6], E - 1))
            break
        if not 0 <= r[3] <= 1000:
            bad("share %d outside 0 to 1000" % r[3])
            break
        if not 0 <= r[4] <= 3:
            bad("standing %d outside 0 to 3" % r[4])
            break
        if r[3] == 0 and r[4] != 0:
            bad("standing %d with share 0" % r[4])
            break
        if last.get(r[:2]) == r[3:]:
            bad("equals the previous row of this settlement and race")
            break
        last[r[:2]] = r[3:]
        per.setdefault(r[0], {}).setdefault(r[1], []).append((r[2], r[3], r[4]))
        prev = r
    else:
        for s in sorted(per):
            ys = sorted({y for lst in per[s].values() for (y, _a, _b) in lst})
            done = False
            for y in ys:
                f = in_force(per[s], y)
                n = sum(v[0] for v in f.values())
                rows_y = [(g, y) for g, lst in per[s].items() for (yy, _a, _b) in lst if yy == y]
                g0, y0 = min(rows_y)
                key = "(%d, %d, %d)" % (s, g0, y0)
                if n != 1000:
                    out.append("corrupt table 'settlement_race': row %s: shares in force at %d sum to %d" % (key, y, n))
                    done = True
                    break
                k = sum(1 for v in f.values() if v[1] == RULING)
                if k != 1:
                    out.append("corrupt table 'settlement_race': row %s: %d ruling races at %d" % (key, k, y))
                    done = True
                    break
            if done:
                break
    rr_pairs = {r[:2] for r in res.get("race_relation", [])}
    for k, evs in sorted(met.items()):
        for e in evs:
            if k not in rr_pairs:
                out.append("corrupt table 'event': row %d: peoples_met without its rows" % e[0])
        if len(evs) > 1:
            out.append("corrupt table 'event': row %d: %d-%d met twice" % (evs[1][0], k[0], k[1]))
    if races is not None:
        for p in sorted(res["event_participant"]):
            if p[3] == RACE and p[4] not in races:
                out.append("corrupt table 'event': row %d: race %d has no row" % (p[0], p[4]))
                break
    return out


# ---------------------------------------------------------------------------
# Statistics ([stats] of 17.4 and the counts of 21) and the golden file
# ---------------------------------------------------------------------------

def stats_of(res, E, mixed_min=100):
    """The 17.4 [stats] keys of a history (rows, and mixed_min_permille: R16)."""
    x_key = {k[0]: k[1] for k in res["event_kind"]}
    vkeys = {}
    for k, seq, key in res["event_kind_value"]:
        vkeys.setdefault(k, {})[key] = seq
    vals = {}
    for e, seq, v in res["event_value"]:
        vals.setdefault(e, {})[seq] = v
    ev_of = {}
    for e in res["event"]:
        ev_of.setdefault(x_key[e[3]], []).append(e)

    def val(e, key):
        return vals.get(e[0], {}).get(vkeys[e[3]][key], 0)
    rr = sorted(res.get("race_relation", []))
    races = sorted({r[0] for r in res.get("race", [])}) if res.get("race") else sorted(
        {c[2] for c in res["civilization"]})
    n_pairs = len(races) * (len(races) - 1) // 2
    last = {}
    for r in rr:
        last[r[:2]] = r
    per = shares_in_force(res.get("settlement_race", []))
    mixed = 0
    for s in res["settlement"]:
        if s[10] in (1, 2) and s[0] in per:
            f = in_force(per[s[0]], E - 1)
            rul = [g for g, v in f.items() if v[1] == RULING]
            if any(g not in rul and v[0] >= mixed_min for g, v in f.items()):   # R16
                mixed += 1
    endings = pe.endings_from_rows(res)
    ruins = [s for s in res["settlement"] if s[10] == 3]
    return [
        ("race_relations", len(rr)), ("settlement_races", len(res.get("settlement_race", []))),
        ("race_pairs", n_pairs), ("race_pairs_met", len(ev_of.get("peoples_met", []))),
        ("dispositions_turned", len(ev_of.get("disposition_turned", []))),
        ("pairs_hatred", sum(1 for r in last.values() if r[4] == HATRED)),
        ("pairs_friendship", sum(1 for r in last.values() if r[4] == FRIENDSHIP)),
        ("refugees_arrived", len(ev_of.get("refugees_arrived", []))),
        ("refugees_people", sum(val(e, "people") for e in ev_of.get("refugees_arrived", []))),
        ("people_expelled", len(ev_of.get("people_expelled", []))),
        ("expelled_people", sum(val(e, "people") for e in ev_of.get("people_expelled", []))),
        ("settlements_mixed", mixed),
        ("settlements_mixed_ever", len({e[6] for e in ev_of.get("settlement_mixed", [])})),
        ("settlements_fled", sum(1 for s, k in endings.items() if k == 6)),
        ("abandoned_without_destination", len(ev_of.get("settlement_abandoned", []))),
        ("abandonment_lost", sum(val(e, "lost") for e in ev_of.get("settlement_abandoned_to", []))),
        ("monster_stories", told_monsters(res, ev_of, val)),
        ("ruins_oldest_year", min((s[12] for s in ruins), default="-")),
    ]


def told_monsters(res, ev_of, val):
    """10.1: the number of told monsters (notable: harm >= 200; at most 3)."""
    mon = {}
    parts = {}
    for p in res["event_participant"]:
        parts.setdefault(p[0], []).append(p)
    for key, w in (("monster_raid", None), ("settlement_overrun", 500), ("settlement_fled", 500)):
        for e in ev_of.get(key, []):
            for p in parts.get(e[0], []):
                if p[3] == MONSTER:
                    mon[p[4]] = mon.get(p[4], 0) + (max(0, val(e, "dead")) if w is None else w)
    return min(3, sum(1 for h in mon.values() if h >= 200))


HEADER = """\
# P5-03 golden values (docs/specs/p5-03-race-relations.md r1 sections 6, 7, 9 to 14, 16, 21, 23), generator 0.24.0
# (schema 10), written by worldgen/tests/reference/p5_03_relations_reference.py golden, an independent reference
# written from the spec (the relations subsystem runs inside p3_conflict_reference.py's replay, the part-B rules in
# p5_02_monsters_reference.py and p3_peoples_reference.py). Do not edit by hand; never regenerate to make a C++
# test pass.
#
# Format: ASCII, LF, tokens separated by one space.
#   case <case> <seed> <map size> <span> <races> [--flag value ...] [data]   a case: a world (the shipped data on
#       the world input <case>.db; no data token) or a fixture (map size 0; data relations-small,
#       monsters-small-fear or zero on the conflict stress geography <seed>)
#   <case> table race_relation <sha256>      the table hash of hash --detail (P0-04 8)
#   <case> table settlement_race <sha256>
#   <case> events_<key> <n>                  kinds 56 to 62
#   <case> stat <key> <value>                the [stats] keys of 17.4 (report --history --level full), in order
"""
FIXTURE_SEEDS = (0, 1, 2, 3, 5, 6, 7, 9, 12, 13)


def case_lines(name, res, E, mixed_min):
    import p3_chronology_reference as pchr
    o = ["%s table race_relation %s" % (name, pchr.table_hash(5, sorted(res.get("race_relation", [])))),
         "%s table settlement_race %s" % (name, pchr.table_hash(5, sorted(res.get("settlement_race", []))))]
    key = {k[0]: k[1] for k in res["event_kind"]}
    for k in P503_KINDS:
        o.append("%s events_%s %d" % (name, k, sum(1 for e in res["event"] if key[e[3]] == k)))
    o += ["%s stat %s %s" % (name, k, v) for k, v in stats_of(res, E, mixed_min)]
    return o


def fixture_sets():
    import p3_conflict_reference as pcr
    return (("relations-small", relations_small_data(pcr.data())),
            ("monsters-small-fear", monsters_small_fear_data(pcr.data())),
            ("zero", zero_data(pcr.data())))


def golden_text(inputs, fixtures_only=False):
    import p3_conflict_reference as pcr
    o = [HEADER.rstrip("\n")]
    for dname, dat in fixture_sets():
        for seed in FIXTURE_SEEDS:
            span, races, civs, intensity = pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
            name = "%s-%d" % (dname, seed)
            res = fixture_case(seed, dat)
            o.append("case %s %d 0 %d %s --civs-per-race %s --event-intensity %s %s" % (name, seed, span, races, civs,
                                                                                       intensity, dname))
            o += case_lines(name, res, span, dat.relations["mixed_min_permille"])
    if not fixtures_only:
        for name, seed, size, span, races, detail, civs, intensity in pcr.world_cases():
            res = pcr.world_case(name, seed, size, span, races, detail, inputs, pcr.data(), civs, intensity)
            o.append("case %s %d %d %d %s --history-detail %s --civs-per-race %s --event-intensity %s" % (
                name, seed, size, span, races, detail, civs, intensity))
            o += case_lines(name, res, span, pcr.data().relations["mixed_min_permille"])
    return "\n".join(o) + "\n"


# ---------------------------------------------------------------------------
# Units (24.1) on hand-checked values
# ---------------------------------------------------------------------------

class Checker:
    def __init__(self):
        self.count = 0
        self.failures = 0

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


SHIPPED_CAMP = {(0, 0): 500, (0, 1): 100, (0, 2): -600, (1, 1): 200, (1, 2): -200, (2, 2): 300}


def check_units(c):
    t = dict(pdr.RELATIONS_TUNING_024)
    # 2: B with stances, floor of a negative half sum
    c.eq("6.2.1 B elf-faerie (good-good 500? camp of the data) + floor((200 + 200) / 2)",
         base_disposition({(0, 0): 400}, 0, 0, 200, 200), 600)
    c.eq("6.2.1 B with a negative odd half sum floors", base_disposition({(0, 2): -250}, 0, 2, -100, -1), -301)
    c.eq("6.2.1 B with one-sided stance -101: floor(-50.5) = -51", base_disposition({(1, 1): 0}, 1, 1, -101, 0), -51)
    c.eq("6.2.1 B with +101: floor(50.5) = 50", base_disposition({(1, 1): 0}, 1, 1, 101, 0), 50)
    c.eq("6.2.1 B is symmetric in its arguments", base_disposition({(0, 2): -600}, 2, 0, -100, -100), -700)
    for M, want in ((0, 0), (999, 0), (1000, 1), (-999, 0), (-1000, -1), (-1999, -1), (2500, 2), (-2500, -2)):
        c.eq("6.2.1 D truncates M %d toward zero" % M, disposition(0, M), want)
    # decay: ticks and quiet steps alike; toward 0, never past it
    c.eq("6.2.2 a tick year decays 400 at 40/century", decay_step(10000, 1, 40), 9600)
    c.eq("6.2.2 a quiet step of 25 years decays 10000", decay_step(-20000, 25, 40), -10000)
    c.eq("6.2.2 decay stops at 0 (positive)", decay_step(300, 1, 40), 0)
    c.eq("6.2.2 decay stops at 0 (negative)", decay_step(-300, 1, 40), 0)
    c.eq("6.2.2 25 ticks equal one quiet step of 25", [decay_step(x, 1, 40) for x in [12345]] and
         __import__("functools").reduce(lambda m, _: decay_step(m, 1, 40), range(25), 12345), decay_step(12345, 25, 40))
    c.eq("6.2.2 clamp to memory_max x 1000", clamp_memory(300000, t), 250000)
    c.eq("6.2.2 clamp to memory_min x 1000", clamp_memory(-300000, t), -250000)
    # bands at a meeting and the hysteresis boundaries (h -500, f 150, m 50)
    for D, want in ((-500, 0), (-499, 1), (149, 1), (150, 2)):
        c.eq("6.2.3 plain band at %d" % D, plain_band(D, t), want)
    for band, D, want in ((1, -500, 0), (1, -499, 1), (1, 150, 2), (1, 149, 1), (0, -450, 0), (0, -449, 1),
                          (2, 100, 2), (2, 99, 1), (0, 200, 1), (2, -600, 1)):
        c.eq("6.2.3 band %d at D %d" % (band, D), band_step(band, D, t), want)
    # 4: mixing with remainders, ties by race id
    c.eq("6.3.2 P_b = 0 gives G exactly", mix_shares({1: 1000}, {2: 700, 3: 300}, 50, 0), {2: 700, 3: 300})
    c.eq("6.3.2 300 people of race 2 into 700 of race 1", mix_shares({1: 1000}, {2: 1000}, 300, 700), {1: 700, 2: 300})
    # S = {1: 1000}, G = {2: 1000}, n = 1, Pb = 2: c = 2000, 1000; T = 3: shares 666, 333, left 1 to race 1 (rem 2)
    c.eq("6.3.2 the leftover to the largest remainder", mix_shares({1: 1000}, {2: 1000}, 1, 2), {1: 667, 2: 333})
    # three races, equal remainders: c = 1000 each, T = 3: 333 each, left 1 to the smallest race id
    c.eq("6.3.2 equal remainders go by race id", mix_shares({1: 500, 2: 500}, {3: 1000}, 1, 2),
         {1: 333, 2: 333, 3: 334} if False else largest_remainder({1: 1000, 2: 1000, 3: 1000}, 1000, 3))
    c.eq("6.3.2 equal remainders: race 1 gets the one", largest_remainder({1: 1000, 2: 1000, 3: 1000}, 1000, 3),
         {1: 334, 2: 333, 3: 333})
    # conquest: x = 0, settlers 100: q 100, the old 900
    c.eq("6.3.3 conquest of a one-race town", conquest_shares({1: 1000}, 2, 100), {1: 900, 2: 100})
    c.eq("6.3.3 conquest where the conquerors are a minority of 200: 200 + 800 x 0.1 = 280",
         conquest_shares({1: 800, 2: 200}, 2, 100), {1: 720, 2: 280})
    c.eq("6.3.3 conquest with two others scaled by largest remainder",
         conquest_shares({1: 600, 3: 400}, 2, 100), {1: 540, 2: 100, 3: 360})
    c.eq("6.3.3 settlers 0 keep shares", conquest_shares({1: 1000}, 2, 0), {1: 1000})
    # drift and fade
    c.eq("6.3.4 drift 100/century over 200 years: 300 loses 60", drift_shares({1: 700, 2: 300}, 1, 100, 200, 20),
         {1: 760, 2: 240})
    c.eq("6.3.4 a minority below fade passes to q", drift_shares({1: 985, 2: 15}, 1, 100, 1, 20), {1: 1000})
    c.eq("6.3.4 drift never takes more than the share", drift_shares({1: 900, 2: 100}, 1, 1000, 12000, 0), {1: 1000})
    # 5: groups
    c.eq("6.4.3 split 250 over 700/300: 175, 75", split_groups(250, {1: 700, 2: 300}, 40), ([(1, 175), (2, 75)], []))
    c.eq("6.4.3 a group below migrate_min is lost", split_groups(100, {1: 700, 2: 300}, 40), ([(1, 70)], [(2, 30)]))
    c.eq("6.4.3 split leftovers by remainder then race", split_groups(10, {1: 333, 2: 333, 3: 334}, 1),
         ([(1, 3), (2, 3), (3, 4)], []))
    c.eq("6.4.3 split 11 over thirds: 3.663, 3.663, 3.674 -> 4, 3, 4",
         split_groups(11, {1: 333, 2: 333, 3: 334}, 1), ([(1, 4), (2, 3), (3, 4)], []))
    # part B formulas (9.2, 9.3, 14.1, 14.2, 11.1)
    c.eq("9.2 below kill_max the raw toll", toll_dead(1000, 60, 200, 250), 60)
    c.eq("9.2 above kill_max a quarter of the rest: 3000 x 0.12 = 360 -> 200 + 40", toll_dead(3000, 120, 200, 250), 240)
    c.eq("9.2 at most P - 1", toll_dead(1, 1000, 200, 250), 0)
    mt = dict(pdr.MONSTERS_TUNING_024)
    c.eq("9.3 struck once: 20", hunt_permille(1, mt), 20)
    c.eq("9.3 struck four times: 20 + 3 x 60 = 200", hunt_permille(4, mt), 200)
    c.eq("9.3 the cap 400", hunt_permille(20, mt), 400)
    for P, want in ((800, 900), (400, 900), (1200, 900 * 666 // 1000), (3000, 900 * 266 // 1000)):
        c.eq("14.1 giant at normal, P %d" % P, overrun_chance(900, 1000, P, 800), want)
    c.eq("14.2 giant 900 at high is 1000", overrun_chance(900, 1600, 100, 800), 1000)
    c.eq("14.2 dragon 600 at high is 960", overrun_chance(600, 1600, 100, 800), 960)
    c.eq("14.2 at low halves", overrun_chance(900, 500, 100, 800), 450)
    for M, want in ((1000, 4000), (1600, 2500), (500, 8000), (0, 4000000)):
        c.eq("14.2 undead threshold at M %d" % M, undead_threshold(4000, M), want)
    c.eq("11.1 road of 100 with u = 0: 5%", road_loss(100, 0, 50, 200), 5)
    c.eq("11.1 road of 100 with the largest u: 20%", road_loss(100, (1 << 64) - 1, 50, 200), 20)
    c.eq("11.1 road of 1 is 0", road_loss(1, (1 << 64) - 1, 1000, 1000), 0)
    c.eq("11.1 road at most P - 1", road_loss(10, 0, 1000, 1000), 9)


def stub(races=("elf", "goblin"), regions=3, relations=None):
    """A hand-made conflict World (p3_conflict_reference.unit_world) with the relations subsystem of the data's
    tuning (edits in `relations`)."""
    import p3_conflict_reference as pcr
    w = pcr.unit_world(regions=regions, races=races)
    d = w.dat if hasattr(w, "dat") else pcr.data()
    t = dict(pdr.RELATIONS_TUNING_024, **(relations or {}))
    camp = {(ka, kb): pcr.data().conflict["camp"][f] for (ka, kb), f in pcr.CAMP_FIELD.items()}
    rel = Relations(w, t, camp, {})
    return w, rel


def check_stub_units(c):
    import p3_conflict_reference as pcr
    # 6.3.1 and 6.3.5: an entry's standing follows its pair's band at a phase start; the cause is the turn
    w, rel = stub()
    ids = {r[1]: r[0] for r in w.inp.races}
    elf, gob = ids["elf"], ids["goblin"]
    c1 = pcr.add_civ(w, elf, 1)
    c2 = pcr.add_civ(w, gob, 2)
    s1 = pcr.add_stl(w, c1, 1, 3, 1000)
    s2 = pcr.add_stl(w, c2, 2, 3, 1000)
    w.civ(c1)["capital"], w.civ(c2)["capital"] = s1, s2
    rel.new_settlements()
    c.eq("6.3.1 a new settlement: its founder's race at 1000, ruling", {g: (e["share"], e["standing"])
                                                                         for g, e in rel.sh[s1]["e"].items()},
         {elf: (1000, RULING)})
    p = rel.pair(elf, gob)
    p.update(met=True, met_event=1, band=WARINESS, M=0)
    rel.mix(s1, {gob: 1000}, 300, 700, 2)
    c.eq("6.3.2 mixing 300 goblins into 700 elves; the newcomers tolerated (wariness)",
         {g: (e["share"], e["standing"]) for g, e in rel.sh[s1]["e"].items()}, {elf: (700, RULING), gob: (300, TOLERATED)})
    p.update(band=FRIENDSHIP, turn_event=len(w.events), turn_year=w.year)
    n0 = len(w.events)
    rel.standings(False)
    ev = w.events[n0:]
    c.eq("6.3.5 one standing_changed after the turn into friendship", [e["key"] for e in ev], ["standing_changed"])
    c.eq("6.3.5 its values (standing 2, share 300) and cause (the turn)", (ev[0]["values"], ev[0]["causes"]),
         ({1: ACCEPTED, 2: 300}, [p["turn_event"]]))
    # 6.5: an expulsion of a tolerated minority in hatred
    p.update(band=HATRED)
    rel.t["expel_permille"] = 1000
    w.serial = 99
    n0 = len(w.events)
    rel.expulsions()
    ev = w.events[n0:]
    c.eq("6.5 accepted (standing 2) is not expelled", [e["key"] for e in ev], [])
    rel.sh[s1]["e"][gob]["standing"] = TOLERATED
    rel.expulsions()
    ev = w.events[n0:]
    c.eq("6.5 one people_expelled of 300 (P x share div 1000)", [(e["key"], e["values"]) for e in ev],
         [("people_expelled", {1: 300})])
    c.eq("6.5 the population falls by 300 and the entry goes", (w.stl(s1)["population"], sorted(rel.sh[s1]["e"])),
         (700, [elf]))
    c.eq("6.5 a displaced record of the race", [(r["kind"], r["race"], r["people"]) for r in rel.pending],
         [("displaced", gob, 300)])
    c.eq("6.5 note_reduction", w.last_red.get(s1), ev[0]["id"])
    # the majority guard
    rel.pending = []
    rel.mix(s1, {gob: 1000}, 800, 700, 3)
    rel.sh[s1]["e"][gob]["standing"] = SUBJECT
    n0 = len(w.events)
    rel.expulsions()
    c.eq("6.5 the majority guard: a share above expel_share_max stays", [e["key"] for e in w.events[n0:]], [])
    # 6.3.3: an owner change to another race; to the same race
    w2, rel2 = stub()
    a1 = pcr.add_civ(w2, elf, 1)
    a2 = pcr.add_civ(w2, gob, 2)
    a3 = pcr.add_civ(w2, elf, 3)
    t1 = pcr.add_stl(w2, a1, 1, 3, 1000)
    rel2.new_settlements()
    w2.stl(t1)["owner_civ"], w2.stl(t1)["owner_event"] = a2, 2
    w2.year = 500
    rel2.owner_changes()
    e1 = rel2.sh[t1]["e"]
    c.eq("6.3.3 the goblins take an elf town: elves 900 subject (conquered), goblins 100 ruling",
         {g: (e["share"], e["standing"], e["conquered"], e["since_event"]) for g, e in e1.items()},
         {elf: (900, SUBJECT, True, 2), gob: (100, RULING, False, 2)})
    w2.stl(t1)["owner_civ"], w2.stl(t1)["owner_event"] = a3, 3
    rel2.owner_changes()
    c.eq("6.3.3 an elf realm retakes it: the elves rule again, the goblins subject",
         {g: (e["share"], e["standing"]) for g, e in rel2.sh[t1]["e"].items()},
         {elf: (900 + 100 * 100 // 1000, RULING), gob: (90, SUBJECT)})
    a4 = pcr.add_civ(w2, elf, 3)
    w2.stl(t1)["owner_civ"], w2.stl(t1)["owner_event"] = a4, 4
    before = {g: dict(e) for g, e in rel2.sh[t1]["e"].items()}
    rel2.owner_changes()
    c.eq("6.3.3 the same race taking it: the owner changes, nothing else", (rel2.sh[t1]["o"], rel2.sh[t1]["e"]),
         (a4, before))
    # 6.2.4 meetings: the key and the region of the younger
    w3, rel3 = stub(regions=4)
    b1 = pcr.add_civ(w3, elf, 1, founded=0)
    b2 = pcr.add_civ(w3, gob, 4, founded=10)
    pcr.add_stl(w3, b1, 1, 3, 500)
    pcr.add_stl(w3, b2, 4, 3, 500)
    rel3.awakened = {elf, gob}
    rel3.aw_cursor = len(w3.events) + 1
    w3.serial = 99
    for mh, want in ((2, []), (3, ["peoples_met"])):
        rel3.t["meet_hops"] = mh
        n0 = len(w3.events)
        rel3.meetings(False)
        c.eq("6.2.4 regions 1 and 4 are 3 hops apart; meet_hops %d" % mh, [e["key"] for e in w3.events[n0:]], want)
    ev = w3.events[-1]
    c.eq("6.2.4 the meeting: region of the younger, B, the plain band, the younger's founding",
         (ev["region"], ev["values"], ev["causes"]),
         (4, {1: rel3.pair(elf, gob)["B"], 2: plain_band(rel3.pair(elf, gob)["B"], rel3.t)},
          [w3.civ(b2)["founding_event"]]))
    rel3b = stub(regions=4, relations=dict(meet_hops=-1))[1]
    c.eq("6.2.4 meet_hops -1: never", rel3b.t["meet_hops"], -1)


# ---------------------------------------------------------------------------
# 16.4 on corrupt rows (24.1 item 10)
# ---------------------------------------------------------------------------

def mini_rows():
    """A hand-made history: races 1, 2, 3; pair (1, 2) met at 100 (e1) and turned at 300 (e2); settlement 1
    founded at 50 by civilization 1 (race 1), mixed with race 2 from 120."""
    return dict(
        race=[(1, "elf"), (2, "goblin"), (3, "human")],
        event_kind=[(57, "peoples_met"), (58, "disposition_turned")],
        event=[(1, 100, 100, 57, 1, 5, 0, 25), (2, 300, 300, 58, 1, 0, 0, 55)],
        event_participant=[(1, 1, 1, RACE, 1), (1, 2, 2, RACE, 2), (2, 1, 1, RACE, 1), (2, 2, 2, RACE, 2)],
        race_relation=[(1, 2, 100, -100, 1), (1, 2, 300, -510, 0)],
        settlement=[(1, "Aa", 0, 0, 5, 1, 50, 0, 1, 1, 1, 10, None, 0, 0)],
        settlement_race=[(1, 1, 50, 1000, 3), (1, 1, 120, 800, 3), (1, 2, 120, 200, 1)])


def check_reader(c):
    E = 1000
    c.eq("16.4 a sound history has no problem", reader_problems(mini_rows(), E), [])
    rr = "corrupt table 'race_relation': row "
    sr = "corrupt table 'settlement_race': row "
    cases = (
        ("race_relation", [(1, 2, 100, -100, 1), (1, 2, 300, -510, 0), (1, 2, 200, -100, 1)],
         rr + "(1, 2, 200): not above the previous row"),
        ("race_relation", [(1, 9, 100, -100, 1)], rr + "(1, 9, 100): unknown race 9"),
        ("race_relation", [(2, 1, 100, -100, 1)], rr + "(2, 1, 100): race_a 2 is not below race_b 1"),
        ("race_relation", [(1, 2, 1000, -100, 1)], rr + "(1, 2, 1000): year 1000 outside 0 to 999"),
        ("race_relation", [(1, 2, 100, -4001, 1)], rr + "(1, 2, 100): disposition -4001 outside -4000 to 4000"),
        ("race_relation", [(1, 2, 100, -100, 3)], rr + "(1, 2, 100): band 3 outside 0 to 2"),
        ("race_relation", [(1, 2, 100, -100, 1), (1, 2, 300, -100, 1)],
         rr + "(1, 2, 300): equals the previous row of this pair"),
        ("race_relation", [(1, 2, 90, -100, 1)], rr + "(1, 2, 90): first row is not at its meeting"),
        ("settlement_race", [(1, 1, 120, 800, 3), (1, 1, 50, 1000, 3)], sr + "(1, 1, 50): not above the previous row"),
        ("settlement_race", [(2, 1, 50, 1000, 3)], sr + "(2, 1, 50): unknown settlement 2"),
        ("settlement_race", [(1, 7, 50, 1000, 3)], sr + "(1, 7, 50): unknown race 7"),
        ("settlement_race", [(1, 1, 49, 1000, 3)], sr + "(1, 1, 49): year 49 outside 50 to 999"),
        ("settlement_race", [(1, 1, 50, 1001, 3)], sr + "(1, 1, 50): share 1001 outside 0 to 1000"),
        ("settlement_race", [(1, 1, 50, 1000, 4)], sr + "(1, 1, 50): standing 4 outside 0 to 3"),
        ("settlement_race", [(1, 1, 50, 1000, 3), (1, 2, 50, 0, 1)], sr + "(1, 2, 50): standing 1 with share 0"),
        ("settlement_race", [(1, 1, 50, 1000, 3), (1, 1, 60, 1000, 3)],
         sr + "(1, 1, 60): equals the previous row of this settlement and race"),
        ("settlement_race", [(1, 1, 50, 1000, 3), (1, 2, 120, 200, 1)],
         sr + "(1, 2, 120): shares in force at 120 sum to 1200"),
        ("settlement_race", [(1, 1, 50, 1000, 3), (1, 1, 120, 800, 3), (1, 2, 120, 200, 3)],
         sr + "(1, 1, 120): 2 ruling races at 120"),
    )
    for table, rows, want in cases:
        r = mini_rows()
        r[table] = rows
        got = [m for m in reader_problems(r, E) if m.startswith("corrupt table '%s'" % table)]
        c.eq("16.4 %s" % want, got[:1], [want])
    r = mini_rows()
    r["race_relation"] = []
    c.eq("16.4 peoples_met without its rows", [m for m in reader_problems(r, E) if "'event'" in m],
         ["corrupt table 'event': row 1: peoples_met without its rows"])
    r = mini_rows()
    r["event"] = r["event"] + [(3, 400, 400, 57, 1, 5, 0, 25)]
    r["event_participant"] = r["event_participant"] + [(3, 1, 1, RACE, 1), (3, 2, 2, RACE, 2)]
    c.eq("16.4 met twice", [m for m in reader_problems(r, E) if "'event'" in m],
         ["corrupt table 'event': row 3: 1-2 met twice"])
    r = mini_rows()
    r["event_participant"] = r["event_participant"][:3] + [(2, 2, 2, RACE, 8)]
    c.eq("16.4 an unknown race participant", [m for m in reader_problems(r, E) if "'event'" in m],
         ["corrupt table 'event': row 2: race 8 has no row"])


# ---------------------------------------------------------------------------
# Running the fixtures and worlds
# ---------------------------------------------------------------------------

def data_base_of(dat):
    """B(a, b) from the data (for invariant 6 of unmet pairs)."""
    import p3_conflict_reference as pcr
    camp = {(ka, kb): dat.conflict["camp"][f] for (ka, kb), f in pcr.CAMP_FIELD.items()}
    return camp


def res_violations(res, dat):
    w = res["_world"]
    inp = res["_inputs"]
    rel = w.race_relations
    borders = [(b["a"], b["b"]) for b in inp.borders if b["walk_pairs"] > 0 or b["pass_id"] > 0]
    res = dict(res, race=[(r[0], r[1]) for r in inp.races])
    base = (lambda a, b: rel.disposition(a, b) if a == b else rel.pair(a, b)["B"]) if rel else (lambda a, b: 0)
    return invariant_violations(res, dat.relations, inp.params["span"], borders, base,
                                {r[0]: r[1] for r in inp.races}, list(dat.base.monsters),
                                dat.peoples.get("ghost_fade_years", 0), rel.log if rel else None)


def check_fixtures(c, seeds=FIXTURE_SEEDS, verbose=False):
    seen = set()
    for dname, dat in fixture_sets():
        for seed in seeds:
            res = fixture_case(seed, dat)
            E = res["_inputs"].params["span"]
            for v in res_violations(res, dat):
                c.ok("%s-%d invariant %s" % (dname, seed, v), False)
            rr = dict(res, race=[(r[0], r[1]) for r in res["_inputs"].races])
            for m in reader_problems(rr, E):
                c.ok("%s-%d reader %s" % (dname, seed, m), False)
            key = {k[0]: k[1] for k in res["event_kind"]}
            kinds = {key[e[3]] for e in res["event"]}
            if dname == "relations-small":
                seen |= kinds & set(P503_KINDS)
            if dname == "monsters-small-fear":
                seen |= kinds & {"settlement_fled"}
            if dname == "zero":
                c.eq("zero-%d: no event of kinds 56 to 62 but flights" % seed,
                     sorted(kinds & set(P503_KINDS[1:])), [])
            if verbose:
                print(dname, seed, sorted(k for k in kinds if k in P503_KINDS), len(res["race_relation"]),
                      len(res["settlement_race"]))
    # X3: standing_changed does not happen on the ten stress maps (hated minorities are expelled first, conquests are
    # between hated peoples): it is covered by check_stub_units and H14
    c.eq("24.3: every kind of part A but standing_changed happens on relations-small, flights on monsters-small-fear",
         sorted(seen - {"standing_changed"}), sorted(set(P503_KINDS) - {"standing_changed"}))


def check_i0(c, inputs=None, quick=True):
    import p3_conflict_reference as pcr
    for seed in (0, 1, 2, 5):
        diff, zero, off = i0_differences(lambda d: fixture_case(seed, d), pcr.stress_data())
        c.eq("7.4 I0 stress-%d: the history tables equal" % seed, diff, [])
    if not quick and inputs:
        for name, seed, size, span, races, detail, civs, intensity in pcr.world_cases():
            if size != 512:
                continue
            diff, _z, _o = i0_differences(lambda d: pcr.world_case(name, seed, size, span, races, detail, inputs, d,
                                                                   civs, intensity))
            c.eq("7.4 I0 %s: the history tables equal" % name, diff, [])


def cmd_check_world(paths, data_root="game/data"):
    import p3_conflict_reference as pcr
    import p3_chronology_reference as pchr
    dat = pcr.data(data_root)
    bad = 0
    camp = data_base_of(dat)
    for path in paths:
        conn = pchr.open_world(path)
        res = {}
        for t in pe.TABLES + ("race",):
            try:
                res[t] = pchr.world_table_hash(conn, t)[1]
            except sqlite3.Error:
                res[t] = []
        meta = dict(conn.execute("SELECT key, value FROM meta").fetchall())
        conn.row_factory = sqlite3.Row
        borders = open_borders([dict(r) for r in conn.execute("SELECT * FROM region_border")])
        races = {r[0]: r for r in conn.execute("SELECT * FROM race").fetchall()}
        conn.close()
        c = Checker()
        c.eq("%s schema_version" % path, meta.get("schema_version"), "10")
        E = int(meta.get("history_end_year", "6000"))
        key_id = {dat.race[r]["key"]: r for r in dat.race}
        st = {key_id[k]: {key_id[x]: v for x, v in dat.race[key_id[k]].get("stances", [])} for k in key_id}

        def base(a, b):
            ka, kb = races[a][5], races[b][5]
            ra, rb = races[a][1], races[b][1]
            return base_disposition(camp, ka, kb, st[key_id[ra]].get(key_id[rb], 0), st[key_id[rb]].get(key_id[ra], 0))
        res["race"] = [(r, races[r][1]) for r in races]
        for v in invariant_violations(res, dat.relations, E, borders, base, {r: races[r][1] for r in races},
                                      list(dat.base.monsters), dat.peoples.get("ghost_fade_years", 0)):
            c.ok("%s invariant %s" % (path, v), False)
        for m in reader_problems(res, E):
            c.ok("%s reader %s" % (path, m), False)
        print("%s: %d race_relation rows, %d settlement_race rows, %d checks, %d failures" % (
            os.path.basename(path), len(res.get("race_relation", [])), len(res.get("settlement_race", [])),
            c.count, c.failures))
        bad += c.failures
    return 1 if bad else 0


def cmd_check(argv):
    import p3_conflict_reference as pcr
    c = Checker()
    check_units(c)
    check_stub_units(c)
    check_reader(c)
    quick = "--quick" in argv
    check_fixtures(c, FIXTURE_SEEDS[:4] if quick else FIXTURE_SEEDS, verbose="-v" in argv)
    check_i0(c, None if quick else (argv[argv.index("--inputs") + 1] if "--inputs" in argv else DEFAULT_INPUTS),
             quick)
    if os.path.exists(GOLDEN) and not quick:
        with open(GOLDEN, "rb") as f:
            have = f.read().decode()
        fresh = golden_text(None, fixtures_only=True)
        got = [l for l in have.splitlines() if l in fresh.splitlines() or l.startswith("#")]
        c.eq("committed golden file's fixture lines are current", [l for l in have.splitlines()
                                                                   if not l.startswith("#")
                                                                   and l.split(" ")[0] != "case"
                                                                   and l.split(" ")[0].startswith(
                                                                       ("relations-small", "monsters-small", "zero"))
                                                                   and l not in got], [])
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    import p3_conflict_reference as pcr
    pcr.use_version("0.24.0")
    inputs = argv[argv.index("--inputs") + 1] if "--inputs" in argv else DEFAULT_INPUTS
    if argv[0] == "check":
        return cmd_check(argv[1:])
    if argv[0] == "units":
        c = Checker()
        check_units(c)
        check_stub_units(c)
        check_reader(c)
        print("%d checks, %d failures" % (c.count, c.failures))
        return 1 if c.failures else 0
    if argv[0] == "golden":
        rest = [a for a in argv[1:] if not a.startswith("--") and a != inputs]
        text = golden_text(inputs, fixtures_only="--fixtures-only" in argv)
        path = rest[0] if rest else GOLDEN
        if path == "-":
            sys.stdout.write(text)
        else:
            with open(path, "w", newline="\n") as f:
                f.write(text)
        return 0
    if argv[0] == "check-world":
        return cmd_check_world([a for a in argv[1:] if not a.startswith("--")])
    if argv[0] == "i0":
        c = Checker()
        check_i0(c, inputs, quick=False)
        print("%d checks, %d failures" % (c.count, c.failures))
        return 1 if c.failures else 0
    if argv[0] == "readings":
        print(READINGS)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
