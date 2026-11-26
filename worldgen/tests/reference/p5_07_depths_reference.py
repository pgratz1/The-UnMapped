#!/usr/bin/env python3
"""Reference of P5-07 "Mountain depths" (docs/specs/p5-07-mountain-depths.md r2, generator 0.29.0, schema 13, history
report format v10), written by the test writer from the spec alone, without the engineer's branch. r2 adds the goblin
deep warrens (6.10) and the allies' aid at a waking (6.11) to the r1 rules (module flag R2, on by default; off: r1).

The deep rules of 6.2 to 6.7 run inside p3_conflict_reference.py's replay: at 0.29.0 (flag `deep` of the conflict
reference) the conflict World installs `Deep` on its monsters subsystem (p5_02_monsters_reference.Monsters), which
calls it in T4 (after the monsters' year: deepening, then one waking at most), in Q4 (deepening) and after every raid
(a deep horror's take in place of the overrun). The seams of 7 are in the other references: AllianceView::forge_depth
and the force factor of 6.3 in p3_conflict_reference.World (forge_depth, strengthened), credit_units of 7.5 in
p5_03_relations_reference and p5_04_alliances_reference, the deeds of 6.9 in p5_06_figures_reference, the catalogue
(8.1), schema 13 and the ending of a hold lost (6.6) in p3_engine_reference, the data of 4 and 8.2 in
p3_data_reference (DEEP_KINDS_029 and the other listings; a 0.28.0 root is read at 0.29.0 with them filled in), the
report v10 of 10 in p3_report_reference. This module holds the rules, the formulas with hand-checked units, the
invariants of 13 on rows (check-world), the reader checks of 9 on rows, the statistics of 10 item 7, the renderer
checks of 16 item 9, the fixture depths-small (4.2) and the golden file of 16 (p5-07-depths.txt).

Commands (from the repository root):
  python3 -B worldgen/tests/reference/p5_07_depths_reference.py units            formulas, templates, data listings
  python3 -B worldgen/tests/reference/p5_07_depths_reference.py fixtures [--quick] the replay of depths-small on the
                                                                                 conflict stress geographies: every
                                                                                 path of 6, invariants of 13, the
                                                                                 reader checks, I0 (14)
  python3 -B worldgen/tests/reference/p5_07_depths_reference.py check-world DB [DB ...] [--data ROOT]
  python3 -B worldgen/tests/reference/p5_07_depths_reference.py golden [PATH|-] --inputs DIR
  python3 -B worldgen/tests/reference/p5_07_depths_reference.py readings

`golden --inputs DIR` reads the geography of depths-small worlds DIR/depths-<seed>-512.db (seeds 0 to 9: any 0.29.0
world file of that seed and size, `--until regions` suffices) and DIR/geo-seed-42-1024.db (the shipped seed 42 at
1024), replays their histories at 0.29.0 and writes worldgen/tests/golden/p5-07-depths.txt. Recorded at integration,
never to make a C++ test pass.
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

GOLDEN = os.path.join(pe.REPO, "worldgen", "tests", "golden", "p5-07-depths.txt")
VERSION = "0.29.0"
# 3: appended to core/streams.hpp after hist.alliances.renew (the 98th), in this order (103 streams)
STREAMS = ("hist.deep.delve", "hist.deep.wake", "hist.deep.kind", "hist.deep.gate", "hist.deep.take",
           # r2: appended after hist.deep.take (107 streams)
           "hist.deep.aid", "hist.deep.warren_delve", "hist.deep.warren_wake", "hist.deep.warren_kind")
STREAM_COUNT = 107
KINDS = {93: "delving_deepened", 94: "deep_woken", 95: "deep_sealed", 96: "hold_lost",
         97: "ally_aided_hold", 98: "warren_delved", 99: "warren_sealed"}   # r2: 97 to 99
R2 = True   # r2's rules (6.10, 6.11); False: the r1 rules (I1 of 14)
DEEP_KIND_IDS = (7, 8, 9)   # 4.1: ymrach 7, ysgarth 8, zhakul 9 (invariant 8)
WAKE_DEEP = 3
CIV, SETTLEMENT, MONSTER, SITE = pe.CIV, pe.SETTLEMENT, pe.MONSTER, pe.SITE
AWAKE, ASLEEP, SLAIN, DIED = 1, 2, 3, 4
LAIR, MINE = 1, 3
OPEN, END_LOST = 0, 4
NEVER = 1 << 40   # a sealed monster's drive_until: it never wakes again (6.5)
HEADER = "# worldgen history v10"
STATS_KEYS = ("mines_deepened", "mines_deepest", "deep_woken", "deep_sealed", "holds_lost", "deep_flights",
              "realms_lost_to_deep", "deep_alive",   # 10 item 7, in this order
              "warrens_delved", "warrens_deepest", "warrens_woken", "warrens_sealed", "deep_aided",
              "wakings_aided")                       # r2
MINE_DEPTH_WORD = {1: "below daylight", 2: "to the mountain's roots", 3: "to the old deep"}   # 11.1

READINGS = """\
D-R1 (16 item 11) The fixture of report v10 is named H19 (worldgen/tests/data/history-report/h19-v10.toml), since
     P5-04a's fixture already holds the name H18 (h18-v9.toml); "from H17" is read as "in H17's shape".
D-R2 (6.5) The samples of 6.5 (a village of 800: 219 and 138) round; the formula floors (218 and 137). The formula
     is used; the town of 3,000 gives 512 and 375 either way.
D-R3 (8.2) The sample sentences print 1206 and 1810 without P3-04 20.3's separator; the renderer's rule (1,206 and
     1,810) is used.
D-R4 (7.3) "20.1 items 2 and 3" include P5-01 24.4's refuge_hidden (kind 42), written right after the deep flight's
     refugees_settled as after conflict's own; conflict's flights counter (a statistic of conflict's wars) does not
     count a deep flight.
D-R5 (6.7) The hops of a later take (deep_reach_hops) are GeoView's (every region_border row), as the raid range of
     P5-02 6.4.1 (reading G1 of p5_02_monsters_reference).
D-R6 (6.4, 6.5) A sealed monster still counts as living for its kind's max_alive (it is asleep, status 2); it never
     wakes because its rewake draw is never made (P5-02 6.4 item 2 skips it).
D-R7 (6.6 item 2) "t is its race's last refuge" is P3-05 r12 22.7's predicate on State at the take: c's only
     standing settlement is t and no other civilization of c's race is alive.
D-R8 (6.2) "Q4 only: quiet_events_left(c) > 0, else next s" is tested after the worked years; a mine with the years
     but no budget waits (no event; the draw, keyed, has no effect).
D-R9 (4.2) depths-small applies monsters-small's edits to every monster kind, the deep kinds included (wake_rate x
     2000 is a common factor of the weights; overrun_permille 1000 makes every allowed later take certain before
     f_p).
D-R10 (9) The reader message `hold_lost <e> did not ruin its settlement` is on the event's row (table 'event', row
     e); `<kind> without its waking` names the kind key.
D-R11 (6.6, 6.7) No note_reduction for a take: the settlement is ruined in the same step.
D-R12 (13 item 6) Checked from rows as: every refugees_settled caused by a hold_lost is of its year, its founder the
     hold_lost's civilization and its origin the hold_lost's settlement; and at E every present race that awoke has an
     alive civilization.
D-R18 (r2 4.5) delving_permille is appended last in the race record (after magic_permille); an absent field encodes in
     the content hash as 0 (the spec gives neither the position nor the encoding of an optional field).
D-R19 (r2 6.10) "its region qualifies" reads the region's P1-06 row: upland is 4 x walk_cells < cells (P3-05's
     upland), else ore_reach_cells >= deep_warren_ore_min; deep_warren_ore_min 0 lets every region qualify.
D-R20 (r2 6.10) The warren waking loop is over settlement ids 1 to the count at its start; "owner c = warren_delver(t)"
     is owner(t) = warren_delver(t) != 0. A warren lost to another realm and won back keeps its depth and, if c started
     another meanwhile, c has two live warrens: the spec's own rules allow it (invariant 12's "at most one live warren"
     is checked from rows with a warren's life ended at the first capture or cession of its settlement).
D-R21 (r2 6.11) The aid's hop distances are GeoView's (every region_border row, as D-R5), from the hold's region; the
     ally's settlement q is chosen among its standing visible settlements; an ally whose race equals the realm's gets no
     credit; the credit goes through RelationsSubsystem::credit (no credit before the races have met, as credit does).
D-R22 (r2 6.11, 6.12 item 1) A forced seal is decided with the aid in S: the aid draws, then the gate draw, then the
     plan; an aid event is written even when the seal is forced.
D-R23 (r2 6.10) A warren's lair region is the settlement's region; the warren's deep_woken has no site and its forced
     seal's plan uses the settlement's region as the threat region.
D-R24 (r2 6.6) hold_lost's telling + 3 holds for every take by a horror from a warren (the first and later ones).
"""


def _pcr():
    import p3_conflict_reference as pcr
    return pcr


def _p502():
    import p5_02_monsters_reference as p502
    return p502


def deep_on():
    return _pcr().FLAGS.get("deep", False)


# ---------------------------------------------------------------------------
# Formulas (6.2 to 6.7, 6.3), each as the spec writes it
# ---------------------------------------------------------------------------

def delve_permille(deep_delve_permille, mining_permille, quiet_length=None):
    """6.2: p = deep_delve_permille x mining_permille div 1000 (T4); Q4: min(1000, p x L)."""
    p = deep_delve_permille * mining_permille // 1000
    if quiet_length is not None:
        p = min(1000, p * quiet_length)
    return p


def can_delve(Y, origin_year, d, deep_years):
    """6.2: a mine at depth d (0 to 2) may reach d + 1 once w = Y - origin_year >= deep_years[d]."""
    return d < 3 and Y - origin_year >= deep_years[d]


def wake_ppm(deep_wake_ppm, d, M):
    """6.4 item 2: p = min(10^6, deep_wake_ppm[d - 1] x M div 1000)."""
    return min(1000000, deep_wake_ppm[d - 1] * M // 1000)


def gate_strength(P, levy_permille, prowess_permille):
    """6.5: S = P_t x deep_gate_levy_permille div 1000 x prowess_permille div 1000."""
    return P * levy_permille // 1000 * prowess_permille // 1000


def gate_odds(S, might, seal_max_permille):
    """6.5: odds = min(deep_seal_max_permille, S x 1000 div (S + might)); S = 0: 0."""
    if S == 0:
        return 0
    return min(seal_max_permille, S * 1000 // (S + might))


def sealed_at(u, odds):
    """6.5: u < odds seals; else the hold falls."""
    return u < odds


def take_toll(P, toll_permille):
    """6.6 item 1: (dead, survivors)."""
    dead = P * toll_permille // 1000
    return dead, P - dead


def take_permille(overrun_permille, M, people_ref, P):
    """6.7: base x f_p div 1000 with base = min(1000, overrun_permille x M div 1000) and f_p = min(1000, ref x 1000 div
    max(1, P_t)) (P_t after the raid)."""
    base = min(1000, overrun_permille * M // 1000)
    fp = min(1000, people_ref * 1000 // max(1, P))
    return base * fp // 1000


def forge_factor(f, forge_strength_permille, forge_depth_permille, D, forged):
    """6.3 item 1: f x (1000 + forge_strength + forge_depth x D(c)) div 1000 when forged(c), else f."""
    if not forged:
        return f
    return f * (1000 + forge_strength_permille + forge_depth_permille * D) // 1000


def telling_delve(site, depth):
    return (site + depth) % 3


def telling_woken(monster):
    return monster % 3


def telling_sealed(monster):
    return (monster + 1) % 3


def telling_lost(monster, settlement):
    return (monster + settlement) % 3


def warren_region_ok(cells, walk_cells, ore_reach_cells, ore_min):
    """r2 6.10 (4.2): upland (4 x walk_cells < cells) or ore_reach_cells >= deep_warren_ore_min; 0: any region."""
    return ore_min == 0 or 4 * walk_cells < cells or ore_reach_cells >= ore_min


def warren_wake_ppm(deep_wake_ppm, d, M, warren_wake_permille):
    """r2 6.10 item 2: p = min(10^6, deep_wake_ppm[d - 1] x M div 1000 x deep_warren_wake_permille div 1000)."""
    return min(1000000, deep_wake_ppm[d - 1] * M // 1000 * warren_wake_permille // 1000)


def aid_permille(deep_aid_permille, ally_join_permille):
    """r2 6.11: p = deep_aid_permille x ally_join_permille(r_a) div 1000."""
    return deep_aid_permille * ally_join_permille // 1000


def aid_strength(P_q, levy_permille, prowess_permille):
    """r2 6.11: S_a = P_q x deep_aid_levy_permille div 1000 x prowess_permille(r_a) div 1000."""
    return P_q * levy_permille // 1000 * prowess_permille // 1000


def telling_warren_delved(settlement, depth):
    return (settlement + depth) % 3


def telling_aid(monster, ally):
    return (monster + ally) % 3


def may_take_later(takes, takes_max, hops, reach_hops, owner, waking_realm, realm_alive, sealed):
    """6.7's conditions before the draw."""
    return (not sealed and owner == waking_realm and realm_alive and hops is not None and hops <= reach_hops
            and takes <= takes_max)


# ---------------------------------------------------------------------------
# The rules (6.2 to 6.7) on a p5_02_monsters_reference.Monsters inside a p3_conflict_reference World
# ---------------------------------------------------------------------------

class Deep:
    """MonstersSubsystem's deep members (5) and rules (6), keyed by ids."""

    def __init__(self, m):
        self.m = m
        self.w = m.w
        self.t = m.t
        for s in STREAMS:
            self.w.keys[s] = cr.stream_key(m.inp.seed, s)
        self.depth = {}           # site -> 0 to 3
        self.woken = set()        # sites that have woken a horror
        self.sealed_site = set()
        self.last_delve = {}      # site -> its last delving_deepened
        self.waking_realm = {}    # monster -> the founder of the mine it woke from
        self.takes = {}           # monster -> holds taken, first included
        self.sealed = set()       # monsters sealed at their waking
        self.woken_event = {}     # monster -> its deep_woken
        # r2 (5): warrens by settlement id, from_warren by monster
        self.warren_depth = {}
        self.warren_delver = {}
        self.warren_woken = set()
        self.warren_sealed = set()
        self.last_warren = {}     # settlement -> its last warren_delved
        self.from_warren = set()
        self.log = []             # (year, what, ids...) for checks

    # --- reads (7.1, 7.2)
    def mine_depth(self, sid):
        """MonstersSubsystem::mine_depth(site): 0 for an unknown site or a site of another kind."""
        if not 1 <= sid <= len(self.m.sites) or self.m.site(sid)["kind"] != MINE:
            return 0
        return self.depth.get(sid, 0)

    def deep_kinds(self):
        return [k for k in range(1, len(self.m.kinds) + 1) if self.m.kind(k)["wake"] == "deep"]

    def living_of(self, k):
        return sum(1 for x in self.m.monsters if x["kind"] == k and x["status"] in (AWAKE, ASLEEP))

    def mines(self):
        return [s for s in self.m.sites if s["kind"] == MINE]

    def civ_ok(self, c):
        cv = self.w.civ(c)
        return cv["ended_year"] is None and self.w.visible(cv)

    def town_ok(self, s, c):
        if not s["settlement"]:
            return None
        t = self.w.stl(s["settlement"])
        if not self.w.standing(t) or not self.w.visible(t) or t["owner_civ"] != c:
            return None
        return t

    def mining(self, c):
        return self.w.pd.race[self.w.race_c(c)]["mining"]

    def delving(self, c):
        return self.w.pd.race[self.w.race_c(c)].get("delving", 0)

    def warren_depth_of(self, sid):
        """7.6 MonstersSubsystem::warren_depth(settlement): 0 for an unknown id."""
        return self.warren_depth.get(sid, 0)

    # --- 6.1 steps
    def t4(self):
        self.deepen(quiet=False)
        if R2:
            self.delve_warrens(quiet=False)
        self.waking()

    def q4(self):
        self.deepen(quiet=True)
        if R2:
            self.delve_warrens(quiet=True)

    # --- r2 6.10 warren delving
    def region_ok(self, rid):
        r = self.w.geo.reg[rid]
        return warren_region_ok(r["cells"], r["walk_cells"], r["ore_reach_cells"], self.t["deep_warren_ore_min"])

    def live_warren(self, c, tid):
        """c has a live warren other than tid: a standing u owned by c, warren_delver(u) = c, depth >= 1, not sealed."""
        for u, d in self.warren_depth.items():
            if u == tid or d < 1 or u in self.warren_sealed or self.warren_delver.get(u) != c:
                continue
            uu = self.w.stl(u)
            if self.w.standing(uu) and uu["owner_civ"] == c:
                return True
        return False

    def delve_warrens(self, quiet):
        w, t = self.w, self.t
        Y = w.year
        for tid in range(1, len(w.settlements) + 1):
            tt = w.stl(tid)
            if not w.standing(tt) or not w.visible(tt):
                continue
            c = tt["owner_civ"]
            if not c or not self.civ_ok(c) or self.mining(c) != 0 or self.delving(c) <= 0:
                continue
            if tid in self.warren_woken or tid in self.warren_sealed:
                continue
            d = self.warren_depth.get(tid, 0)
            if d >= 3:
                continue
            if d == 0:
                if tt["population"] < t["deep_warren_population_min"] or not self.region_ok(tt["region"]) or \
                        self.live_warren(c, tid):
                    continue
            elif self.warren_delver.get(tid) != c:
                continue   # a warren taken by another realm is frozen
            oe = tt["owner_event"]
            if Y - w.events[oe - 1]["year"] < t["deep_years"][d]:
                continue
            p = delve_permille(t["deep_delve_permille"], self.delving(c), w.years if quiet else None)
            if quiet and w.quiet_events_left(c) <= 0:
                continue
            if not pp.chance_pm(w.draw("hist.deep.warren_delve", tid, d + 1, 0), p):
                continue
            cause = self.last_warren.get(tid, oe)
            e = w.append("warren_delved", region=tt["region"], settlement=tid,
                         participants=(("civilization", CIV, c),), values=(d + 1, telling_warren_delved(tid, d + 1)),
                         causes=(cause,))
            self.warren_depth[tid] = d + 1
            self.warren_delver[tid] = c
            self.last_warren[tid] = e
            self.log.append((Y, "warren delved", tid, d + 1, e))

    # --- 6.2 depth
    def deepen(self, quiet):
        w, t = self.w, self.t
        Y = w.year
        for s in self.mines():
            sid = s["id"]
            if s["ending"] != OPEN or sid in self.sealed_site:
                continue
            d = self.depth.get(sid, 0)
            if d >= 3:
                continue
            c = s["founder"]
            if not self.civ_ok(c) or self.town_ok(s, c) is None:
                continue
            mp = self.mining(c)
            if mp <= 0 or not can_delve(Y, s["origin_year"], d, t["deep_years"]):
                continue
            p = delve_permille(t["deep_delve_permille"], mp, w.years if quiet else None)
            if quiet and w.quiet_events_left(c) <= 0:
                continue   # D-R8: it waits
            if not pp.chance_pm(w.draw("hist.deep.delve", sid, d + 1, 0), p):
                continue
            cause = self.last_delve.get(sid, s["origin_event"])
            e = w.append("delving_deepened", region=s["region"], settlement=s["settlement"],
                         participants=(("civilization", CIV, c), ("site", SITE, sid)),
                         values=(d + 1, telling_delve(sid, d + 1)), causes=(cause,))
            self.depth[sid] = d + 1
            self.last_delve[sid] = e
            self.log.append((Y, "deepened", sid, d + 1, e))

    # --- 6.4 the waking
    def waking(self):
        w, t = self.w, self.t
        for s in self.mines():
            sid = s["id"]
            if s["ending"] != OPEN or sid in self.woken or sid in self.sealed_site:
                continue
            d = self.depth.get(sid, 0)
            if d < 1 or t["deep_wake_ppm"][d - 1] <= 0:
                continue
            c = s["founder"]
            if not self.civ_ok(c):
                continue
            tt = self.town_ok(s, c)
            if tt is None:
                continue
            K = [k for k in self.deep_kinds() if self.living_of(k) < self.m.kind(k)["max_alive"]]
            if not K:
                continue
            if not pp.chance_ppm(w.draw("hist.deep.wake", sid, 0, 0), wake_ppm(t["deep_wake_ppm"], d, self.m.M)):
                continue
            ws = [self.m.kind(k)["wake_rate"] for k in K]
            if sum(ws) == 0:
                continue
            k = K[pnr.weighted(w.draw("hist.deep.kind", sid, 0, 0), ws)]
            self.wake(s, d, c, tt, k)
            return
        if not R2:
            return
        for tid in range(1, len(w.settlements) + 1):   # r2 6.10: the warrens, after every mine candidate
            tt = w.stl(tid)
            if not w.standing(tt) or not w.visible(tt):
                continue
            c = self.warren_delver.get(tid, 0)
            if not c or tt["owner_civ"] != c or not self.civ_ok(c):
                continue
            if tid in self.warren_woken or tid in self.warren_sealed:
                continue
            d = self.warren_depth.get(tid, 0)
            if d < 1 or t["deep_wake_ppm"][d - 1] <= 0:
                continue
            K = [k for k in self.deep_kinds() if self.living_of(k) < self.m.kind(k)["max_alive"]]
            if not K:
                continue
            p = warren_wake_ppm(t["deep_wake_ppm"], d, self.m.M, t["deep_warren_wake_permille"])
            if not pp.chance_ppm(w.draw("hist.deep.warren_wake", tid, 0, 0), p):
                continue
            ws = [self.m.kind(k)["wake_rate"] for k in K]
            if sum(ws) == 0:
                continue
            k = K[pnr.weighted(w.draw("hist.deep.warren_kind", tid, 0, 0), ws)]
            self.wake(None, d, c, tt, k)
            return

    # --- r2 6.11 the allies' aid
    def aid(self, mid, c, tt):
        """[(a, q, S_a)] ascending a: the allies that answer (draws only; no event, no state)."""
        w, t, m = self.w, self.t, self.m
        if not R2 or w.alliances is None or w.race_relations is None:
            return []
        H = m.geo.hops(tt["region"])
        out = []
        for a in list(w.alliances.allies(c)):
            if len(out) >= t["deep_aid_max"]:
                break
            if not self.civ_ok(a):
                continue
            qs = [q for q in w.settlements if q["owner_civ"] == a and w.standing(q) and w.visible(q) and
                  H.get(q["region"]) is not None and H[q["region"]] <= t["deep_aid_hops"]]
            if not qs:
                continue
            q = min(qs, key=lambda x: (-x["population"], x["id"]))
            ra = w.race_c(a)
            p = aid_permille(t["deep_aid_permille"], w.pd.race[ra]["ally_join"])
            if pp.chance_pm(w.draw("hist.deep.aid", mid, a, 0), p):
                out.append((a, q["id"], aid_strength(q["population"], t["deep_aid_levy_permille"], m.prowess(ra))))
        return out

    def last_refuge(self, c, tt):
        """D-R7: P3-05 r12 22.7's predicate."""
        w = self.w
        return w.race_last_refuge(c) and [x["id"] for x in w.standing_of(c)] == [tt["id"]]

    def wake(self, s, d, c, tt, k):
        """6.4 items 4 to 7 at a mine s, or (r2, s None) at the warren tt."""
        w, t, m = self.w, self.t, self.m
        p502 = _p502()
        Y = w.year
        warren = s is None
        sid = 0 if warren else s["id"]
        region = tt["region"] if warren else s["region"]
        kd = m.kind(k)
        mid = len(m.monsters) + 1
        if mid > p502.MONSTERS_MAX:
            pe.fatal("history: bound: monsters exceed 100000")
        name, root = m.monster_name(k, mid)
        # r2 6.11: the aid, then 6.5 the gate, decided before any event
        A = self.aid(mid, c, tt)
        P = tt["population"]
        S = gate_strength(P, t["deep_gate_levy_permille"], m.prowess(w.race_c(c))) + sum(x[2] for x in A)
        odds = gate_odds(S, kd["might"], t["deep_seal_max_permille"])
        u = pnr.pick(w.draw("hist.deep.gate", mid, 0, 0), 1000)
        fall = not sealed_at(u, odds)
        forced = False
        if fall and self.last_refuge(c, tt):
            _dead, V = take_toll(P, t["deep_toll_permille"])
            if self.flight_plan(c, tt, V, region) is None:
                fall, forced = False, True   # "there was nowhere to flee"
        lid = len(m.sites) + 1
        if lid > p502.SITES_MAX:
            pe.fatal("history: bound: sites exceed 1000000")
        lname = m.lair_name(k, lid, root)
        if warren:
            e = w.append("deep_woken", region=region, settlement=tt["id"],
                         participants=(("monster", MONSTER, mid), ("civilization", CIV, c), ("lair", SITE, lid)),
                         values=(d, 3 + telling_woken(mid)), causes=(self.last_warren[tt["id"]],))
        else:
            e = w.append("deep_woken", region=region, settlement=tt["id"],
                         participants=(("monster", MONSTER, mid), ("civilization", CIV, c), ("site", SITE, sid),
                                       ("lair", SITE, lid)),
                         values=(d, telling_woken(mid)), causes=(self.last_delve[sid],))
        m.sites.append(dict(id=lid, name=lname, kind=LAIR, region=region, x=tt["x"], y=tt["y"],
                            settlement=tt["id"] if fall else 0, founder=0, monster=mid, origin_year=Y, origin_event=e,
                            ended_year=None, ending_event=0, ending=OPEN, hostile=None))
        m.monsters.append(dict(id=mid, kind=k, name=name, root=root, status=AWAKE, born=Y, born_event=e, lair=lid,
                               waking=e, sleeping=0, drive_until=0, hoard=0, holds=False, raids={}, died=None,
                               death_event=0, slayer=0, overrun=0, raid_years={}, place_raids={}, dead_kind=False))
        if warren:
            self.warren_woken.add(tt["id"])
            self.from_warren.add(mid)
        else:
            self.woken.add(sid)
        self.waking_realm[mid] = c
        self.takes[mid] = 0
        self.woken_event[mid] = e
        self.log.append((Y, "woken", sid, mid, k, d, S, odds, u, forced, tt["id"], warren, len(A)))
        # r2 6.11: one ally_aided_hold per ally in A, ascending, each followed by its credit
        aids = []
        rel = w.race_relations
        for a, q, _Sa in A:
            ea = w.append("ally_aided_hold", region=tt["region"], settlement=tt["id"],
                          participants=(("ally", CIV, a), ("partner", CIV, c), ("monster", MONSTER, mid),
                                        ("origin", SETTLEMENT, q)), values=(telling_aid(mid, a),), causes=(e,))
            aids.append(ea)
            ra, rc = w.race_c(a), w.race_c(c)
            if ra != rc and rel is not None:
                rel.credit(ra, rc, t["deep_aid_memory"], ea)
            self.log.append((Y, "aid", mid, a, q, ea))
        if not fall:
            mon = m.monsters[mid - 1]
            if warren:
                e2 = w.append("warren_sealed", region=region, settlement=tt["id"],
                              participants=(("civilization", CIV, c), ("monster", MONSTER, mid), ("lair", SITE, lid)),
                              values=(telling_sealed(mid),), causes=(e,) + tuple(aids))
                self.warren_sealed.add(tt["id"])
            else:
                e2 = w.append("deep_sealed", region=region, settlement=tt["id"],
                              participants=(("civilization", CIV, c), ("monster", MONSTER, mid), ("site", SITE, sid),
                                            ("lair", SITE, lid)),
                              values=(telling_sealed(mid),), causes=(e,) + tuple(aids))
                self.sealed_site.add(sid)
            mon["status"], mon["sleeping"], mon["drive_until"] = ASLEEP, e2, NEVER
            self.sealed.add(mid)
            self.log.append((Y, "sealed", sid, mid, e2, forced, warren))
            return
        took = self.take(mid, tt, e, tuple(aids))
        assert took, "6.5: a fall that cannot happen is a seal"

    # --- 6.6 the take
    def take(self, mid, tt, cause, aids=()):
        w, t, m = self.w, self.t, self.m
        c = self.waking_realm[mid]
        mon = m.monsters[mid - 1]
        P = tt["population"]
        dead, V = take_toll(P, t["deep_toll_permille"])
        plan = None
        if self.last_refuge(c, tt):
            plan = self.flight_plan(c, tt, V, m.lair_region(mon))
            if plan is None:
                self.log.append((w.year, "no take", mid, tt["id"]))
                return False
        mine = [x for x in m.sites if x["kind"] == MINE and x["ending"] == OPEN and x["settlement"] == tt["id"]]
        parts = (("monster", MONSTER, mid), ("civilization", CIV, c), ("lair", SITE, mon["lair"])) + (
            (("site", SITE, mine[0]["id"]),) if mine else ())
        tel = telling_lost(mid, tt["id"]) + (3 if mid in self.from_warren else 0)   # r2: the goblin wordings
        e = w.append("hold_lost", region=tt["region"], settlement=tt["id"], participants=parts,
                     values=(dead, V, tel), causes=(cause,) + tuple(aids))
        if dead > 0:
            tt["population"] = V
        if plan is not None:
            self.flight(plan, e)
        else:
            w.ruin(tt["id"], e)
        self.takes[mid] += 1
        mon["holds"] = True
        self.log.append((w.year, "take", mid, tt["id"], c, dead, V, plan is not None, e))
        return True

    # --- 6.7 after a raid
    def after_raid(self, mon, kd, tt, c, raid_e):
        w, t, m = self.w, self.t, self.m
        mid = mon["id"]
        if kd["wake"] != "deep" or mid in self.sealed or not w.standing(tt):
            return
        rc = self.waking_realm.get(mid, 0)
        alive = rc != 0 and w.civ(rc)["ended_year"] is None
        h = m.geo.hops(m.lair_region(mon)).get(tt["region"])
        if not may_take_later(self.takes.get(mid, 0), t["deep_takes_max"], h, t["deep_reach_hops"], tt["owner_civ"],
                              rc, alive, False):
            return
        p = take_permille(kd["overrun_permille"], m.M, t["overrun_people_ref"], tt["population"])
        if pp.chance_pm(w.draw("hist.deep.take", mid, tt["id"], 0), p):
            self.take(mid, tt, raid_e)

    # --- 7.3 the flight seam (ConflictSubsystem::deep_flight_plan, deep_flight)
    def flight_plan(self, c, tt, V, threat_region):
        """Pure: (D, site, n) or None. 20.1's destination with h(D) = min(hops(threat_region)(D), march_hops + 1)."""
        w = self.w
        ct = w.ct
        if ct["refuge_flight"] != 1 or ct["last_refuge"] != 1:
            return None
        race = w.race_c(c)
        rt = tt["region"]
        hops_t = w.geo.hops(rt)
        cap = ct["march_hops"] + 1
        hz = w.geo.hops(threat_region)
        cands = []
        for d in w.geo.ids:
            if w.owner[d][0] != 0 or w.geo.islet(d) or d == rt:
                continue
            if d not in hops_t or hops_t[d] > ct["flight_hops"]:
                continue
            if w.geo.capacity(d, race) < w.t["expand_min_capacity"]:
                continue
            cands.append((-min(hz.get(d, cap), cap), -w.geo.sigma(d, race), d))
        for _h, _s, d in sorted(cands):
            st = w.site(d, race, relaxed=False)
            if st is not None:
                n = min(V, 2 * w.geo.capacity(d, race))
                if n < w.t["abandon_min"]:
                    return None
                return (d, st, n)
        return None

    def flight(self, plan, e):
        """20.1 items 2 and 3 with the hold_lost as the cause and the ruin event (D-R4)."""
        w = self.w
        pcr = _pcr()
        d, (sx, sy, how), n = plan
        tt = w.stl(w.events[e - 1]["settlement"])
        c = tt["owner_civ"]
        race = w.race_c(c)
        h = len(w.settlements) + 1
        civ = w.civ(c)
        cnum = civ["culture"]
        cul = w.culture[cnum][1]
        name, _t = pnr.make_name(w.dat.naming[cul], cul, "settlement", w.inp.seed, cnum, h, w.registry,
                                 cores=w.name_cores())
        e2 = w.append("refugees_settled", region=d, settlement=h,
                      participants=(("settlement", SETTLEMENT, h), ("founder", CIV, c), ("origin", SETTLEMENT, tt["id"])),
                      values=(n,), causes=(e,))
        got = w.found_settlement(sx, sy, d, c, w.founding_kind(race, d, n, sx, sy), n, name, e2)
        assert got == h
        if w.ledger is not None:
            w.ledger.moved(tt["id"], h, n)
        w.site_how[h] = how
        w.set_owner(d, c, e2)
        w.hidden_until[h] = w.year + w.hidden_years_of(c)
        if pcr.FLAGS["hidden42"] and w.hidden_years_of(c) >= 1:
            w.append("refuge_hidden", end_year=min(w.hidden_until[h], w.inp.params["span"]) - 1, region=d,
                     settlement=h, participants=(("settlement", SETTLEMENT, h), ("founder", CIV, c)), causes=(e2,))
        if pcr.FLAGS["lie_low"]:
            w.lie_low(c, h, w.hidden_until[h])
            w.low_log.append((w.year, c, w.hidden_until[h]))
        tt["population"] -= n
        if not w.standing(tt) or tt["owner_civ"] == 0:
            pcr.fatal("history: monsters: ruin of settlement %d, which is not standing" % tt["id"])
        w.ruin(tt["id"], e)
        w.version += 1
        self.log.append((w.year, "flight", c, tt["id"], h, d, n, e2))


# ---------------------------------------------------------------------------
# Rows: invariants of 13, reader checks of 9, statistics of 10 item 7
# ---------------------------------------------------------------------------

class X:
    """Indexes over a world's history rows (res-style tuples, or a world file's tables)."""

    def __init__(self, res, borders):
        p502 = _p502()
        self.r = p502.Rows(res, borders)
        self.res = res
        self.E = None

    def of(self, key):
        return [e[0] for e in self.r.events_of(key)]

    def one(self, e, role):
        v = self.r.role(e, role)
        return v[0] if v else 0

    def val(self, e, key):
        return self.r.value(e, key)


def deep_kind_set(res):
    return {k[0] for k in res.get("monster_kind", []) if k[5] == WAKE_DEEP}


def violations(res, borders, tuning, mining_of_race, E, races_present=None, delving_of_race=None):
    """13 items 1 to 9 and 11's deep part on rows, (r2) items 12 to 15. tuning: history.monsters (the deep fields);
    mining_of_race, delving_of_race: race id -> mining_permille, delving_permille; races_present: the race ids that
    awoke (for item 6), or None."""
    x = X(res, borders)
    r = x.r
    out = []
    t = tuning
    deep = deep_kind_set(res)
    site = {s[0]: s for s in r.sites}
    mon = {m[0]: m for m in r.monsters}
    year = lambda e: r.ev[e][1]   # noqa: E731
    quiet = lambda e: r.phase.get(r.ev[e][4], (0, 0, 0))[2] == 2   # noqa: E731
    sealed_of_m = {x.one(e, "monster"): e for e in x.of("deep_sealed") + x.of("warren_sealed")}
    delving_of_race = delving_of_race or {}
    aid_max = t.get("deep_aid_max", 2)
    aids_of = {}
    for e in x.of("ally_aided_hold"):
        cs = r.causes.get(e, [])
        aids_of.setdefault(cs[0] if len(cs) == 1 else 0, []).append(e)
    per_stl = {}
    for e in x.of("warren_delved"):
        per_stl.setdefault(r.ev[e][6], []).append(e)
    names_m = lambda e, mid: any(p[2] == MONSTER and p[3] == mid for p in r.parts.get(e, []))   # noqa: E731
    sealed_of_s = {x.one(e, "site"): e for e in x.of("deep_sealed")}
    # 1
    per_site = {}
    for e in x.of("delving_deepened"):
        per_site.setdefault(x.one(e, "site"), []).append(e)
    for sid, es in sorted(per_site.items()):
        s = site.get(sid)
        if s is None or s[2] != MINE:
            out.append("1: delving_deepened at site %d, not a mine" % sid)
            continue
        ds = [x.val(e, "depth") for e in es]
        if ds != list(range(1, len(ds) + 1)) or len(ds) > 3:
            out.append("1: mine %d depths %s" % (sid, ds))
        for e in es:
            d = x.val(e, "depth")
            if not 1 <= d <= 3 or year(e) - s[9] < t["deep_years"][d - 1]:
                out.append("1: mine %d depth %d at worked years %d" % (sid, d, year(e) - s[9]))
            if s[13] != OPEN and s[11] is not None and (year(e) > s[11] or (year(e) == s[11] and e > s[12])):
                out.append("1: mine %d deepened e%d after its end" % (sid, e))
            if sid in sealed_of_s and e > sealed_of_s[sid]:
                out.append("1: mine %d deepened e%d after its sealing" % (sid, e))
            c = x.one(e, "civilization")
            if c != s[7] or mining_of_race.get(r.civ[c][2], 0) <= 0:
                out.append("1: e%d by c%d (mine of c%d, mining %d)" % (e, c, s[7], mining_of_race.get(r.civ[c][2], 0)))
            if x.val(e, "telling") != telling_delve(sid, d):
                out.append("1: e%d telling" % e)
            cs = r.causes.get(e, [])
            prev = [y for y in es if y < e]
            want = prev[-1] if prev else s[10]
            if cs != [want]:
                out.append("1: e%d causes %s, want [%d]" % (e, cs, want))
    # 2 and 3
    woken = x.of("deep_woken")
    by_site = {}
    by_stl = {}
    for e in woken:
        sid, mid, lid = x.one(e, "site"), x.one(e, "monster"), x.one(e, "lair")
        s, m, lair = site.get(sid), mon.get(mid), site.get(lid)
        d = x.val(e, "depth")
        if quiet(e):
            out.append("2: deep_woken e%d in a quiet step" % e)
        if sid == 0:   # r2: a warren's waking (no site)
            tid, c = r.ev[e][6], x.one(e, "civilization")
            by_stl.setdefault(tid, []).append(e)
            st = r.stl.get(tid)
            wd = [y for y in per_stl.get(tid, []) if y < e]
            if not wd or x.one(wd[-1], "civilization") != c:
                out.append("2: warren waking e%d at settlement %d without a warren of c%d" % (e, tid, c))
                continue
            if not 1 <= d <= 3 or t["deep_wake_ppm"][d - 1] <= 0 or \
                    max(x.val(y, "depth") for y in wd) != d:
                out.append("2: warren waking e%d at depth %d" % (e, d))
            if m is None or m[2] not in deep or m[5] != e:
                out.append("2: deep_woken e%d monster %d not a deep kind born of it" % (e, mid))
            if lair is None or st is None or lair[2] != LAIR or (lair[4], lair[5]) != (st[2], st[3]) or \
                    lair[10] != e or lair[8] != mid:
                out.append("2: warren waking e%d lair %d not at the settlement's cell" % (e, lid))
            if x.val(e, "telling") != 3 + telling_woken(mid):
                out.append("15: warren waking e%d telling %s" % (e, x.val(e, "telling")))
            if r.causes.get(e, []) != [wd[-1]]:
                out.append("2: warren waking e%d causes %s" % (e, r.causes.get(e, [])))
            out += outcome_violations(x, r, e, mid, lair, aids_of.get(e, []), aid_max, True)
            continue
        by_site.setdefault(sid, []).append(e)
        if s is None or s[2] != MINE or not 1 <= d <= 3 or t["deep_wake_ppm"][d - 1] <= 0:
            out.append("2: deep_woken e%d at site %d depth %d" % (e, sid, d))
            continue
        got_d = max([x.val(y, "depth") for y in per_site.get(sid, []) if y < e] or [0])
        if got_d != d:
            out.append("2: deep_woken e%d depth %d, the mine's %d" % (e, d, got_d))
        if m is None or m[2] not in deep or m[5] != e:
            out.append("2: deep_woken e%d monster %d not a deep kind born of it" % (e, mid))
        if lair is None or lair[2] != LAIR or (lair[4], lair[5]) != (s[4], s[5]) or lair[10] != e or lair[8] != mid:
            out.append("2: deep_woken e%d lair %d not at the mine's cell" % (e, lid))
        if x.one(e, "civilization") != s[7]:
            out.append("2: deep_woken e%d civilization" % e)
        if x.val(e, "telling") != telling_woken(mid):
            out.append("2, 15: e%d telling" % e)
        if r.causes.get(e, []) != [max([y for y in per_site.get(sid, []) if y < e] or [0])]:
            out.append("2: deep_woken e%d causes %s" % (e, r.causes.get(e, [])))
        # 3
        out += outcome_violations(x, r, e, mid, lair, aids_of.get(e, []), aid_max, False)
    for sid, es in by_site.items():
        if len(es) > 1:
            out.append("2: mine %d woke %d times" % (sid, len(es)))
    for tid, es in by_stl.items():
        if len(es) > 1:
            out.append("2: warren %d woke %d times" % (tid, len(es)))
    # 4
    for mid, se in sorted(sealed_of_m.items()):
        m = mon.get(mid)
        if m is None or m[9] != ASLEEP:
            out.append("4: sealed monster %d status %s" % (mid, m[9] if m else None))
        for e in sorted(r.ev):
            if e > se and names_m(e, mid) and r.ev[e][3] in (46, 48, 50, 51, 88, 89, 96):
                out.append("4: sealed monster %d in e%d (%s)" % (mid, e, x.r.kind_of(e)))
        if x.val(se, "telling") != telling_sealed(mid):
            out.append("4: e%d telling" % se)
        if x.r.kind_of(se) == "warren_sealed":   # r2: the warren never deepens again
            for y in per_stl.get(r.ev[se][6], []):
                if y > se:
                    out.append("4: warren %d delved e%d after its sealing" % (r.ev[se][6], y))
    # 5 and 9
    realm = {x.one(e, "monster"): x.one(e, "civilization") for e in woken}
    warren_m = {x.one(e, "monster") for e in woken if x.one(e, "site") == 0}
    count = {}
    for e in x.of("hold_lost"):
        mid, c, sid = x.one(e, "monster"), x.one(e, "civilization"), r.ev[e][6]
        st = r.stl.get(sid)
        count[mid] = count.get(mid, 0) + 1
        if st is None or st[10] != 3 or st[13] != e or (len(st) > 14 and st[14] != 2):
            out.append("5: hold_lost e%d settlement %d not ruined by it with ending 2" % (e, sid))
        if realm.get(mid) != c:
            out.append("5: hold_lost e%d by c%d, the waking realm c%s" % (e, c, realm.get(mid)))
        dead, V = x.val(e, "dead"), x.val(e, "survivors")
        if take_toll(dead + V, t["deep_toll_permille"]) != (dead, V):
            out.append("5: hold_lost e%d dead %d survivors %d against the toll" % (e, dead, V))
        if x.val(e, "telling") != telling_lost(mid, sid) + (3 if mid in warren_m else 0):
            out.append("5, 15: e%d telling" % e)
        cs = r.causes.get(e, [])
        first = count[mid] == 1
        if first:
            w0 = [y for y in woken if x.one(y, "monster") == mid][:1]
            if cs != w0 + (aids_of.get(w0[0], []) if w0 else []):
                out.append("5: first hold_lost e%d causes %s" % (e, cs))
        else:
            raid = cs[0] if len(cs) == 1 else 0
            if not raid or x.r.kind_of(raid) != "monster_raid" or x.one(raid, "monster") != mid or \
                    r.ev[raid][6] != sid or year(raid) != year(e):
                out.append("5: later hold_lost e%d causes %s" % (e, cs))
            lair = r.lair_at(mid, e)
            h = r.hops(lair[3]).get(r.ev[e][5]) if lair is not None else None
            if h is None or h > t["deep_reach_hops"]:
                out.append("5: later hold_lost e%d at %s hops" % (e, h))
        for s in r.sites:   # 9
            if s[2] != MINE or s[6] != sid or s[10] > e:
                continue
            if s[13] != OPEN and s[12] != e and (s[11] < year(e) or s[12] < e):
                continue   # ended before the take
            if s[13] != END_LOST or s[12] != e or s[11] != year(e):
                out.append("9: mine %d at the lost hold %d ends %s e%s in %s" % (s[0], sid, s[13], s[12], s[11]))
            if x.one(e, "site") != s[0]:
                out.append("5: hold_lost e%d names site %d, the open mine %d" % (e, x.one(e, "site"), s[0]))
    for mid, n in count.items():
        if n > 1 + t["deep_takes_max"]:
            out.append("5: monster %d took %d holds" % (mid, n))
    # 6 (D-R12)
    for e in x.of("refugees_settled"):
        cs = r.causes.get(e, [])
        if len(cs) == 1 and x.r.kind_of(cs[0]) == "hold_lost":
            h = cs[0]
            if year(h) != year(e) or x.one(e, "founder") != x.one(h, "civilization") or \
                    x.one(e, "origin") != r.ev[h][6]:
                out.append("6: refugees_settled e%d from hold_lost e%d" % (e, h))
    if races_present is not None:
        alive = {c[2] for c in r.civ.values() if c[8] is None}
        for rc in sorted(races_present):
            if rc not in alive:
                out.append("6: race %d has no civilization alive at E" % rc)
    # 7
    for e in x.of("civ_ended"):
        cs = r.causes.get(e, [])
        if len(cs) == 1 and x.r.kind_of(cs[0]) == "hold_lost":
            c = x.one(e, "civilization")
            if year(e) <= year(cs[0]) or any(s[8] == c for s in r.stl.values()):
                out.append("7: civ_ended e%d of c%d after hold_lost e%d" % (e, c, cs[0]))
    out += warren_violations(x, r, t, per_stl, mining_of_race, delving_of_race)
    out += aid_violations(x, r, res, t, aids_of, mining_of_race)
    # 8
    mk = res.get("monster_kind", [])
    if [k[1] for k in mk][:6] != ["dragon", "giant", "ogre", "troll", "undead", "wyvern"] or \
            [(k[0], k[1], k[5]) for k in mk][6:9] != [(7, "ymrach", 3), (8, "ysgarth", 3), (9, "zhakul", 3)]:
        out.append("8: monster_kind rows %s" % [(k[0], k[1], k[5]) for k in mk])
    return out


def outcome_violations(x, r, e, mid, lair, aids, aid_max, warren):
    """13 item 3 (r2: the aid events between, the outcome citing the waking then the aids; warren_sealed only at a
    warren's waking, and the only seal there)."""
    out = []
    year = r.ev[e][1]
    seal = "warren_sealed" if warren else "deep_sealed"
    nxt = [y for y in sorted(r.ev) if y > e and r.ev[y][1] == year and any(
        p[2] == MONSTER and p[3] == mid for p in r.parts.get(y, [])) and
        r.kind_of(y) in ("deep_sealed", "hold_lost", "warren_sealed")]
    first = nxt[0] if nxt else e
    between_others = [y for y in sorted(r.ev) if e < y < first and
                      any(p[2] == MONSTER and p[3] != mid for p in r.parts.get(y, []))]
    outcome = [y for y in nxt if r.causes.get(y, []) == [e] + list(aids)]
    if len(outcome) != 1 or between_others or outcome[0] != first:
        out.append("3: deep_woken e%d outcome %s" % (e, outcome))
        return out
    k = r.kind_of(outcome[0])
    if k not in (seal, "hold_lost"):
        out.append("3: deep_woken e%d outcome e%d is a %s" % (e, outcome[0], k))
    if any(not e < a < outcome[0] for a in aids):
        out.append("3: deep_woken e%d aids %s not between it and its outcome" % (e, aids))
    if len(aids) > aid_max:
        out.append("14: deep_woken e%d has %d aids, above deep_aid_max %d" % (e, len(aids), aid_max))
    allies = [x.one(a, "ally") for a in aids]
    if allies != sorted(set(allies)):
        out.append("14: deep_woken e%d aids not in ascending ally id: %s" % (e, allies))
    if lair is not None:
        if k != "hold_lost" and lair[6] != 0:
            out.append("3: sealed e%d lair %d holds settlement %d" % (outcome[0], lair[0], lair[6]))
        if k == "hold_lost" and lair[6] != r.ev[e][6]:
            out.append("3: fall e%d lair %d settlement %d, the hold %d" % (outcome[0], lair[0], lair[6], r.ev[e][6]))
    return out


def _life_end(r, tid, after):
    """The first event after `after` that ends a warren's life for its delver (r2 invariant 12, reading D-R20): a
    deep_woken of the settlement, its ruin, its capture or its cession. None: alive at E."""
    st = r.stl.get(tid)
    ends = []
    if st is not None and st[13]:
        ends.append(st[13])
    for y in sorted(r.ev):
        if y <= after:
            continue
        k = r.kind_of(y)
        if k == "deep_woken" and r.ev[y][6] == tid:
            ends.append(y)
        elif k in ("settlement_captured", "settlements_ceded") and (r.ev[y][6] == tid or any(
                p[2] == SETTLEMENT and p[3] == tid for p in r.parts.get(y, []))):
            ends.append(y)
    ends = [y for y in ends if y > after]
    return min(ends) if ends else None


def warren_violations(x, r, t, per_stl, mining_of_race, delving_of_race):
    """r2 13 items 12 and 13 on rows."""
    out = []
    year = lambda e: r.ev[e][1]   # noqa: E731
    race = lambda c: r.civ[c][2] if c in r.civ else 0   # noqa: E731
    lives = {}
    for tid, es in sorted(per_stl.items()):
        ds = [x.val(e, "depth") for e in es]
        if ds != list(range(1, len(ds) + 1)) or len(ds) > 3:
            out.append("12: warren %d depths %s" % (tid, ds))
        cs = {x.one(e, "civilization") for e in es}
        if len(cs) != 1:
            out.append("12: warren %d delved by %s" % (tid, sorted(cs)))
        c = x.one(es[0], "civilization")
        rc = race(c)
        if mining_of_race.get(rc, 0) != 0 or delving_of_race.get(rc, 0) <= 0:
            out.append("12: warren %d delved by c%d of race %d (mining %d, delving %d)" % (
                tid, c, rc, mining_of_race.get(rc, 0), delving_of_race.get(rc, 0)))
        root = r.causes.get(es[0], [0])[0] if r.causes.get(es[0]) else 0
        for i, e in enumerate(es):
            d = x.val(e, "depth")
            if x.val(e, "telling") != telling_warren_delved(tid, d):
                out.append("12: e%d telling" % e)
            want = es[i - 1] if i else None
            cs_e = r.causes.get(e, [])
            if want is not None and cs_e != [want]:
                out.append("12: warren_delved e%d causes %s, want [%d]" % (e, cs_e, want))
            if root in r.ev and 1 <= d <= 3 and year(e) - year(root) < t["deep_years"][d - 1]:
                out.append("12: warren %d depth %d at %d years under its owner" % (tid, d, year(e) - year(root)))
        if len(r.causes.get(es[0], [])) != 1:
            out.append("12: first warren_delved e%d causes %s" % (es[0], r.causes.get(es[0], [])))
        end = _life_end(r, tid, es[0])
        for e in es:
            if end is not None and e > end:
                st = r.stl.get(tid)
                ruined = st is not None and st[13] and e > st[13]
                woke = r.kind_of(end) == "deep_woken" and e > end
                if ruined or woke:
                    out.append("12: warren %d delved e%d after its waking or ruin" % (tid, e))
        lives.setdefault(c, []).append((year(es[0]), year(end) if end is not None else None, tid))
    for c, ls in sorted(lives.items()):   # at most one live warren at every year's end
        for i, (a0, a1, t0) in enumerate(ls):
            for b0, b1, t1 in ls[i + 1:]:
                lo, hi = max(a0, b0), min(a1 if a1 is not None else 1 << 40, b1 if b1 is not None else 1 << 40)
                # alive at the end of year Y when it started at or before Y and ended after Y
                if lo < hi:
                    out.append("12: c%d has two live warrens %d and %d in %d" % (c, t0, t1, lo))
    # 13: no kind 3 site and no arms_sold seller of a race with mining_permille 0
    for s in r.sites:
        if s[2] == MINE and mining_of_race.get(race(s[7]), 0) <= 0:
            out.append("13: mine %d of c%d, whose race does not mine" % (s[0], s[7]))
    for e in x.of("arms_sold"):
        c = x.one(e, "seller")
        if c and mining_of_race.get(race(c), 0) <= 0:
            out.append("13: arms_sold e%d by c%d, whose race does not mine" % (e, c))
    return out


def aid_violations(x, r, res, t, aids_of, mining_of_race):
    """r2 13 item 14 on rows."""
    out = []
    hops_max = t.get("deep_aid_hops", 2)
    alliances = [q for q in res.get("relation", []) if q[4] == "alliance"]
    forbidden = {r.kid[k] for k in ("war_declared", "ally_joined_war", "ally_refused", "claim_settlements",
                                    "region_claimed") if k in r.kid}
    cited = {}
    for e in sorted(r.ev):
        for c in r.causes.get(e, []):
            cited.setdefault(c, []).append(e)
    for w0, aids in sorted(aids_of.items()):
        for a in aids:
            ally, partner, mid, q = (x.one(a, k) for k in ("ally", "partner", "monster", "origin"))
            if w0 not in r.ev or r.kind_of(w0) != "deep_woken" or x.one(w0, "civilization") != partner or \
                    x.one(w0, "monster") != mid or r.ev[w0][1] != r.ev[a][1] or ally == partner:
                out.append("14: ally_aided_hold e%d does not answer its waking e%d" % (a, w0))
                continue
            y = r.ev[a][1]
            pair = {ally, partner}
            ok = any({row[1], row[3]} == pair and row[7] < a and (row[6] is None or row[6] == -1 or row[6] > y or
                                                                  (row[8] and row[8] > a)) for row in alliances)
            if not ok:
                out.append("14: ally_aided_hold e%d: c%d not allied with c%d" % (a, ally, partner))
            st = r.stl.get(q)
            h = r.hops(r.ev[a][5]).get(st[4]) if st is not None else None
            if st is None or (r.nb and (h is None or h > hops_max)) or st[6] > y or \
                    (st[13] and st[13] < a):
                out.append("14: ally_aided_hold e%d origin %d not a standing settlement within %d hops" % (
                    a, q, hops_max))
            if x.val(a, "telling") != telling_aid(mid, ally):
                out.append("14: e%d telling" % a)
            for b in cited.get(a, []):
                if r.ev[b][3] in forbidden:
                    out.append("14: e%d (%s) caused by the aid e%d" % (b, r.kind_of(b), a))
            for row in res.get("relation", []):
                if a in (row[7], row[8]):
                    out.append("14: relation %s caused by the aid e%d" % (row[:5], a))
    return out


def reader_problems(res):
    """9's reader checks on rows: [message] in the readers' form `corrupt table '<t>': row <k>: <reason>`, in the
    order a reader meets them (table by table, rows ascending)."""
    r = _p502().Rows(res, [])
    x = X(res, [])
    out = []
    E = lambda e, why: out.append("corrupt table 'event': row %d: %s" % (e, why))   # noqa: E731
    for k in res.get("monster_kind", []):
        if not 1 <= k[5] <= 3:
            out.append("corrupt table 'monster_kind': row %d: wake %d not 1 to 3" % (k[0], k[5]))
    wake = {k[0]: k[5] for k in res.get("monster_kind", [])}
    for m in r.monsters:
        bk = r.kind_of(m[5]) if m[5] in r.ev else None
        if wake.get(m[2]) == WAKE_DEEP and (bk != "deep_woken" or x.one(m[5], "monster") != m[0]):
            out.append("corrupt table 'monster': row %d: born_event %d is not the waking of this monster" % (m[0], m[5]))
    origin = {}
    for s in r.sites:
        origin.setdefault(s[10], []).append(s[0])
    ranges = {"depth": (1, 3), "telling": (0, 2)}
    wide = ("deep_woken", "hold_lost")   # r2: telling 0 to 5
    seen_depth = {}
    woken_site = {}
    woken_m = {}
    first_lost = set()
    sites = {s[0]: s for s in r.sites}
    later = []   # r2: the warren and aid checks run after the r1 checks, per event in id order (9's r2 item)
    for e in sorted(r.ev):
        k = r.kind_of(e)
        if k not in KINDS.values():
            continue
        if k in ("ally_aided_hold", "warren_delved", "warren_sealed"):
            later.append(e)
            continue
        for key in ("depth", "telling", "dead", "survivors"):
            if key in r.vkeys.get(r.ev[e][3], {}):
                v = x.val(e, key)
                lo, hi = (0, 5) if key == "telling" and k in wide else ranges.get(key, (0, None))
                if v < lo or (hi is not None and v > hi):
                    E(e, "value %s %d outside %d to %s" % (key, v, lo, hi if hi is not None else "max"))
        if k == "deep_woken" and x.one(e, "site") == 0:
            later.append(e)   # r2: a warren's waking
            woken_m[x.one(e, "monster")] = e
            if len(origin.get(e, [])) != 1:
                E(e, "deep_woken is the origin_event of %d sites" % len(origin.get(e, [])))
            continue
        if k in ("delving_deepened", "deep_woken"):
            sid, c = x.one(e, "site"), x.one(e, "civilization")
            s = sites.get(sid)
            if s is None or s[2] != MINE or s[7] != c:
                E(e, "site %d is not a mine of %d" % (sid, c))
        if k == "delving_deepened":
            sid, d = x.one(e, "site"), x.val(e, "depth")
            got = seen_depth.setdefault(sid, set())
            if d in got:
                E(e, "depth %d again" % d)
            elif d > 1 and d - 1 not in got:
                E(e, "depth %d without depth %d" % (d, d - 1))
            got.add(d)
        elif k == "deep_woken":
            sid = x.one(e, "site")
            if sid in woken_site:
                E(e, "a second waking of site %d" % sid)
            woken_site[sid] = e
            woken_m[x.one(e, "monster")] = e
            if len(origin.get(e, [])) != 1:
                E(e, "deep_woken is the origin_event of %d sites" % len(origin.get(e, [])))
        elif k in ("deep_sealed", "hold_lost"):
            mid = x.one(e, "monster")
            if k == "deep_sealed" or mid not in first_lost:
                w0 = woken_m.get(mid)
                if w0 is None or w0 not in r.causes.get(e, []):
                    E(e, "%s without its waking" % k)
                if k == "hold_lost":
                    first_lost.add(mid)
            if k == "hold_lost":
                st = r.stl.get(r.ev[e][6])
                if st is None or st[13] != e:
                    E(e, "hold_lost %d did not ruin its settlement" % e)
    # r2 (9): warrens and aid, per event in id order, after the checks above
    wdepth, wciv, wwoken = {}, {}, {}
    for e in later:
        k = r.kind_of(e)
        tid = r.ev[e][6]
        if k != "deep_woken":
            for key in ("depth", "telling"):
                if key in r.vkeys.get(r.ev[e][3], {}):
                    v = x.val(e, key)
                    lo, hi = ranges[key]
                    if v < lo or v > hi:
                        E(e, "value %s %d outside %d to %d" % (key, v, lo, hi))
        if k == "warren_delved":
            d, c = x.val(e, "depth"), x.one(e, "civilization")
            got = wdepth.setdefault(tid, set())
            if d in got:
                E(e, "depth %d again" % d)
            elif d > 1 and d - 1 not in got:
                E(e, "depth %d without depth %d" % (d, d - 1))
            got.add(d)
            if wciv.setdefault(tid, c) != c:
                E(e, "warren %d delved by two realms" % tid)
        elif k == "deep_woken":
            c = x.one(e, "civilization")
            if not wdepth.get(tid) or wciv.get(tid) != c:
                E(e, "waking of settlement %d without a warren" % tid)
            if tid in wwoken:
                E(e, "a second waking of settlement %d" % tid)
            wwoken[tid] = e
        elif k == "warren_sealed":
            w0 = woken_m.get(x.one(e, "monster"))
            if w0 is None or w0 not in r.causes.get(e, []) or x.one(w0, "site") != 0:
                E(e, "warren_sealed without its waking")
        elif k == "ally_aided_hold":
            w0 = [c for c in r.causes.get(e, []) if c in r.ev and r.kind_of(c) == "deep_woken"]
            ok = len(w0) == 1 and r.ev[w0[0]][1] == r.ev[e][1] and \
                x.one(w0[0], "civilization") == x.one(e, "partner") and \
                x.one(w0[0], "monster") == x.one(e, "monster") and x.one(e, "ally") != x.one(e, "partner")
            if not ok:
                E(e, "ally_aided_hold %d does not answer its waking" % e)
    return out


def stats_of_tables(t):
    """10 item 7: [(key, value)] in the order of the spec, from a world's tables (dict of rows)."""
    x = X(t, [])
    r = x.r
    deep = deep_kind_set(t)
    dd = x.of("delving_deepened")
    lost = set(x.of("hold_lost"))
    cause_lost = lambda e: any(c in lost for c in r.causes.get(e, []))   # noqa: E731
    return [("mines_deepened", len(dd)),
            ("mines_deepest", len({x.one(e, "site") for e in dd if x.val(e, "depth") == 3})),
            ("deep_woken", len(x.of("deep_woken"))), ("deep_sealed", len(x.of("deep_sealed"))),
            ("holds_lost", len(lost)),
            ("deep_flights", sum(1 for e in x.of("refugees_settled") if cause_lost(e))),
            ("realms_lost_to_deep", sum(1 for e in x.of("civ_ended") if cause_lost(e))),
            ("deep_alive", sum(1 for m in r.monsters if m[2] in deep and m[9] in (AWAKE, ASLEEP))),
            # r2
            ("warrens_delved", len(x.of("warren_delved"))),
            ("warrens_deepest", len({r.ev[e][6] for e in x.of("warren_delved") if x.val(e, "depth") == 3})),
            ("warrens_woken", sum(1 for e in x.of("deep_woken") if x.one(e, "site") == 0)),
            ("warrens_sealed", len(x.of("warren_sealed"))),
            ("deep_aided", len(x.of("ally_aided_hold"))),
            ("wakings_aided", len({c for e in x.of("ally_aided_hold") for c in r.causes.get(e, [])
                                   if r.kind_of(c) == "deep_woken"}))]


def mine_depth_at(t, site, year):
    """11.1 Timeline::mine_depth_at (9's derivation): the largest depth of the site's kind 93 events of year <= Y, 0
    none or a site of another kind."""
    x = X(t, [])
    s = [y for y in x.r.sites if y[0] == site]
    if not s or s[0][2] != MINE:
        return 0
    return max([x.val(e, "depth") for e in x.of("delving_deepened") if x.one(e, "site") == site and
                x.r.ev[e][1] <= year] or [0])


def warren_depth_at(t, settlement, year):
    """r2 11.1 Timeline::warren_depth_at: the largest depth of the settlement's kind 98 events of year <= Y, 0 none."""
    x = X(t, [])
    return max([x.val(e, "depth") for e in x.of("warren_delved") if x.r.ev[e][6] == settlement and
                x.r.ev[e][1] <= year] or [0])


def sealed_year(t, monster):
    """11.1 Timeline::sealed_year: the year of the monster's deep_sealed (r2: or warren_sealed), -1 none."""
    x = X(t, [])
    for e in x.of("deep_sealed") + x.of("warren_sealed"):
        if x.one(e, "monster") == monster:
            return x.r.ev[e][1]
    return -1


# ---------------------------------------------------------------------------
# Fixtures: depths-small (4.2) and I0 (14)
# ---------------------------------------------------------------------------

def depths_small_data(dat=None):
    """4.2 depths-small: monsters-small (P5-02 19.3, on the conflict stress data) with the deep edits (D-R9)."""
    p502 = _p502()
    base = p502.monsters_small_data(dat)
    d = copy.copy(base)
    d.base = copy.copy(base.base)
    d.base.monsters_tuning = dict(copy.deepcopy(base.base.monsters_tuning), **copy.deepcopy(pdr.DEPTHS_SMALL_029))
    d.race = copy.deepcopy(base.race)   # r2 4.2: goblin delving_permille 1000
    for key, f in pdr.DEPTHS_SMALL_RACES_029.items():
        rid = d.chron.race_by_id[key]["order"]
        d.race[rid]["delving"] = f["delving_permille"]
    return d


def i1_data(dat=None):
    """r2 I1 (14): delving_permille 0 for every race and deep_aid_permille 0."""
    d = depths_small_data(dat)
    d.base = copy.copy(d.base)
    d.base.monsters_tuning = dict(d.base.monsters_tuning, deep_aid_permille=0)
    d.race = copy.deepcopy(d.race)
    for rid in d.race:
        d.race[rid]["delving"] = 0
    return d


def zero_data(dat=None):
    """I0 (14): deep_delve_permille 0 and forge_depth_permille 0."""
    import p3_conflict_reference as pcr
    dat = dat or pcr.data()
    d = pcr.edited(dat, conflict=dict(forge_depth_permille=0))
    d.base = copy.copy(dat.base)
    d.base.monsters_tuning = dict(copy.deepcopy(dat.base.monsters_tuning), deep_delve_permille=0)
    return d


HISTORY_TABLES = ("event", "event_participant", "event_cause", "event_value", "phase", "civilization", "settlement",
                  "settlement_census", "region_control", "relation", "region_name", "beast_population", "monster",
                  "site", "race_relation", "settlement_race", "figure", "book")


def table_hash(rows):
    h = hashlib.sha256()
    for r in rows:
        h.update(repr(tuple(r)).encode())
    return h.hexdigest()


# ---------------------------------------------------------------------------
# Units (16 items 2 to 9 on hand-checked values)
# ---------------------------------------------------------------------------

class Checker:
    def __init__(self):
        self.count = self.failures = 0

    def eq(self, what, got, want):
        self.count += 1
        if got != want:
            self.failures += 1
            print("FAIL %s: got %r, want %r" % (what, got, want))

    def ok(self, what, cond):
        self.eq(what, bool(cond), True)


def check_units(c):
    # 6.2 depth
    c.eq("6.2 worked years at deep_years[0] - 1", can_delve(1149, 1000, 0, [150, 500, 1000]), False)
    c.eq("6.2 worked years at deep_years[0]", can_delve(1150, 1000, 0, [150, 500, 1000]), True)
    c.eq("6.2 depth 2 at 499 worked years", can_delve(1499, 1000, 1, [150, 500, 1000]), False)
    c.eq("6.2 depth 3 at 1000 worked years", can_delve(2000, 1000, 2, [150, 500, 1000]), True)
    c.eq("6.2 depth 3 is the last", can_delve(9000, 1000, 3, [150, 500, 1000]), False)
    c.eq("6.2 T4 p, mining 1000", delve_permille(10, 1000), 10)
    c.eq("6.2 T4 p, mining 500", delve_permille(10, 500), 5)
    c.eq("6.2 T4 p, mining 0", delve_permille(10, 0), 0)
    c.eq("6.2 Q4 p x L (L 50)", delve_permille(10, 1000, 50), 500)
    c.eq("6.2 Q4 p x L capped at 1000 (L 200)", delve_permille(10, 1000, 200), 1000)
    c.eq("6.2 Q4 mining 500, L 30", delve_permille(10, 500, 30), 150)
    c.eq("6.2 telling (s + d) mod 3", [telling_delve(7, d) for d in (1, 2, 3)], [2, 0, 1])
    # 6.3 the forge
    c.eq("6.3 not forged: f unchanged", forge_factor(10000, 50, 15, 3, False), 10000)
    for D, want in ((0, 10500), (1, 10650), (2, 10800), (3, 10950)):
        c.eq("6.3 forge depth %d (+%d)" % (D, want - 10000), forge_factor(10000, 50, 15, D, True), want)
    c.eq("6.3 forge_depth_permille 0 is P5-04's", forge_factor(10000, 50, 0, 3, True), 10500)
    # 6.4 the waking
    c.eq("6.4 wake at depth 1 (0 ppm)", wake_ppm([0, 150, 600], 1, 1000), 0)
    c.eq("6.4 wake at depth 2, M normal", wake_ppm([0, 150, 600], 2, 1000), 150)
    c.eq("6.4 wake at depth 3, M high 1600", wake_ppm([0, 150, 600], 3, 1600), 960)
    c.eq("6.4 wake capped at 10^6", wake_ppm([0, 0, 1000000], 3, 1600), 1000000)
    c.eq("6.4 weights: a weight of 0 never chosen", [pnr.weighted(u << 32, [0, 350, 150]) for u in (0, 1 << 31)],
         [1, 1])
    c.eq("6.4 weights 500/350/150 at the boundaries", [pnr.weighted(u, [500, 350, 150]) for u in
                                                       ((1 << 63) - 1, (1 << 63), 0xDFFFFFFF00000000)], [0, 1, 2])
    c.eq("6.4 telling m mod 3", telling_woken(14), 2)
    # 6.5 the gate (D-R2: the formula floors)
    S3000 = gate_strength(3000, 1000, 1400)
    S800 = gate_strength(800, 1000, 1400)
    c.eq("6.5 S of a dwarven town of 3,000", S3000, 4200)
    c.eq("6.5 town of 3,000 against Ymrach", gate_odds(S3000, 4000, 800), 512)
    c.eq("6.5 town of 3,000 against Zhakul", gate_odds(S3000, 7000, 800), 375)
    c.eq("6.5 village of 800 against Ymrach (spec: 219)", gate_odds(S800, 4000, 800), 218)
    c.eq("6.5 village of 800 against Zhakul (spec: 138)", gate_odds(S800, 7000, 800), 137)
    c.eq("6.5 S = 0: odds 0", gate_odds(0, 4000, 800), 0)
    c.eq("6.5 S = might: 500", gate_odds(4000, 4000, 800), 500)
    c.eq("6.5 the cap", gate_odds(10 ** 9, 2500, 800), 800)
    c.eq("6.5 levy 4000 and prowess 1400 of 100,000 (no overflow)", gate_strength(100000, 4000, 1400), 560000)
    c.eq("6.5 sealed at u = odds - 1", sealed_at(511, 512), True)
    c.eq("6.5 falls at u = odds", sealed_at(512, 512), False)
    c.eq("6.5 telling (m + 1) mod 3", telling_sealed(14), 0)
    # 6.6 the take
    c.eq("6.6 toll of 3,016 (the sample: 1206 died, 1810 fled)", take_toll(3016, 400), (1206, 1810))
    c.eq("6.6 toll of 535 (the sample: 214 died, 321 fled)", take_toll(535, 400), (214, 321))
    c.eq("6.6 toll 0", take_toll(900, 0), (0, 900))
    c.eq("6.6 telling (m + t) mod 3", telling_lost(14, 40), 0)
    # 6.7 a later take
    c.eq("6.7 take p of a town of 1,500 (f_p capped)", take_permille(300, 1000, 3000, 1500), 300)
    c.eq("6.7 take p of a city of 6,000", take_permille(300, 1000, 3000, 6000), 150)
    c.eq("6.7 take p, M high", take_permille(400, 1600, 3000, 12000), 160)
    c.eq("6.7 take p, base capped", take_permille(800, 1600, 3000, 3000), 1000)
    args = dict(takes_max=2, reach_hops=1, waking_realm=4, realm_alive=True, sealed=False)
    c.eq("6.7 takes 1 and 2 allowed, 3 not (1 + deep_takes_max)",
         [may_take_later(n, hops=1, owner=4, **args) for n in (1, 2, 3)], [True, True, False])
    c.eq("6.7 two borders away, reach 1", may_take_later(1, hops=2, owner=4, **args), False)
    c.eq("6.7 another realm", may_take_later(1, hops=0, owner=5, **args), False)
    c.eq("6.7 sealed", may_take_later(1, 2, 0, 1, 4, 4, True, True), False)
    c.eq("6.7 the realm ended", may_take_later(1, 2, 0, 1, 4, 4, False, False), False)
    # 6.7's hunt sample: S 840 against might 4000 and 7000
    c.eq("6.7 hunt odds 840 against Ymrach (17%)", gate_odds(840, 4000, 1000), 173)
    c.eq("6.7 hunt odds 840 against Zhakul (11%)", gate_odds(840, 7000, 1000), 107)
    # 14: the largest products
    c.ok("14 P_t x 4000 x 1400 < 2^43 for P_t 10^6", 10 ** 6 * 4000 * 1400 < (1 << 43))
    c.eq("11.1 mine_depth_word", [MINE_DEPTH_WORD.get(d, "") for d in range(5)],
         ["", "below daylight", "to the mountain's roots", "to the old deep", ""])
    check_units_r2(c)


def check_units_r2(c):
    """r2: 6.10 and 6.11 on hand-checked values (the samples of 6.5, 6.10 and 6.11)."""
    # 6.10 the warren's first level: population, upland, ore
    c.eq("6.10 upland: 4 x 499 < 2000", warren_region_ok(2000, 499, 0, 1), True)
    c.eq("6.10 not upland: 4 x 500 = 2000", warren_region_ok(2000, 500, 0, 1), False)
    c.eq("6.10 ore at deep_warren_ore_min - 1", warren_region_ok(2000, 1500, 4, 5), False)
    c.eq("6.10 ore at deep_warren_ore_min", warren_region_ok(2000, 1500, 5, 5), True)
    c.eq("6.10 deep_warren_ore_min 0: any region", warren_region_ok(2000, 2000, 0, 0), True)
    # 6.10 the pace: goblins 300 at deep_delve_permille 10: 3 per mille; Q4 x L
    c.eq("6.10 warren p (goblin 300)", delve_permille(10, 300), 3)
    c.eq("6.10 warren p, Q4 L 50", delve_permille(10, 300, 50), 150)
    c.eq("6.10 warren p, Q4 L 400 capped", delve_permille(10, 300, 400), 1000)
    c.eq("6.10 depths-small (delving 1000, delve 1000)", delve_permille(1000, 1000), 1000)
    # 6.10 item 2: the warren's wake chance
    c.eq("6.10 warren wake at depth 3 (600 x 200)", warren_wake_ppm([0, 150, 600], 3, 1000, 200), 120)
    c.eq("6.10 warren wake at depth 2", warren_wake_ppm([0, 150, 600], 2, 1000, 200), 30)
    c.eq("6.10 warren wake at depth 1 (0)", warren_wake_ppm([0, 150, 600], 1, 1000, 200), 0)
    c.eq("6.10 warren wake, M high", warren_wake_ppm([0, 150, 600], 3, 1600, 200), 192)
    c.eq("6.10 warren wake, floor twice (151 x 1000 div 1000 x 7 div 1000)", warren_wake_ppm([0, 151, 0], 2, 1000, 7),
         1)
    c.eq("6.10 warren wake, the min over the product", warren_wake_ppm([0, 0, 1000000], 3, 10000, 500), 1000000)
    c.eq("6.10 warren wake permille 0", warren_wake_ppm([0, 150, 600], 3, 1000, 0), 0)
    c.eq("6.10 depths-small at depth 3", warren_wake_ppm([0, 200000, 600000], 3, 1000, 1000), 600000)
    c.eq("6.10 telling (t + d) mod 3", [telling_warren_delved(7, d) for d in (1, 2, 3)], [2, 0, 1])
    c.eq("6.10 warren waking telling 3 + m mod 3", [3 + telling_woken(m) for m in (3, 4, 5)], [3, 4, 5])
    c.eq("6.10 a goblin warren's hold_lost telling + 3", telling_lost(5, 7) + 3, 3)
    # 6.5's goblin samples: a warren of 1,000 (prowess 800, S = 800)
    Sg = gate_strength(1000, 1000, 800)
    c.eq("6.5 S of a goblin warren of 1,000", Sg, 800)
    c.eq("6.5 goblin warren against Ysgarth", gate_odds(Sg, 2500, 800), 242)
    c.eq("6.5 goblin warren against Ymrach", gate_odds(Sg, 4000, 800), 166)
    c.eq("6.5 goblin warren against Zhakul", gate_odds(Sg, 7000, 800), 102)
    # 6.11 the aid
    for race, join, want in (("dwarf", 800, 400), ("human", 600, 300), ("orc", 600, 300), ("goblin", 400, 200),
                             ("elf", 700, 350), ("faerie", 150, 75)):
        c.eq("6.11 aid chance of a %s ally (6.11's list)" % race, aid_permille(500, join), want)
    c.eq("6.11 deep_aid_permille 0: no aid", aid_permille(0, 800), 0)
    c.eq("6.11 depths-small (aid 1000)", aid_permille(1000, 150), 150)
    Sa = aid_strength(2000, 500, 1000)
    c.eq("6.11 S_a of a human town of 2,000", Sa, 1000)
    S = gate_strength(800, 1000, 1400) + Sa
    c.eq("6.5 the village with the human aid: S", S, 2120)
    c.eq("6.5 the village with the human aid against Ymrach (346)", gate_odds(S, 4000, 800), 346)
    c.eq("6.11 S_a floors twice", aid_strength(1999, 500, 1600), 1598)
    c.eq("6.11 S_a no overflow at 10^6 x 4000 x 1600", aid_strength(10 ** 6, 4000, 1600), 6400000)
    c.ok("14 P_q x 4000 x 1600 < 2^44", 10 ** 6 * 4000 * 1600 < (1 << 44))
    c.ok("14 ppm x M x warren permille < 2^44", 10 ** 6 * 10 ** 4 * 10 ** 3 < (1 << 44))
    c.eq("6.11 telling (m + a) mod 3", telling_aid(3, 4), 1)
    # 6.5's boundary with the aid: S + S_a exactly at the odds where u = odds - 1 seals and u = odds falls
    o = gate_odds(S, 4000, 800)
    c.eq("6.5 with the aid: u = odds - 1 seals, u = odds falls", (sealed_at(o - 1, o), sealed_at(o, o)), (True, False))
    c.eq("6.5 without the aid the same u falls", sealed_at(o - 1, gate_odds(gate_strength(800, 1000, 1400), 4000, 800)),
         False)


def check_render_r2(c, cat, view):
    """r2 16 item 18: kinds 94 and 96 at telling 3 to 5, 97 to 99 at each telling and depth; 8.2's samples."""
    for key in ("ally_aided_hold", "warren_delved", "warren_sealed"):
        c.eq("8.2 %s passes template_problem" % key, pe.template_problem(cat[key], cat[key]["templ"]), None)
    wk = ((1, 1, MONSTER, 3), (2, 2, CIV, 3), (4, 4, SITE, 8))   # a warren's waking: no site (role 3)
    for tel, want in ((3, "Burrowing ever deeper under Gnashgut, Clan Skrag woke the Swarm Beneath Gnashgut, a host of "
                          "eyeless things from under the stone."),
                      (4, "Digging after grubs and glitter beneath Gnashgut, Clan Skrag blundered into the old dark and "
                          "woke the Swarm Beneath Gnashgut, a host of eyeless things from under the stone."),
                      (5, "In the stinking deep tunnels of Gnashgut, Clan Skrag woke the Swarm Beneath Gnashgut, a "
                          "host of eyeless things from under the stone.")):
        c.eq("8.2 deep_woken telling %d (a warren)" % tel, pe.render(view(21, "deep_woken", wk, (2, tel),
                                                                          settlement=7), 21), want)
    hl = ((1, 1, MONSTER, 5), (2, 2, CIV, 3), (3, 3, SITE, 8))
    for tel, want in ((3, "The Eyeless of Gnashgut boiled up out of the burrows into Gnashgut of Clan Skrag; 412 died, "
                          "and 618 fled."),
                      (4, "The Eyeless of Gnashgut came up the goblins' own tunnels into Gnashgut of Clan Skrag; 412 "
                          "died, and 618 fled."),
                      (5, "The Eyeless of Gnashgut rose under the warrens of Gnashgut of Clan Skrag; 412 died, and 618 "
                          "fled.")):
        c.eq("8.2 hold_lost telling %d (a warren)" % tel, pe.render(view(22, "hold_lost", hl, (412, 618, tel),
                                                                         settlement=7), 22), want)
    ad = ((1, 1, CIV, 4), (2, 2, CIV, 1), (3, 3, MONSTER, 1), (4, 4, SETTLEMENT, 8))
    for tel, want in ((0, "Answering its ally's call, the Kingdom of Harl sent warriors from Dunmere to stand with the "
                          "Hold of Durn at the gate of Gimlin against Bolgrim the Deep-Coiled."),
                      (1, "Bound by old oaths, the Kingdom of Harl marched out of Dunmere to stand with the Hold of Durn "
                          "at the gate of Gimlin against Bolgrim the Deep-Coiled."),
                      (2, "Unbidden, the Kingdom of Harl hurried its people from Dunmere to stand with the Hold of Durn "
                          "at the gate of Gimlin against Bolgrim the Deep-Coiled.")):
        c.eq("8.2 ally_aided_hold telling %d" % tel, pe.render(view(23, "ally_aided_hold", ad, (tel,)), 23), want)
    opens = ("Ever greedy for the dark, ", "Squabbling and scraping, ", "Grub by grub and stone by stone, ")
    depths = ("deeper", "below the reach of daylight", "down among the mountain's roots",
              "into the oldest dark beneath the world")
    for tel in range(3):
        for d in range(4):
            c.eq("8.2 warren_delved telling %d depth %d" % (tel, d),
                 pe.render(view(24, "warren_delved", ((1, 1, CIV, 3),), (d, tel), settlement=7), 24),
                 "%sClan Skrag dug the warrens of Gnashgut %s." % (opens[tel], depths[d]))
    ws = ((1, 1, CIV, 3), (2, 2, MONSTER, 4), (3, 3, SITE, 9))
    for tel, want in ((0, "Clan Skrag caved in the tunnels over Ysmor Flame-Under-Stone beneath Gnashgut."),
                      (1, "Shrieking and stabbing in the dark, Clan Skrag blocked the deep ways against Ysmor "
                          "Flame-Under-Stone under Gnashgut."),
                      (2, "With rockfall and fire, Clan Skrag buried Ysmor Flame-Under-Stone deep below Gnashgut, and "
                          "no goblin went that way again.")):
        c.eq("8.2 warren_sealed telling %d (the empty first word at 0)" % tel,
             pe.render(view(25, "warren_sealed", ws, (tel,), settlement=7), 25), want)
    # the ruler: {civilization:under} at a warren's sealing when a ruler reigns (civ 1 has one in this view)
    c.eq("8.2 warren_sealed names a reigning ruler",
         pe.render(view(26, "warren_sealed", ((1, 1, CIV, 1), (2, 2, MONSTER, 4), (3, 3, SITE, 9)), (2,),
                        settlement=7), 26),
         "With rockfall and fire, the Hold of Durn under Thrain the Old buried Ysmor Flame-Under-Stone deep below "
         "Gnashgut, and no goblin went that way again.")


def check_data(c):
    """16 item 1 on the listings of p3_data_reference (4.1 to 4.4) and on the data root as read at 0.29.0."""
    pe.use_version(VERSION)
    c.eq("4.1 ids sort after wyvern", sorted(["dragon", "giant", "ogre", "troll", "undead", "wyvern"] +
                                             list(pdr.DEEP_IDS_029), key=lambda s: s.encode())[6:],
         ["ymrach", "ysgarth", "zhakul"])
    for k, m in pdr.DEEP_KINDS_029.items():
        c.eq("4.1 %s wake deep, no biomes, no overruns, ageless, no flare" % k,
             (m["wake"], m["biomes"], m["overruns"], m["lifespan_years"], m["flare"]), ("deep", [], [], 0, False))
        c.eq("4.1 %s passes monster_deep_biomes" % k, pdr.deep_monster_messages(m), [])
        for n, lo, hi in pdr.MONSTER_RANGES:
            c.ok("4.1 %s.%s inside int(%d, %d)" % (k, n, lo, hi), lo <= m[n] <= hi)
    c.eq("4.1 monster_deep_biomes refuses a biome",
         pdr.deep_monster_messages(dict(pdr.DEEP_KINDS_029["ymrach"], biomes=[dict(biome="alpine", weight=1)])),
         ["a deep monster has no biomes"])
    t = {n: v for n, _k, _lo, _hi, v in pdr.MONSTERS_NEW_029}
    c.eq("4.2 the shipped deep_years are ordered", pdr.deep_years_messages(t), [])
    c.eq("4.2 monsters_deep_years_ordered", pdr.deep_years_messages(dict(t, deep_years=[150, 150, 1000])),
         ["deep_years[1] 150 is not above deep_years[0] 150"])
    c.eq("4.2 monsters_deep_years_ordered (third)", pdr.deep_years_messages(dict(t, deep_years=[5, 10, 9])),
         ["deep_years[2] 9 is not above deep_years[1] 10"])
    c.eq("4.2 depths-small passes", pdr.deep_years_messages(dict(t, **pdr.DEPTHS_SMALL_029)), [])
    dat = pdr.load_root("game/data")
    h = dict(dat["history"])
    mo = h["monsters"]
    c.eq("4.2 fields after undead_regions_ref, in order (r2: sixteen)", list(mo)[-17:],
         ["undead_regions_ref"] + [n for n, *_ in pdr.MONSTERS_NEW_029])
    c.eq("4.2 r2 starts", {n: mo[n] for n, *_ in pdr.MONSTERS_NEW_029[8:]},
         dict(deep_warren_population_min=600, deep_warren_ore_min=1, deep_warren_wake_permille=200,
              deep_aid_permille=500, deep_aid_hops=2, deep_aid_levy_permille=500, deep_aid_max=2, deep_aid_memory=30))
    races = dict(dat["race"])
    c.eq("4.5 delving_permille as shipped", {k: races[k]["delving_permille"] for k in sorted(races)},
         {k: pdr.DELVING_029[k] for k in sorted(races)})
    c.eq("4.5 the shipped races pass race_delves_one_way", [k for k in races if pdr.delving_messages(races[k])], [])
    c.eq("4.5 race_delves_one_way", pdr.delving_messages(dict(mining_permille=1, delving_permille=1)),
         ["a race delves mines or warrens, not both"])
    c.eq("4.5 mining only, delving only", (pdr.delving_messages(dict(mining_permille=1000, delving_permille=0)),
                                         pdr.delving_messages(dict(mining_permille=0, delving_permille=300))), ([], []))
    co = list(h["conflict"])
    c.eq("4.3 forge_depth_permille right after arms_strength_permille",
         co[co.index("arms_strength_permille") + 1], "forge_depth_permille")
    rn = [r for r in h["renaming"]["rules"] if r["key"] == "deep"]
    c.eq("4.4 rule 8 deep is the eighth rule", [r["key"] for r in h["renaming"]["rules"]].index("deep"), 7)
    c.eq("4.4 rule 8", rn[0] if rn else None, pdr.RENAMING_RULE_029)
    ids = [k for k, _ in dat["monster"]]
    c.eq("invariant 8: the monster ids in canonical order", ids,
         ["dragon", "giant", "ogre", "troll", "undead", "wyvern", "ymrach", "ysgarth", "zhakul"])


def check_render(c):
    """16 item 9: every new template at each telling and depth, the empty first word, the changed templates."""
    pe.use_version(VERSION)
    dat = pdr.load_root("game/data")
    report = [(e["kind"], e["salience"], e["template"]) for e in dict(dat["history"])["report"]["events"]]
    cat = {k["key"]: k for k in pe.event_catalogue(report)}
    for key in KINDS.values():
        c.eq("8.2 %s passes template_problem" % key, pe.template_problem(cat[key], cat[key]["templ"]), None)
    for key in ("civ_ended", "region_renamed", "refugees_arrived", "ally_refused"):
        c.eq("8.2 changed %s passes template_problem" % key, pe.template_problem(cat[key], cat[key]["templ"]), None)
    names = {(CIV, 1): "The Hold of Durn", (CIV, 2): "Clan Nirgurn", (SETTLEMENT, 5): "Gimlin",
             (SETTLEMENT, 6): "Kazmar", (SITE, 3): "The Mines of Gimlin", (SITE, 4): "The Coils of Bolgrim",
             (MONSTER, 1): "Bolgrim the Deep-Coiled", (MONSTER, 2): "The Swarm Beneath Durak", (pe.REGION, 7): "Iron Downs",
             # r2 (8.2's goblin samples)
             (CIV, 3): "Clan Skrag", (CIV, 4): "The Kingdom of Harl", (SETTLEMENT, 7): "Gnashgut",
             (SETTLEMENT, 8): "Dunmere", (SITE, 8): "The Nest of Gnashgut", (SITE, 9): "The Coils of Ysmor",
             (MONSTER, 3): "The Swarm Beneath Gnashgut", (MONSTER, 4): "Ysmor Flame-Under-Stone",
             (MONSTER, 5): "The Eyeless of Gnashgut"}
    kinds_of = {(MONSTER, 1): "a wyrm of the lightless deep", (MONSTER, 2): "a host of eyeless things from under the stone",
                (MONSTER, 3): "a host of eyeless things from under the stone"}

    def name(t, x, form):
        if form == "kind":
            return kinds_of[(t, x)]
        if t == 6:
            return "Thrain the Old"
        return names[(t, x)]

    def view(eid, key, parts, values, settlement=5, causes=(), extra=None):
        evs = {eid: dict(year=900, end_year=900, kind=cat[key]["id"], region=7, settlement=settlement,
                         participants=list(parts), values=dict(enumerate(values, 1)), causes=list(causes))}
        for cid, ckey in (extra or {}).items():
            evs[cid] = dict(year=899, end_year=899, kind=cat[ckey]["id"], region=7, settlement=settlement,
                            participants=[], values={}, causes=[])
        return dict(catalogue={k["id"]: k for k in cat.values()}, events=evs, name=name,
                    region_at=lambda r, e: names[(pe.REGION, r)], region_at_year=lambda r, y: names[(pe.REGION, r)],
                    ruler_at=lambda c, e: 1 if c == 1 else 0)

    dd = ((1, 1, CIV, 1), (2, 2, SITE, 3))
    want_open = ("Following a rich seam, the", "Year upon year, the", "By hammer and lamp, the")
    want_depth = ("deeper", "below the reach of daylight", "into the roots of the mountain",
                  "into the oldest dark beneath the world")
    for tel in range(3):
        for d in range(4):
            got = pe.render(view(10, "delving_deepened", dd, (d, tel)), 10)
            c.eq("8.2 delving_deepened telling %d depth %d" % (tel, d), got,
                 "%s Hold of Durn delved the Mines of Gimlin %s." % (want_open[tel], want_depth[d]))
    wk = ((1, 1, MONSTER, 1), (2, 2, CIV, 1), (3, 3, SITE, 3), (4, 4, SITE, 4))
    for tel, want in ((0, "Delving too deep beneath Gimlin, the Hold of Durn under Thrain the Old woke Bolgrim the "
                          "Deep-Coiled, a wyrm of the lightless deep."),
                      (1, "In the deepest halls under Gimlin, the Hold of Durn under Thrain the Old woke Bolgrim the "
                          "Deep-Coiled, a wyrm of the lightless deep."),
                      (2, "Where the last seam ran out beneath Gimlin, the Hold of Durn under Thrain the Old broke into "
                          "the old dark and woke Bolgrim the Deep-Coiled, a wyrm of the lightless deep.")):
        c.eq("8.2 deep_woken telling %d" % tel, pe.render(view(11, "deep_woken", wk, (3, tel)), 11), want)
    sl = ((1, 1, CIV, 1), (2, 2, MONSTER, 1), (3, 3, SITE, 3), (4, 4, SITE, 4))
    for tel, want in ((0, "The Hold of Durn under Thrain the Old sealed Bolgrim the Deep-Coiled in the deeps of the "
                          "Mines of Gimlin."),
                      (1, "Holding the gate at great cost, the Hold of Durn under Thrain the Old walled Bolgrim the "
                          "Deep-Coiled up beneath the Mines of Gimlin."),
                      (2, "With stone and rune, the Hold of Durn under Thrain the Old shut Bolgrim the Deep-Coiled away "
                          "below the Mines of Gimlin, and the deep ways were closed.")):
        c.eq("8.2 deep_sealed telling %d (the empty first word at 0)" % tel,
             pe.render(view(12, "deep_sealed", sl, (tel,)), 12), want)
    hl = ((1, 1, MONSTER, 1), (2, 2, CIV, 1), (3, 3, SITE, 4), (4, 4, SITE, 3))
    for tel, vals, causes, extra, want in (
            (2, (1206, 1810), (), None, "Bolgrim the Deep-Coiled broke out of the deeps into Gimlin of the Hold of "
                                        "Durn; 1,206 died, and 1,810 fled."),
            (0, (0, 0), (), None, "Bolgrim the Deep-Coiled came up out of the deep and took Gimlin of the Hold of Durn; "
                                  "no one died, and none escaped."),
            (1, (1, 1), (5,), {5: "monster_raid"}, "After the raids, Bolgrim the Deep-Coiled rose from below and overran "
                                                   "Gimlin of the Hold of Durn; one died, and one escaped.")):
        c.eq("8.2 hold_lost telling %d dead %d" % (tel, vals[0]),
             pe.render(view(13, "hold_lost", hl, vals + (tel,), causes=causes, extra=extra), 13), want)
    c.eq("8.2 hold_lost without its site (0-1)",
         pe.render(view(14, "hold_lost", hl[:3], (214, 321, 1)), 14),
         "Bolgrim the Deep-Coiled rose from below and overran Gimlin of the Hold of Durn; 214 died, and 321 fled.")
    ce = pe.render(view(15, "civ_ended", ((1, 1, CIV, 2),), (), causes=(5,), extra={5: "hold_lost"}), 15)
    c.eq("8.2 civ_ended after a hold lost", ce, "After the loss of its last hold to the deep, Clan Nirgurn came to an end.")
    c.ok("8.2 region_renamed knows hold_lost", "hold_lost=the fall of a hold to the deep|" in cat["region_renamed"]["templ"])
    c.ok("8.2 refugees_arrived knows hold_lost", "hold_lost=the fall of their hold|" in cat["refugees_arrived"]["templ"])
    c.ok("8.2 ally_refused motive 5", "Hoping to profit by its ally's losses" in cat["ally_refused"]["templ"] and
         "Grasping for its own gain" not in cat["ally_refused"]["templ"])
    c.eq("8.1 kinds 93 to 99", [(k["id"], k["key"], k["caused"], k["region_use"], k["settlement_use"])
                                for k in pe.COMPILED_029[-7:]],
         [(93, "delving_deepened", 1, 1, 1), (94, "deep_woken", 1, 1, 1), (95, "deep_sealed", 1, 1, 1),
          (96, "hold_lost", 1, 1, 1), (97, "ally_aided_hold", 1, 1, 1), (98, "warren_delved", 1, 1, 1),
          (99, "warren_sealed", 1, 1, 1)])
    roles = {k["id"]: [(r["role"], r["type"], r["min"], r["max"]) for r in k["roles"]] for k in pe.COMPILED_029[-7:]}
    c.eq("8.1 hold_lost roles", roles[96],
         [("monster", 8, 1, 1), ("civilization", 3, 1, 1), ("lair", 9, 1, 1), ("site", 9, 0, 1)])
    c.eq("8.1 r2 deep_woken roles (site 0-1)", roles[94],
         [("monster", 8, 1, 1), ("civilization", 3, 1, 1), ("site", 9, 0, 1), ("lair", 9, 1, 1)])
    c.eq("8.1 r2 ally_aided_hold roles", roles[97],
         [("ally", 3, 1, 1), ("partner", 3, 1, 1), ("monster", 8, 1, 1), ("origin", pe.SETTLEMENT, 1, 1)])
    c.eq("8.1 r2 warren_delved roles", roles[98], [("civilization", 3, 1, 1)])
    c.eq("8.1 r2 warren_sealed roles", roles[99], [("civilization", 3, 1, 1), ("monster", 8, 1, 1), ("lair", 9, 1, 1)])
    c.eq("8.1 values", [k["values"] for k in pe.COMPILED_029[-7:]],
         [["depth", "telling"], ["depth", "telling"], ["telling"], ["dead", "survivors", "telling"], ["telling"],
          ["depth", "telling"], ["telling"]])
    check_render_r2(c, cat, view)
    c.eq("9 schema 13", pe.SCHEMA_VERSION, 13)
    c.eq("9 wake codes", pe.WAKE_CODE, {"wild": 1, "dead": 2, "deep": 3})


# ---------------------------------------------------------------------------
# Replays: depths-small on the conflict stress geographies (fixtures), world files (check-world), the golden file
# ---------------------------------------------------------------------------

FIXTURE_SEEDS = (0, 1, 2, 3, 5, 6, 7, 9, 12, 13, 14)


ALLIED_SEEDS = (3, 5, 6, 7)


def allied_data():
    """r2: depths-allied, depths-small on P5-04's alliances-small (alliances form at once, every race present: reading
    A11 of p5_04_alliances_reference), so that the aid of 6.11 occurs on the stress geographies (depths-small alone
    forms few alliances before its wakings fill the kinds' max_alive)."""
    import p5_04_alliances_reference as p504
    d = depths_small_data(p504.small_data())
    d.base = copy.copy(d.base)   # and room for wakings all through the history: the deep kinds' max_alive 20
    d.base.monsters = [(k, dict(v, max_alive=ALLIED_MAX_ALIVE) if v["wake"] == "deep" else v) for k, v in d.base.monsters]
    d.base.monsters_tuning = dict(d.base.monsters_tuning, deep_aid_hops=8)   # and allies from afar
    return d


ALLIED_MAX_ALIVE = 20


def run_stress(seed, dat, allied=False):
    pcr = _pcr()
    case = pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
    if allied:
        import p5_04_alliances_reference as p504
        case = p504.small_case(seed)
    inp, _g = pcr.synthetic_inputs(seed, dat, case)
    return pcr.history_stage(dat, inp), inp


def res_problems(res, inp, dat):
    """The invariants of 13 and P5-02's (with 13 item 11) and the reader checks on a run."""
    w = res["_world"]
    borders = [(b["a"], b["b"]) for b in inp.borders]
    mining = {rid: v["mining"] for rid, v in w.pd.race.items()}
    delving = {rid: v.get("delving", 0) for rid, v in w.pd.race.items()}
    present = {r[0] for r in inp.races if r[9] == 1 and any(c["race"] == r[0] for c in w.civs)}
    out = violations(res, borders, w.monsters.t, mining, inp.params["span"], present, delving)
    out += ["P5-02 " + v for v in _p502().res_violations(res, w, quarter=False)]
    out += ["reader " + v for v in reader_problems(res)]
    return out


def cmd_fixtures(argv):
    pcr = _pcr()
    pcr.use_version(VERSION)
    quick = "--quick" in argv
    c = Checker()
    dat = depths_small_data()
    seen, outcomes, where = set(), {}, {}
    runs = [(seed, False) for seed in (FIXTURE_SEEDS[:4] if quick else FIXTURE_SEEDS)] + \
        [(seed, True) for seed in (ALLIED_SEEDS[:2] if quick else ALLIED_SEEDS)]
    allied = None
    for seed, al in runs:
        if al and allied is None:
            allied = allied_data()
        res, inp = run_stress(seed, allied if al else dat, al)
        label = ("allied-%d" if al else "stress-%d") % seed
        w = res["_world"]
        for v in res_problems(res, inp, dat):
            c.ok("depths-small %s %s" % (label, v), False)
        log = w.monsters.deep.log
        for x in log:
            seen.add(x[1])
        seen.update("forced seal" for x in log if x[1] == "sealed" and x[5])
        seen.update("flight" for x in log if x[1] == "take" and x[7])
        seen.update("later take" for x in log if x[1] == "take" and
                    any(w.events[cc - 1]["key"] == "monster_raid" for cc in w.events[x[-1] - 1]["causes"]))
        seen.update("realm lost" for e in w.events if e["key"] == "civ_ended" and
                    any(w.events[cc - 1]["key"] == "hold_lost" for cc in e["causes"]))
        # r2 paths
        seen.update("warren woken" for x in log if x[1] == "woken" and x[11])
        seen.update("warren sealed" for x in log if x[1] == "sealed" and x[6])
        seen.update("warren lost" for x in log if x[1] == "take" and x[2] in w.monsters.deep.from_warren)
        seen.update("warren depth 3" for x in log if x[1] == "warren delved" and x[3] == 3)
        seen.update("aid" for x in log if x[1] == "aid")
        seen.update("two aids" for x in log if x[1] == "woken" and x[12] >= 2)
        aid_es = {x[5] for x in log if x[1] == "aid"}
        rel = w.race_relations
        if rel is not None and any(e in p.get("pos", []) for p in rel.pairs.values() for e in aid_es):
            seen.add("aid credit")
        for x in log:
            if x[1] in ("woken", "aid", "warren delved"):
                where.setdefault(x[1], (label, x[0]))
        st = dict(stats_of_tables(res))
        outcomes[label] = st
        print("%s %s" % (label, " ".join("%s=%d" % kv for kv in stats_of_tables(res))))
    for path in ("deepened", "woken", "sealed", "take", "later take", "warren delved", "warren woken", "aid"):
        c.ok("depths-small shows the path '%s' (seen: %s)" % (path, ", ".join(sorted(seen))), path in seen)
    print("first seen (run, year): %s" % ", ".join("%s %s" % kv for kv in sorted(where.items())))
    for path in ("flight", "forced seal", "no take", "realm lost", "warren sealed", "warren lost", "warren depth 3",
                 "two aids", "aid credit"):
        if path not in seen:
            print("note: depths-small on the stress geographies shows no '%s' (reported, not failed)" % path)
    # I0 (14): deep_delve_permille 0 and forge_depth_permille 0 give 0.28.0's history tables
    for seed in (FIXTURE_SEEDS[:1] if quick else FIXTURE_SEEDS[:3]):
        pcr.use_version(VERSION)
        z = zero_data(pcr.stress_data())
        a, _ = run_stress(seed, z)
        pcr.FLAGS["deep"] = False   # the same build with the deep rules removed (7.5's credit_units kept)
        b, _ = run_stress(seed, pcr.stress_data())
        pcr.FLAGS["deep"] = True
        for t in ("event", "event_participant", "event_cause", "event_value", "civilization", "settlement",
                  "relation", "site", "region_control"):
            c.eq("I0 stress-%d table %s equals the build without the deep rules" % (seed, t), table_hash(a.get(t, [])), table_hash(b.get(t, [])))
        pcr.use_version(VERSION)
    # r2 I1 (14): delving 0 for every race and deep_aid_permille 0 give r1's history tables
    global R2
    for seed in (FIXTURE_SEEDS[:1] if quick else FIXTURE_SEEDS[:3]):
        pcr.use_version(VERSION)
        a, _ = run_stress(seed, i1_data())
        R2 = False
        b, _ = run_stress(seed, i1_data())
        R2 = True
        for t in ("event", "event_participant", "event_cause", "event_value", "civilization", "settlement",
                  "relation", "site", "monster", "region_control", "race_relation"):
            c.eq("I1 stress-%d table %s equals r1's" % (seed, t), table_hash(a.get(t, [])), table_hash(b.get(t, [])))
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def cmd_check_world(argv):
    """13 and 9 on a 0.29.0 world file's rows (no replay; p3_conflict_reference.py check-world replays it)."""
    pcr = _pcr()
    pcr.use_version(VERSION)
    root = argv[argv.index("--data") + 1] if "--data" in argv else "game/data"
    paths = [a for a in argv if not a.startswith("--") and a != root]
    dat = pcr.data(root)
    bad = 0
    for path in paths:
        rows = _p502().rows_from_db(path)
        meta = rows["meta"]
        c = Checker()
        c.eq("%s schema_version" % path, meta.get("schema_version"), "13")
        conn = sqlite3.connect(path)
        mining = {rid: dat.race[rid]["mining"] for rid, in conn.execute("SELECT id FROM race").fetchall()
                  if rid in dat.race}
        delving = {rid: dat.race[rid].get("delving", 0) for rid in mining}
        present = {rid for rid, pr in conn.execute("SELECT id, present FROM race").fetchall() if pr == 1}
        conn.close()
        present &= {cv[2] for cv in rows["civilization"]}
        E = int(meta.get("history_end_year", meta.get("param.history_span", "6000")))
        borders = [(r[0], r[1]) for r in rows["region_border"]]
        for v in violations(rows, borders, dat.base.monsters_tuning, mining, E, present, delving):
            c.ok("%s invariant %s" % (path, v), False)
        for v in reader_problems(rows):
            c.ok("%s reader %s" % (path, v), False)
        print("%s: %s; %d checks, %d failures" % (path, " ".join("%s=%d" % kv for kv in stats_of_tables(rows)),
                                                  c.count, c.failures))
        bad += c.failures
    return 1 if bad else 0


GOLDEN_HEADER = """\
# P5-07 16: golden values of the mountain depths (generator 0.29.0, schema 13), written by
# worldgen/tests/reference/p5_07_depths_reference.py golden (the test writer's reference, never from C++ output).
# Lines: case <name> <seed> <size> <data>; table <name> <sha256 of repr rows>; stat <key> <value>.
"""


def golden_text(inputs):
    pcr = _pcr()
    import p3_chronology_reference as pchr
    pcr.use_version(VERSION)
    o = [GOLDEN_HEADER.rstrip("\n")]
    cases = [("depths-small-%d-512" % s, s, 512, os.path.join(inputs, "depths-%d-512.db" % s), "depths-small")
             for s in range(10)] + [("shipped-42-1024", 42, 1024, os.path.join(inputs, "geo-seed-42-1024.db"), "shipped")]
    for name, seed, size, path, what in cases:
        if not os.path.exists(path):
            print("missing input %s" % path, file=sys.stderr)
            continue
        dat = depths_small_data(pcr.data()) if what == "depths-small" else pcr.data()
        conn = pchr.open_world(path)
        p = pchr.params(span=6000, races=pchr.races_of("all", dat.chron), detail="normal", civs=pp.civs_param("auto"),
                        intensity="normal")
        inp, _chron = pcr.inputs_for(dat, conn, p, seed, size)
        conn.close()
        res = pcr.history_stage(dat, inp)
        o.append("case %s %d %d %s" % (name, seed, size, what))
        for t in ("site", "monster", "event", "event_value"):
            o.append("table %s %s" % (t, pchr.table_hash(pcr.COLUMNS[t], res[t])))
        for k, v in stats_of_tables(res):
            o.append("stat %s %d" % (k, v))
    return "\n".join(o) + "\n"


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    cmd = argv[0]
    if cmd == "units":
        c = Checker()
        check_units(c)
        check_data(c)
        check_render(c)
        print("%d checks, %d failures" % (c.count, c.failures))
        return 1 if c.failures else 0
    if cmd == "fixtures":
        return cmd_fixtures(argv[1:])
    if cmd == "check-world":
        return cmd_check_world(argv[1:])
    if cmd == "readings":
        print(READINGS)
        return 0
    if cmd == "golden":
        inputs = argv[argv.index("--inputs") + 1] if "--inputs" in argv else None
        if inputs is None:
            print("golden needs --inputs DIR", file=sys.stderr)
            return 2
        rest = [a for a in argv[1:] if not a.startswith("--") and a != inputs]
        text = golden_text(inputs)
        path = rest[0] if rest else GOLDEN
        if path == "-":
            sys.stdout.write(text)
        else:
            with open(path, "w", newline="\n") as f:
                f.write(text)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.modules.setdefault("p5_07_depths_reference", sys.modules["__main__"])
    sys.exit(main(sys.argv[1:]))
