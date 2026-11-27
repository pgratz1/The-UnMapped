#!/usr/bin/env python3
"""Reference of P5-04 "Alliances and trade" (docs/specs/p5-04-alliances-trade.md r2, approved 2026-11-24, scope C),
written from the spec alone, without the engineer's branch (p5/04-impl).

What this module holds (phase 1):
  - the formulas of 6 and 7 as functions (warmth, bloc share, the basis order, the formation, call, trade, mine and
    grant chances, the spoils and their shares, the principal's need, the withdrawal test, the casualty split of an
    allied battle, deterrence, the fear of blocs, the allied strain, the strength factors of 7.5, mine names, trade
    credit periods) with hand-checked units (`units`);
  - a row model of a world's history (AW) and, on it, the invariants of 13 that rows can show (1, 4 to 7, 8 as
    AL9's hard bound, 11 to 14, and 10's shelter rule), the reader checks of 9.1 and 9.2 (their messages), the
    [stats] keys of 10.5, the [alliances] block of 10.2, the civilization lines of 10.3, the mine lines of 10.6,
    the timeline reads of 12 and its dump lines, the browser groups of 11.2 and the mine marker of 11.3:
    `check-world`, `stats`, `block`, `timeline` on world files (any generated world: C++ or reference);
  - the golden file worldgen/tests/golden/p5-04-alliances.txt (17): `golden` (integration only, from the world
    cases' files built by this reference chain; never from the C++ implementation's output alone).

  - the replay of sections 6 and 7 (Alliances, installed in p3_conflict_reference.World, whose seams of 7 it uses),
    the fixtures alliances-small and zero, I0 by replay (`fixtures`, `i0-world`), check-world's invariants on the
    replayed histories (`aw_of_res`, `res_problems`). REPLAY_TODO names the entry points.

Commands (from the repository root):
  python3 -B worldgen/tests/reference/p5_04_alliances_reference.py units
  python3 -B worldgen/tests/reference/p5_04_alliances_reference.py check-world DB [DB ...] [--data ROOT]
  python3 -B worldgen/tests/reference/p5_04_alliances_reference.py stats DB
  python3 -B worldgen/tests/reference/p5_04_alliances_reference.py block DB [--level summary|full]
  python3 -B worldgen/tests/reference/p5_04_alliances_reference.py timeline DB YEAR
  python3 -B worldgen/tests/reference/p5_04_alliances_reference.py golden [PATH|-] --inputs DIR
  python3 -B worldgen/tests/reference/p5_04_alliances_reference.py fixtures [--quick]
  python3 -B worldgen/tests/reference/p5_04_alliances_reference.py i0-world --inputs DIR CASE... [--with-off]
  python3 -B worldgen/tests/reference/p5_04_alliances_reference.py readings

Python 3.11 or newer, standard library only, integers only.
"""

import hashlib
import os
import sqlite3
import struct
import sys
import tomllib

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

REPO = os.path.normpath(os.path.join(_HERE, "..", "..", ".."))
GOLDEN = os.path.join(REPO, "worldgen", "tests", "golden", "p5-04-alliances.txt")
GENERATOR_VERSION = "0.27.0"   # P5-06 phase 2: 0.27.0 worlds (schema 12)
SCHEMA_VERSION = 12

# 3: appended to streams.hpp after hist.relations.settlers, in this order
STREAMS = ("hist.alliances.form", "hist.alliances.join", "hist.alliances.trade", "hist.alliances.spoils",
           "hist.alliances.mine")
# 8.1: the new kinds by id (provisional ids, the next free ones at merge)
KINDS = ((63, "alliance_formed"), (64, "alliance_renewed"), (65, "alliance_lapsed"), (66, "alliance_broken"),
         (67, "alliance_betrayed"), (68, "ally_joined_war"), (69, "ally_refused"), (70, "ally_withdrew"),
         (71, "allied_battle"), (72, "trade_opened"), (73, "trade_broken"), (74, "spoils_shared"),
         (75, "spoils_withheld"), (76, "alliance_spurned"), (77, "mine_opened"), (78, "arms_sold"))
KIND_ID = {k: i for i, k in KINDS}
ORIGIN, COMMON_ENEMY, FEAR, FRIENDSHIP, TRADE = 0, 1, 2, 3, 4          # basis codes (8.1)
BASIS_ORDER = (COMMON_ENEMY, FEAR, TRADE, FRIENDSHIP)                   # 6.2: the first reason present
BASIS_WORDS = {0: "by origin", 1: "against a common enemy", 2: "in fear", 3: "in friendship", 4: "after trade"}
DEFENCE, ATTACK = 0, 1                                                  # side (8.1)
SITE_LAIR, SITE_OUTPOST, SITE_MINE = 1, 2, 3                            # 9.2
ENDING_LOST, ENDING_FELL = 4, 5                                         # 9.2 (P5-02 11.3)
SUMMARY_ALLIANCES_MAX = 10                                              # 10.2 (digest.hpp)
# 11.2: browser groups (P4-05 4.4 amended): alliances = 7 appended, ALL_GROUPS 0xFF
GROUP_SETTLEMENTS, GROUP_WAR, GROUP_ALLIANCES = 1, 2, 7
ALL_GROUPS = 0xFF
GROUP_OF_NEW = dict([(k, GROUP_ALLIANCES) for k in range(63, 71)] + [(71, GROUP_WAR)] +
                    [(k, GROUP_ALLIANCES) for k in range(72, 77)] + [(77, GROUP_SETTLEMENTS), (78, GROUP_ALLIANCES)])
# 11.3: the mine marker (P4-04 entry type 6 site, glyph code 16 + kind)
MARKER_SITE_TYPE, MINE_GLYPH = 6, 19
MINE_GLYPH_COLOUR = (60, 60, 60)
BATTLE_KINDS = ("battle", "battle_at_pass", "allied_battle", "battle_led")   # 11.1; P5-06 11.1 battle_led
CIV, SITE, SETTLEMENT, REGION, RACE = 3, 9, 4, 5, 1
THRIVING, DECLINING, RUINED, FADED = 1, 2, 3, 4
OPENING, QUIET, FLARE, FINAL = 1, 2, 3, 4   # P3-04 6.1 (phase.kind)

REPLAY_TODO = """\
The replay of P5-04 inside p3_conflict_reference.py's World (written in phase 1's second part):
  - Alliances (this module): installed after World.race_relations in T2, T6 and Q4 (6.1's order), the cache of
    6.2, the log cursor and, at each war_declared, betrayal (6.6), trade broken by war (6.8) and the calls of 6.5;
    keeping (6.7: cooling, blocs too great, renewal), origin alliances (6.4), formations (6.3), trade breaks,
    openings and credits (6.8, 6.9), the ends of joined wars (spoils of 6.11, the ally-enemy credits of 6.9),
    mines through the monsters' site list (6.12, 7.16) and arms (6.13); every credit through
    World.race_relations.credit at its fixed point;
  - conflict's seams of 7 in World: allied strain (strain_update, 7.2), bloc fear (fear, 7.3), deterrence (deterred,
    in odds and strength, 7.4), the three factors in force and local_force (strengthened, 7.5), join_war and the
    ally records, open_wars_of, at_war (7.6), allied battles in fight (side_forces, allied_casualties, 7.7), the
    score's side dead (7.8), withdrawal (withdrawals, 7.9), ends with allies in end_war and give_up (allies_at_end,
    7.10) and at a side's end in housekeeping and conquer (allies_ended, allies_at_side_end, 7.11), plunder and
    tribute recorded for war_spoils, add_grudge (7.15);
  - I0 (7.14): `fixtures` (stress seeds, against no subsystem and against 0.25.0) and `i0-world` (world cases);
    alliances-small (18.3): `fixtures` (every kind of 63 to 78; check-world's invariants on its histories).
Not replayed: the mine glyph's drawing (11.3, the lead's ruling 13: refused until the engineer's bitmap is in the
spec)."""

READINGS = """\
P1 (10.2) "largest bloc: <bs>X" prints the share as `<bs> permille`: the spec's per-mille sign is not ASCII, and
   the report is ASCII text (C++ lint rule non-ascii-literal; no report line of P3-07 to P5-03a prints the sign).
P2 (10.2) an alliance line's `; joined <j>, refused <r>` part is printed when j + r > 0 (the spec marks it optional
   without saying when); `; renewed <k>` when k > 0; the spoils part when either count is not 0 (as stated).
P3 (10.2) `<to|E>`: an alliance or trade row still open at the end (until NULL) prints the letter E.
P4 (10.2) the ending part names the event that closed the row: lapsed (65), broken (66), spurned (76), betrayed
   (67), else `ended` (a side's ending event).
P5 (10.5) "standing at E", alliances_standing, trade_standing, mines_open: rows still open (until / ended_year
   NULL). arms_buyers_at_end reads the rows in force at E - 1 (as stated).
P6 (6.11) plunder of a sack = the sacked event's dead + survivors (the population at the fall before its toll).
P7 (13 item 8) the end of a T2 or Q4 is not visible in rows: check-world applies AL9's hard bound (the longest run
   of years with a bloc share above 800 permille <= 1000) and reports the longest run above bloc_lapse_share.
P8 (6.12) a mine form whose {settlement} is not at its start lowers a leading `The ` of the town's name; at its
   start the name is kept (no shipped form puts {settlement} first).
P9 (10.2) alliance lines and trade lines are ordered by (from, a, b); `wars with an ally: <k> of <n>` counts
   war_declared events, n all of them.
P10 (10.3) `  allies: <n>` is printed in every civilization block, 0 included; at summary `; at the end: ...` only
   when the realm has allies at E (rows still open).
P12 (10.5) alliances_years_median: the lower middle of the sorted lengths (until - since) of the ended alliances.
P11 (4.1) race_origin_allies' last message names the listed race twice: `origin_allies '<r>' is not returned by race
   '<r>'` with r the race listed (the one that does not list back).
The replay's readings (sections 6 and 7 where they leave an order, a tie or a key open; integration compares these):
A1 (7.4) deterrence also enters P3-06 24.2's strength (the quarrel hold), which is defined as 20.6 item 3's odds.
A2 (6.2, 5) the per-step cache holds the presences and shares (so reach, hops and the bloc shares built from them)
   and the sellers of 6.13; dispositions, warmth, "at war", the alliances and trades are read live.
A3 (7.10) a war ended by search_abandoned (P3-06 20.17, "the truce bookkeeping") ends its allies as a truce does.
A4 (7.9) withdrawal is tested in T6 right after the war's battle step and before 7.7's checks (21.6's goal ending
   included), for the wars that step fights (not in a war's declaration year).
A5 (7.15) the sack of a fleeing last refuge (P3-06 20.1) is plunder: its population before the toll.
A6 (7.7) an ally's dead in the war += its share d_S x F_m div F_S as computed (not the casualties taken under
   20.20's floor); casualties: the winners' allies ascending, the winner, then the losers' allies, the loser.
A7 (7.1) the AllianceView answers from the subsystem's records until it drops them at its next step (6.7's last
   paragraph): a trade with a realm that ended earlier in the step still counts for conflict until then.
A8 (6.7, 6.8, 6.9, 6.13) an alliance or trade with an ended side whose row conflict has not yet closed (a realm
   ended in the same step after conflict's housekeeping) is left alone until then.
A9 (6.3, 6.4, 6.8 with P3-04 6.7's "opened twice before one bookkeeping"; a spec gap) a pair whose alliance (or
   trade) row closed earlier in the same step forms (opens) none again in that step: else a refusal that breaks
   at the declaration and the formation that follows in the same T2 would open the row twice.
A10 (6.2 fear) the cause of a feared bloc whose head holds no region is that realm's founding event.
A11 (18.3) alliances-small: the stress cases with every race (`all`), seeds SMALL_SEEDS; its first three seeds
   hold every kind of 63 to 78.
A12 (6.12) "c's standing settlement in R": every standing settlement of c in R (hidden ones included), those that
   never had a mine first, then the largest population, then the smaller id.
A13 (6.11) the enemy's end closes the ally's record as the war's end (owed); the partner's end leaves the enemy
   the survivor (owed nothing); two ended principals: winner 0, end_event the smaller id's ending event.
A14 (6.9) the ally-enemy credit of a joined record is made once the war or the record is closed, even while its
   spoils wait for the principal's quiet budget; its event is the record's closing event.
A15 (6.2 common enemy) "its year" is the war's declaration year; "open" the war's state in conflict.
A16 (7.11) at a side's end: the records of ended allies close first, then the wars of the ended sides.
A17 (6.8) a trade broken by a betrayal's war cites the declaration (6.8's rule), not the alliance_betrayed.
A18 (6.7 item 2) blocs too great: the civilizations in an open alliance, ascending; in Q4 a shedding whose pair has
   no budget stops c's pass (it waits).
A19 (6.5) the chance of a call and its draw are evaluated only when every condition holds; the draw's key is
   (w, z, 0) for both sides."""


def floor_div(a, b):
    assert b > 0
    return a // b


def clamp(v, lo, hi):
    return min(hi, max(lo, v))


# ---------------------------------------------------------------------------
# Formulas (sections 6 and 7)
# ---------------------------------------------------------------------------

def warmth(d, mn, full):
    """6.2: w(x, y); 0 below ally_min_disposition."""
    if d < mn:
        return 0
    return clamp((d - mn) * 1000 // (full - mn), 0, 1000)


def bloc_share(s_c, s_allies):
    """6.2: bs(c) = min(1000, s(c) + sum of the allies' shares)."""
    return min(1000, s_c + sum(s_allies))


def basis_of(reasons):
    """6.2: the first reason present in the order common enemy, fear, trade, friendship (None when none)."""
    for b in BASIS_ORDER:
        if b in reasons:
            return b
    return None


def form_chance(reasons, w, t):
    """6.3: p = min(form_max_permille, m x w div 1000)."""
    m = ((t["form_foe_permille"] if COMMON_ENEMY in reasons else 0) +
         (t["form_fear_permille"] if FEAR in reasons else 0) +
         (t["form_trade_permille"] if TRADE in reasons else 0) +
         (t["form_friend_permille"] if FRIENDSHIP in reasons else 0))
    return min(t["form_max_permille"], m * w // 1000)


def bloc_test(bs_x, s_x, allies_x, bs_y, s_y, allies_y, bloc_max):
    """6.3: both blocs after forming at most bloc_share_max (bs live; the sums not clamped)."""
    return bs_x + s_y + sum(allies_y) <= bloc_max and bs_y + s_x + sum(allies_x) <= bloc_max


def defence_chance(defend, join, w_zp, force_z, force_p, force_e):
    """6.5: a defence call, floor division left to right."""
    sigma = min(1000, (force_z + force_p) * 1000 // max(1, force_e))
    return defend * join // 1000 * w_zp // 1000 * sigma // 1000


def attack_chance(attack, join, d_ze, hostility_full):
    """6.5: an attack call (made only when d(z, e) < 0)."""
    assert d_ze < 0
    h = min(1000, -d_ze * 1000 // hostility_full)
    return attack * join // 1000 * h // 1000


def trade_weight(d, tmin, full):
    """6.8: wt, for d >= trade_min_disposition."""
    return clamp((d - tmin) * 1000 // (full - tmin), 0, 1000)


def trade_chance(trade_permille, wt, quiet_length=None, one_seller=False, arms_trade_permille=1000):
    """6.8 and 6.13: T2 (quiet_length None) or Q4 of length L; the boost for a pair with exactly one seller."""
    p = trade_permille * wt // 1000
    if quiet_length is not None:
        p = min(1000, p * quiet_length)
    if one_seller:
        p = min(1000, p * arms_trade_permille // 1000)
    return p


def mine_chance(mine_permille, mining_permille, quiet_length=None):
    """6.12."""
    p = mine_permille * mining_permille // 1000
    return p if quiet_length is None else min(1000, p * quiet_length)


def spoils_value(tribute_people, plunder, plunder_permille):
    """6.11: V(w)."""
    return tribute_people + plunder * plunder_permille // 1000


def spoils_shares(V, deads, dead_S, floor_pm, cap_pm):
    """6.11: [(part, share)] for the owed allies in join order (deads: each one's dead)."""
    out, given = [], 0
    for d in deads:
        part = clamp(d * 1000 // max(1, dead_S), floor_pm, cap_pm)
        share = min(V * part // 1000, V - given)
        given += share
        out.append((part, share))
    return out


def principal_need(own_dead, pop0, exhaustion_permille, muster_permille):
    """6.11: ex(W)."""
    return min(1000, own_dead * 1000 // max(1, exhaustion_permille * max(1, pop0 * muster_permille // 1000) // 1000))


def grant_chance(w, part, ex, t, temper=0):
    """6.11: g; P5-06 7.3: + temper_spoils_permille[tau(W)] inside the clamp (temper)."""
    return clamp(t["spoils_grant_permille"] + t["spoils_warmth_permille"] * w // 1000 +
                 t["spoils_part_permille"] * part // 1000 - t["spoils_need_permille"] * ex // 1000 + temper,
                 t["spoils_grant_min"], t["spoils_grant_max"])


def withdraws(dead, pop0, exhaustion_permille, muster_permille):
    """7.9: the ally's test."""
    return dead * 1000 >= exhaustion_permille * max(1, pop0 * muster_permille // 1000)


def side_force(principal_local, ally_locals, effort):
    """7.7: [F_principal, F_ally...] and F_S."""
    fs = [principal_local] + [f * effort // 1000 for f in ally_locals]
    return fs, sum(fs)


def split_dead(d_S, F_principal, F_allies):
    """7.7: each ally (ascending id, F_m > 0) takes d_S x F_m div F_S; the principal the rest.
    F_allies: [(id, F_m)]. Returns (principal's dead, {ally: dead})."""
    F_S = F_principal + sum(f for _, f in F_allies)
    out = {}
    for z, f in sorted(F_allies):
        if f > 0:
            out[z] = d_S * f // F_S
    return d_S - sum(out.values()), out


def deterred_force(force_b, ally_forces, deterrence, defence_permille):
    """7.4: fb (ally_forces: the forces of b's allies z != a, not allied with a, reaching a)."""
    return (force_b + deterrence * sum(ally_forces) // 1000) * defence_permille // 1000


def bloc_fear(fear_weight, bs, fear_share):
    """7.3: fear(b) on the bloc share."""
    return fear_weight * max(0, bs - fear_share) // (1000 - fear_share)


def allied_rate(rate, allied_strain_permille):
    """7.2: an allied pair's strain rate."""
    return rate * allied_strain_permille // 1000


def strengthened(f, partners, forged, arms, c):
    """7.5: the three factors, in order, each with floor division (c: the history.conflict fields)."""
    f = f * (1000 + c["trade_strength_permille"] * min(partners, c["trade_strength_max"])) // 1000
    f = f * (1000 + c["forge_strength_permille"] * (1 if forged else 0)) // 1000
    f = f * (1000 + c["arms_strength_permille"] * (1 if arms else 0)) // 1000
    return f


def mine_name(forms, site_id, settlement_name):
    """6.12: mine_forms[(id - 1) mod n] with {settlement}; a leading `The ` of the town lowered (reading P8)."""
    f = forms[(site_id - 1) % len(forms)]
    at = f.index("{settlement}")
    nm = settlement_name
    if at > 0 and nm.startswith("The "):
        nm = "the " + nm[4:]
    return f.replace("{settlement}", nm)


def trade_credit_periods(Y, next_year, period, quiet_length=None):
    """6.9: k (0 when Y < next) and the next credit year."""
    if Y < next_year:
        return 0, next_year
    k = 1 if quiet_length is None else 1 + (Y + quiet_length - 1 - next_year) // period
    return k, next_year + k * period


def renewal_due(Y, renewal_year, term_years, quiet_length=None):
    """6.7 item 3: Y >= renewal + term (Q4: Y + L - 1 >= it)."""
    last = Y if quiet_length is None else Y + quiet_length - 1
    return last >= renewal_year + term_years


def in_reach(hops, limit):
    """6.2: reach (None: unreachable; limit -1: never)."""
    return hops is not None and limit >= 0 and hops <= limit


def trade_out_of_reach(hops, trade_hops):
    """6.8: broken when hops > trade_hops + 1 (a margin of one border)."""
    return hops is None or hops > trade_hops + 1


# ---------------------------------------------------------------------------
# P5-04a (generator 0.28.0): the formulas of 3.3, 4.2, 5 and 6.3
# ---------------------------------------------------------------------------

STREAM_RENEW = "hist.alliances.renew"   # 2: the 98th stream, key (x, y, d)
MOTIVE_WORDS = ("Loath to send its people to war", "Weary of war", "With a war of its own to fight",
                "Fearing the strength of the foe", "Grown cold toward its ally", "Grasping for its own gain",
                "Nursing a claim on its ally's land", "Remembering the spoils kept from it")   # 4.4
BETRAYAL_WORDS = ("Grasping for more", "Its friendship long cooled", "Seeing its ally weak",
                  "Coveting its ally's land")   # 5
GRASPING = 3   # FigureView::temper's code (2)


def spoils_large(share, part, large_share, large_part):
    """3.3: large(z)."""
    return share >= large_share or part >= large_part


def call_factors(t, join, weary, open_wars, sigma, w_zp, temper, claim, soured):
    """4.2: [f0, ..., f7]; t: history.alliances; join: ally_join_permille(r_z); weary: last_war_end(z)'s year >=
    Y - weary_years (an ended war); open_wars: open_wars_of(z); sigma: P5-04 6.5's; w_zp: P5-04 6.2's warmth;
    temper: FigureView::temper(z); claim: has_claim(z, p); soured: call_due and z the wronged ally."""
    fl, wf = t["join_fear_floor"], t["join_warmth_floor"]
    return [join,
            t["join_weary_permille"] if weary else 1000,
            max(0, 1000 - t["join_busy_permille"] * open_wars),
            fl + (1000 - fl) * sigma // 1000,
            wf + (1000 - wf) * w_zp // 1000,
            t["grasping_join_permille"] if temper == GRASPING else 1000,
            t["join_claim_permille"] if claim else 1000,
            t["soured_join_permille"] if soured else 1000]


def factored_chance(defend, fs):
    """4.2: p = defend_permille, then p = p x f_k div 1000 for k = 0 to 7 in order."""
    p = defend
    for f in fs:
        p = p * f // 1000
    return p


def refusal_motive(fs):
    """4.2: the code k in 1 to 7 with the smallest f_k below 1000 (the smaller code at a tie); else 0."""
    best = None
    for k in range(1, 8):
        if fs[k] < 1000 and (best is None or fs[k] < fs[best]):
            best = k
    return 0 if best is None else best


def betrayal_motive(temper, d_ab, force_a, force_b, cooled, weak):
    """5: 0 a grasping ruler, 1 a cooled friendship, 2 a weak ally, 3 land; the first that holds."""
    if temper == GRASPING:
        return 0
    if d_ab < cooled:
        return 1
    if force_b * 1000 < weak * force_a:
        return 2
    return 3


def renewal_count(s, term, end):
    """6.3: k = #{n >= 1 : s + n x term <= end - 1}."""
    return max(0, (end - 1 - s) // term) if end - 1 >= s else 0


def due_years(s, term, upto):
    """6.1: the due years d_n = s + n x term, n >= 1, up to `upto`."""
    out, n = [], 1
    while s + n * term <= upto:
        out.append(s + n * term)
        n += 1
    return out


# ---------------------------------------------------------------------------
# Units (18.1 items 2, 3, 5, 8 to 13: the formulas on hand-checked values)
# ---------------------------------------------------------------------------

class Checker:
    def __init__(self):
        self.count = 0
        self.failures = 0

    def eq(self, what, got, want):
        self.count += 1
        if got != want:
            self.failures += 1
            print("FAIL: %s: got %r, want %r" % (what, got, want))

    def ok(self, what, cond):
        self.eq(what, bool(cond), True)


def shipped_tuning():
    import p3_data_reference as pdr
    return dict(pdr.ALLIANCES_TUNING_026)


def shipped_conflict_fields():
    import p3_data_reference as pdr
    return {n: v for n, _, _, v in pdr.CONFLICT_NEW_RANGES_026}


def check_units(c):
    t = shipped_tuning()
    cf = shipped_conflict_fields()
    # warmth at its two ends and between (-300 .. 300)
    c.eq("w below min", warmth(-301, -300, 300), 0)
    c.eq("w at min", warmth(-300, -300, 300), 0)
    c.eq("w at full", warmth(300, -300, 300), 1000)
    c.eq("w above full", warmth(900, -300, 300), 1000)
    c.eq("w at 0", warmth(0, -300, 300), 500)
    c.eq("w at 61", warmth(61, -300, 300), 601)   # 361000 div 600
    c.eq("w at -299", warmth(-299, -300, 300), 1)
    # bloc share
    c.eq("bs alone", bloc_share(212, []), 212)
    c.eq("bs with allies", bloc_share(212, [100, 31]), 343)
    c.eq("bs capped", bloc_share(700, [400]), 1000)
    # the basis order
    c.eq("basis none", basis_of(set()), None)
    c.eq("basis friendship", basis_of({FRIENDSHIP}), FRIENDSHIP)
    c.eq("basis trade before friendship", basis_of({FRIENDSHIP, TRADE}), TRADE)
    c.eq("basis fear before trade", basis_of({FRIENDSHIP, TRADE, FEAR}), FEAR)
    c.eq("basis common enemy first", basis_of({FRIENDSHIP, TRADE, FEAR, COMMON_ENEMY}), COMMON_ENEMY)
    # formation chance: m x w div 1000, capped by form_max (60)
    c.eq("p friendship at full", form_chance({FRIENDSHIP}, 1000, t), 10)
    c.eq("p foe at warmth 500", form_chance({COMMON_ENEMY}, 500, t), 20)
    c.eq("p foe + fear at full: capped", form_chance({COMMON_ENEMY, FEAR}, 1000, t), 60)
    c.eq("p all four at 600", form_chance({COMMON_ENEMY, FEAR, TRADE, FRIENDSHIP}, 600, t), 60)
    c.eq("p trade + friendship at 333", form_chance({TRADE, FRIENDSHIP}, 333, t), 9)   # 30 x 333 div 1000
    c.eq("p no reason", form_chance(set(), 1000, t), 0)
    c.eq("p warmth 0", form_chance({COMMON_ENEMY}, 0, t), 0)
    # the bloc test (500): x 300 with an ally 100, y 100 with none
    c.eq("bloc test passes at 500", bloc_test(400, 300, [100], 100, 100, [], 500), True)
    c.eq("bloc test refuses at 501", bloc_test(401, 300, [101], 100, 100, [], 500), False)
    c.eq("bloc test: the other side", bloc_test(100, 100, [], 450, 400, [50], 500), False)
    # defence: 700 x 800 div 1000 = 560; x w 500 div 1000 = 280; sigma min(1000, (300 + 200) x 1000 div 1000) = 500
    c.eq("defence", defence_chance(700, 800, 500, 300, 200, 1000), 140)
    c.eq("defence sigma at cap", defence_chance(700, 800, 1000, 900, 900, 1000), 560)
    c.eq("defence enemy force 0", defence_chance(700, 600, 1000, 1, 1, 0), 420)
    c.eq("defence faerie 150 at full", defence_chance(1000, 150, 1000, 10, 10, 1), 150)
    c.eq("defence warmth 0", defence_chance(1000, 1000, 0, 10, 10, 1), 0)
    # attack: 200 x 600 div 1000 = 120; h = min(1000, 250 x 1000 div 500) = 500 -> 60
    c.eq("attack", attack_chance(200, 600, -250, 500), 60)
    c.eq("attack h at cap", attack_chance(200, 600, -700, 500), 120)
    c.eq("attack d -1", attack_chance(1000, 1000, -1, 500), 2)
    # trade: wt = (d + 250) x 1000 div 550
    c.eq("wt at min", trade_weight(-250, -250, 300), 0)
    c.eq("wt at full", trade_weight(300, -250, 300), 1000)
    c.eq("wt at 0", trade_weight(0, -250, 300), 454)
    c.eq("trade T2 at 454", trade_chance(15, 454), 6)
    c.eq("trade Q4 L 25", trade_chance(15, 454, 25), 150)
    c.eq("trade Q4 capped", trade_chance(1000, 1000, 25), 1000)
    c.eq("trade arms boost", trade_chance(15, 1000, None, True, 2000), 30)
    c.eq("trade arms boost Q4 capped", trade_chance(15, 1000, 100, True, 2000), 1000)
    # mines: 20 x 1000 div 1000
    c.eq("mine T2 dwarves", mine_chance(20, 1000), 20)
    c.eq("mine T2 elves", mine_chance(20, 0), 0)
    c.eq("mine Q4 L 25", mine_chance(20, 1000, 25), 500)
    c.eq("mine Q4 capped", mine_chance(20, 1000, 60), 1000)
    # spoils: V
    c.eq("V tribute only", spoils_value(300, 0, 250), 300)
    c.eq("V sacks only", spoils_value(0, 1203, 250), 300)
    c.eq("V both", spoils_value(100, 400, 250), 200)
    c.eq("V none", spoils_value(0, 0, 250), 0)
    c.eq("V plunder 0 permille", spoils_value(0, 5000, 0), 0)
    # parts and shares: floor 100, cap 400
    c.eq("part at the floor", spoils_shares(1000, [5], 1000, 100, 400), [(100, 100)])
    c.eq("part between", spoils_shares(1000, [250], 1000, 100, 400), [(250, 250)])
    c.eq("part at the cap", spoils_shares(1000, [900], 1000, 100, 400), [(400, 400)])
    c.eq("dead_S 0 (max 1)", spoils_shares(10, [0], 0, 100, 400), [(100, 1)])
    c.eq("two allies", spoils_shares(1000, [400, 300], 1000, 100, 400), [(400, 400), (300, 300)])
    c.eq("three allies at the cap: never above V", spoils_shares(1000, [500, 500, 500], 1000, 100, 400),
         [(400, 400), (400, 400), (400, 200)])
    c.eq("three allies: the remainder", spoils_shares(7, [400, 400, 400], 1000, 100, 400),
         [(400, 2), (400, 2), (400, 2)])
    c.eq("a share of 0", spoils_shares(3, [100], 1000, 100, 400), [(100, 0)])
    # the principal's need: exhaustion 500 (shipped P3-06), muster 1000; pop0 4000 -> denominator 2000
    c.eq("ex none", principal_need(0, 4000, 500, 1000), 0)
    c.eq("ex half", principal_need(1000, 4000, 500, 1000), 500)
    c.eq("ex full", principal_need(5000, 4000, 500, 1000), 1000)
    c.eq("ex pop0 0", principal_need(3, 0, 500, 1000), 1000)
    # g: the spec's sample (warmth 600, part 200, ex 500): 530
    c.eq("g sample of 6.11", grant_chance(600, 200, 500, t), 530)
    c.eq("g at min", grant_chance(0, 100, 1000, dict(t, spoils_grant_permille=0)), 100)
    c.eq("g at max", grant_chance(1000, 400, 0, t), 950)
    c.eq("g base alone", grant_chance(0, 0, 0, t), 450)
    c.eq("g warmth alone", grant_chance(1000, 0, 0, dict(t, spoils_grant_permille=0, spoils_grant_min=0)), 300)
    c.eq("g part alone", grant_chance(0, 400, 0, dict(t, spoils_grant_permille=0, spoils_grant_min=0)), 200)
    c.eq("g need alone", grant_chance(0, 0, 1000, dict(t, spoils_grant_permille=500, spoils_grant_min=0)), 100)
    # withdrawal: dead x 1000 >= exh x max(1, pop0 x muster div 1000)
    c.eq("withdraws at the line", withdraws(1000, 2000, 500, 1000), True)
    c.eq("not one below", withdraws(999, 2000, 500, 1000), False)
    c.eq("pop0 0", withdraws(1, 0, 500, 1000), True)
    # allied battle: side forces and the casualty split
    c.eq("side force with effort 1000", side_force(600, [300, 100], 1000), ([600, 300, 100], 1000))
    c.eq("side force with effort 500", side_force(600, [301], 500), ([600, 150], 750))
    c.eq("split: remainder to the principal", split_dead(100, 600, [(7, 300), (4, 100)]), (60, {4: 10, 7: 30}))
    c.eq("split: rounding", split_dead(10, 1, [(2, 1), (3, 1)]), (4, {2: 3, 3: 3}))
    c.eq("split: an ally of force 0 takes nothing", split_dead(10, 5, [(2, 0), (3, 5)]), (5, {3: 5}))
    # deterrence (300), fear of blocs, allied strain
    c.eq("deterrence", deterred_force(1000, [500, 501], 300, 1200), 1560)   # (1000 + 300) x 1200 div 1000
    c.eq("deterrence 0", deterred_force(1000, [500], 0, 1000), 1000)
    c.eq("fear below", bloc_fear(500, 300, 400), 0)
    c.eq("fear by bloc", bloc_fear(500, 700, 400), 250)
    c.eq("allied strain", allied_rate(37, 250), 9)
    c.eq("allied strain 1000", allied_rate(37, 1000), 37)
    # 7.5: the three factors in order, floor each
    c.eq("force: no partner", strengthened(1000, 0, False, False, cf), 1000)
    c.eq("force: 2 partners", strengthened(1000, 2, False, False, cf), 1100)
    c.eq("force: 5 partners capped at 3", strengthened(1000, 5, False, False, cf), 1150)
    c.eq("force: forged", strengthened(1000, 0, True, False, cf), 1050)
    c.eq("force: arms", strengthened(1000, 0, False, True, cf), 1100)
    c.eq("force: order and floors", strengthened(999, 1, True, False, cf), 1100)   # 999 x 1050 div 1000 = 1048; x 1050
    c.eq("force: floors (arms)", strengthened(999, 1, False, True, cf), 1152)   # 1048 x 1100 div 1000
    c.eq("force: small", strengthened(19, 3, False, True, cf), 23)   # 19 x 1150 div 1000 = 21; 21 x 1.1 = 23
    # mine names
    forms = shipped_tuning()["mine_forms"]
    c.eq("mine name id 1", mine_name(forms, 1, "Kazad Rune"), "The Mines of Kazad Rune")
    c.eq("mine name id 2", mine_name(forms, 2, "Kazad Rune"), "The Delvings of Kazad Rune")
    c.eq("mine name id 3", mine_name(forms, 3, "Brokk"), "The Mines of Brokk")
    c.eq("mine name: The lowered", mine_name(forms, 4, "The Anvil"), "The Delvings of the Anvil")
    # trade credits
    c.eq("credit T2 due", trade_credit_periods(1050, 1050, 50), (1, 1100))
    c.eq("credit T2 not due", trade_credit_periods(1049, 1050, 50), (0, 1050))
    c.eq("credit Q4 of 125 years", trade_credit_periods(1060, 1050, 50, 125), (3, 1200))   # 1060 + 124 = 1184
    c.eq("credit Q4 one period", trade_credit_periods(1050, 1050, 50, 25), (1, 1100))
    # renewal
    c.eq("renewal T2 due", renewal_due(1150, 1000, 150), True)
    c.eq("renewal T2 not", renewal_due(1149, 1000, 150), False)
    c.eq("renewal Q4 within", renewal_due(1130, 1000, 150, 25), True)
    c.eq("renewal Q4 not", renewal_due(1125, 1000, 150, 25), False)
    # reach and trade reach
    c.eq("reach at hops", in_reach(3, 3), True)
    c.eq("reach at hops + 1", in_reach(4, 3), False)
    c.eq("reach -1 never", in_reach(0, -1), False)
    c.eq("reach none", in_reach(None, 8), False)
    c.eq("trade reach margin", (trade_out_of_reach(4, 3), trade_out_of_reach(5, 3)), (False, True))
    # browser groups, marker
    c.eq("groups of 63 to 78", [GROUP_OF_NEW[k] for k in range(63, 79)],
         [7] * 8 + [2] + [7] * 5 + [1, 7])
    c.eq("ALL_GROUPS", ALL_GROUPS, (1 << 8) - 1)
    c.eq("mine glyph", MINE_GLYPH, 16 + SITE_MINE)


# ---------------------------------------------------------------------------
# A world's rows
# ---------------------------------------------------------------------------

READ_TABLES = ("region", "region_border", "race", "event_kind", "event_kind_role", "event_kind_value", "phase",
               "event", "event_participant", "event_cause", "event_value", "civilization", "settlement",
               "settlement_census", "region_control", "relation", "site", "region_name")

ISLET_MAX_DRY_CELLS = None   # read from the report reference (P3-07's islet rule)


def islet_max():
    global ISLET_MAX_DRY_CELLS
    if ISLET_MAX_DRY_CELLS is None:
        import p3_report_reference as prr
        ISLET_MAX_DRY_CELLS = prr.ISLET_MAX_DRY_CELLS
    return ISLET_MAX_DRY_CELLS


def _pk_rows(conn, table):
    info = conn.execute("SELECT name, pk FROM pragma_table_info(?) ORDER BY cid", (table,)).fetchall()
    if not info:
        return None
    cols = [r[0] for r in info]
    pks = [r[0] for r in sorted((r for r in info if r[1] > 0), key=lambda r: r[1])] or ["rowid"]
    order = ", ".join('"%s"' % p if p == "rowid" else '"%s" COLLATE BINARY' % p for p in pks)
    return conn.execute('SELECT %s FROM "%s" ORDER BY %s' % (", ".join('"%s"' % c for c in cols), table,
                                                            order)).fetchall()


def load_rows(path):
    conn = sqlite3.connect("file:%s?mode=ro&immutable=1" % path, uri=True)
    conn.text_factory = lambda b: b.decode("latin-1")
    try:
        meta = dict(conn.execute("SELECT key, value FROM meta").fetchall())
        t = {}
        for n in READ_TABLES:
            rows = _pk_rows(conn, n)
            t[n] = rows if rows is not None else []
    finally:
        conn.close()
    return meta, t


class AW:
    """Indexes over a world's rows (the columns of docs/specs/schema.md at schema 11)."""

    def __init__(self, meta, t):
        self.meta = meta
        self.t = t
        self.E = int(meta.get("history_end_year", "6000"))
        self.key = {k[0]: k[1] for k in t["event_kind"]}
        self.kid = {k[1]: k[0] for k in t["event_kind"]}
        self.ev = {e[0]: e for e in t["event"]}
        self.parts = {}
        for p in t["event_participant"]:
            self.parts.setdefault(p[0], []).append(p)
        self.causes = {}
        self.kids = {}
        for e, c in t["event_cause"]:
            self.causes.setdefault(e, []).append(c)
            self.kids.setdefault(c, []).append(e)
        self.vals = {}
        for e, seq, v in t["event_value"]:
            self.vals.setdefault(e, {})[seq] = v
        self.vseq = {}
        for k, seq, key in t["event_kind_value"]:
            self.vseq.setdefault(k, {})[key] = seq
        self.rseq = {}
        for r in t["event_kind_role"]:
            self.rseq.setdefault(r[0], {})[r[2]] = r[1]
        self.civ = {c[0]: c for c in t["civilization"]}
        self.stl = {s[0]: s for s in t["settlement"]}
        self.region = {r[0]: r for r in t["region"]}
        self.race_of = {c[0]: c[2] for c in t["civilization"]}
        self.site = {x[0]: x for x in t["site"]}
        self.census = {}
        for r in t["settlement_census"]:
            self.census.setdefault(r[0], []).append(r)
        self.by_kind = {}
        for e in t["event"]:
            self.by_kind.setdefault(self.key.get(e[3]), []).append(e[0])
        self.rows = {}
        for r in t["relation"]:
            self.rows.setdefault(r[4], []).append(r)
        self.rc = {}
        for r in t["region_control"]:
            self.rc.setdefault(r[0], []).append(r)
        self.phase_rows = t["phase"]

    # events
    def kind(self, e):
        return self.key.get(self.ev[e][3])

    def year(self, e):
        return self.ev[e][1]

    def of(self, key):
        return list(self.by_kind.get(key, ()))

    def role(self, e, role):
        rs = self.rseq.get(self.ev[e][3], {}).get(role)
        return [p[4] for p in sorted(self.parts.get(e, [])) if p[2] == rs] if rs is not None else []

    def one(self, e, role):
        r = self.role(e, role)
        return r[0] if r else 0

    def value(self, e, key):
        seq = self.vseq.get(self.ev[e][3], {}).get(key)
        return self.vals.get(e, {}).get(seq) if seq is not None else None

    def civs_of(self, e):
        return [p[4] for p in sorted(self.parts.get(e, [])) if p[3] == CIV]

    # civilizations
    def alive(self, c, y):
        r = self.civ[c]
        return r[6] <= y and (r[8] is None or y < r[8])

    def ending_event(self, c):
        return self.civ[c][9]

    # rows of a kind in force at year y (P3-04 10's half-open rule)
    def in_force(self, kind, y):
        return [r for r in self.rows.get(kind, ()) if r[5] <= y and (r[6] is None or y < r[6])]

    def open_at_event(self, r, e):
        """A relation row open when event e is appended: opened by an earlier event, not closed by e or earlier."""
        return r[7] < e and (r[6] is None or r[8] > e)

    def pair_rows(self, kind, a, b):
        x, y = min(a, b), max(a, b)
        return [r for r in self.rows.get(kind, ()) if r[1] == x and r[3] == y]

    def allied_at_event(self, a, b, e):
        return any(self.open_at_event(r, e) for r in self.pair_rows("alliance", a, b))

    # land: the share of the non-islet regions (P3-07's islet rule, as share_max_permille)
    def land(self):
        mx = islet_max()
        return [r[0] for r in self.t["region"] if r[10] - r[11] >= mx]

    def holders_by_year(self):
        """[(y0, y1, {civ: regions held})] for the non-islet land, over [0, E)."""
        land = set(self.land())
        years = {0, self.E}
        for r in self.t["region_control"]:
            years.add(r[1])
            if r[2] is not None:
                years.add(r[2])
        for k in ("alliance",):
            for r in self.rows.get(k, ()):
                years.add(r[5])
                if r[6] is not None:
                    years.add(r[6])
        years = sorted(y for y in years if 0 <= y <= self.E)
        out = []
        for y0, y1 in zip(years, years[1:]):
            held = {}
            for r in self.t["region_control"]:
                if r[0] in land and r[1] <= y0 and (r[2] is None or y0 < r[2]):
                    held[r[3]] = held.get(r[3], 0) + 1
            out.append((y0, y1, held))
        return out

    def bloc_series(self):
        """[(y0, y1, {civ: bs})] (6.2 with the report's islet rule) and the largest bloc (bs, year, civ, allies)."""
        N = len(self.land())
        series, best = [], None
        for y0, y1, held in self.holders_by_year():
            allies = {}
            for r in self.in_force("alliance", y0):
                allies.setdefault(r[1], []).append(r[3])
                allies.setdefault(r[3], []).append(r[1])
            share = {c: held[c] * 1000 // N for c in held} if N else {}
            bs = {}
            for c in sorted(allies):
                bs[c] = bloc_share(share.get(c, 0), [share.get(z, 0) for z in allies[c]])
                if best is None or bs[c] > best[0]:
                    best = (bs[c], y0, c, sorted(allies[c]))
            series.append((y0, y1, bs))
        return series, best

    def longest_run_above(self, limit):
        run = best = 0
        last_end = None
        for y0, y1, bs in self.bloc_series()[0]:
            if bs and max(bs.values()) > limit:
                run = run + (y1 - y0) if last_end == y0 else (y1 - y0)
                last_end = y1
                best = max(best, run)
            else:
                run, last_end = 0, None
        return best

    # settlements
    def census_at(self, s, y):
        best = None
        for r in self.census.get(s, ()):
            if r[1] <= y:
                best = r
        return best

    def holder_at(self, rid, y):
        for r in self.rc.get(rid, ()):
            if r[1] <= y and (r[2] is None or y < r[2]):
                return r[3]
        return 0

    # wars
    def war_row(self, d):
        rs = [r for r in self.rows.get("war", ()) if r[7] == d]
        return rs[0] if rs else None

    def war_events(self, d):
        """The battles caused by declaration d, their sieges and the falls those caused."""
        out = []
        for b in self.kids.get(d, []):
            if self.kind(b) in BATTLE_KINDS:
                out.append(b)
                for sg in self.kids.get(b, []):
                    if self.kind(sg) == "siege":
                        out.append(sg)
                        for f in self.kids.get(sg, []):
                            if self.kind(f) in ("sacked", "settlement_captured"):
                                out.append(f)
        return sorted(out)

    def winner_loser(self, e):
        k = self.kind(e)
        if k in ("peace", "war_won"):
            return self.one(e, "winner"), self.one(e, "loser")
        if k == "civ_conquered":
            return self.one(e, "conqueror"), self.one(e, "civilization")
        return 0, 0

    def spoils_V(self, d, end_event, winner, plunder_permille):
        tribute = sum(self.value(x, "people") or 0 for x in self.kids.get(end_event, [])
                      if self.kind(x) == "tribute" and self.one(x, "payee") == winner)
        plunder = sum((self.value(x, "dead") or 0) + (self.value(x, "survivors") or 0)
                      for x in self.war_events(d) if self.kind(x) == "sacked" and self.one(x, "attacker") == winner)
        return spoils_value(tribute, plunder, plunder_permille), tribute, plunder

    def quiet_length_at(self, y):
        """The length of the quiet phase row holding year y (0 in a tick)."""
        for p in self.phase_rows:
            # phase: (id, age, kind, start_year, end_year, trigger_event); P3-04 9: kind QUIET
            if p[2] == QUIET and p[3] <= y <= p[4]:
                return p[4] - p[3] + 1
        return 0


def aw_of(path):
    meta, t = load_rows(path)
    return AW(meta, t)


# ---------------------------------------------------------------------------
# Data of a root (history.alliances, the race fields), with the 0.26.0 listings standing in for older files
# ---------------------------------------------------------------------------

def data_of(root=None):
    """(alliances tuning, {race key: (origin_allies, ally_join_permille, mining_permille)}, conflict fields)."""
    import p3_data_reference as pdr
    root = os.path.join(REPO, "game", "data") if root is None else root
    t = dict(pdr.ALLIANCES_TUNING_026)
    p = os.path.join(root, "history", "alliances.toml")
    if os.path.exists(p):
        with open(p, "rb") as f:
            t = tomllib.load(f)["history"]["alliances"]
    races = dict(pdr.RACE_FIELDS_026)
    rd = os.path.join(root, "races")
    if os.path.isdir(rd):
        for n in sorted(os.listdir(rd)):
            if n.endswith(".toml"):
                with open(os.path.join(rd, n), "rb") as f:
                    for k, r in tomllib.load(f)["race"].items():
                        if "origin_allies" in r:
                            races[k] = (r["origin_allies"], r["ally_join_permille"], r["mining_permille"])
    cf = shipped_conflict_fields()
    p = os.path.join(root, "history", "conflict.toml")
    if os.path.exists(p):
        with open(p, "rb") as f:
            co = tomllib.load(f)["history"]["conflict"]
        if "allied_strain_permille" in co:
            cf = {k: co[k] for k in cf}
    return t, races, cf


# ---------------------------------------------------------------------------
# Invariants of 13 on rows (check-world)
# ---------------------------------------------------------------------------

def _years_of(rows):
    ys = set()
    for r in rows:
        ys.add(r[5])
        if r[6] is not None:
            ys.add(r[6])
    return sorted(ys)


def invariant_violations(w, t, races):
    """13 items 1, 2 (the betrayed pairs), 3 (origin), 4 to 7, 8 (AL9's hard bound), 10 (shelter), 11 to 14.
    `races`: {race key: (origin_allies, join, mining)}."""
    out = []
    key_of_race = {r[0]: r[1] for r in w.t["race"]}
    rkey = lambda c: key_of_race.get(w.race_of.get(c))
    # 1: counts at every boundary year; alive sides; one open row per pair
    for kind, cap, name in (("alliance", t["allies_max"], "allies"), ("trade", t["trade_max"], "trades")):
        rows = w.rows.get(kind, ())
        for y in _years_of(rows):
            n = {}
            pairs = set()
            for r in w.in_force(kind, y):
                n[r[1]] = n.get(r[1], 0) + 1
                n[r[3]] = n.get(r[3], 0) + 1
                if (r[1], r[3]) in pairs:
                    out.append("1: two %s rows of c%d and c%d in force at %d" % (kind, r[1], r[3], y))
                pairs.add((r[1], r[3]))
            for c, k in sorted(n.items()):
                if k > cap:
                    out.append("1: c%d has %d %s at %d (max %d)" % (c, k, name, y, cap))
        for r in rows:
            if not (r[0] == 3 and r[2] == 3 and r[1] < r[3]):
                out.append("1: %s row %r is not symmetric civilization a < b" % (kind, r[:6]))
                continue
            for c in (r[1], r[3]):
                cv = w.civ[c]
                if r[5] < cv[6] or (cv[8] is not None and (r[6] is None or r[6] > cv[8])):
                    out.append("1: %s row of c%d and c%d %d-%s outlives c%d (%d-%s)" % (
                        kind, r[1], r[3], r[5], r[6], c, cv[6], cv[8]))
    # 2: no alliance of a betrayed pair opens after the betrayal
    for b in w.of("alliance_betrayed"):
        x, y = w.one(b, "betrayer"), w.one(b, "betrayed")
        for r in w.pair_rows("alliance", x, y):
            if r[7] > b:
                out.append("2: alliance of betrayed pair c%d c%d opens at e%d after e%d" % (x, y, r[7], b))
    # 3: an origin alliance names races listing each other
    for e in w.of("alliance_formed"):
        a, b = w.one(e, "ally_a"), w.one(e, "ally_b")
        if not a < b:
            out.append("3: alliance_formed e%d: ally_a c%d not below ally_b c%d" % (e, a, b))
        if w.value(e, "basis") == ORIGIN:
            ka, kb = rkey(a), rkey(b)
            if kb not in races.get(ka, ([],))[0] or ka not in races.get(kb, ([],))[0]:
                out.append("3: origin alliance e%d of %s and %s, not origin allies" % (e, ka, kb))
            if w.role(e, "foe"):
                out.append("3: origin alliance e%d with a foe" % e)
        elif w.value(e, "basis") in (COMMON_ENEMY, FEAR) and not w.role(e, "foe"):
            out.append("3: alliance_formed e%d of basis %d without its foe" % (e, w.value(e, "basis")))
    # 4: joins
    war_of_join = {}
    for j in w.of("ally_joined_war"):
        z, p, en = w.one(j, "ally"), w.one(j, "partner"), w.one(j, "enemy")
        decl = [c for c in w.causes.get(j, []) if w.kind(c) == "war_declared"]
        if len(decl) != 1:
            out.append("4: ally_joined_war e%d cites %d declarations" % (j, len(decl)))
            continue
        d = decl[0]
        war_of_join[j] = d
        sides = {w.one(d, "attacker"), w.one(d, "defender")}
        if w.year(d) != w.year(j) or sides != {p, en}:
            out.append("4: ally_joined_war e%d: declaration e%d of another year or sides" % (j, d))
        side = w.value(j, "side")
        if side not in (0, 1) or (side == DEFENCE) != (w.one(d, "defender") == p):
            out.append("4: ally_joined_war e%d: side %s against the declaration" % (j, side))
        if not w.allied_at_event(z, p, j):
            out.append("4: ally_joined_war e%d: c%d not allied with c%d" % (j, z, p))
        if w.allied_at_event(z, en, j):
            out.append("4: ally_joined_war e%d: c%d allied with the enemy c%d" % (j, z, en))
        rows = [r for r in w.rows.get("war", ()) if r[7] == j]
        if len(rows) != 1 or rows[0][1] != z or rows[0][3] != en:
            out.append("4: ally_joined_war e%d: no joined war row (c%d, c%d)" % (j, z, en))
        # 10: no call against a sheltered realm (P3-06 7.2's shelter, P3-06's shelter_years)
    # 5: allied battles
    joined_rows = {}
    for r in w.rows.get("war", ()):
        if r[7] in w.ev and w.kind(r[7]) == "ally_joined_war":
            joined_rows.setdefault((r[1], r[3]), []).append(r)
    for b in sorted(w.of("allied_battle") + [x for x in w.of("battle_led") if w.role(x, "winner_ally") +
                                              w.role(x, "loser_ally")]):   # P5-06 11.1: battle_led with allies
        W_, L_ = w.one(b, "winner"), w.one(b, "loser")
        wa, la = w.role(b, "winner_ally"), w.role(b, "loser_ally")
        if not wa and not la:
            out.append("5: allied_battle e%d without an ally" % b)
        for z, enemy in [(z, L_) for z in wa] + [(z, W_) for z in la]:
            if not any(w.open_at_event(r, b) for r in joined_rows.get((z, enemy), [])):
                out.append("5: allied_battle e%d: ally c%d has no open joined row against c%d" % (b, z, enemy))
        if wa != sorted(wa) or la != sorted(la):
            out.append("5: allied_battle e%d: allies not ascending" % b)
    # 6: no ally is the besieger, taker or payee of its war's costs
    for j, d in sorted(war_of_join.items()):
        z = w.one(j, "ally")
        row = w.war_row(d)
        for x in w.war_events(d):
            if w.kind(x) in ("siege", "sacked", "settlement_captured") and w.one(x, "attacker") == z:
                out.append("6: ally c%d of e%d is the attacker of e%d" % (z, d, x))
        if row is not None and row[6] is not None:
            for x in w.kids.get(row[8], []):
                if w.kind(x) == "tribute" and w.one(x, "payee") == z:
                    out.append("6: ally c%d of e%d is the payee of e%d" % (z, d, x))
                if w.kind(x) in ("region_ceded", "settlements_ceded") and w.one(x, "to") == z:
                    out.append("6: ally c%d of e%d takes e%d" % (z, d, x))
    # 7: betrayals cite a declaration between the two of the same year
    for b in w.of("alliance_betrayed"):
        x, y = w.one(b, "betrayer"), w.one(b, "betrayed")
        ds = [c for c in w.causes.get(b, []) if w.kind(c) == "war_declared" and w.year(c) == w.year(b)
              and w.one(c, "attacker") == x and w.one(c, "defender") == y]
        if not ds:
            out.append("7: alliance_betrayed e%d cites no declaration of c%d on c%d that year" % (b, x, y))
    # 8: AL9's hard bound (reading P7)
    run = w.longest_run_above(800)
    if run > 1000:
        out.append("8: a bloc above 800 permille for %d consecutive years" % run)
    # 11: spoils
    seen = set()
    per_war = {}
    for s in sorted(w.of("spoils_shared") + w.of("spoils_withheld")):
        P, z, en = w.one(s, "principal"), w.one(s, "ally"), w.one(s, "enemy")
        js = [c for c in w.causes.get(s, []) if w.kind(c) == "ally_joined_war" and w.one(c, "ally") == z]
        ends = [c for c in w.causes.get(s, []) if w.kind(c) in ("peace", "war_won", "civ_conquered")]
        if len(js) != 1 or len(ends) != 1:
            out.append("11: spoils e%d without its war" % s)
            continue
        j, E_ = js[0], ends[0]
        d = war_of_join.get(j)
        if w.year(s) < w.year(E_) or w.winner_loser(E_) != (P, en):
            out.append("11: spoils e%d: end e%d of another winner or a later year" % (s, E_))
        if w.one(j, "partner") != P or w.one(j, "enemy") != en:
            out.append("11: spoils e%d: join e%d of another partner or enemy" % (s, j))
        if d is None:
            continue
        if not any(z in w.role(b, "winner_ally") + w.role(b, "loser_ally")
                   for b in w.war_events(d) if w.kind(b) in ("allied_battle", "battle_led")):
            out.append("11: spoils e%d: c%d fought in no battle of e%d" % (s, z, d))
        for x in w.of("ally_withdrew"):
            if j < x < E_ and (w.one(x, "ally"), w.one(x, "partner"), w.one(x, "enemy")) == (z, P, en):
                out.append("11: spoils e%d: c%d withdrew (e%d)" % (s, z, x))
        if (d, z) in seen:
            out.append("11: two spoils events for war e%d and ally c%d" % (d, z))
        seen.add((d, z))
        sh = w.value(s, "share")
        if sh is None or sh < 1:
            out.append("11: spoils e%d share %r" % (s, sh))
            continue
        V = w.spoils_V(d, E_, P, t["spoils_plunder_permille"])[0]
        per_war.setdefault(d, [V, 0])[1] += sh
        if sh > V * t["spoils_cap_permille"] // 1000:
            out.append("11: spoils e%d share %d above the cap of V %d" % (s, sh, V))
        if w.ev[s][5] != w.ev[d][5]:
            out.append("11: spoils e%d in r%d, not the war's goal region r%d" % (s, w.ev[s][5], w.ev[d][5]))
    for d, (V, tot) in sorted(per_war.items()):
        if tot > V:
            out.append("11: war e%d: shares %d above V %d" % (d, tot, V))
    spoils_ids = set(w.of("spoils_shared") + w.of("spoils_withheld"))
    for r in w.t["region_control"]:
        if r[4] in spoils_ids:
            out.append("11: region_control row of r%d by spoils e%d" % (r[0], r[4]))
    for r in w.t["relation"]:
        if r[0] == 6:   # P5-06 6.4 rule 3: a spoils event may open a ruled row (the ruler it names)
            continue
        if r[7] in spoils_ids or (r[6] is not None and r[8] in spoils_ids):
            out.append("11: relation row %r by a spoils event" % (r[:6],))
    # 12: spurned (P5-04a 16 item 13 replaces it at 0.28.0: invariant_violations_028)
    formed_basis = {e: w.value(e, "basis") for e in w.of("alliance_formed")}
    for s in (w.of("alliance_spurned") if not is_028(w) else ()):
        z, P = w.one(s, "ally"), w.one(s, "principal")
        rows = [r for r in w.pair_rows("alliance", z, P) if r[6] is not None and r[8] == s]
        if len(rows) != 1:
            out.append("12: alliance_spurned e%d closes no alliance of c%d and c%d" % (s, z, P))
            continue
        r = rows[0]
        if formed_basis.get(r[7]) == ORIGIN:
            out.append("12: alliance_spurned e%d closes an origin alliance" % s)
        last = max([r[7]] + [x for x in w.of("alliance_renewed") if r[7] < x < s and
                             {w.one(x, "ally_a"), w.one(x, "ally_b")} == {z, P}])
        wh = [c for c in w.causes.get(s, []) if w.kind(c) == "spoils_withheld"]
        if len(wh) != 1 or w.one(wh[0], "ally") != z or w.one(wh[0], "principal") != P or wh[0] < last:
            out.append("12: alliance_spurned e%d cites no withheld share of c%d after e%d" % (s, z, last))
        elif w.ev[s][5] != w.ev[wh[0]][5]:
            out.append("12: alliance_spurned e%d: region r%d, the withheld share's r%d" % (s, w.ev[s][5],
                                                                                         w.ev[wh[0]][5]))
        if w.value(s, "reason") == 0:
            ly = w.year(last)
            q = w.quiet_length_at(w.year(s))
            due = renewal_due(w.year(s), ly, t["term_years"], q if q else None)
            n = sum(1 for x in w.of("spoils_withheld") if last < x < s and
                    {w.one(x, "ally"), w.one(x, "principal")} == {z, P})
            if not due or n < t["withheld_to_break"]:
                out.append("12: alliance_spurned e%d reason 0: renewal due %s, %d withheld" % (s, due, n))
    # 13: mines
    ore_min, pop_min = t["mine_ore_min"], t["mine_settlement_min"]
    mines = [x for x in w.t["site"] if x[2] == SITE_MINE]
    stl_seen = {}
    for x in mines:
        sid, s_, R, c = x[0], x[6], x[3], x[7]
        y0 = x[9]
        if s_ in stl_seen:
            out.append("13: settlement s%d has two mines (si%d, si%d)" % (s_, stl_seen[s_], sid))
        stl_seen[s_] = sid
        st = w.stl.get(s_)
        if st is None or st[4] != R:
            out.append("13: mine si%d: settlement s%d not in r%d" % (sid, s_, R))
            continue
        cen = w.census_at(s_, y0)
        pop = cen[5] if cen is not None else st[11]
        # the rows show the end of year y0: a mine whose town or region was lost later in its opening year (its
        # site ended in y0 by a later event) is read at y0 - 1
        lost_same_year = x[11] == y0 and x[12] > x[10]
        if lost_same_year:
            cen = w.census_at(s_, y0 - 1) or cen
            pop = cen[5] if cen is not None else pop
        held = w.holder_at(R, y0) == c or (lost_same_year and w.holder_at(R, y0 - 1) == c)
        if (cen is not None and cen[2] != c) or not held:
            out.append("13: mine si%d: s%d or r%d not of c%d at %d" % (sid, s_, R, c, y0))
        # reading (integration): the population at the opening is not in the rows (P3-04 6.7 writes a census row
        # only at a change of owner, kind or status and at a phase's end; civs4-seed-7-1024 si35: s105's last row
        # says 152 fifty years before its mine opened); mine_settlement_min is checked by the replay, not here
        if w.region[R][27] < ore_min:
            out.append("13: mine si%d: r%d ore_reach_cells %d" % (sid, R, w.region[R][27]))
        if races.get(rkey(c), ([], 0, 0))[2] <= 0:
            out.append("13: mine si%d of a race that does not mine" % sid)
        if x[8] != 0:
            out.append("13: mine si%d with a monster" % sid)
        oe = x[10]
        if oe not in w.ev or w.kind(oe) != "mine_opened" or w.ev[oe][5] != R or w.ev[oe][6] != s_ or \
                w.one(oe, "civilization") != c or w.one(oe, "site") != sid or w.year(oe) != y0:
            out.append("13: mine si%d: origin event e%d is not its mine_opened" % (sid, oe))
        want = mine_name(t["mine_forms"], sid, st[1])
        if x[1] != want:
            out.append("13: mine si%d named %r, not %r" % (sid, x[1], want))
        if (x[4], x[5]) != (st[2], st[3]):
            out.append("13: mine si%d not at s%d's cell" % (sid, s_))
        if x[11] is None:
            if x[13] != 0 or x[12] != 0:
                out.append("13: open mine si%d with ending %d" % (sid, x[13]))
        else:
            if x[13] not in (ENDING_LOST, ENDING_FELL):
                out.append("13: mine si%d ending %d" % (sid, x[13]))
            ee = x[12]
            if ee not in w.ev or w.year(ee) != x[11]:
                out.append("13: mine si%d ending event e%d not of year %d" % (sid, ee, x[11]))
            if x[13] == ENDING_FELL and (w.civ[c][8] is None or ee != w.civ[c][9]):
                out.append("13: mine si%d fell but its realm's ending is not e%d" % (sid, ee))
        # (b) open: its town standing and its founder's, its region the founder's, at each year's end
        end = x[11] if x[11] is not None else w.E
        changes = sorted({r[1] for r in w.census.get(s_, ()) if y0 < r[1] < end} |
                         {r[1] for r in w.rc.get(R, ()) if y0 < r[1] < end} |
                         {r[2] for r in w.rc.get(R, ()) if r[2] is not None and y0 < r[2] < end})
        for y in changes:
            cen = w.census_at(s_, y)
            if cen is not None and (cen[2] != c or cen[4] not in (THRIVING, DECLINING)) or w.holder_at(R, y) != c:
                out.append("13: mine si%d open at %d while s%d or r%d is not c%d's" % (sid, y, s_, R, c))
                break
    open_per = {}
    for x in mines:
        open_per.setdefault((x[7], x[3]), []).append((x[9], x[11] if x[11] is not None else w.E + 1))
    for k, iv in sorted(open_per.items()):
        iv.sort()
        for (a0, a1), (b0, b1) in zip(iv, iv[1:]):
            if b0 < a1:
                out.append("13: c%d has two open mines in r%d at %d" % (k[0], k[1], b0))
    # 14: arms
    sold = {}
    for a in w.of("arms_sold"):
        s_, b_, si = w.one(a, "seller"), w.one(a, "buyer"), w.one(a, "site")
        x = w.site.get(si)
        if x is None or x[2] != SITE_MINE or x[7] != s_ or not (x[10] < a and (x[11] is None or x[12] > a)):
            out.append("14: arms_sold e%d: si%d is not an open mine of c%d" % (a, si, s_))
        tr = [r for r in w.pair_rows("trade", s_, b_) if w.open_at_event(r, a)]
        if len(tr) != 1:
            out.append("14: arms_sold e%d: no open trade of c%d and c%d" % (a, s_, b_))
        else:
            # lead ruling (P5-04a integration part 3): 6.13's rule is per ordered (seller, buyer) pair, so a trade
            # between two realms that each hold a mine may see one sale in each direction
            k = (tr[0][7], s_, b_)
            sold[k] = sold.get(k, 0) + 1
            if sold[k] > 1:
                out.append("14: two arms sales by c%d to c%d on trade e%d" % (s_, b_, k[0]))
        if any(m[7] == b_ and m[10] < a and (m[11] is None or m[12] > a) for m in mines):
            out.append("14: arms_sold e%d: the buyer c%d has an open mine" % (a, b_))
    return out


def shelter_violations(w, shelter_years):
    """10: no call against a sheltered realm (P3-06 7.2): a join's year >= the enemy's founding + shelter_years."""
    out = []
    for j in w.of("ally_joined_war"):
        en = w.one(j, "enemy")
        if w.year(j) < w.civ[en][6] + shelter_years:
            out.append("10: ally_joined_war e%d against c%d founded %d" % (j, en, w.civ[en][6]))
    return out


# ---------------------------------------------------------------------------
# Reader checks of 9.1 and 9.2 (the messages; first failing row in key order is the reader's)
# ---------------------------------------------------------------------------

def reader_problems(w):
    out = []
    kind_ends = ("alliance_lapsed", "alliance_broken", "alliance_spurned", "alliance_betrayed")
    for r in sorted(w.t["relation"]):
        key = "%d %d %d %d %s %d" % (r[0], r[1], r[2], r[3], r[4], r[5])
        msg = None
        se = r[7]
        names = lambda e: set(w.civs_of(e)) if e in w.ev else set()
        if r[4] == "alliance":
            if se not in w.ev or w.kind(se) != "alliance_formed" or names(se) - set() < {r[1], r[3]} or \
                    {w.one(se, "ally_a"), w.one(se, "ally_b")} != {r[1], r[3]}:
                msg = "alliance since_event %d is not an alliance_formed of the pair" % se
            elif r[6] is not None:
                u = r[8]
                ok = u in w.ev and ((w.kind(u) in kind_ends and {r[1], r[3]} <= names(u)) or
                                    u in (w.civ[r[1]][9], w.civ[r[3]][9]))
                if not ok:
                    msg = "until_event %d does not end the alliance" % u
        elif r[4] == "trade":
            if se not in w.ev or w.kind(se) != "trade_opened" or \
                    {w.one(se, "partner_a"), w.one(se, "partner_b")} != {r[1], r[3]}:
                msg = "trade since_event %d is not a trade_opened of the pair" % se
        elif r[4] == "war":
            ok = se in w.ev and ((w.kind(se) == "war_declared" and w.one(se, "attacker") == r[1] and
                                  w.one(se, "defender") == r[3]) or
                                 (w.kind(se) == "ally_joined_war" and w.one(se, "ally") == r[1] and
                                  w.one(se, "enemy") == r[3]))
            if not ok:
                msg = "war since_event %d is not a war_declared or an ally_joined_war of the pair" % se
        if msg:
            out.append("corrupt table 'relation': row %s: %s" % (key, msg))
    ranges = {"alliance_formed": {"basis": (0, 4)}, "alliance_renewed": {"basis": (0, 4)},
              "alliance_lapsed": {"reason": (0, 1)}, "alliance_broken": {"reason": (0, 1)},
              "alliance_spurned": {"reason": (0, 2 if is_028(w) else 1)}, "trade_broken": {"reason": (0, 2)},
              "ally_joined_war": {"side": (0, 1)}}
    pass_ids = None
    for e in sorted(w.ev):
        k = w.kind(e)
        for v, (lo, hi) in ranges.get(k, {}).items():
            x = w.value(e, v)
            if x is None or not lo <= x <= hi:
                out.append("event %d: %s %s %r out of range" % (e, k, v, x))
        if k in ("spoils_shared", "spoils_withheld"):
            x = w.value(e, "share")
            if x is None or x < 1:
                out.append("event %d: %s share %r out of range" % (e, k, x))
            z = w.one(e, "ally")
            if not any(w.kind(c) in ("peace", "war_won", "civ_conquered") for c in w.causes.get(e, [])) or \
                    not any(w.kind(c) == "ally_joined_war" and w.one(c, "ally") == z for c in w.causes.get(e, [])):
                out.append("event %d: spoils without its war" % e)
        if k == "allied_battle":
            if not w.role(e, "winner_ally") and not w.role(e, "loser_ally"):
                out.append("event %d: allied_battle without an ally" % e)
    four = int(w.meta.get("schema_version", "11")) >= 12   # P5-06 9.4: site kind 4 tower
    for x in w.t["site"]:
        if x[2] not in ((1, 2, 3, 4) if four else (1, 2, 3)):
            out.append("corrupt table 'site': row %d: kind %d is not %s" % (x[0], x[2],
                                                                           "1, 2, 3 or 4" if four else "1, 2 or 3"))
        elif x[2] == SITE_MINE:
            if x[7] not in w.civ:
                out.append("corrupt table 'site': row %d: unknown civilization %d" % (x[0], x[7]))
            elif x[8] != 0:
                out.append("corrupt table 'site': row %d: mine with a monster" % x[0])
            elif x[6] == 0:
                out.append("corrupt table 'site': row %d: mine without a settlement" % x[0])
            elif x[13] not in (0, ENDING_LOST, ENDING_FELL):
                out.append("corrupt table 'site': row %d: ending %d is not 0, 4 or 5 for a mine" % (x[0], x[13]))
    return out


# ---------------------------------------------------------------------------
# [stats] (10.5), [alliances] (10.2), civilization lines (10.3), mine lines (10.6)
# ---------------------------------------------------------------------------

def _end_kind(w, r):
    if r[6] is None:
        return None
    u = r[8]
    k = w.kind(u) if u in w.ev else None
    return {"alliance_lapsed": "lapsed", "alliance_broken": "broken", "alliance_spurned": "spurned",
            "alliance_betrayed": "betrayed"}.get(k, "ended")


def _median(xs):
    """P3-07's median: the lower middle of the sorted values (as the report's other medians)."""
    import p3_report_reference as prr
    if hasattr(prr, "median"):
        return prr.median(xs)
    s = sorted(xs)
    return s[(len(s) - 1) // 2]


def sellers_at(w, y):
    return {x[7] for x in w.t["site"] if x[2] == SITE_MINE and x[9] <= y and (x[11] is None or y < x[11])}


def arms_buyers(w, y):
    s = sellers_at(w, y)
    out = set()
    for r in w.in_force("trade", y):
        for a, b in ((r[1], r[3]), (r[3], r[1])):
            if a not in s and b in s:
                out.add(a)
    return out


def stats(w):
    """10.5's keys, in order: [(key, value)] (value an int or '-')."""
    al = w.rows.get("alliance", [])
    formed = w.of("alliance_formed")
    race = w.race_of
    ended = [r for r in al if r[6] is not None]
    joins = w.of("ally_joined_war")
    decl_with = {c for j in joins for c in w.causes.get(j, []) if w.kind(c) == "war_declared"}
    owed = w.of("spoils_shared") + w.of("spoils_withheld")
    granted = w.of("spoils_shared")
    o = [("alliances_formed", len(formed)),
         ("alliances_standing", sum(1 for r in al if r[6] is None)),
         ("alliances_cross_race", sum(1 for e in formed if race[w.one(e, "ally_a")] != race[w.one(e, "ally_b")])),
         ("alliances_origin", sum(1 for e in formed if w.value(e, "basis") == ORIGIN)),
         ("alliances_lapsed", len(w.of("alliance_lapsed"))),
         ("alliances_broken", len(w.of("alliance_broken"))),
         ("alliances_betrayed", len(w.of("alliance_betrayed"))),
         ("alliances_ended_with_realm", sum(1 for r in ended if _end_kind(w, r) == "ended")),
         ("alliances_years_median", _median([r[6] - r[5] for r in ended]) if ended else "-"),
         ("allies_joined", len(joins)),
         ("allies_joined_defence", sum(1 for j in joins if w.value(j, "side") == DEFENCE)),
         ("allies_refused", len(w.of("ally_refused"))),
         ("allies_withdrew", len(w.of("ally_withdrew"))),
         ("wars_with_ally", len(decl_with)),
         ("allied_battles", len(w.of("allied_battle")) + sum(   # P5-06 11.1: battle_led with an ally participant
             1 for b in w.of("battle_led") if w.role(b, "winner_ally") + w.role(b, "loser_ally")))]
    _series, best = w.bloc_series()
    o += [("bloc_share_max_permille", best[0] if best else 0),
          ("bloc_over_800_years", w.longest_run_above(800)),
          ("trade_opened", len(w.of("trade_opened"))),
          ("trade_standing", sum(1 for r in w.rows.get("trade", []) if r[6] is None)),
          ("trade_broken", len(w.of("trade_broken"))),
          ("alliances_spurned", len(w.of("alliance_spurned"))),
          ("spoils_owed", len(owed)), ("spoils_granted", len(granted)),
          ("spoils_withheld", len(w.of("spoils_withheld"))),
          ("spoils_granted_permille", len(granted) * 1000 // len(owed) if owed else "-"),
          ("spoils_people_owed", sum(w.value(e, "share") or 0 for e in owed)),
          ("mines_opened", len(w.of("mine_opened"))),
          ("mines_open", sum(1 for x in w.t["site"] if x[2] == SITE_MINE and x[11] is None)),
          ("arms_sold", len(w.of("arms_sold"))),
          ("arms_buyers_at_end", len(arms_buyers(w, w.E - 1)))]
    return o


def _span(r):
    return "%d-%s" % (r[5], "E" if r[6] is None else "%d" % r[6])


def _name(w, c):
    import p3_report_reference as prr
    return prr.esc(w.civ[c][1])


def alliance_line(w, r):
    a, b = r[1], r[3]
    e = r[7]
    basis = w.value(e, "basis")
    line = "  c%d %s and c%d %s: %s, %s" % (a, _name(w, a), b, _name(w, b), _span(r), BASIS_WORDS[basis])
    foe = w.role(e, "foe")
    if foe:
        line += " against c%d" % foe[0]
    line += ", e%d" % e
    # Lead's ruling on P5-04 validation finding 2: the pair's events count by
    # year inside the alliance's span [from, to] (to = E while standing), the
    # ending year included, whatever their id order against the ending event
    # (a share withheld in the year of the break, after it, counts).
    hi = r[6] if r[6] is not None else w.E
    within = lambda x: r[5] <= w.year(x) <= hi
    pair = {a, b}
    ren = sum(1 for x in w.of("alliance_renewed") if within(x) and {w.one(x, "ally_a"), w.one(x, "ally_b")} == pair)
    if ren:
        line += "; renewed %d" % ren
    j = sum(1 for x in w.of("ally_joined_war") if within(x) and {w.one(x, "ally"), w.one(x, "partner")} == pair)
    rf = sum(1 for x in w.of("ally_refused") if within(x) and {w.one(x, "ally"), w.one(x, "partner")} == pair)
    if j or rf:
        line += "; joined %d, refused %d" % (j, rf)
    g = sum(1 for x in w.of("spoils_shared") if within(x) and {w.one(x, "ally"), w.one(x, "principal")} == pair)
    k = sum(1 for x in w.of("spoils_withheld") if within(x) and {w.one(x, "ally"), w.one(x, "principal")} == pair)
    if g or k:
        line += "; spoils %d granted, %d withheld" % (g, k)
    if r[6] is not None:
        line += "; %s e%d" % (_end_kind(w, r), r[8])
    return line


def trade_line(w, r):
    a, b = r[1], r[3]
    line = "  c%d %s and c%d %s: %s, e%d" % (a, _name(w, a), b, _name(w, b), _span(r), r[7])
    hi = r[8] if r[6] is not None else None
    sells = [x for x in w.of("arms_sold") if x > r[7] and (hi is None or x < hi)
             and {w.one(x, "seller"), w.one(x, "buyer")} == {a, b}]
    if sells:
        line += "; arms from c%d" % w.one(sells[0], "seller")
    # reading (integration; spec gap, the engine's form taken): `; broken e<id>` names the row's closing event
    # whatever its kind (a trade_broken, or a side's ending event when conflict's housekeeping closed it)
    if r[6] is not None:
        line += "; broken e%d" % r[8]
    return line


def alliances_block(w, level, civs_cap=None, v9=False):
    """10.2: the [alliances] block's lines; v9: P5-04a 12.3 and 12.5's alliance and trade lines."""
    aline = alliance_line_v9 if v9 else alliance_line
    tline = trade_line_v9 if v9 else trade_line
    import p3_report_reference as prr
    civs_cap = prr.SUMMARY_LINES_CIVS if civs_cap is None else civs_cap
    al = sorted(w.rows.get("alliance", []), key=lambda r: (r[5], r[1], r[3]))
    tr = sorted(w.rows.get("trade", []), key=lambda r: (r[5], r[1], r[3]))
    st = dict(stats(w))
    ends = {k: 0 for k in ("lapsed", "broken", "spurned", "betrayed", "ended")}
    for r in al:
        if r[6] is not None:
            ends[_end_kind(w, r)] += 1
    o = ["[alliances]", "alliances: %d formed, %d standing at E; ended: %d lapsed, %d broken, %d spurned, %d betrayed, "
         "%d with a realm" % (st["alliances_formed"], st["alliances_standing"], ends["lapsed"], ends["broken"],
                              ends["spurned"], ends["betrayed"], ends["ended"])]
    few = len(w.civ) > civs_cap
    if level == "full":
        o += [aline(w, r) for r in al]
    elif not few:
        keep = [r for r in al if r[6] is None or _end_kind(w, r) in ("betrayed", "spurned")]
        o += [aline(w, r) for r in keep[:SUMMARY_ALLIANCES_MAX]]
        if len(keep) > SUMMARY_ALLIANCES_MAX:
            o.append("  and %d more" % (len(keep) - SUMMARY_ALLIANCES_MAX))
    joins = w.of("ally_joined_war")
    nd = len(w.of("war_declared"))
    o.append("calls to arms: %d joined (%d in defence, %d in attack), %d refused, %d withdrew; wars with an ally: %d of %d"
             % (len(joins), st["allies_joined_defence"], len(joins) - st["allies_joined_defence"], st["allies_refused"],
                st["allies_withdrew"], st["wars_with_ally"], nd))
    owed = w.of("spoils_shared") + w.of("spoils_withheld")
    if owed:
        o.append("spoils: %d owed, %d granted, %d withheld; %d people owed, %d granted" % (
            len(owed), st["spoils_granted"], st["spoils_withheld"], st["spoils_people_owed"],
            sum(w.value(e, "share") or 0 for e in w.of("spoils_shared"))))
    else:
        o.append("spoils: none")
    _s, best = w.bloc_series()
    if best is None:
        o.append("largest bloc: none")
    else:
        o.append("largest bloc: %d permille, c%d with %s in %d" % (best[0], best[2],
                                                                   ", ".join("c%d" % z for z in best[3]), best[1]))
    br = [r for r in tr if r[6] is not None and r[8] in w.ev and w.kind(r[8]) == "trade_broken"]
    reasons = [w.value(r[8], "reason") for r in br]
    o.append("trade: %d opened, %d standing at E; broken %d (war %d, cooled %d, out of reach %d); arms sold %d" % (
        st["trade_opened"], st["trade_standing"], st["trade_broken"], reasons.count(0), reasons.count(1),
        reasons.count(2), st["arms_sold"]))
    if level == "full":
        o += [tline(w, r) for r in tr]
    return o


def civ_alliance_lines(w, c, level):
    """10.3: the lines appended at the end of civilization c's block."""
    rows = sorted([r for r in w.rows.get("alliance", []) if c in (r[1], r[3])], key=lambda r: (r[5], r[1], r[3]))
    line = "  allies: %d" % len(rows)
    if level == "full":
        return [line] + ["    c%d %s: %s, %s" % (z, _name(w, z), _span(r), BASIS_WORDS[w.value(r[7], "basis")])
                         for r in rows for z in [r[3] if r[1] == c else r[1]]]
    now = sorted(r[3] if r[1] == c else r[1] for r in rows if r[6] is None)
    if now:
        line += "; at the end: " + ", ".join("c%d" % z for z in now)
    return [line]


def mine_site_line(w, x, ref, settlement_ref, region_ref):
    """10.6: a mine's [sites] line (the refs as the report's)."""
    import p3_report_reference as prr
    end = "open" if x[13] == 0 else "ended %d e%d %s" % (x[11], x[12], prr.SITE_ENDING_WORD[x[13]])
    return "  si%d %s: mine in %s at %d %d, since %d e%d, of %s, at %s; %s" % (
        x[0], prr.esc(x[1]), region_ref, x[4], x[5], x[9], x[10], ref, settlement_ref, end)


# ---------------------------------------------------------------------------
# Timeline reads (12)
# ---------------------------------------------------------------------------

def alliances_at(w, y):
    return sorted((r[1], r[3], r[5], r[7]) for r in w.in_force("alliance", y))


def allies_of(w, c, y):
    return sorted(r[3] if r[1] == c else r[1] for r in w.in_force("alliance", y) if c in (r[1], r[3]))


def alliance_groups_at(w, y):
    """The connected components of the alliance graph among civilizations alive at y with an ally, each ascending,
    ordered by smallest id."""
    nb = {}
    for a, b, _s, _e in alliances_at(w, y):
        if w.alive(a, y) and w.alive(b, y):
            nb.setdefault(a, set()).add(b)
            nb.setdefault(b, set()).add(a)
    seen, out = set(), []
    for c in sorted(nb):
        if c in seen:
            continue
        comp, todo = set(), [c]
        while todo:
            x = todo.pop()
            if x in comp:
                continue
            comp.add(x)
            todo.extend(nb[x] - comp)
        seen |= comp
        out.append(sorted(comp))
    return out


def trades_at(w, y):
    return sorted((r[1], r[3]) for r in w.in_force("trade", y))


def dump_lines(w, y):
    """P4-01 13.1 D(Y)'s added lines (12): `alliance <a> <b> <since>`, `trade <a> <b> <since>`, ascending."""
    return (["alliance %d %d %d" % (a, b, s) for a, b, s, _e in alliances_at(w, y)] +
            ["trade %d %d %d" % (r[1], r[3], r[5]) for r in sorted(w.in_force("trade", y), key=lambda r: (r[1], r[3]))])


def mine_markers_at(w, y):
    """11.3: (site, x, y, glyph) of every kind 3 site existing at y (half-open)."""
    return [(x[0], x[4], x[5], MINE_GLYPH) for x in w.t["site"]
            if x[2] == SITE_MINE and x[9] <= y and (x[11] is None or y < x[11])]


def mine_hover(w, x):
    """11.3: `<site name> (mine of <civilization name>), since <year>`."""
    return "%s (mine of %s), since %d" % (x[1], w.civ[x[7]][1], x[9])


# ---------------------------------------------------------------------------
# Golden file (17): per world case and fixture, the hashes of the alliance, trade and joined war rows, of
# events 63 to 78 (with their participants, causes and values), of the kind 3 site rows, and the counts of 15's
# AL, SP, TR and MC rows ([stats] keys of 10.5).
# ---------------------------------------------------------------------------

def _enc(v):
    if v is None:
        return b"\x00"
    if isinstance(v, int):
        return b"\x01" + struct.pack("<q", v)
    b = v.encode("latin-1")
    return b"\x03" + struct.pack("<Q", len(b)) + b


def rows_hash(ncols, rows):
    data = bytearray(struct.pack("<Q", ncols))
    for r in rows:
        for v in r:
            data += _enc(v)
    return hashlib.sha256(bytes(data)).hexdigest()


def case_lines(name, w):
    out = []
    joined = [r for r in w.rows.get("war", []) if r[7] in w.ev and w.kind(r[7]) == "ally_joined_war"]
    for kind, rows in (("alliance", w.rows.get("alliance", [])), ("trade", w.rows.get("trade", [])),
                       ("war_joined", joined)):
        out.append("%s rows %s %d %s" % (name, kind, len(rows), rows_hash(9, rows)))
    new = {i for i, _k in KINDS if i in w.key}
    evs = [e for e in w.t["event"] if e[3] in new]
    ids = {e[0] for e in evs}
    out.append("%s events %d %s" % (name, len(evs), rows_hash(8, evs)))
    out.append("%s participants %s" % (name, rows_hash(5, [p for p in w.t["event_participant"] if p[0] in ids])))
    out.append("%s causes %s" % (name, rows_hash(2, [c for c in w.t["event_cause"] if c[0] in ids])))
    out.append("%s values %s" % (name, rows_hash(3, [v for v in w.t["event_value"] if v[0] in ids])))
    mines = [x for x in w.t["site"] if x[2] == SITE_MINE]
    out.append("%s sites mine %d %s" % (name, len(mines), rows_hash(14, mines)))
    for k, v in stats(w):
        out.append("%s stat %s %s" % (name, k, v))
    return out


GOLDEN_HEADER = """\
# P5-04 alliances golden vectors for generator 0.27.0 (schema 12), written by
# worldgen/tests/reference/p5_04_alliances_reference.py golden from world files of the reference chain.
# <case> rows <alliance|trade|war_joined> <n> <sha256 of the rows, P0-04 8's encoding, 9 columns>
# <case> events <n> <sha256 of the event rows of kinds 63 to 78>; participants, causes, values of those events
# <case> sites mine <n> <sha256 of the kind 3 site rows>
# <case> stat <key> <value>   (P5-04 10.5's [stats] keys)
"""


def golden_text(inputs):
    names = sorted(n[:-3] for n in os.listdir(inputs) if n.endswith(".db"))
    out = [GOLDEN_HEADER.rstrip("\n")]
    for n in names:
        out += case_lines(n, aw_of(os.path.join(inputs, n + ".db")))
    return "\n".join(out) + "\n"


# ---------------------------------------------------------------------------
# The replay (sections 5, 6): AlliancesSubsystem inside p3_conflict_reference.World, after the relations
# subsystem in T2, T6 and Q4; the conflict seams of 7 are in that World (allied, allies_of, bloc_share,
# strengthened, open_wars_of, join_war, side_forces, allies_at_end, withdrawals, war_spoils, ...).
# ---------------------------------------------------------------------------

def _pcr():
    import p3_conflict_reference as pcr
    return pcr


class Alliances:
    """AlliancesSubsystem (5, 6) and its AllianceView (7.1) for a conflict World w."""

    def __init__(self, w, dat, inp):
        import core_reference as cr
        self.w = w
        self.t = dat.alliances
        self.dat = dat
        for s in STREAMS:
            w.keys[s] = cr.stream_key(inp.seed, s)
        key_id = {dat.race[r]["key"]: r for r in dat.race}
        self.origin = {r: {key_id[k] for k in dat.race[r].get("origin_allies", []) if k in key_id}
                       for r in dat.race}
        self.alliances = {}       # (x, y) -> record (5)
        self.betrayed = set()     # (x, y)
        self.trades = {}          # (x, y) -> dict(year, event, next)
        self.joined = []          # records in record order
        self.mines = {}           # (c, R) -> (site id, settlement)
        self.had_mine = set()     # settlements that ever had a mine
        self.arms_sold = set()    # (seller, buyer)
        self.arms_ev = {}         # P5-04a 9.2: (seller, buyer) -> its arms_sold event
        self.r28 = _pcr().FLAGS.get("p504a", False)   # P5-04a (generator 0.28.0)
        if self.r28:
            w.keys[STREAM_RENEW] = cr.stream_key(inp.seed, STREAM_RENEW)
        self.cursor = 1
        self.closed_now = set()   # reading A9: pairs whose alliance row closed in this step
        self.trade_closed_now = set()   # ... whose trade row closed in this step
        self.step_key = None
        self.pres = {}
        self.shares = {}
        self.sellers0 = set()
        self.log = []             # (year, what, ...) for the checks (not state)
        self.renewals_capped = 0  # P5-08 12.5: quiet renewals made by renew_written_max (a statistic, not state)

    # --- the AllianceView (7.1): the records until this subsystem drops them (reading A7)
    def allied(self, a, b):
        return (min(a, b), max(a, b)) in self.alliances

    def allies(self, c):
        return sorted(({x for x, y in self.alliances if y == c} | {y for x, y in self.alliances if x == c}))

    def trade_partners(self, c):
        return sum(1 for x, y in self.trades if c in (x, y))

    def partners(self, c):
        """P5-08 7.4: AllianceView::partners(c): every civilization with an open alliance or trade with c, ascending,
        with its opening event (the alliance's when both; 0 for an origin alliance)."""
        out = {}
        for (x, y), rec in self.trades.items():
            if c in (x, y):
                out[y if x == c else x] = rec["event"]
        for (x, y), rec in self.alliances.items():
            if c in (x, y):
                out[y if x == c else x] = 0 if rec.get("origin") else rec["event"]
        return sorted(out.items())

    def sellers(self):
        return {c for (c, _R) in self.mines}

    def forged(self, c):
        return any(k[0] == c for k in self.mines)

    def arms(self, c):
        if self.forged(c):
            return False
        sel = self.sellers()
        return any((y if x == c else x) in sel for x, y in self.trades if c in (x, y))

    def arms_event(self, c):
        """P5-04a 9.2: AllianceView::arms_event(c): when c is a buyer, the arms_sold event of the smallest-id seller
        s with an open trade (s, c) whose pair is in the arms-sold set; else 0."""
        if not self.arms(c):
            return 0
        sel = self.sellers()
        for s_ in sorted(sel):
            if (min(s_, c), max(s_, c)) in self.trades and (s_, c) in self.arms_sold:
                return self.arms_ev.get((s_, c), 0)
        return 0

    # --- terms (6.2)
    def quiet(self):
        return self.w.quiet_used is not None

    def budget(self, c):
        return not self.quiet() or self.w.quiet_events_left(c) > 0

    def avf(self, c):
        """Alive, visible and free."""
        w = self.w
        cv = w.civ(c)
        return cv["ended_year"] is None and w.visible(cv) and not w.hides(c)

    def av(self, c):
        cv = self.w.civ(c)
        return cv["ended_year"] is None and self.w.visible(cv)

    def ended(self, c):
        return self.w.civ(c)["ended_year"] is not None

    def cache(self):
        """6.2: presences and shares at the step's start; the sellers of 6.13 at the step's start."""
        w = self.w
        self.pres = {}
        for cv in w.civs:
            c = cv["id"]
            if cv["ended_year"] is None and w.visible(cv):
                self.pres[c] = sorted(set(w.held(c)) | {s["region"] for s in w.svis(c)})
        self.shares = {c: w.share(c) for c in self.pres}
        # lead ruling (integration): fear reads the bloc shares of the step's start, the bloc test reads them live
        self.bs0 = {c: self.bs(c) for c in self.pres}
        self.hops_cache = {}
        self.sellers0 = self.sellers()
        self.closed_now = set()
        self.trade_closed_now = set()

    def s(self, c):
        return self.shares.get(c, 0)

    def bs(self, c):
        return min(1000, self.s(c) + sum(self.s(z) for z in self.allies(c)))

    def hops(self, x, y):
        k = (min(x, y), max(x, y))
        if k not in self.hops_cache:
            best = None
            geo = self.w.geo
            py = self.pres.get(y, [])
            for rx in self.pres.get(x, []):
                h = geo.hops(rx)
                for ry in py:
                    v = h.get(ry)
                    if v is not None and (best is None or v < best):
                        best = v
            self.hops_cache[k] = best
        return self.hops_cache[k]

    def in_reach(self, x, y):
        return in_reach(self.hops(x, y), self.t["ally_hops"])

    def in_trade_reach(self, x, y):
        return in_reach(self.hops(x, y), self.t["trade_hops"])

    def d(self, x, y):
        return self.w.disp(x, y)

    def warmth(self, x, y):
        t = self.t
        return warmth(self.d(x, y), t["ally_min_disposition"], t["ally_full_disposition"])

    def lately_at_war(self, x, y, years):
        w = self.w
        if w.at_war(x, y):
            return True
        lp = w.last_peace.get((min(x, y), max(x, y)))
        return lp is not None and lp[0] > w.year - years

    def race(self, c):
        return self.w.race_c(c)

    def credit(self, a, b, units, e):
        """6.9: RelationsSubsystem::credit (acts only when the races differ and have met)."""
        rel = self.w.race_relations
        ra, rb = self.race(a), self.race(b)
        if rel is None or ra == rb:
            return
        rel.credit(ra, rb, units, e)
        self.log.append((self.w.year, "credit", ra, rb, units, e))

    def credit_units(self, a, b, units):
        """P5-07 7.5: RelationsSubsystem::credit_units (no contribution kept)."""
        rel = self.w.race_relations
        ra, rb = self.race(a), self.race(b)
        if rel is None or ra == rb:
            return
        rel.credit_units(ra, rb, units)
        self.log.append((self.w.year, "credit-units", ra, rb, units))

    def younger_founding(self, x, y):
        return max(self.w.civ(x)["founding_event"], self.w.civ(y)["founding_event"])

    def reasons(self, x, y):
        """6.2: {reason: (cause event, foe)} of the pair (x, y)."""
        w, t = self.w, self.t
        Y = w.year
        out = {}
        best = None
        for war in w.wars:
            if not (war["open"] or war["start"] > Y - t["foe_years"]):
                continue
            for v in (x, y):
                if v not in (war["a"], war["b"]):
                    continue
                z = war["b"] if war["a"] == v else war["a"]
                if z in (x, y) or not self.av(z):
                    continue
                o = y if v == x else x
                if self.d(o, z) < 0 and self.in_reach(o, z):
                    if best is None or war["w"] > best[0]:
                        best = (war["w"], z)
        if best is not None:
            out[COMMON_ENEMY] = best
        fz = None
        for z in sorted(self.pres):
            if z in (x, y) or not self.av(z) or self.allied(z, x) or self.allied(z, y):
                continue
            b = self.bs0.get(z, 0)
            if b < t["fear_share"] or not (self.in_reach(x, z) and self.in_reach(y, z)):
                continue
            if self.d(x, z) < 0 and self.d(y, z) < 0 and (fz is None or b > fz[0]):
                fz = (b, z)
        if fz is not None:
            z = fz[1]
            evs = [w.owner[r][1] for r in w.held(z)]
            # reading A10: a feared bloc whose head holds no region: its founding event
            out[FEAR] = (max(evs) if evs else w.civ(z)["founding_event"], z)
        tr = self.trades.get((x, y))
        if tr is not None and Y - tr["year"] >= t["trade_ally_years"]:
            out[TRADE] = (tr["event"], 0)
        if self.d(x, y) >= t["ally_friend_disposition"]:
            cause = self.younger_founding(x, y)
            rel = w.race_relations
            ra, rb = self.race(x), self.race(y)
            if rel is not None and ra != rb:
                p = rel.pair(ra, rb)
                import p5_03_relations_reference as p503
                if p["band"] == p503.FRIENDSHIP and p["turn_event"]:
                    cause = max(cause, p["turn_event"])
            out[FRIENDSHIP] = (cause, 0)
        return out

    def term_event(self, k):
        return self.alliances[k]["renew_event"]

    # --- relations (6.10 item 1: through the symmetric helper, on key copies)
    def open_alliance(self, x, y, e):
        self.w.open_rel(_pcr().sym_key(x, y, "alliance"), e)

    def close_alliance(self, k, e):
        self.w.close_rel(_pcr().sym_key(k[0], k[1], "alliance"), e)
        del self.alliances[k]
        self.closed_now.add(k)

    def close_trade(self, k, e):
        self.w.close_rel(_pcr().sym_key(k[0], k[1], "trade"), e)
        del self.trades[k]
        self.trade_closed_now.add(k)
        for p in ((k[0], k[1]), (k[1], k[0])):
            self.arms_sold.discard(p)
            self.arms_ev.pop(p, None)

    def drop_closed(self):
        """6.7, 6.8: an alliance or trade whose row conflict closed (a side's end) is dropped with no event."""
        rel = self.w.relations
        pcr = _pcr()
        for k in sorted(self.alliances):
            if pcr.sym_key(k[0], k[1], "alliance") not in rel:
                del self.alliances[k]
                self.log.append((self.w.year, "dropped-alliance", k))
        for k in sorted(self.trades):
            if pcr.sym_key(k[0], k[1], "trade") not in rel:
                del self.trades[k]
                for p in ((k[0], k[1]), (k[1], k[0])):
                    self.arms_sold.discard(p)
                    self.arms_ev.pop(p, None)

    def inert(self, k):
        """Reading A8, as ruled by the lead at integration: not special-cased; the subsystem acts on every row
        that conflict's housekeeping left open."""
        return False

    # --- 6.5, 6.6, 6.8 broken by war: the log
    def read_log(self):
        w = self.w
        last = len(w.events)
        first = self.cursor
        self.cursor = last + 1
        for e in range(first, last + 1):
            ev = w.events[e - 1]
            if ev["key"] != "war_declared":
                continue
            a = [x for (_i, rs, _t, x) in ev["participants"] if rs == 1][0]
            b = [x for (_i, rs, _t, x) in ev["participants"] if rs == 2][0]
            self.betrayal(e, a, b)
            self.trade_war(e, a, b)
            self.calls(e, a, b)

    def betrayal(self, wd, a, b):
        k = (min(a, b), max(a, b))
        if k not in self.alliances:
            return
        vals = ()
        if self.r28:   # P5-04a 5: the motive, the first that holds, and the alliance's age
            t, w = self.t, self.w
            fv = w.figures
            motive = betrayal_motive(fv.temper(a) if fv is not None else -1, self.d(a, b), w.force(a), w.force(b),
                                     t["betray_cooled_disposition"], t["betray_weak_permille"])
            vals = (motive, w.year - self.alliances[k]["year"])
        e = self.w.append("alliance_betrayed", participants=(("betrayer", CIV, a), ("betrayed", CIV, b)),
                          values=vals, causes=tuple(sorted({wd, self.term_event(k)})))
        self.close_alliance(k, e)
        self.betrayed.add(k)
        self.credit(a, b, -self.t["betray_memory"], e)
        self.log.append((self.w.year, "betrayed", a, b, e))

    def trade_war(self, wd, a, b):
        k = (min(a, b), max(a, b))
        if k not in self.trades:
            return
        e = self.w.append("trade_broken", participants=(("partner_a", CIV, k[0]), ("partner_b", CIV, k[1])),
                          values=(0,), causes=(wd,))
        self.close_trade(k, e)

    def calls(self, wd, a, b):
        w, t, ct = self.w, self.t, self.w.ct
        Y = w.year
        for side, p, e in ((DEFENCE, b, a), (ATTACK, a, b)):
            for z in self.allies(p):
                k = (min(z, p), max(z, p))
                if k not in self.alliances:
                    continue        # broken earlier in this declaration's calls
                if not self.avf(z) or z in (a, b) or self.allied(z, e):
                    continue
                if w.at_war(z, e) or w.at_war(z, p) or not w.war_open(wd) or not w.reaches(z, e):
                    continue
                if Y < w.civ(e)["founded_year"] + ct["shelter_years"]:
                    continue
                if w.open_wars_of(z) >= t["ally_wars_max"]:
                    continue
                join = self.dat.race[self.race(z)]["ally_join"]
                rec = self.alliances[k]
                fs = None
                if side == ATTACK:
                    if not w.past_truce(z, e) or self.d(z, e) >= 0:
                        continue
                    pch = attack_chance(t["attack_permille"], join, self.d(z, e), t["hostility_full"])
                    if self.r28:   # P5-04a 4.2: times f1, f2 and f5 in that order
                        fs = self.factors(z, p, e, rec, join)
                        for kk in (1, 2, 5):
                            pch = pch * fs[kk] // 1000
                elif self.r28:     # P5-04a 4.2: the defence chance as eight factors
                    fs = self.factors(z, p, e, rec, join)
                    pch = factored_chance(t["defend_permille"], fs)
                else:
                    pch = defence_chance(t["defend_permille"], join, self.warmth(z, p), w.force(z), w.force(p),
                                         w.force(e))
                joined = _pcr().chance_pm(w.draw("hist.alliances.join", wd, z, 0), pch)
                if self.r28 and side == DEFENCE and rec.get("call_due") and rec["soured"]["z"] == z:
                    rec["call_due"] = False   # 3.5: the soured call, joined or refused
                if joined:
                    ej = w.append("ally_joined_war", participants=(("ally", CIV, z), ("partner", CIV, p),
                                                                    ("enemy", CIV, e)), values=(side,),
                                  causes=tuple(sorted({wd, rec["renew_event"]})))
                    w.join_war(wd, z, side, ej)
                    if self.r28:
                        rec["fought"] = ej    # 6.2 item 1: the pair fought together (the latest join)
                    self.joined.append(dict(w=wd, z=z, p=p, e=e, join=ej, credited=False, settled=False))
                    self.credit(z, p, t["join_memory"], ej)
                    self.log.append((Y, "joined", wd, z, side, pch))
                elif side == DEFENCE and not rec["origin"] and self.r28:
                    self.refused(wd, z, p, e, k, rec, fs)
                elif side == DEFENCE and not rec["origin"]:
                    er = w.append("ally_refused", participants=(("ally", CIV, z), ("partner", CIV, p),
                                                                 ("enemy", CIV, e)),
                                  causes=tuple(sorted({wd, rec["renew_event"]})))
                    rec["refusals"] += 1
                    self.credit(z, p, -t["refuse_memory"], er)
                    if rec["refusals"] >= t["refusals_to_break"]:
                        eb = w.append("alliance_broken", participants=(("ally_a", CIV, k[0]), ("ally_b", CIV, k[1])),
                                      values=(0,), causes=(er,))
                        self.close_alliance(k, eb)

    # --- P5-04a 4.2, 4.3: the factors of a call and a refusal with its motive
    def factors(self, z, p, e, rec, join):
        """[f0, ..., f7] of a call to z (partner p, enemy e) of alliance record rec."""
        w, t = self.w, self.t
        fv = w.figures
        lwe = w.last_war_end(z)
        weary = lwe[1] != 0 and lwe[0] >= w.year - t["weary_years"]   # reading R2: no war ended, not weary
        sigma = min(1000, (w.force(z) + w.force(p)) * 1000 // max(1, w.force(e)))
        soured = rec.get("call_due") and rec["soured"] is not None and rec["soured"]["z"] == z
        return call_factors(t, join, weary, w.open_wars_of(z), sigma, self.warmth(z, p),
                            fv.temper(z) if fv is not None else -1, w.has_claim(z, p), soured)

    def refused(self, wd, z, p, e, k, rec, fs):
        w, t = self.w, self.t
        motive = refusal_motive(fs)
        causes = {wd, rec["renew_event"]}
        if motive == 1:
            causes.add(w.last_war_end(z)[1])
        elif motive == 6:
            causes.add(w.claim_event(z, p))
        elif motive == 7:
            causes.add(rec["soured"]["e"])
        causes.discard(0)
        excused = motive in (1, 2)
        count = 0 if excused else rec["refusals"] + 1
        er = w.append("ally_refused", participants=(("ally", CIV, z), ("partner", CIV, p), ("enemy", CIV, e)),
                      values=(motive, count), causes=tuple(sorted(causes)))
        self.log.append((w.year, "refused", wd, z, p, motive, count))
        if excused:
            self.credit(z, p, -t["excused_memory"], er)
            return
        rec["refusals"] = count
        self.credit(z, p, -t["refuse_memory"], er)
        w.add_grudge(z, p, t["refuse_grudge"])
        if rec["refusals"] >= t["refusals_to_break"]:
            eb = w.append("alliance_broken", participants=(("ally_a", CIV, k[0]), ("ally_b", CIV, k[1])),
                          values=(0,), causes=(er,))
            self.close_alliance(k, eb)

    # --- P5-04a 3.4, 3.5, 6: the spurnings and renewals of 0.28.0
    def spurn_028(self, k, rec, z, region, we, reason, years):
        P = k[0] if z == k[1] else k[1]
        e = self.w.append("alliance_spurned", region=region, participants=(("ally", CIV, z), ("principal", CIV, P)),
                          values=(reason, years), causes=tuple(sorted({we, rec["renew_event"]})))
        self.close_alliance(k, e)
        self.log.append((self.w.year, "spurned", k, reason, years, e))
        return e

    def pending_spurnings(self):
        """3.4 item 1: a spurning at once that waited for its ally's quiet budget (Q4), at the next T2 or Q4."""
        w = self.w
        for k in sorted(self.alliances):
            rec = self.alliances.get(k)
            if rec is None or rec.get("spurn_pending") is None:
                continue
            we, wy, region, z = rec["spurn_pending"]
            if not self.budget(z):
                continue
            self.spurn_028(k, rec, z, region, we, 2, w.year - wy)

    def renew_028(self, k, rec):
        """6.1, 6.2, 3.5: the renewal of the due year d of a non-origin alliance; True when it was dealt with."""
        w, t = self.w, self.t
        d = rec["due"]
        if rec.get("renewal_due"):
            sz = rec["soured"]["z"]
            if not self.budget(sz):
                return
            ok = _pcr().chance_pm(w.draw(STREAM_RENEW, k[0], k[1], d), t["soured_renew_permille"])
            rec["renewal_due"] = False
            if not ok:
                so = rec["soured"]
                self.spurn_028(k, rec, sz, so["region"], so["e"], 0, w.year - so["year"])
                return
        rs = self.reasons(k[0], k[1])
        if not rs:
            if not self.budget(k[0]):
                return
            e = w.append("alliance_lapsed", participants=(("ally_a", CIV, k[0]), ("ally_b", CIV, k[1])),
                         values=(0,), causes=(rec["renew_event"],))
            self.close_alliance(k, e)
            return
        basis = basis_of(rs)
        foe = rs[basis][1] if basis in (COMMON_ENEMY, FEAR) else 0
        notable = [x for x in (rec.get("fought"), rec.get("shared"),
                               rs[basis][0] if basis != rec["basis_R"] else None) if x]
        cap = t.get("renew_written_max", 0) if _pcr().FLAGS.get("p508", False) else 0
        capped = bool(notable) and cap > 0 and rec.get("written", 0) >= cap   # P5-08 12.5
        if capped:
            self.renewals_capped += 1
        if notable and not capped and self.budget(k[0]):
            e = w.append("alliance_renewed",
                         participants=(("ally_a", CIV, k[0]), ("ally_b", CIV, k[1])) +
                         ((("foe", CIV, foe),) if foe else ()), values=(basis,),
                         causes=tuple(sorted(set(notable) | {rec["renew_event"]})))
            rec["renew_event"], rec["basis_R"], rec["fought"], rec["shared"] = e, basis, None, None
            rec["renew_year"] = w.year
            rec["written"] = rec.get("written", 0) + 1
            self.credit(k[0], k[1], t["renew_memory"], e)
            self.log.append((w.year, "renewed", k, d, e))
        else:
            # a renewal with nothing notable (or a notable one without budget: quietly) writes no event; the credit
            # cites R (P5-03a audit A4)
            if _pcr().FLAGS.get("a1_units", False):   # P5-07 7.5 (audit A1): a quiet renewal cites no event
                self.credit_units(k[0], k[1], t["renew_memory"])
            else:
                self.credit(k[0], k[1], t["renew_memory"], rec["renew_event"])
            self.log.append((w.year, "renewed-quietly", k, d, bool(notable)))
        rec["due"] = d + rec["term"]

    # --- 6.7 keeping
    def keeping(self):
        w, t = self.w, self.t
        quiet = self.quiet()
        if self.r28:
            self.pending_spurnings()
        for k in sorted(self.alliances):          # 1 cooling
            if k not in self.alliances or self.inert(k):
                continue
            rec = self.alliances[k]
            if rec["origin"] or self.d(k[0], k[1]) >= t["ally_break_disposition"]:
                continue
            if self.r28:   # P5-04a 3.5: a soured alliance is spurned (reason 1, years since the souring)
                so = rec["soured"]
                if so is None:
                    if not self.budget(k[0]):
                        continue
                    e = w.append("alliance_broken", participants=(("ally_a", CIV, k[0]), ("ally_b", CIV, k[1])),
                                 values=(1,), causes=(rec["renew_event"],))
                    self.close_alliance(k, e)
                else:
                    if not self.budget(so["z"]):
                        continue
                    self.spurn_028(k, rec, so["z"], so["region"], so["e"], 1, w.year - so["year"])
                continue
            if rec["withheld"] == 0:
                if not self.budget(k[0]):
                    continue
                e = w.append("alliance_broken", participants=(("ally_a", CIV, k[0]), ("ally_b", CIV, k[1])),
                             values=(1,), causes=(rec["renew_event"],))
            else:
                if not self.budget(rec["w_ally"]):
                    continue
                e = self.spurned(k, rec, 1)
            self.close_alliance(k, e)
        for c in sorted({x for k in self.alliances for x in k}):   # 2 blocs too great
            while self.bs(c) > t["bloc_lapse_share"]:
                zs = [z for z in self.allies(c) if not self.inert((min(c, z), max(c, z)))]
                if not zs:
                    break
                z = min(zs, key=lambda z: (self.s(z), -z))
                k = (min(c, z), max(c, z))
                if not self.budget(k[0]):
                    break
                e = w.append("alliance_lapsed", participants=(("ally_a", CIV, k[0]), ("ally_b", CIV, k[1])),
                             values=(1,), causes=(self.alliances[k]["renew_event"],))
                self.close_alliance(k, e)
        for k in sorted(self.alliances):          # 3 renewal
            if k not in self.alliances or self.inert(k):
                continue
            rec = self.alliances[k]
            if self.r28:   # P5-04a 6.1: the fixed chain of due years; Y >= d in T2 and Q4 alike
                if w.year < rec["due"]:
                    continue
                if rec["origin"]:
                    rec["due"] += rec["term"]
                    continue
                self.renew_028(k, rec)
                continue
            if not renewal_due(w.year, rec["renew_year"], t["term_years"], w.years if quiet else None):
                continue
            if rec["origin"]:
                rec["renew_year"] = w.year
                rec["withheld"] = 0
                continue
            if rec["withheld"] >= t["withheld_to_break"]:
                if not self.budget(rec["w_ally"]):
                    continue
                e = self.spurned(k, rec, 0)
                self.close_alliance(k, e)
                continue
            rs = self.reasons(k[0], k[1])
            if not self.budget(k[0]):
                continue
            if rs:
                basis = basis_of(rs)
                foe = rs[basis][1] if basis in (COMMON_ENEMY, FEAR) else 0
                e = w.append("alliance_renewed",
                             participants=(("ally_a", CIV, k[0]), ("ally_b", CIV, k[1])) +
                             ((("foe", CIV, foe),) if foe else ()), values=(basis,),
                             causes=tuple(sorted({v[0] for v in rs.values()} | {rec["renew_event"]})))
                rec["renew_year"], rec["renew_event"], rec["withheld"] = w.year, e, 0
                self.credit(k[0], k[1], t["renew_memory"], e)
            else:
                e = w.append("alliance_lapsed", participants=(("ally_a", CIV, k[0]), ("ally_b", CIV, k[1])),
                             values=(0,), causes=(rec["renew_event"],))
                self.close_alliance(k, e)

    def spurned(self, k, rec, reason):
        z = rec["w_ally"]
        P = k[0] if z == k[1] else k[1]
        return self.w.append("alliance_spurned", region=rec["w_region"],
                             participants=(("ally", CIV, z), ("principal", CIV, P)), values=(reason,),
                             causes=tuple(sorted({rec["w_event"], rec["renew_event"]})))

    # --- 6.3, 6.4 formation
    def bloc_ok(self, x, y):
        return bloc_test(self.bs(x), self.s(x), [self.s(z) for z in self.allies(x)],
                         self.bs(y), self.s(y), [self.s(z) for z in self.allies(y)], self.t["bloc_share_max"])

    def form(self, x, y, basis, foe, causes, origin):
        w, t = self.w, self.t
        vals = (basis,)
        if self.r28:   # P5-04a 6.1: term; 10 item 4: the band of the two peoples (2 for one race)
            rel = w.race_relations
            ra, rb = self.race(x), self.race(y)
            band = 2 if ra == rb or rel is None else rel.band(ra, rb)
            vals = (basis, t["term_years"], band)
        e = w.append("alliance_formed", participants=(("ally_a", CIV, x), ("ally_b", CIV, y)) +
                     ((("foe", CIV, foe),) if foe else ()), values=vals, causes=tuple(sorted(set(causes))))
        self.open_alliance(x, y, e)
        # lead ruling (integration): at every alliance_formed, origin included, the pair's strain and claims on each
        # other are dropped both ways
        w.zero_strain(x, y)
        w.drop_claims(x, y)
        self.alliances[(x, y)] = dict(year=w.year, event=e, basis=basis, foe=foe, origin=origin, renew_year=w.year,
                                      renew_event=e, refusals=0, withheld=0, w_event=0, w_region=0, w_ally=0,
                                      # P5-04a 3.2, 6.1, 6.2
                                      due=w.year + t["term_years"], term=t["term_years"], basis_R=basis,
                                      soured=None, renewal_due=False, call_due=False, spurn_pending=None,
                                      fought=None, shared=None)
        self.credit(x, y, t["ally_memory"], e)
        self.log.append((w.year, "formed", x, y, basis, e))

    def candidates(self):
        cs = [c for c in sorted(self.pres) if self.avf(c)]
        return [(x, y) for i, x in enumerate(cs) for y in cs[i + 1:]]

    def origins(self):
        t = self.t
        for x, y in self.candidates():
            k = (x, y)
            if k in self.alliances or k in self.betrayed or k in self.closed_now or self.w.at_war(x, y):
                continue
            rx, ry = self.race(x), self.race(y)
            if ry not in self.origin.get(rx, ()) or rx not in self.origin.get(ry, ()):
                continue
            if len(self.allies(x)) >= t["allies_max"] or len(self.allies(y)) >= t["allies_max"]:
                continue
            if not self.bloc_ok(x, y):
                continue
            self.form(x, y, ORIGIN, 0, [self.younger_founding(x, y)], True)

    def formations(self):
        w, t = self.w, self.t
        for x, y in self.candidates():
            k = (x, y)
            if k in self.alliances or k in self.betrayed or k in self.closed_now:
                continue
            if self.lately_at_war(x, y, t["ally_after_war_years"]) or not self.in_reach(x, y):
                continue
            if self.d(x, y) < t["ally_min_disposition"]:
                continue
            if len(self.allies(x)) >= t["allies_max"] or len(self.allies(y)) >= t["allies_max"]:
                continue
            rs = self.reasons(x, y)
            p = form_chance(set(rs), self.warmth(x, y), t)
            if p <= 0 or not self.bloc_ok(x, y):
                continue
            if not _pcr().chance_pm(w.draw("hist.alliances.form", x, y, 0), p):
                continue
            basis = basis_of(rs)
            foe = rs[basis][1] if basis in (COMMON_ENEMY, FEAR) else 0
            # P5-04a 10 item 2: the causes are the basis reason's cause event only (P5-04 6.3 cited every reason)
            self.form(x, y, basis, foe, [rs[basis][0]] if self.r28 else [v[0] for v in rs.values()], False)

    # --- 6.8 trade
    def trade_breaks(self):
        w, t = self.w, self.t
        for k in sorted(self.trades):
            if k not in self.trades or self.inert(k):
                continue
            tr = self.trades[k]
            if self.d(k[0], k[1]) < t["trade_min_disposition"] - t["trade_break_margin"]:
                reason = 1
            elif trade_out_of_reach(self.hops(k[0], k[1]), t["trade_hops"]):
                reason = 2
            else:
                continue
            if not self.budget(k[0]):
                continue
            e = w.append("trade_broken", participants=(("partner_a", CIV, k[0]), ("partner_b", CIV, k[1])),
                         values=(reason,), causes=(tr["event"],))
            self.close_trade(k, e)

    def n_trades(self, c):
        return self.trade_partners(c)

    def trade_openings(self):
        w, t = self.w, self.t
        quiet = self.quiet()
        for x, y in self.candidates():
            k = (x, y)
            if k in self.trades or k in self.trade_closed_now:
                continue
            if self.lately_at_war(x, y, t["trade_after_war_years"]) or not self.in_trade_reach(x, y):
                continue
            dxy = self.d(x, y)
            if dxy < t["trade_min_disposition"]:
                continue
            if self.n_trades(x) >= t["trade_max"] or self.n_trades(y) >= t["trade_max"]:
                continue
            wt = trade_weight(dxy, t["trade_min_disposition"], t["ally_full_disposition"])
            one = (x in self.sellers0) != (y in self.sellers0)
            p = trade_chance(t["trade_permille"], wt, w.years if quiet else None, one, t["arms_trade_permille"])
            if not _pcr().chance_pm(w.draw("hist.alliances.trade", x, y, 0), p):
                continue
            if not self.budget(x):
                continue
            causes = [self.younger_founding(x, y)]
            lp = w.last_peace.get(k)
            if lp is not None:
                causes.append(lp[1])
            rel = w.race_relations
            if rel is not None and self.race(x) != self.race(y):
                pr = rel.pair(self.race(x), self.race(y))
                if pr["met_event"]:
                    causes.append(pr["met_event"])
            e = w.append("trade_opened", participants=(("partner_a", CIV, x), ("partner_b", CIV, y)),
                         causes=(max(causes),))
            w.open_rel(_pcr().sym_key(x, y, "trade"), e)
            self.trades[k] = dict(year=w.year, event=e, next=w.year + t["trade_credit_years"])

    def trade_credits(self):
        w, t = self.w, self.t
        quiet = self.quiet()
        for k in sorted(self.trades):
            tr = self.trades[k]
            if self.inert(k):
                continue
            n, nxt = trade_credit_periods(w.year, tr["next"], t["trade_credit_years"], w.years if quiet else None)
            if n == 0:
                continue
            self.credit(k[0], k[1], t["trade_memory"] * n, tr["event"])
            tr["next"] = nxt

    # --- 6.12 mines (part C), through the monsters' sites (7.16)
    def mine_closings(self):
        w = self.w
        m = w.monsters
        for (c, R), (sid, ts) in sorted(self.mines.items(), key=lambda kv: kv[1][0]):
            st = w.stl(ts)
            if self.ended(c):
                ev, ending = w.civ(c)["ending_event"], ENDING_FELL
            elif not w.standing(st):
                ev, ending = st["ruin_event"], ENDING_LOST
            elif st["owner_civ"] != c:
                ev, ending = st["owner_event"], ENDING_LOST
            elif w.owner[R][0] != c:
                ev, ending = w.owner[R][1], ENDING_LOST
            else:
                continue
            m.end_site(sid, ev, ending, year=w.events[ev - 1]["year"])
            del self.mines[(c, R)]
            self.log.append((w.year, "mine-closed", sid, ending, ev))

    def mine_openings(self):
        w, t = self.w, self.t
        quiet = self.quiet()
        m = w.monsters
        for c in sorted(self.pres):
            if not self.avf(c):
                continue
            mp = self.dat.race[self.race(c)]["mining"]
            if mp <= 0:
                continue
            for R in sorted(w.held(c)):
                if w.geo.reg[R]["ore_reach_cells"] < t["mine_ore_min"] or (c, R) in self.mines:
                    continue
                ts = [s for s in w.standing_of(c) if s["region"] == R and s["id"] not in self.had_mine]
                if not ts:
                    continue
                st = min(ts, key=lambda s: (-s["population"], s["id"]))
                if st["population"] < t["mine_settlement_min"]:
                    continue
                p = mine_chance(t["mine_permille"], mp, w.years if quiet else None)
                if not _pcr().chance_pm(w.draw("hist.alliances.mine", c, R, 0), p):
                    continue
                if not self.budget(c):
                    continue
                sid = len(m.sites) + 1
                e = w.append("mine_opened", region=R, settlement=st["id"],
                             participants=(("civilization", CIV, c), ("site", SITE, sid)),
                             causes=(max(w.owner[R][1], st["founding_event"]),))
                m.sites.append(dict(id=sid, name=mine_name(t["mine_forms"], sid, st["name"]), kind=SITE_MINE,
                                    region=R, x=st["x"], y=st["y"], settlement=st["id"], founder=c, monster=0,
                                    origin_year=w.year, origin_event=e, ended_year=None, ending_event=0,
                                    ending=0, hostile=None))
                self.mines[(c, R)] = (sid, st["id"])
                self.had_mine.add(st["id"])
                self.log.append((w.year, "mine", sid, c, R, st["id"]))

    def arms_sales(self):
        w, t = self.w, self.t
        sel = self.sellers()
        for k in sorted(self.trades):
            if self.inert(k):
                continue
            x, y = k
            if (x in sel) == (y in sel):
                continue
            s_, b_ = (x, y) if x in sel else (y, x)
            if (s_, b_) in self.arms_sold:
                continue
            if not self.budget(s_):
                continue
            sid = min(v[0] for kk, v in self.mines.items() if kk[0] == s_)
            site = w.monsters.site(sid)
            e = w.append("arms_sold", participants=(("seller", CIV, s_), ("buyer", CIV, b_), ("site", SITE, sid)),
                         causes=tuple(sorted({self.trades[k]["event"], site["origin_event"]})))
            self.arms_sold.add((s_, b_))
            self.arms_ev[(s_, b_)] = e
            self.credit(s_, b_, t["trade_memory"], e)

    # --- 6.9, 6.11: ends of joined wars
    def joined_ends(self):
        w, t = self.w, self.t
        for r in self.joined:
            if not r["settled"]:
                self.spoils(r)
            if not r["credited"]:
                rec = w.ally_record(r["w"], r["z"])
                if not w.war_open(r["w"]) or not rec["open"]:
                    self.credit(r["z"], r["e"], -t["ally_war_memory"], rec["close_event"])
                    r["credited"] = True

    def spoils(self, r):
        w, t = self.w, self.t
        if w.war_open(r["w"]):
            return
        sp = w.war_spoils(r["w"])
        W = sp["winner"]
        V = spoils_value(sp["tribute_people"], sp["plunder"], t["spoils_plunder_permille"])
        war = w.war_of(r["w"])

        # lead ruling (integration): a share is owed only when the war ends by peace, war_won or civ_conquered
        ek = w.events[sp["end_event"] - 1]["key"] if sp["end_event"] else None
        cited = ek in ("peace", "war_won")
        if ek == "civ_conquered":
            # 13 item 11 (validation finding 1, the lead's ruling): the end event's own winner is the principal; a
            # conquest of the enemy by a third realm owes nothing
            ev = w.events[sp["end_event"] - 1]
            seq = [q["seq"] for q in w.cat_by_key["civ_conquered"]["roles"] if q["role"] == "conqueror"][0]
            cited = [x for (_i, s_, _typ, x) in ev["participants"] if s_ == seq] == [W]

        def owed(q):
            return (cited and W == q["partner"] and q["closed_by"] == "end" and not self.ended(q["z"]) and W
                    and not self.ended(W) and q["dead"] > 0 and V > 0)
        recs = [q for q in war["allies"] if owed(q)]
        me = w.ally_record(r["w"], r["z"])
        if me not in recs:
            r["settled"] = True
            return
        shares = spoils_shares(V, [q["dead"] for q in recs], sp["side_dead"], t["spoils_floor_permille"],
                               t["spoils_cap_permille"])
        part, share = shares[recs.index(me)]
        if share <= 0:
            r["settled"] = True
            return
        if not self.budget(W):
            return
        z = r["z"]
        ex = principal_need(sp["winner_dead"], sp["winner_pop0"], w.ct["exhaustion_permille"], w.muster(W))
        fv = w.figures   # P5-06 7.3: the principal's temper (0 without the view)
        g = grant_chance(self.warmth(W, z), part, ex, t, fv.spoils_permille(W) if fv is not None else 0)
        granted = _pcr().chance_pm(w.draw("hist.alliances.spoils", r["w"], z, 0), g)
        k = (min(W, z), max(W, z))
        a = self.alliances.get(k)
        if self.r28:   # P5-04a 3.4: grievance 2 ends now, 1 sours, 0 no open non-origin alliance
            grievance = 0
            if not granted and a is not None and not a["origin"]:
                big = spoils_large(share, part, t["spoils_large_share"], t["spoils_large_part_permille"])
                grievance = 2 if big or a["withheld"] + 1 >= t["withheld_to_break"] else 1
            vals = (share,) if granted else (share, grievance)
        else:
            vals = (share,)
        e = w.append("spoils_shared" if granted else "spoils_withheld", region=sp["goal_region"],
                     participants=(("principal", CIV, W), ("ally", CIV, z), ("enemy", CIV, sp["loser"])),
                     values=vals, causes=tuple(sorted({sp["end_event"], r["join"]})))
        if self.r28:
            if granted:
                self.credit(W, z, t["share_memory"], e)
                if a is not None:
                    a["shared"] = e   # 6.2 item 2
            else:
                self.credit(W, z, -t["withheld_memory"], e)
                w.add_grudge(W, z, t["withheld_grudge"])
                if grievance == 2:
                    a["withheld"] += 1
                    if self.budget(z):
                        self.spurn_028(k, a, z, sp["goal_region"], e, 2, 0)
                    else:
                        a["spurn_pending"] = (e, w.year, sp["goal_region"], z)
                elif grievance == 1:
                    a["withheld"] = 1
                    a["soured"] = dict(e=e, year=w.year, region=sp["goal_region"], z=z)
                    a["renewal_due"] = a["call_due"] = True
            r["settled"] = True
            self.log.append((w.year, "spoils", r["w"], z, share, granted, g))
            return
        if granted:
            self.credit(W, z, t["share_memory"], e)
        else:
            self.credit(W, z, -t["withheld_memory"], e)
            w.add_grudge(W, z, t["withheld_grudge"])
            k = (min(W, z), max(W, z))
            if k in self.alliances:
                a = self.alliances[k]
                a["withheld"] += 1
                a["w_event"], a["w_region"], a["w_ally"] = e, sp["goal_region"], z
        r["settled"] = True
        self.log.append((w.year, "spoils", r["w"], z, share, granted, g))

    # --- the steps (6.1)
    def t2(self):
        self.closed_now = set()
        self.trade_closed_now = set()
        self.drop_closed()
        self.cache()
        self.read_log()
        self.keeping()
        self.origins()
        self.formations()
        self.trade_breaks()
        self.trade_openings()
        self.trade_credits()
        self.mine_closings()
        self.mine_openings()
        self.arms_sales()

    def t6(self):
        self.closed_now = set()
        self.trade_closed_now = set()
        self.drop_closed()
        self.cache()
        self.joined_ends()
        self.mine_closings()

    def q4(self):
        self.closed_now = set()
        self.trade_closed_now = set()
        self.drop_closed()
        self.cache()
        self.joined_ends()
        self.keeping()
        self.trade_breaks()
        self.trade_openings()
        self.trade_credits()
        self.mine_closings()
        self.mine_openings()
        self.arms_sales()


# ---------------------------------------------------------------------------
# The replay's fixtures (18.3): alliances-small, the zero tuning of I0 (7.14), and the row model of a replayed
# history (check-world's invariants on the reference's own histories without a world file)
# ---------------------------------------------------------------------------

# 18.3: alliances-small = the conflict stress data with ALLIANCES_SMALL_026 and CONFLICT_SMALL_026 (p3_data_reference),
# every race present (reading A11: the stress cases' race lists replaced by `all`, so that dwarves mine and the elves
# and faeries ally by origin), on these stress seeds.
SMALL_SEEDS = (3, 5, 6, 7, 10, 0, 1, 2, 9, 12)   # the first four hold every kind of 63 to 78 (as the C++ fixture)
I0_SEEDS = (0, 1, 2, 5)


def small_data(dat=None):
    import p3_data_reference as pdr
    pcr = _pcr()
    d = pcr.edited(pcr.stress_data(dat), conflict=pdr.CONFLICT_SMALL_026)
    d.alliances = dict(d.alliances, **pdr.ALLIANCES_SMALL_026)
    if pcr.FLAGS.get("p504a"):   # P5-04a 14.1: alliances-small at 0.28.0
        d.alliances = dict(d.alliances, **pdr.ALLIANCES_SMALL_028)
    return d


def zero_data(dat=None):
    """4.2: the zero fixture tuning (ally_hops -1, trade_hops -1, mine_permille 0, every origin_allies empty)."""
    import copy
    import p3_data_reference as pdr
    d = _pcr().edited(dat or _pcr().data())
    base_alliances = dict(d.alliances)
    d.alliances = dict(pdr.ALLIANCES_ZERO_026)
    if _pcr().FLAGS.get("p504a"):   # P5-04a 14.1: the zero root holds the new fields at their shipped values
        d.alliances.update({n: base_alliances.get(n, v) for n, _, _, v in pdr.ALLIANCES_NEW_RANGES_028})
    d.race = copy.deepcopy(d.race)
    for r in d.race:
        d.race[r]["origin_allies"] = []
    return d


def small_case(seed):
    pcr = _pcr()
    span, _races, civs, intensity = pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
    return span, "all", civs, intensity


def fixture_run(seed, dat, case=None):
    pcr = _pcr()
    case = case or pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
    inp, _g = pcr.synthetic_inputs(seed, dat, case)
    return pcr.history_stage(dat, inp)


REGION_COLUMNS = 28


def aw_of_res(res):
    """The row model AW of a replayed history (res of p3_conflict_reference.history_stage): its tables as a world
    file would store them, the region rows filled at the columns the checks read (id, name, cells, lake_cells,
    ore_reach_cells)."""
    inp = res["_inputs"]
    regions = []
    for r in inp.regions:
        row = [0] * REGION_COLUMNS
        row[0], row[1], row[10], row[11], row[27] = r["id"], r.get("name", ""), r["cells"], r["lake_cells"], \
            r["ore_reach_cells"]
        regions.append(tuple(row))
    t = {n: list(res.get(n, [])) for n in READ_TABLES}
    t["region"] = regions
    t["region_border"] = [tuple(b.get(k, 0) for k in ("a", "b", "length_m")) for b in inp.borders]
    t["race"] = [tuple(r) for r in inp.races]
    meta = {"history_end_year": str(inp.params["span"]), "schema_version": str(SCHEMA_VERSION),
            "generator_version": GENERATOR_VERSION}
    return AW(meta, t)


def res_problems(res, dat):
    """Invariants 1 to 8, 10's shelter rule and 11 to 14 and the reader checks of 9 on a replayed history."""
    w = aw_of_res(res)
    races = {dat.race[r]["key"]: (dat.race[r]["origin_allies"], dat.race[r]["ally_join"], dat.race[r]["mining"])
             for r in dat.race}
    return (invariant_violations(w, dat.alliances, races) + shelter_violations(w, dat.conflict["shelter_years"]) +
            reader_problems(w))


I0_TABLES = ("event", "event_participant", "event_cause", "event_value", "phase", "civilization", "settlement",
             "settlement_census", "region_control", "relation", "region_name", "beast_population", "monster", "site",
             "race_relation", "settlement_race")


def without_alliances(fn):
    pcr = _pcr()
    saved = pcr.FLAGS["alliances"]
    pcr.FLAGS["alliances"] = False
    try:
        return fn()
    finally:
        pcr.FLAGS["alliances"] = saved


def i0_differences(run, base):
    """7.14: the history tables that differ between the zero tuning with the subsystem and the same data without
    it (no AllianceView)."""
    zero = run(zero_data(base))
    off = without_alliances(lambda: run(zero_data(base)))
    return [t for t in I0_TABLES if zero.get(t, []) != off.get(t, [])], zero, off


def i0_025_differences(run, base_root=None):
    """I0 against generator 0.25.0 itself: the zero tuning at 0.26.0 against the 0.25.0 rules and data (the
    catalogue rows, kinds 63 to 78, aside)."""
    pcr = _pcr()
    pcr.use_version("0.25.0")
    try:
        old = run(pcr.data(base_root) if base_root else None)
    finally:
        pcr.use_version("0.26.0")
    new = run(zero_data(pcr.data(base_root) if base_root else None))
    return [t for t in I0_TABLES if old.get(t, []) != new.get(t, [])], old, new


def check_fixtures(c, seeds=SMALL_SEEDS, i0_seeds=I0_SEEDS, verbose=False):
    """18.3: alliances-small makes every kind of 63 to 78 happen; check-world's invariants and the reader checks
    hold on its histories, on the shipped tuning's stress histories and on the zero tuning's; I0 on stress seeds."""
    import time
    pcr = _pcr()
    seen = set()
    small = small_data()
    for seed in seeds:
        t0 = time.time()
        res = fixture_run(seed, small, small_case(seed))
        key = {k[0]: k[1] for k in res["event_kind"]}
        kinds = {key[e[3]] for e in res["event"] if e[3] >= 63}
        seen |= kinds
        for v in res_problems(res, small):
            c.ok("alliances-small-%d: %s" % (seed, v), False)
        if verbose:
            print("alliances-small-%d: %d events, kinds %s (%.0fs)" % (seed, len(res["event"]), sorted(kinds),
                                                                         time.time() - t0))
            sys.stdout.flush()
    c.eq("18.3: every kind of 63 to 78 happens on alliances-small", sorted(set(k for _i, k in KINDS) - seen), [])
    shipped = pcr.stress_data()
    for seed in i0_seeds:
        res = fixture_run(seed, shipped)
        for v in res_problems(res, shipped):
            c.ok("stress-%d: %s" % (seed, v), False)
        diff, _z, _o = i0_differences(lambda d: fixture_run(seed, d), pcr.stress_data())
        c.eq("7.14 I0 stress-%d: the history tables equal" % seed, diff, [])
        if not pcr.FLAGS.get("p504a"):
            diff, _a, _b = i0_025_differences(lambda d: fixture_run(seed, pcr.stress_data(d)))
            c.eq("7.14 I0 stress-%d against 0.25.0: the history tables equal" % seed, diff, [])
        if verbose:
            print("stress-%d: invariants and I0 done" % seed)
            sys.stdout.flush()



# ---------------------------------------------------------------------------
# P5-04a (generator 0.28.0): check-world's invariants of 16 item 13, the reader checks of 14.4, the [stats] keys of
# 12.6 and the v9 lines of 12.3 and 12.5, on world rows (AW)
# ---------------------------------------------------------------------------

def is_028(w):
    """A world with the catalogue of 0.28.0 (kind 75 carries grievance)."""
    k = w.kid.get("spoils_withheld")
    return k is not None and "grievance" in w.vseq.get(k, {})


WAR_ENDS_028 = ("peace", "war_won", "truce", "ally_withdrew", "search_abandoned")


def war_ends_of(w, c):
    """[(year, event)] of the events that closed a war row of c (principal or joined): last_peace's events
    (reading R5: conflict's last_peace, so search_abandoned too)."""
    out = []
    for r in w.rows.get("war", ()):
        if c in (r[1], r[3]) and r[6] is not None and r[8] in w.ev and w.kind(r[8]) in WAR_ENDS_028:
            out.append((w.year(r[8]), r[8]))
    return sorted(set(out))


def last_war_end_at(w, c, e):
    """last_war_end(c) when event e is appended: the largest (year, event) of an ending before e."""
    xs = [x for x in war_ends_of(w, c) if x[1] < e]
    return xs[-1] if xs else (0, 0)


def open_wars_at(w, c, e):
    """open_wars_of(c) when e is appended (P5-04 7.6: the open wars with c as a side and the open joined records of
    c): the open war rows of c's declarations, and the open joined rows whose ally_joined_war has c as the ally (a
    row opened by another realm joining against c is that realm's record, not one of c's wars; integration, turb-3
    e1678)."""
    return sum(1 for r in w.rows.get("war", ()) if c in (r[1], r[3]) and w.open_at_event(r, e) and
               (w.kind(r[7]) != "ally_joined_war" or w.one(r[7], "ally") == c))


def alliance_row_at(w, a, b, e):
    rs = [r for r in w.pair_rows("alliance", a, b) if r[7] <= e and (r[6] is None or r[8] >= e)]
    return rs[-1] if rs else None


def within_row(r, x):
    """An event id inside an alliance row: after its since_event, up to its until_event (open: no bound)."""
    return r[7] < x and (r[6] is None or x <= r[8])


def soured_share(w, r, z, upto):
    """The first spoils_withheld with grievance 1 of the row's pair, ally z, before upto."""
    for x in sorted(w.of("spoils_withheld")):
        if within_row(r, x) and x < upto and w.value(x, "grievance") == 1 and w.one(x, "ally") == z and \
                {w.one(x, "principal"), z} == {r[1], r[3]}:
            return x
    return None


def call_due_at(w, r, z, p, e):
    """3.5: the alliance's call_due when z's defence call to p at e is made: soured (z wronged) and no defence call
    of z to p evaluated since (a defence join or a refusal: an origin alliance is never soured)."""
    so = soured_share(w, r, z, e)
    if so is None:
        return None
    for x in w.of("ally_joined_war") + w.of("ally_refused"):
        if so < x < e and w.one(x, "ally") == z and w.one(x, "partner") == p and \
                (w.kind(x) == "ally_refused" or w.value(x, "side") == DEFENCE):
            return None
    return so


def invariant_violations_028(w, t):
    """16 item 13: the replaced item 12 and the new invariants that rows show (motive 5 and the hidden factors f3, f4
    and f6 are checked in the replay). t: history.alliances and history.relations fields (merged)."""
    out = []
    if not is_028(w):
        return ["0.28.0: the world's catalogue has no grievance on spoils_withheld"]
    # 12 replaced: every spurning cites a withheld share of its pair with grievance >= 1 after the formation;
    # reason 2 in the share's year unless years > 0; years = Y - the share's year
    for s_ in w.of("alliance_spurned"):
        z, P = w.one(s_, "ally"), w.one(s_, "principal")
        rows = [r for r in w.pair_rows("alliance", z, P) if r[6] is not None and r[8] == s_]
        if len(rows) != 1:
            out.append("12: alliance_spurned e%d closes no alliance of c%d and c%d" % (s_, z, P))
            continue
        r = rows[0]
        if w.value(r[7], "basis") == ORIGIN:
            out.append("12: alliance_spurned e%d closes an origin alliance" % s_)
        wh = [c for c in w.causes.get(s_, []) if w.kind(c) == "spoils_withheld"]
        if len(wh) != 1 or w.one(wh[0], "ally") != z or w.one(wh[0], "principal") != P or wh[0] < r[7] or \
                (w.value(wh[0], "grievance") or 0) < 1:
            out.append("12: alliance_spurned e%d cites no withheld share of c%d with grievance after e%d" % (s_, z, r[7]))
            continue
        reason, years = w.value(s_, "reason"), w.value(s_, "years")
        if years != w.year(s_) - w.year(wh[0]):
            out.append("12: alliance_spurned e%d years %r, the share of %d" % (s_, years, w.year(wh[0])))
        if reason == 2 and w.value(wh[0], "grievance") != 2:
            out.append("12: alliance_spurned e%d reason 2 for a share of grievance %r" % (s_, w.value(wh[0], "grievance")))
        if reason in (0, 1) and w.value(wh[0], "grievance") != 1:
            out.append("12: alliance_spurned e%d reason %d for a share of grievance %r" % (
                s_, reason, w.value(wh[0], "grievance")))
        if w.ev[s_][5] != w.ev[wh[0]][5]:
            out.append("12: alliance_spurned e%d not in its share's region" % s_)
    # every grievance 2 share ends its alliance (at once, or after a quiet budget)
    for x in w.of("spoils_withheld"):
        g = w.value(x, "grievance")
        z, P = w.one(x, "ally"), w.one(x, "principal")
        r = alliance_row_at(w, z, P, x)
        if g == 2 and not any(w.kind(k) == "alliance_spurned" and x in w.causes.get(k, []) and
                              w.value(k, "reason") == 2 for k in w.kids.get(x, [])):
            if r is not None and r[6] is None:
                out.append("3.4: withheld share e%d of grievance 2 ends no alliance" % x)
        if g in (1, 2) and (r is None or w.value(r[7], "basis") == ORIGIN):
            out.append("3.4: withheld share e%d of grievance %d without an open non-origin alliance" % (x, g))
        if g == 0 and r is not None and r[6] is None and w.value(r[7], "basis") != ORIGIN and r[7] < x:
            out.append("3.4: withheld share e%d of grievance 0 on an open alliance" % x)
    # no alliance has three withheld shares with grievance >= 1
    for r in w.rows.get("alliance", ()):
        n = sum(1 for x in w.of("spoils_withheld") if within_row(r, x) and (w.value(x, "grievance") or 0) >= 1 and
                {w.one(x, "ally"), w.one(x, "principal")} == {r[1], r[3]})
        if n >= 3:
            out.append("13: alliance e%d has %d withheld shares with a grievance" % (r[7], n))
    # refusals: count, the excuse, the break, the causes, and the motive against the factors rows show
    for x in sorted(w.of("ally_refused")):
        z, p = w.one(x, "ally"), w.one(x, "partner")
        m, cnt = w.value(x, "motive"), w.value(x, "count")
        r = alliance_row_at(w, z, p, x)
        if r is None:
            out.append("4.3: ally_refused e%d without its alliance" % x)
            continue
        if (cnt == 0) != (m in (1, 2)):
            out.append("4.3: ally_refused e%d motive %d count %d" % (x, m, cnt))
        prior = [y for y in w.of("ally_refused") if within_row(r, y) and y < x and
                 {w.one(y, "ally"), w.one(y, "partner")} == {r[1], r[3]} and (w.value(y, "count") or 0) > 0]
        if cnt > 0 and cnt != len(prior) + 1:
            out.append("4.3: ally_refused e%d count %d after %d counted" % (x, cnt, len(prior)))
        cs = w.causes.get(x, [])
        lwe = last_war_end_at(w, z, x)
        weary = lwe[1] != 0 and lwe[0] >= w.year(x) - t["weary_years"]
        f = {1: t["join_weary_permille"] if weary else 1000,
             2: max(0, 1000 - t["join_busy_permille"] * open_wars_at(w, z, x))}
        so = call_due_at(w, r, z, p, x)
        f[7] = t["soured_join_permille"] if so is not None else 1000
        if m == 0 and any(v < 1000 for v in f.values()):
            out.append("4.2: ally_refused e%d motive 0 with factors %r" % (x, f))
        if m in f:
            if f[m] >= 1000:
                out.append("4.2: ally_refused e%d motive %d with f%d 1000" % (x, m, m))
            for k, v in f.items():
                if k != m and v < 1000 and not (f[m] < v or (f[m] == v and m < k)):
                    out.append("4.2: ally_refused e%d motive %d against f%d %d" % (x, m, k, v))
        want = {1: lwe[1] if m == 1 else None, 7: so if m == 7 else None}.get(m)
        if m in (1, 7) and want not in cs:
            out.append("4.3: ally_refused e%d motive %d does not cite e%r" % (x, m, want))
        if m == 6 and not any(w.kind(c) in ("claim", "claim_settlements") for c in cs):
            out.append("4.3: ally_refused e%d motive 6 cites no claim" % x)
        if cnt > 0 and cnt >= t["refusals_to_break"]:
            if not any(w.kind(k) == "alliance_broken" and w.value(k, "reason") == 0 for k in w.kids.get(x, [])):
                out.append("4.3: ally_refused e%d count %d breaks nothing" % (x, cnt))
    for b in w.of("alliance_broken"):
        if w.value(b, "reason") == 0:
            rf = [c for c in w.causes.get(b, []) if w.kind(c) == "ally_refused"]
            if len(rf) != 1 or (w.value(rf[0], "count") or 0) < t["refusals_to_break"]:
                out.append("4.3: alliance_broken e%d reason 0 without a counted refusal at the bound" % b)
    # betrayals: years
    for b in w.of("alliance_betrayed"):
        rs = [r for r in w.pair_rows("alliance", w.one(b, "betrayer"), w.one(b, "betrayed")) if r[8] == b]
        if len(rs) == 1 and w.value(b, "years") != w.year(b) - w.year(rs[0][7]):
            out.append("5: alliance_betrayed e%d years %r" % (b, w.value(b, "years")))
        if w.value(b, "motive") not in (0, 1, 2, 3):
            out.append("5: alliance_betrayed e%d motive %r" % (b, w.value(b, "motive")))
    # renewals: notable only, on the chain
    for n in sorted(w.of("alliance_renewed")):
        a, b = w.one(n, "ally_a"), w.one(n, "ally_b")
        r = alliance_row_at(w, a, b, n)
        if r is None:
            out.append("6: alliance_renewed e%d without its alliance" % n)
            continue
        R = max([r[7]] + [y for y in w.of("alliance_renewed") if r[7] < y < n and
                          {w.one(y, "ally_a"), w.one(y, "ally_b")} == {a, b}])
        basis_R = w.value(R, "basis")
        fought = [y for y in w.of("ally_joined_war") if R < y < n and {w.one(y, "ally"), w.one(y, "partner")} == {a, b}]
        shared = [y for y in w.of("spoils_shared") if R < y < n and {w.one(y, "ally"), w.one(y, "principal")} == {a, b}]
        if not fought and not shared and w.value(n, "basis") == basis_R:
            out.append("6.2: alliance_renewed e%d with nothing notable since e%d" % (n, R))
        if R not in w.causes.get(n, []):
            out.append("6.2: alliance_renewed e%d does not cite e%d" % (n, R))
        s0, term = w.year(r[7]), w.value(r[7], "term")
        # 6.1: never early. The chain is fixed and a renewal that waited (a quiet step longer than the term, a lapse
        # or a soured draw without budget) is caught up one due year per T2 or Q4, so a written renewal may be of a
        # due year at or before the last written one (integration, alliances-small seed 3 e715). Rows show the
        # renewals written: the j-th written renewal of the row needs j due years at or before its year.
        j = 1 + sum(1 for y in w.of("alliance_renewed") if r[7] < y < n and {w.one(y, "ally_a"), w.one(y, "ally_b")}
                    == {a, b})
        if term and len(due_years(s0, term, w.year(n))) < j:
            out.append("6.1: alliance_renewed e%d in %d is renewal %d of the row with %d due years" % (
                n, w.year(n), j, len(due_years(s0, term, w.year(n)))))
    for e in w.of("alliance_formed"):
        if (w.value(e, "term") or 0) < 1 or w.value(e, "band") not in (0, 1, 2):
            out.append("10: alliance_formed e%d term %r band %r" % (e, w.value(e, "term"), w.value(e, "band")))
        if w.value(e, "basis") != ORIGIN and len(w.causes.get(e, [])) != 1:
            out.append("10 item 2: alliance_formed e%d cites %d events" % (e, len(w.causes.get(e, []))))
    # 9.2: a battle cites at most one arms_sold, the buyer its winner, a sale not cited before
    cited = {}
    for k in BATTLE_KINDS:
        for b in w.of(k):
            arms = [c for c in w.causes.get(b, []) if w.kind(c) == "arms_sold"]
            if len(arms) > 1:
                out.append("9.2: battle e%d cites %d sales" % (b, len(arms)))
            for a_ in arms:
                if w.one(a_, "buyer") != w.one(b, "winner"):
                    out.append("9.2: battle e%d cites arms e%d not of its winner" % (b, a_))
                if a_ in cited:
                    out.append("9.2: arms e%d cited by e%d and e%d" % (a_, cited[a_], b))
                cited.setdefault(a_, b)
    return out


def reader_problems_028(w):
    """14.4: the ranges (P5-03 16.4's message form) and the three consistency messages, first failing event first."""
    out = []
    E = w.E
    ranges = {"disposition_turned": {"change": (0, 3)}, "alliance_formed": {"term": (1, None), "band": (0, 2)},
              "alliance_betrayed": {"motive": (0, 3), "years": (0, E - 1)},
              "ally_refused": {"motive": (0, 7), "count": (0, 100)}, "spoils_withheld": {"grievance": (0, 2)},
              "alliance_spurned": {"years": (0, E - 1), "reason": (0, 2)}, "hero_rose": {"quiet": (0, E - 1)}}
    for e in sorted(w.ev):
        k = w.kind(e)
        for v, (lo, hi) in ranges.get(k, {}).items():
            x = w.value(e, v)
            if x is None or x < lo or (hi is not None and x > hi):
                out.append("corrupt table 'event': row %d: value %s %r outside %d to %s" % (
                    e, v, x, lo, "%d" % hi if hi is not None else "any"))
        cs = w.causes.get(e, [])
        if k == "ally_refused":
            if w.value(e, "motive") == 7 and not any(w.kind(c) == "spoils_withheld" for c in cs):
                out.append("corrupt table 'event': row %d: refusal over spoils without its share" % e)
            if w.value(e, "count") == 0 and w.value(e, "motive") not in (1, 2):
                out.append("corrupt table 'event': row %d: excused refusal with motive %d" % (e, w.value(e, "motive")))
        if k == "alliance_spurned" and w.value(e, "reason") == 2 and not (w.value(e, "years") or 0) > 0:
            if not any(w.kind(c) == "spoils_withheld" and w.year(c) == w.year(e) and w.value(c, "grievance") == 2
                       for c in cs):
                out.append("corrupt table 'event': row %d: spurned at once without its share" % e)
    return out


def stats_028(w):
    """12.6: the [stats] keys appended at v9, in order."""
    o = [("alliances_renewals", sum(alliance_renewals(w, r) for r in w.rows.get("alliance", ()))),
         ("alliances_renewals_written", len(w.of("alliance_renewed"))),
         ("alliances_soured", sum(1 for x in w.of("spoils_withheld") if w.value(x, "grievance") == 1)),
         ("alliances_spurned_at_once", sum(1 for x in w.of("alliance_spurned") if w.value(x, "reason") == 2)),
         ("allies_refused_excused", sum(1 for x in w.of("ally_refused") if w.value(x, "count") == 0))]
    o += [("refusal_motive_%d" % m, sum(1 for x in w.of("ally_refused") if w.value(x, "motive") == m))
          for m in range(8)]
    o += [("betrayal_motive_%d" % m, sum(1 for x in w.of("alliance_betrayed") if w.value(x, "motive") == m))
          for m in range(4)]
    o += [("battles_with_arms", sum(1 for k in BATTLE_KINDS for b in w.of(k)
                                    if any(w.kind(c) == "arms_sold" for c in w.causes.get(b, [])))),
          ("dispositions_thawed", sum(1 for x in w.of("disposition_turned") if w.value(x, "change") == 1))]
    return o


def alliance_renewals(w, r):
    """6.3: renewed k of an alliance row: every due year it stood at (end = until when closed, E standing)."""
    term = w.value(r[7], "term")
    if not term:
        return 0
    end = r[6] if r[6] is not None else w.E
    return renewal_count(r[5], term, end)


def alliance_line_v9(w, r):
    """12.3 (amends P5-04 10.2): the basis part names the foe; renewed by 6.3; spoils by id (11 item 9)."""
    a, b = r[1], r[3]
    e = r[7]
    basis = w.value(e, "basis")
    foe = w.role(e, "foe")
    part = {ORIGIN: "by origin", FRIENDSHIP: "in friendship", TRADE: "after trade"}.get(basis)
    if basis == COMMON_ENEMY:
        part = "against c%d %s" % (foe[0], _name(w, foe[0]))
    elif basis == FEAR:
        part = "in fear of c%d %s" % (foe[0], _name(w, foe[0]))
    line = "  c%d %s and c%d %s: %s, %s, e%d" % (a, _name(w, a), b, _name(w, b), _span(r), part, e)
    hi = r[6] if r[6] is not None else w.E
    within = lambda x: r[5] <= w.year(x) <= hi
    pair = {a, b}
    ren = alliance_renewals(w, r)
    if ren:
        line += "; renewed %d" % ren
    j = sum(1 for x in w.of("ally_joined_war") if within(x) and {w.one(x, "ally"), w.one(x, "partner")} == pair)
    rf = sum(1 for x in w.of("ally_refused") if within(x) and {w.one(x, "ally"), w.one(x, "partner")} == pair)
    if j or rf:
        line += "; joined %d, refused %d" % (j, rf)
    by_id = lambda x: r[7] < x and (r[6] is None or x < r[8])
    g = sum(1 for x in w.of("spoils_shared") if by_id(x) and {w.one(x, "ally"), w.one(x, "principal")} == pair)
    k = sum(1 for x in w.of("spoils_withheld") if by_id(x) and {w.one(x, "ally"), w.one(x, "principal")} == pair)
    if g or k:
        line += "; spoils %d granted, %d withheld" % (g, k)
    if V10:   # P5-07 10 item 5 (critic F4): the first withholding of the pair with grievance 1 in the row's span
        so = [x for x in w.of("spoils_withheld") if by_id(x) and {w.one(x, "ally"), w.one(x, "principal")} == pair
              and w.value(x, "grievance") == 1]
        if so:
            line += "; soured e%d" % min(so)
    if r[6] is not None:
        line += "; %s e%d" % (_end_kind(w, r), r[8])
    return line


V10 = False   # P5-07 10 item 5: set by p3_report_reference.use_version("0.29.0")


def trade_line_v9(w, r):
    """12.5: `; broken e<id>` for a trade_broken, `; ended e<id>` otherwise."""
    line = trade_line(w, r)
    if r[6] is not None and not (r[8] in w.ev and w.kind(r[8]) == "trade_broken"):
        line = line[:line.rindex("; broken e")] + "; ended e%d" % r[8]
    return line


def civ_alliance_lines_v9(w, c, level):
    """12.3: at summary `; at the end: c<id> <name>, ...` (the entity form with the name)."""
    if level == "full":
        return civ_alliance_lines(w, c, level)
    rows = sorted([r for r in w.rows.get("alliance", []) if c in (r[1], r[3])], key=lambda r: (r[5], r[1], r[3]))
    line = "  allies: %d" % len(rows)
    now = sorted(r[3] if r[1] == c else r[1] for r in rows if r[6] is None)
    if now:
        line += "; at the end: " + ", ".join("c%d %s" % (z, _name(w, z)) for z in now)
    return [line]


def tuning_028(root=None):
    """history.alliances and history.relations of a data root at 0.28.0 (the fallback of p3_data_reference fills
    the fields a 0.27.0 root lacks)."""
    pcr = _pcr()
    pcr.use_version("0.28.0")
    d = pcr.data(root) if root else pcr.data()
    return dict(d.relations, **d.alliances)


def check_units_028(c):
    """P5-04a 16 items 2 to 5 and 8: the formulas on hand-checked values."""
    import p3_data_reference as pdr
    t = dict({n: v for n, _, _, v in pdr.ALLIANCES_NEW_RANGES_028}, defend_permille=1000)
    # 3.3 large, at the bounds +- 1
    c.eq("3.3 share 599 part 299", spoils_large(599, 299, 600, 300), False)
    c.eq("3.3 share 600", spoils_large(600, 0, 600, 300), True)
    c.eq("3.3 part 300", spoils_large(1, 300, 600, 300), True)
    c.eq("3.3 part 1001 never", spoils_large(1, 1000, 1000000, 1001), False)
    # 4.2: each factor alone and its motive
    base = dict(join=1000, weary=False, open_wars=0, sigma=1000, w_zp=1000, temper=-1, claim=False, soured=False)
    def fs(**kw):
        a = dict(base, **kw)
        return call_factors(t, a["join"], a["weary"], a["open_wars"], a["sigma"], a["w_zp"], a["temper"],
                            a["claim"], a["soured"])
    c.eq("4.2 all at 1000", fs(), [1000] * 8)
    c.eq("4.2 motive 0 with all factors 1000", refusal_motive(fs(join=200)), 0)
    c.eq("4.2 weary: f1 600, motive 1", (fs(weary=True)[1], refusal_motive(fs(weary=True))), (600, 1))
    c.eq("4.2 one war: f2 750, motive 2", (fs(open_wars=1)[2], refusal_motive(fs(open_wars=1))), (750, 2))
    c.eq("4.2 five wars: f2 0", fs(open_wars=5)[2], 0)
    c.eq("4.2 sigma 0: f3 300, motive 3", (fs(sigma=0)[3], refusal_motive(fs(sigma=0))), (300, 3))
    c.eq("4.2 sigma 500: f3 650", fs(sigma=500)[3], 650)
    c.eq("4.2 warmth 0: f4 400, motive 4", (fs(w_zp=0)[4], refusal_motive(fs(w_zp=0))), (400, 4))
    c.eq("4.2 warmth 999: f4 999 (motive 4)", (fs(w_zp=999)[4], refusal_motive(fs(w_zp=999))), (999, 4))
    c.eq("4.2 grasping: f5 600, motive 5", (fs(temper=3)[5], refusal_motive(fs(temper=3))), (600, 5))
    c.eq("4.2 warlike: f5 1000", fs(temper=1)[5], 1000)
    c.eq("4.2 claim: f6 500, motive 6", (fs(claim=True)[6], refusal_motive(fs(claim=True))), (500, 6))
    c.eq("4.2 soured: f7 500, motive 7", (fs(soured=True)[7], refusal_motive(fs(soured=True))), (500, 7))
    c.eq("4.2 tie f1 = f5 = 600: the smaller code", refusal_motive(fs(weary=True, temper=3)), 1)
    c.eq("4.2 tie f6 = f7 = 500: 6", refusal_motive(fs(claim=True, soured=True)), 6)
    c.eq("4.2 the smallest wins: f3 300 under f1 600", refusal_motive(fs(weary=True, sigma=0)), 3)
    # 4.2: the chance, floor at each step (ajp 500, weary, sigma 500: 1000 x 500 / 1000 x 600 / 1000 x 650 / 1000)
    c.eq("4.2 chance", factored_chance(1000, fs(join=500, weary=True, sigma=500)), 1000 * 500 // 1000 * 600 // 1000 * 650 // 1000)
    c.eq("4.2 chance with codes at their defaults (P5-04's formula when f3 = sigma, f4 = w)",
         factored_chance(1000, call_factors(dict(pdr.ALLIANCES_DEFAULTS_028, defend_permille=1000), 700, False, 1,
                                            400, 800, 3, True, True)),
         1000 * 700 // 1000 * 1000 // 1000 * 1000 // 1000 * 400 // 1000 * 800 // 1000)
    # 5: betrayal motives in order
    c.eq("5 grasping first", betrayal_motive(3, -500, 100, 10, 0, 500), 0)
    c.eq("5 cooled", betrayal_motive(1, -1, 100, 10, 0, 500), 1)
    c.eq("5 at the cooled bound: not cooled", betrayal_motive(1, 0, 100, 100, 0, 500), 3)
    c.eq("5 weak: 49 x 1000 < 500 x 100", betrayal_motive(-1, 10, 100, 49, 0, 500), 2)
    c.eq("5 at the weak bound: 50 x 1000 = 500 x 100, land", betrayal_motive(-1, 10, 100, 50, 0, 500), 3)
    # 6.1, 6.3: the chain and the count
    c.eq("6.1 due years", due_years(480, 150, 1000), [630, 780, 930])
    c.eq("6.3 renewed k at E 2000 from 480", renewal_count(480, 150, 2000), 10)
    c.eq("6.3 closed at a due year counts it", renewal_count(480, 150, 631), 1)
    c.eq("6.3 closed the year before a due year", renewal_count(480, 150, 630), 0)
    c.eq("6.3 closed in the formation year", renewal_count(480, 150, 480), 0)
    # 8.5: the choice of eight words and {m:died}
    import p3_engine_reference as pe
    ws = list(MOTIVE_WORDS)
    c.eq("8.5 eight words, value 7", pe.choice_word(ws, 7), ws[7])
    c.eq("8.5 eight words, value 8: the last", pe.choice_word(ws, 8), ws[7])
    c.eq("8.5 eight words, value -1: the last", pe.choice_word(ws, -1), ws[7])
    c.eq("8.5 eight words, value 0", pe.choice_word(ws, 0), ws[0])
    c.eq("8.5 two words", (pe.choice_word(["year", "years"], 1), pe.choice_word(["year", "years"], 0)), ("year", "years"))
    c.eq("8.5 died", (pe.died_phrase("wild"), pe.died_phrase("dead")), ("died of great age", "crumbled to dust at last"))
    saved = pe.ENGINE_VERSION
    pe.use_version("0.28.0")
    try:
        k69 = pe.find_kind(pe.COMPILED, "ally_refused")
        nine = "{motive?" + "|".join(["w"] * 9) + "}"
        c.eq("8.5 nine words", pe.template_problem(k69, nine), "choice of 9 words at byte 0")
        c.eq("8.5 eight words ok", pe.template_problem(k69, "{motive?" + "|".join(["w"] * 8) + "}"), None)
        c.eq("8.5 five words ok", pe.template_problem(k69, "{motive?a|b|c|d|e}"), None)
        k_died = pe.find_kind(pe.COMPILED, "monster_died")
        c.eq("10.5 {monster:died} ok", pe.template_problem(k_died, "{monster}{monster:aside} {monster:died}."), None)
        c.eq("10.5 {ally:died} refused", pe.template_problem(k69, "{ally:died}"),
             "form 'died' needs a monster role at byte 0")
    finally:
        pe.use_version(saved)


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

READINGS_028 = """\
P5-04a readings (the test writer's; integration compares them):
R1 (3.4) the grievance of a withheld share is decided before the event is written (it is one of its values); a share
   of grievance 2 that waits for its ally's quiet budget (Q4) is spurned at the start of the next T2 or Q4's keeping,
   with years = Y - its year.
R2 (4.2) f1: last_war_end(z) = (0, 0) (no war ended) is not weariness, though 0 >= Y - weary_years in the first
   years of history.
R3 (7.2) a pair that never turned has no cool-down (turn_event 0).
R4 (8) a pair that met in hatred and never turned into it has no hatred_event: a long-held people of it is never
   expelled.
R5 (4.2 f1) last_war_end reads conflict's last_peace, which a search_abandoned also sets (P5-04 reading A3).
R6 (6.2) a notable renewal without its quiet budget renews quietly: the credit cites R and the notable events stay
   notable for the next due year (R is still the last written renewal).
R7 (3.5) the soured renewal draw waits, like the spurning it may write, for its ally's quiet budget in Q4; it is
   drawn once per due year (its key holds d).
R8 (4.4) the template's {count?|, and its ally did not forget it|} prints the clause for count 1 only (count 2 at
   refusals_to_break 2 is followed by its alliance_broken).
R9 (21.7 item 3) the figure queries of the browser's search golden: (a) the display name of the smallest-id figure,
   (b) the first three bytes of the name of the smallest-id figure of role 1, (c) the display name of the
   smallest-id figure with regnal >= 2; figures match by display name, after the regions, in ascending id.
R10 (16 item 12) the fixture is named H18 (h18-v9.toml): P5-06's h17-figures.toml already holds the name H17."""


def check_fixtures_028(quick=False):
    import time
    import p3_report_reference  # noqa: F401  (its import serves 0.27.0: imported before the version is set)
    import p5_03_relations_reference  # noqa: F401
    import p5_06_figures_reference  # noqa: F401
    pcr = _pcr()
    pcr.use_version("0.28.0")
    c = Checker()
    seen, motives, reasons, griev, bets = set(), set(), set(), set(), set()
    small = small_data()
    t28 = dict(small.relations, **small.alliances)
    for seed in (SMALL_SEEDS[:2] if quick else SMALL_SEEDS[:6]):
        t0 = time.time()
        res = fixture_run(seed, small, small_case(seed))
        w = aw_of_res(res)
        for v in res_problems(res, small) + invariant_violations_028(w, t28) + reader_problems_028(w):
            c.ok("alliances-small-%d (0.28.0): %s" % (seed, v), False)
        seen |= {w.kind(e) for e in w.ev if w.ev[e][3] >= 63}
        assert pcr.FLAGS["p504a"] and pcr.pe.RENDER_R28, "the version was reset during the run"
        motives |= {w.value(e, "motive") for e in w.of("ally_refused")}
        reasons |= {w.value(e, "reason") for e in w.of("alliance_spurned")}
        griev |= {w.value(e, "grievance") for e in w.of("spoils_withheld")}
        bets |= {w.value(e, "motive") for e in w.of("alliance_betrayed")}
        print("alliances-small-%d at 0.28.0: %d events; motives %s, spurned %s, grievance %s, betrayals %s (%.0fs)"
              % (seed, len(w.ev), sorted(motives), sorted(reasons), sorted(griev), sorted(bets), time.time() - t0))
        sys.stdout.flush()
    # owed shares are rare on the small stress maps (1 in 12 runs at 0.27.0 and 0.28.0): the kinds missing are
    # reported, not failed; the spoils rules of 3 are checked on H18 and on generated worlds (C++)
    print("alliances-small 0.28.0: kinds of 63 to 78 not seen: %s" % sorted(set(k for _i, k in KINDS) - seen))
    print("refusal motives seen %s, spurned reasons %s, grievances %s, betrayal motives %s" % (
        sorted(motives), sorted(reasons), sorted(griev), sorted(bets)))
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def cmd_check_world(paths, root):
    t, races, _cf = data_of(root)
    import p3_data_reference as pdr
    shelter = 0
    p = os.path.join(root or os.path.join(REPO, "game", "data"), "history", "conflict.toml")
    with open(p, "rb") as f:
        shelter = tomllib.load(f)["history"]["conflict"]["shelter_years"]
    del pdr
    bad = 0
    for path in paths:
        w = aw_of(path)
        c = Checker()
        c.eq("%s schema_version" % path, w.meta.get("schema_version"), str(SCHEMA_VERSION))
        for v in invariant_violations(w, t, races) + shelter_violations(w, shelter):
            c.ok("%s invariant %s" % (os.path.basename(path), v), False)
        for m in reader_problems(w):
            c.ok("%s reader %s" % (os.path.basename(path), m), False)
        if is_028(w):   # P5-04a 16 item 13, 14.4
            t28 = tuning_028(root)
            for v in invariant_violations_028(w, t28):
                c.ok("%s invariant %s" % (os.path.basename(path), v), False)
            for m in reader_problems_028(w):
                c.ok("%s reader %s" % (os.path.basename(path), m), False)
        st = dict(stats(w))
        print("%s: %d alliances, %d trades, %d mines, %d checks, %d failures; longest run above %d: %d years" % (
            os.path.basename(path), st["alliances_formed"], st["trade_opened"], st["mines_opened"], c.count,
            c.failures, t["bloc_lapse_share"], w.longest_run_above(t["bloc_lapse_share"])))
        bad += c.failures
    return 1 if bad else 0


def _import_before_version():
    """The references whose import sets their default version (0.27.0), imported before a command sets its own (as
    check_fixtures_028 does), so that a lazy import during the run does not reset it."""
    import p3_report_reference  # noqa: F401
    import p5_03_relations_reference  # noqa: F401
    import p5_06_figures_reference  # noqa: F401


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    root = None
    if "--data" in argv:
        i = argv.index("--data")
        root = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    cmd = argv[0]
    if cmd == "units":
        c = Checker()
        check_units(c)
        check_units_028(c)
        print("%d checks, %d failures" % (c.count, c.failures))
        return 1 if c.failures else 0
    if cmd == "check-world":
        return cmd_check_world(argv[1:], root)
    if cmd == "stats":
        for k, v in stats(aw_of(argv[1])):
            print("%s %s" % (k, v))
        return 0
    if cmd == "block":
        level = argv[argv.index("--level") + 1] if "--level" in argv else "summary"
        print("\n".join(alliances_block(aw_of(argv[1]), level)))
        return 0
    if cmd == "timeline":
        w = aw_of(argv[1])
        y = int(argv[2])
        print("\n".join(dump_lines(w, y)))
        print("groups %r" % alliance_groups_at(w, y))
        return 0
    if cmd == "golden":
        inputs = argv[argv.index("--inputs") + 1]
        rest = [a for a in argv[1:] if not a.startswith("--") and a != inputs]
        text = golden_text(inputs)
        if rest and rest[0] == "-":
            sys.stdout.write(text)
        else:
            with open(rest[0] if rest else GOLDEN, "w", newline="\n") as f:
                f.write(text)
        return 0
    if cmd == "fixtures":
        # --version 0.28.0 (P5-04a 16 item 14): alliances-small, the stress histories and I0 at 0.28.0 (without the
        # comparison against 0.25.0, whose other rules 0.28.0 changed)
        v28 = "--version" in argv and argv[argv.index("--version") + 1] == "0.28.0"
        _import_before_version()
        _pcr().use_version("0.28.0" if v28 else "0.26.0")
        c = Checker()
        quick = "--quick" in argv
        check_fixtures(c, SMALL_SEEDS[:4] if quick else SMALL_SEEDS, I0_SEEDS[:2] if quick else I0_SEEDS, verbose=True)
        print("%d checks, %d failures" % (c.count, c.failures))
        return 1 if c.failures else 0
    if cmd == "i0-world":
        # 7.14 on world cases of p3_conflict_reference.world_cases() read from --inputs DIR (their geography):
        # the zero tuning against no subsystem, and against 0.25.0
        import time
        pcr = _pcr()
        v28 = "--version" in argv and argv[argv.index("--version") + 1] == "0.28.0"   # P5-04a 16 item 14
        _import_before_version()
        pcr.use_version("0.28.0" if v28 else "0.26.0")
        inputs = argv[argv.index("--inputs") + 1]
        names = [a for a in argv[1:] if not a.startswith("--") and a not in (inputs, "0.28.0")]
        c = Checker()
        for name, seed, size, span, races, detail, civs, intensity in pcr.world_cases():
            if name not in names:
                continue
            t0 = time.time()
            run = lambda d: pcr.world_case(name, seed, size, span, races, detail, inputs, d, civs, intensity)  # noqa
            if not v28:
                diff, a, b = i0_025_differences(lambda d: run(d))
                c.eq("7.14 I0 %s against 0.25.0 (%d events)" % (name, len(a["event"])), diff, [])
            if "--with-off" in argv or v28:
                diff, _z, _o = i0_differences(run, pcr.data())
                c.eq("7.14 I0 %s: zero tuning against no subsystem" % name, diff, [])
            print("%s: %.0fs" % (name, time.time() - t0))
            sys.stdout.flush()
        print("%d checks, %d failures" % (c.count, c.failures))
        return 1 if c.failures else 0
    if cmd == "fixtures-028":
        # P5-04a: alliances-small and the shipped stress data at 0.28.0 (the replay of 3 to 6, 9.2, 10, 11): every
        # value of the new codes occurs; check-world's invariants (P5-04 13, 16 item 13) and the reader checks
        return check_fixtures_028("--quick" in argv)
    if cmd == "readings":
        print(READINGS_028)
        print(READINGS)
        print(REPLAY_TODO)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.modules.setdefault("p5_04_alliances_reference", sys.modules["__main__"])   # one module object when run as a script (P5-04a integration)
    sys.exit(main(sys.argv[1:]))
