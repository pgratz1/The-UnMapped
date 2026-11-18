#!/usr/bin/env python3
"""Independent exact-integer reference of the P1-05 resources stage.

Written by the test writer from docs/specs/p1-05-resources-habitability.md
(second version, generator 0.10.0: seven layers fertility, ore, timber,
fresh_water, forage, habitability, walk_area; steps R1 to R7; the report
keys and the `score` palette of section 9) and docs/specs/p1-00-geography-
overview.md (G6, G7 with k = 1, G8, 10.2, 10.3) only, without reading any
implementation of the stage and without the spec writer's prototype. The
input layers of a world come from p1_biomes_reference.py (itself on the
hydrology and climate references); for the large worlds they are decoded
from a 0.9.0 world file and every one is first checked against the layer
hashes of p1-04-biomes.txt, so no golden value depends on an unchecked
input.

Python 3 standard library only, arbitrary-precision integers, no floating
point.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p1_resources_reference.py check [--quick]
        Every worked example of P1-05 5.1 (helpers, tangents, the chamfer
        grid, R3, R4, R6 with every raw value of the table, R5 (a) to (c)
        and the diagonal limit), the palette example of 9, a second method
        for each step (Dijkstra chamfer, union-find walk areas, recursive
        G7) and invariants 1 to 5 of section 6 on the designed fixtures and
        (without --quick) on the golden worlds up to N = 128.

    python3 -B worldgen/tests/reference/p1_resources_reference.py golden [PATH|-] [--large] [--db-dir DIR]
        Write worldgen/tests/golden/p1-05-resources.txt ("-": standard
        output). --large adds 512 seeds 0, 42, the phrase and 1024 seed 1;
        with --db-dir DIR their inputs are decoded from DIR/<case>.db (a
        0.9.0 world with the shipped data) after each of the seven input
        layers is checked against p1-04-biomes.txt; without it they are
        recomputed by the references (slow: the elevation reference takes
        minutes per world; UNMAPPED_ELEVATION_CACHE=DIR caches it).

    python3 -B worldgen/tests/reference/p1_resources_reference.py case NAME
        Print the items of one golden case.
"""

import hashlib
import heapq
import math
import os
import sqlite3
import sys
from collections import deque

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import core_reference as cr  # noqa: E402
import p1_elevation_reference as er  # noqa: E402
import p1_biomes_reference as br  # noqa: E402

FatalError = cr.FatalError
ONE = 1 << 32
HALF = 1 << 31
INF = 1 << 40

PRE = "resources: input precondition: "
STREAM_ORE = "geo.resources.ore"

WATER_DRY, WATER_RIVER, WATER_LAKE, WATER_OCEAN = 0, 1, 2, 3
DX = (1, 1, 0, -1, -1, -1, 0, 1)
DY = (0, 1, 1, 1, 0, -1, -1, -1)

LAYERS = (("fertility", 1), ("ore", 1), ("timber", 1), ("fresh_water", 1), ("forage", 1), ("habitability", 1),
          ("walk_area", 3))
INPUTS = (("elevation", 4), ("water", 1), ("water_depth", 3), ("flow_dir", 1), ("river", 1), ("rainfall", 3),
          ("biome", 1))


def fatal(msg):
    raise FatalError(msg)


def dec(text):
    v = cr.parse_decimal(text)
    if v is None:
        raise ValueError(text)
    return v


# ---------------------------------------------------------------------------
# Data (4.1, 4.2)
# ---------------------------------------------------------------------------

# 4.1: (fertility, timber, forage) of each code, as decimal text.
BIOME_VALUES = {
    1: ("0", "0", "0"),          # ocean
    2: ("0", "0", "0"),          # lake
    3: ("0.7", "1", "0.5"),      # temperate_forest
    4: ("1", "0.15", "0.3"),     # grassland
    5: ("0.45", "0.3", "0.8"),   # marsh
    6: ("0.3", "0.1", "0.25"),   # highland_moor
    7: ("0", "0", "0.05"),       # alpine
    8: ("0", "0", "0"),          # blighted_waste
    9: ("0.2", "0.05", "0.2"),   # heath
    10: ("0.7", "0.05", "0.35"),  # steppe (fertility 0.7 since 0.10.0)
    11: ("0.5", "0.7", "0.7"),   # wet_woodland
    12: ("0.25", "0.2", "0.6"),  # fen
}


def biome_values():
    return {c: tuple(dec(v) for v in t) for c, t in BIOME_VALUES.items()}


# 4.2 in declaration order: (field, kind, shipped text). kind int | fx | fbm.
TUNING = [
    ("fresh_reach_m", "int", "400"),
    ("fresh_rain_weight", "fx", "0.4"),
    ("fresh_rain_lo_mm", "int", "300"),
    ("fresh_rain_hi_mm", "int", "1500"),
    ("fert_slope_lo", "fx", "0.18"),
    ("fert_slope_hi", "fx", "0.47"),
    ("alluvial_bonus", "fx", "0.3"),
    ("alluvial_hand_m", "fx", "5"),
    ("timber_slope_lo", "fx", "0.7"),
    ("timber_slope_hi", "fx", "1.4"),
    ("fish_river_class_min", "int", "4"),
    ("fish_weight", "fx", "0.3"),
    ("fish_reach_m", "int", "300"),
    ("ore_noise", "fbm", None),
    ("ore_vein_lo", "fx", "0"),
    ("ore_vein_hi", "fx", "0.2"),
    ("ore_elev_lo_m", "int", "100"),
    ("ore_elev_hi_m", "int", "800"),
    ("ore_lowland", "fx", "0.15"),
    ("ore_slope_base", "fx", "0.5"),
    ("ore_slope_lo", "fx", "0.3"),
    ("ore_slope_hi", "fx", "1.0"),
    ("walk_slope_max", "fx", "0.7002"),
    ("walk_min_area_m2", "int", "250000"),
    ("hab_slope_lo", "fx", "0.18"),
    ("hab_slope_hi", "fx", "0.47"),
    ("hab_weight_fertility", "int", "55"),
    ("hab_weight_forage", "int", "20"),
    ("hab_weight_timber", "int", "15"),
    ("hab_weight_ore", "int", "10"),
    ("hab_water_min", "fx", "0.25"),
    ("hab_flood_hand_m", "fx", "1"),
    ("hab_flood_factor", "fx", "0.5"),
    ("hab_pocket_factor", "fx", "0.25"),
]
KIND = {f: k for f, k, _ in TUNING}


def shipped_tuning():
    t = {}
    for f, k, v in TUNING:
        if k == "int":
            t[f] = int(v)
        elif k == "fx":
            t[f] = dec(v)
        else:
            t[f] = (1500, 3, dec("0.5"))  # (wavelength_m, octaves, gain raw)
    return t


def tuning_of(overrides):
    """overrides: list of (field, text): an integer, or an fx as decimal text
    without quotes; ore_noise as wavelength/octaves/gain."""
    t = shipped_tuning()
    for f, v in overrides:
        if f not in KIND:
            raise KeyError(f)
        if KIND[f] == "int":
            t[f] = int(v)
        elif KIND[f] == "fx":
            t[f] = dec(v)
        else:
            a, b, c = v.split("/")
            t[f] = (int(a), int(b), dec(c))
    return t


def check_tuning(t):
    """The named checks of 4.2 (the loader's; here a guard)."""
    for lo, hi in (("fresh_rain_lo_mm", "fresh_rain_hi_mm"), ("fert_slope_lo", "fert_slope_hi"),
                   ("timber_slope_lo", "timber_slope_hi"), ("ore_vein_lo", "ore_vein_hi"),
                   ("ore_elev_lo_m", "ore_elev_hi_m"), ("ore_slope_lo", "ore_slope_hi"),
                   ("hab_slope_lo", "hab_slope_hi")):
        if not t[lo] < t[hi]:
            fatal("%s must be below %s" % (lo, hi))
    if t["hab_weight_fertility"] + t["hab_weight_forage"] + t["hab_weight_timber"] + t["hab_weight_ore"] < 1:
        fatal("the hab_weight fields must not all be 0")


# ---------------------------------------------------------------------------
# Helpers (5, notation)
# ---------------------------------------------------------------------------

def ramp(x, a, b):
    if not a < b:
        fatal("ramp: a must be below b")
    if x <= a:
        return 0
    if x >= b:
        return ONE
    return er.div(x - a, b - a)


def q255(s):
    s = min(max(s, 0), ONE)
    return min(max(er.round_int(er.mul_int(s, 255)), 0), 255)


def metres(d):
    return min(32767, (32 * d + 5) // 10)


def tangent(s2):
    """G8: div_int(from_raw(isqrt_u128(S2 x 2^64)), 320)."""
    if s2 < 0:
        fatal("S2 < 0")
    return er.div_int(math.isqrt(s2 << 64), 320)


def not_steeper(s2, t_raw):
    return s2 * (1 << 64) <= (320 * t_raw) ** 2


# ---------------------------------------------------------------------------
# R3: chamfer (P1-02 H7, 5/7, two in-place passes) and fresh water
# ---------------------------------------------------------------------------

def chamfer(w, h, source):
    n = w * h
    d = [0 if source[i] else INF for i in range(n)]
    fwd = ((-1, 0, 5), (-1, -1, 7), (0, -1, 5), (1, -1, 7))
    bwd = ((1, 0, 5), (1, 1, 7), (0, 1, 5), (-1, 1, 7))
    for i in range(n):
        x, y = i % w, i // w
        best = d[i]
        for dx, dy, wt in fwd:
            xx, yy = x + dx, y + dy
            if 0 <= xx < w and 0 <= yy < h:
                v = d[yy * w + xx] + wt
                if v < best:
                    best = v
        d[i] = best
    for i in range(n - 1, -1, -1):
        x, y = i % w, i // w
        best = d[i]
        for dx, dy, wt in bwd:
            xx, yy = x + dx, y + dy
            if 0 <= xx < w and 0 <= yy < h:
                v = d[yy * w + xx] + wt
                if v < best:
                    best = v
        d[i] = best
    return d


def chamfer_dijkstra(w, h, source):
    """Second method: shortest paths over the 8-neighbour graph with weights
    5 (orthogonal) and 7 (diagonal)."""
    n = w * h
    d = [INF] * n
    q = []
    for i in range(n):
        if source[i]:
            d[i] = 0
            q.append((0, i))
    heapq.heapify(q)
    while q:
        dc, c = heapq.heappop(q)
        if dc != d[c]:
            continue
        x, y = c % w, c // w
        for k in range(8):
            xx, yy = x + DX[k], y + DY[k]
            if 0 <= xx < w and 0 <= yy < h:
                m = yy * w + xx
                v = dc + (7 if k & 1 else 5)
                if v < d[m]:
                    d[m] = v
                    heapq.heappush(q, (v, m))
    return d


def sources(water, river, t):
    src_f = [1 if v in (WATER_RIVER, WATER_LAKE) else 0 for v in water]
    k = t["fish_river_class_min"]
    src_g = [1 if (water[i] in (WATER_LAKE, WATER_OCEAN) or (water[i] == WATER_RIVER and river[i] >= k)) else 0
             for i in range(len(water))]
    return src_f, src_g


def fresh_parts(df, rainfall, t):
    reach = t["fresh_reach_m"]
    fs = 0 if df >= reach else er.from_ratio(reach - df, reach)
    fr = er.mul(t["fresh_rain_weight"], ramp(er.from_int(rainfall), er.from_int(t["fresh_rain_lo_mm"]),
                                             er.from_int(t["fresh_rain_hi_mm"])))
    return fs, fr


def fresh_water_cell(df, rainfall, t):
    fs, fr = fresh_parts(df, rainfall, t)
    return q255(max(fs, fr))


# ---------------------------------------------------------------------------
# R4: fertility, timber, forage, ore
# ---------------------------------------------------------------------------

def fert_parts(fb, s2, hand, t):
    tan = tangent(s2)
    sf = ONE - ramp(tan, t["fert_slope_lo"], t["fert_slope_hi"])
    hm = er.from_ratio(hand, 10)
    al = ONE + er.mul(t["alluvial_bonus"], ONE - ramp(hm, 0, t["alluvial_hand_m"]))
    return sf, al


def fertility_cell(fb, s2, hand, t):
    sf, al = fert_parts(fb, s2, hand, t)
    return q255(min(ONE, er.mul(er.mul(fb, sf), al)))


def timber_factor(s2, t):
    return ONE - ramp(tangent(s2), t["timber_slope_lo"], t["timber_slope_hi"])


def timber_cell(tb, s2, t):
    return q255(er.mul(tb, timber_factor(s2, t)))


def fish_term(dg, t):
    return er.mul(t["fish_weight"], ONE - ramp(er.from_int(dg), 0, er.from_int(t["fish_reach_m"])))


def forage_cell(gb, dg, t):
    return q255(min(ONE, gb + fish_term(dg, t)))


def ore_parts(elevation, s2, noise, t):
    tan = tangent(s2)
    em = er.from_ratio(elevation, 10)
    eu = max(t["ore_lowland"], ramp(em, er.from_int(t["ore_elev_lo_m"]), er.from_int(t["ore_elev_hi_m"])))
    su = t["ore_slope_base"] + er.mul(ONE - t["ore_slope_base"], ramp(tan, t["ore_slope_lo"], t["ore_slope_hi"]))
    vn = ramp(noise, t["ore_vein_lo"], t["ore_vein_hi"])
    return eu, su, vn


def ore_cell(elevation, s2, noise, t):
    eu, su, vn = ore_parts(elevation, s2, noise, t)
    return q255(er.mul(er.mul(eu, su), vn))


def ore_noise(seed, x, y, t, key=None):
    if key is None:
        key = cr.stream_key(seed, STREAM_ORE)
    return er.fbm_m(key, er.from_int(16 * x + 8), er.from_int(16 * y + 8), t["ore_noise"])


# ---------------------------------------------------------------------------
# R5: walk areas
# ---------------------------------------------------------------------------

def walkable_mask(water, s2, walk_raw):
    lim = (320 * walk_raw) ** 2
    return [1 if (water[i] in (WATER_DRY, WATER_RIVER) and s2[i] * (1 << 64) <= lim) else 0
            for i in range(len(water))]


def linked_pair(sa, sb, diagonal, walk_raw):
    wgt = 2 if diagonal else 1
    return (sa - sb) ** 2 * (1 << 64) <= wgt * (160 * walk_raw) ** 2


def walk_components(w, h, S, water, s2, walk_raw):
    """The partition of R5 by FIFO BFS (G2's form), in label order: a list
    of lists of cells."""
    n = w * h
    wk = walkable_mask(water, s2, walk_raw)
    label = [0] * n
    comps = []
    for s in range(n):
        if not wk[s] or label[s]:
            continue
        lab = len(comps) + 1
        label[s] = lab
        q = deque([s])
        cells = []
        while q:
            c = q.popleft()
            cells.append(c)
            x, y = c % w, c // w
            for k in range(8):
                xx, yy = x + DX[k], y + DY[k]
                if not (0 <= xx < w and 0 <= yy < h):
                    continue
                m = yy * w + xx
                if wk[m] and not label[m] and linked_pair(S[c], S[m], k & 1, walk_raw):
                    label[m] = lab
                    q.append(m)
        comps.append(cells)
    return comps


def walk_areas(w, h, S, water, s2, walk_raw, min_cells):
    comps = walk_components(w, h, S, water, s2, walk_raw)
    ids = [0] * (w * h)
    k = 0
    dropped = 0
    for cells in comps:
        if len(cells) < min_cells:
            dropped += 1
            continue
        k += 1
        if k > 65535:
            fatal("resources: more than 65535 walk areas")
        for c in cells:
            ids[c] = k
    return ids, k, dropped


def walk_areas_unionfind(w, h, S, water, s2, walk_raw, min_cells):
    """Second method: union-find over the linked pairs (each pair once,
    east, south-east, south, south-west), ids by smallest index."""
    n = w * h
    wk = walkable_mask(water, s2, walk_raw)
    parent = list(range(n))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for c in range(n):
        if not wk[c]:
            continue
        x, y = c % w, c // w
        for k in (0, 1, 2, 3):
            xx, yy = x + DX[k], y + DY[k]
            if 0 <= xx < w and 0 <= yy < h:
                m = yy * w + xx
                if wk[m] and linked_pair(S[c], S[m], k & 1, walk_raw):
                    ra, rb = find(c), find(m)
                    if ra != rb:
                        parent[max(ra, rb)] = min(ra, rb)
    size = {}
    smallest = {}
    for c in range(n):
        if wk[c]:
            r = find(c)
            size[r] = size.get(r, 0) + 1
            smallest.setdefault(r, c)
    kept = sorted(smallest[r] for r in size if size[r] >= min_cells)
    rank = {find(c): i + 1 for i, c in enumerate(kept)}
    return [rank.get(find(c), 0) if wk[c] else 0 for c in range(n)], len(kept)


def min_cells_of(t):
    return (t["walk_min_area_m2"] + 255) // 256


# ---------------------------------------------------------------------------
# R6: habitability
# ---------------------------------------------------------------------------

def hab_parts(F, W, G, T, O, s2, hand, walk, t):
    sw = t["hab_weight_fertility"] + t["hab_weight_forage"] + t["hab_weight_timber"] + t["hab_weight_ore"]
    sc = (t["hab_weight_fertility"] * F + t["hab_weight_forage"] * G + t["hab_weight_timber"] * T +
          t["hab_weight_ore"] * O)
    b = er.from_ratio(sc, 255 * sw)
    wat = t["hab_water_min"] + er.mul(ONE - t["hab_water_min"], er.from_ratio(W, 255))
    slope = ONE - ramp(tangent(s2), t["hab_slope_lo"], t["hab_slope_hi"])
    flood = er.from_int(hand) < er.mul_int(t["hab_flood_hand_m"], 10)
    pocket = walk == 0
    return sc, b, wat, slope, flood, pocket


def habitability_cell(F, W, G, T, O, s2, hand, walk, t):
    sc, b, wat, slope, flood, pocket = hab_parts(F, W, G, T, O, s2, hand, walk, t)
    h = er.mul(b, wat)
    h = er.mul(h, slope)
    if flood:
        h = er.mul(h, t["hab_flood_factor"])
    if pocket:
        h = er.mul(h, t["hab_pocket_factor"])
    return q255(min(ONE, h))


# ---------------------------------------------------------------------------
# The stage
# ---------------------------------------------------------------------------

def preconditions(w, h, L, codes):
    for name, _ in INPUTS:
        if name not in L:
            fatal(PRE + "missing layer '%s'" % name)
    for name, _ in INPUTS:
        if len(L[name]) != w * h:
            fatal(PRE + "%s length is not width * height" % name)
    for name, hi in (("water", 3), ("river", 15)):
        for i, v in enumerate(L[name]):
            if v > hi:
                fatal(PRE + "%s value %d at (%d, %d)" % (name, v, i % w, i // w))
    for i, v in enumerate(L["biome"]):
        if v not in codes:
            fatal(PRE + "biome value %d at (%d, %d) has no definition" % (v, i % w, i // w))


def resources(w, h, L, t, seed, values=None):
    """R1 to R6 on a W x H grid; returns every intermediate."""
    values = biome_values() if values is None else values
    check_tuning(t)
    preconditions(w, h, L, values)
    e, water, depth, river, fd = L["elevation"], L["water"], L["water_depth"], L["river"], L["flow_dir"]
    rain, biome = L["rainfall"], L["biome"]
    n = w * h
    try:
        hand = br.hand_fifo(w, h, e, water, depth, river, fd, 1)
    except FatalError as ex:
        fatal(PRE + str(ex))
    s2 = br.slope_s2(w, h, e)
    src_f, src_g = sources(water, river, t)
    df = [metres(v) for v in chamfer(w, h, src_f)]
    dg = [metres(v) for v in chamfer(w, h, src_g)]
    key = cr.stream_key(seed, STREAM_ORE)
    noise = [ore_noise(seed, i % w, i // w, t, key) for i in range(n)]
    out = {name: [0] * n for name, _ in LAYERS}
    for i in range(n):
        if water[i] == WATER_LAKE:
            out["fresh_water"][i] = 255
        if water[i] not in (WATER_DRY, WATER_RIVER):
            continue
        fb, tb, gb = values[biome[i]]
        out["fresh_water"][i] = fresh_water_cell(df[i], rain[i], t)
        out["fertility"][i] = fertility_cell(fb, s2[i], hand[i], t)
        out["timber"][i] = timber_cell(tb, s2[i], t)
        out["forage"][i] = forage_cell(gb, dg[i], t)
        out["ore"][i] = ore_cell(e[i], s2[i], noise[i], t)
    S = [e[i] + depth[i] for i in range(n)]
    ids, k, dropped = walk_areas(w, h, S, water, s2, t["walk_slope_max"], min_cells_of(t))
    out["walk_area"] = ids
    for i in range(n):
        if water[i] not in (WATER_DRY, WATER_RIVER):
            continue
        out["habitability"][i] = habitability_cell(out["fertility"][i], out["fresh_water"][i], out["forage"][i],
                                                   out["timber"][i], out["ore"][i], s2[i], hand[i], ids[i], t)
    return {"w": w, "h": h, "L": L, "t": t, "hand": hand, "s2": s2, "df": df, "dg": dg, "noise": noise,
            "out": out, "walk_k": k, "walk_dropped": dropped, "S": S}


# ---------------------------------------------------------------------------
# Report (9) and palette
# ---------------------------------------------------------------------------

def ratio4(a, b):
    if b == 0:
        return "n/a"
    v = cr.rha(a * 10000, b)
    return "%d.%04d" % (v // 10000, v % 10000)


def percentile(values, p):
    if not values:
        return "n/a"
    s = sorted(values)
    return str(s[min(len(s) - 1, (p * len(s)) // 100)])


SCORE_NAMES = ("fertility", "ore", "timber", "fresh_water", "forage", "habitability")


def report_items(e, water, out):
    dry = [i for i in range(len(water)) if water[i] in (WATER_DRY, WATER_RIVER)]
    land = sum(1 for v in e if v > 0)
    items = []
    for p in (50, 90):
        items.append(("scores_p%d" % p, " ".join("%s:%s" % (nm, percentile([out[nm][i] for i in dry], p))
                                                 for nm in SCORE_NAMES)))
    items.append(("fertile_share", ratio4(sum(1 for i in dry if out["fertility"][i] >= 166), len(dry))))
    items.append(("habitable_share", ratio4(sum(1 for i in dry if out["habitability"][i] >= 64), len(dry))))
    items.append(("ore_rich_share", ratio4(sum(1 for i in dry if out["ore"][i] >= 128), len(dry))))
    wa = out["walk_area"]
    items.append(("walk_areas", str(max(wa) if wa else 0)))
    count = {}
    for v in wa:
        if v:
            count[v] = count.get(v, 0) + 1
    items.append(("walk_main_share", ratio4(max(count.values()), land) if count else "0.0000"))
    items.append(("walk_share", ratio4(sum(1 for v in wa if v > 0), land)))
    return items


SCORE_STOPS = [(0, (68, 1, 84)), (64, (59, 82, 139)), (128, (33, 145, 140)), (192, (94, 201, 98)),
               (255, (253, 231, 37))]


def ramp_colour(stops, t):
    if t < stops[0][0]:
        return stops[0][1]
    if t >= stops[-1][0]:
        return stops[-1][1]
    for k in range(len(stops) - 1):
        t0, c0 = stops[k]
        t1, c1 = stops[k + 1]
        if t0 <= t < t1:
            return tuple(c0[j] + (2 * (c1[j] - c0[j]) * (t - t0) + (t1 - t0)) // (2 * (t1 - t0)) for j in range(3))
    raise AssertionError


# ---------------------------------------------------------------------------
# Hashes
# ---------------------------------------------------------------------------

h_i64 = br.h_i64
layer_hash = br.layer_hash


# ---------------------------------------------------------------------------
# Inputs and golden cases
# ---------------------------------------------------------------------------

BIOMES_GOLDEN = os.path.join(_HERE, "..", "golden", "p1-04-biomes.txt")
_BG = None


def biomes_golden():
    global _BG
    if _BG is None:
        out = {}
        with open(BIOMES_GOLDEN) as f:
            for line in f:
                tok = line.split()
                if not tok or tok[0].startswith("#"):
                    continue
                if tok[0] == "case":
                    out[tok[1]] = {}
                else:
                    out[tok[0]][tok[1]] = tok[2]
        _BG = out
    return _BG


def check_inputs(base, w, h, L):
    """Every input layer against the biome golden case `base` (0.9.0)."""
    g = biomes_golden()[base]
    for name, code in INPUTS:
        if layer_hash(w, h, code, L[name]) != g[name]:
            fatal("input %s of %s differs from p1-04-biomes.txt (stale cache or world?)" % (name, base))


def inputs_of(base, fx, n, seed, db_dir=None):
    if db_dir:
        path = os.path.join(db_dir, base + ".db")
        if os.path.exists(path):
            raw = br.db_layers(path)
            L = {}
            for name, code in INPUTS:
                w, h, tcode, values = raw[name]
                if tcode != code or w != n or h != n:
                    fatal("%s: layer %s is not %d x %d of type %d" % (path, name, n, n, code))
                L[name] = values
            check_inputs(base, n, n, L)
            sys.stderr.write("inputs: %s decoded from %s and checked\n" % (base, path))
            return n, n, L
    w, h, Lb, _, _ = br.case_inputs(fx, n, seed)
    rb = br.biomes(w, h, Lb, br.SHIPPED, br.shipped_tuning(), seed)
    L = {name: Lb[name] for name, _ in INPUTS if name != "biome"}
    L["biome"] = rb["biome"]
    check_inputs(base, w, h, L)
    return w, h, L


W16K = ("walk_min_area_m2", "16384")

# (name, base case of p1-04-biomes.txt, tuning overrides, why)
GOLDEN_CASES = [
    ("designed-0", "designed-0", [W16K], "designed fixture 0, smallest walk area (64 cells)"),
    ("designed-1", "designed-1", [W16K], "designed fixture 1, smallest walk area"),
    ("designed-2", "designed-2", [W16K], "designed fixture 2, smallest walk area"),
    ("designed-3", "designed-3", [W16K], "designed fixture 3, smallest walk area"),
    ("designed-0-shipped", "designed-0", [], "shipped tuning: every component below 977 cells, all pockets"),
    ("designed-1-fish1", "designed-1", [W16K, ("fish_river_class_min", "1")], "every river is fish water"),
    ("designed-1-fish15", "designed-1", [W16K, ("fish_river_class_min", "15")],
     "no river is fish water: the sea and lakes only"),
    ("designed-2-reach", "designed-2", [W16K, ("fresh_reach_m", "16"), ("fish_reach_m", "16")],
     "both reaches at their lower bound: only the source cells score"),
    ("designed-2-rain", "designed-2", [W16K, ("fresh_rain_weight", "1"), ("fresh_rain_lo_mm", "400"),
                                       ("fresh_rain_hi_mm", "1000")], "rain term dominant over the fixture's rain range"),
    ("designed-0-alluvial", "designed-0", [W16K, ("alluvial_bonus", "4"), ("alluvial_hand_m", "100")],
     "largest alluvial bonus, reaching 100 m: fertility caps"),
    ("designed-0-noalluvial", "designed-0", [W16K, ("alluvial_bonus", "0"), ("alluvial_hand_m", "0.1")],
     "no alluvial bonus"),
    ("designed-3-ore", "designed-3", [W16K, ("ore_vein_lo", "-1"), ("ore_vein_hi", "1"), ("ore_lowland", "1"),
                                      ("ore_slope_base", "0")], "ore over the whole noise range, height ignored"),
    ("designed-3-ore2", "designed-3", [W16K, ("ore_noise", "64/2/1"), ("ore_elev_lo_m", "0"),
                                       ("ore_elev_hi_m", "3000"), ("ore_slope_lo", "0"), ("ore_slope_hi", "10")],
     "ore noise at its shortest wavelength (64 m, P1-01 4.1), widest ramps"),
    ("designed-1-slopes", "designed-1", [W16K, ("fert_slope_lo", "0"), ("fert_slope_hi", "10"),
                                         ("timber_slope_lo", "0"), ("timber_slope_hi", "0.01"),
                                         ("hab_slope_lo", "0.01"), ("hab_slope_hi", "10")],
     "slope ramps at their extremes"),
    ("designed-0-walk1", "designed-0", [W16K, ("walk_slope_max", "0.01")],
     "walk limit 0.01: almost nothing walkable"),
    ("designed-0-walk10", "designed-0", [W16K, ("walk_slope_max", "10")],
     "walk limit 10: every land cell walkable and linked"),
    ("designed-1-weights-f", "designed-1", [W16K, ("hab_weight_forage", "0"), ("hab_weight_timber", "0"),
                                            ("hab_weight_ore", "0")], "habitability from fertility alone"),
    ("designed-1-weights-o", "designed-1", [W16K, ("hab_weight_fertility", "0"), ("hab_weight_forage", "0"),
                                            ("hab_weight_timber", "0"), ("hab_weight_ore", "1000")],
     "habitability from ore alone"),
    ("designed-1-hab255", "designed-1", [W16K, ("hab_weight_forage", "0"), ("hab_weight_timber", "0"),
                                         ("hab_weight_ore", "0"), ("hab_water_min", "1"), ("hab_flood_hand_m", "0")],
     "fertility alone, water and flooding ignored: habitability reaches 255"),
    ("designed-2-water", "designed-2", [W16K, ("hab_water_min", "0"), ("hab_flood_hand_m", "100"),
                                        ("hab_flood_factor", "0"), ("hab_pocket_factor", "1")],
     "no water minimum, everything floods to 0, no pocket penalty"),
    ("designed-3-water1", "designed-3", [W16K, ("hab_water_min", "1"), ("hab_flood_hand_m", "0"),
                                         ("hab_pocket_factor", "0")],
     "water ignored, nothing floods, pockets 0"),
    ("terrace-0", "terrace-0", [W16K], "terrace fixture 0"),
    ("terrace-1", "terrace-1", [W16K], "terrace fixture 1"),
    ("shipped-48-s3", "shipped-48-s3", [], "small shipped world"),
    ("shipped-64-s0", "shipped-64-s0", [], "small shipped world"),
    ("shipped-128-s0", "shipped-128-s0", [], "P1-05 10.4"),
    ("shipped-128-s9", "shipped-128-s9", [], "shipped world"),
    ("shipped-128-s42", "shipped-128-s42", [], "shipped world"),
    ("small-128-s0", "small-128-s0", [], "small-map fixture"),
    ("small-128-s1", "small-128-s1", [], "small-map fixture"),
    ("small-128-s2", "small-128-s2", [], "small-map fixture"),
]
LARGE_CASES = [
    ("shipped-512-s0", "shipped-512-s0", [], "P1-05 10.4"),
    ("shipped-512-s42", "shipped-512-s42", [], "P1-05 10.4"),
    ("shipped-512-phrase", "shipped-512-phrase", [], "P1-05 10.4 (seed of the phrase \"The Unmapped\")"),
    ("shipped-1024-s1", "shipped-1024-s1", [], "P1-05 10.4"),
]


def base_params(base):
    """(fixture, N, seed) of a biome golden case, from its case line."""
    with open(BIOMES_GOLDEN) as f:
        for line in f:
            tok = line.split()
            if tok and tok[0] == "case" and tok[1] == base:
                return tok[2], int(tok[3]), int(tok[4])
    raise KeyError(base)


def coverage(r):
    """Counts of land cells reaching each branch (golden item `cover`)."""
    L, t, out = r["L"], r["t"], r["out"]
    water, river = L["water"], L["river"]
    n = r["w"] * r["h"]
    vals = biome_values()
    c = dict.fromkeys(("river", "lake", "fishriver", "dfzero", "rainwins", "alluvial", "fertcap", "forgcap",
                       "fertsteep", "timbsteep", "vein0", "lowland", "flood", "pocket", "pocketpos", "habsteep",
                       "unwalkable", "habcap"), 0)
    for i in range(n):
        if water[i] == WATER_LAKE:
            c["lake"] += 1
        if water[i] not in (WATER_DRY, WATER_RIVER):
            continue
        if water[i] == WATER_RIVER:
            c["river"] += 1
            if river[i] >= t["fish_river_class_min"]:
                c["fishriver"] += 1
        if r["df"][i] == 0:
            c["dfzero"] += 1
        fs, fr = fresh_parts(r["df"][i], L["rainfall"][i], t)
        if fr > fs:
            c["rainwins"] += 1
        fb, tb, gb = vals[L["biome"][i]]
        sf, al = fert_parts(fb, r["s2"][i], r["hand"][i], t)
        if al > ONE:
            c["alluvial"] += 1
        if er.mul(er.mul(fb, sf), al) > ONE:
            c["fertcap"] += 1
        if gb + fish_term(r["dg"][i], t) > ONE:
            c["forgcap"] += 1
        tan = tangent(r["s2"][i])
        if tan >= t["fert_slope_hi"]:
            c["fertsteep"] += 1
        if tan >= t["timber_slope_hi"]:
            c["timbsteep"] += 1
        if tan >= t["hab_slope_hi"]:
            c["habsteep"] += 1
        if r["noise"][i] <= t["ore_vein_lo"]:
            c["vein0"] += 1
        eu, _, _ = ore_parts(L["elevation"][i], r["s2"][i], r["noise"][i], t)
        if eu == t["ore_lowland"]:
            c["lowland"] += 1
        sc, b, wat, slope, flood, pocket = hab_parts(out["fertility"][i], out["fresh_water"][i], out["forage"][i],
                                                     out["timber"][i], out["ore"][i], r["s2"][i], r["hand"][i],
                                                     out["walk_area"][i], t)
        if flood:
            c["flood"] += 1
        if pocket:
            c["pocket"] += 1
            if er.mul(er.mul(b, wat), slope) > 0:
                c["pocketpos"] += 1
        if not not_steeper(r["s2"][i], t["walk_slope_max"]):
            c["unwalkable"] += 1
        if out["habitability"][i] == 255:
            c["habcap"] += 1
    return ",".join("%s:%d" % kv for kv in c.items())


def case_result(name, base, overrides, db_dir=None):
    fx, n, seed = base_params(base)
    w, h, L = inputs_of(base, fx, n, seed, db_dir)
    t = tuning_of(overrides)
    return seed, resources(w, h, L, t, seed)


def case_items(r):
    w, h, L, out = r["w"], r["h"], r["L"], r["out"]
    items = []
    for name, code in INPUTS:
        items.append((name, layer_hash(w, h, code, L[name])))
    items.append(("hand", h_i64(r["hand"])))
    items.append(("S2", h_i64(r["s2"])))
    items.append(("df", h_i64(r["df"])))
    items.append(("dg", h_i64(r["dg"])))
    items.append(("noise", h_i64(r["noise"])))
    for name, code in LAYERS:
        items.append((name, layer_hash(w, h, code, out[name])))
    items.append(("walk", "%d,%d" % (r["walk_k"], r["walk_dropped"])))
    items.append(("cover", coverage(r)))
    for k, v in report_items(L["elevation"], L["water"], out):
        items.append((k, v.replace(" ", "_")))
    return items


GOLDEN_HEADER = """\
# P1-05 resources golden vectors for generator 0.10.0 (spec second version,
# 2026-11-18), generated by worldgen/tests/reference/p1_resources_reference.py
# golden, an independent exact-integer reference written from
# docs/specs/p1-05-resources-habitability.md and P1-00 alone. The seven input
# layers of each case are those of the named case of p1-04-biomes.txt
# (generator 0.9.0) and are checked against its layer hashes before use
# (recomputed by the earlier references, or for the 512 and 1024 worlds
# decoded from 0.9.0 world files).
# Do not edit by hand; never regenerate to make a C++ test pass.
#
# Format: ASCII, LF, one value per line, tokens separated by one space.
#   case <name> <base> <fixture> <N> <seed> <overrides>
#       starts a case. <base> is the case of p1-04-biomes.txt whose inputs
#       and biome layer (shipped biome data and tuning, P1-04) are used;
#       <fixture>, <N>, <seed> repeat its case line; the stage's world seed
#       is <seed>. Biome values: P1-05 4.1 (steppe fertility 0.7). Tuning:
#       the shipped worldgen.resources of 4.2 with <overrides>, "-" or
#       comma-separated field=value (fx values as decimal text; ore_noise
#       as wavelength_m/octaves/gain).
#   <name> <item> <value>                 one value, items in step order
# Items, in step order (the test reports the first that differs):
#   elevation water water_depth flow_dir river rainfall biome
#                P0-04 8 layer hash of each input layer
#   hand         SHA-256 of G7 with k = 1 per cell, int64 LE, index order
#   S2           SHA-256 of G8's S2 per cell, int64 LE
#   df dg        SHA-256 of R3's df and dg per cell in metres (metres() of
#                the chamfer, 32767 without a source), int64 LE
#   noise        SHA-256 of fbm_m(K_ore, X, Y, ore_noise) raw on every cell
#                (water cells included), int64 LE
#   fertility ore timber fresh_water forage habitability walk_area
#                P0-04 8 layer hash of each output layer (u8; walk_area u16)
#   walk         K,D: kept walk areas and dropped components of R5
#   cover        land cells (water 0 or 1) reaching each branch: river,
#                lake (lake cells), fishriver, dfzero (df = 0), rainwins
#                (fr > fs), alluvial (al > ONE), fertcap and forgcap (the
#                min(ONE, .) cap binds), fertsteep timbsteep habsteep (tan
#                at or above the ramp's top), vein0 (noise <= ore_vein_lo),
#                lowland (eu = ore_lowland), flood, pocket (walk_area 0),
#                pocketpos (pocket cells with h > 0 before the pocket
#                factor), unwalkable (S2 steeper than walk_slope_max),
#                habcap (habitability 255)
#   scores_p50 scores_p90 fertile_share habitable_share ore_rich_share
#   walk_areas walk_main_share walk_share
#                the report values of P1-05 9, spaces written as "_"
#
"""


def golden_text(large=False, db_dir=None):
    out = [GOLDEN_HEADER]
    for name, base, overrides, why in GOLDEN_CASES + (LARGE_CASES if large else []):
        seed, r = case_result(name, base, overrides, db_dir if large else None)
        fx, n, _ = base_params(base)
        ov = ",".join("%s=%s" % kv for kv in overrides) if overrides else "-"
        out.append("# %s\n" % why)
        out.append("case %s %s %s %d %d %s\n" % (name, base, fx, n, seed, ov))
        for item, v in case_items(r):
            out.append("%s %s %s\n" % (name, item, v))
        sys.stderr.write("golden: %s done\n" % name)
    return "".join(out)


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------

class Checker:
    def __init__(self):
        self.passes = 0
        self.fails = 0

    def eq(self, label, got, want):
        if got == want:
            self.passes += 1
        else:
            self.fails += 1
            sys.stderr.write("FAIL %s:\n  got  %r\n  want %r\n" % (label, got, want))

    def ok(self, label, cond):
        self.eq(label, bool(cond), True)


def check_helpers(c):
    t = shipped_tuning()
    c.eq("ramp(0.3, 0.18, 0.47)", ramp(dec("0.3"), dec("0.18"), dec("0.47")), 1777227848)
    for s, want in ((HALF, 128), (dec("0.762"), 194), (dec("0.001"), 0), (dec("0.002"), 1), (ONE, 255),
                    (-ONE, 0), (2 * ONE, 255)):
        c.eq("q255 %d" % s, q255(s), want)
    for s2, raw in ((0, 0), (3318, 773122075), (9216, 1288490189), (22118, 1996102368), (50205, 3007345802),
                    (102400, ONE), (250000, 6710886400)):
        c.eq("tan %d" % s2, tangent(s2), raw)
    c.eq("walk_slope_max raw", t["walk_slope_max"], 3007336101)
    c.ok("S2 50204 walkable", not_steeper(50204, t["walk_slope_max"]))
    c.ok("S2 50205 not walkable", not not_steeper(50205, t["walk_slope_max"]))
    for d, diag, want in ((112, False, True), (113, False, False), (158, True, True), (159, True, False)):
        c.eq("linked %d %s" % (d, diag), linked_pair(1000 + d, 1000, diag, t["walk_slope_max"]), want)
        c.eq("linked -%d %s" % (d, diag), linked_pair(1000, 1000 + d, diag, t["walk_slope_max"]), want)
    c.eq("walk_min_cells shipped", min_cells_of(t), 977)
    c.eq("walk_min_cells 16384", (16384 + 255) // 256, 64)
    # Palette example of 9.
    c.eq("score t=100", ramp_colour(SCORE_STOPS, 100), (44, 117, 140))
    for tt, col in SCORE_STOPS:
        c.eq("score stop %d" % tt, ramp_colour(SCORE_STOPS, tt), col)


def check_chamfer_example(c):
    w, h = 9, 3
    src = [0] * (w * h)
    src[1 * w + 4] = 1
    d = chamfer(w, h, src)
    c.eq("chamfer row0", d[0:9], [22, 17, 12, 7, 5, 7, 12, 17, 22])
    c.eq("chamfer row1", d[9:18], [20, 15, 10, 5, 0, 5, 10, 15, 20])
    c.eq("chamfer row2", d[18:27], [22, 17, 12, 7, 5, 7, 12, 17, 22])
    m = [metres(v) for v in d]
    c.eq("metres row0", m[0:9], [70, 54, 38, 22, 16, 22, 38, 54, 70])
    c.eq("metres row1", m[9:18], [64, 48, 32, 16, 0, 16, 32, 48, 64])
    c.eq("metres INF", metres(INF), 32767)
    c.eq("dijkstra", chamfer_dijkstra(w, h, src), d)
    c.eq("no source", chamfer(w, h, [0] * (w * h)), [INF] * (w * h))


def check_r3(c):
    t = shipped_tuning()
    for df, rain, want in ((0, 800, 255), (16, 800, 245), (38, 800, 231), (64, 800, 214), (200, 800, 128),
                           (400, 800, 43), (399, 200, 1), (1000, 1500, 102), (1000, 900, 51), (1000, 2500, 102),
                           (1000, 300, 0)):
        c.eq("fresh (%d, %d)" % (df, rain), fresh_water_cell(df, rain, t), want)
    c.eq("fs 16", fresh_parts(16, 800, t)[0], 4123168604)
    c.eq("fs 200", fresh_parts(200, 800, t)[0], HALF)
    c.eq("fr 800", fresh_parts(400, 800, t)[1], 715827883)
    c.eq("fs 399", fresh_parts(399, 200, t)[0], 10737418)


def check_r4(c):
    t = shipped_tuning()
    F = dec
    for fb, s2, hand, want in ((F("1"), 9216, 20, 176), (F("1"), 0, 0, 255), (F("0.7"), 0, 0, 232),
                               (F("0.7"), 0, 60, 178), (F("0.7"), 22118, 100, 3), (F("0.45"), 0, 5, 146),
                               (F("1"), 30000, 0, 0), (F("0.3"), 3318, 49, 77)):
        c.eq("fert (%d, %d, %d)" % (fb, s2, hand), fertility_cell(fb, s2, hand, t), want)
    c.eq("sf al 1", fert_parts(ONE, 9216, 20, t), (2517739448, 5068061410))
    c.eq("al 0", fert_parts(ONE, 0, 0, t)[1], 5583457485)
    c.eq("al 60", fert_parts(ONE, 0, 60, t)[1], ONE)
    c.eq("sf 22118", fert_parts(ONE, 22118, 100, t)[0], 77697452)
    c.eq("sf al 3318 49", fert_parts(ONE, 3318, 49, t), (4294870875, 4320737100))
    for tb, s2, want in ((F("1"), 9216, 255), (F("1"), 50205, 255), (F("1"), 102400, 146), (F("0.15"), 0, 38),
                         (F("0.7"), 250000, 0)):
        c.eq("timber (%d, %d)" % (tb, s2), timber_cell(tb, s2, t), want)
    c.eq("st 50205", timber_factor(50205, t), 4293726303)
    c.eq("st 102400", timber_factor(102400, t), 2454267026)
    for gb, dg, want in ((F("0.8"), 0, 255), (F("0.5"), 0, 204), (F("0.3"), 16, 149), (F("0.3"), 150, 115),
                         (F("0.05"), 299, 13), (F("0.3"), 300, 77), (F("0.2"), 5000, 51)):
        c.eq("forage (%d, %d)" % (gb, dg), forage_cell(gb, dg, t), want)
    for dg, raw in ((0, 1288490189), (16, 1219770712), (150, 644245095), (299, 4294967), (300, 0)):
        c.eq("fish %d" % dg, fish_term(dg, t), raw)
    for e, s2, nz, want in ((8000, 102400, "0.2", 255), (8000, 102400, "0.1", 128), (500, 0, "0.3", 19),
                            (15000, 20000, "0.05", 38), (4500, 9216, "0.15", 48), (8000, 102400, "-0.04", 0)):
        c.eq("ore (%d, %d, %s)" % (e, s2, nz), ore_cell(e, s2, dec(nz), t), want)
    c.eq("vn 0.1", ore_parts(8000, 102400, dec("0.1"), t)[2], 2147483651)
    c.eq("eu 500", ore_parts(500, 0, dec("0.3"), t), (644245094, HALF, ONE))
    c.eq("su vn 15000", ore_parts(15000, 20000, dec("0.05"), t)[1:], (2582937308, 1073741825))
    c.eq("4500", ore_parts(4500, 9216, dec("0.15"), t), (HALF, HALF, 3221225471))
    c.eq("raw 0.1", dec("0.1"), 429496730)
    c.eq("raw -0.04", dec("-0.04"), -171798692)


R6_TABLE = [
    ((232, 255, 204, 255, 3), 0, 20, 1, 20695, 3485660713, ONE, ONE, False, False, 207),
    ((232, 255, 204, 255, 3), 0, 0, 1, 20695, 3485660713, ONE, ONE, True, False, 103),
    ((255, 255, 153, 38, 0), 0, 5, 1, 17655, 2973633240, ONE, ONE, True, False, 88),
    ((255, 255, 153, 38, 0), 0, 5, 0, 17655, 2973633240, ONE, ONE, True, True, 22),
    ((179, 102, 128, 255, 0), 3318, 300, 1, 16230, 2733620361, 2362232013, 4294870875, False, False, 89),
    ((255, 40, 90, 38, 0), 0, 300, 1, 16395, 2761411326, 1579032094, ONE, False, False, 60),
    ((170, 13, 89, 13, 0), 0, 300, 2, 11325, 1907470770, 1237961162, ONE, False, False, 33),
    ((200, 200, 200, 200, 200), 30000, 300, 1, 20000, 3368601801, 3600193175, 0, False, False, 0),
]


def check_r6(c):
    t = shipped_tuning()
    for (F, W, G, T, O), s2, hand, walk, sc, b, wat, sl, fl, po, want in R6_TABLE:
        lab = "R6 %r %d %d %d" % ((F, W, G, T, O), s2, hand, walk)
        c.eq(lab + " parts", hab_parts(F, W, G, T, O, s2, hand, walk, t), (sc, b, wat, sl, fl, po))
        c.eq(lab, habitability_cell(F, W, G, T, O, s2, hand, walk, t), want)
    # 11.1: grassland by a river 177, forest by a river 207, dry steppe about 30.
    c.eq("forest by a river", habitability_cell(232, 255, 204, 255, 3, 0, 20, 1, t), 207)


def grid_rows(rows):
    return [v for r in rows for v in r]


def check_r5(c):
    t = shipped_tuning()
    wr = t["walk_slope_max"]
    # (a)
    w = h = 5
    e = [20] * 25
    e[12] = 300
    water = [0] * 25
    s2 = br.slope_s2(w, h, e)
    for i in (7, 11, 13, 17):
        c.eq("R5a S2 %d" % i, s2[i], 78400)
    c.eq("R5a S2 centre", s2[12], 0)
    ids, k, _ = walk_areas(w, h, e, water, s2, wr, 2)
    want = [0 if i in (7, 11, 12, 13, 17) else 1 for i in range(25)]
    c.eq("R5a ids", ids, want)
    c.eq("R5a uf", walk_areas_unionfind(w, h, e, water, s2, wr, 2)[0], want)
    # (b)
    rows = [[20, 21, 22, 23, 400, 401, 402, 403, 404], [20, 21, 22, 23, 400, 401, 402, 403, 404],
            [21, 22, 23, 24, 401, 402, 403, 404, 405], [21, 22, 23, 24, 401, 402, 403, 404, 405],
            [22, 23, 24, 25, 402, 403, 404, 405, 406]]
    w, h = 9, 5
    e = grid_rows(rows)
    s2 = br.slope_s2(w, h, e)
    c.ok("R5b S2 cols 3, 4", all(s2[y * w + x] >= 142884 for y in range(h) for x in (3, 4)))
    c.eq("R5b min S2 cols 3, 4", min(s2[y * w + x] for y in range(h) for x in (3, 4)), 142884)
    ids, k, _ = walk_areas(w, h, e, [0] * 45, s2, wr, 2)
    want = [1 if x <= 2 else 0 if x <= 4 else 2 for y in range(h) for x in range(w)]
    c.eq("R5b ids", ids, want)
    # (c)
    for hh, want in ((112, [1, 1, 1, 1]), (113, [1, 1, 2, 2])):
        e = [1000, 1000, 1000 + hh, 1000 + hh]
        s2 = br.slope_s2(4, 1, e)
        c.eq("R5c S2 %d" % hh, s2, [0, hh * hh, hh * hh, 0])
        ids, k, _ = walk_areas(4, 1, e, [0] * 4, s2, wr, 1)
        c.eq("R5c %d" % hh, ids, want)
    # Diagonal limit: a 2 x 2 grid, the two cells of one diagonal at 1000,
    # the others steeper than 35 deg? Here: e = [1000, 9000, 9000, 1000 + d]
    # makes (0, 0) and (1, 1) linked only diagonally (the anti-diagonal
    # cells are not walkable). S2 at the edge doubles the one-sided step.
    for d, want in ((158, True), (159, False)):
        c.eq("diag %d" % d, linked_pair(1000 + d, 1000, True, wr), want)


def check_invariants(c, label, r):
    """Section 6 invariants 1 to 5 by independent code."""
    w, h, L, t, out = r["w"], r["h"], r["L"], r["t"], r["out"]
    n = w * h
    water = L["water"]
    vals = biome_values()
    # Second methods for the intermediates.
    hand2 = br.hand_recursive(w, h, L["elevation"], water, L["water_depth"], L["river"], L["flow_dir"], 1)
    c.eq(label + " hand recursive", hand2, r["hand"])
    src_f, src_g = sources(water, L["river"], t)
    c.eq(label + " df dijkstra", [metres(v) for v in chamfer_dijkstra(w, h, src_f)], r["df"])
    c.eq(label + " dg dijkstra", [metres(v) for v in chamfer_dijkstra(w, h, src_g)], r["dg"])
    uf, k = walk_areas_unionfind(w, h, r["S"], water, r["s2"], t["walk_slope_max"], min_cells_of(t))
    c.eq(label + " walk union-find", uf, out["walk_area"])
    bad = []
    for i in range(n):
        if water[i] == WATER_OCEAN and any(out[nm][i] for nm, _ in LAYERS):
            bad.append(("ocean", i))
        if water[i] == WATER_LAKE and (any(out[nm][i] for nm, _ in LAYERS if nm != "fresh_water") or
                                       out["fresh_water"][i] != 255):
            bad.append(("lake", i))
        if water[i] == WATER_RIVER and out["fresh_water"][i] != 255:
            bad.append(("river", i))
        if any(not 0 <= out[nm][i] <= 255 for nm in SCORE_NAMES):
            bad.append(("range", i))
        if water[i] in (WATER_DRY, WATER_RIVER):
            tan = tangent(r["s2"][i])
            fb, tb, gb = vals[L["biome"][i]]
            if tan >= t["hab_slope_hi"] and out["habitability"][i]:
                bad.append(("hab steep", i))
            if (tan >= t["fert_slope_hi"] or fb == 0) and out["fertility"][i]:
                bad.append(("fert", i))
            if (tan >= t["timber_slope_hi"] or tb == 0) and out["timber"][i]:
                bad.append(("timber", i))
            if r["noise"][i] <= t["ore_vein_lo"] and out["ore"][i]:
                bad.append(("ore", i))
            if out["forage"][i] < q255(gb):
                bad.append(("forage", i))
            # Invariant 4: R6 from the stored scores.
            hb = habitability_cell(out["fertility"][i], out["fresh_water"][i], out["forage"][i], out["timber"][i],
                                   out["ore"][i], r["s2"][i], r["hand"][i], out["walk_area"][i], t)
            if hb != out["habitability"][i]:
                bad.append(("R6", i))
            # Invariant 5 at this cell's arguments.
            for s2b in (r["s2"][i] + 1, r["s2"][i] * 2 + 7):
                if fertility_cell(fb, s2b, r["hand"][i], t) > out["fertility"][i]:
                    bad.append(("mono fert", i))
                if timber_cell(tb, s2b, t) > out["timber"][i]:
                    bad.append(("mono timber", i))
                if habitability_cell(out["fertility"][i], out["fresh_water"][i], out["forage"][i], out["timber"][i],
                                     out["ore"][i], s2b, r["hand"][i], out["walk_area"][i], t) > hb:
                    bad.append(("mono hab", i))
            if fresh_water_cell(r["df"][i] + 3, L["rainfall"][i], t) > out["fresh_water"][i]:
                bad.append(("mono fresh", i))
            if forage_cell(gb, r["dg"][i] + 3, t) > out["forage"][i]:
                bad.append(("mono forage", i))
            if out["walk_area"][i] and not not_steeper(r["s2"][i], t["walk_slope_max"]):
                bad.append(("walk steep", i))
        elif out["walk_area"][i]:
            bad.append(("walk water", i))
    c.eq(label + " invariant failures", bad[:5], [])
    # Invariant 3: sizes, ids 1..K each used, increasing smallest index;
    # linked cells share ids.
    ids = out["walk_area"]
    first = {}
    size = {}
    for i, v in enumerate(ids):
        if v:
            first.setdefault(v, i)
            size[v] = size.get(v, 0) + 1
    c.eq(label + " ids dense", sorted(first), list(range(1, len(first) + 1)))
    c.ok(label + " ids by smallest index", all(first[a] < first[a + 1] for a in range(1, len(first))))
    c.ok(label + " sizes", all(v >= min_cells_of(t) for v in size.values()))
    wk = walkable_mask(water, r["s2"], t["walk_slope_max"])
    split = 0
    for i in range(n):
        if not wk[i]:
            continue
        x, y = i % w, i // w
        for k in range(8):
            xx, yy = x + DX[k], y + DY[k]
            if 0 <= xx < w and 0 <= yy < h:
                m = yy * w + xx
                if wk[m] and linked_pair(r["S"][i], r["S"][m], k & 1, t["walk_slope_max"]) and ids[m] != ids[i]:
                    split += 1
    c.eq(label + " linked cells share ids", split, 0)


def check_designed(c):
    for name, base, overrides, _ in GOLDEN_CASES:
        if not base.startswith(("designed", "terrace")):
            continue
        _, r = case_result(name, base, overrides)
        check_invariants(c, name, r)


def check_worlds(c, upto):
    for name, base, overrides, _ in GOLDEN_CASES:
        fx, n, _ = base_params(base)
        if fx in ("shipped", "small") and n <= upto:
            _, r = case_result(name, base, overrides)
            check_invariants(c, name, r)


def check_preconditions(c):
    t = shipped_tuning()
    w, h = 3, 2
    base = {"elevation": [10] * 6, "water": [0] * 6, "water_depth": [0] * 6, "flow_dir": [4] * 6,
            "river": [0] * 6, "rainfall": [800] * 6, "biome": [4] * 6}
    base["water"][0] = base["water"][3] = 1
    base["river"][0] = base["river"][3] = 1

    def expect(label, L, msg):
        try:
            resources(w, h, L, t, 0)
        except FatalError as ex:
            c.eq(label, str(ex), msg)
            return
        c.ok(label + " is fatal", False)

    L = dict(base)
    del L["rainfall"]
    expect("missing rainfall", L, PRE + "missing layer 'rainfall'")
    L = dict(base, water=[0, 0, 4, 1, 4, 0])
    expect("water 4", L, PRE + "water value 4 at (2, 0)")
    L = dict(base, river=[1, 0, 0, 1, 16, 0])
    expect("river 16", L, PRE + "river value 16 at (1, 1)")
    L = dict(base, biome=[4, 4, 4, 4, 13, 0])
    expect("biome 13", L, PRE + "biome value 13 at (1, 1) has no definition")
    L = dict(base, flow_dir=[4, 0, 4, 4, 4, 4])
    try:
        resources(w, h, L, t, 0)
        c.ok("G7 is fatal", False)
    except FatalError as ex:
        c.ok("G7 prefix " + str(ex), str(ex).startswith(PRE + "G7: "))


def main(argv):
    if len(argv) >= 2 and argv[1] == "check":
        c = Checker()
        check_helpers(c)
        check_chamfer_example(c)
        check_r3(c)
        check_r4(c)
        check_r5(c)
        check_r6(c)
        check_preconditions(c)
        check_designed(c)
        if "--quick" not in argv:
            check_worlds(c, 128)
        sys.stderr.write("check: %d passed, %d failed\n" % (c.passes, c.fails))
        return 1 if c.fails else 0
    if len(argv) >= 2 and argv[1] == "golden":
        args = []
        db_dir = None
        it = iter(argv[2:])
        for a in it:
            if a == "--db-dir":
                db_dir = next(it)
            elif not a.startswith("--"):
                args.append(a)
        text = golden_text(large="--large" in argv, db_dir=db_dir)
        path = args[0] if args else os.path.join(_HERE, "..", "golden", "p1-05-resources.txt")
        if path == "-":
            sys.stdout.write(text)
        else:
            with open(path, "w", newline="\n") as f:
                f.write(text)
        return 0
    if len(argv) >= 3 and argv[1] == "case":
        db_dir = argv[4] if len(argv) >= 5 and argv[3] == "--db-dir" else None
        for name, base, overrides, _ in GOLDEN_CASES + LARGE_CASES:
            if name == argv[2]:
                _, r = case_result(name, base, overrides, db_dir)
                for item, v in case_items(r):
                    print(item, v)
                return 0
        return 2
    sys.stderr.write(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
