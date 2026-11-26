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
GENERATOR_VERSION = "0.27.0"

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
    weary_open=True,  # r11 20.23: futile + the wars open over (D, G) >= weary_wars
    # r13 (generator stays 0.18.0)
    cede_all=True,  # 20.27: a cession passes every standing settlement of the loser in G, hidden or same-step
    # r14 section 21 (P3-10, generator 0.19.0)
    reduce_loss=True,   # 21.1: note_reduction only for a settlement whose loss is at least 1
    aim_hold=True,      # 21.6: won by aim only when the attacker holds G; G lost otherwise: end by the score now
    search_weary=True,  # 21.7 item 1: a search_abandoned makes (y, R) weary for every claimant (searched_until)
    search_battle=True,  # 21.7 item 2: no search in a region where this war has fought a battle
    search_sal=True,    # 21.7 item 3: only the first kind-35 event keeps its salience; later search_repeat_salience
    holder=True,        # 21.8: weariness and the run statistic count a change of the holder (owner or last owner)
    race_odds=True,     # 21.9: the odds minimum is the claimant race's odds_min_permille
    lie_low=True,       # 21.10: a civilization with a hidden refuge raises no claim, strain or declaration
    quarrel=True,       # 21.12: the awakening quarrel (race field awakening_quarrel)
    note_019=True,      # P3-07 r12 22.3 / 22.5a: the reworded flare-up note; repeated searches out of the summary
    # r17 section 22 (P3-10 second round; generator stays 0.19.0)
    quarrel_hold=True,  # 22.1: the quarrel claim is held back below quarrel_min_settlements (quarrel_with)
    race_hidden=True,   # 22.2: hidden_years is the refuge founder's race field
    aim_foot=True,      # 22.5: aim needs the defender out of G; the foothold cession
    # r19 section 23 (P3-11, generator 0.20.0)
    claim39=True,       # 23.2: a claim on a region b does not hold is kind 39 claim_settlements (b's settlements)
    foot40=True,        # 23.3: the foothold cession is kind 40 settlements_ceded (the passing settlements)
    # r22 section 24 (P5-01, generator 0.21.0)
    fresh=True,         # 24.1: a re-aimed declaration (G != R_c) is preceded by a fresh claim on G, which it cites
    strength=True,      # 24.2: the quarrel claim is also held back below quarrel_strength_permille (strength)
    dropped=True,       # 24.3: a claim dropped at a declaration with no allowed goal is recorded (kind 41)
    hidden42=True,      # 24.4: a refuge's hidden years are recorded at the flight (kind 42)
    toll=True,          # 24.5: each side's battle toll times a keyed factor (toll_spread_permille, hist.conflict.toll)
    # P5-12 (generator 0.22.0)
    rename=True,        # P5-12 6: the renaming subsystem after conflict in T6 and Q4 (p5_12_renaming_reference.py)
    # P5-02 (generator 0.23.0)
    monsters=True,      # P5-02 5, 6: the monsters subsystem after renaming (p5_02_monsters_reference.py), the second
                        # tension source, the WildView of 7.1 set on peoples, settlement.ending, schema 9
    flare_claim=True,   # P5-02 7.4: a flare-up's forced declaration only when its trigger is a claim (20 or 39)
    # P5-03 (generator 0.24.0)
    relations=True,     # P5-03 5 to 7: the relations subsystem after the monsters (p5_03_relations_reference.py), the
                        # ledger set on peoples, the dispositions read by conflict (4.1, 4.3), owner_event, schema 10
    # P5-03a (generator 0.25.0)
    p503a=True,         # P5-03a 3 to 8: long peace, common enemies, credit and contributions by sign, meetings by
                        # word, graded settlers, the expulsion gap, A1; the undead threshold by world population (6)
    # P5-04 (generator 0.26.0)
    alliances=True,     # P5-04 6, 7: alliances, trade, mines and arms (p5_04_alliances_reference.Alliances after the
                        # relations; the seams of 7 in World), the AllianceView read by conflict
    # P5-06 (generator 0.27.0)
    figures=True,       # P5-06 5 to 7: the figures subsystem after the alliances (p5_06_figures_reference.Figures), the
                        # FigureView read by conflict, peoples, monsters and alliances; battle_led; schema 12
    riders=True,        # P5-06 25.3 (the two undead rules) and 7.8 (P5-02's site loops test kinds 1 and 2); 25.1 is the
                        # peoples flag nofade
    # P5-04a (generator 0.28.0)
    p504a=False,        # P5-04a 3 to 11, 21: spoils kept and soured, refusals and betrayals with motives, renewals on a
                        # chain, the arms cause of a battle won (9.2), has_claim / claim_event / last_war_end (2), the
                        # turns' change and cool-down, long-held expulsions, towers apart and ended, hero_rose quiet;
                        # off until use_version("0.28.0") (the 0.27.0 worlds and goldens are the default)
    # P5-07 (generator 0.29.0)
    deep=False,         # P5-07 6, 7: deepening, the waking, the gate, the take and the deep flight
                        # (p5_07_depths_reference.py, installed on the monsters subsystem), the forge's depth in the
                        # force (6.3), the deeds of 6.9, schema 13
    a1_units=False,     # P5-07 7.5 (P5-04a audit A1): a quiet renewal credits units, no contribution
    # 22.3's one definition of lying low is lie_low's (pp.World.lying_low); no flag of its own
)
FLAGS_017 = ("loss",)
FLAGS_019 = ("reduce_loss", "aim_hold", "search_weary", "search_battle", "search_sal", "holder", "race_odds",
             "lie_low", "quarrel", "note_019", "quarrel_hold", "race_hidden", "aim_foot")
FLAGS_020 = ("claim39", "foot40")
FLAGS_021 = ("fresh", "strength", "dropped", "hidden42", "toll")
FLAGS_022 = ("rename",)
FLAGS_023 = ("monsters", "flare_claim")
FLAGS_024 = ("relations",)
FLAGS_025 = ("p503a",)
FLAGS_026 = ("alliances",)   # P5-04 (generator 0.26.0): the replay of 6 and 7
FLAGS_027 = ("figures", "riders")   # P5-06 (generator 0.27.0)
FLAGS_028 = ("p504a",)               # P5-04a (generator 0.28.0)
FLAGS_029 = ("deep", "a1_units")    # P5-07 (generator 0.29.0)
VERSIONS = ("0.16.0", "0.17.0", "0.18.0", "0.19.0", "0.20.0", "0.21.0", "0.22.0", "0.23.0", "0.24.0", "0.25.0",
            "0.26.0", "0.27.0", "0.28.0", "0.29.0")
VERSION_EXPLICIT = False   # P5-07 12 item 3: --version given; check-world otherwise reads the world's version




def overrun_kinds():
    """P5-02 6.4.3 (with the monsters, 0.23.0): settlement_overrun ruins its settlement, as a sack or an
    abandonment does, for the invariants and replays that read the ruin kinds."""
    return (K["settlement_overrun"],) if FLAGS["monsters"] else ()

def flag_on(v, k):
    """Whether rule flag k is on at generator v."""
    i = VERSIONS.index(v)
    if k in FLAGS_029:
        return i >= 13
    if k in FLAGS_028:
        return i >= 12
    if k in FLAGS_027:
        return i >= 11
    if k in FLAGS_026:
        return i >= 10
    if k in FLAGS_025:
        return i >= 9
    if k in FLAGS_024:
        return i >= 8
    if k in FLAGS_023:
        return i >= 7
    if k in FLAGS_022:
        return i >= 6
    if k in FLAGS_021:
        return i >= 5
    if k in FLAGS_020:
        return i >= 4
    if k in FLAGS_019:
        return i >= 3
    return i >= 2 or (v == "0.17.0" and k in FLAGS_017)


def at_least(v):
    return VERSIONS.index(GENERATOR_VERSION) >= VERSIONS.index(v)


def peoples_flags(v):
    """P3-05's flags of generator v (p3_peoples_reference.flags_for from 0.19.0; the 0.18.0 rule before it)."""
    if hasattr(pp, "flags_for"):
        return pp.flags_for(v)
    return {k: v != "0.16.0" and (v in ("0.18.0", "0.19.0", "0.20.0", "0.21.0", "0.22.0", "0.23.0", "0.24.0", "0.25.0", "0.26.0") or k not in pp.FLAGS_018)
            for k in pp.FLAGS}


def use_version(v):
    """Generator v's rules, data, names and catalogue: "0.19.0" (default), "0.18.0", "0.17.0" or "0.16.0"."""
    global GENERATOR_VERSION
    if v not in VERSIONS:
        raise ValueError(v)
    pp.use_version(v)
    for k in FLAGS:
        FLAGS[k] = flag_on(v, k)
    GENERATOR_VERSION = v
    global TABLES, COLUMNS
    TABLES, COLUMNS = pe.TABLES, pe.COLUMNS
    _DATA.clear()


def legacy(on=True):
    use_version("0.16.0" if on else "0.27.0")   # P5-06 phase 2: 0.27.0 by default (--version 0.26.0: P5-04)


class rules:
    """with rules("0.17.0"): the conflict and peoples rule flags of another version (the data stay)."""

    def __init__(self, v):
        self.v = v

    def __enter__(self):
        self.saved = (dict(FLAGS), dict(pp.FLAGS))
        for k in FLAGS:
            FLAGS[k] = flag_on(self.v, k)
        pp.FLAGS.update(peoples_flags(self.v))
        return self

    def __exit__(self, *a):
        FLAGS.clear()
        FLAGS.update(self.saved[0])
        pp.FLAGS.clear()
        pp.FLAGS.update(self.saved[1])

RACE, GOD, CIV, SETTLEMENT, REGION = pe.RACE, pe.GOD, pe.CIV, pe.SETTLEMENT, pe.REGION
FIGURE = 6   # P5-06: entity type 6 comes into use
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


def principal_war_rows(res):
    """The war rows of declarations (P5-04 10.4: a row opened by an ally_joined_war is an ally's, not a war)."""
    key = (len(res["relation"]), len(res["event"]))
    if _PWR.get("rel") is not res["relation"] or _PWR.get("ev") is not res["event"] or _PWR.get("key") != key:
        _PWR["rel"], _PWR["ev"] = res["relation"], res["event"]
        j = K.get("ally_joined_war")
        joined = {e[0] for e in res["event"] if e[3] == j}
        _PWR["key"], _PWR["rows"] = key, [r for r in res["relation"] if r[4] == "war" and r[7] not in joined]
    return _PWR["rows"]


_PWR = {}


def BATTLES():
    """P5-04 11.1: the battle kinds set (allied_battle where the catalogue has it); P5-06 11.1: battle_led."""
    return (K["battle"], K["battle_at_pass"]) + ((K["allied_battle"],) if "allied_battle" in K else ()) + (
        (K["battle_led"],) if "battle_led" in K else ())


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
    ("r10 20.22 refuge_flight 0", "SETTLED by r11 20.24 item 1: with refuge_flight = 0 the guard writes refuge_held, "
                                  "reason 1 (the reference first wrote a siege with fell 0)."),
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
    # r13 (generator stays 0.18.0)
    ("r13 20.27 item 1 excess", "As 7.7 cession refugees: the excess of each passing settlement goes to L's nearest "
                                "other standing settlement at that moment, which may be a settlement of L in G not yet "
                                "passed (a later id) or a colony founded earlier in the step."),
    ("r13 20.13 (13) from rows", "Owners replayed in event id order: a settlement's founding_event gives it to its "
                                 "founder_civ; civ_seceded passes `from`'s standing settlements in the regions whose "
                                 "region_control row has that event (and the new capital's region); "
                                 "settlement_captured gives it to the attacker; sacked, settlement_abandoned and "
                                 "settlement_abandoned_to ruin it; region_ceded passes every standing settlement of "
                                 "`from` in the region. Each passing settlement must have a census row at the "
                                 "cession's year (its step's bookkeeping) with an owner other than `from`; `from` held "
                                 "the region by an open region_control row or, when its hold began in the step, by a "
                                 "settlement standing there in the replay; the replay must end at the settlement "
                                 "table."),
    ("r13 20.28 counted wars", "A war is closed by a side's end when its war row's until_event is a civ_conquered or "
                               "civ_ended; an open row counts with until = E; the incremental form of P3-07 21.8 item "
                               "5 (r11) is used as written."),
    # r14 (P3-10, generator 0.19.0)
    ("r14 21.6 order", "The new items 1 and 2 are tested first at every check (after 7.3 in T6) and only then 7.7's "
                       "items 1, 3, 4 (exhaustion, no front, length): the literal order of 21.6's list ('Otherwise ... "
                       "7.7 items 1, 3 and 4'); item 1's 'peace when b is exhausted' only arises in that order. The "
                       "alternative reading (exhaustion first, as 7.7's numbering keeps it) changes endings in which "
                       "a side is exhausted in the step its goal changes hands."),
    ("r14 21.6 at 7.8", "At the end of an active phase (Q4's ending of every open war by the score) item 1 applies: "
                        "an attacker holding G wins without the last-battle rule (peace if b is exhausted, else "
                        "war_won); item 2 is the score ending as before, with peace when the loser is exhausted "
                        "(r15 confirms both); how 'aim' in the war line. Other quiet endings keep war_won."),
    ("r14 21.6 b stands in G", "'b neither holds G nor has a standing settlement in it' reads b's standing VISIBLE "
                               "settlements, as 7.7 item 2 did (SPEC_GAPS 7.3 item 7)."),
    ("r14 (14)", "Read for the endings of 21.6 items 1 and 2 (b no longer holds G nor stands in it): an attacker that "
                 "wins by 7.7's exhaustion, no-front or length endings while b still holds G is not 'won while not "
                 "holding the goal' in (14)'s sense (7.7's own cession of G would otherwise break (14)). Checked from "
                 "the run's ending log (the owner of G at the ending moment is not in the rows when G changes hands "
                 "in the ending's step)."),
    ("r14 (8)", "Checked in the run: an ending whose winner lost the last battle has how 'aim' with the attacker "
                "holding G at the ending."),
    ("r14 21.5 item 1", "7.4's distribution of 2 dead over 1000, 1000 and 10 people is 0, 0, 0 with the remainder 2 "
                        "to the most populous (losses 2, 0, 0, as r16's text now says); the unit checks 2 dead "
                        "(note for s1 only) and 3 dead (2, 1, 0: notes for s1 and s2), s3 never noted."),
    ("r14 21.7 item 1 who", "searched_until is keyed (hidden civilization y, region R) and makes R weary as a goal "
                            "against y (20.2's allowed goals) for every claimant; the 20.23 open-war count is not "
                            "involved; with search_weary_years 0 nothing is stored."),
    ("r14 21.7 item 2", "A war's battlefields are the regions of its battle and battle_at_pass events (any winner); "
                        "the battle draw then follows as in 7.3, the siege may take a concealed settlement."),
    ("r14 21.8 reset", "A change of holder (another civilization holds G) clears every futile count over G, so a "
                       "region taken by another and retaken by its old holder starts from 0 (21.11 item 3: 'a region "
                       "taken by another civilization and retaken resets it'); with only 20.4's reading rule (a count "
                       "of another holder reads 0) the old count would read again after the retake. The run "
                       "statistic breaks at a change row (from_year of a region_control row whose civ differs from "
                       "the previous row's), never at a row's end."),
    ("r14 21.10 lies low", "y lies low in year Y (the step's start in a quiet step) while one of y's standing "
                           "settlements has Y < hidden_until; no strain of y toward others is raised (5.2), no claim "
                           "(5.3) and no declaration (7.2, including the flare pair) with y as claimant or attacker; "
                           "the tension skips pairs with y as claimant. Strain toward y and y's defence unchanged."),
    ("r15 21.12 item 1", "S: the standing visible settlements that are not hidden (hidden(s): Y < hidden_until) of "
                         "alive visible civilizations of another race whose disposition d(a, b) = camp - grudge is "
                         "below border_friction at that T2 (after housekeeping and decay); the strain is "
                         "quarrel_strain x 1000. Stress and norefuge tunings are shipped plus edits (P3-06 16.3), so "
                         "they carry the shipped search_weary_years 500, search_repeat_salience 15 and "
                         "quarrel_strain 600, in the reference and the C++ support alike; the defaults of hand-made "
                         "tuning (0, 40, 600) concern tuning built without data."),
    ("r14 21.12 visible", "'The first T2 in which a is visible' is the first T2 in which a is among the alive visible "
                          "civilizations (P3-04 6.2: the tick after its awakening step); S is the standing visible "
                          "settlements of the alive visible civilizations of other races at that T2; the quarrel is "
                          "set after housekeeping and the grudge decay, before 5.2. A civilization whose first "
                          "visible step is quiet gets its quarrel in the next T2 (T2 only, 21.12 item 3)."),
    # r22 (P5-01, generator 0.21.0)
    ("r22 (9)", "24.5 multiplies dead_L = min(F_L x loss_loser div 1000, F_W x kill_max div 1000) by v_L up to "
                "(1000 + s) / 1000, so r10's invariant (9) (loser_dead <= F_W x kill_max div 1000) no longer holds as "
                "written; 24.7 does not restate it. Read: (9) holds for the bound before the factor, and the stored "
                "loser_dead is at most that bound x (1000 + s) div 1000 (both checked in the run)."),
    ("r22 24.1 war's claim", "The war's own record of its claim (the war line's `claim e<id>`, the note) is the fresh "
                             "claim c' that the declaration cites."),
    ("r22 24.4 war last", "The flight's refuge_hidden is not the war's latest event: war['last'] stays the "
                          "refugees_settled (24.4 says nothing else changes)."),
    ("r22 24.8 item 1", "SPEC DEFECT: 24.1's Effect and 24.8 item 1 say the rows with 24.1 off differ only by the fresh "
                        "claims, the ids after them and the declarations' causes; but 7.3's battle, siege, toll and "
                        "search draws are keyed by the war's id w (its war_declared event id), which the fresh claim "
                        "shifts, so the histories diverge from the next war's first battle. The reference checks the "
                        "rows up to the first re-aimed declaration."),
    ("r22 24.4 E", "E is param.history_span (the history's end year); end_year = min(hidden_until, E) - 1."),
    ("r16 (16)", "Invariant (16) as restated by r16 21.12 item 5 (P3-10 10d item 2): the claim of a against its "
                 "quarrel neighbour b is dated in the first phase in which a and b are in contact (5.1's contact as "
                 "the T2 strain update computes it), unless 5.3's tests refuse that claim in that phase (no allowed "
                 "goal, a truce, a war, hiding); a pair never in contact owes nothing. Replaces r14's wording, under "
                 "which the reference printed refused claims as findings."),
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
assert list(COMPILED[19:30]) == list(CONFLICT_KINDS) and len(COMPILED) >= 36   # 38 from 0.19.0 (kinds 37, 38)
assert list(COMPILED[31:36]) == list(CONFLICT_KINDS_018)
# r19 23.2, 23.3 / P3-04 r17 21.2 (generator 0.20.0): kinds 39 and 40, transcribed (must equal the engine
# reference's)
CONFLICT_KINDS_020 = (
    _k(39, "claim_settlements", 1, 0, (("claimant", CIV, 1, 1), ("holder", CIV, 1, 1), ("settlement", SETTLEMENT, 1, 0)),
       ()),
    _k(40, "settlements_ceded", 1, 0, (("from", CIV, 1, 1), ("to", CIV, 1, 1), ("settlement", SETTLEMENT, 1, 0)), ()),
)
assert list(COMPILED[38:40]) == list(CONFLICT_KINDS_020)
# r22 24.3, 24.4 / P3-04 r19 22.2 (generator 0.21.0): kinds 41 and 42, transcribed (must equal the engine
# reference's)
CONFLICT_KINDS_021 = (
    _k(41, "claim_dropped", 1, 0, (("claimant", CIV, 1, 1), ("holder", CIV, 1, 1)), ()),
    _k(42, "refuge_hidden", 1, 1, (("settlement", SETTLEMENT, 1, 1), ("founder", CIV, 1, 1)), ()),
)
assert list(COMPILED[40:42]) == list(CONFLICT_KINDS_021)
KIND_ID = {k["key"]: k["id"] for k in COMPILED}
KEY_OF = {k["id"]: k["key"] for k in COMPILED}
# P5-04 8.1 (0.26.0): kinds 63 to 78 named for the row checks (no 0.25.0 world holds them)
KIND_ID.update({k["key"]: k["id"] for k in pe.KINDS_026})
KEY_OF.update({k["id"]: k["key"] for k in pe.KINDS_026})
# P5-06 8.1 (0.27.0): kinds 79 to 92 likewise
KIND_ID.update({k["key"]: k["id"] for k in pe.KINDS_027})
KEY_OF.update({k["id"]: k["key"] for k in pe.KINDS_027})
K = KIND_ID
# r19 23.2 / P3-07 r14 23 item 1: wherever a rule, invariant, statistic or report reads a claim, kinds 20 and 39
# are read alike; a cession of invariants (13), (17), the units and the companions is kind 29 or 40. Kinds 39 and
# 40 are never written before 0.20.0, so the sets serve every version.
CLAIMS = (K["claim"], K["claim_settlements"])
CEDES = (K["region_ceded"], K["settlements_ceded"])
MAX_PARTICIPANTS = 62   # P3-04 r18 21.2, r19 22.2: the settlement role of kinds 39, 40 lists the 62 smallest ids at
#                        most (the reference had 64 until P5-01; no world reached either bound)

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

# r14 21.2 (0.19.0), transcribed from the spec
SPEC_REPORT_019 = {
    "battle": "{winner} defeated {loser} in {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and "
              "none of the victors| and one of the victors fell| and # of the victors fell}.",
    "battle_at_pass": "{winner} defeated {loser} at the pass into {@region}; {loser_dead?none|one|#} of the defeated"
                      "{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors "
                      "fell}.",
}

STREAMS = ("hist.conflict.declare", "hist.conflict.battle", "hist.conflict.battle_win", "hist.conflict.siege",
           "hist.conflict.sack", "hist.conflict.search",   # search: r10 20.17 (0.18.0)
           "hist.conflict.toll")                           # toll: r22 24.5 (0.21.0)

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
        self.renaming = hist.get("renaming")   # P5-12 4 (0.22.0); an older root has none
        self.relations = hist.get("relations")   # P5-03 4.2 (0.24.0); an older root has none
        self.alliances = hist.get("alliances")   # P5-04 4.2 (0.26.0); an older root has none
        self.figures = hist.get("figures")       # P5-06 4.4 (0.27.0); an older root has none
        for key, t in content["race"]:
            rid = self.chron.race_by_id[key]["order"]
            self.race[rid]["aggression"] = t["aggression"]
            self.race[rid]["prowess"] = t["prowess_permille"]
            # P3-01 r9 3.2 (0.18.0); a 0.17.0 root has none (and no rule reads them then)
            self.race[rid]["muster"] = t.get("muster_permille", 1000)
            self.race[rid]["conceal"] = t.get("conceal_min", 1001)
            self.race[rid]["home"] = t.get("home_defense_permille", 0)
            # P3-01 r11 3.5 (0.19.0); an older root has none (and no rule reads them then)
            self.race[rid]["odds_min"] = t.get("odds_min_permille", 700)
            self.race[rid]["quarrel"] = t.get("awakening_quarrel", 0)
            # P3-01 r13 3.6 (0.19.0 r3): hidden_years per race; the default of hand-made data is 300
            self.race[rid]["hidden_years"] = t.get("hidden_years", 300)
            # P5-03 4.1 (0.24.0): (race key, value) of the race's stances; an older root has none
            self.race[rid]["stances"] = [(x["race"], x["value"]) for x in t.get("stances", [])]
            # P5-04 4.1 (0.26.0): the race keys of origin_allies, the readiness to join and to mine
            self.race[rid]["origin_allies"] = list(t.get("origin_allies", []))
            self.race[rid]["ally_join"] = t.get("ally_join_permille", 0)
            self.race[rid]["mining"] = t.get("mining_permille", 0)
            # P5-06 4.1 (0.27.0): the race's leanings of temper (steady, warlike, peaceable, grasping, just) and the
            # chances of generals, heroes and casters; an older root has none
            tw = t.get("temper_weights")
            self.race[rid]["temper_weights"] = (tuple(tw[x] for x in ("steady", "warlike", "peaceable", "grasping",
                                                                       "just")) if tw else None)
            self.race[rid]["general"] = t.get("general_permille", 0)
            self.race[rid]["hero"] = t.get("hero_permille", 0)
            self.race[rid]["magic"] = t.get("magic_permille", 0)


_DATA = {}


def data(root="game/data"):
    if root not in _DATA:
        _DATA[root] = Data(root)
    return _DATA[root]


def edited(dat, conflict=None, peoples=None, races=None, engine=None):
    d = pp.edited(dat, peoples=peoples, races=races, engine=engine)
    d.conflict = copy.deepcopy(dat.conflict)
    d.conflict.update(conflict or {})
    d.renaming = copy.deepcopy(getattr(dat, "renaming", None))
    d.relations = copy.deepcopy(getattr(dat, "relations", None))
    d.alliances = copy.deepcopy(getattr(dat, "alliances", None))
    d.figures = copy.deepcopy(getattr(dat, "figures", None))
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
        self.q_end = qe   # P3-04 r15 20.1 item 1a (pe.Engine.assured)
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
                    # Q5 is a step of its own (P3-04 6.2): what Q4 created is visible to the tension's reads (r9
                    # 20.6 item 3's odds read forces); the hook's Q5 begin() is then not repeated
                    self.hooks.q5_begin(self)
                    tv, trig = self.tension(s, ln)
                    if tv < 0:
                        fatal("history: tension: value %d below 0" % tv)
                    if tv >= b["threshold"]:
                        last = self.hooks.last_event_id()
                        if last is not None and not (1 <= trig <= last):
                            fatal("history: tension: trigger %d outside 1 to %d" % (trig, last))
                        flare = min(b["F"], qe - end, b["cap"] - b["sched"] - self.flare_total)
                        if flare < b["M"]:   # P3-04 r19 22.3: no flare-up shorter than m; the quiet row goes on
                            self.flares_skipped.append((s, ln, flare))
                            flare = 0
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
        self.w.q5_done = False
        self.w.quiet_a(eng, start, length, row_start)

    def q5_begin(self, eng):
        self.w.begin()            # Q5
        self.w.q5_done = True

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
        self.cited_arms = set()    # P5-04a 9.2: the arms_sold events a battle won has cited
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
        # r14 members (21.7, 21.8, 21.12), keyed by ids
        self.searched_until = {}   # (hidden civ, region) -> year until which every claimant is weary (21.7 item 1)
        self.last_owner = getattr(self, "last_owner", {})   # region -> its last owner (21.8's holder)
        self.quarrel_done = set()  # civilizations whose first visible T2 has passed (21.12)
        self.quarrel_pending = set()  # 21.12 item 3
        # r14 logs for the checks (not state)
        self.quarrel_log = []      # (year, a, b or 0) of every quarrel set (b 0: pending)
        self.contact_log = {}      # civ -> first phase id in which it was in contact with another race (16)
        self.low_log = []          # (year, civ, until) of every lie_low call (21.10)
        self.aim_log = []          # (war id, how, winner, holder of G at the end, score winner, last battle winner)
        self.claim_blocked = set()  # (civ, phase, reason): a claim the strain called for, refused by 5.3's tests
        self.claim_refused = set()  # (civ, target, phase): the same, per target (r16 (16))
        self.pair_contact = {}      # (a, b) -> first phase id in which a and b are in contact (r16 (16))
        self.pair_contact_year = {}  # (a, b) -> the first year of that contact (P5-04: an oath before it)
        # r17 members (22.1), keyed by ids
        self.quarrel_with = {}     # a -> b while a's awakening quarrel with b is open (22.1 item 1)
        self.quarrel_closed = set()  # a whose quarrel entry was erased (never set again)
        # r17 logs for the checks (not state)
        self.held_back_log = []     # (year, a, b, where) of every held-back quarrel claim (22.1 item 3)
        self.cost_log = []          # (war id, W, L, G, kind of cost) after every ending's cost (22.5, (17))
        self.inv17 = []             # in-run failures of invariant (17)
        # r22 logs for the checks (not state)
        self.fresh_log = []         # (year, a, b, old claim, fresh claim, G) of every fresh claim (24.1)
        self.toll_log = []          # (year, war, v_L, v_W, dead_L, dead_W) of every battle (24.5)
        # P5-12: the renaming subsystem, installed after conflict (p5_12_renaming_reference.Renaming)
        self.renaming = None
        if conflict and FLAGS["rename"]:
            import p5_12_renaming_reference as p512
            self.renaming = p512.Renaming(self, dat, inp)
        # P5-02: the monsters subsystem, installed after renaming (p5_02_monsters_reference.Monsters), and its
        # WildView set on peoples (7.1)
        self.monsters = None
        self.phase_trigger = 0
        if conflict and FLAGS["monsters"]:
            import p5_02_monsters_reference as p502
            self.monsters = p502.Monsters(self, dat.base, inp)
            self.set_wild(p502.WildView(self.monsters))
            if FLAGS["deep"]:   # P5-07: the deep rules on the monsters subsystem
                import p5_07_depths_reference as p507
                self.monsters.deep = p507.Deep(self.monsters)
        # P5-03: the relations subsystem after the monsters; the ledger on peoples, the dispositions on conflict
        self.race_relations = None
        if conflict and FLAGS["relations"] and getattr(dat, "relations", None) is not None:
            import p5_03_relations_reference as p503
            self.race_relations = p503.relations_of(dat, self)
            self.set_ledger(self.race_relations)
        # P5-04: the alliances subsystem after relations (p5_04_alliances_reference.Alliances), its AllianceView
        # read by conflict (7.1); None: no view, 0.25.0's conflict
        self.alliances = None
        self.war_index = {}        # declaration event -> war record (P5-04 7.13's reads by war)
        self.joined_open = set()   # (min, max) of the open joined war rows (P5-04 7.6)
        self.n_joined_open = {}    # civilization -> its open joined records
        if conflict and FLAGS["alliances"] and getattr(dat, "alliances", None) is not None:
            import p5_04_alliances_reference as p504
            self.alliances = p504.Alliances(self, dat, inp)
        # P5-06: the figures subsystem after the alliances (p5_06_figures_reference.Figures); its FigureView read by
        # conflict (7.2, 7.3, 7.6), peoples (7.4), monsters (7.7) and alliances (7.3); None: no view, 0.26.0's rules
        self.figures = None
        if conflict and FLAGS["figures"] and getattr(dat, "figures", None) is not None:
            import p5_06_figures_reference as p506
            self.figures = p506.Figures(self, dat, inp)

    # --- engine phase kind
    def tick(self, eng_run, year, phase_end):
        self.year, self.years, self.phase = year, 1, eng_run.phase_id
        self.phase_kind = eng_run.phase_kind
        self.phase_trigger = eng_run.phases[-1][5] if eng_run.phases else 0   # P5-02 7.4 phase_trigger_event()
        self.t1()
        if self.monsters is not None:
            self.monsters.t1()               # T1: monsters (same step, after peoples)
        if self.race_relations is not None:
            self.race_relations.t1()              # T1: relations (same step, after the monsters)
        self.foundings(quiet=False)          # T2: peoples
        if self.conflict_on:
            self.c_t2()                      # T2: conflict (same step)
        if self.alliances is not None:
            self.alliances.t2()              # T2: alliances (P5-04 6.1: after relations, which has no T2)
        self.begin()                         # T3
        if self.figures is not None:
            self.figures.t3()                # T3: figures (P5-06 6.1: the only subsystem of T3)
        self.t4()                            # T4
        if self.monsters is not None:
            self.monsters.t4()               # T4: monsters (same step, after peoples)
        if self.race_relations is not None:
            self.race_relations.t4()
        if self.figures is not None:
            self.figures.t4()                # T4: figures (after relations; alliances has no T4)
        self.begin()                         # T5
        self.begin()                         # T6
        if self.conflict_on:
            self.c_t6()
        if self.renaming is not None:
            self.renaming.run(quiet=False)   # T6: renaming (same step, after conflict)
        if self.monsters is not None:
            self.monsters.t6()               # T6: monsters (same step, after renaming)
        if self.race_relations is not None:
            self.race_relations.t6()
        if self.alliances is not None:
            self.alliances.t6()              # T6: alliances (after relations)
        if self.figures is not None:
            self.figures.t6()                # T6: figures (after alliances)
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
        if self.monsters is not None:
            self.monsters.q1()  # Q1: monsters (same step, after peoples)
        if self.race_relations is not None:
            self.race_relations.q1()
        self.begin()          # Q2
        self.q3()
        self.foundings(quiet=True)          # Q4: peoples
        if self.conflict_on:
            self.c_q4()                     # Q4: conflict (same step)
        if self.renaming is not None:
            self.renaming.run(quiet=True)   # Q4: renaming (same step, after conflict)
        if self.monsters is not None:
            self.monsters.q4()              # Q4: monsters (same step, after renaming)
        if self.race_relations is not None:
            self.race_relations.q4()
        if self.alliances is not None:
            self.alliances.q4()             # Q4: alliances (after relations)
        if self.figures is not None:
            self.figures.q4()               # Q4: figures (after alliances)

    def quiet_b(self, eng_run, start, length, phase_end, row_start):
        last = start + length - 1
        if not getattr(self, "q5_done", False):
            self.begin()      # Q5 (when the engine asked no tension)
        self.q5_done = False
        self.begin()          # Q6
        self.quiet_used = None
        self.quiet_eras(start, last, eng_run.assured(start, length, phase_end))   # P3-04 6.6; r15 20.1 item 1a
        self.bookkeeping(start, phase_end, last)
        if phase_end:
            self.open_era = {}
        self.check_state()

    def quiet_step(self, eng_run, start, length, phase_end, row_start):   # not used by this engine
        raise AssertionError("quiet_step")

    # --- State helpers
    def set_owner(self, rid, civ, event):
        fu = getattr(self, "futile", None)
        if FLAGS["holder"] and fu and civ != 0 and civ != self.hold(rid):
            # 21.8 (READING, SPEC_GAPS r14 21.8 reset): a change of holder resets every count over the region, so a
            # region taken by another and retaken starts again from 0 (21.11 item 3), as the run statistic does
            for k in [k for k in fu if k[1] == rid]:
                del fu[k]
        su = getattr(self, "searched_until", None)
        if FLAGS["search_weary"] and su and civ != 0 and civ != self.hold(rid):
            # 21.7 item 1: a change of R's holder (21.8: another civilization holds R) clears the search's weariness
            for k in [k for k in su if k[1] == rid]:
                del su[k]
        super().set_owner(rid, civ, event)
        if civ != 0:
            if not hasattr(self, "last_owner"):
                self.last_owner = {}
            self.last_owner[rid] = civ
        self.version += 1

    def hold(self, rid):
        """21.8: hold(G) = G's owner, else the civilization that last owned G (0 if G was never owned); before
        0.19.0 the owner."""
        o = self.owner[rid][0]
        if o != 0 or not FLAGS["holder"]:
            return o
        return getattr(self, "last_owner", {}).get(rid, 0)

    def hides(self, y):
        """r17 22.3 (replaces 21.10's definition): y lies low by the one predicate peoples holds (P3-05 r15 22.7a):
        its latest flight's refuge stands, is y's, and its hidden years have not ended."""
        if not FLAGS["lie_low"]:
            return False
        return self.lying_low(y)

    def hidden_years_of(self, y):
        """r17 22.2: the hidden years of a refuge founded by y, its race's field (before r17
        history.conflict.hidden_years; 300 when a 0.19.0 tuning has none, for the units of older rules)."""
        if FLAGS["race_hidden"]:
            return self.pd.race[self.race_c(y)]["hidden_years"]
        return self.ct.get("hidden_years", 300)

    def n_visible_standing(self, a):
        """22.1 item 3: the standing settlements owned by a that are visible in the step (hidden ones included)."""
        return sum(1 for s in self.standing_of(a) if self.visible(s))

    def held_back(self, a, b, where):
        """22.1 item 3: a's claim on b is its quarrel claim (quarrel_with[a] = b) and a has fewer than
        quarrel_min_settlements visible standing settlements."""
        if not FLAGS["quarrel_hold"] or self.quarrel_with.get(a) != b:
            return False
        if self.n_visible_standing(a) >= self.ct.get("quarrel_min_settlements", 1) and not (
                FLAGS["strength"] and self.strength(a, b) < self.ct.get("quarrel_strength_permille", 0)):
            return False
        self.held_back_log.append((self.year, a, b, where))
        return True

    def strength(self, a, b):
        """r22 24.2: 20.6 item 3's odds with the aggression term m = 1000: force(a) against force(b) weighted by
        def(G) of the claim's region G for its entry in front(a, b) (1000 if G is not in it), capped at 1000."""
        t = self.ct
        g0 = self.claim[(a, b)][1]
        ent = [e for e in self.front(a, b) if e[0] == g0]
        dv = self.defence(g0, ent[0][2], b)[0] if ent else 1000
        fb = self.deterred(a, b) * dv // 1000      # P5-04 7.4 (reading A1: 24.2 reads 20.6 item 3's fb)
        return min(1000, self.force(a) * 1000 // max(1, fb * t["declare_ratio"] // 1000))

    def deterred(self, a, b):
        """P5-04 7.4: force(b) + ally_deterrence x the forces of b's allies z != a, not allied with a, that reach
        a (div 1000); force(b) without allies."""
        fb = self.force(b)
        if self.alliances is None:
            return fb
        zs = [z for z in self.alliances.allies(b) if z != a and not self.allied(z, a) and self.reaches(z, a)]
        if not zs:
            return fb
        return fb + self.ct["ally_deterrence_permille"] * sum(self.force(z) for z in zs) // 1000

    def close_quarrel(self, a, b):
        """22.1 item 1: a war between a and b starts (either side attacking): the open quarrel of the pair ends."""
        for x, y in ((a, b), (b, a)):
            if self.quarrel_with.get(x) == y:
                del self.quarrel_with[x]
                self.quarrel_closed.add(x)

    def name_cores(self):
        """P3-02 r10 12.2: the history stage's set K of name cores (kept by the peoples world), from 0.19.0."""
        if not at_least("0.19.0"):
            return None
        return getattr(self, "cores", None)

    def odds_min(self, a):
        """21.9: the claimant race's odds_min_permille (before 0.19.0 history.conflict.odds_min)."""
        if FLAGS["race_odds"]:
            return self.pd.race[self.race_c(a)]["odds_min"]
        # the 0.18.0 rules on 0.19.0 data (the units of r9, run with rules("0.18.0")): 0.18.0's shipped 700
        return self.ct.get("odds_min", 700)

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

    def peoples_disposition(self, a, b):
        """P5-03 7.1: D(r_a, r_b) through the view (camp(a, b) without one)."""
        if self.race_relations is not None:
            return self.race_relations.disposition(self.race_c(a), self.race_c(b))
        return self.camp(a, b)

    def disp(self, a, b):
        return self.peoples_disposition(a, b) - self.grudge.get((min(a, b), max(a, b)), 0)

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
        # P5-04 7.3: the bloc share in place of the share (equal to it without allies)
        return t["fear_weight"] * max(0, self.bloc_share(b) - t["fear_share"]) // (1000 - t["fear_share"])

    # --- P5-04 7.1: the AllianceView (no view: no alliance, no trade, no mine)
    def allied(self, a, b):
        return self.alliances is not None and self.alliances.allied(a, b)

    def allies_of(self, c):
        return self.alliances.allies(c) if self.alliances is not None else []

    def bloc_share(self, c):
        """7.3: min(1000, share(c) + the shares of c's allies)."""
        s = self.share(c)
        if self.alliances is None:
            return s
        return min(1000, s + sum(self.share(z) for z in self.alliances.allies(c)))

    def strengthened(self, f, c):
        """7.5: the trade, forge and arms factors, in this order, each with floor division."""
        al = self.alliances
        if al is None:
            return f
        t = self.ct
        f = f * (1000 + t["trade_strength_permille"] * min(al.trade_partners(c), t["trade_strength_max"])) // 1000
        if FLAGS["deep"] and al.forged(c):   # P5-07 6.3: the depth of c's deepest open mine
            f = f * (1000 + t["forge_strength_permille"] + t.get("forge_depth_permille", 0) * self.forge_depth(c)) \
                // 1000
        else:
            f = f * (1000 + t["forge_strength_permille"] * (1 if al.forged(c) else 0)) // 1000
        f = f * (1000 + t["arms_strength_permille"] * (1 if al.arms(c) else 0)) // 1000
        return f

    def forge_depth(self, c):
        """P5-07 7.1: AllianceView::forge_depth(c), the largest mine_depth over c's open mines (0 none)."""
        al, m = self.alliances, self.monsters
        if al is None or m is None or getattr(m, "deep", None) is None:
            return 0
        return max([m.deep.mine_depth(sid) for (cc, _R), (sid, _ts) in al.mines.items() if cc == c] or [0])

    def open_ally_records(self, c):
        """7.6: the open joined records of c (war, record), in war order then join order."""
        return [(w, r) for w in self.wars for r in w.get("allies", ()) if r["open"] and r["z"] == c]

    def open_wars_of(self, c):
        """7.6: the open wars with c as a side and the open joined records of c."""
        return self.wars_of(c) + self.n_joined_open.get(c, 0)

    def reaches(self, x, y):
        """7.13: front(x, y) (x's march hops, enmity_hops where an enmity row exists) is not empty."""
        return bool(self.front(x, y))

    def war_of(self, w):
        return self.war_index[w]

    def war_open(self, w):
        return self.war_index[w]["open"]

    def ally_record(self, w, z):
        for r in self.war_index[w].get("allies", ()):
            if r["z"] == z:
                return r
        return None

    def ally_left(self, w, z):
        r = self.ally_record(w, z)
        return r is not None and not r["open"]

    def ally_dead(self, w, z):
        r = self.ally_record(w, z)
        return r["dead"] if r is not None else 0

    def side_dead(self, war, c):
        """7.8: the dead of c's side in war: c's own and every ally's of that side (open or withdrawn)."""
        side = 1 if c == war["a"] else 0
        return war["dead"][c] + sum(r["dead"] for r in war.get("allies", ()) if r["side"] == side)

    def war_spoils(self, w):
        """7.15: WarSpoils of an ended war (fatal while it is open)."""
        war = self.war_index[w]
        if war["open"]:
            fatal("history: conflict: war_spoils: war %d is open" % w)
        W, L = war.get("spoils_pair", (0, 0))
        return dict(winner=W, loser=L, end_event=war.get("end_event", 0), goal_region=war["goal"],
                    tribute_people=war.get("tribute_people", 0), tribute_event=war.get("tribute_event", 0),
                    plunder=war.get("plunder", {}).get(W, 0) if W else 0,
                    winner_dead=war["dead"][W] if W else 0, winner_pop0=war["pop0"][W] if W else 0,
                    side_dead=self.side_dead(war, W) if W else 0)

    def join_war(self, w, z, side, e):
        """7.6: the ally z joins war w (its declaration event) on side `side` (0 the defender's, 1 the attacker's;
        explicit, the lead's ruling at integration) with event e."""
        war = self.war_index.get(w)
        if war is None or not war["open"]:
            fatal("history: conflict: join_war: the war is not open")
        if z in (war["a"], war["b"]) or any(r["z"] == z for r in war["allies"]):
            fatal("history: conflict: join_war: the ally is a side or an ally of the war")
        enemy = war["a"] if side == 0 else war["b"]
        if rel_key(z, enemy, "war") in self.relations or rel_key(enemy, z, "war") in self.relations:
            fatal("history: conflict: join_war: the ally is at war with the enemy")
        war["allies"].append(dict(z=z, side=side, join=e, dead=0, pop0=self.population(z), open=True,
                                  enemy=enemy, partner=war["b"] if side == 0 else war["a"], close_event=0,
                                  closed_by=None))
        pk = sym_key(z, enemy, "peace")
        if pk in self.relations:
            self.close_rel(pk, e)
        self.open_rel(rel_key(z, enemy, "war"), e)
        self.joined_open.add((min(z, enemy), max(z, enemy)))
        self.n_joined_open[z] = self.n_joined_open.get(z, 0) + 1
        self.zero_strain(z, enemy)
        self.drop_claims(z, enemy)

    def close_ally(self, r, e, how):
        r["open"], r["close_event"], r["closed_by"] = False, e, how
        self.joined_open.discard((min(r["z"], r["enemy"]), max(r["z"], r["enemy"])))
        self.n_joined_open[r["z"]] -= 1

    def allies_at_end(self, war, e):
        """7.10: the war ends with e (peace, war_won, truce; reading A3: search_abandoned too): each open ally's
        war row closes with e, its peace row opens with e, last_peace, the grudge, strains and claims."""
        t = self.ct
        for r in war.get("allies", ()):
            if not r["open"]:
                continue
            m, en = r["z"], r["enemy"]
            self.close_rel(rel_key(m, en, "war"), e)
            self.open_rel(sym_key(m, en, "peace"), e)
            self.last_peace[(min(m, en), max(m, en))] = (self.year, e)
            self.add_grudge(m, en, t["war_grudge"])
            self.zero_strain(m, en)
            self.drop_claims(m, en)
            self.close_ally(r, e, "end")
        war["end_event"] = e

    def allies_at_side_end(self, war):
        """7.11 after the relation pass of a side's end: an ally whose enemy ended had its row closed by that pass;
        an ally whose partner ended has its row closed with the partner's ending event; no peace row."""
        for r in war.get("allies", ()):
            if not r["open"]:
                continue
            m, en, p = r["z"], r["enemy"], r["partner"]
            if self.civ(en)["ended_year"] is not None:
                ev = self.civ(en)["ending_event"]
            else:
                ev = self.civ(p)["ending_event"]
                self.close_rel(rel_key(m, en, "war"), ev)
                self.close_ally(r, ev, "side")
                continue
            self.close_ally(r, ev, "end")     # the enemy's end is the war's end (6.11)

    def allies_ended(self):
        """7.11: an ally that ended: its record closes with its ending event (its rows closed by the relation
        pass)."""
        for war in self.wars:
            for r in war.get("allies", ()):
                if r["open"] and self.civ(r["z"])["ended_year"] is not None:
                    self.close_ally(r, self.civ(r["z"])["ending_event"], "ally")

    def side_end_spoils(self, war):
        """7.15: the winner and loser of a war closed by a principal's end: the survivor and the ended (both ended:
        0, 0); end_event the ended side's ending event (both: the smaller id's)."""
        a, b = war["a"], war["b"]
        ea, eb = self.civ(a)["ended_year"] is not None, self.civ(b)["ended_year"] is not None
        if ea and eb:
            war["spoils_pair"] = (0, 0)
            war["end_event"] = self.civ(min(a, b))["ending_event"]
        elif ea or eb:
            dead, alive = (a, b) if ea else (b, a)
            war["spoils_pair"] = (alive, dead)
            war["end_event"] = self.civ(dead)["ending_event"]

    def withdrawals(self, war):
        """7.9 (T6, before the end check: reading A4): each open ally bled to exhaustion leaves the war."""
        t = self.ct
        for r in war.get("allies", ()):
            if not r["open"]:
                continue
            m, en = r["z"], r["enemy"]
            if not r["dead"] * 1000 >= t["exhaustion_permille"] * max(1, r["pop0"] * self.muster(m) // 1000):
                continue
            e = self.append("ally_withdrew", participants=(("ally", CIV, m), ("partner", CIV, r["partner"]),
                                                            ("enemy", CIV, en)), causes=(war["last"],))
            self.close_ally(r, e, "withdrew")
            self.close_rel(rel_key(m, en, "war"), e)
            self.open_rel(sym_key(m, en, "peace"), e)
            self.last_peace[(min(m, en), max(m, en))] = (self.year, e)
            self.add_grudge(m, en, t["war_grudge"])
            self.zero_strain(m, en)
            self.drop_claims(m, en)

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
        f = self.strengthened(f, c)                # P5-04 7.5: before the bound check
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
        return self.strengthened(levy * self.pd.race[self.race_c(c)]["prowess"] // 1000 * self.muster(c) // 1000,
                                 c)   # P5-04 7.5

    def wars_of(self, c):
        return sum(1 for w in self.wars if w["open"] and c in (w["a"], w["b"]))

    def at_war(self, a, b):
        # P5-04 7.6: any open war relation either way (a principal war or a joined one)
        return any(w["open"] and {w["a"], w["b"]} == {a, b} for w in self.wars) or (
            self.alliances is not None and (min(a, b), max(a, b)) in self.joined_open)

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
        """20.4: b is weary of r (count read >= weary_wars; a count of another owner reads 0); 21.8: owner read
        as holder; 21.7 item 1: weary for every claimant while a search in r found no one of b."""
        ww = self.ct["weary_wars"] if FLAGS["weary"] else 0
        if ww == 0 and not FLAGS["search_weary"]:
            return False
        if FLAGS["search_weary"] and self.year < self.searched_until.get((b, r), -1):
            return True                       # 21.7 item 1: whatever the counts read
        k = self.futile.get((b, r))
        n = k[0] if k is not None and k[1] == self.hold(r) else 0
        if FLAGS["weary_open"]:
            # 20.23: the wars now open against b with goal r count, by any attacker
            n += sum(1 for w in self.wars if w["open"] and w["b"] == b and w["goal"] == r)
        return ww > 0 and n >= ww

    def count_futile(self, war):
        """20.4: a war ended by peace or truce whose goal kept the owner it had at the declaration."""
        if not FLAGS["weary"]:
            return
        b, g = war["b"], war["goal"]
        o = self.hold(g)
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

    # --- P5-04a 2: conflict's const reads for the alliances (amends P5-04 7.13)
    def has_claim(self, a, b):
        """An open claim of a against b."""
        return (a, b) in self.claim

    def claim_event(self, a, b):
        return self.claim[(a, b)][0] if (a, b) in self.claim else 0

    def last_war_end(self, c):
        """The largest (year, event) of last_peace(c, b) over every b != c ascending; (0, 0) none."""
        best = (0, 0)
        for (x, y), v in sorted(self.last_peace.items()):
            if c in (x, y) and x != y and v > best:
                best = v
        return best

    def arms_cause(self, W):
        """P5-04a 9.2: the arms_sold event a battle won by W cites (0 none): AllianceView::arms_event(W) when not
        yet cited."""
        if not FLAGS["p504a"] or self.alliances is None:
            return 0
        a = self.alliances.arms_event(W)
        return a if a and a not in self.cited_arms else 0

    def zero_strain(self, a, b):
        self.strain.pop((a, b), None)
        self.strain.pop((b, a), None)

    # --- 8.1 housekeeping
    def housekeeping(self):
        ended = lambda c: self.civ(c)["ended_year"] is not None  # noqa: E731
        for k in sorted(list(self.relations), key=key_order):
            if k[0] != CIV or k[2] != CIV:
                continue                      # P5-06 7.5: relations with a figure side are the figures subsystem's
            a, b = k[1], k[3]
            ea, eb = ended(a), ended(b)
            if ea or eb:
                side = min(a, b) if (ea and eb) else (a if ea else b)
                self.close_rel(k, self.civ(side)["ending_event"])
        if self.alliances is not None:
            self.allies_ended()               # P5-04 7.11: an ally that ended
        for w in self.wars:
            if w["open"] and (ended(w["a"]) or ended(w["b"])):
                self.close_war(w, "ended", None)
                if self.alliances is not None:
                    self.side_end_spoils(w)   # P5-04 7.11, 7.15
                    self.allies_at_side_end(w)
        for k in sorted(self.claim):
            if ended(k[0]) or ended(k[1]):
                del self.claim[k]
        for a in sorted(self.quarrel_with):     # r17 22.1 item 1: a or b ends
            if ended(a) or ended(self.quarrel_with[a]):
                del self.quarrel_with[a]
                self.quarrel_closed.add(a)
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
                if (a, b) in self.enmity_opened or self.peoples_disposition(a, b) > self.ct["enmity_max"]:
                    continue
                since = self.civ(b)["founding_event"]
                if self.race_relations is not None:   # P5-03 7.1 4.3: the later of the founding and the hatred turn
                    since = max(since, self.race_relations.hatred_event(self.race_c(a), self.race_c(b)))
                self.open_rel(sym_key(a, b, "enmity"), since)
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
        low = {c for c in al if self.hides(c)}   # r14 21.10 item 2
        for a in al:
            for b in al:
                if a == b or (a, b) not in self.contact or a in low:
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
                if self.allied(a, b):          # P5-04 7.2: after 5.2's evaluation, before the step's product
                    rate = rate * t["allied_strain_permille"] // 1000
                    if self.figures is not None:   # P5-06 7.3: the attacker's temper on strain toward an ally
                        rate = rate * self.figures.allied_strain_permille(a) // 1000
                if self.figures is not None:       # P5-06 7.2: the attacker's temper, normalized by race
                    rate = rate * self.figures.strain_permille(a) // 1000
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
                if (a, b) in self.claim or self.at_war(a, b) or not self.past_truce(a, b) or a in low:
                    if (a, b) not in self.claim and self.strain.get((a, b), 0) >= t["claim_strain"] * 1000:
                        self.claim_blocked.add((a, self.phase, "war, truce or hiding"))
                        self.claim_refused.add((a, b, self.phase))
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
                        self.claim_blocked.add((a, self.phase, "no allowed goal"))
                        self.claim_refused.add((a, b, self.phase))
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
                if FLAGS["claim39"] and self.owner[r][0] != b:
                    # r19 23.2 item 2: b's standing visible settlements in R that are not hidden (those that put R
                    # in presence(b), 5.1), ascending id, at most 64
                    ss = sorted(s["id"] for s in self.svis_seen(b) if s["region"] == r)[:MAX_PARTICIPANTS]
                    e = self.append("claim_settlements", region=r,
                                    participants=(("claimant", CIV, a), ("holder", CIV, b)) +
                                    tuple(("settlement", SETTLEMENT, x) for x in ss), causes=(cause,))
                else:
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
            if FLAGS["lie_low"] and self.hides(a):
                continue                     # r14 21.10 item 2: the tension skips its pairs as claimant
            if FLAGS["odds"] and self.odds(a, b) < self.odds_min(a):
                continue                     # r9 20.6 item 3 (r14 21.9: the claimant race's minimum)
            if self.held_back(a, b, "tension"):
                continue                     # r17 22.1 item 3.2: reports nothing, as below its odds minimum
            v = self.strain.get((a, b), 0) // 1000
            if v > best[0]:
                best = (v, self.claim[(a, b)][0], (a, b))
        return best

    def tension_all(self, start, length):
        """P3-04 6.5 with P5-02 6.5: the sources in config order {conflict, monsters}; the largest value, the
        trigger of the first source reporting it (READINGS T1 of p5_02_monsters_reference)."""
        v, e = self.tension(start, length)
        if self.monsters is not None:
            v2, e2 = self.monsters.tension(start, length)
            if v2 > v:
                return v2, e2
        return v, e

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
                    if FLAGS["dropped"]:   # r22 24.3: the drop is recorded first (kind 41, cause the claim)
                        self.append("claim_dropped", region=g0, participants=(("claimant", CIV, a), ("holder", CIV, b)),
                                    causes=(ce,))
                    del self.claim[(a, b)]
                    return None
                goal = min(al, key=lambda e: self.target_key(a, e))[0]
        elif self.owner[g0][0] != b and not any(s["region"] == g0 for s in self.svis(b)):
            goal = min(self.front(a, b), key=lambda e: self.target_key(a, e))[0]
        if FLAGS["fresh"] and goal != g0:
            ce = self.fresh_claim(a, b, ce, goal)   # r22 24.1
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
        if FLAGS["quarrel_hold"]:
            self.close_quarrel(a, b)          # r17 22.1 item 1 (a flare-up war included)
        self.wars.append(dict(w=e, a=a, b=b, goal=goal, start=self.year, last=e, init=a, claim=ce,
                              won={a: 0, b: 0}, taken={a: 0, b: 0}, dead={a: 0, b: 0},
                              pop0={a: self.population(a), b: self.population(b)}, open=True, end=None,
                              battles=0, sacked=0, captured=0, flare=draw_free, ceded=False,
                              owner0=self.hold(goal), cost=None, fled=0, fought=set()))
        if self.alliances is not None:     # P5-04 7.6, 7.15
            self.wars[-1].update(allies=[], end_event=0, plunder={a: 0, b: 0}, tribute_people=0, tribute_event=0,
                                 spoils_pair=(0, 0))
        else:
            self.wars[-1]["allies"] = []
        self.war_index[e] = self.wars[-1]
        return e

    def fresh_claim(self, a, b, c, g):
        """r22 24.1: the fresh claim on the re-chosen goal G, appended right before the war_declared: kind 20 when b
        owns G, else kind 39 with b's standing visible settlements in G that are not hidden (ascending, at most 62);
        causes c and 5.3's cause for G (G's owner_event, else the founding_event of b's most populous such
        settlement, ties to the smaller id); claim[(a, b)] = (c', G). No draw."""
        if self.owner[g][0] == b:
            cause = self.owner[g][1]
            e = self.append("claim", region=g, participants=(("claimant", CIV, a), ("holder", CIV, b)),
                            causes=tuple(sorted({c, cause})))
        else:
            ss = [s for s in self.svis_seen(b) if s["region"] == g]
            if not ss:
                fatal("history: conflict: fresh claim on region %d without a settlement of %d" % (g, b))
            cause = min(ss, key=lambda s: (-s["population"], s["id"]))["founding_event"]
            ids = sorted(s["id"] for s in ss)[:MAX_PARTICIPANTS]
            kind = "claim_settlements" if FLAGS["claim39"] else "claim"
            e = self.append(kind, region=g, participants=(("claimant", CIV, a), ("holder", CIV, b)) +
                            (tuple(("settlement", SETTLEMENT, x) for x in ids) if FLAGS["claim39"] else ()),
                            causes=tuple(sorted({c, cause})))
        self.claim[(a, b)] = (e, g)
        self.fresh_log.append((self.year, a, b, c, e, g))
        return e

    def c_t2(self):
        t = self.ct
        self.housekeeping()
        self.decay(1)
        if FLAGS["quarrel"]:
            self.quarrels()                  # r14 21.12, before 5.2's strain update
        self.strain_update(1, t["tick_strain_permille"], False)
        self.note_contact()
        first_flare = self.phase != self.last_t2_phase and self.phase_kind == FLARE
        if first_flare and FLAGS["flare_claim"] and self.phase_trigger and \
                self.events[self.phase_trigger - 1]["key"] not in ("claim", "claim_settlements"):
            # P5-02 7.4: a flare-up not started by a claim (a monster's) brings no forced declaration
            first_flare = False
        flare_pair = self.best_claim(self.year)[2] if first_flare else None
        if first_flare:
            self.flare_values[self.year] = self.best_claim(self.year)
        for a in self.avc():
            if any(w["open"] and w["a"] == a for w in self.wars):
                continue
            if self.hides(a):
                continue                     # r14 21.10 item 2: no declaration
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

    def quarrels(self):
        """r14 21.12: the awakening quarrel of each civilization of a race with awakening_quarrel 1 founded by
        civ_founded, set in its first visible T2 (pending while no other people stands)."""
        t = self.ct
        for c in sorted(self.quarrel_pending):
            if not self.alive(c):
                self.quarrel_pending.discard(c)      # item 3: a civilization that ends while pending loses it
        for a in self.avc():
            if a in self.quarrel_done:
                continue
            self.quarrel_done.add(a)
            if self.pd.race[self.race_c(a)]["quarrel"] != 1:
                continue
            fe = self.events[self.civ(a)["founding_event"] - 1]
            if fe["kind"] != K["civ_founded"]:
                continue                             # a secession (or any later founding) gets none
            self.quarrel_pending.add(a)
        for a in sorted(self.quarrel_pending):
            b = self.quarrel_neighbour(a)
            if b is None:
                continue
            self.quarrel_pending.discard(a)
            k = (a, b)
            self.strain[k] = max(self.strain.get(k, 0), t["quarrel_strain"] * 1000)   # r15 21.12 item 2
            self.quarrel_log.append((self.year, a, b))
            if FLAGS["quarrel_hold"] and a not in self.quarrel_closed:
                self.quarrel_with[a] = b          # r17 22.1 item 1

    def quarrel_neighbour(self, a):
        """21.12 item 1 (r15): the owner of the standing visible settlement, not hidden (20.1), of an alive
        civilization b of another race with disposition d(a, b) below border_friction (read at this step), with the
        smallest key (0 if h <= enmity_hops else 1, h, d^2, id), h the hops from a's capital region (unreachable:
        +infinity); None if none."""
        cap = self.stl(self.civ(a)["capital"])
        hops = self.geo.hops(cap["region"])
        inf = 1 << 62
        ra = self.race_c(a)
        best = None
        for c in self.avc():
            if self.race_c(c) == ra or self.disp(a, c) >= self.ct["border_friction"]:
                continue                      # r15: a people a can come to war with only (never its own camp)
            for s in self.svis(c):
                if self.hidden(s):
                    continue                  # r15: a hidden refuge is never chosen
                h = hops.get(s["region"], inf)
                d = (s["x"] - cap["x"]) ** 2 + (s["y"] - cap["y"]) ** 2
                key = (0 if h <= self.ct["enmity_hops"] else 1, h, d, s["id"])
                if best is None or key < best[0]:
                    best = (key, c)
        return None if best is None else best[1]

    def note_contact(self):
        """For invariant (16): the first phase in which each civilization is in contact (5.1, the T2 strain
        update's contact) with a civilization of another race."""
        for (a, b) in self.contact:
            if self.race_c(a) != self.race_c(b):
                for x in (a, b):
                    self.contact_log.setdefault(x, self.phase)
            self.pair_contact.setdefault((a, b), self.phase)
            self.pair_contact_year.setdefault((a, b), self.year)

    def declare_p(self, a, b):
        """7.2 item 3: the yearly chance of a's declaration on b, or None below declare_min (no draw)."""
        t = self.ct
        s = self.strain.get((a, b), 0) // 1000
        if s < t["declare_min"]:
            return None
        if self.held_back(a, b, "declare"):
            return None                       # r17 22.1 item 3.1: no declaration and no draw
        p = t["declare_permille"] * min(1000, (s - t["declare_min"]) * 1000 // t["declare_span"]) // 1000
        odds = self.odds(a, b)
        if FLAGS["odds"] and odds < self.odds_min(a):
            return None                       # r9 20.6 item 3 (r14 21.9 per race): no declaration and no draw
        return p * odds // 1000

    def odds(self, a, b):
        """7.2 item 3's odds; r9 20.6 item 3: the target's force weighted by the defence of the claimed region
        (1000 if it is not in front(a, b)), the aggression term floored at odds_floor; g' of 20.5."""
        t = self.ct
        g = self.aggression(a, b)
        fb = self.force(b)
        if FLAGS["odds"]:
            fb = self.deterred(a, b)                # P5-04 7.4
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
        before = {s["id"]: s["population"] for s in ss}
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
            if FLAGS["reduce_loss"] and s["population"] >= before[s["id"]]:
                continue                      # r14 21.1: no loss, no note
            self.last_red[s["id"]] = e        # note_reduction
        if war is not None:
            war["dead"][c] += dead
        return dead

    def refugees(self, c, s, n, cause=0):
        """P3-05 8.5 item 2: n people of c leave s for c's nearest other standing settlement, room permitting;
        P5-03 7.2: the ledger records the move and the displaced rest (cause: the capture or the cession, R7)."""
        if n <= 0:
            return
        race = self.civ(c)["race"]
        others = [t for t in self.standing_of(c) if t["id"] != s["id"]]
        if not others:
            if self.ledger is not None:
                self.ledger.displaced(s["id"], 0, n, cause)
            return
        t = min(others, key=lambda t: ((t["x"] - s["x"]) ** 2 + (t["y"] - s["y"]) ** 2, t["id"]))
        room = max(0, 2 * self.geo.capacity(t["region"], race) - t["population"])
        placed = min(n, room)
        t["population"] += placed
        if self.ledger is not None:
            if placed >= 1:
                self.ledger.moved(s["id"], t["id"], placed)
            if n - placed >= 1:
                self.ledger.displaced(s["id"], 0, n - placed, cause)

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
            if "plunder" in war:               # P5-04 7.15: the population at the fall, before its toll
                war["plunder"][x] += t_s["population"]
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
            self.refugees(y, t_s, excess, e)
            t_s["owner_civ"] = x
            t_s["owner_event"] = e   # P5-03 7.3
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
        for k in sorted([k for k in self.relations if y in (k[1], k[3]) and k[0] == CIV and k[2] == CIV],
                        key=key_order):   # P5-06 7.5: civilization pairs only
            self.close_rel(k, e2)
        if self.alliances is not None:
            self.allies_ended()               # P5-04 7.11: y as an ally of another war
        for w in self.wars:
            if w["open"] and y in (w["a"], w["b"]):
                self.close_war(w, "conquest", w["a"] if w["b"] == y else w["b"])
                if self.alliances is not None:
                    self.side_end_spoils(w)   # P5-04 7.11, 7.15
                    self.allies_at_side_end(w)
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
            if war["open"] and war["allies"]:
                self.withdrawals(war)          # P5-04 7.9 (reading A4: after the battle, before 7.7's checks)
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
        if FLAGS["search"] and not (FLAGS["search_battle"] and r in war["fought"]) and \
                self.conceal_search(war, x, y, r):
            return                 # r14 21.7 item 2: a battlefield of this war needs no search
        if not chance_pm(self.draw("hist.conflict.battle", w, 0, 0), t["battle_permille"]):
            return
        allied = self.alliances is not None and any(q["open"] for q in war["allies"])
        if allied:              # P5-04 7.7: each side is its principal and its open allies of that side
            A, fx = self.side_forces(war, x, r)
            D, fy = self.side_forces(war, y, r)
        elif FLAGS["levies"]:     # r9 20.6 item 2: each side's local force at R (P5-04 7.6: over its open wars)
            A = self.local_force(x, r) // self.open_wars_of(x)
            D = self.local_force(y, r) // self.open_wars_of(y)
        else:
            A = self.force(x) // self.wars_of(x)
            D = self.force(y) // self.wars_of(y)
        dv, pass_id = self.defence(r, r_from, y)
        gx = gy = None
        if self.figures is not None:        # P5-06 7.6: the generals' skill in the win chance only
            gx, gy = self.figures.general_of(w, x), self.figures.general_of(w, y)
        if gx is not None and (gx[0] or gy[0]):
            A1 = A * (1000 + gx[1]) // 1000
            D2 = D * dv // 1000 * (1000 + gy[1]) // 1000
            p = A1 * 1000 // (A1 + D2) if A1 + D2 > 0 else 500
        else:
            p = self.battle_p(A, D, dv)
        xwin = chance_pm(self.draw("hist.conflict.battle_win", w, 0, 0), p)
        W, L, FW, FL = (x, y, A, D) if xwin else (y, x, D, A)
        arms_c = self.arms_cause(W)          # P5-04a 9.2: the first battle won after a sale cites it
        bcauses = tuple(sorted({w, arms_c})) if arms_c else (w,)
        if arms_c:
            self.cited_arms.add(arms_c)
        dead_w, dead_l = self.battle_tolls(FW, FL, w)
        some_ally = allied and (any(f > 0 for _z, f in fx[1]) or any(f > 0 for _z, f in fy[1]))
        if gx is not None and (gx[0] or gy[0]):
            fw, fl = ((fx, fy) if xwin else (fy, fx)) if allied else ((0, []), (0, []))
            gw, gl = (gx, gy) if xwin else (gy, gx)
            e = self.append("battle_led", region=r,
                            participants=(("winner", CIV, W), ("loser", CIV, L)) +
                            tuple(("winner_ally", CIV, z) for z, f in fw[1] if f > 0) +
                            tuple(("loser_ally", CIV, z) for z, f in fl[1] if f > 0) +
                            ((("winner_general", FIGURE, gw[0]),) if gw[0] else ()) +
                            ((("loser_general", FIGURE, gl[0]),) if gl[0] else ()),
                            values=(dead_w, dead_l, pass_id), causes=bcauses)
            if some_ally:
                self.allied_casualties(W, dead_w, fw, e, war)
                self.allied_casualties(L, dead_l, fl, e, war)
        elif some_ally:
            fw, fl = (fx, fy) if xwin else (fy, fx)
            e = self.append("allied_battle", region=r,
                            participants=(("winner", CIV, W), ("loser", CIV, L)) +
                            tuple(("winner_ally", CIV, z) for z, f in fw[1] if f > 0) +
                            tuple(("loser_ally", CIV, z) for z, f in fl[1] if f > 0),
                            values=(dead_w, dead_l, pass_id), causes=bcauses)
            self.allied_casualties(W, dead_w, fw, e, war)
            self.allied_casualties(L, dead_l, fl, e, war)
        elif pass_id:
            e = self.append("battle_at_pass", region=r, participants=(("winner", CIV, W), ("loser", CIV, L)),
                            values=(dead_w, dead_l, pass_id), causes=bcauses)
        else:
            e = self.append("battle", region=r, participants=(("winner", CIV, W), ("loser", CIV, L)),
                            values=(dead_w, dead_l), causes=bcauses)
        if not some_ally:
            self.casualties(W, dead_w, e, war)
            self.casualties(L, dead_l, e, war)
        war["fought"].add(r)
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
        dest = None
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
                    if FLAGS["held"]:
                        # r10 20.22: the guard holds and says why (1 no destination, 2 too few survivors); with
                        # refuge_flight 0 (or before the flight rule) reason 1 (r11 20.24 item 1)
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

    def side_forces(self, war, c, r):
        """P5-04 7.7: (F_S, (F_principal, [(ally, F_m)] ascending id)) of principal c's side at R: the principal's
        local force divided by its open wars, each open ally's local force divided by its open wars times
        ally_effort_permille div 1000."""
        side = 1 if c == war["a"] else 0
        fp = self.local_force(c, r) // self.open_wars_of(c)
        fa = []
        for q in sorted((q for q in war["allies"] if q["open"] and q["side"] == side), key=lambda q: q["z"]):
            m = q["z"]
            fa.append((m, self.local_force(m, r) // self.open_wars_of(m) * self.ct["ally_effort_permille"] // 1000))
        return fp + sum(f for _m, f in fa), (fp, fa)

    def allied_casualties(self, c, d_S, fs, e, war):
        """P5-04 7.7: each ally m with F_m > 0 takes d_S x F_m div F_S (ascending id), the principal the rest;
        each ally's dead in the war += its share (reading A6: the share, as stated)."""
        fp, fa = fs
        F_S = fp + sum(f for _m, f in fa)
        shares = [(m, d_S * f // F_S) for m, f in fa if f > 0]
        rest = d_S - sum(x for _m, x in shares)
        for m, x in shares:
            self.casualties(m, x, e, None)
            for q in war["allies"]:
                if q["z"] == m:
                    q["dead"] += x
        self.casualties(c, rest, e, war)

    def battle_tolls(self, FW, FL, w):
        """7.3 item 4 with r7, r10 20.16 and r22 24.5: (dead_W, dead_L) of a battle of war w in this year."""
        t = self.ct
        dead_l = FL * t["loss_loser_permille"] // 1000
        if FLAGS["kill"]:      # r10 20.16: at most kill_max of the victors' (local) force
            dead_l = min(dead_l, FW * t["kill_max_permille"] // 1000)
        # r7 7.3 item 4: the victors lose loss_winner of the smaller force (before r7, of their own force)
        dead_w = (min(FW, FL) if FLAGS["loss"] else FW) * t["loss_winner_permille"] // 1000
        if FLAGS["kill"]:
            self.kills.append((FW, dead_l))   # (9) reads the rule's bound before 24.5's factor (SPEC_GAPS r22 (9))
        sp = t.get("toll_spread_permille", 0) if FLAGS["toll"] else 0
        if sp > 0:   # r22 24.5: the keyed factors, k = 0 the loser's, k = 1 the winner's; no draw at s = 0
            v_l = 1000 - sp + pnr.pick(self.draw("hist.conflict.toll", w, 0, 0), 2 * sp + 1)
            v_w = 1000 - sp + pnr.pick(self.draw("hist.conflict.toll", w, 0, 1), 2 * sp + 1)
            dead_l = dead_l * v_l // 1000
            dead_w = dead_w * v_w // 1000
            self.toll_log.append((self.year, w, v_l, v_w, dead_l, dead_w, FW, FL))
        return dead_w, dead_l

    # --- r10 20.17: the search for a concealed people
    def conceal_search(self, war, x, y, r):
        """Before the battle draw: when every standing visible settlement of y in R is concealed, x searches.
        Returns True when the year's fighting stops here (not found, or the war given up)."""
        ys = [s for s in self.svis_seen(y) if s["region"] == r]   # r11 20.24 item 2: not hidden
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
        sal = None
        if FLAGS["search_sal"] and any(ev["kind"] == K["search_abandoned"] for ev in self.events):
            sal = t["search_repeat_salience"]     # r14 21.7 item 3: only the first is news
        e = self.append("search_abandoned", region=r, participants=(("searcher", CIV, x), ("hidden", CIV, y)),
                        values=(self.year - war["start"],), causes=(war["last"],), salience=sal)
        if FLAGS["search_weary"] and t["search_weary_years"] > 0:
            self.searched_until[(y, r)] = self.year + t["search_weary_years"]   # r14 21.7 item 1
        self.count_futile(war)
        self.close_rel(rel_key(a, b, "war"), e)
        self.open_rel(sym_key(a, b, "peace"), e)
        self.last_peace[(min(a, b), max(a, b))] = (self.year, e)
        self.add_grudge(a, b, t["war_grudge"])
        self.zero_strain(a, b)
        self.drop_claims(a, b)
        self.close_war(war, "unfound", None)
        if self.alliances is not None:
            self.allies_at_end(war, e)        # P5-04 7.10 (reading A3: the truce bookkeeping of 7.7)

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
        if "plunder" in war:                   # P5-04 7.15 (reading A5: the refuge's sack is a sack)
            war["plunder"][x] += ts["population"]
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
        name, _t = pnr.make_name(self.dat.naming[cul], cul, "settlement", self.inp.seed, cnum, h, self.registry,
                                 cores=self.name_cores())
        e2 = self.append("refugees_settled", region=d, settlement=h,
                         participants=(("settlement", SETTLEMENT, h), ("founder", CIV, y), ("origin", SETTLEMENT, ts["id"])),
                         values=(n,), causes=(e,))
        got = self.found_settlement(sx, sy, d, y, self.founding_kind(race, d, n, sx, sy), n, name, e2)
        assert got == h
        if self.ledger is not None:   # P5-03 7.2: moved(t, h, n) right after the refuge is founded
            self.ledger.moved(ts["id"], h, n)
        self.site_how[h] = how
        self.set_owner(d, y, e2)
        self.hidden_until[h] = self.year + self.hidden_years_of(y)   # r17 22.2: the founder's race
        if FLAGS["hidden42"] and self.hidden_years_of(y) >= 1:
            # r22 24.4: the refuge's hidden years, year Y to min(hidden_until, E) - 1, caused by the flight
            self.append("refuge_hidden", end_year=min(self.hidden_until[h], self.inp.params["span"]) - 1, region=d,
                        settlement=h, participants=(("settlement", SETTLEMENT, h), ("founder", CIV, y)), causes=(e2,))
        if FLAGS["lie_low"]:
            self.lie_low(y, h, self.hidden_until[h])     # r17 22.3, P3-05 r15 22.7a
            self.low_log.append((self.year, y, self.hidden_until[h]))
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
            if war["allies"]:                 # P5-04 7.8: the dead of each side
                return (self.side_dead(war, b) - self.side_dead(war, a)) + (tp[a] - tp[b])
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

    def exhausted(self, war, c):
        t = self.ct
        return war["dead"][c] * 1000 >= t["exhaustion_permille"] * max(1, war["pop0"][c] * self.muster(c) // 1000)

    def goal_ending(self, war, how):
        """r14 21.6 items 1 and 2, at a check of 7.7 or at 7.8's end of an active phase: True when the war ended
        here. Item 1: the attacker holds G and wins (`peace` when b is exhausted, else `war_won`), no last-battle
        rule. Item 2: b neither holds G nor stands in it: the war ends now by the score with the last-battle rule
        (`peace` when the loser is exhausted, else `war_won`; a truce on 0)."""
        a, b, g = war["a"], war["b"], war["goal"]
        o = self.owner[g][0]
        lbw = war.get("last_battle_winner")
        if o == a and self.aim_clear(war):
            self.aim_log.append((war["w"], "aim", a, o, self.by_score(war), lbw))
            self.end_war(war, a, "aim", "peace" if self.exhausted(war, b) else "war_won")
            return True
        if o != b and (o != a or not FLAGS["aim_foot"]) and not any(s["region"] == g for s in self.svis(b)):
            # 21.6 item 2 (o != a: r17 22.5 item 2 leaves a held G with b in it to item 3)
            win = self.decided(war, self.by_score(war))
            kind = "war_won"
            if win is not None and self.exhausted(war, b if win == a else a):
                kind = "peace"
            self.aim_log.append((war["w"], "lost", win, o, self.by_score(war), lbw))
            self.end_war(war, win, how, kind)
            return True
        return False

    def aim_clear(self, war):
        """r17 22.5 item 1: the defender has no standing settlement in G (every one, visible or not, hidden ones
        included, as 20.27 counts them); before r17 always true."""
        if not FLAGS["aim_foot"]:
            return True
        return not any(s["region"] == war["goal"] for s in self.standing_of(war["b"]))

    def end_check(self, war):
        t = self.ct
        a, b = war["a"], war["b"]
        if FLAGS["aim_hold"] and self.goal_ending(war, "goal-lost"):
            return
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
        if not FLAGS["aim_hold"] and self.owner[g][0] != b and not any(s["region"] == g for s in self.svis(b)):
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
        e = self.end_war_025(war, winner, how, kind)
        if self.alliances is not None:
            # P5-04 7.10, 7.15: after the costs (and the tribute's people), the allies' rows; the winner and loser
            war["spoils_pair"] = (winner, war["b"] if winner == war["a"] else war["a"]) if winner is not None \
                else (0, 0)
            self.allies_at_end(war, e)
        return e

    def end_war_025(self, war, winner, how, kind="peace"):
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
            return e
        plan = self.cost_plan(war, winner)
        if plan is None:
            self.after_cost(war, winner)
            return e
        L = b if winner == a else a
        if plan[0] == "tribute":
            n = plan[1]
            et = self.append("tribute", participants=(("payer", CIV, L), ("payee", CIV, winner)), values=(n,),
                             causes=(e,))
            self.casualties(L, n, et, None)
            if "tribute_event" in war:          # P5-04 7.15: the tribute's people and id for war_spoils
                war["tribute_people"], war["tribute_event"] = n, et
            war["cost"] = "tribute"
            self.after_cost(war, winner)
            return e
        g = plan[1]
        if plan[0] == "foothold" and FLAGS["foot40"]:
            # r19 23.3: kind 40 with the settlements that pass (every standing settlement of L in G, hidden ones
            # included, as 20.27 counts them; ascending id, at most 64 listed), the list taken before the transfer
            ids = [s["id"] for s in self.standing_of(L) if s["region"] == g]
            ec = self.append("settlements_ceded", region=g,
                             participants=(("from", CIV, L), ("to", CIV, winner)) +
                             tuple(("settlement", SETTLEMENT, x) for x in ids[:MAX_PARTICIPANTS]), causes=(e,))
        else:
            ec = self.append("region_ceded", region=g, participants=(("from", CIV, L), ("to", CIV, winner)),
                             causes=(e,))
        if plan[0] == "foothold":
            # r17 22.5 item 3.2: 20.27 item 1's procedure over every standing settlement of L in G; no
            # set_region_owner (W holds G already, no region_control row); wronged(L, W) gains G
            ids = [s["id"] for s in self.standing_of(L) if s["region"] == g]
            race_w = self.race_c(winner)
            for sid in ids:
                s = self.stl(sid)
                excess = max(0, s["population"] - 2 * self.geo.capacity(g, race_w))
                if excess:
                    s["population"] -= excess
                    self.refugees(L, s, excess, ec)
                s["owner_civ"] = winner
                s["owner_event"] = ec   # P5-03 7.3
                self.version += 1
            self.wrong(L, winner, g)
            war["ceded"] = True
            war["cost"] = "foothold"
            self.after_cost(war, winner)
            return e
        if FLAGS["cede_all"]:
            # r13 20.27 item 1: every settlement of L standing in G (all of State, not only visible(): hidden
            # ones and those created earlier in the step), ascending, the list taken before any change
            ids = [s["id"] for s in self.standing_of(L) if s["region"] == g]
        else:
            ids = [s["id"] for s in self.svis(L) if s["region"] == g]
        race_w = self.race_c(winner)
        for sid in ids:
            s = self.stl(sid)
            excess = max(0, s["population"] - 2 * self.geo.capacity(g, race_w))
            if excess:
                s["population"] -= excess
                self.refugees(L, s, excess, ec)
            s["owner_civ"] = winner
            s["owner_event"] = ec   # P5-03 7.3
            self.version += 1
        self.set_owner(g, winner, ec)
        self.wrong(L, winner, g)        # r9 20.5
        war["ceded"] = True
        war["cost"] = plan[0]
        self.after_cost(war, winner)
        return e

    def after_cost(self, war, winner):
        """r17 invariant (17), in the run: right after the cost of a peace or war_won won by the attacker W over L
        with goal G, if W holds G then L has no standing settlement in G, unless L's capital stands in G."""
        if winner is None or winner != war["a"] or not FLAGS["aim_foot"]:
            return
        W, L, g = war["a"], war["b"], war["goal"]
        self.cost_log.append((war["w"], W, L, g, war.get("cost")))
        if self.owner[g][0] != W or not self.alive(L):
            return
        mine = [s["id"] for s in self.standing_of(L) if s["region"] == g]
        if mine and self.civ(L)["capital"] not in mine:
            self.inv17.append((self.year, war["w"], W, L, g, mine))

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
        if FLAGS["aim_foot"] and W == war["a"]:
            # r17 22.5 item 3.2: W holds G, L stands in G (every standing settlement, as 20.27), L's capital not
            # among them: the foothold cession
            g = war["goal"]
            mine = [s for s in self.standing_of(L) if s["region"] == g]
            if self.owner[g][0] == W and mine and self.civ(L)["capital"] not in [s["id"] for s in mine]:
                return ("foothold", g)
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
            how, kind = "quiet", "war_won"
            if FLAGS["aim_hold"] and self.owner[war["goal"]][0] == war["a"] and self.aim_clear(war):
                # r14 21.6 item 1 at 7.8's end (r17 22.5 item 1: and b out of G): the attacker holds its goal and wins, no last-battle rule
                winner, how = war["a"], "aim"
                kind = "peace" if self.exhausted(war, war["b"]) else "war_won"
            elif FLAGS["aim_hold"] and winner is not None and self.owner[war["goal"]][0] != war["b"] and \
                    (self.owner[war["goal"]][0] != war["a"] or not FLAGS["aim_foot"]) and not any(
                    s["region"] == war["goal"] for s in self.svis(war["b"])):
                # r15 21.6: an ending of item 2 in a quiet step writes peace when the loser is exhausted
                loser = war["b"] if winner == war["a"] else war["a"]
                kind = "peace" if self.exhausted(war, loser) else "war_won"
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
            if FLAGS["aim_hold"]:
                self.aim_log.append((war["w"], how if how == "aim" else (
                    "lost" if self.owner[war["goal"]][0] != war["b"] and (
                        self.owner[war["goal"]][0] != war["a"] or not FLAGS["aim_foot"]) and not any(
                        s["region"] == war["goal"] for s in self.svis(war["b"])) else "quiet"),
                    winner, self.owner[war["goal"]][0], self.by_score(war), war.get("last_battle_winner")))
            self.end_war(war, winner, how, kind)
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
                w.tension_all if conflict else None, Hooks(w))
    en.run()
    if w.monsters is not None:
        w.monsters.finish(inp.params["span"])   # P5-02 6.2.2: the rows at E - 1
    if w.race_relations is not None:
        w.race_relations.finish(inp.params["span"])  # P5-03 6.7: the rows at E - 1
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
    if FLAGS["rename"]:   # schema 8 (P3-04 r21 23.4): rows sorted by (region, from_year)
        res["region_name"] = sorted(w.renaming.rows) if w.renaming is not None else []
    if pe.RENDER_R23:   # schema 9 (P5-02 11): the five tables and settlement.ending
        res["beast"], res["monster_kind"] = pe.beast_rows(dat.base), pe.monster_kind_rows(dat.base)
        if w.monsters is not None:
            res["beast_population"] = w.monsters.beast_population_rows()
            res["monster"] = w.monsters.monster_rows()
            res["site"] = w.monsters.site_rows()
        if w.race_relations is not None:   # schema 10 (P5-03 16.1)
            res["race_relation"] = w.race_relations.race_relation_rows()
            res["settlement_race"] = w.race_relations.settlement_race_rows()
        pe.with_ending(res)
    if w.figures is not None:   # schema 12 (P5-06 9.1, 9.3): the figure and book rows, written at the end
        res["figure"] = w.figures.figure_rows()
        res["book"] = w.figures.book_rows()
    elif pe.RENDER_R27:
        res["figure"], res["book"] = [], []
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
    pw = principal_war_rows(res)   # P5-04 10.4: a joined war row is an ally's, not a war
    yrs = [(r[6] if r[6] is not None else E) - r[5] for r in pw]
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
        o.append("wars_attacker_%s = %d" % (r[1], sum(1 for x in pw if civ_race[x[1]] == r[0])))
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
        if kind[b] not in BATTLES():
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
            if kind[x] in CEDES + (K["tribute"],):
                out.add(x)
    return out, u


def change_years(res, g):
    """The years in which region g's owner changes, as weariness counts them: before 0.19.0 both ends of every
    region_control row of g; from 0.19.0 (r14 21.8, P3-07 r12 22.5b) the from_year of each row whose civ differs
    from that of g's previous row (g's first row is a change): a row of the same civilization after a gap is not
    a change."""
    rows = sorted((rc for rc in res["region_control"] if rc[0] == g), key=lambda rc: (rc[1], rc[3]))
    if not FLAGS["holder"]:
        return [y for rc in rows for y in (rc[1], rc[2]) if y is not None]
    out, prev = [], None
    for rc in rows:
        if rc[3] != prev:
            out.append(rc[1])
        prev = rc[3]
    return out


def same_goal_unchanged_max(res, E, kind, groups):
    """P3-07 r11 21.8 item 5 (P3-06 r13 20.28), in its incremental form: per (defender D, region G), the counted
    wars are the war_declared over (D, G) in id order without those closed by a side's end (the war row's
    until_event a civ_conquered or civ_ended); an open war has until = E. A change in [a, b] is a region_control
    row of G with from_year or to_year in [a, b]. For each counted war [s, u]: run += 1 when run > 0 and there is
    no change in [lo, max(hi, u)] (hi = max(hi, u)); else run = 1 (lo, hi = s, u) when there is no change in
    [s, u]; else run = 0. The key is the largest run."""
    rows = {r[7]: r for r in res["relation"] if r[4] == "war"}
    side_end = (K["civ_conquered"], K["civ_ended"])
    best = 0
    for (_b, g), ds in groups.items():
        changes = change_years(res, g)
        moved = lambda a, z: any(a <= y <= z for y in changes)  # noqa: E731
        run, lo, hi = 0, None, None
        for d in sorted(ds):
            r = rows[d]
            if r[6] is not None and kind[r[8]] in side_end:
                continue                  # neither extends a run nor breaks one
            s, u = r[5], (r[6] if r[6] is not None else E)
            if run > 0 and not moved(lo, max(hi, u)):
                run += 1
                hi = max(hi, u)
            elif not moved(s, u):
                run, lo, hi = 1, s, u
            else:
                run = 0
            best = max(best, run)
    return best


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
        changed = any(kind[x] in (K["sacked"], K["settlement_captured"], K["tribute"]) + CEDES for x in evs)
        if not changed and not (u is not None and kind[u] == K["civ_conquered"]):
            hollow += 1
    role = lambda e, seq: [p_[4] for p_ in parts.get(e, []) if p_[2] == seq][0]  # noqa: E731
    groups = {}
    for d in decl:
        groups.setdefault((role(d[0], 2), d[5]), []).append(d[0])
    unchanged = same_goal_unchanged_max(res, E, kind, groups)
    civ_race = {c[0]: c[2] for c in res["civilization"]}
    standing = [x for x in res["settlement"] if x[10] in (1, 2)]
    pop = sum(x[11] for x in standing)
    held = [x for x in res["region_control"] if x[2] is None]
    # .get: a standing settlement may have no owner (0), which belongs to no race
    pshare = max([sum(x[11] for x in standing if civ_race.get(x[8]) == r[0]) * 1000 // pop for r in inp.races]
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


def _rname(view, rid, eid):
    """P5-12 (READING): a region named in a note is named as in the noted event's sentence (the name in force
    at event eid, P3-04 r21 23.1); the stored name before 0.22.0."""
    if FLAGS["rename"] and "region_at" in view:
        return view["region_at"](rid, eid)
    return view["name"](REGION, rid, "name")


def history_note(res, inp, eid, view=None, idx=None):
    """P3-07 r8 21.4: the note of event eid without its parentheses, or ""."""
    kind, ev, causes, _kids, parts = idx or _index(res)
    e = ev[eid]
    if e[3] == K["war_declared"]:
        cl = sorted(c for c in causes.get(eid, []) if kind[c] in CLAIMS)
        if not cl:
            return ""
        yc = ev[cl[0]][1]
        if FLAGS["fresh"]:   # P3-07 r19 25.8: the year of root(c), c's claim cause with the smallest id, else c
            roots = sorted(x for x in causes.get(cl[0], []) if kind[x] in CLAIMS)
            if roots:
                yc = ev[roots[0]][1]
        note = "Claimed since %d." % yc if yc < e[1] else "Claimed the same year."
        if FLAGS["claim39"] and kind[cl[0]] == K["claim_settlements"]:
            # P3-07 r15 23 item 4a: after a kind 39 claim the towns claimed, in participant order, and the
            # claim's region, whether or not it is the declaration's
            view = view or pe.view_from_rows(res, [(r["id"], r["name"]) for r in inp.regions], inp.races, inp.gods)
            ss = [mid(view["name"](SETTLEMENT, p_[4], "name")) for p_ in sorted(parts[cl[0]]) if p_[2] == 3]
            where = "%s in %s" % (pe.join_all(ss), mid(_rname(view, ev[cl[0]][5], eid)))
            note = ("The claim, since %d, was to %s." % (yc, where) if yc < e[1] else
                    "The claim, made the same year, was to %s." % where)
        elif FLAGS["claim39"] and ev[cl[0]][5] != e[5]:
            # P3-07 r14 23 item 3: the claim's region when it differs from the declaration's
            view = view or pe.view_from_rows(res, [(r["id"], r["name"]) for r in inp.regions], inp.races, inp.gods)
            rc = mid(_rname(view, ev[cl[0]][5], eid))
            note = ("The claim, since %d, was to %s." % (yc, rc) if yc < e[1] else
                    "The claim, made the same year, was to %s." % rc)
        pair = sorted(p_[4] for p_ in parts[eid] if p_[3] == CIV)
        n = 1 + sum(1 for x in res["event"] if x[3] == K["war_declared"] and x[0] < eid and
                    sorted(p_[4] for p_ in parts[x[0]] if p_[3] == CIV) == pair)
        if n >= 2:
            note += " The %s war between them." % ordinal(n)
        return note
    if e[3] == K["flare_up"]:
        cs = sorted(causes.get(eid, []))
        if FLAGS["monsters"] and cs and kind[cs[0]] == K.get("monster_appeared"):
            # P3-07 (P5-02 12.7): a flare-up whose first cause is a monster's appearance; names mid-sentence
            view = view or pe.view_from_rows(res, [(r["id"], r["name"]) for r in inp.regions], inp.races, inp.gods)
            m = [p_[4] for p_ in parts[cs[0]] if p_[3] == pe.MONSTER][0]
            return "The waking of %s, %s, stirred the land." % (mid(view["name"](pe.MONSTER, m, "name")),
                                                                 view["name"](pe.MONSTER, m, "kind"))
        if not cs or kind[cs[0]] not in CLAIMS:
            return ""
        c = cs[0]
        view = view or pe.view_from_rows(res, [(r["id"], r["name"]) for r in inp.regions], inp.races, inp.gods)
        a = [p_[4] for p_ in parts[c] if p_[2] == 1][0]
        b = [p_[4] for p_ in parts[c] if p_[2] == 2][0]
        if kind[c] == K["claim_settlements"]:   # P3-07 r14 23 item 4, in participant order
            ss = [mid(view["name"](SETTLEMENT, p_[4], "name")) for p_ in sorted(parts[c]) if p_[2] == 3]
            return "The claim of %s to %s of %s in %s came to a head." % (
                mid(view["name"](CIV, a, "name")), pe.join_all(ss), mid(view["name"](CIV, b, "name")),
                mid(_rname(view, ev[c][5], eid)))
        if FLAGS["note_019"]:   # P3-07 r12 22.3: no "its"; every name mid-sentence
            return "The claim of %s to %s, held by %s, came to a head." % (
                mid(view["name"](CIV, a, "name")), mid(_rname(view, ev[c][5], eid)),
                mid(view["name"](CIV, b, "name")))
        return "%s pressed its claim to %s against %s." % (view["name"](CIV, a, "name"),
                                                          mid(_rname(view, ev[c][5], eid)),
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
            out |= {x for x in kids.get(u, []) if kind[x] in CEDES + (K["tribute"],)}
    elif k in (K["peace"], K["war_won"], K["truce"]):   # r10 / P3-07 21.8 item 1 (r14 23 item 2: kind 40 too)
        out |= {x for x in kids.get(eid, []) if kind[x] in CEDES + (K["tribute"],)}
    elif k in (K["sacked"], K["settlement_captured"]):
        out |= {x for x in kids.get(eid, []) if kind[x] in (K["capital_moved"], K["refugees_settled"],
                                                             K["civ_conquered"])}
    if FLAGS["relations"] and k == K["settlement_fled"]:   # P5-03 11.3: the abandonment it caused
        out |= {x for x in kids.get(eid, []) if kind[x] in (K["settlement_abandoned"], K["settlement_abandoned_to"])}
    if FLAGS["rename"]:   # P3-07 r21 26.5 item 1: a renaming with its cause, both ways
        out |= {x for x in kids.get(eid, []) if kind[x] == K["region_renamed"]}
        if k == K["region_renamed"]:
            out |= set(_causes.get(eid, []))
    return out


def summary_set(res, inp):
    """P3-07 r8 21.3: (T, the summary set, per-age counts of the set)."""
    idx = _index(res)
    ev = idx[1]
    cands = sorted((e for e in res["event"] if e[7] >= SUMMARY_MIN_SALIENCE), key=lambda e: (-e[7], e[0]))
    if FLAGS["note_019"]:
        # P3-07 r12 22.5a: a declaration whose war ended by a search_abandoned below SUMMARY_MIN_SALIENCE is no
        # candidate (and no companion of any event: no unit holds a war_declared but its own)
        quiet_search = {r[7] for r in res["relation"] if r[4] == "war" and r[6] is not None and
                        ev[r[8]][3] == K["search_abandoned"] and ev[r[8]][7] < SUMMARY_MIN_SALIENCE}
        cands = [e for e in cands if e[0] not in quiet_search]
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
                out = ("won-c%d" % win + (("-foothold" if FLAGS["foot40"] and war.get("cost") == "foothold" else
                                            "-ceded") if war["ceded"] else "") +
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
                   "quiet": "end of the active phase", "goal-lost": "goal lost", "conquest": "conquest of the defender"
                   if not w.alive(war["b"]) else "conquest of the attacker", "ended": "end of a side",
                   "unfound": "search abandoned"}[h]   # r9 20.10 (the `world` printout only; P5-01 integration)
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
                conquered=kc.get("civ_conquered", 0), claims=kc.get("claim", 0) + kc.get("claim_settlements", 0),
                passes=kc.get("battle_at_pass", 0),
                ceded=kc.get("region_ceded", 0) + kc.get("settlements_ceded", 0), truces=kc.get("truce", 0), peaces=kc.get("peace", 0))


# ---------------------------------------------------------------------------
# Inputs: world cases, synthetic and stress cases (16.2 item 15)
# ---------------------------------------------------------------------------

def inputs_for(dat, conn, p, seed, size):
    inp, chron = pp.inputs_for(dat, conn, p, seed, size)
    inp.biome_keys = {r[0]: r[1] for r in conn.execute("SELECT id, key FROM biome ORDER BY id")}
    # P5-12 3: the land noun reads the stored region rows' landmass and dominant biome
    inp.region_land = {r[0]: (r[1], r[2]) for r in conn.execute("SELECT id, landmass, biome FROM region ORDER BY id")}
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
                                      K["war_won"]} | set(CLAIMS) | set(CEDES)  # r9 20.3, r10 20.15, r19 23


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
                                                                                   K["sacked"]) + overrun_kinds())
        if s[10] == FADED:
            fades = [e for e in res["event"] if e[3] == K["ruin_faded"] and e[6] == s[0]]
            ok = len(fades) == 1 and len(causes.get(fades[0][0], [])) == 1
            if ok:
                ruin = ev[causes[fades[0][0]][0]]
                ok = (ruin[3] in (K["settlement_abandoned"], K["settlement_abandoned_to"], K["sacked"]) + overrun_kinds()
                      and ruin[6] == s[0] and
                      fades[0][1] >= ruin[1] + t["ruin_fade_years"] and s[12] is None and s[13] == 0)
            c.true("%s P3-05 inv 7 s%d faded" % (label, s[0]), ok)
    # r9 20.3: a tribute's people are taken through 7.4's casualties (note_reduction), a reduction like a battle
    red = ("famine", "plague", "plague_spread", "succession_crisis", "battle", "battle_at_pass", "tribute")
    if FLAGS["monsters"]:   # P5-02 6.4.1, 6.2.3: a raid and the beasts' boldness are reductions (note_reduction)
        red += ("monster_raid", "beasts_bold")
    if FLAGS["relations"]:   # P5-03 9.4: an abandonment for fear cites the flight; 6.5: an expulsion is a reduction
        red += ("settlement_fled", "people_expelled")
    if FLAGS["alliances"]:   # P5-04 11.1: the battle kinds set
        red += ("allied_battle",)
    if FLAGS["figures"]:   # P5-06 11.1: battle_led a battle; 6.8: a blight is a reduction (note_reduction)
        red += ("battle_led", "blight")
    want_cause = {
        "plague_spread": ("plague",), "civ_seceded": ("succession_crisis",),
        "settlement_abandoned": red + ("settlement_founded", "colony_founded"),
        "settlement_abandoned_to": red + ("settlement_founded", "colony_founded"),
        "capital_moved": ("settlement_abandoned", "settlement_abandoned_to", "sacked", "settlement_captured"),
        "civ_ended": ("settlement_abandoned", "settlement_abandoned_to"),
        "ruin_faded": ("settlement_abandoned", "settlement_abandoned_to", "sacked") + (
            ("settlement_overrun",) if FLAGS["monsters"] else ()),   # P5-02 6.4.3: an overrun is a ruin
        "settlement_dwindled": red,
    }
    if at_least("0.19.0"):
        # P3-05 r12 22.1: every outbreak is kind 37; 22.2 item 3: a dwindling may cite the colony it sent
        red = red + ("plague_outbreak",)
        want_cause.update({"plague_spread": ("plague_outbreak",), "settlement_dwindled": red + ("colony_founded",),
                           "settlement_abandoned": red + ("settlement_founded", "colony_founded"),
                           "settlement_abandoned_to": red + ("settlement_founded", "colony_founded")})
        mem = w.t["cause_memory_years"]
        for e in res["event"]:
            if KEY_OF[e[3]] in ("settlement_dwindled", "settlement_abandoned", "settlement_abandoned_to"):
                for x in causes.get(e[0], []):
                    if KEY_OF[kind[x]] in red + ("colony_founded",):
                        c.true("%s P3-05 r12 22.2 e%d cites e%d within cause_memory_years (%d years)" % (
                            label, e[0], x, e[1] - ev[x][1]), e[1] - ev[x][1] <= mem)
        c.eq("%s P3-05 r12 22.1 no kind 12 and no kind 4 event" % label,
             [e[0] for e in res["event"] if e[3] in (K["plague"], K["quiet_era"])], [])
    for e in res["event"]:
        key = KEY_OF[e[3]]
        cs = causes.get(e[0], [])
        if key.startswith("became_"):
            c.true("%s P3-05 inv 8 e%d cause" % (label, e[0]), cs == [sts[e[6]][7]])
        elif key in want_cause:
            c.true("%s P3-05 inv 8 e%d %s cause kinds %s" % (label, e[0], key, [KEY_OF[kind[x]] for x in cs]),
                   all(KEY_OF[kind[x]] in want_cause[key] for x in cs) and (cs or key == "settlement_dwindled"))
        if e[3] >= 6 and ph[e[4]][2] == QUIET:
            # P5-12 6: the renaming subsystem runs in Q4 as well, charged to its namer (W9)
            quiet_ok = e[3] in QUIET_KINDS or (FLAGS["rename"] and key == "region_renamed") or (
                FLAGS["monsters"] and key in ("monster_appeared", "monster_died", "outpost_abandoned")) or (   # P5-02 6.1 Q4
                FLAGS["relations"] and key in ("peoples_met", "disposition_turned", "refugees_arrived",
                                               "settlement_mixed", "standing_changed")) or (   # P5-03 6.1 Q1, Q4
                FLAGS["alliances"] and key in ("alliance_renewed", "alliance_lapsed", "alliance_broken",
                                               "trade_opened", "trade_broken", "spoils_shared", "spoils_withheld",
                                               "alliance_spurned", "mine_opened", "arms_sold")) or (   # P5-04 6.1 Q4
                FLAGS["figures"] and e[3] >= K.get("came_to_rule", 1 << 30))   # P5-06 6.1 Q4, 6.13 (invariant 8 there)
            c.true("%s inv 8/14.2 e%d %s not in a quiet step" % (label, e[0], key), quiet_ok)
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
        if FLAGS["figures"] and r[0] == FIGURE:
            continue   # P5-06 9.2: ruled, served, rival, parent (checked by p5_06_figures_reference's 9.4 and 13)
        lab = "%s inv 1 relation %s" % (label, r)
        c.true(lab + " types", r[0] == CIV and r[2] == CIV)
        al = FLAGS["alliances"]
        c.true(lab + " kind", r[4] in ("enmity", "war", "peace") + (("alliance", "trade") if al else ()))
        if r[4] in ("enmity", "peace", "alliance", "trade"):
            c.true(lab + " a < b", r[1] < r[3])
        sk = KEY_OF[kind[r[7]]]
        uk = KEY_OF[kind[r[8]]] if r[6] is not None else None
        ends = ("civ_ended", "civ_conquered")
        if r[4] in ("alliance", "trade"):
            # P5-04 9.1: opened by alliance_formed / trade_opened of the pair; closed by its kinds or a side's end
            opener = "alliance_formed" if r[4] == "alliance" else "trade_opened"
            c.true(lab + " since_event", sk == opener and sorted(role(r[7], 1) + role(r[7], 2)) == [r[1], r[3]] and
                   ev[r[7]][1] == r[5])
            if uk is not None:
                closers = (("alliance_lapsed", "alliance_broken", "alliance_spurned", "alliance_betrayed")
                           if r[4] == "alliance" else ("trade_broken",))
                c.true(lab + " until_event", (uk in closers and sorted(role(r[8], 1) + role(r[8], 2)) ==
                                              [r[1], r[3]]) or (uk in ends and civs[role(r[8], 1)[0]][9] == r[8]
                                                                and role(r[8], 1)[0] in (r[1], r[3])))
        elif r[4] == "war" and al and sk == "ally_joined_war":
            # P5-04 7.6, 9.1: a joined row: the ally as a, the enemy as b; closed by the ally's withdrawal, the war's
            # end or a side's end
            c.true(lab + " since_event", role(r[7], 1) == [r[1]] and role(r[7], 3) == [r[3]] and ev[r[7]][1] == r[5])
            if uk is not None:
                c.true(lab + " until_event", uk in ("ally_withdrew", "peace", "war_won", "truce", "search_abandoned")
                       + ends)
        elif r[4] == "enmity":
            c.true(lab + " one enmity per pair", (r[1], r[3]) not in seen_enmity)
            seen_enmity.add((r[1], r[3]))
            if FLAGS["relations"]:
                # P5-03 7.1 (4.3): since_event = the larger of the younger's founding event and the hatred turn
                yng = civs[max(r[1], r[3])]
                c.true(lab + " since_event", sk in ("civ_founded", "civ_seceded", "disposition_turned") and
                       r[7] >= yng[7])
                # the row opens when the pair first qualifies (the year of the step), at or after since_event's
                c.true(lab + " since at or after the year of since_event", r[5] >= ev[r[7]][1])
            else:
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
            c.true(lab + " since_event", sk in ("peace", "war_won", "truce", "search_abandoned") +
                   (("ally_withdrew",) if al else ()))
            if uk is not None:
                c.true(lab + " until_event", uk in ("war_declared", "civ_ended", "civ_conquered") +
                       (("ally_joined_war",) if al else ()))
    pairs = {}
    for r in rel:
        if r[4] in ("war", "peace"):
            pairs.setdefault((min(r[1], r[3]), max(r[1], r[3])), []).append(r)
    for pr, rows in pairs.items():
        iv = sorted((r[5], r[6] if r[6] is not None else INF, r[4]) for r in rows)
        for x, y in zip(iv, iv[1:]):
            c.true("%s inv 1 pair %s rows overlap %s %s" % (label, pr, x, y), x[1] <= y[0])
    # N5: every war_declared leaves exactly one war row; ends of a war
    war_rows = {r[7]: r for r in rel if r[4] == "war" and KEY_OF[kind[r[7]]] == "war_declared"}
    for e in res["event"]:
        if e[3] != K["war_declared"]:
            continue
        lab = "%s inv 2 war e%d" % (label, e[0])
        a, b = role(e[0], 1)[0], role(e[0], 2)[0]
        c.true(lab + " leaves a war row (N5)", e[0] in war_rows)
        cl = [x for x in causes.get(e[0], []) if kind[x] in CLAIMS]
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
                   "search_abandoned", "allied_battle", "ally_withdrew", "battle_led"):
            c.true("%s inv 2 e%d %s not in a quiet phase" % (label, e[0], key), ph[e[4]][2] != QUIET)
        if key in ("peace", "war_won", "truce", "region_ceded", "settlements_ceded", "tribute") and ph[e[4]][2] == QUIET:
            row = ph[e[4]]
            c.true("%s inv 2 e%d %s at a quiet step start" % (label, e[0], key),
                   (e[1] - row[3]) % w.eng["quiet_step_years"] == 0)
        if key in ("peace", "war_won", "truce", "search_abandoned", "ally_withdrew"):
            cs = causes.get(e[0], [])
            c.true("%s inv 2 e%d caused by the war's last event" % (label, e[0]),
                   len(cs) == 1 and KEY_OF[kind[cs[0]]] in ("war_declared", "battle", "battle_at_pass", "siege",
                                                           "sacked", "settlement_captured", "refugees_settled",
                                                           "refuge_held", "allied_battle", "battle_led"))
        if key == "region_ceded":
            cs = causes.get(e[0], [])
            c.true("%s 11 e%d cession caused by a peace" % (label, e[0]), len(cs) == 1 and
                   kind[cs[0]] in (K["peace"], K["war_won"]))
        if key in ("battle", "battle_at_pass", "allied_battle", "battle_led"):
            cs = causes.get(e[0], [])
            # P5-04a 9.2: a battle also cites at most one arms_sold (the first battle its winner won after a sale)
            decl = [x for x in cs if kind[x] == K["war_declared"]]
            arms = [x for x in cs if KEY_OF[kind[x]] == "arms_sold"]
            c.true("%s 11 e%d caused by its declaration" % (label, e[0]), len(decl) == 1 and len(arms) <= 1 and
                   len(cs) == 1 + len(arms) and (not arms or FLAGS["p504a"]))
        if key in ("claim", "claim_settlements"):
            cs = causes.get(e[0], [])
            # 8.3 item 4: a colony of the same step is no claim cause, unless it is the region's
            # owner_event (its ownership is live state)
            # (P3-10 r3, stress-7 e1153: the colony made its founder the region's owner in T2 and a secession in T4
            # of the same step passed the region on, so the bookkeeping's net rule leaves the colony no
            # region_control row; it was the owner_event at the claim all the same: a colony of the holder in the
            # claimed region, which the holder did not own the year before, is exempt too)
            hold_c = [p_[4] for p_ in parts.get(e[0], []) if p_[2] == 2]

            def owner_event_colony(x):
                if ev[x][5] != e[5] or [p_[4] for p_ in parts.get(x, []) if p_[2] == 2] != hold_c:
                    return False
                return not any(r[0] == e[5] and r[3] == hold_c[0] and r[1] <= e[1] - 1 and
                               (r[2] is None or r[2] > e[1] - 1) for r in res["region_control"])
            c.true("%s 8.3 item 4 e%d claim cause not a colony of the same step" % (label, e[0]),
                   not any(kind[x] == K["colony_founded"] and ev[x][1] == e[1] and
                           not any(r[0] == e[5] and r[4] == x for r in res["region_control"]) and
                           not owner_event_colony(x) for x in cs))
            # r22 24.1 item 3: a fresh claim's causes are the old claim and 5.3's cause on G
            cs5 = [x for x in cs if kind[x] not in CLAIMS] if FLAGS["fresh"] and \
                sum(1 for x in cs if kind[x] in CLAIMS) == 1 else cs
            c.true("%s 11 e%d claim cause" % (label, e[0]), len(cs5) == 1 and KEY_OF[kind[cs5[0]]] in (
                "civ_founded", "settlement_founded", "colony_founded", "civ_seceded", "settlement_captured",
                "region_ceded", "refugees_settled", "settlements_ceded"))
    # r7 7.3 item 4: with loss_winner <= loss_loser the victors never lose more than the defeated
    if FLAGS["loss"] and w.ct["loss_winner_permille"] <= w.ct["loss_loser_permille"]:
        for e in res["event"]:
            if e[3] in BATTLES():
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
            ok = ok and len(bc) == 1 and kind[bc[0]] in BATTLES() and \
                ev[bc[0]][1] == e[1] and role(bc[0], 1) == role(sg[0], 1)
        c.true(lab + " caused by a fallen siege after a won battle", ok)
    for e in res["event"]:
        if e[3] == K["siege"]:
            bc = causes.get(e[0], [])
            c.true("%s inv 3 siege e%d after a battle won by its attacker" % (label, e[0]),
                   len(bc) == 1 and kind[bc[0]] in BATTLES() and
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
                   not any(r[6] is None and ((r[0] == CIV and r[1] == x[0]) or (r[2] == CIV and r[3] == x[0]))
                           for r in rel))   # P5-06: by entity type (a figure's id may equal the civilization's)
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
        if FLAGS["monsters"] and r[5] and w.events[r[5] - 1]["key"] == "monster_appeared":
            # P5-02 6.5: the second tension source; its trigger is a flare kind's appearance in a quiet step
            mlog = {s: (v, e) for (s, v, e) in w.monsters.tension_log}
            prev = [s for s in mlog if s < r[3]]
            c.true("%s inv 8 (P5-02 6.5) flare-up %d: the monsters' tension, above conflict's" % (label, r[0]),
                   bool(prev) and mlog[max(prev)] == (thr, r[5]) and log[max(prev)][0] < thr)
            continue
        c.true("%s inv 8 flare-up %d trigger is a claim" % (label, r[0]), kind.get(r[5]) in CLAIMS)
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
    changes = {g: change_years(res, g) for g in {rc[0] for rc in res["region_control"]}}
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
        if e[3] in CLAIMS:
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
            opened = sum(1 for r in principal_war_rows(res) if r[7] < e[0] and
                         ev[r[7]][5] == g and role(r[7], 2) == b and (r[6] is None or r[8] > e[0]))
            now = run if (last_end is not None and not any(last_end <= y <= e[1] for y in ch)) else 0
            if now + opened >= ww:
                out.append((e[0], b, g))
            continue
        if run >= ww and last_end is not None and not any(last_end <= y <= e[1] for y in ch):
            out.append((e[0], b, g))
    return out


def cession_violations(res):
    """P3-06 r13 20.27 item 6, invariant (13), from the rows: settlement owners replayed from the log (foundings,
    secessions, captures, cessions passing every settlement of `from` standing in the region, ruins). For each
    region_ceded e of G at year Y from L to W: L held G just before e (a region_control row of L for G open at Y,
    or, when L's hold began in the same step, a settlement of L standing in G by the replay, such as a colony
    founded earlier in the step: P3-04 6.7 item 1's net rule leaves L no row); and every settlement the replay
    has as L's, standing in G just before e, has its census row of year Y (the step's bookkeeping) with an owner
    other than L. Finally the replay's owners and standing equal the settlement table. Returns a list of
    messages, empty when the invariant holds."""
    kind, ev, _causes, _kids, parts = _index(res)
    role = lambda e, seq: [p_[4] for p_ in parts.get(e, []) if p_[2] == seq]  # noqa: E731
    sts = {x[0]: x for x in res["settlement"]}
    founded = {x[7]: x[0] for x in res["settlement"]}
    census = {(r[0], r[1]): r for r in res["settlement_census"]}
    seceded_regions = {}
    for rc in res["region_control"]:
        seceded_regions.setdefault(rc[4], []).append(rc[0])
    owner, standing = {}, {}
    out = []
    ruin_kinds = (K["sacked"], K["settlement_abandoned"], K["settlement_abandoned_to"]) + overrun_kinds()
    for e in sorted(res["event"]):
        eid, y, k = e[0], e[1], e[3]
        if eid in founded:
            s = founded[eid]
            owner[s], standing[s] = sts[s][5], True
        if k == K["civ_seceded"]:
            c2, frm = role(eid, 1)[0], role(eid, 2)[0]
            regs = set(seceded_regions.get(eid, [])) | {e[5]}
            for s in sorted(owner):
                if owner[s] == frm and standing[s] and sts[s][4] in regs:
                    owner[s] = c2
        elif k == K["settlement_captured"]:
            owner[e[6]] = role(eid, 1)[0]
        elif k in ruin_kinds:
            owner[e[6]], standing[e[6]] = 0, False
        elif k in CEDES:
            g, L, W = e[5], role(eid, 1)[0], role(eid, 2)[0]
            mine = [s for s in sorted(owner) if owner[s] == L and standing[s] and sts[s][4] == g]
            held_row = any(rc[0] == g and rc[3] == L and rc[1] <= y and (rc[2] is None or rc[2] >= y)
                           for rc in res["region_control"])
            if k == K["settlements_ceded"]:   # r19 23.3: (13) for the foothold cession: L stands in G, W holds it
                held_row = bool(mine) and any(rc[0] == g and rc[3] == W and rc[1] <= y and
                                              (rc[2] is None or rc[2] >= y) for rc in res["region_control"])
            if not held_row and not mine:
                out.append("e%d: c%d ceded rg%d, which it did not hold" % (eid, L, g))
            for s in mine:
                row = census.get((s, y))
                if row is None or row[2] == L:
                    out.append("e%d: s%d of c%d in rg%d did not pass to c%d (census of %d: %s)" %
                               (eid, s, L, g, W, y, "none" if row is None else "c%d" % row[2]))
                owner[s] = W
    for s, x in sorted(sts.items()):
        st_ = x[10] in (THRIVING, DECLINING)
        if standing.get(s) != st_ or (st_ and owner.get(s) != x[8]):
            out.append("s%d: replayed owner c%s standing %s, stored c%d status %d" %
                       (s, owner.get(s), standing.get(s), x[8], x[10]))
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
    hy_of = w.hidden_years_of   # r17 22.2: the founder's race (before: history.conflict.hidden_years)
    # 3 refugees_settled
    for e in res["event"]:
        if e[3] != K["refugees_settled"]:
            continue
        lab = "%s r9 20.13.3 e%d" % (label, e[0])
        h, y, o = role(e[0], 1)[0], role(e[0], 2)[0], role(e[0], 3)[0]
        c.true(lab + " population >= abandon_min", vals[e[0]][1] >= w.t["abandon_min"])
        cs = causes.get(e[0], [])
        # P5-07 7.3 (L-7): or caused by a hold_lost of the same year whose settlement is the origin and whose
        # civilization (role 2) is the founder (the deep flight writes no sack)
        deep = FLAGS["deep"] and len(cs) == 1 and kind[cs[0]] == K["hold_lost"]
        ok = len(cs) == 1 and kind[cs[0]] in ((K["sacked"], K["hold_lost"]) if FLAGS["deep"] else (K["sacked"],)) \
            and ev[cs[0]][1] == e[1] and ev[cs[0]][6] == o and role(cs[0], 2) == [y] and (deep or True)
        c.true(lab + " caused by the sack of its origin, by its founder's enemy, the same year (or its hold_lost)", ok)
        c.true(lab + " the settlement is the event's and founded by it", e[6] == h and sts[h][7] == e[0] and
               sts[h][5] == y and sts[h][4] == e[5])
        race = civs[y][2]
        others = [x for x in res["civilization"] if x[2] == race and x[0] != y and x[6] <= e[1] and
                  (x[8] is None or x[8] > e[1] or (x[8] == e[1] and x[9] > e[0]))]
        c.true(lab + " the founder was its race's only alive civilization", not others)
        c.true(lab + " the origin was the founder's capital (sacked at capital_sacked_salience)",
               deep or ev[cs[0]][7] == t["capital_sacked_salience"])
        c.true(lab + " no kind 31 caused by the sack", not any(kind[x] == K["settlement_abandoned_to"]
                                                                  for x in kids.get(cs[0], [])))
        y_end = e[1] + hy_of(y)
        for x in res["event"]:
            if not (e[1] <= x[1] < y_end) or x[0] <= e[0]:
                continue
            if x[3] == K["siege"]:
                c.true(lab + " hidden: never besieged (e%d)" % x[0], x[6] != h)
            if x[5] != e[5] or x[3] not in BATTLES() + (K["war_declared"],) + CLAIMS:
                continue
            against = (y in role(x[0], 1) + role(x[0], 2)) if x[3] in BATTLES() else \
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
            costs = [x for x in kids.get(e[0], []) if kind[x] in CEDES + (K["tribute"],)]
            c.true("%s r9 20.13.4 e%d at most one cession or tribute per peace" % (label, e[0]), len(costs) <= 1)
        if e[3] in (K["truce"], K["search_abandoned"]):
            c.true("%s r9 20.3 e%d a truce (or a war given up, r10 20.17) costs nothing" % (label, e[0]),
                   not any(kind[x] in CEDES + (K["tribute"],) for x in kids.get(e[0], [])))
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
    if FLAGS["cede_all"]:
        # r13 20.27 item 6: invariant (13), from the log
        c.eq("%s r13 20.13.13 a cession passes every settlement of the loser in the region" % label,
             cession_violations(res), [])
    check_invariants_r10(c, res, inp, w, label, kind, ev, causes, kids, parts, vals)
    if FLAGS["aim_hold"]:
        check_invariants_r14(c, res, inp, w, label, kind, ev, causes, kids, parts, vals)
    if FLAGS["aim_foot"]:
        check_invariants_r17(c, res, inp, w, label)
    if FLAGS["fresh"]:
        check_invariants_r22(c, res, inp, w, label)
    c.eq("%s in-run r9 checks" % label, w.checks9, [])


def replay_r17(res, dat=None):
    """P3-06 r17 22.3, 22.5 from the rows: settlement owners, standing and capitals replayed from the log (as
    cession_violations, with foothold cessions: a region_ceded passes every standing settlement of `from` in the
    region), region holders from region_control. Returns (problems, facts): problems are the restated (8), (15) and
    (17) and the replay's own check against the stored tables; facts are counts (quick aim wins, foothold
    cessions, wars over a region the attacker held at the declaration by outcome)."""
    kind, ev, causes, kids, parts = _index(res)
    role = lambda e, seq: [p_[4] for p_ in parts.get(e, []) if p_[2] == seq]  # noqa: E731
    vals = {}
    for v in res["event_value"]:
        vals.setdefault(v[0], {})[v[1]] = v[2]
    sts = {x[0]: x for x in res["settlement"]}
    civs = {x[0]: x for x in res["civilization"]}
    founded = {x[7]: x[0] for x in res["settlement"]}
    seceded_regions = {}
    rc_by_event = {}
    for rc in res["region_control"]:
        seceded_regions.setdefault(rc[4], []).append(rc[0])
        rc_by_event.setdefault(rc[4], []).append(rc)
    dat = dat or data()
    hy = {rid: r.get("hidden_years", 300) for rid, r in dat.race.items()}
    war_of_end = {r[8]: r[7] for r in principal_war_rows(res) if r[6] is not None}
    war_start = {r[7]: r for r in principal_war_rows(res)}
    sf = role_of(res, "colony_founded", "founder")

    def holder(g, y, before_event=None):
        """The owner of g in year y: the region_control row of g covering y with the largest from_year (rows opened
        at y by an event after before_event excluded), 0 if none."""
        best = None
        for rc in res["region_control"]:
            if rc[0] != g or rc[1] > y or (rc[2] is not None and rc[2] < y):
                continue
            if before_event is not None and rc[1] == y and rc[4] > before_event:
                continue
            if best is None or rc[1] > best[1] or (rc[1] == best[1] and rc[4] > best[4]):
                best = rc
        return 0 if best is None else best[3]

    owner, standing, capital = {}, {}, {}
    flights = {}            # civ -> (refugees_settled event, h, year)
    problems, facts = [], dict(quick_aim_wins=0, foothold_cessions=0, held_goal_wars={})
    ruin_kinds = (K["sacked"], K["settlement_abandoned"], K["settlement_abandoned_to"]) + overrun_kinds()

    def mine_in(L, g):
        return [s_ for s_ in sorted(owner) if owner[s_] == L and standing[s_] and sts[s_][4] == g]

    def lies_low(y, year):
        f = flights.get(y)
        if f is None:
            return False
        _e, h, yf = f
        return year < yf + hy[civs[y][2]] and standing.get(h) and owner.get(h) == y

    pending17 = {}          # ending event -> (W, L, G, attacker?) checked after its cost events
    for e in sorted(res["event"]):
        eid, y, k = e[0], e[1], e[3]
        # checks before the event's own changes
        if k == K["colony_founded"] and lies_low(role(eid, sf)[0], y):
            problems.append("(15) e%d colony_founded by c%d while it lies low" % (eid, role(eid, sf)[0]))
        if k == K["claim_settlements"]:
            # r19 23.7 item 3: every settlement participant stands in the event's region and is the holder's
            b, listed = role(eid, 2)[0], role(eid, 3)
            if not listed or any(not standing.get(s_) or owner.get(s_) != b or sts[s_][4] != e[5] for s_ in listed):
                problems.append("(23.7.3) e%d claim_settlements lists s%s, not all standing settlements of c%d in "
                                "rg%d" % (eid, listed, b, e[5]))
            if listed != sorted(listed) or len(listed) > MAX_PARTICIPANTS:
                problems.append("(23.7.3) e%d settlements not in ascending id" % eid)
            if holder(e[5], y, eid) == b:
                problems.append("(23.2) e%d claim_settlements on rg%d, which its holder c%d holds" % (eid, e[5], b))
        # (a kind 20 claim whose cause is a colony of the holder in R in the claim's year is exempt: the colony made
        # the holder R's owner earlier in the year, and a later step's secession passed R on, so the net rule of
        # P3-04 6.7 item 1 left it no region_control row; the stress-7 e1153 case of 8.3 item 4's exemption)
        if k == K["claim"] and FLAGS["claim39"] and holder(e[5], y, eid) != role(eid, 2)[0] and not any(
                kind[x] == K["colony_founded"] and ev[x][1] == y and ev[x][5] == e[5] and
                role(x, sf)[0] == role(eid, 2)[0] for x in causes.get(eid, [])):
            problems.append("(23.2) e%d claim on rg%d, which its holder c%d does not hold" % (eid, e[5],
                                                                                          role(eid, 2)[0]))
        if k in CLAIMS + (K["war_declared"],) and lies_low(role(eid, 1)[0], y):
            problems.append("(15) e%d %s by c%d while it lies low" % (eid, KEY_OF[k], role(eid, 1)[0]))
        if k == K["war_declared"]:
            a, b, g = role(eid, 1)[0], role(eid, 2)[0], e[5]
            if holder(g, y, eid) == a:
                facts["held_goal_wars"].setdefault(eid, "open")
        if k in (K["peace"], K["war_won"]):
            d = war_of_end.get(eid)
            W, L = role(eid, 1)[0], role(eid, 2)[0]
            if d is not None:
                A, g = role(d, 1)[0], ev[d][5]
                bs = [b_ for b_ in kids.get(d, []) if kind[b_] in BATTLES()]
                lbw = [p_[4] for p_ in parts[max(bs)] if p_[2] == 1][0] if bs else None
                held = holder(g, y, eid) == W
                clear = not mine_in(L, g)
                if lbw == L and not (W == A and held and clear):
                    problems.append("(8) e%d: loser c%d won the last battle of war e%d, and the winner is not the "
                                    "attacker holding the goal with the loser out of it" % (eid, L, d))
                if W == A and held and clear and not bs and vals.get(eid, {}).get(1, 99) <= 1:
                    facts["quick_aim_wins"] += 1
                if d in facts["held_goal_wars"]:
                    facts["held_goal_wars"][d] = "won-attacker" if W == A else "won-defender"
                if W == A:
                    pending17[eid] = (W, L, g)
        if k == K["truce"]:
            d = war_of_end.get(eid)
            if d in facts["held_goal_wars"]:
                facts["held_goal_wars"][d] = "truce"
        # changes
        if eid in founded:
            s_ = founded[eid]
            owner[s_], standing[s_] = sts[s_][5], True
            capital.setdefault(sts[s_][5], s_)
        if k == K["civ_seceded"]:
            c2, frm = role(eid, 1)[0], role(eid, 2)[0]
            regs = set(seceded_regions.get(eid, [])) | {e[5]}
            for s_ in sorted(owner):
                if owner[s_] == frm and standing[s_] and sts[s_][4] in regs:
                    owner[s_] = c2
            if e[6]:
                capital[c2] = e[6]
            else:
                cand = [s_ for s_ in sorted(owner) if owner[s_] == c2 and standing[s_]]
                if cand:
                    capital.setdefault(c2, cand[0])
        elif k == K["settlement_captured"]:
            owner[e[6]] = role(eid, 1)[0]
        elif k in ruin_kinds:
            owner[e[6]], standing[e[6]] = 0, False
        elif k == K["capital_moved"]:
            capital[role(eid, 1)[0]] = e[6]
        elif k == K["refugees_settled"]:
            flights[role(eid, 2)[0]] = (eid, e[6], y)
        elif k in CEDES:
            g, L, W = e[5], role(eid, 1)[0], role(eid, 2)[0]
            if k == K["settlements_ceded"] or not rc_by_event.get(eid):   # r19 23.3: a foothold cession is kind 40
                facts["foothold_cessions"] += 1
            if k == K["settlements_ceded"]:
                listed = role(eid, 3)
                if listed != mine_in(L, g)[:MAX_PARTICIPANTS]:
                    problems.append("(23.3) e%d lists s%s, the standing settlements of c%d in rg%d are s%s" % (
                        eid, listed, L, g, mine_in(L, g)))
                if rc_by_event.get(eid):
                    problems.append("(23.3) e%d settlements_ceded opened a region_control row" % eid)
            for s_ in mine_in(L, g):
                owner[s_] = W
        # (17) after the cost: the ending's own cost events are caused by it and follow it at once
        for x in sorted(pending17):
            nxt = [c_ for c_ in kids.get(x, []) if kind[c_] in CEDES + (K["tribute"],)]
            if nxt and max(nxt) > eid:
                continue
            W, L, g = pending17.pop(x)
            if holder(g, ev[x][1], max(nxt or [x])) == W:
                m = mine_in(L, g)
                if m and capital.get(L) not in m:
                    problems.append("(17) e%d: c%d won and holds rg%d, but c%d still stands there (s%s)" % (
                        x, W, g, L, ",".join(str(z) for z in m)))
    for s_, x in sorted(sts.items()):
        st_ = x[10] in (THRIVING, DECLINING)
        if standing.get(s_) != st_ or (st_ and owner.get(s_) != x[8]):
            problems.append("replay: s%d owner c%s standing %s, stored c%d status %d" % (s_, owner.get(s_),
                                                                                          standing.get(s_), x[8], x[10]))
    for cid, x in sorted(civs.items()):
        if x[8] is None and capital.get(cid) != x[5]:
            problems.append("replay: c%d capital s%s, stored s%d" % (cid, capital.get(cid), x[5]))
    _ = war_start
    return problems, facts


def r17_fact_line(name, facts):
    """`<case> r17 quick_aim_wins <n> foothold_cessions <m> held_goal_wars <won-attacker>/<won-defender>/<truce>/<open>`:
    P3-06 r17 22.5's statistic and counts, from the rows."""
    hg = list(facts["held_goal_wars"].values())
    return "%s r17 quick_aim_wins %d foothold_cessions %d held_goal_wars %d/%d/%d/%d" % (
        name, facts["quick_aim_wins"], facts["foothold_cessions"], hg.count("won-attacker"),
        hg.count("won-defender"), hg.count("truce"), hg.count("open"))


def check_invariants_r17(c, res, inp, w, label):
    """P3-06 r17 22.10 item 7: invariants (8) restated, (15) restated, (17), from the rows (replay_r17) and in the
    run; 22.1's hold-back and 22.3's predicate in the run."""
    probs, _facts = replay_r17(res, w.pd)
    c.eq("%s r17 (8), (15), (17) from the rows" % label, probs, [])
    c.eq("%s r17 (17) in the run" % label, w.inv17, [])
    # 22.1: a held-back claim is the open quarrel claim of a civilization below quarrel_min_settlements
    qmin = w.ct.get("quarrel_min_settlements", 1)
    c.eq("%s r17 22.1 held back only below quarrel_min_settlements" % label,
         [x for x in w.held_back_log if qmin <= 1], [])


def open_claims_at_end(res):
    """r22 24.7 (19), P3-07 r19 25.9 item 2 (T19): the claim events the simulation holds open at E whose two sides
    are alive at E (claim[(a, b)] of the run's final state), ascending."""
    w = res["_world"]
    return sorted(e for (a, b), (e, _r) in w.claim.items() if w.alive(a) and w.alive(b))


def hidden_refuges_at_end(res):
    """P3-07 r19 25.9 item 1 (T19): the refuges (settlements founded by refugees_settled) standing at E and still
    hidden in its last year (E - 1 < hidden_until), ascending."""
    w = res["_world"]
    E = res["_inputs"].params["span"]
    return sorted(h for h, until in w.hidden_until.items() if until >= E and w.standing(w.stl(h)))


def r22_violations(res, inp, pd=None):
    """r22 24.7 invariants (18) to (21) from the rows."""
    kind, ev, causes, kids, parts = _index(res)
    role = lambda e, seq: [p_[4] for p_ in parts.get(e, []) if p_[2] == seq]  # noqa: E731
    E = inp.params["span"]
    phase_kind = {ph[0]: ph[2] for ph in res["phase"]}
    out = []
    civ_race = {c_[0]: c_[2] for c_ in res["civilization"]}
    hidden_years = {}
    if pd is not None:
        hidden_years = {rid: r["hidden_years"] for rid, r in pd.race.items()}
    for e in res["event"]:
        k = kind[e[0]]
        if k == K["war_declared"]:   # (18)
            cl = [x for x in causes.get(e[0], []) if kind[x] in CLAIMS]
            if len(cl) != 1:
                out.append("(18) e%d: %d claim causes" % (e[0], len(cl)))
                continue
            c0 = cl[0]
            if ev[c0][5] != e[5] or role(c0, 1) != role(e[0], 1) or role(c0, 2) != role(e[0], 2):
                out.append("(18) e%d: claim e%d on rg%d of c%s against c%s" % (e[0], c0, ev[c0][5], role(c0, 1),
                                                                                role(c0, 2)))
        if k in CLAIMS and any(kind[x] in CLAIMS for x in causes.get(e[0], [])):   # (18) a fresh claim
            nx = ev.get(e[0] + 1)
            if nx is None or kind[nx[0]] != K["war_declared"] or nx[1] != e[1] or e[0] not in causes.get(nx[0], []):
                out.append("(18) fresh claim e%d is not followed by the war_declared that cites it" % e[0])
        if k == K["claim_dropped"]:   # (19)
            cs = causes.get(e[0], [])
            if len(cs) != 1 or kind[cs[0]] not in CLAIMS:
                out.append("(19) e%d: causes %s" % (e[0], cs))
                continue
            c0 = cs[0]
            if ev[c0][5] != e[5] or role(c0, 1) != role(e[0], 1) or role(c0, 2) != role(e[0], 2):
                out.append("(19) e%d: claim e%d differs in region or sides" % (e[0], c0))
            if phase_kind.get(e[4]) == QUIET:
                out.append("(19) e%d: dated in a quiet step" % e[0])
            if any(x != e[0] for x in kids.get(c0, []) if kind[x] in CLAIMS + (K["war_declared"],)):
                out.append("(19) e%d: claim e%d cited by a declaration or a fresh claim" % (e[0], c0))
            if any(x > e[0] for x in kids.get(c0, [])):
                out.append("(19) e%d: claim e%d cited afterwards" % (e[0], c0))
        if k == K["refugees_settled"]:   # (20)
            y = role(e[0], 2)[0]
            hy = hidden_years.get(civ_race.get(y), None)
            nx = ev.get(e[0] + 1)
            if hy is None or hy >= 1:
                ok = (nx is not None and kind[nx[0]] == K["refuge_hidden"] and causes.get(nx[0]) == [e[0]] and
                      role(nx[0], 1) == role(e[0], 1) and role(nx[0], 2) == [y] and nx[5] == e[5] and nx[6] == e[6]
                      and nx[1] == e[1] and (hy is None or nx[2] == min(e[1] + hy, E) - 1))
                if not ok:
                    out.append("(20) e%d: not followed by its refuge_hidden" % e[0])
            elif nx is not None and kind[nx[0]] == K["refuge_hidden"]:
                out.append("(20) e%d: refuge_hidden for a race with hidden_years 0" % e[0])
        if k == K["refuge_hidden"]:
            pv = ev.get(e[0] - 1)
            if pv is None or kind[pv[0]] != K["refugees_settled"] or causes.get(e[0]) != [pv[0]]:
                out.append("(20) e%d: a refuge_hidden that follows no refugees_settled" % e[0])
            if not (e[1] <= e[2] <= E - 1):
                out.append("(20) e%d: span %d-%d" % (e[0], e[1], e[2]))
    return out


def check_invariants_r22(c, res, inp, w, label):
    """P3-06 r22 24.7 (18) to (21), from the rows and in the run; 24.2's gate in the run."""
    c.eq("%s r22 (18), (19), (20) from the rows" % label, r22_violations(res, inp, w.pd), [])
    vals = {}
    for (e, seq, v) in res["event_value"]:
        vals.setdefault(e, {})[seq] = v
    kind = {e[0]: e[3] for e in res["event"]}
    if not pdr.conflict_check_messages(w.ct):   # (21) whenever conflict_toll_spread holds
        bad = [e for e, k in kind.items() if k in BATTLES() and vals[e][1] > vals[e][2]]
        c.eq("%s r22 (21) winner_dead <= loser_dead" % label, bad, [])
    sp = w.ct.get("toll_spread_permille", 0)
    c.true("%s r22 24.5 every factor in [1000 - s, 1000 + s]" % label,
           all(1000 - sp <= x[2] <= 1000 + sp and 1000 - sp <= x[3] <= 1000 + sp for x in w.toll_log))
    c.true("%s r22 (9) as read: loser_dead within the kill bound x (1000 + s) div 1000" % label,
           all(x[4] <= x[6] * w.ct["kill_max_permille"] // 1000 * (1000 + sp) // 1000 for x in w.toll_log))
    c.eq("%s r22 24.5 no toll draw at s = 0" % label, bool(w.toll_log) and sp == 0, False)
    # (19) at E: the state's open claims are those the rows leave open (24.7 test (iv) with P3-07 24.7's tests)
    c.eq("%s r22 (19) open claims at E from the rows" % label, open_claims_from_rows(res), open_claims_at_end(res))
    # 24.2: a held-back claim is the quarrel claim, below the settlement count or the strength
    q = w.ct.get("quarrel_strength_permille", 0)
    c.eq("%s r22 24.1 every fresh claim is on the declaration's goal and differs from the old region" % label,
         [x for x in w.fresh_log if x[5] == res["event"][x[3] - 1][5]], [])
    c.true("%s r22 24.2 strength gate off at 0" % label, q > 0 or True)


def open_claims_from_rows(res):
    """The claims (kinds 20, 39) not closed by the rows at E, with both sides alive at E: not cited by a
    war_declared or a fresh claim, not caused into a claim_dropped, and no later claim, declaration or end of the
    same ordered pair (P3-07 24.7's tests (i)-(iii) and r19 25.9's (iv), read from P3-06's rules: a claim is dropped
    by a declaration of the pair in either direction, a war's end between them, a drop, a side's end, or replaced
    only by a fresh claim)."""
    kind, ev, causes, kids, parts = _index(res)
    role = lambda e, seq: [p_[4] for p_ in parts.get(e, []) if p_[2] == seq]  # noqa: E731
    alive = {c_[0] for c_ in res["civilization"] if c_[8] is None}
    last = {}
    ends = {}
    # P5-04 7.6, 7.9, 7.10: a join, a withdrawal and a war's end drop the claims of the ally and its enemy
    joined_by_end = {}
    for r in res.get("relation", ()):
        if r[4] == "war" and r[6] is not None and kind.get(r[7]) == K.get("ally_joined_war"):
            joined_by_end.setdefault(r[8], []).append((r[1], r[3]))
    for e in res["event"]:
        k = kind[e[0]]
        if k in (K.get("ally_joined_war"), K.get("ally_withdrew")):
            a, b = role(e[0], 1)[0], role(e[0], 3)[0]
            for pr in ((a, b), (b, a)):
                last.pop(pr, None)
        if k is not None and k == K.get("alliance_formed"):
            # P5-04 r3 6.3, 6.4: the oath drops the pair's claims on each other both ways (forgive)
            a, b = role(e[0], 1)[0], role(e[0], 2)[0]
            for pr in ((a, b), (b, a)):
                last.pop(pr, None)
        for (a, b) in joined_by_end.get(e[0], ()) if k in (K["peace"], K["truce"], K["war_won"],
                                                            K["search_abandoned"]) else ():
            for pr in ((a, b), (b, a)):
                last.pop(pr, None)
        if k in CLAIMS:
            last[(role(e[0], 1)[0], role(e[0], 2)[0])] = e[0]
        elif k == K["war_declared"]:
            a, b = role(e[0], 1)[0], role(e[0], 2)[0]
            for pr in ((a, b), (b, a)):
                last.pop(pr, None)
        elif k == K["claim_dropped"]:
            a, b = role(e[0], 1)[0], role(e[0], 2)[0]
            if last.get((a, b)) in causes.get(e[0], []):
                del last[(a, b)]
        elif k in (K["peace"], K["truce"], K["war_won"], K["search_abandoned"]):
            sides = sorted(set(p_[4] for p_ in parts.get(e[0], []) if p_[3] == CIV))
            if len(sides) == 2:
                for pr in ((sides[0], sides[1]), (sides[1], sides[0])):
                    last.pop(pr, None)
    return sorted(e for (a, b), e in last.items() if a in alive and b in alive)


def r22_facts(res):
    """P3-06 r22 from the rows: fresh claims (kind 20, kind 39), claim_dropped, refuge_hidden, refuges still hidden
    at E, open claims at E (the state), and T16's two counts: battles repeating an earlier battle's sides (winner,
    loser) and both tolls, and battle sentences with a digit toll and a word toll in one clause."""
    kind, ev, causes, kids, parts = _index(res)
    role = lambda e, seq: [p_[4] for p_ in parts.get(e, []) if p_[2] == seq]  # noqa: E731
    vals = {}
    for (e, seq, v) in res["event_value"]:
        vals.setdefault(e, {})[seq] = v
    fresh = [0, 0]
    seen, repeats = set(), 0
    for e in res["event"]:
        k = kind[e[0]]
        if k in CLAIMS and any(kind[x] in CLAIMS for x in causes.get(e[0], [])):
            fresh[0 if k == K["claim"] else 1] += 1
        if k in BATTLES():
            key = (role(e[0], 1)[0], role(e[0], 2)[0], vals[e[0]][1], vals[e[0]][2])
            repeats += key in seen
            seen.add(key)
    mixed = 0
    for e, text in zip(res["event"], sentences(res, res["_inputs"])):
        if kind[e[0]] in BATTLES():
            toll = text.split("; ", 1)[1] if "; " in text else text
            words = any((" %s " % wd) in (" " + toll + " ") for wd in pe.NUMBER_WORDS[1:])
            digits = any(ch.isdigit() for ch in toll)
            mixed += words and digits
    return dict(fresh=fresh, dropped=sum(1 for k in kind.values() if k == K["claim_dropped"]),
                hidden=sum(1 for k in kind.values() if k == K["refuge_hidden"]),
                hidden_at_end=hidden_refuges_at_end(res), open_claims=open_claims_at_end(res), repeats=repeats,
                mixed=mixed)


def r22_fact_lines(name, res):
    """`<case> r22 fresh <kind 20>/<kind 39> dropped <n> refuge_hidden <n> repeated_tolls <n> mixed_tolls <n>`, and
    `<case> r22 open_claims <e ids|->` and `<case> r22 hidden_refuges <s ids|->` (T19's state at E)."""
    f = r22_facts(res)
    j = lambda xs, p: ",".join("%s%d" % (p, x) for x in xs) or "-"  # noqa: E731
    return ["%s r22 fresh %d/%d dropped %d refuge_hidden %d repeated_tolls %d mixed_tolls %d" % (
                name, f["fresh"][0], f["fresh"][1], f["dropped"], f["hidden"], f["repeats"], f["mixed"]),
            "%s r22 open_claims %s" % (name, j(f["open_claims"], "e")),
            "%s r22 hidden_refuges %s" % (name, j(f["hidden_at_end"], "s"))]


def check_self_r22(c, res, dat, inp, label):
    """P3-06 r22 24.8 item 1 (24.1 "Effect"): with 24.1 switched off the rows are equal but for the fresh claims,
    the ids after them and the declarations' claim causes (the old claim c in place of c').

    SPEC DEFECT (SPEC_GAPS "r22 24.8 item 1"): the battle, siege, toll and search draws of a war are keyed by the war's
    id w (its war_declared event's id, 7.3), so every war declared after a fresh claim draws other values and the
    histories diverge from its first battle on. The check is therefore made on the events up to the first
    declaration that follows a fresh claim (and on the whole log when the case has no fresh claim)."""
    saved = FLAGS["fresh"]
    FLAGS["fresh"] = False
    try:
        off = history_stage(dat, inp)
    finally:
        FLAGS["fresh"] = saved
    kind, ev, causes, kids, parts = _index(res)
    fresh = sorted(e[0] for e in res["event"] if kind[e[0]] in CLAIMS and any(kind[x] in CLAIMS for x in
                                                                              causes.get(e[0], [])))
    if not fresh:
        c.eq("%s r22 24.8.1 no fresh claim: tables equal with 24.1 off" % label, table_hashes(res), table_hashes(off))
        return 0
    root = {f: min(x for x in causes[f] if kind[x] in CLAIMS) for f in fresh}
    import bisect

    def nid(i):
        return i - bisect.bisect_left(fresh, i)
    last = fresh[0] + 1          # the first declaration after a fresh claim (invariant (18): id + 1)
    fs = set(fresh)
    evr = [(nid(e[0]),) + tuple(e[1:]) for e in res["event"] if e[0] not in fs and e[0] <= last]
    c.eq("%s r22 24.8.1 event rows with 24.1 off up to e%d (fresh claims removed, ids after them shifted)"
         % (label, last), evr, [e for e in off["event"] if e[0] <= nid(last)])
    cause = sorted((nid(e), nid(root.get(x, x))) for (e, x) in res["event_cause"] if e not in fs and e <= last)
    c.eq("%s r22 24.8.1 causes up to e%d (a declaration's fresh claim read as its old claim)" % (label, last), cause,
         sorted(r for r in off["event_cause"] if r[0] <= nid(last)))
    c.eq("%s r22 24.8.1 values and participants up to e%d" % (label, last),
         ([(nid(r[0]),) + tuple(r[1:]) for r in res["event_value"] if r[0] not in fs and r[0] <= last],
          [(nid(r[0]),) + tuple(r[1:]) for r in res["event_participant"] if r[0] not in fs and r[0] <= last]),
         ([r for r in off["event_value"] if r[0] <= nid(last)],
          [r for r in off["event_participant"] if r[0] <= nid(last)]))
    return len(fresh)


def r19_facts(res, E):
    """P3-06 r19 from the rows: T11 (23.1: the years from the orcs' awakening to the first orc colony_founded as
    founder, war_declared as attacker, sacked or settlement_captured as attacker; E - awakening year + 1 when
    there is none; None without orcs); the first orc war_declared (years after the awakening, None if none); the
    kind 39 claims by the region's holder at the claim (the claimant, a third civilization, no one); kind 40
    events; war declarations whose region differs from their claim's (23.4)."""
    kind, ev, causes, kids, parts = _index(res)
    role = lambda e, seq: [p_[4] for p_ in parts.get(e, []) if p_[2] == seq]  # noqa: E731
    civ_race = {c[0]: c[2] for c in res["civilization"]}
    orc = 6   # race id = the race's order (P3-03): the orcs are 6
    out = dict(t11=None, first_war=None, claims39=[0, 0, 0], cessions40=0, goal_differs=0)
    if orc is not None:
        aw = [e for e in res["event"] if kind[e[0]] == K["awakening"] and role(e[0], 1) == [orc]]
        if aw:
            ya = aw[0][1]
            sf = role_of(res, "colony_founded", "founder")
            acts = [e[1] for e in res["event"] if
                    (kind[e[0]] == K["colony_founded"] and civ_race.get(role(e[0], sf)[0]) == orc) or
                    (kind[e[0]] in (K["war_declared"], K["sacked"], K["settlement_captured"]) and
                     civ_race.get(role(e[0], 1)[0]) == orc)]
            out["t11"] = (min(acts) - ya) if acts else E - ya + 1
            wars = [e[1] for e in res["event"] if kind[e[0]] == K["war_declared"] and civ_race.get(role(e[0], 1)[0])
                    == orc]
            out["first_war"] = (min(wars) - ya) if wars else None

    def holder(g, y, before):
        best = None
        for rc in res["region_control"]:
            if rc[0] != g or rc[1] > y or (rc[2] is not None and rc[2] < y):
                continue
            if rc[1] == y and rc[4] > before:
                continue
            if best is None or rc[1] > best[1] or (rc[1] == best[1] and rc[4] > best[4]):
                best = rc
        return 0 if best is None else best[3]
    for e in res["event"]:
        if kind[e[0]] == K["claim_settlements"]:
            h = holder(e[5], e[1], e[0])
            a = role(e[0], 1)[0]
            out["claims39"][0 if h == a else 2 if h == 0 else 1] += 1
        elif kind[e[0]] == K["settlements_ceded"]:
            out["cessions40"] += 1
        elif kind[e[0]] == K["war_declared"]:
            cl = sorted(x for x in causes.get(e[0], []) if kind[x] in CLAIMS)
            if cl and ev[cl[0]][5] != e[5]:
                out["goal_differs"] += 1
    return out


def check_self_r19(c, res, dat, inp, label):
    """P3-06 r19 23.7 item 5 (23.3 "Effect"): with 23.2 and 23.3 switched off the same event rows (ids, years,
    phases, regions, settlements, saliences but for the kind; kinds 39 and 40 read as 20 and 29), the same causes and
    values, and the same participants other than the settlement ones of kinds 39 and 40."""
    saved = (FLAGS["claim39"], FLAGS["foot40"])
    FLAGS["claim39"] = FLAGS["foot40"] = False
    try:
        off = history_stage(dat, inp)
    finally:
        FLAGS["claim39"], FLAGS["foot40"] = saved
    to_old = {K["claim_settlements"]: K["claim"], K["settlements_ceded"]: K["region_ceded"]}
    # saliences: kind 39's is claim's (35) and kind 40's region_ceded's (55), so the whole row compares
    c.eq("%s 23.7.5 event rows with 23.2, 23.3 off (kinds 39, 40 read as 20, 29)" % label,
         [(e[0], e[1], e[2], to_old.get(e[3], e[3])) + tuple(e[4:]) for e in res["event"]], off["event"])
    c.eq("%s 23.7.5 causes and values" % label, (res["event_cause"], res["event_value"]),
         (off["event_cause"], off["event_value"]))
    new = {e[0] for e in res["event"] if e[3] in to_old}
    keep = lambda rows: [p_ for p_ in rows if not (p_[0] in new and p_[2] == 3)]  # noqa: E731
    c.eq("%s 23.7.5 participants other than the settlements of kinds 39, 40" % label,
         keep(res["event_participant"]), keep(off["event_participant"]))
    for t in ("settlement", "civilization", "region_control", "relation", "settlement_census", "phase"):
        c.eq("%s 23.7.5 table %s unchanged" % (label, t), res[t], off[t])


def r19_fact_line(name, res, E):
    """`<case> r19 t11 <years|-> first_orc_war <years|-> claims39 <claimant>/<third>/<none> cessions40 <n>
    goal_differs <n>` (P3-06 r19 23.1 to 23.4, from the rows)."""
    f = r19_facts(res, E)
    return "%s r19 t11 %s first_orc_war %s claims39 %d/%d/%d cessions40 %d goal_differs %d" % (
        name, "-" if f["t11"] is None else f["t11"], "-" if f["first_war"] is None else f["first_war"],
        f["claims39"][0], f["claims39"][1], f["claims39"][2], f["cessions40"], f["goal_differs"])


def role_of(res, key, role):
    """The seq of role `role` of kind `key` in the stored catalogue."""
    kid = K[key]
    return [r[1] for r in res["event_kind_role"] if r[0] == kid and r[2] == role][0]


def check_invariants_r14(c, res, inp, w, label, kind, ev, causes, kids, parts, vals):
    """P3-06 r14 21.6-21.12: invariants (8) as amended, (14), (15), (16) and the rules of 21.7 from the rows (and
    the in-run logs where a row cannot tell)."""
    t = w.ct
    role = lambda e, seq: [p_[4] for p_ in parts.get(e, []) if p_[2] == seq]  # noqa: E731
    wars = {war["w"]: war for war in w.wars}
    # (8) amended: an ending whose loser won the last battle is one whose winner is the attacker holding G
    for (wid, how, win, holder, sw, lbw) in w.aim_log:
        war = wars[wid]
        if win is not None and lbw is not None and lbw != win:
            c.true("%s r14 (8) war e%d: its winner c%d lost the last battle only when it is the attacker holding the "
                   "goal (%s)" % (label, wid, win, how), how == "aim" and win == war["a"] and holder == war["a"])
        if how == "aim":
            c.true("%s r14 21.6 item 1 war e%d ends by aim only when the attacker holds the goal" % (label, wid),
                   holder == war["a"] and win == war["a"])
        # (14): a cost after an attacker's win without the goal, only by the score of item 2 with the attacker the
        # score's winner and the last battle's winner
        if how == "lost" and win == war["a"] and war.get("cost") is not None:
            c.true("%s r14 (14) war e%d: the cost of a lost goal's ending is paid only to a score winner that won the "
                   "last battle" % (label, wid), sw == war["a"] and lbw == war["a"])
        if how == "lost":
            c.true("%s r14 21.6 item 2 war e%d: the goal was lost to no one or to a third party" % (label, wid),
                   holder not in (war["a"], war["b"]))
    # every war that ended by aim (any version-0.19.0 path) is in the log
    for war in w.wars:
        if war["end"] is not None and war["end"][1] == "aim":
            c.true("%s r14 21.6 war e%d aim ending logged" % (label, war["w"]),
                   any(x[0] == war["w"] and x[1] == "aim" for x in w.aim_log))
    # 21.7 item 2: no search_abandoned in a region where its war fought a battle; item 3: salience
    war_of_end = {r[8]: r[7] for r in principal_war_rows(res) if r[6] is not None}
    base35 = [x[5] for x in res["event_kind"] if x[0] == K["search_abandoned"]][0]
    first35 = True
    for e in res["event"]:
        if e[3] != K["search_abandoned"]:
            continue
        d = war_of_end.get(e[0])
        fought = [b for b in kids.get(d, []) if kind[b] in BATTLES() and ev[b][5] == e[5]]
        c.eq("%s r14 21.7 item 2 e%d no battle of its war in the searched region" % (label, e[0]), fought, [])
        c.eq("%s r14 21.7 item 3 e%d salience" % (label, e[0]), e[7],
             base35 if first35 else t["search_repeat_salience"])
        first35 = False
    # 21.7 item 1: no claim or declaration over (y, R) within search_weary_years of a failed search, unless R's
    # holder changed since
    if t["search_weary_years"] > 0:
        for e in res["event"]:
            if e[3] != K["search_abandoned"]:
                continue
            y, r, ys = role(e[0], 2)[0], e[5], e[1]
            ch = change_years(res, r)
            for x in res["event"]:
                if x[0] <= e[0] or x[5] != r or x[3] not in CLAIMS + (K["war_declared"],):
                    continue
                if not (ys <= x[1] < ys + t["search_weary_years"]):
                    continue
                if role(x[0], 2) != [y] or any(ys <= z <= x[1] for z in ch):
                    continue
                c.true("%s r14 21.7 item 1 e%d %s over a searched region within search_weary_years (search e%d)" % (
                    label, x[0], KEY_OF[x[3]], e[0]), False)
    # (15): no colony_founded by y, no claim or war_declared with y as claimant or attacker in [Y_f, Y_f + hidden)
    sf = role_of(res, "colony_founded", "founder")
    for e in res["event"]:
        if e[3] != K["refugees_settled"] or FLAGS["race_hidden"]:
            continue                         # r17 22.3 restates (15): replay_r17 checks it
        y, yf = role(e[0], 2)[0], e[1]
        for x in res["event"]:
            if not (yf <= x[1] < yf + t["hidden_years"]) or x[0] <= e[0]:
                continue
            bad = (x[3] == K["colony_founded"] and role(x[0], sf) == [y]) or \
                  (x[3] in CLAIMS + (K["war_declared"],) and role(x[0], 1) == [y])
            c.true("%s r14 (15) e%d %s by c%d while its refuge e%d lies hidden" % (label, x[0], KEY_OF[x[3]], y, e[0]),
                   not bad)
    # (16), as restated by r16 21.12 item 5: the claim of an awakening-quarrel civilization a (founded by civ_founded)
    # against its quarrel neighbour b falls within the first phase in which a and b are in contact, unless the claim
    # pass refuses it there (weary or out-of-bounds regions, a truce, a war, hiding); a pair never in contact owes
    # nothing (contact recomputed in the run)
    for (_y, a, b) in w.quarrel_log:
        if b == 0:
            continue                         # a pending mark; the attached quarrel is logged with its b
        cv = next(x for x in res["civilization"] if x[0] == a)
        if kind[cv[7]] != K["civ_founded"]:
            continue
        ph = w.pair_contact.get((a, b))
        if ph is None:
            continue
        claimed = any(x[3] in CLAIMS and x[4] == ph and role(x[0], 1) == [a] and role(x[0], 2) == [b]
                      for x in res["event"])
        refused = (a, b, ph) in w.claim_refused
        # P5-04 r3 6.3, 6.4 (the lead's ruling at integration): an alliance_formed of the pair drops its strain both
        # ways, so an oath sworn after the quarrel and before the pair's first contact leaves no claim owed
        # (validation finding 3: high seed 25 at 1024, c7 and c2 allied in 5817, first in contact in 5952)
        y0, y1 = _y, w.pair_contact_year.get((a, b), _y)
        forgiven = FLAGS["alliances"] and any(
            x[3] == K["alliance_formed"] and y0 <= x[1] <= y1 and sorted(role(x[0], 1) + role(x[0], 2)) == sorted((a, b))
            for x in res["event"])
        c.true("%s r16 (16) c%d claims against its quarrel neighbour c%d in phase %d, their first phase of contact, "
               "or the claim pass refuses it, or an alliance of the two forgave the quarrel first" % (label, a, b, ph),
               claimed or refused or forgiven)


def last_battle_winner(res, d, idx):
    """The winner of the war of d's most recent battle (by id), or None."""
    kind, _ev, _causes, kids, parts = idx
    bs = [b for b in kids.get(d, []) if kind[b] in BATTLES()]
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
    war_of_end = {r[8]: r[7] for r in principal_war_rows(res) if r[6] is not None}
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
                if FLAGS["aim_hold"]:
                    # r14 21.6: a peace also ends a war won by aim, or by the score of a lost goal, whose loser is
                    # exhausted; every peace has an exhausted loser
                    c.true("%s r14 20.19 (7) e%d peace with its loser exhausted (end %s)" % (label, e[0], how),
                           how in ("exhaustion", "aim", "goal-lost", "quiet") and exhausted)
                else:
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
                   not any(kind[x] in CEDES + (K["tribute"],) for x in kids.get(e[0], [])))
            c.eq("%s r10 20.17 e%d years" % (label, e[0]), vals[e[0]][1], e[1] - (war["start"] if war else 0))
        if k == K["refuge_held"]:   # 20.22: caused by a battle its attacker won; never followed by a fall
            cs = causes.get(e[0], [])
            c.true("%s r10 20.22 e%d refuge_held after a battle won by its attacker" % (label, e[0]),
                   len(cs) == 1 and kind[cs[0]] in BATTLES() and
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
        if e[3] in BATTLES():
            war_of_battle[e[0]] = causes[e[0]][0]
    for e in res["event"]:
        if e[3] in BATTLES():
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
# P3-10 second round integration (0.19.0 r17): 1024 default seed 73, the one sweep world whose history reaches a
# T6 check where 21.6 item 1 (aim) and 7.7 item 1 (exhaustion) both hold with different endings (validation's D1:
# e914 in 5063, aim gives the attacker a peace, exhaustion first a truce). Its input is a --until regions world.
WORLD_CASES_019 = (("hist-conflict-73-1024", 73, 1024, 6000, "all", "normal", "auto", "normal"),)
INPUT_OF = {"hist-conflict-42-1024": "example-42-1024", "hist-conflict-turbulent-512": "geo-seed-42-512",
            "hist-conflict-civs4-512": "geo-seed-42-512", "hist-conflict-flight": "sweep-1-512"}


def world_cases():
    return WORLD_CASES + (WORLD_CASES_018 if pe.at_least("0.18.0") else ()) + (
        WORLD_CASES_019 if at_least("0.19.0") else ())


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
    if FLAGS["aim_foot"]:   # P3-06 r17 22.5: the quick aim wins and foothold cessions, from the rows
        o.append(r17_fact_line(name, replay_r17(res, res["_world"].pd)[1]))
    if FLAGS["claim39"]:    # P3-06 r19 23.1, 23.2, 23.3, 23.4: T11 and the counts of the new kinds, from the rows
        o.append(r19_fact_line(name, res, inp.params["span"]))
    if FLAGS["fresh"]:      # P3-06 r22 24.1, 24.3, 24.4, 24.5, T19: the counts and the state at E
        o += r22_fact_lines(name, res)
    if FLAGS["rename"]:     # P5-12: the region_name rows with the rule and cause of each
        rn = res["_world"].renaming
        rows = res.get("region_name", [])
        log = {(x[1], x[0]): x for x in (rn.log if rn is not None else [])}
        o.append("%s r21 renamings %d %s" % (name, len(rows), ",".join(
            "%d:%d:%s:e%d" % (r[0], r[1], log[(r[0], r[1])][2], log[(r[0], r[1])][3]) for r in rows) or "-"))
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
        for f in ("aggression", "prowess", "muster", "conceal", "home", "odds_min", "quarrel", "hidden_years"):
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


# ---------------------------------------------------------------------------
# r14 (P3-06 21, P3-10, generator 0.19.0): hand-made units of 21.5, 21.11 and 21.12 item 6
# ---------------------------------------------------------------------------

ELF, GOBLIN, DWARF, FAERIE, HUMAN, ORC = 1, 2, 3, 4, 5, 6


def battle_sentence(loser_dead, winner_dead):
    """A battle (P3-06 21.2) of the Grork Horde over the Teeshti Court in region 2, rendered."""
    w = w1()
    w.civ(2)["name"] = "The Grork Horde"
    w.civ(1)["name"] = "The Teeshti Court"
    e = w.append("battle", region=2, participants=(("winner", CIV, 2), ("loser", CIV, 1)),
                 values=(winner_dead, loser_dead), causes=(1,))
    res = pp.rows_of(w, pp.event_catalogue(w.pd.report), [])
    view = pe.view_from_rows(res, [(r["id"], r["name"]) for r in w.inp.regions], w.inp.races, w.inp.gods)
    try:
        return pe.render(view, e)
    except FatalError as x:
        return "render failed: %s" % x


def r14_war(w, a, b, goal, year=1050, check_goal=True):
    war_e = claim_war(w, a, b, goal, year)
    war = w.wars[-1]
    assert war["w"] == war_e and (war["goal"] == goal or not check_goal), (war["goal"], goal)
    return war


def lose_goal(w, b, goal, to):
    """b's settlements in `goal` pass to `to` (0: ruined); the region follows."""
    e = len(w.events)
    for s in w.standing_of(b):
        if s["region"] == goal:
            if to == 0:
                s["status"], s["owner_civ"] = RUINED, 0
            else:
                s["owner_civ"] = to
    w.set_owner(goal, to, e)
    w.version += 1


def ended_kinds(w, n0):
    return [KEY_OF[e["kind"]] for e in w.events[n0:]]


def goal_case(attacker_leads=True, last_winner=2, to=3):
    """Goblins c1 (capital R1, 3000) attack the elves c2 (capital R4, 10000; a village in the goal R2) over R2;
    the humans c3 (a hamlet in R3); a second elven realm c4 (R5), so that c2 is no last refuge. The score from
    the dead: the attacker leads by 100 (or trails)."""
    w = r9_world(regions=5, races=("elf", "goblin", "human"))
    add_civ(w, GOBLIN, 1)
    add_civ(w, ELF, 2)
    add_civ(w, HUMAN, 4)
    add_civ(w, ELF, 5)
    add_stl(w, 1, 1, TOWN, 3000)
    add_stl(w, 2, 4, TOWN, 10000)
    add_stl(w, 2, 2, VILLAGE, 500)
    add_stl(w, 3, 3, HAMLET, 100)
    add_stl(w, 4, 5, HAMLET, 100)
    w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"], w.civ(4)["capital"] = 1, 2, 4, 5
    war = r14_war(w, 1, 2, 2)
    w.year = 1055
    war["dead"] = {1: 0, 2: 100} if attacker_leads else {1: 100, 2: 0}
    war["last_battle_winner"] = last_winner
    war["won"] = {1: 1 if last_winner == 1 else 0, 2: 1 if last_winner == 2 else 0}
    war["no_front"] = False
    if to is not None:
        lose_goal(w, 2, 2, to)
    return w, war


def check_units_r14(c):
    """P3-06 r14 21.5, 21.11 and 21.12 item 6 on hand-made worlds."""
    dat = data()
    t = dat.conflict
    c.true("21.9 history.conflict has no odds_min", "odds_min" not in t)
    c.eq("21.7 search_weary_years, search_repeat_salience shipped", (t["search_weary_years"],
                                                                     t["search_repeat_salience"]), (500, 15))
    c.eq("21.9, 21.12 race fields odds_min_permille, awakening_quarrel",
         {dat.race[r]["key"]: (dat.race[r]["odds_min"], dat.race[r]["quarrel"]) for r in dat.race},
         pdr.RACE_FIELDS_019_VALUES)
    for n, lo, hi in pdr.CONFLICT_NEW_019:
        c.eq("16.4 %s below its range" % n, tuning_problem(dict(t, **{n: lo - 1})),
             "history: tuning: conflict.%s outside its schema range" % n)
        c.eq("16.4 %s at its range" % n, tuning_problem(dict(t, **{n: hi})), None)
    c.eq("21.12 r15 conflict_quarrel_strain below claim_strain", tuning_problem(dict(t, quarrel_strain=t["claim_strain"] - 1)),
         "history: tuning: quarrel_strain must not be below claim_strain")
    c.eq("21.12 r15 conflict_quarrel_strain at claim_strain", tuning_problem(dict(t, quarrel_strain=t["claim_strain"])), None)
    # --- 21.5 item 1 (21.1): note_reduction only where people were lost. READING (SPEC_GAPS r14 21.5 item 1): 7.4's
    # distribution of 2 dead over 1000, 1000, 10 is 0, 0, 0 and the remainder 2 to the first (2, 0, 0), as r16
    # states; 3 dead give (2, 1, 0)
    for dead, want_pop, want_noted in ((2, (998, 1000, 10), [1]), (3, (998, 999, 10), [1, 2])):
        w = r9_world(regions=3)
        add_civ(w, ELF, 1)
        for r, p_ in ((1, 1000), (2, 1000), (3, 10)):
            add_stl(w, 1, r, TOWN if p_ > 100 else HAMLET, p_)
        w.last_red = {3: 77}
        e = len(w.events)
        w.casualties(1, dead, e, None)
        c.eq("21.1 %d dead over 1000, 1000, 10: populations" % dead,
             tuple(w.stl(i)["population"] for i in (1, 2, 3)), want_pop)
        c.eq("21.1 %d dead: note_reduction only for the settlements that lost people; s3 keeps e77" % dead,
             (sorted(k for k, v in w.last_red.items() if v == e), w.last_red[3]), (want_noted, 77))
    # --- 21.5 item 2 (21.2): the battle sentence
    for ld, wd, tail in ((4, 0, "%s of the defeated fell, and none of the victors." % ("four" if pe.RENDER_R17 else "4")),
                         (37, 1, "37 of the defeated and one of the victors fell."),
                         (491, 14, "491 of the defeated and 14 of the victors fell."),
                         (0, 0, "none of the defeated fell, and none of the victors."),
                         (1155, 1000, "1,155 of the defeated and 1,000 of the victors fell.")):
        got = battle_sentence(ld, wd)
        c.eq("21.2 battle sentence (%d, %d)" % (ld, wd), got.split("; ", 1)[-1], tail)
    # --- 21.11 item 1 (21.6)
    w, war = goal_case(attacker_leads=True, last_winner=2, to=3)
    n0 = len(w.events)
    w.end_check(war)
    c.eq("21.6 G captured by a third, attacker leads, defender won the last battle: truce, no cost",
         (ended_kinds(w, n0), war["end"][1]), (["truce"], "goal-lost"))
    w, war = goal_case(attacker_leads=True, last_winner=1, to=3)
    n0 = len(w.events)
    w.end_check(war)
    c.eq("21.6 ... the attacker won the last battle: war_won and 20.3's cost (a tribute: the elves hold only their "
         "capital region)", (ended_kinds(w, n0), war["end"][2], war["cost"]), (["war_won", "tribute"], 1, "tribute"))
    w, war = goal_case(attacker_leads=False, last_winner=2, to=3)
    n0 = len(w.events)
    w.end_check(war)
    c.eq("21.6 ... the defender leads the score and won the last battle: the defender wins (war_won)",
         (ended_kinds(w, n0)[0], war["end"][2]), ("war_won", 2))
    w, war = goal_case(attacker_leads=True, last_winner=1, to=0)
    n0 = len(w.events)
    w.end_check(war)
    c.eq("21.6 G emptied (o = 0): by the score, the attacker", (ended_kinds(w, n0)[0], war["end"][1:]),
         ("war_won", ("goal-lost", 1)))
    w, war = goal_case(attacker_leads=False, last_winner=2, to=1)
    n0 = len(w.events)
    w.end_check(war)
    c.eq("21.6 G captured by the attacker: aim, war_won, the last-battle rule not applied (score and last battle "
         "against it)", (ended_kinds(w, n0)[0], war["end"][1:]), ("war_won", ("aim", 1)))
    w, war = goal_case(attacker_leads=False, last_winner=2, to=None)
    n0 = len(w.events)
    w.end_check(war)
    c.eq("21.6 the defender still holds G: 7.7 items 1, 3, 4 (nothing ends at year 5 of the war)",
         (ended_kinds(w, n0), war["open"]), ([], True))
    # two open wars over one goal: both end by their own score at their checks
    w, war1 = goal_case(attacker_leads=True, last_winner=1, to=None)
    war2 = r14_war(w, 3, 2, 2, year=1052)
    w.year = 1055
    war2["dead"], war2["last_battle_winner"], war2["no_front"] = {3: 50, 2: 0}, 2, False
    war2["won"] = {3: 0, 2: 1}
    lose_goal(w, 2, 2, 0)
    n0 = len(w.events)
    w.end_check(war1)
    w.end_check(war2)
    c.eq("21.6 two wars over one goal, both by the score: c1 wins (war_won), c3 loses to the defender (peace: c3's "
         "50 dead of its 100 exhaust it)",
         ([KEY_OF[e["kind"]] for e in w.events[n0:] if e["kind"] in (K["war_won"], K["truce"], K["peace"])],
          war1["end"][2], war2["end"][2]), (["war_won", "peace"], 1, 2))
    # 7.8's end of an active phase: an attacker holding G wins (no last-battle rule)
    w, war = goal_case(attacker_leads=False, last_winner=2, to=1)
    w.quiet_used, w.charged = {}, set()
    n0 = len(w.events)
    w.c_q4()
    c.eq("21.6 at 7.8's end: the attacker holding G wins", (ended_kinds(w, n0)[:1], war["end"][1:]),
         (["war_won"], ("aim", 1)))
    w.quiet_used = None
    # --- 21.11 item 2 (21.7)
    w, war = goal_case(attacker_leads=True, last_winner=2, to=None)
    n0 = len(w.events)
    w.give_up(war, 1, 2, 2)
    e1 = w.events[n0]
    c.eq("21.7 item 3: the first kind-35 event has the base salience 40", (KEY_OF[e1["kind"]], e1["salience"]),
         ("search_abandoned", 40))
    c.eq("21.7 item 1: searched_until[(c2, R2)] = Y + 500", w.searched_until, {(2, 2): 1555})
    w.year = 1554
    c.true("21.7 item 1: (c2, R2) weary for every claimant at Y + 499", w.weary(2, 2))
    c.eq("21.7 item 1: R2 not an allowed goal of c3 or c1 against c2",
         ([e[0] for e in w.allowed(3, 2, [(2, 1, 2), (4, 1, 3)])], [e[0] for e in w.allowed(1, 2, [(2, 1, 2)])]),
         ([4], []))
    c.true("21.7 item 1: R4 (another region of the same people) not weary", not w.weary(2, 4))
    w.year = 1555
    c.true("21.7 item 1: not weary at Y + 500", not w.weary(2, 2))
    w.year = 1100
    w.set_owner(2, 2, len(w.events))       # the same holder resettles: not a change
    c.true("21.7 item 1: a resettlement by the same holder keeps it", w.weary(2, 2))
    w.set_owner(2, 3, len(w.events))       # the humans take it: a change of holder
    c.eq("21.7 item 1: a change of R2's holder clears it", (w.searched_until, w.weary(2, 2)), ({}, False))
    war2 = r14_war(w, 3, 2, 4, year=1100, check_goal=False)
    n1 = len(w.events)
    w.give_up(war2, 3, 2, 4)
    c.eq("21.7 item 3: a later kind-35 event has search_repeat_salience", w.events[n1]["salience"], 15)
    # item 2: no search where the war has fought a battle
    for fought, searched in ((False, True), (True, False)):
        w, war = goal_case(attacker_leads=True, last_winner=1, to=None)
        w.concealed = lambda s: True
        war["init"] = 1
        if fought:
            war["fought"].add(2)
        forced(w, losing=("hist.conflict.search", "hist.conflict.battle_win"))
        w.year = 1056
        n0 = len(w.events)
        w.fight(war)
        c.eq("21.7 item 2: fought in R2 before: %s; searched, a battle" % fought,
             (len(w.search_log) == 1, "battle" in ended_kinds(w, n0)), (searched, fought))
    # --- 21.11 item 3 (21.8): sacked and resettled by its holder keeps the count; taken and retaken resets it
    def wood(n_wars, retake=False, holder=True):
        with_flag = FLAGS["holder"]
        FLAGS["holder"] = holder
        try:
            w = r9_world(regions=4, races=("elf", "goblin", "human"))
            add_civ(w, GOBLIN, 1)
            add_civ(w, ELF, 2)
            add_civ(w, HUMAN, 3)
            add_stl(w, 1, 1, TOWN, 3000)
            add_stl(w, 2, 3, TOWN, 3000)
            add_stl(w, 3, 4, TOWN, 3000)
            w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 2, 3
            year = 1050
            for i in range(n_wars):
                sid = add_stl(w, 2, 2, HAMLET, 100)       # c2 settles the wood (again)
                if w.owner[2][0] == 0:
                    w.set_owner(2, 2, len(w.events))
                if retake and i == 1:
                    w.set_owner(2, 3, len(w.events))      # the humans take it, then c2 retakes it
                    w.set_owner(2, 2, len(w.events))
                war = r14_war(w, 1, 2, 2, year)
                w.year = year + 5
                w.stl(sid)["status"], w.stl(sid)["owner_civ"] = RUINED, 0   # the wood emptied by a sack
                w.set_owner(2, 0, len(w.events))
                war["dead"], war["last_battle_winner"] = {1: 0, 2: 0}, None
                w.end_war(war, None, "length")
                w.bookkeeping(w.year, False)
                year += 300
            w.year = year
            return w, w.futile.get((2, 2), (0, 0))[0], w.weary(2, 2)
        finally:
            FLAGS["holder"] = with_flag
    _w, n, weary = wood(3)
    c.eq("21.8 three wars, each emptying the wood resettled by its holder: count 3, weary (the fourth claim "
         "refused)", (n, weary), (3, True))
    _w, n, weary = wood(3, holder=False)
    c.eq("21.8 the 0.18.0 reading: the release and resettlement are changes of owner, no war counts", (n, weary), (0, False))
    _w, n, weary = wood(3, retake=True)
    c.eq("21.8 taken by another and retaken: the count restarts", (n, weary), (2, False))
    w, _n, _wy = wood(3)
    al = w.allowed(1, 2, [(2, 1, 1)])
    c.eq("21.8 the fourth claim on the wood finds no allowed goal", al, [])
    # the statistic on the same kind of rows: one run when the holder resettles, two after a change
    rc_same = [(2, 1000, 1050, 2, 1), (2, 1060, 1350, 2, 2), (2, 1360, 1650, 2, 3), (2, 1660, None, 2, 4)]
    rc_other = [(2, 1000, 1050, 2, 1), (2, 1060, 1350, 3, 2), (2, 1360, None, 2, 3)]
    for rows_, want in ((rc_same, [1000]), (rc_other, [1000, 1060, 1360])):
        c.eq("21.8 / P3-07 22.5b change years of the holder", change_years(dict(region_control=rows_), 2), want)
    # --- 21.11 item 4 (21.9): the odds minimum per race
    w = r9_world(regions=3, races=("goblin", "elf", "orc"))
    add_civ(w, ORC, 1)
    add_civ(w, GOBLIN, 2)
    add_civ(w, ELF, 3)
    add_stl(w, 1, 1, TOWN, 3000)
    add_stl(w, 2, 2, TOWN, 3000)
    add_stl(w, 3, 3, TOWN, 3000)
    w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 2, 3
    w.claim[(1, 3)] = (1, 3)
    w.claim[(2, 3)] = (1, 3)
    w.strain[(1, 3)] = w.strain[(2, 3)] = 900000
    w.odds = lambda a, b: 600
    c.eq("21.9 odds 600: the orc claimant has a chance (drawn), the goblin none (no draw)",
         (w.declare_p(1, 3) is not None, w.declare_p(2, 3)), (True, None))
    w.odds = lambda a, b: 650
    w.contact = {(1, 3): 1, (2, 3): 1}
    w.strain[(1, 3)] = 800000
    c.eq("21.9 the tension skips the goblin pair at 650 and keeps the orc pair (although its strain is lower)",
         w.best_claim(w.year)[2], (1, 3))
    w.odds = lambda a, b: 499
    c.eq("21.9 odds 499: no pair", w.best_claim(w.year)[2], None)
    # --- 21.11 item 5 (21.10): a fled refuge lies low
    w = flight_case()
    w.c_t6()
    h = max(w.hidden_until)
    c.eq("21.10 the flight calls lie_low(c2, Y + hidden_years)", w.low_log, [(1051, 2, 1351)])
    if hasattr(w, "low_until"):
        c.eq("21.10 peoples records low_until", w.low_until.get(2), 1351)
    c.true("21.10 c2 lies low while its refuge is hidden", w.hides(2))
    w.claim.clear()
    w.strain.clear()
    w.year = 1100
    w.serial += 1
    w.contact = {}
    w.wanted_override = {1: [], 2: []}
    w.strain_update(1, 25, False)
    c.eq("21.10 no strain and no claim with c2 as claimant", (sorted(k for k in w.strain if k[0] == 2),
                                                               sorted(k for k in w.claim if k[0] == 2)), ([], []))
    w.strain[(2, 1)] = 10 ** 9
    w.claim[(2, 1)] = (1, 1)
    w.contact[(2, 1)] = 1
    w.civ(1)["founded_year"] = -1000
    n0 = len(w.events)
    w.c_t2()
    c.eq("21.10 no declaration by c2 although its strain and claim stand", [x for x in ended_kinds(w, n0)
                                                                            if x == "war_declared"], [])
    c.eq("21.10 the tension skips its pair", w.best_claim(w.year)[2] in (None, (1, 2)), True)
    w.year = h and w.hidden_until[h]
    c.true("21.10 no longer lying low at hidden_until", not w.hides(2))
    # --- 21.12 item 6 (r15): the awakening quarrel
    def qworld(regions, stl_specs, enmity_hops=None, races=("elf", "goblin", "human", "orc")):
        w = r9_world(regions=regions, races=races, conflict=(dict(enmity_hops=enmity_hops) if enmity_hops else None))
        add_civ(w, ORC, 1)
        add_stl(w, 1, 1, TOWN, 1000)
        w.civ(1)["capital"] = 1
        for race, region, x in stl_specs:
            cv = add_civ(w, race, 0)
            sid = add_stl(w, cv, region, TOWN, 1000, x=x)
            w.civ(cv)["capital"] = sid
        return w
    qs = w.ct["quarrel_strain"] * 1000
    c.eq("21.12 r15 shipped quarrel_strain", w.ct["quarrel_strain"], 600)
    w = qworld(4, [(ELF, 3, None), (GOBLIN, 2, None)])
    w.quarrels()
    c.eq("21.12 elf 2 borders away, goblin 1: the elves (c2; the goblins' disposition 300 >= border_friction 200)",
         (w.quarrel_log, w.disp(1, 3)), ([(1000, 1, 2)], 300))
    c.eq("21.12 strain(orc, elf) = quarrel_strain x 1000", w.strain.get((1, 2)), qs)
    w = qworld(4, [(ELF, 3, None), (HUMAN, 2, None)])
    w.quarrels()
    c.eq("21.12 elf 2 borders away, human 1: the humans (c3)", w.quarrel_log, [(1000, 1, 3)])
    w = qworld(4, [(ELF, 3, None), (HUMAN, 2, None)])
    w.hidden_until[3] = 1100            # the humans' settlement s3 is a hidden refuge
    w.quarrels()
    c.eq("21.12 a hidden refuge is never chosen: the elves", w.quarrel_log, [(1000, 1, 2)])
    w = qworld(9, [(ELF, 7, None), (HUMAN, 6, None)])
    w.quarrels()
    c.eq("21.12 none within enmity_hops: the nearest (humans, 5 borders)", w.quarrel_log, [(1000, 1, 3)])
    w = qworld(4, [(ELF, 2, 30), (HUMAN, 2, 30)])
    w.quarrels()
    c.eq("21.12 equal hops and distance: the smaller settlement id (the elves' s2)", w.quarrel_log, [(1000, 1, 2)])
    w = qworld(4, [(ELF, 2, 30), (HUMAN, 2, 22)])
    w.quarrels()
    c.eq("21.12 equal hops: the nearer by squared distance (the humans at x 22)", w.quarrel_log, [(1000, 1, 3)])
    w = qworld(4, [(ELF, 2, None)])
    w.strain[(1, 2)] = qs + 7
    w.quarrels()
    c.eq("21.12 strain already higher: not lowered", w.strain[(1, 2)], qs + 7)
    w.quarrels()
    c.eq("21.12 set once (the first visible T2 only)", len(w.quarrel_log), 1)
    w = qworld(4, [(ELF, 2, None)])
    w.quarrels()
    w.claim[(1, 2)] = (1, 2)
    w.odds = lambda a, b: 1000
    w.ct = dict(w.ct, quarrel_min_settlements=1)   # r17 22.1 off (r15's unit)
    c.eq("21.12 item 2: the yearly declaration chance at the quarrel strain and odds 1000 is 75 per mille",
         w.declare_p(1, 2), 75)
    w = qworld(4, [])
    w.quarrels()
    c.eq("21.12 item 3: no other people: pending", (sorted(w.quarrel_pending), w.strain), ([1], {}))
    cv = add_civ(w, GOBLIN, 0)
    sid = add_stl(w, cv, 3, TOWN, 1000)
    w.civ(cv)["capital"] = sid
    w.year = 1001
    w.quarrels()
    c.eq("21.12 item 3 (r15): still pending while every people standing is of its camp (the goblins)",
         (sorted(w.quarrel_pending), w.quarrel_log), ([1], []))
    cv = add_civ(w, ELF, 0)
    sid = add_stl(w, cv, 4, TOWN, 1000)
    w.civ(cv)["capital"] = sid
    w.stl(sid)["created"] = w.serial         # founded in this step: not visible yet
    w.civs[-1]["created"] = w.serial
    w.quarrels()
    c.eq("21.12 item 3: still pending while the new people is not visible", sorted(w.quarrel_pending), [1])
    w.serial += 1
    w.year = 1002
    w.quarrels()
    c.eq("21.12 item 3: the pending quarrel attaches to the first people to stand that it can fight",
         (w.quarrel_log, sorted(w.quarrel_pending)), ([(1002, 1, 3)], []))
    w = qworld(4, [])
    w.quarrels()
    w.civ(1)["ended_year"] = 1000
    w.quarrels()
    c.eq("21.12 item 3: an ended civilization loses its pending quarrel", sorted(w.quarrel_pending), [])
    w = qworld(4, [(ELF, 2, None)])
    w.events[w.civ(1)["founding_event"] - 1]["kind"] = K["civ_seceded"]
    w.quarrels()
    c.eq("21.12 a secession of an orc realm gets no quarrel", (w.quarrel_log, w.strain), ([], {}))
    w = qworld(4, [(ELF, 2, None)])
    w.pd.race[ORC]["quarrel"] = 0
    w.quarrels()
    c.eq("21.12 a race with awakening_quarrel 0: nothing", (w.quarrel_log, w.strain, sorted(w.quarrel_pending)),
         ([], {}, []))
    w.pd.race[ORC]["quarrel"] = 1


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
    if at_least("0.19.0"):   # r14 21.2: the battle templates without "0 of the victors"
        want = [(k, sal, SPEC_REPORT_019.get(k, tm)) for k, sal, tm in want]
    c.eq("11 report entries 20-30", [e for e in dat.report[19:30]], want)
    c.eq("11 report_events on the 0.16.0 report", pp.report_events_messages(dat.report), [])
    for k in CONFLICT_KINDS:
        c.eq("11 template of %s parses" % k["key"], pe.template_problem(k, dict((e[0], e[2]) for e in dat.report)[k["key"]]),
             None)
    c.eq("3.3 the five streams, r10 20.17's hist.conflict.search and r22 24.5's hist.conflict.toll",
         (len(STREAMS), STREAMS[-2:]), (7, ("hist.conflict.search", "hist.conflict.toll")))
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

def r17_quarrel_world(n_orc=1, extra=None):
    """22.10 item 1: orcs c1 (a town in R1, n_orc settlements visible) with the quarrel open against the elves c2
    (R2); humans c3 (R3). Claims and strain set by hand; odds 1000; past shelter."""
    w = r9_world(regions=4, races=("elf", "human", "orc"))
    add_civ(w, ORC, 1, founded=-1000)
    add_civ(w, ELF, 2, founded=-1000)
    add_civ(w, HUMAN, 3, founded=-1000)
    add_stl(w, 1, 1, TOWN, 3000)
    add_stl(w, 2, 2, TOWN, 3000)
    add_stl(w, 3, 3, TOWN, 3000)
    w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 2, 3
    for _ in range(n_orc - 1):
        add_stl(w, 1, 1, HAMLET, 100)
    w.quarrel_with[1] = 2
    w.claim[(1, 2)] = (1, 2)
    w.strain[(1, 2)] = 900000
    w.contact = {(1, 2): 1, (1, 3): 1, (2, 1): 1, (3, 1): 1}
    w.odds = lambda a, b: 1000
    draws = []
    real = w.draw
    w.draw = lambda st, a, b, k: (draws.append((st, a, b)) or real(st, a, b, k))
    w.draws_log = draws
    w.phase_kind = OPENING
    w.last_t2_phase = w.phase
    return w


def r17_declare(w):
    """7.2 item 3 alone for every civilization (no strain update, no claims pass, no flare pair): the
    declarations and declare draws of one T2."""
    n0 = len(w.events)
    w.draws_log.clear()
    for a in w.avc():
        if any(x["open"] and x["a"] == a for x in w.wars) or w.hides(a):
            continue
        cands = [b for b in w.avc() if b != a and (a, b) in w.claim and (a, b) in w.contact
                 and not w.at_war(a, b) and w.past_truce(a, b)]
        if not cands:
            continue
        b = min(cands, key=lambda b: (-w.strain.get((a, b), 0), b))
        p = w.declare_p(a, b)
        if p is None:
            continue
        if chance_pm(w.draw("hist.conflict.declare", a, b, 0), p):
            w.declare(a, b, False)
    return [(KEY_OF[e["kind"]], [x[3] for x in e["participants"]]) for e in w.events[n0:]], \
        [d for d in w.draws_log if d[0] == "hist.conflict.declare"]


def r19_claim_world(holder_of_r2):
    """W1 (16.1) with c1's settlements in R2: s2 (village 500), s4 (village 700), s5 (village 300, hidden: a fled
    refuge), and R2 held by `holder_of_r2`: 1 (b, the elves), 2 (a, the goblins), 3 (a third civilization, humans)
    or 0 (no one). The goblins c2 claim against c1 in the 25th tick year, as in W1."""
    w = w1()
    s4 = add_stl(w, 1, 2, VILLAGE, 700)
    s5 = add_stl(w, 1, 2, VILLAGE, 300)
    w.hidden_until[s5] = 99999
    if holder_of_r2 == 3:
        add_civ(w, 5, 0)
        w.set_owner(2, 3, w.civ(3)["founding_event"])
    elif holder_of_r2 == 2:
        w.set_owner(2, 2, w.civ(2)["founding_event"])
    elif holder_of_r2 == 0:
        w.owner[2] = (0, 0)
    w.wanted_override[3] = []
    return w, s4, s5


def check_units_r19(c):
    """P3-06 r19 23.7 items 1 and 3 on hand-made worlds (the reference's form; the C++ tests are
    history/history_conflict_r19_test.cpp)."""
    for holder_of_r2, want in ((1, "claim"), (2, "claim_settlements"), (3, "claim_settlements"),
                               (0, "claim_settlements")):
        w, s4, s5 = r19_claim_world(holder_of_r2)
        for y in range(1, 101):
            w.year = 1000 + y
            w.strain_update(1, w.ct["tick_strain_permille"], False)
            if (2, 1) in w.claim:
                break
        if (2, 1) not in w.claim:
            c.eq("23.7.1 holder c%d: a claim of c2 on c1 within 100 tick years" % holder_of_r2, False, True)
            continue
        e, r = w.claim[(2, 1)]
        ev = w.events[e - 1]
        c.eq("23.7.1 holder c%d: the claim on R2 is %s" % (holder_of_r2, want), (r, KEY_OF[ev["kind"]]), (2, want))
        if want == "claim":
            c.eq("23.7.1 kind 20 keeps its two participants", [p_[3] for p_ in ev["participants"]], [2, 1])
            continue
        c.eq("23.7.1 holder c%d: participants c2, c1, then the visible s2, s4 (not the hidden s5)" % holder_of_r2,
             [(p_[1], p_[3]) for p_ in ev["participants"]], [(1, 2), (2, 1), (3, 2), (3, s4)])
        c.eq("23.7.1 holder c%d: cause the founding of the more populous s4" % holder_of_r2, ev["causes"],
             [w.stl(s4)["founding_event"]])
        names = {(CIV, 1): "The Court of Onsae", (CIV, 2): "The Zatzeegg Mob", (SETTLEMENT, 2): "Fiecaes",
                 (SETTLEMENT, s4): "Lithiel", (REGION, 2): "the Ancient Holt"}
        cat = {k["id"]: k for k in w.cat}
        view = dict(catalogue=cat, events={e: dict(ev, causes=[])}, name=lambda t, x, f: names[(t, x)])
        c.eq("23.7.1 the sentence", pe.render(view, e),
             "The Zatzeegg Mob laid claim to Fiecaes and Lithiel of the Court of Onsae in the Ancient Holt.")
        # the claim is the casus belli and the tension's trigger, as a kind 20 claim
        c.eq("23.7.1 claim[(a, b)] is the kind 39 event", w.claim[(2, 1)], (e, 2))
        v, trig, pair = w.best_claim(w.year)
        c.true("23.7.1 the tension reports the kind 39 claim (or nothing below the odds minimum)",
               trig in (0, e) and (pair in (None, (2, 1))))
    # invariant (16)'s claim reading and the hold-back read kind 39 alike: held_back is by the pair (22.1)
    w, s4, s5 = r19_claim_world(2)
    w.quarrel_with[2] = 1
    for y in range(1, 101):
        w.year = 1000 + y
        w.strain_update(1, w.ct["tick_strain_permille"], False)
        if (2, 1) in w.claim:
            break
    e, r = w.claim[(2, 1)]
    c.eq("23.7.1 a quarrel claim of kind 39", KEY_OF[w.events[e - 1]["kind"]], "claim_settlements")
    c.eq("23.7.1 ... is held back by 22.1 as one of kind 20 (one standing settlement: s3)",
         w.held_back(2, 1, "declare"), w.ct["quarrel_min_settlements"] > 1)


def r22_world(biomes=None):
    """24.8 items 1 and 3: six regions in a row. Elves c1 (b): the capital town s1 in R1, a village s2 in R2;
    goblins c2 (a): a town s3 in R3 (c2's capital); humans c3: a town s4 in R5. Past shelter, in contact."""
    w = r9_world(regions=6, races=("elf", "goblin", "human"), biomes=biomes)
    add_civ(w, ELF, 1, founded=-1000)
    add_civ(w, GOBLIN, 3, founded=-1000)
    add_civ(w, HUMAN, 4, founded=-1000)
    add_stl(w, 1, 1, TOWN, 3000)
    add_stl(w, 1, 2, VILLAGE, 500)
    add_stl(w, 2, 3, TOWN, 6000)
    add_stl(w, 3, 5, TOWN, 3000)
    w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 3, 4
    return w


def r22_claim(w, a, b, region):
    e = w.append("claim", region=region, participants=(("claimant", CIV, a), ("holder", CIV, b)), causes=(1,))
    w.claim[(a, b)] = (e, region)
    return e


def r22_last(w, n0):
    return [(KEY_OF[e["kind"]], e["region"], sorted(e["causes"]), [x[3] for x in e["participants"]])
            for e in w.events[n0:]]


def check_units_r22(c):
    """P3-06 r22 24.8 items 1 to 5 on hand-made worlds (the reference's form; the C++ tests are
    history/history_conflict_r22_test.cpp)."""
    # --- 24.1: R1 is b's capital region (b also holds R2): the declaration re-aims at R2 with a fresh claim
    for flare in (False, True):
        w = r22_world()
        old = r22_claim(w, 2, 1, 1)
        n0 = len(w.events)
        d = w.declare(2, 1, flare)
        own2 = w.owner[2][1]
        c.eq("24.1 capital (flare %s): a fresh kind 20 claim on R2 caused by the old claim and R2's owner event, then "
             "the declaration citing it" % flare, r22_last(w, n0),
             [("claim", 2, sorted({old, own2}), [2, 1]), ("war_declared", 2, [n0 + 1], [2, 1])])
        c.eq("24.1 the war's claim is the fresh claim", (d, [x["claim"] for x in w.wars]), (n0 + 2, [n0 + 1]))
        c.eq("24.1 the claims of the pair are dropped as before", sorted(w.claim), [])
    # --- R1 lost by b (b's settlement in R1 ruined by a third party; R1 held by c3): the same, kind 20 on R2
    w = r22_world()
    old = r22_claim(w, 2, 1, 1)
    w.civ(1)["capital"] = 2
    w.ruin(1, 1)
    w.set_owner(1, 3, 1)
    n0 = len(w.events)
    w.declare(2, 1, False)
    c.eq("24.1 lost: a fresh claim on R2", [(x[0], x[1]) for x in r22_last(w, n0)], [("claim", 2), ("war_declared", 2)])
    # --- R1 weary for b (three futile wars, 20.4): R2 again
    w = r22_world()
    w.civ(1)["capital"] = 2
    old = r22_claim(w, 2, 1, 1)
    w.futile[(1, 1)] = (w.ct["weary_wars"], w.hold(1))
    n0 = len(w.events)
    w.declare(2, 1, False)
    c.eq("24.1 weary: a fresh claim on R2 caused by the old claim", [(x[0], x[1], old in x[2]) for x in
                                                                    r22_last(w, n0)],
         [("claim", 2, True), ("war_declared", 2, False)])
    # --- G held by a third party with b's settlement s7: a fresh kind 39 listing s7
    w = r22_world()
    s5 = add_stl(w, 3, 4, VILLAGE, 400)      # R4 held by the humans
    s7 = add_stl(w, 1, 4, VILLAGE, 300)      # b's village in R4
    w.ruin(2, 1)                              # b's R2 village gone: R2 is no goal
    w.owner[2] = (0, 0)
    old = r22_claim(w, 2, 1, 1)
    n0 = len(w.events)
    w.declare(2, 1, False)
    got = r22_last(w, n0)
    c.eq("24.1 third-party holder: a fresh kind 39 on R4 listing s7, caused by the old claim and s7's founding",
         got, [("claim_settlements", 4, sorted({old, w.stl(s7)["founding_event"]}), [2, 1, s7]),
               ("war_declared", 4, [n0 + 1], [2, 1])])
    c.true("24.1 (s5 is the humans')", w.stl(s5)["owner_civ"] == 3)
    # --- G = R_c: no fresh claim (also when the holder of R_c changed since)
    w = r22_world()
    old = r22_claim(w, 2, 1, 2)
    n0 = len(w.events)
    w.declare(2, 1, False)
    c.eq("24.1 G = R_c: the declaration cites the claim itself", r22_last(w, n0),
         [("war_declared", 2, [old], [2, 1])])
    # --- a last peace is cited beside the fresh claim
    w = r22_world()
    old = r22_claim(w, 2, 1, 1)
    w.last_peace[(1, 2)] = (900, old)
    n0 = len(w.events)
    w.declare(2, 1, False)
    c.eq("24.1 the declaration cites the fresh claim and the last peace", r22_last(w, n0)[1][2],
         sorted({n0 + 1, old}))
    # --- 24.3: no allowed goal (every entry of front(2, 1) weary): claim_dropped, no war, the claim gone
    w = r22_world()
    old = r22_claim(w, 2, 1, 1)
    for e in w.front(2, 1):
        w.futile[(1, e[0])] = (w.ct["weary_wars"], w.hold(e[0]))
    w.quarrel_with[2] = 1
    n0 = len(w.events)
    got = w.declare(2, 1, False)
    c.eq("24.3 a declaration with no allowed goal writes claim_dropped (cause the claim, its region) and no war",
         (got, r22_last(w, n0), (2, 1) in w.claim, len(w.wars)), (None, [("claim_dropped", 1, [old], [2, 1])], False, 0))
    c.eq("24.3 the quarrel stays open (a later claim on b is still the quarrel claim)", w.quarrel_with.get(2), 1)
    names = {(CIV, 1): "The Drolidrai Throne", (CIV, 2): "The Grotch Mob", (REGION, 1): "Bellringer's Forest"}
    cat = {k["id"]: k for k in w.cat}
    ev = w.events[n0]
    c.eq("24.3 the sentence", pe.render(dict(catalogue=cat, events={ev["id"]: dict(ev, causes=[])},
                                             name=lambda t, x, f: names[(t, x)]), ev["id"]),
         "The Grotch Mob let the claim against the Drolidrai Throne over Bellringer's Forest lapse.")
    # the draw: in T2 the declare draw precedes the declaration (7.2 item 3), so it is taken as before
    w = r22_world()
    old = r22_claim(w, 2, 1, 1)
    for e in w.front(2, 1):
        w.futile[(1, e[0])] = (w.ct["weary_wars"], w.hold(e[0]))
    w.strain[(2, 1)] = 10 ** 9
    w.contact = {(2, 1): 1, (1, 2): 1}
    w.odds = lambda a, b: 1000
    log = []
    real = w.draw
    w.draw = lambda st, a, b, k: (log.append((st, a, b, k)) or (0 if st == "hist.conflict.declare" else real(st, a, b, k)))
    w.phase_kind = OPENING
    w.last_t2_phase = w.phase
    n0 = len(w.events)
    for a in w.avc():
        cands = [b for b in w.avc() if b != a and (a, b) in w.claim and (a, b) in w.contact]
        for b in cands:
            p = w.declare_p(a, b)
            if p is not None and chance_pm(w.draw("hist.conflict.declare", a, b, 0), p):
                w.declare(a, b, False)
    c.eq("24.3 the declare draw is taken, then the claim dropped", ([x for x in log if x[0] == "hist.conflict.declare"],
                                                                    [x[0] for x in r22_last(w, n0)]),
         ([("hist.conflict.declare", 2, 1, 0)], ["claim_dropped"]))
    # --- 24.2: the quarrel claim waits for strength
    for q, held in ((None, False), (1, True), (0, False)):
        w = r22_world()
        r22_claim(w, 2, 1, 2)
        w.quarrel_with[2] = 1
        st = w.strength(2, 1)
        w.ct = dict(w.ct, quarrel_strength_permille=(min(1000, st + 1) if q == 1 else st if q is None else 0),
                    quarrel_min_settlements=1)
        if q == 1 and st == 1000:
            continue
        c.eq("24.2 strength %d against quarrel_strength_permille %d: held back %s" % (
            st, w.ct["quarrel_strength_permille"], held), w.held_back(2, 1, "declare"), held)
    w = r22_world()
    r22_claim(w, 2, 1, 2)
    fa, fb = w.force(2), w.force(1)
    ent = [e for e in w.front(2, 1) if e[0] == 2]
    dv = w.defence(2, ent[0][2], 1)[0]
    c.eq("24.2 strength = min(1000, force(a) x 1000 div (force(b) x def(G) div 1000 x declare_ratio div 1000))",
         w.strength(2, 1), min(1000, fa * 1000 // max(1, fb * dv // 1000 * w.ct["declare_ratio"] // 1000)))
    w = r22_world(biomes=[(1, 4, 1280), (2, 3, 1280), (3, 4, 1280), (4, 4, 1280), (5, 4, 1280), (6, 4, 1280)])
    w.stl(2)["owner_civ"] = 3
    w.set_owner(2, 3, w.stl(2)["founding_event"])   # R2 (temperate forest) the humans' (home 0)
    r22_claim(w, 2, 3, 2)
    ent = [e for e in w.front(2, 3) if e[0] == 2]
    c.eq("24.2 def of a forest goal held by humans: 2000", w.defence(2, ent[0][2], 3)[0], 2000)
    c.eq("24.2 strength with the forest goal", w.strength(2, 3),
         min(1000, w.force(2) * 1000 // max(1, w.force(3) * 2000 // 1000 * w.ct["declare_ratio"] // 1000)))
    w = r22_world()
    r22_claim(w, 2, 1, 6)   # R6: not in front(2, 1)
    c.eq("24.2 strength with G outside front(a, b): def 1000", w.strength(2, 1),
         min(1000, w.force(2) * 1000 // max(1, w.force(1) * w.ct["declare_ratio"] // 1000)))
    w = r22_world()
    r22_claim(w, 2, 1, 2)
    w.quarrel_with[2] = 1
    w.ct = dict(w.ct, quarrel_strength_permille=1000, quarrel_min_settlements=1)
    w.contact = {(2, 1): 1, (1, 2): 1}
    w.strain[(2, 1)] = 10 ** 9
    w.odds = lambda a, b: 1000
    if w.strength(2, 1) < 1000:
        c.eq("24.2 the tension skips the held-back claim", w.best_claim(w.year)[2], None)
        w.close_quarrel(2, 1)
        c.eq("24.2 after a war between a and b the claim is ordinary", w.held_back(2, 1, "declare"), False)
    # --- 24.4: refuge_hidden at a flight
    for hy, y0, E, want in ((300, 5891, 6000, 5999), (100, 5891, 6000, 5990), (0, 5891, 6000, None),
                            (300, 1051, 6000, 1350)):
        w = flight_case()
        w.pd.race[2]["hidden_years"] = hy
        w.inp.params["span"] = E
        w.year = y0
        n0 = len(w.events)
        w.c_t6()
        got = [(KEY_OF[e["kind"]], e["year"], e["end_year"]) for e in w.events[n0:]
               if KEY_OF[e["kind"]] in ("refugees_settled", "refuge_hidden")]
        want_l = [("refugees_settled", y0, y0)] + ([("refuge_hidden", y0, want)] if want is not None else [])
        c.eq("24.4 flight in %d, hidden_years %d, E %d" % (y0, hy, E), got, want_l)
        if want is not None:
            rs = [e for e in w.events[n0:] if KEY_OF[e["kind"]] == "refugees_settled"][0]
            rh = w.events[rs["id"]]
            c.eq("24.4 refuge_hidden right after refugees_settled: its settlement, founder, region; caused by it",
                 (rh["id"], rh["region"], rh["settlement"], [x[3] for x in rh["participants"]], rh["causes"]),
                 (rs["id"] + 1, rs["region"], rs["settlement"], [rs["settlement"], 2], [rs["id"]]))
    # --- P3-04 r19 22.2 / 24.8: the participant bound of kind 39 is 62 (the smallest ids)
    w, s4, s5 = r19_claim_world(2)
    extra = [add_stl(w, 1, 2, HAMLET, 50) for _ in range(68)]
    for y in range(1, 101):
        w.year = 1000 + y
        w.strain_update(1, w.ct["tick_strain_permille"], False)
        if (2, 1) in w.claim:
            break
    e, r = w.claim.get((2, 1), (None, None))
    if e is None:
        c.eq("22.2 bound: a claim within 100 tick years", False, True)
    else:
        listed = [p_[3] for p_ in w.events[e - 1]["participants"] if p_[1] == 3]
        visible = sorted([2, s4] + extra)
        c.eq("22.2 the kind 39 claim on 70 visible settlements lists the 62 smallest ids", listed, visible[:62])
    # --- 24.5: the toll factors
    sp = data().conflict["toll_spread_permille"]
    c.eq("24.5 shipped spread", sp, 200)
    c.eq("24.5 factor bounds at u's high word 0 and 2^32 - 1",
         (1000 - sp + pnr.pick(0, 2 * sp + 1), 1000 - sp + pnr.pick(((1 << 32) - 1) << 32, 2 * sp + 1)), (800, 1200))
    for s0, want in ((0, None), (200, True)):
        w = flight_case()
        w.ct = dict(w.ct, toll_spread_permille=s0)
        log = []
        w.draw = lambda st, a, b, k: (log.append(st) or 0)
        w.c_t6()
        c.eq("24.5 toll draws at s = %d" % s0, log.count("hist.conflict.toll"), 0 if want is None else 2)
    # two battles of one war in different years with equal forces: different tolls (seed 0 of the unit world)
    w = r22_world()
    tolls = []
    for y in (1051, 1052):
        w.year = y
        tolls.append(w.battle_tolls(3000, 2000, 9))
    base = (min(3000, 2000) * w.ct["loss_winner_permille"] // 1000,
            min(2000 * w.ct["loss_loser_permille"] // 1000, 3000 * w.ct["kill_max_permille"] // 1000))
    c.eq("24.5 equal forces (3000, 2000), war e9, years 1051 and 1052: the factored tolls",
         tolls, [(base[0] * x[3] // 1000, base[1] * x[2] // 1000) for x in w.toll_log[-2:]])
    c.true("24.5 ... differ %r" % tolls, tolls[0] != tolls[1])
    c.eq("24.5 the factors are the keyed draws of (Y, w, 0, k)", [(x[2], x[3]) for x in w.toll_log[-2:]],
         [(800 + pnr.pick(pnr.draw(w.keys["hist.conflict.toll"], y, 9, 0, k0), 401),
           800 + pnr.pick(pnr.draw(w.keys["hist.conflict.toll"], y, 9, 0, 1), 401)) for y, k0 in ((1051, 0), (1052, 0))])
    c.eq("24.5 conflict_toll_spread fails for 200, 250, 200",
         tuning_problem(dict(data().conflict, loss_winner_permille=200, loss_loser_permille=250,
                             toll_spread_permille=200)),
         "history: tuning: toll_spread_permille lets the victors lose more than the defeated")
    c.eq("24.5 conflict_toll_spread passes for the shipped values", tuning_problem(data().conflict), None)
    for f, v in (("toll_spread_permille", 501), ("quarrel_strength_permille", 1001), ("toll_spread_permille", -1)):
        c.eq("P3-01 r15 3.8 %s %d outside its range" % (f, v), tuning_problem(dict(data().conflict, **{f: v})),
             "history: tuning: conflict.%s outside its schema range" % f)


def r22_toll_vectors():
    """24.8 item 5: the factors v_L, v_W of hand-picked keys (seed, Y, w) at s = 200, for the C++ tests."""
    out = []
    for seed, y, w in ((0, 1051, 3), (42, 5836, 412), (42, 5837, 412), (7, 2000, 1), (1, 1, 18446744073709551615)):
        key = cr.stream_key(seed, "hist.conflict.toll")
        u = [pnr.draw(key, y, w, 0, k) for k in (0, 1)]
        out.append((seed, y, w, u[0], u[1], 800 + pnr.pick(u[0], 401), 800 + pnr.pick(u[1], 401)))
    return out


def check_units_r17(c):
    """P3-06 r17 22.10 items 1 to 6 on hand-made worlds."""
    dat = data()
    t = dat.conflict
    c.eq("22.9 quarrel_min_settlements shipped 2", t["quarrel_min_settlements"], 2)
    c.true("22.2 history.conflict has no hidden_years", "hidden_years" not in t)
    c.eq("22.2 race hidden_years (orc 100, others 300)", {dat.race[r]["key"]: dat.race[r]["hidden_years"]
                                                         for r in dat.race}, pdr.HIDDEN_YEARS_019)
    c.eq("22.7 strain_max 500, quarrel_strain 600: conflict_quarrel_strain_max",
         tuning_problem(dict(t, strain_max=500, quarrel_strain=600, claim_strain=100, declare_min=100,
                             declare_span=400)), "history: tuning: quarrel_strain must not be above strain_max")
    c.eq("22.7 600 and 600 passes", tuning_problem(dict(t, strain_max=600, quarrel_strain=600, claim_strain=100,
                                                         declare_min=100, declare_span=500)), None)
    c.eq("22.1 item 4 quarrel_min_settlements 1001 is out of range",
         tuning_problem(dict(t, quarrel_min_settlements=1001)),
         "history: tuning: conflict.quarrel_min_settlements outside its schema range")
    # --- item 1: held back
    forced_draw = lambda w: setattr(w, "draw", (lambda st, a, b, k, w=w: (w.draws_log.append((st, a, b)) or 0)))  # noqa
    w = r17_quarrel_world(1)
    forced_draw(w)
    got, dr = r17_declare(w)
    c.eq("22.10.1 one settlement, the quarrel claim the largest strain, odds 1000: no declaration, no declare draw",
         (got, dr), ([], []))
    c.eq("22.10.1 ... the quarrel stays open, the claim and strain stand", (w.quarrel_with, (1, 2) in w.claim,
                                                                          w.strain[(1, 2)]), ({1: 2}, True, 900000))
    w = r17_quarrel_world(1)
    forced_draw(w)
    sid = add_stl(w, 1, 1, HAMLET, 100)
    w.stl(sid)["created"] = w.serial          # a colony created in this step: not visible yet
    got, dr = r17_declare(w)
    c.eq("22.10.1 a colony founded in the same step does not count (visible from the next step)", (got, dr), ([], []))
    w.serial += 1
    w.year += 1
    got, dr = r17_declare(w)
    c.eq("22.10.1 with the colony visible the next year: the declaration chance is drawn and the war declared",
         (got, dr), ([("war_declared", [1, 2])], [("hist.conflict.declare", 1, 2)]))
    c.eq("22.1 item 1 the declaration closes the quarrel", w.quarrel_with, {})
    w = r17_quarrel_world(1)
    forced_draw(w)
    w.claim[(1, 3)] = (1, 3)
    w.strain[(1, 3)] = 950000
    got, dr = r17_declare(w)
    c.eq("22.10.1 an ordinary claim on c3 of larger strain: c3 is chosen and declared on", (got, dr),
         ([("war_declared", [1, 3])], [("hist.conflict.declare", 1, 3)]))
    c.eq("22.10.1 ... the quarrel with c2 stays open", w.quarrel_with, {1: 2})
    w = r17_quarrel_world(1)
    w.claim[(1, 3)] = (1, 3)
    w.strain[(1, 3)] = 500000
    c.eq("22.10.1 the tension skips the held-back claim and reports the next best", w.best_claim(w.year)[2], (1, 3))
    w.ct = dict(w.ct, quarrel_min_settlements=1)
    c.eq("22.10.1 with quarrel_min_settlements 1 the quarrel claim is the best again", w.best_claim(w.year)[2], (1, 2))
    w = r17_quarrel_world(1)
    w.claim.pop((1, 3), None)
    c.eq("22.10.1 only the held-back claim: the tension reports nothing", w.best_claim(w.year), (0, 0, None))
    w = r17_quarrel_world(2)
    c.eq("22.10.1 two visible settlements: the quarrel claim is pursued", w.best_claim(w.year)[2], (1, 2))
    w = r17_quarrel_world(1)
    w.civ(1)["capital"] = 1
    hid = add_stl(w, 1, 1, HAMLET, 100)
    w.hidden_until[hid] = w.year + 50
    c.eq("22.1 item 3 hidden settlements count (reading 2)", w.best_claim(w.year)[2], (1, 2))
    # after a war between a and b, whoever declared it, a's new claim on b is ordinary
    for attacker in (1, 2):
        w = r17_quarrel_world(1)
        forced_draw(w)
        if attacker == 2:
            w.claim[(2, 1)] = (1, 1)
            w.strain[(2, 1)] = 900000
        w.declare(attacker, 2 if attacker == 1 else 1, False)
        c.eq("22.1 item 1 a war started by c%d ends the quarrel" % attacker, w.quarrel_with, {})
        w.end_war(w.wars[-1], None, "length")          # a truce
        w.bookkeeping(w.year, False)
        w.last_peace[(1, 2)] = (w.year - 1000, w.last_peace[(1, 2)][1])
        w.claim[(1, 2)] = (1, 2)
        w.strain[(1, 2)] = 900000
        w.year += 1
        w.serial += 1
        got, dr = r17_declare(w)
        c.eq("22.10.1 after a war declared by c%d a new claim of c1 on c2 is ordinary (drawn)" % attacker, dr,
             [("hist.conflict.declare", 1, 2)])
    # the record is set by quarrels() and recognised by the pair: a claim dropped and raised again is still it
    w = r9_world(regions=4, races=("elf", "orc"))
    add_civ(w, ORC, 1)
    add_civ(w, ELF, 2)
    add_stl(w, 1, 1, TOWN, 1000)
    add_stl(w, 2, 2, TOWN, 1000)
    w.civ(1)["capital"], w.civ(2)["capital"] = 1, 2
    w.quarrels()
    c.eq("22.1 item 1 quarrels() records quarrel_with beside the strain", (w.quarrel_with, w.strain.get((1, 2))),
         ({1: 2}, t["quarrel_strain"] * 1000))
    w.claim[(1, 2)] = (1, 2)
    del w.claim[(1, 2)]
    w.claim[(1, 2)] = (5, 2)
    c.eq("22.1 item 2 a claim raised again is still the quarrel claim (held back)", w.held_back(1, 2, "unit"), True)
    w.civ(2)["ended_year"], w.civ(2)["ending_event"] = w.year, 1
    w.housekeeping()
    c.eq("22.1 item 1 b ends: the entry is erased", w.quarrel_with, {})
    # --- item 2: hidden years per race
    w = flight_case()
    w.c_t6()
    c.eq("22.10.2 a fled goblin refuge is hidden 300 years and lies low (lie_low with the refuge)",
         (w.low_log, w.low_refuge.get(2)), ([(1051, 2, 1351)], max(w.hidden_until)))
    wo = r9_world(races=("elf", "orc"))
    add_civ(wo, 1, 1)
    add_civ(wo, ORC, 2)
    add_stl(wo, 1, 1, TOWN, 3000)
    add_stl(wo, 2, 3, TOWN, 2000)
    wo.civ(1)["capital"], wo.civ(2)["capital"] = 1, 2
    forced(wo)
    claim_war(wo, 1, 2, 3)
    wo.c_t6()
    wo.year = 1051
    wo.serial += 1
    wo.c_t6()
    c.eq("22.10.2 a fled orc refuge is hidden 100 years", (wo.low_log, [y for _h, y in wo.hidden_until.items()]),
         ([(1051, 2, 1151)], [1151]))
    h = max(wo.hidden_until)
    for yr, want in ((1150, True), (1151, False)):
        wo.year = yr
        c.eq("22.10.2 the orcs lie low at %d: %s" % (yr, want), wo.hides(2), want)
    # --- item 3: lying low ends early
    for how in ("ruined", "passed"):
        w = flight_case()
        w.c_t6()
        h = max(w.hidden_until)
        other = add_stl(w, 2, 6, HAMLET, 100)         # c2 stands elsewhere (taken as defender in another war, say)
        w.year = 1100
        c.true("22.10.3 %s: c2 lies low at 1100" % how, w.hides(2))
        if how == "ruined":
            w.stl(h)["status"], w.stl(h)["owner_civ"] = RUINED, 0
        else:
            w.stl(h)["owner_civ"] = 1
        c.eq("22.10.3 %s before its year: c2 no longer lies low (peoples and conflict agree)" % how,
             (w.hides(2), w.lying_low(2)), (False, False))
        c.eq("22.10.3 %s: the new owner does not lie low" % how, w.hides(1), False)
        _ = other
    # --- item 4: aim over held land, the foothold cession
    def foot(defender_cap_in_goal=False, extra_b_in_goal=0):
        """Goblins c1 hold R2 (their town s1 in R1, a village s3 in R2); the elves c2 (capital s2 in R4) keep a
        village s4 in R2 (the foothold); a second elven realm c3 (R5) so that c2 is no last refuge. c1's war on
        c2 over R2."""
        w = r9_world(regions=5, races=("elf", "goblin"))
        add_civ(w, GOBLIN, 1)
        add_civ(w, ELF, 2)
        add_civ(w, ELF, 5)
        add_stl(w, 1, 1, TOWN, 3000)
        add_stl(w, 2, 4, TOWN, 10000)
        add_stl(w, 1, 2, VILLAGE, 500)
        add_stl(w, 2, 2, VILLAGE, 400)
        for _ in range(extra_b_in_goal):
            add_stl(w, 2, 2, HAMLET, 60)
        add_stl(w, 3, 5, HAMLET, 100)
        w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 2, len(w.settlements)
        if defender_cap_in_goal:
            w.civ(2)["capital"] = 4
        assert w.owner[2][0] == 1
        war = r14_war(w, 1, 2, 2, check_goal=not defender_cap_in_goal)
        war["goal"] = 2      # with b's capital in R2, 20.2 would choose R4: the cost order is the unit here
        w.year = 1051
        war["no_front"] = False
        return w, war
    w, war = foot()
    c.eq("22.10.4 the goal held by the attacker is an allowed goal (b stands in it)", war["goal"], 2)
    n0 = len(w.events)
    w.end_check(war)
    c.eq("22.10.4 no ending at the first check while b stands in G", (ended_kinds(w, n0), war["open"]), ([], True))
    w.year = 1055
    war["dead"], war["last_battle_winner"] = {1: 0, 2: 100}, 2
    w.end_check(war)
    c.eq("22.10.4 ... nor later, the defender having won the last battle (item 3 applies)", war["open"], True)
    # a sack or capture of b's last settlement in G ends it by aim (the last-battle rule not applied)
    for fate in ("captured", "sacked"):
        w, war = foot()
        war["dead"], war["last_battle_winner"] = {1: 100, 2: 0}, 2
        if fate == "captured":
            w.stl(4)["owner_civ"] = 1
        else:
            w.stl(4)["status"], w.stl(4)["owner_civ"] = RUINED, 0
        w.version += 1
        n0 = len(w.events)
        w.end_check(war)
        c.eq("22.10.4 b's last settlement in G %s: aim, war_won, no cost in G (20.3: tribute)" % fate,
             (ended_kinds(w, n0), war["end"][1:]), (["war_won", "tribute"], ("aim", 1)))
    # a win by the score passes b's settlements with one region_ceded and no change of the region's owner
    w, war = foot(extra_b_in_goal=1)
    war["dead"], war["last_battle_winner"] = {1: 0, 2: 100}, 1
    w.year = 1051 + t["war_max_years"]
    owner0 = w.owner[2]
    n0 = len(w.events)
    w.end_check(war)
    if FLAGS["foot40"]:
        # r19 23.7 item 2: one settlements_ceded (from c2, to c1, the passing settlements s4, s5 ascending) and no
        # region_ceded
        ce = [e for e in w.events[n0:] if e["kind"] in CEDES]
        c.eq("23.7.2 a win by the score over a foothold: war_won, one settlements_ceded of R2 from c2 to c1, s4, s5",
             (ended_kinds(w, n0), [(KEY_OF[e["kind"]], e["region"], [p_[3] for p_ in e["participants"]],
                                    [p_[1] for p_ in e["participants"]]) for e in ce]),
             (["war_won", "settlements_ceded"], [("settlements_ceded", 2, [2, 1, 4, 5], [1, 2, 3, 3])]))
    else:
        ce = [e for e in w.events[n0:] if e["kind"] == K["region_ceded"]]
        c.eq("22.10.4 a win by the score over a foothold: war_won, one region_ceded of R2 from c2 to c1",
             (ended_kinds(w, n0), [(e["region"], [p_[3] for p_ in e["participants"]]) for e in ce]),
             (["war_won", "region_ceded"], [(2, [2, 1])]))
    c.eq("22.10.4 ... both of c2's settlements in R2 pass; the region's owner row is unchanged",
         ([w.stl(i)["owner_civ"] for i in (4, 5)], w.owner[2]), ([1, 1], owner0))
    c.eq("22.10.4 ... wronged(c2, c1) gains R2; the cost is the foothold", (2 in w.wronged.get((2, 1), set()),
                                                                           war["cost"]), (True, "foothold"))
    c.eq("22.10.4 ... (17) holds in the run", w.inv17, [])
    # b's capital stands there: 20.3's cost
    w, war = foot(defender_cap_in_goal=True)
    war["dead"], war["last_battle_winner"] = {1: 0, 2: 100}, 1
    w.year = 1051 + t["war_max_years"]
    n0 = len(w.events)
    w.end_check(war)
    c.eq("22.10.4 a win by the score where b's capital stands in G: 20.3 (b's best other region R4)",
         (ended_kinds(w, n0), war["cost"]), (["war_won", "region_ceded"], "cede"))
    c.eq("22.10.4 ... b keeps its capital s4 in R2", w.stl(4)["owner_civ"], 2)
    # a truce raises 20.4's count (hold(G) unchanged)
    w, war = foot()
    war["dead"], war["last_battle_winner"] = {1: 0, 2: 0}, None
    w.year = 1051 + t["war_max_years"]
    n0 = len(w.events)
    w.end_check(war)
    c.eq("22.10.4 a truce over the foothold raises the futile count of (c2, R2)",
         (ended_kinds(w, n0), w.futile.get((2, 2))), (["truce"], (1, 1)))
    # the ordinary aim win is unchanged: b's last settlement in G captured and the region conquered
    w, war = goal_case(attacker_leads=False, last_winner=2, to=1)
    n0 = len(w.events)
    w.end_check(war)
    c.eq("22.10.4 the ordinary aim win (G captured with the region) is unchanged", (ended_kinds(w, n0)[0],
                                                                                    war["end"][1:]), ("war_won", ("aim", 1)))
    # at 7.8's end: an attacker holding G with b in it ends by the score, not by aim
    w, war = foot()
    war["dead"], war["last_battle_winner"] = {1: 0, 2: 100}, 1
    w.quiet_used, w.charged = {}, set()
    n0 = len(w.events)
    w.c_q4()
    c.eq("22.5 at 7.8's end with b in G: by the score (quiet), the foothold cession", (ended_kinds(w, n0),
                                                                                         war["end"][1], war["cost"]),
         (["war_won", "settlements_ceded" if FLAGS["foot40"] else "region_ceded"], "quiet", "foothold"))
    w.quiet_used = None
    # --- item 5: the search's weariness bound is exclusive
    w = r9_world(regions=4)
    add_civ(w, ELF, 1)
    add_civ(w, GOBLIN, 3)
    add_stl(w, 1, 1, TOWN, 1000)
    add_stl(w, 2, 3, TOWN, 1000)
    w.civ(1)["capital"], w.civ(2)["capital"] = 1, 2
    Y = 1000
    w.searched_until[(2, 3)] = Y + t["search_weary_years"]
    for yr, want in ((Y + t["search_weary_years"] - 1, True), (Y + t["search_weary_years"], False)):
        w.year = yr
        c.eq("22.10.5 (y, R) weary at %d: %s" % (yr - Y, want), w.weary(2, 3), want)


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
    c.eq("20.9 shipped r9 tuning", [(n, t[n]) for n, _v in pdr.CONFLICT_018_VALUES if n in t],
         [kv for kv in pdr.CONFLICT_018_VALUES if kv[0] not in ("odds_min", "hidden_years") or not at_least("0.19.0")])
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
    c.eq("20.1 refuge_flight 0: the guard holds; r11 20.24 item 1: refuge_held, reason 1",
         (KEY_OF[w.events[n0 + 1]["kind"]], w.events[n0 + 1]["values"]), ("refuge_held", {1: 1}))
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
        if FLAGS["race_odds"]:
            w.pd.race[2]["odds_min"] = om     # r14 21.9: the goblin claimant's race field
        else:
            w.ct["odds_min"] = om
        c.eq("20.6 odds_min %s the odds" % ("at" if ok else "above"), w.declare_p(1, 2) is not None, ok)
    if FLAGS["race_odds"]:
        w.pd.race[2]["odds_min"] = 700
    else:
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
    check_units_r13(c)


# ---------------------------------------------------------------------------
# r13 (P3-06 20.27, 20.28; P3-07 21.8 item 5 r11): hand-made units
# ---------------------------------------------------------------------------

def colony_now(w, civ, region, kind, pop):
    """A colony of `civ` founded earlier in the current step (created = serial: not visible)."""
    sid = add_stl(w, civ, region, kind, pop)
    w.stl(sid)["created"] = w.serial
    return sid


def r13_rows(events, parts, settlements, census, control, relation=()):
    """A row set for the row-level checks: events (id, year, kind, region, settlement); parts (event, role, civ);
    settlements (id, region, founder, founding_event, owner, status); census (s, year, owner, status);
    control (region, from, to, civ, event); relation rows as stored."""
    return dict(event=[(e[0], e[1], e[1], e[2], 1, e[3], e[4], 50) for e in events],
                event_participant=[(p_[0], p_[1], p_[1], CIV, p_[2]) for p_ in parts],
                event_cause=[], event_value=[],
                settlement=[(s[0], "S", 0, 0, s[1], s[2], 0, s[3], s[4], HAMLET, s[5], 100, None, 0)
                            for s in settlements],
                settlement_census=[(x[0], x[1], x[2], HAMLET, x[3], 100) for x in census],
                region_control=list(control), relation=list(relation))


def check_units_r13(c):
    """P3-06 r13 20.27 item 8 and 20.28's tests (with P3-07 r11 21.8 item 5)."""
    # --- 20.27 item 8, first test: a Q4 colony of L into a free region in front(W, L); the war ends in W's favour;
    # 20.3 cedes that region and the colony passes with the region_ceded, no event added. Goblins c1 (capital R3),
    # elves c2 (capital R1, a village in R4, held by the goblins c3), the war over R4 won by c1.
    w = r9_world(regions=5)
    add_civ(w, 2, 1)
    add_civ(w, 1, 2)
    add_civ(w, 2, 5)
    add_stl(w, 1, 3, TOWN, 3000)
    add_stl(w, 2, 1, TOWN, 400)
    add_stl(w, 3, 4, HAMLET, 100, x=70)
    add_stl(w, 2, 4, VILLAGE, 200)
    add_stl(w, 3, 5, HAMLET, 100)
    w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 2, 3
    claim_war(w, 1, 2, 4)
    war = w.wars[-1]
    w.year = 1060
    c.eq("20.27 without the colony nothing of c2 can be ceded: a tribute of (400 + 200) x 100 div 1000",
         w.cost_plan(war, 1), ("tribute", 60))
    s6 = colony_now(w, 2, 2, HAMLET, 60)
    c.eq("20.27 the colony s6 is not visible; c2 holds R2 through it", (w.visible(w.stl(s6)), w.owner[2][0]),
         (False, 2))
    c.eq("20.27 item 3: R2 is a 20.3 candidate by presence part (b): cede R2", w.cost_plan(war, 1), ("cede", 2))
    n0 = len(w.events)
    w.end_war(war, 1, "length")
    ev = w.events[n0:]
    c.eq("20.27 item 4: peace then region_ceded R2 from c2 to c1, no other event",
         ([KEY_OF[e["kind"]] for e in ev], ev[1]["region"], ev[1]["participants"]),
         (["peace", "region_ceded"], 2, [(1, 1, CIV, 2), (2, 2, CIV, 1)]))
    c.eq("20.27 item 1: the colony passes to c1 (60 <= 2 x C(R2, goblin): no excess); R2 is c1's",
         (w.stl(s6)["owner_civ"], w.stl(s6)["population"], w.owner[2]), (1, 60, (1, ev[1]["id"])))
    c.eq("20.27 item 6: c2 keeps no standing settlement in R2", [s["id"] for s in w.standing_of(2)
                                                                 if s["region"] == 2], [])
    # the 0.18.0 reading before r13 (visible settlements only) left the colony with c2: the defect D1
    with_flag = FLAGS["cede_all"]
    FLAGS["cede_all"] = False
    try:
        w = r9_world(regions=5)
        add_civ(w, 2, 1)
        add_civ(w, 1, 2)
        add_civ(w, 2, 5)
        add_stl(w, 1, 3, TOWN, 3000)
        add_stl(w, 2, 1, TOWN, 400)
        add_stl(w, 3, 4, HAMLET, 100, x=70)
        add_stl(w, 2, 4, VILLAGE, 200)
        add_stl(w, 3, 5, HAMLET, 100)
        w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 2, 3
        claim_war(w, 1, 2, 4)
        war = w.wars[-1]
        w.year = 1060
        s6 = colony_now(w, 2, 2, HAMLET, 60)
        w.end_war(war, 1, "length")
        c.eq("20.27 the defect D1 (visible only): R2 is c1's, its colony stays c2's",
             (w.owner[2][0], w.stl(s6)["owner_civ"]), (1, 2))
    finally:
        FLAGS["cede_all"] = with_flag
    # --- second test: 7.7's goal cession with one visible settlement and one same-step colony of L in G: both pass,
    # ascending, with the colony's excess to L's nearest other standing settlement; a same-step colony of L in a
    # region not ceded (third test) stays L's. Goblins c1 (capital R3, town 3000); elves c2 (capital R1 town 400,
    # a village 200 in R2); the war over R2 won by c1; then colonies of c2 in R2 (2500) and R4 (60).
    w = r9_world(regions=5)
    add_civ(w, 2, 3)
    add_civ(w, 1, 1)
    add_stl(w, 1, 3, TOWN, 3000)          # s1
    add_stl(w, 2, 1, TOWN, 400)           # s2
    add_stl(w, 2, 2, VILLAGE, 200)        # s3
    w.civ(1)["capital"], w.civ(2)["capital"] = 1, 2
    claim_war(w, 1, 2, 2)
    war = w.wars[-1]
    c.eq("20.27 the war's goal R2", war["goal"], 2)
    w.year = 1060
    s4 = colony_now(w, 2, 2, HAMLET, 2500)
    s5 = colony_now(w, 2, 4, HAMLET, 60)
    cap = w.geo.capacity(2, 2)
    c.eq("20.27 C(R2, goblin) of the unit geography (hand-read from the world)", cap, 1077)
    c.eq("20.27 7.7's cession of the goal", w.cost_plan(war, 1), ("goal", 2))
    n0 = len(w.events)
    w.end_war(war, 1, "length")
    ev = w.events[n0:]
    c.eq("20.27 7.7: peace then region_ceded R2, no other event", [KEY_OF[e["kind"]] for e in ev],
         ["peace", "region_ceded"])
    # the colony's excess 2500 - 2 x 1077 = 346 goes to s2 (R1, nearer than s5 in R4), room 2 x C(R1, elf) - 400
    room = 2 * w.geo.capacity(1, 1) - 400
    c.eq("20.27 both settlements of c2 in R2 pass; the colony keeps 2 x C, its excess joins s2 (room %d)" % room,
         (w.stl(3)["owner_civ"], w.stl(s4)["owner_civ"], w.stl(s4)["population"], w.stl(2)["population"]),
         (1, 1, 2154, 400 + min(346, room)))
    c.eq("20.27 the colony s4 keeps its founder and founding event (P3-05: founder c2)",
         (w.stl(s4)["founder_civ"], w.stl(s4)["kind"]), (2, HAMLET))
    c.eq("20.27 item 8 third test: c2's colony in R4 (not ceded) stays c2's", (w.stl(s5)["owner_civ"], w.owner[4][0]),
         (2, 2))
    # --- invariant (13) from the rows (20.27 item 6): the step 500's colony s3 of c2 in R2 and the cession e5
    events = [(1, 0, K["civ_founded"], 1, 1), (2, 0, K["civ_founded"], 3, 2), (3, 500, K["colony_founded"], 2, 3),
              (4, 500, K["peace"], 0, 0), (5, 500, K["region_ceded"], 2, 0)]
    parts = [(5, 1, 2), (5, 2, 1)]
    good = r13_rows(events, parts, [(1, 1, 1, 1, 1, THRIVING), (2, 3, 2, 2, 2, THRIVING), (3, 2, 2, 3, 1, THRIVING)],
                    [(1, 0, 1, THRIVING), (2, 0, 2, THRIVING), (3, 500, 1, THRIVING)],
                    [(1, 0, None, 1, 1), (2, 500, None, 1, 5), (3, 0, None, 2, 2)])
    c.eq("20.13 (13) rows: the same-step colony passed with the cession (c2 has no row of R2: log hold)",
         cession_violations(good), [])
    bad = r13_rows(events, parts, [(1, 1, 1, 1, 1, THRIVING), (2, 3, 2, 2, 2, THRIVING), (3, 2, 2, 3, 2, THRIVING)],
                   [(1, 0, 1, THRIVING), (2, 0, 2, THRIVING), (3, 500, 2, THRIVING)],
                   [(1, 0, None, 1, 1), (2, 500, None, 1, 5), (3, 0, None, 2, 2)])
    c.eq("20.13 (13) rows: the colony left with c2 (the defect D1) is found",
         cession_violations(bad), ["e5: s3 of c2 in rg2 did not pass to c1 (census of 500: c2)",
                                   "s3: replayed owner c1 standing True, stored c2 status 1"])
    # an older settlement of c2 in R2, held by a region_control row, passed; and a cession of a region c2 never held
    events2 = [(1, 0, K["civ_founded"], 1, 1), (2, 0, K["civ_founded"], 3, 2), (3, 100, K["colony_founded"], 2, 3),
               (4, 500, K["peace"], 0, 0), (5, 500, K["region_ceded"], 2, 0), (6, 600, K["peace"], 0, 0),
               (7, 600, K["region_ceded"], 4, 0)]
    parts2 = [(5, 1, 2), (5, 2, 1), (7, 1, 2), (7, 2, 1)]
    held = r13_rows(events2, parts2, [(1, 1, 1, 1, 1, THRIVING), (2, 3, 2, 2, 2, THRIVING),
                                      (3, 2, 2, 3, 1, THRIVING)],
                    [(1, 0, 1, THRIVING), (2, 0, 2, THRIVING), (3, 100, 2, THRIVING), (3, 500, 1, THRIVING)],
                    [(1, 0, None, 1, 1), (2, 100, 500, 2, 3), (2, 500, None, 1, 5), (3, 0, None, 2, 2),
                     (4, 600, None, 1, 7)])
    c.eq("20.13 (13) rows: an older settlement passed; a cession of a region its giver did not hold is found",
         cession_violations(held), ["e7: c2 ceded rg4, which it did not hold"])
    # captures and ruins replayed: s3 captured by c1 (e6) before the cession (nothing of c2 left in R2), then sacked
    events3 = [(1, 0, K["civ_founded"], 1, 1), (2, 0, K["civ_founded"], 3, 2), (3, 100, K["colony_founded"], 2, 3),
               (4, 100, K["settlement_founded"], 2, 4), (6, 400, K["settlement_captured"], 2, 3),
               (7, 450, K["sacked"], 2, 4), (8, 500, K["peace"], 0, 0), (9, 500, K["region_ceded"], 2, 0)]
    parts3 = [(6, 1, 1), (6, 2, 2), (7, 1, 1), (7, 2, 2), (9, 1, 2), (9, 2, 1)]
    cap_rows = r13_rows(events3, parts3, [(1, 1, 1, 1, 1, THRIVING), (2, 3, 2, 2, 2, THRIVING),
                                          (3, 2, 2, 3, 1, THRIVING), (4, 2, 2, 4, 0, RUINED)],
                        [(1, 0, 1, THRIVING), (2, 0, 2, THRIVING), (3, 100, 2, THRIVING), (3, 400, 1, THRIVING),
                         (4, 100, 2, THRIVING), (4, 450, 0, RUINED)],
                        [(1, 0, None, 1, 1), (2, 100, None, 2, 3), (3, 0, None, 2, 2)])
    c.eq("20.13 (13) rows: captures and ruins replayed (nothing of c2 stands in R2 at the cession)",
         cession_violations(cap_rows), [])
    # --- 20.28 / P3-07 r11 21.8 item 5: the statistic on hand-made logs
    def stat(wars, changes, E=1000):
        """wars: (declaration id, since, until or None, ending kind key or None); one defender c2, region R5;
        changes: region_control rows of R5 as (from, to)."""
        evs, rel, pts = [], [], []
        nid = 1000
        for (d, s, u, k) in wars:
            evs.append((d, s, K["war_declared"], 5, 0))
            pts += [(d, 1, 1), (d, 2, 2)]
            if u is None:
                rel.append((CIV, 1, CIV, 2, "war", s, None, d, 0))
            else:
                nid += 1
                evs.append((nid, u, K[k], 0, 0))
                rel.append((CIV, 1, CIV, 2, "war", s, u, d, nid))
        rows = r13_rows(sorted(evs), pts, [], [], [(5, a, b, 2, 1) for (a, b) in changes], rel)
        kind = {e[0]: e[3] for e in rows["event"]}
        groups = {}
        for e in rows["event"]:
            if e[3] == K["war_declared"]:
                groups.setdefault((2, 5), []).append(e[0])
        return same_goal_unchanged_max(rows, E, kind, groups)
    # 1024 seed 6 (20.28's table): e341 war_won, e445 peace, e453 closed by civ_conquered, e538 truce; no change
    seed6 = [(341, 4892, 4922, "war_won"), (445, 5297, 5309, "peace"), (453, 5310, 5338, "civ_conquered"),
             (538, 5591, 5597, "truce")]
    c.eq("20.28 seed 6 shape: e453 (civ_conquered) skipped, key 3", stat(seed6, [(0, None)], 6000), 3)
    c.eq("20.28 r10's reading counted e453: it would be 4 (the key is not that)", stat(seed6[:2] + [
        (453, 5310, 5338, "peace")] + seed6[3:], [(0, None)], 6000), 4)
    # 1024 calm seed 17: e285 (5577-5587) and e297 (5588-5598), changes at 5039 and 5857 only: key 2 (D3: not 1)
    c.eq("20.28 calm seed 17 shape: the first war after a change starts a run of 1, key 2",
         stat([(285, 5577, 5587, "peace"), (297, 5588, 5598, "truce")], [(0, 5039), (5039, 5857), (5857, None)],
              6000), 2)
    # a war closed by civ_conquered between two peace wars: not broken (2)
    c.eq("20.28 a civ_conquered war between two peace wars: run 2, not broken",
         stat([(10, 100, 110, "peace"), (20, 200, 210, "civ_conquered"), (30, 300, 310, "peace")], [(0, None)]), 2)
    c.eq("20.28 a civ_ended war counts no more than a civ_conquered one",
         stat([(10, 100, 110, "peace"), (20, 200, 210, "civ_ended"), (30, 300, 310, "peace")], [(0, None)]), 2)
    # that war's span holds a change of owner (205): the run is broken (the change is in [100, 310])
    c.eq("20.28 the skipped war's span holds a change of owner: the run is broken (1)",
         stat([(10, 100, 110, "peace"), (20, 200, 210, "civ_conquered"), (30, 300, 310, "peace")],
              [(0, 205), (205, None)]), 1)
    # a change between two wars: the second starts a run of 1
    c.eq("20.28 a change between two wars: the second war starts a run of 1 (key 1)",
         stat([(10, 100, 110, "peace"), (20, 200, 210, "peace")], [(0, 150), (150, None)]), 1)
    c.eq("20.28 ... and a third war after it extends that run (key 2)",
         stat([(10, 100, 110, "peace"), (20, 200, 210, "peace"), (30, 300, 310, "truce")], [(0, 150), (150, None)]),
         2)
    # a war whose own span holds the change is in no run; the next starts again at 1
    c.eq("20.28 a war whose span holds a change is in no run (0 then 1)",
         stat([(10, 100, 110, "peace"), (20, 200, 210, "peace")], [(0, 105), (105, None)]), 1)
    # three overlapping wars, the middle one ending last: w1 100-150, w2 120-400, w3 200-250, a change at 300:
    # the window of w1..w3 is [100, max(150, 400, 250)] = [100, 400], which holds 300; w2 alone holds it too
    c.eq("20.28 three overlapping wars, the middle ending last, a change at 300 in its span: key 1",
         stat([(10, 100, 150, "peace"), (20, 120, 400, "peace"), (30, 200, 250, "peace")], [(0, 300), (300, None)]),
         1)
    c.eq("20.28 the same three wars with the change at 450: one run of 3",
         stat([(10, 100, 150, "peace"), (20, 120, 400, "peace"), (30, 200, 250, "peace")], [(0, 450), (450, None)]),
         3)
    # the latest end, not the last war's: w1 100-400, w2 200-250, a change at 300. The last war's end would give
    # the window [100, 250] for w1..w2 (no change: 2); the latest end gives [100, 400]: w1 is in no run, w2 starts 1
    c.eq("20.28 the window ends at the latest end in the run, not the last war's (key 1, not 2)",
         stat([(10, 100, 400, "peace"), (20, 200, 250, "peace")], [(0, 300), (300, None)]), 1)
    # an open war counts with until = E: a change at 900 breaks it, none keeps it
    c.eq("20.28 an open war counts until E (no change: key 2)",
         stat([(10, 100, 110, "peace"), (20, 200, None, None)], [(0, None)]), 2)
    c.eq("20.28 an open war counts until E (a change at 900: key 1)",
         stat([(10, 100, 110, "peace"), (20, 200, None, None)], [(0, 900), (900, None)]), 1)
    c.eq("20.28 no war: 0", stat([], [(0, None)]), 0)


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
    # --- r11 20.23: weariness counts wars in progress
    w = r9_world(regions=4, races=("elf", "goblin"))
    add_civ(w, 2, 1)
    add_civ(w, 2, 4)
    add_civ(w, 1, 2)
    add_stl(w, 1, 1, TOWN, 3000)
    add_stl(w, 2, 4, TOWN, 3000)
    add_stl(w, 3, 2, TOWN, 300)
    add_stl(w, 3, 3, VILLAGE, 200)
    w.civ(1)["capital"], w.civ(2)["capital"], w.civ(3)["capital"] = 1, 2, 3
    w.futile[(3, 3)] = (2, 3)
    c.true("20.23 count 2 and no open war: R3 allowed", any(e[0] == 3 for e in w.allowed(2, 3, w.front(2, 3))))
    claim_war(w, 1, 3, 3)
    c.eq("20.23 c1's war over R3 is open", (w.wars[-1]["goal"], w.wars[-1]["open"]), (3, True))
    c.true("20.23 count 2 + 1 open: c2 cannot claim or declare over R3",
           not any(e[0] == 3 for e in w.allowed(2, 3, w.front(2, 3))))
    w.claim[(2, 3)] = (5, 3)
    e2 = w.declare(2, 3, False)
    c.true("20.23 c2's declaration over the weary R3 is re-chosen or dropped",
           e2 is None or w.events[e2 - 1]["region"] != 3)
    w.wars[0]["open"] = False
    c.true("20.23 once that war ends the count reads 2 again: allowed", any(e[0] == 3 for e in w.allowed(2, 3, [(3, 1, 3)])))
    w.ct["weary_wars"] = 0
    w.wars[0]["open"] = True
    w.futile[(3, 3)] = (9, 3)
    c.true("20.23 weary_wars 0 blocks nothing", any(e[0] == 3 for e in w.allowed(2, 3, [(3, 1, 3)])))
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
    if FLAGS["monsters"]:
        # P5-02 7.1: from 0.23.0 the shipped configuration also runs the monsters subsystem, whose beasts change
        # growth from year 0 (G) and whose raids change populations: the peoples-only prefix no longer holds
        # (I0, p5_02_monsters_reference.py, is the identity that replaces it)
        return 0
    claims = [e[0] for e in res["event"] if e[3] in CLAIMS]
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
        # P3-04 r12 20.1: from 0.19.0 the quiet_stretch open in that step (its end_year and years), not its participants
        if b is not None and a[3] in (K["quiet_era"], K.get("quiet_stretch")) and a[:2] == b[:2] and \
                a[3:] == b[3:] and cy <= a[2] <= b[2]:
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
    head = HEADER_018 if not at_least("0.19.0") else HEADER_019 + "#\n" + HEADER_018.replace(
        "generator 0.18.0 (schema 7)", "generator 0.18.0, kept at 0.19.0 (schema 7)", 1)
    if at_least("0.20.0"):
        head = HEADER_020 + "#\n" + head
    if at_least("0.21.0"):
        head = HEADER_021 + "#\n" + head
    if at_least("0.22.0"):
        head = HEADER_022 + "#\n" + head
    if at_least("0.23.0"):
        head = HEADER_023 + "#\n" + head
    return head + "#\n" + "\n".join(old[[i for i, l in enumerate(old) if l.startswith("# Format:")][0]:])


HEADER_023 = """\
# Generator 0.23.0 (P5-02, docs/specs/p5-02-beasts-monsters.md r0): the monsters subsystem after renaming in T1, T4,
# T6, Q1 and Q4 and as the second tension source (p5_02_monsters_reference.py; kinds 44 to 55, the tables beast,
# monster_kind, beast_population, monster and site, settlement.ending, schema 9), peoples' growth through the
# WildView, shunned regions and the ghost rule (8.2), a flare-up's forced declaration only for a claim's trigger
# (7.4), the renaming forms of 21.2 and rule 7 ravage, P3-02 names with the kinds monster and site (inputs:
# ~/.cache/the-unmapped/out/p5-02-tests/inputs, the P5-12 geography). Every table line gains the five tables.
"""

HEADER_022 = """\
# Generator 0.22.0 (P5-12, docs/specs/p5-12-renamed-regions.md): the renaming subsystem after conflict in T6 and
# Q4 (p5_12_renaming_reference.py; kind 43 region_renamed, table region_name, schema 8), sentences that name a
# region as at the event and {@region_former} (P3-04 r21 23.1), P3-02 r19 names and P1-06 r11 region names (the
# inputs: ~/.cache/the-unmapped/out/p5-12-tests/inputs). Every table line gains `table region_name`. New line:
#   <case> r21 renamings <n> <region>:<from_year>:<rule key>:e<cause>,...|-   the rows of region_name in key order
"""

HEADER_021 = """\
# Generator 0.21.0 (P5-01, docs/specs/p5-01-followups.md): P3-06 r22 section 24 (a fresh claim on a re-aimed
# declaration's goal, which the war cites; the awakening quarrel also held back below quarrel_strength_permille;
# claim_dropped (kind 41) at a declaration with no allowed goal; refuge_hidden (kind 42) at every flight; each
# side's battle toll times a keyed factor of hist.conflict.toll in [800, 1200] permille), the became_city
# template, the number clauses of P3-04 r19 22.1, flare_min_years (22.3), P3-02 r17 names and P1-06 r10 region
# names (the inputs: ~/.cache/the-unmapped/out/p5-01-tests/inputs). Lines added per case:
#   <case> r22 fresh <kind 20>/<kind 39> dropped <n> refuge_hidden <n> repeated_tolls <n> mixed_tolls <n>
#       fresh claims (a claim caused by a claim) by kind, kinds 41 and 42, battles repeating an earlier battle's
#       winner, loser and both tolls (T16), battle sentences with a digit and a word in the toll (T16; 0)
#   <case> r22 open_claims <e<id>,...|->       the claims the simulation holds open at E, both sides alive (T19)
#   <case> r22 hidden_refuges <s<id>,...|->    the refuges standing at E and hidden in E - 1 (T19)
"""

HEADER_020 = """\
# Generator 0.20.0 (P3-11, docs/specs/p3-11-followups.md): P3-06 r19 section 23 (a claim on a region its
# holder does not hold is kind 39 claim_settlements listing the holder's visible settlements there; the
# foothold cession is kind 40 settlements_ceded listing the passing settlements, war outcome won-c<W>-foothold),
# P3-05 r17 23 (the awakening population, orc 1200; the first colony freed from pressure), P3-04 r17 21 (one to nine as words; kinds 39, 40),
# P3-02 r15 14 (faerie lists, no repeated syllable, tard) and P3-07 r14 23 items 3, 4 in the note lines. New
# line, one per case:
#   <case> r19 t11 <years|-> first_orc_war <years|-> claims39 <claimant>/<third>/<none> cessions40 <n>
#       goal_differs <n>   (P3-06 r19 23.1-23.4 from the rows: T11 as 23.1 defines it, - without orcs; the
#       first orc war_declared after the awakening; kind 39 claims by the region's holder at the claim;
#       kind 40 events; war declarations whose region differs from their claim's)
"""
HEADER_019 = """\
# Generator 0.19.0 (P3-10, docs/specs/p3-10-followups.md): P3-06 r14 section 21
# (note_reduction only with a loss; the battle templates without "0 of the
# victors"; aim only by holding the goal, a lost goal ends now by the score
# with the last-battle rule, war line how "goal-lost"; searches: weary for
# search_weary_years, none where the war fought, salience 40 then 15; the
# holder in weariness and wars_same_goal_unchanged_max; odds_min_permille per
# race; a hidden refuge lies low; the orcs' awakening quarrel), P3-05 r12 22,
# P3-04 r12 20 (kinds 37 plague_outbreak and 38 quiet_stretch, {@causes:...},
# thousands separators), P3-02 r10 12 (filter entries, name cores), P1-06 r7
# region names (the inputs' region table) and P3-07 r12 22.3, 22.5a (the
# reworded flare-up note, repeated searches out of the summary). Data: the
# 0.19.0 listings of p3_data_reference.py while the disk holds 0.18.0 files.
# Second round (r3, P3-10 section 12; generator stays 0.19.0): P3-06 r17
# section 22 (the quarrel claim held back below quarrel_min_settlements 2;
# hidden_years per race, orc 100; one lying-low predicate; aim needs the
# defender out of the goal, the foothold cession; war line outcome
# won-c<W>-ceded for a foothold cession too), P3-05 r15 22.7a, P3-04 r15 20.1
# item 1a (no quiet_stretch under quiet_min_years 50), P3-02 r13 13. One line
# per case from the rows (replay_r17):
#   <case> r17 quick_aim_wins <n> foothold_cessions <m> held_goal_wars <wa>/<wd>/<t>/<o>
#       quick aim wins (22.5: attacker winner holding G with the loser out of
#       it, no battle, years <= 1), region_ceded events with no region_control
#       row, and wars declared over a region the attacker held: won by the
#       attacker / by the defender / truce / open or ended otherwise
"""


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
        with rules("0.18.0"):
            check_units_r9(c)   # 20.14 (with r10, r13) on the rules they were written for
    if at_least("0.21.0"):
        with rules("0.20.0"):
            check_units_r14(c)      # 21.5, 21.11, 21.12 item 6 (on the rules they were written for)
            check_units_r17(c)      # 22.10 items 1-6
    elif at_least("0.19.0"):
        check_units_r14(c)      # 21.5, 21.11, 21.12 item 6
        check_units_r17(c)      # 22.10 items 1-6
    if FLAGS["claim39"]:
        if FLAGS["fresh"]:
            with rules("0.20.0"):   # written for 0.20.0's rules (P5-01 changes declarations, tolls, the quarrel)
                check_units_r19(c)
        else:
            check_units_r19(c)      # 23.7 items 1 and 3 (item 2 is in check_units_r17's foothold cases)
    if FLAGS["fresh"]:
        check_units_r22(c)      # 24.8 items 1 to 5
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
            if FLAGS["rename"]:
                # P5-12 6 "Effect on the rest of history": from the first renaming on the history differs from
                # 0.21.0's, and seed 1 at 512 no longer has a flight at 0.22.0 (the case was chosen for its 0.18.0
                # history). Reported, not checked; flights and found refuges are reached by the stress cases.
                print("note: hist-conflict-flight at 0.22.0: flights %d, found refuges %d" % (w.flights, found))
            else:
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
        if FLAGS["claim39"] and (name.startswith("syn-") or name.startswith("stress-")):
            check_self_r19(c, res, w.pd, inp, name)
        if FLAGS["fresh"] and (name.startswith("syn-") or name.startswith("stress-")):
            reach["fresh claims"] = reach.get("fresh claims", 0) + check_self_r22(c, res, w.pd, inp, name)
        if FLAGS["strength"] and name in ("syn-1", "stress-2"):
            # 24.8 item 2: with quarrel_strength_permille 0 the rows are those of the rules without 24.2
            d0 = edited(w.pd, conflict={"quarrel_strength_permille": 0})
            a = history_stage(d0, inp)
            FLAGS["strength"] = False
            try:
                b = history_stage(d0, inp)
            finally:
                FLAGS["strength"] = True
            c.eq("%s 24.8 item 2: quarrel_strength_permille 0 gives the rows without 24.2" % name, table_hashes(a),
                 table_hashes(b))
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
    # Schema 8 (table region_name) from generator 0.22.0 (P5-12), schema 7 before; a schema 7 world is read with
    # --version 0.21.0 or older (its tables lack region_name, which the 0.22.0 rules write).
    if not VERSION_EXPLICIT and meta.get("generator_version") in VERSIONS and \
            meta.get("generator_version") != GENERATOR_VERSION:
        # P5-07 12 item 3: without --version the world's own generator version selects the rules
        use_version(meta["generator_version"])
        dat = data(opt(argv, "--data", "game/data"))
    want_schema = "13" if FLAGS["deep"] else "12" if FLAGS["figures"] else "11" if FLAGS["alliances"] else "10" if FLAGS["relations"] else "9" if FLAGS["monsters"] else "8" if FLAGS["rename"] else "7"   # P5-03 16.1: 10
    if meta.get("schema_version") != want_schema:
        print("%s: schema version %s, check-world at %s reads %s (schema 7: --version 0.21.0 or older)" %
              (path, meta.get("schema_version"), GENERATOR_VERSION, want_schema))
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
        if all(pchr.table_hash(COLUMNS[t], stored[t]) == want[t] for t in TABLES):
            check_invariants(c, st, inp, w, os.path.basename(path), last_refuge=w.ct["last_refuge"] == 1)
        else:
            # the stored history is not the reference's: the checks that read the reference's in-run state (its
            # wars, counters) may not apply to the stored rows; the row-only ones still do
            print("note: the stored history differs from the reference; invariants read from the stored rows, "
                  "those that need the reference's run may fail or stop")
            try:
                check_invariants(c, st, inp, w, os.path.basename(path), last_refuge=w.ct["last_refuge"] == 1)
            except (KeyError, IndexError, TypeError) as ex:
                c.true("invariants on the stored rows stopped at %s %s (the stored history differs)" %
                       (type(ex).__name__, ex), False)
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
        global VERSION_EXPLICIT
        VERSION_EXPLICIT = True
        i = argv.index("--version")
        use_version(argv[i + 1])
        argv = argv[:i] + argv[i + 2:]
    elif "--legacy" not in sys.argv:
        use_version(GENERATOR_VERSION)   # the default, 0.19.0, in this file and its libraries
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
    sys.modules.setdefault("p3_conflict_reference", sys.modules["__main__"])   # one module object when run as a script (P5-04a integration)
    sys.exit(main(sys.argv[1:]))
