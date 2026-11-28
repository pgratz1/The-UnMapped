#!/usr/bin/env python3
"""Reference of P5-11 "Artifacts" (docs/specs/p5-11-artifacts.md r1, generator 0.31.0, schema 15, history report
format v12), written by the test writer from the spec alone, without the engineers' branch (p5/11-impl), with the
lead's rulings L-1 to L-11 as recommended in the spec's 21.

The rules of 5 and 6 run inside p3_conflict_reference.py's replay: at 0.31.0 (conflict flag `artifacts`) the conflict
World installs `Artifacts` last and calls it at the end of T3, T4, T6 and Q4 (6.1). The seams of 7 and the follow-ups
of 12 sit in the other references behind the conflict flags `artifacts` and `p511`:
  - 4 and 8.2 (the kind artifact, artifact_forms, history.artifacts, smith_age_*, cult_return_years, the deeds and
    cause words of 4.5, the faerie form, the report entries 111 to 116): p3_data_reference (KINDS_031, history_031,
    culture_031, god_031). The values are read from the data root; a 0.30.0 root on disk is read at 0.31.0 with the
    test writer's fixture files of the spec's listing (worldgen/tests/data/artifacts/spec-r1/) filled in where the root
    lacks an item (FALLBACK_031_USED), never from values written in a module;
  - 8.1 (kinds 111 to 116), 7.6 (entity type 10's name, the form kind, empty titled), schema 15: p3_engine_reference
    (COMPILED_031, RENDER_R31); 6.6 the name kind artifact: p3_names_reference (STREAMS_023);
  - 7.2 the win factor, the strain factor, the declaration's and the battle_led's causes, and 12.1 the acts in force:
    p3_conflict_reference (fight, the strain loop, declare);
  - 7.2 hunts: p5_02_monsters_reference.Monsters.hunts;
  - 7.3 raise_maker, role 8, brief, caster_site, a smith's place of death and no epithet: p5_06_figures_reference;
  - 7.4 forge_site: p5_04_alliances_reference.Alliances.forge_site;
  - 12.1 force_act and 12.2 the return bar: p5_08_pantheon_reference.Pantheon.

Commands (from the repository root):
  python3 -B worldgen/tests/reference/p5_11_artifacts_reference.py units            formulas, data, templates
  python3 -B worldgen/tests/reference/p5_11_artifacts_reference.py fixtures [--quick] artifacts-small on the conflict
                                                                                   stress geographies: the paths of
                                                                                   6, the invariants of 13, I0 (14)
  python3 -B worldgen/tests/reference/p5_11_artifacts_reference.py check-world DB [DB ...] [--data ROOT]
  python3 -B worldgen/tests/reference/p5_11_artifacts_reference.py golden [PATH|-] [--inputs DIR]
  python3 -B worldgen/tests/reference/p5_11_artifacts_reference.py readings

`golden` writes worldgen/tests/golden/p5-11-artifacts.txt: the fixture lines (artifacts-small on the stress
geographies) always; with --inputs DIR also the world lines of 16 (artifacts-small at 512 on seeds 0 to 9 from
DIR/artifacts-<seed>-512.db, the shipped seed 42 at 1024 from DIR/geo-seed-42-1024.db; any 0.31.0 file of that
geography, `--until regions` suffices). Recorded at integration from this reference, never to make a C++ test pass.
"""

import copy
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import core_reference as cr  # noqa: E402
import p3_engine_reference as pe  # noqa: E402
import p3_names_reference as pnr  # noqa: E402
import p3_peoples_reference as pp  # noqa: E402
import p3_data_reference as pdr  # noqa: E402

GOLDEN = os.path.join(pe.REPO, "worldgen", "tests", "golden", "p5-11-artifacts.txt")
FIXTURE_DIR = os.path.join(pe.REPO, "worldgen", "tests", "data", "artifacts")
VERSION = "0.31.0"
SCHEMA = 15
HEADER = "# worldgen history v12"
# 3: appended after hist.deep.warren_stir (the 120th), in this order (127 streams)
STREAMS = ("hist.artifacts.make", "hist.artifacts.class", "hist.artifacts.kind", "hist.artifacts.take",
           "hist.artifacts.bury", "hist.artifacts.raid", "hist.names.artifact")
STREAM_COUNT = 127
KINDS = {111: "artifact_made", 112: "artifact_taken", 113: "artifact_lost", 114: "artifact_hoarded",
         115: "artifact_recovered", 116: "artifact_buried"}
KIND_ID = {v: k for k, v in KINDS.items()}
CLASSES = ("arms", "armour", "regalia", "relic", "staff")   # codes 1 to 5
ARMS, ARMOUR, REGALIA, RELIC, STAFF = 1, 2, 3, 4, 5
KEPT, BORNE, HOARDED, LOST, BURIED, ENSHRINED = 1, 2, 3, 4, 5, 6
STATE_WORDS = {1: "kept", 2: "borne", 3: "hoarded", 4: "lost", 5: "buried", 6: "enshrined"}   # 11.1 artifact_state_word
HELD_WORDS = {1: "kept by", 2: "borne by", 3: "hoarded by", 4: "lost in", 5: "buried with", 6: "at"}   # 10 item 2
STATE_HOLDER = {1: (3,), 2: (6,), 3: (8,), 4: (4, 5, 9), 5: (6,), 6: (9,)}   # 9
GOD, CIV, SETTLEMENT, REGION, FIGURE, MONSTER, SITE, ARTIFACT = 2, 3, 4, 5, 6, 8, 9, 10
LAIR, MINE, TOWER, TEMPLE, SANCTUM = 1, 3, 4, 6, 7
CASTER, SMITH = 5, 8
AWAKE, ASLEEP = 1, 2
END_SUPPRESSED = 8
SMITH_MADE, COMMISSION, CASTER_MADE, GOD_GIVEN = 1, 2, 3, 4   # artifact.maker_kind
ARTIFACTS_MAX = 100000
SUMMARY_ARTIFACTS_MAX = 8
NEVER = -(1 << 62)
GROUP_ARTIFACTS = 10
ALL_GROUPS = 0x7FF
GLYPH_ARTIFACT = 24
STATS_KEYS = ("artifacts_made", "artifacts_smiths", "artifacts_commissioned", "artifacts_casters", "artifacts_gods",
              "artifacts_deep", "artifacts_arms", "artifacts_armour", "artifacts_regalia", "artifacts_relic",
              "artifacts_staff", "artifacts_taken", "artifacts_lost", "artifacts_hoarded", "artifacts_recovered",
              "artifacts_buried", "artifact_holdings", "artifacts_changed_hands", "artifacts_lost_at_end",
              "artifacts_held_by_others", "smiths", "wars_citing_artifacts", "battles_with_artifacts",
              "battles_under_acts")   # 10 item 6, in this order (cult_returns_barred dropped: lead ruling, A-R14)
REPLAY_STATS = ()   # none since the lead dropped cult_returns_barred (A-R14)
MOVE_KEYS = ("general_appointed", "figure_died", "fell_in_battle", "hero_slain", "monster_slain_by_hero",
             "monster_slain", "monster_died", "monster_raid", "hold_lost", "sacked", "settlement_captured",
             "civ_conquered", "civ_ended", "peace", "war_won", "cult_suppressed")   # 6.7 M1 to M14
QUIET_MOVE_KEYS = ("general_appointed", "figure_died", "monster_died", "cult_suppressed")   # M1, M2, M6, M14
BATTLE_KEYS = ("battle", "battle_at_pass", "allied_battle", "battle_led")

READINGS = """\
A-R1 (9, 8.1) occasion_event: 9 says "that event's first cause", but 8.1 lists the causes ascending and a deep
     artifact also cites its forge site's last delving_deepened, which is older than the occasion whenever the mine
     was deepened before it: the first cause is then the delving, not the occasion. The reference stores the occasion
     (the event e of 6.3) and checks "occasion_event is a cause of made_event and is not a delving_deepened"
     (finding S1).
A-R2 (6.5) A staff is borne by its caster only when the caster bears no staff (6.2: "a figure bears at most one
     artifact of each class"; 6.5's staff row has no such test and a caster can make two): a second staff of the same
     caster is kept by c (finding S2).
A-R3 (6.2, 9) The place columns of a lost holding in an empty lair (holder type 9) have settlement 0 (6.2: "settlement
     (type 4) or 0"), while an enshrined holding has the site's settlement (6.2's state 6 row). A buried holding has the
     figure's death_settlement.
A-R4 (6.2, 6.8) A holding's from_year is the year of its opening event (the step's year Y for an event written by the
     subsystem; the triggering event's year for a quiet move), so that 9's "event of year from_year" holds in a quiet
     step whose events carry years after its start; the closed holding's to_year is the new from_year.
A-R5 (6.3, 6.7 M2) "visible" is P3-04 6.2's visibility (an entity created in an earlier step), as P5-08's
     alive_visible; M2's "hidden" is its negation (finding S3: 6.9's "its seat when the realm is hidden" suggests a
     realm lying low, which the spec does not define here).
A-R6 (8.1 kind 111) The role `site` names the maker site (artifact.maker_site) when it is not 0.
A-R7 (7.1) battle_artifact's "contributor" is a borne artifact whose kind's battle_permille is above 0.
A-R8 (6.7 M12) A seat that stands but whose owner is c itself, none or an ended realm is treated as a ruin: lost in
     t (holder type 4, the settlement's place), as the engine does (integration; 6.7 names only "another's"; the
     test writer's first reading, t = 0, is withdrawn).
A-R21 (6.7 M10, M13; integration, the engine's readings) M10 takes only for a living captor (as M9; no draw
     otherwise); M13 moves nothing when the winner has ended. M6 for a monster with no lair: lost on the field of
     the event's region.
A-R9 (8.1 kind 115) artifact_recovered names a figure only for how 1 by monster_slain_by_hero (the hero, whether it
     took the artifact up or its realm did); a recovery by a won war (how 2) names none.
A-R10 (7.2) The declaration's claim cause is added wherever a war_declared is written (T2 and a flare-up's forced
     declaration alike); 7.2 says "at its T2".
A-R11 (12.2) The bar is tested after 6.8's link test and before the quiet budget: "would otherwise have been a
     candidate" counts a realm with a partner in the cult, whatever the budget.
A-R12 (10 item 2) A `held:` entry is `<from> <state words> <tag><id> <name> e<id>` with the holder's tag (c, f, m, r,
     s, si) and its name; region names as at E.
A-R13 (10 item 3) "holds ... at its end": for an ended realm a holding in force in its ended year (from_year <=
     ended_year, and open or to_year >= ended_year; the engine's reading 6 of part D, taken at integration: the test
     writer's first reading by event ids is withdrawn); for a living realm the holdings open at E.
A-R14 (10 item 6) cult_returns_barred is not in rows (no event): the lead dropped the stat (2026-11-28); the return
     bar of 12.2 stays and is checked by invariant 8.
A-R15 (10 item 6) wars_citing_artifacts: a war_declared that cites the event of a holding of an artifact whose home
     is the attacker and which the defender keeps or one of its figures bears at the declaration.
     battles_with_artifacts: a battle_led with an artifact_made cause; battles_under_acts: a battle of the four kinds
     with a divine_bless or divine_curse cause.
A-R16 (11.1) artifact_at for state 3 gives the monster's lair at the year (the site of the monster with the largest
     origin_year <= year): its region, site and cell; settlement 0.
A-R17 (6.7 M13) "borne by a living figure of civilization l": the figure's civ column; the recovery's figure role is
     empty (A-R9).
A-R19 (6.4, P5-07 9) forge(c): a realm with an open mine at any depth is a forge (lead ruling 2026-11-28 on finding
     S4: 6.4's formula forge_depth(c) >= 1 is corrected to "has an open mine"); among partners the largest
     forge_depth, then the smaller id. The deep test (forge_depth(F) >= deep_metal_depth) is unchanged.
A-R20 (8.1) An artifact event that names a settlement takes that settlement's region (lead ruling 2026-11-28; the
     engineers' reading 3): an artifact_made at a mine or library whose cell lies across a border, an
     artifact_hoarded how 2 and 3 (the raided or lost seat's region, not the lair's).
A-R18 (7.1, 13 item 7) claim_event is the largest holding `event` among the contributing artifacts; a holding opened
     by a quiet move has the triggering event (a sack, a conquest), which the declaration then cites.
"""


def _pcr():
    import p3_conflict_reference as pcr
    return pcr


def _p506():
    import p5_06_figures_reference as p506
    return p506


def _p508():
    import p5_08_pantheon_reference as p508
    return p508


# ---------------------------------------------------------------------------
# Formulas (6.3 to 6.5, 7.1, 7.6), each as the spec writes it (pure; the units check them by hand)
# ---------------------------------------------------------------------------

def kind_form(noun, deep):
    """7.6 the form kind: "a"/"an" + " " + noun, then " of deep iron" when deep is 1."""
    return pe.artifact_kind_form(noun, deep)


def make_permille(chance, forge, forge_chance):
    """6.3: p = min(1000, chance x (F != 0 ? forge_chance : 1000) div 1000)."""
    return min(1000, chance * (forge_chance if forge else 1000) // 1000)


def class_weights(classes, has_god, caster_with_tower):
    """6.3: W = o.classes in the order arms, armour, regalia, relic, staff, relic 0 unless e has a god participant,
    staff 0 unless caster_of(c) masters an open kind 4 site."""
    w = [classes[c] for c in CLASSES]
    if not has_god:
        w[3] = 0
    if not caster_with_tower:
        w[4] = 0
    return w


def forge_of(c, depth, partners):
    """6.4 forge(c) (lead ruling 2026-11-28, reading A-R19): depth(x) is None when x has no open mine, else the
    forge_depth of its open mines (0 for a mine never deepened). c when c has an open mine; else the partner
    (ascending) with an open mine of the largest depth, then the smaller id; else 0."""
    if depth(c) is not None:
        return c
    best = 0
    for p in sorted(partners):
        d = depth(p)
        if d is not None and (best == 0 or d > depth(best)):
            best = p
    return best


def eligible(alive, visible, Y, last_made, rest, made, world_max, quiet, quiet_left):
    """6.3 eligibility (pure; no draw when it fails)."""
    return (alive and visible and Y - last_made >= rest and made < world_max and (not quiet or quiet_left > 0))


def borne_sum(kinds_deep, field, deep_bonus, cap):
    """7.1: min(cap, sum of (k[field] + deep_bonus when deep and k[field] > 0)) over the borne artifacts given as
    (kind record, deep)."""
    s = 0
    for k, deep in kinds_deep:
        v = k[field]
        s += v + (deep_bonus if deep and v > 0 else 0)
    return min(cap, s)


def win_chance(A, D, dv, skill_x, skill_y, arms_x, arms_y):
    """7.2: A' = A (1000 + skill(Gx) + battle(Gx)) div 1000; D' = D dv div 1000 (1000 + skill(Gy) + battle(Gy)) div
    1000; p = A' 1000 div (A' + D'), 500 at 0."""
    A1 = A * (1000 + skill_x + arms_x) // 1000
    D1 = D * dv // 1000 * (1000 + skill_y + arms_y) // 1000
    return A1 * 1000 // (A1 + D1) if A1 + D1 > 0 else 500


def strain_rate(rate, claim):
    """7.2: rate x (1000 + claim_permille(a, b)) div 1000."""
    return rate * (1000 + claim) // 1000


def hunt_values(hunt_permille, hunt_max_permille, hunt_raid_permille, R, H, prowess, hero_hunt, hero_might,
                borne_hunt, motive):
    """7.2 (P5-06 7.7 amended): (p_hunt, S_c)."""
    p = min(hunt_max_permille + hero_hunt + motive,
            hunt_permille + (max(1, R) - 1) * hunt_raid_permille + hero_hunt + motive)
    S = H * prowess // 1000 * (1000 + hero_might + borne_hunt) // 1000
    return p, S


# ---------------------------------------------------------------------------
# Data
# ---------------------------------------------------------------------------

def tuning_of(dat):
    """history.artifacts of the data, or the code defaults of 4.3 (occasions empty, the others their starts)."""
    t = getattr(dat, "artifacts", None)
    if t is not None:
        return t
    return pdr.artifacts_code_defaults_031(pdr.artifacts_spec_history())


def kinds_of(dat):
    """[(id, key, record)] in canonical id order (4.1)."""
    return [(i + 1, k, d) for i, (k, d) in enumerate(getattr(dat, "artifact_kinds", []) or [])]


def artifact_kind_rows(dat):
    """9: table artifact_kind (id, key, title, noun, class, battle_permille, hunt_permille, claim_permille)."""
    return [(i, k, d["title"], d["noun"], CLASSES.index(d["class"]) + 1, d["battle_permille"], d["hunt_permille"],
             d["claim_permille"]) for i, k, d in kinds_of(dat)]


def occasion_messages(occ, catalogue):
    """4.4 checks artifacts_occasion_* (each `occasions[<i>]: ...`), in order, on a compiled catalogue by key."""
    out = []
    for i, o in enumerate(occ):
        pre = "occasions[%d]: " % i
        k = catalogue.get(o["kind"])
        if k is None:
            out.append(pre + "kind %s is unknown" % o["kind"])
            continue
        roles = {r["role"]: r for r in k["roles"]}
        if o["role"] not in roles or roles[o["role"]]["type"] != CIV:
            out.append(pre + "role %s of %s is not a civilization role" % (o["role"], o["kind"]))
        if o["bearer"] == "general" and o["kind"] != "war_declared":
            out.append(pre + "bearer general needs war_declared")
        for b, typ, word in (("figure", FIGURE, "figure"), ("site", SITE, "site")):
            if o["bearer"] == b and not any(r["type"] == typ for r in k["roles"]):
                out.append(pre + "bearer %s needs a %s role in %s" % (b, word, o["kind"]))
        if all(o["classes"][c] == 0 for c in CLASSES):
            out.append(pre + "the class weights are all 0")
    return out


# ---------------------------------------------------------------------------
# The subsystem (5, 6) and its ArtifactView (7.1), on a conflict World w (ids only in its members)
# ---------------------------------------------------------------------------

class Artifacts:
    def __init__(self, w, dat, inp):
        self.w = w
        self.dat = dat
        self.inp = inp
        self.t = tuning_of(dat)
        self.kinds = {i: dict(d, id=i, key=k, cls=CLASSES.index(d["class"]) + 1) for i, k, d in kinds_of(dat)}
        for s in STREAMS:
            w.keys[s] = cr.stream_key(inp.seed, s)
        root = getattr(dat, "root", None) or getattr(getattr(dat, "base", None), "root", None) or "game/data"
        self.cul = {k: c["naming"] for k, c in pdr.load_root(root, kinds=("culture",))["culture"]}
        self.arts = []         # records, id = index + 1 (5)
        self.last_made = {}    # c -> year
        self.seat = {}         # c -> settlement (6.2)
        self.cursor = 1
        self.log = []          # (year, what, ...) for the checks (not state)

    # --- helpers
    def D(self, s, a, b, k):
        return self.w.draw(s, a, b, k)

    def quiet(self):
        return self.w.quiet_used is not None

    def kind(self, a):
        return self.kinds[a["kind"]]

    def cls(self, a):
        return self.kind(a)["cls"]

    def held(self, state, holder):
        return [a for a in self.arts if a["h"] and a["h"][-1]["state"] == state and a["h"][-1]["holder"] == holder]

    def bears_class(self, f, cls):
        return any(self.cls(a) == cls for a in self.held(BORNE, f))

    def fig_alive(self, f):
        fv = self.w.figures
        return fv is not None and fv.figure_exists(f) and fv.alive(f)

    def civ_alive(self, c):
        return 1 <= c <= len(self.w.civs) and self.w.civ(c)["ended_year"] is None

    def lair_of(self, m):
        return self.w.monsters.monsters[m - 1]["lair"]

    def lair_region(self, m):
        return self.w.monsters.lair_region(self.w.monsters.monsters[m - 1])

    def capital_region(self, c):
        cv = self.w.civ(c)
        return self.w.stl(cv["capital"])["region"] if cv["capital"] else cv["home_region"]

    def seed_cell(self, r):
        g = self.w.geo.reg[r]
        return g["seed_x"], g["seed_y"]

    def forge_depth(self, c):
        return self.w.forge_depth(c) if self.w.alliances is not None else 0

    def partners(self, c):
        al = self.w.alliances
        return [p for p, _e in al.partners(c)] if al is not None else []

    def forge(self, c):
        al = self.w.alliances
        return forge_of(c, lambda x: self.forge_depth(x) if al is not None and al.forged(x) else None,
                        self.partners(c))

    def caster_site(self, f):
        fv = self.w.figures
        return fv.caster_site(f) if fv is not None and f else 0

    # --- the ArtifactView (7.1)
    def _borne(self, f):
        return [(self.kind(a), a["deep"]) for a in self.held(BORNE, f)]

    def battle_permille(self, f):
        t = self.t
        return borne_sum(self._borne(f), "battle_permille", t["deep_metal_permille"], t["force_max_permille"])

    def battle_artifact(self, f):
        xs = [a for a in self.held(BORNE, f) if self.kind(a)["battle_permille"] > 0]
        return xs[0]["made_event"] if xs else 0

    def hunt_permille(self, f):
        t = self.t
        return borne_sum(self._borne(f), "hunt_permille", t["deep_metal_permille"], t["hunt_max_permille"])

    def _claimed(self, h, b):
        if h == b or not self.civ_alive(h) or not self.civ_alive(b):
            return []
        fv = self.w.figures
        out = []
        for a in self.arts:
            if a["civ"] != h:
                continue
            o = a["h"][-1]
            if o["state"] == KEPT and o["holder"] == b:
                out.append(a)
            elif o["state"] == BORNE and fv is not None and fv.fig(o["holder"])["civ"] == b and \
                    self.fig_alive(o["holder"]):
                out.append(a)
        return out

    def claim_permille(self, h, b):
        return min(self.t["claim_max_permille"], sum(self.kind(a)["claim_permille"] for a in self._claimed(h, b)))

    def claim_event(self, h, b):
        if self.claim_permille(h, b) <= 0:
            return 0
        return max(a["h"][-1]["event"] for a in self._claimed(h, b))

    def hoard_motive_permille(self, m, c):
        fv = self.w.figures
        for a in self.held(HOARDED, m):
            prev = a["h"][-2] if len(a["h"]) >= 2 else None
            if a["civ"] == c or (prev is not None and (
                    (prev["state"] == KEPT and prev["holder"] == c) or
                    (prev["state"] == BORNE and fv is not None and fv.fig(prev["holder"])["civ"] == c))):
                return self.t["hoard_hunt_permille"]
        return 0

    # --- 6.1 the step (T3, T4, T6, Q4)
    def step(self):
        w = self.w
        quiet = self.quiet()
        last = len(w.events)
        first = self.cursor
        self.cursor = last + 1
        for e in range(first, last + 1):
            ev = w.events[e - 1]
            if ev["key"] in KIND_ID:
                continue
            self.moves(e, ev, quiet)
            self.making(e, ev, quiet)
        for cv in w.civs:   # 6.2: the seat refreshed for every alive civilization ascending
            if cv["ended_year"] is None and cv["capital"]:
                self.seat[cv["id"]] = cv["capital"]

    # --- holdings (6.2)
    def move(self, a, state, htype, holder, place, e):
        """Closes a's open holding and opens (state, holder) with event e; place (region, settlement, site, x, y)."""
        y = self.w.events[e - 1]["year"]   # reading A-R4
        o = a["h"][-1]
        o["to_year"], o["until"] = y, e
        r, s, si, x, yy = place
        a["h"].append(dict(state=state, htype=htype, holder=holder, region=r, settlement=s, site=si, x=x, y=yy,
                           from_year=y, to_year=None, event=e, until=0))

    def emit(self, key, charged, quiet, trigger, region, settlement, participants, values=()):
        """6.7, 6.8: the artifact event (its one cause the trigger), or the trigger itself for a quiet move when the
        charged civilization has no quiet budget left."""
        w = self.w
        if quiet and w.quiet_events_left(charged) <= 0:
            return trigger
        if settlement:   # lead ruling 2026-11-28 (reading A-R20): an artifact event naming a settlement takes its region
            region = w.stl(settlement)["region"]
        return w.append(key, region=region, settlement=settlement, participants=participants, values=values,
                        causes=(trigger,))

    def p_none(self):
        return (0, 0, 0, 0, 0)

    def p_region(self, r):
        x, y = self.seed_cell(r)
        return (r, 0, 0, x, y)

    def p_ruin(self, t):
        s = self.w.stl(t)
        return (s["region"], t, 0, s["x"], s["y"])

    def p_site(self, sid, with_settlement):
        s = self.w.monsters.site(sid)
        return (s["region"], s["settlement"] if with_settlement else 0, sid, s["x"], s["y"])

    def p_grave(self, f):
        fr = self.w.figures.fig(f)
        r, t = fr["death_region"], fr["death_settlement"]
        if t:
            s = self.w.stl(t)
            return (r, t, 0, s["x"], s["y"])
        x, y = self.seed_cell(r)
        return (r, 0, 0, x, y)

    # --- 6.7 moves
    def moves(self, e, ev, quiet):
        k = ev["key"]
        if k not in MOVE_KEYS:
            return
        w = self.w

        def P(role):
            return _one(w, ev, role)

        A = ("artifact", ARTIFACT)
        if k == "general_appointed":   # M1
            f, c = P("figure"), P("civilization")
            for cls in (ARMS, ARMOUR):
                if not self.fig_alive(f) or self.bears_class(f, cls):
                    continue
                xs = [a for a in self.held(KEPT, c) if self.cls(a) == cls]
                if xs:
                    self.move(xs[0], BORNE, FIGURE, f, self.p_none(), e)
                    self.log.append((w.year, "M1", xs[0]["id"], f, e))
        elif k == "figure_died":   # M2
            f = P("figure")
            fr = w.figures.fig(f)
            c2 = fr["civ"]
            for a in self.held(BORNE, f):
                cs = self.caster_site(f) if self.cls(a) == STAFF and a["maker"] == f else 0
                if cs:
                    self.move(a, ENSHRINED, SITE, cs, self.p_site(cs, True), e)
                    self.log.append((w.year, "M2 enshrined", a["id"], cs, e))
                    continue
                cv = w.civ(c2)
                if cv["ended_year"] is None and w.visible(cv):
                    if not pp.chance_pm(self.D("hist.artifacts.bury", a["id"], f, 0), self.t["burial_permille"]):
                        self.move(a, KEPT, CIV, c2, self.p_none(), e)
                        self.log.append((w.year, "M2 kept", a["id"], c2, e))
                        continue
                g = self.p_grave(f)
                x = self.emit("artifact_buried", c2, quiet, e, g[0], g[1],
                              (A + (a["id"],), ("figure", FIGURE, f), ("civilization", CIV, c2)))
                self.move(a, BURIED, FIGURE, f, g, x)
                self.log.append((w.year, "M2 buried", a["id"], f, x))
        elif k == "fell_in_battle":   # M3
            f, c, xx = P("figure"), P("civilization"), P("enemy")
            for a in self.held(BORNE, f):
                if self.civ_alive(xx) and pp.chance_pm(self.D("hist.artifacts.take", e, a["id"], 0),
                                                       self.t["battle_take_permille"]):
                    y = self.emit("artifact_taken", xx, quiet, e, ev["region"], ev["settlement"],
                                  (A + (a["id"],), ("taker", CIV, xx), ("from", CIV, c), ("figure", FIGURE, f)), (1,))
                    self.move(a, KEPT, CIV, xx, self.p_none(), y)
                    self.log.append((w.year, "M3 taken", a["id"], xx, y))
                else:
                    pl = self.p_region(ev["region"])
                    y = self.emit("artifact_lost", c, quiet, e, pl[0], 0,
                                  (A + (a["id"],), ("civilization", CIV, c), ("figure", FIGURE, f)))
                    self.move(a, LOST, REGION, ev["region"], pl, y)
                    self.log.append((w.year, "M3 lost", a["id"], ev["region"], y))
        elif k == "hero_slain":   # M4
            f, c, m = P("figure"), P("civilization"), P("monster")
            for a in self.held(BORNE, f):
                y = self.emit("artifact_hoarded", c, quiet, e, self.lair_region(m), 0,
                              (A + (a["id"],), ("monster", MONSTER, m), ("civilization", CIV, c),
                               ("figure", FIGURE, f)), (1,))
                self.move(a, HOARDED, MONSTER, m, self.p_none(), y)
                self.log.append((w.year, "M4", a["id"], m, y))
        elif k in ("monster_slain_by_hero", "monster_slain"):   # M5
            m, c = P("monster"), P("slayer")
            h = P("hero") if k == "monster_slain_by_hero" else 0
            for a in self.held(HOARDED, m):
                parts = (A + (a["id"],), ("civilization", CIV, c)) + ((("figure", FIGURE, h),) if h else ()) + \
                    (("monster", MONSTER, m),)
                y = self.emit("artifact_recovered", c, quiet, e, self.lair_region(m), 0, parts, (1,))
                if h and self.cls(a) in (ARMS, ARMOUR) and self.fig_alive(h) and not self.bears_class(h, self.cls(a)):
                    self.move(a, BORNE, FIGURE, h, self.p_none(), y)
                else:
                    self.move(a, KEPT, CIV, c, self.p_none(), y)
                self.log.append((w.year, "M5", a["id"], m, h, y))
        elif k == "monster_died":   # M6
            m = P("monster")
            for a in self.held(HOARDED, m):
                sid = self.lair_of(m)
                if sid:
                    self.move(a, LOST, SITE, sid, self.p_site(sid, False), e)
                else:   # A-R21
                    self.move(a, LOST, REGION, ev["region"], self.p_region(ev["region"]), e)
                self.log.append((w.year, "M6", a["id"], sid, e))
        elif k == "monster_raid":   # M7
            m, c = P("monster"), P("civilization")
            t = ev["settlement"]
            if t and t == self.seat.get(c, 0):
                xs = self.held(KEPT, c)
                if xs and pp.chance_pm(self.D("hist.artifacts.raid", e, 0, 0), self.t["raid_take_permille"]):
                    a = xs[0]
                    y = self.emit("artifact_hoarded", c, quiet, e, self.lair_region(m), t,
                                  (A + (a["id"],), ("monster", MONSTER, m), ("civilization", CIV, c)), (2,))
                    self.move(a, HOARDED, MONSTER, m, self.p_none(), y)
                    self.log.append((w.year, "M7", a["id"], m, y))
        elif k == "hold_lost":   # M8
            m, c = P("monster"), P("civilization")
            t = ev["settlement"]
            if t and t == self.seat.get(c, 0):
                for a in self.held(KEPT, c):
                    y = self.emit("artifact_hoarded", c, quiet, e, self.lair_region(m), t,
                                  (A + (a["id"],), ("monster", MONSTER, m), ("civilization", CIV, c)), (3,))
                    self.move(a, HOARDED, MONSTER, m, self.p_none(), y)
                    self.log.append((w.year, "M8", a["id"], m, y))
        elif k == "sacked":   # M9
            xx, d = P("attacker"), P("defender")
            t = ev["settlement"]
            if t and t == self.seat.get(d, 0):
                for a in self.held(KEPT, d):
                    if self.civ_alive(xx) and pp.chance_pm(self.D("hist.artifacts.take", e, a["id"], 0),
                                                           self.t["sack_take_permille"]):
                        y = self.emit("artifact_taken", xx, quiet, e, ev["region"], ev["settlement"],
                                      (A + (a["id"],), ("taker", CIV, xx), ("from", CIV, d)), (2,))
                        self.move(a, KEPT, CIV, xx, self.p_none(), y)
                        self.log.append((w.year, "M9 taken", a["id"], xx, y))
                    elif not w.standing(w.stl(t)):
                        pl = self.p_ruin(t)
                        y = self.emit("artifact_lost", d, quiet, e, pl[0], t,
                                      (A + (a["id"],), ("civilization", CIV, d)))
                        self.move(a, LOST, SETTLEMENT, t, pl, y)
                        self.log.append((w.year, "M9 lost", a["id"], t, y))
        elif k == "settlement_captured":   # M10
            xx, d = P("attacker"), P("defender")
            t = ev["settlement"]
            if t and t == self.seat.get(d, 0):
                for a in self.held(KEPT, d):
                    if self.civ_alive(xx) and pp.chance_pm(self.D("hist.artifacts.take", e, a["id"], 0),
                                                           self.t["capture_take_permille"]):   # A-R21
                        y = self.emit("artifact_taken", xx, quiet, e, ev["region"], ev["settlement"],
                                      (A + (a["id"],), ("taker", CIV, xx), ("from", CIV, d)), (3,))
                        self.move(a, KEPT, CIV, xx, self.p_none(), y)
                        self.log.append((w.year, "M10", a["id"], xx, y))
        elif k == "civ_conquered":   # M11
            c, kk = P("civilization"), P("conqueror")
            for a in self.held(KEPT, c):
                if self.civ_alive(kk):
                    y = self.emit("artifact_taken", kk, quiet, e, ev["region"], ev["settlement"],
                                  (A + (a["id"],), ("taker", CIV, kk), ("from", CIV, c)), (4,))
                    self.move(a, KEPT, CIV, kk, self.p_none(), y)
                    self.log.append((w.year, "M11", a["id"], kk, y))
                else:
                    self.realm_end_move(a, c, e, ev, quiet)
        elif k == "civ_ended":   # M12
            c = P("civilization")
            for a in self.held(KEPT, c):
                self.realm_end_move(a, c, e, ev, quiet)
        elif k in ("peace", "war_won"):   # M13
            wn, ls = P("winner"), P("loser")
            fv = w.figures
            for a in (list(self.arts) if wn and ls and self.civ_alive(wn) else ()):   # A-R21
                if a["civ"] != wn:
                    continue
                o = a["h"][-1]
                if not ((o["state"] == KEPT and o["holder"] == ls) or
                        (o["state"] == BORNE and fv is not None and fv.fig(o["holder"])["civ"] == ls and
                         self.fig_alive(o["holder"]))):
                    continue
                y = self.emit("artifact_recovered", wn, quiet, e, self.capital_region(wn), 0,
                              (A + (a["id"],), ("civilization", CIV, wn), ("from", CIV, ls)), (2,))
                self.move(a, KEPT, CIV, wn, self.p_none(), y)
                self.log.append((w.year, "M13", a["id"], wn, ls, y))
        elif k == "cult_suppressed":   # M14
            kk, c, s2 = P("faction"), P("civilization"), P("sanctum")
            ended = set()
            for s in w.monsters.sites:
                if s["kind"] == TEMPLE and s.get("cult") == kk and s["ending"] == END_SUPPRESSED and \
                        s["ending_event"] == e:
                    ended.add(s["id"])
            for a in list(self.arts):
                o = a["h"][-1]
                if o["state"] != ENSHRINED or o["holder"] not in ended:
                    continue
                if s2:
                    self.move(a, ENSHRINED, SITE, s2, self.p_site(s2, True), e)
                else:
                    self.move(a, KEPT, CIV, c, self.p_none(), e)
                self.log.append((w.year, "M14", a["id"], s2, e))

    def realm_end_move(self, a, c, e, ev, quiet):
        """M12 (and M11 with an ended conqueror): by seat(c) as before the step's reads."""
        w = self.w
        A = ("artifact", ARTIFACT)
        t = self.seat.get(c, 0)
        if t and not w.standing(w.stl(t)):
            pl = self.p_ruin(t)
            y = self.emit("artifact_lost", c, quiet, e, pl[0], t, (A + (a["id"],), ("civilization", CIV, c)))
            self.move(a, LOST, SETTLEMENT, t, pl, y)
            self.log.append((w.year, "M12 ruin", a["id"], t, y))
            return
        o = w.stl(t)["owner_civ"] if t else 0
        if t and o and o != c and self.civ_alive(o):
            y = self.emit("artifact_taken", o, quiet, e, ev["region"], ev["settlement"],
                          (A + (a["id"],), ("taker", CIV, o), ("from", CIV, c)), (4,))
            self.move(a, KEPT, CIV, o, self.p_none(), y)
            self.log.append((w.year, "M12 owner", a["id"], o, y))
            return
        if t:   # reading A-R8 (the engine's): a standing seat with no living other owner keeps it as a ruin would
            pl = self.p_ruin(t)
            y = self.emit("artifact_lost", c, quiet, e, pl[0], t, (A + (a["id"],), ("civilization", CIV, c)))
            self.move(a, LOST, SETTLEMENT, t, pl, y)
            self.log.append((w.year, "M12 seat", a["id"], t, y))
            return
        r = w.civ(c)["home_region"]   # t = 0
        pl = self.p_region(r)
        y = self.emit("artifact_lost", c, quiet, e, r, 0, (A + (a["id"],), ("civilization", CIV, c)))
        self.move(a, LOST, REGION, r, pl, y)
        self.log.append((w.year, "M12 home", a["id"], r, y))

    # --- 6.3 making
    def making(self, e, ev, quiet):
        w, t = self.w, self.t
        occs = [o for o in t["occasions"] if o["kind"] == ev["key"]]
        if not occs:
            return
        Y = w.year
        for o in occs:
            seq = _seq(w, ev["key"], o["role"])
            for (_j, s, _ty, c) in ev["participants"]:
                if s != seq or c == 0:
                    continue
                cv = w.civ(c)
                if not eligible(cv["ended_year"] is None, w.visible(cv), Y, self.last_made.get(c, NEVER),
                                t["realm_rest_years"], len(self.arts), t["world_max"], quiet,
                                w.quiet_events_left(c) if quiet else 1):
                    continue
                F = self.forge(c)
                p = make_permille(o["chance_permille"], F, t["forge_chance_permille"])
                if not pp.chance_pm(self.D("hist.artifacts.make", e, c, 0), p):
                    continue
                G = first_of(ev, GOD)
                caster = w.figures.caster_of(c) if w.figures is not None else 0
                csite = self.caster_site(caster) if caster else 0
                W = class_weights(o["classes"], G != 0, csite != 0)
                if all(x == 0 for x in W):
                    continue
                cls = pnr.weighted(self.D("hist.artifacts.class", e, c, 0), W) + 1
                a_id = len(self.arts) + 1
                K = [k for _i, k in sorted(self.kinds.items()) if k["cls"] == cls]
                if not K:
                    continue
                if a_id > ARTIFACTS_MAX:
                    _pcr().fatal("history: bound: artifacts exceed 100000")
                kd = K[pnr.weighted(self.D("hist.artifacts.kind", a_id, 0, 0), [k["weight"] for k in K])]
                self.make(e, ev, o, c, F, G, caster, csite, cls, a_id, kd)
                return

    def make(self, e, ev, o, c, F, G, caster, csite, cls, a_id, kd):
        w, t = self.w, self.t
        esite = first_of(ev, SITE)
        es = w.monsters.site(esite) if esite and w.monsters is not None else None
        temple = esite if es is not None and es["kind"] == TEMPLE and es["ending"] == 0 else 0
        deep = 0
        if G and cls != STAFF:
            mk, maker, god, mciv, msite, ncul = GOD_GIVEN, 0, G, 0, temple, c
        elif cls == STAFF:
            mk, maker, god, mciv, msite, ncul = CASTER_MADE, caster, 0, c, csite, c
        elif F:
            mk = SMITH_MADE if F == c else COMMISSION
            maker, god, mciv, msite, ncul = w.figures.next_figure_id(), 0, F, w.alliances.forge_site(F), F
            deep = 1 if self.forge_depth(F) >= t["deep_metal_depth"] else 0
        else:
            mk, maker, god, mciv, msite, ncul = SMITH_MADE, w.figures.next_figure_id(), 0, c, 0, c
        # 6.6 the name, before the event
        cnum = w.civ(ncul)["culture"]
        ck = w.culture[cnum][1]
        forms = [x.replace("{title}", kd["title"]) for x in self.cul[ck]["artifact_forms"]]
        d = pnr.with_forms(w.dat.naming[ck], "artifact", forms)
        name, _try = pnr.make_name(d, ck, "artifact", self.inp.seed, cnum, a_id, w.registry)
        # 8.1: region and settlement, participants, values, causes
        if msite:
            ms = w.monsters.site(msite)
            region, settlement = ms["region"], ms["settlement"]
            if settlement:   # reading A-R20: the settlement's region
                region = w.stl(settlement)["region"]
        else:
            region, settlement = self.capital_region(c), 0
        parts = [("artifact", ARTIFACT, a_id), ("civilization", CIV, c)]
        if mk in (SMITH_MADE, COMMISSION, CASTER_MADE):
            parts.append(("maker", FIGURE, maker))
        if mk == GOD_GIVEN:
            parts.append(("god", GOD, god))
        if mk == COMMISSION:
            parts.append(("maker_realm", CIV, mciv))
        if msite:
            parts.append(("site", SITE, msite))
        causes = {e}
        if deep:
            dl = w.monsters.deep.last_delve.get(msite, 0) if getattr(w.monsters, "deep", None) else 0
            if dl:
                causes.add(dl)
        x = w.append("artifact_made", region=region, settlement=settlement, participants=tuple(parts),
                     values=(mk, deep), causes=tuple(sorted(causes)))
        if mk in (SMITH_MADE, COMMISSION):
            w.figures.raise_maker(maker, mciv, x)
        a = dict(id=a_id, name=name, kind=kd["id"], civ=c, maker_kind=mk, maker=maker, god=god, maker_civ=mciv,
                 maker_site=msite, made_year=w.year, made_event=x, occasion_event=e, deep=deep, h=[])
        self.arts.append(a)
        # 6.5 the first holding
        st = None
        if cls == REGALIA:
            st = (KEPT, CIV, c, self.p_none())
        elif cls == STAFF:
            if not self.bears_class(caster, STAFF):   # reading A-R2
                st = (BORNE, FIGURE, caster, self.p_none())
        elif cls == RELIC:
            if temple:
                st = (ENSHRINED, SITE, temple, self.p_site(temple, True))
        elif o["bearer"] == "general":
            gf = w.figures.general_of(e, c)[0] if w.figures is not None else 0
            if gf and self.fig_alive(gf) and not self.bears_class(gf, cls):
                st = (BORNE, FIGURE, gf, self.p_none())
        elif o["bearer"] == "figure":
            f = first_of(ev, FIGURE)
            if f and self.fig_alive(f) and w.figures.fig(f)["civ"] == c and not self.bears_class(f, cls):
                st = (BORNE, FIGURE, f, self.p_none())
        if st is None:
            st = (KEPT, CIV, c, self.p_none())
        r, s, si, xx, yy = st[3]
        a["h"].append(dict(state=st[0], htype=st[1], holder=st[2], region=r, settlement=s, site=si, x=xx, y=yy,
                           from_year=w.year, to_year=None, event=x, until=0))
        self.last_made[c] = w.year
        self.log.append((w.year, "made", a_id, c, mk, cls, kd["key"], deep, x, st[0], o["kind"], o["role"]))

    # --- rows (9)
    def rows(self):
        arts = [(a["id"], a["name"], a["kind"], a["civ"], a["maker_kind"], a["maker"], a["god"], a["maker_civ"],
                 a["maker_site"], a["made_year"], a["made_event"], a["occasion_event"], a["deep"], a["h"][-1]["state"])
                for a in self.arts]
        hold = [(a["id"], i + 1, h["from_year"], h["to_year"], h["state"], h["htype"], h["holder"], h["region"],
                 h["settlement"], h["site"], h["x"], h["y"], h["event"], h["until"])
                for a in self.arts for i, h in enumerate(a["h"])]
        return arts, hold


def _seq(w, key, role):
    return [q["seq"] for q in w.cat_by_key[key]["roles"] if q["role"] == role][0]


def _one(w, ev, role):
    seq = [q["seq"] for q in w.cat_by_key[ev["key"]]["roles"] if q["role"] == role]
    if not seq:
        return 0
    xs = [x for (_i, s, _t, x) in ev["participants"] if s == seq[0]]
    return xs[0] if xs else 0


def first_of(ev, typ):
    """The first participant of entity type typ by seq (6.3), 0 none."""
    for (_i, _s, t, x) in sorted(ev["participants"]):
        if t == typ:
            return x
    return 0


# ---------------------------------------------------------------------------
# Rows: indexes, the invariants of 13, the reader checks of 9, the statistics of 10 item 6, the v12 blocks of 10 and
# the timeline reads of 11.1 (over a replay's res or a world file's tables)
# ---------------------------------------------------------------------------

class R:
    def __init__(self, res):
        self.res = res
        self.cat = {k[0]: k for k in res.get("event_kind", [])}
        self.kid = {k[1]: k[0] for k in res.get("event_kind", [])}
        self.role_seq = {}
        for k, seq, role, _typ, _lo, _hi in res.get("event_kind_role", []):
            self.role_seq.setdefault(k, {})[role] = seq
        self.val_seq = {}
        for k, seq, key in res.get("event_kind_value", []):
            self.val_seq.setdefault(k, {})[key] = seq
        self.ev = {e[0]: e for e in res.get("event", [])}
        self.parts = {}
        for p in res.get("event_participant", []):
            self.parts.setdefault(p[0], []).append(p)
        self.causes = {}
        for e, c in res.get("event_cause", []):
            self.causes.setdefault(e, []).append(c)
        self.vals = {}
        for e, s, v in res.get("event_value", []):
            self.vals.setdefault(e, {})[s] = v
        self.civ = {c[0]: c for c in res.get("civilization", [])}
        self.stl = {s[0]: s for s in res.get("settlement", [])}
        self.site = {s[0]: s for s in res.get("site", [])}
        self.fig = {f[0]: f for f in res.get("figure", [])}
        self.mon = {m[0]: m for m in res.get("monster", [])}
        self.god = {g[0]: g for g in res.get("god", [])}
        self.reg = {r[0]: r for r in res.get("region", [])}
        self.phase = {p[0]: p for p in res.get("phase", [])}
        self.kinds = {k[0]: k for k in res.get("artifact_kind", [])}
        self.arts = {a[0]: a for a in res.get("artifact", [])}
        self.hold = {}
        for h in res.get("artifact_holding", []):
            self.hold.setdefault(h[0], []).append(h)
        for a in self.hold:
            self.hold[a].sort(key=lambda h: h[1])
        self.rel = list(res.get("relation", []))

    def key(self, e):
        return self.cat[self.ev[e][3]][1] if e in self.ev else ""

    def role(self, e, role):
        seq = self.role_seq.get(self.ev[e][3], {}).get(role) if e in self.ev else None
        return [p[4] for p in sorted(self.parts.get(e, [])) if p[2] == seq] if seq is not None else []

    def one(self, e, role):
        xs = self.role(e, role)
        return xs[0] if xs else 0

    def val(self, e, key):
        if e not in self.ev:
            return None
        s = self.val_seq.get(self.ev[e][3], {}).get(key)
        return self.vals.get(e, {}).get(s) if s is not None else None

    def of(self, key):
        k = self.kid.get(key)
        return sorted(e for e, r in self.ev.items() if r[3] == k)

    def year(self, e):
        return self.ev[e][1] if e in self.ev else None

    def quiet(self, e):
        return self.phase.get(self.ev[e][4], (0, 0, 0))[2] == pe.QUIET if e in self.ev else False

    def cause_list(self, e):
        return sorted(self.causes.get(e, []))

    def ended(self, c):
        cv = self.civ.get(c)
        return cv is not None and cv[8] is not None

    def kind_cls(self, a):
        return self.kinds[self.arts[a][2]][4]

    def open_holding(self, a):
        hs = self.hold.get(a, [])
        return hs[-1] if hs else None

    def holding_at_event(self, a, x):
        """The holding of a in force just before event x is applied (opened before x, not closed before x)."""
        for h in self.hold.get(a, []):
            if h[12] < x and (h[13] == 0 or h[13] >= x):
                return h
        return None

    def fig_name(self, f):
        r = self.fig.get(f)
        return pe.figure_display_name(r[1], r[16], r[2]) if r is not None else "?"

    def region_name_at_end(self, rid, E):
        rows = self.res.get("region_name", [])
        base = {r[0]: r[1] for r in self.res.get("region", [])}
        if rid not in base:
            return "?"
        return pe.region_at_year(base, rows, rid, E)


def mine_depth_at(r, sid, x):
    """P5-07 9 from rows: the largest value depth of the delving_deepened of site sid before event x, 0 when none (an
    undeepened mine has depth 0; it is still a forge, reading A-R19)."""
    d = 0
    for e in r.of("delving_deepened"):
        if e < x and r.one(e, "site") == sid:
            d = max(d, r.val(e, "depth") or d)
    return d


def open_mines_at(r, c, x, lenient=False):
    """The forges of c at event x: its mines (site kind 3, founder c) open at x (opened before it, not ended before it)
    at any depth (reading A-R19, the lead's ruling on S4). A mine whose settlement or region changed
    hands is closed by the alliances' next step with the change's event as ending_event, so between the change and
    that step the rows may still show it open: a forge the replay no longer counts (rows check only)."""
    out = []
    y = r.year(x)
    for s in r.site.values():
        if s[2] != MINE or s[7] != c or s[10] >= x:
            continue
        # strict: ended after x; lenient: ended in x's year or later (the closing step may follow the making)
        if s[12] == 0 or s[12] > x or (lenient and s[11] is not None and s[11] >= y):
            out.append(s[0])
    return sorted(out)


def partners_at(r, c, x):
    out = set()
    for q in r.rel:
        if q[4] not in ("alliance", "trade") or q[0] != CIV or q[2] != CIV or c not in (q[1], q[3]):
            continue
        if q[7] < x and (q[8] == 0 or q[8] > x):
            out.add(q[3] if q[1] == c else q[1])
    return sorted(out)


def violations(res, tuning):
    """13 items 1 to 6, 7 (the declaration's cause, from rows) and 8 on rows; the win chances of item 7 are the
    replay's; item 9 is the earlier references'."""
    r = R(res)
    out = []
    t = tuning
    occ = {(o["kind"], o["role"]) for o in t["occasions"]}
    made_for = {}
    out += reader_problems(res)   # 1: 9's checks
    arts = sorted(r.arts.values())
    if len(arts) > t["world_max"]:
        out.append("2: %d artifacts above world_max %d" % (len(arts), t["world_max"]))
    seen_occ = {}
    for a in arts:
        aid, me, oc = a[0], a[10], a[11]
        hs = r.hold.get(aid, [])
        pre = "artifact %d: " % aid
        # 1: exactly one open holding at E; no open holding on an ended realm, a dead figure or a dead monster
        if sum(1 for h in hs if h[3] is None) != 1:
            out.append(pre + "1: open holdings %d" % sum(1 for h in hs if h[3] is None))
        for h in hs:
            st, hd = h[4], h[6]
            end = None
            if st == KEPT and hd in r.civ and r.civ[hd][8] is not None:
                end = r.civ[hd][8]
            if st == BORNE and hd in r.fig and r.fig[hd][10] is not None:
                end = r.fig[hd][10]
            if st == HOARDED and hd in r.mon and r.mon[hd][6] is not None:
                end = r.mon[hd][6]
            if end is not None and h[2] <= end and (h[3] is None or h[3] > end):
                out.append(pre + "1: holding %d (%s by %d) does not close in its holder's end year %d" % (
                    h[1], STATE_WORDS[st], hd, end))
        # 2: the occasion
        ok = r.key(oc)
        c = a[3]
        if not any((ok, role) in occ and c in r.role(oc, role)
                   for role in r.role_seq.get(r.ev[oc][3] if oc in r.ev else 0, {})):
            out.append(pre + "2: occasion e%d (%s) matches no occasion with c%d in its role" % (oc, ok, c))
        seen_occ.setdefault(oc, []).append(aid)
        made_for.setdefault(c, []).append(a[9])
        # 3: makers
        mk = a[4]
        if mk in (SMITH_MADE, COMMISSION):
            f = r.fig.get(a[5])
            if f is None or f[3] != SMITH or f[9] != me or f[6] != a[7]:
                out.append(pre + "3: maker %d is not a smith of c%d created by e%d" % (a[5], a[7], me))
            if mk == COMMISSION:
                if open_mines_at(r, c, me):
                    out.append(pre + "3: a commission while c%d had an open mine" % c)
                if a[7] not in partners_at(r, c, me) or not open_mines_at(r, a[7], me, lenient=True):
                    out.append(pre + "3: commission by c%d, not a partner with an open mine" % a[7])
            dl = max([mine_depth_at(r, s, me) for s in open_mines_at(r, a[7], me, lenient=True)] or [0])
            ds = max([mine_depth_at(r, s, me) for s in open_mines_at(r, a[7], me)] or [0])
            if (a[12] == 1 and dl < t["deep_metal_depth"]) or (a[12] == 0 and ds >= t["deep_metal_depth"]):
                out.append(pre + "3: deep %d against the forge's depth %d" % (a[12], dl))
        if mk == CASTER_MADE:
            f = r.fig.get(a[5])
            s = r.site.get(a[8])
            if f is None or f[3] != CASTER or f[6] != c or s is None or s[2] != TOWER or not (
                    s[10] < me and (s[12] == 0 or s[12] > me)):
                out.append(pre + "3: caster %d of c%d without an open tower %d" % (a[5], c, a[8]))
        if mk == GOD_GIVEN and a[6] not in r.role(oc, "actor") + r.role(oc, "god"):
            out.append(pre + "3: god %d not named by its occasion e%d" % (a[6], oc))
        # 4, 5, 6: per holding
        prev = None
        for h in hs:
            st = h[4]
            cls = r.kind_cls(aid)
            if st == BORNE and cls not in (ARMS, ARMOUR, STAFF):
                out.append(pre + "4: a %s borne" % CLASSES[cls - 1])
            x = h[12]
            xk = r.key(x)
            if h[1] == 1:
                if x != me:
                    out.append(pre + "5: holding 1 opened by e%d" % x)
            else:
                if xk in KIND_ID and xk != "artifact_made":
                    if r.one(x, "artifact") != aid:
                        out.append(pre + "5: holding %d opened by e%d naming another artifact" % (h[1], x))
                    trig = r.cause_list(x)
                    trig_k = r.key(trig[0]) if trig else ""
                else:
                    trig_k = xk
                    if xk not in QUIET_MOVE_KEYS and not (r.quiet(x) and xk in MOVE_KEYS):
                        out.append(pre + "5: holding %d opened by %s e%d, no quiet move" % (h[1], xk, x))
                mv = move_matches(r, prev, h, trig_k, xk)
                if mv:
                    out.append(pre + "5: holding %d: %s" % (h[1], mv))
                if prev is not None and prev[4] in (LOST, BURIED):
                    out.append(pre + "6: left state %d" % prev[4])
                if prev is not None and prev[4] == ENSHRINED and trig_k != "cult_suppressed":
                    out.append(pre + "6: left a shrine by %s" % trig_k)
            prev = h
    for oc, xs in seen_occ.items():
        if len(xs) > 1:
            out.append("2: e%d occasions %d artifacts" % (oc, len(xs)))
    for c, ys in made_for.items():
        ys.sort()
        for y0, y1 in zip(ys, ys[1:]):
            if y1 - y0 < t["realm_rest_years"]:
                out.append("2: c%d has artifacts made in %d and %d (rest %d)" % (c, y0, y1, t["realm_rest_years"]))
    # 4: a figure bears at most one of each class at every year's end
    per = {}
    for aid, hs in r.hold.items():
        for h in hs:
            if h[4] == BORNE:
                per.setdefault((h[6], r.kind_cls(aid)), []).append((h[2], h[3]))
    for (f, cls), iv in sorted(per.items()):
        for y in sorted({a0 for a0, _b in iv}):
            n = sum(1 for a0, b in iv if a0 <= y and (b is None or b > y))
            if n > 1:
                out.append("4: f%d bears %d of class %s at the end of %d" % (f, n, CLASSES[cls - 1], y))
                break
    # 7: a war_declared cites claim_event when the claim was above 0 (rows: the holdings before the declaration)
    kclaim = {k[0]: k[7] for k in r.kinds.values()}
    for e in r.of("war_declared"):
        a_, b_ = r.one(e, "attacker"), r.one(e, "defender")
        xs = []
        for aid, a in r.arts.items():
            if a[3] != a_:
                continue
            h = r.holding_at_event(aid, e)
            if h is None:
                continue
            if (h[4] == KEPT and h[6] == b_) or (h[4] == BORNE and r.fig.get(h[6], (0,) * 7)[6] == b_):
                xs.append((kclaim[a[2]], h[12]))
        cl = min(t["claim_max_permille"], sum(v for v, _ in xs))
        if cl > 0 and max(x for _v, x in xs) not in r.cause_list(e):
            out.append("7: war_declared e%d does not cite its claim's event e%d" % (e, max(x for _v, x in xs)))
    # 8: 12.1 the cited acts in force for their side; 12.2 no spread back within cult_return_years
    for key in BATTLE_KEYS:
        for e in r.of(key):
            W, L = r.one(e, "winner"), r.one(e, "loser")
            y = r.year(e)
            for x in r.cause_list(e):
                xk = r.key(x)
                if xk == "divine_bless" and r.val(x, "effect") == 2:
                    if r.one(x, "civilization") != W or not (r.year(x) <= y < r.year(x) + r.val(x, "years")):
                        out.append("8: %s e%d cites valour e%d not in force for its winner" % (key, e, x))
                if xk == "divine_curse" and r.val(x, "effect") == 3:
                    if r.one(x, "civilization") != L or not (r.year(x) <= y < r.year(x) + r.val(x, "years")):
                        out.append("8: %s e%d cites defeat e%d not in force for its loser" % (key, e, x))
    R_ = t.get("_cult_return_years")
    if R_:
        sup = {}
        for e in r.of("cult_suppressed"):
            sup.setdefault((r.one(e, "faction"), r.one(e, "civilization")), []).append((r.year(e), e))
        for e in r.of("cult_spread"):
            k, a_ = r.one(e, "faction"), r.one(e, "civilization")
            y = r.year(e)
            # a suppression read before the spread (an earlier event): 6.8 spreads before 6.9 suppresses in a step
            if any(y - R_ < ys <= y and x < e for ys, x in sup.get((k, a_), ())):
                out.append("8: cult_spread e%d of fa%d into c%d within %d years of its suppression" % (e, k, a_, R_))
    return out


MOVE_STATES = {
    "general_appointed": [(KEPT, BORNE)],
    "figure_died": [(BORNE, ENSHRINED), (BORNE, BURIED), (BORNE, KEPT)],
    "fell_in_battle": [(BORNE, KEPT), (BORNE, LOST)],
    "hero_slain": [(BORNE, HOARDED)],
    "monster_slain_by_hero": [(HOARDED, BORNE), (HOARDED, KEPT)],
    "monster_slain": [(HOARDED, KEPT)],
    "monster_died": [(HOARDED, LOST)],
    "monster_raid": [(KEPT, HOARDED)],
    "hold_lost": [(KEPT, HOARDED)],
    "sacked": [(KEPT, KEPT), (KEPT, LOST)],
    "settlement_captured": [(KEPT, KEPT)],
    "civ_conquered": [(KEPT, KEPT), (KEPT, LOST)],
    "civ_ended": [(KEPT, KEPT), (KEPT, LOST)],
    "peace": [(KEPT, KEPT), (BORNE, KEPT)],
    "war_won": [(KEPT, KEPT), (BORNE, KEPT)],
    "cult_suppressed": [(ENSHRINED, ENSHRINED), (ENSHRINED, KEPT)],
}


def move_matches(r, prev, h, trig_k, xk):
    """5: "the move matches its row" (the trigger's kind against the states before and after, and the bearer or
    keeper it names); None when it does."""
    if prev is None:
        return None
    ok = MOVE_STATES.get(trig_k)
    if ok is None:
        return "trigger %s moves nothing" % trig_k
    if (prev[4], h[4]) not in ok:
        return "%s from %s to %s" % (trig_k, STATE_WORDS[prev[4]], STATE_WORDS[h[4]])
    trig = h[12] if xk not in KIND_ID else r.cause_list(h[12])[0]
    if trig_k in ("hero_slain", "fell_in_battle", "figure_died") and prev[6] != r.one(trig, "figure"):
        return "%s of another bearer" % trig_k
    if trig_k in ("monster_slain_by_hero", "monster_slain", "monster_died") and prev[6] != r.one(trig, "monster"):
        return "%s of another keeper" % trig_k
    return None


def reader_problems(res):
    """9's reader checks on rows, in table order (messages `corrupt table '<t>': row <k>: <reason>`)."""
    r = R(res)
    out = []
    regions = set(r.reg) or None
    W = res.get("_map_size")
    for i, (kid, k) in enumerate(sorted(r.kinds.items())):
        pre = "corrupt table 'artifact_kind': row %d: " % kid
        if kid != i + 1:
            out.append(pre + "ids not dense")
        if not 1 <= k[4] <= 5:
            out.append(pre + "class %d not 1 to 5" % k[4])
        if not 0 <= k[5] <= 500 or (k[5] > 0 and k[4] not in (1, 2)):
            out.append(pre + "battle_permille %d" % k[5])
        if not 0 <= k[6] <= 1000 or (k[6] > 0 and k[4] in (3, 4)):
            out.append(pre + "hunt_permille %d" % k[6])
        if not 0 <= k[7] <= 1000:
            out.append(pre + "claim_permille %d" % k[7])
    for i, (aid, a) in enumerate(sorted(r.arts.items())):
        pre = "corrupt table 'artifact': row %d: " % aid
        if aid != i + 1:
            out.append(pre + "ids not dense")
        if a[2] not in r.kinds:
            out.append(pre + "kind %d is not a row of artifact_kind" % a[2])
        if a[3] not in r.civ:
            out.append(pre + "civ %d is not a civilization" % a[3])
        me = a[10]
        mk = a[4]
        if r.key(me) != "artifact_made" or r.one(me, "artifact") != aid or r.year(me) != a[9]:
            out.append(pre + "made_event %d is not its artifact_made of year %d" % (me, a[9]))
        else:
            want = dict(civilization=a[3], maker=a[5] if mk in (1, 2, 3) else 0, god=a[6] if mk == 4 else 0,
                        maker_realm=a[7] if mk == 2 else 0, site=a[8])
            for role, v in want.items():
                if r.one(me, role) != v:
                    out.append(pre + "made_event %d role %s %d against %d" % (me, role, r.one(me, role), v))
            if r.val(me, "maker_kind") != mk or r.val(me, "deep") != a[12]:
                out.append(pre + "made_event %d values against maker_kind %d deep %d" % (me, mk, a[12]))
            if mk in (1, 3) and r.fig.get(a[5], (0,) * 7)[6] != a[7]:
                out.append(pre + "maker_civ %d is not its maker's civ" % a[7])
            if mk == 4 and (a[5] != 0 or a[7] != 0):
                out.append(pre + "a god-given artifact with a maker")
            if a[11] not in r.cause_list(me) or r.key(a[11]) == "delving_deepened":   # reading A-R1
                out.append(pre + "occasion_event %d is not its occasion" % a[11])
        if not 1 <= mk <= 4:
            out.append(pre + "maker_kind %d not 1 to 4" % mk)
        f = r.fig.get(a[5])
        if mk in (1, 2) and (f is None or f[3] != SMITH or f[9] != me):
            out.append(pre + "maker %d is not a smith made by e%d" % (a[5], me))
        if mk == 3 and (f is None or f[3] != CASTER):
            out.append(pre + "maker %d is not a caster" % a[5])
        if a[12] not in (0, 1) or (a[12] == 1 and mk not in (1, 2)):
            out.append(pre + "deep %d for maker_kind %d" % (a[12], mk))
        hs = r.hold.get(aid, [])
        if hs and a[13] != hs[-1][4]:
            out.append(pre + "status %d is not its last holding's state %d" % (a[13], hs[-1][4]))
    for aid in sorted(r.hold):
        hs = r.hold[aid]
        a = r.arts.get(aid)
        pre0 = "corrupt table 'artifact_holding': row %d: " % aid
        for k, h in enumerate(hs):
            pre = pre0 + "holding %d of artifact %d: " % (h[1], aid)
            if h[1] != k + 1:
                out.append(pre + "seq not dense")
                break
            if a is not None and k == 0 and (h[2] != a[9] or h[12] != a[10]):
                out.append(pre + "the first holding is not the making")
            if k + 1 < len(hs):
                n = hs[k + 1]
                if h[3] != n[2] or h[13] != n[12]:
                    out.append(pre + "does not close where the next opens")
            elif h[3] is not None or h[13] != 0:
                out.append(pre + "the last holding is closed")
            if h[3] is not None and h[2] > h[3]:
                out.append(pre + "from_year above to_year")
            if r.year(h[12]) != h[2]:
                out.append(pre + "event %d is not of year %d" % (h[12], h[2]))
            st, ht, hd = h[4], h[5], h[6]
            if st not in STATE_HOLDER or ht not in STATE_HOLDER[st]:
                out.append(pre + "state %d with holder type %d" % (st, ht))
                continue
            exists = {3: r.civ, 4: r.stl, 5: r.reg if r.reg else {hd: 1}, 6: r.fig, 8: r.mon, 9: r.site}[ht]
            if hd not in exists:
                out.append(pre + "holder %d does not exist" % hd)
            place = h[7:12]
            if st <= 3 and any(place):
                out.append(pre + "place columns not 0")
            if st >= 4:
                if regions is not None and h[7] not in regions:
                    out.append(pre + "region %d" % h[7])
                if W is not None and not (0 <= h[10] < W and 0 <= h[11] < W):
                    out.append(pre + "cell (%d, %d) outside the map" % (h[10], h[11]))
                if ht == 9 and h[9] != hd:
                    out.append(pre + "site %d is not its holder %d" % (h[9], hd))
                if ht == 4 and h[8] != hd:
                    out.append(pre + "settlement %d is not its holder %d" % (h[8], hd))
    for fid, f in sorted(r.fig.items()):
        if f[3] == SMITH and (r.key(f[9]) != "artifact_made" or r.one(f[9], "maker") != fid):
            out.append("corrupt table 'figure': row %d: a smith's start_event %d" % (fid, f[9]))
    ranges = {"artifact_made": dict(maker_kind=(1, 4), deep=(0, 1)), "artifact_taken": dict(how=(1, 4)),
              "artifact_hoarded": dict(how=(1, 3)), "artifact_recovered": dict(how=(1, 2))}
    for key, rg in ranges.items():
        for e in r.of(key):
            for v, (lo, hi) in rg.items():
                x = r.val(e, v)
                if x is None or not lo <= x <= hi:
                    out.append("corrupt table 'event_value': row %d: %s %s outside [%d, %d]" % (e, v, x, lo, hi))
    for e in r.of("artifact_made"):
        mk = r.val(e, "maker_kind")
        has = {role: bool(r.role(e, role)) for role in ("maker", "god", "maker_realm")}
        if has["maker"] != (mk in (1, 2, 3)) or has["god"] != (mk == 4) or has["maker_realm"] != (mk == 2):
            out.append("corrupt table 'event_participant': row %d: artifact_made roles against maker_kind %s" % (e, mk))
    for p in res.get("event_participant", []):
        if p[3] == ARTIFACT:
            a = r.arts.get(p[4])
            if a is None or a[10] > p[0]:
                out.append("corrupt table 'event_participant': row %d: artifact %d not made at or before it" % (
                    p[0], p[4]))
    return out


def stats_of_tables(res, _unused=0):
    """10 item 6 (cult_returns_barred dropped by the lead, A-R14)."""
    r = R(res)
    arts = list(r.arts.values())
    ev = {k: r.of(k) for k in KINDS.values()}
    moved = set()
    for k in ("artifact_taken", "artifact_lost", "artifact_hoarded", "artifact_recovered"):
        moved |= {r.one(e, "artifact") for e in ev[k]}
    lost_end = 0
    others = 0
    for a in arts:
        h = r.open_holding(a[0])
        st = h[4]
        if st in (HOARDED, LOST, BURIED) or (st == ENSHRINED and (r.site[h[6]][2] == SANCTUM or r.site[h[6]][12] != 0)):
            lost_end += 1
        if not r.ended(a[3]) and ((st == KEPT and h[6] != a[3]) or
                                  (st == BORNE and r.fig.get(h[6], (0,) * 7)[6] != a[3])):
            others += 1
    wars = 0
    for e in r.of("war_declared"):   # spec 10 item 6 (lead ruling on part D; A-R15)
        a_, b_ = r.one(e, "attacker"), r.one(e, "defender")
        cs = set(r.cause_list(e))
        for aid, a in r.arts.items():
            h = r.holding_at_event(aid, e)
            if a[3] == a_ and h is not None and h[12] in cs and (
                    (h[4] == KEPT and h[6] == b_) or (h[4] == BORNE and r.fig.get(h[6], (0,) * 7)[6] == b_)):
                wars += 1
                break
    st = [("artifacts_made", len(arts)),
          ("artifacts_smiths", sum(1 for a in arts if a[4] in (1, 2))),
          ("artifacts_commissioned", sum(1 for a in arts if a[4] == 2)),
          ("artifacts_casters", sum(1 for a in arts if a[4] == 3)),
          ("artifacts_gods", sum(1 for a in arts if a[4] == 4)),
          ("artifacts_deep", sum(1 for a in arts if a[12] == 1))]
    st += [("artifacts_" + CLASSES[i], sum(1 for a in arts if r.kinds[a[2]][4] == i + 1)) for i in range(5)]
    st += [("artifacts_taken", len(ev["artifact_taken"])), ("artifacts_lost", len(ev["artifact_lost"])),
           ("artifacts_hoarded", len(ev["artifact_hoarded"])), ("artifacts_recovered", len(ev["artifact_recovered"])),
           ("artifacts_buried", len(ev["artifact_buried"])),
           ("artifact_holdings", sum(len(v) for v in r.hold.values())),
           ("artifacts_changed_hands", len(moved)), ("artifacts_lost_at_end", lost_end),
           ("artifacts_held_by_others", others), ("smiths", sum(1 for f in r.fig.values() if f[3] == SMITH)),
           ("wars_citing_artifacts", wars),
           ("battles_with_artifacts", sum(1 for e in r.of("battle_led")
                                          if any(r.key(x) == "artifact_made" for x in r.cause_list(e)))),
           ("battles_under_acts", sum(1 for k in BATTLE_KEYS for e in r.of(k)
                                      if any(r.key(x) in ("divine_bless", "divine_curse") for x in r.cause_list(e))))]
    assert tuple(k for k, _ in st) == STATS_KEYS
    return st


# --- 10: the v12 lines (text over rows; names from the rows; region names as at E)
def _ref(r, ht, hd, E):
    if ht == CIV:
        return "c%d %s" % (hd, r.civ[hd][1])
    if ht == FIGURE:
        return "f%d %s" % (hd, r.fig_name(hd))
    if ht == MONSTER:
        return "m%d %s" % (hd, r.mon[hd][1])
    if ht == REGION:
        return "rg%d %s" % (hd, r.region_name_at_end(hd, E))   # rg<id> (lead ruling; spec 10 item 2)
    if ht == SETTLEMENT:
        return "s%d %s" % (hd, r.stl[hd][1])
    return "si%d %s" % (hd, r.site[hd][1])


def present_text(r, h, E):
    st, ht, hd = h[4], h[5], h[6]
    if st == KEPT:
        return "kept by c%d %s" % (hd, r.civ[hd][1])
    if st == BORNE:
        c = r.fig[hd][6]
        return "borne by f%d %s of c%d %s" % (hd, r.fig_name(hd), c, r.civ[c][1])
    if st == HOARDED:
        return "in the hoard of m%d %s" % (hd, r.mon[hd][1])
    if st == LOST:
        if ht == REGION:
            return "lost in rg%d %s" % (hd, r.region_name_at_end(hd, E))
        if ht == SETTLEMENT:
            return "lost in the ruin of s%d %s" % (hd, r.stl[hd][1])
        return "lost in si%d %s" % (hd, r.site[hd][1])
    if st == BURIED:
        return "buried with f%d %s in rg%d %s" % (hd, r.fig_name(hd), h[7], r.region_name_at_end(h[7], E))
    return "at si%d %s" % (hd, r.site[hd][1])


def artifact_line(r, a, E):
    k = r.kinds[a[2]]
    if a[4] == GOD_GIVEN:
        maker = "g%d %s" % (a[6], r.god[a[6]][1])
    else:
        maker = "f%d %s of c%d %s" % (a[5], r.fig_name(a[5]), a[7], r.civ[a[7]][1])
    s = "  ar%d %s, %s: made %d e%d by %s" % (a[0], a[1], kind_form(k[3], a[12]), a[9], a[10], maker)
    if a[8]:
        s += " at si%d %s" % (a[8], r.site[a[8]][1])
    h = r.open_holding(a[0])
    s += " for c%d %s; %s since %d e%d" % (a[3], r.civ[a[3]][1], present_text(r, h, E), h[2], h[12])
    return s


def held_line(r, a, E):
    return "    held: " + "; ".join("%d %s %s e%d" % (h[2], HELD_WORDS[h[4]], _ref(r, h[5], h[6], E), h[12])
                                    for h in r.hold[a[0]])


def end_year_of(res):
    return res.get("_end", max([e[1] for e in res.get("event", [])] + [0]))


def artifacts_block(res, level="full"):
    """10 item 2: the lines of [artifacts] (the block header itself not included)."""
    r = R(res)
    E = end_year_of(res)
    arts = sorted(r.arts.values())
    st = [r.open_holding(a[0])[4] for a in arts]
    moved = set()
    for k in ("artifact_taken", "artifact_lost", "artifact_hoarded", "artifact_recovered"):
        moved |= {r.one(e, "artifact") for e in r.of(k)}
    lines = ["artifacts: %d (kept %d, borne %d, hoarded %d, lost %d, buried %d, enshrined %d); made by smiths %d (for "
             "other realms %d), casters %d, gods %d; of deep iron %d; changed hands %d" % (
                 len(arts), st.count(1), st.count(2), st.count(3), st.count(4), st.count(5), st.count(6),
                 sum(1 for a in arts if a[4] in (1, 2)), sum(1 for a in arts if a[4] == 2),
                 sum(1 for a in arts if a[4] == 3), sum(1 for a in arts if a[4] == 4),
                 sum(1 for a in arts if a[12] == 1), len(moved))]
    if level == "full":
        for a in arts:
            lines += [artifact_line(r, a, E), held_line(r, a, E)]
    else:
        shown = sorted(arts, key=lambda a: (-len(r.hold[a[0]]), a[0]))
        lines += [artifact_line(r, a, E) for a in shown[:SUMMARY_ARTIFACTS_MAX]]
        if len(shown) > SUMMARY_ARTIFACTS_MAX:
            lines.append("  and %d more" % (len(shown) - SUMMARY_ARTIFACTS_MAX))
    return lines


def _limited(items, cap=4):
    if len(items) <= cap:
        return ", ".join(items)
    return ", ".join(items[:cap]) + " and %d more" % (len(items) - cap)


def realm_line(res, c):
    """10 item 3: the realm block line `artifacts:` of civilization c (None when every part is empty)."""
    r = R(res)
    made = [a for a in sorted(r.arts.values()) if a[3] == c]
    cv = r.civ[c]
    holds = []
    for a in sorted(r.arts.values()):
        # reading A-R13 (the engine's, integration): at E, or for an ended realm a holding in force in its ended year
        # (from_year <= ended_year <= to_year, or open)
        y = cv[8]
        hs = [h for h in r.hold[a[0]] if (h[3] is None if y is None else (h[2] <= y and (h[3] is None or h[3] >= y)))]
        if any((h[4] == KEPT and h[6] == c) or (h[4] == BORNE and r.fig[h[6]][6] == c) for h in hs):
            holds.append(a)
    taken = []
    for a in made:
        h = r.open_holding(a[0])
        if (h[4] == KEPT and h[6] != c) or (h[4] == BORNE and r.fig[h[6]][6] != c) or h[4] == HOARDED:
            taken.append((a, h))
    parts = []
    if made:
        parts.append("made for it " + _limited(["ar%d %s e%d" % (a[0], a[1], a[10]) for a in made]))
    if holds:
        parts.append("holds " + _limited(["ar%d %s" % (a[0], a[1]) for a in holds]))
    if taken:
        parts.append("taken from it " + _limited(["ar%d %s e%d" % (a[0], a[1], h[12]) for a, h in taken]))
    return "  artifacts: " + "; ".join(parts) if parts else None


def figure_fragments(res):
    """10 item 4: {figure: fragment} of the full [figures] lines: `; bore ar<id>[, ...]` and a smith's `; made ar<id>`."""
    r = R(res)
    out = {}
    bore = {}
    for aid, hs in sorted(r.hold.items()):
        for h in hs:
            if h[4] == BORNE and aid not in bore.setdefault(h[6], []):
                bore[h[6]].append(aid)
    for f, xs in bore.items():
        out[f] = "; bore " + ", ".join("ar%d" % a for a in sorted(xs))
    for a in sorted(r.arts.values()):
        if a[4] in (1, 2):
            out[a[5]] = out.get(a[5], "") + "; made ar%d" % a[0]
    return out


def monster_fragments(res):
    """10 item 5: {monster: `, hoard ar<id>[, ...]`} for the monsters hoarding at E."""
    r = R(res)
    out = {}
    for a in sorted(r.arts.values()):
        h = r.open_holding(a[0])
        if h[4] == HOARDED:
            out.setdefault(h[6], []).append(a[0])
    return {m: ", hoard " + ", ".join("ar%d" % x for x in xs) for m, xs in out.items()}


# --- 11.1 the timeline reads
def lair_at(r, m, year):
    xs = [s for s in r.site.values() if s[2] == LAIR and s[8] == m and s[9] <= year]
    return max(xs, key=lambda s: (s[9], s[0])) if xs else None


def artifact_at(res, a, year, r=None):
    """ArtifactPlace (state, holder_type, holder, region, settlement, site, x, y, since, event)."""
    r = r or R(res)
    hs = [h for h in r.hold.get(a, []) if h[2] <= year]
    if not hs:
        return (0,) * 10
    h = hs[-1]
    if h[4] in (KEPT, BORNE):
        return (h[4], h[5], h[6], 0, 0, 0, 0, 0, h[2], h[12])
    if h[4] == HOARDED:
        s = lair_at(r, h[6], year)
        return (h[4], h[5], h[6], s[3] if s else 0, 0, s[0] if s else 0, s[4] if s else 0, s[5] if s else 0, h[2],
                h[12])
    return (h[4], h[5], h[6], h[7], h[8], h[9], h[10], h[11], h[2], h[12])


def artifacts_held(res, holder_type, holder, year):
    r = R(res)
    return [a for a in sorted(r.arts) if artifact_at(res, a, year, r)[1:3] == (holder_type, holder)]


def artifacts_placed(res, year):
    r = R(res)
    return [a for a in sorted(r.arts) if artifact_at(res, a, year, r)[0] >= 3]


# ---------------------------------------------------------------------------
# Fixture data (4.6) and runs
# ---------------------------------------------------------------------------

ARTIFACTS_SMALL = dict(realm_rest_years=0, world_max=500, burial_permille=500, battle_take_permille=500,
                       raid_take_permille=500)


def with_artifacts(dat, edits=None, chance=None, occasions=None):
    d = _pcr().edited(dat)
    d.base = dat.base
    t = copy.deepcopy(tuning_of(dat))
    if occasions is not None:
        t["occasions"] = copy.deepcopy(occasions)
    if chance is not None:
        t["occasions"] = [dict(o, chance_permille=chance) for o in t["occasions"]]
    t.update(copy.deepcopy(edits or {}))
    d.artifacts = t
    d.artifact_kinds = copy.deepcopy(getattr(dat, "artifact_kinds", []))
    for k in ("gods", "pantheon"):
        setattr(d, k, copy.deepcopy(getattr(dat, k, None)))
    return d


def artifacts_small_data(dat=None):
    """4.6 artifacts-small: pantheon-small with every shipped occasion at 1000 and the five edits."""
    base = _p508().pantheon_small_data(dat)
    return with_artifacts(base, ARTIFACTS_SMALL, chance=1000)


def zero_data(dat=None):
    """4.3 the fixture tuning zero (I0, 14): the shipped values with occasions empty."""
    return with_artifacts(dat or _pcr().data(), occasions=[])


def run_case(seed, dat, case=None):
    return _p508().run_case(seed, dat, case)


def fixture_files():
    return sorted(f for f in os.listdir(FIXTURE_DIR) if f.endswith(".toml")) if os.path.isdir(FIXTURE_DIR) else []


def fixture_data(name):
    """A fixture file: base artifacts-small with [artifacts] edits (and `chance` for every occasion), [figures],
    [pantheon], [monsters] edits and its cases (seed, (span, races, civs, intensity))."""
    import tomllib
    with open(os.path.join(FIXTURE_DIR, name), "rb") as f:
        doc = tomllib.load(f)
    assert doc["base"] == "artifacts-small", name
    d = artifacts_small_data()
    ed = dict(doc.get("artifacts", {}))
    ch = ed.pop("chance", None)
    d = with_artifacts(d, ed, chance=ch)
    for sub in ("figures", "pantheon", "conflict", "alliances"):
        if sub in doc:
            setattr(d, sub, dict(copy.deepcopy(getattr(d, sub)), **doc[sub]))
    if "monsters" in doc:
        d.base = copy.copy(d.base)
        d.base.monsters_tuning = dict(copy.deepcopy(d.base.monsters_tuning), **doc["monsters"])
    cases = [(c["seed"], (c["span"], c["races"], c["civs"], c["intensity"])) for c in doc["case"]]
    return d, cases, doc.get("paths", []), doc


def paths_of(res):
    """The paths of 6 a run shows (from rows)."""
    r = R(res)
    seen = set()
    for a in r.arts.values():
        seen.add("maker %d" % a[4])
        seen.add("class %s" % CLASSES[r.kinds[a[2]][4] - 1])
        if a[12]:
            seen.add("deep")
        seen.add("occasion %s" % r.key(a[11]))
        hs = r.hold[a[0]]
        seen.add("first %s" % STATE_WORDS[hs[0][4]])
        for prev, h in zip(hs, hs[1:]):
            x = h[12]
            xk = r.key(x)
            trig = r.key(r.cause_list(x)[0]) if xk in KIND_ID else xk
            seen.add("move %s %s>%s%s" % (trig, STATE_WORDS[prev[4]], STATE_WORDS[h[4]],
                                          "" if xk in KIND_ID else " quiet"))
            if xk not in KIND_ID and xk not in QUIET_MOVE_KEYS:
                seen.add("quiet-step move")
    for e in r.of("artifact_made"):
        if r.quiet(e):
            seen.add("made in Q4")
    for k in KINDS.values():
        if r.of(k):
            seen.add(k)
    st = dict(stats_of_tables(res))
    if st["wars_citing_artifacts"]:
        seen.add("claim cited")
    if st["battles_with_artifacts"]:
        seen.add("battle cites artifact")
    if st["battles_under_acts"]:
        seen.add("battle cites act")
    return seen


REQUIRED_PATHS = ("maker 1", "maker 2", "maker 3", "maker 4", "class arms", "class armour", "class regalia",
                  "class relic", "class staff", "first kept", "first borne", "first enshrined", "artifact_taken",
                  "artifact_lost", "artifact_hoarded", "artifact_recovered", "artifact_buried")
REPORTED_PATHS = ("deep", "made in Q4", "quiet-step move", "claim cited", "battle cites artifact", "battle cites act",
                  "move general_appointed kept>borne quiet", "move figure_died borne>enshrined quiet",
                  "move figure_died borne>kept quiet", "move figure_died borne>buried",
                  "move fell_in_battle borne>kept", "move fell_in_battle borne>lost", "move hero_slain borne>hoarded",
                  "move monster_slain_by_hero hoarded>borne", "move monster_slain_by_hero hoarded>kept",
                  "move monster_slain hoarded>kept", "move monster_died hoarded>lost quiet",
                  "move monster_raid kept>hoarded", "move hold_lost kept>hoarded", "move sacked kept>kept",
                  "move sacked kept>lost", "move settlement_captured kept>kept", "move civ_conquered kept>kept",
                  "move civ_ended kept>lost", "move civ_ended kept>kept", "move peace kept>kept",
                  "move war_won kept>kept", "move peace borne>kept", "move cult_suppressed enshrined>enshrined quiet",
                  "move cult_suppressed enshrined>kept quiet")


def run_problems(res, inp, dat):
    """13 on the run's rows, the replay-only checks (7's win chances are inside the replay), and P5-08's (with
    P5-07's and P5-02's) invariants."""
    w = res["_world"]
    t = dict(w.artifacts.t, _cult_return_years=(dat.pantheon or {}).get("cult_return_years", 0)
             if _pcr().FLAGS["p511"] else 0)
    rows = dict(res)
    rows["region"] = [(r["id"], r["name"]) for r in inp.regions]
    rows["_map_size"] = None
    rows["_end"] = inp.params["span"] - 1
    out = violations(rows, t)
    out += ["P5-08 " + v for v in _p508().run_problems(res, inp, dat)]
    return out


def table_hash(name, rows):
    pchr = __import__("p3_chronology_reference")
    cols = dict(_pcr().COLUMNS, artifact_kind=8, artifact=14, artifact_holding=14, figure=19, god=6, faction=13)
    n = cols.get(name, len(rows[0]) if rows else 1)
    return pchr.table_hash(n, [tuple(r) for r in rows])


I0_TABLES = ("event", "event_participant", "event_cause", "event_value", "civilization", "settlement", "relation",
             "site", "monster", "region_control", "figure", "faction")


# ---------------------------------------------------------------------------
# Units (16 items 1 to 3, 7, 8 on hand-checked values)
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
    c.eq("7.6 an axe", kind_form("axe", 0), "an axe")
    c.eq("7.6 a sword of deep iron", kind_form("sword", 1), "a sword of deep iron")
    c.eq("7.6 an orb", kind_form("orb", 0), "an orb")
    c.eq("7.6 an idol of deep iron", kind_form("idol", 1), "an idol of deep iron")
    c.eq("7.6 a helm (h is no vowel)", kind_form("helm", 0), "a helm")
    c.eq("7.6 an ugly mace (u)", kind_form("ugly mace", 0), "an ugly mace")
    c.eq("7.6 a ring (r)", kind_form("ring", 0), "a ring")
    c.eq("6.3 p no forge", make_permille(60, 0, 2000), 60)
    c.eq("6.3 p with a forge", make_permille(60, 7, 2000), 120)
    c.eq("6.3 p capped", make_permille(600, 7, 2000), 1000)
    c.eq("6.3 p forge factor 0", make_permille(600, 7, 0), 0)
    c.eq("6.3 p 4000 x 1000", make_permille(1000, 1, 4000), 1000)
    c.eq("6.3 W relic zeroed without a god", class_weights(dict(arms=1, armour=2, regalia=3, relic=4, staff=5), False,
                                                           True), [1, 2, 3, 0, 5])
    c.eq("6.3 W staff zeroed without a caster's tower",
         class_weights(dict(arms=1, armour=2, regalia=3, relic=4, staff=5), True, False), [1, 2, 3, 4, 0])
    c.eq("6.3 W both kept", class_weights(dict(arms=0, armour=0, regalia=0, relic=1000, staff=0), True, True),
         [0, 0, 0, 1000, 0])
    dep = {1: 0, 2: 1, 3: 3, 4: 3, 5: 0, 6: 2}.get   # absent: no open mine
    c.eq("6.4 own mine", forge_of(2, dep, [3, 4]), 2)
    c.eq("6.4 own undeepened mine", forge_of(1, dep, [3]), 1)
    c.eq("6.4 partner by depth, then the smaller id", forge_of(8, dep, [6, 4, 3]), 3)
    c.eq("6.4 partner by depth", forge_of(8, dep, [2, 6]), 6)
    c.eq("6.4 partner with an undeepened mine", forge_of(8, dep, [5]), 5)
    c.eq("6.4 two undeepened partners: the smaller id", forge_of(8, dep, [5, 1]), 1)
    c.eq("6.4 no forge", forge_of(7, dep, [8]), 0)
    c.eq("6.4 no partners", forge_of(8, dep, []), 0)
    c.ok("6.3 eligible", eligible(True, True, 100, 50, 50, 3, 40, False, 0))
    c.ok("6.3 rest: Y - last < rest", not eligible(True, True, 100, 51, 50, 3, 40, False, 0))
    c.ok("6.3 never made", eligible(True, True, 0, NEVER, 10000, 0, 40, False, 0))
    c.ok("6.3 ended", not eligible(False, True, 100, NEVER, 50, 0, 40, False, 0))
    c.ok("6.3 not visible", not eligible(True, False, 100, NEVER, 50, 0, 40, False, 0))
    c.ok("6.3 world cap", not eligible(True, True, 100, NEVER, 50, 40, 40, False, 0))
    c.ok("6.3 world cap 0", not eligible(True, True, 100, NEVER, 50, 0, 0, False, 0))
    c.ok("6.3 Q4 without budget", not eligible(True, True, 100, NEVER, 50, 0, 40, True, 0))
    c.ok("6.3 Q4 with budget", eligible(True, True, 100, NEVER, 50, 0, 40, True, 1))
    sword, axe, helm, staff = (dict(battle_permille=50, hunt_permille=60), dict(battle_permille=40, hunt_permille=60),
                               dict(battle_permille=30, hunt_permille=30), dict(battle_permille=0, hunt_permille=40))
    c.eq("7.1 battle sum", borne_sum([(sword, 0), (helm, 0)], "battle_permille", 30, 150), 80)
    c.eq("7.1 battle sum, deep sword", borne_sum([(sword, 1), (helm, 0)], "battle_permille", 30, 150), 110)
    c.eq("7.1 battle sum capped", borne_sum([(sword, 1), (axe, 1), (helm, 1)], "battle_permille", 30, 150), 150)
    c.eq("7.1 deep bonus not on a 0 field", borne_sum([(staff, 1)], "battle_permille", 30, 150), 0)
    c.eq("7.1 hunt deep staff", borne_sum([(staff, 1)], "hunt_permille", 30, 200), 70)
    c.eq("7.1 hunt capped", borne_sum([(sword, 1), (axe, 1), (helm, 1)], "hunt_permille", 30, 200), 200)
    c.eq("7.1 none", borne_sum([], "hunt_permille", 30, 200), 0)
    c.eq("7.2 win chance, no arms = P5-06", win_chance(1000, 1000, 1500, 100, 200, 0, 0),
         _p506().led_win_chance(1000, 1000, 1500, 100, 200))
    c.eq("7.2 win chance with arms", win_chance(1000, 1000, 1000, 0, 0, 150, 0), 1150 * 1000 // 2150)
    c.eq("7.2 win chance arms both", win_chance(2000, 1000, 1200, 100, 50, 80, 30),
         (2000 * 1180 // 1000) * 1000 // (2000 * 1180 // 1000 + 1000 * 1200 // 1000 * 1080 // 1000))
    c.eq("7.2 win chance zero forces", win_chance(0, 0, 1000, 0, 0, 100, 100), 500)
    c.eq("7.2 strain factor", strain_rate(1234, 500), 1851)
    c.eq("7.2 strain no claim", strain_rate(1234, 0), 1234)
    c.eq("7.2 strain cap 1000", strain_rate(7, 1000), 14)
    c.eq("7.2 hunt with a hero and arms", hunt_values(20, 200, 5, 3, 1000, 1200, 30, 500, 60, 0), (60, 1872))
    c.eq("7.2 hunt motive in both terms", hunt_values(20, 200, 100, 9, 1000, 1000, 30, 0, 0, 50)[0], 280)
    c.eq("7.2 hunt motive, small term", hunt_values(20, 200, 5, 1, 1000, 1000, 0, 0, 0, 50)[0], 70)
    c.eq("7.2 hunt no hero = P5-06", hunt_values(20, 200, 5, 1, 1000, 1000, 0, 0, 0, 0),
         _p506().hero_hunt(20, 200, 5, 1, 1000, 1000, 0, 0))


def check_data(c):
    """16 item 1 on the data root (game/data, or the fixture listing while it lacks the kind)."""
    pdr.use_version(VERSION)
    dat = pdr.load_root("game/data", kinds=("artifact", "culture", "history", "god"))
    kinds = dat["artifact"]
    c.ok("4.1 kinds defined (%d)" % len(kinds), len(kinds) >= 1)
    for did, d in kinds:
        c.eq("4.1 %s checks" % did, pdr.artifact_messages(did, d), [])
    c.eq("4.1 artifact_battle_class", pdr.artifact_messages("x", dict(kinds[0][1], **{"class": "relic",
                                                                                         "battle_permille": 10,
                                                                                         "hunt_permille": 0})),
         ["battle_permille needs arms or armour"])
    c.eq("4.1 artifact_hunt_class", pdr.artifact_messages("x", dict(kinds[0][1], **{"class": "regalia",
                                                                                       "battle_permille": 0,
                                                                                       "hunt_permille": 5})),
         ["hunt_permille needs arms, armour or staff"])
    c.eq("4.1 title grammar", pdr.artifact_messages("x", dict(kinds[0][1], title="sword"))[:1],
         ["title 'sword' is not an artifact_word"])
    c.eq("4.1 noun grammar", pdr.artifact_messages("x", dict(kinds[0][1], noun="Sword"))[:1],
         ["noun 'Sword' is not an artifact_noun"])
    c.ok("4.1 every class has a kind", {d["class"] for _k, d in kinds} == set(CLASSES))
    cul = dict(dat["culture"])
    for k, d in sorted(cul.items()):
        forms = d["naming"]["artifact_forms"]
        c.ok("4.2 %s artifact_forms 1 to 8" % k, 1 <= len(forms) <= 8)
        for f in forms:
            c.ok("4.2 %s form %s" % (k, f), pdr.artifact_form_ok(f))
    c.ok("4.2 {title} twice refused", not pdr.artifact_form_ok("{title} {root} {title}"))
    c.ok("4.2 no {root} refused", not pdr.artifact_form_ok("The {title}"))
    c.ok("4.5 faerie cult form", "The Silver Ring of {god}" in cul["faerie"]["naming"]["cult_forms"] and
         "The Hidden Ring of {god}" not in cul["faerie"]["naming"]["cult_forms"])
    h = dict(dat["history"])
    t = h["artifacts"]
    c.eq("4.3 field order", list(t), ["occasions"] + [n for n, _lo, _hi in pdr.ARTIFACTS_FIELDS_031])
    for n, lo, hi in pdr.ARTIFACTS_FIELDS_031:
        c.ok("4.3 %s in [%d, %d]" % (n, lo, hi), lo <= t[n] <= hi)
    pe.use_version(VERSION)
    cat = {k["key"]: k for k in pe.COMPILED}
    c.eq("4.4 shipped occasions pass", occasion_messages(t["occasions"], cat), [])
    c.ok("4.4 at most 32", len(t["occasions"]) <= 32)
    one = dict.fromkeys(CLASSES, 1)
    bad = [dict(kind="nonesuch", role="x", chance_permille=1, bearer="realm", classes=one),
           dict(kind="sacked", role="settlement", chance_permille=1, bearer="realm", classes=one),
           dict(kind="sacked", role="defender", chance_permille=1, bearer="general", classes=one),
           dict(kind="sacked", role="defender", chance_permille=1, bearer="figure", classes=one),
           dict(kind="sacked", role="defender", chance_permille=1, bearer="site", classes=one),
           dict(kind="sacked", role="defender", chance_permille=1, bearer="realm", classes=dict.fromkeys(CLASSES, 0))]
    c.eq("4.4 the occasion checks", occasion_messages(bad, cat),
         ["occasions[0]: kind nonesuch is unknown", "occasions[1]: role settlement of sacked is not a civilization role",
          "occasions[2]: bearer general needs war_declared", "occasions[3]: bearer figure needs a figure role in sacked",
          "occasions[4]: bearer site needs a site role in sacked", "occasions[5]: the class weights are all 0"])
    c.ok("4.3 figures smith fields", all(n in h["figures"] for n, _l, _h in pdr.FIGURES_FIELDS_031))
    c.eq("4.3 figures smith fields last", list(h["figures"])[-2:], ["smith_age_permille", "smith_age_span_permille"])
    c.eq("4.3 pantheon cult_return_years last", [k for k, v in h["pantheon"].items() if not isinstance(v, dict)][-1],
         "cult_return_years")   # the last scalar field (a sub-table such as `creator` follows it in TOML)
    gods = dict(dat["god"])
    c.ok("4.5 craft deed", any(d["kind"] == "artifact_made" and d["role"] == "maker_realm" and d["points"] == 20 and
                               d["who"] == "any" for d in gods["craft"]["deeds"]))
    c.ok("4.5 war deed", any(d["kind"] == "artifact_taken" and d["role"] == "taker" and d["points"] == 10 and
                             d["who"] == "any" for d in gods["war"]["deeds"]))
    rep = {e["kind"]: e for e in h["report"]["events"]}
    for k in ("divine_bless", "divine_curse", "divine_champion", "divine_monster", "divine_mark"):
        c.ok("4.5 %s cause words" % k, "artifact_made=a great work forged" in rep[k]["template"] and
             "artifact_taken=a treasure taken" in rep[k]["template"])
    for k in KINDS.values():
        c.ok("8.2 %s entry" % k, k in rep)
    c.eq("8.2 saliences", [rep[k]["salience"] for k in KINDS.values()], [45, 40, 35, 50, 50, 25])
    if pdr.FALLBACK_031_USED:
        print("note: the data root lacks P5-11 items; read with the fixture listing for %s" %
              ", ".join(sorted(pdr.FALLBACK_031_USED)))


def check_render(c):
    """16 item 8: each template at each maker_kind and how; the kind form; empty titled and all."""
    pe.use_version(VERSION)
    dat = pdr.load_root("game/data")
    report = [(e["kind"], e["salience"], e["template"]) for e in dict(dat["history"])["report"]["events"]]
    cat = {k["key"]: k for k in pe.event_catalogue(report)}
    for key in KINDS.values():
        c.eq("8.2 %s passes template_problem" % key, pe.template_problem(cat[key], cat[key]["templ"]), None)
    c.eq("8.1 kinds 111 to 116", [(k["id"], k["key"], k["caused"], k["region_use"], k["settlement_use"])
                                  for k in pe.COMPILED if k["id"] >= 111],
         [(111, "artifact_made", 1, 1, 2), (112, "artifact_taken", 1, 1, 2), (113, "artifact_lost", 1, 1, 2),
          (114, "artifact_hoarded", 1, 1, 2), (115, "artifact_recovered", 1, 1, 0), (116, "artifact_buried", 1, 1, 2)])
    names = {(CIV, 1): "The Kingdom of Cear", (CIV, 2): "The Gub Horde", (CIV, 3): "The Hold of Durn",
             (CIV, 4): "The House of Finen", (FIGURE, 5): "Thrain", (FIGURE, 6): "Maela Dragonslayer",
             (GOD, 12): "Ormala", (MONSTER, 2): "Saelith the Black", (ARTIFACT, 1): "Kazrak",
             (ARTIFACT, 2): "The Silver Helm of Ithil", (ARTIFACT, 3): "The Crown of Aldmar",
             (SITE, 7): "The Temple of Ormala", (SITE, 8): "The Star-Tower of Femaeth", (REGION, 7): "Iron Downs",
             (SETTLEMENT, 9): "Dunmere", (FIGURE, 9): "Femaeth"}
    forms = {1: "a sword of deep iron", 2: "a helm", 3: "a crown"}

    def name(t, x, form):
        if form == "kind":
            return forms[x]
        return names[(t, x)]

    def view(eid, key, parts, values, causes=(), extra=None):
        evs = {eid: dict(year=900, end_year=900, kind=cat[key]["id"], region=7, settlement=9,
                         participants=[(i + 1, s, t, x) for i, (s, t, x) in enumerate(parts)],
                         values=dict(enumerate(values, 1)), causes=list(causes))}
        for cid, ckey in (extra or {}).items():
            evs[cid] = dict(year=899, end_year=899, kind=cat[ckey]["id"], region=7, settlement=9, participants=[],
                            values={}, causes=[])
        return dict(catalogue={k["id"]: k for k in cat.values()}, events=evs, name=name,
                    region_at=lambda r, e: names[(REGION, r)], region_at_year=lambda r, y: names[(REGION, r)],
                    ruler_at=lambda c, e: 0, god_title=lambda g: "the Keeper of Oaths")

    def rr(eid, key, parts, values, causes=(), extra=None):
        return pe.render(view(eid, key, parts, values, causes, extra), eid)

    seq = {r["role"]: r["seq"] for r in cat["artifact_made"]["roles"]}
    mk2 = ((seq["artifact"], ARTIFACT, 1), (seq["civilization"], CIV, 1), (seq["maker"], FIGURE, 5),
           (seq["maker_realm"], CIV, 3))
    c.eq("8.2 sample commission", rr(30, "artifact_made", mk2, (2, 1), (5,), {5: "war_declared"}),
         "In answer to the coming of war, Thrain forged Kazrak, a sword of deep iron, for the Kingdom of Cear at the "
         "forges of the Hold of Durn.")
    mk1 = ((seq["artifact"], ARTIFACT, 1), (seq["civilization"], CIV, 1), (seq["maker"], FIGURE, 5))
    c.eq("8.2 smith", rr(31, "artifact_made", mk1, (1, 1), (5,), {5: "sacked"}),
         "In answer to the sack of a city, Thrain forged Kazrak, a sword of deep iron, for the Kingdom of Cear.")
    mk3 = ((seq["artifact"], ARTIFACT, 3), (seq["civilization"], CIV, 1), (seq["maker"], FIGURE, 9),
           (seq["site"], SITE, 8))
    c.eq("8.2 caster", rr(32, "artifact_made", mk3, (3, 0), (5,), {5: "deep_woken"}),
         "In answer to a horror woken in the deep, Femaeth wrought the Crown of Aldmar, a crown, for the Kingdom of "
         "Cear.")
    mk4 = ((seq["artifact"], ARTIFACT, 2), (seq["civilization"], CIV, 4), (seq["god"], GOD, 12))
    c.eq("8.2 sample god-given", rr(33, "artifact_made", mk4, (4, 0), (5,), {5: "divine_champion"}),
         "In answer to a champion raised, Ormala the Keeper of Oaths bestowed the Silver Helm of Ithil, a helm, upon "
         "the House of Finen.")
    for ck, words in (("civ_conquered", "a realm conquered"), ("succession_disputed", "a throne disputed"),
                      ("hero_rose", "a hunt begun"), ("settlement_overrun", "a town overrun"),
                      ("cult_founded", "a cult founded")):
        c.eq("8.2 made cause %s" % ck, rr(34, "artifact_made", mk1, (1, 1), (5,), {5: ck}),
             "In answer to %s, Thrain forged Kazrak, a sword of deep iron, for the Kingdom of Cear." % words)
    sq = {r["role"]: r["seq"] for r in cat["artifact_taken"]["roles"]}
    tk = ((sq["artifact"], ARTIFACT, 1), (sq["taker"], CIV, 2), (sq["from"], CIV, 1), (sq["figure"], FIGURE, 6))
    for how, ck, words in ((1, "fell_in_battle", "the fall of its bearer"), (2, "sacked", "the sack of a city"),
                           (3, "settlement_captured", "the taking of a city"), (4, "civ_conquered", "the fall of a realm"),
                           (4, "civ_ended", "the end of a realm")):
        c.eq("8.2 taken how %d %s" % (how, ck), rr(40, "artifact_taken", tk if how == 1 else tk[:3], (how,), (5,),
                                                    {5: ck}),
             "After %s, the Gub Horde took Kazrak, a sword of deep iron, from the Kingdom of Cear." % words)
    sq = {r["role"]: r["seq"] for r in cat["artifact_lost"]["roles"]}
    ls = ((sq["artifact"], ARTIFACT, 1), (sq["civilization"], CIV, 1))
    c.eq("8.2 lost", rr(41, "artifact_lost", ls, (), (5,), {5: "fell_in_battle"}),
         "After the fall of its bearer, Kazrak, a sword of deep iron, was lost to the Kingdom of Cear in Iron Downs.")
    sq = {r["role"]: r["seq"] for r in cat["artifact_hoarded"]["roles"]}
    hd = ((sq["artifact"], ARTIFACT, 3), (sq["monster"], MONSTER, 2), (sq["civilization"], CIV, 1))
    for how, ck, words in ((1, "hero_slain", "the death of its bearer"), (2, "monster_raid", "a raid"),
                           (3, "hold_lost", "the fall of a hold")):
        c.eq("8.2 hoarded how %d" % how, rr(42, "artifact_hoarded", hd, (how,), (5,), {5: ck}),
             "After %s, Saelith the Black carried the Crown of Aldmar, a crown, off from the Kingdom of Cear to its "
             "hoard." % words)
    sq = {r["role"]: r["seq"] for r in cat["artifact_recovered"]["roles"]}
    rc1 = ((sq["artifact"], ARTIFACT, 1), (sq["civilization"], CIV, 1), (sq["monster"], MONSTER, 2))
    c.eq("8.2 sample recovered how 1", rr(43, "artifact_recovered", rc1, (1,), (5,), {5: "monster_slain_by_hero"}),
         "After the slaying of its keeper, the Kingdom of Cear recovered Kazrak, a sword of deep iron, from the hoard "
         "of Saelith the Black.")
    rc2 = ((sq["artifact"], ARTIFACT, 1), (sq["civilization"], CIV, 1), (sq["from"], CIV, 2))
    for ck in ("peace", "war_won"):
        c.eq("8.2 recovered how 2 %s" % ck, rr(44, "artifact_recovered", rc2, (2,), (5,), {5: ck}),
             "After a war won, the Kingdom of Cear recovered Kazrak, a sword of deep iron, from the Gub Horde.")
    sq = {r["role"]: r["seq"] for r in cat["artifact_buried"]["roles"]}
    bu = ((sq["artifact"], ARTIFACT, 1), (sq["figure"], FIGURE, 6), (sq["civilization"], CIV, 1))
    c.eq("8.2 sample buried", rr(45, "artifact_buried", bu, ()),
         "Kazrak, a sword of deep iron, was laid in the grave of Maela Dragonslayer of the Kingdom of Cear.")
    cb = {r["role"]: r["seq"] for r in cat["divine_bless"]["roles"]}
    for ck, words in (("artifact_made", "a great work forged"), ("artifact_taken", "a treasure taken")):
        c.eq("4.5 bless after %s" % ck, rr(46, "divine_bless", ((cb["actor"], GOD, 12), (cb["civilization"], CIV, 1)),
                                           (2, 50, 0, 0), (5,), {5: ck}),
             "Pleased by %s, Ormala the Keeper of Oaths blessed the arms of the Kingdom of Cear for 50 years." % words)


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

FIXTURE_SEEDS = (0, 1, 2, 3, 5, 6, 7, 9, 12, 13, 14)


def cmd_units(argv):
    _pcr().use_version(VERSION)
    c = Checker()
    check_units(c)
    check_data(c)
    check_render(c)
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def cmd_fixtures(argv):
    pcr = _pcr()
    pcr.use_version(VERSION)
    quick = "--quick" in argv
    c = Checker()
    dat = artifacts_small_data()
    seen, where = set(), {}
    for seed in (FIXTURE_SEEDS[:3] if quick else FIXTURE_SEEDS):
        res, inp = run_case(seed, dat)
        for v in run_problems(res, inp, dat):
            c.ok("artifacts-small stress-%d %s" % (seed, v), False)
        ps = paths_of(res)
        for pth in ps:
            where.setdefault(pth, seed)
        seen |= ps
        w = res["_world"]
        print("stress-%d %s" % (seed, " ".join("%s=%d" % kv for kv in stats_of_tables(res, w.pantheon.returns_barred))))
    for pth in REQUIRED_PATHS:
        c.ok("artifacts-small shows the path '%s'" % pth, pth in seen or quick)
    for pth in REPORTED_PATHS:
        if pth not in seen:
            print("note: artifacts-small shows no '%s' (reported, not failed)" % pth)
    print("first seen (path: stress seed): %s" % ", ".join("%s %d" % kv for kv in sorted(where.items())))
    for name in fixture_files():
        d, cases, want_paths, _doc = fixture_data(name)
        got = set()
        for seed, case in (cases[:1] if quick else cases):
            res, inp = run_case(seed, d, case)
            for v in run_problems(res, inp, d):
                c.ok("%s seed %d %s" % (name, seed, v), False)
            got |= paths_of(res)
        for pth in want_paths:
            c.ok("%s shows '%s'" % (name, pth), pth in got or quick)
    # I0 (14): occasions empty gives the build without the subsystem, apart from kinds 111 to 116 and artifact_kind
    for seed in (FIXTURE_SEEDS[:1] if quick else FIXTURE_SEEDS[:3]):
        pcr.use_version(VERSION)
        z = zero_data(pcr.stress_data())
        a, _ = run_case(seed, z)
        pcr.FLAGS["artifacts"] = False
        b, _ = run_case(seed, z)
        pcr.FLAGS["artifacts"] = True
        for tname in I0_TABLES:
            c.eq("I0 stress-%d table %s equals the build without the subsystem" % (seed, tname),
                 table_hash(tname, a.get(tname, [])), table_hash(tname, b.get(tname, [])))
        c.eq("I0 stress-%d no artifact" % seed, (a.get("artifact"), a.get("artifact_holding")), ([], []))
        c.ok("I0 stress-%d no smith" % seed, not any(f[3] == SMITH for f in a.get("figure", [])))
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def rows_of_db(path):
    return _p508().rows_of_db(path)


def cmd_check_world(argv):
    """13 on a 0.31.0 world file's rows (no replay; p3_conflict_reference.py check-world replays it)."""
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
        rows["_map_size"] = int(meta.get("param.map_size", "0")) or None
        rows["_end"] = int(meta.get("history_end_year", "0"))
        t = dict(tuning_of(dat), _cult_return_years=(dat.pantheon or {}).get("cult_return_years", 0))
        for v in violations(rows, t):
            c.ok("%s invariant %s" % (path, v), False)
        print("%s: %s; %d checks, %d failures" % (path, " ".join("%s=%d" % kv for kv in stats_of_tables(rows)
                                                                 if kv[0] not in REPLAY_STATS), c.count, c.failures))
        bad += c.failures
    return 1 if bad else 0


GOLDEN_HEADER = """\
# P5-11 16: golden values of the artifacts (generator 0.31.0, schema 15), written by
# worldgen/tests/reference/p5_11_artifacts_reference.py golden (the test writer's reference, never from C++ output).
# Lines: case <name> <seed> <size> <data>; table <name> <sha256 of the P0-04 8 encoding>; stat <key> <value>.
# Fixture cases (size 0: a conflict stress geography with artifacts-small, 4.6) also list path <path> for every path
# of 6 the run shows. World lines (artifacts-small at 512 seeds 0 to 9, shipped 42 at 1024) are added at integration
# with --inputs.
"""
GOLDEN_TABLES = ("artifact_kind", "artifact", "artifact_holding", "figure", "event", "event_cause")


def case_lines(name, seed, size, what, res):
    w = res["_world"]
    o = ["case %s %d %d %s" % (name, seed, size, what)]
    for t in GOLDEN_TABLES:
        o.append("table %s %s" % (t, table_hash(t, res.get(t, []))))
    o += ["stat %s %d" % kv for kv in stats_of_tables(res, w.pantheon.returns_barred if w.pantheon else 0)]
    return o


def golden_text(inputs=None):
    pcr = _pcr()
    pchr = __import__("p3_chronology_reference")
    pcr.use_version(VERSION)
    o = [GOLDEN_HEADER.rstrip("\n")]
    dat = artifacts_small_data()
    for seed in FIXTURE_SEEDS:
        res, inp = run_case(seed, dat)
        bad = run_problems(res, inp, dat)
        assert not bad, (seed, bad[:5])
        o += case_lines("artifacts-small-stress-%d" % seed, seed, 0, "artifacts-small", res)
        o += ["path %s" % p for p in sorted(paths_of(res))]
    for name in fixture_files():
        d, cases, _paths, _doc = fixture_data(name)
        for seed, case in cases:
            res, inp = run_case(seed, d, case)
            bad = run_problems(res, inp, d)
            assert not bad, (name, seed, bad[:5])
            o += case_lines("%s-%d" % (name[:-5], seed), seed, 0, name[:-5], res)
            o += ["path %s" % p for p in sorted(paths_of(res))]
    if inputs:
        cases = [("artifacts-small-%d-512" % s, s, 512, os.path.join(inputs, "artifacts-%d-512.db" % s),
                  "artifacts-small") for s in range(10)] + \
                [("shipped-42-1024", 42, 1024, os.path.join(inputs, "geo-seed-42-1024.db"), "shipped")]
        for name, seed, size, path, what in cases:
            if not os.path.exists(path):
                print("missing input %s" % path, file=sys.stderr)
                continue
            dat = artifacts_small_data(pcr.data()) if what == "artifacts-small" else pcr.data()
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
    cmd, rest = argv[0], argv[1:]
    if cmd == "units":
        return cmd_units(rest)
    if cmd == "fixtures":
        return cmd_fixtures(rest)
    if cmd == "check-world":
        return cmd_check_world(rest)
    if cmd == "readings":
        print(READINGS)
        return 0
    if cmd == "golden":
        inputs = rest[rest.index("--inputs") + 1] if "--inputs" in rest else None
        out = [a for a in rest if not a.startswith("--") and a != inputs]
        text = golden_text(inputs)
        if out and out[0] == "-":
            sys.stdout.write(text)
        else:
            with open(out[0] if out else GOLDEN, "w", newline="\n") as f:
                f.write(text)
            print("wrote %s" % (out[0] if out else GOLDEN))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
