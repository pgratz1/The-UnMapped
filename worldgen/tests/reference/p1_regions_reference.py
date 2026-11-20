#!/usr/bin/env python3
"""Independent exact-integer reference of the P1-06 regions stage.

Written by the test writer from docs/specs/p1-06-regions.md (second
version, generator 0.11.0, schema 5: RG1 to RG14, the layer `region`, the
tables `region`, `region_border` and `region_biome`, the report keys and
the `ids` palette and `regions` composite of section 9, P0-04 amendment 18)
and docs/specs/p1-00-geography-overview.md (G1, G2, G6, G7 with k = 1,
D1 to D10, 10.2, 10.3) only, without reading any implementation of the
stage and without the spec writer's prototype. The input layers of a world
are those of the named case of p1-05-resources.txt (generator 0.10.0): for
the fixtures and the worlds up to N = 128 they are recomputed by
p1_resources_reference.py (itself on the earlier references), for the 512
and 1024 worlds decoded from a 0.10.0 world file; every input layer is
first checked against the layer hashes of p1-05-resources.txt, and table
`pass` against the `pass_rows` item of p1-01-elevation.txt, so no golden
value depends on an unchecked input.

Python 3 standard library only, arbitrary-precision integers, no floating
point.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p1_regions_reference.py check [--quick]
        Every worked example of P1-06 5.1 (RG3 weights, the five flood
        grids with every owner and cost, RG5 seeds with the bank exception,
        RG8 merge, RG11 sectors, numerals and names, RG12 border row and
        pass crossings), the composite example of P0-04 amendment 18 (e),
        the word grammar of P0-03 amendment 17, the preconditions of section
        2 with their messages, a second method for each step (a bucket
        queue for the flood, a recount merge, borders and crossings by a
        second enumeration) and invariants 1 to 6 of section 6 on the
        designed and terrace fixtures and (without --quick) on the golden
        worlds up to N = 128.

    python3 -B worldgen/tests/reference/p1_regions_reference.py golden [PATH|-] [--large] [--db-dir DIR]
        Write worldgen/tests/golden/p1-06-regions.txt ("-": standard
        output). --large adds 512 seeds 0, 42, the phrase and 1024 seed 1,
        whose inputs are decoded from DIR/<case>.db (a 0.10.0 world with
        the shipped data, checked against p1-05-resources.txt).

    python3 -B worldgen/tests/reference/p1_regions_reference.py case NAME [--db-dir DIR]
        Print the items of one golden case.

    python3 -B worldgen/tests/reference/p1_regions_reference.py world DB...
        Run the stage on the stored layers of 0.10.0 world files (any size)
        with the shipped tuning and print the report keys of section 9 and
        the sweep measures checked against section 9's statistics.
"""

import hashlib
import heapq
import os
import sqlite3
import struct
import sys
from collections import deque

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import core_reference as cr  # noqa: E402
import p1_elevation_reference as er  # noqa: E402
import p1_biomes_reference as br  # noqa: E402
import p1_resources_reference as rr  # noqa: E402

FatalError = cr.FatalError
ONE = 1 << 32
INF = 1 << 62

PRE = "regions: input precondition: "
STREAM_SEED = "geo.regions.seed"
STREAM_COST = "geo.regions.cost"

WATER_DRY, WATER_RIVER, WATER_LAKE, WATER_OCEAN = 0, 1, 2, 3
DX = (1, 1, 0, -1, -1, -1, 0, 1)
DY = (0, 1, 1, 1, 0, -1, -1, -1)

# Section 2, in the order of the table.
INPUTS = (("elevation", 4), ("water", 1), ("water_depth", 3), ("flow_dir", 1), ("river", 1), ("biome", 1),
          ("fertility", 1), ("ore", 1), ("timber", 1), ("fresh_water", 1), ("forage", 1), ("habitability", 1),
          ("walk_area", 3))
UPSTREAM = ("elevation", "water", "water_depth", "flow_dir", "river", "biome")
RESOURCE_LAYERS = ("fertility", "ore", "timber", "fresh_water", "forage", "habitability", "walk_area")

D160 = er.SQRT2 * 160  # mul_int(SQRT2, 160)
assert D160 == 971840160000


def fatal(msg):
    raise FatalError(msg)


def dec(text):
    v = cr.parse_decimal(text)
    if v is None:
        raise ValueError(text)
    return v


# ---------------------------------------------------------------------------
# Tuning (section 4)
# ---------------------------------------------------------------------------

NAME_FIELDS = ("central", "north", "northeast", "east", "southeast", "south", "southwest", "west", "northwest",
               "isle", "upland")
SHIPPED_NAMES = ("Central", "North", "Northeast", "East", "Southeast", "South", "Southwest", "West", "Northwest",
                 "Isle", "Fells")
SHIPPED_UPLAND_OWN = ("alpine", "heath", "highland_moor")

# (field, kind, lo, hi, shipped text): kind int | fx | fbm | names; bounds as text.
TUNING = [
    ("seed_tile_m", "int", "256", "32768", "864"),
    ("seed_spacing_m", "int", "160", "32768", "1200"),
    ("hand_ref_m", "int", "1", "10000", "25"),
    ("cost_noise", "fbm", None, None, "1500/3/0.5"),
    ("cost_noise_amp", "fx", "0", "0.9", "0.5"),
    ("lake_cost_factor", "fx", "1", "10", "2"),
    ("river_border_class", "int", "1", "16", "6"),
    ("river_cost_m", "int", "0", "100000", "2000"),
    ("region_min_area_m2", "int", "0", "100000000", "750000"),
    ("ore_rich_min", "int", "1", "255", "128"),
    ("ore_reach_m", "int", "0", "10000", "300"),
    ("name_central_frac", "fx", "0", "0.5", "0.12"),
    ("names", "names", None, None, None),
    ("upland_own_noun", "keys", None, None, None),  # r2: array(ref(biome), 0, 16)
]
KIND = {f: k for f, k, _, _, _ in TUNING}


def parse_value(kind, text):
    if kind == "int":
        return int(text)
    if kind == "fx":
        return dec(text)
    if kind == "fbm":
        a, b, c = text.split("/")
        return (int(a), int(b), dec(c))
    if kind == "keys":
        return () if text == "none" else tuple(text.split("/"))
    if kind == "names":
        words = text.split("/")
        if len(words) != len(NAME_FIELDS):
            raise ValueError(text)
        return tuple(words)
    raise KeyError(kind)


def shipped_tuning():
    t = {}
    for f, k, _, _, v in TUNING:
        t[f] = SHIPPED_NAMES if k == "names" else SHIPPED_UPLAND_OWN if k == "keys" else parse_value(k, v)
    return t


def tuning_of(overrides):
    """overrides: list of (field, text); fx as decimal text, cost_noise as
    wavelength/octaves/gain, names as the eleven words joined by '/',
    upland_own_noun as biome keys joined by '/' ("none": empty)."""
    t = shipped_tuning()
    for f, v in overrides:
        t[f] = parse_value(KIND[f], v)
    return t


def word_ok(s):
    """The word grammar of 4 (P0-03 amendment 17): [A-Z][a-z]*, 1 to 32
    bytes, ASCII only."""
    if not isinstance(s, str):
        return False
    try:
        b = s.encode("ascii")
    except UnicodeEncodeError:
        return False
    if not 1 <= len(b) <= 32:
        return False
    if not (0x41 <= b[0] <= 0x5A):
        return False
    return all(0x61 <= c <= 0x7A for c in b[1:])


def check_tuning(t):
    """Check 3 of section 2: ranges in declaration order, then the named
    check. (The seam's guard; the loader's own errors are not modelled.)"""
    for f, k, lo, hi, _ in TUNING:
        v = t[f]
        if k == "int":
            if not int(lo) <= v <= int(hi):
                fatal("regions: tuning: %s outside its schema range" % f)
        elif k == "fx":
            if not dec(lo) <= v <= dec(hi):
                fatal("regions: tuning: %s outside its schema range" % f)
        elif k == "fbm":
            # P1-01 4.1's fbm_m record: wavelength_m int(64, 65536),
            # octaves int(1, 10), gain fx(0.0625, 1).
            wl, oc, g = v
            if not (64 <= wl <= 65536 and 1 <= oc <= 10 and dec("0.0625") <= g <= ONE):
                fatal("regions: tuning: %s outside its schema range" % f)
        elif k == "keys":
            if len(v) > 16:
                fatal("regions: tuning: %s outside its schema range" % f)
        elif k == "names":
            if len(v) != 11 or not all(word_ok(x) for x in v):
                fatal("regions: tuning: %s outside its schema range" % f)
    if not t["seed_tile_m"] <= 2 * t["seed_spacing_m"]:
        fatal("regions: tuning: seed_tile_m must be at most twice seed_spacing_m")


def min_cells_of(t):
    return (t["region_min_area_m2"] + 255) // 256


# ---------------------------------------------------------------------------
# Biome definitions: (code, key, water, region_noun) in ascending code
# ---------------------------------------------------------------------------

def shipped_defs():
    return sorted((d["code"], k, bool(d["water"]), d["region_noun"]) for k, d in br.shipped_definitions().items())


def check_nouns(defs):
    for code, key, _, noun in sorted(defs):
        if not word_ok(noun):
            fatal("regions: biome '%s' has a region_noun that is not one word" % key)


# ---------------------------------------------------------------------------
# RG3: step weight
# ---------------------------------------------------------------------------

def step_weight(nz, hand, t):
    nf = ONE + er.mul(t["cost_noise_amp"], nz)
    hf = ONE + er.from_ratio(hand, 10 * t["hand_ref_m"])
    return er.mul(nf, hf)


def cost_noise(key, x, y, t):
    return er.fbm_m(key, er.from_int(16 * x + 8), er.from_int(16 * y + 8), t["cost_noise"])


# ---------------------------------------------------------------------------
# RG4, RG5: candidates and seeds
# ---------------------------------------------------------------------------

def site_key(i, w, hab, S, kseed):
    """key(c) for sorting best first: larger key better, then smaller index.
    Returned as a tuple that sorts ascending best first."""
    x, y = i % w, i // w
    hh = er.hash_at(kseed, x, y)
    return (-hab[i], S[i], -hh, i)


def candidates(w, h, water, hab, S, kseed, t):
    tile = t["seed_tile_m"] // 16
    out = []
    ty = 0
    while ty * tile < h:
        tx = 0
        while tx * tile < w:
            best = None
            for y in range(ty * tile, min(ty * tile + tile, h)):
                for x in range(tx * tile, min(tx * tile + tile, w)):
                    i = y * w + x
                    if water[i] != WATER_DRY:
                        continue
                    k = site_key(i, w, hab, S, kseed)
                    if best is None or k < best[0]:
                        best = (k, i)
            if best is not None:
                out.append(best[1])
            tx += 1
        ty += 1
    return out


def border_river_mask(water, river, t):
    k = t["river_border_class"]
    return [1 if (water[i] == WATER_RIVER and river[i] >= k) else 0 for i in range(len(water))]


def crosses(w, border, s, c):
    """cross(s, c) of RG5; s, c are (x, y)."""
    sx, sy = s
    dx, dy = c[0] - sx, c[1] - sy
    m = max(abs(dx), abs(dy))
    px, py = sx, sy
    for j in range(1, m + 1):
        qx, qy = sx + er.rha(dx * j, m), sy + er.rha(dy * j, m)
        if border[qy * w + qx]:
            return True
        if qx != px and qy != py and border[py * w + qx] and border[qy * w + px]:
            return True
        px, py = qx, qy
    return False


def select_seeds(w, sorted_cands, border, t, stats=None):
    """RG5 on candidates already sorted best first: the accepted cells in
    acceptance order."""
    sp2 = t["seed_spacing_m"] ** 2
    acc = []
    for c in sorted_cands:
        cx, cy = c % w, c // w
        ok = True
        for s in acc:
            sx, sy = s % w, s // w
            if (16 * (cx - sx)) ** 2 + (16 * (cy - sy)) ** 2 < sp2:
                if not crosses(w, border, (sx, sy), (cx, cy)):
                    ok = False
                    break
                if stats is not None:
                    stats["bank"] = stats.get("bank", 0) + 1
        if ok:
            acc.append(c)
        elif stats is not None:
            stats["rejected"] = stats.get("rejected", 0) + 1
    return acc


def landmass_seeds(w, h, water, landmass, nlm, seeds, hab, S, kseed):
    """RG6: one seed per landmass that holds none, appended in label order."""
    has = [False] * (nlm + 1)
    for s in seeds:
        has[landmass[s]] = True
    best = [None] * (nlm + 1)
    first = [None] * (nlm + 1)
    for i in range(w * h):
        lm = landmass[i]
        if lm == 0 or has[lm]:
            continue
        if first[lm] is None:
            first[lm] = i
        if water[i] == WATER_DRY:
            k = site_key(i, w, hab, S, kseed)
            if best[lm] is None or k < best[lm][0]:
                best[lm] = (k, i)
    out = list(seeds)
    for lm in range(1, nlm + 1):
        if has[lm]:
            continue
        out.append(best[lm][1] if best[lm] is not None else first[lm])
    return out


def landmasses(w, h, water):
    label, comps = er.g2_components(w, h, [1 if v != WATER_OCEAN else 0 for v in water], 8)
    return label, len(comps)


# ---------------------------------------------------------------------------
# RG7: the cost flood
# ---------------------------------------------------------------------------

def step_cost(w, c, m, d, wt, water, border, t):
    if d & 1:
        hz = er.round_int(er.mul(wt[m], D160))
    else:
        hz = er.round_int(wt[m] * 160)
    if water[m] == WATER_LAKE:
        hz = er.round_int(t["lake_cost_factor"] * hz)
    rv = 10 * t["river_cost_m"]
    cost = hz + (rv if border[m] else 0)
    if d & 1:
        cx, cy, mx, my = c % w, c // w, m % w, m // w
        if border[cy * w + mx] and border[my * w + cx]:
            cost += rv
    return cost


def flood(w, h, wt, water, border, seeds, t):
    """RG7 with a binary heap: (owner by seed number, cost); ocean owner 0,
    cost INF."""
    n = w * h
    best = [INF] * n
    owner = [0] * n
    done = bytearray(n)
    q = []
    seq = 0
    for k, s in enumerate(seeds):
        best[s] = 0
        owner[s] = k + 1
        q.append((0, seq, s))
        seq += 1
    heapq.heapify(q)
    pop, push = heapq.heappop, heapq.heappush
    while q:
        k, _, c = pop(q)
        if done[c]:
            continue
        done[c] = 1
        x, y = c % w, c // w
        for d in range(8):
            xx, yy = x + DX[d], y + DY[d]
            if not (0 <= xx < w and 0 <= yy < h):
                continue
            m = yy * w + xx
            if water[m] == WATER_OCEAN or done[m]:
                continue
            nk = k + step_cost(w, c, m, d, wt, water, border, t)
            if nk < best[m]:
                best[m] = nk
                owner[m] = owner[c]
                push(q, (nk, seq, m))
                seq += 1
    return owner, best


def flood_buckets(w, h, wt, water, border, seeds, t):
    """Second method for RG7: a bucket queue keyed by cost (FIFO within a
    bucket is ascending seq), with a sorted list of non-empty keys."""
    n = w * h
    best = [INF] * n
    owner = [0] * n
    done = bytearray(n)
    buckets = {}
    keys = []
    for k, s in enumerate(seeds):
        best[s] = 0
        owner[s] = k + 1
        buckets.setdefault(0, deque()).append(s)
    if buckets:
        heapq.heappush(keys, 0)
    while keys:
        k = keys[0]
        b = buckets[k]
        c = b.popleft()
        if not b:
            del buckets[k]
            heapq.heappop(keys)
        if done[c]:
            continue
        done[c] = 1
        x, y = c % w, c // w
        for d in range(8):
            xx, yy = x + DX[d], y + DY[d]
            if not (0 <= xx < w and 0 <= yy < h):
                continue
            m = yy * w + xx
            if water[m] == WATER_OCEAN or done[m]:
                continue
            nk = k + step_cost(w, c, m, d, wt, water, border, t)
            if nk < best[m]:
                best[m] = nk
                owner[m] = owner[c]
                if nk not in buckets:
                    buckets[nk] = deque()
                    heapq.heappush(keys, nk)
                buckets[nk].append(m)
    return owner, best


# ---------------------------------------------------------------------------
# RG8, RG9: merge and renumber
# ---------------------------------------------------------------------------

def pair_counts(w, h, owner, water, border):
    """{(i, j): [n, nr]} over border pairs, i < j (region numbers)."""
    cnt = {}
    for c in range(w * h):
        oc = owner[c]
        if oc == 0:
            continue
        x, y = c % w, c // w
        for m in ((c + 1) if x + 1 < w else -1, (c + w) if y + 1 < h else -1):
            if m < 0:
                continue
            om = owner[m]
            if om == 0 or om == oc:
                continue
            key = (oc, om) if oc < om else (om, oc)
            e = cnt.get(key)
            if e is None:
                e = cnt[key] = [0, 0]
            e[0] += 1
            if not border[c] and not border[m]:
                e[1] += 1
    return cnt


def merge_graph(cells, pairs, min_cells, log=None):
    """RG8 on the region graph: cells {number: count}, pairs {(i, j): [n,
    nr]} with i < j. Returns {number: absorbing number} for every number."""
    cells = dict(cells)
    adj = {r: {} for r in cells}
    for (i, j), (nn, nr) in pairs.items():
        e = [nn, nr]  # one list shared by both directions
        adj[i][j] = e
        adj[j][i] = e
    into = {r: r for r in cells}
    alive = set(cells)
    while True:
        small = [(cells[r], r) for r in alive if cells[r] < min_cells and adj[r]]
        if not small:
            break
        _, i = min(small)
        j = min(adj[i].items(), key=lambda kv: (-kv[1][1], -kv[1][0], kv[0]))[0]
        if log is not None:
            log.append((i, j))
        cells[j] += cells[i]
        cells[i] = 0
        for k, (nn, nr) in list(adj[i].items()):
            del adj[k][i]
            if k == j:
                continue
            e = adj[j].get(k)
            if e is None:
                e = [nn, nr]
                adj[j][k] = e
                adj[k][j] = e
            else:
                e[0] += nn
                e[1] += nr
        adj[i] = {}
        alive.discard(i)
        into[i] = j

    def root(r):
        while into[r] != r:
            r = into[r]
        return r
    return {r: root(r) for r in cells}, cells, adj


def merge(w, h, owner, water, border, count, min_cells, log=None):
    """RG8 on the grid: the merged owner array (absorbing numbers kept)."""
    cells = {r: 0 for r in range(1, count + 1)}
    for o in owner:
        if o:
            cells[o] += 1
    final, _, _ = merge_graph(cells, pair_counts(w, h, owner, water, border), min_cells, log)
    return [final[o] if o else 0 for o in owner]


def merge_recount(w, h, owner, water, border, count, min_cells):
    """Second method for RG8: recount cells and pairs from the grid after
    every merge (slow; fixtures only)."""
    cur = list(owner)
    while True:
        cells = {}
        for o in cur:
            if o:
                cells[o] = cells.get(o, 0) + 1
        cnt = pair_counts(w, h, cur, water, border)
        nb = {}
        for (i, j), (nn, nr) in cnt.items():
            nb.setdefault(i, []).append((j, nn, nr))
            nb.setdefault(j, []).append((i, nn, nr))
        small = sorted((cells[r], r) for r in cells if cells[r] < min_cells and r in nb)
        if not small:
            return cur
        i = small[0][1]
        j = sorted(nb[i], key=lambda e: (-e[2], -e[1], e[0]))[0][0]
        cur = [j if o == i else o for o in cur]


def renumber(owner, n):
    """RG9: ids by smallest cell index; returns (region layer, number -> id)."""
    ids = {}
    for i in range(n):
        o = owner[i]
        if o and o not in ids:
            ids[o] = len(ids) + 1
    if len(ids) > 65535:
        fatal("regions: more than 65535 regions")
    return [ids[o] if o else 0 for o in owner], ids


# ---------------------------------------------------------------------------
# RG10: region rows
# ---------------------------------------------------------------------------

def kth(values, k):
    return sorted(values)[k]


def region_rows_props(w, h, L, region, R, seeds_of, landmass, walk_dist, defs, t):
    """The columns of 3.2 except the name: list of dicts, ids 1 .. R."""
    water, river, e = L["water"], L["river"], L["elevation"]
    walk = L["walk_area"]
    water_code = {c: wat for c, _, wat, _ in defs}
    acc = [None] + [{"cells": 0, "lake": 0, "riv": 0, "rcmax": 0, "coast": 0, "walk": 0, "wa": {},
                     "xmin": w, "ymin": h, "xmax": -1, "ymax": -1, "dry": 0, "esum": 0, "emax": None,
                     "sums": [0] * 6, "hab": [], "rich": 0, "reach": 0, "biome": {}} for _ in range(R)]
    scores = ("fertility", "forage", "timber", "ore", "fresh_water", "habitability")
    for i in range(w * h):
        r = region[i]
        if r == 0:
            continue
        a = acc[r]
        x, y = i % w, i // w
        a["cells"] += 1
        a["xmin"] = min(a["xmin"], x)
        a["ymin"] = min(a["ymin"], y)
        a["xmax"] = max(a["xmax"], x)
        a["ymax"] = max(a["ymax"], y)
        wv = water[i]
        if wv == WATER_LAKE:
            a["lake"] += 1
        if wv == WATER_RIVER:
            a["riv"] += 1
            a["rcmax"] = max(a["rcmax"], river[i])
        coast = False
        for d in (0, 2, 4, 6):
            xx, yy = x + DX[d], y + DY[d]
            if 0 <= xx < w and 0 <= yy < h and water[yy * w + xx] == WATER_OCEAN:
                coast = True
        if coast:
            a["coast"] += 1
        if walk[i] > 0:
            a["walk"] += 1
            a["wa"][walk[i]] = a["wa"].get(walk[i], 0) + 1
        b = L["biome"][i]
        a["biome"][b] = a["biome"].get(b, 0) + 1
        if wv in (WATER_DRY, WATER_RIVER):
            a["dry"] += 1
            a["esum"] += e[i]
            a["emax"] = e[i] if a["emax"] is None else max(a["emax"], e[i])
            for k, nm in enumerate(scores):
                a["sums"][k] += L[nm][i]
            a["hab"].append(L["habitability"][i])
            if L["ore"][i] >= t["ore_rich_min"]:
                a["rich"] += 1
                if walk_dist[i] <= t["ore_reach_m"]:
                    a["reach"] += 1
    rows = []
    for r in range(1, R + 1):
        a = acc[r]
        s = seeds_of[r]
        wa = 0
        if a["wa"]:
            wa = min(a["wa"].items(), key=lambda kv: (-kv[1], kv[0]))[0]
        land = [(cnt, code) for code, cnt in a["biome"].items() if not water_code[code]]
        if not land:
            # 3.2 / RG10 (ruling 2026-11-18): a region with no dry cell takes
            # the most common code over all its cells.
            land = [(cnt, code) for code, cnt in a["biome"].items()]
        dom = min(land, key=lambda v: (-v[0], v[1]))[1]
        nd = a["dry"]
        # A region with no dry cell: every value over dry cells is 0.
        means = [er.rha(v, nd) if nd else 0 for v in a["sums"]]
        rows.append({"id": r, "biome": dom, "x_min": a["xmin"], "y_min": a["ymin"], "x_max": a["xmax"],
                     "y_max": a["ymax"], "seed_x": s % w, "seed_y": s // w, "landmass": landmass[s],
                     "cells": a["cells"], "lake_cells": a["lake"], "river_cells": a["riv"],
                     "river_class_max": a["rcmax"], "coast_cells": a["coast"], "walk_cells": a["walk"],
                     "walk_area": wa, "elev_mean_dm": er.rha(a["esum"], nd) if nd else 0,
                     "elev_max_dm": a["emax"] if nd else 0,
                     "fertility_mean": means[0], "forage_mean": means[1], "timber_mean": means[2],
                     "ore_mean": means[3], "fresh_water_mean": means[4], "habitability_mean": means[5],
                     "habitability_p90": kth(a["hab"], min(nd - 1, (90 * nd) // 100)) if nd else 0,
                     "ore_rich_cells": a["rich"], "ore_reach_cells": a["reach"],
                     "biomes": sorted(a["biome"].items())})
    return rows


REGION_COLUMNS = ("id", "name", "biome", "x_min", "y_min", "x_max", "y_max", "seed_x", "seed_y", "landmass",
                  "cells", "lake_cells", "river_cells", "river_class_max", "coast_cells", "walk_cells", "walk_area",
                  "elev_mean_dm", "elev_max_dm", "fertility_mean", "forage_mean", "timber_mean", "ore_mean",
                  "fresh_water_mean", "habitability_mean", "habitability_p90", "ore_rich_cells", "ore_reach_cells")
BORDER_COLUMNS = ("a", "b", "length_m", "low_dm", "low_x", "low_y", "walk_pairs", "river_pairs", "river_class_max",
                  "pass_id", "passes")


# ---------------------------------------------------------------------------
# RG11: names
# ---------------------------------------------------------------------------

ROMAN = ((1000, "M"), (900, "CM"), (500, "D"), (400, "CD"), (100, "C"), (90, "XC"), (50, "L"), (40, "XL"),
         (10, "X"), (9, "IX"), (5, "V"), (4, "IV"), (1, "I"))


def roman(k):
    if k < 1:
        fatal("roman: k below 1")
    out = []
    for v, s in ROMAN:
        while k >= v:
            out.append(s)
            k -= v
    return "".join(out)


SECTORS = ("central", "north", "northeast", "east", "southeast", "south", "southwest", "west", "northwest")


def sector(dx, dy, rc):
    """Index into NAME_FIELDS (0 central .. 8 northwest)."""
    if dx * dx + dy * dy < rc * rc:
        return 0
    ax, ay = abs(dx), abs(dy)
    if 29 * ay <= 12 * ax:
        return 3 if dx > 0 else 7
    if 29 * ax <= 12 * ay:
        return 5 if dy > 0 else 1
    if dy > 0:
        return 4 if dx > 0 else 6
    return 2 if dx > 0 else 8


def centroid(w, h, water):
    n = 0
    sx = sy = 0
    for i in range(w * h):
        if water[i] != WATER_OCEAN:
            n += 1
            sx += i % w
            sy += i // w
    if n == 0:
        return None
    return er.rha(sx, n), er.rha(sy, n)


def name_list(entries, cx, cy, rc, words, noun, mere_noun, own, stats=None):
    """RG11 steps 2 to 4 for entries (seed_x, seed_y, biome code, isle,
    mere, upland) in id order; own: the codes of upland_own_noun. Noun rules
    (r2) in order: a isle, b mere (2 lake_cells >= cells), c upland unless
    the dominant biome is in upland_own_noun, d the dominant biome's noun."""
    seen = {}
    out = []
    for sx, sy, biome, isle, mere, upland in entries:
        sec = sector(sx - cx, sy - cy, rc)
        if isle:
            nn, rule = words[9], "isle"
        elif mere:
            nn, rule = mere_noun, "mere"
        elif upland and biome not in own:
            nn, rule = words[10], "fells"
        else:
            nn, rule = noun[biome], None
        base = words[sec] + " " + nn
        k = seen.get(base, 0) + 1
        seen[base] = k
        out.append(base if k == 1 else base + " " + roman(k))
        if stats is not None:
            for flag, key in ((rule == "isle", "isle"), (rule == "mere", "mere"), (rule == "fells", "fells"),
                              (sec == 0, "central"), (k >= 2, "numbered")):
                if flag:
                    stats[key] = stats.get(key, 0) + 1
    return out


def lake_noun(defs):
    """The region_noun of the definition with water_body "lake"."""
    for code, key, _, nn in defs:
        if br.shipped_definitions().get(key, {}).get("water_body") == "lake":
            return nn
    fatal("regions: no lake definition")


def base_entries(rows, landmass_cells):
    """(seed_x, seed_y, biome, isle, mere, upland) per row, id order."""
    per_lm = {}
    for r in rows:
        per_lm[r["landmass"]] = per_lm.get(r["landmass"], 0) + 1
    largest = min(landmass_cells.items(), key=lambda kv: (-kv[1], kv[0]))[0]
    return [(r["seed_x"], r["seed_y"], r["biome"], per_lm[r["landmass"]] == 1 and r["landmass"] != largest,
             2 * r["lake_cells"] >= r["cells"], 4 * r["walk_cells"] < r["cells"]) for r in rows]


def base_and_land(entries, cx, cy, rc, words, defs, own, mere_noun, pn):
    """RG11 1-4 (r3): (base name, land definition id) per entry, no numeral.
    pn: the placename data (placename_data)."""
    noun = {c: nn for c, _, _, nn in defs}
    key_of = {c: k for c, k, _, _ in defs}
    water_of = {c: wat for c, _, wat, _ in defs}
    out = []
    for sx, sy, biome, isle, mere, upland in entries:
        sec = sector(sx - cx, sy - cy, rc)
        if isle:
            nn, land = words[9], pn["special"]["isle"]
        elif mere:
            nn, land = mere_noun, pn["special"]["lake"]
        elif upland and biome not in own:
            nn, land = words[10], pn["special"]["upland"]
        else:
            nn = noun[biome]
            land = pn["special"]["lake"] if water_of[biome] else pn["by_biome"].get(key_of[biome])
            if land is None:
                fatal("regions: placename: biome '%s' has no land definition" % key_of[biome])
        out.append((words[sec] + " " + nn, land))
    return out


def names(rows, w, h, water, landmass_cells, t, defs, stats=None, brows=None, seed=0, pn=None):
    """RG11 for the rows in id order (each with seed_x, seed_y, landmass,
    biome, cells, lake_cells, walk_cells and the trait columns); N is the
    grid width. r3: brows (border rows with `passes`) and the world seed are
    needed for the draws. Returns the names; stats gains the counts and
    the per-region attempt record (`name_how`)."""
    code_of = {key: c for c, key, _, _ in defs}
    own = set(code_of[k] for k in t["upland_own_noun"])
    cx, cy = centroid(w, h, water)
    rc = er.round_int(t["name_central_frac"] * w)
    entries = base_entries(rows, landmass_cells)
    pn = placename_data() if pn is None else pn
    bl = base_and_land(entries, cx, cy, rc, t["names"], defs, own, lake_noun(defs), pn)
    if stats is not None:
        # The cover counts of r2 (numbered names of r2 = drawn names of r3:
        # the same regions, every non-first region of a base name).
        seen = {}
        for (sx, sy, biome, isle, mere, upland), (base, _) in zip(entries, bl):
            sec = sector(sx - cx, sy - cy, rc)
            k = seen.get(base, 0) + 1
            seen[base] = k
            rule = "isle" if isle else "mere" if mere else "fells" if (upland and biome not in own) else None
            for flag, key in ((rule == "isle", "isle"), (rule == "mere", "mere"), (rule == "fells", "fells"),
                              (sec == 0, "central"), (k >= 2, "numbered")):
                if flag:
                    stats[key] = stats.get(key, 0) + 1
    passed = set()
    for b in (brows or []):
        if b["passes"] > 0:
            passed.add(b["a"])
            passed.add(b["b"])
    out, how = rg11_r3(rows, bl, passed, seed, w, pn, t)
    if stats is not None:
        stats["name_how"] = how
    return out


# ---------------------------------------------------------------------------
# RG11 r3: drawn names (data kind `placename`, 4.1; tuning name_pools and
# name_traits, 4)
# ---------------------------------------------------------------------------

STREAM_NAME = "geo.regions.name"
PN_LISTS = ("nouns", "suffixes", "prefixes", "adjectives", "features", "possessors", "places", "objects")
SLOT_LIST = {"noun": "nouns", "suffix": "suffixes", "prefix": "prefixes", "adjective": "adjectives",
             "feature": "features", "possessor": "possessors", "place": "places", "object": "objects"}
TRAITS = ("coast", "river", "lake", "high", "ore", "pass", "barren", "fertile")  # trait order (RG11 5)
SPECIALS = ("none", "isle", "lake", "upland")
ROLES = ("common", "land", "trait")
# P1-06 4 (r3): the shipped block; stands in while game/data/worldgen/regions.toml
# lacks the two fields (the engineer adds them with the implementation).
SHIPPED_POOLS = {"nouns": (0, 1, 0), "suffixes": (0, 4, 1), "prefixes": (6, 3, 2), "adjectives": (6, 3, 2),
                 "features": (5, 1, 2), "possessors": (8, 1, 1), "places": (8, 1, 1), "objects": (6, 3, 2)}
SHIPPED_TRAITS = {"coast_cells": 128, "river_class": 7, "lake_permille": 125, "high_mean_m": 400,
                  "ore_rich_cells": 1000, "barren_hab_max": 20, "fertile_min": 200}
TRAIT_RANGES = (("coast_cells", 0, 100000000), ("river_class", 1, 16), ("lake_permille", 1, 1000),
                ("high_mean_m", 0, 10000), ("ore_rich_cells", 0, 100000000), ("barren_hab_max", -1, 255),
                ("fertile_min", 0, 256))
PLACENAMES_DIR = os.path.join(_HERE, "..", "..", "..", "game", "data", "placenames")
FIELD_ORDER = ("role", "biomes", "special", "trait", "forms") + PN_LISTS + ("forbidden", "excluded")


def k_name(seed, n):
    """K_name = substream(stream_key(seed, "geo.regions.name"), N) (P1-06 2, r3)."""
    return cr.substream(cr.stream_key(seed, STREAM_NAME), n)


def pick(u, n):
    """P3-02 4.1."""
    return ((u >> 32) * n) >> 32


def weighted(u, w):
    """P3-02 4.1: the smallest i with (u >> 32) W < 2^32 (w0 + ... + wi)."""
    total = sum(w)
    if total <= 0:
        fatal("regions: weighted: weights sum to 0")
    hi = (u >> 32) * total
    acc = 0
    for i, x in enumerate(w):
        acc += x
        if hi < (acc << 32):
            return i
    raise AssertionError("weighted")


def read_placename_files(directory=None):
    """{id: table} of every *.toml of a placenames directory (tomllib)."""
    import tomllib
    directory = PLACENAMES_DIR if directory is None else directory
    defs = {}
    for name in sorted(os.listdir(directory), key=lambda s: s.encode()):
        if not name.endswith(".toml"):
            continue
        with open(os.path.join(directory, name), "rb") as f:
            doc = tomllib.load(f)
        for did, table in doc.get("placename", {}).items():
            if did in defs:
                fatal("placename: duplicate id '%s'" % did)
            defs[did] = table
    return defs


def placename_data(defs=None):
    """The definitions with the lookups RG11 needs: common, lands by biome
    and special, traits by trait (each a definition dict with 'id')."""
    defs = read_placename_files() if defs is None else defs
    out = {"defs": defs, "by_biome": {}, "special": {}, "trait": {}, "common": None}
    for did in sorted(defs, key=lambda s: s.encode()):
        d = dict(defs[did])
        d["id"] = did
        if d["role"] == "common":
            out["common"] = d
        elif d["role"] == "land":
            for b in d["biomes"]:
                out["by_biome"][b] = d
            if d["special"] != "none":
                out["special"][d["special"]] = d
        elif d["role"] == "trait":
            out["trait"][d["trait"]] = d
    return out


# --- 4.1 grammars ---------------------------------------------------------

def g_word(s, n):
    return word_ok(s) and len(s) <= n


def g_element(s, n):
    return isinstance(s, str) and 1 <= len(s) <= n and all("a" <= ch <= "z" for ch in s)


def g_object(s, n):
    if not isinstance(s, str) or len(s) > n:
        return False
    return word_ok(s[4:]) if s.startswith("the ") else word_ok(s)


def _spaces_ok(s):
    return s and s[0] != " " and s[-1] != " " and "  " not in s


def g_place_name(s, n):
    if not isinstance(s, str) or not 1 <= len(s) <= n or not _spaces_ok(s):
        return False
    if not (("A" <= s[0] <= "Z") or ("a" <= s[0] <= "z")):
        return False
    return all(("A" <= ch <= "Z") or ("a" <= ch <= "z") or ch in " '" for ch in s)


def parse_form(form):
    """[("lit", text) | ("slot", name)], or None if not of the place_form
    grammar (literals: letters, space, '; slots of SLOT_LIST; at least one
    slot, none twice; no other braces; no leading, trailing, double space)."""
    if not isinstance(form, str) or not _spaces_ok(form):
        return None
    toks = []
    i = 0
    lit = ""
    seen = set()
    while i < len(form):
        ch = form[i]
        if ch == "{":
            j = form.find("}", i)
            if j < 0:
                return None
            s = form[i + 1:j]
            if s not in SLOT_LIST or s in seen:
                return None
            seen.add(s)
            if lit:
                toks.append(("lit", lit))
                lit = ""
            toks.append(("slot", s))
            i = j + 1
            continue
        if ch == "}":
            return None
        if not (("A" <= ch <= "Z") or ("a" <= ch <= "z") or ch in " '"):
            return None
        lit += ch
        i += 1
    if lit:
        toks.append(("lit", lit))
    if not seen:
        return None
    return toks


def g_form(s, n):
    return isinstance(s, str) and len(s) <= n and parse_form(s) is not None


LIST_GRAMMAR = {"nouns": (g_word, 24, "word"), "suffixes": (g_element, 24, "element"),
                "prefixes": (g_word, 24, "word"), "adjectives": (g_word, 24, "word"),
                "features": (g_element, 24, "element"), "possessors": (g_word, 24, "word"),
                "places": (g_word, 24, "word"), "objects": (g_object, 32, "name_object"),
                "forbidden": (g_element, 16, "element"), "excluded": (g_place_name, 48, "place_name")}
LIST_MAX = dict((f, 1024) for f in PN_LISTS)
LIST_MAX.update(forbidden=64, excluded=1024)


def placename_problem(defs, biome_keys):
    """The first problem of the loader's checks of 4.1 (types and grammars,
    then the named checks per definition in id order, then the kind checks),
    as (check, id or None, message); None when the data passes."""
    ids = sorted(defs, key=lambda s: s.encode())
    for did in ids:
        d = defs[did]
        for k in d:
            if k not in FIELD_ORDER:
                return ("schema", did, "unknown key '%s'" % k)
        for f in FIELD_ORDER:
            if f not in d:
                return ("schema", did, "missing required field '%s'" % f)
        if d["role"] not in ROLES:
            return ("schema", did, "role")
        if d["special"] not in SPECIALS:
            return ("schema", did, "special")
        if d["trait"] not in ("none",) + TRAITS:
            return ("schema", did, "trait")
        if len(d["biomes"]) > 16 or any(b not in biome_keys for b in d["biomes"]):
            return ("schema", did, "biomes")
        if len(d["forms"]) > 16:
            return ("schema", did, "forms")
        for i, fm in enumerate(d["forms"]):
            if not isinstance(fm, dict) or set(fm) != {"form", "weight"}:
                return ("schema", did, "forms[%d]" % i)
            if not g_form(fm["form"], 48):
                return ("grammar", did, "forms[%d].form" % i)
            if not isinstance(fm["weight"], int) or not 1 <= fm["weight"] <= 1000:
                return ("schema", did, "forms[%d].weight" % i)
        for f in PN_LISTS + ("forbidden", "excluded"):
            g, n, gname = LIST_GRAMMAR[f]
            if len(d[f]) > LIST_MAX[f]:
                return ("schema", did, "%s too long" % f)
            for j, s in enumerate(d[f]):
                if not g(s, n):
                    return ("grammar", did, "%s[%d] '%s' is not a %s" % (f, j, s, gname))
    for did in ids:
        d = defs[did]
        msg = role_fields_problem(d)
        if msg:
            return ("placename_role_fields", did, msg)
        for f in ("biomes",) + PN_LISTS + ("forbidden", "excluded", "forms"):
            vals = [x["form"] for x in d[f]] if f == "forms" else d[f]
            first = {}
            for j, v in enumerate(vals):
                if v in first:
                    return ("placename_lists_unique", did, "%s[%d] '%s' repeats %s[%d]" % (f, j, v, f, first[v]))
                first[v] = j
    commons = [i for i in ids if defs[i]["role"] == "common"]
    if len(commons) != 1:
        return ("placename_one_common", None, "there must be exactly one common definition, found %d" % len(commons))
    seen_b, seen_s, seen_t = {}, {}, {}
    for did in ids:
        d = defs[did]
        if d["role"] == "land":
            for b in d["biomes"]:
                if b in seen_b:
                    return ("placename_lands_distinct", None, "biome '%s' is in '%s' and '%s'" % (b, seen_b[b], did))
                seen_b[b] = did
        if d["special"] != "none":
            if d["special"] in seen_s:
                return ("placename_lands_distinct", None,
                        "special '%s' is in '%s' and '%s'" % (d["special"], seen_s[d["special"]], did))
            seen_s[d["special"]] = did
        if d["trait"] != "none":
            if d["trait"] in seen_t:
                return ("placename_lands_distinct", None,
                        "trait '%s' is in '%s' and '%s'" % (d["trait"], seen_t[d["trait"]], did))
            seen_t[d["trait"]] = did
    c = defs[commons[0]]
    for f, n in (("prefixes", 100), ("adjectives", 100), ("features", 16), ("possessors", 32), ("places", 16),
                 ("objects", 32)):
        if len(c[f]) < n:
            return ("placename_common_sizes", None,
                    "the common definition needs at least %d %s, has %d" % (n, f, len(c[f])))
    return None


def role_fields_problem(d):
    """placename_role_fields: the first failing condition in the order written."""
    role = d["role"]
    def need(f, state):
        v = d[f]
        ok = {"empty": v == [] if isinstance(v, list) else None, "none": v == "none",
              "non-empty": isinstance(v, list) and len(v) > 0, "not none": v != "none"}[state]
        return None if ok else "a %s definition must have %s %s" % (role, f, state)
    if role == "common":
        for f, s in (("biomes", "empty"), ("special", "none"), ("trait", "none"), ("forms", "empty"),
                     ("nouns", "empty")):
            m = need(f, s)
            if m:
                return m
    elif role == "land":
        m = need("trait", "none")
        if m:
            return m
        hb, hs = len(d["biomes"]) > 0, d["special"] != "none"
        if hb and hs:
            return "a land definition must name biomes or a special, not both"
        if not hb and not hs:
            return "a land definition must name biomes or a special, not neither"
        for f, s in (("forms", "non-empty"), ("nouns", "non-empty"), ("suffixes", "non-empty"),
                     ("forbidden", "empty"), ("excluded", "empty")):
            m = need(f, s)
            if m:
                return m
    else:
        for f, s in (("trait", "not none"), ("biomes", "empty"), ("special", "none"), ("forms", "empty"),
                     ("nouns", "empty"), ("forbidden", "empty"), ("excluded", "empty")):
            m = need(f, s)
            if m:
                return m
    return None


def check_4a(pn, defs_biomes, pools):
    """Stage check 4a (r3), messages with the prefix 'regions: placename: '.
    defs_biomes: (code, key, water, noun) ascending code."""
    P = "regions: placename: "
    lands = [d for d in (dict(v, id=k) for k, v in pn["defs"].items()) if d["role"] == "land"]
    for code, key, water, _ in sorted(defs_biomes):
        if water:
            continue
        if sum(1 for d in lands if key in d["biomes"]) != 1:
            fatal(P + "biome '%s' has no land definition" % key)
    for s in ("isle", "lake", "upland"):
        if not any(d["special"] == s for d in lands):
            fatal(P + "no land definition for '%s'" % s)
    common = pn["common"]
    for d in sorted(lands, key=lambda x: x["id"].encode()):
        for i, fm in enumerate(d["forms"]):
            for kind, s in parse_form(fm["form"]):
                if kind != "slot":
                    continue
                L = SLOT_LIST[s]
                w0, w1, _ = pools[L]
                if not ((w0 > 0 and common[L]) or (w1 > 0 and d[L])):
                    fatal(P + "'%s' form %d slot '%s' has no words" % (d["id"], i, s))


# --- RG11 5 to 8 ------------------------------------------------------------

def region_traits(row, has_pass, nt):
    """The traits a region holds, in trait order (RG11 5)."""
    held = {
        "coast": row["coast_cells"] >= nt["coast_cells"],
        "river": row["river_class_max"] >= nt["river_class"],
        "lake": row["lake_cells"] > 0 and 1000 * row["lake_cells"] >= nt["lake_permille"] * row["cells"],
        "high": row["elev_mean_dm"] >= 10 * nt["high_mean_m"],
        "ore": row["ore_rich_cells"] >= nt["ore_rich_cells"],
        "pass": bool(has_pass),
        "barren": row["habitability_mean"] <= nt["barren_hab_max"],
        "fertile": row["fertility_mean"] >= nt["fertile_min"],
    }
    return [tr for tr in TRAITS if held[tr]]


VOWELS = "aeiou"


def name_valid(toks, values, common):
    """V1 to V6 of RG11 7 for a form (parsed) and its slot values (in slot
    order); returns (valid, rule failed or None)."""
    vals = iter(values)
    parts = []
    for kind, s in toks:
        parts.append((kind, s if kind == "lit" else next(vals)))
    name = "".join(v for _, v in parts)
    for (k1, x), (k2, y) in zip(parts, parts[1:]):
        if k1 == "slot" and k2 == "slot":
            x, y = x.lower(), y.lower()
            if x[-1] == y[0] or (x[-1] in VOWELS and y[0] in VOWELS) or len(x) + len(y) > 12:
                return False, "V1"
    low = name.lower()
    for i in range(len(low) - 2):
        if low[i] == low[i + 1] == low[i + 2] and "a" <= low[i] <= "z":
            return False, "V2"
    sv = [v.lower() for k, v in parts if k == "slot"]
    for i in range(len(sv)):
        for j in range(len(sv)):
            if i != j and sv[i] in sv[j]:
                return False, "V3"
    for wd in low.split(" "):
        if any(f in wd for f in common["forbidden"]):
            return False, "V4"
    if name in common["excluded"]:
        return False, "V5"
    one = set(e for e in common["excluded"] if " " not in e)
    if any(wd in one for wd in name.split(" ")):
        return False, "V5"
    if len(name.encode()) > 48:
        return False, "V6"
    return True, None


def pools_for(slot_list, land, common, trait_defs, pw):
    P = (common[slot_list], land[slot_list], [x for td in trait_defs for x in td[slot_list]])
    w = [pw[slot_list][i] if len(P[i]) > 0 else 0 for i in range(3)]
    return P, w


def name_attempt(key, rid, t, land, common, trait_defs, pw, trace=None):
    """One attempt t of RG11 6 and 7: (valid, name, rule). trace: a list to
    receive (k, u, choice) records."""
    def u(k):
        v = cr.hash_at3(key, rid, t, k)
        return v
    F = land["forms"]
    u0 = u(0)
    fi = weighted(u0, [f["weight"] for f in F])
    form = F[fi]["form"]
    if trace is not None:
        trace.append(("form", u0, fi, form))
    toks = parse_form(form)
    k = 1
    values = []
    for kind, s in toks:
        if kind != "slot":
            continue
        P, w = pools_for(SLOT_LIST[s], land, common, trait_defs, pw)
        ua, ub = u(k), u(k + 1)
        p = weighted(ua, w)
        idx = pick(ub, len(P[p]))
        values.append(P[p][idx])
        if trace is not None:
            trace.append((s, ua, tuple(w), p, len(P[p]), ub, idx, P[p][idx]))
        k += 2
    ok, rule = name_valid(toks, values, common)
    parts = iter(values)
    name = "".join(v if kd == "lit" else next(parts) for kd, v in toks)
    return ok, name, rule


FALLBACK_TOKS = [("slot", "prefix"), ("slot", "suffix")]


def name_fallback(land, common, used):
    for p in common["prefixes"] + land["prefixes"]:
        for s in land["suffixes"] + common["suffixes"]:
            n = p + s
            if name_valid(FALLBACK_TOKS, [p, s], common)[0] and n not in used:
                return n
    return None


def rg11_r3(rows, bl, passed, seed, n, pn, t):
    """RG11 4 and 8: keepers, then the draws in id order. bl: (base, land)
    per row. Returns (names, how) with how 'K', 'F' or the attempt number."""
    pw = t.get("name_pool_weights", SHIPPED_POOLS)
    nt = t.get("name_traits", SHIPPED_TRAITS)
    key = k_name(seed, n)
    best = {}
    for row, (base, _) in zip(rows, bl):
        b = best.get(base)
        if b is None or (row["cells"], -row["id"]) > (b["cells"], -b["id"]):
            best[base] = row
    keeper = {b["id"] for b in best.values()}
    used = set(best)
    out, how = [], []
    common = pn["common"]
    for row, (base, land) in zip(rows, bl):
        if row["id"] in keeper:
            out.append(base)
            how.append("K")
            continue
        trs = region_traits(row, row["id"] in passed, nt)
        tdefs = [pn["trait"][tr] for tr in trs if tr in pn["trait"]]
        got = None
        for att in range(64):
            ok, nm, _ = name_attempt(key, row["id"], att, land, common, tdefs, pw)
            if ok and nm not in used:
                got = (nm, str(att))
                break
        if got is None:
            nm = name_fallback(land, common, used)
            if nm is None:
                fatal("regions: no free name for region %d" % row["id"])
            got = (nm, "F")
        used.add(got[0])
        out.append(got[0])
        how.append(got[1])
    return out, how


# ---------------------------------------------------------------------------
# RG12: borders and passes
# ---------------------------------------------------------------------------

def border_rows(w, h, region, S, walk, water, river, border):
    rows = {}
    for c in range(w * h):
        rc = region[c]
        if rc == 0:
            continue
        x, y = c % w, c // w
        for m in ((c + 1) if x + 1 < w else -1, (c + w) if y + 1 < h else -1):
            if m < 0:
                continue
            rm = region[m]
            if rm == 0 or rm == rc:
                continue
            key = (rc, rm) if rc < rm else (rm, rc)
            r = rows.get(key)
            if r is None:
                r = rows[key] = {"a": key[0], "b": key[1], "length_m": 0, "low_dm": None, "low_x": 0, "low_y": 0,
                                 "walk_pairs": 0, "river_pairs": 0, "river_class_max": 0, "pass_id": 0,
                                 "passes": 0, "_passes": set()}
            r["length_m"] += 16
            hi = max(S[c], S[m])
            if r["low_dm"] is None or hi < r["low_dm"]:
                r["low_dm"] = hi
                cell = m if S[m] > S[c] else c
                r["low_x"], r["low_y"] = cell % w, cell // w
            if walk[c] and walk[c] == walk[m]:
                r["walk_pairs"] += 1
            if border[c] or border[m]:
                r["river_pairs"] += 1
            for p in (c, m):
                if water[p] == WATER_RIVER:
                    r["river_class_max"] = max(r["river_class_max"], river[p])
    return rows


def path_cells(path):
    return [int.from_bytes(path[4 * k:4 * k + 4], "little") for k in range(len(path) // 4)]


def pass_steps(w, cells, water):
    """The step sequence of RG12: corners inserted on diagonal steps (the
    first candidate if land, else the second, else none), equal consecutive
    cells skipped."""
    def land(i):
        return water[i] != WATER_OCEAN
    seq = []
    def add(i):
        if not seq or seq[-1] != i:
            seq.append(i)
    for j, c in enumerate(cells):
        if j > 0:
            p = cells[j - 1]
            px, py, qx, qy = p % w, p // w, c % w, c // w
            if px != qx and py != qy:
                c1 = py * w + qx
                c2 = qy * w + px
                if land(c1):
                    add(c1)
                elif land(c2):
                    add(c2)
        add(c)
    return seq


def pass_steps_j(w, cells, water):
    """pass_steps with, for each sequence cell, the step j that produced it
    (the step from c_j to c_j+1; its corner included); -1 for c_0."""
    def land(i):
        return water[i] != WATER_OCEAN
    seq = []
    def add(i, j):
        if not seq or seq[-1][0] != i:
            seq.append((i, j))
    for j, c in enumerate(cells):
        if j > 0:
            p = cells[j - 1]
            px, py, qx, qy = p % w, p // w, c % w, c // w
            if px != qx and py != qy:
                c1 = py * w + qx
                c2 = qy * w + px
                if land(c1):
                    add(c1, j - 1)
                elif land(c2):
                    add(c2, j - 1)
        add(c, j - 1)
    return seq


def pass_attachment(w, region, water, path, col):
    """RG12 (r2): (row key or None, every crossing as (d, j, order, key))."""
    cells = path_cells(path)
    q = cells.index(col)
    seq = pass_steps_j(w, cells, water)
    found = []
    for k in range(len(seq) - 1):
        (u, _), (v, j) = seq[k], seq[k + 1]
        ux, uy, vx, vy = u % w, u // w, v % w, v // w
        if abs(ux - vx) + abs(uy - vy) != 1:
            continue
        ru, rv = region[u], region[v]
        if ru == 0 or rv == 0 or ru == rv:
            continue
        d = min(abs(j - q), abs(j + 1 - q))
        found.append((d, j, k, (min(ru, rv), max(ru, rv))))
    if not found:
        return None, found
    return min(found)[3], found


def pass_crossings(w, h, region, water, passes, rows):
    """Adds pass_id and passes to the border rows (r2: each pass on at most
    one row); returns the number of attached passes."""
    total = 0
    for pid, path, col in sorted(passes):
        key, _ = pass_attachment(w, region, water, path, col)
        if key is not None:
            rows[key]["_passes"].add(pid)
            total += 1
    for r in rows.values():
        r["passes"] = len(r["_passes"])
        r["pass_id"] = min(r["_passes"]) if r["_passes"] else 0
    return total


def border_rows_second(w, h, region, S, walk, water, river, border):
    """Second method for RG12's rows: every border pair listed with its
    enumeration rank first, then each row reduced from its own pair list."""
    pairs = []
    rank = 0
    for c in range(w * h):
        x, y = c % w, c // w
        for k, m in enumerate(((c + 1) if x + 1 < w else -1, (c + w) if y + 1 < h else -1)):
            if m < 0 or region[c] == 0 or region[m] == 0 or region[c] == region[m]:
                continue
            pairs.append((rank, c, m))
            rank += 1
    by = {}
    for rk, c, m in pairs:
        by.setdefault((min(region[c], region[m]), max(region[c], region[m])), []).append((rk, c, m))
    out = {}
    for key, ps in by.items():
        lo = min(max(S[c], S[m]) for _, c, m in ps)
        rk, c, m = min((p for p in ps if max(S[p[1]], S[p[2]]) == lo), key=lambda p: p[0])
        cell = c if S[c] >= S[m] else m
        rc = [river[p] for _, c2, m2 in ps for p in (c2, m2) if water[p] == WATER_RIVER]
        out[key] = (16 * len(ps), lo, cell % w, cell // w,
                    sum(1 for _, c2, m2 in ps if walk[c2] and walk[c2] == walk[m2]),
                    sum(1 for _, c2, m2 in ps if border[c2] or border[m2]), max(rc) if rc else 0)
    return out


# ---------------------------------------------------------------------------
# Preconditions (section 2, check 5)
# ---------------------------------------------------------------------------

def preconditions(w, h, L, passes, codes):
    n = w * h
    for name, _ in INPUTS:
        if name not in L:
            fatal(PRE + "missing layer '%s'" % name)
    for name, _ in INPUTS:
        if len(L[name]) != n:
            fatal(PRE + "%s length is not width * height" % name)
    for name, hi in (("water", 3), ("river", 15)):
        for i, v in enumerate(L[name]):
            if v > hi:
                fatal(PRE + "%s value %d at (%d, %d)" % (name, v, i % w, i // w))
    for i, v in enumerate(L["biome"]):
        if v not in codes:
            fatal(PRE + "biome value %d at (%d, %d) has no definition" % (v, i % w, i // w))
    for pid, path, col in sorted(passes):
        bad = len(path) == 0 or len(path) % 4 != 0
        if not bad:
            cells = path_cells(path)
            for k, c in enumerate(cells):
                if c >= n:
                    bad = True
                    break
                if k > 0:
                    p = cells[k - 1]
                    if abs(p % w - c % w) > 1 or abs(p // w - c // w) > 1:
                        bad = True
                        break
        if bad:
            fatal(PRE + "pass %d: path is not a chain of cells" % pid)
    # Ruling (r2): every path's chain check first, then every col.
    for pid, path, col in sorted(passes):
        if col is None or not 0 <= col < n or col not in path_cells(path):
            fatal(PRE + "pass %d: col cell is not on its path" % pid)


# ---------------------------------------------------------------------------
# The stage
# ---------------------------------------------------------------------------

def regions(w, h, L, passes, t, seed, defs=None, second=False):
    """RG1 to RG13 on a W x H grid; returns every intermediate. passes: a
    list of (id, path bytes)."""
    defs = shipped_defs() if defs is None else defs
    check_tuning(t)
    check_nouns(defs)
    preconditions(w, h, L, passes, set(c for c, _, _, _ in defs))
    n = w * h
    e, water, depth, river, fd = L["elevation"], L["water"], L["water_depth"], L["river"], L["flow_dir"]
    S = [e[i] + depth[i] for i in range(n)]
    try:
        hand = br.hand_fifo(w, h, e, water, depth, river, fd, 1)
    except FatalError as ex:
        fatal(PRE + str(ex))
    kc = cr.stream_key(seed, STREAM_COST)
    ks = cr.stream_key(seed, STREAM_SEED)
    noise = [0] * n
    wt = [0] * n
    for i in range(n):
        if water[i] != WATER_OCEAN:
            noise[i] = cost_noise(kc, i % w, i // w, t)
            wt[i] = step_weight(noise[i], hand[i], t)
    hab = L["habitability"]
    stats = {}
    cands = candidates(w, h, water, hab, S, ks, t)
    border = border_river_mask(water, river, t)
    order = sorted(cands, key=lambda i: site_key(i, w, hab, S, ks))
    greedy = select_seeds(w, order, border, t, stats)
    landmass, nlm = landmasses(w, h, water)
    seeds = landmass_seeds(w, h, water, landmass, nlm, greedy, hab, S, ks)
    owner0, cost = flood(w, h, wt, water, border, seeds, t)
    if second:
        o2, c2 = flood_buckets(w, h, wt, water, border, seeds, t)
        if (o2, c2) != (owner0, cost):
            fatal("check: the bucket flood differs from the heap flood")
    log = []
    owner = merge(w, h, owner0, water, border, len(seeds), min_cells_of(t), log)
    if second and n <= 128 * 128:
        if merge_recount(w, h, owner0, water, border, len(seeds), min_cells_of(t)) != owner:
            fatal("check: the recount merge differs")
    region, ids = renumber(owner, n)
    R = len(ids)
    seeds_of = [None] * (R + 1)
    for k, s in enumerate(seeds):
        num = k + 1
        if num in ids:
            seeds_of[ids[num]] = s
    walk = L["walk_area"]
    walk_dist = [rr.metres(v) for v in rr.chamfer(w, h, [1 if v > 0 else 0 for v in walk])]
    rows = region_rows_props(w, h, L, region, R, seeds_of, landmass, walk_dist, defs, t)
    lm_cells = {}
    for i in range(n):
        if landmass[i]:
            lm_cells[landmass[i]] = lm_cells.get(landmass[i], 0) + 1
    brows = border_rows(w, h, region, S, walk, water, river, border)
    stats["crossings"] = pass_crossings(w, h, region, water, passes, brows)
    # RG11 after RG12 (r3: the pass trait reads the border rows).
    nm = names(rows, w, h, water, lm_cells, t, defs, stats, list(brows.values()), seed) if rows else []
    for r, s in zip(rows, nm):
        r["name"] = s
    stats["crossed"] = sum(len(set(k for _, _, _, k in pass_attachment(w, region, water, path, col)[1]))
                           for _, path, col in passes)
    if second:
        b2 = border_rows_second(w, h, region, S, walk, water, river, border)
        b1 = {k: (r["length_m"], r["low_dm"], r["low_x"], r["low_y"], r["walk_pairs"], r["river_pairs"],
                  r["river_class_max"]) for k, r in brows.items()}
        if b1 != b2:
            fatal("check: the second border enumeration differs")
    border_list = [brows[k] for k in sorted(brows)]
    biome_rows = [(r["id"], code, cnt) for r in rows for code, cnt in r["biomes"]]
    stats["greedy"] = len(greedy)
    stats["landmass_seeds"] = len(seeds) - len(greedy)
    stats["merges"] = len(log)
    stats["lake_cells"] = sum(1 for v in water if v == WATER_LAKE)
    stats["border_river_cells"] = sum(border)
    stats["corner"] = corner_count(w, h, water, border)
    return {"w": w, "h": h, "L": L, "t": t, "S": S, "hand": hand, "noise": noise, "wt": wt, "cands": cands,
            "border": border, "greedy": greedy, "seeds": seeds, "landmass": landmass, "nlm": nlm,
            "owner0": owner0, "cost": cost, "owner": owner, "region": region, "rows": rows,
            "borders": border_list, "biome_rows": biome_rows, "walk_dist": walk_dist, "stats": stats,
            "passes": passes, "merge_log": log}


def corner_count(w, h, water, border):
    """Diagonal land steps whose two corners are both border river cells
    (where the corner term of RG7 applies)."""
    c = 0
    for i in range(w * h):
        x, y = i % w, i // w
        for d in (1, 3):
            xx, yy = x + DX[d], y + DY[d]
            if 0 <= xx < w and 0 <= yy < h and border[y * w + xx] and border[yy * w + x]:
                if water[i] != WATER_OCEAN and water[yy * w + xx] != WATER_OCEAN:
                    c += 1
    return c


def region_table_rows(r):
    return [tuple(row[c] for c in REGION_COLUMNS) for row in r["rows"]]


def border_table_rows(r):
    return [tuple(row[c] for c in BORDER_COLUMNS) for row in r["borders"]]


# ---------------------------------------------------------------------------
# Report (9) and palette
# ---------------------------------------------------------------------------

def km2(cells):
    v = er.rha(cells * 256, 10 ** 4)
    return "%d.%02d" % (v // 100, v % 100)


def report_items(r):
    rows, brs = r["rows"], r["borders"]
    items = [("regions", str(len(rows)))]
    if rows:
        cells = sorted(x["cells"] for x in rows)
        n = len(cells)
        items.append(("region_area_km2", " ".join(km2(cells[min(n - 1, (p * n) // 100)]) for p in (0, 50, 100))))
    else:
        items.append(("region_area_km2", "n/a n/a n/a"))
    items.append(("region_borders", str(len(brs))))
    items.append(("region_borders_walkable", str(sum(1 for b in brs if b["walk_pairs"] > 0))))
    items.append(("region_borders_river", str(sum(1 for b in brs if b["river_pairs"] > 0))))
    items.append(("region_borders_pass", str(sum(1 for b in brs if b["pass_id"] > 0))))
    used = set()
    for b in brs:
        used.add(b["a"])
        used.add(b["b"])
    items.append(("regions_isolated", str(sum(1 for x in rows if x["id"] not in used))))
    items.append(("regions_upland", str(sum(1 for x in rows if 4 * x["walk_cells"] < x["cells"]))))
    return items


def ids_colour(v):
    if v == 0:
        return (0, 0, 0)
    hh = cr.mix64(v)
    return tuple(64 + (((hh >> (8 * j)) & 0xFF) * 191) // 255 for j in range(3))


def composite_black(w, h, region):
    """P0-04 amendment 18 (e): cells drawn black over the biomes composite."""
    out = set()
    for y in range(h):
        for x in range(w):
            v = region[y * w + x]
            if v == 0:
                continue
            if x + 1 < w and region[y * w + x + 1] not in (0, v):
                out.add((x, y))
            if y + 1 < h and region[(y + 1) * w + x] not in (0, v):
                out.add((x, y))
    return out


# ---------------------------------------------------------------------------
# Hashes
# ---------------------------------------------------------------------------

h_i64 = br.h_i64
layer_hash = br.layer_hash
table_hash = br.table_hash


def h_i32(values):
    return hashlib.sha256(struct.pack("<%di" % len(values), *values)).hexdigest()


def h_u32(values):
    return hashlib.sha256(struct.pack("<%dI" % len(values), *values)).hexdigest()


def passes_hash(passes):
    """Golden item `passes`: SHA-256 over the rows in id order, each id and
    path length in bytes as int64 LE, then the path bytes; "-" if none."""
    if not passes:
        return "-"
    b = bytearray()
    for pid, path, col in sorted(passes):
        b += struct.pack("<qqq", pid, col, len(path)) + path
    return hashlib.sha256(bytes(b)).hexdigest()


# ---------------------------------------------------------------------------
# Inputs (p1-05-resources.txt cases; table pass from p1-01-elevation.txt)
# ---------------------------------------------------------------------------

RESOURCES_GOLDEN = os.path.join(_HERE, "..", "golden", "p1-05-resources.txt")
ELEVATION_GOLDEN = os.path.join(_HERE, "..", "golden", "p1-01-elevation.txt")


def read_golden(path):
    """{case: {"line": [tokens of the case line], "items": {item: value}}}."""
    out = {}
    with open(path) as f:
        for line in f:
            tok = line.split()
            if not tok or tok[0].startswith("#"):
                continue
            if tok[0] == "case":
                out[tok[1]] = {"line": tok, "items": {}}
            else:
                out[tok[0]]["items"][tok[1]] = tok[2]
    return out


_G = {}


def golden_of(path):
    if path not in _G:
        _G[path] = read_golden(path)
    return _G[path]


def resources_case(name):
    """(base, fixture, N, seed, overrides) of a p1-05 case."""
    tok = golden_of(RESOURCES_GOLDEN)[name]["line"]
    ov = [] if tok[6] == "-" else [tuple(p.split("=", 1)) for p in tok[6].split(",")]
    return tok[2], tok[3], int(tok[4]), int(tok[5]), ov


def check_layers(case, w, h, L):
    """Every input layer (and rainfall, when present) against the p1-05 case."""
    g = golden_of(RESOURCES_GOLDEN)[case]["items"]
    for name, code in INPUTS + (("rainfall", 3),):
        if name not in L:
            continue
        if layer_hash(w, h, code, L[name]) != g[name]:
            fatal("input %s of %s differs from p1-05-resources.txt (stale cache or world?)" % (name, case))


def pass_rows_text(rows):
    """P1-01 golden item pass_rows text from full pass rows (dicts)."""
    return er.rows_text(rows)


def elevation_pass_rows(fixture, n, seed):
    """Table pass of a world by the elevation reference (slow), cached in
    UNMAPPED_ELEVATION_CACHE as passes-<fixture>-<n>-<seed>.txt."""
    cache = os.environ.get("UNMAPPED_ELEVATION_CACHE")
    path = os.path.join(cache, "passes-%s-%d-%d.txt" % (fixture, n, seed)) if cache else None
    if path and os.path.exists(path):
        rows = []
        with open(path) as f:
            for line in f:
                v = line.split()
                rows.append({"id": int(v[0]), "range_id": int(v[1]), "kind": int(v[2]), "capped": int(v[3]),
                             "col_x": int(v[4]), "col_y": int(v[5]), "saddle_dm": int(v[6]), "col_dm": int(v[7]),
                             "path": bytes.fromhex(v[8])})
        return rows
    t = er.shipped_tuning() if fixture == "shipped" else er.small_tuning()
    sys.stderr.write("passes: running the elevation reference for %s %d %d\n" % (fixture, n, seed))
    r = er.run_world(seed, n, t, upto="heights")
    rows = er.pass_rows(n, r["heights"]["passes"]["sites"], r["heights"]["h7"])
    if path:
        os.makedirs(cache, exist_ok=True)
        with open(path, "w") as f:
            for x in rows:
                f.write("%d %d %d %d %d %d %d %d %s\n" % (x["id"], x["range_id"], x["kind"], x["capped"], x["col_x"],
                                                        x["col_y"], x["saddle_dm"], x["col_dm"], x["path"].hex()))
    return rows


def check_pass_rows(case, rows):
    """Against p1-01-elevation.txt's pass_rows item of the same case name,
    where it exists; returns whether it was checked."""
    g = golden_of(ELEVATION_GOLDEN).get(case)
    if g is None:
        return False
    want = g["items"]["pass_rows"]
    got = hashlib.sha256(pass_rows_text(rows).encode()).hexdigest() if rows else "-"
    if got != want:
        fatal("table pass of %s differs from p1-01-elevation.txt" % case)
    return True


def db_pass_rows(path):
    con = sqlite3.connect("file:%s?mode=ro" % path, uri=True)
    try:
        rows = []
        for r in con.execute("SELECT id, range_id, kind, capped, col_x, col_y, saddle_dm, col_dm, path FROM pass "
                             "ORDER BY id"):
            rows.append({"id": r[0], "range_id": r[1], "kind": r[2], "capped": r[3], "col_x": r[4], "col_y": r[5],
                         "saddle_dm": r[6], "col_dm": r[7], "path": bytes(r[8])})
        return rows
    finally:
        con.close()


def db_world(path):
    """(N, L with the 13 inputs and rainfall, pass rows) of a world file."""
    raw = br.db_layers(path)
    L = {}
    n = None
    for name, code in INPUTS + (("rainfall", 3),):
        w, h, tcode, values = raw[name]
        if tcode != code or w != h or (n is not None and w != n):
            fatal("%s: layer %s is not N x N of type %d" % (path, name, code))
        n = w
        L[name] = values
    return n, L, db_pass_rows(path)


def case_inputs(case, db_dir=None):
    """(w, h, L, pass rows, seed) of a p1-05 case, every input checked."""
    base, fixture, n, seed, ov = resources_case(case)
    if db_dir:
        path = os.path.join(db_dir, case + ".db")
        if os.path.exists(path):
            nn, L, rows = db_world(path)
            if nn != n:
                fatal("%s: N %d, expected %d" % (path, nn, n))
            check_layers(case, n, n, L)
            if not check_pass_rows(case, rows):
                sys.stderr.write("note: %s: table pass not in p1-01-elevation.txt (checked as a chain only)\n" % case)
            sys.stderr.write("inputs: %s decoded from %s and checked\n" % (case, path))
            return n, n, L, rows, seed
    seed2, r = rr.case_result(case, base, ov)
    assert seed2 == seed
    w, h = r["w"], r["h"]
    L = {name: r["L"][name] for name in UPSTREAM}
    L["rainfall"] = r["L"]["rainfall"]
    for name in RESOURCE_LAYERS:
        L[name] = r["out"][name]
    check_layers(case, w, h, L)
    rows = []
    if fixture in ("shipped", "small"):
        want = golden_of(ELEVATION_GOLDEN).get(case)
        if want is None:
            fatal("no p1-01 golden case %s for table pass" % case)
        if want["items"]["pass_rows"] != "-":
            rows = elevation_pass_rows(fixture, n, seed)
        check_pass_rows(case, rows)
    return w, h, L, rows, seed


# Synthetic pass tables for the fixtures (golden column <passes>).
def synthetic_passes(name, w):
    def path(cells):
        return b"".join((y * w + x).to_bytes(4, "little") for x, y in cells)
    if name == "X":
        return [
            (1, path([(x, 20) for x in range(2, 38)]), 20 * w + 30),          # west to east along row 20
            (2, path([(k, k) for k in range(4, 36)]), 10 * w + 10),           # the main diagonal
            (4, path([(20, y) for y in range(2, 38)]), 2 * w + 20),           # north to south along column 20
            (7, path([(10, 30), (11, 29), (11, 29), (12, 30), (13, 29), (14, 30), (13, 31), (12, 32),
                      (11, 31), (10, 30)]), 30 * w + 10),                     # a zigzag loop back to its start
            (9, path([(35, 4), (34, 5), (33, 6), (32, 7), (31, 8), (30, 9), (29, 10), (28, 11)]),
             8 * w + 31),                                                     # anti-diagonal
        ]
    raise KeyError(name)


T40 = [("seed_tile_m", "256"), ("seed_spacing_m", "200"), ("region_min_area_m2", "16384")]
T128 = [("seed_tile_m", "256"), ("seed_spacing_m", "400"), ("region_min_area_m2", "65536")]
WORDS = "Mid/Nor/Norost/Ost/Sudost/Sud/Sudwest/West/Norwest/Holm/Hoch"

# (name, p1-05 case, regions overrides, passes ("world" or a synthetic set), why)
GOLDEN_CASES = [
    ("designed-0", "designed-0", T40, "world", "designed fixture 0, test tuning of 40 x 40 (tile 16, spacing 12.5 cells)"),
    ("designed-1", "designed-1", T40, "world", "designed fixture 1, test tuning"),
    ("designed-2", "designed-2", T40, "world", "designed fixture 2, test tuning"),
    ("designed-3", "designed-3", T40, "world", "designed fixture 3, test tuning"),
    ("designed-0-shipped", "designed-0", [], "world", "shipped tuning on a 640 m fixture"),
    ("designed-1-passes", "designed-1", T40, "X", "synthetic pass table X (five paths, ids 1 2 4 7 9)"),
    ("designed-2-passes", "designed-2", T40 + [("region_min_area_m2", "0")], "X",
     "synthetic passes, no merging"),
    ("designed-1-allriver", "designed-1", T40 + [("river_border_class", "1"), ("river_cost_m", "500")], "world",
     "every river cell a border river: bank rule, corner rule, river pairs"),
    ("designed-0-allriver", "designed-0", T40 + [("river_border_class", "1")], "world",
     "every river cell a border river, shipped river cost"),
    ("designed-2-noriver", "designed-2", T40 + [("river_border_class", "16")], "world",
     "no border river (class 16)"),
    ("designed-3-nomerge", "designed-3", T40 + [("region_min_area_m2", "0")], "world", "no merging"),
    ("designed-0-bigmerge", "designed-0", T40 + [("region_min_area_m2", "300000")], "world",
     "min 1172 cells: merges down to the largest regions"),
    ("designed-1-flat", "designed-1", T40 + [("cost_noise_amp", "0"), ("hand_ref_m", "10000")], "world",
     "no noise, height above drainage almost free: distance regions"),
    ("designed-2-rough", "designed-2", T40 + [("cost_noise_amp", "0.9"), ("cost_noise", "64/1/1"),
                                             ("hand_ref_m", "1")], "world",
     "largest noise at the shortest wavelength, steepest hand weight"),
    ("designed-3-lake", "designed-3", T40 + [("lake_cost_factor", "10")], "world", "lake factor 10"),
    ("designed-0-lake1", "designed-0", T40 + [("lake_cost_factor", "1"), ("river_cost_m", "0")], "world",
     "lake factor 1, river cost 0"),
    ("designed-0-ore", "designed-0", T40 + [("ore_rich_min", "1"), ("ore_reach_m", "0")], "world",
     "every ore > 0 rich, reach 0"),
    ("designed-3-ore", "designed-3", T40 + [("ore_rich_min", "40"), ("ore_reach_m", "10000")], "world",
     "ore 40 rich, reach 10 km"),
    ("designed-2-names", "designed-2", T40 + [("name_central_frac", "0.5"), ("names", WORDS)], "world",
     "other words, central radius half the map"),
    ("designed-3-central0", "designed-3", T40 + [("name_central_frac", "0")], "world", "no central sector"),
    ("designed-1-ownnone", "designed-1", T40 + [("upland_own_noun", "none")], "world",
     "r2: upland_own_noun empty: every upland region is Fells"),
    ("designed-3-ownall", "designed-3", T40 + [("upland_own_noun", "temperate_forest/grassland/alpine/heath")], "world",
     "r2: forest and grassland keep their noun in upland regions"),
    ("designed-0-lakemerge", "designed-0", T40 + [("region_min_area_m2", "0"), ("seed_spacing_m", "160"),
                                                  ("seed_tile_m", "256"), ("lake_cost_factor", "1")], "world",
     "r2: small regions and cheap lakes: Fells regions"),
    ("designed-0-tile", "designed-0", [("seed_tile_m", "640"), ("seed_spacing_m", "320"),
                                       ("region_min_area_m2", "16384")], "world",
     "one tile covers the map (40 cells): one candidate"),
    ("designed-1-spacing", "designed-1", [("seed_tile_m", "256"), ("seed_spacing_m", "160"),
                                          ("region_min_area_m2", "0")], "world",
     "smallest spacing (10 cells): every candidate a seed"),
    ("terrace-0", "terrace-0", T40, "world", "terrace fixture 0"),
    ("terrace-1", "terrace-1", T40, "world", "terrace fixture 1"),
    ("shipped-48-s3", "shipped-48-s3", [], "world", "small shipped world"),
    ("shipped-64-s0", "shipped-64-s0", [], "world", "small shipped world"),
    ("shipped-64-s0-t", "shipped-64-s0", T128, "world", "small shipped world, test tuning of 10.2"),
    ("shipped-128-s0", "shipped-128-s0", [], "world", "P1-06 10.4"),
    ("shipped-128-s9", "shipped-128-s9", [], "world", "shipped world"),
    ("shipped-128-s42", "shipped-128-s42", [], "world", "shipped world"),
    ("small-128-s0", "small-128-s0", [], "world", "small-map fixture"),
    ("small-128-s1", "small-128-s1", [], "world", "small-map fixture"),
    ("small-128-s2", "small-128-s2", [], "world", "small-map fixture with natural passes (table pass)"),
    ("shipped-128-s0-t", "shipped-128-s0", T128, "world", "test tuning of 10.2 (tile 256, spacing 400, 256 cells)"),
    ("shipped-128-s42-t", "shipped-128-s42", T128, "world", "test tuning of 10.2"),
    ("small-128-s2-t", "small-128-s2", T128, "world", "test tuning of 10.2, natural passes"),
]
LARGE_CASES = [
    ("shipped-512-s0", "shipped-512-s0", [], "world", "P1-06 10.4; the report example of 9"),
    ("shipped-512-s42", "shipped-512-s42", [], "world", "P1-06 10.4 (natural passes)"),
    ("shipped-512-phrase", "shipped-512-phrase", [], "world",
     "P1-06 10.4 (seed of the phrase \"The Unmapped\"; natural passes)"),
    ("shipped-1024-s1", "shipped-1024-s1", [], "world", "P1-06 10.4"),
]


def case_result(name, base, overrides, passes_src, db_dir=None, second=False):
    w, h, L, rows, seed = case_inputs(base, db_dir)
    passes = ([(r["id"], r["path"], r["col_y"] * w + r["col_x"]) for r in rows] if passes_src == "world"
              else synthetic_passes(passes_src, w))
    t = tuning_of(overrides)
    return seed, regions(w, h, L, passes, t, seed, second=second)


def coverage(r):
    st = r["stats"]
    keys = ("greedy", "landmass_seeds", "rejected", "bank", "merges", "isle", "central", "numbered", "crossings",
            "lake_cells", "border_river_cells", "corner", "mere", "fells", "crossed")
    return ",".join("%s:%d" % (k, st.get(k, 0)) for k in keys)


def case_items(r):
    w, h, L = r["w"], r["h"], r["L"]
    n = w * h
    land = [L["water"][i] != WATER_OCEAN for i in range(n)]
    items = []
    for name, code in INPUTS:
        items.append((name, layer_hash(w, h, code, L[name])))
    items.append(("passes", passes_hash(r["passes"])))
    items.append(("hand", h_i64(r["hand"])))
    items.append(("noise", h_i64([r["noise"][i] if land[i] else 0 for i in range(n)])))
    items.append(("wt", h_i64(r["wt"])))
    items.append(("candidates", h_u32(r["cands"])))
    items.append(("seeds", h_u32(r["seeds"])))
    items.append(("seed_count", "%d,%d" % (len(r["greedy"]), len(r["seeds"]) - len(r["greedy"]))))
    items.append(("flood", h_i32(r["owner0"])))
    items.append(("cost", h_i64([r["cost"][i] if land[i] else -1 for i in range(n)])))
    items.append(("merged", "%d,%d" % (len(r["seeds"]), len(r["rows"]))))
    items.append(("region", layer_hash(w, h, 3, r["region"])))
    items.append(("table_region", table_hash(28, region_table_rows(r))))
    items.append(("table_region_biome", table_hash(3, r["biome_rows"])))
    items.append(("table_region_border", table_hash(11, border_table_rows(r))))
    items.append(("landmasses", str(r["nlm"])))
    items.append(("cover", coverage(r)))
    for k, v in report_items(r):
        items.append((k, v.replace(" ", "_")))
    return items


GOLDEN_HEADER = """\
# P1-06 regions golden vectors for generator 0.11.0 (spec second version,
# 2026-11-18, revision r2: one border per pass, the Mere and Fells nouns;
# schema 5), generated by
# worldgen/tests/reference/p1_regions_reference.py golden, an independent
# exact-integer reference written from docs/specs/p1-06-regions.md and P1-00
# alone. The thirteen input layers of each case are those of the named case
# of p1-05-resources.txt (generator 0.10.0) and are checked against its
# layer hashes before use; table pass of a world is checked against the
# pass_rows item of p1-01-elevation.txt where that file has the case.
# Do not edit by hand; never regenerate to make a C++ test pass.
#
# Format: ASCII, LF, one value per line, tokens separated by one space.
#   case <name> <base> <fixture> <N> <seed> <overrides> <passes>
#       starts a case. <base> is the case of p1-05-resources.txt whose seven
#       upstream layers and seven resources layers are used (its own
#       resources tuning); <fixture>, <N>, <seed> repeat its case line; the
#       stage's world seed is <seed>. Biome definitions: the shipped twelve.
#       Tuning: the shipped worldgen.regions of 4 with <overrides>, "-" or
#       comma-separated field=value (fx values as decimal text; cost_noise as
#       wavelength_m/octaves/gain; names as the eleven words central/north/
#       northeast/east/southeast/south/southwest/west/northwest/isle/upland;
#       upland_own_noun as biome keys joined by "/", "none" for empty).
#       <passes>: "world" = the world's table pass (none on the designed and
#       terrace fixtures), or a synthetic table of the reference
#       (synthetic_passes: "X" on a 40-wide grid, ids 1, 2, 4, 7, 9, col in
#       brackets: 1 row 20 x 2..37 [(30, 20)]; 2 (k, k) k 4..35 [(10, 10)];
#       4 column 20 y 2..37 [(20, 2)]; 7 (10,30) (11,29) (11,29) (12,30)
#       (13,29) (14,30) (13,31) (12,32) (11,31) (10,30) [(10, 30)];
#       9 (35-k, 4+k) k 0..7 [(31, 8)]).
#   <name> <item> <value>                 one value, items in step order
# Items, in step order (the test reports the first that differs):
#   elevation water water_depth flow_dir river biome fertility ore timber
#   fresh_water forage habitability walk_area
#                P0-04 8 layer hash of each input layer
#   passes       SHA-256 over the pass rows in id order, each id, col cell
#                index (col_y N + col_x) and path length in bytes as int64
#                LE followed by the path; "-" if none
#   hand         SHA-256 of G7 with k = 1 per cell, int64 LE, index order
#   noise        SHA-256 of RG3's nz raw on land cells, 0 on ocean, int64 LE
#   wt           SHA-256 of RG3's wt raw on land cells, 0 on ocean, int64 LE
#   candidates   SHA-256 of RG4's candidate cells in tile order, u32 LE
#   seeds        SHA-256 of the seed cells in seed order (RG5, RG6), u32 LE
#   seed_count   G,M: seeds of RG5 and seeds added by RG6
#   flood        SHA-256 of RG7's owner (seed number) per cell, 0 on ocean, int32 LE
#   cost         SHA-256 of RG7's best per cell, -1 on ocean, int64 LE
#   merged       S,R: regions before RG8 (= seeds) and after
#   region       P0-04 8 layer hash of the layer region (u16)
#   table_region table_region_biome table_region_border
#                P0-04 8 logical hash of each table (28, 3, 11 columns)
#   landmasses   M of RG6
#   cover        counts: greedy seeds, landmass seeds, rejected candidates,
#                bank (near but across a border river: a check passed by
#                the bank rule), merges, isle names, central names, numbered
#                names, crossings (passes attached to a row, RG12 r2), lake
#                cells, border river cells, corner (diagonal land steps
#                between two border river cells), mere and fells (names by
#                RG11 3 b and c, r2), crossed ((pass, row) pairs crossed by a
#                path, before the attachment)
#   regions region_area_km2 region_borders region_borders_walkable
#   region_borders_river region_borders_pass regions_isolated regions_upland
#                the report values of P1-06 9, spaces written as "_"
#
"""


def golden_text(large=False, db_dir=None):
    out = [GOLDEN_HEADER]
    for name, base, overrides, psrc, why in GOLDEN_CASES + (LARGE_CASES if large else []):
        seed, r = case_result(name, base, overrides, psrc, db_dir if large else None)
        _, fx, n, _, _ = resources_case(base)
        ov = ",".join("%s=%s" % kv for kv in overrides) if overrides else "-"
        out.append("# %s\n" % why)
        out.append("case %s %s %s %d %d %s %s\n" % (name, base, fx, n, seed, ov, psrc))
        for item, v in case_items(r):
            out.append("%s %s %s\n" % (name, item, v))
        sys.stderr.write("golden: %s done\n" % name)
    return "".join(out)


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------

class Checker(rr.Checker):
    pass


def grid_of(rows):
    return [int(v) for row in rows for v in row.split()]


def flat_world(w, h):
    return {"water": [WATER_DRY] * (w * h), "river": [0] * (w * h)}


def run_flood(w, h, water, river, wt, seeds_xy, t):
    border = border_river_mask(water, river, t)
    seeds = [y * w + x for x, y in seeds_xy]
    o, c = flood(w, h, wt, water, border, seeds, t)
    o2, c2 = flood_buckets(w, h, wt, water, border, seeds, t)
    return o, c, (o, c) == (o2, c2)


def rows_of(v, w, h):
    return [" ".join(str(v[y * w + x]) for x in range(w)) for y in range(h)]


def check_rg3(c):
    t = shipped_tuning()
    c.eq("RG3 hand 600", step_weight(0, 600, t), 14602888806)
    c.eq("RG3 hand 300", step_weight(0, 300, t), 9448928051)
    w = step_weight(0, 600, t)
    c.eq("RG3 orth step", er.round_int(w * 160), 544)
    c.eq("RG3 diag step", er.round_int(er.mul(w, D160)), 769)
    c.eq("RG3 hand 0", step_weight(0, 0, t), ONE)
    c.eq("D160", D160, 971840160000)
    # nf at the extremes of nz with the largest amplitude 0.9 (7: [0.1, 1.9]).
    t2 = tuning_of([("cost_noise_amp", "0.9")])
    c.eq("RG3 nf low", step_weight(-ONE, 0, t2), ONE - dec("0.9"))
    c.eq("RG3 nf high", step_weight(ONE, 0, t2), ONE + dec("0.9"))


def check_rg7(c):
    t = shipped_tuning()
    W, H = 9, 5
    n = W * H
    A, B = (1, 2), (8, 2)
    flat_owner = ["1 1 1 1 1 2 2 2 2"] * 5
    # Flat.
    w0 = flat_world(W, H)
    o, cost, same = run_flood(W, H, w0["water"], w0["river"], [ONE] * n, [A, B], t)
    c.ok("RG7 flat: bucket queue agrees", same)
    c.eq("RG7 flat owners", rows_of(o, W, H), flat_owner)
    c.eq("RG7 flat row 2", rows_of(cost, W, H)[2], "160 0 160 320 480 480 320 160 0")
    # Ridge: hand 600 in column 3.
    wt = [step_weight(0, 600 if i % W == 3 else 0, t) for i in range(n)]
    o, cost, same = run_flood(W, H, w0["water"], w0["river"], wt, [A, B], t)
    c.ok("RG7 ridge: bucket queue agrees", same)
    c.eq("RG7 ridge owners", rows_of(o, W, H), ["1 1 1 1 2 2 2 2 2"] * 5)
    want = ["386 320 386 930 772 612 452 386 320", "226 160 226 770 706 546 386 226 160",
            "160 0 160 704 640 480 320 160 0"]
    want = want + [want[1], want[0]]
    c.eq("RG7 ridge costs", rows_of(cost, W, H), want)
    # Large river: column 3 class 6; class 5 is the flat case.
    for cls in (6, 5):
        water = [WATER_RIVER if i % W == 3 else WATER_DRY for i in range(n)]
        river = [cls if i % W == 3 else 0 for i in range(n)]
        o, cost, same = run_flood(W, H, water, river, [ONE] * n, [A, B], t)
        c.ok("RG7 river %d: bucket queue agrees" % cls, same)
        if cls == 6:
            c.eq("RG7 river owners", rows_of(o, W, H), ["1 1 1 1 2 2 2 2 2"] * 5)
            c.eq("RG7 river row 2", rows_of(cost, W, H)[2], "160 0 160 20320 640 480 320 160 0")
        else:
            c.eq("RG7 river 5 owners", rows_of(o, W, H), flat_owner)
            c.eq("RG7 river 5 row 2", rows_of(cost, W, H)[2], "160 0 160 320 480 480 320 160 0")
    # Diagonal river, class 7.
    diag = {(2, 0), (3, 1), (4, 2), (5, 3), (6, 4)}
    water = [WATER_RIVER if (i % W, i // W) in diag else WATER_DRY for i in range(n)]
    river = [7 if (i % W, i // W) in diag else 0 for i in range(n)]
    o, cost, same = run_flood(W, H, water, river, [ONE] * n, [(0, 4), (8, 0)], t)
    c.ok("RG7 diagonal: bucket queue agrees", same)
    c.eq("RG7 diagonal owners", rows_of(o, W, H),
         ["1 1 1 2 2 2 2 2 2", "1 1 1 1 2 2 2 2 2", "1 1 1 1 1 2 2 2 2", "1 1 1 1 1 2 2 2 2",
          "1 1 1 1 1 1 2 2 2"])
    c.eq("RG7 diagonal row 0", rows_of(cost, W, H)[0], "640 706 20772 800 640 480 320 160 0")
    c.eq("RG7 diagonal row 4", rows_of(cost, W, H)[4], "0 160 320 480 640 800 20772 706 640")
    border = border_river_mask(water, river, t)
    c.eq("RG7 corner step (2,1)->(3,0)", step_cost(W, 1 * W + 2, 0 * W + 3, 7, [ONE] * n, water, border, t), 20226)
    # Lake: columns 3 to 5, rows 1 to 3.
    water = [WATER_LAKE if (3 <= i % W <= 5 and 1 <= i // W <= 3) else WATER_DRY for i in range(n)]
    o, cost, same = run_flood(W, H, water, [0] * n, [ONE] * n, [A, B], t)
    c.ok("RG7 lake: bucket queue agrees", same)
    c.eq("RG7 lake owners", rows_of(o, W, H), flat_owner)
    c.eq("RG7 lake row 2", rows_of(cost, W, H)[2], "160 0 160 480 800 640 320 160 0")
    border = [0] * n
    c.eq("RG7 lake orth step", step_cost(W, 2 * W + 2, 2 * W + 3, 0, [ONE] * n, water, border, t), 320)
    c.eq("RG7 lake diag step", step_cost(W, 1 * W + 2, 2 * W + 3, 1, [ONE] * n, water, border, t), 452)


def check_rg5(c):
    t = shipped_tuning()
    W = H = 200
    n = W * H
    water = [WATER_RIVER if i % W == 168 else WATER_DRY for i in range(n)]
    river = [6 if i % W == 168 else 0 for i in range(n)]
    border = border_river_mask(water, river, t)
    c1, c2, c3, c4, c5 = (100, 100), (160, 100), (150, 175), (40, 40), (175, 120)
    order = [y * W + x for x, y in (c2, c1, c3, c4, c5)]
    got = [(s % W, s // W) for s in select_seeds(W, order, border, t)]
    c.eq("RG5 seeds with the river", got, [c2, c3, c4, c5])
    got = [(s % W, s // W) for s in select_seeds(W, order, [0] * n, t)]
    c.eq("RG5 seeds without the river", got, [c2, c3, c4])
    c.ok("RG5 c2 -> c5 crosses", crosses(W, border, c2, c5))
    c.ok("RG5 c3 -> c5 crosses", crosses(W, border, c3, c5))
    c.ok("RG5 c2 -> c1 does not cross", not crosses(W, border, c2, c1))
    c.eq("RG5 RHA(150, 20)", er.rha(150, 20), 8)
    c.eq("RG5 RHA(-150, 20)", er.rha(-150, 20), -8)
    # The first point on column 168 of the segment c2 -> c5 is j = 10.
    pts = [(160 + er.rha(15 * j, 20), 100 + er.rha(20 * j, 20)) for j in range(1, 21)]
    c.eq("RG5 first river point", next(j + 1 for j, p in enumerate(pts) if p[0] == 168), 10)
    c.eq("RG5 point j=10", pts[9], (168, 110))
    # Diagonal slip: a segment between two diagonal river cells.
    W2 = 10
    bm = [0] * 100
    bm[3 * W2 + 4] = 1
    bm[4 * W2 + 3] = 1
    c.ok("RG5 diagonal slip", crosses(W2, bm, (3, 3), (4, 4)))
    c.ok("RG5 diagonal slip on the way", crosses(W2, bm, (3, 3), (5, 5)))
    bm[4 * W2 + 3] = 0
    c.ok("RG5 one corner is no slip", not crosses(W2, bm, (3, 3), (4, 4)))
    # Spacing: near is strictly below the spacing.
    c.eq("RG5 near 74 cells?", (16 * 75) ** 2 < 1200 ** 2, False)
    # Site order: habitability, then the lower surface, then the hash.
    hab = [5, 5, 7, 5]
    S = [10, 9, 50, 9]
    key = cr.stream_key(0, STREAM_SEED)
    srt = sorted(range(4), key=lambda i: site_key(i, 4, hab, S, key))
    c.eq("site order: most habitable first", srt[0], 2)
    h1, h3 = er.hash_at(key, 1, 0), er.hash_at(key, 3, 0)
    c.eq("site order: lower surface, then larger hash", srt[1:], ([1, 3] if h1 > h3 else [3, 1]) + [0])
    c.eq("hash_at fast form", er.hash_at(key, 3, 7), cr.hash_at(key, 3, 7))


def merge_example_grid():
    """A W x H grid realising the RG8 example (owners 1 to 4, river cells of
    class 6 between regions 1 and 3); used by the C++ test as well."""
    W, H = 32, 40
    owner = [0] * (W * H)
    water = [WATER_OCEAN] * (W * H)
    river = [0] * (W * H)

    def put(x0, x1, y0, y1, o):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                owner[y * W + x] = o
                water[y * W + x] = WATER_DRY
    put(0, 24, 0, 19, 1)
    put(25, 28, 0, 19, 2)
    put(15, 24, 20, 20, 2)
    put(25, 29, 20, 25, 2)
    put(3, 14, 20, 20, 3)
    put(3, 21, 21, 21, 3)
    put(3, 14, 22, 25, 3)
    put(3, 13, 26, 26, 3)
    put(0, 9, 33, 38, 4)
    for x in range(3, 13):
        water[20 * W + x] = WATER_RIVER
        river[20 * W + x] = 6
    return W, H, owner, water, river


def check_rg8(c):
    cells = {1: 500, 2: 120, 3: 90, 4: 60}
    pairs = {(1, 2): [30, 30], (1, 3): [12, 2], (2, 3): [8, 8]}
    log = []
    final, cl, adj = merge_graph(cells, pairs, 100, log)
    c.eq("RG8 log", log, [(3, 2)])
    c.eq("RG8 result", sorted((r, cl[r]) for r in cl if cl[r] > 0), [(1, 500), (2, 210), (4, 60)])
    c.eq("RG8 n(1,2)", adj[1][2], [42, 32])
    c.eq("RG8 final", final, {1: 1, 2: 2, 3: 2, 4: 4})
    W, H, owner, water, river = merge_example_grid()
    t = shipped_tuning()
    border = border_river_mask(water, river, t)
    cnt = pair_counts(W, H, owner, water, border)
    c.eq("RG8 grid pairs", cnt, {(1, 2): [30, 30], (1, 3): [12, 2], (2, 3): [8, 8]})
    c.eq("RG8 grid cells", [sum(1 for o in owner if o == r) for r in (1, 2, 3, 4)], [500, 120, 90, 60])
    m = merge(W, H, owner, water, border, 4, 100)
    c.eq("RG8 grid merge", [sum(1 for o in m if o == r) for r in (1, 2, 3, 4)], [500, 210, 0, 60])
    c.eq("RG8 grid recount", merge_recount(W, H, owner, water, border, 4, 100), m)
    c.eq("RG8 min_cells of 750000", min_cells_of(shipped_tuning()), 2930)


def check_rg11(c):
    t = shipped_tuning()
    rc = er.round_int(t["name_central_frac"] * 1024)
    c.eq("RG11 frac raw", t["name_central_frac"], 515396076)
    c.eq("RG11 r_c", rc, 123)
    noun = {code: nn for code, _, _, nn in shipped_defs()}
    own = {7, 9, 6}  # alpine, heath, highland_moor
    entries = [(520, 470, 4, False, False, False), (800, 500, 3, False, False, False),
               (200, 200, 7, False, False, False), (510, 900, 5, False, False, False),
               (790, 560, 3, False, False, False), (500, 100, 3, True, False, False),
               # r2: rules b and c.
               (700, 300, 3, False, 2 * 120 >= 200, False), (210, 700, 3, False, False, True),
               (230, 720, 7, False, False, True), (230, 720, 3, False, True, True)]
    c.eq("RG11 names", name_list(entries, 500, 480, rc, t["names"], noun, lake_noun(shipped_defs()), own),
         ["Central Downs", "East Wood", "Northwest Peaks", "South Marsh", "East Wood II", "North Isle",
          "Northeast Mere", "Southwest Fells", "Southwest Peaks", "Southwest Mere"])
    c.eq("RG11 sector of (210, 700)", NAME_FIELDS[sector(210 - 500, 700 - 480, rc)], "southwest")
    c.eq("RG11 isle before mere and upland",
         name_list([(500, 100, 3, True, True, True)], 500, 480, rc, t["names"], noun, "Mere", own), ["North Isle"])
    c.eq("RG11 upland moor and heath keep their noun",
         name_list([(800, 480, 6, False, False, True), (800, 480, 9, False, False, True),
                    (800, 480, 4, False, False, True)], 500, 480, rc, t["names"], noun, "Mere", own),
         ["East Moor", "East Heath", "East Fells"])
    c.eq("RG11 upland_own_noun empty: alpine upland is Fells",
         name_list([(800, 480, 7, False, False, True)], 500, 480, rc, t["names"], noun, "Mere", set()),
         ["East Fells"])
    c.eq("RG11 lake noun", lake_noun(shipped_defs()), "Mere")
    secs = [NAME_FIELDS[sector(dx, dy, 0)] for dx, dy in ((10, 4), (10, 5), (-3, -7), (0, -9), (4, 10), (5, 12))]
    c.eq("RG11 sectors", secs, ["east", "southeast", "northwest", "north", "south", "southeast"])
    c.eq("RG11 roman", [roman(k) for k in (1, 2, 4, 9, 14, 40, 90, 400, 1994, 4000)],
         ["I", "II", "IV", "IX", "XIV", "XL", "XC", "CD", "MCMXCIV", "MMMM"])
    # Every sector boundary: 29 ay <= 12 ax.
    c.eq("RG11 (29, 12)", NAME_FIELDS[sector(29, 12, 0)], "east")
    c.eq("RG11 (29, 13)", NAME_FIELDS[sector(29, 13, 0)], "southeast")
    c.eq("RG11 (-12, 29)", NAME_FIELDS[sector(-12, 29, 0)], "south")
    c.eq("RG11 (-13, -29)", NAME_FIELDS[sector(-13, -29, 0)], "northwest")
    c.eq("RG11 (-29, 1)", NAME_FIELDS[sector(-29, 1, 0)], "west")
    c.eq("RG11 (13, -29)", NAME_FIELDS[sector(13, -29, 0)], "northeast")
    c.eq("RG11 (-13, 29)", NAME_FIELDS[sector(-13, 29, 0)], "southwest")
    c.eq("RG11 d2 = rc2 is not central", NAME_FIELDS[sector(3, 4, 5)], "northeast" if False else
         NAME_FIELDS[sector(3, 4, 0)])
    c.eq("RG11 d2 < rc2 central", NAME_FIELDS[sector(3, 4, 6)], "central")


EXAMPLE_42_1024 = os.path.join(os.path.expanduser("~"), ".cache", "the-unmapped", "out", "p3-06-tests", "inputs",
                               "example-42-1024.db")

# P1-06 14.4: 1024 seed 42, every region (id, new name, land, traits, how).
TABLE_14_4 = """\
1|The Scarred Downs|downs|coast|t0
2|The Sour Plain|downs|coast|t0
3|The Rises of Hares|downs||t0
4|Warlord's Rises|downs|fertile|t0
5|Northwest Steppe|steppe|coast,high,ore,barren|plain
6|North Downs|downs|river,fertile|plain
7|The Windswept Wolds|downs|fertile|t0
8|Northwest Downs|downs|ore|plain
9|Northwest Heath|heath|high,barren|plain
10|Knelldown|downs|river,fertile|t0
11|Beggar's Plain|downs|fertile|t0
12|Witchgate Barrows|downs|river,fertile|t0
13|North Heath|heath|high,barren|plain
14|Northeast Wood|wood|river|plain
15|Northwest Peaks|peaks|high,ore,barren|plain
16|Steward's Vale|downs|river|t0
17|Wrecker's Reach|wood|coast|t0
18|Northeast Downs|downs|river,fertile|plain
19|The Shaw of Widows|wood||t0
20|West Peaks|peaks|lake,high|plain
21|Central Heath|heath|high,ore,barren|plain
22|West Wood|wood||plain
23|Central Wood|wood|lake,high,ore|plain
24|Drover's Sorrow|wood|river|t0
25|Central Downs|downs||plain
26|East Wood|wood|river|plain
27|Sprayridge Thicket|wood|coast,river|t0
28|East Downs|downs|river|plain
29|Dawthorn Woods|wood|ore,barren|t0
30|Vetchdown|downs|river,lake,fertile|t0
31|West Downs|downs|lake,fertile|plain
32|South Downs|downs|high,ore|plain
33|The Sorrowing Shaw|wood||t0
34|Southeast Wood|wood||plain
35|The Wood of Stags|wood||t0
36|Southwest Steppe|steppe||plain
37|The Brake of the Forgotten|wood||t0
38|Southwest Downs|downs|high|plain
39|Harrowholt|wood|coast|t0
40|Prince's Vow|wood||t1
41|Larkfell|downs|high,ore,pass,barren|t0
42|Stranger's Oath|wood||t0
43|Southwest Wood|wood|coast|plain
44|South Wood|wood|high,pass|plain
45|Badgerhorn Wood|wood||t0
46|The Tangle of Yews|wood||t0
47|Thistlehorn Shaw|wood||t0
48|South Fells|fells|high,barren|plain
49|The Gnarled Forest|wood|coast,barren|t0
50|Southeast Isle|isle|coast|plain"""

# 14.3: distinct valid names per land (no trait; one word; every trait).
SPACE_14_3 = {"wood": (70880, 1332, 127225), "carr": (53285, 976, 92792), "downs": (49230, 914, 88609),
              "marsh": (41451, 801, 71992), "fen": (51444, 984, 90767), "moor": (37905, 878, 68962),
              "heath": (33174, 783, 60050), "peaks": (60610, 1282, 107103), "waste": (38279, 636, 69015),
              "steppe": (38242, 626, 69460), "isle": (33367, 630, 58364), "mere": (43866, 761, 76472),
              "fells": (48932, 1065, 86451)}


# The same with the shipped (r4) forbidden list of 21 entries: computed by
# this reference (not in the spec).
SPACE_R4 = {"carr": (53253, 976, 92752), "downs": (49198, 914, 88569), "fells": (48899, 1064, 86410),
            "fen": (51412, 984, 90727), "heath": (33154, 783, 60025), "isle": (33346, 629, 58338),
            "marsh": (41426, 800, 71961), "mere": (43838, 761, 76437), "moor": (37881, 877, 68932),
            "peaks": (60570, 1282, 107053), "steppe": (38216, 624, 69428), "waste": (38255, 636, 68985),
            "wood": (70835, 1331, 127169)}


def land_space(land, common, trait_defs, pw):
    """Every distinct valid name a land can make over its forms and every
    pool of non-zero weight (14.3's enumeration): (all, one-word)."""
    import itertools
    names_all = set()
    one = set()
    for fm in land["forms"]:
        toks = parse_form(fm["form"])
        lists = []
        for kind, s in toks:
            if kind == "slot":
                P, w = pools_for(SLOT_LIST[s], land, common, trait_defs, pw)
                vals = []
                for i in range(3):
                    if w[i] > 0:
                        vals.extend(P[i])
                lists.append(sorted(set(vals)))
        for combo in itertools.product(*lists):
            ok, _ = name_valid(toks, list(combo), common)
            if ok:
                it = iter(combo)
                nm = "".join(v if k == "lit" else next(it) for k, v in toks)
                names_all.add(nm)
                if " " not in nm:
                    one.add(nm)
    return len(names_all), len(one)


def check_rg11_r3(c, quick=False):
    """P1-06 5.1 (r3) draw examples, 14.4's table, 14.3's space sizes,
    the validity examples, check 4a and the loader checks on the shipped
    placename files."""
    t = shipped_tuning()
    pn = placename_data()
    key = k_name(42, 1024)
    c.eq("RG11 r3 stream_key(42, geo.regions.name)", cr.stream_key(42, STREAM_NAME), 0x224A148B98263929)
    c.eq("RG11 r3 K_name(42, 1024)", key, 0xE1A9F22A266E243A)
    uw = {(1, 0, 0): 0x6859A960857622CB, (1, 0, 1): 0x7EE3FBE2B3DAA4E4, (1, 0, 2): 0xA731588330C4201D,
          (1, 0, 3): 0xE72102C4301D4A38, (1, 0, 4): 0x26E0EFE6AE26CED5,
          (10, 0, 0): 0x42002C340A52E948, (10, 0, 1): 0x2056F9F4076F2CC2, (10, 0, 2): 0x80DC08382FB40AEE,
          (10, 0, 3): 0xA85791F9B2BC6F64, (10, 0, 4): 0x672EF6DA06D6A3DA,
          (39, 0, 2): 0x673CC715D61964E8, (39, 0, 4): 0x99255536F43A47E7,
          (40, 0, 0): 0x1DAD0A4F3DA9416D, (40, 0, 1): 0xD767ECABF011F7C6, (40, 0, 2): 0x2574A736E868C8E5,
          (40, 0, 3): 0x21EE6D0295292197, (40, 0, 4): 0xA4FFEA0405079A69,
          (40, 1, 0): 0xF8C4AD91FCA3D645, (40, 1, 1): 0xB4FB47D898CFA306, (40, 1, 2): 0xA07474902FABD7AE,
          (40, 1, 3): 0xDC51F8D891CC3346, (40, 1, 4): 0xF07596C36D36F72F}
    for (r, at, k), v in sorted(uw.items()):
        c.eq("RG11 r3 u(%d) region %d attempt %d" % (k, r, at), cr.hash_at3(key, r, at, k), v)
    pw, common = SHIPPED_POOLS, pn["common"]
    land = {d: pn["defs"][d] for d in pn["defs"]}
    def L(x):
        return dict(land[x], id=x)
    tr = pn["trait"]
    cases = [(1, "downs", ["coast"], 0, (True, "The Scarred Downs"),
              [("form", 1), ("adjective", (6, 3, 2), 0, 159, 103, "Scarred"), ("noun", (0, 1, 0), 1, 8, 1, "Downs")]),
             (10, "downs", ["river", "fertile"], 0, (True, "Knelldown"),
              [("form", 0), ("prefix", (6, 3, 2), 0, 156, 78, "Knell"), ("suffix", (0, 4, 1), 1, 6, 2, "down")]),
             (39, "wood", ["coast"], 0, (True, "Harrowholt"),
              [("form", 0), ("prefix", None, 0, 156, 62, "Harrow"), ("suffix", None, 1, 8, 4, "holt")]),
             (40, "wood", [], 0, (False, "Birchhurst"),
              [("form", 0), ("prefix", (6, 3, 0), 1, 23, 3, "Birch"), ("suffix", (0, 4, 0), 1, 8, 5, "hurst")]),
             (40, "wood", [], 1, (True, "Prince's Vow"),
              [("form", 5), ("possessor", (8, 0, 0), 0, 94, 58, "Prince"), ("place", (8, 0, 0), 0, 52, 48, "Vow")])]
    for rid, ld, trs, at, want, steps in cases:
        trace = []
        ok, nm, rule = name_attempt(key, rid, at, L(ld), common, [tr[x] for x in trs], pw, trace)
        c.eq("RG11 r3 region %d attempt %d name" % (rid, at), (ok, nm), want)
        c.eq("RG11 r3 region %d attempt %d form" % (rid, at), trace[0][2], steps[0][1])
        for got, st in zip(trace[1:], steps[1:]):
            s, wts, pool, size, idx, word = st
            c.eq("RG11 r3 region %d attempt %d slot %s" % (rid, at, s),
                 (got[0], got[3], got[4], got[6], got[7]) + ((got[2],) if wts else ()),
                 (s, pool, size, idx, word) + ((wts,) if wts else ()))
        if rid == 40 and at == 0:
            c.eq("RG11 r3 Birchhurst fails V1", rule, "V1")
    ok, nm, _ = name_attempt(key, 1, 1, L("downs"), common, [tr["coast"]], pw)
    c.eq("RG11 r3 region 1 attempt 1", nm, "Fletcher's Wolds")
    # Validity examples of 5.1.
    def v(form, vals):
        return name_valid(parse_form(form), vals, common)
    for form, vals, rule in ((("{prefix}{suffix}"), ["Birch", "hurst"], "V1"),
                             ("{prefix}{suffix}", ["Bramble", "edge"], "V1"),
                             ("{prefix}{suffix}", ["Hallow", "well"], "V1"),
                             ("{prefix}{suffix}", ["Bramble", "alder"], "V1"),
                             ("{prefix}{suffix}", ["Hemlock", "hollow"], "V1"),
                             ("The {adjective} {noun}", ["Hollow", "Hollow"], "V3"),
                             ("{prefix}{feature} {noun}", ["Fen", "moss", "Moss"], "V3"),
                             ("The {noun} of {object}", ["Wood", "Woods"], "V3"),
                             ("{prefix}{suffix}", ["Black", "wood"], "V5"),
                             ("The {adjective} {noun}", ["Haunted", "Forest"], "V5"),
                             ("{prefix}{feature} {noun}", ["Black", "rock", "Downs"], "V5"),
                             ("{prefix}{suffix}", ["Holly", "wood"], "V5"),
                             ("{prefix}{suffix}", ["Bone", "reach"], "V4"),
                             ("{prefix}{suffix}", ["Dun", "grove"], "V4"),
                             ("{prefix}{suffix}", ["Adder", "bog"], None),
                             ("{prefix}{suffix}", ["Thistle", "horn"], None),
                             ("{prefix}{suffix}", ["Bracken", "stone"], None)):
        c.eq("RG11 r3 validity %s %r" % (form, vals), v(form, vals)[1], rule)
    c.eq("RG11 r3 fallback fen", name_fallback(L("fen"), common, set()), "Adderbog")
    c.eq("RG11 r3 fen suffixes", L("fen")["suffixes"], ["bog", "fen", "mire", "moss", "quag", "sike"])
    c.eq("RG11 r3 forbidden has boner and dung, 21 entries, sorted (r4)",
         ("boner" in common["forbidden"], "dung" in common["forbidden"], len(common["forbidden"]),
          common["forbidden"] == sorted(common["forbidden"])), (True, True, 21, True))
    c.eq("4.1 loader checks pass on the shipped files", placename_problem(pn["defs"], set(br.shipped_definitions())),
         None)
    try:
        check_4a(pn, shipped_defs(), SHIPPED_POOLS)
        c.ok("check 4a passes on the shipped files", True)
    except FatalError as e:
        c.ok("check 4a passes on the shipped files: %s" % e, False)
    # Sizes of 4.1.
    counts = {f: len(common[f]) for f in PN_LISTS}
    c.eq("4.1 common sizes", counts, dict(nouns=0, suffixes=0, prefixes=156, adjectives=159, features=33,
                                          possessors=94, places=52, objects=78))
    # (4.1's table says 79 common objects and 141 distinct objects; the
    # normative files hold 78 and 140: the files rule, the table is off by one.)
    c.eq("4.1 excluded entries", len(common["excluded"]), 90)
    distinct = {f: len(set(x for d in pn["defs"].values() for x in d[f])) for f in PN_LISTS}
    c.eq("4.1 distinct words over all definitions", distinct,
         dict(nouns=87, suffixes=67, prefixes=282, adjectives=222, features=50, possessors=105, places=70,
              objects=140))
    # 14.4 on the 0.16.0 world of 1024 seed 42 (region and border rows of r2).
    if os.path.exists(EXAMPLE_42_1024):
        rows, nm, how, traits, bases, lands = world_names(EXAMPLE_42_1024, pn, t)
        check_stored_bases(EXAMPLE_42_1024, rows, bases)
        got = "\n".join("%d|%s|%s|%s|%s" % (r["id"], n, ld, ",".join(trs), "plain" if hw == "K" else "t" + hw)
                        for r, n, hw, trs, ld in zip(rows, nm, how, traits, lands))
        c.eq("14.4 1024 seed 42, every region", got, TABLE_14_4)
    else:
        sys.stderr.write("note: %s missing; 14.4 not checked\n" % EXAMPLE_42_1024)
    if not quick:
        # 14.3 was measured with r3's 19 forbidden entries (before r4 added
        # boner and dung): reproduced exactly with those two removed; with
        # the shipped 21 the reference's own counts are SPACE_R4.
        r3common = dict(common, forbidden=[f for f in common["forbidden"] if f not in ("boner", "dung")])
        alltr = [tr[x] for x in TRAITS if x in tr]
        for ld in sorted(SPACE_14_3):
            for label, cm, want in (("r3 lists", r3common, SPACE_14_3[ld]), ("r4 lists", common, SPACE_R4[ld])):
                got0 = land_space(L(ld), cm, [], pw)
                gotall = land_space(L(ld), cm, alltr, pw)[0]
                c.eq("14.3 space of %s (%s)" % (ld, label), (got0[0], got0[1], gotall), want)


def check_rg12(c):
    t = shipped_tuning()
    W, H = 4, 3
    region = grid_of(["1 1 2 2", "1 1 2 2", "1 1 2 2"])
    S = grid_of(["10 20 30 10", "10 50 40 10", "10 25 25 10"])
    walk = [1] * 12
    walk[1 * W + 1] = 0
    walk[1 * W + 2] = 0
    water = [WATER_DRY] * 12
    river = [0] * 12
    water[2] = WATER_RIVER
    river[2] = 6
    border = border_river_mask(water, river, t)
    rows = border_rows(W, H, region, S, walk, water, river, border)
    c.eq("RG12 rows", sorted(rows), [(1, 2)])
    r = rows[(1, 2)]
    c.eq("RG12 row", (r["length_m"], r["low_dm"], r["low_x"], r["low_y"], r["walk_pairs"], r["river_pairs"],
                      r["river_class_max"]), (48, 25, 1, 2, 2, 1, 6))
    c.eq("RG12 second method", border_rows_second(W, H, region, S, walk, water, river, border),
         {(1, 2): (48, 25, 1, 2, 2, 1, 6)})
    river4 = list(river)
    river4[2] = 4
    r4 = border_rows(W, H, region, S, walk, water, river4, border_river_mask(water, river4, t))[(1, 2)]
    c.eq("RG12 class 4: river_pairs 0, river_class_max 4", (r4["river_pairs"], r4["river_class_max"]), (0, 4))
    p3 = b"".join(i.to_bytes(4, "little") for i in (1 * W + 0, 0 * W + 1, 1 * W + 2, 1 * W + 3))
    c.eq("RG12 steps", pass_steps(W, path_cells(p3), water), [4, 5, 1, 2, 6, 7])
    p5 = b"".join(i.to_bytes(4, "little") for i in (2 * W + 1, 2 * W + 2))
    p6 = b"".join(i.to_bytes(4, "little") for i in (0, 1, 1))  # inside region 1 only
    n = pass_crossings(W, H, region, water, [(3, p3, 0 * W + 1), (5, p5, 2 * W + 1), (6, p6, 1)], rows)
    c.eq("RG12 pass_id, passes", (rows[(1, 2)]["pass_id"], rows[(1, 2)]["passes"]), (3, 2))
    c.eq("RG12 attached passes", n, 2)
    key, found = pass_attachment(W, region, water, p3, 0 * W + 1)
    c.eq("RG12 pass 3 crossing: step 1, d 0", [(d, j) for d, j, _, _ in found], [(0, 1)])
    # Attachment examples of 5.1 (r2): a 9 x 1 strip, regions of three cells.
    strip = [1, 1, 1, 2, 2, 2, 3, 3, 3]
    sw = [WATER_DRY] * 9
    path9 = b"".join(i.to_bytes(4, "little") for i in range(9))
    for col, want, ds in ((5, (2, 3), [(2, 2), (0, 5)]), (4, (1, 2), [(1, 2), (1, 5)])):
        key, found = pass_attachment(9, strip, sw, path9, col)
        c.eq("RG12 attachment col %d" % col, key, want)
        c.eq("RG12 attachment col %d (d, j)" % col, [(d, j) for d, j, _, _ in found], ds)
    key, found = pass_attachment(9, [1] * 9, sw, path9, 1)
    c.eq("RG12 attachment: no crossing", (key, found), (None, []))
    rows9 = border_rows(9, 1, strip, [0] * 9, [0] * 9, sw, [0] * 9, [0] * 9)
    c.eq("RG12 attachment rows", pass_crossings(9, 1, strip, sw, [(1, path9, 5), (2, path9, 4), (3, path9, 6)], rows9), 3)
    c.eq("RG12 attachment rows' columns", sorted((k, r["pass_id"], r["passes"]) for k, r in rows9.items()),
         [((1, 2), 2, 1), ((2, 3), 1, 2)])
    # The col occurring twice: its first position. Path 0..8 then back to 4:
    # q = 4 (first), so the crossing at j = 2 (d 1) beats the back crossing.
    back = b"".join(i.to_bytes(4, "little") for i in list(range(9)) + [7, 6, 5, 4])
    c.eq("RG12 col twice", pass_attachment(9, strip, sw, back, 4)[0], (1, 2))
    # The corner rule's second choice and a diagonal step with no land corner.
    water2 = [WATER_DRY] * 12
    water2[0 * W + 2] = WATER_OCEAN  # (2, 0) ocean: the step (1,0)->(2,1) takes (1, 1)
    c.eq("RG12 second corner", pass_steps(W, path_cells(p3), water2), [4, 5, 1, 5, 6, 7])
    water3 = [WATER_DRY] * 12
    water3[0 * W + 2] = WATER_OCEAN
    water3[1 * W + 1] = WATER_OCEAN
    water3[0 * W + 0] = WATER_OCEAN
    c.eq("RG12 no corner", pass_steps(W, path_cells(p3), water3), [4, 1, 6, 7])
    # Composite example (P0-04 amendment 18 (e)).
    c.eq("composite regions", sorted(composite_black(3, 2, grid_of(["1 1 2", "1 2 2"]))), [(0, 1), (1, 0)])
    c.eq("composite: ocean neighbour is no border", sorted(composite_black(3, 1, [1, 0, 2])), [])
    # ids palette of P1-00 10.2.
    hh = cr.mix64(1)
    c.eq("ids 1", ids_colour(1), tuple(64 + (((hh >> (8 * j)) & 0xFF) * 191) // 255 for j in range(3)))
    c.eq("ids 0", ids_colour(0), (0, 0, 0))


def check_words(c):
    for s in ("North", "Isle", "A", "A" + "b" * 31):
        c.ok("word %r" % s, word_ok(s))
    for s in ("north", "North West", "North-West", "Nörth", "NORTH", "North2", "", "A" + "b" * 32):
        c.ok("not a word %r" % s, not word_ok(s))
    for _, _, _, nn in shipped_defs():
        c.ok("shipped noun %s" % nn, word_ok(nn))
    c.eq("shipped nouns", [nn for _, _, _, nn in shipped_defs()],
         ["Sea", "Mere", "Wood", "Downs", "Marsh", "Moor", "Peaks", "Waste", "Heath", "Steppe", "Carr", "Fen"])
    # The shipped tuning file.
    path = os.path.join(_HERE, "..", "..", "..", "game", "data", "worldgen", "regions.toml")
    if os.path.exists(path):
        c.ok("shipped regions.toml present", True)


def check_tuning_errors(c):
    def err(over):
        try:
            check_tuning(tuning_of(over))
        except FatalError as e:
            return str(e)
        return None
    c.eq("tuning ok", err([]), None)
    c.eq("tile check", err([("seed_tile_m", "2401")]),
         "regions: tuning: seed_tile_m must be at most twice seed_spacing_m")
    c.eq("tile at 2400", err([("seed_tile_m", "2400")]), None)
    for f, k, lo, hi, _ in TUNING:
        if k == "int":
            c.eq("range %s low" % f, err([(f, str(int(lo) - 1))]), "regions: tuning: %s outside its schema range" % f)
            if f != "seed_tile_m":
                c.eq("range %s high" % f, err([(f, str(int(hi) + 1))]),
                     "regions: tuning: %s outside its schema range" % f)
    c.eq("range amp", err([("cost_noise_amp", "0.9000000003")]),
         "regions: tuning: cost_noise_amp outside its schema range")


def make_world(rows_e, rows_w, river_cells=()):
    """A hand-made world (elevation dm rows, water rows) with flow_dir
    pointing at the lowest neighbour (a valid G7 input when every land cell
    descends to water)."""
    e = grid_of(rows_e)
    water = grid_of(rows_w)
    h = len(rows_e)
    w = len(e) // h
    n = w * h
    depth = [5 if water[i] in (WATER_LAKE, WATER_OCEAN) else 0 for i in range(n)]
    river = [0] * n
    for (x, y, k) in river_cells:
        river[y * w + x] = k
    fd = [8] * n
    for i in range(n):
        if water[i] == WATER_OCEAN:
            continue
        x, y = i % w, i // w
        best = None
        for d in range(8):
            xx, yy = x + DX[d], y + DY[d]
            if 0 <= xx < w and 0 <= yy < h:
                m = yy * w + xx
                s = e[m] + depth[m]
                if best is None or s < best[0]:
                    best = (s, d)
        fd[i] = best[1]
    L = {"elevation": e, "water": water, "water_depth": depth, "flow_dir": fd, "river": river,
         "biome": [1 if water[i] == WATER_OCEAN else 2 if water[i] == WATER_LAKE else 4 for i in range(n)]}
    for name in RESOURCE_LAYERS:
        L[name] = [0] * n
    L["habitability"] = [0 if water[i] in (WATER_LAKE, WATER_OCEAN) else 10 for i in range(n)]
    L["walk_area"] = [1 if water[i] in (WATER_DRY, WATER_RIVER) else 0 for i in range(n)]
    return w, h, L


def rg6_fixture():
    """RG6 and RG9 fixture (20 x 8, also built by the C++ test): ocean
    border; a mainland x 1..12, y 1..6 (e 20 + x dm, habitability 10, 30 at
    (5, 4)); an islet (14, 2), (14, 3) (e 30; habitability 50 and 200); a
    landmass of a river cell (17, 6) (class 1) and a lake cell (18, 6) (e 5).
    Tiles of 16 cells: tile 0 (x 0..15) has the islet's (14, 3) as its best
    site, tile 1 (x 16..19) no site cell. So RG5 gives one seed, on the
    islet; RG6 adds the mainland's best site (5, 4), then the river cell
    (17, 6), the smallest index of a landmass without a site cell. RG9 gives
    the mainland id 1 (seed number 2), the islet 2 (seed 1), the river islet
    3 (seed 3)."""
    W, H = 20, 8
    e = [[-50] * W for _ in range(H)]
    wat = [[3] * W for _ in range(H)]
    for y in range(1, 7):
        for x in range(1, 13):
            e[y][x] = 20 + x
            wat[y][x] = 0
    for y in (2, 3):
        e[y][14] = 30
        wat[y][14] = 0
    e[6][17] = 5
    wat[6][17] = 1
    e[6][18] = 5
    wat[6][18] = 2
    w, h, L = make_world([" ".join(str(v) for v in r) for r in e], [" ".join(str(v) for v in r) for r in wat],
                         [(17, 6, 1)])
    L["habitability"][4 * w + 5] = 30
    L["habitability"][2 * w + 14] = 50
    L["habitability"][3 * w + 14] = 200
    t = tuning_of([("seed_tile_m", "256"), ("seed_spacing_m", "160"), ("region_min_area_m2", "0")])
    return w, h, L, t


def check_rg6_rg9(c):
    w, h, L, t = rg6_fixture()
    r = regions(w, h, L, [], t, 0, second=True)
    c.eq("RG6 landmasses", r["nlm"], 3)
    c.eq("RG4 candidates", [(s % w, s // w) for s in r["cands"]], [(14, 3)])
    c.eq("RG6 seeds", [(s % w, s // w) for s in r["seeds"]], [(14, 3), (5, 4), (17, 6)])
    c.eq("RG6 greedy", len(r["greedy"]), 1)
    c.eq("RG9 ids", [r["region"][1 * w + 1], r["region"][2 * w + 14], r["region"][6 * w + 17],
                     r["region"][6 * w + 18]], [1, 2, 3, 3])
    c.eq("RG9 seeds of ids", [(row["seed_x"], row["seed_y"]) for row in r["rows"]], [(5, 4), (14, 3), (17, 6)])
    c.eq("RG6 rows", [(row["cells"], row["lake_cells"], row["river_cells"], row["landmass"]) for row in r["rows"]],
         [(72, 0, 0, 1), (2, 0, 0, 2), (2, 1, 1, 3)])
    c.eq("RG11 names", [row["name"] for row in r["rows"]], ["West Downs", "East Isle", "East Isle II"])
    c.eq("RG10 river islet", (r["rows"][2]["river_class_max"], r["rows"][2]["walk_cells"],
                              r["rows"][2]["elev_mean_dm"], r["rows"][2]["biomes"]), (1, 1, 5, [(2, 1), (4, 1)]))
    c.eq("RG10 islet p90", (r["rows"][1]["habitability_mean"], r["rows"][1]["habitability_p90"]), (125, 200))
    c.eq("report fixture", report_items(r)[1:2], [("region_area_km2", "0.00 0.00 0.02")])
    return r


def check_lake_only(c):
    """Ruling of 2026-11-18 (3.2, RG10): the RG6 fixture with a one-cell
    lake islet at (17, 1): a region with no dry cell."""
    w, h, L, t = rg6_fixture()
    i = 1 * w + 17
    L["elevation"][i] = 5
    L["water"][i] = WATER_LAKE
    L["water_depth"][i] = 5
    L["biome"][i] = 2
    L["habitability"][i] = 0
    L["walk_area"][i] = 0
    L["flow_dir"][i] = 8
    r = regions(w, h, L, [], t, 0, second=True)
    c.eq("lake-only seeds", [(s % w, s // w) for s in r["seeds"]], [(14, 3), (5, 4), (17, 1), (17, 6)])
    row = [x for x in r["rows"] if (x["seed_x"], x["seed_y"]) == (17, 1)][0]
    c.eq("lake-only row", (row["id"], row["cells"], row["lake_cells"], row["biome"], row["walk_area"], row["walk_cells"],
                           row["elev_mean_dm"], row["elev_max_dm"], row["habitability_mean"], row["habitability_p90"],
                           row["fresh_water_mean"], row["ore_rich_cells"]), (2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0))
    c.eq("lake-only names", [x["name"] for x in r["rows"]], ["West Downs", "East Isle", "East Isle II", "East Isle III"])
    check_invariants(c, "lake-only", r)
    return r


def check_mere(c):
    """RG11 3 b (r2) on the RG6 fixture: the mainland's east half (x 7..12)
    a lake, so its one region is exactly half lake: Mere."""
    w, h, L, t = rg6_fixture()
    for y in range(1, 7):
        for x in range(7, 13):
            i = y * w + x
            L["water"][i] = WATER_LAKE
            L["water_depth"][i] = 5
            L["biome"][i] = 2
            L["habitability"][i] = 0
            L["walk_area"][i] = 0
    r = regions(w, h, L, [], t, 0, second=True)
    row = r["rows"][0]
    c.eq("mere row", (row["cells"], row["lake_cells"], row["walk_cells"], row["biome"]), (72, 36, 36, 4))
    c.eq("mere names", [x["name"] for x in r["rows"]], ["West Mere", "East Isle", "East Isle II"])
    # One lake cell fewer: Downs.
    i = 6 * w + 12
    L["water"][i] = WATER_DRY
    L["water_depth"][i] = 0
    L["biome"][i] = 4
    L["habitability"][i] = 10
    L["walk_area"][i] = 1
    r = regions(w, h, L, [], t, 0)
    c.eq("not mere", r["rows"][0]["name"], "West Downs")
    # Upland (no walkable cell) and half lake: Mere before Fells.
    w, h, L, t = rg6_fixture()
    for i in range(w * h):
        L["walk_area"][i] = 0
    r = regions(w, h, L, [], t, 0)
    c.eq("upland grassland is Fells", r["rows"][0]["name"], "West Fells")


def check_preconditions(c):
    w, h, L = make_world(["-5 -5 -5 -5", "-5 10 12 -5", "-5 14 16 -5", "-5 -5 -5 -5"],
                         ["3 3 3 3", "3 0 0 3", "3 0 0 3", "3 3 3 3"])
    t = shipped_tuning()

    def err(L2, passes=(), defs=None):
        try:
            regions(w, h, L2, list(passes), t, 0, defs)
        except FatalError as e:
            return str(e)
        return None
    c.eq("precondition ok", err(L), None)
    L2 = dict(L)
    del L2["walk_area"]
    c.eq("missing walk_area", err(L2), PRE + "missing layer 'walk_area'")
    L2 = dict(L)
    L2["biome"] = list(L["biome"])
    L2["biome"][5] = 13
    c.eq("biome no definition", err(L2), PRE + "biome value 13 at (1, 1) has no definition")
    c.eq("pass of 6 bytes", err(L, [(4, b"\x05\x00\x00\x00\x06\x00", 5)]), PRE + "pass 4: path is not a chain of cells")
    jump = b"".join(i.to_bytes(4, "little") for i in (5, 7))
    c.eq("pass jump", err(L, [(2, jump, 5)]), PRE + "pass 2: path is not a chain of cells")
    c.eq("pass empty", err(L, [(1, b"", 5)]), PRE + "pass 1: path is not a chain of cells")
    big = (16).to_bytes(4, "little")
    c.eq("pass index", err(L, [(1, big, 5)]), PRE + "pass 1: path is not a chain of cells")
    wrap = b"".join(i.to_bytes(4, "little") for i in (7, 8))  # (3, 1) -> (0, 2): not neighbours
    c.eq("pass wrap", err(L, [(1, wrap, 7)]), PRE + "pass 1: path is not a chain of cells")
    rpt = b"".join(i.to_bytes(4, "little") for i in (5, 5, 6, 10))
    c.eq("pass repeat ok", err(L, [(1, rpt, 6)]), None)
    c.eq("col not on path", err(L, [(3, rpt, 9)]), PRE + "pass 3: col cell is not on its path")
    c.eq("col outside grid", err(L, [(3, rpt, 16)]), PRE + "pass 3: col cell is not on its path")
    c.eq("chain checked before col", err(L, [(3, jump, 9)]), PRE + "pass 3: path is not a chain of cells")
    c.eq("all chains before any col", err(L, [(1, rpt, 9), (2, jump, 5)]), PRE + "pass 2: path is not a chain of cells")
    L2 = dict(L)
    L2["water"] = list(L["water"])
    L2["water"][6] = 4
    c.eq("water value", err(L2), PRE + "water value 4 at (2, 1)")
    L2 = dict(L)
    L2["river"] = list(L["river"])
    L2["river"][9] = 16
    c.eq("river value", err(L2), PRE + "river value 16 at (1, 2)")
    L2 = dict(L)
    L2["fertility"] = L["fertility"][:-1]
    c.eq("length", err(L2), PRE + "fertility length is not width * height")
    defs = [d if d[0] != 3 else (3, "temperate_forest", False, "Wet Wood") for d in shipped_defs()]
    c.eq("noun", err(L, (), defs), "regions: biome 'temperate_forest' has a region_noun that is not one word")
    L2 = dict(L)
    L2["flow_dir"] = list(L["flow_dir"])
    L2["flow_dir"][5] = 0
    L2["flow_dir"][6] = 4  # (1,1) <-> (2,1): a cycle that never reaches water
    e = err(L2)
    c.ok("G7 prefix: %s" % e, e is not None and e.startswith(PRE + "G7: "))


def check_invariants(c, label, r):
    w, h, L, t = r["w"], r["h"], r["L"], r["t"]
    n = w * h
    water, region = L["water"], r["region"]
    rows = r["rows"]
    R = len(rows)
    # 1.
    c.ok("%s inv1" % label, all((region[i] > 0) == (water[i] != WATER_OCEAN) for i in range(n)))
    # 2.
    first = {}
    for i in range(n):
        if region[i] and region[i] not in first:
            first[region[i]] = i
    c.eq("%s inv2 ids" % label, sorted(first), list(range(1, R + 1)))
    c.ok("%s inv2 order" % label, all(first[k] < first[k + 1] for k in range(1, R)))
    # 3.
    cells = {}
    for i in range(n):
        if region[i]:
            cells.setdefault(region[i], []).append(i)
    lm = r["landmass"]
    for row in rows:
        k = row["id"]
        cs = cells[k]
        s = row["seed_y"] * w + row["seed_x"]
        has_site = any(water[i] == WATER_DRY for i in range(n) if lm[i] == row["landmass"])
        ok = region[s] == k and (water[s] == WATER_DRY or not has_site) and all(lm[i] == row["landmass"] for i in cs)
        seen = {cs[0]}
        q = deque([cs[0]])
        while q:
            x = q.popleft()
            for d in range(8):
                xx, yy = x % w + DX[d], x // w + DY[d]
                if 0 <= xx < w and 0 <= yy < h:
                    m = yy * w + xx
                    if region[m] == k and m not in seen:
                        seen.add(m)
                        q.append(m)
        ok = ok and len(seen) == len(cs)
        if not ok:
            c.ok("%s inv3 region %d" % (label, k), False)
    # 4.
    mc = min_cells_of(t)
    nbr = set()
    for b in r["borders"]:
        nbr.add(b["a"])
        nbr.add(b["b"])
    c.ok("%s inv4" % label, all(len(cells[k]) >= mc or k not in nbr for k in cells))
    # 5 (an independent recount of some columns per region; borders by the
    # second enumeration in regions(second=True)).
    for row in rows:
        cs = cells[row["id"]]
        dry = [i for i in cs if water[i] in (WATER_DRY, WATER_RIVER)]
        xs = [i % w for i in cs]
        ys = [i // w for i in cs]
        want = (min(xs), min(ys), max(xs), max(ys), len(cs), sum(1 for i in cs if water[i] == WATER_LAKE),
                er.rha(sum(L["elevation"][i] for i in dry), len(dry)) if dry else 0,
                max(L["elevation"][i] for i in dry) if dry else 0,
                sorted(L["habitability"][i] for i in dry)[min(len(dry) - 1, 90 * len(dry) // 100)] if dry else 0)
        got = (row["x_min"], row["y_min"], row["x_max"], row["y_max"], row["cells"], row["lake_cells"],
               row["elev_mean_dm"], row["elev_max_dm"], row["habitability_p90"])
        if want != got:
            c.eq("%s inv5 region %d" % (label, row["id"]), got, want)
        if sum(cnt for rid, _, cnt in r["biome_rows"] if rid == row["id"]) != row["cells"]:
            c.ok("%s inv5 biome rows of %d" % (label, row["id"]), False)
    c.ok("%s inv5 border keys" % label, all(b["a"] < b["b"] for b in r["borders"]))
    # 8 (r2): each pass attached to at most one row, to one exactly when its
    # path crosses a border.
    total = sum(b["passes"] for b in r["borders"])
    c.ok("%s inv8 sum" % label, total <= len(r["passes"]))
    crossing = sum(1 for _, path, col in r["passes"] if pass_attachment(w, region, water, path, col)[1])
    c.eq("%s inv8 attached" % label, total, crossing)
    # 6.
    names_ = [row["name"] for row in rows]
    c.eq("%s inv6 unique" % label, len(set(names_)), len(names_))
    nouns = set(nn for _, _, _, nn in shipped_defs()) | {t["names"][9], t["names"][10]}
    for nm in names_:
        p = nm.split(" ")
        ok = (len(p) in (2, 3) and p[0] in t["names"][:9] and p[1] in nouns and
              (len(p) == 2 or (p[2] != "I" and any(roman(k) == p[2] for k in range(2, 400)))))
        if not ok:
            c.ok("%s inv6 name %r" % (label, nm), False)


def check_designed(c, quick):
    for name, base, overrides, psrc, _ in GOLDEN_CASES:
        _, fx, n, _, _ = resources_case(base)
        if fx not in ("designed", "terrace") and (quick or n > 128):
            continue
        _, r = case_result(name, base, overrides, psrc, second=n <= 64)
        check_invariants(c, name, r)
        sys.stderr.write("check: %s %s\n" % (name, coverage(r)))


def check_all(quick):
    c = Checker()
    check_rg3(c)
    check_rg7(c)
    check_rg5(c)
    check_rg8(c)
    check_rg11(c)
    check_rg12(c)
    check_words(c)
    check_tuning_errors(c)
    check_preconditions(c)
    check_rg6_rg9(c)
    check_lake_only(c)
    check_mere(c)
    check_designed(c, quick)
    sys.stderr.write("%d checks passed, %d failed\n" % (c.passes, c.fails))
    return 1 if c.fails else 0


# ---------------------------------------------------------------------------
# Real worlds (statistics of section 9)
# ---------------------------------------------------------------------------

def world_report(path):
    n, L, rows = db_world(path)
    con = sqlite3.connect("file:%s?mode=ro" % path, uri=True)
    try:
        seed = int(con.execute("SELECT value FROM meta WHERE key = 'seed'").fetchone()[0])
    finally:
        con.close()
    passes = [(x["id"], x["path"], x["col_y"] * n + x["col_x"]) for x in rows]
    r = regions(n, n, L, passes, shipped_tuning(), seed)
    rep = report_items(r)
    river_cells = [i for i in range(n * n) if r["border"][i]]
    on_border = 0
    reg = r["region"]
    for i in river_cells:
        x, y = i % n, i // n
        if any(0 <= x + DX[d] < n and 0 <= y + DY[d] < n and reg[(y + DY[d]) * n + x + DX[d]] not in (0, reg[i])
               for d in range(8)):
            on_border += 1
    return n, seed, rep, r, (on_border, len(river_cells))


def db_meta(con):
    return {k: v for k, v in con.execute("SELECT key, value FROM meta")}


def strip_numeral(name):
    p = name.split(" ")
    if len(p) == 3 and p[2] and all(ch in "IVXLCDM" for ch in p[2]):
        return " ".join(p[:2])
    return name


def world_names(path, pn=None, t=None):
    """RG11 r3 on a stored world (any generator from 0.11.0, whose region
    and border rows r3 leaves unchanged): the world seed from meta, N the
    width of layer `water`, the land centroid from that layer, the region
    and border rows from the tables. Returns (rows, names, how, traits,
    bases); the stored names (r2, with numerals) are checked against the
    recomputed base names."""
    t = shipped_tuning() if t is None else t
    pn = placename_data() if pn is None else pn
    defs = shipped_defs()
    con = sqlite3.connect("file:%s?mode=ro" % path, uri=True)
    try:
        meta = db_meta(con)
        seed = int(meta["seed"])
        blob = con.execute("SELECT data FROM layer WHERE name = 'water'").fetchone()[0]
        tcode, w, h, water = br.decode_blob(bytes(blob))
        water = list(water)
        cols = [c for c in REGION_COLUMNS]
        rows = [dict(zip(cols, r)) for r in con.execute("SELECT %s FROM region ORDER BY id" % ", ".join(cols))]
        brows = [{"a": a, "b": b, "passes": p} for a, b, p in
                 con.execute("SELECT a, b, passes FROM region_border ORDER BY a, b")]
    finally:
        con.close()
    lm = {}
    for r in rows:
        lm[r["landmass"]] = lm.get(r["landmass"], 0) + r["cells"]
    code_of = {key: c for c, key, _, _ in defs}
    own = set(code_of[k] for k in t["upland_own_noun"])
    cx, cy = centroid(w, h, water)
    rc = er.round_int(t["name_central_frac"] * w)
    bl = base_and_land(base_entries(rows, lm), cx, cy, rc, t["names"], defs, own, lake_noun(defs), pn)
    passed = set()
    for b in brows:
        if b["passes"] > 0:
            passed.add(b["a"])
            passed.add(b["b"])
    nm, how = rg11_r3(rows, bl, passed, seed, w, pn, t)
    nt = t.get("name_traits", SHIPPED_TRAITS)
    traits = [region_traits(r, r["id"] in passed, nt) for r in rows]
    return rows, nm, how, traits, [b for b, _ in bl], [ld["id"] for _, ld in bl]


def check_stored_bases(path, rows, bases):
    """A world of 0.11.0 to 0.16.0 stores the r2 names: base name plus a
    numeral for the k-th repeat; the r3 base names must agree."""
    bad = [(r["id"], r["name"], b) for r, b in zip(rows, bases) if strip_numeral(r["name"]) != b]
    if bad:
        fatal("%s: stored r2 names disagree with the recomputed base names: %r" % (path, bad[:5]))


def rename_db(src, dst):
    """Copy a world file and replace the `name` column of table `region` by
    the r3 names (nothing else changes)."""
    import shutil
    rows, nm, how, _, bases, _ = world_names(src)
    check_stored_bases(src, rows, bases)
    tmp = dst + ".tmp"
    shutil.copyfile(src, tmp)
    con = sqlite3.connect(tmp)
    try:
        for r, n in zip(rows, nm):
            con.execute("UPDATE region SET name = ? WHERE id = ?", (n, r["id"]))
        con.commit()
    finally:
        con.close()
    os.replace(tmp, dst)
    return rows, nm, how


def main(argv):
    if len(argv) >= 4 and argv[1] == "rename-db":
        rows, nm, how = rename_db(argv[2], argv[3])
        drawn = sum(1 for x in how if x != "K")
        print("%s: %d regions, %d keep their base name, %d drawn, fallback %d, highest attempt %s" % (
            os.path.basename(argv[2]), len(rows), len(rows) - drawn, drawn, how.count("F"),
            max([int(x) for x in how if x not in ("K", "F")], default=-1)))
        return 0
    if len(argv) >= 3 and argv[1] == "names":
        for path in argv[2:]:
            rows, nm, how, traits, bases, lands = world_names(path)
            for r, n, hw, tr, b, ld in zip(rows, nm, how, traits, bases, lands):
                print("%d|%s|%s|%s|%s|%s|%s" % (r["id"], r["name"], n, ld, ",".join(tr),
                                               "plain" if hw == "K" else ("t" + hw if hw != "F" else "F"), b))
        return 0
    if len(argv) >= 2 and argv[1] == "check":
        return check_all("--quick" in argv)
    if len(argv) >= 2 and argv[1] == "golden":
        path = os.path.join(_HERE, "..", "golden", "p1-06-regions.txt")
        db_dir = None
        rest = argv[2:]
        if "--db-dir" in rest:
            k = rest.index("--db-dir")
            db_dir = rest[k + 1]
            rest = rest[:k] + rest[k + 2:]
        rest = [a for a in rest if a != "--large"]
        if rest:
            path = rest[0]
        text = golden_text(large="--large" in argv, db_dir=db_dir)
        if path == "-":
            sys.stdout.write(text)
        else:
            with open(path, "w", newline="\n") as f:
                f.write(text)
        return 0
    if len(argv) >= 3 and argv[1] == "case":
        db_dir = argv[4] if len(argv) >= 5 and argv[3] == "--db-dir" else None
        for name, base, overrides, psrc, _ in GOLDEN_CASES + LARGE_CASES:
            if name == argv[2]:
                _, r = case_result(name, base, overrides, psrc, db_dir)
                for item, v in case_items(r):
                    print("%s %s %s" % (name, item, v))
                for row in r["rows"]:
                    print("#", row["id"], row["name"], row["cells"], row["seed_x"], row["seed_y"])
                return 0
        sys.stderr.write("no case %s\n" % argv[2])
        return 2
    if len(argv) >= 3 and argv[1] == "world":
        for path in argv[2:]:
            n, seed, rep, r, (ob, nr) = world_report(path)
            print("%s N=%d seed=%d %s large_river_on_border=%d/%d" % (
                os.path.basename(path), n, seed, " ".join("%s=%s" % (k, v.replace(" ", "_")) for k, v in rep), ob,
                nr))
            sys.stdout.flush()
        return 0
    sys.stderr.write(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
