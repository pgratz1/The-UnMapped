#!/usr/bin/env python3
"""Reference of P5-06 "Figures" (docs/specs/p5-06-figures.md r3, generator 0.27.0, schema 12, history report v8),
written from the spec alone, without the engineer's branch (p5/06-impl).

What this module holds (phase 1):
  - the formulas of 6 and 7 as functions (reigns, lives, the temper normalization, the split bonus, the generals'
    win chance, the hero's hunt, the regnal reuse, epithets, roman numerals, the tower glyph, the undead thresholds
    of 25.3) with hand-checked units (`units`);
  - the replay of sections 5 to 7 (Figures, installed in p3_conflict_reference.World after the alliances, whose
    seams it uses; the seams of 7 in conflict, peoples, monsters and alliances; the riders of 25.1 and 25.3 behind
    the peoples flag nofade and the conflict flag riders);
  - a row model of a world's history (FW) and, on it, the invariants of 13 that rows can show (1 to 9 and 11 to 16)
    and the reader checks of 9.4 as row checks with their messages (`check-world`, on any world file of schema 12:
    C++ or reference; `res_problems` on a replayed history);
  - the fixtures figures-small (18.3) and zero (4.4), I0 (7.11) by replay (`fixtures`, `i0-world`);
  - the golden file worldgen/tests/golden/p5-06-figures.txt (17): `golden` (integration only; never from the C++
    implementation's output alone);
  - the readings this reference takes where the spec leaves an order, a tie, a rounding or a wording open, and the
    spec gaps found (`readings`).

Commands (from the repository root):
  python3 -B worldgen/tests/reference/p5_06_figures_reference.py units
  python3 -B worldgen/tests/reference/p5_06_figures_reference.py check-world DB [DB ...]
  python3 -B worldgen/tests/reference/p5_06_figures_reference.py fixtures [--quick]
  python3 -B worldgen/tests/reference/p5_06_figures_reference.py i0-world --inputs DIR CASE... [--compare-db DIR]
  python3 -B worldgen/tests/reference/p5_06_figures_reference.py world --inputs DIR CASE [--small] [--out DB]
  python3 -B worldgen/tests/reference/p5_06_figures_reference.py golden [PATH|-] --inputs DIR
  python3 -B worldgen/tests/reference/p5_06_figures_reference.py readings

Integers only. The module itself uses no syntax newer than Python 3.8; the reference chain it runs in
(p3_data_reference.py and the others) needs Python 3.11 for tomllib.
"""

import hashlib
import os
import sqlite3
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

REPO = os.path.normpath(os.path.join(_HERE, "..", "..", ".."))
GOLDEN = os.path.join(REPO, "worldgen", "tests", "golden", "p5-06-figures.txt")
GENERATOR_VERSION = "0.27.0"
SCHEMA_VERSION = 12

# 3, 14: appended to streams.hpp after hist.alliances.mine (the 82nd), in this order: the 83rd to 97th
STREAMS = ("hist.figures.reign", "hist.figures.regnal", "hist.figures.life", "hist.figures.appoint",
           "hist.figures.skill", "hist.figures.fall", "hist.figures.exile", "hist.figures.hero",
           "hist.figures.caster", "hist.figures.book", "hist.figures.blight", "hist.figures.tower_name",
           "hist.names.person", "hist.names.book", "hist.monsters.hero_fall")
STREAMS_BEFORE = 82
# 8.1: the new kinds by id (final)
KINDS = ((79, "came_to_rule"), (80, "figure_died"), (81, "succession_disputed"), (82, "exiled"),
         (83, "reigned_at_end"), (84, "general_appointed"), (85, "battle_led"), (86, "fell_in_battle"),
         (87, "hero_rose"), (88, "monster_slain_by_hero"), (89, "hero_slain"), (90, "tower_founded"),
         (91, "book_written"), (92, "blight"))
KIND_ID = {k: i for i, k in KINDS}
FIGURE_KIND_KEYS = tuple(k for _i, k in KINDS)
CIV, SETTLEMENT, REGION, FIGURE, MONSTER, SITE = 3, 4, 5, 6, 8, 9
RULER, CLAIMANT, GENERAL, HERO, CASTER = 1, 2, 3, 4, 5                  # 9.1 role
ROLE_WORDS = {1: "ruler", 2: "claimant", 3: "general", 4: "hero", 5: "caster"}
ALIVE, EXILE, DIED, FELL, SLAIN, FELL_WITH_REALM = 1, 2, 3, 4, 5, 6       # 9.1 status
TEMPERS = ("steady", "warlike", "peaceable", "grasping", "just")         # 9.1 temper 0 to 4
HOW_AGE, HOW_CRISIS, HOW_EXILE = 0, 1, 2                                 # 8.1 how
SITE_TOWER = 4
RELATION_KINDS = ("ruled", "served", "rival", "parent")                 # 9.2
# 11.3: browser groups (P4-05 4.4 amended): figures = 8 appended; ALL_GROUPS 0x1FF
GROUP_SETTLEMENTS, GROUP_WAR, GROUP_CALAMITY, GROUP_MONSTERS, GROUP_FIGURES = 1, 2, 3, 5, 8
ALL_GROUPS = 0x1FF
GROUP_OF_NEW = dict([(k, GROUP_FIGURES) for k in (79, 80, 81, 82, 83, 84, 86, 87, 89, 90, 91)] +
                    [(85, GROUP_WAR), (88, GROUP_MONSTERS), (92, GROUP_CALAMITY)])
# 11.4: the tower marker (entry type 6 site, glyph 16 + kind)
GLYPH_TOWER = 20
MARKER_TOWER = (120, 60, 180)
SITE_GLYPH_OFFSET = 7
TOWER_ROWS = ("..hhh..", ".hhKhh.", ".hKFKh.", ".hKFKh.", ".hKFKh.", ".hKKKh.", ".hhhhh.")
# 10.2, 25.1 (e), 25.2: constants of other sections
SUMMARY_FIGURES_MAX = 6
OLD_RUIN_YEARS = 1500
SUMMARY_CALAMITY_MAJOR_PERMILLE = 100
SUMMARY_CALAMITY_MAJOR_MAX = 8
FIGURES_MAX = 100000
BOOKS_MAX = 100000

READINGS = """\
Spec gaps that can make the engine write a row its own reader (9.4) or an invariant of 13 refuses:
G1 (7.7, 6.2) a hero slain by monster m1 in a T4 is still FigureView::hero_of(c) for the hunts of a later monster m2
   in the same T4 (figures reads the hero_slain only in its own T4, after the monsters): m2's hunt may then write a
   monster_slain_by_hero or a second hero_slain naming a dead hero (invariant 6; F15 for a second death). Reading:
   the monsters subsystem treats a hero named by a hero_slain it wrote in this step as no hero for the rest of the
   step (MonstersSubsystem keeps the step's slain heroes; 0.26.0's behaviour for c after that).
G2 (6.6 standing down, 9.4 L7) a war that ends by search_abandoned (P3-06 20.17, a truce's bookkeeping; P5-04's
   reading A3) closes its served rows with war_end_event(w) = the search_abandoned, which L7's list (peace, war_won,
   truce, civ_conquered, civ_ended, fell_in_battle, figure_died, b's ending_event) does not hold. Reading: L7
   accepts search_abandoned as a war's end (this reference's row check does); the spec should add it.
G3 (6.6 vacancies, 6.9) a general that dies of age while serving: 6.9 closes its served row, but 6.6 does not say
   that the war's general of that side becomes 0; general_of would then name a dead figure (invariant 5: its served
   row is closed). Reading: the side's general becomes 0 at the death, with no vacancy (6.6's vacancy is for a
   general that fell); likewise a general whose served row closes at its realm's end.
G4 (6.4 rule 4, 9.1 F15, 9.4 L4) a ruler named by rule 4 at a civ_ended or civ_conquered gets ruled opened and closed
   by the same event (since = until): L3 and L4 hold, L5's interval is the single event; stated for the engineer
   (no reader refuses it).
G5 (6.5 item 2, 6.3) a split with name_rulers false still starts reign 1 of c' at Y (6.3: the first reign starts at
   the claimant's secession) with its draws: the reading this reference takes (else c' has no temper for 7.2).
G7 (4.4, 9.4 F26) skill_min is int(0, 1000) and a general's skill is skill_min + pick(...): with skill_min 0 a skill
   of 0 can be drawn, which F26 (1 <= skill <= 1000) refuses. Either skill_min's range starts at 1 or F26 reads
   0 <= skill; the shipped 100 never meets it.
G6 (9.4 L7) served closed by "b's ending_event": a general whose realm ends while its war is open (the realm is the
   side) is closed by standing down with war_end_event = the civ_conquered or civ_ended (in the list).
The replay's readings (where the spec leaves an order, a tie, a rounding or a wording open; integration compares):
R1 (6.2) every step reads the log from its cursor in ascending id; events the subsystem writes in the same pass
   are read by its next step and skipped (none of them is a kind it acts on).
R2 (6.2, 6.5) a civ_seceded citing a crisis is handled with that crisis (6.5) when the crisis is read and skipped
   when the cursor reaches it; the civ_seceded is the first one after the crisis that cites it.
R3 (6.4 rule 3) alliance_betrayed is read after its war's war_declared (P5-04 writes it after the declaration):
   the betrayer's ruler is already named by the declaration; the betrayal adds only the tally.
R4 (6.3) a reign whose planned end lies before Y at a Q4 (a named ruler's death that waited) ends at Y; an unnamed
   reign never waits, so its next reign's start s = e always lies in the step.
R5 (6.4) a ruler's born = max(awakening_year, s - age(n)) with the race row's awakening_year (the chronology's).
R6 (6.9) the place of a death, by role first: an exile (status 2): the host's capital region while the host is
   alive, else the host's capital region at the exile, else (in the wilds) the region of its start event; a caster:
   its tower's region; any other figure: its realm's capital region while the realm is alive (settlement: that
   capital for a ruler), else the region of its start event. A ruler's death at a crisis (how 1) likewise.
R7 (6.6) "w's goal region" is the war record's goal at the appointment (re-chosen goals included).
R8 (6.6) at a vacancy the side's draw is made every T3 while the war is open and no general was named since the
   fall (v_c + 1 each year).
R9 (6.6) "not serving another war": the general is no war's serving general at the appointment; the candidate
   with the smallest id among the living generals of c within the career window.
R10 (6.6 battles) a battle_led's general participants are taken in seq order (winner_general, then loser_general);
   each its tally then its fall draw.
R11 (6.8) a caster risen in a T3 may write a book and cause a blight in the same T3 (each living caster).
R12 (6.8) the rise's checks in order: the chance draw; a capital; a library (city, no open tower site of kind 4 at
   t) or the height of t's region (none: no rise, no figure); then the figure (its life draws and name), then the
   site id and the site's name.
R13 (6.8 books) in Q4 the draw is made first, then the budget of the caster's realm is tested (an exhausted budget:
   no book, the draw is lost).
R14 (6.10) b(t) counts the figures earlier in closing order whose epithet is t, however earned; a living figure's
   deed values at E (reign end E for a living ruler, E for founded_years of a realm alive at E).
R15 (6.10) {slayer} with v = 0 (nothing slain) gives no epithet (v < min); the kind of the largest might, the last
   slain at a tie.
R16 (6.11) a fallback person name (P3-02 4.3's numbered form) is not in N(c) (6.14) and its root is the name less
   the form's literal parts and the numeral.
R17 (6.14) N(c) gains a base name at each recorded ruler of c not named by the fallback, in creation order; a reuse
   increments k(x).
R18 (6.5 item 3) the host: alive, visible (created before this T4), not c, not at_war(c, h) (joined rows included),
   hops(c, h) <= exile_hops with hops over the presences of P3-06 5.1 at T4 (the conflict reference's presence) and
   PeoplesView::hops (open borders); the largest disposition_of(c, h), then the smaller id.
R19 (6.9, 6.3) a ruler's age at death in figure_died is Y - born; a death that waited for a budget is dated at the
   step that writes it.
R20 (6.12) the present rulers in T6 of E - 1 after the log and standing down; the epithets after them.
R21 (6.13) civ_founded read in a quiet step (none occurs: awakenings tick) names no founder there: the reign starts
   unnamed (a guard).
R22 (7.6) battle_led's ally participants as P5-04 7.7's (only allies with a force above 0), casualties as P5-04 7.7
   when an ally has a force, else 7.4's; G_x and G_y by the battle's principals x (the battle's initiator) and y.
R23 (7.7) with p503 off (before 0.24.0's hunt rule) a hero adds hero_hunt_permille to hunt_permille; never used.
R24 (8.2, reading F-D1) the cause words battle_led=<word> right after allied_battle=<word>, and blight=blight right
   after battle_led= in kinds 10, 16 and 31.
R25 (4.2, reading F-D2) the five culture fields are in the naming record, after site_forms (where site_forms is).
R26 (4.3, reading F-D3) the grammar `epithet` excludes the empty string (a required field); 9.1 F3 allows an empty
   column apart.
R27 (4.4, reading F-D4) the checks of 4.4 report the first failing entry of each check.
R28 (25.2) "the candidates of 24.3 item 1" are read here as every famine, plague and plague_outbreak event (the
   report reference's candidate filter is applied at integration); the census row of largest year < year(e).
R29 (6.9) a non-ruler's death draws are made at its creation (D_at at its entry year), for a claimant with the
   accession_* fields as A and S.
R30 (6.4 rule 4) the fallen ruler's death_settlement is 0 (the rule names only the region).
R31 (9.4 F22) F22 tests the earlier rows of the name (role 1 and the row's civ), not the row's own role: a
   non-ruler that shares an earlier ruler's name of its own realm is reported by F23 (regnal 0 but k earlier).
R32 (9.4) reader checks run in 9.4's order (figure rows, book rows, relation rows of the four kinds in key order,
   then events, then the amended site and monster checks); this reference lists every failure, the first is the
   reader's message.
R34 (6.2) the reign tallies follow the step order: a T2 declaration or betrayal is read in T3's log before the lines,
   so it counts for (and names) the reign current at the start of the year, also when that reign ends in the same
   T3; T6's peace, war_won and conquests count for the reign current after T3.
R35 (6.4 rule 3) a deed event of a realm already ended (its ending event before the deed) names no ruler (a guard;
   none occurs).
R36 (6.5) a crisis in the accession year of a reign already named (a founder named in that year): f_w is that ruler,
   its ruled row stays (not reopened with the dispute); no parent row (no ruler died).
R37 (6.6) a general's "first command" is its office_from (the year of its first general_appointed); a re-appointment
   keeps its start_event and office_from.
R38 (6.8) "no open site of kind 4 has t as its settlement" reads every kind 4 site with ending 0 (towers never end).
R39 (6.8) T3's casters in this order: the rises over every alive visible realm, then the books of every living
   caster, then the blights of every living caster (each loop ascending).
R40 (6.1, 6.9) Q4: the lines before the deaths of age (6.1's order); a death of age in Q4 waits while the budget of
   the figure's realm is 0.
R41 (7.2, 7.3) the strain's floors in this order: rate x allied_strain_permille div 1000 x temper_allied_permille div
   1000 (allied pairs), then x strain_pm div 1000.
R42 (7.7) S_c = H x prowess div 1000 x (1000 + might) div 1000, left to right.
R43 (25.3) R is the number of region rows of the world (islets included), as P5-03a's.
R44 (7.8) P5-02's "taken" cells include every open site but mines (towers and libraries: a tower's cell is taken).
R45 (6.6) a war closed by a side's end (conquest or civ_ended) stands its generals down at the next T6 or Q4 with
   war_end_event = that ending event.
R33 (18.3) figures-small adds P5-02's monsters-small edits (every monster kind's wake_rate x 2000 up to 10^7,
   living_permille 1000, overrun_permille 1000) to the alliances stress data, so that heroes rise, slay and fall on
   the small maps; the spec's list names the alliances stress data only."""

REPLAY_TODO = """\
The replay of P5-06 inside p3_conflict_reference.py's World (the conflict flags figures and riders and the peoples
flag nofade, on from 0.27.0):
  - Figures (this module): installed after the alliances in T3 (the only subsystem of T3), T4, T6 and Q4; the log
    (6.2), lines (6.3), naming (6.4) with regnal reuse (6.14), disputes and exiles (6.5), generals (6.6), heroes
    (6.7), casters (6.8), lives and deaths (6.9), epithets (6.10), names (6.11), the present (6.12); its FigureView
    (7.1): strain_permille, allied_strain_permille, spoils_permille, split_bonus_permille, general_of, hero_of,
    figure_exists;
  - the seams: conflict's strain (7.2, 7.3 in World.strain_update), the generals' skill and battle_led in
    World.fight (7.6), housekeeping and conquest on civilization pairs only (7.5), BATTLES() with battle_led;
    alliances' grant (grant_chance's temper term, 7.3); peoples' split bonus (split_plan, 7.4); monsters' hunts with
    a hero (Monsters.hunts, hero_of, 7.7), hero_fall; site kind 4 through Monsters.sites (7.8), the occupied test of
    kinds 1 and 2 (occupies, 7.8);
  - the riders: no ruin fades (p3_peoples_reference.FLAGS nofade, 25.1 (a)); the two undead rules
    (p5_02_monsters_reference.undead_threshold_027, 25.3); 25.2 is report text: major_calamities() here;
  - I0 (7.11): `fixtures` (figures-small, the zero tuning on stress seeds against the same data without the
    subsystem) and `i0-world` (world cases: the zero tuning with the riders off against 0.26.0's rules and data;
    with the riders on against a 0.26.0 world file's tables, up to the first ruin_faded event).
Not replayed: the report text of 10 and 25.2's summary set as a whole (the summary candidates are the report
reference's), the browser and markers (only the constants of 11.3, 11.4 here)."""


def floor_div(a, b):
    assert b > 0
    return a // b


def clamp(v, lo, hi):
    return min(hi, max(lo, v))


def _pnr():
    import p3_names_reference as pnr
    return pnr


def _pcr():
    import p3_conflict_reference as pcr
    return pcr


def chance_pm(u, p):
    import p3_peoples_reference as pp
    return pp.chance_pm(u, p)


def chance_ppm(u, p):
    import p3_peoples_reference as pp
    return pp.chance_ppm(u, p)


def pick(u, n):
    return _pnr().pick(u, n)


def weighted(u, ws):
    return _pnr().weighted(u, ws)


def roman(k):
    return _pnr().roman(k)


# ---------------------------------------------------------------------------
# The formulas (6, 7) as functions
# ---------------------------------------------------------------------------

def reign_values(g, lam, t, u_len, u_age, u_temper, weights):
    """6.3: (len, age, temper) of a reign from its three draws."""
    ln = max(1, g * (t["reign_min_permille"] + pick(u_len, t["reign_span_permille"] + 1)) // 1000)
    age = lam * (t["accession_age_permille"] + pick(u_age, t["accession_span_permille"] + 1)) // 1000
    tau = weighted(u_temper, list(weights))
    return ln, age, tau


def life_values(lam, aw, y, A, S, t, u0, u1):
    """6.9: (born, death) of a non-ruler entering at y."""
    age0 = lam * (A + pick(u0, S + 1)) // 1000
    born = max(aw, y - age0)
    death = max(y + 1, born + lam * (t["life_min_permille"] + pick(u1, t["life_span_permille"] + 1)) // 1000)
    return born, death


def temper_mean(weights, tsp):
    """7.2: mean_r = sum w_t x f_t div sum w_t (once per race)."""
    return sum(w * f for w, f in zip(weights, tsp)) // sum(weights)


def strain_factor(tau, weights, tsp):
    """7.2: strain_pm(c) = temper_strain_permille[tau] x 1000 div max(1, mean_r)."""
    return tsp[tau] * 1000 // max(1, temper_mean(weights, tsp))


def split_bonus(Y, s, g, t):
    """7.4: short_reign_split_permille when the reign started less than short_reign_permille x g div 1000 years
    before Y."""
    return t["short_reign_split_permille"] if Y - s < t["short_reign_permille"] * g // 1000 else 0


def led_win_chance(A, D, dv, skill_x, skill_y):
    """7.6: A' = A (1000 + skill x) div 1000; p = A' 1000 div (A' + D' (1000 + skill y) div 1000); 500 at 0."""
    A1 = A * (1000 + skill_x) // 1000
    D1 = D * dv // 1000 * (1000 + skill_y) // 1000
    return A1 * 1000 // (A1 + D1) if A1 + D1 > 0 else 500


def hero_hunt(hunt_permille, hunt_max_permille, hunt_raid_permille, R, H, prowess, hero_hunt_pm, hero_might_pm):
    """7.7: (p_hunt, S_c) with a hero (hero_hunt_pm, hero_might_pm; 0, 0 without one)."""
    p = min(hunt_max_permille + hero_hunt_pm, hunt_permille + (max(1, R) - 1) * hunt_raid_permille + hero_hunt_pm)
    S = H * prowess // 1000 * (1000 + hero_might_pm) // 1000
    return p, S


def undead_threshold(min_dead, permille, P_W, M, R, regions_ref):
    import p5_02_monsters_reference as p502
    return p502.undead_threshold_027(min_dead, permille, P_W, M, R, regions_ref)


def display_name(name, regnal, epithet):
    """6.10, 6.14: name, ' ' + roman(regnal) when regnal >= 2, ' ' + epithet when not empty."""
    out = name
    if regnal >= 2:
        out += " " + roman(regnal)
    if epithet:
        out += " " + epithet
    return out


def epithet_of(role, values, slayer_text, entries, borne, repeat_max):
    """6.10 for one figure: entries tried in list order; values: deed -> v; borne: text -> bearers earlier in
    closing order. Returns (text, how) with how 'substantial', 'ordinary' or '' (none)."""
    for e in entries:
        if e["role"] != role:
            continue
        v = values.get(e["deed"], 0)
        t = e["text"].replace("{slayer}", slayer_text or "")
        if e["text"] == "{slayer}" and not slayer_text:
            continue
        if e["substantial"] > 0 and v >= e["substantial"]:
            return t, "substantial"
        if v >= e["min"] and borne.get(t, 0) < repeat_max:
            return t, "ordinary"
    return "", ""


def regnal_candidates(names, regnal_max, registry):
    """6.14: E_c = the names x of N(c) (in N(c)'s order) with k(x) < regnal_max and x + ' ' + roman(k(x) + 1) not in
    the registry."""
    return [x for x, k in names if k < regnal_max and (x + " " + roman(k + 1)) not in registry]


def is_fallback_name(name):
    """6.14: a P3-02 4.3 fallback name ends in ' ' and a Roman numeral >= II."""
    if " " not in name:
        return False
    tail = name.rsplit(" ", 1)[1]
    return any(roman(k) == tail for k in range(2, 400))


def tower_glyph_cells():
    """11.4: (dx, dy) -> class (1 halo, 2 fill, 3 ink) of the tower glyph, from its rule (P4-04 5's halo rule: the
    cells within one step, eight-neighbourhood, of a fill or ink cell that are neither, inside radius 3)."""
    cls = {}
    for dy in range(-3, 4):
        for dx in range(-3, 4):
            if dx == 0 and -1 <= dy <= 1:
                cls[(dx, dy)] = 2
            elif (abs(dx) == 1 and -1 <= dy <= 2) or (dx == 0 and dy in (-2, 2)):
                cls[(dx, dy)] = 3
    for dy in range(-3, 4):
        for dx in range(-3, 4):
            if (dx, dy) in cls:
                continue
            if any(cls.get((dx + i, dy + j), 0) in (2, 3) for i in (-1, 0, 1) for j in (-1, 0, 1)):
                cls[(dx, dy)] = 1
    return cls


def tower_rows_from_rule():
    cls = tower_glyph_cells()
    ch = {1: "h", 2: "F", 3: "K"}
    return tuple("".join(ch.get(cls.get((dx, dy), 0), ".") for dx in range(-3, 4)) for dy in range(-3, 4))


def site_glyph_offset(kind, settlement):
    """11.4: the drawn centre's offset from the site's cell: a mine (+7, -7), a library (-7, -7), a wild tower
    (0, 0)."""
    if kind == 3:
        return (SITE_GLYPH_OFFSET, -SITE_GLYPH_OFFSET)
    if kind == 4:
        return (-SITE_GLYPH_OFFSET, -SITE_GLYPH_OFFSET) if settlement else (0, 0)
    return (0, 0)


def marker_hover(site_name, figure_display, year, library):
    """11.4: the hover text of a tower or library."""
    return "%s (%s of %s), since %d" % (site_name, "library" if library else "tower", figure_display, year)


# ---------------------------------------------------------------------------
# The subsystem (5, 6) and its FigureView (7.1)
# ---------------------------------------------------------------------------

def _role_seq(w, key, role):
    return [q["seq"] for q in w.cat_by_key[key]["roles"] if q["role"] == role][0]


def _parts(w, ev, role):
    seq = _role_seq(w, ev["key"], role)
    return [x for (_i, s, _t, x) in ev["participants"] if s == seq]


def _one(w, ev, role):
    p = _parts(w, ev, role)
    return p[0] if p else 0


class Figures:
    """FiguresSubsystem (5, 6) and its FigureView (7.1) for a conflict World w."""

    def __init__(self, w, dat, inp):
        import core_reference as cr
        import p3_data_reference as pdr
        self.w = w
        self.t = dat.figures
        self.dat = dat
        self.inp = inp
        for s in STREAMS:
            w.keys[s] = cr.stream_key(inp.seed, s)
        self.E = inp.params["span"]
        self.figs = []           # figure records, id = index + 1 (5)
        self.lines = {}          # civ -> line (5)
        self.wars = {}           # war_declared id -> record (5; std::map)
        self.hero = {}           # civ -> living hero (0 none)
        self.books = []          # book records (5)
        self.cursor = 1
        self.skip = set()        # civ_seceded events handled with their crisis (R2)
        self.log = []            # (year, what, ...) for the checks (not state)
        # data: the cultures' 0.27.0 fields (4.2), by culture key; the monsters' slayer epithets (4.3)
        root = getattr(dat, "root", None) or getattr(dat.base, "root", None) or "game/data"
        content = pdr.load_root(root, kinds=("culture",))
        self.cul = {k: c["naming"] for k, c in content["culture"]}
        self.mean = {}
        for rid, r in dat.race.items():
            if r.get("temper_weights"):
                self.mean[rid] = temper_mean(r["temper_weights"], self._rec("temper_strain_permille"))

    # --- tuning helpers
    def _rec(self, name):
        return tuple(self.t[name][x] for x in TEMPERS)

    def race_row(self, race_id):
        return self.w.race[race_id]

    def lam(self, race_id):
        return self.race_row(race_id)[6]

    def gen(self, race_id):
        return self.race_row(race_id)[7]

    def awakening(self, race_id):
        a = self.race_row(race_id)[10]
        return a if a is not None else 0

    def civ(self, c):
        return self.w.civ(c)

    def culture_key(self, c):
        return self.w.culture[self.civ(c)["culture"]][1]

    def quiet(self):
        return self.w.quiet_used is not None

    def Z(self):
        return self.w.year + self.w.years - 1

    # --- draws (3)
    def D(self, s, a, b, k):
        return self.w.draw(s, a, b, k)

    def D_at(self, s, y, a, b, k):
        """7.10: StepContext::draw_at."""
        w = self.w
        Y, Z = w.year, self.Z()
        if not (Y <= y <= Z):
            _pcr().fatal("history: draw_at: year %d outside the step %d-%d" % (y, Y, Z))
        return _pnr().draw(w.keys[s], y, a, b, k)

    # --- the FigureView (7.1)
    def reign_temper(self, c):
        L = self.lines.get(c)
        return None if L is None else L["temper"]

    def strain_permille(self, c):
        tau = self.reign_temper(c)
        if tau is None:
            return 1000
        r = self.civ(c)["race"]
        return self._rec("temper_strain_permille")[tau] * 1000 // max(1, self.mean.get(r, 1000))

    def allied_strain_permille(self, c):
        tau = self.reign_temper(c)
        return 1000 if tau is None else self._rec("temper_allied_permille")[tau]

    def spoils_permille(self, c):
        tau = self.reign_temper(c)
        return 0 if tau is None else self._rec("temper_spoils_permille")[tau]

    def split_bonus_permille(self, c):
        L = self.lines.get(c)
        if L is None:
            return 0
        return split_bonus(self.w.year, L["s"], self.gen(self.civ(c)["race"]), self.t)

    def general_of(self, war, c):
        k = self.wars.get(war)
        if k is None or c not in k["gen"] or not k["gen"][c]:
            return (0, 0)
        f = k["gen"][c]
        return (f, self.fig(f)["skill"])

    def hero_of(self, c):
        f = self.hero.get(c, 0)
        if not f:
            return (0, 0, 0)
        return (f, self.t["hero_hunt_permille"], self.t["hero_might_permille"])

    def figure_exists(self, f):
        return 1 <= f <= len(self.figs)

    # --- figures
    def fig(self, f):
        return self.figs[f - 1]

    def alive(self, f):
        return self.fig(f)["status"] in (ALIVE, EXILE)

    def new_figure(self, role, c, office_from, born, death_plan, reign_no=0, temper=0):
        w = self.w
        if len(self.figs) >= FIGURES_MAX:
            _pcr().fatal("history: bound: figures exceed 100000")
        cv = self.civ(c)
        f = dict(id=len(self.figs) + 1, name="", epithet="", role=role, race=cv["race"], culture=cv["culture"],
                 civ=c, born=born, office_from=office_from, start_event=0, death_plan=death_plan, died=None,
                 death_event=0, death_region=0, death_settlement=0, status=ALIVE, reign_no=reign_no, regnal=0,
                 temper=temper, skill=0, place=0, host=0, tower=0, root="", fallback=False,
                 wars_declared=0, wars_won=0, conquests=0, betrayals=0, battles_won=0, battles_lost=0, slain=[],
                 books=0, blights=0, exiled=0, realm_fell=0, reign_end=None)
        self.figs.append(f)
        return f

    def person_name(self, f):
        """6.11: P3-02 4.3 with kind person, the culture's person_forms, entity id the figure id."""
        import p5_02_monsters_reference as p502
        pnr = _pnr()
        cnum = f["culture"]
        ck = self.w.culture[cnum][1]
        forms = self.cul[ck]["person_forms"]
        d = pnr.with_forms(self.w.dat.naming[ck], "person", forms)
        name, t = pnr.make_name(d, ck, "person", self.inp.seed, cnum, f["id"], self.w.registry)
        f["name"], f["fallback"] = name, t == "F"
        base = name
        if f["fallback"]:
            base = name.rsplit(" ", 1)[0]
        f["root"] = p502.realm_root(base, forms) if forms != ["{root}"] else base

    def non_ruler(self, role, c, A, S):
        """6.9: a new claimant, general, hero or caster of c entering at Y, with its life draws and name."""
        Y = self.w.year
        r = self.civ(c)["race"]
        fid = len(self.figs) + 1
        born, death = life_values(self.lam(r), self.awakening(r), Y, A, S, self.t,
                                  self.D_at("hist.figures.life", Y, fid, 0, 0),
                                  self.D_at("hist.figures.life", Y, fid, 0, 1))
        f = self.new_figure(role, c, Y, born, death)
        self.person_name(f)
        return f

    # --- lines (6.3)
    def start_reign(self, c, n, s):
        r = self.civ(c)["race"]
        ln, age, tau = reign_values(self.gen(r), self.lam(r), self.t,
                                    self.D_at("hist.figures.reign", s, c, n, 0),
                                    self.D_at("hist.figures.reign", s, c, n, 1),
                                    self.D_at("hist.figures.reign", s, c, n, 2),
                                    self.dat.race[r]["temper_weights"])
        old = self.lines.get(c)
        names = old["names"] if old is not None else []
        self.lines[c] = dict(n=n, s=s, e=s + ln, temper=tau, age=age, f=0, names=names,
                             tally=dict(wars_declared=0, wars_won=0, conquests=0, betrayals=0))
        self.log.append((self.w.year, "reign", c, n, s, s + ln, tau, age))

    def name_reign(self, c):
        """6.4's naming of c's current reign (figure only; the caller opens ruled with the record event). 6.14's
        reuse before 6.11's draw."""
        w = self.w
        L = self.lines[c]
        n = L["n"]
        r = self.civ(c)["race"]
        born = max(self.awakening(r), L["s"] - L["age"])
        f = self.new_figure(RULER, c, L["s"], born, L["e"], reign_no=n, temper=L["temper"])
        ck = self.culture_key(c)
        rp = self.cul[ck]["regnal_permille"]
        E_c = regnal_candidates(L["names"], self.t["regnal_max"], w.registry)
        reused = False
        if rp > 0 and E_c and chance_pm(self.D("hist.figures.regnal", c, n, 0), rp):
            i = pick(self.D("hist.figures.regnal", c, n, 1), len(E_c))
            x = E_c[i]
            for ent in L["names"]:
                if ent[0] == x:
                    f["regnal"] = ent[1] + 1
                    ent[1] += 1
            f["name"], f["root"] = x, x
            w.registry.add(x + " " + roman(f["regnal"]))
            reused = True
        if not reused:
            self.person_name(f)
            if not f["fallback"]:
                L["names"].append([f["name"], 1])
        L["f"] = f["id"]
        for k, v in L["tally"].items():
            f[k] += v
        L["tally"] = dict(wars_declared=0, wars_won=0, conquests=0, betrayals=0)
        self.log.append((w.year, "named", c, n, f["id"], f["name"], f["regnal"]))
        return f

    def open_ruled(self, f, e):
        f["start_event"] = e
        self.w.open_rel((FIGURE, f["id"], CIV, f["civ"], "ruled"), e)

    def tally(self, c, key):
        L = self.lines.get(c)
        if L is None:
            return
        if L["f"]:
            self.fig(L["f"])[key] += 1
        else:
            L["tally"][key] += 1

    def name_by_deed(self, c, e):
        """6.4 rule 3: the realm's reign is named with the deed event e as its record (no event)."""
        if not self.t["name_rulers"]:
            return
        L = self.lines.get(c)
        if L is None or L["f"] or self.civ(c)["ended_year"] is not None and self.civ(c)["ending_event"] < e:
            return
        f = self.name_reign(c)
        self.open_ruled(f, e)

    # --- deaths (6.9)
    def place_of(self, f):
        """Reading R6: (region, settlement) of f's death of age or at a crisis."""
        w = self.w
        cv = self.civ(f["civ"])
        if f["status"] == EXILE:
            if f["host"] and w.civ(f["host"])["ended_year"] is None:
                return w.stl(w.civ(f["host"])["capital"])["region"], 0
            if f["place"]:
                return f["place"], 0
            return w.events[f["start_event"] - 1]["region"], 0
        if f["role"] == CASTER and f["tower"]:
            return w.monsters.site(f["tower"])["region"], 0
        if cv["ended_year"] is None:
            cap = w.stl(cv["capital"])
            return cap["region"], (cap["id"] if f["role"] == RULER else 0)
        return w.events[f["start_event"] - 1]["region"], 0

    def close_all(self, fid, e, kinds=("ruled", "served", "rival")):
        w = self.w
        keys = [k for k in w.relations if k[4] in kinds and ((k[0] == FIGURE and k[1] == fid) or
                                                            (k[2] == FIGURE and k[3] == fid))]
        for k in sorted(keys, key=_pcr().key_order):
            w.close_rel(k, e)

    def end_life(self, f, status, e, region, settlement):
        Y = self.w.year
        f["status"], f["died"], f["death_event"] = status, Y, e
        f["death_region"], f["death_settlement"] = region, settlement
        if f["role"] == RULER and f["reign_end"] is None:
            f["reign_end"] = Y
        self.close_all(f["id"], e)
        for kw in self.wars.values():          # reading G3: a dead general serves no war
            for c in kw["gen"]:
                if kw["gen"][c] == f["id"]:
                    kw["gen"][c] = 0
        if self.hero.get(f["civ"]) == f["id"]:
            self.hero[f["civ"]] = 0
        self.log.append((Y, "died", f["id"], status, e))

    def die(self, f, how):
        """6.9: figure_died (how 0 of age, 1 at a crisis, 2 in exile)."""
        w = self.w
        region, settlement = self.place_of(f)
        if f["status"] == EXILE and how == HOW_AGE:
            how = HOW_EXILE
        e = w.append("figure_died", region=region, settlement=settlement,
                     participants=(("figure", FIGURE, f["id"]), ("civilization", CIV, f["civ"])),
                     values=(how, w.year - f["born"]), causes=(f["start_event"],))
        self.end_life(f, DIED, e, region, settlement)
        return e

    # --- the steps (6.1)
    def t3(self):
        self.read_log()
        self.lines_t3()
        self.deaths(quiet=False)
        self.vacancies()
        self.casters()

    def t4(self):
        self.read_log()

    def t6(self):
        self.read_log()
        self.stand_down()
        if self.w.year == self.E - 1:
            self.present()
            self.epithets()

    def q4(self):
        self.read_log()
        self.stand_down()
        self.lines_q4()
        self.deaths(quiet=True)
        self.books_step(quiet=True)

    # --- 6.2 the log
    def read_log(self):
        w = self.w
        last = len(w.events)
        first = self.cursor
        self.cursor = last + 1
        for e in range(first, last + 1):
            ev = w.events[e - 1]
            k = ev["key"]
            if k == "civ_founded":
                self.founded(ev, e)
            elif k == "succession_crisis":
                self.crisis(ev, e, first, last)
            elif k == "civ_seceded":
                if e in self.skip:
                    continue
                if not any(w.events[x - 1]["key"] == "succession_crisis" for x in ev["causes"]):
                    self.founded(ev, e, seceded=True)
            elif k == "war_declared":
                a, b = _one(w, ev, "attacker"), _one(w, ev, "defender")
                self.name_by_deed(a, e)
                self.tally(a, "wars_declared")
                self.declared(e, a, b)
            elif k == "alliance_betrayed":
                a = _one(w, ev, "betrayer")
                self.name_by_deed(a, e)
                self.tally(a, "betrayals")
            elif k in ("spoils_shared", "spoils_withheld"):
                self.name_by_deed(_one(w, ev, "principal"), e)
            elif k in ("peace", "war_won"):
                self.tally(_one(w, ev, "winner"), "wars_won")
            elif k == "civ_conquered":
                self.tally(_one(w, ev, "conqueror"), "conquests")
                self.realm_end(_one(w, ev, "civilization"), e)
            elif k == "civ_ended":
                self.realm_end(_one(w, ev, "civilization"), e)
            elif k == "battle_led":
                self.battle(ev, e)
            elif k == "monster_raid":
                self.raid(ev, e)
            elif k == "monster_slain_by_hero":
                f = self.fig(_one(w, ev, "hero"))
                m = w.monsters.monsters[_one(w, ev, "monster") - 1]
                f["slain"].append((m["kind"], w.monsters.kind(m["kind"])["might"]))
            elif k == "hero_slain":
                f = self.fig(_one(w, ev, "figure"))
                if self.alive(f["id"]):
                    self.end_life(f, SLAIN, e, ev["region"], 0)

    def founded(self, ev, e, seceded=False):
        """6.4 rule 1 (and 6.2's guard for a civ_seceded not citing a crisis)."""
        w = self.w
        c = _one(w, ev, "civilization")
        self.start_reign(c, 1, ev["year"])
        if not self.t["name_rulers"] or self.quiet():   # reading R21
            return
        f = self.name_reign(c)
        e2 = w.append("came_to_rule", region=self.civ(c)["home_region"],
                      participants=(("figure", FIGURE, f["id"]), ("civilization", CIV, c)), causes=(e,))
        self.open_ruled(f, e2)

    def realm_end(self, c, e):
        """6.4 rule 4."""
        w = self.w
        L = self.lines.get(c)
        if L is None:
            return
        if not L["f"] and self.t["name_rulers"]:
            f = self.name_reign(c)
            self.open_ruled(f, e)
        if L["f"]:
            f = self.fig(L["f"])
            ev = w.events[e - 1]
            f["realm_fell"] = 1
            self.end_life(f, FELL_WITH_REALM, e, ev["region"], 0)
            f["died"] = ev["year"]
        L["ended"] = True

    # --- 6.5
    def crisis(self, ev, e, first, last):
        w, t = self.w, self.t
        c = _one(w, ev, "civilization")
        L = self.lines.get(c)
        if L is None:
            return
        Y = w.year
        sigma = 0
        for x in range(e + 1, last + 1):
            ex = w.events[x - 1]
            if ex["key"] == "civ_seceded" and e in ex["causes"]:
                sigma = x
                break
        if sigma:
            self.skip.add(sigma)
        same_year = L["s"] == Y
        f_d = 0
        if not same_year:
            if L["f"]:
                f_d = L["f"]
                self.die(self.fig(f_d), HOW_CRISIS)
            self.start_reign(c, L["n"] + 1, Y)
        c2 = _one(w, w.events[sigma - 1], "civilization") if sigma else 0
        if sigma:
            self.start_reign(c2, 1, Y)    # reading G5: whatever name_rulers
        if not t["name_rulers"]:
            return
        if not sigma and not chance_pm(self.D("hist.figures.exile", e, c, 0), t["exile_permille"]):
            return
        if self.lines[c]["f"]:
            f_w = self.fig(self.lines[c]["f"])    # a crisis in the year of an accession that is named
            fresh_w = False
        else:
            f_w = self.name_reign(c)
            fresh_w = True
        if sigma:
            f_c = self.name_reign(c2)
            parts = (("civilization", CIV, c), ("ruler", FIGURE, f_w["id"]), ("claimant", FIGURE, f_c["id"]),
                     ("new_realm", CIV, c2))
            ed = w.append("succession_disputed", region=ev["region"], settlement=ev["settlement"],
                          participants=parts, values=(1,), causes=tuple(sorted({e, sigma})))
        else:
            f_c = self.non_ruler(CLAIMANT, c, t["accession_age_permille"], t["accession_span_permille"])
            ed = w.append("succession_disputed", region=ev["region"], settlement=ev["settlement"],
                          participants=(("civilization", CIV, c), ("ruler", FIGURE, f_w["id"]),
                                        ("claimant", FIGURE, f_c["id"])), values=(0,), causes=(e,))
            f_c["start_event"] = ed
        if fresh_w:
            self.open_ruled(f_w, ed)
        if sigma:
            self.open_ruled(f_c, ed)
        a, b = min(f_w["id"], f_c["id"]), max(f_w["id"], f_c["id"])
        w.open_rel((FIGURE, a, FIGURE, b, "rival"), ed)
        if f_d:
            for ch in (f_w, f_c):
                w.open_rel((FIGURE, f_d, FIGURE, ch["id"], "parent"), ed)
        if sigma:
            return
        h = self.host_of(c)
        if h:
            cap = w.stl(w.civ(h)["capital"])
            region = cap["region"]
        else:
            region = ev["region"]
        ex = w.append("exiled", region=region,
                      participants=(("figure", FIGURE, f_c["id"]), ("civilization", CIV, c)) +
                      ((("host", CIV, h),) if h else ()), values=(1 if h else 0,), causes=(ed,))
        f_c["status"], f_c["place"], f_c["host"], f_c["exiled"] = EXILE, (region if h else 0), h, 1
        self.log.append((Y, "exiled", f_c["id"], c, h, ex))

    def host_of(self, c):
        """6.5 item 3: the host (reading R18)."""
        w, t = self.w, self.t
        pc = w.presence(c)
        best = None
        for cv in w.civs:
            h = cv["id"]
            if h == c or cv["ended_year"] is not None or not w.visible(cv) or w.at_war(c, h):
                continue
            hp = None
            ph = w.presence(h)
            for rc in pc:
                d = w.geo.hops(rc)
                for rh in ph:
                    v = d.get(rh)
                    if v is not None and (hp is None or v < hp):
                        hp = v
            if hp is None or hp > t["exile_hops"]:
                continue
            k = (-w.disp(c, h), h)
            if best is None or k < best[0]:
                best = (k, h)
        return best[1] if best is not None else 0

    # --- 6.3 lines
    def lines_t3(self):
        w = self.w
        Y = w.year
        for c in sorted(self.lines):
            L = self.lines[c]
            if self.civ(c)["ended_year"] is not None or L.get("ended"):
                continue
            if L["e"] > Y:
                continue
            if L["f"]:
                self.die(self.fig(L["f"]), HOW_AGE)
            self.start_reign(c, L["n"] + 1, Y)

    def lines_q4(self):
        w = self.w
        Y, Z = w.year, self.Z()
        for c in sorted(self.lines):
            if self.civ(c)["ended_year"] is not None or self.lines[c].get("ended"):
                continue
            while self.lines[c]["e"] <= Z:
                L = self.lines[c]
                if L["f"]:
                    if w.quiet_events_left(c) <= 0:
                        break
                    self.die(self.fig(L["f"]), HOW_AGE)
                    s = Y
                else:
                    s = L["e"]
                    assert s >= Y, (c, s, Y)   # reading R4
                self.start_reign(c, L["n"] + 1, s)

    def deaths(self, quiet):
        """6.9: deaths of age of the living non-rulers, ascending id."""
        w = self.w
        lim = self.Z() if quiet else w.year
        for f in list(self.figs):
            if f["role"] == RULER or not self.alive(f["id"]) or f["death_plan"] > lim:
                continue
            if quiet and w.quiet_events_left(f["civ"]) <= 0:
                continue
            self.die(f, HOW_EXILE if f["status"] == EXILE else HOW_AGE)

    # --- 6.6 generals
    def declared(self, e, a, b):
        w = self.w
        self.wars[e] = dict(a=a, b=b, gen={a: 0, b: 0}, v={a: 0, b: 0}, vacant={a: False, b: False})
        if self.quiet():
            return
        for c, en in ((a, b), (b, a)):
            if self.civ(c)["ended_year"] is None:
                self.appoint(e, c, en)

    def appoint(self, war, c, enemy):
        w, t = self.w, self.t
        kw = self.wars[war]
        r = self.civ(c)["race"]
        p = self.dat.race[r]["general"] * t["general_scale_permille"] // 1000
        if not chance_pm(self.D("hist.figures.appoint", war, c, kw["v"][c]), p):
            return False
        Y = w.year
        serving = {g for k in self.wars.values() for g in k["gen"].values() if g}
        window = self.lam(r) * t["career_permille"] // 1000
        cands = [f for f in self.figs if f["role"] == GENERAL and f["civ"] == c and self.alive(f["id"])
                 and f["id"] not in serving and Y - f["office_from"] <= window]
        if cands:
            f = cands[0]
            again = 1
        else:
            f = self.non_ruler(GENERAL, c, t["general_age_permille"], t["general_age_span_permille"])
            # the lead's ruling G7: a skill is at least 1
            f["skill"] = max(1, t["skill_min"] + pick(self.D("hist.figures.skill", f["id"], 0, 0),
                                                      t["skill_max"] - t["skill_min"] + 1))
            again = 0
        e = w.append("general_appointed", region=w.war_of(war)["goal"],
                     participants=(("figure", FIGURE, f["id"]), ("civilization", CIV, c), ("enemy", CIV, enemy)),
                     values=(again,), causes=(war,))
        if not again:
            f["start_event"] = e
        w.open_rel((FIGURE, f["id"], CIV, c, "served"), e)
        kw["gen"][c] = f["id"]
        kw["vacant"][c] = False
        self.log.append((Y, "general", war, c, f["id"], again))
        return True

    def vacancies(self):
        w = self.w
        for war in sorted(self.wars):
            kw = self.wars[war]
            if not w.war_open(war):
                continue
            for c, en in ((kw["a"], kw["b"]), (kw["b"], kw["a"])):
                if kw["vacant"][c] and not kw["gen"][c]:
                    kw["v"][c] += 1
                    self.appoint(war, c, en)

    def battle(self, ev, b):
        w, t = self.w, self.t
        war = ev["causes"][0]
        W, L = _one(w, ev, "winner"), _one(w, ev, "loser")
        for role, side, other, won in (("winner_general", W, L, True), ("loser_general", L, W, False)):
            for fid in _parts(w, ev, role):
                f = self.fig(fid)
                f["battles_won" if won else "battles_lost"] += 1
                p = t["fall_winner_permille"] if won else t["fall_loser_permille"]
                if not chance_pm(self.D("hist.figures.fall", b, fid, 0), p):
                    continue
                e = w.append("fell_in_battle", region=ev["region"],
                             participants=(("figure", FIGURE, fid), ("civilization", CIV, f["civ"]),
                                           ("enemy", CIV, other)), causes=(b,))
                self.end_life(f, FELL, e, ev["region"], 0)
                kw = self.wars.get(war)
                if kw is not None:
                    kw["gen"][side] = 0
                    kw["vacant"][side] = True

    def war_end_event(self, war):
        w = self.w
        rec = w.war_of(war)
        if rec.get("end_event"):
            return rec["end_event"]
        key = _pcr().rel_key(rec["a"], rec["b"], "war")
        for op, k, e in reversed(w.rel_log):
            if op == "close" and k == key:
                return e
        for r in w.rel_rows:
            if tuple(r[:5]) == key and r[7] == war and r[8]:
                return r[8]
        _pcr().fatal("history: figures: war %d has no end event" % war)

    def stand_down(self):
        w = self.w
        for war in sorted(self.wars):
            if w.war_open(war):
                continue
            kw = self.wars[war]
            ee = None
            for c in (kw["a"], kw["b"]):
                f = kw["gen"][c]
                if f:
                    ee = ee or self.war_end_event(war)
                    w.close_rel((FIGURE, f, CIV, c, "served"), ee)
                    kw["gen"][c] = 0
            del self.wars[war]

    # --- 6.7 heroes
    def raid(self, ev, r):
        w, t = self.w, self.t
        c = _one(w, ev, "civilization")
        m = _one(w, ev, "monster")
        cv = self.civ(c)
        if cv["ended_year"] is not None or not w.visible(cv) or self.hero.get(c, 0):
            return
        p = self.dat.race[cv["race"]]["hero"] * t["hero_scale_permille"] // 1000
        if not chance_pm(self.D("hist.figures.hero", r, c, 0), p):
            return
        f = self.non_ruler(HERO, c, t["hero_age_permille"], t["hero_age_span_permille"])
        e = w.append("hero_rose", region=ev["region"], settlement=ev["settlement"],
                     participants=(("figure", FIGURE, f["id"]), ("civilization", CIV, c), ("monster", MONSTER, m)),
                     causes=(r,))
        f["start_event"] = e
        self.hero[c] = f["id"]
        self.log.append((w.year, "hero", c, f["id"], m))

    # --- 6.8 casters
    def living_caster(self, c):
        return any(f["role"] == CASTER and f["civ"] == c and self.alive(f["id"]) for f in self.figs)

    def casters(self):
        self.rises()
        self.books_step(quiet=False)
        self.blights()

    def rises(self):
        w, t = self.w, self.t
        m = w.monsters
        for cv in list(w.civs):
            c = cv["id"]
            if cv["ended_year"] is not None or not w.visible(cv) or self.living_caster(c):
                continue
            p = t["caster_ppm"] * self.dat.race[cv["race"]]["magic"] // 1000
            if not chance_ppm(self.D("hist.figures.caster", c, 0, 0), p):
                continue
            if not cv["capital"]:
                continue
            ts = w.stl(cv["capital"])
            library = ts["kind"] == 4 and not any(s["kind"] == SITE_TOWER and s["ending"] == 0 and
                                                  s["settlement"] == ts["id"] for s in m.sites)
            if library:
                cell = (ts["x"], ts["y"])
            else:
                cell = m.height(ts["region"])
                if cell is None:
                    continue
            f = self.non_ruler(CASTER, c, t["caster_age_permille"], t["caster_age_span_permille"])
            sid = len(m.sites) + 1
            name = self.tower_name(f, sid, library)
            e = w.append("tower_founded", region=ts["region"], settlement=ts["id"] if library else 0,
                         participants=(("figure", FIGURE, f["id"]), ("civilization", CIV, c), ("site", SITE, sid)),
                         values=(1 if library else 0,), causes=(max(ts["founding_event"], cv["founding_event"]),))
            f["start_event"], f["tower"] = e, sid
            self.open_site(dict(id=sid, name=name, kind=SITE_TOWER, region=ts["region"], x=cell[0], y=cell[1],
                                settlement=ts["id"] if library else 0, founder=c, monster=0, origin_year=w.year,
                                origin_event=e, ended_year=None, ending_event=0, ending=0, hostile=None))
            self.log.append((w.year, "tower", c, f["id"], sid, library))

    def open_site(self, spec):
        """7.8: MonstersSubsystem::open_site with kind 3 or 4 and the next id."""
        m = self.w.monsters
        if spec["id"] != len(m.sites) + 1 or spec["kind"] not in (3, 4):
            _pcr().fatal("history: monsters: open_site: site %d of kind %d is not the next site of kind 3 or 4"
                         % (spec["id"], spec["kind"]))
        m.sites.append(spec)

    def tower_name(self, f, sid, library):
        import p5_02_monsters_reference as p502
        w = self.w
        ck = w.culture[f["culture"]][1]
        forms = self.cul[ck]["library_forms" if library else "tower_forms"]
        n = len(forms)
        start = pick(self.D("hist.figures.tower_name", sid, 0, 0), n)
        first = None
        for j in range(n):
            nm, parts = p502.fill_form(forms[(start + j) % n], "{root}", f["root"])
            if first is None:
                first = nm
            if p502.name_problem(nm, parts) is None and nm not in w.registry:
                w.registry.add(nm)
                return nm
        k = 2
        while first + " " + roman(k) in w.registry:
            k += 1
        nm = first + " " + roman(k)
        w.registry.add(nm)
        return nm

    def books_step(self, quiet):
        w, t = self.w, self.t
        for f in list(self.figs):
            if f["role"] != CASTER or not self.alive(f["id"]) or f["books"] >= t["books_max"]:
                continue
            p = min(1000, t["book_permille"] * w.years) if quiet else t["book_permille"]
            if not chance_pm(self.D("hist.figures.book", f["id"], 0, 0), p):
                continue
            if quiet and w.quiet_events_left(f["civ"]) <= 0:   # reading R13
                continue
            self.write_book(f)

    def write_book(self, f):
        w = self.w
        if len(self.books) >= BOOKS_MAX:
            _pcr().fatal("history: bound: books exceed 100000")
        k = len(self.books) + 1
        ck = w.culture[f["culture"]][1]
        T = self.cul[ck]["book_titles"]
        start = pick(self.D("hist.names.book", k, 0, 0), len(T))
        title = None
        for j in range(len(T)):
            x = T[(start + j) % len(T)]
            if x not in w.registry:
                title = x
                break
        if title is None:
            j = 2
            while T[start] + " " + roman(j) in w.registry:
                j += 1
            title = T[start] + " " + roman(j)
        w.registry.add(title)
        site = w.monsters.site(f["tower"])
        causes = {site["origin_event"]}
        mine = [b for b in self.books if b["author"] == f["id"]]
        if mine:
            causes.add(mine[-1]["event"])
        e = w.append("book_written", region=site["region"],
                     participants=(("figure", FIGURE, f["id"]), ("civilization", CIV, f["civ"]),
                                   ("site", SITE, f["tower"])), causes=tuple(sorted(causes)))
        self.books.append(dict(id=k, title=title, author=f["id"], site=f["tower"], year=w.year, event=e))
        f["books"] += 1

    def blights(self):
        w, t = self.w, self.t
        for f in list(self.figs):
            if f["role"] != CASTER or not self.alive(f["id"]) or f["blights"] > 0:
                continue
            if not chance_ppm(self.D("hist.figures.blight", f["id"], 0, 0), t["blight_ppm"]):
                continue
            site = w.monsters.site(f["tower"])
            R = site["region"]
            hit = []
            for u in w.settlements:
                if u["region"] != R or not w.standing(u) or not w.visible(u):
                    continue
                dead = u["population"] * t["blight_toll_permille"] // 1000
                if dead > 0:
                    u["population"] -= dead
                    hit.append((u["id"], dead))
            e = w.append("blight", region=R, participants=(("figure", FIGURE, f["id"]), ("civilization", CIV, f["civ"]),
                                                           ("site", SITE, f["tower"])),
                         values=(sum(d for _u, d in hit),), causes=(site["origin_event"],))
            for u, _d in hit:
                w.last_red[u] = e      # note_reduction
            f["blights"] += 1
            self.log.append((w.year, "blight", f["id"], R, hit))

    # --- 6.12 and 6.10
    def present(self):
        w = self.w
        if not self.t["name_rulers"]:
            return
        for cv in w.civs:
            c = cv["id"]
            L = self.lines.get(c)
            if cv["ended_year"] is not None or L is None or L["f"]:
                continue
            f = self.name_reign(c)
            e = w.append("reigned_at_end", region=w.stl(cv["capital"])["region"],
                         participants=(("figure", FIGURE, f["id"]), ("civilization", CIV, c)))
            self.open_ruled(f, e)

    def deed_values(self, f):
        """6.10's deed table for f (at E for the living)."""
        E = self.E
        v = dict.fromkeys(("founded_years", "wars_won", "conquests", "betrayals", "realm_fell", "reign_permille",
                           "battles_won", "battles_unbeaten", "slew_might", "exiled", "books", "blights"), 0)
        if f["role"] == RULER:
            cv = self.civ(f["civ"])
            if f["reign_no"] == 1:
                end = self.w.events[cv["ending_event"] - 1]["year"] if cv["ending_event"] else E
                v["founded_years"] = end - cv["founded_year"]
            v["wars_won"], v["conquests"], v["betrayals"] = f["wars_won"], f["conquests"], f["betrayals"]
            v["realm_fell"] = f["realm_fell"]
            end = f["reign_end"] if f["reign_end"] is not None else E
            v["reign_permille"] = (end - f["office_from"]) * 1000 // self.gen(f["race"])
        v["battles_won"] = f["battles_won"]
        v["battles_unbeaten"] = f["battles_won"] if f["battles_lost"] == 0 else 0
        v["slew_might"] = max([mt for _k, mt in f["slain"]] or [0])
        v["exiled"] = f["exiled"]
        v["books"], v["blights"] = f["books"], f["blights"]
        return v

    def slayer_text(self, f):
        if not f["slain"]:
            return ""
        best = None
        for k, mt in f["slain"]:
            if best is None or mt >= best[1]:
                best = (k, mt)
        return self.w.monsters.kind(best[0])["slayer_epithet"]

    def epithets(self):
        order = sorted([f for f in self.figs if not self.alive(f["id"])], key=lambda f: f["death_event"]) + \
            [f for f in self.figs if self.alive(f["id"])]
        borne = {}
        for f in order:
            text, how = epithet_of(ROLE_WORDS[f["role"]], self.deed_values(f), self.slayer_text(f),
                                   self.t["epithets"], borne, self.t["epithet_repeat_max"])
            f["epithet"] = text
            f["epithet_how"] = how
            if text:
                borne[text] = borne.get(text, 0) + 1

    # --- rows (9.1, 9.3)
    def figure_rows(self):
        return [(f["id"], f["name"], f["epithet"], f["role"], f["race"], f["culture"], f["civ"], f["born"],
                 f["office_from"], f["start_event"], f["died"], f["death_event"], f["death_region"],
                 f["death_settlement"], f["status"], f["reign_no"], f["regnal"],
                 f["temper"] if f["role"] == RULER else 0, f["skill"]) for f in self.figs]

    def book_rows(self):
        return [(b["id"], b["title"], b["author"], b["site"], b["year"], b["event"]) for b in self.books]


# ---------------------------------------------------------------------------
# The row model (any world of schema 12, or a replayed history): FW
# ---------------------------------------------------------------------------

READ_TABLES = ("region", "race", "culture", "event_kind", "event_kind_role", "event_kind_value", "phase", "event",
               "event_participant", "event_cause", "event_value", "civilization", "settlement", "settlement_census",
               "relation", "monster_kind", "monster", "site", "figure", "book")
FIG_COLS = ("id", "name", "epithet", "role", "race", "culture", "civ", "born", "office_from", "start_event", "died",
            "death_event", "death_region", "death_settlement", "status", "reign_no", "regnal", "temper", "skill")


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
            t[n] = [tuple(r) for r in rows] if rows is not None else []
    finally:
        conn.close()
    return meta, t


class FW:
    """Indexes over a world's rows."""

    def __init__(self, meta, t):
        self.meta, self.t = meta, t
        self.E = int(meta.get("history_end_year") or meta.get("param.history_span") or 0)
        self.key = {r[0]: r[1] for r in t["event_kind"]}
        self.kid = {v: k for k, v in self.key.items()}
        self.role_name = {(r[0], r[1]): r[2] for r in t["event_kind_role"]}
        self.value_key = {(r[0], r[1]): r[2] for r in t["event_kind_value"]}
        self.ev = {r[0]: r for r in t["event"]}
        self.parts = {}
        for p in t["event_participant"]:
            k = self.ev[p[0]][3] if p[0] in self.ev else 0
            self.parts.setdefault(p[0], []).append((self.role_name.get((k, p[2]), "?"), p[3], p[4]))
        self.vals = {}
        for v in t["event_value"]:
            k = self.ev[v[0]][3] if v[0] in self.ev else 0
            self.vals.setdefault(v[0], {})[self.value_key.get((k, v[1]), "?")] = v[2]
        self.causes = {}
        for c in t["event_cause"]:
            self.causes.setdefault(c[0], []).append(c[1])
        self.by_kind = {}
        for r in t["event"]:
            self.by_kind.setdefault(self.key.get(r[3]), []).append(r[0])
        self.fig = {r[0]: dict(zip(FIG_COLS, r)) for r in t["figure"]}
        self.civ = {r[0]: r for r in t["civilization"]}
        self.stl = {r[0]: r for r in t["settlement"]}
        self.site = {r[0]: r for r in t["site"]}
        self.race = {r[0]: r for r in t["race"]}
        self.region_ids = {r[0] for r in t["region"]}
        self.culture = {r[0]: r for r in t["culture"]}
        self.phase = {r[0]: r for r in t["phase"]}
        self.rel = {}
        for r in t["relation"]:
            self.rel.setdefault(r[4], []).append(r)

    def kind(self, e):
        return self.key.get(self.ev[e][3]) if e in self.ev else None

    def year(self, e):
        return self.ev[e][1]

    def of(self, key):
        return list(self.by_kind.get(key, ()))

    def role(self, e, role):
        return [x for (r, _t, x) in self.parts.get(e, ()) if r == role]

    def one(self, e, role):
        p = self.role(e, role)
        return p[0] if p else 0

    def value(self, e, key):
        return self.vals.get(e, {}).get(key)

    def ending_event(self, c):
        return self.civ[c][9] if c in self.civ else 0

    def ruler_at(self, c, e):
        """7.9: the figure a of the ruled row (a, c) with since_event <= e and (until_event 0 or e <= until_event)."""
        out = [r[1] for r in self.rel.get("ruled", ()) if r[3] == c and r[7] <= e and (r[8] == 0 or e <= r[8])]
        return out

    def display(self, f):
        x = self.fig[f]
        return display_name(x["name"], x["regnal"], x["epithet"])


def fw_of(path):
    meta, t = load_rows(path)
    return FW(meta, t)


REGION_COLUMNS = 28


def fw_of_res(res):
    """The row model of a replayed history (res of p3_conflict_reference.history_stage)."""
    inp = res["_inputs"]
    regions = []
    for r in inp.regions:
        row = [0] * REGION_COLUMNS
        row[0], row[1] = r["id"], r.get("name", "")
        regions.append(tuple(row))
    t = {n: list(res.get(n, [])) for n in READ_TABLES}
    t["region"] = regions
    t["race"] = [tuple(r) for r in inp.races]
    t["culture"] = [tuple(c[:4]) if len(c) >= 4 else tuple(c) for c in inp.cultures]
    t["event_kind"] = [(r[0], r[1]) for r in res["event_kind"]]
    meta = {"history_end_year": str(inp.params["span"]), "schema_version": str(SCHEMA_VERSION),
            "generator_version": GENERATOR_VERSION}
    return FW(meta, t)


# ---------------------------------------------------------------------------
# 9.4: the reader checks as row checks (their messages, in the order of 9.4: figure, book, relation, events;
# then the amended site and monster checks)
# ---------------------------------------------------------------------------

def epithet_column_ok(s):
    import p3_data_reference as pdr
    return s == "" or pdr.epithet_ok(s)


def entity_name_ok(s):
    """P3-04's is_entity_name as the references read it (1 to 64 bytes of letters, spaces, ' and -, starting
    with an upper-case letter or a digit-free word)."""
    if not (1 <= len(s) <= 64) or s != s.strip() or "  " in s:
        return False
    return all(c.isalpha() and c.isascii() or c in " '-" for c in s)


def reader_problems(w):
    out = []
    F = "corrupt table 'figure': row %d: "
    prev = 0
    last_ruler = {}
    seen_names = {}
    for fid in sorted(w.fig):
        x = w.fig[fid]
        p = F % fid
        if fid != prev + 1:
            out.append(p + "id %d is not %d" % (fid, prev + 1))
        prev = fid
        if not entity_name_ok(x["name"]):
            out.append(p + "name is not a name")
        if not epithet_column_ok(x["epithet"]):
            out.append(p + "epithet is not an epithet")
        if not 1 <= x["role"] <= 5:
            out.append(p + "role %d outside 1 to 5" % x["role"])
        if x["race"] not in w.race:
            out.append(p + "unknown race %d" % x["race"])
        if x["culture"] not in w.culture:
            out.append(p + "unknown culture %d" % x["culture"])
        if x["civ"] not in w.civ:
            out.append(p + "unknown civilization %d" % x["civ"])
        if x["start_event"] not in w.ev:
            out.append(p + "start_event %d is not an event" % x["start_event"])
        if x["born"] > x["office_from"]:
            out.append(p + "born %d after office_from %d" % (x["born"], x["office_from"]))
        aw = w.race[x["race"]][10] if x["race"] in w.race else None
        if aw is not None and x["born"] < aw:
            out.append(p + "born %d before the race's awakening" % x["born"])
        if x["start_event"] in w.ev and x["office_from"] > w.year(x["start_event"]):
            out.append(p + "office_from %d after the year of start_event" % x["office_from"])
        if not 1 <= x["status"] <= 6:
            out.append(p + "status %d outside 1 to 6" % x["status"])
        alive = x["status"] in (ALIVE, EXILE)
        if not ((x["died"] is None) == (x["death_event"] == 0) == alive) or (
                alive and (x["death_region"] != 0 or x["death_settlement"] != 0)):
            out.append(p + "end columns disagree")
        if not alive and x["died"] is not None:
            if x["died"] < x["office_from"]:
                out.append(p + "died %d before office_from" % x["died"])
            de = x["death_event"]
            want = {DIED: "figure_died", FELL: "fell_in_battle", SLAIN: "hero_slain"}.get(x["status"])
            ok = False
            if x["status"] == FELL_WITH_REALM:
                ok = de == w.ending_event(x["civ"]) and de != 0
            elif want is not None:
                ok = de in w.ev and w.kind(de) == want and w.year(de) == x["died"] and w.one(de, "figure") == fid
            if not ok:
                out.append(p + "death_event %d is not this figure's death" % de)
            if x["death_region"] not in w.region_ids:
                out.append(p + "death_region %d is not a region" % x["death_region"])
        if x["death_settlement"] != 0 and x["death_settlement"] not in w.stl:
            out.append(p + "death_settlement %d is not a settlement" % x["death_settlement"])
        rw = ROLE_WORDS.get(x["role"], "?")
        if (x["reign_no"] >= 1) != (x["role"] == RULER):
            out.append(p + "reign_no %d of a %s" % (x["reign_no"], rw))
        if x["role"] == RULER:
            lr = last_ruler.get(x["civ"])
            if lr is not None and x["reign_no"] <= lr[1]:
                out.append(p + "reign_no %d not above %d of figure %d" % (x["reign_no"], lr[1], lr[0]))
            last_ruler[x["civ"]] = (fid, x["reign_no"])
        if not (x["regnal"] == 0 or 2 <= x["regnal"] <= 39):
            out.append(p + "regnal %d outside 0 or 2 to 39" % x["regnal"])
        if x["regnal"] != 0 and x["role"] != RULER:
            out.append(p + "regnal %d of a %s" % (x["regnal"], rw))
        earlier = seen_names.get(x["name"], [])
        bad = [g for g in earlier if not (w.fig[g]["role"] == RULER and w.fig[g]["civ"] == x["civ"])]
        if bad:
            out.append(p + "name repeats figure %d" % bad[0])
        k = len(earlier)
        if (x["regnal"] >= 2 and k != x["regnal"] - 1) or (x["regnal"] == 0 and k != 0):
            out.append(p + "regnal %d but %d earlier bearers" % (x["regnal"], k))
        seen_names.setdefault(x["name"], []).append(fid)
        if not 0 <= x["temper"] <= 4:
            out.append(p + "temper %d outside 0 to 4" % x["temper"])
        if x["temper"] != 0 and x["role"] != RULER:
            out.append(p + "temper %d of a %s" % (x["temper"], rw))
        if x["role"] == GENERAL and not 1 <= x["skill"] <= 1000:
            out.append(p + "skill %d outside 1 to 1000" % x["skill"])
        if x["skill"] != 0 and x["role"] != GENERAL:
            out.append(p + "skill %d of a %s" % (x["skill"], rw))
        if x["status"] == EXILE and x["role"] != CLAIMANT:
            out.append(p + "in exile but not a claimant")
    B = "corrupt table 'book': row %d: "
    prev = 0
    events_seen = {}
    for b in w.t["book"]:
        bid, title, author, site, year, ev = b
        p = B % bid
        if bid != prev + 1:
            out.append(p + "id %d is not %d" % (bid, prev + 1))
        prev = bid
        if not entity_name_ok(title):
            out.append(p + "title is not a name")
        if author not in w.fig or w.fig[author]["role"] != CASTER:
            out.append(p + "author %d is not a caster" % author)
        s = w.site.get(site)
        if s is None or s[2] != SITE_TOWER or w.one(s[10], "figure") != author:
            out.append(p + "site %d is not the author's tower" % site)
        if not (ev in w.ev and w.kind(ev) == "book_written" and w.year(ev) == year and
                w.one(ev, "figure") == author and w.one(ev, "site") == site):
            out.append(p + "event %d is not a book_written of year %d naming the author and the site" % (ev, year))
        if ev in events_seen:
            out.append(p + "event %d has two book rows" % ev)
        events_seen[ev] = bid
    R = "corrupt table 'relation': row (%d, %d, %d, %d, %s, %d): "
    for r in sorted((r for r in w.t["relation"] if r[4] in RELATION_KINDS), key=lambda r: (r[0], r[1], r[2], r[3],
                                                                                          r[4].encode(), r[5])):
        at, a, bt, b, kind, since, until, se, ue = r
        p = R % (at, a, bt, b, kind, since)
        want_types = (FIGURE, CIV) if kind in ("ruled", "served") else (FIGURE, FIGURE)
        if (at, bt) != want_types:
            out.append(p + "%s with sides of types %d and %d" % (kind, at, bt))
            continue
        fa = w.fig.get(a)
        if kind == "ruled":
            if fa is None or fa["role"] != RULER or fa["civ"] != b:
                out.append(p + "ruled a %d is not a ruler of civilization %d" % (a, b))
                continue
            if se != fa["start_event"]:
                out.append(p + "ruled since_event %d is not the ruler's record" % se)
            if ue and not ((ue == fa["death_event"] and w.kind(ue) == "figure_died") or ue == w.ending_event(b)):
                out.append(p + "ruled until_event %d does not end the reign" % ue)
        elif kind == "served":
            if fa is None or fa["role"] != GENERAL or fa["civ"] != b or not (
                    se in w.ev and w.kind(se) == "general_appointed" and w.one(se, "figure") == a and
                    w.one(se, "civilization") == b):
                out.append(p + "served since_event %d is not a general_appointed of the pair" % se)
            if ue and not ((w.kind(ue) in ("peace", "war_won", "truce", "civ_conquered", "civ_ended",
                                          "search_abandoned")) or   # search_abandoned: reading G2
                           (w.kind(ue) in ("fell_in_battle", "figure_died") and w.one(ue, "figure") == a) or
                           ue == w.ending_event(b)):
                out.append(p + "served until_event %d does not end the command" % ue)
        elif kind == "rival":
            if not (a < b and se in w.ev and w.kind(se) == "succession_disputed" and
                    {w.one(se, "ruler"), w.one(se, "claimant")} == {a, b}):
                out.append(p + "rival since_event %d is not a succession_disputed of the pair" % se)
            if ue and ue not in (w.fig.get(a, {}).get("death_event"), w.fig.get(b, {}).get("death_event")):
                out.append(p + "rival until_event %d is not a death of the pair" % ue)
        else:
            if not (se in w.ev and w.kind(se) == "succession_disputed" and b in (w.one(se, "ruler"),
                                                                                 w.one(se, "claimant"))):
                out.append(p + "parent since_event %d is not a succession_disputed of the child" % se)
            if until is not None or ue:
                out.append(p + "parent with an end")
    by_civ = {}
    for r in w.rel.get("ruled", ()):
        by_civ.setdefault(r[3], []).append(r)
    for c in sorted(by_civ):
        rows = sorted(by_civ[c], key=lambda r: r[7])
        for i in range(1, len(rows)):
            prev_r, r = rows[i - 1], rows[i]
            if prev_r[8] == 0 or r[7] <= prev_r[8]:
                out.append(R % tuple(r[:6]) + "two rulers of civilization %d at event %d" % (c, r[7]))
    Ev = "corrupt table 'event': row %d: "
    ranges = {"how": (0, 2), "outcome": (0, 1), "hosted": (0, 1), "again": (0, 1), "library": (0, 1)}
    for e in sorted(w.ev):
        k = w.kind(e)
        for (role, typ, x) in w.parts.get(e, ()):
            if typ == FIGURE and x not in w.fig:
                out.append(Ev % e + "figure %d has no row" % x)
        if k not in FIGURE_KIND_KEYS:
            continue
        if k == "battle_led" and not (w.role(e, "winner_general") or w.role(e, "loser_general")):
            out.append(Ev % e + "battle_led without a general")
        for key, v in sorted(w.vals.get(e, {}).items()):
            if key in ranges:
                lo, hi = ranges[key]
                if not lo <= v <= hi:
                    out.append(Ev % e + "value %s %d outside %d to %d" % (key, v, lo, hi))
            elif key in ("age", "dead", "hunters", "pass", "winner_dead", "loser_dead") and v < 0:
                out.append(Ev % e + "value %s %d outside 0 to its maximum" % (key, v))
        if k == "succession_disputed" and (w.value(e, "outcome") == 1) != bool(w.role(e, "new_realm")):
            out.append(Ev % e + "succession_disputed roles and values disagree")
        if k == "exiled" and (w.value(e, "hosted") == 1) != bool(w.role(e, "host")):
            out.append(Ev % e + "exiled roles and values disagree")
        if k == "book_written" and e not in events_seen:
            out.append(Ev % e + "book_written without its book row")
    S = "corrupt table 'site': row %d: "
    for s in w.t["site"]:
        if s[2] not in (1, 2, 3, 4):
            out.append(S % s[0] + "kind %d is not 1, 2, 3 or 4" % s[2])
        if s[2] == SITE_TOWER:
            if s[7] not in w.civ:
                out.append(S % s[0] + "unknown civilization %d" % s[7])
            if s[8] != 0:
                out.append(S % s[0] + "tower with a monster")
            if s[13] != 0:
                out.append(S % s[0] + "ending %d is not 0 for a tower" % s[13])
            oe = s[10]
            if not (oe in w.ev and w.kind(oe) == "tower_founded" and w.year(oe) == s[9] and w.one(oe, "site") == s[0]):
                out.append(S % s[0] + "origin_event %d is not an event of its year naming this site" % oe)
            if s[6] and (s[6] not in w.stl or (w.stl[s[6]][2], w.stl[s[6]][3]) != (s[4], s[5])):
                out.append(S % s[0] + "settlement %d is not at the site's cell" % s[6])
    M = "corrupt table 'monster': row %d: "
    for m in w.t["monster"]:
        if m[9] == 3:
            de = m[7]
            if not (de in w.ev and w.kind(de) in ("monster_slain", "monster_slain_by_hero") and w.year(de) == m[6]
                    and w.one(de, "monster") == m[0] and w.one(de, "slayer") == m[8]):
                out.append(M % m[0] + "death_event %d is not this monster's slaying" % de)
    return out


# ---------------------------------------------------------------------------
# 13: the invariants rows can show (1 to 9, 12 to 16; 10 is I0, 11 the earlier references' check-world)
# ---------------------------------------------------------------------------

def _in_force(r, e):
    return r[7] <= e and (r[8] == 0 or e <= r[8])


def major_calamities(w):
    """25.2: G (ids in rank order) and every major calamity's (ended, I, id)."""
    kinds = ("famine", "plague", "plague_outbreak")
    census = {}
    for r in w.t["settlement_census"]:
        census.setdefault(r[0], []).append(r)
    ruin_causes = set()
    for s in w.t["settlement"]:
        if s[10] == 3 and s[13]:
            ruin_causes |= set(w.causes.get(s[13], []))
    majors = []
    for k in kinds:
        for e in w.of(k):
            dead = w.value(e, "total") if k == "plague_outbreak" else w.value(e, "dead")
            c = 0
            for (role, typ, x) in w.parts.get(e, ()):
                if typ == CIV:
                    c = x
                    break
            P = 0
            if c:
                y = w.year(e)
                for sid, rows in census.items():
                    best = None
                    for r in rows:
                        if r[1] < y and (best is None or r[1] > best[1]):
                            best = r
                    if best is not None and best[2] == c and best[4] in (1, 2):
                        P += best[5]
            I = (dead or 0) * 1000 // P if P > 0 else 0
            ended = 1 if e in ruin_causes else 0
            if ended or I >= SUMMARY_CALAMITY_MAJOR_PERMILLE:
                majors.append((ended, I, e))
    majors.sort(key=lambda m: (-m[0], -m[1], m[2]))
    return [m[2] for m in majors[:SUMMARY_CALAMITY_MAJOR_MAX]], majors


def invariant_violations(w, tuning, cultures_regnal=None, quiet_events_max=None):
    """13's invariants on rows; tuning: history.figures (name_rulers, books_max, epithets, epithet_repeat_max);
    cultures_regnal: culture key -> regnal_permille."""
    out = []
    E = w.E
    name_rulers = tuning["name_rulers"]
    ruled = w.rel.get("ruled", [])
    served = w.rel.get("served", [])
    rulers = sorted((x for x in w.fig.values() if x["role"] == RULER), key=lambda x: x["id"])
    # 1
    for c in w.civ:
        rows = sorted((r for r in ruled if r[3] == c), key=lambda r: r[7])
        for i in range(1, len(rows)):
            if rows[i - 1][8] == 0 or rows[i][7] <= rows[i - 1][8]:
                out.append("1: two rulers of c%d at e%d" % (c, rows[i][7]))
        line = [x for x in rulers if x["civ"] == c]
        for i in range(1, len(line)):
            a, b = line[i - 1], line[i]
            if b["reign_no"] <= a["reign_no"]:
                out.append("1: c%d reigns out of order f%d f%d" % (c, a["id"], b["id"]))
            end_a = a["died"] if a["died"] is not None else E
            if end_a > b["office_from"]:
                out.append("1: c%d reign of f%d (to %d) overlaps f%d (from %d)" % (c, a["id"], end_a, b["id"],
                                                                                b["office_from"]))
    for r in ruled:
        f = w.fig.get(r[1])
        if f is not None and f["office_from"] > r[5]:
            out.append("1: f%d office_from %d after its ruled since %d" % (f["id"], f["office_from"], r[5]))
    # 2
    if name_rulers:
        for c, cv in sorted(w.civ.items()):
            if cv[8] is None and not any(r[3] == c and r[8] == 0 for r in ruled):
                out.append("2: alive c%d has no open ruled row at E" % c)
            first = [x for x in rulers if x["civ"] == c and x["reign_no"] == 1]
            if not first:
                out.append("2: c%d has no recorded first ruler" % c)
            else:
                se = first[0]["start_event"]
                if not (w.kind(se) == "came_to_rule" or (w.kind(se) == "succession_disputed" and
                                                         w.value(se, "outcome") == 1)):
                    out.append("2: c%d first ruler f%d start e%d %s" % (c, first[0]["id"], se, w.kind(se)))
    # 3
    if name_rulers:
        acting = (("war_declared", "attacker"), ("alliance_betrayed", "betrayer"), ("spoils_shared", "principal"),
                  ("spoils_withheld", "principal"), ("civ_conquered", "civilization"), ("civ_ended", "civilization"))
        for k, role in acting:
            for e in w.of(k):
                c = w.one(e, role)
                if len([r for r in ruled if r[3] == c and _in_force(r, e)]) != 1:
                    out.append("3: %s e%d: c%d has no recorded ruler at the event" % (k, e, c))
    # 4
    for f in w.fig.values():
        if not (f["born"] <= f["office_from"] <= (w.year(f["start_event"]) if f["start_event"] in w.ev else -1)):
            out.append("4: f%d born %d office_from %d start e%d" % (f["id"], f["born"], f["office_from"],
                                                                     f["start_event"]))
        lam = w.race[f["race"]][6]
        if f["died"] is not None and f["died"] - f["born"] > 2 * lam:
            # a death that waited for a quiet step's budget: some quiet phase lies between 2 lifespans and the death
            lo_y, hi_y = f["born"] + 2 * lam, f["died"]
            quiet = any(ph[2] == 2 and ph[3] <= hi_y and ph[4] >= lo_y for ph in w.phase.values())
            if not quiet:
                out.append("4: f%d lived %d years, lifespan %d" % (f["id"], f["died"] - f["born"], lam))
    # 5
    for b in w.of("battle_led"):
        gens = [(x, w.one(b, "winner")) for x in w.role(b, "winner_general")] + \
               [(x, w.one(b, "loser")) for x in w.role(b, "loser_general")]
        if not gens:
            out.append("5: battle_led e%d without a general" % b)
        for f, side in gens:
            if w.fig.get(f, {}).get("civ") != side:
                out.append("5: battle_led e%d general f%d not of c%d" % (b, f, side))
            if not any(r[1] == f and r[3] == side and _in_force(r, b) for r in served):
                out.append("5: battle_led e%d general f%d has no served row in force" % (b, f))
    # 6
    for k, frole, crole in (("monster_slain_by_hero", "hero", "slayer"), ("hero_slain", "figure", "civilization")):
        for e in w.of(k):
            f = w.fig.get(w.one(e, frole))
            if f is None or f["role"] != HERO or f["civ"] != w.one(e, crole) or f["start_event"] > e or (
                    f["death_event"] and f["death_event"] < e):
                out.append("6: %s e%d hero %s not a living hero of its realm" % (k, e, w.one(e, frole)))
    heroes = {}
    for f in w.fig.values():
        if f["role"] == HERO:
            heroes.setdefault(f["civ"], []).append((f["start_event"], f["death_event"] or (1 << 60), f["id"]))
    for c, hs in heroes.items():
        hs.sort()
        for i in range(1, len(hs)):
            if hs[i][0] < hs[i - 1][1]:
                out.append("6: c%d has two living heroes f%d f%d" % (c, hs[i - 1][2], hs[i][2]))
    # 7
    seceded = {}
    for e in w.of("civ_seceded"):
        for x in w.causes.get(e, []):
            if w.kind(x) == "succession_crisis":
                seceded[x] = e
    for e in w.of("succession_disputed"):
        c = w.one(e, "civilization")
        cr = [x for x in w.causes.get(e, []) if w.kind(x) == "succession_crisis"]
        if not cr or w.one(cr[0], "civilization") != c or w.year(cr[0]) != w.year(e):
            out.append("7: succession_disputed e%d follows no crisis of c%d in its year" % (e, c))
            continue
        out1 = w.value(e, "outcome") == 1
        if out1 != (cr[0] in seceded):
            out.append("7: succession_disputed e%d outcome %d, seceded %s" % (e, w.value(e, "outcome"),
                                                                             cr[0] in seceded))
        if out1:
            cl = w.fig.get(w.one(e, "claimant"), {})
            if not (cl.get("role") == RULER and cl.get("reign_no") == 1 and cl.get("civ") == w.one(e, "new_realm")):
                out.append("7: succession_disputed e%d claimant is not the new realm's first ruler" % e)
    # 8: figure events in quiet steps: only 80 and 91, and each realm within its budget
    per = {}
    for e in sorted(w.ev):
        ph = w.phase.get(w.ev[e][4])
        if ph is None or ph[2] != 2:
            continue
        ch = 0
        for (_r, typ, x) in w.parts.get(e, ()):
            if typ == CIV:
                ch = x
                break
        per[(w.ev[e][1], ch)] = per.get((w.ev[e][1], ch), 0) + (0 if w.kind(e) == "quiet_stretch" else 1)
        if w.kind(e) in FIGURE_KIND_KEYS and w.kind(e) not in ("figure_died", "book_written"):
            out.append("8: %s e%d in a quiet step" % (w.kind(e), e))
    if quiet_events_max is not None:
        for (y, ch), n in sorted(per.items()):
            if ch and n > quiet_events_max:
                out.append("8: c%d wrote %d events in the quiet step of %d" % (ch, n, y))
    # 9
    for s in w.t["site"]:
        if s[2] != SITE_TOWER:
            continue
        oe = s[10]
        f = w.fig.get(w.one(oe, "figure"), {})
        if w.kind(oe) != "tower_founded" or f.get("role") != CASTER or s[7] != f.get("civ") or \
                w.one(oe, "civilization") != s[7]:
            out.append("9: tower si%d is not a site of its caster's realm" % s[0])
    nb = {}
    for b in w.t["book"]:
        nb[b[2]] = nb.get(b[2], 0) + 1
        s = w.site.get(b[3])
        if s is None or s[2] != SITE_TOWER or w.one(s[10], "figure") != b[2]:
            out.append("9: book %d is not at its author's tower" % b[0])
    for f, n in nb.items():
        if n > tuning["books_max"]:
            out.append("9: f%d wrote %d books" % (f, n))
    bl = {}
    for e in w.of("blight"):
        f = w.one(e, "figure")
        bl[f] = bl.get(f, 0) + 1
        if bl[f] > 1:
            out.append("9: f%d caused %d blights" % (f, bl[f]))
    # 12
    names_by = {}
    for x in sorted(w.fig.values(), key=lambda x: x["id"]):
        names_by.setdefault(x["name"], []).append(x)
    for x in rulers:
        if x["regnal"] >= 2:
            prev = [y for y in names_by[x["name"]] if y["id"] < x["id"] and y["civ"] == x["civ"] and y["role"] == RULER]
            if len(prev) != x["regnal"] - 1 or prev[0]["regnal"] != 0:
                out.append("12: f%d regnal %d with %d earlier bearers" % (x["id"], x["regnal"], len(prev)))
            if x["reign_no"] == 1:
                out.append("12: f%d a first ruler with a regnal number" % x["id"])
            if cultures_regnal is not None:
                ck = w.culture[x["culture"]][1]
                if cultures_regnal.get(ck, 0) <= 0:
                    out.append("12: f%d regnal number in culture %s" % (x["id"], ck))
    for n, xs in names_by.items():
        if len(xs) > 1 and (len({y["civ"] for y in xs}) > 1 or any(y["role"] != RULER for y in xs)):
            out.append("12: name %r shared by %s" % (n, [y["id"] for y in xs]))
    disp = {}
    for x in w.fig.values():
        d = w.display(x["id"])
        if d in disp:
            out.append("12: display name %r of f%d and f%d" % (d, disp[d], x["id"]))
        disp[d] = x["id"]
    # 13 (on the deed values rows determine exactly; reading R14)
    out += epithet_violations(w, tuning)
    # 14
    for s in w.t["settlement"]:
        if s[10] == 4:
            out.append("14: settlement %d faded" % s[0])
    for e in w.of("ruin_faded"):
        out.append("14: ruin_faded e%d" % e)
    first_ruin = {}
    for r in w.t["settlement_census"]:
        if r[4] in (3, 4) and r[0] not in first_ruin:
            first_ruin[r[0]] = r[1]
    for r in w.t["settlement_census"]:
        if r[0] in first_ruin and r[1] > first_ruin[r[0]] and r[4] != 3:
            out.append("14: settlement %d census of %d status %d after its ruin" % (r[0], r[1], r[4]))
    for sid in first_ruin:
        s = w.stl.get(sid)
        if s is None or s[10] != 3 or s[12] is None or not s[13] or (len(s) > 14 and (s[14] or 0) < 1):
            out.append("14: settlement %d ruined in %d is not ruined at E" % (sid, first_ruin[sid]))
    # 15 (the members of G are major and ranked; their presence in the summary set is the report's)
    G, majors = major_calamities(w)
    if len(G) > SUMMARY_CALAMITY_MAJOR_MAX:
        out.append("15: |G| %d" % len(G))
    # 16
    towers = [s for s in w.t["site"] if s[2] == SITE_TOWER]
    seen_s, seen_c = {}, {}
    for s in towers:
        if s[6] and s[13] == 0:
            if s[6] in seen_s:
                out.append("16: towers si%d and si%d at settlement %d" % (seen_s[s[6]], s[0], s[6]))
            seen_s[s[6]] = s[0]
        if (s[4], s[5]) in seen_c:
            out.append("16: towers si%d and si%d share a cell" % (seen_c[(s[4], s[5])], s[0]))
        seen_c[(s[4], s[5])] = s[0]
        if s[10] in w.ev and w.ev[s[10]][5] != s[3]:
            out.append("16: tower si%d not in the region of its tower_founded" % s[0])
    return out


def row_deeds(w, f):
    """6.10's deed values from rows, for the deeds that rows give exactly (None for the reign tallies)."""
    x = w.fig[f]
    v = {}
    v["realm_fell"] = 1 if x["status"] == FELL_WITH_REALM else 0
    if x["role"] == RULER:
        g = w.race[x["race"]][7]
        end = x["died"] if x["died"] is not None else w.E
        v["reign_permille"] = (end - x["office_from"]) * 1000 // g
        if x["reign_no"] == 1:
            ce = w.ending_event(x["civ"])
            v["founded_years"] = (w.year(ce) if ce else w.E) - w.civ[x["civ"]][6]
        else:
            v["founded_years"] = 0
    else:
        v["reign_permille"] = v["founded_years"] = 0
    won = sum(1 for b in w.of("battle_led") if f in w.role(b, "winner_general"))
    lost = sum(1 for b in w.of("battle_led") if f in w.role(b, "loser_general"))
    v["battles_won"], v["battles_unbeaten"] = won, (won if lost == 0 else 0)
    mk = {r[0]: r for r in w.t["monster_kind"]}
    mons = {m[0]: m for m in w.t["monster"]}
    slain = [(mk[mons[w.one(e, "monster")][2]][6], mk[mons[w.one(e, "monster")][2]][1])
             for e in sorted(w.of("monster_slain_by_hero")) if w.one(e, "hero") == f]
    v["slew_might"] = max([m for m, _k in slain] or [0])
    best = None
    for m, k in slain:   # the largest might, the last slain at a tie (6.10)
        if best is None or m >= best[0]:
            best = (m, k)
    v["_slayer_kind"] = best[1] if best else None
    v["exiled"] = 1 if any(w.one(e, "figure") == f for e in w.of("exiled")) else 0
    v["books"] = sum(1 for b in w.t["book"] if b[2] == f)
    v["blights"] = sum(1 for e in w.of("blight") if w.one(e, "figure") == f)
    for k in ("wars_won", "conquests", "betrayals"):
        v[k] = None
    return v


def epithet_violations(w, tuning, slayer_epithets=None):
    """13 item 13 on the deeds rows give exactly: an entry whose deed is a reign tally (wars_won, conquests,
    betrayals) stops the check of that figure (the tallies' attribution to a reign by step order is not in the
    rows; the replay compares them)."""
    out = []
    order = sorted((x for x in w.fig.values() if x["death_event"]), key=lambda x: x["death_event"]) + \
        sorted((x for x in w.fig.values() if not x["death_event"]), key=lambda x: x["id"])
    borne = {}
    for x in order:
        v = row_deeds(w, x["id"])
        want = None
        for e in tuning["epithets"]:
            if e["role"] != ROLE_WORDS[x["role"]]:
                continue
            val = v.get(e["deed"])
            if val is None:
                want = "?"
                break
            if e["text"] == "{slayer}":
                if val == 0:
                    continue
                import p3_data_reference as pdr
                txt = (slayer_epithets or pdr.SLAYER_EPITHET_027).get(v["_slayer_kind"], "?")
            else:
                txt = e["text"]
            if e["substantial"] > 0 and val >= e["substantial"]:
                want = txt
                break
            if val >= e["min"] and borne.get(txt, 0) < tuning["epithet_repeat_max"]:
                want = txt
                break
        if want is None:
            want = ""
        if want != "?" and want != x["epithet"]:
            out.append("13: f%d epithet %r, rule %r" % (x["id"], x["epithet"], want))
        if x["epithet"]:
            borne[x["epithet"]] = borne.get(x["epithet"], 0) + 1
    return out


def stats(w):
    """The counts of 15's FG rows and the [stats] keys of 10.5 that rows give."""
    f = list(w.fig.values())
    by_role = lambda r: sum(1 for x in f if x["role"] == r)   # noqa: E731
    ep = {}
    for x in f:
        if x["epithet"]:
            ep[x["epithet"]] = ep.get(x["epithet"], 0) + 1
    rulers = [x for x in f if x["role"] == RULER]
    last = {}
    for x in rulers:
        last[x["civ"]] = max(last.get(x["civ"], 0), x["reign_no"])
    ages = sorted(x["died"] - x["born"] for x in f if x["died"] is not None)
    deeds = ("war_declared", "alliance_betrayed", "spoils_shared", "spoils_withheld")
    G, _m = major_calamities(w)
    return [("figures", len(f)), ("figures_rulers", by_role(RULER)), ("figures_claimants", by_role(CLAIMANT)),
            ("figures_generals", by_role(GENERAL)), ("figures_heroes", by_role(HERO)),
            ("figures_casters", by_role(CASTER)),
            ("figures_alive", sum(1 for x in f if x["status"] in (ALIVE, EXILE))),
            ("figures_epithet", sum(1 for x in f if x["epithet"])),
            ("epithet_bearers_max", max(ep.values()) if ep else 0),
            ("reigns", sum(last.values())), ("rulers_recorded", len(rulers)),
            ("rulers_by_deed", sum(1 for x in rulers if w.kind(x["start_event"]) in deeds)),
            ("rulers_regnal", sum(1 for x in rulers if x["regnal"] >= 2)),
            ("successions_disputed", len(w.of("succession_disputed"))), ("exiles", len(w.of("exiled"))),
            ("generals_appointed", len(w.of("general_appointed"))), ("battles_led", len(w.of("battle_led"))),
            ("generals_fallen", len(w.of("fell_in_battle"))), ("heroes_risen", len(w.of("hero_rose"))),
            ("monsters_slain_by_heroes", len(w.of("monster_slain_by_hero"))),
            ("heroes_slain", len(w.of("hero_slain"))), ("towers", len(w.of("tower_founded"))),
            ("books", len(w.t["book"])), ("blights", len(w.of("blight"))),
            ("figure_age_median", ages[(len(ages) - 1) // 2] if ages else "-"),
            ("summary_major_calamities", len(G))]


# ---------------------------------------------------------------------------
# The golden file (17)
# ---------------------------------------------------------------------------

def _enc(v):
    import struct
    if v is None:
        return b"\x00"
    if isinstance(v, int):
        return b"\x01" + struct.pack("<q", v)
    b = v.encode("latin-1")
    return b"\x03" + struct.pack("<Q", len(b)) + b


def rows_hash(ncols, rows):
    import struct
    data = bytearray(struct.pack("<Q", ncols))
    for r in rows:
        for v in r:
            data += _enc(v)
    return hashlib.sha256(bytes(data)).hexdigest()


def case_lines(name, w, world_case=True):
    out = ["%s figure %d %s" % (name, len(w.t["figure"]), rows_hash(19, w.t["figure"])),
           "%s book %d %s" % (name, len(w.t["book"]), rows_hash(6, w.t["book"]))]
    for kind in RELATION_KINDS:
        rows = [r for r in w.t["relation"] if r[4] == kind]
        out.append("%s rows %s %d %s" % (name, kind, len(rows), rows_hash(9, rows)))
    new = {i for i, _k in KINDS}
    evs = [e for e in w.t["event"] if e[3] in new]
    ids = {e[0] for e in evs}
    out.append("%s events %d %s" % (name, len(evs), rows_hash(8, evs)))
    out.append("%s participants %s" % (name, rows_hash(5, [p for p in w.t["event_participant"] if p[0] in ids])))
    out.append("%s causes %s" % (name, rows_hash(2, [c for c in w.t["event_cause"] if c[0] in ids])))
    out.append("%s values %s" % (name, rows_hash(3, [v for v in w.t["event_value"] if v[0] in ids])))
    towers = [x for x in w.t["site"] if x[2] == SITE_TOWER]
    out.append("%s sites tower %d %s" % (name, len(towers), rows_hash(14, towers)))
    for k, v in stats(w):
        out.append("%s stat %s %s" % (name, k, v))
    if world_case:
        G, _m = major_calamities(w)
        out.append("%s calamities %d %s" % (name, len(G), " ".join("e%d" % e for e in G) or "-"))
    return out


GOLDEN_HEADER = """\
# P5-06 figures golden vectors for generator 0.27.0 (schema 12), written by
# worldgen/tests/reference/p5_06_figures_reference.py golden from world files of the reference chain.
# <case> figure <n> <sha256 of the figure rows, P0-04 8's encoding, 19 columns>; <case> book <n> <sha256, 6 columns>
# <case> rows <ruled|served|rival|parent> <n> <sha256 of the relation rows of the kind, 9 columns>
# <case> events <n> <sha256 of the event rows of kinds 79 to 92>; participants, causes, values of those events
# <case> sites tower <n> <sha256 of the kind 4 site rows>
# <case> stat <key> <value>   (the FG counts of 15 and 10.5's [stats] keys that rows give)
# <case> calamities <|G|> <the ids of G in rank order, or ->   (25.2; world cases)
"""


def golden_text(inputs):
    names = sorted(n[:-3] for n in os.listdir(inputs) if n.endswith(".db"))
    out = [GOLDEN_HEADER.rstrip("\n")]
    for n in names:
        out += case_lines(n, fw_of(os.path.join(inputs, n + ".db")))
    return "\n".join(out) + "\n"


# ---------------------------------------------------------------------------
# Fixtures (4.4 zero, 18.3 figures-small), I0 (7.11)
# ---------------------------------------------------------------------------

def use_027():
    _pcr().use_version("0.27.0")


def data_027(root=None):
    use_027()
    return _pcr().data(root) if root else _pcr().data()


def zero_data(dat=None):
    """4.4: the zero fixture tuning (name_rulers false, no generals, heroes, casters, exiles, split bonus; neutral
    tempers)."""
    import p3_data_reference as pdr
    d = _pcr().edited(dat or data_027())
    d.figures = dict(pdr.FIGURES_ZERO_027)
    return d


def small_data(dat=None):
    """18.3: figures-small = the alliances stress data (p5_04_alliances_reference.small_data) with the figures
    edits: general_scale 1000 and every race's general_permille 1000, hero_scale 1000 and hero_permille 1000,
    exile_permille 1000, caster_ppm 1000000, book_permille 1000, blight_ppm 100000, fall_loser_permille 500, every
    culture's regnal_permille 1000, regnal_max 3."""
    import copy
    import p3_data_reference as pdr
    import p5_04_alliances_reference as p504
    import p5_02_monsters_reference as p502
    use_027()
    d = p504.small_data(dat)
    # reading R33: with P5-02's monsters-small edits (every wake_rate x 2000, living_permille 1000, overrun 1000), so
    # that raids raise heroes on the small stress maps (18.3 names the alliances stress data only)
    d = p502.with_monsters(d, p502.MONSTERS_SMALL_TUNING, lambda k, v: dict(
        wake_rate=min(10000000, v["wake_rate"] * p502.MONSTERS_SMALL_KINDS["wake_rate_x"]),
        overrun_permille=p502.MONSTERS_SMALL_KINDS["overrun_permille"]))
    d.figures = dict(d.figures, **pdr.FIGURES_SMALL_027)
    d.race = copy.deepcopy(d.race)
    for r in d.race:
        d.race[r]["general"] = 1000
        d.race[r]["hero"] = 1000
    content = pdr.load_root(getattr(d, "root", None) or "game/data", kinds=("culture",))
    d.figure_cultures = {k: dict(c["naming"], regnal_permille=1000) for k, c in content["culture"]}
    return d


class riders_off:
    """with riders_off(): the riders of 25.1 and 25.3 off (no data switch exists for them, 7.11's reading): the
    peoples' fading of 0.26.0 and P5-03a's undead threshold; 7.8's kind test with them."""

    def __enter__(self):
        import p3_peoples_reference as pp
        self.saved = (pp.FLAGS.get("nofade"), _pcr().FLAGS.get("riders"))
        pp.FLAGS["nofade"] = False
        _pcr().FLAGS["riders"] = False
        return self

    def __exit__(self, *a):
        import p3_peoples_reference as pp
        pp.FLAGS["nofade"], _pcr().FLAGS["riders"] = self.saved


def faded_data(d):
    """The 0.26.0 fields the riders removed (P5-03 11.1, P3-05 12.1), for a run with the riders off."""
    d2 = _pcr().edited(d, peoples=dict(ruin_fade_years=1500, ghost_fade_years=0))
    d2.figures = d.figures
    if hasattr(d, "figure_cultures"):
        d2.figure_cultures = d.figure_cultures
    return d2


def without_figures(fn):
    pcr = _pcr()
    saved = pcr.FLAGS["figures"]
    pcr.FLAGS["figures"] = False
    try:
        return fn()
    finally:
        pcr.FLAGS["figures"] = saved


I0_TABLES = ("event", "event_participant", "event_cause", "event_value", "phase", "civilization", "settlement",
             "settlement_census", "region_control", "relation", "region_name", "beast_population", "monster", "site",
             "race_relation", "settlement_race")


def i0_differences(run, base):
    """7.11 against the same rules without the subsystem (no FigureView): (tables that differ, zero, off)."""
    zero = run(zero_data(base))
    off = without_figures(lambda: run(zero_data(base)))
    diff = [t for t in I0_TABLES if zero.get(t, []) != off.get(t, [])]
    if zero.get("figure") or zero.get("book"):
        diff.append("figure/book not empty")
    return diff, zero, off


def i0_026_differences(run, base_root=None):
    """7.11 against generator 0.26.0 itself: the zero tuning at 0.27.0 with the riders off (riders_off,
    faded_data) against the 0.26.0 rules and data (the catalogue rows of kinds 79 to 92 and the changed templates
    aside: event_kind is not compared)."""
    pcr = _pcr()
    pcr.use_version("0.26.0")
    try:
        old = run(pcr.data(base_root) if base_root else None)
    finally:
        use_027()
    d = faded_data(zero_data(pcr.data(base_root) if base_root else data_027()))   # before riders_off: data_027
    with riders_off():                                                             # resets the flags
        new = run(d)
    return [t for t in I0_TABLES if old.get(t, []) != new.get(t, [])], old, new


def first_difference(a, b):
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return i, x, y
    if len(a) != len(b):
        return min(len(a), len(b)), None, None
    return None


SMALL_SEEDS = (3, 5, 6, 7, 10, 0, 1, 2, 9, 12)
I0_SEEDS = (0, 1, 2, 5)


def fixture_run(seed, dat, case=None):
    import p5_04_alliances_reference as p504
    return p504.fixture_run(seed, dat, case)


def small_case(seed):
    import p5_04_alliances_reference as p504
    return p504.small_case(seed)


def res_problems(res, dat):
    w = fw_of_res(res)
    cr = {k: v["regnal_permille"] for k, v in (getattr(dat, "figure_cultures", None) or {}).items()} or None
    qmax = dat.engine["quiet_events_max"] if isinstance(dat.engine, dict) and "quiet_events_max" in dat.engine \
        else None
    return invariant_violations(w, dat.figures, cr, qmax) + reader_problems(w)


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
        self.count += 1
        if not cond:
            self.failures += 1
            print("FAIL: %s" % what)


def kinds_of(res):
    key = {k[0]: k[1] for k in res["event_kind"]}
    return {key[e[3]] for e in res["event"] if e[3] >= 79}


def check_fixtures(c, seeds=SMALL_SEEDS, i0_seeds=I0_SEEDS, verbose=False):
    """18.3: figures-small makes every kind of 79 to 92 happen, regnal numbers up to III and the regnal_max stop;
    the invariants and reader checks hold on its histories and on the shipped tuning's stress histories; I0."""
    import time
    pcr = _pcr()
    use_027()
    seen = set()
    small = small_data()
    regnal_seen = set()
    stops = 0
    for seed in seeds:
        t0 = time.time()
        res = fixture_run(seed, small, small_case(seed))
        ks = kinds_of(res)
        seen |= ks
        regnal_seen |= {r[16] for r in res["figure"]}
        # 18.3: the regnal_max stop: a line whose name has regnal_max (3) recorded bearers, none above
        lines = {}
        for r in res["figure"]:
            if r[3] == RULER:
                lines.setdefault((r[6], r[1]), []).append(r[16])
        stops += sum(1 for v in lines.values() if len(v) >= small.figures["regnal_max"])
        c.ok("figures-small-%d: no regnal number above regnal_max" % seed,
             max([r[16] for r in res["figure"]] or [0]) <= small.figures["regnal_max"])
        for v in res_problems(res, small):
            c.ok("figures-small-%d: %s" % (seed, v), False)
        if verbose:
            print("figures-small-%d: %d events, %d figures, kinds %s (%.0fs)" % (
                seed, len(res["event"]), len(res["figure"]), sorted(KIND_ID[k] for k in ks), time.time() - t0))
            sys.stdout.flush()
    c.eq("18.3: every kind of 79 to 92 happens on figures-small", sorted(set(FIGURE_KIND_KEYS) - seen), [])
    c.ok("18.3: regnal numbers up to III on figures-small (seen %s)" % sorted(regnal_seen), 3 in regnal_seen)
    c.ok("18.3: a name reaches regnal_max bearers on figures-small (%d lines)" % stops, stops > 0)
    shipped = pcr.stress_data()
    for seed in i0_seeds:
        t0 = time.time()
        res = fixture_run(seed, shipped)
        for v in res_problems(res, shipped):
            c.ok("stress-%d: %s" % (seed, v), False)
        diff, _z, _o = i0_differences(lambda d: fixture_run(seed, d), pcr.stress_data())
        c.eq("7.11 I0 stress-%d: zero tuning against no subsystem" % seed, diff, [])
        diff, _a, _b = i0_026_differences(lambda d: fixture_run(seed, pcr.stress_data(d)))
        c.eq("7.11 I0 stress-%d against 0.26.0 (riders off)" % seed, diff, [])
        if verbose:
            print("stress-%d: invariants and I0 done (%.0fs)" % (seed, time.time() - t0))
            sys.stdout.flush()


# ---------------------------------------------------------------------------
# Units (hand-checked values)
# ---------------------------------------------------------------------------

def shipped_tuning():
    import p3_data_reference as pdr
    return dict(pdr.FIGURES_TUNING_027)


def check_units(c):
    import p3_data_reference as pdr
    t = shipped_tuning()
    tsp = tuple(t["temper_strain_permille"][x] for x in TEMPERS)
    W = {k: v[0] for k, v in pdr.RACE_FIELDS_027.items()}
    # 7.2: the means and factors the spec states, at its starting values (1000, 1300, 700, 1000, 1000)
    start = (1000, 1300, 700, 1000, 1000)
    c.eq("7.2 goblin mean_r (start)", temper_mean(W["goblin"], start), 1075)
    c.eq("7.2 goblin warlike (start)", strain_factor(1, W["goblin"], start), 1209)
    c.eq("7.2 goblin peaceable (start)", strain_factor(2, W["goblin"], start), 651)
    c.eq("7.2 elf mean_r (start)", temper_mean(W["elf"], start), 955)
    c.eq("7.2 orc mean_r (start)", temper_mean(W["orc"], start), 1135)
    # at the calibrated values (1000, 2000, 0, 1000, 1000), by hand: goblin (300 + 600 + 0 + 300 + 50) = 1250,
    # warlike 2000000 div 1250 = 1600, peaceable 0; elf 450 + 200 + 0 + 50 + 150 = 850; orc 250 + 1000 + 0 + 150 + 50
    c.eq("7.2 shipped temper_strain_permille", tsp, (1000, 2000, 0, 1000, 1000))
    c.eq("7.2 goblin mean_r", temper_mean(W["goblin"], tsp), 1250)
    c.eq("7.2 goblin warlike", strain_factor(1, W["goblin"], tsp), 1600)
    c.eq("7.2 goblin peaceable", strain_factor(2, W["goblin"], tsp), 0)
    c.eq("7.2 elf mean_r", temper_mean(W["elf"], tsp), 850)
    c.eq("7.2 orc mean_r", temper_mean(W["orc"], tsp), 1450)
    for r, ws in sorted(W.items()):   # 18.1 item 11: a race's mean factor is 1000 +- 1 (weighted by its leanings)
        m = sum(wt * strain_factor(i, ws, tsp) for i, wt in enumerate(ws)) // sum(ws)
        c.ok("7.2 %s mean factor %d within 1000 +- 2" % (r, m), abs(m - 1000) <= 2)
    zero = tuple(pdr.FIGURES_ZERO_027["temper_strain_permille"][x] for x in TEMPERS)
    for r, ws in sorted(W.items()):
        c.eq("7.2 zero tuning neutral for %s" % r, [strain_factor(i, ws, zero) for i in range(5)], [1000] * 5)
    # 6.3: a reign's extremes (u = 0 is the smallest pick, u = 2^64 - 1 the largest)
    lo, hi = 0, (1 << 64) - 1
    for race, g, lam in (("elf", 150, 700), ("goblin", 15, 40), ("human", 25, 70)):
        ln0, a0, _ = reign_values(g, lam, t, lo, lo, lo, (1, 1, 1, 1, 1))
        ln1, a1, _ = reign_values(g, lam, t, hi, hi, hi, (1, 1, 1, 1, 1))
        c.eq("6.3 %s reign length extremes" % race, (ln0, ln1), (max(1, g * 500 // 1000), g * 1500 // 1000))
        c.eq("6.3 %s accession age extremes" % race, (a0, a1), (lam * 250 // 1000, lam * 500 // 1000))
    c.eq("6.3 len at least 1", reign_values(1, 10, t, lo, lo, lo, (1, 0, 0, 0, 0))[0], 1)
    c.eq("6.3 temper weighted: only warlike", reign_values(10, 10, t, 5, 5, 12345, (0, 1, 0, 0, 0))[2], 1)
    # 6.9: lives at their extremes (the spec's elven and goblin generals)
    for race, lam, lo_age, hi_age, lo_d, hi_d in (("elf", 700, 210, 420, 490, 840), ("goblin", 40, 12, 24, 28, 48)):
        b0, d0 = life_values(lam, 0, 5000, 300, 300, t, lo, lo)
        b1, d1 = life_values(lam, 0, 5000, 300, 300, t, hi, hi)
        c.eq("6.9 %s general ages at entry" % race, (5000 - b0, 5000 - b1), (lo_age, hi_age))
        c.eq("6.9 %s general ages at death" % race, (d0 - b0, d1 - b1), (lo_d, hi_d))
    b, d = life_values(700, 4900, 5000, 300, 300, t, hi, lo)
    c.eq("6.9 born clamped at the awakening", b, 4900)
    c.eq("6.9 death at least y + 1", life_values(10, 0, 100, 0, 0, dict(t, life_min_permille=0, life_span_permille=0),
                                                 lo, lo)[1], 101)
    # 7.4
    c.eq("7.4 short reign bonus", split_bonus(1000, 990, 25, t), 100)
    c.eq("7.4 a reign of 12 years (12 < 12 false)", split_bonus(1012, 1000, 25, t), 0)
    c.eq("7.4 a reign of 11 years", split_bonus(1011, 1000, 25, t), 100)
    c.eq("7.4 the accession year", split_bonus(1000, 1000, 25, t), 100)
    # 7.6
    c.eq("7.6 no skill: 0.26.0's chance", led_win_chance(1000, 1000, 1000, 0, 0), 500)
    c.eq("7.6 skill 300 on the attacker", led_win_chance(1000, 1000, 1000, 300, 0), 1300 * 1000 // 2300)
    c.eq("7.6 skill on both", led_win_chance(1000, 1000, 1500, 100, 200), 1100000 // (1100 + 1500 * 1200 // 1000))
    c.eq("7.6 zero forces", led_win_chance(0, 0, 1000, 300, 300), 500)
    # 7.7
    c.eq("7.7 hunt with a hero", hero_hunt(20, 200, 5, 3, 1000, 1200, 30, 500), (60, 1800))
    c.eq("7.7 the hunt cap moves with the hero", hero_hunt(20, 200, 100, 9, 1000, 1000, 30, 0)[0], 230)
    c.eq("7.7 no hero", hero_hunt(20, 200, 5, 1, 1000, 1000, 0, 0), (20, 1000))
    # 25.3 (18.1 item 22): T at M = 500, 1000, 1600; R = 64, 65, 200 with undead_regions_ref 64 and 0
    P = 2000000
    for M, want in ((500, 50000), (1000, 50000), (1600, 31250)):
        c.eq("25.3 T at M %d" % M, undead_threshold(1000, 25, P, M, 40, 64), want)
    for R, ref, want in ((64, 64, 50000), (65, 64, 50000 * 64 // 65), (200, 64, 50000 * 64 // 200), (200, 0, 50000)):
        c.eq("25.3 T at R %d, ref %d" % (R, ref), undead_threshold(1000, 25, P, 1000, R, ref), want)
    c.eq("25.3 the floor undead_min_dead", undead_threshold(1000, 25, 1000, 1000, 200, 64), 1000)
    c.eq("25.3 the floor at high pressure", undead_threshold(1000, 25, 1000, 1600, 200, 64), 625)
    # 6.14 and 6.10
    reg = set()
    c.eq("6.14 E_c of an empty line", regnal_candidates([], 9, reg), [])
    c.eq("6.14 E_c", regnal_candidates([["Balin", 1], ["Dori", 2]], 9, reg), ["Balin", "Dori"])
    c.eq("6.14 regnal_max stops a name", regnal_candidates([["Balin", 3], ["Dori", 2]], 3, reg), ["Dori"])
    c.eq("6.14 a numbered form in the registry", regnal_candidates([["Balin", 1]], 9, {"Balin II"}), [])
    c.eq("6.14 roman II to XXXIX", (roman(2), roman(4), roman(9), roman(14), roman(38), roman(39)),
         ("II", "IV", "IX", "XIV", "XXXVIII", "XXXIX"))
    c.eq("6.14 the longest numeral", max(len(roman(k)) for k in range(2, 40)), 7)
    c.eq("6.10 display names", (display_name("Balin", 2, "the Victorious"), display_name("Maela", 0, "Dragonslayer"),
                                display_name("Bolli", 0, ""), display_name("Aldric", 3, "")),
         ("Balin II the Victorious", "Maela Dragonslayer", "Bolli", "Aldric III"))
    c.ok("6.14 fallback names", is_fallback_name("Gorm II") and not is_fallback_name("Gorm") and
         not is_fallback_name("Gorm the Old"))
    ent = pdr._epithet_dicts(pdr.EPITHETS_027)
    v0 = dict.fromkeys(pdr.DEEDS_027, 0)
    c.eq("6.10 nothing", epithet_of("ruler", v0, "", ent, {}, 2), ("", ""))
    c.eq("6.10 one conquest", epithet_of("ruler", dict(v0, conquests=1), "", ent, {}, 2), ("the Conqueror", "ordinary"))
    c.eq("6.10 a third ordinary conqueror takes the next entry",
         epithet_of("ruler", dict(v0, conquests=1, realm_fell=1), "", ent, {"the Conqueror": 2}, 2),
         ("the Last", "ordinary"))
    c.eq("6.10 a third and fourth substantial conqueror",
         [epithet_of("ruler", dict(v0, conquests=2), "", ent, {"the Conqueror": n}, 2) for n in (2, 3)],
         [("the Conqueror", "substantial")] * 2)
    c.eq("6.10 an ordinary earning refused with two substantial bearers",
         epithet_of("ruler", dict(v0, conquests=1), "", ent, {"the Conqueror": 2}, 2), ("", ""))
    c.eq("6.10 substantial 0 is never substantial",
         epithet_of("ruler", dict(v0, realm_fell=1), "", ent, {"the Last": 2}, 2), ("", ""))
    c.eq("6.10 rule order: conquests before wars_won",
         epithet_of("ruler", dict(v0, conquests=1, wars_won=5), "", ent, {}, 2), ("the Conqueror", "ordinary"))
    c.eq("6.10 {slayer}", epithet_of("hero", dict(v0, slew_might=6000), "Dragonslayer", ent, {}, 2),
         ("Dragonslayer", "substantial"))
    c.eq("6.10 {slayer} ordinary", epithet_of("hero", dict(v0, slew_might=700), "Trollbane", ent,
                                              {"Trollbane": 2}, 2), ("", ""))
    c.eq("6.10 reign_permille 1300 ordinary", epithet_of("ruler", dict(v0, reign_permille=1300), "", ent, {}, 2),
         ("the Old", "ordinary"))
    c.eq("6.10 battles unbeaten before battles won",
         epithet_of("general", dict(v0, battles_won=10, battles_unbeaten=10), "", ent, {}, 2),
         ("the Unbeaten", "substantial"))
    c.eq("6.10 the Hammer", epithet_of("general", dict(v0, battles_won=5), "", ent, {}, 2), ("the Hammer", "ordinary"))
    # 11.4: the tower glyph
    c.eq("11.4 tower glyph from its rule", tower_rows_from_rule(), TOWER_ROWS)
    cls = tower_glyph_cells()
    c.eq("11.4 counts: halo, fill, ink", (sum(1 for v in cls.values() if v == 1), sum(1 for v in cls.values()
                                                                                    if v == 2),
                                          sum(1 for v in cls.values() if v == 3)), (20, 3, 10))
    c.eq("11.4 offsets", (site_glyph_offset(3, 5), site_glyph_offset(4, 5), site_glyph_offset(4, 0)),
         ((7, -7), (-7, -7), (0, 0)))
    c.eq("11.4 hover", (marker_hover("The Tower of Edric", "Edric the Learned", 4321, False),
                        marker_hover("The Library of Edric", "Edric", 4321, True)),
         ("The Tower of Edric (tower of Edric the Learned), since 4321",
          "The Library of Edric (library of Edric), since 4321"))
    c.eq("11.3 groups of 79 to 92", [GROUP_OF_NEW[k] for k in range(79, 93)],
         [8, 8, 8, 8, 8, 8, 2, 8, 8, 5, 8, 8, 8, 3])
    c.eq("3 streams", (STREAMS_BEFORE + len(STREAMS), STREAMS[0], STREAMS[-1]),
         (97, "hist.figures.reign", "hist.monsters.hero_fall"))
    # 9.4: reader messages on hand-made rows (the stub world of `reader_stub`)
    check_reader_units(c)


def reader_stub():
    """A minimal world of rows: one realm with a founder, a general and a hero; every row valid."""
    t = {n: [] for n in READ_TABLES}
    t["region"] = [(1, "Grey Fells") + (0,) * 26, (2, "Cold Peaks") + (0,) * 26]
    t["race"] = [(1, "human", "Human", "Humans", "human", 1, 70, 25, 1, 1, 900, 1)]
    t["culture"] = [(1, "human", "Human", 1)]
    t["event_kind"] = [(i, k) for i, k in ((2, "civ_founded"), (18, "civ_ended"), (21, "war_declared"), (27, "peace"))]
    t["event_kind"] += [(i, k) for i, k in KINDS] + [(48, "monster_raid"), (45, "monster_slain")]
    roles = {79: ("figure", "civilization"), 80: ("figure", "civilization"),
             81: ("civilization", "ruler", "claimant", "new_realm"), 82: ("figure", "civilization", "host"),
             83: ("figure", "civilization"), 84: ("figure", "civilization", "enemy"),
             85: ("winner", "loser", "winner_ally", "loser_ally", "winner_general", "loser_general"),
             86: ("figure", "civilization", "enemy"), 87: ("figure", "civilization", "monster"),
             88: ("monster", "slayer", "hero"), 89: ("figure", "civilization", "monster"),
             90: ("figure", "civilization", "site"), 91: ("figure", "civilization", "site"),
             92: ("figure", "civilization", "site"), 2: ("civilization", "race"), 21: ("attacker", "defender"),
             27: ("winner", "loser"), 18: ("civilization",)}
    t["event_kind_role"] = [(k, i + 1, r, 0, 0, 1) for k, rs in roles.items() for i, r in enumerate(rs)]
    vals = {80: ("how", "age"), 81: ("outcome",), 82: ("hosted",), 84: ("again",), 85: ("winner_dead", "loser_dead",
                                                                                    "pass"),
            88: ("hunters",), 90: ("library",), 92: ("dead",), 27: ("years",)}
    t["event_kind_value"] = [(k, i + 1, v) for k, vs in vals.items() for i, v in enumerate(vs)]
    t["phase"] = [(1, 1, 1, 1000, 1999, 0)]
    ev = [(1, 1000, 1000, 2, 1, 1, 0, 50), (2, 1000, 1000, 79, 1, 1, 0, 15), (3, 1010, 1010, 21, 1, 1, 0, 70),
          (4, 1010, 1010, 84, 1, 1, 0, 15), (5, 1020, 1020, 27, 1, 1, 0, 30), (6, 1030, 1030, 80, 1, 1, 1, 15)]
    t["event"] = ev
    t["event_participant"] = [(1, 1, 1, CIV, 1), (2, 1, 1, FIGURE, 1), (2, 2, 2, CIV, 1), (3, 1, 1, CIV, 1),
                              (3, 2, 2, CIV, 2), (4, 1, 1, FIGURE, 2), (4, 2, 2, CIV, 1), (4, 3, 3, CIV, 2),
                              (5, 1, 1, CIV, 1), (5, 2, 2, CIV, 2), (6, 1, 1, FIGURE, 1), (6, 2, 2, CIV, 1)]
    t["event_cause"] = [(2, 1), (4, 3), (6, 2)]
    t["event_value"] = [(4, 1, 0), (5, 1, 10), (6, 1, 0), (6, 2, 50)]
    t["civilization"] = [(1, "The Kingdom of Cear", 1, 1, 1, 1, 1000, 1, None, 0),
                         (2, "The Hold of Durn", 1, 1, 2, 2, 1000, 1, None, 0)]
    t["settlement"] = [(1, "Cear", 5, 5, 1, 1, 1000, 1, 1, 3, 1, 500, None, 0, 0),
                       (2, "Durn", 9, 9, 2, 2, 1000, 1, 2, 3, 1, 500, None, 0, 0)]
    t["figure"] = [(1, "Aldric", "", 1, 1, 1, 1, 980, 1000, 2, 1030, 6, 1, 1, 3, 1, 0, 1, 0),
                   (2, "Edric", "", 3, 1, 1, 1, 980, 1010, 4, None, 0, 0, 0, 1, 0, 0, 0, 150)]
    t["relation"] = [(FIGURE, 1, CIV, 1, "ruled", 1000, 1030, 2, 6),
                     (FIGURE, 2, CIV, 1, "served", 1010, 1020, 4, 5)]
    meta = {"history_end_year": "2000", "schema_version": "12"}
    return meta, t


def check_reader_units(c):
    import copy
    meta, t = reader_stub()
    c.eq("9.4 the stub is valid", reader_problems(FW(meta, t)), [])

    def first(edit):
        t2 = copy.deepcopy(t)
        edit(t2)
        p = reader_problems(FW(meta, t2))
        return p[0] if p else None

    def fig(i, col, v):
        def ed(t2):
            r = list(t2["figure"][i])
            r[FIG_COLS.index(col)] = v
            t2["figure"][i] = tuple(r)
        return ed
    F1 = "corrupt table 'figure': row 1: "
    F2 = "corrupt table 'figure': row 2: "
    for edit, want in (
            (fig(0, "name", "aldric 7"), F1 + "name is not a name"),
            (fig(0, "epithet", " the Old"), F1 + "epithet is not an epithet"),
            (fig(0, "role", 6), F1 + "role 6 outside 1 to 5"),
            (fig(0, "race", 9), F1 + "unknown race 9"),
            (fig(0, "culture", 9), F1 + "unknown culture 9"),
            (fig(0, "civ", 9), F1 + "unknown civilization 9"),
            (fig(0, "start_event", 99), F1 + "start_event 99 is not an event"),
            (fig(0, "born", 1001), F1 + "born 1001 after office_from 1000"),
            (fig(0, "born", 999), None),
            (fig(1, "born", 990), None),
            (fig(0, "born", 899), F1 + "born 899 before the race's awakening"),
            (fig(1, "office_from", 1011), F2 + "office_from 1011 after the year of start_event"),
            (fig(0, "status", 7), F1 + "status 7 outside 1 to 6"),
            (fig(1, "death_region", 1), F2 + "end columns disagree"),
            (fig(0, "died", 999), F1 + "died 999 before office_from"),
            (fig(0, "death_event", 5), F1 + "death_event 5 is not this figure's death"),
            (fig(0, "death_region", 9), F1 + "death_region 9 is not a region"),
            (fig(0, "death_settlement", 9), F1 + "death_settlement 9 is not a settlement"),
            (fig(1, "reign_no", 1), F2 + "reign_no 1 of a general"),
            (fig(0, "regnal", 1), F1 + "regnal 1 outside 0 or 2 to 39"),
            (fig(1, "regnal", 2), F2 + "regnal 2 of a general"),
            (lambda t2: (fig(1, "name", "Aldric")(t2), fig(1, "civ", 2)(t2)), F2 + "name repeats figure 1"),
            (fig(1, "name", "Aldric"), F2 + "regnal 0 but 1 earlier bearers"),   # reading R31
            (fig(0, "regnal", 2), F1 + "regnal 2 but 0 earlier bearers"),
            (fig(0, "temper", 5), F1 + "temper 5 outside 0 to 4"),
            (fig(1, "skill", 0), F2 + "skill 0 outside 1 to 1000"),
            (fig(1, "temper", 2), F2 + "temper 2 of a general"),
            (fig(0, "skill", 3), F1 + "skill 3 of a ruler"),
            (fig(1, "status", 2), F2 + "in exile but not a claimant")):
        c.eq("9.4 %s" % want, first(edit), want)

    def rel(i, col, v):
        def ed(t2):
            r = list(t2["relation"][i])
            r[col] = v
            t2["relation"][i] = tuple(r)
        return ed
    c.eq("9.4 L1", first(rel(0, 2, FIGURE)), "corrupt table 'relation': row (6, 1, 6, 1, ruled, 1000): ruled with "
                                             "sides of types 6 and 6")
    c.eq("9.4 L3", first(rel(0, 7, 1)), "corrupt table 'relation': row (6, 1, 3, 1, ruled, 1000): ruled since_event "
                                        "1 is not the ruler's record")
    c.eq("9.4 L4", first(rel(0, 8, 5)), "corrupt table 'relation': row (6, 1, 3, 1, ruled, 1000): ruled until_event "
                                        "5 does not end the reign")
    c.eq("9.4 L6", first(rel(1, 7, 3)), "corrupt table 'relation': row (6, 2, 3, 1, served, 1010): served since_event "
                                        "3 is not a general_appointed of the pair")
    c.eq("9.4 L7", first(rel(1, 8, 4)), "corrupt table 'relation': row (6, 2, 3, 1, served, 1010): served until_event "
                                        "4 does not end the command")
    c.eq("9.4 E1", first(lambda t2: t2["event_participant"].append((5, 3, 3, FIGURE, 9))),
         "corrupt table 'event': row 5: figure 9 has no row")
    c.eq("9.4 E3", first(lambda t2: t2["event_value"].__setitem__(2, (6, 1, 3))),
         "corrupt table 'event': row 6: value how 3 outside 0 to 2")


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def shipped_cultures_regnal():
    import p3_data_reference as pdr
    return {k: v[3] for k, v in pdr.CULTURE_FIELDS_027.items()}


def cmd_check_world(paths, tuning=None):
    import p3_data_reference as pdr
    t = tuning or shipped_tuning()
    bad = 0
    for path in paths:
        w = fw_of(path)
        c = Checker()
        c.eq("%s schema_version" % path, w.meta.get("schema_version"), str(SCHEMA_VERSION))
        for v in invariant_violations(w, t, shipped_cultures_regnal()):
            c.ok("%s invariant %s" % (os.path.basename(path), v), False)
        for m in reader_problems(w):
            c.ok("%s reader %s" % (os.path.basename(path), m), False)
        st = dict(stats(w))
        print("%s: %d figures (%d rulers, %d generals, %d heroes, %d casters), %d books; %d checks, %d failures" % (
            os.path.basename(path), st["figures"], st["figures_rulers"], st["figures_generals"], st["figures_heroes"],
            st["figures_casters"], st["books"], c.count, c.failures))
        bad += c.failures
    del pdr
    return 1 if bad else 0


def world_run(name, inputs, dat):
    pcr = _pcr()
    for n, seed, size, span, races, detail, civs, intensity in pcr.world_cases():
        if n == name:
            return pcr.world_case(n, seed, size, span, races, detail, inputs, dat, civs, intensity)
    raise SystemExit("unknown world case %s" % name)


def stored(path, tables):
    pcr = _pcr()
    import p3_chronology_reference as pchr
    conn = pchr.open_world(path)
    try:
        return {t: pcr.stored_rows(conn, t) for t in tables}
    finally:
        conn.close()


def cmd_i0_world(argv):
    """7.11 on world cases (their geography from --inputs DIR): (a) the zero tuning against no subsystem (0.27.0
    rules, riders on); (b) the zero tuning with the riders off against 0.26.0's rules and data; (c) with
    --compare-db DIR (worlds of the 0.26.0 C++ worldgen-cli, DIR/<case>.db): (b)'s tables against the stored ones,
    and the zero tuning with the riders on against them up to the first ruin_faded event."""
    import time
    inputs = argv[argv.index("--inputs") + 1]
    cdb = argv[argv.index("--compare-db") + 1] if "--compare-db" in argv else None
    names = [a for a in argv if not a.startswith("--") and a not in (inputs, cdb)]
    c = Checker()
    pcr = _pcr()
    for name in names:
        t0 = time.time()
        use_027()
        run = lambda d: world_run(name, inputs, d)   # noqa: E731
        if "--no-off" not in argv:
            diff, zero, _off = i0_differences(run, data_027())
            c.eq("7.11 I0 %s: zero tuning against no subsystem (%d events)" % (name, len(zero["event"])), diff, [])
        diff, old, new = i0_026_differences(run)
        c.eq("7.11 I0 %s against 0.26.0, riders off (%d events)" % (name, len(old["event"])), diff, [])
        if cdb:
            st = stored(os.path.join(cdb, name + ".db"), I0_TABLES)
            for tname in I0_TABLES:
                fd = first_difference(st[tname], new.get(tname, []))
                c.eq("7.11 I0 %s: table %s equals the 0.26.0 C++ world (riders off)" % (name, tname), fd, None)
            use_027()
            on = run(zero_data(data_027()))
            faded = [e for e in st["event"] if pcr.KEY_OF.get(e[3]) == "ruin_faded"]
            cut = faded[0][0] if faded else None
            ev_on = on["event"] if cut is None else [e for e in on["event"] if e[0] < cut]
            ev_st = st["event"] if cut is None else [e for e in st["event"] if e[0] < cut]
            c.eq("7.11 I0 %s: riders on, events %s equal the 0.26.0 C++ world" % (
                name, "all" if cut is None else "before the first ruin_faded e%d" % cut),
                first_difference(ev_st, ev_on), None)
            print("%s: %d ruin_faded in the 0.26.0 world; riders on: %d events against %d" % (
                name, len(faded), len(on["event"]), len(st["event"])))
        print("%s: %.0fs" % (name, time.time() - t0))
        sys.stdout.flush()
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def write_world_db(res, path, template):
    """A schema 12 world file of a replayed history: a copy of `template` (any world of the same seed and map
    size: its geography and non-history tables) with every history table replaced by the replay's rows, figure and
    book created. For check-world and golden runs on the reference's own histories."""
    import shutil
    shutil.copyfile(template, path)
    conn = sqlite3.connect(path)
    try:
        for tname in I0_TABLES + ("event_kind",):
            rows = res.get(tname)
            if rows is None or not conn.execute("SELECT 1 FROM sqlite_schema WHERE name = ?", (tname,)).fetchone():
                continue
            ncol = len(conn.execute('SELECT * FROM "%s" LIMIT 0' % tname).description)
            conn.execute('DELETE FROM "%s"' % tname)
            if rows:
                rr = [tuple(r)[:ncol] + (0,) * (ncol - len(r)) for r in rows]
                conn.executemany('INSERT INTO "%s" VALUES (%s)' % (tname, ",".join("?" * ncol)), rr)
        conn.execute("DELETE FROM event_kind_role")
        conn.executemany("INSERT INTO event_kind_role VALUES (?,?,?,?,?,?)", res["event_kind_role"])
        conn.execute("DELETE FROM event_kind_value")
        conn.executemany("INSERT INTO event_kind_value VALUES (?,?,?)", res["event_kind_value"])
        conn.execute("DROP TABLE IF EXISTS figure")
        conn.execute("DROP TABLE IF EXISTS book")
        conn.execute("CREATE TABLE figure (id INTEGER PRIMARY KEY, name TEXT NOT NULL, epithet TEXT NOT NULL, role "
                     "INTEGER NOT NULL, race INTEGER NOT NULL, culture INTEGER NOT NULL, civ INTEGER NOT NULL, born "
                     "INTEGER NOT NULL, office_from INTEGER NOT NULL, start_event INTEGER NOT NULL, died INTEGER, "
                     "death_event INTEGER NOT NULL, death_region INTEGER NOT NULL, death_settlement INTEGER NOT NULL, "
                     "status INTEGER NOT NULL, reign_no INTEGER NOT NULL, regnal INTEGER NOT NULL, temper INTEGER NOT "
                     "NULL, skill INTEGER NOT NULL) STRICT")
        conn.execute("CREATE TABLE book (id INTEGER PRIMARY KEY, title TEXT NOT NULL, author INTEGER NOT NULL, site "
                     "INTEGER NOT NULL, year INTEGER NOT NULL, event INTEGER NOT NULL) STRICT")
        conn.executemany("INSERT INTO figure VALUES (%s)" % ",".join("?" * 19), res.get("figure", []))
        conn.executemany("INSERT INTO book VALUES (?,?,?,?,?,?)", res.get("book", []))
        conn.execute("UPDATE meta SET value = '12' WHERE key = 'schema_version'")
        conn.execute("UPDATE meta SET value = '0.27.0' WHERE key = 'generator_version'")
        conn.commit()
    finally:
        conn.close()


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    cmd = argv[0]
    if cmd == "units":
        use_027()
        c = Checker()
        check_units(c)
        print("%d checks, %d failures" % (c.count, c.failures))
        return 1 if c.failures else 0
    if cmd == "check-world":
        use_027()
        return cmd_check_world(argv[1:])
    if cmd == "fixtures":
        c = Checker()
        quick = "--quick" in argv
        check_fixtures(c, SMALL_SEEDS[:4] if quick else SMALL_SEEDS, I0_SEEDS[:2] if quick else I0_SEEDS,
                       verbose=True)
        print("%d checks, %d failures" % (c.count, c.failures))
        return 1 if c.failures else 0
    if cmd == "i0-world":
        return cmd_i0_world(argv[1:])
    if cmd == "world":
        import time
        inputs = argv[argv.index("--inputs") + 1]
        out = argv[argv.index("--out") + 1] if "--out" in argv else None
        name = [a for a in argv[1:] if not a.startswith("--") and a not in (inputs, out)][0]
        t0 = time.time()
        use_027()
        dat = small_data() if "--small" in argv else data_027()
        res = world_run(name, inputs, dat)
        print("%s: %d events, %d figures, %d books, kinds %s (%.0fs)" % (
            name, len(res["event"]), len(res["figure"]), len(res["book"]),
            sorted(KIND_ID[k] for k in kinds_of(res)), time.time() - t0))
        probs = res_problems(res, dat)
        for p in probs[:40]:
            print("  problem: %s" % p)
        print("  %d problems" % len(probs))
        for k, v in stats(fw_of_res(res)):
            print("  stat %s %s" % (k, v))
        if out:
            write_world_db(res, out, os.path.join(inputs, _pcr().INPUT_OF.get(name, name) + ".db"))
        return 1 if probs else 0
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
    if cmd == "readings":
        print(READINGS)
        print(REPLAY_TODO)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
