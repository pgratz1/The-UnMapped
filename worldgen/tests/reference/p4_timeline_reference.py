#!/usr/bin/env python3
"""Independent exact-integer reference of P4-01, the timeline in the library, as amended by P4-03.

Written by the test writer from docs/specs/p4-01-timeline.md alone (sections 4, 5.5, 5.6 and 13), with
P0-04 amendment 23 and P4-00, without reading the implementation. It reads a world file with the
standard-library sqlite3 module only and computes, from the definitions of section 4 applied directly to
the stored rows (no index of section 6), the canonical dumps of 13.1, the civilization colours of 5.5 and
the territory image of 5.6.

P4-03 (docs/specs/p4-03-history-view.md sections 7, 8, 9.2, 9.4, 11.1), also without the implementation:
the explicit colour table of 7.2 and race_civilization_colour (keys outside the table keep 5.5's hash
rule), territory_holders, the inner-border rule of 8.2 in the territory renderer, the colour distance rule
of 7.3 (checked, never used for output) and the texts of Timeline::build's control-run check (9.4). These
are the default rules; `--rules p4-01` gives P4-01's colours and image (holders ignored) and, with the
0.20.0 inputs, reproduces the golden file as P4-01 committed it.

P4-03a (docs/specs/p4-03a-territory-colour-followups.md r1, with P4-03 r4 7.2, 7.5, 8.2, 9.2), the default
rule set `p4-03a`, also without the implementation: the r4 table of 7.2 (TABLE_P403A: goblin 3 (140, 155,
30); goblin 1, elf 1 and human 1 darkened), the integer luma L = floor((77 R + 150 G + 29 B) / 256), the
inner-border dim by 10/16 when L > 175 and 12/16 otherwise, and half(U) (each channel of the cell's pixel
in the biomes composite times 8/16, floored) on a border candidate whose sides are all free when the holder
vector is not empty. `--rules p5-12` keeps P4-03 r3's rules on the same 0.22.0 inputs and reproduces the
golden file as P5-12 recorded it.

Reused, each written independently from its own spec (nothing of P4-01 is in them):
  p3_report_reference.py  connect and pk_rows (read-only world access, rows in primary-key order),
                          detail_tables (hash --detail's table lines, P0-04 8), expand_fixture,
                          ddl_statements and write_fixture (the P3-07 fixture worlds of 14.3 and their
                          TOML format, which T1 extends with layers), FIXTURE_NAMES
  p0_world_check.py       decode_blob and encode_delta_varint (the layer blob format of P0-02 7), TYPES
  core_reference.py       hash_words, pack_bytes (P0-01 7.1, 7.2: the byte-string encoding) and
                          tag_value (the tag "UNM:COLR")
Written here: the dumps, the colours, and the composites `map` (steps 1 to 4 of P1-00 10.2), `biomes`
(P1-04 9) and `regions` (P0-04 amendment 18 (e)) with the territory pass of 5.6; no earlier reference
renders a composite, so this is the first Python one.

Python 3.11 or newer (tomllib), standard library only, no floating point.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p4_timeline_reference.py dump DB [--year Y | --static | --years]
        The canonical dump of 13.1: S (--static, the default), D(Y), or D(0) ... D(E).

    python3 -B worldgen/tests/reference/p4_timeline_reference.py territory DB YEAR OUT.ppm
        The territory image at scale 1 as a binary PPM (P6), and its SHA-256 on stdout.

    python3 -B worldgen/tests/reference/p4_timeline_reference.py fixture NAME OUT.db
        Writes the reference's copy of a fixture: t1-edges (worldgen/tests/data/timeline/) or one of the
        P3-07 fixtures (worldgen/tests/data/history-report/).

    python3 -B worldgen/tests/reference/p4_timeline_reference.py golden [PATH|-] [--inputs DIR] [--rules R]
        Writes worldgen/tests/golden/p4-01-timeline.txt ("-": stdout). World cases are read from DIR
        (default ~/.cache/the-unmapped/out/p5-06-int/report-inputs, the 0.27.0 worlds of rule set p5-06; --rules p5-04
        ~/.cache/the-unmapped/out/p5-04-int/report-inputs, the 0.26.0 ones; formerly
        ~/.cache/the-unmapped/out/p5-12-int/inputs, the 0.22.0 worlds, also for --rules p5-12; with --rules p4-03
        ~/.cache/the-unmapped/out/p4-03-tests/inputs, the 0.21.0 ones, no `name` lines; with --rules p4-01
        ~/.cache/the-unmapped/out/p4-01-tests/inputs, the 0.20.0 ones), one <case>.db per case, generated
        by worldgen-cli with the checkout's game/data as tools/ci/world-hashes.sh does (sample-42-1024:
        generate --seed 42 --map-size 1024).

    python3 -B worldgen/tests/reference/p4_timeline_reference.py distances
        The table of 7.3: for each entry of 7.2 floor(sqrt(floor(D / 256))) to the nearest colour of B,
        within its race and across races, with the nearest colour named.

    python3 -B worldgen/tests/reference/p4_timeline_reference.py check [--inputs DIR] [--quick]
        Self-checks: the worked colour table of 5.5, the 4 x 1 example of 5.6, the hand-checked facts of
        T1, the invariants of section 8 that the reference can state on every fixture (and input world),
        and the committed golden file against a fresh computation (skipped with --quick). Exit 1 on any
        disagreement.

Readings chosen where the spec is open are listed in READINGS below and in the test writer's report.
Never regenerate the golden file to make a C++ test pass.
"""

import hashlib
import os
import sqlite3
import struct
import sys
import tomllib

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import core_reference as cr  # noqa: E402
import p0_world_check as pw  # noqa: E402
import p3_report_reference as prr  # noqa: E402

REPO = os.path.normpath(os.path.join(_HERE, "..", "..", ".."))
GOLDEN = os.path.join(REPO, "worldgen", "tests", "golden", "p4-01-timeline.txt")
T1_DIR = os.path.join(REPO, "worldgen", "tests", "data", "timeline")
INPUTS_P4_01 = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p4-01-tests", "inputs")
INPUTS_P4_03 = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p4-03-tests", "inputs")
INPUTS_P5_12 = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p5-12-int", "inputs")
# P5-02 (0.23.0): the report reference's reference-built worlds (p3_report_reference.py build-inputs)
INPUTS_P5_02 = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p5-02-int", "report-inputs")
SHIPPED_BIOMES = os.path.join(REPO, "game", "data", "biomes")

# The rule set: "p4-03a" (default: P4-03 r4's colours and image, that is the P4-03a follow-ups, with P4-01 r3
# 16's region names, generator 0.22.0), "p5-12" (P4-03 r3's colours and image, the same 0.22.0 inputs), "p4-03"
# (generator 0.21.0, no name lines) or "p4-01" (P4-01's colours and image, generator 0.20.0).
RULES = "p5-06"
RULE_SETS = ("p4-01", "p4-03", "p5-12", "p4-03a", "p5-02", "p5-03", "p5-03a", "p5-04", "p5-06")
# P5-06 (0.27.0, the default rule set "p5-06"; P4-01 r7 = P5-06 12): the figure table and the ruled and served rows,
# ruler_at and D(Y)'s `ruler <civ> <figure>` lines (READINGS F1 to F4); schema 12 fixtures (empty figure and book
# tables, meta 0.27.0 and 12; READINGS F5). `--rules p5-04` keeps the 0.26.0 file.
INPUTS_P5_06 = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p5-06-int", "report-inputs")
# P5-04 (0.26.0, the default rule set "p5-04"; P4-01 r6): alliances and trades by year (12) and their dump lines;
# the 0.26.0 report inputs (phase 2). `--rules p5-03a` keeps the 0.25.0 file.
INPUTS_P5_04 = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p5-04-int", "report-inputs")
# P5-03 (0.24.0): the 0.24.0 report inputs (p3_report_reference.py build-inputs at 0.24.0; phase 2)
INPUTS_P5_03 = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p5-03-int", "report-inputs")
# P5-03a (0.25.0, the default rule set "p5-03a"): no timeline rule changes (no table or column is added); the 0.25.0
# report inputs (p3_report_reference.py build-inputs at 0.25.0; phase 2). `--rules p5-03` keeps the 0.24.0 file.
INPUTS_P5_03A = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p5-03a-int", "report-inputs")


def p403():
    return RULES in ("p4-03", "p5-12", "p4-03a", "p5-02", "p5-03", "p5-03a", "p5-04", "p5-06")


def p403a():
    """P4-03a (P4-03 r4): the r4 table of 7.2, the luma dim of 8.2 and half strength on free-free borders."""
    return RULES in ("p4-03a", "p5-02", "p5-03", "p5-03a", "p5-04", "p5-06")


def r3():
    """P4-01 r3 16 (P5-12): the static dump's `name` lines and the region_name rows."""
    return RULES in ("p5-12", "p4-03a", "p5-02", "p5-03", "p5-03a", "p5-04", "p5-06")


def r4():
    """P4-01 r4 (P5-02 13): sites, monsters, beasts and settlement endings in the dumps (READINGS P5)."""
    return RULES in ("p5-02", "p5-03", "p5-03a", "p5-04", "p5-06")


def r5():
    """P4-01 r5 (P5-03 18): race relations and settlement shares by year, and their dump lines."""
    return RULES in ("p5-03", "p5-03a", "p5-04", "p5-06")


def r6():
    """P4-01 r6 (P5-04 12): alliances and trades by year, and their dump lines."""
    return RULES in ("p5-04", "p5-06")


def r7():
    """P4-01 r7 (P5-06 12): figures by year (ruler_at and D(Y)'s ruler lines); schema 12."""
    return RULES == "p5-06"


def default_inputs():
    return INPUTS_P5_06 if r7() else INPUTS_P5_04 if r6() else INPUTS_P5_03A if RULES == "p5-03a" else INPUTS_P5_03 if r5() else INPUTS_P5_02 if r4() else INPUTS_P5_12 if r3() else INPUTS_P4_03 if p403() else INPUTS_P4_01


def generator_version():
    return "0.27.0" if r7() else "0.26.0" if r6() else "0.25.0" if RULES == "p5-03a" else "0.24.0" if r5() else "0.23.0" if r4() else "0.22.0" if r3() else "0.21.0" if p403() else "0.20.0"

READINGS = """
T1 13.4 cases: "the world cases of world-hashes.sh with history" are the seven cases geo-phrase-512,
   geo-seed-0-512, geo-seed-1-1024, geo-seed-42-512, hist-dense-1024, hist-races-elf-orc-1024 and
   hist-span-2000-512 (every case but empty-* and pattern-*); P3-07's extra r17 cases civs4-* and seed-0-1024
   are not world-hashes cases and are not included. With sample-42-1024 and the fixtures.
T2 13.4 fixtures: the twelve P3-07 fixtures (h1 ... h10, no geography layers) and T1, named by their file
   stem (t1-edges). Cases sorted by the bytes of the name.
T3 13.4 items 2 and 4: the year sets are sets (each year once), ascending; item 4 for a world whose history
   has no holder change omits "the smallest holder change year".
T4 13.1: every line of a dump ends with LF, the last included; SHA-256 of the bytes. A list with elements
   prints them separated by single spaces; an empty list prints "-". D(Y) has no line for an empty
   relation, alive, event or start list.
T5 5.6 step 1: "drawn by step 4" with river given means an entry of the paint list was painted on the
   cell (inside the map, water 0 or 1); a water-1 cell of class 0 adds no entry and is therefore fillable
   (it shows the holder's colour), as is a cell whose only entries were refused (lake or ocean cells are
   unfillable anyway).
T6 5.6 step 2 / 4: the shading factor of a filled cell is hillshade_factor of the elevation at that cell,
   the same factor its biome colour received in B.
T7 13.2 item 3 "a region freed (since = its last to_year)": the free since of a region is the largest
   to_year <= Y of its rows (section 4), which equals 6.2's "the to_year of the last row with
   from_year <= Y" because rows of a region do not overlap.
P4-03 readings:
A3a 9.4: a region's "run" is a maximal sequence of consecutive rows (in table order) with that region; the
    rows of a region split into two runs are not joined (the C++ refuses them earlier, "region_control
    row of region <r> out of order", P4-01). Rows are visited in order; for each row r, r's own check
    (to_year <= from_year) runs before the overlap check of the row before it with r (the engineer's
    order, accepted by the lead): own(r0), own(r1), pair(r0, r1), own(r2), pair(r1, r2), ...
    timeline_borders_test.cpp asserts it on rows where both orders would differ.
A3b 9.4: the texts are exactly those of the spec with the integers in decimal; nothing else is checked.
P1  8.2: a border cell that is drawn by step 3 or 4 (a lake or river cell), or has e < 0, is kind 0: its
    pixel is B (black), its region, east and south 0 (9.2: "v when kind is 1 or 2, else 0").
P2  8.2: dim is applied to fill[v] (the colour the caller passed for the cell's own region); the fill of
    the other regions O is not consulted, only their holders.
P3  9.2: territory_holders has territory_fill's size and id rules (min(largest id, 65535) + 1; ids outside
    1..65535 left out); render_territory treats holders[v] as 0 for v >= holders.size(), and with holders
    empty draws P4-01's image.
P4  7.3 item 4: the biome rows of B are the twelve `colour` values of game/data/biomes/*.toml; `check`
    confirms that every input world's table biome holds exactly these colours.
P5  9.6: the golden file keeps P4-01's case list and years; the `territory` lines are the image with the
    holders of territory_holders at that year (the CLI's call, 9.2).
P4-03a readings:
Q1  8.2 rule 3: "both sides free" is holders[v] = 0 and holders[w] = 0 for every w of O (one or two values);
    regions past the end of the holder vector are free. The rule applies only with a non-empty vector.
Q2  9.2: half(U) uses U before the border pass (shaded, then halved); on a kind-2 cell U is the biomes
    composite's pixel (such a cell is not drawn by steps 3 or 4 and has e >= 0).
Q3  P4-03a 5 (a): "the two header comment lines naming the P4-03 rules gain 'and P4-03a'": line 1 ends
    "with the colours of P4-03 and P4-03a", line 2 "(P4-03 9.6 and P4-03a)."
Q4  P4-03a 7 item 5 "the golden years of the 512 world cases": the years of the territory lines (scales 1
    and 3), and the years of the year lines at scale 1 (timeline_colours_p403a_test.cpp).
P5-06 readings (generator 0.27.0, rule set p5-06; P5-06 r3 12 = P4-01 r7):
F1  12 "D(Y) gains `ruler <civ> <figure>` per alive civilization with a recorded ruler at Y, ascending": the lines
    come after every earlier line of D(Y) (after P5-04's trade lines), one per civilization of the alive list
    (ascending id) whose ruler_at(c, Y) is not 0. The C++ dump of worldgen/tests/timeline_support.hpp had no
    ruler line before this change; its year_dump gains the same lines at the same place.
F2  12 ruler_at: a reign is a `ruled` row (a_type 6, b_type 3) taken from its figure's office_from (not the row's
    since: 6.4 rule 3, an accession may precede the event that opened the row) to the row's until, else E,
    half-open. 13 item 1 says reigns of a realm do not overlap; should two qualify, the one begun last (largest
    office_from, then largest figure id) is taken, as the engine does (Timeline::ruler_at, the latest reign
    begun wins). `check` counts such overlaps on the input worlds (none expected).
F3  12: by the half-open rule ruler_at(c, E) is 0 for every c (an open row ends at E), so D(E) has no ruler line;
    the present's ruler is ruler_at(c, E - 1). The engine does the same.
F4  The brief's "7.9's inclusive lookup" is the rendering lookup by event (since_event <= e <= until_event) of
    the form `under`; 12 defines ruler_at by years, which is what D(Y) uses.
F5  Fixtures at 0.27.0: the C++ writer gives the P3-07 fixtures and T1 (and P4-04's T2) the schema 12 tables
    (figure and book, empty) and meta generator_version 0.27.0 and schema_version 12; the rows are 0.26.0's
    (no fixture fills a figure). The P3-07 reference serves no 0.27.0 yet, so fixture_ddl and fixture_meta
    adjust its 0.26.0 output; before p5-06 they drop figure and book (schema.md holds version 12 since P5-06,
    which prr.ddl_statements does not filter), so `--rules p5-04` reproduces the 0.26.0 file.
"""

def fixture_names_p3():
    """The P3-07 fixtures: the twelve of P4-01 (h1 ... h10) under --rules p4-01; with P4-03's rules at 0.21.0
    also h11-v2 (P3-07 r19 25.13), which worldgen-tests' hrtest::fixture_names() lists at 0.21.0."""
    return prr.fixture_names()
T1_NAME = "t1-edges"
WORLD_CASES = ("geo-phrase-512", "geo-seed-0-512", "geo-seed-1-1024", "geo-seed-42-512", "hist-dense-1024",
               "hist-races-elf-orc-1024", "hist-span-2000-512", "sample-42-1024")

CIV, SETTLEMENT, REGION, FIGURE = 3, 4, 5, 6

# ---------------------------------------------------------------------------
# Colours (5.5)
# ---------------------------------------------------------------------------

TAG_COLOUR = cr.tag_value("UNM:COLR")
assert TAG_COLOUR == 0x524C4F433A4D4E55

BASE = {b"elf": (52, 150, 70), b"goblin": (190, 160, 50), b"dwarf": (160, 82, 45), b"faerie": (150, 90, 200),
        b"human": (50, 105, 200), b"orc": (200, 40, 35)}


def colour_hash(key):
    return cr.hash_words(TAG_COLOUR, cr.pack_bytes(key))


# P4-03 7.2: four explicit colours per shipped key; entry k mod 4 for the civilization of ordinal k.
TABLE_P403 = {
    b"elf": ((40, 190, 70), (150, 230, 150), (0, 140, 40), (130, 205, 40)),
    b"goblin": ((230, 195, 20), (250, 235, 110), (170, 125, 0), (205, 160, 60)),
    b"dwarf": ((220, 105, 25), (245, 165, 105), (150, 65, 15), (195, 80, 55)),
    b"faerie": ((175, 70, 225), (215, 160, 250), (115, 35, 165), (200, 110, 200)),
    b"human": ((0, 190, 195), (125, 235, 230), (0, 120, 130), (50, 215, 175)),
    b"orc": ((205, 30, 30), (240, 120, 110), (135, 15, 20), (230, 50, 80)),
}
# The "min to B" and "min within race" columns of 7.2, as stated there.
TABLE_P403_MINS = {b"elf": (109, 126), b"goblin": (88, 100), b"dwarf": (93, 78), b"faerie": (127, 97),
                   b"human": (98, 94), b"orc": (165, 93)}

# P4-03 r4 7.2 (P4-03a items 1 and 2): goblin 3 olive (140, 155, 30); goblin 1, elf 1 and human 1 darkened.
TABLE_P403A = {
    b"elf": ((40, 190, 70), (130, 215, 130), (0, 140, 40), (130, 205, 40)),
    b"goblin": ((230, 195, 20), (235, 215, 90), (170, 125, 0), (140, 155, 30)),
    b"dwarf": ((220, 105, 25), (245, 165, 105), (150, 65, 15), (195, 80, 55)),
    b"faerie": ((175, 70, 225), (215, 160, 250), (115, 35, 165), (200, 110, 200)),
    b"human": ((0, 190, 195), (100, 215, 215), (0, 120, 130), (50, 215, 175)),
    b"orc": ((205, 30, 30), (240, 120, 110), (135, 15, 20), (230, 50, 80)),
}
TABLE_P403A_MINS = {b"elf": (109, 126), b"goblin": (89, 89), b"dwarf": (93, 78), b"faerie": (127, 97),
                    b"human": (96, 94), b"orc": (165, 93)}
# P4-03 r4 7.5: L of every entry, as the spec's table states it.
LUMA_P403A = {b"elf": (131, 179, 86, 163), b"goblin": (185, 206, 124, 136), b"dwarf": (130, 182, 84, 111),
              b"faerie": (119, 186, 73, 147), b"human": (133, 180, 85, 160), b"orc": (82, 154, 51, 107)}
LUMA_BOUND = 175   # 8.2: n = 10 when L > 175


def table():
    """The colour table of 7.2 of the rule set (r4 under p4-03a, r3 otherwise)."""
    return TABLE_P403A if p403a() else TABLE_P403


def table_mins():
    return TABLE_P403A_MINS if p403a() else TABLE_P403_MINS


def luma(c):
    """P4-03 r4 7.5: floor((77 R + 150 G + 29 B) / 256)."""
    return (77 * c[0] + 150 * c[1] + 29 * c[2]) // 256


def race_base_colour(key):
    """key: bytes."""
    if p403() and key in table():
        return table()[key][0]
    if not p403() and key in BASE:
        return BASE[key]
    h = colour_hash(key)
    return tuple(48 + (((h >> (8 * j)) & 0xFF) * 160) // 255 for j in range(3))


def race_civilization_colour(key, ordinal):
    """P4-03 7.4 (under --rules p4-01: P4-01 5.5's shade of the base). ValueError for ordinal < 0."""
    if ordinal < 0:
        raise ValueError("race_civilization_colour: ordinal %d" % ordinal)
    if p403() and key in table():
        return table()[key][ordinal % 4]
    return civilization_shade(race_base_colour(key), ordinal)


def civilization_shade(base, ordinal):
    s = ordinal % 4
    if s == 0:
        return tuple(base)
    if s == 1:
        return tuple(c + ((255 - c) * 8) // 16 for c in base)
    if s == 2:
        return tuple((c * 10) // 16 for c in base)
    return tuple((c * 7) // 16 for c in base)


# ---------------------------------------------------------------------------
# The world's history tables
# ---------------------------------------------------------------------------

HIST_TABLES = ("region", "biome", "race", "age", "phase", "event", "event_participant", "event_cause",
               "civilization", "settlement", "settlement_census", "region_control", "relation")
REGION_NAME_COLS = ("region", "from_year", "name", "culture", "event")


class History:
    """The rows the timeline is defined on (section 3), in primary-key order."""

    def __init__(self, path):
        conn = prr.connect(path)
        try:
            self.meta = dict(conn.execute("SELECT key, value FROM meta").fetchall())
            t = {name: prr.pk_rows(conn, name)[1] for name in HIST_TABLES}
            has_names = conn.execute("SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
                                     "name = 'region_name'").fetchone()[0] == 1
            t["region_name"] = prr.pk_rows(conn, "region_name")[1] if has_names else []
            has_sites = conn.execute("SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
                                     "name = 'site'").fetchone()[0] == 1
            for tn in ("beast", "monster_kind", "beast_population", "monster", "site"):
                t[tn] = prr.pk_rows(conn, tn)[1] if has_sites else []
            has_rr = conn.execute("SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
                                  "name = 'race_relation'").fetchone()[0] == 1
            for tn in ("race_relation", "settlement_race"):   # schema 10 (P5-03 16.1)
                t[tn] = prr.pk_rows(conn, tn)[1] if has_rr else []
            has_fig = conn.execute("SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
                                   "name = 'figure'").fetchone()[0] == 1
            t["figure"] = prr.pk_rows(conn, "figure")[1] if has_fig else []   # schema 12 (P5-06 9.1)
        finally:
            conn.close()
        self.E = int(self.meta["history_end_year"])
        self.regions = [r[0] for r in t["region"]]
        self.geo_name = {r[0]: r[1] for r in t["region"]}
        self.region_names = t["region_name"]   # region, from_year, name, culture, event (primary-key order)
        self.race_key = {r[0]: r[1].encode("latin-1") for r in t["race"]}
        self.ages = t["age"]            # id, race, start_year, end_year, name
        self.phases = t["phase"]        # id, age, kind, start_year, end_year, trigger_event
        self.events = t["event"]        # id, year, end_year, kind, phase, region, settlement, salience
        self.parts = {}
        for p in t["event_participant"]:   # event, seq, role, entity_type, entity_id
            self.parts.setdefault(p[0], []).append(p)
        self.causes = {}
        for e, c in t["event_cause"]:
            self.causes.setdefault(e, []).append(c)
        self.civs = t["civilization"]   # id, name, race, culture, home_region, capital, founded, fevent, ended, eevent
        self.settlements = t["settlement"]
        self.census = {}
        for c in t["settlement_census"]:   # settlement, year, owner, kind, status, population
            self.census.setdefault(c[0], []).append(c)
        self.control = {}
        for c in t["region_control"]:      # region, from, to, civ, event
            self.control.setdefault(c[0], []).append(c)
        self.relations = t["relation"]     # a_type, a_id, b_type, b_id, kind, since, until, sev, uev
        # events of each year (section 4: year <= Y <= end_year), and starting in each year
        self.by_year = {}
        self.starting = {}
        for e in self.events:
            self.starting.setdefault(e[1], []).append(e[0])
            for y in range(e[1], e[2] + 1):
                self.by_year.setdefault(y, []).append(e[0])
        self.civ_index = {c[0]: i for i, c in enumerate(self.civs)}
        # P5-02 13 (schema 9)
        self.sites = t["site"]             # id, name, kind, region, x, y, settlement, founder, monster, oy, oe, ey, ee, ending
        self.monsters = t["monster"]       # id, name, kind, lair_site, born, born_event, died, death_event, slain_by, status, hoard
        self.beast_rows = t["beast_population"]
        self.race_relation = t.get("race_relation", [])      # race_a, race_b, year, disposition, band
        self.settlement_race = t.get("settlement_race", [])  # settlement, race, year, share, standing
        # P5-06 9.1 (schema 12): id, name, epithet, role, race, culture, civ, born, office_from, start_event, died,
        # death_event, death_region, death_settlement, status, reign_no, regnal, temper, skill
        self.figures = {f[0]: f for f in t["figure"]}
        self.kind_key = {}
        self.kind_ids = {}
        conn2 = prr.connect(path)
        try:
            for k in conn2.execute("SELECT id, key FROM event_kind ORDER BY id"):
                self.kind_key[k[0]] = k[1]
                self.kind_ids[k[1]] = k[0]
        finally:
            conn2.close()

    # --- section 4, at year Y ---------------------------------------------------------------------

    def region_name(self, region, y):
        """P4-01 r3 16 item 1: the name of r's region_name row with the largest from_year <= Y, else
        region.name (a linear scan of the stored rows)."""
        best = None
        for row in self.region_names:
            if row[0] == region and row[1] <= y and (best is None or row[1] > best[1]):
                best = row
        return best[2] if best is not None else self.geo_name[region]

    def holder(self, region, y):
        rows = self.control.get(region, [])
        match = [r for r in rows if r[1] <= y and (r[2] is None or y < r[2])]
        assert len(match) <= 1, "overlapping region_control rows"
        if match:
            r = match[0]
            return (region, r[3], r[1], r[4])
        since = max([r[2] for r in rows if r[2] is not None and r[2] <= y], default=0)
        return (region, 0, since, 0)

    def settlement_state(self, s, y):
        row = self.settlements[s - 1]
        assert row[0] == s
        if row[6] > y:
            return (s, 0, 0, 0, 0, 0, 0, 0) if r4() else (s, 0, 0, 0, 0, 0, 0)   # r4: the ending field too
        best = None
        for c in self.census.get(s, []):
            if c[1] <= y and (best is None or c[1] > best[1]):
                best = c
        assert best is not None, "settlement %d has no census row at or before %d" % (s, y)
        if r4():   # P5-02 13: SettlementState gains ending, 0 while standing at Y, else settlement.ending
            return (s, 1, best[2], best[3], best[4], best[5], best[1], 0 if best[4] in (1, 2) else row[14])
        return (s, 1, best[2], best[3], best[4], best[5], best[1])

    # --- P5-02 13: sites, monsters, beasts at year Y -----------------------------------------------------

    def sites_existing(self, y):
        return [x[0] for x in self.sites if x[9] <= y]

    # --- P5-03 18 (P4-01 r5)
    def race_relation_at(self, a, b, y):
        """RaceRelationState (race_a, race_b, met, disposition, band) at Y: met iff the pair's peoples_met has year
        <= Y; then the row with the largest year <= Y; else 0, 0. Symmetric (the arguments ordered)."""
        if a not in self.race_key or b not in self.race_key:
            raise ValueError("unknown race")
        if not 0 <= y <= self.E:
            raise ValueError("year outside 0..E")
        a, b = min(a, b), max(a, b)
        met = False
        for e in self.events:
            if e[1] > y:
                break
            if self.kind_key.get(e[3]) == "peoples_met":
                ps = sorted(self.parts.get(e[0], []))
                if (ps[0][4], ps[1][4]) == (a, b):
                    met = True
        if not met:
            return (a, b, 0, 0, 0)
        best = None
        for r in self.race_relation:
            if (r[0], r[1]) == (a, b) and r[2] <= y:
                best = r
        return (a, b, 1, best[3], best[4]) if best is not None else (a, b, 1, 0, 0)

    def settlement_races(self, s, y):
        """[(race, share, standing)] of a settlement standing at Y (the census reading): per race the last row with
        year <= Y and share > 0, ascending race; empty when not standing at Y."""
        st = self.settlement_state(s, y)
        if not (st[1] and st[4] in (1, 2)):
            return []
        last = {}
        for r in self.settlement_race:
            if r[0] == s and r[2] <= y:
                last[r[1]] = r
        return [(g, r[3], r[4]) for g, r in sorted(last.items()) if r[3] > 0]

    def site_at(self, sid, y):
        """SiteState: (site, kind, region, x, y, settlement, monster, founder, open, ending); open iff ended_year
        NULL or Y < ended_year."""
        x = self.sites[sid - 1]
        op = x[11] is None or y < x[11]
        return (x[0], x[2], x[3], x[4], x[5], x[6], x[8], x[7], 1 if op else 0, 0 if op else x[13])

    def monster_at(self, mid, y):
        """MonsterState: (monster, kind, alive, awake, lair_site, region) by 13's definitions."""
        m = self.monsters[mid - 1]
        alive = m[4] <= y and (m[6] is None or y < m[6])
        awake = False
        wake = {"monster_appeared", "undead_rose", "monster_woke"}
        sleep = {"monster_slept", "monster_driven_off"}
        for e in self.events:
            if e[1] > y:
                break
            key = self.kind_key.get(e[3])
            if key in wake or key in sleep:
                if any(p[3] == 8 and p[4] == mid and p[2] == 1 for p in self.parts.get(e[0], [])):
                    awake = key in wake
        lair, region = 0, 0
        for x in self.sites:
            if x[2] == 1 and x[8] == mid and x[9] <= y and (x[11] is None or y < x[11]):
                lair, region = x[0], x[3]
        return (mid, m[2], 1 if alive else 0, 1 if (alive and awake) else 0, lair, region)

    def beast_count(self, region, beast, y):
        best = 0
        for r in self.beast_rows:
            if r[0] == region and r[1] == beast and r[2] <= y:
                best = r[3]
        return best

    # P5-04 12 (P4-01 r6): alliances and trades in force at Y (since <= Y < until)
    def alliances_at(self, y):
        return sorted((r[1], r[3], r[5], r[7]) for r in self.relations
                      if r[4] == "alliance" and r[5] <= y and (r[6] is None or y < r[6]))

    def allies_of(self, c, y):
        return sorted(b if a == c else a for a, b, _s, _e in self.alliances_at(y) if c in (a, b))

    def alliance_groups_at(self, y):
        """Connected components of the alliance graph among the civilizations alive at Y with an ally."""
        alive = set(self.alive(y))
        nb = {}
        for a, b, _s, _e in self.alliances_at(y):
            if a in alive and b in alive:
                nb.setdefault(a, set()).add(b)
                nb.setdefault(b, set()).add(a)
        out, seen = [], set()
        for c in sorted(nb):
            if c in seen:
                continue
            comp, todo = set(), [c]
            while todo:
                x = todo.pop()
                if x not in comp:
                    comp.add(x)
                    todo += sorted(nb[x] - comp)
            seen |= comp
            out.append(sorted(comp))
        return out

    def trades_at(self, y):
        return sorted((r[1], r[3]) for r in self.relations
                      if r[4] == "trade" and r[5] <= y and (r[6] is None or y < r[6]))

    # P5-06 12 (P4-01 r7): figures by year
    def reigns(self, civ):
        """The recorded reigns of civ: (office_from, end, figure) per `ruled` row (a figure 6, b civilization 3),
        end = the row's until, else E (12). Ascending (office_from, figure)."""
        out = []
        for r in self.relations:
            if r[4] == "ruled" and r[0] == FIGURE and r[2] == CIV and r[3] == civ:
                f = self.figures.get(r[1])
                if f is None:
                    raise ValueError("ruled row of figure %d names no figure" % r[1])
                out.append((f[8], self.E if r[6] is None else r[6], r[1]))
        return sorted(out)

    def ruler_at(self, civ, y):
        """P5-06 12: the recorded ruler with office_from <= Y < the reign's end; 0 none. Reading F2: when more
        than one reign qualifies, the one begun last (largest office_from, then largest figure id)."""
        if not 0 <= y <= self.E:
            raise ValueError("year outside 0..E")
        got = [f for a, b, f in self.reigns(civ) if a <= y < b]
        return got[-1] if got else 0

    def reign_overlaps(self, civ):
        """The years at which two or more reigns of civ qualify (13 item 1 says none)."""
        rs = self.reigns(civ)
        out = []
        for i in range(len(rs)):
            for j in range(i + 1, len(rs)):
                lo, hi = max(rs[i][0], rs[j][0]), min(rs[i][1], rs[j][1])
                if lo < hi:
                    out.append((rs[i][2], rs[j][2], lo, hi))
        return out

    def relations_in_force(self, y):
        return [i for i, r in enumerate(self.relations) if r[5] <= y and (r[6] is None or y < r[6])]

    def alive(self, y):
        return [c[0] for c in self.civs if c[6] <= y and (c[8] is None or y < c[8])]

    def phase(self, y):
        if y == self.E:
            return 0
        m = [p[0] for p in self.phases if p[3] <= y <= p[4]]
        assert len(m) == 1
        return m[0]

    def age(self, y):
        if y == self.E:
            return self.ages[-1][0]
        m = [a[0] for a in self.ages if a[2] <= y <= a[3]]
        assert len(m) == 1
        return m[0]

    def events_of(self, y):
        return list(self.by_year.get(y, [])) if y != self.E else []

    def events_starting(self, y):
        return list(self.starting.get(y, [])) if y != self.E else []

    def previous_event_year(self, y):
        c = [e[1] for e in self.events if e[1] < y]
        return max(c) if c else None

    def next_event_year(self, y):
        c = [e[1] for e in self.events if e[1] > y]
        return min(c) if c else None

    # --- independent of Y -------------------------------------------------------------------------

    def change_years(self):
        s = set()
        for rows in self.control.values():
            for r in rows:
                s.add(r[1])
                if r[2] is not None:
                    s.add(r[2])
        return sorted(s)

    def events_of_entity(self, typ, i):
        out = []
        for e in self.events:
            place = (typ == REGION and e[5] == i) or (typ == SETTLEMENT and e[6] == i)
            if place or any(p[3] == typ and p[4] == i for p in self.parts.get(e[0], [])):
                out.append(e[0])
        return out

    def effects(self, eid):
        return [e[0] for e in self.events if eid in self.causes.get(e[0], [])]

    def colours(self):
        out = []
        seen = {}
        for c in self.civs:
            k = seen.get(c[2], 0)
            seen[c[2]] = k + 1
            out.append(race_civilization_colour(self.race_key[c[2]], k))
        return out


def control_run_error(rows):
    """P4-03 9.4: the FatalError text Timeline::build gives for these region_control rows (region, from,
    to or None, civ, event) in table order, or None. A run is a maximal sequence of consecutive rows of one
    region. Rows in order; for each row r: first r's own check (to <= from), then the pair check of the
    row before it with r (p open, or p.to > r.from: overlap at r.from). Readings A3a, A3b."""
    prev = None
    for r in rows:
        if r[2] is not None and not r[2] > r[1]:
            return "timeline: region_control row (%d, %d) ends at %d, not after it starts" % (r[0], r[1], r[2])
        if prev is not None and prev[0] == r[0]:
            if prev[2] is None or prev[2] > r[1]:
                return "timeline: region_control rows of region %d overlap at year %d" % (r[0], r[1])
        prev = r
    return None


# ---------------------------------------------------------------------------
# Dumps (13.1)
# ---------------------------------------------------------------------------

def ids(lst):
    return " ".join(str(x) for x in lst) if lst else "-"


def year_dump(h, y):
    out = ["year %d phase %d age %d" % (y, h.phase(y), h.age(y))]
    for r in h.regions:
        out.append("hold %d %d %d %d" % h.holder(r, y))
    for s in h.settlements:
        st = h.settlement_state(s[0], y)
        out.append(("settlement" + " %d" * len(st)) % st)
    for i in h.relations_in_force(y):
        out.append("relation %d" % i)
    for c in h.alive(y):
        out.append("alive %d" % c)
    for e in h.events_of(y):
        out.append("event %d" % e)
    for e in h.events_starting(y):
        out.append("start %d" % e)
    if r4():   # P5-02 13 (READINGS P5): the open sites and the living monsters with their awake flag
        for sid in h.sites_existing(y):
            st = h.site_at(sid, y)
            if st[8]:
                out.append("site %d" % sid)
        for m in h.monsters:
            ms = h.monster_at(m[0], y)
            if ms[2]:
                out.append("monster %d %d %d %d" % (m[0], ms[3], ms[4], ms[5]))
    if r5():   # P5-03 18: every pair ascending, then the mix of each standing settlement of more than one race
        races = sorted(h.race_key)
        for i, a in enumerate(races):
            for b in races[i + 1:]:
                out.append("races %d %d %d %d %d" % h.race_relation_at(a, b, y))
        for s in h.settlements:
            mx = h.settlement_races(s[0], y)
            if len(mx) > 1:
                for g, sh, st in mx:
                    out.append("mix %d %d %d %d" % (s[0], g, sh, st))
    if r6():   # P5-04 12 (reading T1: after every earlier line of D(Y)): the alliances, then the trades in force
        for a, b, since, _e in h.alliances_at(y):
            out.append("alliance %d %d %d" % (a, b, since))
        for r in sorted((r for r in h.relations if r[4] == "trade" and r[5] <= y and (r[6] is None or y < r[6])),
                        key=lambda r: (r[1], r[3])):
            out.append("trade %d %d %d" % (r[1], r[3], r[5]))
    if r7():   # P5-06 12 (READINGS F1: after every earlier line of D(Y)): the ruler of each alive civilization
        for c in h.alive(y):
            f = h.ruler_at(c, y)
            if f:
                out.append("ruler %d %d" % (c, f))
    return ("\n".join(out) + "\n").encode("ascii")


def static_dump(h):
    out = ["end %d" % h.E]
    for y in h.change_years():
        out.append("change %d" % y)
    for c, rgb in zip(h.civs, h.colours()):
        out.append("colour %d %d %d %d" % ((c[0],) + tuple(rgb)))
    # one pass per entity type, each list ascending and each event once (section 4)
    civ_ev = {c[0]: [] for c in h.civs}
    reg_ev = {r: [] for r in h.regions}
    set_ev = {s[0]: [] for s in h.settlements}
    eff = {e[0]: [] for e in h.events}
    for e in h.events:
        eid = e[0]
        ps = h.parts.get(eid, [])
        for c in sorted({p[4] for p in ps if p[3] == CIV}):
            civ_ev[c].append(eid)
        rs = {p[4] for p in ps if p[3] == REGION}
        if e[5]:
            rs.add(e[5])
        for r in sorted(rs):
            reg_ev[r].append(eid)
        ss = {p[4] for p in ps if p[3] == SETTLEMENT}
        if e[6]:
            ss.add(e[6])
        for s in sorted(ss):
            set_ev[s].append(eid)
        for c in h.causes.get(eid, []):
            eff[c].append(eid)
    for c in h.civs:
        out.append("civ %d %s" % (c[0], ids(civ_ev[c[0]])))
    for r in h.regions:
        out.append("region %d %s" % (r, ids(reg_ev[r])))
    if r3():
        # P4-01 r3 16 item 4: after the region lines, one line per region_name row in primary-key order.
        for row in h.region_names:
            out.append("name %d %d %s" % (row[0], row[1], row[2]))
    for s in h.settlements:
        out.append("settlement %d %s" % (s[0], ids(set_ev[s[0]])))
    if r4():   # P5-02 13 (READINGS P5): site, monster and beast lines after the settlement lines
        for x in h.sites:
            out.append("site %d %d %d %d %d %d %d %d %d %s %d" % (x[0], x[2], x[3], x[4], x[5], x[6], x[7], x[8], x[9],
                                                               "-" if x[11] is None else str(x[11]), x[13]))
        for m in h.monsters:
            out.append("monster %d %d %d %s %d %d" % (m[0], m[2], m[4], "-" if m[6] is None else str(m[6]), m[9],
                                                     m[3]))
        for r in h.beast_rows:
            out.append("beast %d %d %d %d" % tuple(r))
    if r5():   # P5-03 18: after the beast lines, every row of the two tables in key order
        for r in h.race_relation:
            out.append("relation %d %d %d %d %d" % tuple(r))
        for r in h.settlement_race:
            out.append("share %d %d %d %d %d" % tuple(r))
    for e in h.events:
        out.append("effects %d %s" % (e[0], ids(eff[e[0]])))
    return ("\n".join(out) + "\n").encode("ascii")


def sha(b):
    return hashlib.sha256(b).hexdigest()


def years_hash(h):
    m = hashlib.sha256()
    for y in range(0, h.E + 1):
        m.update(year_dump(h, y))
    return m.hexdigest()


def golden_years(h):
    s = {0, h.E - 1, h.E, h.E // 2}
    s.update(a[2] for a in h.ages)
    s.update(h.change_years())
    return sorted(s)


def territory_years(h):
    s = {h.E // 2, h.E}
    ch = h.change_years()
    if ch:
        s.add(ch[0])
    return sorted(s)


# ---------------------------------------------------------------------------
# Composites (P1-00 10.2 steps 1 to 4, P1-04 9, P0-04 amendment 18 (e)) and territory (5.6)
# ---------------------------------------------------------------------------

OCEAN_STOPS = ((0, (120, 170, 205)), (200, (88, 140, 190)), (1000, (56, 104, 168)), (5000, (32, 66, 130)),
               (30000, (16, 34, 82)))
LAKE = (64, 128, 204)
RIVER_NO_CLASS = (52, 100, 200)
BLACK = (0, 0, 0)
MAGENTA = (255, 0, 255)
DX = (1, 1, 0, -1, -1, -1, 0, 1)   # P1-00 2.3, codes 0 to 7


def ramp(stops, t):
    if t < stops[0][0]:
        return stops[0][1]
    if t >= stops[-1][0]:
        return stops[-1][1]
    for k in range(len(stops) - 1):
        t0, c0 = stops[k]
        t1, c1 = stops[k + 1]
        if t0 <= t < t1:
            return tuple(a + (2 * (b - a) * (t - t0) + (t1 - t0)) // (2 * (t1 - t0)) for a, b in zip(c0, c1))
    raise AssertionError("ramp")


def river_colour(k):
    return {1: (70, 125, 215), 2: (55, 110, 210), 3: (40, 95, 200), 4: (30, 80, 190),
            5: (25, 65, 175)}.get(k, (20, 50, 160))


def hillshade(elev, w, h, x, y):
    def ep(xx, yy):
        xx = min(max(xx, 0), w - 1)
        yy = min(max(yy, 0), h - 1)
        return max(elev[yy * w + xx], 0)
    gx = ep(x + 1, y) - ep(x - 1, y)
    gy = ep(x, y + 1) - ep(x, y - 1)
    return min(max(256 + (gx + gy) // 2, 160), 352)


def shade(c, f):
    return tuple(min(255, (v * f + 128) >> 8) for v in c)


class Base:
    """Per cell at scale 1: B (the regions composite), the factor, and v when fillable (else 0)."""

    def __init__(self, w, h, elevation, biome, region, biome_colours, water=None, river=None, flow_dir=None):
        self.w, self.h = w, h
        n = w * h
        table = dict(biome_colours)   # id -> 0xRRGGBB (first row of an id; ids are unique)
        px = [None] * n
        self.factor = [0] * n
        for y in range(h):
            for x in range(w):
                i = y * w + x
                e = elevation[i]
                f = hillshade(elevation, w, h, x, y)
                self.factor[i] = f
                if e < 0:
                    px[i] = ramp(OCEAN_STOPS, -e)
                else:
                    c = table.get(biome[i])
                    rgb = MAGENTA if c is None else ((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF)
                    px[i] = shade(rgb, f)
        unbordered = list(px)   # step 2: the biomes composite's pixel (U of 8.2 rule 3 on land cells)
        drawn = [False] * n   # drawn by step 3 or 4
        if water is not None:
            for i in range(n):
                if water[i] == 2:
                    px[i] = LAKE
                    drawn[i] = True
            if river is None:
                for i in range(n):
                    if water[i] == 1:
                        px[i] = RIVER_NO_CLASS
                        drawn[i] = True
            else:
                lst = []
                for y in range(h):
                    for x in range(w):
                        i = y * w + x
                        if water[i] != 1:
                            continue
                        k = river[i]
                        if k == 0:
                            continue
                        lst.append((x, y, k))
                        if flow_dir is not None:
                            d = flow_dir[i]
                            if d <= 7 and d % 2 == 1:
                                lst.append((x + DX[d], y, k))
                        if k >= 4:
                            lst.append((x + 1, y, k))
                            lst.append((x, y + 1, k))
                lst.sort(key=lambda en: en[2])   # stable: equal k keep list order
                for x, y, k in lst:
                    if 0 <= x < w and 0 <= y < h:
                        j = y * w + x
                        if water[j] <= 1:
                            px[j] = river_colour(k)
                            drawn[j] = True
        border = [False] * n
        east = [0] * n    # the E neighbour's value when it makes the cell a border cell, else 0 (P4-03 8.2)
        south = [0] * n   # the same for the S neighbour
        for y in range(h):
            for x in range(w):
                i = y * w + x
                v = region[i]
                if v == 0:
                    continue
                if x + 1 < w and region[i + 1] > 0 and region[i + 1] != v:
                    east[i] = region[i + 1]
                if y + 1 < h and region[i + w] > 0 and region[i + w] != v:
                    south[i] = region[i + w]
                border[i] = bool(east[i] or south[i])
                if border[i]:
                    px[i] = BLACK
        self.B = px
        self.drawn = drawn
        self.border = border
        # P4-01 5.6: v when fillable, else 0. P4-03 8.2: kind 0 B only, 1 fillable, 2 inner border candidate.
        cand = [region[i] > 0 and elevation[i] >= 0 and not drawn[i] for i in range(n)]
        self.v = [region[i] if (cand[i] and not border[i]) else 0 for i in range(n)]
        self.kind = [(2 if border[i] else 1) if cand[i] else 0 for i in range(n)]
        self.region = [region[i] if cand[i] else 0 for i in range(n)]
        self.east = [east[i] if self.kind[i] == 2 else 0 for i in range(n)]
        self.south = [south[i] if self.kind[i] == 2 else 0 for i in range(n)]
        # P4-03 r4 9.2: half(U) on kind-2 cells, (0, 0, 0) elsewhere
        self.half = [tuple((ch * 8) // 16 for ch in unbordered[i]) if self.kind[i] == 2 else BLACK
                     for i in range(n)]

    def pixel(self, i, fill, holders):
        """The colour of cell i at scale 1 (P4-01 5.6; with P4-03 8.2 unless --rules p4-01)."""
        v = self.v[i]
        if v:
            if v < len(fill) and fill[v] is not None:
                return shade(fill[v], self.factor[i])
            return self.B[i]
        if p403() and self.kind[i] == 2 and holders:
            def hold(r):
                return holders[r] if r < len(holders) else 0
            r = self.region[i]
            c = hold(r)
            others = [o for o in (self.east[i], self.south[i]) if o]
            if c != 0 and all(hold(o) == c for o in others) and r < len(fill) and fill[r] is not None:
                return shade(dim(fill[r]), self.factor[i])
            if p403a() and c == 0 and all(hold(o) == 0 for o in others):
                return self.half[i]   # r4 rule 3: both sides free
        return self.B[i]

    def render(self, fill, holders=(), scale=1):
        """fill: list indexed by region id of an (r, g, b) or None; holders: list indexed by region id of
        the holding civilization (0 free), may be empty (no inner border drawn)."""
        w, h = self.w, self.h
        out = bytearray(3 * w * h * scale * scale)
        for y in range(h):
            for x in range(w):
                i = y * w + x
                c = self.pixel(i, fill, holders)
                for dy in range(scale):
                    row = (y * scale + dy) * w * scale
                    for dx in range(scale):
                        o = 3 * (row + x * scale + dx)
                        out[o:o + 3] = bytes(c)
        return bytes(out)


def dim(c):
    """P4-03 8.2: each channel floor(c n / 16), n = 12 (r3); r4 (p4-03a): n = 10 when L(c) > 175."""
    n = 10 if p403a() and luma(c) > LUMA_BOUND else 12
    return tuple((v * n) // 16 for v in c)


def index_size(h):
    """P4-01 5.3 (territory_fill), P4-03 9.2 (territory_holders): min(largest region id, 65535) + 1, 1
    with no regions; region ids outside 1..65535 are left out."""
    return (min(max(h.regions), 65535) + 1) if h.regions else 1


def territory_fill(h, colours, y):
    fill = [None] * index_size(h)
    for r in h.regions:
        if not 1 <= r <= 65535:
            continue
        _, civ, _, _ = h.holder(r, y)
        if civ:
            fill[r] = colours[h.civ_index[civ]]
    return fill


def territory_holders(h, y):
    """P4-03 9.2: index region id, the holder at y (0 free); [0] = 0."""
    if not 0 <= y <= h.E:
        raise ValueError("timeline: year %d outside 0 to %d" % (y, h.E))
    out = [0] * index_size(h)
    for r in h.regions:
        if 1 <= r <= 65535:
            out[r] = h.holder(r, y)[1]
    return out


def territory_image(h, base, colours, y):
    """The territory image at scale 1 as worldgen-cli render --composite territory --year y draws it."""
    return base.render(territory_fill(h, colours, y), territory_holders(h, y) if p403() else ())


# ---------------------------------------------------------------------------
# P4-03 7.3: the colour distance rule (checked; never used for output)
# ---------------------------------------------------------------------------

def distance(a, b):
    """D(a, b) = (512 + rm)(R1 - R2)^2 + 1024 (G1 - G2)^2 + (767 - rm)(B1 - B2)^2, rm = floor((R1 + R2) / 2)."""
    rm = (a[0] + b[0]) // 2
    return (512 + rm) * (a[0] - b[0]) ** 2 + 1024 * (a[1] - b[1]) ** 2 + (767 - rm) * (a[2] - b[2]) ** 2


def isqrt_floor(n):
    import math
    return math.isqrt(n)


def distance_units(d):
    """floor(sqrt(floor(D / 256))), the unit of the 'min' columns of 7.2."""
    return isqrt_floor(d // 256)


def shipped_biome_colours():
    """The `colour` of every biome of the shipped game/data (key, (r, g, b)), sorted by key. The table
    `biome` of a world holds exactly these (checked against an input world in `check`)."""
    out = []
    for name in sorted(os.listdir(SHIPPED_BIOMES)):
        if not name.endswith(".toml"):
            continue
        with open(os.path.join(SHIPPED_BIOMES, name), "rb") as f:
            d = tomllib.load(f)
        for key, b in d["biome"].items():
            c = b["colour"]
            out.append((key, ((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF)))
    return out


def background_set(biome_rows=None):
    """B of 7.3 as (label, colour): the ocean ramp for t = 0 ... 30000, the lake colour, the river colours
    k = 1 ... 6 and the colour of every row of table biome (biome_rows: (label, colour), default the
    shipped data)."""
    seen = {}
    for t in range(0, 30001):
        c = ramp(OCEAN_STOPS, t)
        seen.setdefault(c, "ocean depth %d" % t)
    out = [(lab, c) for c, lab in seen.items()]
    out.append(("lake", LAKE))
    out += [("river %d" % k, river_colour(k)) for k in range(1, 7)]
    out += [("biome %s" % lab, c) for lab, c in (biome_rows if biome_rows is not None else shipped_biome_colours())]
    return out


C1_MIN = 256 * 80 * 80
C2_MIN = 256 * 70 * 70
C3_MIN = 256 * 80 * 80


def distance_table(biome_rows=None):
    """Per (key, entry): (nearest of B in units, its label, nearest of the race, nearest of other races)."""
    bg = background_set(biome_rows)
    rows = []
    for key, entries in table().items():
        for k, c in enumerate(entries):
            dB, lab = min((distance(c, b), l) for l, b in bg)
            dW = min(distance(c, o) for j, o in enumerate(entries) if j != k)
            dA = min(distance(c, o) for k2, es in table().items() if k2 != key for o in es)
            rows.append((key, k, c, dB, lab, dW, dA))
    return rows


def read_layers(path):
    conn = sqlite3.connect("file:%s?mode=ro&immutable=1" % path, uri=True)
    try:
        layers = {}
        for name, data in conn.execute("SELECT name, data FROM layer ORDER BY id"):
            tcode, _codec, w, hh, vals = pw.decode_blob(bytes(data))
            layers[name] = (w, hh, tcode, vals)
        biome = conn.execute("SELECT id, colour FROM biome ORDER BY id").fetchall()
    finally:
        conn.close()
    return layers, biome


def world_base(path):
    layers, biome = read_layers(path)
    w, hh, _, elev = layers["elevation"]

    def opt(name):
        return layers[name][3] if name in layers else None
    return Base(w, hh, elev, layers["biome"][3], layers["region"][3], biome, opt("water"), opt("river"),
                opt("flow_dir"))


# ---------------------------------------------------------------------------
# Fixtures (13.3)
# ---------------------------------------------------------------------------

def scratch_dir():
    d = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p4-01-tests", "fixtures-ref")
    os.makedirs(d, exist_ok=True)
    return d


def layer_blob(tname, w, h, cells):
    tcode = pw.TYPE_CODE[tname]
    payload = pw.encode_delta_varint(cells)
    return b"UMLY" + struct.pack("<HBBIIQ", 1, tcode, 1, w, h, len(payload)) + payload


def load_t1():
    with open(os.path.join(T1_DIR, T1_NAME + ".toml"), "rb") as f:
        return tomllib.load(f)


FIGURE_TABLES = ("figure", "book")   # schema 12 (P5-06 9.1, 9.3)


def fixture_ddl():
    """The DDL of a fixture of the rule set's schema. schema.md holds version 12 since P5-06, so before p5-06 the
    figure and book tables are left out (the P3-07 reference serves schema 11 and older only; READINGS F5)."""
    out = prr.ddl_statements()
    if not r7():
        return [s for s in out if s.split(" ")[2] not in FIGURE_TABLES]
    have = {s.split(" ")[2] for s in out}
    assert all(t in have for t in FIGURE_TABLES), "schema.md lacks the schema 12 tables"
    return out


def fixture_meta(meta):
    """READINGS F5: a 0.27.0 fixture is the 0.26.0 one with meta generator_version 0.27.0 and schema_version 12."""
    if r7():
        meta = dict(meta, generator_version="0.27.0", schema_version="12")
    return meta


def write_p3_fixture(name, out):
    """prr.write_fixture with the rule set's schema (fixture_ddl, fixture_meta)."""
    meta, rows = prr.expand_fixture(prr.load_fixture(name))
    meta = fixture_meta(meta)
    if os.path.exists(out):
        os.remove(out)
    conn = sqlite3.connect(out)
    try:
        for s in fixture_ddl():
            conn.execute(s)
        for k in sorted(meta):
            conn.execute("INSERT INTO meta VALUES (?, ?)", (k, meta[k]))
        for table, rs in rows.items():
            for r in rs:
                conn.execute('INSERT INTO "%s" VALUES (%s)' % (table, ", ".join("?" * len(r))), r)
        conn.commit()
    finally:
        conn.close()


def write_t1(out):
    fx = load_t1()
    meta, rows = prr.expand_fixture(fx)
    meta = fixture_meta(meta)
    if os.path.exists(out):
        os.remove(out)
    conn = sqlite3.connect(out)
    try:
        for s in fixture_ddl():
            conn.execute(s)
        for k in sorted(meta):
            conn.execute("INSERT INTO meta VALUES (?, ?)", (k, meta[k]))
        for table, rs in rows.items():
            for r in rs:
                conn.execute('INSERT INTO "%s" VALUES (%s)' % (table, ", ".join("?" * len(r))), r)
        for i, L in enumerate(fx.get("layer", [])):
            assert len(L["cells"]) == L["width"] * L["height"]
            conn.execute("INSERT INTO layer VALUES (?, ?, ?, ?, ?, ?)",
                         (i + 1, L["name"], L["width"], L["height"], L["type"],
                          layer_blob(L["type"], L["width"], L["height"], L["cells"])))
        conn.commit()
    finally:
        conn.close()


def fixture_db(name):
    out = os.path.join(scratch_dir(), name + ".db")
    if name == T1_NAME:
        write_t1(out)
    else:
        write_p3_fixture(name, out)
    return out


# ---------------------------------------------------------------------------
# Golden file (13.4)
# ---------------------------------------------------------------------------

def case_lines(name, path, is_fixture):
    h = History(path)
    lines = ["%s static %s" % (name, sha(static_dump(h)))]
    for y in golden_years(h):
        lines.append("%s year %d %s" % (name, y, sha(year_dump(h, y))))
    lines.append("%s years %s" % (name, years_hash(h)))
    if not is_fixture:
        base = world_base(path)
        colours = h.colours()
        for y in territory_years(h):
            lines.append("%s territory %d %s" % (name, y, sha(territory_image(h, base, colours, y))))
    else:
        for t, hx in prr.detail_tables(path):
            lines.append("%s table %s %s" % (name, t, hx))
    return lines


def golden_text(inputs):
    cases = [(n, fixture_db(n), True) for n in fixture_names_p3() + (T1_NAME,)]
    cases += [(n, os.path.join(inputs, n + ".db"), False) for n in WORLD_CASES]
    lines = []
    for name, path, is_fixture in sorted(cases, key=lambda c: c[0].encode()):
        if not os.path.exists(path):
            raise SystemExit("missing input world %s" % path)
        lines += case_lines(name, path, is_fixture)
    if p403():
        if p403a():   # P4-03a 5 (a): the two lines naming the P4-03 rules gain "and P4-03a"
            first = ["# P4-01 timeline golden values (docs/specs/p4-01-timeline.md 13.4), generator %s, with the colours"
                     " of P4-03 and P4-03a" % generator_version(),
                     "# and territory image of docs/specs/p4-03-history-view.md 7 and 8 (P4-03 9.6 and P4-03a)."]
        else:
            first = ["# P4-01 timeline golden values (docs/specs/p4-01-timeline.md 13.4), generator %s, with the colours"
                     % generator_version(),
                     "# and territory image of docs/specs/p4-03-history-view.md 7 and 8 (P4-03 9.6)."]
        if r3():
            first.append("# Region names by year (P4-01 r3 16): S gains a line `name <region> <from_year> <name>` per "
                         "region_name row.")
        if r4():
            first.append("# P5-02 13 (P4-01 r4): S gains `site <id> <kind> <region> <x> <y> <settlement> <founder> "
                         "<monster> <origin_year> <ended_year|-> <ending>`,")
            first.append("# `monster <id> <kind> <born> <died|-> <status> <lair_site>` and `beast <region> <beast> "
                         "<year> <count>` after the settlement lines;")
            first.append("# D(Y)'s settlement lines gain the ending at Y, and D(Y) ends with `site <id>` per open site "
                         "and `monster <id> <awake> <lair> <region>`.")
        if r7():
            first.append("# P5-06 12 (P4-01 r7): D(Y) ends with `ruler <civ> <figure>` per alive civilization with a "
                         "recorded ruler at Y, ascending.")
    else:
        first = ["# P4-01 timeline golden values (docs/specs/p4-01-timeline.md 13.4), generator %s."
                 % generator_version()]
    head = first + [
            "# Written by worldgen/tests/reference/p4_timeline_reference.py golden; never edited by hand.",
            "# Dumps S and D(Y) of 13.1; cases ascending: the world cases of tools/ci/world-hashes.sh with history,",
            "# sample-42-1024 (generate --seed 42 --map-size 1024), the P3-07 fixtures and T1 (t1-edges).",
            "#   <case> static <sha256 of S>",
            "#   <case> year <Y> <sha256 of D(Y)>    Y in {0, E - 1, E, E / 2} + age starts + holder change years",
            "#   <case> years <sha256 of D(0) D(1) ... D(E)>",
            "#   <case> territory <Y> <sha256 of the RGB buffer at scale 1>   world cases; Y in {first change, E / 2, E}",
            "#   <fixture> table <name> <hex>   hash --detail's table lines of the reference's copy of a fixture"]
    return "\n".join(head + lines) + "\n"


# ---------------------------------------------------------------------------
# Self-checks
# ---------------------------------------------------------------------------

class Checker:
    def __init__(self):
        self.failures = 0
        self.count = 0

    def eq(self, what, got, want):
        self.count += 1
        if got != want:
            self.failures += 1
            print("FAIL %s:\n  got  %r\n  want %r" % (what, got, want))


def check_colours(c):
    table = {b"elf": [(52, 150, 70), (153, 202, 162), (32, 93, 43), (22, 65, 30)],
             b"goblin": [(190, 160, 50), (222, 207, 152), (118, 100, 31), (83, 70, 21)],
             b"dwarf": [(160, 82, 45), (207, 168, 150), (100, 51, 28), (70, 35, 19)],
             b"faerie": [(150, 90, 200), (202, 172, 227), (93, 56, 125), (65, 39, 87)],
             b"human": [(50, 105, 200), (152, 180, 227), (31, 65, 125), (21, 45, 87)],
             b"orc": [(200, 40, 35), (227, 147, 145), (125, 25, 21), (87, 17, 15)]}
    for key, want in table.items():
        got = [civilization_shade(race_base_colour(key), k) for k in range(6)]
        c.eq("5.5 %s" % key, got, want + want[:2])
        c.eq("8 item 8: %s shades distinct" % key, len(set(got[:4])), 4)
    c.eq("5.5 troll hash", colour_hash(b"troll"), 0x90674847EA21F9EA)
    c.eq("5.5 troll", race_base_colour(b"troll"), (194, 204, 68))
    c.eq("5.5 empty hash", colour_hash(b""), 0x665E00601317AE04)
    c.eq("5.5 empty", race_base_colour(b""), (50, 157, 62))
    for key in (b"troll", b""):
        c.eq("8 item 8: %r shades distinct" % key,
             len({civilization_shade(race_base_colour(key), k) for k in range(4)}), 4)
    c.eq("5.5 byte-exact keys", race_base_colour(b"Elf") == race_base_colour(b"elf"), False)


def check_example_56(c):
    """5.6's 4 x 1 example: elevation 10, region 1 1 2 0, region 1 held by an elf s0, region 2 free."""
    b = Base(4, 1, [10] * 4, [1] * 4, [1, 1, 2, 0], [(1, 0x808000)])
    elf0 = race_civilization_colour(b"elf", 0)
    img = b.render([None, elf0, None], [0, 1, 0])
    px = [tuple(img[3 * i:3 * i + 3]) for i in range(4)]
    c.eq("5.6 example factors", b.factor, [256] * 4)
    c.eq("5.6 example pixels", px, [(40, 190, 70) if p403() else (52, 150, 70), (0, 0, 0), (128, 128, 0),
                                    (128, 128, 0)])
    c.eq("5.6 example empty fill = regions composite", b.render([]), bytes(sum((list(p) for p in b.B), [])))


def check_colours_p403(c):
    """P4-03 11.1 item 1: the table of 7.2, k = 0 ... 7, race_base_colour entry 0, keys outside the table."""
    for key, entries in table().items():
        c.eq("7.2 %s" % key, [race_civilization_colour(key, k) for k in range(8)], list(entries) * 2)
        c.eq("7.2 %s base" % key, race_base_colour(key), entries[0])
        c.eq("7.2 %s distinct" % key, len(set(entries)), 4)
    c.eq("7.2 troll base (hash rule)", race_base_colour(b"troll"), (194, 204, 68))
    c.eq("7.2 empty base (hash rule)", race_base_colour(b""), (50, 157, 62))
    for key in (b"troll", b"", b"Elf", b"elf "):
        c.eq("7.2 %r shades" % key, [race_civilization_colour(key, k) for k in range(8)],
             [civilization_shade(race_base_colour(key), k) for k in range(8)])
    c.eq("7.2 troll ordinal 1", race_civilization_colour(b"troll", 1), (224, 229, 161))
    c.eq("7.2 Elf is not elf", race_base_colour(b"Elf") == table()[b"elf"][0], False)
    if p403a():   # the four entries P4-03a changes, written out
        c.eq("P4-03a goblin 3", race_civilization_colour(b"goblin", 3), (140, 155, 30))
        c.eq("P4-03a goblin 1", race_civilization_colour(b"goblin", 1), (235, 215, 90))
        c.eq("P4-03a elf 1", race_civilization_colour(b"elf", 1), (130, 215, 130))
        c.eq("P4-03a human 1", race_civilization_colour(b"human", 1), (100, 215, 215))
        changed = sorted((k.decode(), i) for k in TABLE_P403 for i in range(4) if TABLE_P403[k][i] != TABLE_P403A[k][i])
        c.eq("P4-03a exactly four entries change", changed, [("elf", 1), ("goblin", 1), ("goblin", 3), ("human", 1)])
    try:
        race_civilization_colour(b"elf", -1)
        c.eq("7.4 ordinal -1 refused", False, True)
    except ValueError:
        c.eq("7.4 ordinal -1 refused", True, True)


def check_distances(c, biome_rows=None):
    """P4-03 11.1 item 2: rules C1 to C3 of 7.3, the 'min' columns of 7.2 and the numbers quoted in 7.1."""
    rows = distance_table(biome_rows)
    for key, k, col, dB, lab, dW, dA in rows:
        c.eq("C1 %s %d against %s (%d)" % (key.decode(), k, lab, distance_units(dB)), dB >= C1_MIN, True)
        c.eq("C2 %s %d within race (%d)" % (key.decode(), k, distance_units(dW)), dW >= C2_MIN, True)
        c.eq("C3 %s %d across races (%d)" % (key.decode(), k, distance_units(dA)), dA >= C3_MIN, True)
    for key, (mb, mw) in table_mins().items():
        mine = [r for r in rows if r[0] == key]
        c.eq("7.2 %s min to B" % key.decode(), min(distance_units(r[3]) for r in mine), mb)
        c.eq("7.2 %s min within race" % key.decode(), min(distance_units(r[5]) for r in mine), mw)
    worst = min(rows, key=lambda r: r[3])
    c.eq("7.3 worst against B", (worst[0], worst[1], worst[4], distance_units(worst[3])),
         (b"goblin", 1, "biome steppe", 89 if p403a() else 88))
    worst_in = min(rows, key=lambda r: r[5])
    c.eq("7.2 closest pair of one race", (worst_in[0], distance_units(worst_in[5])), (b"dwarf", 78))
    across = min((distance(a, b), k1, i, k2, j) for k1, e1 in table().items() for i, a in enumerate(e1)
                 for k2, e2 in table().items() if k2 > k1 for j, b in enumerate(e2))
    c.eq("7.2 closest pair across races", (distance_units(across[0]),) + across[1:], (90, b"dwarf", 1, b"orc", 1))
    if p403a():
        # P4-03a 2.2: the four changed entries' nearest colours, in units
        by = {(r[0], r[1]): r for r in rows}
        for key, k, (nb, nw, na) in ((b"goblin", 3, (99, 89, 102)), (b"goblin", 1, (89, 109, 103)),
                                     (b"elf", 1, (114, 143, 142)), (b"human", 1, (96, 100, 143))):
            r = by[(key, k)]
            c.eq("P4-03a 2.2 %s %d" % (key.decode(), k),
                 (distance_units(r[3]), distance_units(r[5]), distance_units(r[6])), (nb, nw, na))
        c.eq("P4-03a 2.2 goblin 3 nearest in B", by[(b"goblin", 3)][4], "biome grassland")
        c.eq("P4-03a 2.2 goblin 1 nearest in B", by[(b"goblin", 1)][4], "biome steppe")
        c.eq("P4-03a 2.2 elf 1 nearest in B", by[(b"elf", 1)][4], "biome grassland")
        c.eq("P4-03a 2.2 human 1 nearest in B", by[(b"human", 1)][4], "ocean depth 0")
        # the effects on the rest of the table that 2.2 names
        c.eq("P4-03a 2.2 goblin 0 within race", distance_units(by[(b"goblin", 0)][5]), 109)
        c.eq("P4-03a 2.2 goblin 2 within race", distance_units(by[(b"goblin", 2)][5]), 89)
        c.eq("P4-03a 2.2 elf 3 across", distance_units(by[(b"elf", 3)][6]), 102)
        c.eq("P4-03a 2.2 elf 0 across", distance_units(by[(b"elf", 0)][6]), 180)
        c.eq("P4-03a 2.2 C3 minimum", distance_units(min(r[6] for r in rows)), 90)
        # r0's (150, 150, 30) for goblin 3: 101 to grassland, 115 to elf 3, 75 to goblin 2 (2.2, last paragraph)
        r0 = (150, 150, 30)
        c.eq("P4-03a r0 goblin 3 to goblin 2", distance_units(distance(r0, TABLE_P403A[b"goblin"][2])), 75)
    # the measurements of 7.1 on the interim table, with the same D
    c.eq("7.1 human base - river 2", distance_units(distance((50, 105, 200), river_colour(2))), 20)
    c.eq("7.1 elf dark - temperate forest", distance_units(distance((32, 93, 43), (46, 96, 52))), 26)
    c.eq("7.1 goblin light - steppe", distance_units(distance((222, 207, 152), (224, 210, 150))), 7)
    c.eq("7.1 dwarf deep - orc deep", distance_units(distance((70, 35, 19), (87, 17, 15))), 44)
    # D is symmetric and an integer of the stated form
    c.eq("7.3 D symmetric", distance((1, 2, 3), (200, 100, 50)), distance((200, 100, 50), (1, 2, 3)))
    c.eq("7.3 D example", distance((0, 0, 0), (255, 255, 255)), (512 + 127) * 65025 + 1024 * 65025 + 640 * 65025)


def check_example_82(c):
    """P4-03 8.2's 4 x 2 example (r4 under p4-03a: elf 1 (130, 215, 130), and the free and goblin cases)."""
    b = Base(4, 2, [10] * 8, [1] * 8, [1, 2, 3, 3, 1, 2, 3, 3], [(1, 0x808000)])
    e0, e1 = race_civilization_colour(b"elf", 0), race_civilization_colour(b"elf", 1)
    fill = [None, e0, e0, e1]
    img = b.render(fill, [0, 1, 1, 2])
    px = [tuple(img[3 * i:3 * i + 3]) for i in range(8)]
    E1 = (130, 215, 130) if p403a() else (150, 230, 150)
    c.eq("8.2 example dim", dim(e0), (30, 142, 52))
    c.eq("8.2 example pixels", px, [(30, 142, 52), (0, 0, 0), E1, E1] * 2)
    c.eq("8.2 example kinds", b.kind, [2, 2, 1, 1] * 2)
    c.eq("8.2 example east", b.east, [2, 3, 0, 0] * 2)
    c.eq("8.2 example without holders", [tuple(b.render(fill)[3 * i:3 * i + 3]) for i in range(4)],
         [(0, 0, 0), (0, 0, 0), E1, E1])
    if p403a():
        # region 3 free, 1 and 2 held as before: cells (1, .) black (one side held)
        img = b.render([None, e0, e0, None], [0, 1, 1, 0])
        c.eq("8.2 r4 example region 3 free", [tuple(img[3 * i:3 * i + 3]) for i in (1, 5)], [(0, 0, 0)] * 2)
        # every region free, biome steppe: cells (0, .) and (1, .) half((224, 210, 150)) = (112, 105, 75)
        s = Base(4, 2, [10] * 8, [1] * 8, [1, 2, 3, 3, 1, 2, 3, 3], [(1, 0xE0D296)])
        img = s.render([None] * 4, [0] * 4)
        c.eq("8.2 r4 example all free", [tuple(img[3 * i:3 * i + 3]) for i in range(8)],
             [(112, 105, 75), (112, 105, 75), (224, 210, 150), (224, 210, 150)] * 2)
        c.eq("8.2 r4 example all free, no holders", [tuple(s.render([None] * 4)[3 * i:3 * i + 3]) for i in range(2)],
             [(0, 0, 0)] * 2)
        # an inner border of a goblin civilization of ordinal 1 (L 206): (146, 134, 56), n = 10
        g1 = race_civilization_colour(b"goblin", 1)
        c.eq("8.2 r4 goblin 1 L", luma(g1), 206)
        c.eq("8.2 r4 goblin 1 dim", dim(g1), (146, 134, 56))


# P4-03 11.1 item 5: hand-made layers of 8 x 6 cells (the same as timeline_borders_test.cpp). Elevation 10
# everywhere (factor 256), biome 1 (0x808000) everywhere, water 0 but a lake at (0, 1). Holders: regions 1,
# 2, 3 and 7 by civilization 1 (elf 0), region 4 by civilization 2 (dwarf 0), 5 and 6 free; fill for 1 to
# 4 only (region 7 is held but has no fill).
BORDER_REGION = [1, 1, 2, 2, 4, 4, 5, 6,
                 1, 1, 2, 2, 4, 4, 5, 6,
                 3, 3, 3, 2, 2, 4, 5, 6,
                 7, 7, 4, 4, 4, 4, 0, 0,
                 3, 3, 4, 4, 4, 4, 0, 0,
                 3, 3, 4, 4, 4, 4, 0, 0]
BORDER_WATER = [0] * 48
BORDER_WATER[8] = 2
BORDER_HOLDERS = [0, 1, 1, 1, 2, 0, 0, 1]


def border_layout():
    return Base(8, 6, [10] * 48, [1] * 48, BORDER_REGION, [(1, 0x808000)], BORDER_WATER)


def check_inner_borders(c):
    b = border_layout()
    E, D = race_civilization_colour(b"elf", 0), race_civilization_colour(b"dwarf", 0)
    fill = [None, E, E, E, D, None, None, None]
    img = b.render(fill, BORDER_HOLDERS)

    def px(x, y, im=img, w=8):
        i = 3 * (y * w + x)
        return tuple(im[i:i + 3])
    K, G, d = (0, 0, 0), (128, 128, 0), dim(E)
    H = (64, 64, 0) if p403a() else K   # r4: regions 5 | 6, both free: half((128, 128, 0))
    c.eq("borders dim elf", d, (30, 142, 52))
    want = [[E, d, E, K, D, K, H, G],
            [K, d, d, K, K, K, H, G],
            [d, d, K, K, K, K, H, G],
            [K, K, D, D, D, D, G, G],
            [E, K, D, D, D, D, G, G],
            [E, K, D, D, D, D, G, G]]
    for y in range(6):
        c.eq("borders row %d" % y, [px(x, y) for x in range(8)], want[y])
    c.eq("borders kinds", b.kind, [1, 2, 1, 2, 1, 2, 2, 1,
                                   0, 2, 2, 2, 2, 2, 2, 1,
                                   2, 2, 2, 2, 2, 2, 2, 1,
                                   2, 2, 1, 1, 1, 1, 0, 0,
                                   1, 2, 1, 1, 1, 1, 0, 0,
                                   1, 2, 1, 1, 1, 1, 0, 0])
    c.eq("borders east", [b.east[i] for i in (1, 9, 16, 18, 20, 24, 25, 33)], [2, 2, 0, 2, 4, 0, 4, 4])
    c.eq("borders south", [b.south[i] for i in (1, 9, 16, 18, 20, 24, 25, 33)], [0, 3, 7, 4, 4, 3, 3, 0])
    c.eq("borders lake cell", (b.kind[8], b.east[8], b.south[8], b.region[8]), (0, 0, 0, 0))
    # no holders: P4-01's image; all free: the regions composite
    reg = bytes(sum((list(p) for p in b.B), []))
    c.eq("borders no holders, no fill", b.render([], []), reg)
    if p403a():   # every kind-2 cell half((128, 128, 0)); the lake cell (kind 0) black
        want_free = bytes(sum((list((64, 64, 0) if b.kind[i] == 2 else b.B[i]) for i in range(48)), []))
        c.eq("borders all free", b.render([None] * 8, [0] * 8), want_free)
        c.eq("borders half", [b.half[i] for i in (6, 8, 9)], [(64, 64, 0), (0, 0, 0), (64, 64, 0)])
    else:
        c.eq("borders all free", b.render([None] * 8, [0] * 8), reg)
    p401 = b.render(fill, [])
    for i in range(48):
        if b.kind[i] == 2:
            c.eq("borders no holders: black %d" % i, tuple(p401[3 * i:3 * i + 3]), K)
    # holders shorter than a region id: that region counts as free
    short = b.render(fill, BORDER_HOLDERS[:2])
    c.eq("borders short holders (1, 0)", px(1, 0, short), K)
    # scale 2
    img2 = b.render(fill, BORDER_HOLDERS, scale=2)
    for y in range(6):
        for x in range(8):
            for dy in (0, 1):
                for dx in (0, 1):
                    c.eq("borders scale 2 (%d, %d)" % (x, y), px(2 * x + dx, 2 * y + dy, img2, 16), want[y][x])


def check_luma(c):
    """P4-03 r4 7.5 and 8.2 (P4-03a 2.1, 3): L of every entry, the split at 175, the r3 pale entries."""
    c.eq("7.5 L table", {k: tuple(luma(x) for x in es) for k, es in TABLE_P403A.items()}, LUMA_P403A)
    hi = sorted((k.decode(), i) for k, es in TABLE_P403A.items() for i, x in enumerate(es) if luma(x) > LUMA_BOUND)
    c.eq("8.2 n = 10 set", hi, [("dwarf", 1), ("elf", 1), ("faerie", 1), ("goblin", 0), ("goblin", 1), ("human", 1)])
    lo = [luma(x) for es in TABLE_P403A.values() for x in es if luma(x) <= LUMA_BOUND]
    c.eq("8.2 n = 12 largest and smallest", (len(lo), max(lo), min(lo)), (18, 163, 51))
    pale = sorted((k.decode(), i, luma(x)) for k, es in TABLE_P403.items() for i, x in enumerate(es) if luma(x) > 190)
    c.eq("2.1 pale entries of r3", pale, [("elf", 1, 196), ("goblin", 1, 225), ("human", 1, 201)])
    c.eq("2.1 r3 entries 1", [luma(TABLE_P403[k][1]) for k in (b"faerie", b"dwarf", b"orc")], [186, 182, 154])
    c.eq("2.1 r3 no entry 0, 2, 3 above 185",
         max(luma(x) for es in TABLE_P403.values() for i, x in enumerate(es) if i != 1) <= 185, True)
    # keys without a row follow the same test: troll 0 (194, 204, 68) L 185, n = 10; troll 2 (121, 127, 42) L 115
    t0, t2 = race_civilization_colour(b"troll", 0), race_civilization_colour(b"troll", 2)
    c.eq("8.2 troll L", (luma(t0), luma(t2)), (185, 115))
    c.eq("8.2 troll dims", (dim(t0), dim(t2)), ((121, 127, 42), (90, 95, 31)))


# P4-03a 7 item 4: the free-border fixture of timeline_colours_p403a_test.cpp, 8 x 6 cells. Elevation 10 but
# 30 at (2, 0) (factor 266 at (1, 0)); biome 1 (0xFA6325) in region 1, biome 2 (steppe 0xE0D296) elsewhere;
# a lake at (1, 1), a river cell (water 1, class 1, no flow_dir) at (6, 5). Holders: 3, 4 by civilization 1,
# 6, 7 by civilization 2, the rest free.
FREE_REGION = [1, 1, 2, 2, 3, 3, 4, 4,
               1, 1, 2, 2, 3, 3, 4, 4,
               1, 1, 2, 2, 3, 3, 4, 4,
               5, 6, 6, 6, 7, 7, 8, 8,
               5, 5, 6, 6, 7, 7, 8, 9,
               5, 5, 6, 6, 7, 7, 8, 9]
FREE_HOLDERS = [0, 0, 0, 1, 1, 0, 2, 2, 0, 0]


def free_layout():
    elev = [10] * 48
    elev[2] = 30
    biome = [1 if FREE_REGION[i] == 1 else 2 for i in range(48)]
    water = [0] * 48
    water[9] = 2
    water[46] = 1
    river = [0] * 48
    river[46] = 1
    return Base(8, 6, elev, biome, FREE_REGION, [(1, 0xFA6325), (2, 0xE0D296)], water, river)


def check_free_borders(c):
    b = free_layout()
    g1, o0 = race_civilization_colour(b"goblin", 1), race_civilization_colour(b"orc", 0)
    fill = [None, None, None, g1, g1, None, o0, o0, None, None]
    img = b.render(fill, FREE_HOLDERS)

    def px(x, y, im=img, w=8):
        i = 3 * (y * w + x)
        return tuple(im[i:i + 3])
    K = (0, 0, 0)
    c.eq("free factor (1, 0)", b.factor[1], 266)
    named = {(1, 0): (127, 51, 19),      # 1 | 2 free, U = shade((250, 99, 37), 266) = (255, 103, 38)
             (0, 2): (125, 49, 18),      # 1 | 5 free (S), factor 256
             (6, 4): (112, 105, 75),     # 8 | 9 free, steppe
             (7, 3): (112, 105, 75),     # 8 | 9 free (S)
             (1, 1): K,                  # lake on the 1 | 2 free border
             (6, 5): K,                  # river cell on the 8 | 9 free border
             (1, 2): K,                  # E 2 free, S 6 held
             (3, 0): K,                  # 2 free | 3 held
             (0, 3): K,                  # 5 free | 6 held
             (5, 3): K,                  # 7 held | 8 free
             (4, 2): K,                  # 3 | 7, two realms
             (5, 2): K,                  # E 4 of the realm, S 7 of another
             (5, 0): (146, 134, 56),     # 3 | 4 goblin 1, L 206: n = 10
             (5, 1): (146, 134, 56),
             (3, 3): (153, 22, 22),      # 6 | 7 orc 0, L 82: n = 12
             (3, 4): (153, 22, 22),
             (3, 5): (153, 22, 22)}
    for (x, y), rgb in named.items():
        c.eq("free (%d, %d)" % (x, y), px(x, y), rgb)
    c.eq("free half only where both sides free",
         sorted(i for i in range(48) if b.kind[i] == 2 and px(i % 8, i // 8) == b.half[i] and b.half[i] != K),
         [1, 16, 31, 38])
    c.eq("free kinds", b.kind, [1, 2, 1, 2, 1, 2, 1, 1,
                                1, 0, 1, 2, 1, 2, 1, 1,
                                2, 2, 2, 2, 2, 2, 2, 2,
                                2, 2, 1, 2, 1, 2, 1, 2,
                                1, 2, 1, 2, 1, 2, 2, 1,
                                1, 2, 1, 2, 1, 2, 0, 1])
    # all zero holders: every kind-2 cell half; empty holders: every border black
    z = b.render([None] * 10, [0] * 10)
    c.eq("free all zero", z, bytes(sum((list(b.half[i] if b.kind[i] == 2 else b.B[i]) for i in range(48)), [])))
    c.eq("free empty holders, empty fill = regions", b.render([], []), bytes(sum((list(p) for p in b.B), [])))
    # a hashed key: troll 0 (L 185) n = 10, troll 2 (L 115) n = 12
    t0, t2 = race_civilization_colour(b"troll", 0), race_civilization_colour(b"troll", 2)
    imt = b.render([None, None, None, t0, t0, None, t2, t2, None, None], FREE_HOLDERS)
    c.eq("free troll", (px(5, 0, imt), px(3, 3, imt)), ((121, 127, 42), (90, 95, 31)))
    img2 = b.render(fill, FREE_HOLDERS, scale=2)
    c.eq("free scale 2", all(px(2 * x + dx, 2 * y + dy, img2, 16) == px(x, y) for y in range(6) for x in range(8)
                             for dy in (0, 1) for dx in (0, 1)), True)


def check_control_runs(c):
    """P4-03 11.1 item 7 on hand-made rows (region, from, to, civ, event)."""
    t1 = [(1, 0, None, 1, 2), (2, 300, 1200, 3, 4), (2, 1300, None, 3, 14), (3, 1000, 1600, 2, 7),
          (3, 1600, None, 1, 23), (4, 1010, 1800, 4, 9)]
    c.eq("A3 T1 accepted (equal boundary at r3 1600)", control_run_error(t1), None)
    a = [(1, 0, None, 1, 2), (2, 1300, 1400, 3, 14), (2, 300, 1200, 3, 4)] + t1[3:]
    c.eq("A3 (a) from_year descending", control_run_error(a),
         "timeline: region_control rows of region 2 overlap at year 300")
    b = [t1[0], (2, 300, 1350, 3, 4), t1[2]] + t1[3:]
    c.eq("A3 (b) to_year past the next from_year", control_run_error(b),
         "timeline: region_control rows of region 2 overlap at year 1300")
    cc = [t1[0], (2, 300, None, 3, 4), t1[2]] + t1[3:]
    c.eq("A3 (c) open row followed by another", control_run_error(cc),
         "timeline: region_control rows of region 2 overlap at year 1300")
    d = t1[:5] + [(4, 1010, 1010, 4, 9)]
    c.eq("A3 (d) to_year = from_year", control_run_error(d),
         "timeline: region_control row (4, 1010) ends at 1010, not after it starts")
    both = [t1[0], (2, 300, 300, 3, 4), (2, 200, None, 3, 14)] + t1[3:]
    c.eq("A3 a row failing its own check before the overlap with the next", control_run_error(both),
         "timeline: region_control row (2, 300) ends at 300, not after it starts")
    later = [t1[0], (2, 300, 1400, 3, 4), (2, 1300, 1300, 3, 14)] + t1[3:]
    c.eq("A3 the later row's own check before the pair check", control_run_error(later),
         "timeline: region_control row (2, 1300) ends at 1300, not after it starts")
    e = t1[:5] + [(4, 1010, 900, 4, 9)]
    c.eq("A3 to_year before from_year", control_run_error(e),
         "timeline: region_control row (4, 1010) ends at 900, not after it starts")


def check_t1(c):
    h = History(fixture_db(T1_NAME))
    E = h.E
    c.eq("T1 E", E, 2000)
    hold = h.holder
    c.eq("T1 r2 299", hold(2, 299), (2, 0, 0, 0))
    c.eq("T1 r2 300", hold(2, 300), (2, 3, 300, 4))
    c.eq("T1 r2 1199", hold(2, 1199), (2, 3, 300, 4))
    c.eq("T1 r2 1200", hold(2, 1200), (2, 0, 1200, 0))
    c.eq("T1 r2 1299", hold(2, 1299), (2, 0, 1200, 0))
    c.eq("T1 r2 1300", hold(2, 1300), (2, 3, 1300, 14))
    c.eq("T1 r2 E", hold(2, E), (2, 3, 1300, 14))
    c.eq("T1 r3 1599", hold(3, 1599), (3, 2, 1000, 7))
    c.eq("T1 r3 1600", hold(3, 1600), (3, 1, 1600, 23))
    c.eq("T1 r4 1800", hold(4, 1800), (4, 0, 1800, 0))
    c.eq("T1 r4 E", hold(4, E), (4, 0, 1800, 0))
    c.eq("T1 r6", [hold(6, y) for y in (0, 1000, E)], [(6, 0, 0, 0)] * 3)
    c.eq("T1 r1 0", hold(1, 0), (1, 1, 0, 2))
    c.eq("T1 change years", h.change_years(), [0, 300, 1000, 1010, 1200, 1300, 1600, 1800])

    def st(sid, y):   # P5-02 13: the eighth field, ending, is checked by the r4 cases of check_r4
        return h.settlement_state(sid, y)[:7]
    c.eq("T1 s1 148", st(1, 148), (1, 1, 1, 1, 1, 200, 0))
    c.eq("T1 s1 149", st(1, 149), (1, 1, 1, 2, 1, 600, 149))
    c.eq("T1 s1 1000 (gap)", st(1, 1000), (1, 1, 1, 2, 1, 600, 149))
    c.eq("T1 s1 1499 (gap)", st(1, 1499), (1, 1, 1, 2, 1, 600, 149))
    c.eq("T1 s1 1500", st(1, 1500), (1, 1, 1, 3, 1, 900, 1500))
    c.eq("T1 s2 299", st(2, 299), (2, 0, 0, 0, 0, 0, 0))
    c.eq("T1 s2 300", st(2, 300), (2, 1, 3, 1, 1, 100, 300))
    c.eq("T1 s2 1200 ruined", st(2, 1200), (2, 1, 0, 1, 3, 0, 1200))
    c.eq("T1 s2 1400 faded", st(2, 1400), (2, 1, 0, 1, 4, 0, 1400))
    c.eq("T1 s3 1600", st(3, 1600), (3, 1, 1, 2, 2, 200, 1600))
    c.eq("T1 s5 E", st(5, E), (5, 1, 0, 1, 3, 0, 1800))
    c.eq("T1 s6 1899", st(6, 1899), (6, 0, 0, 0, 0, 0, 0))
    c.eq("T1 s6 1900", st(6, 1900), (6, 1, 1, 1, 1, 50, 1900))
    rel = h.relations_in_force
    c.eq("T1 relations", [rel(y) for y in (1099, 1100, 1300, 1500, 1550, 1599, 1600, 1998, 1999, E)],
         [[], [1], [1, 3], [0, 1, 3], [0, 1, 3], [0, 1, 3], [1, 3], [1, 3], [1], [1]])
    c.eq("T1 relation 2 in no year", any(2 in rel(y) for y in range(E + 1)), False)
    c.eq("T1 alive", [h.alive(y) for y in (0, 300, 1000, 1010, 1599, 1600, E)],
         [[1], [1, 3], [1, 2, 3], [1, 2, 3, 4], [1, 2, 3, 4], [1, 3, 4], [1, 3, 4]])
    c.eq("T1 phase", [h.phase(y) for y in (0, 149, 150, 1549, 1550, 1999, E)], [1, 1, 2, 5, 6, 7, 0])
    c.eq("T1 age", [h.age(y) for y in (0, 999, 1000, 1999, E)], [1, 1, 2, 2, 2])
    c.eq("T1 events", [h.events_of(y) for y in (0, 1150, 1200, 1499, 1500, 1590, 1595, 1600, 1601, 1999, E)],
         [[1, 2, 3], [12], [12, 13], [12], [16, 17], [22], [22], [22, 23, 24], [], [27, 28], []])
    c.eq("T1 starts", [h.events_starting(y) for y in (0, 1150, 1200, 1499, 1595, 1600, 1999, E)],
         [[1, 2, 3], [12], [13], [], [], [23, 24], [28], []])
    c.eq("T1 previous", [h.previous_event_year(y) for y in (0, 1, 300, 1160, 1595, 1999, E)],
         [None, 0, 0, 1150, 1590, 1950, 1999])
    c.eq("T1 next", [h.next_event_year(y) for y in (0, 1151, 1595, 1950, 1999, E)], [300, 1200, 1600, 1999, None,
                                                                                  None])
    c.eq("T1 civ events", [h.events_of_entity(CIV, i) for i in (1, 2, 3, 4)],
         [[2, 3, 11, 12, 17, 21, 22, 23, 24, 26, 27], [7, 8, 12, 17, 18, 19, 20, 22, 23, 24],
          [4, 5, 12, 13, 14, 27], [9, 10, 11, 12, 18, 19, 20, 21, 25, 27]])
    c.eq("T1 region events", [h.events_of_entity(REGION, i) for i in (1, 2, 3, 4, 6)],
         [[1, 2, 3, 21, 26], [4, 5, 13, 14, 15], [6, 7, 8, 17, 22, 23, 24], [9, 10, 11, 18, 19, 25], [21]])
    c.eq("T1 settlement events", [h.events_of_entity(SETTLEMENT, i) for i in range(1, 7)],
         [[3, 21], [5, 13, 15], [8, 22, 23], [14], [10, 25], [26]])
    c.eq("T1 effects", [h.effects(e) for e in (1, 6, 11, 16, 17, 19, 21, 28)],
         [[2, 4], [7, 9], [16, 17], [17], [21, 22], [20, 21], [], []])
    if p403a():   # P4-03 r4 7.2: c1 elf 0, c2 dwarf 0, c3 elf 1, c4 dwarf 1
        c.eq("T1 colours", h.colours(), [(40, 190, 70), (220, 105, 25), (130, 215, 130), (245, 165, 105)])
    elif p403():   # P4-03 7.2: c1 elf 0, c2 dwarf 0, c3 elf 1, c4 dwarf 1
        c.eq("T1 colours", h.colours(), [(40, 190, 70), (220, 105, 25), (150, 230, 150), (245, 165, 105)])
    else:
        c.eq("T1 colours", h.colours(), [(52, 150, 70), (160, 82, 45), (153, 202, 162), (207, 168, 150)])
    # the static dump's lists equal the direct definitions
    st_lines = static_dump(h).decode().split("\n")
    c.eq("T1 static civ 1", st_lines[st_lines.index("civ 1 2 3 11 12 17 21 22 23 24 26 27")],
         "civ 1 2 3 11 12 17 21 22 23 24 26 27")
    c.eq("T1 static effects 28", "effects 28 -" in st_lines, True)
    check_t1_territory(c, h)


def check_t1_territory(c, h):
    fx = load_t1()
    L = {x["name"]: x for x in fx["layer"]}
    biome = [(r[0], r[3]) for r in fx["tables"]["biome"]]
    w, hh = 8, 6
    args = (w, hh, L["elevation"]["cells"], L["biome"]["cells"], L["region"]["cells"], biome)
    base = Base(*args, L["water"]["cells"], L["river"]["cells"], L["flow_dir"]["cells"])
    colours = h.colours()
    img = territory_image(h, base, colours, 1500)
    if p403():   # c1 (40, 190, 70), c2 (220, 105, 25), c3 (150, 230, 150), c4 (245, 165, 105)
        held = {"c1": (40, 190, 70), "c3": (130, 215, 130) if p403a() else (150, 230, 150),
                "c4 251": (240, 162, 103), "c4": (245, 165, 105), "c2 251": (216, 103, 25)}
    else:
        held = {"c1": (52, 150, 70), "c3": (153, 202, 162), "c4 251": (203, 165, 147), "c4": (207, 168, 150),
                "c2 251": (157, 80, 44)}

    def px(x, y, im=img, ww=w):
        i = 3 * (y * ww + x)
        return tuple(im[i:i + 3])
    want = {(0, 0): held["c1"],       # r1 held by c1 (elf s0), factor 256
            (1, 0): (0, 0, 0),           # border (E neighbour region 2)
            (0, 1): (64, 128, 204),      # lake (step 3)
            (2, 1): held["c3"],     # water 1 with class 0: no entry, fillable, c3 (elf s1)
            (4, 0): (70, 125, 215),      # river class 1
            (5, 0): (70, 125, 215),      # diagonal closing of (4, 0) onto a dry cell, unfilled
            (1, 3): (30, 80, 190),       # river class 4
            (1, 4): (30, 80, 190),       # widening (x, y + 1) onto a dry cell, unfilled
            (2, 3): (0, 0, 0),           # widening (x + 1, y), but a border cell
            (0, 4): held["c4 251"],     # c4 (dwarf s1) shaded by factor 251
            (0, 5): held["c4"],     # elevation 0 is land: fillable, factor 256
            (5, 1): held["c2 251"],       # c2 (dwarf s0) shaded by factor 251
            (6, 1): (119, 169, 205),     # region 3 but e = -5: ocean colour of depth 5, not filled
            (7, 0): (112, 163, 201),     # ocean, depth 50, region 0
            (6, 3): (96, 160, 64),       # r6, never held: B (biome 1, factor 256)
            (3, 3): (255, 0, 255),       # region value 5 with no region row, biome 9 without a row: B
            }
    if p403a():
        # r4: regions 5 (no row, free) | 6 (never held) at (4, 4) and (4, 5): half of biome 9 (no row: magenta)
        want[(4, 4)] = (127, 0, 127)
        want[(4, 5)] = (127, 0, 127)
        want[(5, 3)] = (0, 0, 0)   # region 3 (held) | 6 (free)
    for (x, y), rgb in want.items():
        c.eq("T1 territory 1500 (%d, %d)" % (x, y), px(x, y), rgb)
    c.eq("T1 factors", (base.factor[4 * w + 0], base.factor[1 * w + 5], base.factor[5 * w + 0]), (251, 251, 256))
    # free land and unfilled cells are B; every pixel is B or the shaded holder colour
    for i in range(w * hh):
        p = tuple(img[3 * i:3 * i + 3])
        if base.v[i] and territory_fill(h, colours, 1500)[base.v[i]] is not None:
            c.eq("T1 filled %d" % i, p, shade(territory_fill(h, colours, 1500)[base.v[i]], base.factor[i]))
        elif p403a() and i in (4 * w + 4, 5 * w + 4):
            c.eq("T1 half %d" % i, p, (127, 0, 127))
        else:
            c.eq("T1 unfilled %d" % i, p, base.B[i])
    # at 1250 region 2 is free: its fillable cells show B
    if p403a():
        # r4 at 2000 (r4 free since 1800): 4 | 5 free at (2, 4), (2, 5): half of marsh (64, 108, 96); (2, 3)
        # is river-painted (widening of (1, 3)), kind 0: black
        img2000 = territory_image(h, base, colours, 2000)
        c.eq("T1 2000 half", [px(x, y, img2000) for x, y in ((2, 4), (2, 5), (2, 3), (4, 4), (4, 5))],
             [(32, 54, 48), (32, 54, 48), (0, 0, 0), (127, 0, 127), (127, 0, 127)])
        # at 299 only r1 is held: 2 | 3 at (3, 0 ... 2) half of grassland (48, 80, 32); 3 | 6 at (5, 3)
        # half of mountain (64, 64, 64); 5 | 3 at (4, 3) half of magenta
        img299 = territory_image(h, base, colours, 299)
        c.eq("T1 299 half", [px(x, y, img299) for x, y in ((3, 0), (3, 1), (3, 2), (2, 2), (4, 2), (5, 3), (4, 3),
                                                          (1, 0))],
             [(48, 80, 32)] * 4 + [(64, 64, 64), (64, 64, 64), (127, 0, 127), (0, 0, 0)])
        c.eq("T1 299 half cells", sorted(i for i in range(w * hh) if base.kind[i] == 2 and img299[3 * i:3 * i + 3] !=
                                         bytes(base.B[i])),
             sorted(y * w + x for x, y in ((3, 0), (3, 1), (3, 2), (2, 2), (4, 2), (5, 3), (4, 3), (4, 4), (4, 5),
                                           (2, 4), (2, 5))))
    img1250 = base.render(territory_fill(h, colours, 1250))
    c.eq("T1 territory 1250 (2, 1)", px(2, 1, img1250), base.B[1 * w + 2])
    c.eq("T1 territory 1250 (2, 1) is the shaded biome", base.B[1 * w + 2], (96, 160, 64))
    # scale 2: each cell a 2 x 2 block
    img2 = base.render(territory_fill(h, colours, 1500), territory_holders(h, 1500), scale=2)
    for (x, y), rgb in want.items():
        for dy in (0, 1):
            for dx in (0, 1):
                c.eq("T1 scale 2 (%d, %d)" % (x, y), px(2 * x + dx, 2 * y + dy, img2, 2 * w), rgb)
    # without river: every water-1 cell (52, 100, 200) and unfillable; without water: rivers not drawn
    nr = Base(*args, L["water"]["cells"], None, L["flow_dir"]["cells"])
    imgn = nr.render(territory_fill(h, colours, 1500))
    c.eq("T1 no river (2, 1)", px(2, 1, imgn), (52, 100, 200))
    c.eq("T1 no river (5, 0) filled", px(5, 0, imgn), shade(colours[1], 251))
    nw = Base(*args)
    imgw = nw.render(territory_fill(h, colours, 1500))
    c.eq("T1 no water (0, 1) filled", px(0, 1, imgw), colours[0])


def check_invariants(c, name, path, every_year=True):
    h = History(path)
    E = h.E
    prev = None
    years = range(E + 1) if every_year else golden_years(h)
    ch = set(h.change_years())
    for y in years:
        hold = [h.holder(r, y)[1] for r in h.regions]
        if every_year:
            before = prev if prev is not None else [0] * len(h.regions)
            c.count += 1
            if (y in ch) != (hold != before):
                c.eq("%s invariant 2 at %d" % (name, y), y in ch, hold != before)
        prev = hold
    open_rows = sorted((r[0], r[3]) for rows in h.control.values() for r in rows if r[2] is None)
    c.eq("%s invariant 2 at E" % name, sorted((r, x) for r, x in ((r, h.holder(r, E)[1]) for r in h.regions) if x),
         open_rows)
    for s in h.settlements:
        c.eq("%s invariant 3 s%d" % (name, s[0]), h.settlement_state(s[0], E)[2:6], (s[8], s[9], s[10], s[11]))
    for f in ("holder", "settlement", "relations", "alive"):
        if f == "holder":
            a = [h.holder(r, E)[1:] for r in h.regions]
            b = [h.holder(r, E - 1)[1:] for r in h.regions]
        elif f == "settlement":
            a = [h.settlement_state(s[0], E) for s in h.settlements]
            b = [h.settlement_state(s[0], E - 1) for s in h.settlements]
        elif f == "relations":
            a, b = h.relations_in_force(E), h.relations_in_force(E - 1)
        else:
            a, b = h.alive(E), h.alive(E - 1)
        c.eq("%s invariant 4 %s" % (name, f), a, b)
    c.eq("%s invariant 7 open relations" % name, h.relations_in_force(E),
         [i for i, r in enumerate(h.relations) if r[6] is None])
    c.eq("%s invariant 7 alive" % name, h.alive(E), [x[0] for x in h.civs if x[8] is None])


def check_p503(c):
    """P5-03 24.1 item 12 on fixture H14 (worldgen/tests/data/history-report/h14-v5.toml): race_relation_at before,
    at and after a meeting and a turn; settlement_races around an arrival, a capture, a drift and a ruin."""
    h = History(fixture_db("h14-v5"))
    for (a, b, y), want in (((1, 2, 149), (1, 2, 0, 0, 0)), ((1, 2, 150), (1, 2, 1, -600, 0)),
                            ((2, 1, 1998), (1, 2, 1, -600, 0)), ((1, 2, 1999), (1, 2, 1, -650, 0)),
                            ((2, 3, 249), (2, 3, 0, 0, 0)), ((2, 3, 250), (2, 3, 1, -200, 1)),
                            ((3, 2, 799), (2, 3, 1, -200, 1)), ((2, 3, 800), (2, 3, 1, -520, 0)),
                            ((2, 3, 2000), (2, 3, 1, -450, 0)), ((1, 3, 1999), (1, 3, 0, 0, 0))):
        c.eq("P5-03 18 race_relation_at(%d, %d, %d)" % (a, b, y), h.race_relation_at(a, b, y), want)
    for (sid, y), want in (((4, 499), [(1, 1000, 3)]), ((4, 500), [(1, 900, 0), (2, 100, 3)]),
                           ((4, 999), [(1, 900, 0), (2, 100, 3)]), ((4, 1000), [(1, 450, 0), (2, 550, 3)]),
                           ((4, 1300), [(2, 1000, 3)]), ((3, 699), [(3, 1000, 3)]),
                           ((3, 700), [(2, 200, 1), (3, 800, 3)]), ((3, 1300), [(1, 120, 1), (2, 160, 0), (3, 720, 3)]),
                           ((5, 699), [(2, 1000, 3)]), ((5, 700), []), ((3, 199), [])):
        c.eq("P5-03 18 settlement_races(%d, %d)" % (sid, y), h.settlement_races(sid, y), want)
    for args, msg in (((9, 1, 0), "unknown race"), ((1, 2, 2001), "year outside 0..E")):
        try:
            h.race_relation_at(*args)
            c.ok("P5-03 18 race_relation_at%r fails" % (args,), False)
        except ValueError as ex:
            c.eq("P5-03 18 race_relation_at%r fails" % (args,), str(ex), msg)


def check_p504(c):
    """P5-04 18.1 item 17 on fixture H16 (h16-alliances.toml): the four reads around a formation, a betrayal, a
    spurning and a lapse; sites_existing with a mine."""
    h = History(fixture_db("h16-alliances"))
    for y, want in ((49, []), (50, [(1, 2)]), (479, [(1, 2)]), (480, [(1, 2), (3, 5)]), (629, [(1, 2), (3, 5)]),
                    (630, [(1, 2)]), (650, [(1, 2), (6, 7)]), (800, [(1, 2)]), (900, [(1, 2), (4, 7)]),
                    (999, [(1, 2), (4, 7)]), (1000, [(1, 2)]), (1249, [(1, 2), (1, 5)]), (1250, [(1, 2)]),
                    (1300, [(1, 2), (1, 3)]), (1999, [(1, 2)])):
        c.eq("P5-04 12 alliances_at(%d)" % y, [(a, b) for a, b, _s, _e in h.alliances_at(y)], want)
    c.eq("P5-04 12 alliances_at(480) since", [(s_, e) for _a, _b, s_, e in h.alliances_at(480)][1][0], 480)
    for (civ, y), want in (((1, 1300), [2, 3]), ((3, 1300), [1]), ((5, 600), [3]), ((5, 630), []), ((7, 999), [4]),
                           ((4, 1000), []), ((2, 0), [])):
        c.eq("P5-04 12 allies_of(%d, %d)" % (civ, y), h.allies_of(civ, y), want)
    for y, want in ((1300, [[1, 2, 3]]), (480, [[1, 2], [3, 5]]), (1999, [[1, 2]]), (40, [])):
        c.eq("P5-04 12 alliance_groups_at(%d)" % y, h.alliance_groups_at(y), want)
    for y, want in ((399, []), (400, [(3, 5)]), (420, [(1, 5), (3, 5), (4, 7)]), (999, [(1, 5), (3, 5), (4, 7)]),
                    (1000, [(1, 5), (3, 5)])):
        c.eq("P5-04 12 trades_at(%d)" % y, h.trades_at(y), want)
    for y, want in ((449, []), (450, [1]), (454, [1]), (455, [1, 2]), (1999, [1, 2])):
        c.eq("P5-04 12 sites_existing(%d) (mines)" % y, h.sites_existing(y), want)
    for (sid, y), want in (((2, 1504), 1), ((2, 1505), 0), ((1, 1999), 1)):
        c.eq("P5-04 12 site_at(%d, %d) open" % (sid, y), h.site_at(sid, y)[8], want)
    d = year_dump(h, 1300).decode().splitlines()
    c.eq("P5-04 12 D(1300)'s alliance and trade lines", [x for x in d if x.split(" ")[0] in ("alliance", "trade")],
         ["alliance 1 2 50", "alliance 1 3 1300", "trade 1 5 410", "trade 3 5 400"])


def check_p506(c):
    """P5-06 12 (P4-01 r7): ruler_at and D(Y)'s ruler lines on fixture H16 with hand-made figure and ruled rows
    (no P3-07 fixture holds a figure): office_from before the row's since, the half-open end, an open row ending
    at E, an unrecorded gap, a general's served row (not a reign), an overlap (reading F2)."""
    h = History(fixture_db("h16-alliances"))
    E = h.E

    def fig(i, civ, role, office_from):
        return (i, "F%d" % i, "", role, 1, 1, civ, office_from - 20, office_from, 0, None, 0, 0, 0, 1,
                1 if role == 1 else 0, 0, 0, 0)
    h.figures = {1: fig(1, 1, 1, 100), 2: fig(2, 1, 1, 290), 3: fig(3, 1, 3, 400), 4: fig(4, 1, 1, 700),
                 5: fig(5, 2, 1, 1500), 6: fig(6, 2, 1, 1600)}
    h.relations = list(h.relations) + [
        (6, 1, 3, 1, "ruled", 120, 290, 0, 0),   # office_from 100 precedes since 120; ends at 290
        (6, 2, 3, 1, "ruled", 300, 600, 0, 0),   # accession 290, row from 300
        (6, 3, 3, 1, "served", 400, 500, 0, 0),  # a general: no reign
        (6, 4, 3, 1, "ruled", 700, None, 0, 0),  # open: to E
        (6, 5, 3, 2, "ruled", 1500, 1700, 0, 0), (6, 6, 3, 2, "ruled", 1600, 1800, 0, 0)]   # overlap 1600..1699
    for (civ, y), want in (((1, 0), 0), ((1, 99), 0), ((1, 100), 1), ((1, 119), 1), ((1, 289), 1), ((1, 290), 2),
                           ((1, 450), 2), ((1, 599), 2), ((1, 600), 0), ((1, 699), 0), ((1, 700), 4),
                           ((1, E - 1), 4), ((1, E), 0), ((2, 1499), 0), ((2, 1500), 5), ((2, 1599), 5),
                           ((2, 1600), 6), ((2, 1699), 6), ((2, 1799), 6), ((2, 1800), 0), ((3, 1000), 0)):
        c.eq("P5-06 12 ruler_at(%d, %d)" % (civ, y), h.ruler_at(civ, y), want)
    c.eq("P5-06 12 overlaps of civ 2", h.reign_overlaps(2), [(5, 6, 1600, 1700)])
    c.eq("P5-06 12 no overlap in civ 1", h.reign_overlaps(1), [])
    alive = h.alive(1650)
    d = year_dump(h, 1650).decode().splitlines()
    want = ["ruler %d %d" % (cv, f) for cv, f in ((1, 4), (2, 6)) if cv in alive]
    c.eq("P5-06 12 D(1650)'s ruler lines (last)", d[len(d) - len(want):], want)
    c.eq("P5-06 12 D(E) has no ruler line", [x for x in year_dump(h, E).decode().splitlines()
                                             if x.startswith("ruler ")], [])


def check_rulers_world(c, name, h):
    """P5-06 12 on a generated world: every recorded ruler is a role-1 figure of its civilization, and no two
    reigns of a realm overlap in years (13 item 1; reading F2)."""
    overlaps = 0
    for civ in h.civs:
        ov = h.reign_overlaps(civ[0])
        overlaps += len(ov)
        if ov:
            print("info: %s civ %d reigns overlap: %r" % (name, civ[0], ov[:3]))
        for a, b, f in h.reigns(civ[0]):
            fr = h.figures[f]
            if fr[3] != 1 or fr[6] != civ[0]:
                c.eq("%s ruled row of figure %d is a ruler of civ %d" % (name, f, civ[0]), (fr[3], fr[6]),
                     (1, civ[0]))
    c.eq("%s reigns do not overlap (13 item 1)" % name, overlaps, 0)


def cmd_check(argv):
    inputs = argv[argv.index("--inputs") + 1] if "--inputs" in argv else default_inputs()
    quick = "--quick" in argv
    c = Checker()
    if p403():
        check_colours_p403(c)
        check_distances(c)
        check_example_82(c)
        check_inner_borders(c)
        if p403a():
            check_luma(c)
            check_free_borders(c)
        check_control_runs(c)
    else:
        check_colours(c)
    check_example_56(c)
    check_t1(c)
    if r5():
        check_p503(c)
    if r6():
        check_p504(c)
    if r7():
        check_p506(c)
    for name in fixture_names_p3() + (T1_NAME,):
        check_invariants(c, name, fixture_db(name))
    have = all(os.path.exists(os.path.join(inputs, n + ".db")) for n in WORLD_CASES)
    if have and not quick:
        dimmed = 0
        for name in WORLD_CASES:
            path = os.path.join(inputs, name + ".db")
            check_invariants(c, name, path, every_year=name.endswith("-512"))
            if r7():
                check_rulers_world(c, name, History(path))
            if p403():
                # 7.3 item 4: the world's table biome holds the shipped colours, so B is the same
                _, rows = read_layers(path)
                c.eq("%s table biome = shipped colours" % name, sorted(((x[1] >> 16) & 0xFF, (x[1] >> 8) & 0xFF,
                                                                         x[1] & 0xFF) for x in rows),
                     sorted(col for _, col in shipped_biome_colours()))
                if name.endswith("-512"):
                    dimmed += count_dimmed(path)
        if p403():
            print("info: dimmed inner-border cells at E in the 512 world cases: %d" % dimmed)
            c.eq("8.2 fires on the 512 world cases", dimmed > 0, True)
        if os.path.exists(GOLDEN):
            with open(GOLDEN) as f:
                c.eq("golden file is current", f.read(), golden_text(inputs))
        else:
            c.eq("golden file exists", False, True)
    elif not have:
        print("note: %s lacks the world cases; world invariants and the golden comparison are skipped" % inputs)
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def count_dimmed(path):
    """The number of kind-2 cells drawn dimmed at E (rule 8.2 holds)."""
    h = History(path)
    base = world_base(path)
    fill, holders = territory_fill(h, h.colours(), h.E), territory_holders(h, h.E)
    return sum(1 for i, k in enumerate(base.kind) if k == 2 and base.pixel(i, fill, holders) != base.B[i])


def cmd_distances(argv):
    for key, k, col, dB, lab, dW, dA in distance_table():
        print("%-6s %d %-15s to B %3d (%s), within race %3d, across races %3d"
              % (key.decode(), k, col, distance_units(dB), lab, distance_units(dW), distance_units(dA)))
    return 0


def cmd_dump(argv):
    h = History(argv[0])
    if "--year" in argv:
        sys.stdout.buffer.write(year_dump(h, int(argv[argv.index("--year") + 1])))
    elif "--years" in argv:
        for y in range(h.E + 1):
            sys.stdout.buffer.write(year_dump(h, y))
    else:
        sys.stdout.buffer.write(static_dump(h))
    return 0


def cmd_territory(argv):
    path, year, out = argv[0], int(argv[1]), argv[2]
    h = History(path)
    base = world_base(path)
    img = territory_image(h, base, h.colours(), year)
    with open(out, "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (base.w, base.h) + img)
    print(sha(img))
    return 0


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    global RULES
    if "--rules" in argv:
        i = argv.index("--rules")
        RULES = argv[i + 1]
        if RULES not in RULE_SETS:
            raise SystemExit("--rules: " + ", ".join(RULE_SETS))
        argv = argv[:i] + argv[i + 2:]
        if RULES == "p4-01":
            prr.use_version("0.20.0")   # the P3-07 fixtures' meta as P4-01 recorded them
        elif RULES == "p4-03":
            prr.use_version("0.21.0")
        elif RULES in ("p5-12", "p4-03a"):
            prr.use_version("0.22.0")
        elif RULES == "p5-02":
            prr.use_version("0.23.0")
        elif RULES == "p5-03":
            prr.use_version("0.24.0")
        elif RULES == "p5-03a":
            prr.use_version("0.25.0")
    cmd, rest = argv[0], argv[1:]
    if cmd == "distances":
        return cmd_distances(rest)
    if cmd == "dump":
        return cmd_dump(rest)
    if cmd == "territory":
        return cmd_territory(rest)
    if cmd == "fixture":
        if rest[0] == T1_NAME:
            write_t1(rest[1])
        else:
            write_p3_fixture(rest[0], rest[1])
        return 0
    if cmd == "golden":
        inputs = rest[rest.index("--inputs") + 1] if "--inputs" in rest else default_inputs()
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
    sys.exit(main(sys.argv[1:]))
