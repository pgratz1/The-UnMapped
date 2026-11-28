#!/usr/bin/env python3
"""Reference of P5-08 "Pantheon, divine interventions and cults" (docs/specs/p5-08-pantheon-cults.md r1, generator
0.30.0, schema 14, history report format v11), written by the test writer from the spec alone, without the
engineer's branch (p5/08-impl), with the lead's rulings L-1 to L-12 (docs/design/proposals.md, last entry).

The rules of 6.2 to 6.10 run inside p3_conflict_reference.py's replay: at 0.30.0 (conflict flag `pantheon`) the
conflict World installs `Pantheon` after the figures subsystem and calls it in T4 (last), T5 (the interventions: the
pure source and the applier of 6.3 and 7.6) and Q4 (after the figures), asks its tension third (6.6), and reads its
DivineView (7.1) in peoples (growth, the shunned regions of the site and colony searches) and conflict (force). The
other seams of 7 and the follow-ups of 12 sit in the other references behind the conflict flag `p508`:
  - 6.1 (the pantheon's rows, names after the creators): p3_chronology_reference.chronology with
    pantheon_god_rows below;
  - 4 and 8.2 (the data kind god, history.pantheon, creator_title, the culture forms, the report entries 100 to 110,
    the changed templates, the follow-up fields): p3_data_reference (KINDS_030, GODS_030, PANTHEON_030, ...; a 0.29.0
    root on disk is read at 0.30.0 with these listings filled in, FALLBACK_030_USED);
  - 8.1 (kinds 100 to 110), {g:titled}, faction names, schema 14: p3_engine_reference (COMPILED_030, RENDER_R30);
  - 7.3 (sent monsters outside L_world): p5_02_monsters_reference.Monsters.caps_allow; prepare_sent and create_sent
    are here (Pantheon.prepare_sent, create_sent) on the monsters' lists;
  - 7.4 AllianceView::partners and 12.5 the renewal cap: p5_04_alliances_reference;
  - 7.5 champions, prophets, raise_divine, hero_of, the appointment, the epithets, the deeds of 6.5, and 12.6 (the
    take-up, a fresh tower's cause, the lapse): p5_06_figures_reference;
  - 12.2 (the stir), 12.3 (tellings mod 9), 12.4 (aid after the outcome), 12.7 A1 and A2, and the P5-07 validation
    finding D1 (invariant 12 and a secession): p5_07_depths_reference.

Commands (from the repository root):
  python3 -B worldgen/tests/reference/p5_08_pantheon_reference.py units            formulas, data listings, templates
  python3 -B worldgen/tests/reference/p5_08_pantheon_reference.py fixtures [--quick] pantheon-small and the fixture
                                                                                   files on the conflict stress
                                                                                   geographies: every path of 6,
                                                                                   invariants of 13, I0 (14)
  python3 -B worldgen/tests/reference/p5_08_pantheon_reference.py check-world DB [DB ...] [--data ROOT]
  python3 -B worldgen/tests/reference/p5_08_pantheon_reference.py golden [PATH|-] [--inputs DIR]
  python3 -B worldgen/tests/reference/p5_08_pantheon_reference.py readings

`golden` writes worldgen/tests/golden/p5-08-pantheon.txt: the fixture lines (the cases of the files under
worldgen/tests/data/pantheon/ on the stress geographies) always; with --inputs DIR also the world lines of 16
(pantheon-small at 512 on seeds 0 to 9 from DIR/pantheon-<seed>-512.db, the shipped seed 42 at 1024 from
DIR/geo-seed-42-1024.db; any 0.30.0 file of that geography, `--until regions` suffices). Recorded at integration from
this reference, never to make a C++ test pass.
"""

import copy
import hashlib
import os
import sqlite3
import sys
import tomllib

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import core_reference as cr  # noqa: E402
import p3_engine_reference as pe  # noqa: E402
import p3_names_reference as pnr  # noqa: E402
import p3_peoples_reference as pp  # noqa: E402
import p3_data_reference as pdr  # noqa: E402

GOLDEN = os.path.join(pe.REPO, "worldgen", "tests", "golden", "p5-08-pantheon.txt")
FIXTURE_DIR = os.path.join(pe.REPO, "worldgen", "tests", "data", "pantheon")
VERSION = "0.30.0"
SCHEMA = 14
HEADER = "# worldgen history v11"
# 3: appended after hist.deep.warren_kind (the 107th), in this order (120 streams); 119 and 120 are the deep's (12.2)
STREAMS = ("hist.pantheon.act", "hist.pantheon.kind", "hist.pantheon.flare", "hist.pantheon.monster_kind",
           "hist.pantheon.site_name", "hist.pantheon.cult_found", "hist.pantheon.cult_name",
           "hist.pantheon.cult_spread", "hist.pantheon.adopt", "hist.pantheon.suppress", "hist.pantheon.hide")
DEEP_STREAMS = ("hist.deep.stir", "hist.deep.warren_stir")
STREAM_COUNT = 120
KINDS = {100: "divine_bless", 101: "divine_curse", 102: "divine_champion", 103: "divine_monster", 104: "divine_mark",
         105: "cult_founded", 106: "cult_spread", 107: "cult_adopted", 108: "cult_suppressed",
         109: "tower_taken_up", 110: "tower_abandoned"}
KIND_ID = {v: k for k, v in KINDS.items()}
ACT_KEYS = ("divine_bless", "divine_curse", "divine_champion", "divine_monster", "divine_mark")
ACT_KINDS = ("bless", "curse", "champion", "monster", "mark")      # 6.3, in 8.1's order
FAVOUR = ("bless", "champion", "mark")                             # 4.1 favour weights
WRATH = ("curse", "monster", "mark")                               # 4.1 wrath weights
CAMPS = {"good": 0, "neutral": 1, "evil": 2}
CAMP_WORDS = ("good", "neutral", "evil")
DOMAINS = {"creation": 1, "war": 2, "death": 3, "harvest": 4, "craft": 5, "wild": 6, "oaths": 7, "lore": 8, "ruin": 9}
DOMAIN_WORDS = {v: k for k, v in DOMAINS.items()}
BLESS_EFFECT = {"plenty": 1, "valour": 2}
CURSE_EFFECT = {"pestilence": 1, "withering": 2, "defeat": 3}
GOD, CIV, SETTLEMENT, REGION, FIGURE, FACTION, MONSTER, SITE = 2, 3, 4, 5, 6, 7, 8, 9
LAIR, TOWER, SHRINE, TEMPLE, SANCTUM = 1, 4, 5, 6, 7
END_LOST, END_ABANDONED, END_SUPPRESSED = 4, 7, 8
PRESENT, HIDDEN, ENDED = 1, 2, 3
ENDING_SUPPRESSED, ENDING_FELL = 1, 2
CHAMPION, PROPHET, CASTER, RULER = 6, 7, 5, 1
TEMPERS = ("steady", "warlike", "peaceable", "grasping", "just")
FACTIONS_MAX = 100000
SUMMARY_GODS_MAX = 8
SUMMARY_CULTS_MAX = 6
NEVER = -(1 << 62)
STATS_KEYS = ("divine_acts", "divine_bless", "divine_curse", "divine_champion", "divine_monster", "divine_mark",
              "divine_good", "divine_neutral", "divine_evil", "divine_creators", "divine_flares", "divine_dead",
              "cults_founded", "cults_spread", "cults_adopted", "cults_suppressed", "cults_hidden", "cults_present",
              "cult_founders_prophets", "shrines_sacred", "shrines_profane", "temples", "towers_taken_up",
              "towers_abandoned", "renewals_capped", "deep_stirred")   # 10 item 8, in this order
SHRINE_WORD = {False: "holy to", True: "cursed by"}   # 11.1 shrine_word(profane)
GLYPHS = {21: "shrine", 22: "temple", 23: "sanctum"}  # 11.1
GROUP_GODS = 9
ALL_GROUPS = 0x3FF

READINGS = """\
P-R1 (4.2, 12) The follow-up fields (history.monsters.deep_stir_permille, history.alliances.renew_written_max,
     history.figures.tower_lapse_years) and history.chronology.creator_title are appended last in their records (the
     spec gives no position). The god kind's fields are in 4.1's order; a deed is {kind, role, points, who, against}.
P-R2 (7.1) The positive growth term of P3-05 is multiplied by (1000 + growth_permille) div 1000 after its floor of 1
     (max(1, d) for a growing race), so a withering can bring a term of 1 to 0.
P-R3 (6.2, 6.6, 6.10) The order inside the pantheon's T4 and Q4: the pairs of realms whose ending was read in the
     previous step are dropped; the log (6.2's deeds; the realm endings are noted); 6.10 (rows closed, cults whose
     realms fell, temples); the fade; Q4 only: the flare (6.6); 6.7 foundings; 6.8 spread; 6.9 faith and
     suppression.
P-R4 (6.3) "certain": in the first tick year of a flare-up whose trigger is the flared pair's lead deed, the flared
     pair is the candidate when it passes the candidate tests (even if another pair has a larger |R|), and acts with
     no hist.pantheon.act draw; otherwise the candidate is the largest |R| as usual.
P-R5 (6.3) The causes of an act: the pair's contributions of R's sign ranked by (|points| descending, event id
     descending); an event that holds two contributions (trade_opened's two partners) is one cause; the first three
     distinct events, ascending. e* is the first event of the ranking.
P-R6 (6.4) The regions: bless and champion at c's capital region, else its home region; curse needs a standing
     visible capital (every effect, 6.3's table); the lair search and mark's "c's capital region" read the capital
     region, else the home region. A free wild cell is a cell of P5-02 6.6.1's cells_of (not taken, not water).
P-R7 (7.5) A champion's first command writes general_appointed with again 0, later ones again 1; its start_event
     stays its divine_champion.
P-R8 (12.6) "t holds an open kind 4 site of c, or t's region holds one": an open kind 4 site of c whose settlement is
     t or whose region is t's region; the smallest id is taken up. A taken-up site's master is the new caster (its
     `tower`); the master of a site is the last caster whose tower it is.
P-R9 (6.7 to 6.9) A cult founded in a step is an entity created in the step (P5-02 6.7): it neither spreads nor is
     adopted or suppressed in that step.
P-R10 (6.9) With no reign (FigureView::temper -1) the temper factor is 1000. A faith needs c's capital (the event's
     settlement); with no capital no faith is taken. The rival gods of a faith are the gods (creators included) whose
     camp is opposed to g's, and c's creator when its camp differs from g's, each once, ascending.
P-R11 (6.8) The affinity of a creator god: same when the realm's race is its race, opposed when good against evil,
     else near; of a pantheon god: same when the camps are equal.
P-R12 (6.9) A suppression decides its hiding (the hide draw when k has no other open cult_in, then the sanctum place)
     before its event; the sanctum's hops are P5-02 7.1's (every region border) from the seat's region; the sanctum is
     named in the suppressing realm's culture.
P-R13 (6.10) A temple's ending event older than the temple waits (as P5-04a 21.4 r3); "older" compares event ids.
P-R14 (10 item 2) A flare by a god: a flare-up phase (kind 3) whose trigger_event is a cause of an act of the
     phase's first year.
P-R15 (9) The reader message for a god row lists the first failing column; the C++ tests compare message prefixes.
P-R16 (16 item 11) The v11 report fixture is H20 (h20-v11.toml), built in H19's shape by p5_08_h20_fixture.py.
P-R17 (6.6) "the pair that would be a candidate of 6.3 at year Y": the same tests with Y the quiet step's start.
P-R18 (6.4 mark) e*'s region is the event's region column, 0 none (then c's capital region, else its home region).
P-R19 (12.3, P5-04a 8.5) The tellings of kinds 93 and 98 are choices of nine words, above P5-04a 8.5's limit of 8
     ("choice of 9 words" is refused): the limit is read as 9 from 0.30.0 (the spec does not amend 8.5).
P-R20 (8.2, P3-04 8.4) The two-word choices {profane? hallowed| cursed} and {hidden?|, and its faithful fled into
     hiding} render inverted under the two-word rule (w0 for 1, w1 otherwise): the samples need cursed for profane 1
     and the hiding clause for hidden 1. The reference's listing swaps the two words (p3_data_reference.
     SPEC_TWO_WORD_030); the C++ tests expect the samples.
P-R21 (6.2, 6.3) A contribution older than regard_memory_years at the act is not cited and does not make a pair a
     candidate (add drops old contributions only when a new one comes to the pair).
Integration (docs/reviews/p5-08-integration-notes.md; the engine's readings where the spec is silent):
P-R22 (6.5, P5-06 6.4 rule 3) A cult_adopted or cult_suppressed read by figures in a later step names no ruler when
     the reign began after the event's year (the pantheon runs after the figures).
P-R23 (12.3) The tellings' d is P5-07 160's d, the old depth: (s + 4 (depth - 1)) mod 9 of the value depth.
P-R24 (12.6 item 4) A book and a blight cite the event that made the site the caster's own: its tower_founded or
     its tower_taken_up (the caster's start_event).
P-R25 (7.1) The divine force factor stands after P5-04 7.5's and P5-07 6.3's factors wherever they apply: the force
     and the local force of P3-06 r9 20.6 (invariant 10: in every battle).
P-R26 (10 item 4, 12.1 item 2) The realm lines come after the allies lines in the order gods, faith, deep, before
     the rulers lines.
P-R27 (10 item 3) "in" and "faith of" list the rows open at E or closed by the cult's ending event, by (since, id);
     `in none` when empty; the faith part is left out when empty.
P-R28 (10 item 6) A temple's or sanctum's cult reads `, of <name> (fa<id>)` after the site's name; a shrine's
     `, sacred` or `, profane`.
P-R29 (P5-06 6.4 rule 3, integration part 3a) A deed read while its realm has already ended in the state (its
     civ_ended or civ_conquered later in the same read range, e.g. a cult_adopted of T4 read in T6 after a conquest
     of T5) does not name the ruler; rule 4 names the reign with the ending event (p5_06_figures_reference.name_by_deed).
Lead rulings mirrored (PN5, docs/reviews/p5-08-impl-notes.md, last two entries): a champion needs work (an open war
     of c as a side or an open joined ally, or a huntable monster) when chosen; a champion is c's hunter ahead of its
     hero; at its raising it commands the first open war of c with no general on c's side, else waits on each such
     side for its general's death of age (p5_06_figures_reference.champion_to_war, vacancies).
"""


def _pcr():
    import p3_conflict_reference as pcr
    return pcr


def _p502():
    import p5_02_monsters_reference as p502
    return p502


def _p506():
    import p5_06_figures_reference as p506
    return p506


# ---------------------------------------------------------------------------
# Formulas (6.2 to 6.9), each as the spec writes it
# ---------------------------------------------------------------------------

def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def opposed(a, b):
    """Good (0) against evil (2); neutral has none."""
    return {a, b} == {0, 2}


def kin_of(god_race, god_camp, civ_race, civ_camp):
    """4.3: a creator (god_race != 0) is kin to its race; a pantheon god to the realms of its camp."""
    if god_race:
        return civ_race == god_race
    return civ_camp == god_camp


def foe_of(god_camp, civ_camp):
    """4.3: the opposed camp (a creator's camp is its race's)."""
    return opposed(god_camp, civ_camp)


def who_ok(who, kin, foe):
    return kin if who == "kin" else foe if who == "foe" else True


def fade_F(per_century, t):
    """6.2: F(t) = regard_fade_per_century x t div 100."""
    return per_century * t // 100


def fade_amount(per_century, Y, L):
    return fade_F(per_century, Y + L) - fade_F(per_century, Y)


def faded(R, amount):
    """R moves toward 0 by min(|R|, amount)."""
    d = min(abs(R), amount)
    return R - d if R > 0 else R + d


def add_contribution(R, con, p, e, y, regard_max, memory, cmax):
    """6.2's add on a pair: returns (R', contributions')."""
    R = clamp(R + p, -regard_max, regard_max)
    con = list(con) + [(e, y, p)]
    con = [x for x in con if x[1] >= y - memory]
    while len(con) > cmax:
        con.pop(0)
    return R, con


def act_permille(act_pm, A):
    return min(1000, act_pm * A // 1000)


def rank_causes(con, sign):
    """6.3 (reading P-R5): (lead deed, causes ascending)."""
    xs = sorted((x for x in con if x[2] * sign > 0), key=lambda x: (-abs(x[2]), -x[0]))
    order = []
    for e, _y, _p in xs:
        if e not in order:
            order.append(e)
    if not order:
        return 0, ()
    return order[0], tuple(sorted(order[:3]))


def act_telling(c, acts):
    return (c + acts) % 3


def cult_found_permille(found, fear, favourable):
    return found * (1000 if favourable else fear) // 1000


def spread_permille(spread, affinity, L=None):
    p = spread * affinity // 1000
    return p if L is None else min(1000, p * L)


def temper_permille(base, factor, L=None):
    p = base * factor // 1000
    return p if L is None else min(1000, p * L)


def growth_sum(effects, y, plenty, withering, sacred=0):
    """7.1: the effects in force at y (start <= y < start + years) added, plus a sacred shrine; clamped."""
    g = sacred
    for x in effects:
        if x["start"] <= y < x["start"] + x["years"]:
            g += plenty if x["kind"] == "plenty" else -withering if x["kind"] == "withering" else 0
    return clamp(g, -900, 1000)


def force_sum(effects, y, valour, defeat):
    f = 0
    for x in effects:
        if x["start"] <= y < x["start"] + x["years"]:
            f += valour if x["kind"] == "valour" else -defeat if x["kind"] == "defeat" else 0
    return clamp(f, -500, 500)


def growth_term(d, g):
    """7.1: the positive term times (1000 + growth) div 1000 (reading P-R2)."""
    return d * (1000 + g) // 1000 if d > 0 else d


def force_term(f, fp):
    return f * (1000 + fp) // 1000


def pestilence_dead(P, toll):
    return P * toll // 1000


def form_name(forms, start, god_name, registry, problem=None):
    """4.4's naming: the first valid free form in cyclic order from start; else the start form + roman(k)."""
    p502 = _p502()
    problem = problem or p502.name_problem
    n = len(forms)
    first = None
    for j in range(n):
        nm, parts = p502.fill_form(forms[(start + j) % n], "{god}", god_name)
        if first is None:
            first = nm
        if problem(nm, parts) is None and nm not in registry:
            registry.add(nm)
            return nm
    k = 2
    while first + " " + pnr.roman(k) in registry:
        k += 1
    nm = first + " " + pnr.roman(k)
    registry.add(nm)
    return nm


def god_title_ok(s, with_plural=False):
    """4.1 grammar god_title: `the ` then ASCII letters, single spaces, ' and -; at most 40 bytes. creator_title: with
    {plural} exactly once."""
    if len(s.encode()) > 40 or not s.startswith("the "):
        return False
    if with_plural:
        if s.count("{plural}") != 1:
            return False
        s = s.replace("{plural}", "X")
    elif "{" in s or "}" in s:
        return False
    ok = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ '-")
    return all(ch in ok for ch in s) and "  " not in s and not s.endswith(" ")


def god_form_ok(s, limit=48):
    """4.4 grammar god_form: name_form's with the slot {god} exactly once in place of {root}."""
    if len(s.encode()) > limit or s.count("{god}") != 1 or "{root}" in s:
        return False
    rest = s.replace("{god}", "")
    return all(32 <= ord(ch) < 127 and ch not in "{}" for ch in rest)


# ---------------------------------------------------------------------------
# Data (4): the god definitions, history.pantheon and their checks
# ---------------------------------------------------------------------------

def god_messages(did, d, monsters, cultures):
    """4.1's checks on one definition, in the order of the spec (each message prefixed by the caller)."""
    out = []
    if d["name_culture"] not in cultures:
        out.append("god_culture: name_culture %s is not a culture" % d["name_culture"])
    if d["domain"] == "creation" or d["domain"] not in DOMAINS:
        out.append("grammar: domain %s" % d["domain"])
    if not god_title_ok(d["title"]):
        out.append("god_title: title %s" % d["title"])
    for i, k in enumerate(d["monsters"]):
        if monsters.get(k) not in ("wild", "dead"):
            out.append("god_monsters: monsters[%d] %s is not a wild or dead monster" % (i, k))
    if d["wrath"]["monster"] > 0 and not d["monsters"]:
        out.append("god_monster_weight: wrath.monster needs monsters")
    if all(d["favour"][x] == 0 for x in FAVOUR):
        out.append("god_favour_weights: favour weights must not all be 0")
    if all(d["wrath"][x] == 0 for x in WRATH):
        out.append("god_wrath_weights: wrath weights must not all be 0")
    return out


def deed_messages(deeds, catalogue):
    """4.3 pantheon_deed_* (each `deeds[<i>]: ...`); catalogue: {key: kind dict}."""
    out = []
    for i, d in enumerate(deeds):
        k = catalogue.get(d["kind"])
        if k is None:
            out.append("deeds[%d]: kind %s is unknown" % (i, d["kind"]))
            continue
        civ_roles = {r["role"] for r in k["roles"] if r["type"] == CIV}
        for r in (d["role"],) + ((d["against"],) if d["against"] else ()):
            if r not in civ_roles:
                out.append("deeds[%d]: role %s of %s is not a civilization role" % (i, r, d["kind"]))
        if d["against"] and d["against"] == d["role"]:
            out.append("deeds[%d]: against is role" % i)
        if d["points"] == 0:
            out.append("deeds[%d]: points 0" % i)
        if d["who"] == "foe" and d["points"] >= 0:
            out.append("deeds[%d]: a deed for foes must be negative" % i)
    return out


def pantheon_messages(t):
    """4.2's two checks."""
    out = []
    if t["flare_regard"] < t["act_threshold"]:
        out.append("pantheon_flare_above_act: flare_regard %d is below act_threshold %d" % (
            t["flare_regard"], t["act_threshold"]))
    if t["act_threshold"] > t["regard_max"]:
        out.append("pantheon_act_below_max: act_threshold %d is above regard_max %d" % (
            t["act_threshold"], t["regard_max"]))
    return out


def code_default_tuning():
    """4.2: a tuning built in code (inert)."""
    t = pdr.pantheon_shipped_030()
    t.update(pdr.PANTHEON_CODE_DEFAULTS_030)
    return t


def pantheon_god_rows(data, seed, registry):
    """6.1 C5a and 8.1 item 4 for p3_chronology_reference.chronology: [(row, attempt)] of the pantheon gods, rows
    (id, name, camp, domain, title, race 0) in canonical id order, named after the creators."""
    root = getattr(data, "root", "game/data")
    defs = pdr.load_root(root, kinds=("god",))["god"]
    n = len(data.races)
    out = []
    for i, (did, d) in enumerate(defs):
        gid = n + i + 1
        cid = d["name_culture"]
        c = data.culture_number[cid]
        name, t = pnr.make_name(data.cultures[cid]["naming"], cid, "god", seed, c, gid, registry,
                                filt=getattr(data, "filter", pnr.EMPTY_FILTER))
        out.append(((gid, name, CAMPS[d["camp"]], DOMAINS[d["domain"]], d["title"], 0), t))
    return out


# ---------------------------------------------------------------------------
# The subsystem (5, 6, 7) inside a p3_conflict_reference World
# ---------------------------------------------------------------------------

class Pantheon:
    """PantheonSubsystem (5, 6) and its DivineView (7.1) for a conflict World w (ids only in its members)."""

    def __init__(self, w, dat, inp):
        self.w = w
        self.inp = inp
        self.dat = dat
        self.t = dat.pantheon if getattr(dat, "pantheon", None) is not None else code_default_tuning()
        for s in STREAMS:
            w.keys[s] = cr.stream_key(inp.seed, s)
        self.A = self.t["activity_permille"][inp.params.get("activity", "normal")]
        self.threshold = dat.base.engine[inp.params["detail"]]["flare_threshold"]
        self.n_creators = len(inp.races)
        defs = list(getattr(dat, "gods", []) or [])
        self.gods = {}
        for g in inp.gods:
            gid = g[0]
            if gid <= self.n_creators:
                rec = self.t["creator"]
                race = g[5] if len(g) > 5 else gid
                title = g[4] if len(g) > 4 else ""
                self.gods[gid] = dict(id=gid, name=g[1], camp=g[2], race=race, title=title, key="", **copy.deepcopy(
                    {k: rec[k] for k in ("favour", "wrath", "bless", "curse", "monsters", "deeds")}))
            else:
                did, d = defs[gid - self.n_creators - 1]
                self.gods[gid] = dict(id=gid, name=g[1], camp=g[2], race=0, title=g[4], key=did, **copy.deepcopy(
                    {k: d[k] for k in ("favour", "wrath", "bless", "curse", "monsters", "deeds")}))
        # deeds by kind, compiled once (ascending god, then deed index)
        self.deeds_by_kind = {}
        for gid in sorted(self.gods):
            for i, d in enumerate(self.gods[gid]["deeds"]):
                if d["kind"] in w.cat_by_key:
                    self.deeds_by_kind.setdefault(d["kind"], []).append((gid, i, d))
        root = getattr(dat, "root", None) or getattr(dat.base, "root", None) or "game/data"
        self.cul = {k: c["naming"] for k, c in pdr.load_root(root, kinds=("culture",))["culture"]}
        # state (5)
        self.pairs = {}            # (g, c) -> dict(R, con [(e, y, p)], last)
        self.last_act = {}         # g -> year
        self.acts = {g: 0 for g in self.gods}
        self.effects = {}          # c -> [dict(kind, start, years, event)]
        self.pending_acts = []
        self.act_meta = {}         # act event -> dict(kind, god, civ, figure, site, settlement, profane, sign)
        self.cults = []            # faction records, id = index + 1
        self.live_cult = {}        # g -> k (status 1 or 2)
        self.last_kc = {}          # (k, c) -> the latest cult_founded / cult_spread / cult_adopted of k in c
        self.pending_flare = 0
        self.flared = None
        self.cursor = 1
        self.ended_seen = set()    # realms whose ending was read in this step (their pairs dropped at the next)
        self.realm_ends = []       # (c, ending event) read in this step, for 6.10
        self._last_phase = None
        self.first_flare = False
        self.log = []              # (year, what, ...) for the checks (not state)
        self.suppressed_years = {}   # P5-11 12.2: (k, c) -> years of k's cult_suppressed with civilization c
        self.returns_barred = 0      # P5-11 10 item 6: cult_returns_barred (not in rows)

    # --- helpers
    def god(self, g):
        return self.gods[g]

    def civ_camp(self, c):
        return self.w.race[self.w.race_c(c)][5]

    def kin(self, g, c):
        gd = self.gods[g]
        return kin_of(gd["race"], gd["camp"], self.w.race_c(c), self.civ_camp(c))

    def foe(self, g, c):
        return foe_of(self.gods[g]["camp"], self.civ_camp(c))

    def alive_visible(self, c):
        cv = self.w.civ(c)
        return cv["ended_year"] is None and self.w.visible(cv)

    def D(self, s, a, b, k):
        return self.w.draw(s, a, b, k)

    def quiet(self):
        return self.w.quiet_used is not None

    def capital_region(self, c):
        cv = self.w.civ(c)
        return self.w.stl(cv["capital"])["region"] if cv["capital"] else cv["home_region"]

    def standing_capital(self, c):
        w = self.w
        cv = w.civ(c)
        if not cv["capital"]:
            return None
        t = w.stl(cv["capital"])
        return t if w.standing(t) and w.visible(t) and t["owner_civ"] == c else None

    def culture_key(self, c):
        return self.w.culture[self.w.civ(c)["culture"]][1]

    def open_rel(self, kind, k, c):
        return (FACTION, k, CIV, c, kind) in self.w.relations

    def faith_of(self, c):
        for key in self.w.relations:
            if key[4] == "faith_of" and key[2] == CIV and key[3] == c:
                return key[1]
        return 0

    def faith_god(self, c):
        k = self.faith_of(c)
        return self.cults[k - 1]["god"] if k else 0

    def cult_in_keys(self, k=None):
        return sorted((key for key in self.w.relations if key[4] == "cult_in" and (k is None or key[1] == k)),
                      key=_pcr().key_order)

    def sites(self):
        return self.w.monsters.sites

    def next_site_id(self):
        return len(self.w.monsters.sites) + 1

    def open_temple_at(self, t):
        return any(s["kind"] == TEMPLE and s["ending"] == 0 and s["settlement"] == t for s in self.sites())

    def name_place(self, c, field, key, god_name):
        forms = self.cul[self.culture_key(c)][field]
        start = pnr.pick(self.D("hist.pantheon.site_name" if field != "cult_forms" else "hist.pantheon.cult_name",
                                key, 0, 0), len(forms))
        return form_name(forms, start, god_name, self.w.registry)

    def open_site(self, spec):
        """MonstersSubsystem::open_site with kind 5 to 7 and the next id."""
        m = self.w.monsters
        if spec["id"] != len(m.sites) + 1 or spec["kind"] not in (SHRINE, TEMPLE, SANCTUM):
            _pcr().fatal("history: monsters: open_site: site %d of kind %d is not the next site" % (
                spec["id"], spec["kind"]))
        m.sites.append(spec)

    # --- the DivineView (7.1)
    def growth_permille(self, civ, settlement, year):
        t = self.t
        sacred = 0
        for s in self.sites():
            if s["kind"] == SHRINE and not s.get("profane") and s["settlement"] == settlement and settlement:
                sacred = t["sacred_growth_permille"]
                break
        return growth_sum(self.effects.get(civ, ()), year, t["plenty_growth_permille"],
                          t["withering_growth_permille"], sacred)

    def force_permille(self, civ, year):
        t = self.t
        return force_sum(self.effects.get(civ, ()), year, t["valour_permille"], t["defeat_permille"])

    def force_act(self, civ, year, valour):
        """P5-11 12.1: DivineView::force_act: the largest event id among the acts of kind 100 with effect 2 (valour)
        or of kind 101 with effect 3 (defeat) on civ whose effect is in force at year; 0 none."""
        want = "valour" if valour else "defeat"
        es = [x["event"] for x in self.effects.get(civ, ()) if x["kind"] == want and
              x["start"] <= year < x["start"] + x["years"]]
        return max(es) if es else 0

    def shunned(self, region, year):
        for s in self.sites():
            if s["kind"] == SHRINE and s.get("profane") and s["region"] == region and \
                    s["origin_year"] <= year < s["origin_year"] + self.t["profane_years"]:
                return True
        return False

    # --- 6.1 steps
    def t4(self):
        w = self.w
        self.first_flare = w.phase_kind == pe.FLARE and w.phase != self._last_phase
        self._last_phase = w.phase
        self.step(quiet=False)

    def q4(self):
        self.step(quiet=True)

    def t6(self):
        """6.10 and 6.12 item 5 (integration E3): in T6 of the last year, after every other subsystem, the endings
        of the log after the T4 cursor are applied as a next step would, with no event and no draw."""
        w = self.w
        if w.year != w.figures.E - 1:
            return
        self.realm_ends = []
        last = len(w.events)
        for e in range(self.cursor, last + 1):
            ev = w.events[e - 1]
            if ev["key"] in ("civ_ended", "civ_conquered"):
                self.realm_ends.append((_one(w, ev, "civilization"), e))
        self.cursor = last + 1
        self.endings()

    def step(self, quiet):
        for c in sorted(self.ended_seen):   # 6.2: dropped at the step after the ending event was read
            for k in [k for k in self.pairs if k[1] == c]:
                del self.pairs[k]
        self.ended_seen = set()
        self.realm_ends = []
        self.read_log()
        self.endings()
        self.fade()
        if quiet:
            self.flare()
        self.foundings(quiet)
        self.spread(quiet)
        self.faith_and_suppression(quiet)

    # --- 6.2 regard
    def add(self, g, c, p, e, y):
        t = self.t
        pr = self.pairs.setdefault((g, c), dict(R=0, con=[], last=None))
        pr["R"], pr["con"] = add_contribution(pr["R"], pr["con"], p, e, y, t["regard_max"], t["regard_memory_years"],
                                              t["contributions_max"])

    def read_log(self):
        w = self.w
        pcr = _pcr()
        last = len(w.events)
        first = self.cursor
        self.cursor = last + 1
        for e in range(first, last + 1):
            ev = w.events[e - 1]
            key = ev["key"]
            if key in ("civ_ended", "civ_conquered"):
                c = _one(w, ev, "civilization")
                self.realm_ends.append((c, e))
                self.ended_seen.add(c)
            for g, _i, d in self.deeds_by_kind.get(key, ()):
                seq = _seq(w, key, d["role"])
                against = [x for (_j, s, _t, x) in ev["participants"] if d["against"] and s == _seq(w, key,
                                                                                                    d["against"])]
                for (_j, s, _t, r) in ev["participants"]:
                    if s != seq or r == 0:
                        continue
                    if not who_ok(d["who"], self.kin(g, r), self.foe(g, r)):
                        continue
                    if d["against"] and not any(self.kin(g, x) for x in against if x):
                        continue
                    if d["points"] > 0 and self.foe(g, r):
                        continue
                    cv = w.civ(r)
                    if cv["ended_year"] is not None and cv["ended_year"] < ev["year"]:
                        continue
                    self.add(g, r, d["points"], e, ev["year"])
        del pcr

    def fade(self):
        w = self.w
        amount = fade_amount(self.t["regard_fade_per_century"], w.year, w.years)
        for k in sorted(self.pairs):
            self.pairs[k]["R"] = faded(self.pairs[k]["R"], amount)

    # --- 6.3 the source (pure) and the applier
    def remembered(self, g, c):
        """Reading P-R21: the contributions not older than regard_memory_years at the step's year (add drops older
        ones only when a new one comes; a forgotten contribution is not cited)."""
        Y = self.w.year
        return [x for x in self.pairs[(g, c)]["con"] if x[1] >= Y - self.t["regard_memory_years"]]

    def is_candidate(self, g, c, Y, min_abs):
        pr = self.pairs.get((g, c))
        if pr is None:
            return False
        R = pr["R"]
        if abs(R) < min_abs or R == 0 or not self.alive_visible(c):
            return False
        t = self.t
        la = self.last_act.get(g)
        if la is not None and Y - la < t["god_rest_years"]:
            return False
        if pr["last"] is not None and Y - pr["last"] < t["pair_rest_years"]:
            return False
        sign = 1 if R > 0 else -1
        if not any(x[2] * sign > 0 for x in self.remembered(g, c)):
            return False
        if R > 0 and self.foe(g, c):
            return False
        if R < 0 and self.faith_god(c) == g:
            return False
        return True

    def best_candidate(self, Y, min_abs):
        best = None
        for (g, c) in sorted(self.pairs):
            if not self.is_candidate(g, c, Y, min_abs):
                continue
            key = (-abs(self.pairs[(g, c)]["R"]), g, c)
            if best is None or key < best:
                best = key
        return None if best is None else (best[1], best[2])

    def lead_and_causes(self, g, c):
        pr = self.pairs[(g, c)]
        return rank_causes(self.remembered(g, c), 1 if pr["R"] > 0 else -1)

    def monster_kinds_of(self, g):
        m = self.w.monsters
        keys = {m.kind_key(k): k for k in range(1, len(m.kinds) + 1)}
        out = []
        for key in self.gods[g]["monsters"]:
            k = keys.get(key)
            if k is None:
                continue
            if sum(1 for x in m.living() if x["kind"] == k) < m.kind(k)["max_alive"]:
                out.append(k)
        return out

    def lair_region_for_sent(self, from_region, hops):
        """7.3 MonstersSubsystem::lair_region_for_sent: (hops, id) ascending, within hops, no open lair, a free
        wild cell; 0 none."""
        m = self.w.monsters
        H = m.geo.hops(from_region)
        for r in sorted(H, key=lambda r: (H[r], r)):
            if H[r] > hops or m.open_lair_in(r):
                continue
            if any(True for _ in m.cells_of(r)):
                return r
        return 0

    def mark_place(self, c, e_star):
        """6.4 mark: (cell, settlement, region) or None."""
        w, m = self.w, self.w.monsters
        ev = w.events[e_star - 1] if e_star else None
        u = ev["settlement"] if ev is not None else 0
        if u and not any(s["kind"] == SHRINE and s["settlement"] == u for s in m.sites):
            st = w.stl(u)
            return (st["x"], st["y"]), u, st["region"]
        R2 = ev["region"] if ev is not None and ev["region"] else self.capital_region(c)
        if any(s["kind"] == SHRINE and s["ending"] == 0 and s["region"] == R2 for s in m.sites):
            return None
        cell = _p506().height_cell(m, R2, [], 0)
        if cell is None:
            return None
        return cell, 0, R2

    def eligible(self, kind, g, c, e_star):
        w = self.w
        if kind == "bless":
            return True
        if kind == "curse":
            return self.standing_capital(c) is not None
        if kind == "champion":
            return w.figures is not None and w.figures.champion_of(c) == 0 and self.champion_work(c)
        if kind == "monster":
            return bool(self.monster_kinds_of(g)) and self.lair_region_for_sent(
                self.capital_region(c), self.t["send_hops"]) != 0
        if kind == "mark":
            return self.mark_place(c, e_star) is not None
        return False

    def champion_work(self, c):
        """6.3 (lead ruling PN5, first): c is a side or an open joined ally of an open war (P5-04 7.6), or some awake,
        unsealed monster remembers a raid on c within hunt_memory_years and c's hunters against it (P5-02 6.4.2, the
        levy times the prowess, before any hero) are above 0. War first, then the monsters, by ascending id."""
        w = self.w
        if w.open_wars_of(c) > 0:
            return True
        m = w.monsters
        return m is not None and m.huntable_by(c)

    def interventions(self):
        """6.3 the source: [] or [(year, god, kind, civilization, region, scripted)]; pure (draws only)."""
        w, t = self.w, self.t
        Y = w.year
        cand, certain = None, False
        if self.flared is not None and self.first_flare and w.phase_trigger == self.flared[2]:
            g, c, _e = self.flared
            if self.is_candidate(g, c, Y, t["act_threshold"]):
                cand, certain = (g, c), True
        if cand is None:
            cand = self.best_candidate(Y, t["act_threshold"])
        if cand is None:
            return []
        g, c = cand
        if not certain and not pp.chance_pm(self.D("hist.pantheon.act", g, c, 0), act_permille(t["act_permille"],
                                                                                                self.A)):
            return []
        R = self.pairs[(g, c)]["R"]
        e_star, _causes = self.lead_and_causes(g, c)
        names = FAVOUR if R > 0 else WRATH
        rec = self.gods[g]["favour" if R > 0 else "wrath"]
        W = [rec[n] if self.eligible(n, g, c, e_star) else 0 for n in names]
        if sum(W) == 0:
            return []
        kind = names[pnr.weighted(self.D("hist.pantheon.kind", g, c, 0), W)]
        return [dict(year=Y, god=g, kind=kind, civ=c, region=self.kind_region(kind, g, c, e_star), scripted=False,
                     certain=certain)]

    def kind_region(self, kind, g, c, e_star):
        if kind == "monster":
            return self.lair_region_for_sent(self.capital_region(c), self.t["send_hops"])
        if kind == "mark":
            return self.mark_place(c, e_star)[2]
        return self.capital_region(c)

    def t5(self):
        """7.6: the engine validates each intervention and calls the applier in source order."""
        w = self.w
        for iv in self.interventions():
            if iv["year"] != w.year or iv["kind"] not in ACT_KINDS or not self.alive_visible(iv["civ"]):
                continue   # invalid: dropped without an event
            self.apply(iv)

    def apply(self, iv):
        w, t = self.w, self.t
        Y, g, c, kind = w.year, iv["god"], iv["civ"], iv["kind"]
        if not self.eligible(kind, g, c, self.lead_and_causes(g, c)[0]):
            _pcr().fatal("history: pantheon: %s of god %d on civilization %d could not be applied" % (kind, g, c))
        e_star, causes = self.lead_and_causes(g, c)
        R = self.pairs[(g, c)]["R"]
        tel = act_telling(c, self.acts[g])
        gd = self.gods[g]
        meta = dict(kind=kind, god=g, civ=c, figure=0, site=0, settlement=0, profane=R < 0, sign=1 if R > 0 else -1,
                    lead=e_star, certain=iv.get("certain", False))
        actor = (("actor", GOD, g),)
        if kind == "bless":
            eff = BLESS_EFFECT[gd["bless"]]
            e = w.append("divine_bless", region=iv["region"], participants=actor + (("civilization", CIV, c),),
                         values=(eff, t["bless_years"], tel, 0), causes=causes)
            self.effects.setdefault(c, []).append(dict(kind=gd["bless"], start=Y, years=t["bless_years"], event=e))
        elif kind == "curse":
            eff = CURSE_EFFECT[gd["curse"]]
            hit = []
            if gd["curse"] == "pestilence":
                for u in w.settlements:
                    if u["owner_civ"] == c and u["region"] == iv["region"] and w.standing(u) and w.visible(u):
                        hit.append((u["id"], pestilence_dead(u["population"], t["pestilence_toll_permille"])))
            dead = sum(d for _u, d in hit)
            years = 0 if gd["curse"] == "pestilence" else t["curse_years"]
            e = w.append("divine_curse", region=iv["region"], participants=actor + (("civilization", CIV, c),),
                         values=(eff, dead, years, tel, 0), causes=causes)
            for u, d in hit:
                if d > 0:
                    st = w.stl(u)
                    st["population"] -= d
                    w.last_red[u] = e    # note_reduction
            if years:
                self.effects.setdefault(c, []).append(dict(kind=gd["curse"], start=Y, years=years, event=e))
            meta["hit"] = hit
        elif kind == "champion":
            fv = w.figures
            f = fv.next_figure_id()
            e = w.append("divine_champion", region=iv["region"],
                         participants=actor + (("figure", FIGURE, f), ("civilization", CIV, c)), values=(tel, 0),
                         causes=causes)
            fv.raise_divine(f, CHAMPION, c, e, t["champion_epithet"].replace("{god}", gd["name"]))
            meta["figure"] = f
        elif kind == "monster":
            ks = self.monster_kinds_of(g)
            m = w.monsters
            k = ks[pnr.weighted(self.D("hist.pantheon.monster_kind", g, c, 0), [m.kind(x)["wake_rate"] for x in ks])]
            sm = self.prepare_sent(k, iv["region"])
            e = w.append("divine_monster", region=iv["region"],
                         participants=actor + (("monster", MONSTER, sm["monster"]), ("civilization", CIV, c),
                                               ("lair", SITE, sm["lair"])), values=(tel, 0), causes=causes)
            self.create_sent(sm, e)
            meta["monster"], meta["lair"] = sm["monster"], sm["lair"]
        else:
            cell, u, R2 = self.mark_place(c, e_star)
            s = self.next_site_id()
            name = self.name_place(c, "profane_forms" if R < 0 else "sacred_forms", s, gd["name"])
            e = w.append("divine_mark", region=R2, settlement=u,
                         participants=actor + (("civilization", CIV, c), ("site", SITE, s)),
                         values=(1 if R < 0 else 0, tel, 0), causes=causes)
            self.open_site(dict(id=s, name=name, kind=SHRINE, region=R2, x=cell[0], y=cell[1], settlement=u, founder=c,
                                monster=0, origin_year=Y, origin_event=e, ended_year=None, ending_event=0, ending=0,
                                hostile=None, profane=R < 0, god=g))
            meta["site"], meta["settlement"] = s, u
        # the spend
        pr = self.pairs[(g, c)]
        pr["R"], pr["con"], pr["last"] = 0, [], Y
        self.last_act[g] = Y
        self.acts[g] += 1
        self.pending_acts.append(e)
        self.act_meta[e] = meta
        self.log.append((Y, "act", kind, g, c, e, meta["certain"]))

    # --- 7.3 sent monsters
    def prepare_sent(self, k, region):
        """Names, ids and the cell; no state change (the names join the registry when the event is written)."""
        m = self.w.monsters
        p502 = _p502()
        mid, lid = len(m.monsters) + 1, len(m.sites) + 1
        if mid > p502.MONSTERS_MAX:
            pe.fatal("history: bound: monsters exceed 100000")
        name, root = m.monster_name(k, mid, m.place_of(region, 0) if p502.p503() and m.kind(k)["wake"] == "dead"
                                    else None)
        cell = m.wild_cell(region)
        lname = m.lair_name(k, lid, root)
        return dict(monster=mid, lair=lid, kind=k, region=region, cell=cell, name=name, root=root, lair_name=lname)

    def create_sent(self, sm, e):
        w, m = self.w, self.w.monsters
        Y = w.year
        m.sites.append(dict(id=sm["lair"], name=sm["lair_name"], kind=LAIR, region=sm["region"], x=sm["cell"][0],
                            y=sm["cell"][1], settlement=0, founder=0, monster=sm["monster"], origin_year=Y,
                            origin_event=e, ended_year=None, ending_event=0, ending=0, hostile=None))
        m.monsters.append(dict(id=sm["monster"], kind=sm["kind"], name=sm["name"], root=sm["root"], status=1, born=Y,
                               born_event=e, lair=sm["lair"], waking=e, sleeping=0, drive_until=0, hoard=0,
                               holds=False, raids={}, died=None, death_event=0, slayer=0, overrun=0, raid_years={},
                               place_raids={}, dead_kind=m.kind(sm["kind"])["wake"] == "dead"))

    # --- 6.6 the flare
    def flare(self):
        w, t = self.w, self.t
        self.pending_flare = 0
        self.flared = None
        cand = self.best_candidate(w.year, t["flare_regard"])
        if cand is None:
            return
        g, c = cand
        if pp.chance_pm(self.D("hist.pantheon.flare", g, c, 0), min(1000, t["flare_permille"] * self.A // 1000)):
            e_star, _ = self.lead_and_causes(g, c)
            self.pending_flare = e_star
            self.flared = (g, c, e_star)
            self.log.append((w.year, "flare", g, c, e_star))

    def tension(self, start, length):
        """6.6: pure (the log is the checks' record of what the source reported)."""
        v = (self.threshold, self.pending_flare) if self.pending_flare else (0, 0)
        if not hasattr(self, "tension_log"):
            self.tension_log = []
        self.tension_log.append((start, v[0], v[1]))
        return v

    # --- 6.7 foundings
    def foundings(self, quiet):
        w, t = self.w, self.t
        keep = []
        for a in sorted(self.pending_acts):
            meta = self.act_meta[a]
            c, g = meta["civ"], meta["god"]
            if not self.alive_visible(c) or self.live_cult.get(g, 0):
                continue
            if quiet and w.quiet_events_left(c) <= 0:
                keep.append(a)
                continue
            fav = meta["kind"] in ("bless", "champion") or (meta["kind"] == "mark" and not meta["profane"])
            p = cult_found_permille(t["cult_found_permille"], t["cult_fear_permille"], fav)
            if not pp.chance_pm(self.D("hist.pantheon.cult_found", a, 0, 0), p):
                continue
            tt = self.seat_of(meta, c)
            if tt is None:
                continue
            self.found(a, meta, c, g, tt)
        self.pending_acts = keep

    def seat_of(self, meta, c):
        w = self.w
        tt = None
        if meta["kind"] == "mark" and meta["settlement"]:
            u = w.stl(meta["settlement"])
            if w.standing(u) and w.visible(u) and u["owner_civ"] == c:
                tt = u
        if tt is None:
            tt = self.standing_capital(c)
        if tt is not None and not self.open_temple_at(tt["id"]):
            return tt
        cands = [s for s in w.settlements if s["owner_civ"] == c and w.standing(s) and w.visible(s) and
                 not self.open_temple_at(s["id"])]
        if not cands:
            return None
        return min(cands, key=lambda s: (-s["population"], s["id"]))

    def found(self, a, meta, c, g, tt):
        w = self.w
        fv = w.figures
        f, prophet = 0, False
        if meta["kind"] == "champion" and meta["figure"] and fv.alive(meta["figure"]):
            f = meta["figure"]
        if not f and fv is not None:
            f = fv.caster_of(c) or fv.ruler_figure(c)
        if not f:
            if fv is None:
                return
            f, prophet = fv.next_figure_id(), True
        k = len(self.cults) + 1
        if k > FACTIONS_MAX:
            _pcr().fatal("history: bound: factions exceed 100000")
        gname = self.gods[g]["name"]
        cname = self.name_place(c, "cult_forms", k, gname)
        s = self.next_site_id()
        tname = self.name_place(c, "temple_forms", s, gname)
        e = w.append("cult_founded", region=tt["region"], settlement=tt["id"],
                     participants=(("faction", FACTION, k), ("god", GOD, g), ("civilization", CIV, c),
                                   ("founder", FIGURE, f), ("site", SITE, s)), values=(k % 3,), causes=(a,))
        if prophet:
            fv.raise_divine(f, PROPHET, c, e, "")
        self.open_site(dict(id=s, name=tname, kind=TEMPLE, region=tt["region"], x=tt["x"], y=tt["y"],
                            settlement=tt["id"], founder=c, monster=0, origin_year=w.year, origin_event=e,
                            ended_year=None, ending_event=0, ending=0, hostile=None, cult=k))
        self.cults.append(dict(id=k, name=cname, god=g, civ=c, founder=f, seat=s, founded=w.year, event=e,
                               status=PRESENT, ended=None, ending_event=0, ending=0, sanctum=0, serial=w.serial,
                               prophet=prophet))
        self.live_cult[g] = k
        w.open_rel((FACTION, k, CIV, c, "cult_in"), e)
        self.last_kc[(k, c)] = e
        self.log.append((w.year, "founded", k, g, c, f, prophet, e))

    # --- 6.8 spread
    def affinity(self, g, a):
        gd = self.gods[g]
        aff = self.t["affinity_permille"]
        same = gd["camp"] == self.civ_camp(a)   # 6.8 (integration E2): camps, also for a creator
        if same:
            return aff["same"]
        return aff["opposed"] if opposed(gd["camp"], self.civ_camp(a)) else aff["near"]

    def spread(self, quiet):
        w, t = self.w, self.t
        al = w.alliances
        if al is None:
            return
        for cult in list(self.cults):
            k = cult["id"]
            if cult["status"] != PRESENT or cult["serial"] == w.serial:
                continue
            g = cult["god"]
            for a in w.avc():
                if self.open_rel("cult_in", k, a):
                    continue
                link = None
                for cp, ev0 in al.partners(a):
                    if self.open_rel("cult_in", k, cp):
                        link = (cp, ev0)
                        break
                if link is None:
                    continue
                if self.return_barred(k, a):   # P5-11 12.2: no candidate within cult_return_years of a suppression
                    self.returns_barred += 1
                    continue
                if quiet and w.quiet_events_left(a) <= 0:
                    continue
                p = spread_permille(t["cult_spread_permille"], self.affinity(g, a), w.years if quiet else None)
                if not pp.chance_pm(self.D("hist.pantheon.cult_spread", k, a, 0), p):
                    continue
                causes = tuple(sorted({cult["event"]} | ({link[1]} if link[1] else set())))
                e = w.append("cult_spread", region=self.capital_region(a),
                             participants=(("faction", FACTION, k), ("god", GOD, g), ("civilization", CIV, a),
                                           ("from", CIV, link[0])), causes=causes)
                w.open_rel((FACTION, k, CIV, a, "cult_in"), e)
                self.last_kc[(k, a)] = e
                self.log.append((w.year, "spread", k, a, link[0], link[1], e))
                break

    # --- 6.9 faith, suppression, hiding
    def temper_factor(self, rec, c):
        fv = self.w.figures
        tau = fv.temper(c) if fv is not None else -1
        return 1000 if tau < 0 else rec[TEMPERS[tau]]

    def faith_and_suppression(self, quiet):
        w, t = self.w, self.t
        for key in self.cult_in_keys():
            k, c = key[1], key[3]
            if key not in w.relations:
                continue
            cult = self.cults[k - 1]
            if cult["serial"] == w.serial or not self.alive_visible(c):
                continue
            g = cult["god"]
            if quiet and w.quiet_events_left(c) <= 0:
                continue
            done = False
            pr = self.pairs.get((g, c))
            R = pr["R"] if pr else 0
            tt = self.standing_capital(c) if w.civ(c)["capital"] else None
            cap = w.civ(c)["capital"]
            if not self.faith_of(c) and cap and (R >= t["adopt_regard"] or cult["civ"] == c):
                p = temper_permille(t["adopt_permille"], self.temper_factor(t["temper_adopt_permille"], c),
                                    w.years if quiet else None)
                if pp.chance_pm(self.D("hist.pantheon.adopt", k, c, 0), p):
                    self.adopt(k, c, g, cult, tt or w.stl(cap))
                    done = True
            if done:
                continue
            fg = self.faith_god(c)
            if self.faith_of(c) == k:
                continue
            hostile = opposed(self.gods[g]["camp"], self.civ_camp(c)) or (
                fg != 0 and opposed(self.gods[fg]["camp"], self.gods[g]["camp"]))
            if not hostile:
                continue
            p = temper_permille(t["suppress_permille"], self.temper_factor(t["temper_suppress_permille"], c),
                                w.years if quiet else None)
            if pp.chance_pm(self.D("hist.pantheon.suppress", k, c, 0), p):
                self.suppress(k, c, g, cult)

    def adopt(self, k, c, g, cult, tt):
        w, t = self.w, self.t
        s = 0
        name = None
        if w.standing(tt) and not self.open_temple_at(tt["id"]):
            s = self.next_site_id()
            name = self.name_place(c, "temple_forms", s, self.gods[g]["name"])
        cause = self.last_kc.get((k, c)) or w.relations[(FACTION, k, CIV, c, "cult_in")]
        e = w.append("cult_adopted", region=tt["region"], settlement=tt["id"],
                     participants=(("faction", FACTION, k), ("god", GOD, g), ("civilization", CIV, c)) +
                     ((("site", SITE, s),) if s else ()), causes=(w.relations[(FACTION, k, CIV, c, "cult_in")],))
        del cause
        if s:
            self.open_site(dict(id=s, name=name, kind=TEMPLE, region=tt["region"], x=tt["x"], y=tt["y"],
                                settlement=tt["id"], founder=c, monster=0, origin_year=w.year, origin_event=e,
                                ended_year=None, ending_event=0, ending=0, hostile=None, cult=k))
        w.open_rel((FACTION, k, CIV, c, "faith_of"), e)
        self.last_kc[(k, c)] = e
        self.add(g, c, t["faith_points"], e, w.year)
        creator = w.race_c(c)
        rivals = sorted({x for x in self.gods if opposed(self.gods[x]["camp"], self.gods[g]["camp"])} |
                        ({creator} if self.gods[creator]["camp"] != self.gods[g]["camp"] else set()))
        for x in rivals:
            self.add(x, c, -t["rival_faith_points"], e, w.year)
        self.log.append((w.year, "faith", k, c, s, e, tuple(rivals)))

    def sanctum_place(self, k):
        w, m = self.w, self.w.monsters
        seat = m.site(self.cults[k - 1]["seat"])
        H = m.geo.hops(seat["region"])
        for r in sorted(H, key=lambda r: (H[r], r)):
            if H[r] > self.t["sanctum_hops"]:
                continue
            cell = _p506().height_cell(m, r, [], 0)
            if cell is not None:
                return r, cell
        return None

    def end_temples(self, k, e, ending, only_civ=None):
        w = self.w
        for s in sorted(self.sites(), key=lambda s: s["id"]):
            if s["kind"] != TEMPLE or s["ending"] != 0 or s.get("cult") != k:
                continue
            if only_civ is not None and w.stl(s["settlement"])["owner_civ"] != only_civ:
                continue
            w.monsters.end_site(s["id"], e, ending, year=w.events[e - 1]["year"])

    def return_barred(self, k, a):
        """P5-11 12.2: a cult_suppressed of k with civilization a in the years Y - cult_return_years < y <= Y (Y the
        step's year; a quiet step's start)."""
        if not _pcr().FLAGS.get("p511", False):
            return False
        Y, R = self.w.year, self.t.get("cult_return_years", 0)
        return any(Y - R < y <= Y for y in self.suppressed_years.get((k, a), ()))

    def suppress(self, k, c, g, cult):
        w, t = self.w, self.t
        self.suppressed_years.setdefault((k, c), []).append(w.year)
        others = [key for key in self.cult_in_keys(k) if key[3] != c]
        hidden, place = False, None
        if not others:
            if pp.chance_pm(self.D("hist.pantheon.hide", k, 0, 0), t["hide_permille"]):
                place = self.sanctum_place(k)
                hidden = place is not None
        s2 = self.next_site_id() if hidden else 0
        name = self.name_place(c, "sanctum_forms", s2, self.gods[g]["name"]) if hidden else None
        cause = self.last_kc.get((k, c)) or w.relations[(FACTION, k, CIV, c, "cult_in")]
        e = w.append("cult_suppressed", region=self.capital_region(c),
                     participants=(("faction", FACTION, k), ("god", GOD, g), ("civilization", CIV, c)) +
                     ((("sanctum", SITE, s2),) if hidden else ()), values=(1 if hidden else 0,), causes=(cause,))
        self.add(g, c, -t["suppress_points"], e, w.year)
        w.close_rel((FACTION, k, CIV, c, "cult_in"), e)
        self.end_temples(k, e, END_SUPPRESSED, only_civ=c)
        if not others:
            if hidden:
                r, cell = place
                self.open_site(dict(id=s2, name=name, kind=SANCTUM, region=r, x=cell[0], y=cell[1], settlement=0,
                                    founder=c, monster=0, origin_year=w.year, origin_event=e, ended_year=None,
                                    ending_event=0, ending=0, hostile=None, cult=k))
                cult["status"], cult["sanctum"] = HIDDEN, s2
            else:
                cult["status"], cult["ending"], cult["ended"], cult["ending_event"] = ENDED, ENDING_SUPPRESSED, \
                    w.year, e
                self.end_temples(k, e, END_ABANDONED)
                self.live_cult[g] = 0
        self.log.append((w.year, "suppressed", k, c, hidden, s2, e))

    # --- 6.10 endings and temples
    def endings(self):
        w = self.w
        for c, e in self.realm_ends:
            keys = sorted((key for key in w.relations if key[4] in ("cult_in", "faith_of") and key[2] == CIV and
                           key[3] == c), key=_pcr().key_order)
            touched = sorted({key[1] for key in keys if key[4] == "cult_in"})
            for key in keys:
                w.close_rel(key, e)
            for k in touched:
                cult = self.cults[k - 1]
                if cult["status"] == PRESENT and not self.cult_in_keys(k):
                    cult["status"], cult["ending"] = ENDED, ENDING_FELL
                    cult["ended"], cult["ending_event"] = w.events[e - 1]["year"], e
                    self.end_temples(k, e, END_ABANDONED)
                    self.live_cult[cult["god"]] = 0
                    self.log.append((w.year, "cult fell", k, e))
        for s in sorted(self.sites(), key=lambda s: s["id"]):
            if s["kind"] != TEMPLE or s["ending"] != 0:
                continue
            st = w.stl(s["settlement"])
            k = s["cult"]
            if not w.standing(st):
                ev = st["ruin_event"]
            elif st["owner_civ"] != s["founder"] and not self.open_rel("cult_in", k, st["owner_civ"]):
                ev = st["owner_event"]
            else:
                continue
            if not ev or ev < s["origin_event"]:
                continue   # reading P-R13: it waits
            w.monsters.end_site(s["id"], ev, END_LOST, year=w.events[ev - 1]["year"])

    # --- rows (9)
    def faction_rows(self):
        return [(k["id"], k["name"], 1, k["civ"], k["god"], k["founder"], k["seat"], k["founded"], k["event"],
                 k["ended"], k["ending_event"], k["ending"], k["status"]) for k in self.cults]


def _seq(w, key, role):
    return [q["seq"] for q in w.cat_by_key[key]["roles"] if q["role"] == role][0]


def _one(w, ev, role):
    seq = _seq(w, ev["key"], role)
    xs = [x for (_i, s, _t, x) in ev["participants"] if s == seq]
    return xs[0] if xs else 0


# ---------------------------------------------------------------------------
# Rows: the invariants of 13, the statistics of 10 item 8, the v11 blocks of 10 and the timeline reads of 11.1
# ---------------------------------------------------------------------------

class X:
    """Indexes over a world's rows (a replay's res or a world file's tables)."""

    def __init__(self, res):
        p502 = _p502()
        self.res = res
        self.r = p502.Rows(res, [(b[0], b[1]) for b in res.get("region_border", [])] or None)
        r = self.r
        self.year = {e: r.ev[e][1] for e in r.ev}
        self.factions = {f[0]: f for f in res.get("faction", [])}
        self.figures = {f[0]: f for f in res.get("figure", [])}
        self.sites = {s[0]: s for s in res.get("site", [])}
        self.monsters = {m[0]: m for m in res.get("monster", [])}
        self.gods = {g[0]: g for g in res.get("god", [])}
        self.rel = list(res.get("relation", []))

    def of(self, key):
        return [e[0] for e in self.r.events_of(key)]

    def kind(self, e):
        return self.r.kind_of(e) if e in self.r.ev else ""

    def one(self, e, role):
        xs = self.r.role(e, role)
        return xs[0] if xs else 0

    def val(self, e, key):
        return self.r.value(e, key)

    def causes(self, e):
        return sorted(self.r.causes.get(e, []))

    def civs_of(self, e):
        return [p[3] for p in self.r.parts.get(e, []) if p[2] == CIV]

    def quiet(self, e):
        return self.r.phase.get(self.r.ev[e][4], (0, 0, 0))[2] == pe.QUIET

    def rel_rows(self, kind):
        return [q for q in self.rel if q[4] == kind]

    def in_force(self, q, e):
        """A relation row in force at event e: opened at or before e and not closed before it."""
        return q[7] <= e and (q[8] == 0 or q[8] > e)


def act_sign(x, e):
    k = x.kind(e)
    if k in ("divine_bless", "divine_champion"):
        return 1
    if k in ("divine_curse", "divine_monster"):
        return -1
    if k == "divine_mark":
        return -1 if x.val(e, "profane") == 1 else 1
    return 0


def violations(res, gods_data, tuning, creator_title=None, region_names=(), age_names=(), races=None):
    """13 items 1 to 9, 12 and 13 on rows (10 and 11 are the replay's; 14 the earlier references')."""
    x = X(res)
    r = x.r
    out = []
    t = tuning
    gods = sorted(x.gods.values())
    races = races or {}
    n = len(races) if races else sum(1 for g in gods if len(g) > 5 and g[5] != 0)
    # 1: the god rows
    for g in gods:
        if g[0] <= n:
            rc = races.get(g[0])
            if g[5] != g[0] or g[3] != 1 or (rc is not None and g[2] != rc[5]):
                out.append("1: creator god %d row %s" % (g[0], g))
            if creator_title is not None and rc is not None and g[4] != creator_title.replace("{plural}", rc[3]):
                out.append("1: creator god %d title %r" % (g[0], g[4]))
        else:
            i = g[0] - n - 1
            if i >= len(gods_data):
                out.append("1: god %d beyond the definitions" % g[0])
                continue
            _did, d = gods_data[i]
            if g[5] != 0 or g[2] != CAMPS[d["camp"]] or g[3] != DOMAINS[d["domain"]] or g[4] != d["title"]:
                out.append("1: pantheon god %d row %s against %s" % (g[0], g, _did))
    names = [g[1] for g in gods]
    if len(set(names)) != len(names) or set(names) & (set(region_names) | set(age_names)):
        out.append("1: god names not distinct: %s" % names)
    ngods = len(gods)
    if [g[0] for g in gods] != list(range(1, ngods + 1)):
        out.append("1: god ids not dense")
    camp_of_god = {g[0]: g[2] for g in gods}
    race_of_god = {g[0]: (g[5] if len(g) > 5 else 0) for g in gods}
    civ_race = {c: r.civ[c][2] for c in r.civ}
    race_camp = {k: v[5] for k, v in races.items()} if races else {}

    def kin(g, c):
        return kin_of(race_of_god[g], camp_of_god[g], civ_race.get(c, 0), race_camp.get(civ_race.get(c, 0), -1))

    def foe(g, c):
        return foe_of(camp_of_god[g], race_camp.get(civ_race.get(c, 0), -1))

    def deeds_of(g):
        if g <= n:
            return t["creator"]["deeds"]
        i = g - n - 1
        return gods_data[i][1]["deeds"] if i < len(gods_data) else []

    faith_rows = x.rel_rows("faith_of")
    # 2, 3, 4: the acts
    acts = sorted(e for k in ACT_KEYS for e in x.of(k))
    years = {}
    last_g, last_gc = {}, {}
    shrine_settlements = {}
    for e in acts:
        y = x.year[e]
        g, c = x.one(e, "actor"), x.one(e, "civilization")
        if x.quiet(e):
            out.append("2: act e%d in a quiet step" % e)
        if x.val(e, "divine_script") != 0:
            out.append("2: act e%d divine_script %s" % (e, x.val(e, "divine_script")))
        cs = x.causes(e)
        sign = act_sign(x, e)
        if not 1 <= len(cs) <= 3:
            out.append("2: act e%d has %d causes" % (e, len(cs)))
        for cc in cs:
            k = x.kind(cc)
            ok = False
            if k in ("cult_adopted", "cult_suppressed") and x.one(cc, "civilization") == c:
                # 6.2: the subsystem's own events add regard through 6.9 (faith +, rival gods -, suppression -)
                fg = x.one(cc, "god")
                ok = (k == "cult_suppressed" and fg == g and sign < 0) or (k == "cult_adopted" and (
                    (fg == g and sign > 0) or (fg != g and sign < 0)))
            for d in deeds_of(g):
                if d["kind"] != k or d["points"] * sign <= 0:
                    continue
                if c in r.role(cc, d["role"]) and who_ok(d["who"], kin(g, c), foe(g, c)):
                    ok = True
            if not ok:
                out.append("2: act e%d cause e%d (%s) is not a deed of god %d for c%d of its sign" % (e, cc, k, g, c))
            if y - x.year.get(cc, y) > t["regard_memory_years"]:
                out.append("2: act e%d cause e%d older than regard_memory_years" % (e, cc))
        years[y] = years.get(y, 0) + 1
        if years[y] > 1:
            out.append("2: two acts in year %d" % y)
        if g in last_g and y - last_g[g] < t["god_rest_years"]:
            out.append("2: god %d acts at %d and %d" % (g, last_g[g], y))
        if (g, c) in last_gc and y - last_gc[(g, c)] < t["pair_rest_years"]:
            out.append("2: pair (%d, %d) acts at %d and %d" % (g, c, last_gc[(g, c)], y))
        last_g[g], last_gc[(g, c)] = y, y
        if sign > 0 and foe(g, c):
            out.append("3: favourable act e%d on a foe c%d of god %d" % (e, c, g))
        if sign < 0:
            for q in faith_rows:
                if q[3] == c and x.in_force(q, e) and x.factions.get(q[1], (0,) * 13)[4] == g:
                    out.append("3: wrathful act e%d on c%d whose faith is the cult of god %d" % (e, c, g))
        k = x.kind(e)
        if k == "divine_curse" and x.val(e, "dead") < 0:
            out.append("4: curse e%d dead %d" % (e, x.val(e, "dead")))
        if k == "divine_monster":
            mid, lid = x.one(e, "monster"), x.one(e, "lair")
            mrow = x.monsters.get(mid)
            if mrow is None or mrow[5] != e:
                out.append("4: divine_monster e%d monster %d not born of it" % (e, mid))
            elif mrow[2] in x.r.res.get("monster_kind_keys", {}) and False:
                pass
            srow = x.sites.get(lid)
            if srow is None or srow[2] != LAIR or srow[10] != e:
                out.append("4: divine_monster e%d lair %d" % (e, lid))
            elif x.r.nb:
                cap0 = r.civ[c][4]
                h = r.hops(cap0).get(srow[3])
                if h is None:
                    out.append("4: divine_monster e%d lair region %d unreachable from c%d" % (e, srow[3], c))
        if k == "divine_champion":
            f = x.figures.get(x.one(e, "figure"))
            if f is None or f[3] != CHAMPION or f[6] != c or f[9] != e:
                out.append("4: divine_champion e%d figure %s" % (e, x.one(e, "figure")))
            # 4 (lead ruling PN5): work for a champion in the act's year, as a necessary condition on rows: a war row
            # of c (a principal's or a joined ally's) open in year Y, or a raid on c by a monster not ended before Y
            # (within hunt_memory_years when the tuning carries `_hunt_memory_years`)
            Y = x.year[e]
            war = any(q[4] == "war" and c in (q[1], q[3]) and q[5] <= Y and (q[6] is None or q[6] >= Y)
                      for q in x.rel)
            mem = t.get("_hunt_memory_years")
            raid = any(x.one(m, "civilization") == c and x.year[m] <= Y and (mem is None or Y - x.year[m] <= mem) and
                       (x.monsters.get(x.one(m, "monster"), (0,) * 7)[6] is None or
                        x.monsters[x.one(m, "monster")][6] >= Y) for m in x.of("monster_raid"))
            if not (war or raid):
                out.append("4: divine_champion e%d: c%d had no open war and no raid by a living monster" % (e, c))
        if k == "divine_mark":
            s = x.sites.get(x.one(e, "site"))
            if s is None or s[2] != SHRINE or s[10] != e:
                out.append("4: divine_mark e%d site %s" % (e, x.one(e, "site")))
            elif s[6]:
                shrine_settlements.setdefault(s[6], []).append(s[0])
    for u, ss in sorted(shrine_settlements.items()):
        if len(ss) > 1:
            out.append("4: settlement %d has shrines %s" % (u, ss))
    # 5: cults
    live = {}
    for k, f in sorted(x.factions.items()):
        ev = f[8]
        if x.kind(ev) != "cult_founded" or x.one(ev, "faction") != k:
            out.append("5: faction %d founding_event e%d" % (k, ev))
            continue
        a = x.causes(ev)
        if len(a) != 1 or x.kind(a[0]) not in ACT_KEYS or x.one(a[0], "actor") != f[4] or \
                x.one(a[0], "civilization") != f[3] or x.year[ev] < x.year[a[0]] + 1:
            out.append("5: faction %d founding e%d does not cite an earlier act of its god on c%d" % (k, ev, f[3]))
        fig = x.figures.get(f[5])
        if fig is None:
            out.append("5: faction %d founder %d is no figure" % (k, f[5]))
        else:
            born, died = fig[7], fig[10]
            if (fig[3] == PROPHET and fig[9] != ev) or born > x.year[ev] or (died is not None and died < x.year[ev]):
                out.append("5: faction %d founder f%d not alive at the founding" % (k, f[5]))
        seat = x.sites.get(f[6])
        if seat is None or seat[2] != TEMPLE or seat[10] != ev or seat[7] != f[3]:
            out.append("5: faction %d seat %d" % (k, f[6]))
        end = f[9] if f[12] == ENDED else None
        for (a0, a1, k0) in live.get(f[4], []):
            if a1 is None or a1 > x.year[ev] or (a1 == x.year[ev] and False):
                out.append("5: god %d has two live cults %d and %d" % (f[4], k0, k))
        live.setdefault(f[4], []).append((x.year[ev], end, k))
    # 6: cult_in and spread
    cult_in = x.rel_rows("cult_in")
    for q in cult_in:
        if x.kind(q[7]) not in ("cult_founded", "cult_spread"):
            out.append("6: cult_in %s opened by e%d (%s)" % (q[:5], q[7], x.kind(q[7])))
    allied = [q for q in x.rel if q[4] in ("alliance", "trade")]
    steps = {}
    for e in x.of("cult_spread"):
        k, a, frm = x.one(e, "faction"), x.one(e, "civilization"), x.one(e, "from")
        f = x.factions.get(k)
        if f is None or f[8] not in x.causes(e):
            out.append("6: cult_spread e%d does not cite its cult's founding" % e)
        if not any(q[1] == k and q[3] == frm and x.in_force(q, e) for q in cult_in):
            out.append("6: cult_spread e%d from c%d without cult_in" % (e, frm))
        if not any({q[1], q[3]} == {a, frm} and x.in_force(q, e) for q in allied):
            out.append("6: cult_spread e%d: c%d and c%d have no alliance or trade" % (e, a, frm))
        st = (k, r.ev[e][4], x.year[e])
        steps[st] = steps.get(st, 0) + 1
        if steps[st] > 1:
            out.append("6: two spreads of cult %d in one step (year %d)" % (k, x.year[e]))
        if f is not None and f[8] < e and x.kind(f[8]) != "cult_founded":
            out.append("9: cult_spread e%d names a faction not founded before it" % e)
    # 7: faith
    per_civ = {}
    for q in faith_rows:
        per_civ.setdefault(q[3], []).append(q)
        if not any(c[1] == q[1] and c[3] == q[3] and c[7] <= q[7] and (c[8] == 0 or (q[8] != 0 and c[8] >= q[8]))
                   for c in cult_in):
            out.append("7: faith_of %s without its cult_in" % (q[:5],))
    for c, qs in sorted(per_civ.items()):
        qs = sorted(qs, key=lambda q: q[7])
        for a_, b_ in zip(qs, qs[1:]):
            if a_[8] == 0 or a_[8] > b_[7]:
                out.append("7: c%d has two faiths in force" % c)
    for e in x.of("cult_suppressed"):
        k, c, g = x.one(e, "faction"), x.one(e, "civilization"), x.one(e, "god")
        own = [q for q in faith_rows if q[3] == c and x.in_force(q, e)]
        if any(q[1] == k for q in own):
            out.append("7: cult_suppressed e%d of c%d's own faith" % (e, c))
        fgod = x.factions.get(own[0][1], (0,) * 13)[4] if own else 0
        if not (foe(g, c) or (fgod and opposed(camp_of_god.get(fgod, 1), camp_of_god.get(g, 1)))):
            out.append("7: cult_suppressed e%d: c%d is not hostile to god %d" % (e, c, g))
    # 8, 9: endings and temples at E
    for k, f in sorted(x.factions.items()):
        if f[12] == HIDDEN:
            sanct = [s for s in x.sites.values() if s[2] == SANCTUM and x.kind(s[10]) == "cult_suppressed" and
                     x.one(s[10], "faction") == k]
            if len(sanct) != 1 or any(q[1] == k and q[8] == 0 for q in cult_in):
                out.append("8: hidden faction %d sanctums %s" % (k, [s[0] for s in sanct]))
        if f[12] == ENDED:
            if f[11] not in (ENDING_SUPPRESSED, ENDING_FELL):
                out.append("8: ended faction %d ending %d" % (k, f[11]))
            for s in x.sites.values():
                if s[2] == TEMPLE and _temple_cult(x, s) == k and (s[13] == 0 or s[11] > f[9]):
                    out.append("8: temple %d of ended faction %d open after %s" % (s[0], k, f[9]))
        if (f[12] < ENDED) != (f[11] == 0 and f[9] is None):
            out.append("9: faction %d status %d ending %d ended %s" % (k, f[12], f[11], f[9]))
    for s in x.sites.values():
        if s[2] != TEMPLE or s[13] != 0:
            continue
        st = r.stl.get(s[6])
        k = _temple_cult(x, s)
        if st is None or st[10] not in (1, 2):
            out.append("9: open temple %d on a settlement not standing" % s[0])
            continue
        o = st[8]
        if o != s[7] and not any(q[1] == k and q[3] == o and q[8] == 0 for q in cult_in):
            out.append("9: open temple %d at s%d of c%d without its cult" % (s[0], s[6], o))
    # 12 (12.4): aid after the outcome
    for e in x.of("ally_aided_hold"):
        cs = x.causes(e)
        if len(cs) != 2 or x.kind(cs[0]) != "deep_woken" or x.kind(cs[1]) not in ("deep_sealed", "warren_sealed",
                                                                                     "hold_lost") or cs[1] > e:
            out.append("12: ally_aided_hold e%d causes %s" % (e, cs))
    for e in x.of("deep_woken"):
        m = x.one(e, "monster")
        nxt = [y for y in sorted(r.ev) if y > e and x.kind(y) in ("deep_sealed", "warren_sealed", "hold_lost") and
               x.one(y, "monster") == m]
        if nxt and x.year[nxt[0]] == x.year[e] and x.causes(nxt[0]) != [e]:
            out.append("12: outcome e%d of waking e%d cites %s" % (nxt[0], e, x.causes(nxt[0])))
        if x.quiet(e):
            out.append("11: waking e%d in a quiet step" % e)
    # 13 (12.6): towers
    lapse = t.get("_tower_lapse_years", 0)
    for e in x.of("tower_abandoned"):
        s = x.sites.get(x.one(e, "site"))
        f = x.figures.get(x.one(e, "figure"))
        if s is None or s[2] != TOWER or s[13] != END_ABANDONED or s[12] != e:
            out.append("13: tower_abandoned e%d site %s" % (e, x.one(e, "site")))
        if f is not None and f[10] is not None and lapse and x.year[e] < f[10] + lapse:
            out.append("13: tower_abandoned e%d %d years after its master's death" % (e, x.year[e] - f[10]))
    after = t.get("_rule_from_event", 0)
    open4 = {}
    for s in x.sites.values():
        if s[2] == TOWER and s[10] > after and s[6] == 0:
            open4.setdefault((s[7], s[3]), []).append(s)
    for (c, reg), ss in sorted(open4.items()):
        for a_ in ss:
            for b_ in ss:
                if a_[0] < b_[0] and (a_[13] == 0 or a_[12] > b_[10]):
                    out.append("13: c%d has towers %d and %d open in region %d" % (c, a_[0], b_[0], reg))
    return out


def _temple_cult(x, s):
    """The cult of a temple: the faction its origin event names."""
    return x.one(s[10], "faction")


def stats_of_tables(res, renewals_capped=0, deep_stirred=0):
    """10 item 8 (renewals_capped and deep_stirred are not in rows: the replay passes them)."""
    x = X(res)
    acts = {k: x.of(k) for k in ACT_KEYS}
    every = sorted(e for v in acts.values() for e in v)
    camps = [0, 0, 0]
    creators = 0
    n = sum(1 for g in x.gods.values() if len(g) > 5 and g[5] != 0)
    for e in every:
        g = x.one(e, "actor")
        if g in x.gods:
            camps[x.gods[g][2]] += 1
        if g <= n:
            creators += 1
    founders = [f[5] for f in x.factions.values()]
    shrines = [s for s in x.sites.values() if s[2] == SHRINE]
    st = [("divine_acts", len(every))] + [("divine_" + k, len(acts["divine_" + k])) for k in ACT_KINDS]
    st += [("divine_good", camps[0]), ("divine_neutral", camps[1]), ("divine_evil", camps[2]),
           ("divine_creators", creators), ("divine_flares", flares_of(x)),
           ("divine_dead", sum(x.val(e, "dead") for e in acts["divine_curse"])),
           ("cults_founded", len(x.factions)), ("cults_spread", len(x.of("cult_spread"))),
           ("cults_adopted", len(x.of("cult_adopted"))), ("cults_suppressed", len(x.of("cult_suppressed"))),
           ("cults_hidden", sum(1 for f in x.factions.values() if f[12] == HIDDEN)),
           ("cults_present", sum(1 for f in x.factions.values() if f[12] == PRESENT)),
           ("cult_founders_prophets", sum(1 for f in founders if x.figures.get(f, (0,) * 4)[3] == PROPHET)),
           ("shrines_sacred", sum(1 for s in shrines if x.val(s[10], "profane") == 0)),
           ("shrines_profane", sum(1 for s in shrines if x.val(s[10], "profane") == 1)),
           ("temples", sum(1 for s in x.sites.values() if s[2] == TEMPLE)),
           ("towers_taken_up", len(x.of("tower_taken_up"))), ("towers_abandoned", len(x.of("tower_abandoned"))),
           ("renewals_capped", renewals_capped), ("deep_stirred", deep_stirred)]
    assert tuple(k for k, _ in st) == STATS_KEYS
    return st


def flares_of(x):
    """10 item 2 (reading P-R14)."""
    acts = sorted(e for k in ACT_KEYS for e in x.of(k))
    n = 0
    for p in x.r.res["phase"]:
        if p[2] != pe.FLARE or not p[5]:
            continue
        if any(x.year[e] == p[3] and p[5] in x.causes(e) for e in acts):
            n += 1
    return n


# --- 10: the v11 blocks (text over rows; names from the rows)
def pantheon_block(res, level="full"):
    x = X(res)
    acts = {k: x.of("divine_" + k) for k in ACT_KINDS}
    every = sorted(e for v in acts.values() for e in v)
    lines = ["gods: %d; acts: %d (%s); flares: %d" % (len(x.gods), len(every), ", ".join(
        "%s %d" % (k, len(acts[k])) for k in ACT_KINDS), flares_of(x))]
    live = {}
    for k, f in sorted(x.factions.items()):
        if f[12] != ENDED:
            live[f[4]] = f
    per = []
    for gid, g in sorted(x.gods.items()):
        n = [e for e in every if x.one(e, "actor") == gid]
        kinds = [(k, sum(1 for e in acts[k] if x.one(e, "actor") == gid)) for k in ACT_KINDS]
        s = "  g%d %s %s (%s, %s): acts %d" % (gid, g[1], g[4], CAMP_WORDS[g[2]], DOMAIN_WORDS[g[3]], len(n))
        if n:
            s += " (" + ", ".join("%s %d" % kv for kv in kinds if kv[1]) + ")"
        cults = [f for f in x.factions.values() if f[4] == gid]
        cult = live.get(gid) or (cults[-1] if cults else None)
        if cult is not None:
            s += "; cult fa%d %s" % (cult[0], cult[1])
        per.append((len(n), gid, s, bool(n) or cult is not None))
    if level == "full":
        lines += [p[2] for p in per]
    else:
        shown = sorted((p for p in per if p[3]), key=lambda p: (-p[0], p[1]))
        lines += [p[2] for p in shown[:SUMMARY_GODS_MAX]]
        if len(shown) > SUMMARY_GODS_MAX:
            lines.append("  and %d more" % (len(shown) - SUMMARY_GODS_MAX))
    return lines


def cults_block(res, level="full"):
    x = X(res)
    r = x.r
    fs = sorted(x.factions.values())
    lines = ["cults: %d (present %d, hidden %d, ended %d)" % (len(fs), sum(1 for f in fs if f[12] == PRESENT),
                                                           sum(1 for f in fs if f[12] == HIDDEN),
                                                           sum(1 for f in fs if f[12] == ENDED))]
    cult_in = x.rel_rows("cult_in")
    faith = x.rel_rows("faith_of")
    out = []
    for f in fs:
        k = f[0]
        g = x.gods.get(f[4], (0, "?"))
        fig = x.figures.get(f[5])
        fig = (f[5], pe.figure_display_name(fig[1], fig[16], fig[2]) if fig is not None else "?")   # P5-06 10.1
        seat = x.sites.get(f[6], (0, "?", 0, 0, 0, 0, 0))
        t = r.stl.get(seat[6], (0, "?"))
        end_e = f[10] if f[12] == ENDED else (0 if f[12] == PRESENT else 0)
        ins = [q for q in cult_in if q[1] == k and (q[8] == 0 or (end_e and q[8] == end_e))]
        s = "  fa%d %s, of g%d %s: founded %d e%d by f%d %s at si%d %s in s%d %s" % (
            k, f[1], f[4], g[1], f[7], f[8], f[5], fig[1], f[6], seat[1], seat[6], t[1])
        if ins:   # reading P-R27 (the engine's; 10 item 3 gives no order): by (since, civilization id)
            s += "; in " + ", ".join("c%d %s" % (q[3], r.civ[q[3]][1]) for q in sorted(ins, key=lambda q: (q[5], q[3])))
        else:   # P-R27: `in none` when no row qualifies (the faith part is left out when empty)
            s += "; in none"
        # reading P-R27 (the engine's; 10 item 3 is silent): the faith_of rows open at E or closed by the cult's
        # ending event, as for "in"
        fa = [q for q in faith if q[1] == k and (q[8] == 0 or (end_e and q[8] == end_e))]
        if fa:
            s += "; faith of " + ", ".join("c%d %s since %d" % (q[3], r.civ[q[3]][1], q[5])
                                           for q in sorted(fa, key=lambda q: (q[5], q[3])))
        if f[12] == PRESENT:
            s += "; present"
        elif f[12] == HIDDEN:
            sx = [z for z in x.sites.values() if z[2] == SANCTUM and _temple_cult(x, z) == k]
            z = sx[0] if sx else (0, "?", 0, 0, 0, 0, 0, 0, 0, 0, 0)
            s += "; hidden at si%d %s since %d e%d" % (z[0], z[1], z[9], z[10])
        else:
            s += "; ended %d e%d %s" % (f[9], f[10], "suppressed" if f[11] == ENDING_SUPPRESSED else "its realms fell")
        out.append(((f[12], k), s))
    if level == "full":
        lines += [s for _k, s in out]
    else:
        shown = sorted(out)
        lines += [s for _k, s in shown[:SUMMARY_CULTS_MAX]]
        if len(shown) > SUMMARY_CULTS_MAX:
            lines.append("  and %d more" % (len(shown) - SUMMARY_CULTS_MAX))
    return lines


def _fig_name(x, f):
    """P5-06 10.1: a figure's display name (with its regnal number and epithet)."""
    r = x.figures.get(f)
    return pe.figure_display_name(r[1], r[16], r[2]) if r is not None else "?"


def _parts_limited(items, cap=4):
    if len(items) <= cap:
        return ", ".join(items)
    return ", ".join(items[:cap]) + " and %d more" % (len(items) - cap)


def realm_lines(res, c):
    """10 item 4: the realm block lines `gods:` and `faith:` of civilization c."""
    x = X(res)
    gname = {g[0]: g[1] for g in x.gods.values()}
    parts = []
    acts = [e for k in ACT_KEYS for e in x.of(k) if x.one(e, "civilization") == c]
    acts.sort()
    groups = (("blessed by", "divine_bless", lambda e: "g%d %s e%d" % (x.one(e, "actor"), gname[x.one(e, "actor")], e)),
              ("cursed by", "divine_curse", lambda e: "g%d %s e%d" % (x.one(e, "actor"), gname[x.one(e, "actor")], e)),
              ("champion", "divine_champion", lambda e: "f%d %s e%d" % (
                  x.one(e, "figure"), _fig_name(x, x.one(e, "figure")), e)),
              ("sent", "divine_monster", lambda e: "m%d %s e%d" % (
                  x.one(e, "monster"), x.monsters.get(x.one(e, "monster"), (0, "?"))[1], e)),
              ("marked", "divine_mark", lambda e: "si%d %s e%d" % (
                  x.one(e, "site"), x.sites.get(x.one(e, "site"), (0, "?"))[1], e)))
    for word, key, fmt in groups:
        items = [fmt(e) for e in acts if x.kind(e) == key]
        if items:
            parts.append("%s %s" % (word, _parts_limited(items)))
    lines = []
    if parts:
        lines.append("  gods: " + "; ".join(parts))
    fa = sorted((q for q in x.rel_rows("faith_of") if q[3] == c), key=lambda q: q[5])
    for q in fa[:1]:
        lines.append("  faith: fa%d %s since %d e%d" % (q[1], x.factions[q[1]][1], q[5], q[7]))
    return lines


def faith_at(res, civ, year):
    """11.1 Timeline::faith_at: the faction of the faith_of row in force at the end of year (0 none)."""
    for q in res.get("relation", []):
        if q[4] == "faith_of" and q[3] == civ and q[5] <= year and (q[6] is None or q[6] == -1 or q[6] > year):
            return q[1]
    return 0


def cults_in_at(res, civ, year):
    return sorted(q[1] for q in res.get("relation", []) if q[4] == "cult_in" and q[3] == civ and q[5] <= year and
                  (q[6] is None or q[6] == -1 or q[6] > year))


def cult_status_at(res, faction, year):
    x = X(res)
    f = x.factions.get(faction)
    if f is None or year < f[7]:
        return 0
    if f[12] == ENDED and year >= f[9]:
        return ENDED
    if f[12] == HIDDEN:
        sx = [z for z in x.sites.values() if z[2] == SANCTUM and _temple_cult(x, z) == faction]
        if sx and year >= sx[0][9]:
            return HIDDEN
    return PRESENT


def divine_effect_at(res, civ, year):
    """11.1: bit 1 plenty, 2 withering, 4 valour, 8 defeat, from kinds 100 and 101 with their years."""
    x = X(res)
    bits = 0
    for e in x.of("divine_bless") + x.of("divine_curse"):
        if x.one(e, "civilization") != civ:
            continue
        y0, n = x.year[e], x.val(e, "years")
        if not (y0 <= year < y0 + n):
            continue
        eff = x.val(e, "effect")
        if x.kind(e) == "divine_bless":
            bits |= 1 if eff == 1 else 4
        else:
            bits |= 2 if eff == 2 else 8 if eff == 3 else 0
    return bits


# ---------------------------------------------------------------------------
# Fixtures: pantheon-small (4.1) and the fixture files of worldgen/tests/data/pantheon/
# ---------------------------------------------------------------------------

PANTHEON_SMALL = dict(act_threshold=30, act_permille=1000, god_rest_years=5, pair_rest_years=10,
                      cult_found_permille=1000, cult_spread_permille=300, adopt_permille=300, suppress_permille=200,
                      hide_permille=500, flare_permille=1000)


def with_pantheon(dat, edits=None, gods=None, activity=None):
    d = _pcr().edited(dat)
    d.base = dat.base
    d.pantheon = dict(copy.deepcopy(dat.pantheon or pdr.pantheon_shipped_030()), **copy.deepcopy(edits or {}))
    if gods is not None:
        d.gods = gods
    return d


def pantheon_small_data(dat=None):
    """4.1 pantheon-small: monsters-small (P5-02 19.3, on the conflict stress data) with the pantheon edits."""
    base = _p502().monsters_small_data(dat)
    d = with_pantheon(base, PANTHEON_SMALL)
    return d


def zero_data(dat=None):
    """14 I0: the shipped values with act_permille 0 and flare_permille 0."""
    return with_pantheon(dat or _pcr().data(), dict(act_permille=0, flare_permille=0))


def fixture_files():
    return sorted(f for f in os.listdir(FIXTURE_DIR) if f.endswith(".toml")) if os.path.isdir(FIXTURE_DIR) else []


def fixture_data(name):
    """A fixture file: base pantheon-small with [pantheon] edits, [god.<id>] edits, [alliances], [figures],
    [monsters] edits and its cases (seed, (span, races, civs, intensity), activity)."""
    with open(os.path.join(FIXTURE_DIR, name), "rb") as f:
        doc = tomllib.load(f)
    assert doc["base"] == "pantheon-small", name
    base = pantheon_small_data()
    gods = [] if doc.get("no_gods") else copy.deepcopy(base.gods)
    for did, ed in doc.get("god", {}).items():
        for i, (k, v) in enumerate(gods):
            if k == did:
                gods[i] = (k, dict(v, **ed))
    d = with_pantheon(base, dict(base.pantheon, **doc.get("pantheon", {})), gods)
    for sub in ("alliances", "figures", "conflict"):
        if sub in doc:
            setattr(d, sub, dict(copy.deepcopy(getattr(d, sub)), **doc[sub]))
    if "monsters" in doc:
        d.base = copy.copy(d.base)
        d.base.monsters_tuning = dict(copy.deepcopy(d.base.monsters_tuning), **doc["monsters"])
    cases = [(c["seed"], (c["span"], c["races"], c["civs"], c["intensity"]), c.get("activity", "normal"))
             for c in doc["case"]]
    return d, cases, doc.get("paths", []), doc


def run_case(seed, dat, case=None, activity="normal"):
    pcr = _pcr()
    case = case or pcr.STRESS_CASES.get(seed, pcr.synthetic_case(seed))
    inp, _g = pcr.synthetic_inputs(seed, dat, case)
    if not getattr(dat, "gods", None):   # 4.1: a root with no god definitions: the pantheon is the creators alone
        inp.gods = [g for g in inp.gods if g[0] <= len(inp.races)]
    inp.params["activity"] = activity
    return pcr.history_stage(dat, inp), inp


def paths_of(res):
    """The paths of 6 a run shows (from rows and the replay's logs)."""
    x = X(res)
    w = res["_world"]
    seen = set()
    for k in ACT_KINDS:
        if x.of("divine_" + k):
            seen.add(k)
    for e in x.of("divine_curse"):
        seen.add(("pestilence", "withering", "defeat")[x.val(e, "effect") - 1])
    for e in x.of("divine_bless"):
        seen.add(("plenty", "valour")[x.val(e, "effect") - 1])
    for e in x.of("divine_mark"):
        seen.add("profane mark" if x.val(e, "profane") else "sacred mark")
        if r_settlement(x, e):
            seen.add("mark at a settlement")
    if x.factions:
        seen.add("founded")
    for f in x.factions.values():
        fig = x.figures.get(f[5])
        if fig is not None:
            seen.add({PROPHET: "prophet founder", CHAMPION: "champion founder", CASTER: "caster founder",
                      RULER: "ruler founder"}.get(fig[3], "other founder"))
        if f[12] == HIDDEN:
            seen.add("hidden")
        if f[12] == ENDED:
            seen.add("ended suppressed" if f[11] == ENDING_SUPPRESSED else "ended fell")
    for key in ("cult_spread", "cult_adopted", "cult_suppressed", "tower_taken_up", "tower_abandoned"):
        if x.of(key):
            seen.add(key)
    p = w.pantheon
    if any(lg[1] == "flare" for lg in p.log):
        seen.add("flare")
    if any(lg[1] == "act" and lg[6] for lg in p.log):
        seen.add("certain act")
    for s in x.sites.values():
        if s[2] == TEMPLE and s[13] == END_LOST:
            seen.add("temple lost")
        if s[2] == TEMPLE and s[13] == END_SUPPRESSED:
            seen.add("temple suppressed")
    if w.monsters.deep is not None and any(lg[1] in ("stir", "stir warren") for lg in w.monsters.deep.log):
        seen.add("stir")
    if w.alliances is not None and w.alliances.renewals_capped:
        seen.add("renewal capped")
    for f in w.figures.figs if w.figures is not None else ():
        if f["role"] == CHAMPION and (f["battles_won"] or f["battles_lost"] or f["slain"]):
            seen.add("champion fought")
    for e in x.of("cult_adopted"):
        if not x.one(e, "site"):
            seen.add("faith without temple")
    return seen


def r_settlement(x, e):
    return x.r.ev[e][6]


def run_problems(res, inp, dat):
    """13 on the run's rows (with the data), the replay-only checks (4's pestilence, 10's effects), P5-07's and
    P5-02's invariants."""
    w = res["_world"]
    p = w.pantheon
    out = []
    gods = list(dat.gods)
    t = dict(p.t, _tower_lapse_years=(dat.figures or {}).get("tower_lapse_years", 0),
             _hunt_memory_years=w.monsters.t["hunt_memory_years"] if w.monsters is not None else None)
    rows = dict(res)
    rows["region_border"] = [(b["a"], b["b"]) for b in inp.borders]
    races = {r[0]: r for r in inp.races}
    out += violations(rows, gods, t, p.dat.chron.creator_title if hasattr(p.dat.chron, "creator_title") else None,
                      [r["name"] for r in inp.regions], [a[4] for a in inp.ages], races)
    # 4 (replay): a pestilence's dead is the sum of its settlements' losses
    for lg in p.log:
        if lg[1] == "act" and lg[2] == "curse":
            e = lg[5]
            hit = p.act_meta[e].get("hit", [])
            if w.events[e - 1]["values"][2] != sum(d for _u, d in hit):
                out.append("4: curse e%d dead against its settlements" % e)
    # 4 (replay): a sent monster's lair within send_hops of its target's capital region at the act
    for e, meta in sorted(p.act_meta.items()):
        if meta["kind"] == "monster":
            ev = w.events[e - 1]
            if ev["region"] != w.monsters.site(meta["lair"])["region"]:
                out.append("4: divine_monster e%d region" % e)
    import p5_07_depths_reference as p507
    borders = [(b["a"], b["b"]) for b in inp.borders]
    mining = {rid: v["mining"] for rid, v in w.pd.race.items()}
    delving = {rid: v.get("delving", 0) for rid, v in w.pd.race.items()}
    present = {r[0] for r in inp.races if r[9] == 1 and any(c["race"] == r[0] for c in w.civs)}
    out += ["P5-07 " + v for v in p507.violations(res, borders, w.monsters.t, mining, inp.params["span"], present,
                                                  delving)]
    out += ["P5-02 " + v for v in _p502().res_violations(res, w, quarter=False)]
    return out


def table_hash(rows):
    h = hashlib.sha256()
    for r in rows:
        h.update(repr(tuple(r)).encode())
    return h.hexdigest()


I0_TABLES = ("event", "event_participant", "event_cause", "event_value", "civilization", "settlement", "relation",
             "site", "monster", "region_control", "figure")


# ---------------------------------------------------------------------------
# Units (16 items 1 to 9 on hand-checked values)
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
    # 4.3 kin and foe
    c.eq("4.3 creator kin: its race", [kin_of(3, 2, r, 2) for r in (3, 4)], [True, False])
    c.eq("4.3 pantheon kin: its camp", [kin_of(0, 0, 9, k) for k in (0, 1, 2)], [True, False, False])
    c.eq("4.3 foe: good against evil only", [foe_of(a, b) for a, b in ((0, 2), (2, 0), (1, 2), (0, 1), (1, 1))],
         [True, True, False, False, False])
    c.eq("4.3 who", [who_ok(w_, True, False) for w_ in ("kin", "any", "foe")], [True, True, False])
    # 6.2 add: clamp, memory, contributions_max
    R, con = add_contribution(390, [], 30, 5, 1000, 400, 300, 8)
    c.eq("6.2 clamp at regard_max", R, 400)
    R, con = add_contribution(-390, [], -30, 5, 1000, 400, 300, 8)
    c.eq("6.2 clamp at -regard_max", R, -400)
    con = [(i, 1000, 10) for i in range(1, 9)]
    R, con2 = add_contribution(80, con, 10, 9, 1000, 400, 300, 8)
    c.eq("6.2 contributions_max drops the oldest", [x[0] for x in con2], list(range(2, 10)))
    R, con3 = add_contribution(0, [(1, 699, 10), (2, 700, 10)], 10, 3, 1000, 400, 300, 8)
    c.eq("6.2 memory: year < y - 300 dropped, = kept", [x[0] for x in con3], [2, 3])
    # 6.2 the fade is cumulative
    c.eq("6.2 F(t) = 20 t div 100", [fade_F(20, t) for t in (0, 4, 5, 99, 100)], [0, 0, 1, 19, 20])
    one = sum(fade_amount(20, y, 1) for y in range(1000, 1100))
    split = fade_amount(20, 1000, 37) + fade_amount(20, 1037, 63)
    c.eq("6.2 fade over 100 years in steps of 1 = 37 + 63 = 20", (one, split), (20, 20))
    c.eq("6.2 toward 0, never past it", [faded(r, 7) for r in (30, 5, -30, -5, 0)], [23, 0, -23, 0, 0])
    # 6.3
    c.eq("6.3 p = min(1000, act x A div 1000)", [act_permille(150, a) for a in (300, 1000, 2500, 10000)],
         [45, 150, 375, 1000])
    lead, cs = rank_causes([(3, 1, 25), (7, 2, 40), (9, 3, 25), (11, 4, -60), (12, 5, 10)], 1)
    c.eq("6.3 causes: |points| desc, id desc, first three, ascending", (lead, cs), (7, (3, 7, 9)))
    lead, cs = rank_causes([(4, 1, 10), (4, 1, 10), (6, 2, 15)], 1)
    c.eq("6.3 one event twice is one cause (P-R5)", (lead, cs), (6, (4, 6)))
    c.eq("6.3 a sign with no contribution", rank_causes([(1, 1, 10)], -1), (0, ()))
    c.eq("6.3 telling (c + acts) mod 3", [act_telling(4, a) for a in range(3)], [1, 2, 0])
    c.eq("6.3 weighted kind 500/300/200", [pnr.weighted(u, [500, 300, 200]) for u in (0, 1 << 63, (1 << 64) - 1)],
         [0, 1, 2])
    c.eq("6.3 a weight 0 is never chosen", [pnr.weighted(u, [0, 300, 0]) for u in (0, (1 << 64) - 1)], [1, 1])
    # 6.4 pestilence and the effects
    c.eq("6.4 pestilence 12% of 3,016 and 535", [pestilence_dead(p, 120) for p in (3016, 535, 8)], [361, 64, 0])
    eff = [dict(kind="plenty", start=1000, years=50), dict(kind="withering", start=1020, years=50)]
    c.eq("6.4 plenty in force 1000 to 1049", [growth_sum(eff[:1], y, 300, 500) for y in (999, 1000, 1049, 1050)],
         [0, 300, 300, 0])
    c.eq("6.4 plenty and withering add", growth_sum(eff, 1030, 300, 500), -200)
    c.eq("6.4 growth clamp -900", growth_sum([dict(kind="withering", start=0, years=9)] * 3, 1, 300, 500), -900)
    c.eq("6.4 growth clamp 1000", growth_sum([dict(kind="plenty", start=0, years=9)] * 4, 1, 300, 500, 200), 1000)
    c.eq("6.4 a sacred shrine adds", growth_sum([], 5, 300, 500, 200), 200)
    fe = [dict(kind="valour", start=10, years=50), dict(kind="defeat", start=20, years=50)]
    c.eq("6.4 force valour 50, with defeat 60", [force_sum(fe, y, 50, 60) for y in (9, 10, 30, 65, 70)],
         [0, 50, -10, -60, 0])
    c.eq("6.4 force clamp", force_sum([dict(kind="defeat", start=0, years=9)] * 10, 1, 50, 60), -500)
    c.eq("7.1 growth term", [growth_term(d, g) for d, g in ((10, 300), (1, -500), (0, 300), (-5, 300), (7, -900))],
         [13, 0, 0, -5, 0])
    c.eq("7.1 force term", [force_term(10000, f) for f in (50, -60, 0)], [10500, 9400, 10000])
    # 6.7 to 6.9
    c.eq("6.7 founding p", [cult_found_permille(250, 500, f) for f in (True, False)], [250, 125])
    c.eq("6.7 pantheon-small founds every time", cult_found_permille(1000, 500, True), 1000)
    c.eq("6.8 spread p by affinity", [spread_permille(20, a) for a in (1000, 500, 100)], [20, 10, 2])
    c.eq("6.8 Q4 p x L and its cap", [spread_permille(20, 1000, 30), spread_permille(20, 1000, 60)], [600, 1000])
    c.eq("6.9 adopt by temper", [temper_permille(15, f) for f in (1000, 500, 1500, 700)], [15, 7, 22, 10])
    c.eq("6.9 Q4 adopt p x L", temper_permille(15, 1500, 40), 880)
    c.eq("6.9 suppress just 1300", temper_permille(10, 1300), 13)
    # grammar
    c.eq("4.1 god_title", [god_title_ok(s) for s in ("the Keeper of Oaths", "Keeper", "the  Red", "the Red-Banner",
                                                      "the Red Banner ", "the Ash{x}")],
         [True, False, False, True, False, False])
    c.eq("4.2 creator_title", [god_title_ok(s, True) for s in ("the Maker of {plural}", "the Maker",
                                                                "the {plural} of {plural}")], [True, False, False])
    c.eq("4.4 god_form", [god_form_ok(s) for s in ("The Order of {god}", "The Order", "{god} of {god}",
                                                    "The {root} of {god}")], [True, False, False, False])
    c.eq("4.2 champion_epithet", god_form_ok("Chosen of {god}", 24), True)
    # 4.2 the checks
    tt = pdr.pantheon_shipped_030()
    c.eq("4.2 shipped passes its checks", pantheon_messages(tt), [])
    c.eq("4.2 flare below act", pantheon_messages(dict(tt, flare_regard=90))[0],
         "pantheon_flare_above_act: flare_regard 90 is below act_threshold 100")
    c.eq("4.2 act above max", pantheon_messages(dict(tt, act_threshold=500, flare_regard=600))[-1],
         "pantheon_act_below_max: act_threshold 500 is above regard_max 400")
    c.eq("4.2 code defaults inert", [code_default_tuning()[k] for k in ("act_permille", "cult_found_permille",
                                                                       "flare_permille", "regard_max")],
         [0, 0, 0, 400])
    # 14 the largest products
    c.ok("14 act x A <= 10^7", 1000 * 10000 <= 10 ** 7)
    import p5_07_depths_reference as p507
    c.eq("12.2 stir ppm: 150 x M 1000 x L 30 x 1000", p507.stir_ppm([0, 150, 600], 2, 1000, 30, 1000), 4500)
    c.eq("12.2 stir ppm, warren x 500", p507.stir_ppm([0, 150, 600], 3, 1000, 50, 1000, 500), 15000)
    c.eq("12.2 stir ppm capped", p507.stir_ppm([0, 150, 600000], 3, 1600, 25, 1000), 1000000)
    c.eq("12.2 stir 0 at deep_stir_permille 0", p507.stir_ppm([0, 150, 600], 3, 1000, 50, 0), 0)
    with _pcr().rules(VERSION):
        # the argument is the event's depth value d + 1 (P-R23: 12.3's d is the old depth of P5-07 160)
        c.eq("12.3 telling (s + 4 d) mod 9", [p507.telling_delve(7, d + 1) for d in (1, 2, 3)], [2, 6, 1])
        c.eq("12.3 warren telling (t + 4 d) mod 9", [p507.telling_warren_delved(5, d + 1) for d in (1, 2, 3)], [0, 4, 8])
    c.eq("11.1 shrine_word", (SHRINE_WORD[False], SHRINE_WORD[True]), ("holy to", "cursed by"))
    c.eq("11.1 group gods 9, ALL_GROUPS 0x3FF", (GROUP_GODS, ALL_GROUPS), (9, 1023))
    c.eq("3 streams: 107 + 13", 107 + len(STREAMS) + len(DEEP_STREAMS), STREAM_COUNT)


def check_data(c):
    """16 item 1 on the listings of p3_data_reference (the shipped data of 4.1 to 4.4) and its checks."""
    pdr.use_version(VERSION)
    gods = [(k, pdr.god_def_030(k)) for k in sorted(pdr.GODS_030, key=lambda s: s.encode())]
    c.eq("4.1 nine gods in canonical order", [k for k, _ in gods],
         ["conquest", "craft", "death", "harvest", "lore", "oaths", "ruin", "war", "wild"])
    c.eq("4.1 three per camp", sorted(sum(1 for _k, d in gods if d["camp"] == x) for x in CAMPS), [3, 3, 3])
    monsters = {k: v["wake"] for k, v in pdr.load_root("game/data", kinds=("monster",))["monster"]}
    cultures = {k for k, _ in pdr.load_root("game/data", kinds=("culture",))["culture"]}
    for k, d in gods:
        c.eq("4.1 god %s passes its checks" % k, god_messages(k, d, monsters, cultures), [])
    bad = dict(gods[0][1], name_culture="dwarf", domain="creation", title="Red", monsters=["ymrach"],
               wrath=dict(curse=0, monster=10, mark=0), favour=dict(bless=0, champion=0, mark=0))
    c.eq("4.1 the god checks", [m.split(":")[0] for m in god_messages("x", bad, monsters, cultures)],
         ["god_culture", "grammar", "god_title", "god_monsters", "god_favour_weights"])
    c.eq("4.1 god_monster_weight", god_messages("x", dict(gods[0][1], wrath=dict(curse=0, monster=10, mark=0)),
                                                monsters, cultures), ["god_monster_weight: wrath.monster needs monsters"])
    pe.use_version(VERSION)
    cat = {k["key"]: k for k in pe.COMPILED}
    for k, d in gods:
        c.eq("4.3 deeds of %s pass" % k, deed_messages(d["deeds"], cat), [])
    c.eq("4.3 creator deeds pass", deed_messages(pdr.CREATOR_030["deeds"], cat), [])
    c.eq("4.3 deed checks", deed_messages([dict(kind="nope", role="x", points=1, who="any", against=""),
                                           dict(kind="sacked", role="settlement", points=1, who="any", against=""),
                                           dict(kind="sacked", role="attacker", points=5, who="foe", against=""),
                                           dict(kind="sacked", role="attacker", points=0, who="any",
                                                against="attacker")], cat),
         ["deeds[0]: kind nope is unknown", "deeds[1]: role settlement of sacked is not a civilization role",
          "deeds[2]: a deed for foes must be negative", "deeds[3]: against is role", "deeds[3]: points 0"])
    c.eq("4.3 creator record", (pdr.CREATOR_030["favour"], pdr.CREATOR_030["wrath"], pdr.CREATOR_030["bless"],
                                pdr.CREATOR_030["curse"], len(pdr.CREATOR_030["deeds"])),
         (dict(bless=500, champion=300, mark=200), dict(curse=700, monster=0, mark=300), "plenty", "pestilence", 9))
    tt = pdr.pantheon_shipped_030()
    c.eq("4.2 the 37 fields in order", [n for n, *_ in pdr.PANTHEON_030][:4] + [len(pdr.PANTHEON_030)],
         ["activity_permille", "regard_max", "regard_fade_per_century", "regard_memory_years", 37])
    c.eq("4.2 starts", [tt[k] for k in ("act_threshold", "act_permille", "god_rest_years", "pair_rest_years",
                                         "flare_regard", "send_hops", "sanctum_hops")], [100, 150, 30, 100, 200, 2, 3])
    c.eq("4.2 activity", tt["activity_permille"], dict(distant=300, normal=1000, meddling=2500))
    c.eq("4.4 forms of every culture", sorted(pdr.CULTURE_FORMS_030), sorted(cultures))
    for cul, forms in sorted(pdr.CULTURE_FORMS_030.items()):
        for f, field in zip(forms, pdr.CULTURE_FORM_FIELDS_030):
            c.ok("4.4 %s.%s 1 to 8 god_forms" % (cul, field), 1 <= len(f) <= 8 and all(god_form_ok(s) for s in f))
    root = pdr.load_root("game/data", kinds=("history", "culture", "god"))
    h = dict(root["history"])
    c.eq("4.2 the history root has pantheon and creator_title", ("pantheon" in h, h["chronology"]["creator_title"]),
         (True, "the Maker of {plural}"))
    # 12.2, 12.5, 12.6: the shipped values within the fields' ranges (the starts 1000, 3 and 200 were moved by the
    # calibration: deep_stir_permille 700 at 0.30.0)
    c.ok("12 follow-up fields in range", 0 <= h["monsters"]["deep_stir_permille"] <= 1000 and
         0 <= h["alliances"]["renew_written_max"] <= 100 and 0 <= h["figures"]["tower_lapse_years"] <= 10000)


def check_render(c):
    """16 item 9: every new template at each value form, {g:titled}, the changed templates of 12.3 to 12.6."""
    pe.use_version(VERSION)
    dat = pdr.load_root("game/data")
    report = [(e["kind"], e["salience"], e["template"]) for e in dict(dat["history"])["report"]["events"]]
    cat = {k["key"]: k for k in pe.event_catalogue(report)}
    for key in list(KINDS.values()) + ["delving_deepened", "warren_delved", "hold_lost", "ally_aided_hold",
                                       "tower_founded"]:
        c.eq("8.2 %s passes template_problem" % key, pe.template_problem(cat[key], cat[key]["templ"]), None)
    c.eq("8.1 kinds 100 to 110", [(k["id"], k["key"], k["caused"]) for k in pe.COMPILED if k["id"] >= 100],
         [(i, KINDS[i], 0 if i <= 104 else 1) for i in range(100, 111)])
    names = {(GOD, 1): "Saene", (GOD, 12): "Ormala", (GOD, 14): "Thorvald", (GOD, 15): "Hethra", (GOD, 3): "Kruskux",
             (CIV, 1): "The Gub Horde", (CIV, 2): "The Kingdom of Harl", (CIV, 3): "The Hold of Durn",
             (CIV, 4): "The Horde of Krog", (FIGURE, 5): "Edric", (MONSTER, 2): "Grumm", (SITE, 6): "The Stinking Ground of Kruskux",
             (SITE, 7): "The Temple of Ormala", (SITE, 8): "The Star-Tower of Femaeth", (FACTION, 1): "The Order of Ormala",
             (FACTION, 2): "The Children of Saene", (SETTLEMENT, 9): "Dunmere", (REGION, 7): "Iron Downs",
             (CIV, 5): "The House of Glaefaenn"}
    titles = {1: "the Maker of Elves", 12: "the Keeper of Oaths", 14: "the Shield of Battles",
              15: "the Hunter in the Green", 3: "the Maker of Goblins"}

    def name(t, x, form):
        if form == "kind":
            return "a troll of the hills"
        if t == FIGURE and x == 99:
            return "Zig"
        return names[(t, x)]

    def view(eid, key, parts, values, settlement=9, causes=(), extra=None):
        evs = {eid: dict(year=900, end_year=900, kind=cat[key]["id"], region=7, settlement=settlement,
                         participants=[(i + 1, s, t, x) for i, (s, t, x) in enumerate(parts)],
                         values=dict(enumerate(values, 1)), causes=list(causes))}
        for cid, ckey in (extra or {}).items():
            evs[cid] = dict(year=899, end_year=899, kind=cat[ckey]["id"], region=7, settlement=settlement,
                            participants=[], values={}, causes=[])
        return dict(catalogue={k["id"]: k for k in cat.values()}, events=evs, name=name,
                    region_at=lambda r, e: names[(REGION, r)], region_at_year=lambda r, y: names[(REGION, r)],
                    ruler_at=lambda c, e: 99 if c == 4 else 0, god_title=lambda g: titles[g])

    act = ((1, GOD, 1), (2, CIV, 1))
    c.eq("8.2 sample curse", pe.render(view(20, "divine_curse", act, (1, 1206, 0, 0, 0), causes=(5,),
                                            extra={5: "sacked"}), 20),
         "Angered by the sack of a city, Saene the Maker of Elves sent a pestilence upon the Gub Horde; 1,206 died.")
    for eff, dead, want in ((2, 0, "Saene the Maker of Elves withered the fields of the Gub Horde."),
                            (3, 0, "Saene the Maker of Elves turned the fortunes of war against the Gub Horde."),
                            (1, 1, "Saene the Maker of Elves sent a pestilence upon the Gub Horde; one died.")):
        c.eq("8.2 curse effect %d dead %d" % (eff, dead),
             pe.render(view(21, "divine_curse", act, (eff, dead, 50, 0, 0)), 21), want)
    bl = ((1, GOD, 12), (2, CIV, 2))
    c.eq("8.2 sample bless", pe.render(view(22, "divine_bless", bl, (2, 50, 0, 0), causes=(5, 6),
                                            extra={5: "alliance_formed", 6: "ally_joined_war"}), 22),
         "Pleased by an oath sworn and an ally's faith kept, Ormala the Keeper of Oaths blessed the arms of the Kingdom "
         "of Harl for 50 years.")
    for tel, eff, yrs, want in ((1, 1, 1, "Ormala the Keeper of Oaths smiled upon the fields of the Kingdom of Harl for "
                                          "a year."),
                                (2, 1, 0, "Ormala the Keeper of Oaths set its hand over the fields of the Kingdom of "
                                          "Harl.")):
        c.eq("8.2 bless telling %d years %d" % (tel, yrs), pe.render(view(23, "divine_bless", bl, (eff, yrs, tel, 0)),
                                                                    23), want)
    ch = ((1, GOD, 14), (2, FIGURE, 5), (3, CIV, 2))
    for tel, verb in ((0, "raised up"), (1, "chose"), (2, "set its mark upon")):
        c.eq("8.2 champion telling %d" % tel, pe.render(view(24, "divine_champion", ch, (tel, 0), causes=(5,),
                                                             extra={5: "war_won"}), 24),
             "Pleased by a war won, Thorvald the Shield of Battles %s Edric as the champion of the Kingdom of Harl." % verb)
    mo = ((1, GOD, 15), (2, MONSTER, 2), (3, CIV, 2), (4, SITE, 6))
    for tel, verb in ((0, "sent"), (1, "loosed"), (2, "woke")):
        c.eq("8.2 monster telling %d" % tel, pe.render(view(25, "divine_monster", mo, (tel, 0), causes=(5,),
                                                            extra={5: "colony_founded"}), 25),
             "Angered by the felling of the wild, Hethra the Hunter in the Green %s Grumm, a troll of the hills, against "
             "the Kingdom of Harl." % verb)
    mk = ((1, GOD, 3), (2, CIV, 1), (3, SITE, 6))
    c.eq("8.2 profane mark", pe.render(view(26, "divine_mark", mk, (1, 0, 0), causes=(5,), extra={5: "sacked"}), 26),
         "After the sack of a city, Kruskux the Maker of Goblins cursed the ground of the Stinking Ground of Kruskux.")
    c.eq("8.2 sacred mark", pe.render(view(27, "divine_mark", mk, (0, 0, 0)), 27),
         "Kruskux the Maker of Goblins hallowed the ground of the Stinking Ground of Kruskux.")
    cf = ((1, FACTION, 1), (2, GOD, 12), (3, CIV, 2), (4, FIGURE, 5), (5, SITE, 7))
    for tel, lead in ((0, "In the name of"), (1, "Calling upon"), (2, "Sworn to")):
        c.eq("8.2 cult_founded telling %d" % tel, pe.render(view(28, "cult_founded", cf, (tel,), causes=(4,),
                                                                 extra={4: "divine_bless"}), 28),
             "%s Ormala the Keeper of Oaths, Edric founded the Order of Ormala at the Temple of Ormala in Dunmere." % lead)
    sp = ((1, FACTION, 1), (2, GOD, 12), (3, CIV, 3), (4, CIV, 2))
    c.eq("8.2 cult_spread by trade", pe.render(view(29, "cult_spread", sp, (), causes=(4, 5),
                                                    extra={4: "cult_founded", 5: "trade_opened"}), 29),
         "The Order of Ormala spread from the Kingdom of Harl into the Hold of Durn along the roads of trade.")
    c.eq("8.2 cult_spread by an origin alliance (no link event)",
         pe.render(view(29, "cult_spread", sp, (), causes=(4,), extra={4: "cult_founded"}), 29),
         "The Order of Ormala spread from the Kingdom of Harl into the Hold of Durn.")
    ad = ((1, FACTION, 1), (2, GOD, 12), (3, CIV, 4))
    c.eq("8.2 cult_adopted with a ruler", pe.render(view(30, "cult_adopted", ad, ()), 30),
         "The Horde of Krog under Zig took the Order of Ormala as the faith of the realm.")
    su = ((1, FACTION, 2), (2, GOD, 1), (3, CIV, 4))
    c.eq("8.2 sample suppression into hiding", pe.render(view(31, "cult_suppressed", su, (1,)), 31),
         "The Horde of Krog under Zig put down the Children of Saene, and its faithful fled into hiding.")
    c.eq("8.2 suppression", pe.render(view(31, "cult_suppressed", su, (0,)), 31),
         "The Horde of Krog under Zig put down the Children of Saene.")
    ab = ((1, SITE, 8), (2, CIV, 5), (3, FIGURE, 5))
    c.eq("8.2 sample tower_abandoned", pe.render(view(32, "tower_abandoned", ab, (), causes=(4,),
                                                      extra={4: "figure_died"}), 32),
         "After the death of its last master, the Star-Tower of Femaeth of the House of Glaefaenn stood empty.")
    tu = ((1, FIGURE, 5), (2, CIV, 5), (3, SITE, 8))
    c.eq("8.2 tower_taken_up", pe.render(view(33, "tower_taken_up", tu, (), causes=(4,), extra={4: "figure_died"}), 33),
         "After the death of its last master, Edric of the House of Glaefaenn became master of the Star-Tower of "
         "Femaeth.")
    with _pcr().rules(VERSION):
        dd = ((1, CIV, 3), (2, SITE, 8))
        c.eq("12.3 delving telling 8 depth 3", pe.render(view(34, "delving_deepened", dd, (3, 8)), 34),
             "Singing the old delving songs, the Hold of Durn broke the floor of the Star-Tower of Femaeth into the "
             "oldest dark beneath the world, and found gold in the dark.")
        c.eq("12.3 delving telling 0 depth 1", pe.render(view(34, "delving_deepened", dd, (1, 0)), 34),
             "Following a rich seam, the Hold of Durn sank the shafts of the Star-Tower of Femaeth below the reach of "
             "daylight, and struck a vein of silver.")
        wd = ((1, CIV, 1),)
        c.eq("12.3 warren telling 4 depth 2", pe.render(view(35, "warren_delved", wd, (2, 4)), 35),
             "Stealing each other's picks, the Gub Horde dug the warrens of Dunmere down among the mountain's roots, and "
             "found old bones to gnaw.")
        hl = ((1, MONSTER, 2), (2, CIV, 3), (3, SITE, 6))
        c.eq("12.3 hold_lost telling 0", pe.render(view(36, "hold_lost", hl, (1206, 1810, 0)), 36),
             "Grumm came up out of the deep into Dunmere, and tore it from the Hold of Durn; 1,206 died, and 1,810 fled.")
        ai = ((1, CIV, 2), (2, CIV, 3), (3, MONSTER, 2), (4, SETTLEMENT, 9))
        c.eq("12.4 aid after a seal", pe.render(view(37, "ally_aided_hold", ai, (0,), causes=(4, 5),
                                                     extra={4: "deep_woken", 5: "deep_sealed"}), 37),
             "Answering its ally's call, the Kingdom of Harl sent warriors from Dunmere to stand with the Hold of Durn at "
             "the gate of Dunmere against Grumm, and the gate held.")
        c.eq("12.4 aid after a fall", pe.render(view(37, "ally_aided_hold", ai, (1,), causes=(4, 5),
                                                     extra={4: "deep_woken", 5: "hold_lost"}), 37),
             "Bound by old oaths, the Kingdom of Harl marched out of Dunmere to stand with the Hold of Durn at the gate of "
             "Dunmere against Grumm, but the hold fell all the same.")
    c.ok("12.6 tower_founded's new cause words", cat["tower_founded"]["templ"].startswith(
        "{@causes:figure_died=the death of the last master|After |, }"))
    c.eq("8.2 cause words of the acts: 36 entries", pdr.CAUSE_WORDS_LIST_030.count("=") , 36)
    bad = dict(cat["divine_bless"], templ="{civilization:titled}")
    c.ok("8.2 {g:titled} needs a god role", pe.template_problem(bad, bad["templ"]) is not None)


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

FIXTURE_SEEDS = (0, 1, 2, 3, 5, 6, 7, 9, 12, 13, 14)
REQUIRED_PATHS = ("bless", "curse", "champion", "monster", "mark", "founded", "cult_spread", "cult_adopted",
                  "cult_suppressed", "flare", "prophet founder")
REPORTED_PATHS = ("plenty", "valour", "pestilence", "withering", "defeat", "sacred mark", "profane mark",
                  "mark at a settlement", "champion founder", "caster founder", "ruler founder", "hidden",
                  "ended suppressed", "ended fell", "certain act", "temple lost", "temple suppressed", "stir",
                  "renewal capped", "champion fought", "tower_taken_up", "tower_abandoned", "faith without temple")


def cmd_fixtures(argv):
    pcr = _pcr()
    pcr.use_version(VERSION)
    quick = "--quick" in argv
    c = Checker()
    dat = pantheon_small_data()
    seen, where = set(), {}
    seeds = FIXTURE_SEEDS[:4] if quick else FIXTURE_SEEDS
    for seed in seeds:
        res, inp = run_case(seed, dat)
        for v in run_problems(res, inp, dat):
            c.ok("pantheon-small stress-%d %s" % (seed, v), False)
        ps = paths_of(res)
        for pth in ps:
            where.setdefault(pth, seed)
        seen |= ps
        w = res["_world"]
        print("stress-%d %s" % (seed, " ".join("%s=%d" % kv for kv in stats_of_tables(
            res, w.alliances.renewals_capped if w.alliances else 0,
            sum(1 for lg in w.monsters.deep.log if lg[1].startswith("stir")) if w.monsters.deep else 0))))
    for pth in REQUIRED_PATHS:
        c.ok("pantheon-small shows the path '%s' (seen: %s)" % (pth, ", ".join(sorted(seen))), pth in seen)
    for pth in REPORTED_PATHS:
        if pth not in seen:
            print("note: pantheon-small shows no '%s' (reported, not failed)" % pth)
    print("first seen (path: stress seed): %s" % ", ".join("%s %d" % kv for kv in sorted(where.items())))
    # the fixture files
    for name in fixture_files():
        d, cases, want_paths, _doc = fixture_data(name)
        got = set()
        for seed, case, activity in (cases[:1] if quick else cases):
            res, inp = run_case(seed, d, case, activity)
            for v in run_problems(res, inp, d):
                c.ok("%s seed %d %s" % (name, seed, v), False)
            got |= paths_of(res)
        for pth in want_paths:
            c.ok("%s shows '%s' (seen: %s)" % (name, pth, ", ".join(sorted(got))), pth in got or quick)
    # I0 (14): act_permille 0 and flare_permille 0 give the build without the subsystem, apart from kinds 100 to 110
    for seed in (FIXTURE_SEEDS[:1] if quick else FIXTURE_SEEDS[:3]):
        pcr.use_version(VERSION)
        z = zero_data(pcr.stress_data())
        a, _ = run_case(seed, z)
        pcr.FLAGS["pantheon"] = False
        b, _ = run_case(seed, pcr.stress_data())
        pcr.FLAGS["pantheon"] = True
        for tname in I0_TABLES:
            c.eq("I0 stress-%d table %s equals the build without the pantheon" % (seed, tname),
                 table_hash(a.get(tname, [])), table_hash(b.get(tname, [])))
        c.eq("I0 stress-%d no faction" % seed, a.get("faction"), [])
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def rows_of_db(path):
    """{table: rows in primary-key order} of every table of a world file, plus meta."""
    conn = sqlite3.connect("file:%s?mode=ro" % path, uri=True)
    out = {}
    for (t,) in conn.execute("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name").fetchall():
        if t == "meta":
            continue
        pk = [r[1] for r in conn.execute("PRAGMA table_info(%s)" % t).fetchall() if r[5]]
        order = ", ".join(pk) if pk else "rowid"
        out[t] = [tuple(r) for r in conn.execute("SELECT * FROM %s ORDER BY %s" % (t, order)).fetchall()]
    out["meta"] = dict(conn.execute("SELECT key, value FROM meta").fetchall())
    conn.close()
    if "relation" in out:   # the reference's form: until NULL -> None, rows sorted as relation_table
        out["relation"] = sorted(out["relation"], key=lambda r: (r[0], r[1], r[2], r[3], r[4].encode(), r[5]))
    return out


def cmd_check_world(argv):
    """13 on a 0.30.0 world file's rows (no replay; p3_conflict_reference.py check-world replays it)."""
    pcr = _pcr()
    pcr.use_version(VERSION)
    root = argv[argv.index("--data") + 1] if "--data" in argv else "game/data"
    paths = [a for a in argv if not a.startswith("--") and a != root]
    dat = pcr.data(root)
    bad = 0
    for path in paths:
        rows = rows_of_db(path)
        meta = rows["meta"]
        c = Checker()
        c.eq("%s schema_version" % path, meta.get("schema_version"), str(SCHEMA))
        races = {r[0]: r for r in rows.get("race", [])}
        mt = getattr(getattr(dat, "base", None), "monsters_tuning", None) or {}
        t = dict(dat.pantheon or pdr.pantheon_shipped_030(), _tower_lapse_years=dat.figures.get("tower_lapse_years", 0),
                 _hunt_memory_years=mt.get("hunt_memory_years"))
        creator_title = dat.chron.creator_title if hasattr(dat.chron, "creator_title") else None
        for v in violations(rows, list(dat.gods), t, creator_title, [r[1] for r in rows.get("region", [])],
                            [a[4] for a in rows.get("age", [])], races):
            c.ok("%s invariant %s" % (path, v), False)
        for v in reader_problems(rows):
            c.ok("%s reader %s" % (path, v), False)
        print("%s: %s; %d checks, %d failures" % (path, " ".join("%s=%d" % kv for kv in stats_of_tables(rows)),
                                                  c.count, c.failures))
        bad += c.failures
    return 1 if bad else 0


def reader_problems(rows):
    """9's reader checks on rows (the first is the reader's message; the C++ tests compare prefixes)."""
    out = []
    x = X(rows)
    races = rows.get("race", [])
    n = len(races)
    camp = {r[0]: r[5] for r in races}
    gods = rows.get("god", [])
    if [g[0] for g in gods] != list(range(1, len(gods) + 1)):
        out.append("corrupt table 'god': ids not dense")
    for g in gods:
        if len(g) < 6:
            out.append("corrupt table 'god': row %d: schema 13 row" % g[0])
            continue
        if g[0] <= n and (g[5] != g[0] or g[3] != 1 or g[2] != camp.get(g[0])):
            out.append("corrupt table 'god': row %d: creator row" % g[0])
        if g[0] > n and g[5] != 0:
            out.append("corrupt table 'god': row %d: race %d for a pantheon god" % (g[0], g[5]))
        if g[0] > n and not 2 <= g[3] <= 9:
            out.append("corrupt table 'god': row %d: domain %d not 2 to 9 for a pantheon god" % (g[0], g[3]))
        if not 0 <= g[2] <= 2:
            out.append("corrupt table 'god': row %d: camp %d" % (g[0], g[2]))
    fs = rows.get("faction", [])
    if [f[0] for f in fs] != list(range(1, len(fs) + 1)):
        out.append("corrupt table 'faction': ids not dense")
    hidden_by = {x.one(e, "faction") for e in x.of("cult_suppressed") if x.val(e, "hidden") == 1}
    for f in fs:
        k = f[0]
        pre = "corrupt table 'faction': row %d: " % k
        if f[2] != 1:
            out.append(pre + "kind %d" % f[2])
        if f[4] not in x.gods:
            out.append(pre + "god %d is not a god" % f[4])
        if f[3] not in x.r.civ:
            out.append(pre + "civ %d is not a civilization" % f[3])
        if f[5] not in x.figures:
            out.append(pre + "founder %d is not a figure" % f[5])
        seat = x.sites.get(f[6])
        if seat is None or seat[2] != TEMPLE or seat[10] != f[8]:
            out.append(pre + "seat %d is not its founding temple" % f[6])
        if x.kind(f[8]) != "cult_founded" or x.one(f[8], "faction") != k:
            out.append(pre + "founding_event %d is not its cult_founded" % f[8])
        if not 1 <= f[12] <= 3:
            out.append(pre + "status %d" % f[12])
        if (f[12] < 3) != (f[11] == 0 and f[9] is None):
            out.append(pre + "ending %d with status %d" % (f[11], f[12]))
        if f[12] == 3 and (f[10] not in x.r.ev or x.year[f[10]] != f[9]):
            out.append(pre + "ending_event %d not of year %s" % (f[10], f[9]))
        if (f[12] == 2) != (k in hidden_by):
            out.append(pre + "faction %d hidden without a sanctum" % k)
    for s in rows.get("site", []):
        pre = "corrupt table 'site': row %d: " % s[0]
        if not 1 <= s[2] <= 7:
            out.append(pre + "kind %d not 1 to 7" % s[2])
            continue
        oe = s[10]
        if s[2] == SHRINE and (x.kind(oe) != "divine_mark" or x.one(oe, "site") != s[0]):
            out.append(pre + "origin_event %d is not its divine_mark" % oe)
        if s[2] == TEMPLE and (x.kind(oe) not in ("cult_founded", "cult_adopted") or x.one(oe, "site") != s[0]):
            out.append(pre + "origin_event %d is not its cult_founded or cult_adopted" % oe)
        if s[2] == SANCTUM and (x.kind(oe) != "cult_suppressed" or x.one(oe, "sanctum") != s[0]):
            out.append(pre + "origin_event %d is not its cult_suppressed" % oe)
        allowed = {SHRINE: (0,), SANCTUM: (0,), TEMPLE: (0, 4, 7, 8), TOWER: (0, 4, 5, 7)}.get(s[2])
        if allowed is not None and s[13] not in allowed:
            out.append(pre + "ending %d is not %s for a %s" % (s[13], " or ".join(map(str, allowed)),
                                                               {4: "tower", 5: "shrine", 6: "temple",
                                                                7: "sanctum"}[s[2]]))
    for f in rows.get("figure", []):
        pre = "corrupt table 'figure': row %d: " % f[0]
        if not 1 <= f[3] <= (8 if _pcr().FLAGS.get("artifacts", False) else 7):   # P5-11 9: role 8 smith
            out.append(pre + "role %d" % f[3])
        if f[3] == CHAMPION and (x.kind(f[9]) != "divine_champion" or x.one(f[9], "figure") != f[0]):
            out.append(pre + "a champion's start_event %d" % f[9])
        if f[3] == PROPHET and (x.kind(f[9]) != "cult_founded" or x.one(f[9], "founder") != f[0]):
            out.append(pre + "a prophet's start_event %d" % f[9])
    ranges = {"divine_bless": dict(effect=(1, 2), telling=(0, 2), divine_script=(0, 1)),
              "divine_curse": dict(effect=(1, 3), telling=(0, 2), divine_script=(0, 1), dead=(0, 1 << 62)),
              "divine_champion": dict(telling=(0, 2), divine_script=(0, 1)),
              "divine_monster": dict(telling=(0, 2), divine_script=(0, 1)),
              "divine_mark": dict(profane=(0, 1), telling=(0, 2), divine_script=(0, 1)),
              "cult_founded": dict(telling=(0, 2)), "cult_suppressed": dict(hidden=(0, 1)),
              "delving_deepened": dict(telling=(0, 8)), "warren_delved": dict(telling=(0, 8))}
    for key, vs in ranges.items():
        for e in x.of(key):
            for v, (lo, hi) in vs.items():
                val = x.val(e, v)
                if val is None or not lo <= val <= hi:
                    out.append("corrupt table 'event_value': row %d: value %s %s outside %d to %d" % (
                        e, v, val, lo, hi))
    founded_at = {f[0]: f[8] for f in fs}
    for key in ("cult_spread", "cult_adopted", "cult_suppressed"):
        for e in x.of(key):
            k = x.one(e, "faction")
            if k not in founded_at or founded_at[k] > e:
                out.append("corrupt table 'event': row %d: %s names a faction not founded before it" % (e, key))
    for e in x.of("tower_abandoned"):
        s = x.sites.get(x.one(e, "site"))
        if s is None or s[2] != TOWER or s[13] != END_ABANDONED or s[12] != e:
            out.append("corrupt table 'event': row %d: tower_abandoned of a site not ended by it" % e)
    return out


GOLDEN_HEADER = """\
# P5-08 16: golden values of the pantheon and cults (generator 0.30.0, schema 14), written by
# worldgen/tests/reference/p5_08_pantheon_reference.py golden (the test writer's reference, never from C++ output).
# Lines: case <name> <seed> <size> <data>; table <name> <sha256 of repr rows>; stat <key> <value>.
# Fixture cases (size 0: a conflict stress geography; the files of worldgen/tests/data/pantheon/) also list
# path <path> for every path of 6 the run shows. World lines (pantheon-small at 512 seeds 0 to 9, shipped 42 at
# 1024) are added at integration with --inputs.
"""
GOLDEN_TABLES = ("god", "faction", "site", "figure", "event", "event_value")


def case_lines(name, seed, size, what, res):
    pchr = __import__("p3_chronology_reference")
    pcr = _pcr()
    w = res["_world"]
    o = ["case %s %d %d %s" % (name, seed, size, what)]
    cols = dict(pcr.COLUMNS, god=6, faction=13)
    for t in GOLDEN_TABLES:
        o.append("table %s %s" % (t, pchr.table_hash(cols.get(t, len(res[t][0]) if res.get(t) else 1),
                                                     res.get(t, []))))
    st = stats_of_tables(res, w.alliances.renewals_capped if w.alliances else 0,
                         sum(1 for lg in w.monsters.deep.log if lg[1].startswith("stir")) if w.monsters.deep else 0)
    o += ["stat %s %d" % kv for kv in st]
    return o


def golden_text(inputs=None):
    pcr = _pcr()
    pchr = __import__("p3_chronology_reference")
    pcr.use_version(VERSION)
    o = [GOLDEN_HEADER.rstrip("\n")]
    for name in fixture_files():
        d, cases, _paths, _doc = fixture_data(name)
        for seed, case, activity in cases:
            res, inp = run_case(seed, d, case, activity)
            bad = run_problems(res, inp, d)
            assert not bad, (name, seed, bad[:5])
            o += case_lines("%s-%d" % (name[:-5], seed), seed, 0, name[:-5], res)
            o += ["path %s" % p for p in sorted(paths_of(res))]
    if inputs:
        cases = [("pantheon-small-%d-512" % s, s, 512, os.path.join(inputs, "pantheon-%d-512.db" % s), "pantheon-small")
                 for s in range(10)] + [("shipped-42-1024", 42, 1024, os.path.join(inputs, "geo-seed-42-1024.db"),
                                         "shipped")]
        for name, seed, size, path, what in cases:
            if not os.path.exists(path):
                print("missing input %s" % path, file=sys.stderr)
                continue
            dat = pantheon_small_data(pcr.data()) if what == "pantheon-small" else pcr.data()
            conn = pchr.open_world(path)
            p = pchr.params(span=6000, races=pchr.races_of("all", dat.chron), detail="normal",
                            civs=pp.civs_param("auto"), intensity="normal")
            inp, _chron = pcr.inputs_for(dat, conn, p, seed, size)
            conn.close()
            res = pcr.history_stage(dat, inp)
            o += case_lines(name, seed, size, what, res)
    return "\n".join(o) + "\n"


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    cmd = argv[0]
    if cmd == "units":
        c = Checker()
        _pcr().use_version(VERSION)
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
    sys.modules.setdefault("p5_08_pantheon_reference", sys.modules["__main__"])
    sys.exit(main(sys.argv[1:]))
