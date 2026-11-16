#!/usr/bin/env python3
"""Independent exact-integer reference of the P1-02 hydrology stage.

Written by the test writer from docs/specs/p1-02-hydrology.md (approved
2026-11-15) and docs/specs/p1-00-geography-overview.md (G2 to G6, D1 to D10)
only, without reading any implementation of the stage. Steps H1 to H9 are
implemented here from the spec text; the elevation input of a world comes
from p1_elevation_reference.py (the test writer's reference of P1-01), so a
hydrology golden value never depends on the C++ elevation stage.

Python 3 standard library only, arbitrary-precision integers, no floating
point.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p1_hydrology_reference.py check [--quick]
        The worked example of P1-02 5.1, every expected value of 8.1, the
        designed fixtures, cross-checks against independent second methods
        (Dijkstra minimax levels, recursive accumulation, union-find lakes,
        Dijkstra chamfer, a binary-heap flood) and the invariants of section
        6 on the random fixtures and on the golden worlds (a few minutes).

    python3 -B worldgen/tests/reference/p1_hydrology_reference.py case FIXTURE N SEED [AREA]
        Print the per-step values of one world (FIXTURE shipped or small;
        AREA the river_min_area_m2, default 125000, the shipped value).

    python3 -B worldgen/tests/reference/p1_hydrology_reference.py golden [PATH|-] [--large] [--legacy]
        Write the golden vector file (default worldgen/tests/golden/
        p1-02-hydrology.txt); "-" writes it to standard output. --large adds
        the N = 512 and 1024 cases of P1-02 8.4 (about half an hour, almost
        all of it the elevation reference). --legacy writes the 0.4.0 file
        (t = 0 everywhere, no tie item), which must equal the file of
        generator 0.4.0 byte for byte.

    python3 -B worldgen/tests/reference/p1_hydrology_reference.py fixtures
        Print every output grid of the hand-made fixtures (5.1 and 8.1 items
        3 to 6), as the C++ tests hard-code them.

Environment: UNMAPPED_ELEVATION_CACHE=DIR keeps the elevation grids of the
reference worlds in DIR (outside the repository) so repeated runs skip the
elevation reference. The cache holds only the reference's own output.
"""

import hashlib
import heapq
import os
import sys
from array import array
from collections import deque

sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
import core_reference as cr  # noqa: E402

FatalError = cr.FatalError

# D8 codes, P1-00 2.3.
DX = (1, 1, 0, -1, -1, -1, 0, 1)
DY = (0, 1, 1, 1, 0, -1, -1, -1)

INF = 1 << 40
CLASS_CAP = 15
COAST_CAP = 32767
PRE = "hydrology: elevation precondition: "


def fatal(msg):
    raise FatalError(msg)


# ---------------------------------------------------------------------------
# Neighbour tables
# ---------------------------------------------------------------------------

_NB = {}


def nbt(w, h):
    """For each cell, the list of (code, neighbour) in 8-neighbour order."""
    key = (w, h)
    if key not in _NB:
        tab = []
        for i in range(w * h):
            x, y = i % w, i // w
            row = []
            for k in range(8):
                xx, yy = x + DX[k], y + DY[k]
                if 0 <= xx < w and 0 <= yy < h:
                    row.append((k, yy * w + xx))
            tab.append(row)
        _NB[key] = tab
    return _NB[key]


def step(w, h, c, code):
    x, y = c % w + DX[code], c // w + DY[code]
    if 0 <= x < w and 0 <= y < h:
        return y * w + x
    return -1


def bit_length(v):
    return v.bit_length()


def tuning_threshold(area_m2):
    """Section 4: T = ceil(river_min_area_m2 / 256)."""
    if not (256 <= area_m2 <= 1000000000):
        fatal("river_min_area_m2 out of bounds")
    return (area_m2 + 255) // 256


def river_class(a, t):
    """H6: min(15, bit_length(A div T)) for A >= T, else 0."""
    if a < t:
        return 0
    return min(CLASS_CAP, bit_length(a // t))


# ---------------------------------------------------------------------------
# H1 preconditions
# ---------------------------------------------------------------------------

def preconditions(w, h, e):
    if len(e) != w * h:
        fatal(PRE + "layer size")
    for v in e:
        if v == 0:
            fatal(PRE + "a cell is 0")
    ocean = bytearray(1 if v < 0 else 0 for v in e)
    if not any(ocean):
        fatal(PRE + "no ocean")
    # 4-connected flood from the ocean cells on the map edge, seeds in index
    # order, FIFO (D5).
    seen = bytearray(w * h)
    q = deque()
    for i in range(w * h):
        x, y = i % w, i // w
        if ocean[i] and (x == 0 or y == 0 or x == w - 1 or y == h - 1):
            seen[i] = 1
            q.append(i)
    tab = nbt(w, h)
    while q:
        c = q.popleft()
        for k, m in tab[c]:
            if (k & 1) == 0 and ocean[m] and not seen[m]:
                seen[m] = 1
                q.append(m)
    for i in range(w * h):
        if ocean[i] and not seen[i]:
            fatal(PRE + "ocean not connected to the edge")
    return ocean


# ---------------------------------------------------------------------------
# H2: G3 with a bucket queue (and a binary heap, for the queue check)
# ---------------------------------------------------------------------------

def flood_bucket(w, h, keys, ocean):
    """G3 by a bucket queue: FIFO per level (P1-02 section 7)."""
    n = w * h
    L = [0] * n
    parent = [8] * n
    visited = bytearray(n)
    buckets = {}
    levels = []  # min-heap of levels that have a non-empty bucket
    for i in range(n):
        if ocean[i]:
            visited[i] = 1
            buckets.setdefault(0, deque()).append(i)
        elif keys[i] < 1:
            fatal("G3: land key below 1")
    if 0 not in buckets:
        fatal("G3: no ocean")
    heapq.heappush(levels, 0)
    tab = nbt(w, h)
    order = []
    while levels:
        lv = levels[0]
        b = buckets[lv]
        if not b:
            heapq.heappop(levels)
            del buckets[lv]
            continue
        c = b.popleft()
        order.append(c)
        lc = L[c]
        for code, m in tab[c]:
            if visited[m]:
                continue
            visited[m] = 1
            km = keys[m]
            lm = km if km > lc else lc
            L[m] = lm
            parent[m] = (code + 4) & 7
            bm = buckets.get(lm)
            if bm is None:
                bm = deque()
                buckets[lm] = bm
                heapq.heappush(levels, lm)
            bm.append(m)
    return L, parent, order


def flood_heap(w, h, keys, ocean):
    """G3 by a binary heap on (key, seq), the pseudocode of P1-00 7.3."""
    n = w * h
    L = [0] * n
    parent = [8] * n
    visited = bytearray(n)
    q = []
    seq = 0
    for i in range(n):
        if ocean[i]:
            visited[i] = 1
            heapq.heappush(q, (0, seq, i))
            seq += 1
    tab = nbt(w, h)
    order = []
    while q:
        _, _, c = heapq.heappop(q)
        order.append(c)
        for code, m in tab[c]:
            if visited[m]:
                continue
            visited[m] = 1
            L[m] = max(keys[m], L[c])
            parent[m] = (code + 4) % 8
            heapq.heappush(q, (L[m], seq, m))
            seq += 1
    return L, parent, order


# ---------------------------------------------------------------------------
# H3 lakes (G2, 8-connected, key L)
# ---------------------------------------------------------------------------

def lakes_of(w, h, e, L, ocean):
    n = w * h
    is_lake = bytearray(1 if (not ocean[i] and L[i] > e[i]) else 0 for i in range(n))
    label = [0] * n
    tab = nbt(w, h)
    comps = []
    nxt = 0
    for s in range(n):
        if not is_lake[s] or label[s]:
            continue
        nxt += 1
        label[s] = nxt
        q = deque([s])
        cells = []
        while q:
            c = q.popleft()
            cells.append(c)
            for _, m in tab[c]:
                if is_lake[m] and not label[m] and L[m] == L[c]:
                    label[m] = nxt
                    q.append(m)
        comps.append(cells)
    levels = [L[cells[0]] for cells in comps]
    return label, comps, levels


# ---------------------------------------------------------------------------
# H4 receivers: G4, then one outlet per lake
# ---------------------------------------------------------------------------

def receivers_g4(w, h, L, parent, ocean):
    n = w * h
    tab = nbt(w, h)
    rcv = [8] * n
    for c in range(n):
        if ocean[c]:
            continue
        lc = L[c]
        best = -1
        best_s = 0
        for code, m in tab[c]:
            lm = L[m]
            if lm < lc:
                d = lc - lm
                s = (1 if code & 1 else 2) * d * d
                if s > best_s:
                    best_s = s
                    best = code
        rcv[c] = best if best >= 0 else parent[c]
    return rcv


def reroute_lakes(w, h, rcv_g4, parent, order, label, comps):
    n = w * h
    rcv = list(rcv_g4)
    pos = [0] * n
    for p, c in enumerate(order):
        pos[c] = p
    tab = nbt(w, h)
    outlets = []
    for k, cells in enumerate(comps):
        lid = k + 1
        outlet = min(cells, key=lambda c: pos[c])
        outlets.append(outlet)
        rcv[outlet] = parent[outlet]
        seen = {outlet}
        q = deque([outlet])
        while q:
            c = q.popleft()
            for code, m in tab[c]:
                if label[m] == lid and m not in seen:
                    seen.add(m)
                    rcv[m] = (code + 4) % 8
                    q.append(m)
        if len(seen) != len(cells):
            fatal("H4: lake BFS did not reach every cell")
    return rcv, outlets


# ---------------------------------------------------------------------------
# H5 accumulation (Kahn)
# ---------------------------------------------------------------------------

def accumulate_kahn(w, h, rcv, ocean):
    n = w * h
    A = [0] * n
    indeg = [0] * n
    down = [-1] * n
    for c in range(n):
        if ocean[c]:
            continue
        A[c] = 1
        m = step(w, h, c, rcv[c])
        if m < 0:
            fatal("H5: receiver off the map")
        down[c] = m
        if not ocean[m]:
            indeg[m] += 1
    q = deque(c for c in range(n) if not ocean[c] and indeg[c] == 0)
    done = 0
    while q:
        u = q.popleft()
        done += 1
        m = down[u]
        if not ocean[m]:
            A[m] += A[u]
            indeg[m] -= 1
            if indeg[m] == 0:
                q.append(m)
    if done != n - sum(ocean):
        fatal("H5: receivers have a cycle")
    return A


# ---------------------------------------------------------------------------
# H7 chamfer
# ---------------------------------------------------------------------------

def chamfer(w, h, source):
    n = w * h
    d = [0 if source[i] else INF for i in range(n)]
    fwd = ((-1, 0, 5), (-1, -1, 7), (0, -1, 5), (1, -1, 7))
    bwd = ((1, 0, 5), (1, 1, 7), (0, 1, 5), (-1, 1, 7))
    for i in range(n):
        x, y = i % w, i // w
        for dx, dy, wt in fwd:
            xx, yy = x + dx, y + dy
            if 0 <= xx < w and 0 <= yy < h:
                v = d[yy * w + xx] + wt
                if v < d[i]:
                    d[i] = v
    for i in range(n - 1, -1, -1):
        x, y = i % w, i // w
        for dx, dy, wt in bwd:
            xx, yy = x + dx, y + dy
            if 0 <= xx < w and 0 <= yy < h:
                v = d[yy * w + xx] + wt
                if v < d[i]:
                    d[i] = v
    return d


def coast_metres(d):
    return min(COAST_CAP, (32 * d + 5) // 10)


# ---------------------------------------------------------------------------
# The stage
# ---------------------------------------------------------------------------

TIE_SHIFT = 16
TIE_MAX = (1 << TIE_SHIFT) - 1

# flat_noise of P1-02 4 (shipped and fixture): wavelength 400 m, 1 octave,
# gain 0.5 (raw).
FLAT_NOISE = (400, 1, 1 << 31)


def flat_tie(seed, w, h, ocean, rec=FLAT_NOISE):
    """H2 step 1: t per cell, 0 on ocean, min(65535, (nz.raw + 2^32) >> 17) on
    land with nz = fbm_m(K_flat, X, Y, flat_noise) at the cell centre."""
    import p1_elevation_reference as er
    key = cr.stream_key(seed, "geo.hydrology.flat")
    t = [0] * (w * h)
    for i in range(w * h):
        if ocean[i]:
            continue
        x, y = i % w, i // w
        nz = er.fbm_m(key, (16 * x + 8) << 32, (16 * y + 8) << 32, rec)
        if not -(1 << 32) <= nz <= (1 << 32):
            fatal("flat_tie: noise outside [-ONE, ONE]")
        t[i] = min(TIE_MAX, (nz + (1 << 32)) >> 17)
    return t


def random_tie(f, w, h, e):
    """The random tie values of P1-02 8.2 for fixture f: uniform_i64(0, 65535)
    per land cell in index order from Rng(substream(stream_key(0,
    "test.hydro"), 1000 + f)), 0 on ocean."""
    rng = cr.Rng(cr.substream(cr.stream_key(0, b"test.hydro"), 1000 + f))
    return [0 if v < 0 else rng.uniform_i64(0, TIE_MAX) for v in e]


def flood_tie(w, h, e, ocean, tie, queue="heap"):
    """H2 step 2 (P1-02, re-specified 2026-11-16, "variant B"): L[m] =
    max(e[m], L[c]); push key L[m] * 2^16 + t[m]; a cell is visited when it
    is pushed; pop the smallest (key, seq). queue: "heap" (heapq) or
    "sorted" (a list kept sorted with bisect), two independent queues."""
    import bisect
    n = w * h
    L = [0] * n
    parent = [8] * n
    visited = bytearray(n)
    q = []
    seq = 0

    def push(item):
        if queue == "heap":
            heapq.heappush(q, item)
        else:
            bisect.insort(q, item)

    def pop():
        if queue == "heap":
            return heapq.heappop(q)
        return q.pop(0)

    for i in range(n):
        if ocean[i]:
            visited[i] = 1
            push((0, seq, i))
            seq += 1
    if seq == 0:
        fatal("H2: no ocean")
    tab = nbt(w, h)
    order = []
    while q:
        _, _, c = pop()
        order.append(c)
        lc = L[c]
        for code, m in tab[c]:
            if visited[m]:
                continue
            visited[m] = 1
            em = e[m]
            lm = em if em > lc else lc
            L[m] = lm
            parent[m] = (code + 4) & 7
            push(((lm << TIE_SHIFT) + tie[m], seq, m))
            seq += 1
    return L, parent, order


def hydrology(w, h, e, area_m2, tie=None):
    """Every intermediate of H1 to H8, as a dict. tie: the t grid of H2 (None
    or empty: t = 0 everywhere)."""
    T = tuning_threshold(area_m2)
    ocean = preconditions(w, h, e)
    n = w * h
    if not tie:
        tie = [0] * n
    if len(tie) != n:
        fatal("tie: wrong length")
    for i in range(n):
        if ocean[i] and tie[i] != 0:
            fatal("tie: non-zero on ocean")
        if not 0 <= tie[i] <= TIE_MAX:
            fatal("tie: outside [0, 65535]")
    # H2: the decimetre flood ordered by (L * 2^16 + t, seq), for every t
    # (t = 0 included: golden --legacy checks it against the 0.4.0 file, which
    # the bucket-queue G3 flood produced).
    L, parent, order = flood_tie(w, h, e, ocean, tie)
    label, comps, levels = lakes_of(w, h, e, L, ocean)
    # H4 step 1: G4 on the decimetre levels, flood parent fallback.
    rcv_g4 = receivers_g4(w, h, L, parent, ocean)
    rcv, outlets = reroute_lakes(w, h, rcv_g4, parent, order, label, comps)
    A = accumulate_kahn(w, h, rcv, ocean)
    water = [0] * n
    depth = [0] * n
    flow_dir = [8] * n
    accum = [0] * n
    river = [0] * n
    lake = [0] * n
    for c in range(n):
        if ocean[c]:
            water[c] = 3
            depth[c] = -e[c]
            continue
        flow_dir[c] = rcv[c]
        accum[c] = A[c]
        if label[c]:
            water[c] = 2
            depth[c] = L[c] - e[c]
            lake[c] = label[c]
        elif A[c] >= T:
            water[c] = 1
            river[c] = river_class(A[c], T)
    land = bytearray(0 if ocean[i] else 1 for i in range(n))
    dl = chamfer(w, h, ocean)
    do = chamfer(w, h, land)
    coast = [coast_metres(dl[c]) if land[c] else -coast_metres(do[c]) for c in range(n)]
    for v in depth:
        if v > 65535:
            fatal("water_depth above the u16 bound")
    return {
        "w": w, "h": h, "e": list(e), "T": T, "ocean": ocean, "tie": list(tie), "L": L,
        "parent": parent, "order": order,
        "label": label, "comps": comps, "levels": levels, "rcv_g4": rcv_g4, "rcv": rcv, "outlets": outlets,
        "A": A, "dl": dl, "do": do,
        "layers": [("water", 1, water), ("water_depth", 3, depth), ("flow_dir", 1, flow_dir),
                   ("flow_accum", 5, accum), ("river", 1, river), ("lake", 5, lake), ("coast_dist", 4, coast)],
    }


def layer(r, name):
    for nm, _, v in r["layers"]:
        if nm == name:
            return v
    raise KeyError(name)


# ---------------------------------------------------------------------------
# Independent second methods and the invariants of section 6
# ---------------------------------------------------------------------------

def minimax_levels(w, h, keys, ocean):
    """Flood levels by a Dijkstra-style minimax search (no seq, no order)."""
    n = w * h
    L = [None] * n
    q = [(0, i) for i in range(n) if ocean[i]]
    for _, i in q:
        L[i] = 0
    heapq.heapify(q)
    tab = nbt(w, h)
    while q:
        lv, c = heapq.heappop(q)
        if lv != L[c]:
            continue
        for _, m in tab[c]:
            if ocean[m]:
                continue
            v = max(keys[m], lv)
            if L[m] is None or v < L[m]:
                L[m] = v
                heapq.heappush(q, (v, m))
    return L


def chamfer_dijkstra(w, h, source):
    n = w * h
    d = [INF] * n
    q = [(0, i) for i in range(n) if source[i]]
    for _, i in q:
        d[i] = 0
    heapq.heapify(q)
    tab = nbt(w, h)
    while q:
        dc, c = heapq.heappop(q)
        if dc != d[c]:
            continue
        for k, m in tab[c]:
            v = dc + (7 if k & 1 else 5)
            if v < d[m]:
                d[m] = v
                heapq.heappush(q, (v, m))
    return d


def union_find_lakes(w, h, e, L, ocean):
    n = w * h
    par = list(range(n))

    def find(x):
        while par[x] != x:
            par[x] = par[par[x]]
            x = par[x]
        return x

    mask = [not ocean[i] and L[i] > e[i] for i in range(n)]
    tab = nbt(w, h)
    for i in range(n):
        if not mask[i]:
            continue
        for _, m in tab[i]:
            if mask[m] and L[m] == L[i]:
                a, b = find(i), find(m)
                if a != b:
                    par[max(a, b)] = min(a, b)
    lab = [0] * n
    roots = {}
    for i in range(n):
        if mask[i]:
            r = find(i)
            if r not in roots:
                roots[r] = len(roots) + 1
            lab[i] = roots[r]
    return lab


def check_invariants(r, c, label):
    """Invariants 1 to 9 of section 6 on one result, by code that does not
    reuse the stage's steps."""
    w, h = r["w"], r["h"]
    n = w * h
    e = r["e"]
    T = r["T"]
    water = layer(r, "water")
    depth = layer(r, "water_depth")
    fd = layer(r, "flow_dir")
    acc = layer(r, "flow_accum")
    riv = layer(r, "river")
    lk = layer(r, "lake")
    cd = layer(r, "coast_dist")
    ocean = [x < 0 for x in e]
    ok = True

    def bad(msg):
        nonlocal ok
        if ok:
            c.fail("%s: %s" % (label, msg))
        ok = False

    down = [-1] * n
    for i in range(n):
        if ocean[i]:
            if fd[i] != 8:
                bad("inv 1: flow_dir %d on ocean %d" % (fd[i], i))
        else:
            if not 0 <= fd[i] <= 7:
                bad("inv 1: flow_dir %d on land %d" % (fd[i], i))
                continue
            m = step(w, h, i, fd[i])
            if m < 0:
                bad("inv 1: flow_dir off the map at %d" % i)
            down[i] = m
    if not ok:
        return False
    # 2: reach the sea in fewer than N^2 steps (memoized walk).
    state = [0] * n  # 0 unknown, 1 on stack, 2 reaches sea
    for s in range(n):
        if ocean[s] or state[s] == 2:
            continue
        path = []
        x = s
        while not ocean[x] and state[x] == 0:
            state[x] = 1
            path.append(x)
            x = down[x]
        if not ocean[x] and state[x] == 1:
            bad("inv 2: cycle through %d" % x)
            return False
        for p in path:
            state[p] = 2
    # 3: W non-increasing downstream; W = L (minimax).
    Lm = minimax_levels(w, h, e, ocean)
    W = [0 if ocean[i] else e[i] + depth[i] for i in range(n)]
    for i in range(n):
        if ocean[i]:
            continue
        if W[down[i]] > W[i]:
            bad("inv 3: uphill at %d" % i)
        if W[i] != Lm[i]:
            bad("inv 3: W %d != L %d at %d" % (W[i], Lm[i], i))
    # 4: accumulation by recursion over upstream cells.
    ups = [[] for _ in range(n)]
    for i in range(n):
        if not ocean[i]:
            ups[down[i]].append(i)
    A = [0] * n
    sys.setrecursionlimit(max(10000, n + 100))
    done = bytearray(n)
    for s in range(n):
        if ocean[s] or done[s]:
            continue
        stack = [(s, 0)]
        while stack:
            x, k = stack.pop()
            if k < len(ups[x]):
                stack.append((x, k + 1))
                u = ups[x][k]
                if not done[u]:
                    stack.append((u, 0))
            else:
                A[x] = 1 + sum(A[u] for u in ups[x])
                done[x] = 1
    for i in range(n):
        if acc[i] != (0 if ocean[i] else A[i]):
            bad("inv 4: flow_accum at %d" % i)
            break
    # 5: rivers.
    for i in range(n):
        is_r = (not ocean[i]) and lk[i] == 0 and acc[i] >= T
        if (riv[i] > 0) != (water[i] == 1) or (water[i] == 1) != is_r:
            bad("inv 5: river equivalence at %d" % i)
            break
        if is_r:
            if riv[i] != min(15, (acc[i] // T).bit_length()):
                bad("inv 5: class at %d" % i)
            m = down[i]
            if not (water[m] in (1, 2, 3)):
                bad("inv 5: river %d flows onto dry land" % i)
            if water[m] == 1 and riv[m] < riv[i]:
                bad("inv 5: class decreases downstream at %d" % i)
    # 6: lakes.
    uf = union_find_lakes(w, h, e, Lm, ocean)
    if uf != lk:
        bad("inv 6: lake ids differ from union-find components")
    K = max(lk) if n else 0
    exits = [0] * (K + 1)
    levels = [None] * (K + 1)
    for i in range(n):
        k = lk[i]
        if k:
            if depth[i] <= 0:
                bad("inv 6: lake depth")
            if levels[k] is None:
                levels[k] = W[i]
            elif levels[k] != W[i]:
                bad("inv 6: lake %d not flat" % k)
            m = down[i]
            if lk[m] != k:
                exits[k] += 1
                if ocean[m] or e[m] != levels[k]:
                    bad("inv 6: outlet of lake %d does not spill over a rim at the level" % k)
    for k in range(1, K + 1):
        if exits[k] != 1:
            bad("inv 6: lake %d has %d exits" % (k, exits[k]))
    # 7: water and depth.
    for i in range(n):
        if (water[i] == 3) != ocean[i] or (ocean[i] and depth[i] != -e[i]):
            bad("inv 7: ocean at %d" % i)
            break
        if (water[i] == 2) != (lk[i] > 0) or (lk[i] > 0) != ((not ocean[i]) and depth[i] > 0):
            bad("inv 7: lake at %d" % i)
            break
    # 8: ocean 4-connected from the edge on the water layer.
    seen = [False] * n
    q = deque()
    for i in range(n):
        x, y = i % w, i // w
        if water[i] == 3 and (x in (0, w - 1) or y in (0, h - 1)):
            seen[i] = True
            q.append(i)
    while q:
        x = q.popleft()
        for k in (0, 2, 4, 6):
            m = step(w, h, x, k)
            if m >= 0 and water[m] == 3 and not seen[m]:
                seen[m] = True
                q.append(m)
    if any(water[i] == 3 and not seen[i] for i in range(n)):
        bad("inv 8: ocean pocket")
    # 9: coast distance.
    dl = chamfer_dijkstra(w, h, [1 if o else 0 for o in ocean])
    do = chamfer_dijkstra(w, h, [0 if o else 1 for o in ocean])
    for i in range(n):
        want = coast_metres(dl[i]) if not ocean[i] else -coast_metres(do[i])
        if cd[i] != want:
            bad("inv 9: coast_dist at %d" % i)
            break
        if (cd[i] > 0) == ocean[i]:
            bad("inv 9: sign at %d" % i)
            break
    return ok


# ---------------------------------------------------------------------------
# Hashes (P0-04 8)
# ---------------------------------------------------------------------------

_FMT = {1: "B", 3: "H", 4: "h", 5: "I"}


def _pack(fmt, values):
    a = array(fmt, values)
    if sys.byteorder != "little":
        a.byteswap()
    return a.tobytes()


def h_i64(values):
    return hashlib.sha256(_pack("q", values)).hexdigest()


def h_u8(values):
    return hashlib.sha256(bytes(values)).hexdigest()


def h_u32(values):
    return hashlib.sha256(_pack("I", values)).hexdigest()


def layer_hash(w, h, code, values):
    head = w.to_bytes(4, "little") + h.to_bytes(4, "little") + bytes([code])
    return hashlib.sha256(head + _pack(_FMT[code], values)).hexdigest()


# ---------------------------------------------------------------------------
# Worlds: elevation from the elevation reference
# ---------------------------------------------------------------------------

def elevation_of(fixture, n, seed):
    cache = os.environ.get("UNMAPPED_ELEVATION_CACHE")
    path = None
    if cache:
        path = os.path.join(cache, "elev-%s-%d-%d.i16" % (fixture, n, seed))
        if os.path.exists(path):
            a = array("h")
            with open(path, "rb") as f:
                a.frombytes(f.read())
            if sys.byteorder != "little":
                a.byteswap()
            if len(a) == n * n:
                return list(a)
    import p1_elevation_reference as er
    t = er.shipped_tuning() if fixture == "shipped" else er.small_tuning()
    e = er.run_world(seed, n, t)["e_final"]
    if path:
        os.makedirs(cache, exist_ok=True)
        with open(path, "wb") as f:
            f.write(_pack("h", e))
    return e


def random_fixture(f):
    """Random fixture f of P1-02 8.2: a 34 x 34 grid, ocean border e in
    [-50, -1], interior e in [1, 200], drawn in index order from
    Rng(substream(stream_key(0, "test.hydro"), f))."""
    rng = cr.Rng(cr.substream(cr.stream_key(0, b"test.hydro"), f))
    w = h = 34
    e = []
    for i in range(w * h):
        x, y = i % w, i // w
        if x == 0 or y == 0 or x == w - 1 or y == h - 1:
            e.append(rng.uniform_i64(-50, -1))
        else:
            e.append(rng.uniform_i64(1, 200))
    return w, h, e


def h_u16(values):
    return hashlib.sha256(_pack("H", values)).hexdigest()


def world_items(r, elev_layer, legacy=False):
    """The golden items of one result. legacy: the item list of the 0.4.0 file
    (no tie), used to prove that t = 0 reproduces it."""
    w, h = r["w"], r["h"]
    lv = r["levels"]
    head = [("elevation", elev_layer)]
    if not legacy:
        head += [("tie", h_u16(r["tie"]))]
    items = head + [
        ("L", h_i64(r["L"])),
        ("parent", h_u8(r["parent"])),
        ("order", h_u32(r["order"])),
        ("rcv_g4", h_u8(r["rcv_g4"])),
        ("lakes", str(len(lv))),
        ("lake_levels", ",".join(str(x) for x in lv) or "-"),
        ("lake_cells", ",".join(str(len(c)) for c in r["comps"]) or "-"),
        ("outlets", ",".join(str(x) for x in r["outlets"]) or "-"),
        ("max_accum", str(max(r["A"]))),
        ("river_cells", str(sum(1 for v in layer(r, "water") if v == 1))),
        ("max_class", str(max(layer(r, "river")))),
        ("dl", h_i64(r["dl"])),
        ("do", h_i64(r["do"])),
    ]
    for name, code, vals in r["layers"]:
        items.append((name, layer_hash(w, h, code, vals)))
    return items


# ---------------------------------------------------------------------------
# Hand-made fixtures
# ---------------------------------------------------------------------------

def grid(rows):
    h = len(rows)
    w = len(rows[0])
    out = []
    for row in rows:
        assert len(row) == w
        out.extend(row)
    return w, h, out


EX51 = [
    [-5, -5, -5, -5, -5, -5, -5, -5],
    [-5, 60, 70, 80, 70, 60, 50, -5],
    [-5, 50, 20, 22, 60, 40, 30, -5],
    [-5, 45, 21, 23, 55, 35, 25, -5],
    [-5, 40, 30, 28, 40, 30, 20, -5],
    [-5, 35, 33, 31, 25, 20, 10, -5],
    [-5, -5, -5, -5, -5, -5, -5, -5],
]

EX51_L = [
    [0] * 8,
    [0, 60, 70, 80, 70, 60, 50, 0],
    [0, 50, 28, 28, 60, 40, 30, 0],
    [0, 45, 28, 28, 55, 35, 25, 0],
    [0, 40, 30, 28, 40, 30, 20, 0],
    [0, 35, 33, 31, 25, 20, 10, 0],
    [0] * 8,
]

EX51_PARENT = [
    [8] * 8,
    [8, 5, 5, 5, 5, 5, 5, 8],
    [8, 5, 2, 3, 3, 1, 7, 8],
    [8, 5, 1, 2, 3, 1, 7, 8],
    [8, 5, 0, 1, 1, 1, 7, 8],
    [8, 5, 3, 3, 3, 3, 7, 8],
    [8] * 8,
]

EX51_ORDER = [0, 1, 2, 3, 4, 5, 6, 7, 8, 15, 16, 23, 24, 31, 32, 39, 40, 47, 48, 49, 50, 51, 52, 53, 54, 55, 46,
              38, 45, 30, 44, 35, 26, 27, 18, 19, 22, 37, 34, 43, 42, 41, 29, 33, 36, 21, 25, 14, 17, 28, 9, 13, 20,
              10, 12, 11]

EX51_FLOW = [
    [8] * 8,
    [8, 4, 6, 6, 6, 6, 0, 8],
    [8, 4, 2, 3, 4, 1, 0, 8],
    [8, 4, 1, 4, 4, 1, 0, 8],
    [8, 4, 0, 1, 2, 1, 0, 8],
    [8, 2, 2, 2, 2, 2, 0, 8],
    [8] * 8,
]

EX51_ACCUM = [
    [0] * 8,
    [0, 1, 1, 1, 1, 1, 1, 0],
    [0, 1, 1, 2, 1, 1, 1, 0],
    [0, 1, 6, 2, 1, 1, 2, 0],
    [0, 1, 1, 8, 1, 1, 2, 0],
    [0, 1, 1, 1, 10, 1, 2, 0],
    [0] * 8,
]

EX51_WATER = [
    [3] * 8,
    [3, 0, 0, 0, 0, 0, 0, 3],
    [3, 0, 2, 2, 0, 0, 0, 3],
    [3, 0, 2, 2, 0, 0, 0, 3],
    [3, 0, 0, 1, 0, 0, 0, 3],
    [3, 0, 0, 0, 1, 0, 0, 3],
    [3] * 8,
]

EX51_COAST = [
    [-22, -16, -16, -16, -16, -16, -16, -22],
    [-16, 16, 16, 16, 16, 16, 16, -16],
    [-16, 16, 32, 32, 32, 32, 16, -16],
    [-16, 16, 32, 48, 48, 32, 16, -16],
    [-16, 16, 32, 32, 32, 32, 16, -16],
    [-16, 16, 16, 16, 16, 16, 16, -16],
    [-22, -16, -16, -16, -16, -16, -16, -22],
]

# 8.1 item 3: score ties.
TIES = [
    [-1] * 6,
    [-1, 10, 10, 10, 10, 10],
    [-1] * 6,
]

# 8.1 item 4: a plateau.
PLATEAU = [
    [-1] * 7,
    [-1, 10, 10, 10, 10, 10, -1],
    [-1, 10, 10, 10, 10, 10, -1],
    [-1, 10, 10, 10, 10, 10, -1],
    [-1] * 7,
]

# 8.1 item 5 (designed by the test writer): two lakes at different levels,
# the upper one (level 150, rows 2-3) draining over its rim (6, 4) down a
# channel into the lower one (level 80, rows 7-8), which spills over (1, 8)
# into the sea. The upper lake has the smaller index, so it is lake 1 although
# it is flooded later.
B = 200
TERRACE = [
    [-10] * 11,
    [-10, B, B, B, B, B, B, B, B, B, -10],
    [-10, B, B, B, B, 120, 110, 120, B, B, -10],
    [-10, B, B, B, B, 125, 115, 130, B, B, -10],
    [-10, B, B, B, B, B, 150, B, B, B, -10],
    [-10, B, B, B, B, 140, B, B, B, B, -10],
    [-10, B, B, B, B, 120, B, B, B, B, -10],
    [-10, B, 50, 40, 50, B, B, B, B, B, -10],
    [-10, 80, 45, 35, 50, B, B, B, B, B, -10],
    [-10, B, B, B, B, B, B, B, B, B, -10],
    [-10] * 11,
]

# 8.1 item 6 (designed by the test writer): a 2 x 3 basin (level 40) with
# two rim cells at the lake level, (2, 3) on the west and (6, 2) on the east,
# each with a descending step to the sea. G4 alone gives two exits; H4 keeps
# the first-popped lake cell's.
RIMS = [
    [-5] * 9,
    [-5, 100, 100, 100, 100, 100, 100, 100, -5],
    [-5, 100, 100, 20, 25, 20, 40, 30, -5],
    [-5, 30, 40, 22, 21, 24, 100, 100, -5],
    [-5, 100, 100, 100, 100, 100, 100, 100, -5],
    [-5] * 9,
]


def rows_of(w, vals):
    return [vals[i:i + w] for i in range(0, len(vals), w)]


# ---------------------------------------------------------------------------
# check
# ---------------------------------------------------------------------------

class Checker:
    def __init__(self):
        self.fails = 0
        self.passes = 0

    def eq(self, what, got, want):
        if got == want:
            self.passes += 1
        else:
            self.fails += 1
            sys.stderr.write("FAIL %s:\n  got  %r\n  want %r\n" % (what, got, want))

    def ok(self, what, cond):
        self.eq(what, bool(cond), True)

    def fail(self, msg):
        self.fails += 1
        sys.stderr.write("FAIL %s\n" % msg)


def flat(rows):
    return [v for row in rows for v in row]


def expect_fatal(c, what, fn, prefix=PRE):
    try:
        fn()
    except FatalError as ex:
        c.ok(what + " (message %r)" % str(ex), str(ex).startswith(prefix))
        return
    c.fail(what + ": no fatal error")


def check_example(c):
    w, h, e = grid(EX51)
    r = hydrology(w, h, e, 600)
    c.eq("5.1 T", r["T"], 3)
    c.eq("5.1 L", r["L"], flat(EX51_L))
    c.eq("5.1 parent", r["parent"], flat(EX51_PARENT))
    c.eq("5.1 order", r["order"], EX51_ORDER)
    c.eq("5.1 lakes", ([sorted(x) for x in r["comps"]], r["levels"]), ([[18, 19, 26, 27]], [28]))
    c.eq("5.1 lake cells", sorted(r["comps"][0]), [8 * 2 + 2, 8 * 2 + 3, 8 * 3 + 2, 8 * 3 + 3])
    # G4 alone: the lake cells keep their flood parents; two exits.
    g4 = r["rcv_g4"]
    c.eq("5.1 G4 lake cells (2,2),(3,2),(2,3),(3,3)", [g4[18], g4[19], g4[26], g4[27]], [2, 3, 1, 2])
    exits_g4 = [i for i in (18, 19, 26, 27) if step(w, h, i, g4[i]) not in (18, 19, 26, 27)]
    c.eq("5.1 G4 exits", exits_g4, [26, 27])
    c.eq("5.1 outlet", r["outlets"], [26])
    c.eq("5.1 flow_dir", layer(r, "flow_dir"), flat(EX51_FLOW))
    c.eq("5.1 flow_accum", layer(r, "flow_accum"), flat(EX51_ACCUM))
    c.eq("5.1 water", layer(r, "water"), flat(EX51_WATER))
    riv = [0] * 56
    riv[8 * 4 + 3] = 2
    riv[8 * 5 + 4] = 2
    c.eq("5.1 river", layer(r, "river"), riv)
    depth = [5 if v < 0 else 0 for v in e]
    depth[18], depth[19], depth[26], depth[27] = 8, 6, 7, 5
    c.eq("5.1 water_depth", layer(r, "water_depth"), depth)
    lk = [0] * 56
    for i in (18, 19, 26, 27):
        lk[i] = 1
    c.eq("5.1 lake", layer(r, "lake"), lk)
    c.eq("5.1 coast_dist", layer(r, "coast_dist"), flat(EX51_COAST))
    c.eq("5.1 (3,3) chamfer", r["dl"][27], 15)
    c.eq("5.1 (0,0) chamfer", r["do"][0], 7)
    # 8.1 item 2: G3 properties on the example.
    pos = {cell: p for p, cell in enumerate(r["order"])}
    keys_popped = [r["L"][x] for x in r["order"]]
    c.ok("8.1.2 monotone pops", all(keys_popped[i] <= keys_popped[i + 1] for i in range(len(keys_popped) - 1)))
    good = True
    for i in range(w * h):
        if e[i] < 0:
            continue
        p = step(w, h, i, r["parent"][i])
        if pos[p] >= pos[i]:
            good = False
        for _, m in nbt(w, h)[i]:
            if r["L"][m] < r["L"][i] and pos[m] >= pos[i]:
                good = False
    c.ok("8.1.2 parents and lower neighbours precede", good)
    c.ok("5.1 invariants", check_invariants(r, c, "5.1"))
    # Cross-check with the elevation reference's G3 to G5 and chamfer.
    import p1_elevation_reference as er
    ocean = [1 if v < 0 else 0 for v in e]
    L2, p2, o2 = er.g3_flood(w, h, e, ocean)
    c.eq("5.1 G3 = elevation reference g3_flood", (L2, p2, o2), (r["L"], r["parent"], r["order"]))
    c.eq("5.1 G4 = elevation reference g4_receivers", er.g4_receivers(w, h, L2, p2, ocean), r["rcv_g4"])
    c.eq("5.1 chamfer = elevation reference chamfer", er.chamfer(w, h, ocean), r["dl"])


def check_units(c):
    # 8.1 item 3: score ties.
    w, h, e = grid(TIES)
    r = hydrology(w, h, e, 256)
    c.eq("8.1.3 ties flow_dir row 1", layer(r, "flow_dir")[6:12], [8, 2, 2, 2, 2, 2])
    c.ok("8.1.3 invariants", check_invariants(r, c, "ties"))
    # 8.1 item 4: plateau.
    w, h, e = grid(PLATEAU)
    r = hydrology(w, h, e, 256)
    fd = rows_of(w, layer(r, "flow_dir"))
    c.eq("8.1.4 plateau flow_dir rows 1-3", fd[1:4],
         [[8, 4, 6, 6, 6, 0, 8], [8, 4, 5, 5, 5, 0, 8], [8, 2, 2, 2, 2, 0, 8]])
    ac = rows_of(w, layer(r, "flow_accum"))
    c.eq("8.1.4 plateau flow_accum rows 1-3", ac[1:4],
         [[0, 2, 2, 2, 1, 1, 0], [0, 1, 1, 1, 1, 1, 0], [0, 1, 1, 1, 1, 1, 0]])
    c.eq("8.1.4 plateau parents (2..4, 2)", [r["parent"][2 * 7 + x] for x in (2, 3, 4)], [5, 5, 5])
    c.eq("8.1.4 plateau no lake", r["comps"], [])
    c.ok("8.1.4 invariants", check_invariants(r, c, "plateau"))
    # 8.1 item 5: terrace.
    w, h, e = grid(TERRACE)
    r = hydrology(w, h, e, 256 * 4)
    c.eq("8.1.5 terrace lake levels", r["levels"], [150, 80])
    c.eq("8.1.5 terrace lake 1 cells", sorted(r["comps"][0]), sorted([2 * 11 + 5, 2 * 11 + 6, 2 * 11 + 7,
                                                                      3 * 11 + 5, 3 * 11 + 6, 3 * 11 + 7]))
    c.eq("8.1.5 terrace lake 2 cells", sorted(r["comps"][1]), sorted([7 * 11 + 2, 7 * 11 + 3, 7 * 11 + 4,
                                                                      8 * 11 + 2, 8 * 11 + 3, 8 * 11 + 4]))
    c.ok("8.1.5 terrace invariants", check_invariants(r, c, "terrace"))
    # The upper lake's water reaches the lower lake, which reaches the sea.
    fd = layer(r, "flow_dir")
    x = r["outlets"][0]
    path = []
    while e[x] > 0:
        path.append(x)
        x = step(w, h, x, fd[x])
    lk = layer(r, "lake")
    c.ok("8.1.5 upper outlet path enters the lower lake", any(lk[p] == 2 for p in path))
    # 8.1 item 6: rims.
    w, h, e = grid(RIMS)
    r = hydrology(w, h, e, 256)
    lake_cells = r["comps"][0]
    g4_exits = [i for i in sorted(lake_cells) if step(w, h, i, r["rcv_g4"][i]) not in lake_cells]
    c.eq("8.1.6 G4 alone: four exits over both rims", g4_exits, [21, 23, 30, 32])
    fd = layer(r, "flow_dir")
    exits = [i for i in sorted(lake_cells) if step(w, h, i, fd[i]) not in lake_cells]
    pos = {cell: p for p, cell in enumerate(r["order"])}
    c.eq("8.1.6 one exit, the first popped", exits, [min(lake_cells, key=lambda q: pos[q])])
    c.ok("8.1.6 invariants", check_invariants(r, c, "rims"))
    # 8.1 item 7: river classes and T.
    for a, k in ((3, 1), (5, 1), (6, 2), (11, 2), (12, 3), (3 * 2 ** 14, 15), (3 * 2 ** 20, 15), (2, 0)):
        c.eq("8.1.7 class of A = %d" % a, river_class(a, 3), k)
    c.eq("8.1.7 T(256)", tuning_threshold(256), 1)
    c.eq("8.1.7 T(257)", tuning_threshold(257), 2)
    c.eq("8.1.7 T(500000)", tuning_threshold(500000), 1954)
    c.eq("4 T(125000), the shipped value since 2026-11-16", tuning_threshold(125000), 489)
    c.eq("4 T(513)", tuning_threshold(513), 3)
    c.eq("4 T(768)", tuning_threshold(768), 3)
    c.eq("4 T(769)", tuning_threshold(769), 4)
    # H6 class cap and the T derivation on a one-row corridor (e = 1 + x div 2,
    # ocean at x = 0): A at x is W - x.
    for wdt, area in ((4000, 500000), (4000, 499968), (32771, 256), (32771, 512)):
        e = [-1] + [1 + x // 2 for x in range(1, wdt)]
        r = hydrology(wdt, 1, e, area)
        T = tuning_threshold(area)
        c.eq("corridor %d area %d accum" % (wdt, area), layer(r, "flow_accum")[1:], [wdt - x for x in range(1, wdt)])
        c.eq("corridor %d area %d river" % (wdt, area), layer(r, "river")[1:],
             [river_class(wdt - x, T) for x in range(1, wdt)])
    c.eq("H6 cap: A = 32770, T = 1", river_class(32770, 1), 15)
    # 8.1 item 8: chamfer on a 9 x 9 grid with one source cell at (4, 4).
    src = [0] * 81
    src[4 * 9 + 4] = 1
    dl = chamfer(9, 9, src)
    do = chamfer(9, 9, [1 - s for s in src])
    want = {(5, 4): (5, 16), (5, 5): (7, 22), (6, 5): (12, 38), (8, 8): (28, 90), (8, 4): (20, 64),
            (0, 0): (28, 90)}
    for (x, y), (d, m) in sorted(want.items()):
        c.eq("8.1.8 chamfer (%d,%d)" % (x, y), (dl[y * 9 + x], coast_metres(dl[y * 9 + x])), (d, m))
    c.eq("8.1.8 (4,4) ocean", -coast_metres(do[4 * 9 + 4]), -16)
    c.eq("8.1.8 chamfer = Dijkstra", dl, chamfer_dijkstra(9, 9, src))
    # No source at all: INF saturates.
    c.eq("7 chamfer with no source", chamfer(3, 2, [0] * 6), [INF] * 6)
    c.eq("7 INF saturates", coast_metres(INF), 32767)
    # 8.1 item 9: preconditions.
    w, h, e = grid(EX51)
    e0 = list(e)
    e0[8 * 3 + 3] = 0
    expect_fatal(c, "8.1.9 a 0 cell", lambda: hydrology(w, h, e0, 600))
    expect_fatal(c, "8.1.9 no ocean", lambda: hydrology(w, h, [abs(v) for v in e], 600))
    ep = list(e)
    ep[8 * 3 + 3] = -4
    expect_fatal(c, "8.1.9 ocean pocket", lambda: hydrology(w, h, ep, 600))
    # A diagonal-only ocean link is not 4-connected.
    ed = [1] * 16
    ed[0] = -1
    ed[5] = -1
    expect_fatal(c, "8.1.9 diagonal ocean pocket", lambda: hydrology(4, 4, ed, 256))
    # Tuning bounds.
    expect_fatal(c, "4 river_min_area_m2 255", lambda: tuning_threshold(255), "")
    expect_fatal(c, "4 river_min_area_m2 1e9+1", lambda: tuning_threshold(1000000001), "")


LAKE_RELATED = ("water_depth", "lake", "coast_dist")


def same_lakes(c, what, a, b):
    """Invariant 11: equal water_depth, lake, coast_dist, lake and ocean cells,
    and equal decimetre levels."""
    for nm in LAKE_RELATED:
        c.eq("%s: %s" % (what, nm), layer(a, nm), layer(b, nm))
    wa, wb = layer(a, "water"), layer(b, "water")
    c.eq("%s: lake and ocean cells" % what, [v if v >= 2 else 0 for v in wa], [v if v >= 2 else 0 for v in wb])
    c.eq("%s: L" % what, a["L"], b["L"])
    c.eq("%s: lake levels" % what, a["levels"], b["levels"])


def check_random(c, count=200):
    for f in range(count):
        w, h, e = random_fixture(f)
        tie = random_tie(f, w, h, e)
        for area in (256 * 8, 256):
            r = hydrology(w, h, e, area)
            ocean = r["ocean"]
            L2, p2, o2 = flood_heap(w, h, e, ocean)
            c.eq("8.3 queue independence fixture %d" % f, (L2, p2, o2), (r["L"], r["parent"], r["order"]))
            rt = hydrology(w, h, e, area, tie)
            if area == 256 * 8:
                l2, p3, o3 = flood_tie(w, h, e, ocean, tie, queue="sorted")
                c.eq("8.3 queue independence with t (heap vs sorted list), fixture %d" % f, (l2, p3, o3),
                     (rt["L"], rt["parent"], rt["order"]))
                l0, p0, o0 = flood_tie(w, h, e, ocean, [0] * (w * h))
                c.eq("8.3 t = 0: H2 flood = G3 bucket queue, fixture %d" % f, (l0, p0, o0),
                     (r["L"], r["parent"], r["order"]))
            c.eq("H2: L is the minimax level, fixture %d" % f, rt["L"], minimax_levels(w, h, e, ocean))
            Ls = [rt["L"][x] for x in rt["order"]]
            c.ok("H2: pops non-decreasing in L, fixture %d" % f, all(Ls[i] <= Ls[i + 1] for i in range(len(Ls) - 1)))
            same_lakes(c, "inv 11 fixture %d area %d" % (f, area), r, rt)
            for res, lab in ((r, "t=0"), (rt, "random t")):
                if not check_invariants(res, c, "random fixture %d %s area %d" % (f, lab, area)):
                    c.fail("random fixture %d %s invariants" % (f, lab))
                else:
                    c.passes += 1


GOLDEN_CASES = [
    # (name, fixture, N, seed, river_min_area_m2, why)
    ("shipped-64-s0", "shipped", 64, 0, 125000, "small shipped world"),
    ("shipped-48-s3", "shipped", 48, 3, 125000, "N not a power of two"),
    ("shipped-128-s0", "shipped", 128, 0, 125000, "P1-02 8.4 library case"),
    ("shipped-128-s9", "shipped", 128, 9, 125000, "mountain floor world (P1-01 E5.1 triggers)"),
    ("shipped-128-s42", "shipped", 128, 42, 125000, "seed 42"),
    ("small-128-s0", "small", 128, 0, 125000, "one lake"),
    ("small-128-s1", "small", 128, 1, 125000, "mountain floor, small-map fixture"),
    ("small-128-s2", "small", 128, 2, 125000, "two lakes"),
    ("small-128-s2-t100", "small", 128, 2, 25600, "T = 100: lake outlets feed rivers, rivers reach the sea"),
    ("shipped-128-s0-t20", "shipped", 128, 0, 5120, "T = 20: dense river network"),
    ("shipped-256-s0", "shipped", 256, 0, 125000, "N = 256"),
]

LARGE_CASES = [
    ("shipped-512-s0", "shipped", 512, 0, 125000, "P1-02 8.4"),
    ("shipped-512-s42", "shipped", 512, 42, 125000, "P1-02 8.4"),
    ("shipped-512-phrase", "shipped", 512, None, 125000, "P1-02 8.4, seed phrase \"The Unmapped\""),
    ("shipped-1024-s1", "shipped", 1024, 1, 125000, "P1-02 8.4"),
]

RANDOM_GOLDEN = [(f, 256 * 8) for f in range(4)]

# Cases run with t = 0 in the new file as well, to keep a t = 0 world in the
# goldens (the 5.1 and 8.1 tests are t = 0 by the spec).
ZERO_TIE_CASES = [
    ("small-128-s2-tie0", "small", 128, 2, 25600, "t = 0: the stage without the tie-break, T = 100"),
]

GOLDEN_HEADER_LEGACY = """\
# P1-02 hydrology golden vectors, generated by
# worldgen/tests/reference/p1_hydrology_reference.py golden, an independent
# exact-integer reference written from docs/specs/p1-02-hydrology.md, with the
# elevation input from worldgen/tests/reference/p1_elevation_reference.py.
# Do not edit by hand; never regenerate to make a C++ test pass.
#
# Format: ASCII, LF, one value per line, tokens separated by one space.
#   case <name> <fixture> <N> <seed> <river_min_area_m2>
#       starts a case; fixture shipped | small (elevation tuning of P1-01 4.4)
#       or random (the 34 x 34 random fixture of P1-02 8.2 number <seed>,
#       N = 34, no elevation stage)
#   <name> <item> <value>                 one value, items in step order
# Items, in step order (the test reports the first that differs):
#   elevation    P0-04 8 layer hash of the input elevation layer (equal to the
#                elevation golden `layer` item of the same case)
#   L            SHA-256 of the int64 flood levels (H2, G3), little-endian
#   parent       SHA-256 of one byte per cell, the flood parent (8 on ocean)
#   order        SHA-256 of the pop order as uint32 cell indices
#   rcv_g4       SHA-256 of one byte per cell, G4 receivers before H4 step 2
#   lakes        number of lakes K (H3)
#   lake_levels  lake level of each lake in id order, decimetres, or -
#   lake_cells   cell count of each lake in id order, or -
#   outlets      outlet cell index of each lake in id order (H4), or -
#   max_accum    largest flow_accum (H5)
#   river_cells  cells with water = 1 (H6)
#   max_class    largest river class (H6)
#   dl do        SHA-256 of the int64 chamfer grids to the ocean and to land (H7)
#   water water_depth flow_dir flow_accum river lake coast_dist
#                P0-04 8 layer hash of each output layer (H9)
"""


GOLDEN_HEADER = """\
# P1-02 hydrology golden vectors (with the H2 tie-break, generator 0.5.0),
# generated by worldgen/tests/reference/p1_hydrology_reference.py golden, an
# independent exact-integer reference written from
# docs/specs/p1-02-hydrology.md, with the elevation input from
# worldgen/tests/reference/p1_elevation_reference.py.
# Do not edit by hand; never regenerate to make a C++ test pass.
#
# Format: ASCII, LF, one value per line, tokens separated by one space.
#   case <name> <fixture> <N> <seed> <river_min_area_m2> <tie>
#       starts a case; fixture shipped | small (elevation tuning of P1-01 4.4)
#       or random (the 34 x 34 random fixture of P1-02 8.2 number <seed>,
#       N = 34, no elevation stage); tie flat (flat_tie of the world seed
#       with the shipped flat_noise, wavelength 400, octaves 1, gain 0.5),
#       random (the random tie values of P1-02 8.2 for the fixture) or zero
#       (t = 0 everywhere)
#   <name> <item> <value>                 one value, items in step order
# Items, in step order (the test reports the first that differs):
#   elevation    P0-04 8 layer hash of the input elevation layer (equal to the
#                elevation golden `layer` item of the same case)
#   tie          SHA-256 of the tie values t as uint16, little-endian (H2 step 1)
#   (the H2 flood of the re-specified tie-break, variant B: L[m] = max(e[m],
#   L[c]), pushed with key L[m] * 2^16 + t[m], popped by (key, seq))
#   L            SHA-256 of the int64 flood levels in decimetres (H2)
#   parent       SHA-256 of one byte per cell, the flood parent (8 on ocean)
#   order        SHA-256 of the pop order as uint32 cell indices
#   rcv_g4       SHA-256 of one byte per cell, G4 receivers before H4 step 2
#   lakes        number of lakes K (H3)
#   lake_levels  lake level of each lake in id order, decimetres, or -
#   lake_cells   cell count of each lake in id order, or -
#   outlets      outlet cell index of each lake in id order (H4), or -
#   max_accum    largest flow_accum (H5)
#   river_cells  cells with water = 1 (H6)
#   max_class    largest river class (H6)
#   dl do        SHA-256 of the int64 chamfer grids to the ocean and to land (H7)
#   water water_depth flow_dir flow_accum river lake coast_dist
#                P0-04 8 layer hash of each output layer (H9)
"""


def golden_text(large=False, legacy=False):
    """The golden file. legacy: the 0.4.0 file, every case with t = 0 and the
    0.4.0 item list and case lines."""
    out = [GOLDEN_HEADER_LEGACY if legacy else GOLDEN_HEADER]
    cases = [c + ("flat",) for c in GOLDEN_CASES]
    if not legacy:
        cases += [c + ("zero",) for c in ZERO_TIE_CASES]
    cases += [c + ("flat",) for c in LARGE_CASES] if large else []
    for name, fx, n, seed, area, why, mode in cases:
        if seed is None:
            seed = cr.seed_from_phrase(b"The Unmapped")[1]
        e = elevation_of(fx, n, seed)
        if legacy or mode == "zero":
            tie = None
        else:
            tie = flat_tie(seed, n, n, [1 if v < 0 else 0 for v in e])
        r = hydrology(n, n, e, area, tie)
        out.append("# %s\n" % why)
        if legacy:
            out.append("case %s %s %d %d %d\n" % (name, fx, n, seed, area))
        else:
            out.append("case %s %s %d %d %d %s\n" % (name, fx, n, seed, area, mode))
        for item, v in world_items(r, layer_hash(n, n, 4, e), legacy):
            out.append("%s %s %s\n" % (name, item, v))
        sys.stderr.write("golden: %s done\n" % name)
    for f, area in RANDOM_GOLDEN:
        w, h, e = random_fixture(f)
        r = hydrology(w, h, e, area, None if legacy else random_tie(f, w, h, e))
        name = "random-%d" % f
        out.append("# random fixture %d of P1-02 8.2\n" % f)
        if legacy:
            out.append("case %s random %d %d %d\n" % (name, w, f, area))
        else:
            out.append("case %s random %d %d %d random\n" % (name, w, f, area))
        for item, v in world_items(r, layer_hash(w, h, 4, e), legacy):
            out.append("%s %s %s\n" % (name, item, v))
    return "".join(out)


EX52_TIE = [
    [0, 900, 800, 700, 600],
    [0, 500, 50, 400, 300],
    [0, 200, 100, 60, 650],
    [0, 990, 950, 980, 970],
]


def check_example_52(c):
    w, h = 5, 4
    e = [-5 if i % 5 == 0 else 10 for i in range(20)]
    r0 = hydrology(w, h, e, 256)
    c.eq("5.2 t = 0 land pop order", [x for x in r0["order"] if e[x] > 0],
         [1, 6, 11, 16, 2, 7, 12, 17, 3, 8, 13, 18, 4, 9, 14, 19])
    c.eq("5.2 t = 0 flow_dir", rows_of(w, layer(r0, "flow_dir")),
         [[8, 4, 4, 4, 4], [8, 4, 5, 5, 5], [8, 4, 5, 5, 5], [8, 4, 5, 5, 5]])
    c.eq("5.2 t = 0 flow_accum", rows_of(w, layer(r0, "flow_accum")),
         [[0, 10, 6, 3, 1], [0, 3, 3, 2, 1], [0, 2, 2, 2, 1], [0, 1, 1, 1, 1]])
    tie = flat(EX52_TIE)
    rt = hydrology(w, h, e, 256, tie)
    c.eq("5.2 land pop order", [x for x in rt["order"] if e[x] > 0],
         [11, 7, 13, 12, 9, 8, 6, 4, 14, 3, 2, 1, 17, 19, 18, 16])
    c.eq("5.2 flow_dir", rows_of(w, layer(rt, "flow_dir")),
         [[8, 4, 2, 3, 2], [8, 4, 3, 4, 3], [8, 4, 4, 5, 4], [8, 4, 5, 6, 5]])
    c.eq("5.2 flow_accum", rows_of(w, layer(rt, "flow_accum")),
         [[0, 1, 1, 1, 1], [0, 1, 10, 1, 2], [0, 13, 1, 6, 1], [0, 1, 1, 1, 1]])
    c.eq("5.2 L = 10 on land", [rt["L"][i] for i in range(20) if e[i] > 0], [10] * 16)
    c.eq("5.2 no lake", (r0["comps"], rt["comps"]), ([], []))
    c.ok("5.2 invariants", check_invariants(rt, c, "5.2"))
    same_lakes(c, "5.2 inv 11", r0, rt)
    l2, p2, o2 = flood_tie(w, h, e, r0["ocean"], tie, queue="sorted")
    c.eq("5.2 sorted-list queue", (l2, p2, o2), (rt["L"], rt["parent"], rt["order"]))


def check_flat_tie(c):
    """flat_tie: 0 on ocean, [0, 65535] on land; t = 0 reproduces the stage
    without the tie-break on 5.1."""
    import p1_elevation_reference as er
    n = 64
    e = elevation_of("shipped", n, 0)
    ocean = [1 if v < 0 else 0 for v in e]
    t = flat_tie(0, n, n, ocean)
    c.ok("flat_tie 0 on ocean", all(t[i] == 0 for i in range(n * n) if ocean[i]))
    c.ok("flat_tie in range", all(0 <= v <= TIE_MAX for v in t))
    c.ok("flat_tie not constant on land", len(set(t[i] for i in range(n * n) if not ocean[i])) > 100)
    key = cr.stream_key(0, "geo.hydrology.flat")
    land = [i for i in range(n * n) if not ocean[i]]
    for i in (land[0], land[len(land) // 2], land[-1]):
        x, y = i % n, i // n
        nz = er.fbm_m(key, (16 * x + 8) << 32, (16 * y + 8) << 32, FLAT_NOISE)
        c.eq("flat_tie seed 0 N 64 cell (%d, %d)" % (x, y), t[i], min(TIE_MAX, (nz + (1 << 32)) >> 17))
    sys.stderr.write("flat_tie: seed 0 N 64 cells %s values %s\n"
                     % ([(i % n, i // n) for i in (land[0], land[len(land) // 2], land[-1])],
                        [t[i] for i in (land[0], land[len(land) // 2], land[-1])]))
    w, h, e5 = grid(EX51)
    c.eq("t = 0 on 5.1 equals no tie", hydrology(w, h, e5, 600, [0] * (w * h))["layers"],
         hydrology(w, h, e5, 600)["layers"])


def check_worlds(c):
    for name, fx, n, seed, area, _ in GOLDEN_CASES:
        e = elevation_of(fx, n, seed)
        rt = hydrology(n, n, e, area, flat_tie(seed, n, n, [1 if v < 0 else 0 for v in e]))
        r = hydrology(n, n, e, area)
        same_lakes(c, "%s inv 11" % name, r, rt)
        c.ok("%s invariants with flat_tie" % name, check_invariants(rt, c, name + " flat_tie"))
        L2, p2, o2 = flood_heap(n, n, e, r["ocean"])
        c.eq("%s queue independence" % name, (L2, p2, o2), (r["L"], r["parent"], r["order"]))
        c.ok("%s invariants" % name, check_invariants(r, c, name))
        sys.stderr.write("check: %s done (lakes %d, river cells %d)\n"
                         % (name, len(r["levels"]), sum(1 for v in layer(r, "water") if v == 1)))


def print_case(fx, n, seed, area):
    e = elevation_of(fx, n, seed)
    r = hydrology(n, n, e, area)
    for item, v in world_items(r, layer_hash(n, n, 4, e)):
        print(item, v)
    water = layer(r, "water")
    fd = layer(r, "flow_dir")
    mouths = sum(1 for i in range(n * n) if water[i] == 1 and e[step(n, n, i, fd[i])] < 0)
    print("river_mouths", mouths)


def print_fixtures():
    for nm, rows, area in (("5.1", EX51, 600), ("ties", TIES, 256), ("plateau", PLATEAU, 256),
                           ("terrace", TERRACE, 1024), ("rims", RIMS, 256)):
        w, h, e = grid(rows)
        r = hydrology(w, h, e, area)
        print("== %s (%d x %d, T %d)" % (nm, w, h, r["T"]))
        print("outlets", r["outlets"], "levels", r["levels"])
        for key in ("L", "parent", "rcv_g4"):
            print(key)
            for row in rows_of(w, r[key]):
                print("  " + " ".join("%d" % v for v in row))
        for name, _, vals in r["layers"]:
            print(name)
            for row in rows_of(w, vals):
                print("  " + " ".join("%d" % v for v in row))


def main(argv):
    if len(argv) >= 2 and argv[1] == "check":
        c = Checker()
        check_example(c)
        check_units(c)
        check_random(c)
        check_flat_tie(c)
        check_example_52(c)
        if "--quick" not in argv:
            check_worlds(c)
        sys.stderr.write("check: %d passed, %d failed\n" % (c.passes, c.fails))
        return 1 if c.fails else 0
    if len(argv) in (5, 6) and argv[1] == "case":
        area = int(argv[5]) if len(argv) == 6 else 125000
        print_case(argv[2], int(argv[3]), int(argv[4]), area)
        return 0
    if len(argv) >= 2 and argv[1] == "golden":
        args = [a for a in argv[2:] if not a.startswith("--")]
        text = golden_text(large="--large" in argv, legacy="--legacy" in argv)
        path = args[0] if args else os.path.join(_HERE, "..", "golden", "p1-02-hydrology.txt")
        if path == "-":
            sys.stdout.write(text)
        else:
            with open(path, "w", newline="\n") as f:
                f.write(text)
        return 0
    if len(argv) >= 2 and argv[1] == "fixtures":
        print_fixtures()
        return 0
    sys.stderr.write(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
