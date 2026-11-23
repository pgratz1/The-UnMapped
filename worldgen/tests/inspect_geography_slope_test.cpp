// The step, slope and edge-run statistics of [geography] (spec P1-00
// section 10.3, the five keys after max_step_m added on 2026-11-15, with
// appendix C items 6, 18 and 20): the three worked examples of 10.3 with
// their intermediate values, the slope definition on hand-made grids (flat
// land, planes in every direction, one-sided differences at grid edges,
// ocean neighbours as 0), the exact thresholds on both sides, tan4, the
// rank rule of step_p999_m, the thresholds of slope_p50_above_500m and
// lowland_flat_share, edge-parallel runs, the empty cases, extremes, a
// sweep of small layers of every shape against a reference written here
// from the spec, and the whole section byte for byte.
//
// Written from the spec and worldgen/include/worldgen/inspect/geography.hpp
// only. Every expected value is derived by hand in the comments.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "db_test_support.hpp"
#include "p0_04_reference.hpp"
#include "p1_00_geography_reference.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/inspect/geography.hpp"
#include "worldgen/inspect/report.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

namespace wi = worldgen::inspect;
using worldgen::FatalError;
using worldgen::db::Codec;
using worldgen::db::LayerGrid;
using worldgen::stages::testing::FixtureLayer;
using Cells = std::vector<std::int16_t>;
using Seq = std::vector<std::int64_t>;

constexpr std::int64_t U = wi::EDGE_UNDEFINED;

std::string section_of(std::uint32_t w, std::uint32_t h, const Cells& e) {
    const LayerGrid g = p100::grid(w, h, e);
    wi::GeographyLayers layers;
    layers.elevation = &g;
    std::string problem;
    const auto s = wi::geography_section(layers, &problem);
    INFO("problem: ", problem);
    REQUIRE(s.has_value());
    return *s;
}

// The value of one "<key> = <value>" line of a section.
std::string value_of(const std::string& section, std::string_view key) {
    const std::string tag = "\n" + std::string(key) + " = ";
    const std::size_t p = section.find(tag);
    REQUIRE(p != std::string::npos);
    const std::size_t b = p + tag.size();
    const std::size_t e = section.find('\n', b);
    REQUIRE(e != std::string::npos);
    return section.substr(b, e - b);
}

// The five keys, as printed, of an elevation grid.
struct Five {
    std::string p999, over, p50, flat, run;
    friend bool operator==(const Five&, const Five&) = default;
};
std::ostream& operator<<(std::ostream& os, const Five& f) {
    return os << "{" << f.p999 << " | " << f.over << " | " << f.p50 << " | " << f.flat << " | " << f.run << "}";
}
Five five_of(std::uint32_t w, std::uint32_t h, const Cells& e) {
    const std::string s = section_of(w, h, e);
    return Five{value_of(s, "step_p999_m"), value_of(s, "slope_over"), value_of(s, "slope_p50_above_500m"),
                value_of(s, "lowland_flat_share"), value_of(s, "edge_parallel_run_m")};
}

std::size_t at(std::uint32_t w, std::uint32_t x, std::uint32_t y) {
    return static_cast<std::size_t>(y) * w + x;
}

// A plane e = base + a x + b y on a w x h grid.
Cells plane(std::uint32_t w, std::uint32_t h, int base, int a, int b) {
    Cells c;
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            c.push_back(static_cast<std::int16_t>(base + a * static_cast<int>(x) + b * static_cast<int>(y)));
        }
    }
    return c;
}

// Every land cell's S2 in index order.
std::vector<std::int64_t> land_s2(const Cells& e, std::uint32_t w, std::uint32_t h) {
    std::vector<std::int64_t> v;
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            if (e[at(w, x, y)] > 0) {
                v.push_back(wi::cell_slope_s2(e, w, h, x, y));
            }
        }
    }
    return v;
}

// --- A reference of the five keys, written from P1-00 10.3 -----------------------

std::uint64_t ref_isqrt(std::uint64_t v) {
    // floor(sqrt(v)) by bisection; v < 2^63.
    std::uint64_t lo = 0;
    std::uint64_t hi = std::uint64_t{1} << 32;  // hi^2 > v
    while (hi - lo > 1) {
        const std::uint64_t mid = lo + (hi - lo) / 2;
        if (mid * mid <= v) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

std::string ref_tan4(std::int64_t s2) {
    const std::uint64_t r = ref_isqrt(static_cast<std::uint64_t>(s2) * 100000000U);
    const std::uint64_t t = (r + 160U) / 320U;
    std::string frac = std::to_string(t % 10000U);
    frac.insert(0, 4 - frac.size(), '0');
    return std::to_string(t / 10000U) + "." + frac;
}

std::int64_t ref_s2(const Cells& e, std::int64_t w, std::int64_t h, std::int64_t x, std::int64_t y) {
    auto ep = [&](std::int64_t xx, std::int64_t yy) {
        return std::max<std::int64_t>(e[static_cast<std::size_t>(yy * w + xx)], 0);
    };
    const std::int64_t xa = std::max<std::int64_t>(x - 1, 0);
    const std::int64_t xb = std::min<std::int64_t>(x + 1, w - 1);
    const std::int64_t ya = std::max<std::int64_t>(y - 1, 0);
    const std::int64_t yb = std::min<std::int64_t>(y + 1, h - 1);
    const std::int64_t gx = xb > xa ? (ep(xb, y) - ep(xa, y)) * (2 / (xb - xa)) : 0;
    const std::int64_t gy = yb > ya ? (ep(x, yb) - ep(x, ya)) * (2 / (yb - ya)) : 0;
    return gx * gx + gy * gy;
}

std::uint64_t ref_run(const Seq& t) {
    std::uint64_t best = 0;
    for (std::size_t a = 0; a < t.size(); ++a) {
        std::int64_t lo = std::numeric_limits<std::int64_t>::max();
        std::int64_t hi = std::numeric_limits<std::int64_t>::min();
        std::uint64_t r = 0;
        for (std::size_t b = a; b < t.size() && t[b] != U; ++b) {
            lo = std::min(lo, t[b]);
            hi = std::max(hi, t[b]);
            if (hi - lo > 8) {
                break;
            }
            ++r;
        }
        best = std::max(best, r);
    }
    return best;
}

Five ref_five(const Cells& e, std::uint32_t w32, std::uint32_t h32) {
    const std::int64_t w = w32;
    const std::int64_t h = h32;
    auto land = [&](std::int64_t x, std::int64_t y) { return e[static_cast<std::size_t>(y * w + x)] > 0; };
    std::vector<std::int64_t> steps, high;
    std::uint64_t n = 0, s35 = 0, s45 = 0, low = 0, flat = 0;
    for (std::int64_t y = 0; y < h; ++y) {
        for (std::int64_t x = 0; x < w; ++x) {
            if (!land(x, y)) {
                continue;
            }
            const std::int64_t v = e[static_cast<std::size_t>(y * w + x)];
            ++n;
            std::int64_t m = 0;
            const std::int64_t dx[4] = {1, 0, -1, 0};
            const std::int64_t dy[4] = {0, 1, 0, -1};
            for (int k = 0; k < 4; ++k) {
                const std::int64_t nx = x + dx[k];
                const std::int64_t ny = y + dy[k];
                if (nx >= 0 && ny >= 0 && nx < w && ny < h && land(nx, ny)) {
                    const std::int64_t d = v - e[static_cast<std::size_t>(ny * w + nx)];
                    m = std::max(m, d < 0 ? -d : d);
                }
            }
            steps.push_back(m);
            const std::int64_t s = ref_s2(e, w, h, x, y);
            // 10^8 S2 against (320 t)^2, t = 7002, 10000, 87.
            if (100000000 * s > std::int64_t{2240640} * 2240640) ++s35;
            if (100000000 * s > std::int64_t{3200000} * 3200000) ++s45;
            if (v >= 5000) high.push_back(s);
            if (v < 1000) {
                ++low;
                if (100000000 * s < std::int64_t{27840} * 27840) ++flat;
            }
        }
    }
    Five f;
    std::sort(steps.begin(), steps.end());
    std::sort(high.begin(), high.end());
    f.p999 = n == 0 ? "n/a" : wi::metres1(steps[std::min<std::uint64_t>(n - 1, 999 * n / 1000)]);
    f.over = "35:" + wi::ratio4(s35, n) + " 45:" + wi::ratio4(s45, n);
    f.p50 = high.empty() ? "n/a" : ref_tan4(high[std::min<std::size_t>(high.size() - 1, 50 * high.size() / 100)]);
    f.flat = wi::ratio4(flat, low);
    // Edge sequences.
    std::uint64_t best = 0;
    Seq west, east, north, south;
    for (std::int64_t y = 0; y < h; ++y) {
        std::int64_t l = U, r = U;
        for (std::int64_t x = 0; x < w; ++x) {
            if (land(x, y)) {
                if (l == U) l = x;
                r = x;
            }
        }
        west.push_back(l);
        east.push_back(r == U ? U : w - 1 - r);
    }
    for (std::int64_t x = 0; x < w; ++x) {
        std::int64_t t = U, b = U;
        for (std::int64_t y = 0; y < h; ++y) {
            if (land(x, y)) {
                if (t == U) t = y;
                b = y;
            }
        }
        north.push_back(t);
        south.push_back(b == U ? U : h - 1 - b);
    }
    for (const Seq* s : {&west, &east, &north, &south}) {
        best = std::max(best, ref_run(*s));
    }
    f.run = std::to_string(16U * best);
    return f;
}

// Transforms for the four sides: mirror x -> w - 1 - x swaps west and east;
// transpose (x, y) -> (y, x) swaps west with north and east with south.
Cells mirror_x(const Cells& c, std::uint32_t w, std::uint32_t h) {
    Cells m(c.size());
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            m[at(w, w - 1 - x, y)] = c[at(w, x, y)];
        }
    }
    return m;
}
Cells transpose(const Cells& c, std::uint32_t w, std::uint32_t h) {
    Cells t(c.size());
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            t[at(h, y, x)] = c[at(w, x, y)];
        }
    }
    return t;
}

// A 40-wide test coast: row y has one land cell at x = west[y] (none when
// west[y] is U), and every even row with land also has a "breaker" cell at
// x = 35. So the west sequence is exactly `west`; the east sequence
// alternates between 39 - 35 = 4 and 39 - west[y] (>= 14 for west[y] <= 25),
// which differ by more than 8, so the east side's runs are 1; and the
// north and south sequences have one defined entry per column used.
constexpr std::uint32_t CW = 40;
Cells coast(const Seq& west) {
    const auto h = static_cast<std::uint32_t>(west.size());
    Cells c(static_cast<std::size_t>(CW) * h, -10);
    for (std::uint32_t y = 0; y < h; ++y) {
        if (west[y] == U) {
            continue;
        }
        c[at(CW, static_cast<std::uint32_t>(west[y]), y)] = 50;
        if (y % 2 == 0) {
            c[at(CW, 35, y)] = 50;
        }
    }
    return c;
}

wi::EdgeRuns runs(const Cells& e, std::uint32_t w, std::uint32_t h) { return wi::edge_parallel_runs(e, w, h); }

void check_runs(const Cells& e, std::uint32_t w, std::uint32_t h, std::uint64_t west, std::uint64_t east,
                std::uint64_t north, std::uint64_t south) {
    const wi::EdgeRuns r = runs(e, w, h);
    CHECK(r.west == west);
    CHECK(r.east == east);
    CHECK(r.north == north);
    CHECK(r.south == south);
    const std::uint64_t best = std::max({west, east, north, south});
    CHECK(wi::edge_parallel_run_m(e, w, h) == 16U * best);
    CHECK(value_of(section_of(w, h, e), "edge_parallel_run_m") == std::to_string(16U * best));
}

}  // namespace

// --- The constants of the report (10.3) ----------------------------------------------

TEST_CASE("geography 10.3 slope: constants of the report") {
    CHECK(wi::SLOPE_TAN4_35_DEG == 7002);
    CHECK(wi::SLOPE_TAN4_45_DEG == 10000);
    CHECK(wi::SLOPE_TAN4_HALF_DEG == 87);
    CHECK(wi::SLOPE_HIGH_DM == 5000);
    CHECK(wi::SLOPE_LOWLAND_DM == 1000);
    CHECK(wi::EDGE_RUN_TOLERANCE == 8);
}

// --- The three worked examples of 10.3 ------------------------------------------------

TEST_CASE("geography 10.3 slope: worked example 1 (7 x 6, ocean border)") {
    const Cells e = {
        -50, -50, -50, -50, -50, -50, -50,  //
        -50, 1,   2,   2,   3,   2,   -50,  //
        -50, 2,   2,   2,   200, 400, -50,  //
        -50, 2,   3,   100, 500, 900, -50,  //
        -50, 1,   2,   3,   400, 700, -50,  //
        -50, -50, -50, -50, -50, -50, -50,  //
    };
    const std::uint32_t w = 7, h = 6;
    // The S2 values the spec lists.
    CHECK(wi::cell_slope_s2(e, w, h, 1, 1) == 8);       // (2, 2)
    CHECK(wi::cell_slope_s2(e, w, h, 2, 1) == 5);       // (1, 2)
    CHECK(wi::cell_slope_s2(e, w, h, 4, 1) == 40000);   // (0, 200), 32.0 degrees
    CHECK(wi::cell_slope_s2(e, w, h, 3, 2) == 48808);   // (198, 98), 34.6 degrees
    CHECK(wi::cell_slope_s2(e, w, h, 4, 2) == 405413);  // (398, 497)
    // Steeper than 35 and than 45 degrees: the same 9 cells.
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> steep = {{5, 1}, {4, 2}, {5, 2}, {3, 3}, {4, 3},
                                                                        {5, 3}, {3, 4}, {4, 4}, {5, 4}};
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> flat = {{2, 1}, {3, 1}, {1, 2}, {2, 2}};
    std::vector<std::pair<std::uint32_t, std::uint32_t>> got35, got45, got_flat;
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            if (e[at(w, x, y)] <= 0) {
                continue;
            }
            const std::int64_t s = wi::cell_slope_s2(e, w, h, x, y);
            if (wi::slope_steeper_than(s, wi::SLOPE_TAN4_35_DEG)) got35.emplace_back(x, y);
            if (wi::slope_steeper_than(s, wi::SLOPE_TAN4_45_DEG)) got45.emplace_back(x, y);
            if (wi::slope_flatter_than(s, wi::SLOPE_TAN4_HALF_DEG)) got_flat.emplace_back(x, y);
        }
    }
    CHECK(got35 == steep);
    CHECK(got45 == steep);
    CHECK(got_flat == flat);  // (1,1) and (1,4) at S2 8 are not flat
    const wi::SlopeStats st = wi::slope_stats(e, w, h);
    CHECK(st.land == 20U);
    CHECK(st.steeper_35 == 9U);
    CHECK(st.steeper_45 == 9U);
    CHECK(st.high == 0U);
    CHECK(!st.median_high_s2.has_value());
    CHECK(st.lowland == 20U);
    CHECK(st.lowland_flat == 4U);
    CHECK(wi::slope_over(e, w, h) == "35:0.4500 45:0.4500");
    CHECK(wi::lowland_flat_share(e, w, h) == "0.2000");
    CHECK(wi::slope_p50_above_500m(e, w, h) == "n/a");
    // Cell steps sorted: eight 1s, then 97 197 198 300 300 397 397 398 400
    // 400 500 500.
    std::vector<std::int64_t> steps = wi::land_cell_steps(e, w, h);
    std::sort(steps.begin(), steps.end());
    CHECK(steps == std::vector<std::int64_t>{1, 1, 1, 1, 1, 1, 1, 1, 97, 197, 198, 300, 300, 397, 397, 398, 400, 400,
                                             500, 500});
    CHECK(wi::permille_rank(20, 999) == 19U);  // min(19, floor(19980 / 1000))
    CHECK(wi::land_step_p999(e, w, h) == std::optional<std::int64_t>(500));
    CHECK(wi::max_land_step(e, w, h) == 500U);
    // edge_parallel_run_m, which the spec does not state for this example:
    // land is x 1..5, y 1..4. West t = (-, 1, 1, 1, 1, -): 4; east 6 - 5 = 1
    // on rows 1..4: 4; north t = 1 on columns 1..5: 5; south 5 - 4 = 1 on
    // columns 1..5: 5. 16 x 5 = 80.
    CHECK(wi::edge_distances(e, w, h, wi::MapSide::west) == Seq{U, 1, 1, 1, 1, U});
    CHECK(wi::edge_distances(e, w, h, wi::MapSide::north) == Seq{U, 1, 1, 1, 1, 1, U});
    check_runs(e, w, h, 4, 4, 5, 5);
    CHECK(five_of(w, h, e) == Five{"50.0", "35:0.4500 45:0.4500", "n/a", "0.2000", "80"});
}

TEST_CASE("geography 10.3 slope: worked example 2 (4 x 3, all land)") {
    const Cells e = {
        5000, 5010, 5040, 5100,  //
        5000, 5020, 5080, 5200,  //
        5010, 5060, 5150, 5320,  //
    };
    const std::uint32_t w = 4, h = 3;
    // At (0, 0) the one-sided differences are doubled: gx = (5010 - 5000) x 2
    // = 20, gy = (5000 - 5000) x 2 = 0, S2 400.
    CHECK(wi::cell_slope_s2(e, w, h, 0, 0) == 400);
    std::vector<std::int64_t> s = land_s2(e, w, h);
    std::sort(s.begin(), s.end());
    CHECK(s == std::vector<std::int64_t>{400, 1700, 2000, 8900, 10400, 14500, 26000, 44500, 54400, 87200, 106000,
                                         173200});
    // Steeper than 35: (3,0) (3,1) (2,2) (3,2); than 45: (3,1) (3,2).
    CHECK(wi::slope_steeper_than(wi::cell_slope_s2(e, w, h, 3, 0), 7002));
    CHECK(wi::slope_steeper_than(wi::cell_slope_s2(e, w, h, 2, 2), 7002));
    CHECK(!wi::slope_steeper_than(wi::cell_slope_s2(e, w, h, 3, 0), 10000));
    CHECK(wi::slope_steeper_than(wi::cell_slope_s2(e, w, h, 3, 1), 10000));
    CHECK(wi::slope_steeper_than(wi::cell_slope_s2(e, w, h, 3, 2), 10000));
    const wi::SlopeStats st = wi::slope_stats(e, w, h);
    CHECK(st.land == 12U);
    CHECK(st.steeper_35 == 4U);
    CHECK(st.steeper_45 == 2U);
    CHECK(st.high == 12U);
    CHECK(st.median_high_s2 == std::optional<std::int64_t>(26000));  // rank floor(600 / 100) = 6
    CHECK(st.lowland == 0U);
    CHECK(wi::slope_over(e, w, h) == "35:0.3333 45:0.1667");
    CHECK(wi::tan4(26000) == "0.5039");  // r = 1612451, T = 1612611 div 320 = 5039
    CHECK(wi::slope_p50_above_500m(e, w, h) == "0.5039");
    CHECK(wi::lowland_flat_share(e, w, h) == "n/a");
    // Not stated by the spec for this example: the largest pair step is
    // (2,2) 5150 / (3,2) 5320 = 170 (others: 5200 / 5320 120, 5080 / 5200
    // 120, 5100 / 5200 100); n 12, rank min(11, floor(11988 / 1000)) = 11:
    // 17.0. edge_parallel_run_m: all land, west and east t = 0 on 3 rows,
    // north and south t = 0 on 4 columns: 4, 64.
    check_runs(e, w, h, 3, 3, 4, 4);
    CHECK(five_of(w, h, e) == Five{"17.0", "35:0.3333 45:0.1667", "0.5039", "n/a", "64"});
}

TEST_CASE("geography 10.3 slope: worked example 3 (an edge run of 4)") {
    const Seq t = {U, 20, 21, 25, 28, 29, 30, 12, 13, 14, U, 15};
    CHECK(wi::longest_edge_run(t, 8) == 4U);
    // Starts the spec walks through: a = 1 gives 4 (20 21 25 28), a = 3 gives
    // 4, a = 7 gives 3, a = 11 gives 1.
    CHECK(wi::longest_edge_run(Seq(t.begin() + 1, t.begin() + 6), 8) == 4U);
    CHECK(wi::longest_edge_run(Seq(t.begin() + 7, t.end()), 8) == 3U);
    CHECK(wi::longest_edge_run(Seq(t.begin() + 11, t.end()), 8) == 1U);
    // As a layer: 40 x 12, one land cell (t_a, a) per row a with t_a defined.
    // West: exactly t, run 4. East: 39 - t = (-, 19, 18, 14, 11, 10, 9, 27,
    // 26, 25, -, 24): 19 18 14 11 (range 8) then 10 breaks: 4. North, per
    // column x: 12 -> 7, 13 -> 8, 14 -> 9, 15 -> 11 (range 4, run 4), 20 -> 1,
    // 21 -> 2 (2), 25 -> 3 (1), 28 -> 4, 29 -> 5, 30 -> 6 (3); other columns
    // empty: 4. South 11 - y: 4 3 2 0 on columns 12..15: 4. So it is the
    // largest side (tied) and the key is 64.
    Cells e(40U * 12U, -10);
    for (std::uint32_t a = 0; a < 12; ++a) {
        if (t[a] != U) {
            e[at(40, static_cast<std::uint32_t>(t[a]), a)] = 50;
        }
    }
    CHECK(wi::edge_distances(e, 40, 12, wi::MapSide::west) == t);
    CHECK(wi::edge_distances(e, 40, 12, wi::MapSide::east) == Seq{U, 19, 18, 14, 11, 10, 9, 27, 26, 25, U, 24});
    check_runs(e, 40, 12, 4, 4, 4, 4);
}

// --- The slope definition on hand-made grids --------------------------------------------

TEST_CASE("geography 10.3 slope: flat land") {
    // All land at one height: every gx and gy is 0, at the edges too.
    const Cells e(20, 1234);
    for (const std::int64_t s : land_s2(e, 5, 4)) {
        CHECK(s == 0);
    }
    // 1234 dm is not lowland: n/a; nothing high or steep. All land: west and
    // east t = 0 on 4 rows, north and south on 5 columns: 80.
    CHECK(five_of(5, 4, e) == Five{"0.0", "35:0.0000 45:0.0000", "n/a", "n/a", "80"});
    // The same at 123 dm: every cell lowland and flat.
    CHECK(five_of(5, 4, Cells(20, 123)).flat == "1.0000");
    // A plateau inside the ocean: only the cells whose four neighbours are
    // land are flat (x 2..3, y 2 of the 4 x 3 block x 1..4, y 1..3).
    Cells p(6U * 5U, -5);
    for (std::uint32_t y = 1; y <= 3; ++y) {
        for (std::uint32_t x = 1; x <= 4; ++x) {
            p[at(6, x, y)] = 100;
        }
    }
    CHECK(wi::cell_slope_s2(p, 6, 5, 2, 2) == 0);
    CHECK(wi::cell_slope_s2(p, 6, 5, 3, 2) == 0);
    CHECK(wi::cell_slope_s2(p, 6, 5, 1, 2) == 10000);   // gx = 100 - 0
    CHECK(wi::cell_slope_s2(p, 6, 5, 1, 1) == 20000);   // (100, 100)
    CHECK(wi::cell_slope_s2(p, 6, 5, 2, 1) == 10000);   // (0, 100)
    CHECK(wi::slope_stats(p, 6, 5).lowland_flat == 2U);  // 2 of 12: 0.1667
    CHECK(wi::lowland_flat_share(p, 6, 5) == "0.1667");
}

TEST_CASE("geography 10.3 slope: planes in each direction, inland and at the grid edge") {
    // e = 1000 + 10 x on a 5 x 4 grid: inland gx = e(x+1) - e(x-1) = 20; at
    // x = 0 and x = 4 the one-sided difference 10 is doubled to 20. So every
    // cell, edges included, has S2 = 400; gy = 0.
    const std::uint32_t w = 5, h = 4;
    struct Case {
        const char* name;
        int a;
        int b;
        std::int64_t s2;
    };
    for (const Case& c : {Case{"rising east", 10, 0, 400}, Case{"rising west", -10, 0, 400},
                          Case{"rising south", 0, 10, 400}, Case{"rising north", 0, -10, 400},
                          Case{"diagonal SE", 10, 10, 800}, Case{"diagonal NE", 10, -10, 800},
                          Case{"steeper diagonal", 30, 40, 10000}}) {
        CAPTURE(std::string_view(c.name));
        const Cells e = plane(w, h, 2000, c.a, c.b);
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                CAPTURE(x);
                CAPTURE(y);
                CHECK(wi::cell_slope_s2(e, w, h, x, y) == c.s2);
            }
        }
    }
    // A curved profile e = 1000 + 10 x^2 tells central from one-sided:
    // x 0: (1010 - 1000) x 2 = 20 -> 400; x 1: 1040 - 1000 = 40 -> 1600; x 2:
    // 1090 - 1010 = 80 -> 6400; x 3: 1160 - 1040 = 120 -> 14400; x 4: (1160 -
    // 1090) x 2 = 140 -> 19600. Three rows, gy = 0.
    Cells q;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 5; ++x) {
            q.push_back(static_cast<std::int16_t>(1000 + 10 * x * x));
        }
    }
    CHECK(land_s2(q, 5, 3) == std::vector<std::int64_t>{400, 1600, 6400, 14400, 19600, 400, 1600, 6400, 14400,
                                                        19600, 400, 1600, 6400, 14400, 19600});
    // The same profile down a column (transposed).
    CHECK(land_s2(transpose(q, 5, 3), 3, 5) ==
          std::vector<std::int64_t>{400, 400, 400, 1600, 1600, 1600, 6400, 6400, 6400, 14400, 14400, 14400, 19600,
                                    19600, 19600});
    // A 2-wide grid: xa = 0, xb = 1 for both columns, so both get (e(1) -
    // e(0)) x 2.
    const Cells two = {100, 130, 100, 130};
    CHECK(land_s2(two, 2, 2) == std::vector<std::int64_t>{3600, 3600, 3600, 3600});
    // 1-wide and 1-high grids: the missing axis contributes 0.
    const Cells col = {100, 110, 130};
    CHECK(land_s2(col, 1, 3) == std::vector<std::int64_t>{400, 900, 1600});  // 20, 30, 40
    CHECK(land_s2(col, 3, 1) == std::vector<std::int64_t>{400, 900, 1600});
}

TEST_CASE("geography 10.3 slope: ocean neighbours count as 0, a cliff beside the sea") {
    // 3 x 3: ocean in column 0, land 500 in columns 1 and 2.
    for (const std::int16_t sea : {std::int16_t{-1}, std::int16_t{-3000}, std::int16_t{-32768}}) {
        CAPTURE(sea);
        const Cells e = {sea, 500, 500, sea, 500, 500, sea, 500, 500};
        // Column 1: gx = 500 - e'(0) = 500 - 0, gy = 0 (doubled 0 at y edges):
        // S2 250000 whatever the depth. Column 2: xa = 1, xb = 2, (500 - 500)
        // x 2 = 0.
        CHECK(land_s2(e, 3, 3) == std::vector<std::int64_t>{250000, 0, 250000, 0, 250000, 0});
        CHECK(five_of(3, 3, e) == Five{"0.0", "35:0.5000 45:0.5000", "n/a", "0.5000", "48"});
    }
    // A land cell at the grid edge beside the ocean: (0,0) 300 with ocean at
    // (1,0): gx = (0 - 300) x 2 = -600.
    const Cells edge = {300, -5};
    CHECK(wi::cell_slope_s2(edge, 2, 1, 0, 0) == 360000);
    // A cell with elevation 0 is "neither" but its e' is 0 like the sea.
    CHECK(wi::cell_slope_s2(Cells{300, 0}, 2, 1, 0, 0) == 360000);
    // S2 is defined for any cell (header): an ocean cell between two land
    // cells, (1, 0): gx = 70 - 30.
    CHECK(wi::cell_slope_s2(Cells{30, -9, 70}, 3, 1, 1, 0) == 1600);
    CHECK_THROWS_AS((void)wi::cell_slope_s2(Cells{30, -9, 70}, 3, 1, 3, 0), FatalError);
    CHECK_THROWS_AS((void)wi::cell_slope_s2(Cells{30, -9, 70}, 3, 1, 0, 1), FatalError);
}

TEST_CASE("geography 10.3 slope: the 35 degree threshold on both sides") {
    // (320 x 7002)^2 = 5020467609600; 10^8 S2 > that <=> S2 > 50204.68 <=>
    // S2 >= 50205.
    CHECK(!wi::slope_steeper_than(50204, 7002));
    CHECK(wi::slope_steeper_than(50205, 7002));
    // Neither 50204 nor 50205 is a sum of two squares; the nearest integer
    // gradients are (99, 201) -> 50202 below and (103, 199) or (37, 221) ->
    // 50210 above. A 3 x 3 grid with the gradient at the centre.
    auto centre = [](int gx, int gy) {
        Cells c(9, 10000);
        c[at(3, 2, 1)] = static_cast<std::int16_t>(10000 + gx);
        c[at(3, 1, 2)] = static_cast<std::int16_t>(10000 + gy);
        return wi::cell_slope_s2(c, 3, 3, 1, 1);
    };
    CHECK(centre(99, 201) == 50202);
    CHECK(!wi::slope_steeper_than(centre(99, 201), 7002));
    CHECK(centre(103, 199) == 50210);
    CHECK(wi::slope_steeper_than(centre(103, 199), 7002));
    CHECK(centre(-37, 221) == 50210);
    CHECK(wi::slope_steeper_than(centre(-37, 221), 7002));
    // Whole grids: a plane e = 10000 + a x + b y has (gx, gy) = (2a, 2b) on
    // every cell. (112, 2): S2 4 x 12548 = 50192, below; (112, 3): 4 x 12553 =
    // 50212, above. All five keys: steps max(112, 3) = 112 on every cell
    // (11.2), nothing lowland, everything high with S2 the same: tan4; all
    // land, 5 columns: 80.
    const Cells below = plane(5, 4, 10000, 112, 2);
    const Cells above = plane(5, 4, 10000, 112, 3);
    CHECK(land_s2(below, 5, 4) == std::vector<std::int64_t>(20, 50192));
    CHECK(land_s2(above, 5, 4) == std::vector<std::int64_t>(20, 50212));
    // tan4(50192): r = isqrt(5019200000000) = 2240357, T = 2240517 div 320 =
    // 7001; tan4(50212): r = 2240803, T = 2240963 div 320 = 7003.
    CHECK(five_of(5, 4, below) == Five{"11.2", "35:0.0000 45:0.0000", "0.7001", "n/a", "80"});
    CHECK(five_of(5, 4, above) == Five{"11.2", "35:1.0000 45:0.0000", "0.7003", "n/a", "80"});
}

TEST_CASE("geography 10.3 slope: the 45 degree threshold, S2 = 102400 is not steeper") {
    // (320 x 10000)^2 = 1.024 x 10^13 = 10^8 x 102400 exactly: not greater.
    CHECK(!wi::slope_steeper_than(102400, 10000));
    CHECK(wi::slope_steeper_than(102401, 10000));
    // Planes: (160, 0) -> gx 320, S2 102400 (exactly 45 degrees); (160, 1)
    // -> 4 x 25601 = 102404.
    const Cells exact = plane(4, 3, 8000, 160, 0);
    const Cells over = plane(4, 3, 8000, 160, 1);
    CHECK(land_s2(exact, 4, 3) == std::vector<std::int64_t>(12, 102400));
    CHECK(land_s2(over, 4, 3) == std::vector<std::int64_t>(12, 102404));
    CHECK(wi::slope_over(exact, 4, 3) == "35:1.0000 45:0.0000");
    CHECK(wi::slope_over(over, 4, 3) == "35:1.0000 45:1.0000");
    CHECK(wi::slope_p50_above_500m(exact, 4, 3) == "1.0000");  // r 3200000, T 10000
    // tan4(102404): r = isqrt(10240400000000) = 3200062, T = 3200222 div 320
    // = 10000: still 1.0000, the four decimals do not show 45.0006 degrees.
    CHECK(wi::slope_p50_above_500m(over, 4, 3) == "1.0000");
}

TEST_CASE("geography 10.3 slope: the 0.5 degree threshold, S2 7 is flat and 8 is not") {
    // (320 x 87)^2 = 775065600 = 10^8 x 7.75: flatter <=> S2 <= 7.
    CHECK(wi::slope_flatter_than(7, 87));
    CHECK(!wi::slope_flatter_than(8, 87));
    CHECK(wi::slope_flatter_than(0, 87));
    // 7 is not a sum of two squares, so no integer gradient gives it: the
    // flat S2 values that occur are 0 1 2 4 5, i.e. (gx, gy) up to (2, 1).
    // (2, 2) gives 8 and is not flat although it changes by 2 dm over 32 m
    // in each axis (10.3 says "at most 2 dm over 32 m in each axis";
    // reported as an imprecision).
    struct Case {
        int a, b;
        std::int64_t s2;
        bool flat;
    };
    for (const Case& c : {Case{0, 0, 0, true}, Case{1, 0, 4, true}, Case{0, 1, 4, true}, Case{1, 1, 8, false},
                          Case{-1, 1, 8, false}}) {
        CAPTURE(c.a);
        CAPTURE(c.b);
        // Plane with (gx, gy) = (2a, 2b), lowland at 100 dm.
        const Cells e = plane(5, 4, 100, c.a, c.b);
        CHECK(land_s2(e, 5, 4) == std::vector<std::int64_t>(20, c.s2));
        CHECK(wi::lowland_flat_share(e, 5, 4) == (c.flat ? "1.0000" : "0.0000"));
    }
    // Odd gradients from a 3 x 3 centre: (2, 1) -> 5 flat, (1, 1) -> 2 flat,
    // (3, 0) -> 9 not flat.
    auto centre = [](int gx, int gy) {
        Cells c(9, 100);
        c[at(3, 2, 1)] = static_cast<std::int16_t>(100 + gx);
        c[at(3, 1, 2)] = static_cast<std::int16_t>(100 + gy);
        return wi::cell_slope_s2(c, 3, 3, 1, 1);
    };
    CHECK(centre(2, 1) == 5);
    CHECK(wi::slope_flatter_than(centre(2, 1), 87));
    CHECK(centre(1, 1) == 2);
    CHECK(wi::slope_flatter_than(centre(1, 1), 87));
    CHECK(centre(3, 0) == 9);
    CHECK(!wi::slope_flatter_than(centre(3, 0), 87));
    CHECK(centre(2, 2) == 8);
    CHECK(!wi::slope_flatter_than(centre(2, 2), 87));
}

TEST_CASE("geography 10.3 slope: steeper and flatter for other tangents, and their contracts") {
    // t = 0: steeper <=> S2 > 0; flatter never.
    CHECK(!wi::slope_steeper_than(0, 0));
    CHECK(wi::slope_steeper_than(1, 0));
    CHECK(!wi::slope_flatter_than(0, 0));
    // t = 10^5 (tangent 10, 84.3 degrees): (320 x 10^5)^2 / 10^8 = 10240000.
    CHECK(!wi::slope_steeper_than(10240000, 100000));
    CHECK(wi::slope_steeper_than(10240001, 100000));
    CHECK(wi::slope_flatter_than(10239999, 100000));
    CHECK(!wi::slope_flatter_than(10240000, 100000));
    // Neither steeper nor flatter at equality.
    CHECK(!wi::slope_steeper_than(102400, 10000));
    CHECK(!wi::slope_flatter_than(102400, 10000));
    // The largest S2 a layer can give (2 x 65534^2) and the bound 2^34.
    const std::int64_t big = std::int64_t{2} * 65534 * 65534;
    CHECK(wi::slope_steeper_than(big, 100000));
    CHECK(wi::slope_steeper_than((std::int64_t{1} << 34) - 1, 100000));
    CHECK_THROWS_AS((void)wi::slope_steeper_than(std::int64_t{1} << 34, 7002), FatalError);
    CHECK_THROWS_AS((void)wi::slope_steeper_than(-1, 7002), FatalError);
    CHECK_THROWS_AS((void)wi::slope_flatter_than(-1, 87), FatalError);
    CHECK_THROWS_AS((void)wi::slope_steeper_than(5, 100001), FatalError);
    CHECK_THROWS_AS((void)wi::slope_flatter_than(5, -1), FatalError);
}

TEST_CASE("geography 10.3 slope: tan4 to four decimals") {
    // r = isqrt(S2 10^8), T = (r + 160) div 320 (round half up of r / 320).
    CHECK(wi::tan4(0) == "0.0000");
    CHECK(wi::tan4(1) == "0.0031");   // r 10000, T 10160 div 320 = 31 (1/320 = 0.003125)
    CHECK(wi::tan4(4) == "0.0063");   // r 20000 = 62 x 320 + 160: exactly half, up to 63
    CHECK(wi::tan4(627) == "0.0782");  // r 250399 = 782 x 320 + 159: down
    CHECK(wi::tan4(8) == "0.0088");   // r 28284, T 28444 div 320 = 88
    CHECK(wi::tan4(26000) == "0.5039");
    CHECK(wi::tan4(50205) == "0.7002");  // r 2240647
    CHECK(wi::tan4(102400) == "1.0000");
    CHECK(wi::tan4(132500) == "1.1375");
    CHECK(wi::tan4(400009801) == "62.5008");
    // The largest S2 of an i16 layer: 2 x 65534^2 = 8589410312, r =
    // 926790715, T = 926790875 div 320 = 2896221.
    CHECK(wi::tan4(8589410312) == "289.6221");
    // The bound: 2^34 - 1, r = 1310719999, T = 1310720159 div 320 = 4096000.
    CHECK(wi::tan4((std::int64_t{1} << 34) - 1) == "409.6000");
    CHECK_THROWS_AS((void)wi::tan4(-1), FatalError);
    CHECK_THROWS_AS((void)wi::tan4(std::int64_t{1} << 34), FatalError);
    // Against the reference written here, over a range.
    for (std::int64_t s = 0; s < 3000; ++s) {
        CAPTURE(s);
        CHECK(wi::tan4(s) == ref_tan4(s));
    }
}

// --- step_p999_m (10.3 "Cell step") ---------------------------------------------------------

TEST_CASE("geography 10.3 step: the per-mille rank rule") {
    // min(n - 1, floor(999 n / 1000)).
    CHECK(wi::permille_rank(1, 999) == 0U);
    CHECK(wi::permille_rank(999, 999) == 998U);    // floor(998.001)
    CHECK(wi::permille_rank(1000, 999) == 999U);   // floor(999), the last
    CHECK(wi::permille_rank(1001, 999) == 999U);   // floor(999.999): second largest
    CHECK(wi::permille_rank(2000, 999) == 1998U);  // second largest
    CHECK(wi::permille_rank(2001, 999) == 1998U);  // floor(1998.999): third largest
    CHECK(wi::permille_rank(3000, 999) == 2997U);
    CHECK(wi::permille_rank(10, 0) == 0U);
    CHECK(wi::permille_rank(10, 1000) == 9U);
    CHECK(wi::permille_rank(std::uint64_t{2048} * 2048, 999) == 4190109U);  // floor(4190109.696)
    CHECK_THROWS_AS((void)wi::permille_rank(0, 999), FatalError);
    CHECK_THROWS_AS((void)wi::permille_rank(10, 1001), FatalError);
}

TEST_CASE("geography 10.3 step: step_p999_m at n = 1, 999, 1000, 1001, 2000 and 2001 land cells") {
    // A row of n land cells: 600, then 100 ..., then 1100 at the end. Cell
    // steps: cell 0 |600 - 100| = 500, cell 1 max(500, 0) = 500, cells 2 to
    // n - 3 0, cell n - 2 max(0, 1000) = 1000, cell n - 1 1000. Sorted: n - 4
    // zeros, 500 500 1000 1000 (indices n-4 .. n-1). Every pair step is shared
    // by its two cells, so the largest m(c) always occurs at least twice and
    // step_p999_m equals max_step_m while at most one value lies above the
    // rank, that is for every n <= 2000 (not only n < 1000).
    auto row = [](std::uint32_t n) {
        Cells c(n, 100);
        c.front() = 600;
        c.back() = 1100;
        return c;
    };
    struct Case {
        std::uint32_t n;
        std::int64_t want;  // decimetres
    };
    // n 999: rank 998 = n - 1 -> 1000. n 1000: 999 -> 1000. n 1001: 999 =
    // n - 2 -> 1000. n 2000: 1998 = n - 2 -> 1000. n 2001: 1998 = n - 3 ->
    // 500, the first value that differs from max_step_m.
    for (const Case& c : {Case{999, 1000}, Case{1000, 1000}, Case{1001, 1000}, Case{2000, 1000}, Case{2001, 500}}) {
        CAPTURE(c.n);
        const Cells e = row(c.n);
        CHECK(wi::max_land_step(e, c.n, 1) == 1000U);
        CHECK(wi::land_step_p999(e, c.n, 1) == std::optional<std::int64_t>(c.want));
        const std::string s = section_of(c.n, 1, e);
        CHECK(value_of(s, "max_step_m") == "100.0");
        CHECK(value_of(s, "step_p999_m") == wi::metres1(c.want));
    }
    // n = 1: the only cell has no land neighbour: 0.
    CHECK(wi::land_step_p999(Cells{100}, 1, 1) == std::optional<std::int64_t>(0));
    // Ocean cells do not count in n: the 2001-cell row with an ocean row
    // below it (2001 x 2) is still 2001 land cells, 50.0.
    Cells two_rows = row(2001);
    two_rows.insert(two_rows.end(), 2001, -7);
    CHECK(value_of(section_of(2001, 2, two_rows), "step_p999_m") == "50.0");
    // With 2001 cells and a single 1000 step (no 500): the rank 1998 is 0.
    Cells one_step(2001, 100);
    one_step.back() = 1100;
    CHECK(wi::land_step_p999(one_step, 2001, 1) == std::optional<std::int64_t>(0));
    CHECK(wi::max_land_step(one_step, 2001, 1) == 1000U);
}

TEST_CASE("geography 10.3 step: cell steps, equal steps, ocean and zero neighbours") {
    // A 4 x 4 checkerboard of 100 and 300: every cell's step is 200.
    Cells e;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            e.push_back(static_cast<std::int16_t>((x + y) % 2 == 0 ? 100 : 300));
        }
    }
    CHECK(wi::land_cell_steps(e, 4, 4) == std::vector<std::int64_t>(16, 200));
    CHECK(wi::land_step_p999(e, 4, 4) == std::optional<std::int64_t>(200));
    // Index order, land cells only; ocean and 0 cells are not neighbours:
    // 3 x 2: (0,0) 50 | (1,0) -9 | (2,0) 70 / (0,1) 0 | (1,1) 80 | (2,1) 75.
    // (0,0): neighbours (1,0) ocean, (0,1) zero: 0. (2,0): (2,1) 75 -> 5.
    // (1,1): (2,1) 75 -> 5 (ocean above, zero left). (2,1): max(5, 5) = 5.
    const Cells m = {50, -9, 70, 0, 80, 75};
    CHECK(wi::land_cell_steps(m, 3, 2) == std::vector<std::int64_t>{0, 5, 5, 5});
    CHECK(wi::land_cell_steps(Cells{-1, 0}, 2, 1).empty());
    CHECK(!wi::land_step_p999(Cells{-1, 0}, 2, 1).has_value());
}

// --- slope_p50_above_500m and lowland_flat_share --------------------------------------------

TEST_CASE("geography 10.3 slope: slope_p50_above_500m, the 500 m threshold and the median rank") {
    // 1-high rows; a high cell x between two low cells L and R has gx = R - L
    // (gy = 0), whatever its own height. Cells: (1, 5000, 101) gives the 5000
    // cell gx 100 -> S2 10000; (1, 6000, 201) gx 200 -> 40000; (1, 7000, 401)
    // gx 400 -> 160000; (1, 8000, 1601) gx 1600 -> 2560000. The low cells are
    // below 5000 dm.
    const Cells three = {1, 5000, 101, 1, 6000, 201, 1, 7000, 401};
    // e >= 5000 includes the cell at exactly 5000: n 3, S2 sorted 10000 40000
    // 160000, rank floor(150 / 100) = 1: 40000; r = 2000000, T = 2000160 div
    // 320 = 6250: 0.6250.
    CHECK(wi::slope_stats(three, 9, 1).high == 3U);
    CHECK(wi::slope_stats(three, 9, 1).median_high_s2 == std::optional<std::int64_t>(40000));
    CHECK(wi::slope_p50_above_500m(three, 9, 1) == "0.6250");
    // The same with 4999 instead of 5000: that cell drops out, n 2 (even),
    // S2 40000 160000, rank floor(100 / 100) = 1: 160000, r 4000000, T
    // 4000160 div 320 = 12500: 1.2500.
    Cells below = three;
    below[1] = 4999;
    CHECK(wi::slope_stats(below, 9, 1).high == 2U);
    CHECK(wi::slope_p50_above_500m(below, 9, 1) == "1.2500");
    // Four cells (even): 10000 40000 160000 2560000, rank 2: 160000, 1.2500.
    Cells four = three;
    for (const std::int16_t v : {std::int16_t{1}, std::int16_t{8000}, std::int16_t{1601}}) {
        four.push_back(v);
    }
    CHECK(wi::slope_stats(four, 12, 1).median_high_s2 == std::optional<std::int64_t>(160000));
    CHECK(wi::slope_p50_above_500m(four, 12, 1) == "1.2500");
    // One cell above: (1, 5000, 101): S2 10000, r 1000000, T 1000160 div 320
    // = 3125: 0.3125.
    CHECK(wi::slope_p50_above_500m(Cells{1, 5000, 101}, 3, 1) == "0.3125");
    // Land but none at or above 500 m: n/a.
    CHECK(wi::slope_p50_above_500m(Cells{1, 4999, 101}, 3, 1) == "n/a");
    CHECK(!wi::slope_stats(Cells{1, 4999, 101}, 3, 1).median_high_s2.has_value());
    // In the section.
    CHECK(value_of(section_of(9, 1, three), "slope_p50_above_500m") == "0.6250");
    CHECK(value_of(section_of(3, 1, Cells{1, 4999, 101}), "slope_p50_above_500m") == "n/a");
}

TEST_CASE("geography 10.3 slope: lowland_flat_share, the 100 m threshold") {
    // Row 999 999 999 1000 1050: x0 (999 - 999) x 2 = 0; x1 999 - 999 = 0;
    // x2 1000 - 999 = 1 -> S2 1; all flat. The cell at exactly 1000 is not
    // lowland (e < 1000): lowland 3, flat 3: 1.0000.
    const Cells a = {999, 999, 999, 1000, 1050};
    const wi::SlopeStats sa = wi::slope_stats(a, 5, 1);
    CHECK(sa.lowland == 3U);
    CHECK(sa.lowland_flat == 3U);
    CHECK(wi::lowland_flat_share(a, 5, 1) == "1.0000");
    // With 999 there instead: it is lowland, gx = 1050 - 999 = 51, S2 2601,
    // not flat: 3 of 4, 0.7500.
    const Cells b = {999, 999, 999, 999, 1050};
    CHECK(wi::slope_stats(b, 5, 1).lowland == 4U);
    CHECK(wi::lowland_flat_share(b, 5, 1) == "0.7500");
    // Land, but none below 100 m: n/a.
    CHECK(wi::lowland_flat_share(Cells{1000, 1000, 1000}, 3, 1) == "n/a");
    CHECK(wi::slope_stats(Cells{1000, 1000, 1000}, 3, 1).lowland == 0U);
    // All lowland, none flat: 100 200 300, gx 200 everywhere (doubled at
    // the ends), S2 40000.
    CHECK(wi::lowland_flat_share(Cells{100, 200, 300}, 3, 1) == "0.0000");
    // All flat.
    CHECK(wi::lowland_flat_share(Cells{5, 5, 5, 5}, 2, 2) == "1.0000");
    CHECK(value_of(section_of(5, 1, b), "lowland_flat_share") == "0.7500");
}

// --- edge_parallel_run_m (10.3 "Edge-parallel run") -----------------------------------------

TEST_CASE("geography 10.3 edge: a straight coast of known length on each of the four sides") {
    // coast(): a spine at x = 5 on rows 10 .. 10 + L - 1 of a 40 x 40 grid,
    // with breakers at x = 35 on even rows. West: t = 5 on L rows: L. East:
    // 4 and 34 alternate: 1. North: column 5 -> 10, column 35 -> 10, not
    // adjacent: 1. South: 1. The key is 16 L. Mirrored, the east side has L;
    // transposed, the north side; transposed after mirroring, the south.
    for (const std::uint32_t len : {1U, 2U, 7U, 20U, 30U}) {
        CAPTURE(len);
        Seq t(40, U);
        for (std::uint32_t y = 10; y < 10 + len; ++y) {
            t[y] = 5;
        }
        const Cells w = coast(t);
        CHECK(wi::edge_distances(w, CW, 40, wi::MapSide::west) == t);
        const std::uint64_t one = 1;
        check_runs(w, CW, 40, len, one, one, one);
        const Cells e = mirror_x(w, CW, 40);
        check_runs(e, CW, 40, one, len, one, one);
        const Cells n = transpose(w, CW, 40);
        check_runs(n, 40, CW, one, one, len, one);
        const Cells s = transpose(e, CW, 40);
        check_runs(s, 40, CW, one, one, one, len);
    }
}

TEST_CASE("geography 10.3 edge: wiggles of exactly the tolerance and one cell beyond") {
    // Spine on rows 10..29 alternating x = 5 (even rows) and x = 5 + d (odd).
    // Breakers on even rows: east alternates 4 with 34 or 34 - d: 1. North
    // and south: columns 5 and 5 + d each hold one value, not adjacent for d
    // >= 2: 1.
    auto wiggle = [](std::int64_t d) {
        Seq t(40, U);
        for (std::int64_t y = 10; y < 30; ++y) {
            t[static_cast<std::size_t>(y)] = (y % 2 == 0) ? 5 : 5 + d;
        }
        return t;
    };
    check_runs(coast(wiggle(8)), CW, 40, 20, 1, 1, 1);  // range 8: the whole spine
    check_runs(coast(wiggle(9)), CW, 40, 1, 1, 1, 1);   // range 9: every pair breaks
    // A single deviation on a straight spine: row 20 at x = 13 (range 8)
    // keeps 20 rows; at x = 14 the run is rows 10..19 (10) or 21..29 (9).
    Seq t(40, U);
    for (std::size_t y = 10; y < 30; ++y) {
        t[y] = 5;
    }
    t[20] = 13;
    check_runs(coast(t), CW, 40, 20, 1, 1, 1);
    t[20] = 14;
    check_runs(coast(t), CW, 40, 10, 1, 1, 1);
    // A deviation inward and back that stays in tolerance: 5 9 13 9 5 ... .
    CHECK(wi::longest_edge_run(Seq{5, 9, 13, 9, 5, 5}, 8) == 6U);
    CHECK(wi::longest_edge_run(Seq{5, 9, 14, 9, 5, 5}, 8) == 3U);  // 9 14 9, or 9 5 5; 14 9 5 spans 9
}

TEST_CASE("geography 10.3 edge: a ramping coast") {
    // t = 5 + k on rows 10 + k, k = 0..19: any 9 consecutive values span 8,
    // so the west run is 9. The diagonal also ramps by 1 in the columns:
    // column 5 + k has its only spine cell at row 10 + k, so north t = 10 + k
    // (9) and south 39 - (10 + k) (9); column 35 holds even rows 10..28, alone
    // (column 34 is empty): 1. East 1. Key 16 x 9 = 144.
    Seq t(40, U);
    for (std::int64_t k = 0; k < 20; ++k) {
        t[static_cast<std::size_t>(10 + k)] = 5 + k;
    }
    check_runs(coast(t), CW, 40, 9, 1, 9, 9);
    // A ramp of 2 per row, t = 5 + 2k on rows 10..19: windows of 5 values
    // (span 8): west 5; the columns used (5, 7, ..., 23) are never adjacent,
    // so north and south are 1. Key 80.
    Seq t2(40, U);
    for (std::int64_t k = 0; k < 10; ++k) {
        t2[static_cast<std::size_t>(10 + k)] = 5 + 2 * k;
    }
    check_runs(coast(t2), CW, 40, 5, 1, 1, 1);
}

TEST_CASE("geography 10.3 edge: lines with no land break a run") {
    // The 20-row spine without row 20 (no land there at all): 10 and 9.
    Seq t(40, U);
    for (std::size_t y = 10; y < 30; ++y) {
        t[y] = 5;
    }
    t[20] = U;
    CHECK(wi::edge_distances(coast(t), CW, 40, wi::MapSide::west) == t);
    check_runs(coast(t), CW, 40, 10, 1, 1, 1);
    // Directly on sequences.
    CHECK(wi::longest_edge_run(Seq{}, 8) == 0U);
    CHECK(wi::longest_edge_run(Seq{U, U, U}, 8) == 0U);
    CHECK(wi::longest_edge_run(Seq{3, U, 3, 3, U, 3}, 8) == 2U);
    CHECK(wi::longest_edge_run(Seq{0, 0, 0}, 0) == 3U);
    CHECK(wi::longest_edge_run(Seq{0, 1, 1, 0}, 0) == 2U);
    CHECK(wi::longest_edge_run(Seq{0, 1, 2, 3}, 1) == 2U);
    // The header's contract: entries other than U outside [0, 2^32] and a
    // negative tolerance are fatal.
    CHECK_THROWS_AS((void)wi::longest_edge_run(Seq{0, -2}, 8), FatalError);
    CHECK_THROWS_AS((void)wi::longest_edge_run(Seq{0, (std::int64_t{1} << 32) + 1}, 8), FatalError);
    CHECK_THROWS_AS((void)wi::longest_edge_run(Seq{0}, -1), FatalError);
}

TEST_CASE("geography 10.3 edge: the outermost land of each line, offshore islets and inland water") {
    // The definition takes the smallest x with land in the row, whatever is
    // connected to what: an offshore islet becomes the coast of its row.
    Seq t(40, U);
    for (std::size_t y = 10; y < 30; ++y) {
        t[y] = 15;
    }
    Cells e = coast(t);
    check_runs(e, CW, 40, 20, 1, 1, 1);
    // An islet at (8, 20), 7 cells off the coast: within tolerance, 20.
    // (Column 8 holds one value: north and south stay 1.)
    e[at(CW, 8, 20)] = 50;
    check_runs(e, CW, 40, 20, 1, 1, 1);
    // An islet at (2, 20), 13 off: the row's t is 2 and the run splits into
    // rows 10..19 (10) and 21..29 (9).
    e[at(CW, 8, 20)] = -10;
    e[at(CW, 2, 20)] = 50;
    CHECK(wi::edge_distances(e, CW, 40, wi::MapSide::west)[20] == 2);
    check_runs(e, CW, 40, 10, 1, 1, 1);
    // Inland water does not change any t: a block x 5..12, rows 10..29, with
    // an ocean hole at (8, 15) and an inland cell of elevation 0 at (9, 20);
    // lakes are land (e > 0) anyway. West t = 5 on 20 rows: 20; north t = 10
    // on columns 5..12: 8; south 39 - 29 = 10: 8; east 4 (breakers on even
    // rows) against 27: 1.
    Seq none(40, U);
    Cells b = coast(none);
    for (std::uint32_t y = 10; y < 30; ++y) {
        for (std::uint32_t x = 5; x <= 12; ++x) {
            b[at(CW, x, y)] = 50;
        }
        if (y % 2 == 0) {
            b[at(CW, 35, y)] = 50;
        }
    }
    check_runs(b, CW, 40, 20, 1, 8, 8);
    b[at(CW, 8, 15)] = -10;
    b[at(CW, 9, 20)] = 0;
    check_runs(b, CW, 40, 20, 1, 8, 8);
    // A one-cell bay at the coast itself does move t: (5, 15) ocean makes
    // row 15's t 6, still within tolerance.
    b[at(CW, 5, 15)] = -10;
    CHECK(wi::edge_distances(b, CW, 40, wi::MapSide::west)[15] == 6);
    check_runs(b, CW, 40, 20, 1, 8, 8);
}

TEST_CASE("geography 10.3 edge: 1-wide and 1-high grids") {
    // 1 x 10, all land: west and east t = 0 on 10 rows: 10; north and south
    // have one column: 1. 160.
    check_runs(Cells(10, 7), 1, 10, 10, 10, 1, 1);
    check_runs(Cells(10, 7), 10, 1, 1, 1, 10, 10);
    // Ocean at row 4: runs 4 (rows 0..3) and 5 (rows 5..9); the column's
    // north t = 0, south t = 0.
    Cells c(10, 7);
    c[4] = -7;
    check_runs(c, 1, 10, 5, 5, 1, 1);
    check_runs(c, 10, 1, 1, 1, 5, 5);
    CHECK(wi::edge_distances(c, 1, 10, wi::MapSide::north) == Seq{0});
    CHECK(wi::edge_distances(c, 1, 10, wi::MapSide::south) == Seq{0});
    CHECK(wi::edge_distances(c, 1, 10, wi::MapSide::east) == Seq{0, 0, 0, 0, U, 0, 0, 0, 0, 0});
    // A 1 x 1 land cell: every side 1, 16 m.
    check_runs(Cells{3}, 1, 1, 1, 1, 1, 1);
}

// --- Empty cases (appendix C 6) and elevation 0 (appendix C 3) -------------------------------------

TEST_CASE("geography appendix C 6: the five keys with no land, no ocean, one cell, elevation 0") {
    // No land: step n/a (n_L = 0); slope_over is "35:" ratio4(0, 0) and
    // "45:" ratio4(0, 0), each n/a by appendix C 6 (the same reading as
    // land_above_m in item 18); no cell >= 500 m; lowland denominator 0; no
    // land, run 0.
    const Five none{"n/a", "35:n/a 45:n/a", "n/a", "n/a", "0"};
    CHECK(five_of(3, 3, Cells(9, -5)) == none);
    CHECK(five_of(1, 1, Cells{-32768}) == none);
    CHECK(five_of(4, 2, Cells(8, 0)) == none);  // elevation 0 only: neither land nor ocean
    CHECK(five_of(2, 2, Cells{0, -1, -1, 0}) == none);
    CHECK(wi::edge_parallel_run_m(Cells(9, -5), 3, 3) == 0U);
    const wi::EdgeRuns r = wi::edge_parallel_runs(Cells(9, 0), 3, 3);
    CHECK((r.west == 0U && r.east == 0U && r.north == 0U && r.south == 0U));
    CHECK(wi::edge_distances(Cells(9, 0), 3, 3, wi::MapSide::west) == Seq{U, U, U});
    const wi::SlopeStats st = wi::slope_stats(Cells(9, -5), 3, 3);
    CHECK(st.land == 0U);
    CHECK(st.high == 0U);
    CHECK(st.lowland == 0U);
    CHECK(!st.median_high_s2.has_value());
    // No ocean: 3 x 2 all 6000 -> steps 0, S2 0, high median tan4(0); no
    // lowland; every side t = 0: north and south 3, west and east 2: 48.
    CHECK(five_of(3, 2, Cells(6, 6000)) == Five{"0.0", "35:0.0000 45:0.0000", "0.0000", "n/a", "48"});
    // One land cell of 32767.
    CHECK(five_of(1, 1, Cells{32767}) == Five{"0.0", "35:0.0000 45:0.0000", "0.0000", "n/a", "16"});
}

TEST_CASE("geography appendix C 3: elevation 0 cells are neither land nor a land neighbour") {
    // 3 x 1: 5, 0, 900. Steps: neither land cell has a land 4-neighbour: 0
    // and 0 (rank 1 of 2): 0.0. Slopes: x 0: (e'(1) - e'(0)) x 2 = (0 - 5) x
    // 2 = -10 -> 100; x 2: (900 - 0) x 2 = 1800 -> 3240000, steeper than both:
    // 1 of 2, 0.5000. Both lowland, neither flat: 0.0000. Edges: west t = 0,
    // east 0 (one row): 1; north t = 0 on columns 0 and 2, column 1 empty:
    // 1; south 1. 16.
    const Cells e = {5, 0, 900};
    CHECK(wi::land_cell_steps(e, 3, 1) == std::vector<std::int64_t>{0, 0});
    CHECK(land_s2(e, 3, 1) == std::vector<std::int64_t>{100, 3240000});
    CHECK(five_of(3, 1, e) == Five{"0.0", "35:0.5000 45:0.5000", "n/a", "0.0000", "16"});
    CHECK(wi::edge_distances(e, 3, 1, wi::MapSide::north) == Seq{0, U, 0});
}

// --- Extremes ------------------------------------------------------------------------------

TEST_CASE("geography 10.3 slope: the widest i16 steps and slopes") {
    // 2 x 2: 1 32767 / 32767 1. Every cell: gx and gy are one-sided and
    // doubled, +-(32767 - 1) x 2 = +-65532, S2 2 x 65532^2 = 8588886048. Every
    // cell step is 32766. The two high cells (32767) give the median at rank
    // 1 of 2: r = isqrt(858888604800000000) = 926762431, T = 926762591 div
    // 320 = 2896133: 289.6133. The two 1 dm cells are lowland, not flat.
    const Cells a = {1, 32767, 32767, 1};
    CHECK(land_s2(a, 2, 2) == std::vector<std::int64_t>(4, 8588886048));
    CHECK(five_of(2, 2, a) == Five{"3276.6", "35:1.0000 45:1.0000", "289.6133", "0.0000", "32"});
    // The largest S2 possible: a land cell of 32767 at a grid corner whose two
    // neighbours are ocean (e' 0): gx = gy = (0 - 32767) x 2 = -65534, S2 2 x
    // 65534^2 = 8589410312 (< 2^33). tan4 289.6221. No land neighbour: step 0.
    const Cells b = {32767, -32768, -32768, -32768};
    CHECK(wi::cell_slope_s2(b, 2, 2, 0, 0) == 8589410312);
    CHECK(five_of(2, 2, b) == Five{"0.0", "35:1.0000 45:1.0000", "289.6221", "n/a", "16"});
    // An inland cell between the sea and a 32767 peak: gx = 32767 - 0, not
    // doubled.
    CHECK(wi::cell_slope_s2(Cells{-32768, 1, 32767}, 3, 1, 1, 0) == std::int64_t{32767} * 32767);
    // max step and p999 at the widest step.
    CHECK(wi::land_step_p999(Cells{32767, 1, -32768}, 3, 1) == std::optional<std::int64_t>(32766));
}

TEST_CASE("geography 10.3 slope: a ridge one cell wide") {
    // 7 x 5, all land 100 except column 3 at 6000. Central differences do
    // not see a cell's own height, so the ridge crest has gx = e(4) - e(2) =
    // 0 and gy = 0: S2 0. Its flanks (columns 2 and 4) have gx = +-5900, S2
    // 34810000. Columns 0, 1, 5, 6: 0 (doubled 0 at the edges).
    Cells e(35, 100);
    for (std::uint32_t y = 0; y < 5; ++y) {
        e[at(7, 3, y)] = 6000;
    }
    for (std::uint32_t y = 0; y < 5; ++y) {
        CHECK(wi::cell_slope_s2(e, 7, 5, 3, y) == 0);
        CHECK(wi::cell_slope_s2(e, 7, 5, 2, y) == 34810000);
        CHECK(wi::cell_slope_s2(e, 7, 5, 4, y) == 34810000);
        CHECK(wi::cell_slope_s2(e, 7, 5, 0, y) == 0);
    }
    // Steps: 5900 on columns 2, 3, 4 (15 cells), 0 elsewhere: n 35, rank 34:
    // 590.0. Steeper: the 10 flank cells, ratio4(10, 35) = 0.2857 for both.
    // High: the 5 crest cells, all S2 0: median tan4(0) = 0.0000, although
    // the ridge stands 590 m above its flanks. Lowland: the 30 other cells,
    // flat those of columns 0, 1, 5, 6 = 20: 0.6667. All land: west and east
    // 5, north and south 7: 112.
    CHECK(five_of(7, 5, e) == Five{"590.0", "35:0.2857 45:0.2857", "0.0000", "0.6667", "112"});
}

TEST_CASE("geography 10.3 slope: nothing is fatal on any valid i16 layer, and every key matches the reference") {
    // Every shape from 1 x 1 to 7 x 7 and a few long ones, with values from a
    // fixed LCG mixed with the extremes and the thresholds; each key against
    // the reference written above from 10.3.
    const std::int16_t specials[] = {-32768, -1, 0, 1, 2, 999, 1000, 4999, 5000, 32767, 32766, 100, 101};
    std::uint64_t state = 0x9E3779B97F4A7C15ULL;
    auto next = [&]() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(state >> 33);
    };
    std::vector<std::pair<std::uint32_t, std::uint32_t>> shapes;
    for (std::uint32_t w = 1; w <= 7; ++w) {
        for (std::uint32_t h = 1; h <= 7; ++h) {
            shapes.emplace_back(w, h);
        }
    }
    for (const auto& s : {std::pair<std::uint32_t, std::uint32_t>{1, 23}, {23, 1}, {2, 17}, {17, 3}, {12, 12}}) {
        shapes.push_back(s);
    }
    int checked = 0;
    for (const auto& [w, h] : shapes) {
        for (int trial = 0; trial < 6; ++trial) {
            Cells e(static_cast<std::size_t>(w) * h);
            for (std::int16_t& v : e) {
                const std::uint32_t r = next();
                switch (trial % 3) {
                    case 0:  // anything
                        v = static_cast<std::int16_t>(static_cast<std::int32_t>(r % 65536U) - 32768);
                        break;
                    case 1:  // specials
                        v = specials[r % (sizeof specials / sizeof specials[0])];
                        break;
                    default:  // gentle terrain around the thresholds, some sea
                        v = static_cast<std::int16_t>(static_cast<std::int32_t>(r % 1400U) - 200 +
                                                      ((r >> 16) % 2U == 0U ? 0 : 4400));
                        break;
                }
            }
            CAPTURE(w);
            CAPTURE(h);
            CAPTURE(trial);
            Five got;
            REQUIRE_NOTHROW(got = five_of(w, h, e));
            CHECK(got == ref_five(e, w, h));
            REQUIRE_NOTHROW((void)wi::slope_stats(e, w, h));
            REQUIRE_NOTHROW((void)wi::edge_parallel_runs(e, w, h));
            REQUIRE_NOTHROW((void)wi::land_cell_steps(e, w, h));
            ++checked;
        }
    }
    CHECK(checked == 54 * 6);
}

// --- The whole section and report, byte for byte ------------------------------------------------

TEST_CASE("geography 10.3 slope: the whole section of the slope world") {
    const Cells& e = p100::SLOPE_ELEVATION;
    const std::uint32_t w = p100::SLW, h = p100::SLH;
    // The intermediates of p1_00_geography_reference.hpp.
    CHECK(wi::cell_slope_s2(e, w, h, 6, 1) == 25360000);  // (600, 5000), doubled gx
    CHECK(wi::cell_slope_s2(e, w, h, 2, 2) == 4);
    CHECK(wi::cell_slope_s2(e, w, h, 5, 5) == 50000);     // just under 35 degrees
    CHECK(wi::cell_slope_s2(e, w, h, 6, 3) == 102500);    // just over 45 degrees
    CHECK(wi::cell_slope_s2(e, w, h, 6, 4) == 100000);    // between
    const wi::SlopeStats st = wi::slope_stats(e, w, h);
    CHECK(st.land == 30U);
    CHECK(st.steeper_35 == 21U);
    CHECK(st.steeper_45 == 19U);
    CHECK(st.high == 9U);
    CHECK(st.median_high_s2 == std::optional<std::int64_t>(132500));
    CHECK(st.lowland == 17U);
    CHECK(st.lowland_flat == 1U);
    check_runs(e, w, h, 5, 5, 6, 6);
    CHECK(section_of(w, h, e) == p100::SLOPE_GEOGRAPHY);
    CHECK(five_of(w, h, e) == ref_five(e, w, h));
}

TEST_CASE("report v2: the whole report of the slope world") {
    dbtest::TempDir dir("p100-report-slope");
    const std::string out = dir.file("w.db");
    const LayerGrid g = p100::grid(p100::SLW, p100::SLH, p100::SLOPE_ELEVATION);
    std::vector<FixtureLayer> layers;
    layers.push_back({"elevation", g, Codec::delta_varint});
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 11, layers).ok());
    std::string expected = "# worldgen report v2\n[meta]\n";
    for (const auto& [k, v] : p004::meta_rows(11, {}, "test_fixture")) {
        expected += v.empty() ? k + " =\n" : k + " = " + v + "\n";
    }
    expected += "[tables]\nage = 0\nbiome = 0\ncivilization = 0\nclimate = 0\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\ngod = 0\nlayer = 1\nmeta = 17\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nregion = 0\nregion_biome = 0\nregion_border = 0\nregion_control = 0\nregion_name = 0\nrelation = 0\nsettlement = 0\nsettlement_census = 0\n[layers]\nelevation i16 7x6 codec=1 bytes=" +
                std::to_string(worldgen::db::encode_layer_blob(g).size()) + " min=-40 max=5400\n" +
                p100::with_empty_pass_lines(p100::SLOPE_GEOGRAPHY);
    auto world = worldgen::db::WorldReader::open(out);
    REQUIRE(world.ok());
    const auto r = worldgen::inspect::build_report(world.value());
    REQUIRE(r.ok());
    CHECK(r.value() == expected);
}
