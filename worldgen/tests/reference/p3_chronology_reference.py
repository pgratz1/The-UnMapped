#!/usr/bin/env python3
"""Independent exact-integer reference of P3-03 chronology.

Written by the test writer from docs/specs/p3-03-chronology.md (sections 4
to 10: the eight meta keys, the five tables, the data checks D1-D5, the
entry checks C1, the schedule C2/C3, the ages C4, the creator gods C5, the
registry order of 8.1 and the [chronology] report keys of 9.1) and
P3-00 (2.1, 4.6), without reading any implementation and without the spec
writer's prototype. Libraries reused, all written independently from their
specs: core_reference.py (P0-01: stream_key, hash_words, decimal parse,
SHA-256), p3_names_reference.py (P3-02: draw, pick, make_name, roman) and
p3_data_reference.py (P0-03/P3-01: the data roots read with tomllib).

Python 3 standard library only, arbitrary-precision integers, no floating
point.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p3_chronology_reference.py check
        Every worked example of 7.1 (K_aw, u, pick, d, base and year per
        race, ages, gods with attempts, the subsets, the collision and
        clamp cases, G = 333), the shipped data against the data checks,
        each data check failing once on edited data, the C1 entry checks,
        the 63 subsets of 14.1 item 3, a property sweep of 14.2 items 13
        and 14 (smaller than the C++ one), the statistics claims of 7 C3
        and 12 (seeds 0-1999), and the committed golden file against a
        fresh computation. Exit 1 on any disagreement.

    python3 -B worldgen/tests/reference/p3_chronology_reference.py golden [PATH|-] [--inputs DIR]
        Writes worldgen/tests/golden/p3-03-chronology.txt ("-": stdout).
        The world cases read the region names from geography worlds in
        DIR (default ~/.cache/the-unmapped/out/p3-03-tests/inputs), one
        <case>.db per world case (any generator whose table `region` has
        the hash pinned below; P1-06 golden values for the four P1 cases).

    python3 -B worldgen/tests/reference/p3_chronology_reference.py schedule SEED SPAN [RACES]
        Prints the schedule trace (base, u, pick, d, year), ages and gods of
        one world on the shipped data (RACES: all or ids comma separated).

    python3 -B worldgen/tests/reference/p3_chronology_reference.py check-world DB [--data ROOT]
        Checks a schema-6 world written by the C++ (for the validation
        sweep, P3-03 12 "recomputed from the data by the sweep script"):
        recomputes the five tables from meta (seed, the eight param keys),
        the region names of the world and the data root (default
        game/data), and compares table by table; also invariants 1-9.
        Exit 1 on a difference.

Never regenerate the golden file to make a C++ test pass.
"""

import hashlib
import os
import sqlite3
import struct
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import core_reference as cr  # noqa: E402
import p3_data_reference as pdr  # noqa: E402
import p3_names_reference as pnr  # noqa: E402

FatalError = cr.FatalError
ONE = 1 << 32
M64 = (1 << 64) - 1
REPO = os.path.normpath(os.path.join(_HERE, "..", "..", ".."))
GOLDEN = os.path.join(REPO, "worldgen", "tests", "golden", "p3-03-chronology.txt")
DEFAULT_INPUTS = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p5-12-tests", "inputs")
# P5-12 (generator 0.22.0, the default): P1-06 r11's region names (V8; the P5-01 inputs renamed by rename-db).
_P5_01_INPUTS = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p5-01-tests", "inputs")
# P5-01 (generator 0.21.0, the default): the inputs carry P1-06 r10's region names (the P3-11 inputs renamed by
# p1_regions_reference.py rename-db); P3-10 (0.19.0, 0.20.0): r7's names; --version 0.18.0 (or 0.17.0) reads the
# P3-08 inputs (r3 names).
_P3_10_INPUTS = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p3-10-tests", "inputs")
INPUTS_BY_VERSION = {"0.22.0": DEFAULT_INPUTS, "0.21.0": _P5_01_INPUTS,
                     "0.20.0": _P3_10_INPUTS, "0.19.0": _P3_10_INPUTS,   # P3-11: region names unchanged
                     "0.18.0": os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p3-08-tests",
                                            "inputs")}

# The P3-08 input worlds (the 0.16.0 inputs with P1-06 r3 region names) are
# complete when the regions writer has left this marker.
INPUTS_READY = os.path.join(DEFAULT_INPUTS, "READY")
AWAKENING_STREAM = "hist.chronology.awakening"  # 3: K_aw
GOD_STREAM = "hist.names.god"                   # P3-02 4.5
CAMPS = {"good": 0, "neutral": 1, "evil": 2}    # 5.2 race.camp
SPAN_MIN, SPAN_MAX = 2000, 12000                 # 4.1, C1 3
AGE_NAME_MAX = 48                                # D4


def fatal(msg):
    raise FatalError(msg)


# ---------------------------------------------------------------------------
# Data (kinds race, culture, biome, history.chronology) as the stage sees it
# ---------------------------------------------------------------------------

def fx_raw(v):
    """A loaded fx value: TOML integer n is n << 32, a string a decimal (P0-03)."""
    return (v << 32) if isinstance(v, int) else cr.parse_decimal(v)


class Data:
    """The loaded definitions the stage reads, independent of TOML layout."""

    def __init__(self, races, cultures, biomes, tuning):
        # races: [dict] in race order (ascending `order`), each with id, order,
        # name, plural, adjective, camp, share (raw), lifespan, generation,
        # biomes [(biome id, weight)] as listed, culture (culture id).
        self.races = sorted(races, key=lambda r: r["order"])
        # cultures: {id: dict(race, name, naming)}; numbering in canonical id
        # order (ascending id bytes), 5.2 culture.
        self.cultures = cultures
        self.culture_ids = sorted(cultures, key=lambda c: c.encode())
        self.culture_number = {c: i + 1 for i, c in enumerate(self.culture_ids)}
        # biomes: {id: dict(code, water, fallback, rules)}
        self.biomes = biomes
        # tuning: dict(jitter (raw), gap, form)
        self.tuning = tuning
        self.race_by_id = {r["id"]: r for r in self.races}


def data_from_root(root):
    v017 = pdr.at_least("0.17.0")   # P3-03 r3 from 0.17.0 (and later)
    content = pdr.load_root(root, kinds=("biome", "culture", "history", "race") + (("name_filter",) if v017 else ()))
    races = []
    for rid, t in content["race"]:
        races.append(dict(id=rid, order=t["order"], name=t["name"], plural=t["plural"], adjective=t["adjective"],
                          camp=t["camp"], share=fx_raw(t["awakening_share"]), lifespan=t["lifespan_years"],
                          generation=t["generation_years"], biomes=[(b["biome"], b["weight"]) for b in t["biomes"]],
                          culture=t["culture"]))
    cultures = {cid: dict(race=t["race"], name=t["name"], naming=t["naming"]) for cid, t in content["culture"]}
    biomes = {bid: dict(code=t["code"], water=t["water"], fallback=t["fallback"], rules=t["rules"])
              for bid, t in content["biome"]}
    ch = dict(content["history"])["chronology"]
    tuning = dict(jitter=fx_raw(ch["awakening_jitter_frac"]), gap=ch["awakening_min_gap_years"],
                  form=ch["age_name_form"])
    d = Data(races, cultures, biomes, tuning)
    d.root = root   # P5-08 6.1: the pantheon's god definitions are read from the same root
    d.creator_title = ch.get("creator_title", "the Maker of {plural}")   # P5-08 4.2 (code default the start)
    # P3-02 r5 11.2 (0.17.0): the effective filter, the union of every
    # name_filter definition; empty for 0.16.0 data.
    d.filter = pnr.EMPTY_FILTER
    if v017:
        a, b, w = [], [], []
        for _, t in content["name_filter"]:
            a += t["anywhere"]
            b += t["at_start"]
            w += t["whole"]
        d.filter = pnr.NameFilter(a, b, w)
    return d


_SHIPPED = None


def shipped():
    global _SHIPPED
    if _SHIPPED is None:
        _SHIPPED = data_from_root("game/data")
    return _SHIPPED


def edited(data, races=None, tuning=None, cultures=None, biomes=None):
    """A copy of data with replaced parts (dicts copied, not shared)."""
    import copy
    d = Data(copy.deepcopy(races if races is not None else data.races),
             copy.deepcopy(cultures if cultures is not None else data.cultures),
             copy.deepcopy(biomes if biomes is not None else data.biomes),
             dict(tuning if tuning is not None else data.tuning))
    d.filter = getattr(data, "filter", pnr.EMPTY_FILTER)
    return d


# ---------------------------------------------------------------------------
# C4 step 1: the age name text
# ---------------------------------------------------------------------------

def age_name_text(form, plural, adjective):
    """Every {plural} and {adjective} replaced, left to right, replacement not rescanned."""
    out = []
    i = 0
    while i < len(form):
        if form.startswith("{plural}", i):
            out.append(plural)
            i += len("{plural}")
        elif form.startswith("{adjective}", i):
            out.append(adjective)
            i += len("{adjective}")
        else:
            out.append(form[i])
            i += 1
    return "".join(out)


def age_name_charset_ok(name):
    """D5: ASCII letters, space, ' and -; no leading or trailing space; no double space."""
    for ch in name:
        if not ("A" <= ch <= "Z" or "a" <= ch <= "z" or ch in " '-"):
            return False
    if name.startswith(" ") or name.endswith(" ") or "  " in name:
        return False
    return True


# ---------------------------------------------------------------------------
# Section 6: check_history_data
# ---------------------------------------------------------------------------

def is_natural(b):
    return b["fallback"] or len(b["rules"]) > 0


def check_history_data(data):
    """[(path, message)] sorted and deduplicated (P0-03 9: path, no position, message bytes)."""
    errs = []
    if not data.races:  # D0 (lead's decision 2026-11-19), first in the D order
        errs.append("no race is defined; kind 'race' needs at least one definition")
    for r in data.races:  # race order, every definition
        for bid, _w in r["biomes"]:  # D1, D2 per entry in list order
            b = data.biomes[bid]
            if b["water"]:
                errs.append("race '%s' prefers water biome '%s'" % (r["id"], bid))
            elif not is_natural(b):
                errs.append("race '%s' prefers biome '%s', which no geography stage assigns" % (r["id"], bid))
        c = data.cultures[r["culture"]]
        if c["race"] != r["id"]:  # D3
            errs.append("race '%s' has culture '%s' of race '%s'" % (r["id"], r["culture"], c["race"]))
        a = age_name_text(data.tuning["form"], r["plural"], r["adjective"])
        if len(a.encode()) > AGE_NAME_MAX:  # D4
            errs.append("race '%s' has the age name '%s', which is longer than 48 bytes" % (r["id"], a))
        if not age_name_charset_ok(a):  # D5
            errs.append("race '%s' has the age name '%s', which is not ASCII letters with single spaces, ' and -"
                        % (r["id"], a))
    out = sorted(set(errs), key=lambda m: m.encode())
    return [("races", m) for m in out]


# ---------------------------------------------------------------------------
# HistoryParams and the eight meta keys (4.1, 4.2)
# ---------------------------------------------------------------------------

DETAILS = ("sparse", "normal", "dense")
INTENSITIES = ("calm", "normal", "turbulent")
PRESSURES = ("low", "normal", "high")
ACTIVITIES = ("distant", "normal", "meddling")


def params(span=6000, races=None, detail="normal", civs=None, intensity="normal", pressure="normal",
           activity="normal", script="", data=None):
    """HistoryParams as a dict; races None = every loaded race in race order."""
    d = data or shipped()
    return dict(span=span, races=list(races) if races is not None else [r["id"] for r in d.races],
                detail=detail, civs=civs, intensity=intensity, pressure=pressure, activity=activity, script=script)


def meta_entries(p):
    """The eight keys of 4.2 in ascending key order, canonical text."""
    return [
        ("param.civs_per_race", "auto" if p["civs"] is None else str(p["civs"])),
        ("param.divine_activity", p["activity"]),
        ("param.divine_script", p["script"]),
        ("param.event_intensity", p["intensity"]),
        ("param.history_detail", p["detail"]),
        ("param.history_span", str(p["span"])),
        ("param.monster_pressure", p["pressure"]),
        ("param.races", ",".join(p["races"])),
    ]


def is_def_id(s):
    """P0-03 definition id grammar: [a-z][a-z0-9_]*, 1 to 64 bytes."""
    return 1 <= len(s) <= 64 and "a" <= s[0] <= "z" and all("a" <= c <= "z" or "0" <= c <= "9" or c == "_"
                                                            for c in s)


def meta_value_ok(key, v):
    """The reader's syntax check of 4.2."""
    if key == "param.civs_per_race":
        return v in ("auto", "1", "2", "3", "4")
    if key == "param.divine_activity":
        return v in ACTIVITIES
    if key == "param.divine_script":
        return v == "" or (len(v) == 64 and all(c in "0123456789abcdef" for c in v))
    if key == "param.event_intensity":
        return v in INTENSITIES
    if key == "param.history_detail":
        return v in DETAILS
    if key == "param.history_span":
        if not v or not v.isdigit() or (v[0] == "0") or not v.isascii():
            return False
        return SPAN_MIN <= int(v) <= SPAN_MAX
    if key == "param.monster_pressure":
        return v in PRESSURES
    if key == "param.races":
        ids = v.split(",")
        return all(is_def_id(i) for i in ids) and len(set(ids)) == len(ids)
    raise KeyError(key)


def params_from_meta(meta):
    civ = meta["param.civs_per_race"]
    return dict(span=int(meta["param.history_span"]), races=meta["param.races"].split(","),
                detail=meta["param.history_detail"], civs=None if civ == "auto" else int(civ),
                intensity=meta["param.event_intensity"], pressure=meta["param.monster_pressure"],
                activity=meta["param.divine_activity"], script=meta["param.divine_script"])


# ---------------------------------------------------------------------------
# C2, C3: the schedule
# ---------------------------------------------------------------------------

def bounds(span, present, jitter_raw, gap):
    """C2: (G, jmax)."""
    g = min(gap, span // present)
    jmax = cr.rha(jitter_raw * span, ONE)  # round_int(mul_int(jitter, E)), RHA
    return g, jmax


def schedule(seed, span, present, jitter_raw, gap):
    """C3. present: [(race id, share raw)] in race order. Returns a list of
    dicts per present race: id, base, u (None for the first), pick, d, lo, hi, year."""
    p = len(present)
    if p == 0:
        fatal("schedule: no race present")
    g, jmax = bounds(span, p, jitter_raw, gap)
    key = cr.stream_key(seed, AWAKENING_STREAM)
    out = []
    for i, (rid, share) in enumerate(present, start=1):
        base = cr.rha(share * span, ONE)
        if i == 1:
            out.append(dict(id=rid, base=base, u=None, pick=None, d=None, lo=None, hi=None, year=0))
            continue
        u = pnr.draw(key, 0, rid, 0, 0)
        pk = pnr.pick(u, 2 * jmax + 1)
        d = pk - jmax
        lo = out[-1]["year"] + g
        hi = span - g * (p - i + 1)
        assert lo <= hi, "C3: lo > hi"
        y = min(max(base + d, lo), hi)
        out.append(dict(id=rid, base=base, u=u, pick=pk, d=d, lo=lo, hi=hi, year=y))
    return out


# ---------------------------------------------------------------------------
# C1 to C5: the stage
# ---------------------------------------------------------------------------

def entry_checks(data, p, region_names):
    """C1, in order; returns the first message or None."""
    if len(region_names) == 0:
        return "chronology: input precondition: no region rows"
    t = data.tuning
    if not (0 <= t["jitter"] <= cr.parse_decimal("0.05")):
        return "chronology: tuning: awakening_jitter_frac outside its schema range"
    if not (1 <= t["gap"] <= 2000):
        return "chronology: tuning: awakening_min_gap_years outside its schema range"
    if len(t["form"].encode()) > 64:
        return "chronology: tuning: age_name_form outside its schema range"
    if not form_ok(t["form"]):
        return "chronology: tuning: age_name_form must contain {plural} or {adjective} and no other placeholder"
    e = p["span"]
    if not (SPAN_MIN <= e <= SPAN_MAX):
        return "chronology: settings: history_span %d outside 2000 to 12000" % e
    if len(p["races"]) == 0:
        return "chronology: settings: no race present"
    for rid in p["races"]:
        if rid not in data.race_by_id:
            return "chronology: settings: unknown race '%s'" % rid
    prev = None
    for rid in p["races"]:
        o = data.race_by_id[rid]["order"]
        if prev is not None and o <= prev:
            return "chronology: settings: races not in race order at '%s'" % rid
        prev = o
    errs = check_history_data(data)
    if errs:
        return "chronology: data: " + errs[0][1]
    # P3-03 r3: P3-02 r5's stage check names_form_text, after the checks of
    # the culture data and before any name is drawn. ASSUMPTION (spec gap):
    # the message is 11.3's text as written, with its prefix `history:`.
    try:
        pnr.names_form_text(getattr(data, "filter", pnr.EMPTY_FILTER),
                            {cid: cu["naming"] for cid, cu in data.cultures.items()})
    except pnr.FatalError as ex:
        return str(ex)
    return None


def form_ok(form):
    """P3-01 5.1 named check chronology_age_name_form."""
    rest = form.replace("{plural}", "\0").replace("{adjective}", "\0")
    return "\0" in rest and "{" not in rest


def chronology(data, p, seed, region_names, stage_entry=True):
    """C1 to C5. Returns dict with rows of the five tables, the schedule
    trace, god attempts and the final registry. stage_entry False is the
    seam chronology_stage of 14, which takes the region names as given, an
    empty list included (14.3 item 21): C1 item 1 is the stage's check on
    table region (StageContext), every other check of C1 still applies."""
    msg = entry_checks(data, p, region_names if stage_entry else ["(seam)"])
    if msg is not None:
        fatal(msg)
    e = p["span"]
    present_ids = set(p["races"])
    present = [r for r in data.races if r["id"] in present_ids]  # race order
    sched = schedule(seed, e, [(r["order"], r["share"]) for r in present], data.tuning["jitter"],
                     data.tuning["gap"])
    year_of = {r["id"]: s["year"] for r, s in zip(present, sched)}
    registry = set(region_names)  # 8.1 step 1
    # C4 ages, id order
    ages = []
    for i, r in enumerate(present):
        start = sched[i]["year"]
        end = sched[i + 1]["year"] - 1 if i + 1 < len(present) else e - 1
        a = age_name_text(data.tuning["form"], r["plural"], r["adjective"])
        name = a
        if name in registry:
            k = 2
            while a + " " + pnr.roman(k) in registry:
                k += 1
            name = a + " " + pnr.roman(k)
        registry.add(name)
        ages.append((i + 1, r["order"], start, end, name))
    # C5 gods, race id order, every race
    gods = []
    attempts = []
    for r in data.races:
        cid = r["culture"]
        c = data.culture_number[cid]
        name, t = pnr.make_name(data.cultures[cid]["naming"], cid, "god", seed, c, r["order"], registry,
                                filt=getattr(data, "filter", pnr.EMPTY_FILTER))
        if pdr.at_least("0.30.0"):   # P5-08 6.1 C5: domain 1 (creation), the creator title, race = the race id
            gods.append((r["order"], name, CAMPS[r["camp"]], 1,
                         getattr(data, "creator_title", "the Maker of {plural}").replace("{plural}", r["plural"]),
                         r["order"]))
        else:
            gods.append((r["order"], name, CAMPS[r["camp"]]))
        attempts.append(t)
    if pdr.at_least("0.30.0"):   # P5-08 6.1 C5a and 8.1 item 4: the rest of the pantheon, named after the creators
        import p5_08_pantheon_reference as p508
        for row, t in p508.pantheon_god_rows(data, seed, registry):
            gods.append(row)
            attempts.append(t)
    races = []
    race_biome = []
    for r in data.races:
        cnum = data.culture_number[r["culture"]]
        pres = 1 if r["id"] in present_ids else 0
        races.append((r["order"], r["id"], r["name"], r["plural"], r["adjective"], CAMPS[r["camp"]], r["lifespan"],
                      r["generation"], cnum, pres, year_of.get(r["id"]), r["order"]))
        for bid, w in r["biomes"]:
            race_biome.append((r["order"], data.biomes[bid]["code"], w))
    race_biome.sort(key=lambda t: (t[0], t[1]))
    cultures = []
    for cid in data.culture_ids:
        cultures.append((data.culture_number[cid], cid, data.cultures[cid]["name"],
                         data.race_by_id[data.cultures[cid]["race"]]["order"]))
    return dict(race=races, race_biome=race_biome, culture=cultures, god=gods, age=ages, schedule=sched,
                attempts=attempts, registry=registry, present=present)


# ---------------------------------------------------------------------------
# P0-04 8 logical table hashes
# ---------------------------------------------------------------------------

COLUMNS = {
    "race": 12, "race_biome": 3, "culture": 4, "god": 3, "age": 5,
}
TABLES = ("age", "culture", "god", "race", "race_biome")  # ascending name, as hash --detail lists them


def enc_value(v):
    if v is None:
        return b"\x00"
    if isinstance(v, int):
        return b"\x01" + struct.pack("<q", v)
    b = v.encode("utf-8") if isinstance(v, str) else bytes(v)
    return b"\x03" + struct.pack("<Q", len(b)) + b


def table_hash(ncols, rows):
    """sha256 of u64 column count, then every value of every row (rows in key order)."""
    data = bytearray(struct.pack("<Q", ncols))
    for row in rows:
        assert len(row) == ncols
        for v in row:
            data += enc_value(v)
    h = cr.sha256_hex(bytes(data))
    assert h == hashlib.sha256(bytes(data)).hexdigest()
    return h


def table_hashes(res):
    return {t: table_hash(COLUMNS[t], res[t]) for t in TABLES}


# ---------------------------------------------------------------------------
# 9.1 the [chronology] report section
# ---------------------------------------------------------------------------

def report_lines(res):
    if not res["age"]:
        return []
    years = [r[10] for r in res["race"] if r[9] == 1]
    lens = [a[3] - a[2] + 1 for a in res["age"]]
    return [
        "[chronology]",
        "races_present = %d" % sum(1 for r in res["race"] if r[9] == 1),
        "awakening_years = " + " ".join(str(y) for y in years),
        "age_years = " + " ".join(str(n) for n in lens),
        "age_years_min = %d" % min(lens),
        "gods = %d" % len(res["god"]),
    ]


# ---------------------------------------------------------------------------
# Worlds (sqlite3 read-only)
# ---------------------------------------------------------------------------

def open_world(path):
    uri = "file:" + os.path.abspath(path) + "?mode=ro&immutable=1"
    conn = sqlite3.connect(uri, uri=True)
    return conn


def world_table_hash(conn, table):
    info = conn.execute("SELECT name, pk FROM pragma_table_info(?) ORDER BY cid", (table,)).fetchall()
    cols = [r[0] for r in info]
    pks = [r[0] for r in sorted((r for r in info if r[1] > 0), key=lambda r: r[1])]
    order = ", ".join('"%s" COLLATE BINARY' % p for p in pks)
    rows = conn.execute('SELECT %s FROM "%s" ORDER BY %s' % (", ".join('"%s"' % c for c in cols), table,
                                                              order)).fetchall()
    for row in rows:
        for v in row:
            if isinstance(v, float):
                raise ValueError("REAL value in %s" % table)
    return table_hash(len(cols), rows), rows


def world_region_names(conn):
    return [r[0] for r in conn.execute("SELECT name FROM region ORDER BY id").fetchall()]


# ---------------------------------------------------------------------------
# Golden cases
# ---------------------------------------------------------------------------

SEAM_SEEDS = (0, 1, 42, M64, cr.phrase_seed(b"the unmapped"))
SEAM_SPANS = (2000, 6000, 12000)
SEAM_RACES = ("all", "elf,orc", "orc", "goblin,dwarf,human")
COLLISION_REGIONS = ("Fielie", "The Age of Elves", "Krok", "Krok II")  # 7.1 "Collisions"

# World cases of 14.4: (case, seed, map size, span, races, the hash of the
# input world's table region; the P1-06 golden values for the four P1 cases).
WORLD_CASES = (
    ("geo-seed-0-512", 0, 512, 6000, "all", None),
    ("geo-seed-42-512", 42, 512, 6000, "all", None),
    ("geo-phrase-512", 15629206466951235176, 512, 6000, "all", None),
    ("geo-seed-1-1024", 1, 1024, 6000, "all", None),
    ("hist-span-2000-512", 7, 512, 2000, "all", None),
    ("hist-races-elf-orc-1024", 3, 1024, 6000, "elf,orc", None),
)
# The P1-06 golden cases of p1-06-regions.txt whose table region the world
# cases above must reproduce (the P1-06 golden pins the region names).
P1_06_CASE = {"geo-seed-0-512": "shipped-512-s0", "geo-seed-42-512": "shipped-512-s42",
              "geo-phrase-512": "shipped-512-phrase", "geo-seed-1-1024": "shipped-1024-s1"}

SCHED_SEEDS = tuple(range(32)) + (42, M64)
SCHED_SPANS = (2000, 2001, 6000, 11999, 12000)
SCHED_SUBSETS = ("all", "elf", "goblin", "dwarf", "faerie", "human", "orc", "elf,orc",
                 "goblin,dwarf,faerie,human,orc", "goblin,dwarf,human")


def races_of(text, data=None):
    d = data or shipped()
    if text == "all":
        return [r["id"] for r in d.races]
    ids = text.split(",")
    return [r["id"] for r in d.races if r["id"] in ids]


def p1_06_region_hash(case):
    """The table_region hash of a P1-06 golden case for the served version: from 0.19.0 the committed file
    (P1-06 r7 names); before, the file as of the P3-10 base commit (r3 names, the inputs of those versions)."""
    path = os.path.join(REPO, "worldgen", "tests", "golden", "p1-06-regions.txt")
    if pdr.at_least("0.22.0"):
        with open(path) as f:
            text = f.read()
    elif pdr.at_least("0.21.0"):   # P5-12 (P1-06 r11) moved the committed file: r10's, as of the P5-12 base
        import subprocess
        text = subprocess.run(["git", "-C", REPO, "show", pdr.P5_12_BASE_COMMIT +
                               ":worldgen/tests/golden/p1-06-regions.txt"], check=True, capture_output=True,
                              text=True).stdout
    elif pdr.at_least("0.19.0"):   # P5-01 (P1-06 r10) moved the committed file: r7's, as of the P5-01 base
        import subprocess
        text = subprocess.run(["git", "-C", REPO, "show", pdr.P5_01_BASE_COMMIT +
                               ":worldgen/tests/golden/p1-06-regions.txt"], check=True, capture_output=True,
                              text=True).stdout
    else:
        import subprocess
        text = subprocess.run(["git", "-C", REPO, "show", pdr.P3_10_BASE_COMMIT +
                               ":worldgen/tests/golden/p1-06-regions.txt"], check=True, capture_output=True,
                              text=True).stdout
    if True:
        for line in text.splitlines():
            parts = line.split()
            if len(parts) == 3 and parts[0] == case and parts[1] == "table_region":
                return parts[2]
    raise KeyError(case)


def tuned_cases():
    """Hand-made shares and tuning through the awakening_years seam (14.1
    items 1, 2; 14.2 item 14). (name, seed, span, gap, jitter raw, ids, shares raw)."""
    out = [
        ("clamp", 0, 6000, 100, 0, [1, 2, 3, 4], [0, ONE // 2, cr.parse_decimal("0.501"),
                                                  cr.parse_decimal("0.999")]),
        ("gap333", 0, 2000, 2000, 0, [1, 2, 3, 4, 5, 6], [r["share"] for r in shipped().races]),
        ("gap333-j", 42, 2000, 2000, cr.parse_decimal("0.05"), [1, 2, 3, 4, 5, 6],
         [r["share"] for r in shipped().races]),
        ("jitter0", 42, 6000, 100, 0, [1, 2, 3, 4, 5, 6], [r["share"] for r in shipped().races]),
        ("jitter-max", 42, 12000, 100, cr.parse_decimal("0.05"), [1, 2, 3, 4, 5, 6],
         [r["share"] for r in shipped().races]),
        ("one", 9, 6000, 100, cr.parse_decimal("0.01"), [4], [cr.parse_decimal("0.7")]),
        ("sparse-ids", 5, 6000, 300, cr.parse_decimal("0.03"), [2, 5, 6],
         [cr.parse_decimal("0.35"), cr.parse_decimal("0.83"), cr.parse_decimal("0.93")]),
    ]
    # 14.2 item 14 in small: shares packed close, from a test-owned keyed sequence.
    for k in range(12):
        key = cr.stream_key(k, "test.chronology.shares")
        n = 2 + k % 5
        raw = sorted((cr.hash_words(cr.tag_value("UNM:TEST"), [key, i]) >> 32) for i in range(n - 1))
        shares = [0] + raw
        gap = 1 + (cr.hash_words(cr.tag_value("UNM:TEST"), [key, 100]) % 2000)
        jit = cr.hash_words(cr.tag_value("UNM:TEST"), [key, 101]) % (cr.parse_decimal("0.05") + 1)
        span = 2000 + (cr.hash_words(cr.tag_value("UNM:TEST"), [key, 102]) % 10001)
        out.append(("packed-%d" % k, k, span, gap, jit, list(range(1, n + 1)), shares))
    return out


def fmt_ages(ages):
    return "|".join("%d:%d:%d:%d:%s" % a for a in ages)


def fmt_gods(gods, attempts):
    return "|".join("%d:%s:%s" % (g[0], g[1], t) for g, t in zip(gods, attempts))


def fmt_trace(sched):
    parts = []
    for s in sched:
        if s["u"] is None:
            parts.append("%d:%d:-:-:-:%d" % (s["id"], s["base"], s["year"]))
        else:
            parts.append("%d:%d:%016x:%d:%d:%d" % (s["id"], s["base"], s["u"], s["pick"], s["d"], s["year"]))
    return "|".join(parts)


def case_lines(name, res, p):
    o = []
    o.append("%s years %s" % (name, " ".join(str(s["year"]) for s in res["schedule"])))
    o.append("%s trace %s" % (name, fmt_trace(res["schedule"])))
    o.append("%s ages %s" % (name, fmt_ages(res["age"])))
    o.append("%s gods %s" % (name, fmt_gods(res["god"], res["attempts"])))
    h = table_hashes(res)
    for t in TABLES:
        o.append("%s table %s %s" % (name, t, h[t]))
    for k, v in meta_entries(p):
        o.append("%s meta %s=%s" % (name, k, v))
    for line in report_lines(res)[1:]:
        o.append("%s report %s" % (name, line))
    return o


HEADER = """\
# P3-03 chronology golden vectors for generator 0.13.0 (schema 6), spec
# docs/specs/p3-03-chronology.md approved 2026-11-19, generated by
# worldgen/tests/reference/p3_chronology_reference.py golden, an
# independent exact-integer reference written from the spec, P3-00 and
# P3-02 alone (core_reference.py for hashing, p3_names_reference.py for
# names, p3_data_reference.py to read the data roots). Data: the shipped
# game/data (the fixture roots worldgen/tests/data/geo and geo-small carry
# identical races/, cultures/, history/ and biomes/).
# Do not edit by hand; never regenerate to make a C++ test pass.
#
# Format: ASCII, LF, tokens separated by one space; a value that holds
# names runs to the end of the line.
#   case <name> seam <seed> <span> <races> <regions>
#       a seam case (14.4): chronology_stage on the shipped data with
#       HistoryParams defaults except span and races ("all" = the six
#       ids), seed <seed>; <regions> "none" = empty region list,
#       "collision" = Fielie, The Age of Elves, Krok, Krok II (7.1).
#   case <name> world <seed> <map size> <span> <races>
#       a world case (14.4): generate with the shipped data, --seed <seed>
#       --map-size <map size>, the span and races as given (flags), every
#       stage; region names from the world's table region, whose P0-04 8
#       hash is the item "input_region" (for the four P1 cases equal to
#       table_region of p1-06-regions.txt, cases shipped-512-s0,
#       shipped-512-s42, shipped-512-phrase, shipped-1024-s1).
#   <name> years <y1> ... <yP>          awakening years of the present races
#   <name> trace <id:base:u:pick:d:year|...>
#                                       C3 per present race; u in hex
#                                       (16 digits), "-" for the first race
#   <name> ages <id:race:start:end:name|...>
#   <name> gods <id:name:attempt|...>   attempt of the accepted name (P3-02
#                                       choose_name), "F" for the numeral
#                                       fallback
#   <name> table <table> <hex>          P0-04 8 logical hash of the table,
#                                       as the line "table <table> <hex>" of
#                                       hash --detail; tables age, culture,
#                                       god, race, race_biome
#   <name> meta <key>=<value>           the eight param.* keys of 4.2
#   <name> report <line>                the [chronology] lines of 9.1 (after
#                                       the section header)
#   <name> input_region <hex>           (world cases) see above
#   <name> regions <count>              (world cases) rows of table region
#   sched <seed> <span> <races> <y1> ... <yP>
#       awakening_years on the shipped races and tuning (14.2 item 13)
#   tuned <name> <seed> <span> <gap> <jitter raw> <ids> <shares raw> <G> <jmax> <years>
#       awakening_years with hand-made tuning and shares (comma lists);
#       G and jmax are C2's
#
# Where the worked examples of spec 7.1 appear (unchanged):
#   seed 42, span 6000, all races   case seam-42-6000-all (u, pick, d,
#                                   years, ages, gods with attempts)
#   seed 0 / seed 1, span 6000      cases seam-0-6000-all, seam-1-6000-all
#   seed 7, span 2000               case hist-span-2000-512 (and sched 7 2000 all)
#   seed 3, span 6000, elf,orc      case hist-races-elf-orc-1024 (and
#                                   sched 3 6000 elf,orc)
#   seed 42, goblin,dwarf,human     case seam-42-6000-goblin,dwarf,human
#   seed 2^64-1, 12000, orc         case seam-18446744073709551615-12000-orc
#   collisions                      case collision-42-6000-all
#   clamp, G = 333                  tuned clamp, tuned gap333
"""


def golden_text(inputs):
    data = shipped()
    o = [HEADER.rstrip("\n")]
    for seed in SEAM_SEEDS:
        for span in SEAM_SPANS:
            for races in SEAM_RACES:
                name = "seam-%d-%d-%s" % (seed, span, races)
                p = params(span=span, races=races_of(races))
                res = stage_with_regions(data, p, seed, [])
                o.append("case %s seam %d %d %s none" % (name, seed, span, races))
                o += case_lines(name, res, p)
    p = params()
    res = stage_with_regions(data, p, 42, list(COLLISION_REGIONS))
    o.append("case collision-42-6000-all seam 42 6000 all collision")
    o += case_lines("collision-42-6000-all", res, p)
    for name, seed, size, span, races, _ in WORLD_CASES:
        path = os.path.join(inputs, name + ".db")
        conn = open_world(path)
        h, _rows = world_table_hash(conn, "region")
        regions = world_region_names(conn)
        conn.close()
        if name in P1_06_CASE:
            want = p1_06_region_hash(P1_06_CASE[name])
            if h != want:
                fatal("%s: table region %s differs from p1-06-regions.txt %s" % (path, h, want))
        p = params(span=span, races=races_of(races))
        res = chronology(data, p, seed, regions)
        o.append("case %s world %d %d %d %s" % (name, seed, size, span, races))
        o.append("%s input_region %s" % (name, h))
        o.append("%s regions %d" % (name, len(regions)))
        o += case_lines(name, res, p)
    for seed in SCHED_SEEDS:
        for span in SCHED_SPANS:
            for races in SCHED_SUBSETS:
                ids = races_of(races)
                present = [(data.race_by_id[i]["order"], data.race_by_id[i]["share"]) for i in ids]
                s = schedule(seed, span, present, data.tuning["jitter"], data.tuning["gap"])
                o.append("sched %d %d %s %s" % (seed, span, races, " ".join(str(x["year"]) for x in s)))
    for name, seed, span, gap, jit, ids, shares in tuned_cases():
        s = schedule(seed, span, list(zip(ids, shares)), jit, gap)
        g, jmax = bounds(span, len(ids), jit, gap)
        o.append("tuned %s %d %d %d %d %s %s %d %d %s" % (
            name, seed, span, gap, jit, ",".join(map(str, ids)), ",".join(map(str, shares)), g, jmax,
            ",".join(str(x["year"]) for x in s)))
    return "\n".join(o) + "\n"


def stage_with_regions(data, p, seed, regions):
    """The seam chronology_stage (14)."""
    return chronology(data, p, seed, regions, stage_entry=False)


# ---------------------------------------------------------------------------
# check
# ---------------------------------------------------------------------------

class Checker:
    def __init__(self):
        self.failures = 0
        self.count = 0

    def eq(self, what, got, want):
        self.count += 1
        if got != want:
            self.failures += 1
            print("FAIL %s: got %r, want %r" % (what, got, want))

    def true(self, what, cond):
        self.eq(what, bool(cond), True)


def expect_fatal(c, what, fn, msg):
    try:
        fn()
    except FatalError as e:
        c.eq(what, str(e), msg)
        return
    c.eq(what, "no FatalError", msg)


def check_examples(c):
    data = shipped()
    # Data as the spec states it.
    c.eq("share raws", [r["share"] for r in data.races],
         [0, 1503238554, 2362232013, 3006477107, 3564822856, 3994319585])
    c.eq("culture numbering", data.culture_ids, ["dwarven", "elven", "faerie", "goblin", "human", "orcish"])
    c.eq("tuning", (data.tuning["jitter"], data.tuning["gap"], data.tuning["form"]),
         (42949673, 100, "The Age of {plural}"))
    for cid in data.culture_ids:  # the TOML naming data equals P3-02 5 as the names reference transcribed it
        c.eq("naming %s" % cid, data.cultures[cid]["naming"], pnr.CULTURES[cid])
    c.eq("race_biome count", sum(len(r["biomes"]) for r in data.races), 48)  # 45 before P3-05 12.2
    c.eq("race_biome per race", [len(r["biomes"]) for r in data.races], [7, 8, 7, 9, 8, 9])  # P3-05 12.2
    c.eq("shipped data checks", check_history_data(data), [])
    for root in ("worldgen/tests/data/geo", "worldgen/tests/data/geo-small"):
        d = data_from_root(root)
        r1 = stage_with_regions(d, params(data=d), 42, [])
        r0 = stage_with_regions(data, params(), 42, [])
        c.eq("fixture root %s gives the shipped rows" % root, table_hashes(r1), table_hashes(r0))
    # C2
    for span, jm in ((2000, 20), (6000, 60), (12000, 120)):
        for p in range(1, 7):
            c.eq("C2 %d %d" % (span, p), bounds(span, p, data.tuning["jitter"], 100), (100, jm))
    c.eq("C2 G 333", bounds(2000, 6, 0, 2000)[0], 333)
    # 7.1 seed 42
    c.eq("K_aw 42", cr.stream_key(42, AWAKENING_STREAM), 0xA7284FA6D2415A60)
    res = stage_with_regions(data, params(), 42, [])
    s = res["schedule"]
    c.eq("42 bases", [x["base"] for x in s], [0, 2100, 3300, 4200, 4980, 5580])
    c.eq("42 u", [x["u"] for x in s[1:]], [0x48F251C73BC89E2C, 0x04C4CCA630137EBA, 0xDA392A12D57DEAEB,
                                            0x67CC0B87B4CE8D5A, 0xA1211F03C3A8FF14])
    c.eq("42 pick", [x["pick"] for x in s[1:]], [34, 2, 103, 49, 76])
    c.eq("42 d", [x["d"] for x in s[1:]], [-26, -58, 43, -11, 16])
    c.eq("42 years", [x["year"] for x in s], [0, 2074, 3242, 4243, 4969, 5596])
    c.eq("42 ages", res["age"], [
        (1, 1, 0, 2073, "The Age of Elves"), (2, 2, 2074, 3241, "The Age of Goblins"),
        (3, 3, 3242, 4242, "The Age of Dwarves"), (4, 4, 4243, 4968, "The Age of Faeries"),
        (5, 5, 4969, 5595, "The Age of Humans"), (6, 6, 5596, 5999, "The Age of Orcs")])
    c.eq("42 gods", [g[1] for g in res["god"]], ["Fielie", "Krok", "Bronhak", "Syfiash", "Wealstuld", "Kruu"])
    c.eq("42 attempts", res["attempts"], [0, 0, 1, 0, 0, 0])
    c.eq("42 report", report_lines(res), [
        "[chronology]", "races_present = 6", "awakening_years = 0 2074 3242 4243 4969 5596",
        "age_years = 2074 1168 1001 726 627 404", "age_years_min = 404", "gods = 6"])
    # seed 0, seed 1
    res = stage_with_regions(data, params(), 0, [])
    c.eq("0 years", [x["year"] for x in res["schedule"]], [0, 2068, 3265, 4178, 5003, 5534])
    c.eq("0 d", [x["d"] for x in res["schedule"][1:]], [-32, -35, -22, 23, -46])
    c.eq("0 gods", [g[1] for g in res["god"]], ["Saene", "Kruskux", "Nurnthaimm", "Whaia", "Steldhil", "Bald"])
    c.eq("0 faerie attempt", res["attempts"][3], 1)
    res = stage_with_regions(data, params(), 1, [])
    c.eq("1 years", [x["year"] for x in res["schedule"]], [0, 2071, 3341, 4178, 4934, 5580])
    c.eq("1 d", [x["d"] for x in res["schedule"][1:]], [-29, 41, -22, -46, 0])
    c.eq("1 gods", [g[1] for g in res["god"]], ["Laesiel", "Gragg-Gnurk", "Khummhor", "Synmia", "Steck", "Kaag"])
    # seed 7 span 2000
    res = stage_with_regions(data, params(span=2000), 7, [])
    c.eq("7 bases", [x["base"] for x in res["schedule"]], [0, 700, 1100, 1400, 1660, 1860])
    c.eq("7 years", [x["year"] for x in res["schedule"]], [0, 688, 1114, 1409, 1644, 1879])
    c.eq("7 age ends", [a[3] for a in res["age"]], [687, 1113, 1408, 1643, 1878, 1999])
    c.eq("7 gods", [g[1] for g in res["god"]], ["Vanir", "Grux", "Mammga", "Ouwheefy", "Gord", "Maark'zaagh"])
    # seed 3 elf,orc
    res = stage_with_regions(data, params(races=["elf", "orc"]), 3, [])
    c.eq("3 orc u", res["schedule"][1]["u"], 0xAEA8CB7C6C7550B5)
    c.eq("3 orc d", res["schedule"][1]["d"], 22)
    c.eq("3 years", [x["year"] for x in res["schedule"]], [0, 5602])
    c.eq("3 ages", res["age"], [(1, 1, 0, 5601, "The Age of Elves"), (2, 6, 5602, 5999, "The Age of Orcs")])
    c.eq("3 gods", [g[1] for g in res["god"]], ["Vaennela", "Buzz", "Nonthom", "Pyia", "Alis", "Uguuggrush"])
    c.eq("3 attempts", [res["attempts"][2], res["attempts"][4]], [1, 1])
    c.eq("3 absent rows", [(r[9], r[10], r[11]) for r in res["race"][1:5]],
         [(0, None, 2), (0, None, 3), (0, None, 4), (0, None, 5)])
    # seed 42 goblin,dwarf,human
    res = stage_with_regions(data, params(races=["goblin", "dwarf", "human"]), 42, [])
    c.eq("42 gdh ages", [a[:4] for a in res["age"]], [(1, 2, 0, 3241), (2, 3, 3242, 4968), (3, 5, 4969, 5999)])
    c.eq("42 gdh gods", [g[1] for g in res["god"]], ["Fielie", "Krok", "Bronhak", "Syfiash", "Wealstuld", "Kruu"])
    # 2^64-1, 12000, orc
    res = stage_with_regions(data, params(span=12000, races=["orc"]), M64, [])
    c.eq("max ages", res["age"], [(1, 6, 0, 11999, "The Age of Orcs")])
    c.eq("max gods", [g[1] for g in res["god"]], ["Ciefa", "Slogob", "Narinnadin", "Oupoubia", "Hard", "Kruurz"])
    # collisions
    res = stage_with_regions(data, params(), 42, list(COLLISION_REGIONS))
    c.eq("collision age 1", res["age"][0][4], "The Age of Elves II")
    c.eq("collision gods", [(g[1], t) for g, t in zip(res["god"][:2], res["attempts"][:2])],
         [("Mical", 2), ("Zatch", 1)])
    # clamp
    s = schedule(0, 6000, [(1, 0), (2, ONE // 2), (3, cr.parse_decimal("0.501")), (4, cr.parse_decimal("0.999"))],
                 0, 100)
    c.eq("clamp bases", [x["base"] for x in s], [0, 3000, 3006, 5994])
    c.eq("clamp years", [x["year"] for x in s], [0, 3000, 3100, 5900])
    # 8.2: the registry is exactly the names of region, age and god
    regs = ["North Downs", "North Downs II", "East Wood"]
    res = stage_with_regions(data, params(), 42, regs)
    c.eq("8.2 registry", res["registry"], set(regs) | {a[4] for a in res["age"]} | {g[1] for g in res["god"]})


def check_data_checks(c):
    data = shipped()
    import copy
    races = copy.deepcopy(data.races)
    races[0]["biomes"].append(("lake", 10))  # elf prefers lake
    c.eq("D1", check_history_data(edited(data, races=races)), [("races", "race 'elf' prefers water biome 'lake'")])
    races = copy.deepcopy(data.races)
    races[1]["biomes"].append(("blighted_waste", 10))
    c.eq("D2", check_history_data(edited(data, races=races)),
         [("races", "race 'goblin' prefers biome 'blighted_waste', which no geography stage assigns")])
    races = copy.deepcopy(data.races)
    races[2]["culture"] = "elven"
    c.eq("D3", check_history_data(edited(data, races=races)),
         [("races", "race 'dwarf' has culture 'elven' of race 'elf'")])
    long_form = "X" * 42 + "{plural}"  # 49 bytes for the seven-letter plurals
    t = dict(data.tuning, form=long_form)
    want = ["race '%s' has the age name '%s', which is longer than 48 bytes" % (rid, "X" * 42 + pl)
            for rid, pl in (("dwarf", "Dwarves"), ("faerie", "Faeries"), ("goblin", "Goblins"))]
    c.eq("D4", check_history_data(edited(data, tuning=t)), [("races", m) for m in want])
    t = dict(data.tuning, form="Age {plural} 2")
    got = check_history_data(edited(data, tuning=t))
    c.eq("D5 count", len(got), 6)
    c.eq("D5 first", got[0], ("races", "race 'dwarf' has the age name 'Age Dwarves 2', which is not ASCII letters "
                                        "with single spaces, ' and -"))
    # two at once, sorted by message bytes
    races = copy.deepcopy(data.races)
    races[5]["biomes"].append(("ocean", 1))
    races[0]["biomes"].append(("lake", 1))
    got = check_history_data(edited(data, races=races))
    c.eq("D0", check_history_data(edited(data, races=[], cultures={})),
         [("races", "no race is defined; kind 'race' needs at least one definition")])
    c.eq("two sorted", got, [("races", "race 'elf' prefers water biome 'lake'"),
                             ("races", "race 'orc' prefers water biome 'ocean'")])
    for form, ok in (("The Age of {plural}", True), (" Age {plural}", False), ("Age  {plural}", False),
                     ("Age {plural} ", False), ("O'{adjective}-Age", True)):
        c.eq("D5 %r" % form, age_name_charset_ok(age_name_text(form, "Elves", "Elven")), ok)
    # C1 entry checks
    p = params()
    c.eq("C1 regions", entry_checks(data, p, []), "chronology: input precondition: no region rows")
    c.eq("C1 span", entry_checks(data, params(span=1999), ["r"]),
         "chronology: settings: history_span 1999 outside 2000 to 12000")
    c.eq("C1 empty", entry_checks(data, params(races=[]), ["r"]), "chronology: settings: no race present")
    c.eq("C1 unknown", entry_checks(data, params(races=["elf", "troll", "gnome"]), ["r"]),
         "chronology: settings: unknown race 'troll'")
    c.eq("C1 order", entry_checks(data, params(races=["orc", "elf"]), ["r"]),
         "chronology: settings: races not in race order at 'elf'")
    c.eq("C1 repeat", entry_checks(data, params(races=["elf", "elf"]), ["r"]),
         "chronology: settings: races not in race order at 'elf'")
    races = copy.deepcopy(data.races)
    races[0]["biomes"].append(("lake", 10))
    c.eq("C1 data", entry_checks(edited(data, races=races), p, ["r"]),
         "chronology: data: race 'elf' prefers water biome 'lake'")
    # age name forms
    c.eq("form adjective", age_name_text("The {adjective} Age", "Elves", "Elven"), "The Elven Age")
    c.eq("form both twice", age_name_text("{plural} {adjective} {plural} {adjective}", "Elves", "Elven"),
         "Elves Elven Elves Elven")
    c.eq("form no rescan", age_name_text("{plural}", "{adjective}", "X"), "{adjective}")
    # numeral fallback II then III
    res = stage_with_regions(data, params(), 42, ["The Age of Elves", "The Age of Elves II"])
    c.eq("age III", res["age"][0][4], "The Age of Elves III")
    # meta
    c.eq("meta defaults", meta_entries(params()), [
        ("param.civs_per_race", "auto"), ("param.divine_activity", "normal"), ("param.divine_script", ""),
        ("param.event_intensity", "normal"), ("param.history_detail", "normal"), ("param.history_span", "6000"),
        ("param.monster_pressure", "normal"), ("param.races", "elf,goblin,dwarf,faerie,human,orc")])
    for bad in ("2000 ", "02000", "1999", "12001", "+2000", ""):
        c.eq("span %r" % bad, meta_value_ok("param.history_span", bad), False)
    c.eq("detail", [meta_value_ok("param.history_detail", v) for v in ("Normal", "normal", "dense ")],
         [False, True, False])
    c.eq("civs", [meta_value_ok("param.civs_per_race", v) for v in ("auto", "4", "5", "0", "auto,1", "01")],
         [True, True, False, False, False, False])
    c.eq("races ok", meta_value_ok("param.races", "elf,orc"), True)
    c.eq("races bad", [meta_value_ok("param.races", v) for v in ("elf,,orc", "elf,elf", "Elf", "auto,1", "")],
         [False, False, False, False, False])
    c.eq("script", [meta_value_ok("param.divine_script", v) for v in ("", "a" * 64, "a" * 63, "A" * 64)],
         [True, True, False, False])
    p = params(span=2000, races=["orc"], detail="dense", civs=3, intensity="calm", pressure="high",
               activity="meddling")
    c.eq("meta round trip", params_from_meta(dict(meta_entries(p))), p)


def check_properties(c, seeds=100):
    data = shipped()
    jit, gap = data.tuning["jitter"], data.tuning["gap"]
    full = {}
    import itertools
    subsets = [s for k in range(1, 7) for s in itertools.combinations(data.races, k)]
    c.eq("63 subsets", len(subsets), 63)
    bad = 0
    for seed in range(seeds):
        for span in (2000, 2001, 6000, 11999, 12000):
            sf = schedule(seed, span, [(r["order"], r["share"]) for r in data.races], jit, gap)
            yfull = {x["id"]: x["year"] for x in sf}
            g, jmax = bounds(span, 6, jit, gap)
            for sub in subsets:
                s = schedule(seed, span, [(r["order"], r["share"]) for r in sub], jit, gap)
                ys = [x["year"] for x in s]
                ok = ys[0] == 0 and all(b - a >= g for a, b in zip(ys, ys[1:])) and ys[-1] <= span - g
                ok = ok and all(abs(x["year"] - x["base"]) <= jmax for x in s[1:])
                ok = ok and all(x["year"] == yfull[x["id"]] for x in s[1:])  # independence of the subset
                bad += 0 if ok else 1
    c.eq("property sweep (invariants 2, 3; subset independence)", bad, 0)
    # 14.2 item 14 in small
    bad = 0
    for name, seed, span, gp, jt, ids, shares in tuned_cases():
        s = schedule(seed, span, list(zip(ids, shares)), jt, gp)
        g, _ = bounds(span, len(ids), jt, gp)
        ys = [x["year"] for x in s]
        bad += 0 if (ys[0] == 0 and all(b - a >= g for a, b in zip(ys, ys[1:])) and ys[-1] <= span - g) else 1
    c.eq("packed shares keep order", bad, 0)
    # 14.1 item 3 at seed 42, span 6000
    rf = stage_with_regions(data, params(), 42, [])
    yfull = {r[0]: r[10] for r in rf["race"]}
    bad = 0
    for sub in subsets:
        ids = [r["id"] for r in sub]
        res = stage_with_regions(data, params(races=ids), 42, [])
        ok = res["age"][0][2] == 0 and [a[0] for a in res["age"]] == list(range(1, len(ids) + 1))
        ok = ok and all(a[2] == yfull[a[1]] for a in res["age"][1:])
        ok = ok and res["god"] == rf["god"]
        bad += 0 if ok else 1
    c.eq("63 subsets at seed 42", bad, 0)


def check_statistics(c, seeds=2000):
    """7 C3 and 12: no clamp on seeds 0..1999; least gap 161/483/966; every
    value of base +- jmax occurs for goblins and orcs; age_years_min ranges."""
    data = shipped()
    jit, gap = data.tuning["jitter"], data.tuning["gap"]
    present = [(r["order"], r["share"]) for r in data.races]
    for span, least, (glo, ghi), (olo, ohi), (mlo, mhi) in (
            (2000, 161, (680, 720), (1840, 1880), (120, 160)),
            (6000, 483, (2040, 2160), (5520, 5640), (360, 480)),
            (12000, 966, (4080, 4320), (11040, 11280), (720, 960))):
        gaps = []
        clamps = 0
        gob = set()
        orc = set()
        mins = []
        for seed in range(seeds):
            s = schedule(seed, span, present, jit, gap)
            ys = [x["year"] for x in s]
            clamps += sum(1 for x in s[1:] if x["year"] != x["base"] + x["d"])
            gaps.append(min(b - a for a, b in zip(ys, ys[1:])))
            gob.add(ys[1])
            orc.add(ys[5])
            lens = [b - a for a, b in zip(ys, ys[1:] + [span])]
            mins.append(min(lens))
            if lens.index(min(lens)) != 5:
                clamps += 1000000  # the last age is always the shortest (12)
        c.eq("no clamp %d" % span, clamps, 0)
        c.eq("least gap %d" % span, min(gaps), least)
        c.eq("goblin range %d" % span, (min(gob), max(gob), len(gob)), (glo, ghi, ghi - glo + 1))
        c.eq("orc range %d" % span, (min(orc), max(orc), len(orc)), (olo, ohi, ohi - olo + 1))
        c.true("age_years_min %d within %d-%d" % (span, mlo, mhi), mlo <= min(mins) and max(mins) <= mhi)
    # god names at attempt > 0 on seeds 0, 1, 3, 7, 42: 4 of 30
    n = 0
    for seed in (0, 1, 3, 7, 42):
        res = stage_with_regions(data, params(), seed, [])
        n += sum(1 for t in res["attempts"] if t != 0)
    c.eq("gods at attempt > 0", n, 4)


class version:
    """Context: serve data version v (p3_data_reference) with the names
    rules of that version (P3-02 r4 for 0.16.0, r5 for 0.17.0)."""

    def __init__(self, v):
        self.v = v

    def __enter__(self):
        global _SHIPPED
        self.saved = (pdr.DATA_VERSION, pnr.RULES, _SHIPPED)
        pnr.use_version(self.v)
        pnr.use_rules("r4" if self.v == "0.16.0" else "r5")
        _SHIPPED = None
        return self

    def __exit__(self, *a):
        global _SHIPPED
        pnr.use_version(self.saved[0])
        pnr.use_rules(self.saved[1])
        _SHIPPED = self.saved[2]
        return False


def check_r3(c):
    """P3-03 r3 (generator 0.17.0): gods with the filter and god_patterns
    (P3-02 11.6's god table, drawn by this stage in race order with an
    empty region list), and the stage check names_form_text."""
    data = shipped()
    for seed, row in pnr.GODS_R5.items():
        res = stage_with_regions(data, params(), seed, [])
        got = [g[1] for g in res["god"]]
        if pdr.at_least("0.20.0"):
            # P3-02 r15 14: the faerie god changes with 14.1's lists; another god only where 0.20.0 rejects its
            # 0.19.0 attempt (14.2, 14.3)
            old = row.split(", ")
            for i, (g, o) in enumerate(zip(got, old)):
                cid = pnr.GOD_RACE_CULTURES[i]
                if g != o and cid != "faerie":
                    c.eq("r3 gods seed %d %s: %s redrawn because 0.20.0 rejects it" % (seed, cid, o),
                         pnr.old_name_rejected(pnr.CULTURES[cid], "god", seed, pnr.CULTURE_NUMBER[cid], i + 1, o), True)
        elif pdr.at_least("0.19.0"):
            # P3-02 r10 12.1: only the gods the new filter entries reject (Grammam, Gordman) are redrawn
            old = row.split(", ")
            c.eq("r3 gods seed %d (P3-02 11.6, r10 12.1)" % seed, [g for g, o in zip(got, old) if g != o],
                 [g for g, o in zip(got, old) if pnr.name_filtered(pnr.shipped_filter(), pnr.lower(o))])
        else:
            c.eq("r3 gods seed %d (P3-02 11.6)" % seed, ", ".join(got), row)
    c.eq("r3 shipped data passes names_form_text", entry_checks(data, params(), ["x"]), None)
    for form, entry in (("{root}" + "cock", "cock"), ("Shit" + "{root}", "shit"),
                        ("The Scrap" + "yard of {root}", "crap"), ("The Snot" + "ty Court of {root}", "snot")):
        cu = dict(data.cultures)
        cu["human"] = dict(cu["human"], naming=dict(cu["human"]["naming"], settlement_forms=[form]))
        d = edited(data, cultures=cu)
        c.eq("r3 names_form_text refuses %s" % form, entry_checks(d, params(), ["x"]),
             "history: culture 'human' settlement_forms[0] '%s' holds the filtered text '%s'" % (form, entry))
        try:
            stage_with_regions(d, params(), 42, [])
            c.eq("r3 names_form_text is fatal before any name", "passed", "fatal")
        except FatalError:
            pass
    # A data check failing earlier wins (names_form_text comes after them).
    races = [dict(r) for r in data.races]
    races[0]["culture"] = "orcish"  # D3
    cu = dict(data.cultures)
    cu["human"] = dict(cu["human"], naming=dict(cu["human"]["naming"], settlement_forms=["Shit" + "{root}"]))
    msg = entry_checks(edited(data, races=races, cultures=cu), params(), ["x"])
    c.eq("r3 names_form_text after the data checks: %s" % msg, msg is not None and msg.startswith("chronology: data: "),
         True)


def cmd_check():
    c = Checker()
    # The worked examples, data checks and statistics of 7.1, 7 and 12 are
    # those of P3-02 r4 (0.13.0 to 0.16.0): they run on the 0.16.0 data and
    # rules; r3's changes (0.17.0) are checked by check_r3, the properties on
    # both, and the golden file on 0.17.0.
    with version("0.16.0"):
        check_examples(c)
        check_data_checks(c)
        check_properties(c)
        check_statistics(c)
    check_r3(c)
    check_properties(c)
    if os.path.exists(GOLDEN) and os.path.exists(INPUTS_READY):
        if pdr.at_least("0.21.0"):
            with open(GOLDEN, "rb") as f:
                committed = f.read().decode()
        else:   # P3-10, P3-11, P5-01: the file of the served version, as of its base commit
            import subprocess
            committed = subprocess.run(["git", "-C", REPO, "show", (pdr.P5_01_BASE_COMMIT if pdr.DATA_VERSION == "0.20.0"
                                                                     else pdr.P3_11_BASE_COMMIT if pdr.DATA_VERSION == "0.19.0"
                                                                     else pdr.P3_10_BASE_COMMIT) +
                                        ":worldgen/tests/golden/p3-03-chronology.txt"], check=True,
                                       capture_output=True, text=True).stdout
        c.eq("golden file current", committed, golden_text(DEFAULT_INPUTS))
    else:
        print("note: golden comparison skipped (no golden file or no input worlds)")
    print("%d checks, %d failures" % (c.count, c.failures))
    return 1 if c.failures else 0


def cmd_golden(argv):
    out = GOLDEN
    inputs = DEFAULT_INPUTS
    rest = list(argv)
    if "--inputs" in rest:
        i = rest.index("--inputs")
        inputs = rest[i + 1]
        del rest[i:i + 2]
    if rest:
        out = rest[0]
    text = golden_text(inputs)
    if out == "-":
        sys.stdout.write(text)
    else:
        with open(out, "w", newline="\n") as f:
            f.write(text)
    return 0


def cmd_schedule(argv):
    seed, span = int(argv[0], 0), int(argv[1])
    races = argv[2] if len(argv) > 2 else "all"
    data = shipped()
    p = params(span=span, races=races_of(races))
    res = stage_with_regions(data, p, seed, [])
    print("K_aw %016x" % cr.stream_key(seed, AWAKENING_STREAM))
    print("G jmax %d %d" % bounds(span, len(p["races"]), data.tuning["jitter"], data.tuning["gap"]))
    for line in case_lines("s", res, p):
        print(line)
    return 0


def cmd_check_world(argv):
    path = argv[0]
    root = "game/data"
    if "--data" in argv:
        root = argv[argv.index("--data") + 1]
    data = data_from_root(root)
    conn = open_world(path)
    meta = dict(conn.execute("SELECT key, value FROM meta").fetchall())
    c = Checker()
    for k, v in meta_entries(params()):
        c.true("meta %s present" % k, k in meta)
        if k in meta:
            c.true("meta %s=%r canonical" % (k, meta[k]), meta_value_ok(k, meta[k]))
    if c.failures:
        print("%d failures" % c.failures)
        return 1
    p = params_from_meta(meta)
    c.eq("races in race order", p["races"], races_of(",".join(p["races"]), data))
    regions = world_region_names(conn)
    stages = meta["stages"].split(",") if meta["stages"] else []
    if "chronology" in stages:
        res = chronology(data, p, int(meta["seed"]), regions)
        want = table_hashes(res)
        for t in TABLES:
            got, _ = world_table_hash(conn, t)
            c.eq("table %s" % t, got, want[t])
        names = regions + [a[4] for a in res["age"]] + [g[1] for g in res["god"]]
        c.eq("names distinct", len(set(names)), len(names))
    else:
        for t in TABLES:
            c.eq("table %s empty" % t, conn.execute('SELECT count(*) FROM "%s"' % t).fetchone()[0], 0)
    conn.close()
    print("%s: %d checks, %d failures" % (path, c.count, c.failures))
    return 1 if c.failures else 0


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    global DEFAULT_INPUTS, INPUTS_READY, _SHIPPED
    if "--version" in argv:   # P3-10: 0.19.0 (default) or 0.18.0 / 0.17.0 (the filter and inputs of that version)
        i = argv.index("--version")
        v = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
        pnr.use_version(v)
        _SHIPPED = None
        DEFAULT_INPUTS = INPUTS_BY_VERSION.get(v, INPUTS_BY_VERSION["0.18.0"])
        INPUTS_READY = os.path.join(DEFAULT_INPUTS, "READY") if v in ("0.19.0", "0.20.0", "0.21.0", "0.22.0") else DEFAULT_INPUTS
    cmd = argv[0]
    if cmd == "check":
        return cmd_check()
    if cmd == "golden":
        return cmd_golden(argv[1:])
    if cmd == "schedule":
        return cmd_schedule(argv[1:])
    if cmd == "check-world":
        return cmd_check_world(argv[1:])
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.modules.setdefault("p3_chronology_reference", sys.modules["__main__"])   # one module object when run as a script (P5-04a integration)
    sys.exit(main(sys.argv[1:]))
