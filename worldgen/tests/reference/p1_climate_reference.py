#!/usr/bin/env python3
"""Independent exact-integer reference of the P1-03 climate stage.

Written by the test writer from docs/specs/p1-03-climate.md (revised
2026-11-17: triangle kernel C4, diagonal taps across the wind in a raster
sweep C5, preconditions in order with S <= 32767, shipped rain_oro_mm 2500
and moisture_path_mm_km 32000) and
docs/specs/p1-00-geography-overview.md (2.1 to 2.3, G6, D1 to D10, 10.2,
10.3) only, without reading any implementation of the stage. Steps C1 to C6
are implemented here from the spec text. The inputs of a world (elevation,
water, water_depth, coast_dist) come from p1_hydrology_reference.py, whose
elevation comes from p1_elevation_reference.py, so no climate golden value
depends on the C++ elevation or hydrology stage.

Python 3 standard library only, arbitrary-precision integers, no floating
point.

Usage (from the repository root; -B keeps __pycache__ out of the tree):

    python3 -B worldgen/tests/reference/p1_climate_reference.py check [--quick]
        Every worked example of P1-03 5.1 (wind draws, temperature table, box
        mean, both sweeps cell by cell, rainfall table and grid, the palette
        example of 9), cross-checks of the fast Fx operations against
        core_reference.py, a second, independent sweep (per-cell recursion
        instead of line order), the symmetry claims of 10.1 test 4 on every
        wind code, the invariants of section 6 on the random fixtures of
        10.2 and (without --quick) on the golden worlds.

    python3 -B worldgen/tests/reference/p1_climate_reference.py case FIXTURE N SEED [WEIGHTS]
        Print the golden items of one world (FIXTURE shipped or small, the
        elevation fixture of P1-01 4.4; WEIGHTS eight comma-separated
        wind_weights, default 1,1,1,1,1,1,1,1).

    python3 -B worldgen/tests/reference/p1_climate_reference.py golden [PATH|-] [--large]
        Write the golden vector file (default worldgen/tests/golden/
        p1-03-climate.txt); "-" writes it to standard output. --large adds
        the N = 512 and 1024 worlds of P1-03 10.4.

    python3 -B worldgen/tests/reference/p1_climate_reference.py winds FIRST LAST
        Print k_w for the shipped wind_weights for seeds FIRST to LAST.

Environment: UNMAPPED_ELEVATION_CACHE=DIR keeps the elevation grids of the
reference worlds in DIR (outside the repository), as for the hydrology
reference; every cached grid is checked against the hydrology golden file's
`elevation` item of its case before use when that case exists.
"""

import hashlib
import os
import sys
from array import array

sys.dont_write_bytecode = True
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
import core_reference as cr  # noqa: E402
import p1_elevation_reference as er  # noqa: E402

FatalError = cr.FatalError
ONE = 1 << 32
HALF = 1 << 31
SQRT2 = 6074001000

# D8 codes, P1-00 2.3.
DX = (1, 1, 0, -1, -1, -1, 0, 1)
DY = (0, 1, 1, 1, 0, -1, -1, -1)

WATER_OCEAN = 3
PRE = "climate: input precondition: "

STREAM_WIND = "geo.climate.wind"
STREAM_TEMP = "geo.climate.temperature"
STREAM_RAIN = "geo.climate.rainfall"
STREAM_TEST = "test.climate"

TEMP_MIN, TEMP_MAX = -600, 500
RAIN_MAX = 10000


def fatal(msg):
    raise FatalError(msg)


# ---------------------------------------------------------------------------
# Tuning (P1-03 4)
# ---------------------------------------------------------------------------

def dec(text):
    return cr.parse_decimal(text)


def shipped_tuning(**over):
    """The shipped (= fixture = small-map fixture) values of P1-03 4, as raw
    Fx (fx fields), ints (int fields), tuples (wavelength_m, octaves,
    gain raw) for the fbm_m records and a list for wind_weights."""
    t = {
        "temp_north_c": er.from_int(5),
        "temp_south_c": er.from_int(9),
        "lapse_c_per_km": dec("6.5"),
        "temp_noise": (6000, 3, dec("0.5")),
        "temp_noise_amp_c": er.from_int(1),
        "wind_weights": [1, 1, 1, 1, 1, 1, 1, 1],
        "smooth_radius_m": 320,
        "rain_base_mm": 800,
        "rain_oro_mm": 2500,
        "moisture_path_mm_km": 32000,
        "loss_max": dec("0.25"),
        "ocean_recharge_m": 500,
        "coast_rain_mm": 150,
        "coast_rain_reach_m": 1500,
        "rain_noise": (5000, 3, dec("0.5")),
        "rain_noise_amp": dec("0.4"),
        "rain_min_mm": 250,
    }
    for k, v in over.items():
        if k not in t:
            raise KeyError(k)
        t[k] = v
    return t


def quiet_tuning(**over):
    """Shipped values with both noise amplitudes 0 (the worked examples of
    5.1: "Noise amplitudes are 0 unless stated")."""
    base = {"temp_noise_amp_c": 0, "rain_noise_amp": 0}
    base.update(over)
    return shipped_tuning(**base)


def one_hot(k):
    return [5 if i == k else 0 for i in range(8)]


# ---------------------------------------------------------------------------
# C2: prevailing wind
# ---------------------------------------------------------------------------

def wind(seed, weights):
    """k_w = Rng(K_wind).weighted_index(weights), one call (P0-01 6.3).
    Returns (k_w, draws consumed)."""
    rng = cr.Rng(cr.stream_key(seed, STREAM_WIND))
    k = rng.weighted_index(list(weights))
    return k, rng.draws


# ---------------------------------------------------------------------------
# C1: preconditions, surface
# ---------------------------------------------------------------------------

def surface(e, depth):
    return [e[i] + depth[i] for i in range(len(e))]


S_MAX = 32767


def preconditions(w, h, e, water, depth, coast):
    """Section 2, in order: 1. the four layers, W x H, of their types;
    2. every water value in 0..3; 3. S = 0 on ocean, 1 <= S elsewhere,
    S <= 32767 everywhere. Returns S."""
    n = w * h
    for name, a, lo, hi in (("elevation", e, -32768, 32767), ("water", water, 0, 255),
                            ("water_depth", depth, 0, 65535), ("coast_dist", coast, -32768, 32767)):
        if len(a) != n:
            fatal(PRE + "layer '%s' is not %d x %d" % (name, w, h))
        for v in a:
            if not lo <= v <= hi:
                fatal(PRE + "layer '%s' value outside its type" % name)
    for i in range(n):
        if water[i] > 3:
            fatal(PRE + "water value %d at cell %d" % (water[i], i))
    S = surface(e, depth)
    for i in range(n):
        if water[i] == WATER_OCEAN:
            if S[i] != 0:
                fatal(PRE + "surface is not 0 on ocean cell %d" % i)
        elif S[i] < 1:
            fatal(PRE + "surface below 1 on non-ocean cell %d" % i)
        if S[i] > S_MAX:
            fatal(PRE + "surface above 32767 at cell %d" % i)
    return S


# ---------------------------------------------------------------------------
# C3: temperature
# ---------------------------------------------------------------------------

def temp_parts(y, hgt, s, noise, t):
    """C3 for one cell: (t_sea, lap, tn, T, temperature). noise = the raw
    fbm_m value (before temp_noise_amp_c)."""
    V = er.from_ratio(2 * y + 1, 2 * hgt)
    t_sea = er.add(t["temp_north_c"], er.mul(er.sub(t["temp_south_c"], t["temp_north_c"]), V))
    lap = er.div_int(er.mul_int(t["lapse_c_per_km"], s), 10000)
    tn = er.mul(t["temp_noise_amp_c"], noise)
    T = er.add(er.sub(t_sea, lap), tn)
    temp = er.clamp(er.round_int(er.mul_int(T, 10)), TEMP_MIN, TEMP_MAX)
    return t_sea, lap, tn, T, temp


def noise_grid(seed, stream, w, h, rec):
    key = cr.stream_key(seed, stream)
    out = [0] * (w * h)
    for y in range(h):
        Y = (16 * y + 8) << 32
        for x in range(w):
            out[y * w + x] = er.fbm_m(key, (16 * x + 8) << 32, Y, rec)
    return out


# ---------------------------------------------------------------------------
# C4: smoothed surface
# ---------------------------------------------------------------------------

INT64_MAX = (1 << 63) - 1


def window_sums(w, h, V, r):
    """(sum over the clipped (2r+1)^2 window, c_w) per cell, by a row pass
    then a column pass of exact integer sums. Every partial sum is checked
    against int64 (the bound of section 7)."""
    rows = [0] * (w * h)
    for y in range(h):
        pre = [0]
        for x in range(w):
            pre.append(pre[-1] + V[y * w + x])
        for x in range(w):
            lo, hi = max(0, x - r), min(w - 1, x + r)
            rows[y * w + x] = pre[hi + 1] - pre[lo]
    out = [None] * (w * h)
    for x in range(w):
        pre = [0]
        for y in range(h):
            pre.append(pre[-1] + rows[y * w + x])
        for y in range(h):
            lo, hi = max(0, y - r), min(h - 1, y + r)
            cw = (min(w - 1, x + r) - max(0, x - r) + 1) * (hi - lo + 1)
            s = pre[hi + 1] - pre[lo]
            if abs(s) > INT64_MAX:
                fatal("C4: window sum overflows int64")
            out[y * w + x] = (s, cw)
    return out


def check_surface_range(S):
    for v in S:
        if not 0 <= v <= S_MAX:
            fatal("C4: S outside [0, 32767]")


def box_mean(w, h, S, r):
    """One box pass of half-width r (seam climate_box_mean, pass 1 of C4):
    hs1 = from_ratio(sum, c_w)."""
    if r < 0:
        fatal("box radius")
    check_surface_range(S)
    return [er.from_ratio(s, cw) for s, cw in window_sums(w, h, S, r)]


def triangle_mean(w, h, S, r):
    """C4 (seam climate_triangle_mean): h = r div 2; pass 1 = box_mean(S, h);
    pass 2 on the raw values of hs1: hs = div_int(from_raw(sum2), c_w)."""
    if r < 0:
        fatal("triangle radius")
    hh = r // 2
    hs1 = box_mean(w, h, S, hh)
    return [er.div_int(s, cw) for s, cw in window_sums(w, h, hs1, hh)]


def box_direct(w, h, V, r, final):
    """A box pass by summing every window directly (second method);
    final(sum, c_w) gives the value."""
    out = [0] * (w * h)
    for y in range(h):
        for x in range(w):
            s = 0
            c = 0
            for yy in range(max(0, y - r), min(h - 1, y + r) + 1):
                for xx in range(max(0, x - r), min(w - 1, x + r) + 1):
                    s += V[yy * w + xx]
                    c += 1
            out[y * w + x] = final(s, c)
    return out


def box_mean_direct(w, h, S, r):
    return box_direct(w, h, S, r, er.from_ratio)


def triangle_mean_direct(w, h, S, r):
    hh = r // 2
    return box_direct(w, h, box_mean_direct(w, h, S, hh), hh, er.div_int)


# ---------------------------------------------------------------------------
# C5: moisture sweep
# ---------------------------------------------------------------------------

def sweep_constants(code, t):
    dx, dy = DX[code], DY[code]
    diagonal = dx != 0 and dy != 0
    step_m = er.mul_int(SQRT2, 16) if diagonal else er.from_int(16)
    step_dm = er.mul_int(SQRT2, 160) if diagonal else er.from_int(160)
    rec = min(ONE, er.div(step_m, er.from_int(t["ocean_recharge_m"])))
    return dx, dy, step_m, step_dm, rec


def is_diagonal(code):
    return DX[code] != 0 and DY[code] != 0


def taps(code, x, y):
    """Taps a, b (weight 2), c of C5."""
    dx, dy = DX[code], DY[code]
    if dx != 0 and dy != 0:
        return ((x - dx, y), (x - dx, y - dy), (x, y - dy))
    if dx != 0:
        return ((x - dx, y - 1), (x - dx, y), (x - dx, y + 1))
    return ((x - 1, y - dy), (x, y - dy), (x + 1, y - dy))


def cell_step(ocean, hs_c, q_up, hs_up, consts, t):
    """One cell of C5 given its upwind values; returns (q, p, rise, g, lam)."""
    _, _, step_m, step_dm, rec = consts
    base = er.from_int(t["rain_base_mm"])
    if ocean:
        q = er.add(q_up, er.mul(ONE - q_up, rec))
        p = er.mul(q_up, base)
        return q, p, None, None, None
    rise = max(0, er.sub(hs_c, hs_up))
    g = er.div(rise, step_dm)
    p = er.mul(q_up, er.add(base, er.mul_int(g, t["rain_oro_mm"])))
    lam = min(t["loss_max"], er.div_int(er.mul(p, step_m), 1000 * t["moisture_path_mm_km"]))
    q = er.sub(q_up, er.mul(q_up, lam))
    return q, p, rise, g, lam


def upwind(w, h, code, x, y, q, hs):
    vals = []
    for (xx, yy) in taps(code, x, y):
        if 0 <= xx < w and 0 <= yy < h:
            vals.append((q[yy * w + xx], hs[yy * w + xx]))
        else:
            vals.append((ONE, 0))
    (qa, ha), (qb, hb), (qc, hc) = vals
    q_up = er.div_int(er.add(er.add(qa, er.mul_int(qb, 2)), qc), 4)
    hs_up = er.div_int(er.add(er.add(ha, er.mul_int(hb, 2)), hc), 4)
    return q_up, hs_up


def lines_of(w, h, code):
    """Processing order as a list of groups of cells (x, y). Orthogonal:
    the lines (each reads only the previous line). Diagonal: the rows of the
    raster order, whose cells are sequential (a reads the previous cell)."""
    dx, dy = DX[code], DY[code]
    if dx != 0 and dy != 0:
        ys = range(h) if dy == 1 else range(h - 1, -1, -1)
        xs = list(range(w)) if dx == 1 else list(range(w - 1, -1, -1))
        return [[(x, y) for x in xs] for y in ys]
    if dx != 0:
        xs = range(w) if dx == 1 else range(w - 1, -1, -1)
        return [[(x, y) for y in range(h)] for x in xs]
    ys = range(h) if dy == 1 else range(h - 1, -1, -1)
    return [[(x, y) for x in range(w)] for y in ys]


def sweep(w, h, hs, water, code, t, trace=None):
    """C5 in the stated order. Orthogonal lines are computed from a snapshot
    of the previous line (a check that no cell reads its own line); the
    diagonal raster is computed in place, cell after cell. Every tap read
    must already be computed (asserted). Returns (q, p) raw grids."""
    consts = sweep_constants(code, t)
    n = w * h
    q = [None] * n
    p = [None] * n
    diagonal = is_diagonal(code)
    for line in lines_of(w, h, code):
        new = []
        for (x, y) in line:
            for (xx, yy) in taps(code, x, y):
                if 0 <= xx < w and 0 <= yy < h and q[yy * w + xx] is None:
                    raise AssertionError("C5 order: tap read before it is computed")
            q_up, hs_up = upwind(w, h, code, x, y, q, hs)
            i = y * w + x
            r = cell_step(water[i] == WATER_OCEAN, hs[i], q_up, hs_up, consts, t)
            if trace is not None:
                trace[i] = (q_up, hs_up) + r
            if diagonal:
                q[i], p[i] = r[0], r[1]
            else:
                new.append((i, r[0], r[1]))
        for i, qi, pi in new:
            q[i] = qi
            p[i] = pi
    return q, p


def sweep_recursive(w, h, hs, water, code, t):
    """Second method for C5: each cell's q from its three taps by memoized
    recursion (no line order at all)."""
    consts = sweep_constants(code, t)
    memo = {}
    sys.setrecursionlimit(max(10000, 4 * (w + h) + 1000))

    def qp(x, y):
        k = (x, y)
        if k in memo:
            return memo[k]
        vals = []
        for (xx, yy) in taps(code, x, y):
            if 0 <= xx < w and 0 <= yy < h:
                vals.append((qp(xx, yy)[0], hs[yy * w + xx]))
            else:
                vals.append((ONE, 0))
        (qa, ha), (qb, hb), (qc, hc) = vals
        q_up = er.div_int(qa + 2 * qb + qc, 4)
        hs_up = er.div_int(ha + 2 * hb + hc, 4)
        i = y * w + x
        r = cell_step(water[i] == WATER_OCEAN, hs[i], q_up, hs_up, consts, t)
        memo[k] = (r[0], r[1])
        return memo[k]

    q = [0] * (w * h)
    p = [0] * (w * h)
    for line in lines_of(w, h, code):
        for (x, y) in line:
            q[y * w + x], p[y * w + x] = qp(x, y)
    return q, p


# ---------------------------------------------------------------------------
# C6: rainfall
# ---------------------------------------------------------------------------

def rain_parts(p, coast, rn, t):
    f = max(0, er.add(ONE, er.mul(t["rain_noise_amp"], rn)))
    pr = er.mul(p, f)
    d = max(0, coast)
    reach = t["coast_rain_reach_m"]
    cb = 0 if d >= reach else er.from_ratio(t["coast_rain_mm"] * (reach - d), reach)
    rain = er.clamp(er.round_int(er.add(pr, cb)), t["rain_min_mm"], RAIN_MAX)
    return f, pr, cb, rain


# ---------------------------------------------------------------------------
# The stage
# ---------------------------------------------------------------------------

def climate(w, h, e, water, depth, coast, t, seed):
    """Every intermediate of C1 to C6 as a dict, on a W x H grid (V uses H)."""
    S = preconditions(w, h, e, water, depth, coast)
    k, _ = wind(seed, t["wind_weights"])
    n = w * h
    tnoise = noise_grid(seed, STREAM_TEMP, w, h, t["temp_noise"])
    T = [0] * n
    temp = [0] * n
    for i in range(n):
        y = i // w
        _, _, _, T[i], temp[i] = temp_parts(y, h, S[i], tnoise[i], t)
    r = t["smooth_radius_m"] // 16
    hs = triangle_mean(w, h, S, r)
    q, p = sweep(w, h, hs, water, k, t)
    rnoise = noise_grid(seed, STREAM_RAIN, w, h, t["rain_noise"])
    rain = [rain_parts(p[i], coast[i], rnoise[i], t)[3] for i in range(n)]
    return {"w": w, "h": h, "e": list(e), "water": list(water), "depth": list(depth), "coast": list(coast),
            "S": S, "wind": k, "tnoise": tnoise, "T": T, "temperature": temp, "r": r, "hs": hs, "q": q, "p": p,
            "rnoise": rnoise, "rainfall": rain}


# ---------------------------------------------------------------------------
# Report keys (P1-03 9, P1-00 10.3)
# ---------------------------------------------------------------------------

def kth(values, k):
    return sorted(values)[k]


def percentiles(values, ps):
    n = len(values)
    if n == 0:
        return [None] * len(ps)
    s = sorted(values)
    return [s[min(n - 1, (p * n) // 100)] for p in ps]


def metres1(v):
    return ("-" if v < 0 else "") + "%d.%d" % (abs(v) // 10, abs(v) % 10)


def ratio4(a, b):
    if b == 0:
        return "n/a"
    v = cr.rha(a * 10000, b)
    return "%d.%04d" % (v // 10000, v % 10000)


def report_lines(r):
    land = [i for i in range(r["w"] * r["h"]) if r["e"][i] > 0]
    tp = percentiles([r["temperature"][i] for i in land], (0, 10, 50, 90, 100))
    rp = percentiles([r["rainfall"][i] for i in land], (0, 10, 50, 90, 100))
    out = []
    # Percentiles over no values print one n/a for the key (P1-00 appendix C 6).
    out.append(("temperature_c", "n/a" if not land else " ".join(metres1(v) for v in tp)))
    out.append(("rainfall_mm", "n/a" if not land else " ".join(str(v) for v in rp)))
    below = sum(1 for i in land if r["temperature"][i] < 0)
    out.append(("land_below_0c", ratio4(below, len(land))))
    if rp[1] is None:
        out.append(("rain_contrast", "n/a"))
    else:
        out.append(("rain_contrast", ratio4(rp[3], rp[1])))
    return out


# ---------------------------------------------------------------------------
# Palettes (P1-03 9, P1-00 10.2 interpolation rule)
# ---------------------------------------------------------------------------

TEMP_STOPS = [(-300, (49, 54, 149)), (-100, (69, 117, 180)), (0, (171, 217, 233)), (50, (255, 255, 191)),
              (100, (253, 174, 97)), (200, (215, 48, 39)), (300, (165, 0, 38))]
RAIN_STOPS = [(0, (140, 81, 10)), (250, (191, 129, 45)), (500, (223, 194, 125)), (750, (246, 232, 195)),
              (1000, (199, 234, 229)), (1500, (128, 205, 193)), (2500, (53, 151, 143)), (4000, (1, 102, 94)),
              (10000, (0, 60, 48))]


def ramp(stops, v):
    if v < stops[0][0]:
        return stops[0][1]
    if v >= stops[-1][0]:
        return stops[-1][1]
    for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
        if t0 <= v < t1:
            return tuple(c0[j] + (2 * (c1[j] - c0[j]) * (v - t0) + (t1 - t0)) // (2 * (t1 - t0)) for j in range(3))
    raise AssertionError("unreachable")


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


def layer_hash(w, h, code, values):
    head = w.to_bytes(4, "little") + h.to_bytes(4, "little") + bytes([code])
    return hashlib.sha256(head + _pack(_FMT[code], values)).hexdigest()


# ---------------------------------------------------------------------------
# Inputs: worlds from the hydrology reference, random fixtures of 10.2
# ---------------------------------------------------------------------------

def _hydrology_golden_elevation():
    """{(fixture, N, seed): elevation layer hash} of the flat-tie cases of
    the hydrology golden file."""
    path = os.path.join(_HERE, "..", "golden", "p1-02-hydrology.txt")
    out = {}
    if not os.path.exists(path):
        return out
    names = {}
    with open(path) as f:
        for line in f:
            tok = line.split()
            if not tok or tok[0].startswith("#"):
                continue
            if tok[0] == "case":
                names[tok[1]] = (tok[2], int(tok[3]), int(tok[4]), tok[6] if len(tok) > 6 else "flat")
            elif tok[1] == "elevation" and tok[0] in names:
                fx, n, seed, mode = names[tok[0]]
                if fx != "random":
                    out[(fx, n, seed)] = tok[2]
    return out


_ELEV_GOLDEN = None


def world_inputs(fixture, n, seed):
    """(e, water, water_depth, coast_dist) of a world: the hydrology
    reference with flat_tie of the seed and the shipped threshold."""
    global _ELEV_GOLDEN
    import p1_hydrology_reference as hr
    if _ELEV_GOLDEN is None:
        _ELEV_GOLDEN = _hydrology_golden_elevation()
    e = hr.elevation_of(fixture, n, seed)
    want = _ELEV_GOLDEN.get((fixture, n, seed))
    got = layer_hash(n, n, 4, e)
    if want is not None and got != want:
        fatal("elevation input of %s %d %d differs from the hydrology golden value (stale cache?)"
              % (fixture, n, seed))
    tie = hr.flat_tie(seed, n, n, [1 if v < 0 else 0 for v in e])
    r = hr.hydrology(n, n, e, 125000, tie)
    return e, hr.layer(r, "water"), hr.layer(r, "water_depth"), hr.layer(r, "coast_dist")


def coast_of(w, h, water):
    import p1_hydrology_reference as hr
    ocean = bytearray(1 if v == WATER_OCEAN else 0 for v in water)
    land = bytearray(0 if v == WATER_OCEAN else 1 for v in water)
    dl = hr.chamfer(w, h, ocean)
    do = hr.chamfer(w, h, land)
    return [hr.coast_metres(dl[i]) if land[i] else -hr.coast_metres(do[i]) for i in range(w * h)]


def random_fixture(f):
    """Random fixture f of P1-03 10.2 (test writer's construction): a 34 x 34
    grid, the 32 x 32 interior land with S = elevation in [1, 3000], water 0,
    water_depth 0; the one-cell border ocean with elevation -10, water 3,
    water_depth 10 (S = 0). Draws from Rng(substream(stream_key(0,
    "test.climate"), f)): first the wind code uniform_u64(8), then S per
    interior cell in index order (uniform_i64(1, 3000)). coast_dist is H7
    of P1-02 on the ocean mask. Returns (w, h, e, water, depth, coast, code)."""
    rng = cr.Rng(cr.substream(cr.stream_key(0, STREAM_TEST), f))
    code = rng.uniform_u64(8)
    w = h = 34
    e, water, depth = [], [], []
    for i in range(w * h):
        x, y = i % w, i // w
        if x == 0 or y == 0 or x == w - 1 or y == h - 1:
            e.append(-10)
            water.append(WATER_OCEAN)
            depth.append(10)
        else:
            e.append(rng.uniform_i64(1, 3000))
            water.append(0)
            depth.append(0)
    return w, h, e, water, depth, coast_of(w, h, water), code


# ---------------------------------------------------------------------------
# Invariants (P1-03 6), by code independent of the stage functions above
# ---------------------------------------------------------------------------

def check_invariants(c, label, w, h, e, water, depth, coast, t, seed):
    r = climate(w, h, e, water, depth, coast, t, seed)
    n = w * h
    # 1. ranges
    c.ok(label + " inv 1 temperature", all(TEMP_MIN <= v <= TEMP_MAX for v in r["temperature"]))
    c.ok(label + " inv 1 rainfall", all(t["rain_min_mm"] <= v <= RAIN_MAX for v in r["rainfall"]))
    # 3 and 4 from a traced sweep.
    trace = {}
    sweep(w, h, r["hs"], water, r["wind"], t, trace)
    ok3 = ok4a = ok4b = True
    base = er.from_int(t["rain_base_mm"])
    for i in range(n):
        q_up, hs_up, q, p, rise = trace[i][:5]
        ok3 = ok3 and 0 <= q_up <= ONE and 0 <= q <= ONE
        if water[i] != WATER_OCEAN:
            ok4a = ok4a and q <= q_up
            if rise == 0:
                ok4b = ok4b and p == er.mul(q_up, base)
    c.ok(label + " inv 3", ok3)
    c.ok(label + " inv 4 q <= q_up", ok4a)
    c.ok(label + " inv 4 lee", ok4b)
    return r


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


def expect_fatal(c, label, fn, prefix=None):
    try:
        fn()
    except FatalError as ex:
        c.ok(label + " message", prefix is None or str(ex).startswith(prefix))
        return
    c.ok(label + " is fatal", False)


def rows(w, vals):
    return [vals[i:i + w] for i in range(0, len(vals), w)]


def mm3(raw):
    """A raw Fx value in mm to three decimals (RHA), as 5.1 prints p."""
    v = cr.rha(raw * 1000, ONE)
    return "%s%d.%03d" % ("-" if v < 0 else "", abs(v) // 1000, abs(v) % 1000)


# ---------------------------------------------------------------------------
# Worked examples of 5.1 and other checks
# ---------------------------------------------------------------------------

SWEEP_S = [[0, 20, 60, 120, 80, 0], [0, 30, 90, 160, 100, 0], [0, 10, 40, 80, 60, 0]]


def sweep_grid():
    w, h = 6, 3
    S = [v for row in SWEEP_S for v in row]
    water = [WATER_OCEAN if (i % w) in (0, 5) else 0 for i in range(w * h)]
    hs = [er.from_int(v) for v in S]  # r = 0: hs = from_int(S)
    return w, h, S, water, hs


def check_core(c):
    # The P0-01 vectors 5.1 quotes, and the fast Fx operations of the
    # elevation reference against core_reference on the values used here.
    c.eq("stream_key(0, geo.elevation.erosion)", cr.stream_key(0, "geo.elevation.erosion"), 0x7d5f47a9629481b8)
    c.eq("Rng(0) first output", cr.Rng(0).next(), 0x99ec5f36cb75f2b4)
    c.eq("parse_decimal 0.4", dec("0.4"), 1717986918)
    c.eq("lapse 6.5 raw", dec("6.5"), 27917287424)
    c.eq("loss_max 0.25", dec("0.25"), 1 << 30)
    rng = cr.Rng(cr.stream_key(7, "test.climate"))
    for _ in range(2000):
        a = cr.as_i64(rng.next()) >> rng.uniform_u64(40)
        b = cr.as_i64(rng.next()) >> (20 + rng.uniform_u64(40))
        if b == 0:
            b = 1
        try:
            want = cr.fx_mul(a, b)
        except FatalError:
            want = "FATAL"
        try:
            got = er.mul(a, b)
        except FatalError:
            got = "FATAL"
        c.eq("mul %d %d" % (a, b), got, want)
        try:
            want = cr.fx_div(a, b)
        except FatalError:
            want = "FATAL"
        try:
            got = er.div(a, b)
        except FatalError:
            got = "FATAL"
        c.eq("div %d %d" % (a, b), got, want)
        k = 1 + rng.uniform_u64(1 << 31)
        c.eq("div_int", er.div_int(a, k), cr.fx_div_int(a, k))
        c.eq("from_ratio", er.from_ratio(b, k), cr.fx_from_ratio(b, k))
        c.eq("round_int", er.round_int(a), cr.fx_round_int(a))
    # fbm_m of the elevation reference against core fbm2 at a few cells.
    key = cr.stream_key(42, STREAM_TEMP)
    for (x, y) in ((0, 0), (5, 9), (63, 17), (1023, 511)):
        X, Y = (16 * x + 8) << 32, (16 * y + 8) << 32
        want = cr.fbm2(key, X, Y, 3, cr.fx_from_ratio(1, 6000), 2 << 32, HALF)
        c.eq("fbm_m (%d, %d)" % (x, y), er.fbm_m(key, X, Y, (6000, 3, HALF)), want)


def check_wind(c):
    ks = [wind(s, [1] * 8) for s in range(12)]
    c.eq("C2 seeds 0..11", [k for k, _ in ks], [1, 3, 4, 1, 1, 6, 0, 2, 5, 1, 2, 1])
    c.eq("C2 seeds 0..11 one draw each", [d for _, d in ks], [1] * 12)
    c.eq("C2 seed 42", wind(42, [1] * 8)[0], 6)
    for s in list(range(12)) + [42]:
        c.eq("C2 single weight seed %d" % s, wind(s, [0, 0, 0, 0, 5, 0, 0, 0]), (4, 1))
        for k in range(8):
            c.eq("C2 one-hot %d seed %d" % (k, s), wind(s, one_hot(k))[0], k)
    expect_fatal(c, "C2 all zero", lambda: wind(0, [0] * 8))
    # The draw is uniform_u64(total) of P0-01 6.3: the first output of the
    # stream, mapped by Lemire's method.
    rng = cr.Rng(cr.stream_key(0, STREAM_WIND))
    x = rng.next()
    c.eq("C2 seed 0 by hand", (x * 8) >> 64, 1)


TEMP_TABLE = [
    # N, y, S, t_sea raw, lap raw, temperature
    (1024, 511, 12000, 30056382464, 33500744909, -8),
    (1024, 0, 0, 21483225088, 0, 50),
    (1024, 1023, 0, 38646317056, 0, 90),
    (512, 0, 30000, 21491613696, 83751862272, -145),
    (512, 255, 5, 30047993856, 13958644, 70),
    (512, 100, 7000, 24847056896, 19542101197, 12),
]


def check_temperature(c):
    t = quiet_tuning()
    for n, y, s, ts, lp, want in TEMP_TABLE:
        t_sea, lap, tn, T, temp = temp_parts(y, n, s, 0, t)
        lab = "C3 N %d y %d S %d" % (n, y, s)
        c.eq(lab + " t_sea", t_sea, ts)
        c.eq(lab + " lap", lap, lp)
        c.eq(lab + " tn", tn, 0)
        c.eq(lab + " temperature", temp, want)
        # Any noise value has no effect with amplitude 0.
        c.eq(lab + " noise ignored", temp_parts(y, n, s, -ONE, t)[4], want)
    # By hand, first row: V = 1023/2048; t_sea exactly 6.998046875.
    c.eq("C3 by hand t_sea", TEMP_TABLE[0][3], 5 * ONE + (4 * 1023 * ONE) // 2048)
    # Clamps: the most extreme inputs inside the bounds.
    hot = shipped_tuning(temp_north_c=er.from_int(40), temp_south_c=er.from_int(40), lapse_c_per_km=0,
                         temp_noise_amp_c=er.from_int(5))
    c.eq("C3 clamp high", temp_parts(0, 16, 1, ONE, hot)[4], 450)
    cold = shipped_tuning(temp_north_c=er.from_int(-30), temp_south_c=er.from_int(-30),
                          lapse_c_per_km=er.from_int(15), temp_noise_amp_c=er.from_int(5))
    c.eq("C3 clamp low", temp_parts(0, 16, 30000, -ONE, cold)[4], -600)
    c.eq("C3 clamp low T", temp_parts(0, 16, 30000, -ONE, cold)[3], er.from_int(-80))


BOX_S = [0, 10, 20, 30, 0, 40, 50, 60, 0, 0, 70, 80]
TRIANGLE_R2 = [
    [66512340764, 98028651710, 148733126732, 178956970667],
    [70588582874, 106711378805, 166761384518, 201624853618],
    [70986265031, 111748686127, 182138427923, 221906643627],
]
TRIANGLE_R2_DM = [["15.4861", "22.8241", "34.6296", "41.6667"], ["16.4352", "24.8457", "38.8272", "46.9444"],
                  ["16.5278", "26.0185", "42.4074", "51.6667"]]


def dm4(raw):
    """A raw Fx value to four decimals (RHA), as 5.1 prints hs in dm."""
    v = cr.rha(raw * 10000, ONE)
    return "%s%d.%04d" % ("-" if v < 0 else "", abs(v) // 10000, abs(v) % 10000)


def dm1(raw):
    v = cr.rha(raw * 10, ONE)
    return "%d.%d" % (v // 10, v % 10) if v % 10 else "%d" % (v // 10)


def check_box(c):
    S = BOX_S
    # One box pass (climate_box_mean), half-width 1.
    hs = box_mean(4, 3, S, 1)
    c.eq("box row 0", hs[0:4], [er.from_ratio(25, 2), er.from_int(20), er.from_int(35), er.from_int(40)])
    c.eq("box row 1 raw", hs[4:8], [35791394133, 90671531804, 171798691840, 221906643627])
    c.eq("box row 2", hs[8:12], [er.from_int(10), 114532461227, er.from_int(50), er.from_int(65)])
    c.eq("box (0, 1) = 50 / 6", hs[4], er.from_ratio(50, 6))
    c.eq("box direct", box_mean_direct(4, 3, S, 1), hs)
    c.eq("box r = 0", box_mean(4, 3, S, 0), [er.from_int(v) for v in S])
    for r in (3, 4, 100):
        c.eq("box r %d = grid mean" % r, box_mean(4, 3, S, r), [er.from_ratio(sum(S), 12)] * 12)
    # C4 triangle, r = 2 (h = 1).
    tri = triangle_mean(4, 3, S, 2)
    c.eq("C4 triangle r 2 raw", rows(4, tri), TRIANGLE_R2)
    c.eq("C4 triangle r 2 dm", [[dm4(v) for v in row] for row in rows(4, tri)], TRIANGLE_R2_DM)
    c.eq("C4 triangle (0, 0) sum2", 53687091200 + 85899345920 + 35791394133 + 90671531804, 266049363057)
    c.eq("C4 triangle (0, 0) hs1 window", [hs[0], hs[1], hs[4], hs[5]],
         [53687091200, 85899345920, 35791394133, 90671531804])
    c.eq("C4 triangle (0, 0) by hand", tri[0], cr.rha(266049363057, 4))
    c.eq("C4 triangle direct", triangle_mean_direct(4, 3, S, 2), tri)
    c.eq("C4 triangle r 3 = r 2", triangle_mean(4, 3, S, 3), tri)
    for r in (0, 1):
        c.eq("C4 triangle r %d = from_int(S)" % r, triangle_mean(4, 3, S, r), [er.from_int(v) for v in S])
    for r in (6, 7, 9, 40):
        c.eq("C4 triangle r %d = grid mean" % r, triangle_mean(4, 3, S, r), [128849018880] * 12)
    c.eq("C4 from_ratio(360, 12)", er.from_ratio(360, 12), 128849018880)
    # Edge clipping: 9 x 9, S = 900 at (4, 4), r = 4 (h = 2).
    S9 = [900 if (x, y) == (4, 4) else 0 for y in range(9) for x in range(9)]
    t9 = triangle_mean(9, 9, S9, 4)
    row4 = t9[4 * 9:5 * 9]
    c.eq("C4 clip row 4 dm", [dm1(v) for v in row4], ["12", "18", "21.6", "28.8", "36", "28.8", "21.6", "18", "12"])
    c.eq("C4 clip row 4 raw", row4[:5], [51539607552, 77309411328, 92771293594, 123695058125, 154618822656])
    c.eq("C4 clip row 4 symmetric", row4, row4[::-1])
    c.eq("C4 clip direct", triangle_mean_direct(9, 9, S9, 4), t9)
    # Unclipped x = 2..6: 900 w(x) w(4), w = (1,2,3,4,5,4,3,2,1)/25.
    wts = [1, 2, 3, 4, 5, 4, 3, 2, 1]
    for x in range(2, 7):
        c.eq("C4 clip unclipped x %d" % x, row4[x], er.from_ratio(900 * wts[x] * 5, 625))
    # smooth_radius_m div 16, then div 2
    c.eq("C4 r of 320 m", (320 // 16, 320 // 16 // 2), (20, 10))
    c.eq("C4 r of 4000 m", (4000 // 16, 4000 // 16 // 2), (250, 125))
    c.eq("C4 r of 32..47 m", {v // 16 for v in range(32, 48)}, {2})
    # Fatal arguments of both seams.
    expect_fatal(c, "box r < 0", lambda: box_mean(4, 3, S, -1))
    expect_fatal(c, "triangle r < 0", lambda: triangle_mean(4, 3, S, -1))
    expect_fatal(c, "box S > 32767", lambda: box_mean(2, 1, [1, 32768], 1))
    expect_fatal(c, "triangle S < 0", lambda: triangle_mean(2, 1, [-1, 5], 2))


def check_overflow_margin(c):
    """Section 7: S = 32767 everywhere with smooth_radius_m 4000 (h = 125)
    must not overflow; every hs is exactly 32767. The largest pass-2 window
    sum is 63001 x 32767 x 2^32 (< 2^63 - 1). Also on a grid where windows
    are full (251 x 251 cells) and a corner where they are clipped."""
    for (w, h) in ((260, 255), (300, 1)):
        S = [S_MAX] * (w * h)
        tri = triangle_mean(w, h, S, 250)
        c.ok("overflow %d x %d hs = 32767" % (w, h), all(v == er.from_int(S_MAX) for v in tri))
    full = 251 * 251 * S_MAX * ONE
    c.ok("overflow bound below int64", full < INT64_MAX)
    c.eq("overflow bound value", full, 63001 * 32767 << 32)
    # The running-window bound of section 7 (252 x 251 rows) also fits.
    c.ok("running window bound", 252 * 251 * S_MAX * ONE < INT64_MAX)


SWEEP_Q = [
    [4294967296, 4292578220, 4289950298, 4286870704, 4286633270, 4288375589],
    [4294967296, 4292242676, 4288516008, 4284675152, 4284396886, 4285979494],
    [4294967296, 4292913765, 4290620742, 4288210164, 4287302465, 4288699479],
]
SWEEP_P = [
    ["800.000", "1112.500", "1463.423", "1852.908", "798.767", "798.732"],
    ["800.000", "1268.750", "1853.620", "2203.391", "798.350", "798.271"],
    ["800.000", "956.250", "1229.199", "1384.716", "798.891", "798.794"],
]
SWEEP_RAIN = [[950, 1261, 1612, 2001, 947, 949], [950, 1417, 2002, 2352, 947, 948], [950, 1105, 1378, 1533, 947, 949]]
SE_Q = [
    [4294967296, 4291866607, 4289918572, 4287755798, 4289060547, 4293557436],
    [4294967296, 4290924843, 4286792934, 4283581311, 4284617726, 4289340761],
    [4294967296, 4291444379, 4287597741, 4283771548, 4281470892, 4285462311],
]
SE_P = [
    ["800.000", "1020.971", "1407.416", "1959.521", "1351.859", "799.725"],
    ["800.000", "1076.019", "1434.014", "1653.805", "798.523", "798.902"],
    ["800.000", "827.427", "799.079", "798.365", "797.936", "798.146"],
]
SE_RAIN = [[950, 1169, 1556, 2108, 1500, 950], [950, 1224, 1582, 1802, 947, 949], [950, 976, 947, 947, 946, 948]]


def check_sweep(c):
    t = quiet_tuning()
    w, h, S, water, hs = sweep_grid()
    coast = [-16 if water[i] == WATER_OCEAN else 16 for i in range(w * h)]
    consts = sweep_constants(0, t)
    c.eq("C5 rec raw", consts[4], 137438953)
    c.eq("C5 rec = 0.032", consts[4], er.div(er.from_int(16), er.from_int(500)))
    trace = {}
    q, p = sweep(w, h, hs, water, 0, t, trace)
    # (1, 0) by hand
    q_up, hs_up, q10, p10, rise, g, lam = trace[1]
    c.eq("C5 E (1,0) q_up", q_up, ONE)
    c.eq("C5 E (1,0) hs_up", hs_up, 0)
    c.eq("C5 E (1,0) rise", rise, er.from_int(20))
    c.eq("C5 E (1,0) g", g, 536870912)
    c.eq("C5 E (1,0) p", p10, er.from_ratio(2225, 2))
    c.eq("C5 E (1,0) lam", lam, 2389076)
    c.eq("C5 E (1,0) q", q10, 4294967296 - 2389076)
    c.eq("C5 E (1,0) q value", q10, 4292578220)
    # (2, 1) by hand
    i = 1 * w + 2
    q_up, hs_up, _, p21, rise, g, _ = trace[i]
    c.eq("C5 E (2,1) taps", taps(0, 2, 1), ((1, 0), (1, 1), (1, 2)))
    c.eq("C5 E (2,1) taps q", [q[0 * w + 1], q[1 * w + 1], q[2 * w + 1]], [4292578220, 4292242676, 4292913765])
    c.eq("C5 E (2,1) tap sum", 4292578220 + 2 * 4292242676 + 4292913765, 17169977337)
    c.eq("C5 E (2,1) q_up", q_up, 4292494334)
    c.eq("C5 E (2,1) hs_up", hs_up, er.from_ratio(45, 2))
    c.eq("C5 E (2,1) rise", rise, er.from_ratio(135, 2))
    c.eq("C5 E (2,1) g", g, 1811939328)
    c.eq("C5 E (2,1) p", p21, 7961235585091)
    c.eq("C5 E (2,1) p mm", mm3(p21), "1853.620")
    c.eq("C5 E (2,1) p = q_up x 1854.6875", p21, er.mul(4292494334, er.from_ratio(1854 * 16 + 11, 16)))
    c.eq("C5 E q grid", rows(w, q), SWEEP_Q)
    c.eq("C5 E p grid", [[mm3(v) for v in row] for row in rows(w, p)], SWEEP_P)
    c.eq("C5 E recursive", sweep_recursive(w, h, hs, water, 0, t), (q, p))
    for y in range(h):
        j = y * w + 4
        c.ok("C5 E column 4 row %d rise 0" % y, trace[j][4] == 0)
        c.eq("C5 E column 4 row %d p" % y, p[j], er.mul(trace[j][0], er.from_int(800)))
    for y in range(h):
        j = y * w + 5
        q_up = trace[j][0]
        c.eq("C5 E column 5 row %d" % y, (q[j], p[j]),
             (q_up + er.mul(ONE - q_up, 137438953), er.mul(q_up, er.from_int(800))))
    rain = [rain_parts(p[i], coast[i], 0, t)[3] for i in range(w * h)]
    c.eq("C6 E sweep rainfall", rows(w, rain), SWEEP_RAIN)
    # Through the whole stage (S = elevation, r = 0, wind forced, noise 0).
    e = [v if v > 0 else -5 for v in S]
    depth = [5 if v == 0 else 0 for v in S]
    for sr in (0, 15, 31):  # r = 0, 0, 1: all h = 0
        tw = quiet_tuning(smooth_radius_m=sr, wind_weights=one_hot(0))
        r = climate(w, h, e, water, depth, coast, tw, 0)
        c.eq("C5 E through the stage q (radius %d)" % sr, r["q"], q)
        c.eq("C6 E through the stage rainfall (radius %d)" % sr, rows(w, r["rainfall"]), SWEEP_RAIN)

    # Diagonal, wind SE (k_w = 1), raster order.
    consts = sweep_constants(1, t)
    c.eq("C5 SE step_m", consts[2], er.mul_int(SQRT2, 16))
    c.eq("C5 SE step_dm", consts[3], 971840160000)
    c.eq("C5 SE step_dm = SQRT2 x 160", consts[3], er.mul_int(SQRT2, 160))
    c.eq("C5 SE rec", consts[4], 194368032)
    c.eq("C5 SE order", [cell for row in lines_of(w, h, 1) for cell in row][:7],
         [(0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (5, 0), (0, 1)])
    for k, want in ((1, ((-1, 0), (-1, -1), (0, -1))), (3, ((1, 0), (1, -1), (0, -1))),
                    (5, ((1, 0), (1, 1), (0, 1))), (7, ((-1, 0), (-1, 1), (0, 1)))):
        c.eq("C5 taps table code %d" % k, tuple((a - 0, b - 0) for a, b in taps(k, 0, 0)), want)
    trace = {}
    q1, p1 = sweep(w, h, hs, water, 1, t, trace)
    c.eq("C5 SE (1,0) taps", taps(1, 1, 0), ((0, 0), (0, -1), (1, -1)))
    q_up, hs_up, q10, p10, rise, g, lam = trace[1]
    c.eq("C5 SE (1,0) q_up hs_up", (q_up, hs_up), (ONE, 0))
    c.eq("C5 SE (1,0) g", g, 379625062)
    c.eq("C5 SE (1,0) g = div", g, er.div(er.from_int(20), er.mul_int(SQRT2, 160)))
    c.eq("C5 SE (1,0) p", p10, 4385036491800)
    c.eq("C5 SE (1,0) p mm", mm3(p10), "1020.971")
    c.eq("C5 SE (1,0) lam", lam, 3100689)
    c.eq("C5 SE (1,0) q", q10, 4291866607)
    i = 1 * w + 2
    c.eq("C5 SE (2,1) taps", taps(1, 2, 1), ((1, 1), (1, 0), (2, 0)))
    c.eq("C5 SE (2,1) taps q", [q1[1 * w + 1], q1[0 * w + 1], q1[0 * w + 2]], [4290924843, 4291866607, 4289918572])
    q_up, hs_up, q21, p21, rise, _, _ = trace[i]
    c.eq("C5 SE (2,1) q_up", q_up, 4291144157)
    c.eq("C5 SE (2,1) hs_up", hs_up, er.from_ratio(65, 2))
    c.eq("C5 SE (2,1) rise", rise, er.from_ratio(115, 2))
    c.eq("C5 SE (2,1) p", p21, 6159041656350)
    c.eq("C5 SE (2,1) p mm", mm3(p21), "1434.014")
    c.eq("C5 SE (2,1) q", q21, 4286792934)
    c.eq("C5 SE (4,0) taps", taps(1, 4, 0), ((3, 0), (3, -1), (4, -1)))
    q_up, hs_up, _, p40, rise, _, _ = trace[4]
    c.eq("C5 SE (4,0) a", (q1[3], hs[3]), (4287755798, er.from_int(120)))
    c.eq("C5 SE (4,0) q_up", q_up, 4293164422)
    c.eq("C5 SE (4,0) hs_up", hs_up, er.from_int(30))
    c.eq("C5 SE (4,0) rise", rise, er.from_int(50))
    c.eq("C5 SE (4,0) p mm", mm3(p40), "1351.859")
    c.eq("C5 SE q grid", rows(w, q1), SE_Q)
    c.eq("C5 SE p grid", [[mm3(v) for v in row] for row in rows(w, p1)], SE_P)
    c.eq("C5 SE recursive", sweep_recursive(w, h, hs, water, 1, t), (q1, p1))
    rain = [rain_parts(p1[i], coast[i], 0, t)[3] for i in range(w * h)]
    c.eq("C6 SE sweep rainfall", rows(w, rain), SE_RAIN)
    tw = quiet_tuning(smooth_radius_m=0, wind_weights=one_hot(1))
    r = climate(w, h, e, water, depth, coast, tw, 0)
    c.eq("C6 SE through the stage rainfall", rows(w, r["rainfall"]), SE_RAIN)


RAIN_TABLE = [
    # p mm, coast_dist, rn text, cb raw, rainfall
    (800, 16, "0", er.from_ratio(150 * 1484, 1500), 948),
    (800, 1500, "0", 0, 800),
    (100, 3000, "0", 0, 250),
    (12000, 50, "0", er.from_int(145), 10000),
    (800, 700, "0.5", er.from_int(80), 1040),
    (800, -40, "-0.3", er.from_int(150), 854),
]


def check_rainfall(c):
    t = shipped_tuning()
    for pm, d, rn, cb, want in RAIN_TABLE:
        f, pr, got_cb, rain = rain_parts(er.from_int(pm), d, dec(rn), t)
        lab = "C6 p %d d %d rn %s" % (pm, d, rn)
        c.eq(lab + " cb", got_cb, cb)
        c.eq(lab + " rainfall", rain, want)
    c.eq("C6 cb 148.4", mm3(RAIN_TABLE[0][3]), "148.400")
    # Edge of the reach and the noise floor.
    c.eq("C6 d = reach - 1", rain_parts(er.from_int(800), 1499, 0, t)[2], er.from_ratio(150, 1500))
    c.eq("C6 f floor", rain_parts(er.from_int(800), 5000, -ONE, shipped_tuning(rain_noise_amp=ONE))[0], 0)


def check_palette(c):
    c.eq("palette temperature 25", ramp(TEMP_STOPS, 25), (213, 236, 212))
    c.eq("palette temperature stops", [ramp(TEMP_STOPS, s) for s, _ in TEMP_STOPS], [col for _, col in TEMP_STOPS])
    c.eq("palette temperature below", ramp(TEMP_STOPS, -600), TEMP_STOPS[0][1])
    c.eq("palette temperature above", ramp(TEMP_STOPS, 500), TEMP_STOPS[-1][1])
    c.eq("palette rainfall stops", [ramp(RAIN_STOPS, s) for s, _ in RAIN_STOPS], [col for _, col in RAIN_STOPS])


# --- Symmetry (10.1 test 4) -------------------------------------------------

def transform(w, h, vals, kind):
    """kind: 'mx' mirror x, 'my' mirror y, 'tr' transpose (w x h -> h x w).
    Returns (w', h', vals')."""
    if kind == "mx":
        return w, h, [vals[y * w + (w - 1 - x)] for y in range(h) for x in range(w)]
    if kind == "my":
        return w, h, [vals[(h - 1 - y) * w + x] for y in range(h) for x in range(w)]
    if kind == "tr":
        return h, w, [vals[x * w + y] for y in range(w) for x in range(h)]
    raise ValueError(kind)


def code_under(kind, code):
    dx, dy = DX[code], DY[code]
    if kind == "mx":
        dx = -dx
    elif kind == "my":
        dy = -dy
    else:
        dx, dy = dy, dx
    for k in range(8):
        if (DX[k], DY[k]) == (dx, dy):
            return k
    raise AssertionError


def symmetry_fixture():
    """10.1 test 4: 5 x 5, S = 10 on land with an ocean border."""
    w = h = 5
    water = [WATER_OCEAN if x in (0, 4) or y in (0, 4) else 0 for y in range(h) for x in range(w)]
    S = [0 if v == WATER_OCEAN else 10 for v in water]
    return w, h, S, water


def symmetric_sweep_ok(w, h, hs, water, t):
    """Invariant 7 for C5 on one grid: for each code and each generator,
    sweep(sigma(G), sigma(k)) == sigma(sweep(G, k)). Returns failures."""
    bad = []
    base = {k: sweep(w, h, hs, water, k, t) for k in range(8)}
    for kind in ("mx", "my", "tr"):
        w2, h2, hs2 = transform(w, h, hs, kind)
        _, _, water2 = transform(w, h, water, kind)
        for k in range(8):
            q2, p2 = sweep(w2, h2, hs2, water2, code_under(kind, k), t)
            if (transform(w2, h2, q2, kind)[2], transform(w2, h2, p2, kind)[2]) != base[k]:
                bad.append((kind, k))
    return bad


def symmetric_triangle_ok(w, h, S, r):
    base = triangle_mean(w, h, S, r)
    bad = []
    for kind in ("mx", "my", "tr"):
        w2, h2, S2 = transform(w, h, S, kind)
        if transform(w2, h2, triangle_mean(w2, h2, S2, r), kind)[2] != base:
            bad.append(kind)
    return bad


def hs_bound_ok(w, h, S, r, hs):
    """Invariant 7, second part: each hs lies between the smallest and the
    largest S within (Chebyshev) distance 2h."""
    hh = r // 2
    for y in range(h):
        for x in range(w):
            vals = [S[yy * w + xx] for yy in range(max(0, y - 2 * hh), min(h - 1, y + 2 * hh) + 1)
                    for xx in range(max(0, x - 2 * hh), min(w - 1, x + 2 * hh) + 1)]
            v = hs[y * w + x]
            if not er.from_int(min(vals)) <= v <= er.from_int(max(vals)):
                return False
    return True


def check_symmetry(c):
    t = quiet_tuning()
    fixtures = [symmetry_fixture()]
    w, h, S, water, _ = sweep_grid()
    fixtures.append((w, h, S, water))
    # 10.1 test 4: a non-square 7 x 5 grid of random S with a one-cell ocean border.
    rng = cr.Rng(cr.stream_key(1, STREAM_TEST))
    for (w, h) in ((7, 5), (5, 7), (9, 4), (1, 6), (6, 1)):
        water = [WATER_OCEAN if x in (0, w - 1) or y in (0, h - 1) else 0 for y in range(h) for x in range(w)]
        S = [0 if v == WATER_OCEAN else rng.uniform_i64(1, 3000) for v in water]
        fixtures.append((w, h, S, water))
    # Grids without an ocean border and with random water (the off-map rule
    # and lakes are symmetric too).
    for _ in range(6):
        w, h = 2 + rng.uniform_u64(9), 2 + rng.uniform_u64(9)
        water = [rng.uniform_u64(4) for _ in range(w * h)]
        S = [0 if v == WATER_OCEAN else rng.uniform_i64(1, 32767) for v in water]
        fixtures.append((w, h, S, water))
    for fi, (w, h, S, water) in enumerate(fixtures):
        for r in (0, 2, 3, 6):
            hs = triangle_mean(w, h, S, r)
            c.eq("symmetry fixture %d (%d x %d) r %d: sweep pairs failing" % (fi, w, h, r),
                 symmetric_sweep_ok(w, h, hs, water, t), [])
            c.eq("symmetry fixture %d r %d: triangle" % (fi, r), symmetric_triangle_ok(w, h, S, r), [])
            c.ok("inv 7 hs bound fixture %d r %d" % (fi, r), hs_bound_ok(w, h, S, r, hs))
    # The shipped tuning (noise does not enter C4 and C5).
    w, h, S, water = fixtures[2]
    c.eq("symmetry shipped tuning", symmetric_sweep_ok(w, h, triangle_mean(w, h, S, 2), water, shipped_tuning()), [])


# --- Random fixtures (10.2) ---------------------------------------------------

def check_random(c, count=200):
    t = shipped_tuning()
    codes = [0] * 8
    for f in range(count):
        w, h, e, water, depth, coast, code = random_fixture(f)
        codes[code] += 1
        tw = shipped_tuning(wind_weights=one_hot(code))
        r = check_invariants(c, "random %d" % f, w, h, e, water, depth, coast, tw, f)
        c.eq("random %d wind" % f, r["wind"], code)
        c.ok("random %d inv 7 hs bound" % f, hs_bound_ok(w, h, r["S"], r["r"], r["hs"]))
        if f < 40:
            c.eq("random %d inv 7 sweep" % f, symmetric_sweep_ok(w, h, r["hs"], water, tw), [])
            c.eq("random %d inv 7 triangle" % f, symmetric_triangle_ok(w, h, r["S"], r["r"]), [])
        if f < 20:
            c.eq("random %d recursive sweep" % f, sweep_recursive(w, h, r["hs"], water, code, tw), (r["q"], r["p"]))
            c.eq("random %d triangle direct" % f, triangle_mean_direct(w, h, r["S"], r["r"]), r["hs"])
            # Invariant 2 with temp_noise_amp_c 0.
            r0 = climate(w, h, e, water, depth, coast, shipped_tuning(wind_weights=one_hot(code),
                                                                       temp_noise_amp_c=0), f)
            ok = True
            for y in range(h):
                for a in range(w):
                    for b in range(w):
                        ia, ib = y * w + a, y * w + b
                        if r0["S"][ia] >= r0["S"][ib] and r0["temperature"][ia] > r0["temperature"][ib]:
                            ok = False
            c.ok("random %d inv 2 rows" % f, ok)
            # Invariant 5: rain_oro_mm 0, both amplitudes 0: land rainfall
            # does not change when the land heights change.
            t5 = shipped_tuning(wind_weights=one_hot(code), rain_oro_mm=0, temp_noise_amp_c=0, rain_noise_amp=0)
            a5 = climate(w, h, e, water, depth, coast, t5, f)
            e2 = [v if water[i] == WATER_OCEAN else 1 + (v * 7 + 13) % 3000 for i, v in enumerate(e)]
            b5 = climate(w, h, e2, water, depth, coast, t5, f)
            c.eq("random %d inv 5" % f, a5["rainfall"], b5["rainfall"])
    c.ok("random fixtures cover every wind code", all(v > 0 for v in codes))
    sys.stderr.write("random: wind codes %s\n" % codes)
    _ = t


def check_preconditions(c):
    w, h, S, water, _ = sweep_grid()
    e = [v if v > 0 else -5 for v in S]
    depth = [5 if v == 0 else 0 for v in S]
    coast = [16] * (w * h)
    t = quiet_tuning()
    climate(w, h, e, water, depth, coast, t, 0)

    def bad(label, ee=e, wa=water, de=depth, co=coast):
        expect_fatal(c, "C1 " + label, lambda: climate(w, h, ee, wa, de, co, t, 0), PRE)

    d = list(depth)
    d[0] = 4
    bad("ocean S != 0", de=d)
    e2 = list(e)
    e2[1] = 0
    bad("land S < 1", ee=e2)
    wa = list(water)
    wa[1] = 4
    bad("water 4", wa=wa)
    d = list(depth)
    d[1] = 32767 - e[1] + 1
    bad("S > 32767 on land", de=d)
    wa = list(water)
    wa[2] = 2
    d = list(depth)
    d[2] = 32767 - e[2]
    climate(w, h, e, wa, d, coast, t, 0)  # S = 32767 on a lake: allowed
    bad("wrong size", ee=e[:-1])
    # Precondition 2 is checked before 3: water 4 on a cell whose S is also bad.
    wa = list(water)
    wa[0] = 4
    d = list(depth)
    d[0] = 4
    try:
        climate(w, h, e, wa, d, coast, t, 0)
        c.ok("C1 order", False)
    except FatalError as ex:
        c.ok("C1 order: water first", "water" in str(ex))


# ---------------------------------------------------------------------------
# Golden file
# ---------------------------------------------------------------------------

UNIFORM = [1] * 8

GOLDEN_CASES = [
    # (name, fixture, N, seed, weights, why)
    ("shipped-64-s0", "shipped", 64, 0, UNIFORM, "small shipped world, wind SE (code 1)"),
    ("shipped-48-s3", "shipped", 48, 3, UNIFORM, "N not a power of two, wind SE"),
    ("shipped-128-s0", "shipped", 128, 0, UNIFORM, "P1-03 10.4 library case, wind SE"),
    ("shipped-128-s9", "shipped", 128, 9, UNIFORM, "mountain floor world (P1-01 E5.1), wind SE"),
    ("shipped-128-s42", "shipped", 128, 42, UNIFORM, "seed 42, wind N (code 6)"),
    ("small-128-s0", "small", 128, 0, UNIFORM, "small-map fixture, one lake, wind SE"),
    ("small-128-s1", "small", 128, 1, UNIFORM, "small-map fixture, mountain floor, wind SW (code 3)"),
    ("small-128-s2", "small", 128, 2, UNIFORM, "small-map fixture, two lakes, wind W (code 4)"),
    ("shipped-256-s0", "shipped", 256, 0, UNIFORM, "N = 256"),
] + [
    ("shipped-64-s0-w%d" % k, "shipped", 64, 0, one_hot(k), "wind forced to code %d by wind_weights" % k)
    for k in range(8)
] + [
    ("small-128-s2-w%d" % k, "small", 128, 2, one_hot(k), "two lakes, wind forced to code %d" % k)
    for k in (0, 2, 5, 7)
]

LARGE_CASES = [
    ("shipped-512-s0", "shipped", 512, 0, UNIFORM, "P1-03 10.4"),
    ("shipped-512-s42", "shipped", 512, 42, UNIFORM, "P1-03 10.4"),
    ("shipped-512-phrase", "shipped", 512, None, UNIFORM, "P1-03 10.4, seed phrase \"The Unmapped\""),
    ("shipped-1024-s1", "shipped", 1024, 1, UNIFORM, "P1-03 10.4"),
]

RANDOM_GOLDEN = list(range(4))

GOLDEN_HEADER = """\
# P1-03 climate golden vectors (spec revised 2026-11-17: triangle kernel C4,
# diagonal taps across the wind C5, rain_oro_mm 2500, moisture_path_mm_km
# 32000; input layers of generator 0.6.0), generated by
# worldgen/tests/reference/p1_climate_reference.py golden, an independent
# exact-integer reference written from docs/specs/p1-03-climate.md, with the
# input layers from worldgen/tests/reference/p1_hydrology_reference.py (and
# its elevation from p1_elevation_reference.py).
# Do not edit by hand; never regenerate to make a C++ test pass.
#
# Format: ASCII, LF, one value per line, tokens separated by one space.
#   case <name> <fixture> <N> <seed> <wind_weights>
#       starts a case; fixture shipped | small (elevation tuning of P1-01
#       4.4; hydrology of P1-02 with river_min_area_m2 125000 and flat_tie of
#       the seed) or random (the 34 x 34 random fixture of P1-03 10.2 number
#       <seed>, N = 34, no elevation or hydrology stage); climate tuning: the
#       shipped values of P1-03 4 with wind_weights replaced by the eight
#       comma-separated values given; the world seed is <seed>
#   <name> <item> <value>                 one value, items in step order
# Items, in step order (the test reports the first that differs):
#   elevation water water_depth coast_dist
#                P0-04 8 layer hash of each input layer (equal to the
#                hydrology golden items of the same world)
#   wind         k_w (C2)
#   S            SHA-256 of the surface S = elevation + water_depth, int64
#                little-endian (section 2)
#   tnoise       SHA-256 of fbm_m(K_temp, X, Y, temp_noise) per cell, raw
#                int64 (C3, before temp_noise_amp_c)
#   T            SHA-256 of T per cell, raw int64 (C3, before x 10)
#   temperature  P0-04 8 layer hash (i16) (C3)
#   hs           SHA-256 of hs per cell, raw int64 (C4, the triangle kernel,
#                r = smooth_radius_m div 16)
#   q p          SHA-256 of the moisture q and precipitation p per cell, raw
#                int64 (C5)
#   rnoise       SHA-256 of fbm_m(K_rain, X, Y, rain_noise) per cell, raw
#                int64 (C6)
#   rainfall     P0-04 8 layer hash (u16) (C6)
#   temperature_range rainfall_range
#                min,max of each layer over every cell
#   temperature_c rainfall_mm land_below_0c rain_contrast
#                the report values of P1-03 9 (P1-00 10.3 rules), spaces
#                written as "_"
# One case of fixture overflow (the C4 overflow margin of P1-03 7): a grid
# of N columns and N - 5 rows with S = 32767 - (7x + 13y) mod 768, items S
# (SHA-256 of S as int64), hs1 (one box pass, half-width 125), hs (triangle,
# r = 250) and hs_corner (raw hs at (0, 0)).
"""


def world_items(r):
    w, h = r["w"], r["h"]
    items = [
        ("elevation", layer_hash(w, h, 4, r["e"])),
        ("water", layer_hash(w, h, 1, r["water"])),
        ("water_depth", layer_hash(w, h, 3, r["depth"])),
        ("coast_dist", layer_hash(w, h, 4, r["coast"])),
        ("wind", str(r["wind"])),
        ("S", h_i64(r["S"])),
        ("tnoise", h_i64(r["tnoise"])),
        ("T", h_i64(r["T"])),
        ("temperature", layer_hash(w, h, 4, r["temperature"])),
        ("hs", h_i64(r["hs"])),
        ("q", h_i64(r["q"])),
        ("p", h_i64(r["p"])),
        ("rnoise", h_i64(r["rnoise"])),
        ("rainfall", layer_hash(w, h, 3, r["rainfall"])),
        ("temperature_range", "%d,%d" % (min(r["temperature"]), max(r["temperature"]))),
        ("rainfall_range", "%d,%d" % (min(r["rainfall"]), max(r["rainfall"]))),
    ]
    for k, v in report_lines(r):
        items.append((k, v.replace(" ", "_")))
    return items


_INPUT_CACHE = {}


def inputs_cached(fx, n, seed):
    k = (fx, n, seed)
    if k not in _INPUT_CACHE:
        _INPUT_CACHE[k] = world_inputs(fx, n, seed)
    return _INPUT_CACHE[k]


def golden_text(large=False):
    out = [GOLDEN_HEADER]
    cases = GOLDEN_CASES + (LARGE_CASES if large else [])
    for name, fx, n, seed, weights, why in cases:
        if seed is None:
            seed = cr.seed_from_phrase(b"The Unmapped")[1]
        e, water, depth, coast = inputs_cached(fx, n, seed)
        r = climate(n, n, e, water, depth, coast, shipped_tuning(wind_weights=list(weights)), seed)
        out.append("# %s\n" % why)
        out.append("case %s %s %d %d %s\n" % (name, fx, n, seed, ",".join(str(v) for v in weights)))
        for item, v in world_items(r):
            out.append("%s %s %s\n" % (name, item, v))
        sys.stderr.write("golden: %s done (wind %d)\n" % (name, r["wind"]))
    # The C4 overflow margin (section 7): a 260 x 255 grid with S near the
    # top, 32767 - (7x + 13y) mod 768, triangle r = 250 (h = 125, full
    # 251 x 251 windows in the interior).
    w, h = 260, 255
    S = [S_MAX - (7 * x + 13 * y) % 768 for y in range(h) for x in range(w)]
    hs1 = box_mean(w, h, S, 125)
    hs = triangle_mean(w, h, S, 250)
    out.append("# C4 overflow margin (P1-03 7): %d x %d, S = 32767 - (7x + 13y) mod 768, r = 250\n" % (w, h))
    out.append("case overflow-260x255 overflow %d 0 %s\n" % (w, ",".join(str(v) for v in UNIFORM)))
    for item, v in (("S", h_i64(S)), ("hs1", h_i64(hs1)), ("hs", h_i64(hs)), ("hs_corner", str(hs[0]))):
        out.append("overflow-260x255 %s %s\n" % (item, v))
    for f in RANDOM_GOLDEN:
        w, h, e, water, depth, coast, code = random_fixture(f)
        weights = one_hot(code)
        r = climate(w, h, e, water, depth, coast, shipped_tuning(wind_weights=weights), f)
        name = "random-%d" % f
        out.append("# random fixture %d of P1-03 10.2 (wind code %d)\n" % (f, code))
        out.append("case %s random %d %d %s\n" % (name, w, f, ",".join(str(v) for v in weights)))
        for item, v in world_items(r):
            out.append("%s %s %s\n" % (name, item, v))
    return "".join(out)


def check_worlds(c):
    for name, fx, n, seed, weights, _ in GOLDEN_CASES:
        if n > 128 or (weights != UNIFORM and fx == "small"):
            continue
        e, water, depth, coast = inputs_cached(fx, n, seed)
        t = shipped_tuning(wind_weights=list(weights))
        r = check_invariants(c, name, n, n, e, water, depth, coast, t, seed)
        if n <= 64:
            c.eq(name + " recursive sweep", sweep_recursive(n, n, r["hs"], water, r["wind"], t), (r["q"], r["p"]))
        sys.stderr.write("check: %s done (wind %d, temperature %d..%d, rainfall %d..%d)\n"
                         % (name, r["wind"], min(r["temperature"]), max(r["temperature"]),
                            min(r["rainfall"]), max(r["rainfall"])))


def print_case(fx, n, seed, weights):
    e, water, depth, coast = world_inputs(fx, n, seed)
    r = climate(n, n, e, water, depth, coast, shipped_tuning(wind_weights=weights), seed)
    for item, v in world_items(r):
        print(item, v)


def main(argv):
    if len(argv) >= 2 and argv[1] == "check":
        c = Checker()
        check_core(c)
        check_wind(c)
        check_temperature(c)
        check_box(c)
        check_overflow_margin(c)
        check_sweep(c)
        check_rainfall(c)
        check_palette(c)
        check_symmetry(c)
        check_preconditions(c)
        check_random(c)
        if "--quick" not in argv:
            check_worlds(c)
        sys.stderr.write("check: %d passed, %d failed\n" % (c.passes, c.fails))
        return 1 if c.fails else 0
    if len(argv) in (5, 6) and argv[1] == "case":
        weights = [int(v) for v in argv[5].split(",")] if len(argv) == 6 else UNIFORM
        print_case(argv[2], int(argv[3]), int(argv[4]), weights)
        return 0
    if len(argv) >= 2 and argv[1] == "golden":
        args = [a for a in argv[2:] if not a.startswith("--")]
        text = golden_text(large="--large" in argv)
        path = args[0] if args else os.path.join(_HERE, "..", "golden", "p1-03-climate.txt")
        if path == "-":
            sys.stdout.write(text)
        else:
            with open(path, "w", newline="\n") as f:
                f.write(text)
        return 0
    if len(argv) == 4 and argv[1] == "winds":
        for s in range(int(argv[2]), int(argv[3]) + 1):
            print(s, wind(s, UNIFORM)[0])
        return 0
    sys.stderr.write(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
