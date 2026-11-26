#!/usr/bin/env python3
"""Reference of P5-07 "Mountain depths" (docs/specs/p5-07-mountain-depths.md r1, generator 0.29.0, schema 13, history
report format v10), written by the test writer from the spec alone, without the engineer's branch.

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
STREAMS = ("hist.deep.delve", "hist.deep.wake", "hist.deep.kind", "hist.deep.gate", "hist.deep.take")
STREAM_COUNT = 103
KINDS = {93: "delving_deepened", 94: "deep_woken", 95: "deep_sealed", 96: "hold_lost"}
DEEP_KIND_IDS = (7, 8, 9)   # 4.1: ymrach 7, ysgarth 8, zhakul 9 (invariant 8)
WAKE_DEEP = 3
CIV, SETTLEMENT, MONSTER, SITE = pe.CIV, pe.SETTLEMENT, pe.MONSTER, pe.SITE
AWAKE, ASLEEP, SLAIN, DIED = 1, 2, 3, 4
LAIR, MINE = 1, 3
OPEN, END_LOST = 0, 4
NEVER = 1 << 40   # a sealed monster's drive_until: it never wakes again (6.5)
HEADER = "# worldgen history v10"
STATS_KEYS = ("mines_deepened", "mines_deepest", "deep_woken", "deep_sealed", "holds_lost", "deep_flights",
              "realms_lost_to_deep", "deep_alive")   # 10 item 7, in this order
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

    # --- 6.1 steps
    def t4(self):
        self.deepen(quiet=False)
        self.waking()

    def q4(self):
        self.deepen(quiet=True)

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

    def last_refuge(self, c, tt):
        """D-R7: P3-05 r12 22.7's predicate."""
        w = self.w
        return w.race_last_refuge(c) and [x["id"] for x in w.standing_of(c)] == [tt["id"]]

    def wake(self, s, d, c, tt, k):
        w, t, m = self.w, self.t, self.m
        p502 = _p502()
        Y = w.year
        sid = s["id"]
        kd = m.kind(k)
        mid = len(m.monsters) + 1
        if mid > p502.MONSTERS_MAX:
            pe.fatal("history: bound: monsters exceed 100000")
        name, root = m.monster_name(k, mid)
        # 6.5 the gate, decided before any event
        P = tt["population"]
        S = gate_strength(P, t["deep_gate_levy_permille"], m.prowess(w.race_c(c)))
        odds = gate_odds(S, kd["might"], t["deep_seal_max_permille"])
        u = pnr.pick(w.draw("hist.deep.gate", mid, 0, 0), 1000)
        fall = not sealed_at(u, odds)
        forced = False
        if fall and self.last_refuge(c, tt):
            _dead, V = take_toll(P, t["deep_toll_permille"])
            if self.flight_plan(c, tt, V, s["region"]) is None:
                fall, forced = False, True   # "there was nowhere to flee"
        lid = len(m.sites) + 1
        if lid > p502.SITES_MAX:
            pe.fatal("history: bound: sites exceed 1000000")
        lname = m.lair_name(k, lid, root)
        e = w.append("deep_woken", region=s["region"], settlement=tt["id"],
                     participants=(("monster", MONSTER, mid), ("civilization", CIV, c), ("site", SITE, sid),
                                   ("lair", SITE, lid)),
                     values=(d, telling_woken(mid)), causes=(self.last_delve[sid],))
        m.sites.append(dict(id=lid, name=lname, kind=LAIR, region=s["region"], x=tt["x"], y=tt["y"],
                            settlement=tt["id"] if fall else 0, founder=0, monster=mid, origin_year=Y, origin_event=e,
                            ended_year=None, ending_event=0, ending=OPEN, hostile=None))
        m.monsters.append(dict(id=mid, kind=k, name=name, root=root, status=AWAKE, born=Y, born_event=e, lair=lid,
                               waking=e, sleeping=0, drive_until=0, hoard=0, holds=False, raids={}, died=None,
                               death_event=0, slayer=0, overrun=0, raid_years={}, place_raids={}, dead_kind=False))
        self.woken.add(sid)
        self.waking_realm[mid] = c
        self.takes[mid] = 0
        self.woken_event[mid] = e
        self.log.append((Y, "woken", sid, mid, k, d, S, odds, u, forced))
        if not fall:
            mon = m.monsters[mid - 1]
            e2 = w.append("deep_sealed", region=s["region"], settlement=tt["id"],
                          participants=(("civilization", CIV, c), ("monster", MONSTER, mid), ("site", SITE, sid),
                                        ("lair", SITE, lid)),
                          values=(telling_sealed(mid),), causes=(e,))
            mon["status"], mon["sleeping"], mon["drive_until"] = ASLEEP, e2, NEVER
            self.sealed.add(mid)
            self.sealed_site.add(sid)
            self.log.append((Y, "sealed", sid, mid, e2, forced))
            return
        took = self.take(mid, tt, e)
        assert took, "6.5: a fall that cannot happen is a seal"

    # --- 6.6 the take
    def take(self, mid, tt, cause):
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
        e = w.append("hold_lost", region=tt["region"], settlement=tt["id"], participants=parts,
                     values=(dead, V, telling_lost(mid, tt["id"])), causes=(cause,))
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


def violations(res, borders, tuning, mining_of_race, E, races_present=None):
    """13 items 1 to 9 and 11's deep part on rows. tuning: history.monsters (the deep fields); mining_of_race: race id
    -> mining_permille; races_present: the race ids that awoke (for item 6), or None."""
    x = X(res, borders)
    r = x.r
    out = []
    t = tuning
    deep = deep_kind_set(res)
    site = {s[0]: s for s in r.sites}
    mon = {m[0]: m for m in r.monsters}
    year = lambda e: r.ev[e][1]   # noqa: E731
    quiet = lambda e: r.phase.get(r.ev[e][4], (0, 0, 0))[2] == 2   # noqa: E731
    sealed_of_m = {x.one(e, "monster"): e for e in x.of("deep_sealed")}
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
    for e in woken:
        sid, mid, lid = x.one(e, "site"), x.one(e, "monster"), x.one(e, "lair")
        by_site.setdefault(sid, []).append(e)
        s, m, lair = site.get(sid), mon.get(mid), site.get(lid)
        d = x.val(e, "depth")
        if quiet(e):
            out.append("2: deep_woken e%d in a quiet step" % e)
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
            out.append("2: e%d telling" % e)
        if r.causes.get(e, []) != [max([y for y in per_site.get(sid, []) if y < e] or [0])]:
            out.append("2: deep_woken e%d causes %s" % (e, r.causes.get(e, [])))
        # 3
        nxt = [y for y in sorted(r.ev) if y > e and r.ev[y][1] == year(e) and names_m(y, mid) and
               x.r.kind_of(y) in ("deep_sealed", "hold_lost")]
        between_others = [y for y in sorted(r.ev) if e < y < (nxt[0] if nxt else e) and
                          any(p[2] == MONSTER and p[3] != mid for p in r.parts.get(y, []))]
        outcome = [y for y in nxt if r.causes.get(y, []) == [e]]
        if len(outcome) != 1 or between_others:
            out.append("3: deep_woken e%d outcome %s" % (e, outcome))
        elif lair is not None:
            k = x.r.kind_of(outcome[0])
            if k == "deep_sealed" and lair[6] != 0:
                out.append("3: sealed e%d lair %d holds settlement %d" % (outcome[0], lid, lair[6]))
            if k == "hold_lost" and lair[6] != r.ev[e][6]:
                out.append("3: fall e%d lair %d settlement %d, the hold %d" % (outcome[0], lid, lair[6], r.ev[e][6]))
    for sid, es in by_site.items():
        if len(es) > 1:
            out.append("2: mine %d woke %d times" % (sid, len(es)))
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
    # 5 and 9
    realm = {x.one(e, "monster"): x.one(e, "civilization") for e in woken}
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
        if x.val(e, "telling") != telling_lost(mid, sid):
            out.append("5: e%d telling" % e)
        cs = r.causes.get(e, [])
        first = count[mid] == 1
        if first:
            if cs != [y for y in woken if x.one(y, "monster") == mid][:1]:
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
    # 8
    mk = res.get("monster_kind", [])
    if [k[1] for k in mk][:6] != ["dragon", "giant", "ogre", "troll", "undead", "wyvern"] or \
            [(k[0], k[1], k[5]) for k in mk][6:9] != [(7, "ymrach", 3), (8, "ysgarth", 3), (9, "zhakul", 3)]:
        out.append("8: monster_kind rows %s" % [(k[0], k[1], k[5]) for k in mk])
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
    seen_depth = {}
    woken_site = {}
    woken_m = {}
    first_lost = set()
    sites = {s[0]: s for s in r.sites}
    for e in sorted(r.ev):
        k = r.kind_of(e)
        if k not in KINDS.values():
            continue
        for key in ("depth", "telling", "dead", "survivors"):
            if key in r.vkeys.get(r.ev[e][3], {}):
                v = x.val(e, key)
                lo, hi = ranges.get(key, (0, None))
                if v < lo or (hi is not None and v > hi):
                    E(e, "value %s %d outside %d to %s" % (key, v, lo, hi if hi is not None else "max"))
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
            ("deep_alive", sum(1 for m in r.monsters if m[2] in deep and m[9] in (AWAKE, ASLEEP)))]


def mine_depth_at(t, site, year):
    """11.1 Timeline::mine_depth_at (9's derivation): the largest depth of the site's kind 93 events of year <= Y, 0
    none or a site of another kind."""
    x = X(t, [])
    s = [y for y in x.r.sites if y[0] == site]
    if not s or s[0][2] != MINE:
        return 0
    return max([x.val(e, "depth") for e in x.of("delving_deepened") if x.one(e, "site") == site and
                x.r.ev[e][1] <= year] or [0])


def sealed_year(t, monster):
    """11.1 Timeline::sealed_year: the year of the monster's deep_sealed, -1 none."""
    x = X(t, [])
    for e in x.of("deep_sealed"):
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
    c.eq("4.2 fields after undead_regions_ref, in order", list(mo)[-9:],
         ["undead_regions_ref"] + [n for n, *_ in pdr.MONSTERS_NEW_029])
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
             (MONSTER, 1): "Bolgrim the Deep-Coiled", (MONSTER, 2): "The Swarm Beneath Durak", (pe.REGION, 7): "Iron Downs"}
    kinds_of = {(MONSTER, 1): "a wyrm of the lightless deep", (MONSTER, 2): "a host of eyeless things from under the stone"}

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
    c.eq("8.1 kinds 93 to 96", [(k["id"], k["key"], k["caused"], k["region_use"], k["settlement_use"])
                                for k in pe.COMPILED_029[-4:]],
         [(93, "delving_deepened", 1, 1, 1), (94, "deep_woken", 1, 1, 1), (95, "deep_sealed", 1, 1, 1),
          (96, "hold_lost", 1, 1, 1)])
    c.eq("8.1 hold_lost roles", [(r["role"], r["type"], r["min"], r["max"]) for r in pe.COMPILED_029[-1]["roles"]],
         [("monster", 8, 1, 1), ("civilization", 3, 1, 1), ("lair", 9, 1, 1), ("site", 9, 0, 1)])
    c.eq("8.1 values", [k["values"] for k in pe.COMPILED_029[-4:]],
         [["depth", "telling"], ["depth", "telling"], ["telling"], ["dead", "survivors", "telling"]])
    c.eq("9 schema 13", pe.SCHEMA_VERSION, 13)
    c.eq("9 wake codes", pe.WAKE_CODE, {"wild": 1, "dead": 2, "deep": 3})


# ---------------------------------------------------------------------------
# Replays: depths-small on the conflict stress geographies (fixtures), world files (check-world), the golden file
# ---------------------------------------------------------------------------

FIXTURE_SEEDS = (0, 1, 2, 3, 5, 6, 7, 9, 12, 13, 14)


def run_stress(seed, dat):
    pcr = _pcr()
    case = pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
    inp, _g = pcr.synthetic_inputs(seed, dat, case)
    return pcr.history_stage(dat, inp), inp


def res_problems(res, inp, dat):
    """The invariants of 13 and P5-02's (with 13 item 11) and the reader checks on a run."""
    w = res["_world"]
    borders = [(b["a"], b["b"]) for b in inp.borders]
    mining = {rid: v["mining"] for rid, v in w.pd.race.items()}
    present = {r[0] for r in inp.races if r[9] == 1 and any(c["race"] == r[0] for c in w.civs)}
    out = violations(res, borders, w.monsters.t, mining, inp.params["span"], present)
    out += ["P5-02 " + v for v in _p502().res_violations(res, w, quarter=False)]
    out += ["reader " + v for v in reader_problems(res)]
    return out


def cmd_fixtures(argv):
    pcr = _pcr()
    pcr.use_version(VERSION)
    quick = "--quick" in argv
    c = Checker()
    dat = depths_small_data()
    seen, outcomes = set(), {}
    for seed in FIXTURE_SEEDS[:4] if quick else FIXTURE_SEEDS:
        res, inp = run_stress(seed, dat)
        w = res["_world"]
        for v in res_problems(res, inp, dat):
            c.ok("depths-small stress-%d %s" % (seed, v), False)
        log = w.monsters.deep.log
        for x in log:
            seen.add(x[1])
        seen.update("forced seal" for x in log if x[1] == "sealed" and x[5])
        seen.update("flight" for x in log if x[1] == "take" and x[7])
        seen.update("later take" for x in log if x[1] == "take" and
                    any(w.events[cc - 1]["key"] == "monster_raid" for cc in w.events[x[-1] - 1]["causes"]))
        seen.update("realm lost" for e in w.events if e["key"] == "civ_ended" and
                    any(w.events[cc - 1]["key"] == "hold_lost" for cc in e["causes"]))
        st = dict(stats_of_tables(res))
        outcomes[seed] = st
        print("stress-%d %s" % (seed, " ".join("%s=%d" % kv for kv in stats_of_tables(res))))
    for path in ("deepened", "woken", "sealed", "take", "later take"):
        c.ok("depths-small shows the path '%s' (seen: %s)" % (path, ", ".join(sorted(seen))), path in seen)
    for path in ("flight", "forced seal", "no take", "realm lost"):
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
        present = {rid for rid, pr in conn.execute("SELECT id, present FROM race").fetchall() if pr == 1}
        conn.close()
        present &= {cv[2] for cv in rows["civilization"]}
        E = int(meta.get("history_end_year", meta.get("param.history_span", "6000")))
        borders = [(r[0], r[1]) for r in rows["region_border"]]
        for v in violations(rows, borders, dat.base.monsters_tuning, mining, E, present):
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
