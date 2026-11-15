// The [geography] section of the report, format v2 (spec P1-00 section 10.3).

#include "worldgen/inspect/geography.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/math.hpp"
#include "worldgen/core/wide.hpp"
#include "worldgen/db/schema.hpp"

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
    std::uint64_t seen = 0;
    for (std::size_t b = 0; b < bins; ++b) {
        seen += count_of(b);
        if (k < seen) {
            return value_of(b);
        }
    }
    fatal("kth_in_bins: rank beyond the values");
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
    const std::uint64_t r = q.lo;
    std::string frac = db::format_u64(r % SCALE);
    while (frac.size() < 4U) {
        frac.insert(frac.begin(), '0');
    }
    return db::format_u64(r / SCALE) + "." + frac;
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

std::uint64_t percentile_rank(std::uint64_t n, std::uint32_t p) {
    if (n == 0U || p > 100U) {
        fatal("percentile_rank: no values or p above 100");
    }
    // floor(p n / 100) on U128: p n < 2^71.
    const U128DivMod q = divmod_u128_u64(mul_u64_wide(p, n), 100U);
    const std::uint64_t k = q.quotient.lo;  // <= n
    return k < n - 1U ? k : n - 1U;
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
    // MERGE (p1/foundation): G2 (P1-00 7.2) with P = land, 8-connectivity.
    // Labels are not needed, only the count and the sizes, which do not
    // depend on the visit order; the BFS is FIFO in 8-neighbour order anyway.
    Landmasses result;
    std::vector<std::uint8_t> seen(e.size(), 0U);
    std::vector<std::size_t> queue;
    for (std::size_t start = 0; start < e.size(); ++start) {
        if (e[start] <= 0 || seen[start] != 0U) {
            continue;
        }
        ++result.count;
        queue.clear();
        queue.push_back(start);
        seen[start] = 1U;
        for (std::size_t head = 0; head < queue.size(); ++head) {
            const std::size_t c = queue[head];
            for (std::uint64_t code = 0; code < 8U; ++code) {
                const std::optional<std::size_t> m = downstream(c, code, w, h);
                if (m && e[*m] > 0 && seen[*m] == 0U) {
                    seen[*m] = 1U;
                    queue.push_back(*m);
                }
            }
        }
        const auto size = static_cast<std::uint64_t>(queue.size());
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
        if (wet_upstream[c] == 0U) {
            ++s.river_sources;
            if (paths.d[c] > s.longest_river_raw) s.longest_river_raw = paths.d[c];
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
    if (!layers.has_hydrology()) {
        return std::nullopt;
    }
    const struct {
        const db::LayerGrid* grid;
        std::string_view name;
        db::ElementType type;
    } hydro[] = {
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
    return std::nullopt;
}

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

    if (!layers.has_hydrology()) {
        return out;
    }
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
        return std::nullopt;
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
    return out;
}

}  // namespace worldgen::inspect
