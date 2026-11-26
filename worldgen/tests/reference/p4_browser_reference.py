#!/usr/bin/env python3
"""Independent exact-integer reference of P4-05 Part A, the browser of civilizations and events.

Written by the test writer from docs/specs/p4-05-browser.md r1 (sections 3, 4, 10 and 11), without reading the
implementation. It reads a world file with the standard-library sqlite3 module only and computes, by the
definitions of section 4 applied to the stored rows: the linked sentences of 4.1 (render_event_linked), the block
lines of 4.2 (civilization_block with its report and display forms), the kind groups of 4.4, the browser index of
4.5 (civilization entries, event details, select, search) and the place of an event of 4.6.

Reused, each written independently from its own spec (nothing of P4-05 is in them):
  p3_report_reference.py   the world, the digest with P5-02 12.9's second choice of the summary set (the set the
                           report prints, history_report_digest), the notes, civ_block (the report's lines, which
                           the block lines here must reproduce), the P3-07 fixtures
  p3_engine_reference.py   the sentence renderer, given an optional list that collects the links of 4.1 (added
                           for this stage; its text is unchanged, which `check` confirms on every event)
  p4_timeline_reference.py fixture T1
  p4_markers_reference.py  fixture T2, and the places of P4-04 3.4 (compared with event_place in `check`)
Written here: everything of P4-05.

Python 3.11 or newer (tomllib), standard library only, no floating point.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p4_browser_reference.py dump DB NAME
        One dump of 10.1 (civilizations, links, details, blocks-summary, blocks-full, filters, search) or the
        groups dump (groups; no DB needed, pass -), as raw bytes on stdout.

    python3 -B worldgen/tests/reference/p4_browser_reference.py fixture t3-browser OUT.db
        Writes the reference's copy of fixture T3 (worldgen/tests/data/browser/).

    python3 -B worldgen/tests/reference/p4_browser_reference.py golden [PATH|-] [--inputs DIR]
        Writes worldgen/tests/golden/p4-05-browser.txt ("-": stdout). World cases are read from DIR (default
        ~/.cache/the-unmapped/out/p5-02-fix/report-inputs), one <case>.db per case.

    python3 -B worldgen/tests/reference/p4_browser_reference.py check [--inputs DIR] [--quick]
        Self-checks (exit 1 on any disagreement): T3's hand-checked sentences, links, blocks, filters and
        searches; on every case (fixtures only with --quick): the linked text equals the sentence, links ascend
        and do not overlap and cover the names they claim, the block lines' report form equals the report's
        [civilizations] lines byte for byte (finish applied), the display form under B4's transform equals the
        report form, the summary flag equals the report's summary chronicle, select against a brute-force
        filter, search's order and limit, event_place against the places of P4-04 3.4; and the committed golden
        file against a fresh computation (not with --quick).

Readings chosen where the spec is open are listed in READINGS below and in the test writer's report.
Never regenerate the golden file to make a C++ test pass.
"""

import hashlib
import os
import sqlite3
import sys
import tomllib

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import p3_engine_reference as pe  # noqa: E402
import p3_report_reference as prr  # noqa: E402
import p4_markers_reference as pm  # noqa: E402
import p4_timeline_reference as tl  # noqa: E402

REPO = os.path.normpath(os.path.join(_HERE, "..", "..", ".."))
GOLDEN = os.path.join(REPO, "worldgen", "tests", "golden", "p4-05-browser.txt")
T3_DIR = os.path.join(REPO, "worldgen", "tests", "data", "browser")
T3_NAME = "t3-browser"
INPUTS_023 = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p5-02-fix", "report-inputs")
INPUTS_024 = tl.INPUTS_P5_03
INPUTS_025 = tl.INPUTS_P5_03A
INPUTS_026 = tl.INPUTS_P5_04
INPUTS = prr.DEFAULT_INPUTS_027   # 0.27.0 (P5-06 phase 2); --version 0.26.0 to 0.23.0 serves the file of that version
SCRATCH = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p4-05-a-tests", "fixtures-ref")
GENERATOR_VERSION = "0.28.0"   # P5-04a: the groups are P5-06's; P5-06 11.3: group figures = 8, ALL_GROUPS 0x1FF (0.26.0: P5-04's alliances = 7, 0xFF)
WORLD_CASES = tl.WORLD_CASES
SLOW_CASES = tuple(n for n in WORLD_CASES if n.endswith("-1024"))

READINGS = """
B1  4.2, an entity with no name (a reserved type 6 or 7, or an id the world lacks): its report form is the tag
    alone; the display form shows the tag too, with a link of that type and id that is NOT tagged, so that B4's
    transform (a tagged link's text t becomes escape(t) + " (" + tag + ")") gives the report form back. A tagged
    link over an empty text would turn into " (f1)", and one over the tag into "f1 (f1)"; neither is the report.
B2  4.2, the segments of each line (which the spec gives only for the first line, notes and milestones): the
    report's fixed words and numbers are literals; world strings without a tag (the civilization's own name on
    its first line, the age name, the biome key, a milestone's kind key, a relation kind) are text segments;
    every "<name> (<tag>)" and every bare tag of an entity is an entity segment; every "e<id>" is an event
    segment; a sentence (fate, milestones) is a sentence segment; a note is " (" literal, text, ")" literal.
    The first line "c<id> <name>" is a literal "c<id> " and a text segment: it carries no link.
B3  4.1, a link is added for every name appended, also when the appended text is empty (it never is in any
    golden case; `check` confirms that no link has length 0). Form `kind` (P5-02) links the monster (type 8) over
    the noun it appends; {@beast} appends a value (the beast's plural) and gets no link.
B4  4.5 EventDetail.participants: the name of a type 5 participant is its name at the event (N(r, e), as the
    sentence names it, not lower-cased); of types 1 to 4, 8 and 9 HistoryView::name (a race: race.name); of
    types 6 and 7 empty.
B5  10.1 dump lines: fields separated by single spaces; a line whose last field is text keeps the separating
    space when the text is empty ("note " for an event without a note). The ids line of `filters` is the ids
    separated by single spaces, an empty line when no id is kept. Search queries are written `query <limit>
    <q>`.
B6  10.1 `filters`: the descriptions are `all summary`, `all full`, `civ <c> summary|full`, `settlement <s>
    full`, `region <r> full`, `group <word> full`, `years <from> <to> full` with E / 2 floored, and `civ 1 group
    war full`, in that order; the settlements and regions with at least one event are taken in ascending id; a
    world without civilization 1 has no `civ 1 group war` line (select would raise "browser: no civilization 1").
B7  10.1 `search`: "a query matching nothing" is `qqqzzzxxq` (checked to match nothing in every case); the
    queries of each civilization's first three bytes follow the civilizations in ascending id, repeated queries
    kept.
B8  10.1 `groups`: the keys of the compiled catalogue (P5-02's 55 kinds) in id order, then `unknown_key`. P5-02's
    kinds 44 to 55 are in the group `monsters`, appended as code 5 with ALL_GROUPS 0x3F as section 9 foresees
    (lead's ruling at the integration), so that 10.2 item 2's "none falls to other" holds for all 55 kinds. The
    `filters` dump's "each group" lines cover the six groups.
B9  10.1 cases: P4-04 10.3's (the world cases of world-hashes.sh with history, sample-42-1024, the P3-07
    fixtures, T1 and T2) plus T3; the 1024 world cases are the slow ones.
B10 4.5 search, the name of a region: its name at E (the last region_name row with from_year <= E, else
    region.name), then region.name, then every region_name row by ascending from_year; the match reports the
    first name tried that matches. NameMatch.name is that name as stored (not folded).
B12 10.2 "T3 built through the writer API and through SQL as P4-01 13.3": the golden file carries `t3-browser
    table <name> <hex>` lines (hash --detail's table lines of this SQL copy), as P4-04's M10 did for T2.
B13 (P5-03, 0.24.0) the spec names no browser group for kinds 56 to 62: 56 settlement_fled joins `monsters`;
    57 to 62 form a new group `peoples`, code 6, ALL_GROUPS 0x7F, as P5-02 appended `monsters` (B8).
B11 4.6 event_place: an event with settlement s != 0 is placed at s's (x, y); else with region r != 0 at r's
    (seed_x, seed_y); else none. Participants are not used (P4-04 reading M6).
"""

RACE, GOD, CIV, SETTLEMENT, REGION, FIGURE, FACTION, MONSTER, SITE = 1, 2, 3, 4, 5, 6, 7, 8, 9

# 4.4
GROUP_WORDS = ("realms", "settlements", "war", "calamity", "other", "monsters", "peoples")
GROUP_KEYS = {
    0: ("awakening", "civ_founded", "civ_seceded", "civ_ended", "civ_conquered", "capital_moved", "succession_crisis",
        "quiet_era", "quiet_stretch", "region_renamed"),
    1: ("settlement_founded", "colony_founded", "became_village", "became_town", "became_city", "settlement_dwindled",
        "settlement_abandoned", "settlement_abandoned_to", "ruin_faded", "refugees_settled", "refuge_hidden"),
    2: ("flare_up", "claim", "claim_settlements", "claim_dropped", "war_declared", "battle", "battle_at_pass", "siege",
        "sacked", "settlement_captured", "peace", "truce", "war_won", "region_ceded", "settlements_ceded", "tribute",
        "search_abandoned", "refuge_held"),
    3: ("famine", "plague", "plague_spread", "plague_outbreak"),
    5: ("monster_appeared", "undead_rose", "monster_woke", "monster_slept", "monster_raid", "settlement_overrun",
        "monster_slain", "monster_driven_off", "monster_died", "beasts_bold", "outpost_built", "outpost_abandoned"),
}
ALL_GROUPS = 0x3F
GROUP_KEYS_023 = dict(GROUP_KEYS)
# P5-03 (0.24.0; reading B13): kind 56 settlement_fled joins `monsters`; kinds 57 to 62 form the group `peoples`,
# appended as code 6 with ALL_GROUPS 0x7F, as P5-02 appended `monsters` (the spec names no group).
GROUP_KEYS_024 = dict(GROUP_KEYS)
GROUP_KEYS_024.update({5: GROUP_KEYS[5] + ("settlement_fled",),
                       6: ("peoples_met", "disposition_turned", "refugees_arrived", "settlement_mixed",
                           "standing_changed", "people_expelled")})
# P5-04 11.2 (0.26.0): kinds 63 to 70, 72 to 76 and 78 in the new group `alliances` = 7; 71 allied_battle in `war`;
# 77 mine_opened in `settlements`; ALL_GROUPS 0xFF.
GROUP_KEYS_026 = dict(GROUP_KEYS_024)
GROUP_KEYS_026.update({1: GROUP_KEYS_024[1] + ("mine_opened",), 2: GROUP_KEYS_024[2] + ("allied_battle",),
                       7: ("alliance_formed", "alliance_renewed", "alliance_lapsed", "alliance_broken",
                           "alliance_betrayed", "ally_joined_war", "ally_refused", "ally_withdrew", "trade_opened",
                           "trade_broken", "spoils_shared", "spoils_withheld", "alliance_spurned", "arms_sold")})
# P5-06 11.3 (0.27.0): kinds 79 to 84, 86, 87, 89 to 91 in the new group `figures` = 8; 85 battle_led in `war`;
# 88 monster_slain_by_hero in `monsters`; 92 blight in `calamity`; ALL_GROUPS 0x1FF.
GROUP_KEYS_027 = dict(GROUP_KEYS_026)
GROUP_KEYS_027.update({2: GROUP_KEYS_026[2] + ("battle_led",), 3: GROUP_KEYS_026[3] + ("blight",),
                       5: GROUP_KEYS_026[5] + ("monster_slain_by_hero",),
                       8: ("came_to_rule", "figure_died", "succession_disputed", "exiled", "reigned_at_end",
                           "general_appointed", "fell_in_battle", "hero_rose", "hero_slain", "tower_founded",
                           "book_written")})
# P5-07 11.1 (generator 0.29.0): kind 93 in settlements (with mine_opened), 94 to 96 in monsters; ALL_GROUPS unchanged
GROUP_KEYS_029 = dict(GROUP_KEYS_027)
GROUP_KEYS_029.update({1: GROUP_KEYS_027[1] + ("delving_deepened",),
                       5: GROUP_KEYS_027[5] + ("deep_woken", "deep_sealed", "hold_lost")})
GROUP_VERSION = "0.27.0"


def use_groups(v):
    global GROUP_KEYS, GROUP_OF, ALL_GROUPS, GROUP_VERSION, GROUP_WORDS
    GROUP_VERSION = v
    if v == "0.29.0":
        GROUP_KEYS = GROUP_KEYS_029
        GROUP_WORDS = ("realms", "settlements", "war", "calamity", "other", "monsters", "peoples", "alliances",
                       "figures")
        GROUP_OF = {k: g for g, ks in GROUP_KEYS.items() for k in ks}
        ALL_GROUPS = 0x1FF
        assert sum(len(x) for x in GROUP_KEYS.values()) == 96 and len(GROUP_OF) == 96
        return
    GROUP_KEYS = (GROUP_KEYS_027 if v == "0.27.0" else GROUP_KEYS_026 if v == "0.26.0" else GROUP_KEYS_024
                  if v == "0.24.0" else GROUP_KEYS_023)
    GROUP_WORDS = ("realms", "settlements", "war", "calamity", "other", "monsters") + (
        ("peoples", "alliances", "figures") if v == "0.27.0" else ("peoples", "alliances") if v == "0.26.0"
        else ("peoples",) if v == "0.24.0" else ())
    GROUP_OF = {k: g for g, ks in GROUP_KEYS.items() for k in ks}
    ALL_GROUPS = 0x1FF if v == "0.27.0" else 0xFF if v == "0.26.0" else 0x7F if v == "0.24.0" else 0x3F
    n = 92 if v == "0.27.0" else 78 if v == "0.26.0" else 62 if v == "0.24.0" else 55
    assert sum(len(x) for x in GROUP_KEYS.values()) == n and len(GROUP_OF) == n


use_groups("0.27.0")


def event_group(key):
    """key: str (Latin-1 decoded bytes). 4 = other."""
    return GROUP_OF.get(key, 4)


# ---------------------------------------------------------------------------
# Text of the dumps (10.1): raw bytes, only "\", LF and bytes below 0x20 escaped as escape_history_text does
# ---------------------------------------------------------------------------

def dtext(s):
    out = []
    for ch in s:
        b = ord(ch)
        if b == 0x5C:
            out.append("\\\\")
        elif b < 0x20:
            out.append("\\x%02X" % b)
        else:
            out.append(ch)
    return "".join(out)


def fold(s):
    """4.5 search: A-Z folded to a-z, every other byte as is."""
    return "".join(chr(ord(ch) + 32) if "A" <= ch <= "Z" else ch for ch in s)


# ---------------------------------------------------------------------------
# Segments (4.2): (kind, text, type, id); kind "lit", "text", "ent", "ev", "sen"
# ---------------------------------------------------------------------------

def lit(s):
    return ("lit", s, 0, 0)


def txt(s):
    return ("text", s, 0, 0)


def ent(t, i):
    return ("ent", None, t, i)


def evs(i):
    return ("ev", None, 0, i)


def sen(i):
    return ("sen", None, 0, i)


class Browser:
    """The browser index of one world (4.5) with the block lines (4.2) and linked sentences (4.1)."""

    def __init__(self, path):
        self.path = path
        self.w = w = prr.load_world(path)
        self.d = prr.Digest(w)          # history_report_digest: the set the report prints (P5-02 12.9 included)
        self.E = w.E
        self.events = w.t["event"]      # id, year, end_year, kind, phase, region, settlement, salience
        self.ids = [e[0] for e in self.events]
        assert self.ids == list(range(1, len(self.ids) + 1))
        self.summary = set(self.d.shown)
        self._linked = {}
        # Timeline's lists (P4-01 4): civilization by participant; region and settlement by column or participant
        self.of = {CIV: {}, SETTLEMENT: {}, REGION: {}}
        for e in self.events:
            got = set()
            if e[5]:
                got.add((REGION, e[5]))
            if e[6]:
                got.add((SETTLEMENT, e[6]))
            for p in w.parts.get(e[0], []):
                if p[3] in (CIV, SETTLEMENT, REGION):
                    got.add((p[3], p[4]))
            for t, i in sorted(got):
                self.of[t].setdefault(i, []).append(e[0])
        self.eff = {}
        for e in self.events:
            for c in sorted(w.causes.get(e[0], [])):
                self.eff.setdefault(c, []).append(e[0])
        self.civ_ids = [c[0] for c in w.t["civilization"]]
        self.settlement_ids = [s[0] for s in w.t["settlement"]]
        self.region_ids = [r[0] for r in w.t["region"]]
        self.kind_key = w.kind_key
        self.roles = {(r[0], r[1]): r[2] for r in w.t["event_kind_role"]}
        self.vkeys = {(v[0], v[1]): v[2] for v in w.t["event_kind_value"]}

    # --- 4.1 -----------------------------------------------------------------------------------------------

    def linked(self, eid):
        r = self._linked.get(eid)
        if r is None:
            links = []
            text = pe.render(self.w.view, eid, links)
            r = (text, [(a, n, t, i) for a, n, t, i in links])
            self._linked[eid] = r
        return r

    # --- 4.4, 4.5 entries ---------------------------------------------------------------------------------

    def civ_entries(self):
        out = []
        for c in self.w.t["civilization"]:
            ended = c[8] is not None
            out.append((c[0], c[2], c[6], c[8] if ended else -1, c[9] if ended else 0,
                        len(self.w.civ_events.get(c[0], [])), c[1], self.w.race[c[2]][2]))
        return out

    def group(self, eid):
        return event_group(self.kind_key[self.w.ev[eid][3]])

    def note(self, eid):
        return prr.note(self.w, eid)

    def row_text(self, eid):
        e = self.w.ev[eid]
        return "%s  %s" % (prr.span(e), prr.with_note(self.linked(eid)[0], self.note(eid)))

    def place(self, eid):
        e = self.w.ev[eid]
        if e[6]:
            s = self.w.settlement[e[6]]
            return (s[2], s[3])
        if e[5]:
            r = self.w.region[e[5]]
            return (r[prr.RC["seed_x"]], r[prr.RC["seed_y"]])
        return None

    def participant_name(self, eid, t, i):
        if t == REGION:
            return self.w.view["region_at"](i, eid)
        n = self.w.name(t, i)
        return "" if n is None else n

    def detail(self, eid):
        w = self.w
        e = w.ev[eid]
        parts = [(self.roles[(e[3], p[2])], p[3], p[4], self.participant_name(eid, p[3], p[4]))
                 for p in sorted(w.parts.get(eid, []))]
        vals = [(self.vkeys[(e[3], v[1])], v[2]) for v in sorted(w.vals.get(eid, []))]
        return dict(id=eid, year=e[1], end_year=e[2], kind=e[3], salience=e[7], region=e[5], settlement=e[6],
                    group=self.group(eid), summary=eid in self.summary, kind_key=self.kind_key[e[3]],
                    sentence=self.linked(eid), note=self.note(eid), participants=parts, values=vals,
                    causes=sorted(w.causes.get(eid, [])), effects=list(self.eff.get(eid, [])),
                    place=self.place(eid))

    # --- 4.5 select ---------------------------------------------------------------------------------------

    def select(self, civ=0, settlement=0, region=0, from_year=-1, to_year=-1, groups=ALL_GROUPS,
               summary_only=True):
        for t, i, have in ((CIV, civ, self.civ_ids), (SETTLEMENT, settlement, self.settlement_ids),
                           (REGION, region, self.region_ids)):
            if i and i not in have:
                raise ValueError("browser: no %s %d" % ({CIV: "civilization", SETTLEMENT: "settlement",
                                                         REGION: "region"}[t], i))
        lists = [self.of[t].get(i, []) for t, i in ((CIV, civ), (SETTLEMENT, settlement), (REGION, region)) if i]
        cand = lists[0] if lists else self.ids
        rest = [set(x) for x in lists[1:]]
        out = []
        for eid in cand:
            e = self.w.ev[eid]
            if any(eid not in s for s in rest):
                continue
            if to_year != -1 and not e[1] <= to_year:
                continue
            if from_year != -1 and not e[2] >= from_year:
                continue
            if not groups & (1 << self.group(eid)):
                continue
            if summary_only and eid not in self.summary:
                continue
            out.append(eid)
        return out

    # --- 4.5 search ---------------------------------------------------------------------------------------

    def region_names_tried(self, rid):
        w = self.w
        return [w.present_name(rid), w.region[rid][1]] + [r[2] for r in w.renames.get(rid, ())]

    def search(self, q, limit):
        if q == "":
            return []
        fq = fold(q)
        out = []
        w = self.w
        for c in w.t["civilization"]:
            if fq in fold(c[1]):
                out.append((CIV, c[0], c[1]))
        for s in w.t["settlement"]:
            if fq in fold(s[1]):
                out.append((SETTLEMENT, s[0], s[1]))
        for r in w.t["region"]:
            for nm in self.region_names_tried(r[0]):
                if fq in fold(nm):
                    out.append((REGION, r[0], nm))
                    break
        # P5-06 11.3 (P5-04a 21.7 item 3): figures by display name after the regions, ascending id
        for f in sorted(w.t.get("figure", ())):
            dn = figure_display(f)
            if fq in fold(dn):
                out.append((FIGURE, f[0], dn))
        return out[:limit]

    # --- 4.2 block lines ----------------------------------------------------------------------------------

    def block_lines(self, cid, level):
        """civilization_block: the segment lines of prr.civ_block, written segment by segment."""
        w, d = self.w, self.d
        c = w.civ[cid]
        short = level == "summary" and c[8] is not None   # P3-07 25.5 (V2)
        o = [[lit("c%d " % cid), txt(c[1])]]
        o.append([lit("  race: "), ent(RACE, c[2])])
        reg = w.region[c[4]]
        up = ", upland" if 4 * reg[prr.RC["walk_cells"]] < reg[prr.RC["cells"]] else ""
        if not short:
            line = [lit("  home: "), ent(REGION, c[4]), lit(", "), txt(w.biome_key[reg[prr.RC["biome"]]])]
            if up:
                line.append(lit(up))
            o.append(line)
        a = prr.age_of(w, c[6])
        o.append([lit("  founded: %d in " % c[6]), txt(a[4]), lit(" (a%d), " % a[0]), evs(c[7])])
        if w.ev[c[7]][3] == w.K["civ_seceded"]:
            frm = prr.role_ids(w, c[7], "from")
            o.append([lit("  seceded from: "), ent(CIV, frm[0]), lit(", "), evs(c[7])])
        cap = "capital" if c[8] is None else "last capital"
        o.append([lit("  %s: " % cap), ent(SETTLEMENT, c[5])] if c[5] else [lit("  %s: none" % cap)])
        m, y0, y1 = d.peak_regions[cid]
        pp = d.peak_pop.get(cid)
        if not short:
            o.append([lit("  peak regions: %d, %d-%d" % (m, y0, y1) if m else "  peak regions: 0")])
            o.append([lit("  peak population: %d in %d" % pp if pp else "  peak population: 0")])
        if prr.V8:   # P5-06 25.1 (d): ", <d> faded" only when d > 0
            o.append([lit("  settlements: %d founded, %d held at the end, %d ruined%s" % (
                d.founded[cid], d.held_settlements[cid], d.ruined_only[cid],
                ", %d faded" % d.faded[cid] if d.faded[cid] > 0 else ""))])
        else:
            o.append([lit("  settlements: %d founded, %d held at the end, %d ruined, %d faded" % (
                d.founded[cid], d.held_settlements[cid], d.ruined_only[cid], d.faded[cid]))])
        rel = [x for x in w.t["relation"] if (x[0] == CIV and x[1] == cid) or (x[2] == CIV and x[3] == cid)]
        joined = getattr(w, "joined", set())   # P5-04 10.4: an ally's war rows are not wars (prr.civ_block)
        wars = [x for x in rel if x[4] == "war" and x[7] not in joined]
        nd = sum(1 for x in wars if x[0] == CIV and x[1] == cid)
        nf = sum(1 for x in wars if x[2] == CIV and x[3] == cid)
        out = {"won": 0, "lost": 0, "drawn": 0, "open": 0}
        for x in wars:
            out[prr.war_outcome(w, x, cid)] += 1
        o.append([lit("  wars: %d declared, %d defended; won %d, lost %d, drawn %d, open %d" % (
            nd, nf, out["won"], out["lost"], out["drawn"], out["open"]))])
        if level == "full":
            if not rel:
                o.append([lit("  relations: none")])
            else:
                kinds = {}
                for x in rel:
                    kinds[x[4]] = kinds.get(x[4], 0) + 1
                ks = sorted(kinds, key=lambda s: s.encode("latin-1"))
                line = [lit("  relations: %d (" % len(rel))]
                for j, k in enumerate(ks):
                    line += [lit(", ") if j else None, txt(k), lit(" %d" % kinds[k])]
                line.append(lit(")"))
                o.append([s for s in line if s is not None])
                for x in sorted(rel, key=lambda x: (x[5], x[0], x[1], x[2], x[3], x[4].encode("latin-1"))):
                    head = [lit("    "), txt(x[4]), lit(": "), ent(x[0], x[1]), lit(", "), ent(x[2], x[3])]
                    if x[6] is None:
                        o.append(head + [lit(", since %d, " % x[5]), evs(x[7])])
                    else:
                        last = x[5] if x[6] == x[5] else x[6] - 1
                        o.append(head + [lit(", %d-%d, " % (x[5], last)), evs(x[7]), lit(" to "), evs(x[8])])
        else:   # P5-04 10.4 (reading R18): an ally's war rows, alliances and trades are not adversaries
            o += self.adversary_lines(cid, [x for x in rel if not (x[4] == "war" and x[7] in joined)
                                            and not (prr.V7 and x[4] in ("alliance", "trade"))])
        if not short:
            o.append([lit("  events: %d" % len(w.civ_events.get(cid, [])))])
        if c[8] is None:
            o.append([lit("  fate: alive in %d: %s, %s, population %d" % (
                w.E, prr.pl(d.regions_held[cid], "region"), prr.pl(d.held_settlements[cid], "settlement"),
                d.population[cid]))])
        else:
            o.append([lit("  fate: ended in %d, " % c[8]), evs(c[9]), lit(": "), sen(c[9])])
        ms = prr.milestones(w, cid, prr.MILESTONES_ENDED if short else prr.MILESTONES)
        if prr.V9 and level == "summary":   # P5-04a 12.2 (report v9): the count of own events of salience >= 40, top three
            mv = prr.milestones_v9(w, cid)
            o.append([lit("  milestones: %d" % len(mv))])
            ms = [] if len(w.t["civilization"]) > prr.SUMMARY_LINES_CIVS else sorted(
                sorted(mv, key=lambda x: (-w.ev[x][7], x))[:prr.SUMMARY_MILESTONES_V9])
        elif prr.V6 and level == "summary":   # P5-03a 9.2 (report v6): the full list's count, then its quiet stretches
            full_ms = prr.milestones(w, cid, prr.MILESTONES)
            o.append([lit("  milestones: %d" % len(full_ms))])
            ms = [x for x in full_ms if w.ev[x][3] == w.K["quiet_stretch"]]
        else:
            o.append([lit("  milestones:")])
        for x in ms:
            e = w.ev[x]
            line = [lit("    %s " % prr.span(e)), evs(x), lit(" "), txt(w.kind_key[e[3]]), lit(": "), sen(x)]
            nt = prr.note(w, x, cid)
            if nt:
                line += [lit(" ("), txt(nt), lit(")")]
            o.append(line)
        if prr.V7:   # P5-04 10.3: the allies lines at the end of the block (the report's text, plain segments)
            for t_ in (prr.p504.civ_alliance_lines_v9 if prr.V9 else prr.p504.civ_alliance_lines)(prr.alliance_world(w), cid, level):
                if t_.startswith("    c"):
                    z = int(t_[5:t_.index(" ", 5)])
                    o.append([lit("    c%d " % z), txt(w.civ[z][1]), lit(t_[t_.index(": "):])])
                else:
                    o.append([lit(t_)])
        if prr.V8 and (level == "full" or len(w.t["civilization"]) <= prr.SUMMARY_LINES_CIVS):
            # P5-06 10.3: the rulers lines (as the C++ BlockLines: the figure references are entity segments)
            rulers = sorted((x for x in w.t["figure"] if x[3] == 1 and x[6] == cid), key=lambda x: x[15])
            n = max([x[15] for x in rulers] or [0])
            head = [lit("  rulers: %s, %d recorded" % (prr.pl(n, "reign"), len(rulers)))]
            if rulers:
                head += [lit("; first "), ent(FIGURE, rulers[0][0])]
                last = None
                if c[8] is None:
                    for x in rulers:
                        r = prr.ruled_row(w, x[0])
                        if r is not None and r[6] is None:
                            last = x
                if last is not None:
                    head += [lit("; at the end "), ent(FIGURE, last[0])]
            o.append(head)
            if level == "full":
                for x in rulers:
                    o.append([lit("    "), ent(FIGURE, x[0]), lit(": %d-%s, reign %d, %s" % (
                        x[8], prr.reign_end(w, x[0]), x[15], prr.TEMPER_WORD.get(x[17], "?")))])
        return o

    def adversary_lines(self, cid, rel):
        """P3-07 24.5 and 25.4 (prr.adversary_lines) in segments."""
        w = self.w
        if not rel:
            return [[lit("  adversaries: none")]]
        by = {}
        for x in rel:
            other = (x[2], x[3]) if (x[0] == CIV and x[1] == cid) else (x[0], x[1])
            by.setdefault(other, []).append(x)
        order = sorted(by, key=lambda o_: (min(x[5] for x in by[o_]), o_[0], o_[1]))
        out = [[lit("  adversaries: %d" % len(order))]]
        no_war = [0, 0]
        for o_ in order:
            rows = by[o_]
            open_kinds = {x[4] for x in rows if x[6] is None}
            state = ("at war" if "war" in open_kinds else "at peace" if "peace" in open_kinds else
                     "enmity" if "enmity" in open_kinds else "none")
            wars = [x for x in rows if x[4] == "war"]
            if not wars:
                no_war[0] += 1
                no_war[1] += 1 if state == "enmity" else 0
                continue
            cnt = {"won": 0, "lost": 0, "drawn": 0, "open": 0}
            for x in wars:
                cnt[prr.war_outcome(w, x, cid)] += 1
            first = min(x[5] for x in wars)
            last = max((w.E - 1) if x[6] is None else x[6] for x in wars)
            out.append([lit("    "), ent(o_[0], o_[1]), lit(": %s, won %d, lost %d, drawn %d, open %d, %d-%d; now %s" % (
                prr.pl(len(wars), "war"), cnt["won"], cnt["lost"], cnt["drawn"], cnt["open"], first, last, state))])
        if no_war[0] and not prr.V6:   # P5-03a 9.2: no "without war" line at v6
            out.append([lit("    without war: %d (enmity %d, none %d)" % (no_war[0], no_war[1],
                                                                          no_war[0] - no_war[1]))])
        return out

    def tag(self, t, i):
        return "%s%d" % (prr.PREFIX[t], i)

    def seg_report(self, s):
        k, text, t, i = s
        if k in ("lit", "text"):
            return text
        if k == "ent":
            n = self.w.name(t, i)
            return self.tag(t, i) if n is None or n == "" else "%s (%s)" % (n, self.tag(t, i))
        if k == "ev":
            return "e%d" % i
        return self.w.sentence(i)

    def line_report(self, line):
        """block_line_report_text before the report's escapes (the report escapes the whole line with finish)."""
        return "".join(self.seg_report(s) for s in line)

    def line_display(self, line):
        """block_line_display: (text, [(start, length, type, id, tagged)])."""
        text, links = "", []
        for k, s, t, i in line:
            if k in ("lit", "text"):
                text += s
            elif k == "ent":
                n = self.w.name(t, i)
                if n is None or n == "":   # reading B1
                    nm, tagged = self.tag(t, i), 0
                else:
                    nm, tagged = n, 1
                links.append((len(text), len(nm), t, i, tagged))
                text += nm
            elif k == "ev":
                nm = "e%d" % i
                links.append((len(text), len(nm), 0, i, 0))
                text += nm
            else:
                st, ls = self.linked(i)
                links += [(len(text) + a, n, tt, ii, 0) for a, n, tt, ii in ls]
                text += st
        return text, links

    def b4_transform(self, disp):
        """B4: the display text with each tagged link's text t replaced by t + " (" + tag + ")" (unescaped)."""
        text, links = disp
        out, at = "", 0
        for a, n, t, i, tagged in links:
            if tagged:
                out += text[at:a + n] + " (%s)" % self.tag(t, i)
                at = a + n
        return out + text[at:]


# ---------------------------------------------------------------------------
# Dumps (10.1)
# ---------------------------------------------------------------------------

def enc(lines):
    return ("".join(ln + "\n" for ln in lines)).encode("latin-1")


def dump_civilizations(b):
    return enc(["civ %d %d %d %d %d %d %s|%s" % (c[0], c[1], c[2], c[3], c[4], c[5], dtext(c[6]), dtext(c[7]))
                for c in b.civ_entries()])


def dump_links(b):
    out = []
    for eid in b.ids:
        text, links = b.linked(eid)
        out.append("event %d %s" % (eid, dtext(text)))
        out += ["link %d %d %d %d" % lk for lk in links]
    return enc(out)


def dump_details(b):
    out = []
    for eid in b.ids:
        d = b.detail(eid)
        out.append("detail %d %s %d %s" % (eid, GROUP_WORDS[d["group"]], 1 if d["summary"] else 0,
                                           dtext(d["kind_key"])))
        out.append("note %s" % dtext(d["note"]))
        out += ["part %s %d %d %s" % (dtext(r), t, i, dtext(n)) for r, t, i, n in d["participants"]]
        out += ["value %s %d" % (dtext(k), v) for k, v in d["values"]]
        out += ["cause %d" % c for c in d["causes"]]
        out += ["effect %d" % c for c in d["effects"]]
        out.append("place none" if d["place"] is None else "place %d %d" % d["place"])
    return enc(out)


def dump_blocks(b, level):
    out = []
    for cid in b.civ_ids:
        out.append("block %d" % cid)
        for line in b.block_lines(cid, level):
            text, links = b.line_display(line)
            out.append("line %s" % dtext(text))
            out += ["link %d %d %d %d %d" % lk for lk in links]
    return enc(out)


def filter_list(b):
    """[(description, kwargs)] of 10.1 `filters` (reading B6)."""
    E = b.E
    out = [("all summary", {}), ("all full", dict(summary_only=False))]
    for c in b.civ_ids:
        out.append(("civ %d summary" % c, dict(civ=c)))
        out.append(("civ %d full" % c, dict(civ=c, summary_only=False)))
    for s in b.settlement_ids:
        if b.of[SETTLEMENT].get(s):
            out.append(("settlement %d full" % s, dict(settlement=s, summary_only=False)))
    for r in b.region_ids:
        if b.of[REGION].get(r):
            out.append(("region %d full" % r, dict(region=r, summary_only=False)))
    for g, word in enumerate(GROUP_WORDS):
        out.append(("group %s full" % word, dict(groups=1 << g, summary_only=False)))
    out.append(("years 0 %d full" % (E // 2), dict(from_year=0, to_year=E // 2, summary_only=False)))
    out.append(("years %d %d full" % (E // 2, E), dict(from_year=E // 2, to_year=E, summary_only=False)))
    if 1 in b.civ_ids:
        out.append(("civ 1 group war full", dict(civ=1, groups=1 << 2, summary_only=False)))
    return out


def dump_filters(b):
    out = []
    for desc, kw in filter_list(b):
        out.append("filter %s" % desc)
        out.append(" ".join(str(i) for i in b.select(**kw)))
    return enc(out)


NOTHING = "qqqzzzxxq"


FIGURE = 6


def figure_display(f):
    """P5-06 6.10, 6.14: the display name of a figure row (name 1, epithet 2, regnal 16)."""
    import p5_06_figures_reference as p506
    return p506.display_name(f[1], f[16], f[2])


def figure_queries(b):
    """P5-04a 21.7 item 3: (a) the full display name of the smallest-id figure, (b) the first three bytes of the
    first ruler's name (the smallest-id figure of role 1), (c) the display name of the smallest-id figure with a
    regnal number (regnal >= 2), each when the case has one; limit 50."""
    figs = sorted(b.w.t.get("figure", ()))
    qs = []
    if figs:
        qs.append((figure_display(figs[0]), 50))
    rulers = [f for f in figs if f[3] == 1]
    if rulers:
        qs.append((rulers[0][1][:3], 50))
    regnal = [f for f in figs if f[16] >= 2]
    if regnal:
        qs.append((figure_display(regnal[0]), 50))
    return qs


def search_queries(b):
    qs = [("a", 50), ("the", 50), ("THE", 50)]
    qs += [(c[1][:3], 50) for c in b.w.t["civilization"]]
    qs += [(NOTHING, 50), ("a", 3)]
    return qs + figure_queries(b)


def dump_search(b):
    out = []
    for q, limit in search_queries(b):
        out.append("query %d %s" % (limit, dtext(q)))
        out += ["match %d %d %s" % (t, i, dtext(n)) for t, i, n in b.search(q, limit)]
    return enc(out)


def groups_dump():
    keys = [k["key"] for k in pe.COMPILED] + ["unknown_key"]
    return enc(["group %s %s" % (k, GROUP_WORDS[event_group(k)]) for k in keys])


DUMP_NAMES = ("civilizations", "links", "details", "blocks-summary", "blocks-full", "filters", "search")


def dump(b, name):
    if name == "civilizations":
        return dump_civilizations(b)
    if name == "links":
        return dump_links(b)
    if name == "details":
        return dump_details(b)
    if name == "blocks-summary":
        return dump_blocks(b, "summary")
    if name == "blocks-full":
        return dump_blocks(b, "full")
    if name == "filters":
        return dump_filters(b)
    if name == "search":
        return dump_search(b)
    raise SystemExit("dump: unknown name %s" % name)


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

def write_marked_fixture(toml_path, out):
    """The P3-07 expansion of a fixture, then the region seed cells and pass rows of its [markers] table (the
    builder of p4_markers_reference.write_t2, for any path)."""
    with open(toml_path, "rb") as f:
        fx = tomllib.load(f)
    meta, rows = prr.expand_fixture(fx)
    if os.path.exists(out):
        os.remove(out)
    conn = sqlite3.connect(out)
    try:
        for s in prr.ddl_statements():
            conn.execute(s)
        for k in sorted(meta):
            conn.execute("INSERT INTO meta VALUES (?, ?)", (k, meta[k]))
        for table, rs in rows.items():
            for r in rs:
                conn.execute('INSERT INTO "%s" VALUES (%s)' % (table, ", ".join("?" * len(r))), r)
        mk = fx.get("markers", {})
        for rid, x, y in mk.get("region_seed", []):
            conn.execute("UPDATE region SET seed_x = ?, seed_y = ? WHERE id = ?", (x, y, rid))
        for p in mk.get("pass", []):
            path = b"".join(int(c).to_bytes(4, "little") for c in p[8])
            conn.execute("INSERT INTO pass VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)", tuple(p[:8]) + (path,))
        conn.commit()
    finally:
        conn.close()


def write_t3(out):
    write_marked_fixture(os.path.join(T3_DIR, T3_NAME + ".toml"), out)


def fixture_db(name):
    os.makedirs(SCRATCH, exist_ok=True)
    out = os.path.join(SCRATCH, "%s-%d.db" % (name, os.getpid()))
    if name == T3_NAME:
        write_t3(out)
    elif name == pm.T2_NAME:
        pm.write_t2(out)
    elif name == tl.T1_NAME:
        tl.write_t1(out)
    else:
        prr.write_fixture(name, out)
    return out


def fixture_names():
    return tuple(prr.fixture_names()) + (tl.T1_NAME, pm.T2_NAME, T3_NAME)


# ---------------------------------------------------------------------------
# Golden file (10.1)
# ---------------------------------------------------------------------------

def sha(b):
    return hashlib.sha256(b).hexdigest()


def case_lines(name, path):
    b = Browser(path)
    lines = ["%s %s %s" % (name, n, sha(dump(b, n))) for n in DUMP_NAMES]
    if name == T3_NAME:   # reading B12: T3's table hashes, so that the C++ copy (writer API) is held to this one
        lines += ["%s table %s %s" % (name, t, hx) for t, hx in prr.detail_tables(path)]
    return lines


def all_cases(inputs):
    cases = [(n, None) for n in fixture_names()]
    cases += [(n, os.path.join(inputs, n + ".db")) for n in WORLD_CASES]
    return sorted(cases, key=lambda c: c[0].encode())


def golden_text(inputs):
    lines = []
    for name, path in all_cases(inputs):
        if path is None:
            path = fixture_db(name)
        elif not os.path.exists(path):
            raise SystemExit("missing input world %s" % path)
        lines += case_lines(name, path)
    head = [
        "# P4-05 browser golden values (docs/specs/p4-05-browser.md 10.1), generator %s." % GENERATOR_VERSION,
        "# Written by worldgen/tests/reference/p4_browser_reference.py golden; never edited by hand.",
        "# Cases ascending: the world cases of tools/ci/world-hashes.sh with history, sample-42-1024 (generate",
        "# --seed 42 --map-size 1024), the P3-07 fixtures, T1 (t1-edges), T2 (t2-markers) and T3 (t3-browser).",
        "#   <case> <name> <sha256 of the dump>   name in civilizations, links, details, blocks-summary, blocks-full,",
        "#                                         filters, search (dump lines of 10.1; readings B5 to B7)",
        "#   t3-browser table <name> <hex>   hash --detail's table lines of the reference's copy of T3",
        "#   groups <sha256>   group <key> <word> for every compiled kind key in id order, then unknown_key"]
    return "\n".join(head + lines + ["groups %s" % sha(groups_dump())]) + "\n"


# ---------------------------------------------------------------------------
# Self-checks
# ---------------------------------------------------------------------------

def report_block(text, head, stop):
    lines = text.split("\n")
    i = lines.index(head)
    out = []
    for ln in lines[i + 1:]:
        if ln == stop:
            break
        out.append(ln)
    return out


def check_case(c, name, b, every=True):
    w = b.w
    # 4.1 / 11 item 1
    for eid in b.ids:
        text, links = b.linked(eid)
        if text != w.sentence(eid):
            c.eq("%s e%d linked text = render_event" % (name, eid), text, w.sentence(eid))
        prev_end = 0
        for a, n, t, i in links:
            c.count += 1
            if a < prev_end or n <= 0 or a + n > len(text):
                c.eq("%s e%d links ascend, length > 0, inside" % (name, eid), (a, n, prev_end, len(text)), None)
            prev_end = a + n
            got = text[a:a + n]
            if t == RACE:
                r = w.race[i]
                ok = got in (r[2], r[3], r[4])
            elif t == REGION:
                nm = w.view["region_at"](i, eid)
                nm2 = w.view["region_at"](i, eid - 1)
                ok = got in (nm, nm2) or (got.startswith("the ") and "The " + got[4:] in (nm, nm2))
            else:
                nm = w.name(t, i)
                ok = got == nm or (got.startswith("the ") and nm == "The " + got[4:])
                if t == MONSTER and not ok:   # form kind: the monster's noun
                    ok = got == w.view["name"](MONSTER, i, "kind")
            if not ok:
                c.eq("%s e%d link %r names %d/%d" % (name, eid, got, t, i), False, True)
    # 4.2 / 11 item 2: the report's [civilizations] lines from the block lines, at both levels
    for level in ("summary", "full"):
        rep = prr.report(w, level).decode("latin-1")
        want = report_block(rep, "[civilizations]", "[present]")
        got = []
        for cid in b.civ_ids:
            lines = b.block_lines(cid, level)
            plain = [b.line_report(ln) for ln in lines]
            c.eq("%s c%d %s block = civ_block" % (name, cid, level), plain,
                 prr.civ_block(w, b.d, w.civ[cid], level))
            got += [prr.finish(p) for p in plain]
            for ln, p in zip(lines, plain):
                disp = b.line_display(ln)
                if b.b4_transform(disp) != p:
                    c.eq("%s c%d %s B4 transform" % (name, cid, level), b.b4_transform(disp), p)
                prev = 0
                for a, n, _t, _i, _g in disp[1]:
                    if a < prev:
                        c.eq("%s c%d block links ascend" % (name, cid), (a, prev), None)
                    prev = a + n
        if not b.civ_ids:
            got = ["(none)"]
        c.eq("%s [civilizations] %s from block lines" % (name, level), got, want)
        if level == "summary":   # the summary flag: the ids of the summary chronicle
            chron = report_block(rep, "[chronicle]", "[civilizations]")
            shown = sorted(int(ln.split(" ")[1][1:]) for ln in chron
                           if ln[:1].isdigit() and " e" in ln and ln.split(" ")[1][:1] == "e")
            c.eq("%s summary flag = summary chronicle" % name, sorted(b.summary), shown)
    # 4.5 select against a brute-force filter
    E = b.E
    spans = [(-1, -1), (0, E // 2), (E // 2, E), (E, E), (0, 0)]
    ents = [(0, 0, 0)] + [(cc, 0, 0) for cc in b.civ_ids[:4]] + [(0, s, 0) for s in b.settlement_ids[:4]] + \
        [(0, 0, r) for r in b.region_ids[:4]]
    if b.civ_ids and b.region_ids:
        ents.append((b.civ_ids[0], 0, b.region_ids[0]))
    for cv, st, rg in ents:
        for fy, ty in spans:
            for g in (ALL_GROUPS, 1 << 2, (1 << 0) | (1 << 3)):
                for so in (True, False):
                    got = b.select(cv, st, rg, fy, ty, g, so)
                    want = []
                    for e in b.events:
                        if cv and e[0] not in b.of[CIV].get(cv, []):
                            continue
                        if st and e[0] not in b.of[SETTLEMENT].get(st, []):
                            continue
                        if rg and e[0] not in b.of[REGION].get(rg, []):
                            continue
                        if ty != -1 and e[1] > ty or fy != -1 and e[2] < fy:
                            continue
                        if not g >> b.group(e[0]) & 1 or so and e[0] not in b.summary:
                            continue
                        want.append(e[0])
                    if got != want:
                        c.eq("%s select %r" % (name, (cv, st, rg, fy, ty, g, so)), got, want)
                    c.count += 1
    # 10.2 B3 at full: a civilization's list is its events, count = the block's events line
    for cid in b.civ_ids:
        c.eq("%s c%d events" % (name, cid), len(b.select(civ=cid, summary_only=False)),
             len(w.civ_events.get(cid, [])))
    c.eq("%s all full = every id" % name, b.select(summary_only=False), b.ids)
    # search
    c.eq("%s search empty" % name, b.search("", 50), [])
    c.eq("%s search nothing" % name, b.search(NOTHING, 50), [])
    full = b.search("a", 10 ** 9)
    c.eq("%s search limit" % name, b.search("a", 3), full[:3])
    c.eq("%s search order" % name, [(t, i) for t, i, _ in full],
         sorted({(t, i) for t, i, _ in full}, key=lambda x: ({CIV: 0, SETTLEMENT: 1, REGION: 2, FIGURE: 3}[x[0]], x[1])))
    c.eq("%s search THE = the" % name, b.search("THE", 50), b.search("the", 50))
    # event_place against the places of P4-04 3.4 (every year of each event's span)
    if every:
        mw = pm.World(b.path)
        at = {}
        for y in range(E):
            for x, yy, _sym, ids in mw.places(y):
                for eid in ids:
                    at.setdefault(eid, set()).add((x, yy))
        for eid in b.ids:
            p = b.place(eid)
            e = w.ev[eid]
            want = at.get(eid, set())
            if e[1] >= E:
                continue
            if (p is None and want) or (p is not None and want != {p}):
                c.eq("%s event_place e%d" % (name, eid), p, want)
            c.count += 1


def check_t3(c):
    b = Browser(fixture_db(T3_NAME))
    E = b.E
    c.eq("T3 E", E, 2000)
    L = b.linked
    c.eq("T3 e1 plural", L(1), ("Wealstuld awakened the Humans in Arn Vale.",
                                [(0, 9, GOD, 1), (23, 6, RACE, 1), (33, 8, REGION, 1)]))
    c.eq("T3 e2 a lower-cased The mid-sentence", L(2), ("The Humans founded the Kingdom of Arn in Arn Vale.",
                                         [(4, 6, RACE, 1), (19, 18, CIV, 1), (41, 8, REGION, 1)]))
    c.eq("T3 e4 former region name, {settlement}", L(4), (
        "The Kingdom of Arn founded Yewford in the Holt of Yews.",
        [(0, 18, CIV, 1), (27, 7, SETTLEMENT, 2), (38, 16, REGION, 2)]))
    c.eq("T3 e10 non-ASCII settlement (offsets in bytes)", L(10), (
        "Stonehelm founded Khaz\xc3\xa2d in Deep Halls.", [(0, 9, CIV, 3), (18, 7, SETTLEMENT, 4),
                                                         (29, 10, REGION, 4)]))
    c.eq("T3 e11 adjective", L(11), ("Dwarven smiths of Stonehelm forged a crown.", [(0, 7, RACE, 2), (18, 9, CIV, 3)]))
    c.eq("T3 e15 region_former and region", L(15), (
        "After the slaughter, the Holt of Yews became known as the Red Holt.",
        [(21, 16, REGION, 2), (54, 12, REGION, 2)]))
    c.eq("T3 e19 parties:all with three", L(19), (
        "The Kingdom of Arn, Belmark and Stonehelm met at Arnton from 1500 to 1502.",
        [(0, 18, CIV, 1), (20, 7, CIV, 2), (32, 9, CIV, 3), (49, 6, SETTLEMENT, 1)]))
    c.eq("T3 e20, e21", (L(20)[0], L(21)[0]), ("Cor Wold became known as Ash Wold.",
                                                 "Ash Wold became known as Grey Wold."))
    c.eq("T3 e7 no links", L(7), ("A comet crossed the sky.", []))
    c.eq("T3 civilizations", b.civ_entries(), [
        (1, 1, 0, -1, 0, 15, "The Kingdom of Arn", "Human"), (2, 1, 500, -1, 0, 4, "Belmark", "Human"),
        (3, 2, 1000, 1800, 22, 9, "Stonehelm", "Dwarf")])
    c.eq("T3 groups", [GROUP_WORDS[b.group(e)] for e in b.ids], [
        "realms", "realms", "settlements", "settlements", "settlements", "realms", "other", "realms", "realms",
        "settlements", "other", "war", "war", "war", "realms", "war", "war", "war", "other", "realms", "realms",
        "realms", "calamity"])
    c.eq("T3 effects", [b.eff.get(e, []) for e in (1, 2, 3, 13, 23)], [[2], [3, 4], [5], [14, 16], []])
    d16 = b.detail(16)
    c.eq("T3 e16 detail", (d16["participants"], d16["values"], d16["causes"], d16["effects"], d16["place"]),
         ([("attacker", CIV, 1, "The Kingdom of Arn"), ("defender", CIV, 3, "Stonehelm")], [("fell", 0)], [13],
          [22], (400, 400)))
    c.eq("T3 e4 detail region name at the event", b.detail(4)["participants"],
         [("settlement", SETTLEMENT, 2, "Yewford"), ("founder", CIV, 1, "The Kingdom of Arn")])
    c.eq("T3 places", [b.place(e) for e in (7, 11, 12, 15, 19, 20)],
         [None, None, (300, 60), (300, 60), (100, 100), (60, 460)])
    c.eq("T3 e13 note", b.note(13), "Claimed since 1200.")
    c.eq("T3 row text", (b.row_text(13), b.row_text(16)), (
        "1250  Stonehelm declared war on the Kingdom of Arn over the Holt of Yews. (Claimed since 1200.)",
        "1350-1360  The Kingdom of Arn besieged Khaz\xc3\xa2d of Stonehelm from 1350 to 1360; it held."))
    # select (4.5): span overlap at both ends, multi-year events, two entity fields, summary_only
    S = b.select
    c.eq("T3 select to 1350 keeps the siege starting then", S(to_year=1350, from_year=1350, summary_only=False), [16])
    c.eq("T3 select from 1360 keeps the siege ending then", S(from_year=1360, to_year=1360, summary_only=False), [16])
    c.eq("T3 select 1361-1389", S(from_year=1361, to_year=1389, summary_only=False), [])
    c.eq("T3 select 1501 inside the summit", S(from_year=1501, to_year=1501, summary_only=False), [19])
    c.eq("T3 select open to", S(from_year=1800, summary_only=False), [22, 23])
    c.eq("T3 select open from", S(to_year=0, summary_only=False), [1, 2, 3])
    c.eq("T3 select c2", S(civ=2, summary_only=False), [6, 17, 18, 19])
    c.eq("T3 select c1 and s1", S(civ=1, settlement=1, summary_only=False), [3, 5, 19, 23])
    c.eq("T3 select s3 and r3", S(settlement=3, region=3, summary_only=False), [5, 6, 18])
    c.eq("T3 select c3 and r2, war", S(civ=3, region=2, groups=1 << 2, summary_only=False), [12, 13, 14])
    c.eq("T3 select r5", S(region=5, summary_only=False), [20, 21])
    c.eq("T3 select group other", S(groups=1 << 4, summary_only=False), [7, 11, 19])
    c.eq("T3 omen not in the summary", 7 in b.summary, False)
    # the further cases of browser_unit_test.cpp
    A = ALL_GROUPS
    c.eq("T3 select 1349", S(from_year=1349, to_year=1349, summary_only=False), [])
    c.eq("T3 select 1355-1390", S(from_year=1355, to_year=1390, summary_only=False), [16, 17])
    c.eq("T3 select 1502-1600", S(from_year=1502, to_year=1600, summary_only=False), [19, 20])
    c.eq("T3 select all", len(S(summary_only=False)), 23)
    c.eq("T3 select r5 c2", S(civ=2, region=5, summary_only=False), [])
    c.eq("T3 select c2 s3 r3", S(civ=2, settlement=3, region=3, summary_only=False), [6, 18])
    c.eq("T3 select calamity | settlements", S(groups=(1 << 3) | (1 << 1), summary_only=False), [3, 4, 5, 10, 23])
    c.eq("T3 select no group", S(groups=0, summary_only=False), [])
    c.eq("T3 select c3 full", S(civ=3, summary_only=False), [9, 10, 11, 12, 13, 14, 16, 19, 22])
    c.eq("T3 select c3 summary", S(civ=3), [9, 10, 11, 13, 14, 16, 19, 22])
    c.eq("T3 summary size", len(S()), 21)
    c.eq("T3 select 700-1200 summary", S(from_year=700, to_year=1200), [8, 9, 10, 11])
    c.eq("T3 select c1 size", len(S(civ=1, summary_only=False)), 15)
    c.eq("T3 select s1", S(settlement=1, summary_only=False), [3, 5, 19, 23])
    c.eq("T3 select s3", S(settlement=3, summary_only=False), [5, 6, 18])
    for bad, msg in (((1, 0, 6), "browser: no region 6"), ((-2, 0, 0), "browser: no civilization -2")):
        try:
            S(*bad)
            c.eq("T3 select %r raises" % (bad,), False, True)
        except ValueError as e:
            c.eq("T3 select %r message" % (bad,), str(e), msg)
    c.eq("T3 search more", [[(t, i) for t, i, _ in b.search(q, 50)] for q in
                            ("THE KINGDOM", "tHe HoLt", "z\xe2", "khaz", "khaz\xc3\xa2", "ar")],
         [[(CIV, 1)], [(REGION, 2)], [], [(SETTLEMENT, 4)], [(SETTLEMENT, 4)],
          [(CIV, 1), (CIV, 2), (SETTLEMENT, 1), (REGION, 1)]])
    c.eq("T3 search limit 0", b.search("ar", 0), [])
    c.eq("T3 e14", L(14), ("The Kingdom of Arn defeated Stonehelm in the Holt of Yews; 40 of the defeated and 10 of "
                           "the victors fell.", [(0, 18, CIV, 1), (28, 9, CIV, 3), (41, 16, REGION, 2)]))
    c.eq("T3 e16", L(16), ("The Kingdom of Arn besieged Khaz\xc3\xa2d of Stonehelm from 1350 to 1360; it held.",
                           [(0, 18, CIV, 1), (28, 7, SETTLEMENT, 4), (39, 9, CIV, 3)]))
    c.eq("T3 row texts 7, 19", (b.row_text(7), b.row_text(19)), (
        "700  A comet crossed the sky.",
        "1500-1502  The Kingdom of Arn, Belmark and Stonehelm met at Arnton from 1500 to 1502."))
    c.eq("T3 details", [(b.detail(e)["causes"], b.detail(e)["effects"]) for e in (1, 2, 3, 5, 13)],
         [([], [2]), ([1], [3, 4]), ([2], [5]), ([3], []), ([12], [14, 16])])
    c.eq("T3 e18 values", b.detail(18)["values"], [("dead", 20), ("fled", 5)])
    c.eq("T3 e19 parts", b.detail(19)["participants"], [("parties", CIV, 1, "The Kingdom of Arn"),
                                                        ("parties", CIV, 2, "Belmark"), ("parties", CIV, 3, "Stonehelm")])
    c.eq("T3 e16 entry", (b.detail(16)["year"], b.detail(16)["end_year"], b.detail(16)["kind"],
                          b.detail(16)["salience"], b.detail(16)["region"], b.detail(16)["settlement"]),
         (1350, 1360, 12, 25, 4, 4))
    c.eq("T3 c1 race line", b.line_display(b.block_lines(1, "full")[1]), ("  race: Human", [(8, 5, RACE, 1, 1)]))
    c2s = [b.line_display(ln) for ln in b.block_lines(2, "summary")]
    c.eq("T3 c2 seceded links", [x for x in c2s if x[0].startswith("  seceded")],
         [("  seceded from: The Kingdom of Arn, e6", [(16, 18, CIV, 1, 1), (36, 2, 0, 6, 0)])])
    c3s = [b.line_display(ln) for ln in b.block_lines(3, "summary")]
    c.eq("T3 c3 heads", c3s[:4], [("c3 Stonehelm", []), ("  race: Dwarf", [(8, 5, RACE, 2, 1)]),
                                  ("  founded: 1000 in The Age of Dwarves (a2), e9", [(44, 2, 0, 9, 0)]),
                                  ("  last capital: Khaz\xc3\xa2d", [(16, 7, SETTLEMENT, 4, 1)])])
    c.eq("T3 c3 no home at summary", [x for x in c3s if x[0].startswith("  home: ")], [])
    c.eq("T3 c3 milestone e13", [x for x in c3s if x[0].startswith("    1250 e13 ")],
         [("    1250 e13 war_declared: Stonehelm declared war on the Kingdom of Arn over the Holt of Yews. "
           "(Claimed since 1200.)", [(9, 3, 0, 13, 0), (27, 9, CIV, 3, 0), (53, 18, CIV, 1, 0), (77, 16, REGION, 2, 0)])])
    c.eq("T3 c3 full home links", b.line_display(b.block_lines(3, "full")[2]),
         ("  home: Deep Halls, mountain, upland", [(8, 10, REGION, 4, 1)]))
    for bad, msg in (((9, 0, 0), "browser: no civilization 9"), ((0, 5, 0), "browser: no settlement 5"),
                     ((0, 0, 6), "browser: no region 6")):
        try:
            S(*bad)
            c.eq("T3 select %r raises" % (bad,), False, True)
        except ValueError as e:
            c.eq("T3 select %r message" % (bad,), str(e), msg)
    # search (4.5)
    c.eq("T3 search ash (a region_name row)", b.search("ash", 50), [(REGION, 5, "Ash Wold")])
    c.eq("T3 search yews (the geographic name)", b.search("YEWS", 50), [(REGION, 2, "The Holt of Yews")])
    c.eq("T3 search red (the name at E)", b.search("red", 50), [(REGION, 2, "The Red Holt")])
    c.eq("T3 search wold (name at E first)", b.search("wold", 50), [(REGION, 5, "Grey Wold")])
    c.eq("T3 search KHAZ", b.search("KHAZ", 50), [(SETTLEMENT, 4, "Khaz\xc3\xa2d")])
    c.eq("T3 search non-ASCII not folded", (b.search("KHAZ\xc3\x82D", 50), b.search("khaz\xc3\xa2d", 50)),
         ([], [(SETTLEMENT, 4, "Khaz\xc3\xa2d")]))
    c.eq("T3 search ar", b.search("ar", 50), [(CIV, 1, "The Kingdom of Arn"), (CIV, 2, "Belmark"),
                                              (SETTLEMENT, 1, "Arnton"), (REGION, 1, "Arn Vale")])
    c.eq("T3 search ar limit 2", b.search("ar", 2), [(CIV, 1, "The Kingdom of Arn"), (CIV, 2, "Belmark")])
    # blocks (4.2): no capital, ended, seceded, an adversary of type 6 with no name
    full1 = [b.line_display(ln) for ln in b.block_lines(1, "full")]
    c.eq("T3 c1 full enmity line (reading B1)", [x for x in full1 if x[0].startswith("    enmity")],
         [("    enmity: The Kingdom of Arn, f1, since 700, e7",
           [(12, 18, CIV, 1, 1), (32, 2, FIGURE, 1, 0), (47, 2, 0, 7, 0)])])
    c2 = [b.line_display(ln)[0] for ln in b.block_lines(2, "summary")]
    c.eq("T3 c2 capital none, seceded", ("  capital: none" in c2, "  seceded from: The Kingdom of Arn, e6" in c2),
         (True, True))
    c3 = [b.line_display(ln) for ln in b.block_lines(3, "summary")]
    c.eq("T3 c3 first lines", [x[0] for x in c3[:4]], ["c3 Stonehelm", "  race: Dwarf", "  founded: 1000 in "
                                                      "The Age of Dwarves (a2), e9", "  last capital: Khaz\xc3\xa2d"])
    fate = [x for x in c3 if x[0].startswith("  fate:")][0]
    c.eq("T3 c3 fate links", fate, ("  fate: ended in 1800, e22: Stonehelm fell to the Kingdom of Arn.",
                                     [(23, 3, 0, 22, 0), (28, 9, CIV, 3, 0), (46, 18, CIV, 1, 0)]))
    c.eq("T3 c3 home upland at full", b.line_display(b.block_lines(3, "full")[2])[0],
         "  home: Deep Halls, mountain, upland")


def cmd_check(argv):
    inputs = argv[argv.index("--inputs") + 1] if "--inputs" in argv else INPUTS
    quick = "--quick" in argv
    c = tl.Checker()
    # 4.4 table
    for g, ks in GROUP_KEYS.items():
        for k in ks:
            c.eq("4.4 %s" % k, event_group(k), g)
    shipped = [k["key"] for k in pe.COMPILED_022]
    c.eq("4.4 the 43 kinds of 0.22.0 each have a row", [k for k in shipped if event_group(k) == 4], [])
    c.eq("4.4 P5-02 kinds are monsters (reading B8)", [k["key"] for k in pe.COMPILED[43:56] if event_group(k["key"]) != 5],
         [])
    if len(pe.COMPILED) > 56:   # reading B13
        c.eq("4.4 P5-03 kinds 57 to 62 are peoples (reading B13)",
             [k["key"] for k in pe.COMPILED[56:62] if event_group(k["key"]) != 6], [])
    c.eq("4.4 no compiled kind falls to other", [k["key"] for k in pe.COMPILED if event_group(k["key"]) == 4], [])
    c.eq("4.4 near misses", [event_group(k) for k in ("", "warx", "Battle", "battle ", "unknown_key")], [4] * 5)
    check_t3(c)
    for name in fixture_names():
        check_case(c, name, Browser(fixture_db(name)))
    have = all(os.path.exists(os.path.join(inputs, n + ".db")) for n in WORLD_CASES)
    if have and not quick:
        for name in WORLD_CASES:
            b = Browser(os.path.join(inputs, name + ".db"))
            check_case(c, name, b, every=name not in SLOW_CASES)
            c.eq("%s nothing matches %s" % (name, NOTHING), b.search(NOTHING, 50), [])
        if os.path.exists(GOLDEN):
            with open(GOLDEN) as f:
                c.eq("golden file is current", f.read(), golden_text(inputs))
        else:
            c.eq("golden file exists", False, True)
    elif not have:
        print("note: %s lacks the world cases; world checks and the golden comparison are skipped" % inputs)
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    global INPUTS, GENERATOR_VERSION
    if "--version" in argv:   # 0.23.0: the committed P4-05 file (P5-02 rules, groups of 55 kinds, its inputs)
        i = argv.index("--version")
        v = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
        if v == "0.23.0":
            prr.use_version("0.23.0")
            tl.RULES = "p5-02"
            use_groups("0.23.0")
            INPUTS, GENERATOR_VERSION = INPUTS_023, "0.23.0"
        elif v == "0.24.0":
            prr.use_version("0.24.0")
            tl.RULES = "p5-03"
            use_groups("0.24.0")
            INPUTS, GENERATOR_VERSION = INPUTS_024, "0.24.0"
        elif v == "0.25.0":
            prr.use_version("0.25.0")
            tl.RULES = "p5-03a"
            use_groups("0.24.0")
            INPUTS, GENERATOR_VERSION = INPUTS_025, "0.25.0"
        elif v == "0.26.0":
            prr.use_version("0.26.0")
            tl.RULES = "p5-04"
            use_groups("0.26.0")
            INPUTS, GENERATOR_VERSION = INPUTS_026, "0.26.0"
        elif v == "0.29.0":   # P5-07 11.1: the groups of the deep kinds
            prr.use_version("0.29.0")
            use_groups("0.29.0")
            GENERATOR_VERSION = "0.29.0"
        elif v not in ("0.27.0", "0.28.0"):
            raise SystemExit("--version: 0.23.0 to 0.29.0")
    cmd, rest = argv[0], argv[1:]
    if cmd == "dump":
        if rest[1] == "groups":
            sys.stdout.buffer.write(groups_dump())
            return 0
        sys.stdout.buffer.write(dump(Browser(rest[0]), rest[1]))
        return 0
    if cmd == "fixture":
        if rest[0] != T3_NAME:
            raise SystemExit("fixture: only %s" % T3_NAME)
        write_t3(rest[1])
        return 0
    if cmd == "golden":
        inputs = rest[rest.index("--inputs") + 1] if "--inputs" in rest else INPUTS
        text = golden_text(inputs)
        path = rest[0] if rest and not rest[0].startswith("--") else GOLDEN
        if path == "-":
            sys.stdout.write(text)
        else:
            with open(path, "w", newline="\n") as f:
                f.write(text)
        return 0
    if cmd == "check":
        return cmd_check(rest)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.modules.setdefault("p4_browser_reference", sys.modules["__main__"])   # one module object when run as a script (P5-04a integration)
    sys.exit(main(sys.argv[1:]))
