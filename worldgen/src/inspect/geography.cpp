// The [geography] section of the report, format v2 (spec P1-00 section 10.3).

#include "worldgen/inspect/geography.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <utility>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/math.hpp"
#include "worldgen/core/wide.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/geo/components.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/geo/select.hpp"
#include "worldgen/inspect/report.hpp"

namespace worldgen::inspect {

namespace {

// D8 offsets in code order (P1-00 2.3): E, SE, S, SW, W, NW, N, NE.
constexpr std::array<std::int64_t, 8> DX = {1, 1, 0, -1, -1, -1, 0, 1};
constexpr std::array<std::int64_t, 8> DY = {0, 1, 1, 1, 0, -1, -1, -1};

void require_grid(std::size_t size, std::uint32_t w, std::uint32_t h, const char* what) {
    if (w == 0U || h == 0U || static_cast<std::uint64_t>(size) != static_cast<std::uint64_t>(w) * h) {
        fatal(what);
    }
}

// The neighbour of cell i = y w + x in D8 direction code, or std::nullopt if
// the code is not 0..7 or the neighbour is outside the map.
std::optional<std::size_t> downstream(std::size_t i, std::uint64_t code, std::uint32_t w, std::uint32_t h) {
    if (code > 7U) {
        return std::nullopt;
    }
    const auto x = static_cast<std::int64_t>(i % w) + DX[static_cast<std::size_t>(code)];
    const auto y = static_cast<std::int64_t>(i / w) + DY[static_cast<std::size_t>(code)];
    if (x < 0 || y < 0 || x >= static_cast<std::int64_t>(w) || y >= static_cast<std::int64_t>(h)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x);
}

// Histogram of an i16 grid: count[v + 32768] for each value v.
std::vector<std::uint64_t> i16_histogram(std::span<const std::int16_t> e) {
    std::vector<std::uint64_t> count(65536U, 0U);
    for (const std::int16_t v : e) {
        ++count[static_cast<std::size_t>(static_cast<std::int32_t>(v) + 32768)];
    }
    return count;
}

// The k-th smallest (0-based) of the values listed by bins, visited in
// ascending value order as (value, count) by bin_value / bin_count over
// indices 0..bins-1. Exact: a value-only result (G1).
template <class ValueOf, class CountOf>
std::int64_t kth_in_bins(std::size_t bins, std::uint64_t k, const ValueOf& value_of, const CountOf& count_of) {
    // One return at the end, after the check, so that no compiler sees a
    // path without a return value (MSVC C4715) or code after the
    // [[noreturn]] fatal (C4702).
    std::uint64_t seen = 0;
    std::size_t b = 0;
    while (b < bins) {
        seen += count_of(b);
        if (k < seen) {
            break;
        }
        ++b;
    }
    if (b == bins) {
        fatal("kth_in_bins: rank beyond the values");
    }
    return value_of(b);
}

// v / 10^4 printed as its integer part, '.', and four digits (ratio4, tan4).
std::string fixed4(std::uint64_t v) {
    std::string frac = db::format_u64(v % 10000U);
    while (frac.size() < 4U) {
        frac.insert(frac.begin(), '0');
    }
    return db::format_u64(v / 10000U) + "." + frac;
}

}  // namespace

// --- Number formats ---------------------------------------------------------------

std::string ratio4(std::uint64_t a, std::uint64_t b) {
    if (b == 0U) {
        return std::string(NOT_AVAILABLE);
    }
    constexpr std::uint64_t SCALE = 10000U;
    // RHA of a non-negative quotient: floor((2 a 10^4 + b) / (2 b)), on
    // U128 (the numerator is below 2^80), as floor(floor(num / b) / 2) so
    // that 2 b never has to be formed.
    const U128 num = mul_u64_wide(2U * SCALE, a) + u128_from_u64(b);
    const U128DivMod q1 = divmod_u128_u64(num, b);
    const U128 q = q1.quotient >> 1U;
    if (q.hi != 0U) {
        fatal("ratio4: result too large");
    }
    return fixed4(q.lo);
}

std::string metres1(std::int64_t v) {
    // |v| as uint64, exact for INT64_MIN.
    const std::uint64_t mag = v < 0 ? 0U - static_cast<std::uint64_t>(v) : static_cast<std::uint64_t>(v);
    std::string out = v < 0 ? "-" : "";
    out += db::format_u64(mag / 10U);
    out += '.';
    out += static_cast<char>('0' + static_cast<int>(mag % 10U));
    return out;
}

namespace {

// min(n - 1, floor(p n / den)) for 0 <= p <= den, n >= 1: the percentile
// rank rule of P1-00 10.3 with den = 100 (percent) or 1000 (per mille).
std::uint64_t fraction_rank(std::uint64_t n, std::uint32_t p, std::uint32_t den) {
    // floor(p n / den) on U128: p n < 2^74.
    const U128DivMod q = divmod_u128_u64(mul_u64_wide(p, n), den);
    const std::uint64_t k = q.quotient.lo;  // <= n
    return k < n - 1U ? k : n - 1U;
}

}  // namespace

std::uint64_t percentile_rank(std::uint64_t n, std::uint32_t p) {
    if (n == 0U || p > 100U) {
        fatal("percentile_rank: no values or p above 100");
    }
    return fraction_rank(n, p, 100U);
}

std::uint64_t permille_rank(std::uint64_t n, std::uint32_t p) {
    if (n == 0U || p > 1000U) {
        fatal("permille_rank: no values or p above 1000");
    }
    return fraction_rank(n, p, 1000U);
}

// --- Elevation statistics ---------------------------------------------------------

LandOceanCounts count_land_ocean(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    require_grid(e.size(), w, h, "count_land_ocean: grid size");
    LandOceanCounts c;
    c.cells = static_cast<std::uint64_t>(w) * h;
    for (const std::int16_t v : e) {
        if (v > 0) ++c.land;
        if (v < 0) ++c.ocean;
    }
    return c;
}

Landmasses count_landmasses(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    require_grid(e.size(), w, h, "count_landmasses: grid size");
    if (static_cast<std::uint64_t>(e.size()) > geo::MAX_GRID_CELLS) {
        fatal("count_landmasses: grid larger than geo::MAX_GRID_CELLS");
    }
    // G2 (P1-00 7.2) with P = land and 8-connectivity. Only the count and
    // the component sizes are used.
    std::vector<std::uint8_t> land(e.size());
    for (std::size_t i = 0; i < e.size(); ++i) {
        land[i] = e[i] > 0 ? 1U : 0U;
    }
    const geo::Grid grid{static_cast<std::int32_t>(w), static_cast<std::int32_t>(h)};
    const geo::Components comps = geo::label_components(grid, land, geo::Connectivity::eight);
    Landmasses result;
    result.count = comps.count;
    for (std::uint32_t k = 1; k <= comps.count; ++k) {
        const std::uint64_t size = comps.size(k);
        if (size > result.largest_cells) {
            result.largest_cells = size;
        }
    }
    return result;
}

std::uint64_t coastline_m(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    require_grid(e.size(), w, h, "coastline_m: grid size");
    const auto coast_pair = [](std::int16_t a, std::int16_t b) { return (a > 0 && b < 0) || (a < 0 && b > 0); };
    std::uint64_t pairs = 0;
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            const std::size_t i = y * w + x;
            if (x + 1U < w && coast_pair(e[i], e[i + 1U])) ++pairs;
            if (y + 1U < h && coast_pair(e[i], e[i + w])) ++pairs;
        }
    }
    return 16U * pairs;
}

std::uint64_t circle_perimeter_raw(std::uint64_t land_cells) {
    if (land_cells >= (std::uint64_t{1} << 40U)) {
        fatal("circle_perimeter_raw: too many cells");
    }
    // 4 PI.raw < 2^36 and A = 256 land_cells < 2^48: product < 2^84, then
    // times 2^32 < 2^116.
    const auto four_pi = static_cast<std::uint64_t>(4 * Fx::PI.raw());
    const U128 n = mul_u64_wide(four_pi, 256U * land_cells) << 32U;
    return isqrt_u128(n);
}

std::optional<Fx> coast_ratio(std::uint64_t coastline, std::uint64_t land_cells) {
    if (land_cells == 0U) {
        return std::nullopt;
    }
    const std::uint64_t pq = circle_perimeter_raw(land_cells);  // >= isqrt(2^76) > 0
    if (coastline >= (std::uint64_t{1} << 63U)) {
        fatal("coast_ratio: coastline too long");
    }
    // RHA(coastline 2^64 / pq) for positive values: q = floor(n / pq), plus 1
    // if 2 rem >= pq. n = coastline 2^64 < 2^127.
    const U128 n = U128{coastline, 0U};
    const U128DivMod q = divmod_u128_u64(n, pq);
    U128 raw = q.quotient;
    if (q.remainder >= pq - q.remainder) {
        raw = raw + u128_from_u64(1U);
    }
    if (raw.hi != 0U || raw.lo > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        fatal("coast_ratio: ratio out of range");
    }
    return Fx::from_raw(static_cast<std::int64_t>(raw.lo));
}

std::uint64_t convex_hull_area2(std::span<const HullPoint> points) {
    constexpr std::int64_t LIMIT = std::int64_t{1} << 29;
    std::vector<HullPoint> p(points.begin(), points.end());
    for (const HullPoint& q : p) {
        if (q.x < -LIMIT || q.x > LIMIT || q.y < -LIMIT || q.y > LIMIT) {
            fatal("convex_hull_area2: coordinate out of range");
        }
    }
    const auto less = [](const HullPoint& a, const HullPoint& b) { return a.x != b.x ? a.x < b.x : a.y < b.y; };
    // determinism-lint: allow(unstable-sort) -- (x, y) is a strict total order on points and equal points are identical, so the result is unique
    std::sort(p.begin(), p.end(), less);
    p.erase(std::unique(p.begin(), p.end()), p.end());
    if (p.size() < 3U) {
        return 0U;
    }
    // Cross product of (a - o) and (b - o); each product of differences is
    // at most 2^60 in magnitude, so the result is at most 2^61.
    const auto cross = [](const HullPoint& o, const HullPoint& a, const HullPoint& b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };
    std::vector<HullPoint> hull;
    hull.reserve(2U * p.size());
    for (const HullPoint& q : p) {  // lower hull
        while (hull.size() >= 2U && cross(hull[hull.size() - 2U], hull.back(), q) <= 0) {
            hull.pop_back();
        }
        hull.push_back(q);
    }
    const std::size_t lower = hull.size() + 1U;
    for (std::size_t k = p.size() - 1U; k-- > 0U;) {  // upper hull
        while (hull.size() >= lower && cross(hull[hull.size() - 2U], hull.back(), p[k]) <= 0) {
            hull.pop_back();
        }
        hull.push_back(p[k]);
    }
    hull.pop_back();  // the first point again
    // Shoelace formula taken relative to hull[0] (the same sum, as a fan of
    // triangles): the hull is counter-clockwise and convex, so every term is
    // >= 0 and each partial sum is twice the area of a convex sub-polygon
    // inside the 2^30 x 2^30 box, at most 2^61.
    std::int64_t twice = 0;
    for (std::size_t k = 1; k + 1U < hull.size(); ++k) {
        twice += cross(hull[0], hull[k], hull[k + 1U]);
    }
    return static_cast<std::uint64_t>(twice < 0 ? -twice : twice);
}

std::vector<HullPoint> land_hull_points(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    require_grid(e.size(), w, h, "land_hull_points: grid size");
    std::vector<HullPoint> points;
    for (std::size_t y = 0; y < h; ++y) {
        std::optional<std::size_t> xl;
        std::size_t xr = 0;
        for (std::size_t x = 0; x < w; ++x) {
            if (e[y * w + x] > 0) {
                if (!xl) xl = x;
                xr = x;
            }
        }
        if (!xl) {
            continue;
        }
        const auto l = static_cast<std::int64_t>(*xl);
        const auto r = static_cast<std::int64_t>(xr) + 1;
        const auto t = static_cast<std::int64_t>(y);
        points.push_back({l, t});
        points.push_back({l, t + 1});
        points.push_back({r, t});
        points.push_back({r, t + 1});
    }
    return points;
}

std::uint64_t land_hull_area2(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    const std::vector<HullPoint> points = land_hull_points(e, w, h);
    return convex_hull_area2(points);
}

std::vector<std::int64_t> land_percentiles(std::span<const std::int16_t> e, std::span<const std::uint32_t> ps) {
    const std::vector<std::uint64_t> count = i16_histogram(e);
    // Land values 1..32767 are bins 32769..65535.
    constexpr std::size_t FIRST = 32769U;
    std::uint64_t n = 0;
    for (std::size_t b = FIRST; b < count.size(); ++b) n += count[b];
    std::vector<std::int64_t> out;
    if (n == 0U) {
        return out;
    }
    for (const std::uint32_t p : ps) {
        out.push_back(kth_in_bins(
            count.size() - FIRST, percentile_rank(n, p),
            [](std::size_t b) { return static_cast<std::int64_t>(b + FIRST) - 32768; },
            [&count](std::size_t b) { return count[b + FIRST]; }));
    }
    return out;
}

namespace {

// The land percentiles of a 16-bit climate layer: an exact histogram over
// the 65536 values of the type, offset so that bin b holds value b + base.
template <class T>
std::vector<std::int64_t> land_layer_percentiles_of(std::span<const std::int16_t> e, std::span<const T> values,
                                                    std::span<const std::uint32_t> ps, std::int64_t base) {
    if (values.size() != e.size()) {
        fatal("land_layer_percentiles: sizes differ");
    }
    std::vector<std::uint64_t> count(65536U, 0U);
    std::uint64_t n = 0;
    for (std::size_t i = 0; i < e.size(); ++i) {
        if (e[i] > 0) {
            ++count[static_cast<std::size_t>(static_cast<std::int64_t>(values[i]) - base)];
            ++n;
        }
    }
    std::vector<std::int64_t> out;
    if (n == 0U) {
        return out;
    }
    for (const std::uint32_t p : ps) {
        out.push_back(kth_in_bins(
            count.size(), percentile_rank(n, p),
            [base](std::size_t b) { return static_cast<std::int64_t>(b) + base; },
            [&count](std::size_t b) { return count[b]; }));
    }
    return out;
}

}  // namespace

std::vector<std::int64_t> land_layer_percentiles(std::span<const std::int16_t> e,
                                                 std::span<const std::int16_t> values,
                                                 std::span<const std::uint32_t> ps) {
    return land_layer_percentiles_of(e, values, ps, -32768);
}

std::vector<std::int64_t> land_layer_percentiles(std::span<const std::int16_t> e,
                                                 std::span<const std::uint16_t> values,
                                                 std::span<const std::uint32_t> ps) {
    return land_layer_percentiles_of(e, values, ps, 0);
}

std::vector<std::int64_t> ocean_depth_percentiles(std::span<const std::int16_t> e,
                                                  std::span<const std::uint32_t> ps) {
    const std::vector<std::uint64_t> count = i16_histogram(e);
    // Depths 1..32768 ascending are values -1..-32768, bins 32767 down to 0.
    constexpr std::size_t BINS = 32768U;
    std::uint64_t n = 0;
    for (std::size_t b = 0; b < BINS; ++b) n += count[b];
    std::vector<std::int64_t> out;
    if (n == 0U) {
        return out;
    }
    for (const std::uint32_t p : ps) {
        // Visit index j = 0..32767 as depth j + 1, bin 32767 - j.
        out.push_back(kth_in_bins(
            BINS, percentile_rank(n, p), [](std::size_t j) { return static_cast<std::int64_t>(j) + 1; },
            [&count](std::size_t j) { return count[BINS - 1U - j]; }));
    }
    return out;
}

std::uint64_t land_at_or_above(std::span<const std::int16_t> e, std::int64_t threshold_dm) {
    std::uint64_t n = 0;
    for (const std::int16_t v : e) {
        if (v > 0 && v >= threshold_dm) ++n;
    }
    return n;
}

std::uint64_t max_land_step(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    require_grid(e.size(), w, h, "max_land_step: grid size");
    std::int64_t best = 0;
    const auto consider = [&best](std::int16_t a, std::int16_t b) {
        if (a > 0 && b > 0) {
            const std::int64_t d = static_cast<std::int64_t>(a) - static_cast<std::int64_t>(b);
            const std::int64_t m = d < 0 ? -d : d;
            if (m > best) best = m;
        }
    };
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            const std::size_t i = y * w + x;
            if (x + 1U < w) consider(e[i], e[i + 1U]);
            if (y + 1U < h) consider(e[i], e[i + w]);
        }
    }
    return static_cast<std::uint64_t>(best);
}

// --- Step, slope and edge statistics ------------------------------------------------

namespace {

// m(c) of land cell i = y w + x (P1-00 10.3 "Cell step"), 4-neighbours in
// the order E, S, W, N; at most 32766.
std::int64_t cell_step_at(std::span<const std::int16_t> e, std::size_t w, std::size_t h, std::size_t x,
                          std::size_t y) {
    const std::size_t i = y * w + x;
    const std::int64_t c = e[i];
    std::int64_t m = 0;
    const auto consider = [&m, c](std::int16_t v) {
        if (v > 0) {
            const std::int64_t d = c - static_cast<std::int64_t>(v);
            const std::int64_t a = d < 0 ? -d : d;
            if (a > m) m = a;
        }
    };
    if (x + 1U < w) consider(e[i + 1U]);
    if (y + 1U < h) consider(e[i + w]);
    if (x > 0U) consider(e[i - 1U]);
    if (y > 0U) consider(e[i - w]);
    return m;
}

// e'(p) = max(e(p), 0) (P1-00 10.3 "Cell slope").
constexpr std::int64_t sea_surface(std::int16_t v) noexcept {
    return v > 0 ? static_cast<std::int64_t>(v) : 0;
}

// S2 of cell (x, y), written as in P1-00 10.3: xa, xb, ya, yb clamped to the
// grid and the difference multiplied by 2 div (xb - xa). |gx|, |gy| <=
// 2 x 32767, so S2 <= 2 x 65534^2 < 2^33.
std::int64_t slope_s2_at(std::span<const std::int16_t> e, std::size_t w, std::size_t h, std::size_t x,
                         std::size_t y) {
    const std::size_t xa = x > 0U ? x - 1U : 0U;
    const std::size_t xb = x + 1U < w ? x + 1U : w - 1U;
    const std::size_t ya = y > 0U ? y - 1U : 0U;
    const std::size_t yb = y + 1U < h ? y + 1U : h - 1U;
    std::int64_t gx = 0;
    std::int64_t gy = 0;
    if (xb > xa) {
        const auto factor = static_cast<std::int64_t>(2U / (xb - xa));
        gx = (sea_surface(e[y * w + xb]) - sea_surface(e[y * w + xa])) * factor;
    }
    if (yb > ya) {
        const auto factor = static_cast<std::int64_t>(2U / (yb - ya));
        gy = (sea_surface(e[yb * w + x]) - sea_surface(e[ya * w + x])) * factor;
    }
    return gx * gx + gy * gy;
}

// S2 accepted by the tangent functions: 0 <= S2 < 2^34, so 10^8 S2 < 2^61.
void require_s2(std::int64_t s2, const char* what) {
    if (s2 < 0 || s2 >= (std::int64_t{1} << 34)) {
        fatal(what);
    }
}

// (320 t)^2 for a threshold t / 10^4, 0 <= t <= 10^5: at most 1.024 x 10^15.
std::int64_t tangent_bound(std::int64_t tan4_value, const char* what) {
    if (tan4_value < 0 || tan4_value > 100000) {
        fatal(what);
    }
    const std::int64_t s = 320 * tan4_value;
    return s * s;
}

constexpr std::int64_t TAN_SCALE2 = 100000000;  // 10^8

// The thresholds in S2 terms quoted by P1-00 10.3, checked against the
// definition so that the constants and the text cannot drift apart.
static_assert(TAN_SCALE2 * 50205 > (320 * SLOPE_TAN4_35_DEG) * (320 * SLOPE_TAN4_35_DEG));
static_assert(TAN_SCALE2 * 50204 <= (320 * SLOPE_TAN4_35_DEG) * (320 * SLOPE_TAN4_35_DEG));
static_assert(TAN_SCALE2 * 102401 > (320 * SLOPE_TAN4_45_DEG) * (320 * SLOPE_TAN4_45_DEG));
static_assert(TAN_SCALE2 * 102400 <= (320 * SLOPE_TAN4_45_DEG) * (320 * SLOPE_TAN4_45_DEG));
static_assert(TAN_SCALE2 * 7 < (320 * SLOPE_TAN4_HALF_DEG) * (320 * SLOPE_TAN4_HALF_DEG));
static_assert(TAN_SCALE2 * 8 >= (320 * SLOPE_TAN4_HALF_DEG) * (320 * SLOPE_TAN4_HALF_DEG));

std::string format_slope_over(const SlopeStats& s) {
    return "35:" + ratio4(s.steeper_35, s.land) + " 45:" + ratio4(s.steeper_45, s.land);
}

// t for all four sides in one pass over the grid (P1-00 10.3).
struct EdgeDistanceSet {
    std::vector<std::int64_t> west, east, north, south;
};

EdgeDistanceSet all_edge_distances(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    EdgeDistanceSet t;
    t.west.assign(h, EDGE_UNDEFINED);
    t.east.assign(h, EDGE_UNDEFINED);
    t.north.assign(w, EDGE_UNDEFINED);
    t.south.assign(w, EDGE_UNDEFINED);
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            if (e[y * w + x] <= 0) {
                continue;
            }
            const auto xi = static_cast<std::int64_t>(x);
            const auto yi = static_cast<std::int64_t>(y);
            if (t.west[y] == EDGE_UNDEFINED) t.west[y] = xi;
            t.east[y] = static_cast<std::int64_t>(w) - 1 - xi;  // the last land cell of the row wins
            if (t.north[x] == EDGE_UNDEFINED) t.north[x] = yi;
            t.south[x] = static_cast<std::int64_t>(h) - 1 - yi;  // the last land cell of the column wins
        }
    }
    return t;
}

}  // namespace

std::vector<std::int64_t> land_cell_steps(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    require_grid(e.size(), w, h, "land_cell_steps: grid size");
    std::vector<std::int64_t> out;
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            if (e[y * w + x] > 0) out.push_back(cell_step_at(e, w, h, x, y));
        }
    }
    return out;
}

std::optional<std::int64_t> land_step_p999(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    require_grid(e.size(), w, h, "land_step_p999: grid size");
    // m(c) is in 0..32766 (two land values in 1..32767).
    constexpr std::size_t BINS = 32767U;
    std::vector<std::uint64_t> count(BINS, 0U);
    std::uint64_t n = 0;
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            if (e[y * w + x] > 0) {
                ++count[static_cast<std::size_t>(cell_step_at(e, w, h, x, y))];
                ++n;
            }
        }
    }
    if (n == 0U) {
        return std::nullopt;
    }
    return kth_in_bins(
        BINS, permille_rank(n, 999U), [](std::size_t b) { return static_cast<std::int64_t>(b); },
        [&count](std::size_t b) { return count[b]; });
}

std::int64_t cell_slope_s2(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h, std::uint32_t x,
                           std::uint32_t y) {
    require_grid(e.size(), w, h, "cell_slope_s2: grid size");
    if (x >= w || y >= h) {
        fatal("cell_slope_s2: cell outside the grid");
    }
    return slope_s2_at(e, w, h, x, y);
}

bool slope_steeper_than(std::int64_t s2, std::int64_t tan4_value) {
    require_s2(s2, "slope_steeper_than: S2 out of range");
    return TAN_SCALE2 * s2 > tangent_bound(tan4_value, "slope_steeper_than: threshold out of range");
}

bool slope_flatter_than(std::int64_t s2, std::int64_t tan4_value) {
    require_s2(s2, "slope_flatter_than: S2 out of range");
    return TAN_SCALE2 * s2 < tangent_bound(tan4_value, "slope_flatter_than: threshold out of range");
}

std::string tan4(std::int64_t s2) {
    require_s2(s2, "tan4: S2 out of range");
    // r = floor(10^4 sqrt(S2)) < 2^31; T = round(r / 320), halves up.
    const std::uint64_t r = isqrt_u64(static_cast<std::uint64_t>(TAN_SCALE2 * s2));
    return fixed4((r + 160U) / 320U);
}

SlopeStats slope_stats(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    require_grid(e.size(), w, h, "slope_stats: grid size");
    // The bounds in S2 terms, from the exact definitions (P1-00 10.3).
    const std::int64_t bound_35 = tangent_bound(SLOPE_TAN4_35_DEG, "slope_stats");
    const std::int64_t bound_45 = tangent_bound(SLOPE_TAN4_45_DEG, "slope_stats");
    const std::int64_t bound_flat = tangent_bound(SLOPE_TAN4_HALF_DEG, "slope_stats");
    SlopeStats s;
    std::vector<std::int64_t> high;
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            const std::int64_t v = e[y * w + x];
            if (v <= 0) {
                continue;
            }
            const std::int64_t s2 = slope_s2_at(e, w, h, x, y);
            const std::int64_t scaled = TAN_SCALE2 * s2;  // < 2^60
            ++s.land;
            if (scaled > bound_35) ++s.steeper_35;
            if (scaled > bound_45) ++s.steeper_45;
            if (v >= SLOPE_HIGH_DM) high.push_back(s2);
            if (v < SLOPE_LOWLAND_DM) {
                ++s.lowland;
                if (scaled < bound_flat) ++s.lowland_flat;
            }
        }
    }
    s.high = static_cast<std::uint64_t>(high.size());
    if (!high.empty()) {
        // G1 in the scratch array: only the value is read (P1-00 7.1).
        s.median_high_s2 = geo::kth_in_place(high, percentile_rank(s.high, 50U));
    }
    return s;
}

std::string slope_over(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    return format_slope_over(slope_stats(e, w, h));
}

std::string slope_p50_above_500m(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    const SlopeStats s = slope_stats(e, w, h);
    return s.median_high_s2 ? tan4(*s.median_high_s2) : std::string(NOT_AVAILABLE);
}

std::string lowland_flat_share(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    const SlopeStats s = slope_stats(e, w, h);
    return ratio4(s.lowland_flat, s.lowland);
}

std::vector<std::int64_t> edge_distances(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h,
                                         MapSide side) {
    require_grid(e.size(), w, h, "edge_distances: grid size");
    EdgeDistanceSet t = all_edge_distances(e, w, h);
    switch (side) {
        case MapSide::west:
            return std::move(t.west);
        case MapSide::east:
            return std::move(t.east);
        case MapSide::north:
            return std::move(t.north);
        case MapSide::south:
            break;
    }
    return std::move(t.south);
}

std::uint64_t longest_edge_run(std::span<const std::int64_t> t, std::int64_t tolerance) {
    constexpr std::int64_t LIMIT = std::int64_t{1} << 32;
    if (tolerance < 0 || tolerance > LIMIT) {
        fatal("longest_edge_run: tolerance out of range");
    }
    for (const std::int64_t v : t) {
        if (v < EDGE_UNDEFINED || v > LIMIT) {
            fatal("longest_edge_run: entry out of range");
        }
    }
    // Every sub-interval of a valid run is valid, so the largest run over
    // all starts is the longest valid window. Two pointers: for each end r,
    // the smallest start `left` whose window is valid, with the window's
    // maximum and minimum kept in monotone queues of indices (vectors with
    // a head position). An undefined entry empties the window.
    std::vector<std::size_t> maxq;
    std::vector<std::size_t> minq;
    std::size_t max_head = 0;
    std::size_t min_head = 0;
    std::size_t left = 0;
    std::uint64_t best = 0;
    for (std::size_t r = 0; r < t.size(); ++r) {
        if (t[r] == EDGE_UNDEFINED) {
            maxq.clear();
            minq.clear();
            max_head = 0;
            min_head = 0;
            left = r + 1U;
            continue;
        }
        while (maxq.size() > max_head && t[maxq.back()] <= t[r]) maxq.pop_back();
        maxq.push_back(r);
        while (minq.size() > min_head && t[minq.back()] >= t[r]) minq.pop_back();
        minq.push_back(r);
        while (t[maxq[max_head]] - t[minq[min_head]] > tolerance) {
            ++left;
            if (maxq[max_head] < left) ++max_head;
            if (minq[min_head] < left) ++min_head;
        }
        const auto run = static_cast<std::uint64_t>(r + 1U - left);
        if (run > best) best = run;
    }
    return best;
}

EdgeRuns edge_parallel_runs(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    require_grid(e.size(), w, h, "edge_parallel_runs: grid size");
    const EdgeDistanceSet t = all_edge_distances(e, w, h);
    EdgeRuns runs;
    runs.west = longest_edge_run(t.west, EDGE_RUN_TOLERANCE);
    runs.east = longest_edge_run(t.east, EDGE_RUN_TOLERANCE);
    runs.north = longest_edge_run(t.north, EDGE_RUN_TOLERANCE);
    runs.south = longest_edge_run(t.south, EDGE_RUN_TOLERANCE);
    return runs;
}

std::uint64_t edge_parallel_run_m(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h) {
    const EdgeRuns r = edge_parallel_runs(e, w, h);
    const std::uint64_t best = std::max(std::max(r.west, r.east), std::max(r.north, r.south));
    return static_cast<std::uint64_t>(geo::CELL_M) * best;
}

// --- Hydrology statistics ---------------------------------------------------------

FlowPaths flow_path_lengths(std::span<const std::int16_t> e, std::span<const std::uint8_t> flow_dir,
                            std::uint32_t w, std::uint32_t h) {
    require_grid(e.size(), w, h, "flow_path_lengths: grid size");
    require_grid(flow_dir.size(), w, h, "flow_path_lengths: flow_dir size");
    constexpr std::uint8_t UNKNOWN = 0U;
    constexpr std::uint8_t ON_PATH = 1U;
    constexpr std::uint8_t DONE = 2U;
    constexpr std::uint8_t BAD = 3U;
    const std::size_t n = e.size();
    std::vector<std::int64_t> d(n, 0);
    std::vector<std::uint8_t> state(n, UNKNOWN);
    std::vector<std::size_t> path;
    for (std::size_t start = 0; start < n; ++start) {
        if (state[start] != UNKNOWN) {
            continue;
        }
        // Walk down until a cell whose D is known (or the ocean), or a
        // failure; then fill the walked cells in reverse.
        path.clear();
        std::size_t cur = start;
        bool ok = false;
        std::int64_t base = 0;
        for (;;) {
            if (e[cur] < 0) {
                state[cur] = DONE;
                d[cur] = 0;
                ok = true;
                base = 0;
                break;
            }
            if (state[cur] == DONE) {
                ok = true;
                base = d[cur];
                break;
            }
            if (state[cur] != UNKNOWN) {  // ON_PATH: a cycle; BAD: leads to a failure
                break;
            }
            state[cur] = ON_PATH;
            path.push_back(cur);
            const std::optional<std::size_t> next = downstream(cur, flow_dir[cur], w, h);
            if (!next) {
                break;
            }
            cur = *next;
        }
        for (std::size_t k = path.size(); k-- > 0U;) {
            const std::size_t c = path[k];
            if (ok) {
                const std::int64_t step = (flow_dir[c] & 1U) != 0U ? DIAGONAL_STEP_RAW : ORTHOGONAL_STEP_RAW;
                // A path visits each cell once, so base < n * 2^37 < 2^63
                // for every grid of fewer than 2^26 cells; checked anyway.
                base = checked_add(base, step);
                d[c] = base;
                state[c] = DONE;
            } else {
                state[c] = BAD;
            }
        }
    }
    FlowPaths result;
    for (std::size_t i = 0; i < n; ++i) {
        if (state[i] == BAD) {
            result.bad_cell = static_cast<std::uint64_t>(i);
            return result;
        }
    }
    result.d = std::move(d);
    return result;
}

HydrologyResult hydrology_stats(std::span<const std::int16_t> e, const HydrologyCells& hydro, std::uint32_t w,
                                std::uint32_t h) {
    require_grid(e.size(), w, h, "hydrology_stats: elevation size");
    require_grid(hydro.water.size(), w, h, "hydrology_stats: water size");
    require_grid(hydro.flow_dir.size(), w, h, "hydrology_stats: flow_dir size");
    require_grid(hydro.flow_accum.size(), w, h, "hydrology_stats: flow_accum size");
    require_grid(hydro.river.size(), w, h, "hydrology_stats: river size");
    require_grid(hydro.lake.size(), w, h, "hydrology_stats: lake size");

    HydrologyResult result;
    FlowPaths paths = flow_path_lengths(e, hydro.flow_dir, w, h);
    if (paths.bad_cell) {
        result.bad_cell = paths.bad_cell;
        return result;
    }
    const std::size_t n = e.size();
    HydrologyStats s;

    // Lakes. The non-zero lake values are sorted so that equal ids are
    // adjacent; counting runs gives the size of each lake.
    std::vector<std::uint32_t> ids;
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint32_t id = hydro.lake[i];
        if (id > s.lakes) s.lakes = id;
        if (id != 0U) ids.push_back(id);
        if (hydro.water[i] == 2U) ++s.lake_cells;
        if (hydro.water[i] == 1U) ++s.river_cells;
    }
    // determinism-lint: allow(unstable-sort) -- plain uint32 values: equal elements are indistinguishable
    std::sort(ids.begin(), ids.end());
    for (std::size_t k = 0; k < ids.size();) {
        std::size_t j = k;
        while (j < ids.size() && ids[j] == ids[k]) ++j;
        const auto run = static_cast<std::uint64_t>(j - k);
        if (run > s.largest_lake_cells) s.largest_lake_cells = run;
        k = j;
    }

    // Rivers: mark the cells that have an upstream river or lake neighbour.
    std::vector<std::uint8_t> wet_upstream(n, 0U);
    for (std::size_t u = 0; u < n; ++u) {
        if (hydro.water[u] == 1U || hydro.water[u] == 2U) {
            if (const std::optional<std::size_t> m = downstream(u, hydro.flow_dir[u], w, h)) {
                wet_upstream[*m] = 1U;
            }
        }
    }
    for (std::size_t c = 0; c < n; ++c) {
        const std::optional<std::size_t> m = downstream(c, hydro.flow_dir[c], w, h);
        const bool to_ocean = e[c] >= 0 && m && e[*m] < 0;
        if (to_ocean && hydro.flow_accum[c] > s.largest_basin_cells) {
            s.largest_basin_cells = hydro.flow_accum[c];
        }
        if (hydro.water[c] != 1U) {
            continue;
        }
        // P1-00 appendix C item 8: over every river cell, so a river that
        // begins at a lake outlet is measured from there.
        if (paths.d[c] > s.longest_river_raw) s.longest_river_raw = paths.d[c];
        if (wet_upstream[c] == 0U) {
            ++s.river_sources;
        }
        if (m && e[*m] < 0) {
            ++s.river_mouths;
            if (hydro.river[c] >= 4U) ++s.major_rivers;
        }
    }
    result.stats = s;
    return result;
}

// --- The section ------------------------------------------------------------------

namespace {

// One hydrology layer [geography] reads and its catalogue type.
struct HydrologyLayerSpec {
    const db::LayerGrid* grid = nullptr;
    std::string_view name;
    db::ElementType type = db::ElementType::u8;
};

std::optional<std::string> layer_problem(const db::LayerGrid* grid, std::string_view name, db::ElementType type,
                                         const db::LayerGrid& elevation) {
    if (!db::is_valid_layer_grid(*grid)) {
        return "layer '" + std::string(name) + "' is not a valid grid";
    }
    if (grid->type() != type) {
        return "layer '" + std::string(name) + "' is " + std::string(db::element_type_name(grid->type())) +
               ", expected " + std::string(db::element_type_name(type));
    }
    if (grid->width != elevation.width || grid->height != elevation.height) {
        return "layer '" + std::string(name) + "' is " + db::format_u64(grid->width) + "x" +
               db::format_u64(grid->height) + ", not the size of layer 'elevation'";
    }
    return std::nullopt;
}

void line(std::string& out, std::string_view key, const std::string& value) {
    out += key;
    out += " = ";
    out += value;
    out += '\n';
}

std::string metres_list(const std::vector<std::int64_t>& values) {
    if (values.empty()) {
        return std::string(NOT_AVAILABLE);
    }
    std::string s;
    for (const std::int64_t v : values) {
        if (!s.empty()) s += ' ';
        s += metres1(v);
    }
    return s;
}

}  // namespace

std::optional<std::string> geography_layers_problem(const GeographyLayers& layers) {
    if (layers.elevation == nullptr) {
        return std::string("the world has no layer 'elevation'");
    }
    const db::LayerGrid& e = *layers.elevation;
    if (!db::is_valid_layer_grid(e)) {
        return std::string("layer 'elevation' is not a valid grid");
    }
    if (e.type() != db::ElementType::i16) {
        return "layer 'elevation' is " + std::string(db::element_type_name(e.type())) + ", expected i16";
    }
    if (e.cell_count() > geo::MAX_GRID_CELLS) {
        return "layer 'elevation' has " + db::format_u64(e.cell_count()) + " cells, more than the " +
               db::format_u64(geo::MAX_GRID_CELLS) + " the shared geography algorithms support";
    }
    if (layers.has_hydrology()) {
        const HydrologyLayerSpec hydro[] = {
            {layers.water, "water", db::ElementType::u8},
            {layers.flow_dir, "flow_dir", db::ElementType::u8},
            {layers.flow_accum, "flow_accum", db::ElementType::u32},
            {layers.river, "river", db::ElementType::u8},
            {layers.lake, "lake", db::ElementType::u32},
        };
        for (const auto& l : hydro) {
            if (std::optional<std::string> p = layer_problem(l.grid, l.name, l.type, e)) {
                return p;
            }
        }
    }
    if (layers.has_biomes()) {
        if (std::optional<std::string> p = layer_problem(layers.biome, "biome", db::ElementType::u8, e)) {
            return p;
        }
    }
    if (layers.has_climate()) {
        const HydrologyLayerSpec climate[] = {
            {layers.temperature, "temperature", db::ElementType::i16},
            {layers.rainfall, "rainfall", db::ElementType::u16},
        };
        for (const auto& l : climate) {
            if (std::optional<std::string> p = layer_problem(l.grid, l.name, l.type, e)) {
                return p;
            }
        }
    }
    return std::nullopt;
}

bool pass_walkable(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h,
                   std::span<const std::uint32_t> path) {
    require_grid(e.size(), w, h, "pass_walkable: grid size");
    if (path.empty()) {
        fatal("pass_walkable: empty path");
    }
    const std::uint64_t cells = std::uint64_t{w} * h;
    std::int64_t x0 = w;
    std::int64_t y0 = h;
    std::int64_t x1 = -1;
    std::int64_t y1 = -1;
    for (const std::uint32_t c : path) {
        if (c >= cells) {
            fatal("pass_walkable: path cell outside the grid");
        }
        x0 = std::min<std::int64_t>(x0, c % w);
        x1 = std::max<std::int64_t>(x1, c % w);
        y0 = std::min<std::int64_t>(y0, c / w);
        y1 = std::max<std::int64_t>(y1, c / w);
    }
    // Every cell of R lies within 37 cells of the path's bounding box.
    constexpr std::int64_t FOOT_R = 37;
    constexpr std::int64_t BAND_R = 19;
    x0 = std::max<std::int64_t>(0, x0 - FOOT_R);
    y0 = std::max<std::int64_t>(0, y0 - FOOT_R);
    x1 = std::min<std::int64_t>(std::int64_t{w} - 1, x1 + FOOT_R);
    y1 = std::min<std::int64_t>(std::int64_t{h} - 1, y1 + FOOT_R);
    const std::int64_t bw = x1 - x0 + 1;
    const std::int64_t bh = y1 - y0 + 1;
    // Per box cell: bit 0 in R, bit 1 walkable, bit 2 in F_a, bit 3 in F_b, bit 4 visited.
    std::vector<std::uint8_t> st(static_cast<std::size_t>(bw * bh), 0);
    const auto at = [&](std::int64_t x, std::int64_t y) -> std::uint8_t& {
        return st[static_cast<std::size_t>((y - y0) * bw + (x - x0))];
    };
    const auto disc = [&](std::uint32_t c, std::int64_t r, std::uint8_t bits) {
        const std::int64_t cx = c % w;
        const std::int64_t cy = c / w;
        for (std::int64_t y = std::max(y0, cy - r); y <= std::min(y1, cy + r); ++y) {
            for (std::int64_t x = std::max(x0, cx - r); x <= std::min(x1, cx + r); ++x) {
                if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) {
                    std::uint8_t& cell = at(x, y);
                    cell = static_cast<std::uint8_t>(cell | bits);
                }
            }
        }
    };
    static_assert(BAND_R * BAND_R == PASS_BAND_R2 && FOOT_R * FOOT_R == PASS_FOOT_R2);
    for (const std::uint32_t c : path) {
        disc(c, BAND_R, 1U);
    }
    disc(path.front(), FOOT_R, 1U | 4U);
    disc(path.back(), FOOT_R, 1U | 8U);
    std::vector<std::pair<std::int64_t, std::int64_t>> stack;
    for (std::int64_t y = y0; y <= y1; ++y) {
        for (std::int64_t x = x0; x <= x1; ++x) {
            std::uint8_t& s = at(x, y);
            const std::int16_t v = e[static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x)];
            if ((s & 1U) == 0U || v <= 0 ||
                slope_steeper_than(slope_s2_at(e, w, h, static_cast<std::size_t>(x), static_cast<std::size_t>(y)),
                                   SLOPE_TAN4_35_DEG)) {
                s = 0;  // not a walkable cell of R
                continue;
            }
            s = static_cast<std::uint8_t>(s | 2U);
            if ((s & 4U) != 0U) {
                s = static_cast<std::uint8_t>(s | 16U);
                stack.emplace_back(x, y);
            }
        }
    }
    // Breadth or depth makes no difference to the yes or no answer.
    while (!stack.empty()) {
        const auto [x, y] = stack.back();
        stack.pop_back();
        if ((at(x, y) & 8U) != 0U) {
            return true;
        }
        for (std::int64_t dy = -1; dy <= 1; ++dy) {
            for (std::int64_t dx = -1; dx <= 1; ++dx) {
                const std::int64_t nx = x + dx;
                const std::int64_t ny = y + dy;
                if ((dx == 0 && dy == 0) || nx < x0 || nx > x1 || ny < y0 || ny > y1) {
                    continue;
                }
                std::uint8_t& s = at(nx, ny);
                if ((s & 2U) != 0U && (s & 16U) == 0U) {
                    s = static_cast<std::uint8_t>(s | 16U);
                    stack.emplace_back(nx, ny);
                }
            }
        }
    }
    return false;
}

std::string pass_lines(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h,
                       std::span<const PassRecord> passes) {
    require_grid(e.size(), w, h, "pass_lines: grid size");
    std::uint64_t lowered = 0;
    std::uint64_t gaps = 0;
    std::uint64_t capped = 0;
    std::uint64_t walkable = 0;
    std::vector<std::int64_t> col_e;
    for (const PassRecord& p : passes) {
        if (p.capped == 1) ++capped;
        if (p.kind != 1) {
            if (p.kind == 0) ++gaps;
            continue;
        }
        ++lowered;
        if (p.col_x < 0 || p.col_y < 0 || p.col_x >= w || p.col_y >= h) {
            fatal("pass_lines: col outside the grid");
        }
        col_e.push_back(e[static_cast<std::size_t>(p.col_y) * w + static_cast<std::size_t>(p.col_x)]);
        if (pass_walkable(e, w, h, p.path)) ++walkable;
    }
    std::string out;
    line(out, "passes", db::format_u64(lowered));
    line(out, "natural_gaps", db::format_u64(gaps));
    line(out, "passes_capped", db::format_u64(capped));
    std::string cols;
    if (col_e.empty()) {
        cols = "n/a n/a n/a";
    } else {
        std::vector<std::int64_t> sorted = col_e;
        std::sort(sorted.begin(), sorted.end());  // determinism-lint: allow(unstable-sort) -- int64 values: equal elements are indistinguishable
        for (const std::uint32_t p : {0U, 50U, 100U}) {
            if (!cols.empty()) cols += ' ';
            cols += metres1(sorted[percentile_rank(sorted.size(), p)]);
        }
    }
    line(out, "pass_col_m", cols);
    line(out, "passes_walkable", db::format_u64(walkable));
    return out;
}

std::string climate_lines(std::span<const std::int16_t> e, std::span<const std::int16_t> temperature,
                          std::span<const std::uint16_t> rainfall) {
    if (temperature.size() != e.size() || rainfall.size() != e.size()) {
        fatal("climate_lines: sizes differ");
    }
    static constexpr std::array<std::uint32_t, 5> PS = {0U, 10U, 50U, 90U, 100U};
    const std::vector<std::int64_t> t = land_layer_percentiles(e, temperature, PS);
    const std::vector<std::int64_t> r = land_layer_percentiles(e, rainfall, PS);
    std::uint64_t land = 0;
    std::uint64_t below = 0;
    for (std::size_t i = 0; i < e.size(); ++i) {
        if (e[i] > 0) {
            ++land;
            if (temperature[i] < 0) ++below;
        }
    }
    std::string out;
    std::string temps;
    std::string rains;
    // Percentiles over no values print one n/a (P1-00 10.3, as elevation_m).
    if (t.empty()) {
        temps = std::string(NOT_AVAILABLE);
        rains = std::string(NOT_AVAILABLE);
    }
    for (std::size_t k = 0; k < t.size(); ++k) {
        if (k != 0U) {
            temps += ' ';
            rains += ' ';
        }
        temps += metres1(t[k]);
        rains += db::format_i64(r[k]);
    }
    line(out, "temperature_c", temps);
    line(out, "rainfall_mm", rains);
    line(out, "land_below_0c", ratio4(below, land));
    const bool contrast = !r.empty() && r[1] > 0;
    line(out, "rain_contrast",
         contrast ? ratio4(static_cast<std::uint64_t>(r[3]), static_cast<std::uint64_t>(r[1]))
                  : std::string(NOT_AVAILABLE));
    return out;
}

// --- Tables of schema 3 (P1-03 9, P1-04 9) ------------------------------------------

std::string_view wind_code_name(std::int64_t code) {
    static constexpr std::array<std::string_view, 8> NAMES = {"E", "SE", "S", "SW", "W", "NW", "N", "NE"};
    if (code < 0 || code > 7) {
        fatal("wind_code_name: not a D8 code");
    }
    return NAMES[static_cast<std::size_t>(code)];
}

ClimateTableResult read_climate_rows(std::span<const std::vector<db::ColumnValue>> rows) {
    ClimateTableResult r;
    if (rows.empty()) {
        return r;
    }
    if (rows.size() > 1U) {
        r.problem = "the table has " + db::format_u64(rows.size()) + " rows, expected 1";
        return r;
    }
    const std::vector<db::ColumnValue>& row = rows[0];
    if (row.size() != 2U || row[0].type != db::ValueType::integer || row[1].type != db::ValueType::integer) {
        r.problem = "the row does not have two integer columns";
        return r;
    }
    if (row[0].integer != 1) {
        r.problem = "the row has id " + db::format_i64(row[0].integer) + ", expected 1";
        return r;
    }
    if (row[1].integer < 0 || row[1].integer > 7) {
        r.problem = "wind_toward " + db::format_i64(row[1].integer) + " is not a D8 code 0..7";
        return r;
    }
    r.wind_toward = row[1].integer;
    return r;
}

BiomeTableResult read_biome_rows(std::span<const std::vector<db::ColumnValue>> rows) {
    BiomeTableResult r;
    for (const std::vector<db::ColumnValue>& row : rows) {
        const std::string id = (!row.empty() && row[0].type == db::ValueType::integer) ? db::format_i64(row[0].integer)
                                                                                       : std::string("?");
        const std::string at = "row " + id + ": ";
        if (row.size() != 5U) {
            r.problem = at + "has " + db::format_u64(row.size()) + " columns, expected 5";
            return r;
        }
        if (row[0].type != db::ValueType::integer || row[3].type != db::ValueType::integer ||
            row[4].type != db::ValueType::integer) {
            r.problem = at + "id, colour and water must be integers";
            return r;
        }
        if (row[1].type != db::ValueType::text || row[2].type != db::ValueType::text) {
            r.problem = at + "key and name must be text";
            return r;
        }
        BiomeRecord b;
        b.id = row[0].integer;
        b.key = row[1].bytes;
        b.name = row[2].bytes;
        b.colour = row[3].integer;
        b.water = row[4].integer;
        if (b.id < 1 || b.id > 254) {
            r.problem = at + "id is not a biome code 1..254";
            return r;
        }
        if (b.colour < 0 || b.colour > 0xFFFFFF) {
            r.problem = at + "colour " + db::format_i64(b.colour) + " is not 0xRRGGBB";
            return r;
        }
        if (b.water != 0 && b.water != 1) {
            r.problem = at + "water " + db::format_i64(b.water) + " is not 0 or 1";
            return r;
        }
        r.records.push_back(std::move(b));
    }
    return r;
}

std::string biome_lines(std::span<const std::uint8_t> biome, std::uint32_t w, std::uint32_t h,
                        std::span<const BiomeRecord> records) {
    require_grid(biome.size(), w, h, "biome_lines: grid size");
    // Land rows by code (ids are unique: the primary key), ascending id.
    std::array<std::uint8_t, 256> land{};
    std::map<std::int64_t, const BiomeRecord*> land_rows;
    for (const BiomeRecord& r : records) {
        if (r.water == 0 && r.id >= 0 && r.id <= 255) {
            land[static_cast<std::size_t>(r.id)] = 1U;
            land_rows.emplace(r.id, &r);
        }
    }
    std::array<std::uint64_t, 256> count{};
    std::uint64_t land_cells = 0;
    std::vector<std::uint8_t> mask(biome.size(), 0U);
    std::vector<std::int64_t> key(biome.size(), 0);
    for (std::size_t i = 0; i < biome.size(); ++i) {
        ++count[biome[i]];
        key[i] = biome[i];
        if (land[biome[i]] != 0U) {
            mask[i] = 1U;
            ++land_cells;
        }
    }
    std::string share;
    for (const auto& [id, rec] : land_rows) {
        if (!share.empty()) share += ' ';
        share += escape_report_text(rec->key);
        share += ':';
        share += ratio4(count[static_cast<std::size_t>(id)], land_cells);
    }
    if (share.empty()) {
        share = std::string(NOT_AVAILABLE);
    }
    const geo::Grid grid{static_cast<std::int32_t>(w), static_cast<std::int32_t>(h)};
    const geo::Components c = geo::label_components(grid, mask, geo::Connectivity::eight, key);
    std::string out;
    line(out, "biome_share", share);
    line(out, "biome_patches", db::format_u64(c.count));
    return out;
}

namespace {

// The hydrology lines of [geography]; false with *problem set when the
// statistics fail (defined below geography_section).
bool append_hydrology_lines(const GeographyLayers& layers, std::span<const std::int16_t> e, std::uint32_t w,
                            std::uint32_t h, const LandOceanCounts& counts, std::string& out, std::string* problem);

}  // namespace

std::optional<std::string> geography_section(const GeographyLayers& layers, std::string* problem) {
    if (std::optional<std::string> p = geography_layers_problem(layers)) {
        if (problem != nullptr) *problem = *p;
        return std::nullopt;
    }
    const db::LayerGrid& grid = *layers.elevation;
    const std::uint32_t w = grid.width;
    const std::uint32_t h = grid.height;
    const std::span<const std::int16_t> e(*grid.get_if<std::int16_t>());

    std::string out = "[geography]\n";
    const LandOceanCounts counts = count_land_ocean(e, w, h);
    line(out, "cells", db::format_u64(counts.cells));
    line(out, "land_cells", db::format_u64(counts.land));
    line(out, "ocean_cells", db::format_u64(counts.ocean));
    line(out, "land_fraction", ratio4(counts.land, counts.cells));
    const Landmasses masses = count_landmasses(e, w, h);
    line(out, "landmasses", db::format_u64(masses.count));
    line(out, "largest_landmass_share", ratio4(masses.largest_cells, counts.land));
    const std::uint64_t coast = coastline_m(e, w, h);
    line(out, "coastline_m", db::format_u64(coast));
    const std::optional<Fx> ratio = coast_ratio(coast, counts.land);
    line(out, "coast_ratio", ratio ? ratio->to_decimal(4) : std::string(NOT_AVAILABLE));
    line(out, "land_solidity", ratio4(2U * counts.land, land_hull_area2(e, w, h)));
    static constexpr std::array<std::uint32_t, 9> LAND_PS = {0U, 1U, 10U, 25U, 50U, 75U, 90U, 99U, 100U};
    static constexpr std::array<std::uint32_t, 3> DEPTH_PS = {0U, 50U, 100U};
    line(out, "elevation_m", metres_list(land_percentiles(e, LAND_PS)));
    line(out, "depth_m", metres_list(ocean_depth_percentiles(e, DEPTH_PS)));
    static constexpr std::array<std::int64_t, 5> ABOVE_M = {50, 500, 1000, 1500, 2000};
    std::string above;
    for (const std::int64_t m : ABOVE_M) {
        if (!above.empty()) above += ' ';
        above += db::format_i64(m);
        above += ':';
        above += ratio4(land_at_or_above(e, 10 * m), counts.land);
    }
    line(out, "land_above_m", above);
    line(out, "max_step_m", metres1(static_cast<std::int64_t>(max_land_step(e, w, h))));
    const std::optional<std::int64_t> step999 = land_step_p999(e, w, h);
    line(out, "step_p999_m", step999 ? metres1(*step999) : std::string(NOT_AVAILABLE));
    const SlopeStats slopes = slope_stats(e, w, h);
    line(out, "slope_over", format_slope_over(slopes));
    line(out, "slope_p50_above_500m",
         slopes.median_high_s2 ? tan4(*slopes.median_high_s2) : std::string(NOT_AVAILABLE));
    line(out, "lowland_flat_share", ratio4(slopes.lowland_flat, slopes.lowland));
    line(out, "edge_parallel_run_m", db::format_u64(edge_parallel_run_m(e, w, h)));
    if (layers.passes != nullptr) {
        out += pass_lines(e, w, h, *layers.passes);
    }

    if (layers.has_hydrology()) {
        if (!append_hydrology_lines(layers, e, w, h, counts, out, problem)) {
            return std::nullopt;
        }
    }
    if (layers.wind_toward) {
        line(out, "wind_toward", std::string(wind_code_name(*layers.wind_toward)));
    }
    if (layers.has_climate()) {
        out += climate_lines(e, *layers.temperature->get_if<std::int16_t>(),
                             *layers.rainfall->get_if<std::uint16_t>());
    }
    if (layers.has_biomes()) {
        out += biome_lines(*layers.biome->get_if<std::uint8_t>(), w, h, *layers.biomes);
    }
    return out;
}

namespace {

bool append_hydrology_lines(const GeographyLayers& layers, std::span<const std::int16_t> e, std::uint32_t w,
                            std::uint32_t h, const LandOceanCounts& counts, std::string& out, std::string* problem) {
    const HydrologyCells cells{
        *layers.water->get_if<std::uint8_t>(),     *layers.flow_dir->get_if<std::uint8_t>(),
        *layers.flow_accum->get_if<std::uint32_t>(), *layers.river->get_if<std::uint8_t>(),
        *layers.lake->get_if<std::uint32_t>(),
    };
    const HydrologyResult hydro = hydrology_stats(e, cells, w, h);
    if (!hydro.stats) {
        if (problem != nullptr) {
            const std::uint64_t c = *hydro.bad_cell;
            *problem = "layer 'flow_dir' does not lead to the ocean from cell (" + db::format_u64(c % w) + ", " +
                       db::format_u64(c / w) + ")";
        }
        return false;
    }
    const HydrologyStats& s = *hydro.stats;
    line(out, "lakes", db::format_u64(s.lakes));
    line(out, "lake_cells", db::format_u64(s.lake_cells));
    line(out, "lake_share_of_land", ratio4(s.lake_cells, counts.land));
    line(out, "largest_lake_cells", db::format_u64(s.largest_lake_cells));
    line(out, "river_cells", db::format_u64(s.river_cells));
    line(out, "river_share_of_land", ratio4(s.river_cells, counts.land));
    line(out, "river_sources", db::format_u64(s.river_sources));
    line(out, "river_mouths", db::format_u64(s.river_mouths));
    line(out, "major_rivers", db::format_u64(s.major_rivers));
    line(out, "longest_river_m", db::format_i64(Fx::from_raw(s.longest_river_raw).round_int()));
    line(out, "largest_basin_share", ratio4(s.largest_basin_cells, counts.land));
    return true;
}

}  // namespace

}  // namespace worldgen::inspect
