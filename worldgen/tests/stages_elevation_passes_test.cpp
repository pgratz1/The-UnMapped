// Tests of the fourth version of P1-01 (generator 0.6.0): natural passes
// (E6.5 and the E7 change) and the final talus (E8.7). Worked examples T1
// to T7 of 10.1, the pass derived integers of 4.5, the properties of 10.2
// for E6.5 and E8.7 (invariants 10 to 15) against the test's own
// implementation (p1_01_passes_support.hpp), and the queue independence of
// 10.3.
//
// Every figure below comes from the spec; T1 to T7 were also reproduced by
// worldgen/tests/reference/p1_elevation_reference.py (command `check`).
//
// Larger sweeps (N = 128 seeds 0 to 63 with both fixtures, N = 512 seeds 0
// to 3) are in the suite "elevation-slow", skipped by default:
//   worldgen-tests -ts=elevation-slow --no-skip

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "db_test_support.hpp"
#include "p0_04_reference.hpp"
#include "p1_01_elevation_api.hpp"
#include "p1_01_elevation_support.hpp"
#include "p1_01_passes_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/math.hpp"
#include "worldgen/core/noise.hpp"
#include "worldgen/core/rng.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/inspect/report.hpp"
#include "worldgen/stages/elevation.hpp"
#include "worldgen/stages/testing.hpp"

using namespace elev_test;
using worldgen::Fx;
using worldgen::geo::Grid;

namespace {

// Tuning and the test's values for one set of overrides.
struct TuningPair {
    elev_api::Tuning tuning;
    Values values;
};

TuningPair tuning_with(const Overrides& o) {
    const elev_api::TuningLoad l = elev_api::load_tuning_text(elevation_toml(o));
    REQUIRE_MESSAGE(l.ok(), "fixture with overrides does not load");
    return TuningPair{elev_api::tuning_from(*l.content), Values(o)};
}

const TuningPair& fixture(std::string_view name) {
    static const TuningPair shipped = tuning_with({});
    static const TuningPair small = tuning_with(small_overrides());
    static const TuningPair shipped0 = tuning_with(fixture_overrides("shipped-neutral"));
    static const TuningPair small0 = tuning_with(fixture_overrides("small-neutral"));
    return name == "shipped" ? shipped : name == "small" ? small : name == "shipped-neutral" ? shipped0 : small0;
}

std::string xy(Grid g, std::int64_t c) { return "(" + std::to_string(xof(g, c)) + ", " + std::to_string(yof(g, c)) + ")"; }

std::vector<std::int64_t> as64(std::span<const worldgen::geo::CellIndex> v) { return {v.begin(), v.end()}; }

// --- T1 (P1-01 10.1) -----------------------------------------------------------------------

constexpr std::int32_t T1_W = 17;
constexpr std::int32_t T1_H = 9;

const std::int64_t T1_K[5][13] = {
    {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
    {440, 400, 420, 440, 400, 420, 440, 400, 420, 440, 400, 420, 440},
    {800, 850, 900, 950, 900, 700, 550, 650, 900, 950, 900, 850, 800},
    {380, 400, 380, 400, 380, 400, 380, 400, 380, 400, 380, 400, 380},
    {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
};

struct T1Grid {
    Grid g{T1_W, T1_H};
    std::vector<std::int64_t> h0, unit, mask, profile, nb;
    std::vector<std::uint8_t> ocean;
    std::int64_t scale = Fx::from_int(1024).raw();
    std::int64_t w_b = Fx::from_int(4).raw();
    [[nodiscard]] PassGrids grids() const { return PassGrids{g, h0, unit, mask, profile, nb, ocean, scale, 0, w_b}; }
};

T1Grid t1_grid() {
    T1Grid t;
    const std::size_t n = cells(t.g);
    t.h0.assign(n, 0);
    t.unit.assign(n, 0);
    t.mask.assign(n, 0);
    t.profile.assign(n, 0);
    t.nb.assign(n, 0);
    t.ocean.assign(n, 0);
    for (std::int32_t y = 0; y < T1_H; ++y) {
        for (std::int32_t x = 0; x < T1_W; ++x) {
            const auto i = static_cast<std::size_t>(y * T1_W + x);
            t.ocean[i] = (x == 0 || x == T1_W - 1 || y == 0 || y == T1_H - 1) ? 1 : 0;
            t.nb[i] = y <= 3 ? ONE : -ONE;
            if (x >= 2 && x <= 14 && y >= 2 && y <= 6) {
                t.mask[i] = ONE;
                t.profile[i] = ONE;
                t.unit[i] = T1_K[y - 2][x - 2] << 22;  // mh = K m exactly
            }
        }
    }
    return t;
}

Overrides t1_overrides(const Overrides& extra = {}) {
    return concat({{"pass_saddle_window_m", "32"},
                   {"pass_saddle_rise_reach_m", "96"},
                   {"pass_size_ref_n", "16"},
                   {"pass_min_length_m", "0"},
                   {"pass_coast_clear_m", "0"},
                   {"pass_halfwidth_m", "20"}},
                  extra);
}

elev_api::PassLayout t1_stage(const T1Grid& t, const elev_api::Tuning& tn) {
    return elev_api::pass_layout(0, t.g, t.h0, t.unit, t.mask, t.profile, t.nb, t.ocean, t.scale, 0, t.w_b, tn);
}

std::int64_t at(std::int32_t x, std::int32_t y) { return y * T1_W + x; }

// --- Comparison of the stage's layout with the test's own ---------------------------------

void compare_layout(Grid g, const elev_api::PassLayout& got, const OwnLayout& own) {
    REQUIRE(got.ranges.size() == own.ranges.size());
    for (std::size_t k = 0; k < own.ranges.size(); ++k) {
        CAPTURE(k);
        const auto& a = got.ranges[k];
        const OwnRange& b = own.ranges[k];
        CHECK(a.label == b.label);
        CHECK(static_cast<std::int64_t>(a.s0) == b.s0);
        CHECK(static_cast<std::int64_t>(a.end_a) == b.end_a);
        CHECK(static_cast<std::int64_t>(a.end_b) == b.end_b);
        CHECK(static_cast<std::int64_t>(a.top) == b.top);
        CHECK(a.length == b.length);
        CHECK(a.p90 == b.p90);
        CHECK(a.slots == b.slots);
        // Lead's ruling on the seam: divide is empty when E6.5.3 did not run
        // (no range has a slot); candidates are filled only for ranges with
        // slots and a non-empty divide list. The own implementation follows
        // the same rules, so both are compared for every range.
        CHECK(as64(a.divide) == b.divide);
        CHECK(as64(a.candidates) == b.candidates);
    }
    REQUIRE(got.tries.size() == own.tries.size());
    for (std::size_t i = 0; i < own.tries.size(); ++i) {
        CAPTURE(i);
        CAPTURE(xy(g, own.tries[i].col));
        CHECK(static_cast<std::int64_t>(got.tries[i].col) == own.tries[i].col);
        CHECK(got.tries[i].range == own.tries[i].range);
        CHECK(got.tries[i].slot == own.tries[i].slot);
        CHECK(elev_api::reject_name(got.tries[i].reject) == own.tries[i].rule);
    }
    REQUIRE(got.sites.size() == own.sites.size());
    for (std::size_t s = 0; s < own.sites.size(); ++s) {
        CAPTURE(s);
        const auto& a = got.sites[s];
        const OwnSite& b = own.sites[s];
        CHECK(a.range == b.range);
        CHECK(static_cast<std::int64_t>(a.col) == b.col);
        CHECK(static_cast<std::int64_t>(a.partner) == b.partner);
        CHECK(a.natural_gap == b.gap);
        CHECK(a.capped == b.capped);
        CHECK(as64(a.axis) == b.axis);
        CHECK(a.col_index == b.ci);
        CHECK(a.dh == b.dh);
        CHECK(a.maxm == b.maxm);
        CHECK(a.saddle.raw() == b.saddle);
        CHECK(a.halfwidth.raw() == b.halfwidth);
        // A natural gap has depth 0 and capped false (lead's ruling).
        CHECK(a.depth.raw() == b.depth);
        if (b.gap) {
            CHECK(a.depth.raw() == 0);
            CHECK_FALSE(a.capped);
        }
    }
    CHECK(got.factor == own.factor);
    CHECK(got.adj == own.adj);
}

}  // namespace

// --- 4.5: pass derived integers ----------------------------------------------------------------

TEST_CASE("P1-01 4.5, 10.1: pass derived integers") {
    const Values& v = shipped_values();
    const Values& s = small_values();
    struct Row {
        std::int64_t n, coast_clear, min_len, side_max_m;
    };
    for (const Row& r : {Row{128, 27, 117, 375}, Row{512, 109, 468, 1500}, Row{1024, 219, 937, 3000},
                         Row{2048, 219, 937, 3000}}) {
        CAPTURE(r.n);
        const Values::PassInts p = v.pass_ints(r.n);
        CHECK(p.coast_clear == r.coast_clear);
        CHECK(p.min_len == r.min_len);
        CHECK(p.side_max == Fx::from_int(r.side_max_m).raw());
    }
    const Values::PassInts p = v.pass_ints(1024);
    const Values::PassInts q = s.pass_ints(128);
    CHECK(p.spacing == 937);
    CHECK(q.spacing == 312);
    CHECK(p.reach5 == 313);  // mul_div rounds 312.5 up
    CHECK(q.reach5 == 125);
    CHECK(std::vector<std::int64_t>{p.win, p.rise_b, p.spacing_b, p.tarn_b} == std::vector<std::int64_t>{25, 93, 156, 10});
    CHECK(std::vector<std::int64_t>{q.win, q.rise_b, q.spacing_b, q.tarn_b} == std::vector<std::int64_t>{12, 37, 50, 4});
    CHECK(p.rise_u == 9830400);
    CHECK(q.rise_u == 4915200);
    CHECK(p.body_u == 19660800);
    CHECK(Fx::from_ratio(8, 15).raw() == 2290649225);
    CHECK(Fx::from_ratio(9, 10).raw() == 3865470566);
    CHECK(Fx::from_ratio(1, 1500).raw() == 2863312);
    CHECK(worldgen::mul_int(v.fx("pass_walk_grade"), 48).raw() == 113387136624);
    CHECK(p.walk == 113387136624);
    CHECK(p.drop_o == 343597383680);
    CHECK(p.drop_d == 485920080000);
    CHECK(worldgen::stream_key(0, "geo.elevation.pass") == 0x964f8d9d472fb93bULL);
    CHECK(worldgen::substream(worldgen::stream_key(0, "geo.elevation.pass"), 3) == 0x262b01aeab682941ULL);
}

// --- T1 -----------------------------------------------------------------------------------

TEST_CASE("P1-01 10.1 T1: divide, candidate and crossing tests on a small grid") {
    const TuningPair tp = tuning_with(t1_overrides());
    const T1Grid t = t1_grid();
    const Grid g = t.g;
    const elev_api::PassLayout lay = t1_stage(t, tp.tuning);
    REQUIRE(lay.ranges.size() == 1U);
    const auto& r = lay.ranges[0];
    CHECK(r.label == 1);
    CHECK(r.s0 == 36U);
    CHECK(r.end_a == 116U);
    CHECK(r.end_b == 36U);
    CHECK(r.length == 68);
    CHECK(r.slots == 1);
    CHECK(static_cast<std::int64_t>(r.top) == at(5, 4));  // 950 m; ties go to the smaller index
    CHECK(r.p90 == std::int64_t{850} << 16);
    // D_1 in index order: (3..13, 4), then (2, 5) and (14, 5).
    std::vector<std::int64_t> want_d;
    for (std::int32_t x = 3; x <= 13; ++x) {
        want_d.push_back(at(x, 4));
    }
    want_d.push_back(at(2, 5));
    want_d.push_back(at(14, 5));
    CHECK(as64(r.divide) == want_d);
    CHECK(as64(r.candidates) == std::vector<std::int64_t>{at(8, 4)});
    REQUIRE(lay.tries.size() == 1U);
    CHECK(static_cast<std::int64_t>(lay.tries[0].col) == at(8, 4));
    CHECK(lay.tries[0].range == 1);
    CHECK(lay.tries[0].slot == 0);
    CHECK(elev_api::reject_name(lay.tries[0].reject) == "shoulders");
    CHECK(lay.sites.empty());
    CHECK(lay.factor == std::vector<std::int64_t>(cells(g), ONE));
    CHECK(lay.adj == std::vector<std::int64_t>(cells(g), 0));

    // The divide flood: side 2 on rows y <= 3, side 1 on rows y >= 5; on
    // row 4, (2, 4) and (14, 4) side 2 and (3, 4) .. (13, 4) side 1.
    std::vector<std::uint8_t> support(cells(g), 0);
    std::vector<std::int64_t> key(cells(g), 0);
    for (std::size_t i = 0; i < cells(g); ++i) {
        support[i] = t.mask[i] >= Values(t1_overrides()).raw("pass_region_mask") ? 1 : 0;
        key[i] = worldgen::mul(R(t.scale), R(t.unit[i])).raw() >> 16;
    }
    const worldgen::stages::DivideFlood fl = elev_api::divide_flood(g, support, key, t.nb, 0);
    for (std::int32_t x = 2; x <= 14; ++x) {
        CAPTURE(x);
        CHECK(fl.side[static_cast<std::size_t>(at(x, 2))] == 2U);
        CHECK(fl.side[static_cast<std::size_t>(at(x, 3))] == 2U);
        CHECK(fl.side[static_cast<std::size_t>(at(x, 4))] == ((x == 2 || x == 14) ? 2U : 1U));
        CHECK(fl.side[static_cast<std::size_t>(at(x, 5))] == 1U);
        CHECK(fl.side[static_cast<std::size_t>(at(x, 6))] == 1U);
    }
    // chainA = (8, 4), (8, 5), (7, 6), (6, 7); chainB = (8, 3), (7, 2), (6, 1).
    auto chain = [&fl](std::int64_t c) {
        std::vector<std::int64_t> out{c};
        while (static_cast<std::int64_t>(fl.parent[static_cast<std::size_t>(c)]) != c) {
            c = static_cast<std::int64_t>(fl.parent[static_cast<std::size_t>(c)]);
            out.push_back(c);
        }
        return out;
    };
    CHECK(chain(at(8, 4)) == std::vector<std::int64_t>{at(8, 4), at(8, 5), at(7, 6), at(6, 7)});
    CHECK(chain(at(8, 3)) == std::vector<std::int64_t>{at(8, 3), at(7, 2), at(6, 1)});

    // The test's own implementation agrees, including the divide pairs.
    const OwnLayout own = own_pass_layout(0, t.grids(), tp.values);
    compare_layout(g, lay, own);
    const std::vector<std::int64_t> dh_m = {850, 900, 950, 900, 700, 550, 650, 900, 950, 900, 850, 800, 800};
    for (std::size_t k = 0; k < want_d.size(); ++k) {
        const auto c = static_cast<std::size_t>(want_d[k]);
        CHECK(own.dh[c] == dh_m[k] << 16);
        CHECK(own.partner[c] == want_d[k] - T1_W);  // the cell above
    }
    // (h): the hashes of the col.
    const std::uint64_t kp = worldgen::stream_key(0, "geo.elevation.pass");
    CHECK(static_cast<std::int64_t>(worldgen::hash_at(kp, at(8, 4), 16) >> 32U) * 2 - ONE == 3576141052);
    CHECK(static_cast<std::int64_t>(worldgen::hash_at(kp, at(8, 4), 17) >> 32U) * 2 - ONE == 879522112);
    CHECK(fm(19).raw() == 261134011597);
}

TEST_CASE("P1-01 10.1 T1: variants, one value each") {
    struct Variant {
        Overrides o;
        std::string rule;
    };
    const std::vector<Variant> vs = {
        {{{"pass_coast_clear_m", "100"}}, "coast"},
        {{{"pass_barrier_m", "1100"}}, "barrier"},
        {{{"pass_side_max_m", "30"}}, "side"},
        {{{"pass_halfwidth_m", "450"}}, "summit_top"},
    };
    for (const Variant& v : vs) {
        CAPTURE(v.rule);
        const TuningPair tp = tuning_with(t1_overrides(v.o));
        const T1Grid t = t1_grid();
        const elev_api::PassLayout lay = t1_stage(t, tp.tuning);
        REQUIRE(lay.tries.size() == 1U);
        CHECK(static_cast<std::int64_t>(lay.tries[0].col) == at(8, 4));
        CHECK(elev_api::reject_name(lay.tries[0].reject) == v.rule);
        CHECK(lay.sites.empty());
        compare_layout(t.g, lay, own_pass_layout(0, t.grids(), tp.values));
    }
    {
        // h0 = 10 m at (6, 7): the feet rule.
        const TuningPair tp = tuning_with(t1_overrides());
        T1Grid t = t1_grid();
        t.h0[static_cast<std::size_t>(at(6, 7))] = Fx::from_int(10).raw();
        const elev_api::PassLayout lay = t1_stage(t, tp.tuning);
        REQUIRE(lay.tries.size() == 1U);
        CHECK(elev_api::reject_name(lay.tries[0].reject) == "feet");
        compare_layout(t.g, lay, own_pass_layout(0, t.grids(), tp.values));
    }
    {
        // Rule (c), which no fixture world reaches (10.2 "Rule coverage"): a
        // constructed variant with row y = 7 made ocean, so the side-1 chain
        // of the col ends at an ocean seed (reference: fails "sea").
        const TuningPair tp = tuning_with(t1_overrides());
        T1Grid t = t1_grid();
        for (std::int32_t x = 0; x < T1_W; ++x) {
            t.ocean[static_cast<std::size_t>(at(x, 7))] = 1;
        }
        const elev_api::PassLayout lay = t1_stage(t, tp.tuning);
        REQUIRE(lay.tries.size() == 1U);
        CHECK(static_cast<std::int64_t>(lay.tries[0].col) == at(8, 4));
        CHECK(elev_api::reject_name(lay.tries[0].reject) == "sea");
        compare_layout(t.g, lay, own_pass_layout(0, t.grids(), tp.values));
    }
    {
        // pass_min_col_m = pass_saddle_max_m = 600: saddle 600 m >= M = 550 m,
        // a natural gap (rule (i)) with the axis of T1.
        const TuningPair tp = tuning_with(t1_overrides({{"pass_min_col_m", "600"}, {"pass_saddle_max_m", "600"}}));
        const T1Grid t = t1_grid();
        const elev_api::PassLayout lay = t1_stage(t, tp.tuning);
        REQUIRE(lay.tries.size() == 1U);
        CHECK_FALSE(lay.tries[0].reject.has_value());
        REQUIRE(lay.sites.size() == 1U);
        const auto& s = lay.sites[0];
        CHECK(s.range == 1);
        CHECK(static_cast<std::int64_t>(s.col) == at(8, 4));
        CHECK(static_cast<std::int64_t>(s.partner) == at(8, 3));
        CHECK(s.natural_gap);
        CHECK_FALSE(s.capped);
        CHECK(as64(s.axis) ==
              std::vector<std::int64_t>{at(6, 7), at(7, 6), at(8, 5), at(8, 4), at(8, 3), at(7, 2), at(6, 1)});
        CHECK(s.col_index == 3);
        CHECK(s.dh == std::int64_t{550} << 16);
        CHECK(s.maxm == 36044800);
        CHECK(s.saddle == Fx::from_int(600));
        CHECK(s.halfwidth.raw() == 91176478600);  // 21.23 m
        // Natural gaps get no factor and no adjustment.
        CHECK(lay.factor == std::vector<std::int64_t>(cells(t.g), ONE));
        CHECK(lay.adj == std::vector<std::int64_t>(cells(t.g), 0));
        compare_layout(t.g, lay, own_pass_layout(0, t.grids(), tp.values));
        // Its row (9.1): kind 0, capped 0, saddle_dm 5500.
        const std::vector<std::int64_t> h7(cells(t.g), 0);
        const PassRow row = pass_row(1, s.range, s.natural_gap, s.capped, s.col, s.dh, 0, as64(s.axis), T1_W);
        CHECK(row.kind == 0);
        CHECK(row.capped == 0);
        CHECK(row.col_x == 8);
        CHECK(row.col_y == 4);
        CHECK(row.saddle_dm == 5500);
    }
}

// --- T2 -----------------------------------------------------------------------------------

TEST_CASE("P1-01 10.1 T2: the direction rules on given vectors") {
    auto along = [](std::int64_t vx, std::int64_t vy, std::int64_t tx, std::int64_t ty) {
        const std::int64_t vt = vx * tx + vy * ty;
        return 2 * vt * vt > (vx * vx + vy * vy) * (tx * tx + ty * ty);
    };
    CHECK(along(-3, 1, -7, 0));   // 882 > 490
    CHECK_FALSE(along(-1, 2, -7, 0));
    CHECK_FALSE(along(-1, -2, -7, 0));
    CHECK(along(-4, -1, -11, -2));  // 4232 > 2125
    CHECK_FALSE(along(-1, 2, -11, -2));
    CHECK_FALSE(along(-1, -2, -11, -2));
    CHECK(4 * 40 * 40 > 225 * 16);
    CHECK_FALSE(4 * 24 * 24 > 225 * 16);
    CHECK(100 * 41 * 41 > 33 * 25 * 125);
    CHECK_FALSE(100 * 8 * 8 > 33 * 16 * 125);
    CHECK(fdiv(-21, 6) == -4);
    CHECK(fdiv(21, 6) == 3);
}

// --- T3 -----------------------------------------------------------------------------------

TEST_CASE("P1-01 10.1 T3: depth (pass_depth)") {
    const elev_api::Tuning& t = fixture("shipped").tuning;
    const Values& v = shipped_values();
    const std::vector<std::int64_t> mask(7, ONE);
    auto profs = [](std::vector<const char*> p) {
        std::vector<std::int64_t> out;
        for (const char* s : p) {
            out.push_back(dec(s));
        }
        return out;
    };
    auto mprime = [&](std::int64_t scale, const std::vector<std::int64_t>& pr, std::int64_t p) {
        std::vector<std::int64_t> out;
        for (const std::int64_t x : pr) {
            out.push_back(elev_api::pass_lowered(scale, ONE, x, p, v.raw("foothill_fraction"), v.raw("pass_texture_keep")));
        }
        return out;
    };
    auto rounded = [](const std::vector<std::int64_t>& m, std::int64_t unit) {
        std::vector<std::int64_t> out;
        for (const std::int64_t x : m) {
            out.push_back(worldgen::mul_div(x, unit, ONE));
        }
        return out;
    };
    {
        // A: the walk grade binds, the cap not.
        const std::vector<std::int64_t> pr = profs({"0.6", "0.62", "0.64", "0.65", "0.64", "0.62", "0.6"});
        const std::int64_t scale = Fx::from_int(1000).raw();
        const auto [lo, capped] = elev_api::pass_depth(mask, pr, scale, Fx::from_int(600).raw(), false, 42598399, t);
        CHECK(lo == 2267742738);
        CHECK_FALSE(capped);
        CHECK(ONE - lo == 2027224558);
        const std::vector<std::int64_t> m = mprime(scale, pr, lo);
        CHECK(*std::max_element(m.begin(), m.end()) == 1474032779000);
        CHECK(rounded(m, 100) == std::vector<std::int64_t>{31680, 32736, 33792, 34320, 33792, 32736, 31680});
        CHECK(own_pass_depth(mask, pr, scale, Fx::from_int(600).raw(), false, 42598399, v) ==
              std::pair<std::int64_t, bool>{lo, capped});
    }
    {
        // B: the walk grade asks too much; the cap raises.
        const std::vector<std::int64_t> pr = profs({"0.15", "0.4", "0.7", "0.85", "0.75", "0.45", "0.15"});
        const std::int64_t scale = Fx::from_int(1000).raw();
        const std::int64_t maxm = std::int64_t{850} << 16;
        const auto [lo, capped] = elev_api::pass_depth(mask, pr, scale, Fx::from_int(600).raw(), false, maxm, t);
        CHECK(lo == 1768515945);
        CHECK(capped);
        CHECK(ONE - lo == 2526451351);
        const std::vector<std::int64_t> m = mprime(scale, pr, lo);
        CHECK(*std::max_element(m.begin(), m.end()) == 1503238554000);
        CHECK(rounded(m, 1000) == std::vector<std::int64_t>{61765, 164706, 288235, 350000, 308824, 185294, 61765});
        CHECK(own_pass_depth(mask, pr, scale, Fx::from_int(600).raw(), false, maxm, v) ==
              std::pair<std::int64_t, bool>{lo, capped});
    }
    {
        // C: capped at (h); the raise moves p by 1 raw.
        const std::vector<std::int64_t> pr = profs({"0.5", "0.9", "1", "1", "0.95", "0.7", "0.4"});
        const std::int64_t scale = Fx::from_int(1400).raw();
        const std::int64_t maxm = std::int64_t{1400} << 16;
        const auto [lo, capped] = elev_api::pass_depth(mask, pr, scale, Fx::from_int(900).raw(), true, maxm, t);
        CHECK(lo == 2761050405);
        CHECK(capped);
        CHECK(ONE - lo == 1533916891);
        const std::vector<std::int64_t> m = mprime(scale, pr, lo);
        CHECK(*std::max_element(m.begin(), m.end()) == 3865470567000);
        // 1 raw below lo, every m' is <= 900 m but the top is below M - cap.
        const std::vector<std::int64_t> m1 = mprime(scale, pr, lo - 1);
        CHECK(*std::max_element(m1.begin(), m1.end()) < Fx::from_int(900).raw());
        CHECK(own_pass_depth(mask, pr, scale, Fx::from_int(900).raw(), true, maxm, v) ==
              std::pair<std::int64_t, bool>{lo, capped});
    }
}

TEST_CASE("P1-01 E6.5.0, 6.5: m'(c, p) is monotone in p and m'(c, ONE) is within R of mul(scale, unit)") {
    const Values& v = shipped_values();
    worldgen::Rng rng(worldgen::substream(worldgen::stream_key(0, "test.noise"), 61));
    for (int k = 0; k < 400; ++k) {
        const std::int64_t scale = rng.uniform_i64(0, Fx::from_int(8700).raw());
        const std::int64_t mk = rng.uniform_i64(0, ONE);
        const std::int64_t pr = rng.uniform_i64(v.raw("foothill_fraction"), ONE);
        const std::int64_t unit = worldgen::mul(R(mk), R(pr)).raw();
        const std::int64_t nat = worldgen::mul(R(scale), R(unit)).raw();
        const std::int64_t r = (scale >> 32) + 1;
        const std::int64_t m1 = elev_api::pass_lowered(scale, mk, pr, ONE, v.raw("foothill_fraction"), v.raw("pass_texture_keep"));
        CHECK(m1 == lowered(scale, mk, pr, ONE, v.raw("foothill_fraction"), v.raw("pass_texture_keep")));
        CHECK(m1 <= nat + r);
        CHECK(m1 >= nat - r);
        std::int64_t prev = 0;
        for (const std::int64_t p : {std::int64_t{0}, ONE / 8, ONE / 4, ONE / 3, ONE / 2, (3 * ONE) / 4, ONE}) {
            const std::int64_t m = elev_api::pass_lowered(scale, mk, pr, p, v.raw("foothill_fraction"), v.raw("pass_texture_keep"));
            CHECK(m == lowered(scale, mk, pr, p, v.raw("foothill_fraction"), v.raw("pass_texture_keep")));
            CHECK(m >= prev);
            prev = m;
        }
    }
}

// --- T4 -----------------------------------------------------------------------------------

TEST_CASE("P1-01 10.1 T4: bump and factor (pass_bump)") {
    const Values& v = shipped_values();
    const std::uint64_t k3 = worldgen::substream(worldgen::stream_key(0, "geo.elevation.pass"), 3);
    const std::int64_t hw = Fx::from_int(450).raw();
    const std::int64_t depth = dec("0.4");
    const std::int64_t wv = v.raw("pass_width_var");
    const Fx hwmax = worldgen::mul(R(hw), Fx::ONE + R(wv));
    CHECK(hwmax.raw() == 2609192632500);
    CHECK(worldgen::mul_div(hwmax.raw(), 5, 16 * ONE) + 2 == 192);
    struct Row {
        std::int64_t d, arc_a, x, nz, bump, p;
    };
    const std::vector<Row> rows = {
        {0, 100, 0, 0, ONE, 2576980378},
        {35, 150, 458129920, -413989064, 3815349666, 2768827430},
        {100, 150, 458129920, -413989064, 510662794, 4090702178},
        {60, 40, -549755904, 476092256, 1039159827, 3879303365},
        {0, 240, 1282763776, -589897286, 44140840, 4277310960},
    };
    for (const Row& r : rows) {
        CAPTURE(r.d);
        CAPTURE(r.arc_a);
        const Fx s_m = fm(r.arc_a - 100);
        const Fx dend = fm(std::min(r.arc_a, 250 - r.arc_a));
        const Fx x = worldgen::mul(s_m, Fx::from_ratio(1, 1500));
        CHECK(x.raw() == r.x);
        const Fx nz = worldgen::noise2(k3, x, Fx::from_int(2));
        CHECK(nz.raw() == r.nz);
        const std::int64_t b = elev_api::pass_bump(fm(r.d).raw(), dend.raw(), hw, wv, nz.raw());
        CHECK(b == r.bump);
        CHECK(b == own_bump(fm(r.d).raw(), dend.raw(), hw, wv, nz.raw()));
        CHECK(ONE - worldgen::mul(R(depth), R(b)).raw() == r.p);
    }
    CHECK(worldgen::mul(R(hw), Fx::ONE + worldgen::mul(R(wv), R(-413989064))).raw() == 1867532005800);
    CHECK(worldgen::mul(R(hw), Fx::ONE + worldgen::mul(R(wv), R(476092256))).raw() == 2007719813700);
    // dist >= hwv gives 0.
    CHECK(elev_api::pass_bump(1867532005800, Fx::from_int(320).raw(), hw, wv, -413989064) == 0);
    CHECK(elev_api::pass_bump(1867532005801, Fx::from_int(320).raw(), hw, wv, -413989064) == 0);
    CHECK(elev_api::pass_bump(1867532005799, Fx::from_int(320).raw(), hw, wv, -413989064) >= 0);
}

// --- T5 -----------------------------------------------------------------------------------

TEST_CASE("P1-01 10.1 T5: floor drainage (pass_drainage)") {
    std::vector<std::int64_t> N;
    std::vector<std::int64_t> S;
    for (const std::int64_t m : {100, 300, 600, 900, 1000, 950, 900, 800, 760}) {
        N.push_back(Fx::from_int(m).raw());
    }
    for (const std::int64_t m : {100, 280, 520, 640, 600, 610, 560, 380, 760}) {
        S.push_back(Fx::from_int(m).raw());
    }
    const std::int64_t eps = dec("0.1");
    const std::vector<std::int64_t> d = elev_api::pass_drainage(S, N, 4, eps);
    const std::vector<std::int64_t> want = {0, 0, 0, -172228188570, 0, 645533584590, 859852452660, 1632517069210, 0};
    CHECK(d == want);
    CHECK(own_drainage(S, N, 4, eps) == want);
    CHECK(-worldgen::mul(R(d[5]), Fx::HALF).raw() == -322766792295);
    CHECK(-worldgen::mul(R(d[7]), Fx::HALF).raw() == -816258534605);
}

// --- T6 -----------------------------------------------------------------------------------

TEST_CASE("P1-01 10.1 T6: one final-talus step and 100 steps (final_talus)") {
    const Values& v = shipped_values();
    const Grid g{4, 3};
    const std::vector<std::int64_t> rows = {-5, 300, 310, 320, -5, 200, 400, 420, -5, 150, 380, 410};
    std::vector<std::int64_t> h;
    std::vector<std::uint8_t> ocean;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        h.push_back(Fx::from_int(rows[i]).raw());
        ocean.push_back(i % 4 == 0 ? 1 : 0);
    }
    const Values::PassInts pi = v.pass_ints(4);
    const std::int64_t rate = v.raw("final_talus_rate");
    const std::int64_t floor = v.raw("land_floor_m");
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        CAPTURE(threads);
        const std::vector<std::int64_t> one = elev_api::final_talus(g, h, ocean, pi.drop_o, pi.drop_d, rate, floor, 1, threads);
        const std::vector<std::int64_t> want = {h[0],          1173905047560, 1334124216320, 1379758243840,
                                                h[4],          835676373000,  1646351450120, 1798517555200,
                                                h[8],          692563476480,  1573873877000, 1760936591360};
        CHECK(one == want);
        CHECK(own_final_talus(g, h, ocean, pi.drop_o, pi.drop_d, rate, floor, 1) == want);
        const std::vector<std::int64_t> full = elev_api::final_talus(g, h, ocean, pi.drop_o, pi.drop_d, rate, floor, 100, threads);
        CHECK(full == own_final_talus(g, h, ocean, pi.drop_o, pi.drop_d, rate, floor, 100));
        std::vector<std::int64_t> mm;
        for (std::size_t i = 0; i < full.size(); ++i) {
            if (i % 4 != 0) {
                mm.push_back(worldgen::mul_div(full[i], 1000, ONE));
            } else {
                CHECK(full[i] == h[i]);
            }
        }
        CHECK(mm == std::vector<std::int64_t>{97018, 190660, 278227, 99632, 195395, 284157, 100483, 196936, 286085});
    }
    // 0 iterations change nothing.
    CHECK(elev_api::final_talus(g, h, ocean, pi.drop_o, pi.drop_d, rate, floor, 0, 1) == h);
}

TEST_CASE("P1-01 6.6, E8.7: final_talus is fatal on a land height below the floor, unless no step runs") {
    const Values& v = shipped_values();
    const Grid g{4, 3};
    std::vector<std::int64_t> h(12, Fx::from_int(50).raw());
    std::vector<std::uint8_t> ocean(12, 0);
    for (std::size_t i = 0; i < 12; i += 4) {
        ocean[i] = 1;
        h[i] = Fx::from_int(-5).raw();  // ocean below the floor is fine
    }
    const Values::PassInts pi = v.pass_ints(4);
    const std::int64_t rate = v.raw("final_talus_rate");
    const std::int64_t floor = v.raw("land_floor_m");
    CHECK(elev_api::final_talus(g, h, ocean, pi.drop_o, pi.drop_d, rate, floor, 3, 1).size() == 12U);
    h[6] = floor - 1;  // one land cell 1 raw below the floor: the E7/E8 contract is broken
    for (const std::uint32_t threads : {1U, 2U}) {
        CAPTURE(threads);
        CHECK_THROWS_AS(static_cast<void>(elev_api::final_talus(g, h, ocean, pi.drop_o, pi.drop_d, rate, floor, 1,
                                                                threads)),
                        worldgen::FatalError);
    }
    // With 0 iterations no step runs and nothing is checked: the grid is returned unchanged.
    CHECK(elev_api::final_talus(g, h, ocean, pi.drop_o, pi.drop_d, rate, floor, 0, 1) == h);
}

TEST_CASE("P1-01 E6.5.1: the basin flood is fatal when no land cell borders the ocean (pass_layout on a test grid)") {
    const TuningPair tp = tuning_with(t1_overrides());
    T1Grid t = t1_grid();
    // The T1 grid reaches E6.5.1 (its range has a slot); with the ocean
    // border made land there is no ocean cell next to land.
    std::fill(t.ocean.begin(), t.ocean.end(), std::uint8_t{0});
    CHECK_THROWS_AS(static_cast<void>(t1_stage(t, tp.tuning)), worldgen::FatalError);
    // The unchanged grid runs (T1).
    CHECK(t1_stage(t1_grid(), tp.tuning).tries.size() == 1U);
}

TEST_CASE("P1-01 10.2: E8.7 alone on random inputs meeting the contract (and against plain Jacobi steps)") {

    worldgen::Rng rng(worldgen::substream(worldgen::stream_key(0, "test.noise"), 62));
    const Values& v = shipped_values();
    for (int fx = 0; fx < 30; ++fx) {
        CAPTURE(fx);
        const auto w = static_cast<std::int32_t>(rng.uniform_i64(3, 24));
        const auto hh = static_cast<std::int32_t>(rng.uniform_i64(3, 24));
        const Grid g{w, hh};
        const std::size_t n = cells(g);
        std::vector<std::uint8_t> ocean(n);
        std::vector<std::int64_t> h(n);
        const std::int64_t floor = v.raw("land_floor_m");
        for (std::size_t i = 0; i < n; ++i) {
            ocean[i] = rng.uniform_i64(0, 4) == 0 ? 1 : 0;
            h[i] = ocean[i] != 0U ? -rng.uniform_i64(HALF, Fx::from_int(2900).raw())
                                  : rng.uniform_i64(floor, Fx::from_int(rng.uniform_i64(1, 2990)).raw());
        }
        // Rates and slopes inside the bounds of 4.2.
        const std::int64_t slope = rng.uniform_i64(dec("0.05"), Fx::from_int(16).raw());
        const std::int64_t drop_o = slope * 16;
        const std::int64_t drop_d = worldgen::mul(R(drop_o), Fx::SQRT2).raw();
        const std::int64_t rate = rng.uniform_i64(0, dec("0.125"));
        const auto iters = static_cast<std::int32_t>(rng.uniform_i64(0, 40));
        const std::vector<std::int64_t> out =
            elev_api::final_talus(g, h, ocean, drop_o, drop_d, rate, floor, iters, 1 + static_cast<std::uint32_t>(fx % 3));
        CHECK(out == own_final_talus(g, h, ocean, drop_o, drop_d, rate, floor, iters));
        std::int64_t max_in = 0;
        std::int64_t max_out = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (ocean[i] != 0U) {
                REQUIRE(out[i] == h[i]);
                continue;
            }
            REQUIRE(out[i] >= floor);
            REQUIRE(out[i] <= H_MAX_RAW);
            max_in = std::max(max_in, h[i]);
            max_out = std::max(max_out, out[i]);
        }
        CHECK(max_out <= max_in);
    }
}

// --- T7 and the rows of table `pass` (9.1) --------------------------------------------------

TEST_CASE("P1-01 10.1 T7: pass rows and report pass lines, small-map fixture N = 128 seeds 8 and 31") {
    const Grid g{128, 128};
    auto sha = [](const std::vector<std::uint8_t>& b) { return worldgen::to_hex(worldgen::sha256(b)); };
    auto rows_of = [](const worldgen::stages::ElevationResult& r) {
        std::vector<PassRow> rows;
        for (std::size_t s = 0; s < r.sites.size(); ++s) {
            const auto& x = r.sites[s];
            rows.push_back(pass_row(static_cast<std::int64_t>(s) + 1, x.range, x.natural_gap, x.capped, x.col, x.dh,
                                    r.h7[s], as64(x.axis), 128));
        }
        return rows;
    };
    {
        const worldgen::stages::ElevationResult r = elev_api::elevation_run(8, 128, fixture("small").tuning, THREADS);
        const std::vector<PassRow> rows = rows_of(r);
        REQUIRE(rows.size() == 2U);
        CHECK(std::vector<std::int64_t>{rows[0].id, rows[0].range_id, rows[0].kind, rows[0].capped, rows[0].col_x,
                                        rows[0].col_y, rows[0].saddle_dm, rows[0].col_dm} ==
              std::vector<std::int64_t>{1, 1, 1, 1, 72, 94, 6878, 5151});
        CHECK(rows[0].path.size() == 16U * 4U);
        CHECK(sha(rows[0].path) == "22011adaf46efab7a64646e862e9fc7b14dee30df9af79259faa3e6f01068a4f");
        CHECK(std::vector<std::int64_t>{rows[1].id, rows[1].range_id, rows[1].kind, rows[1].capped, rows[1].col_x,
                                        rows[1].col_y, rows[1].saddle_dm, rows[1].col_dm} ==
              std::vector<std::int64_t>{2, 1, 1, 1, 69, 35, 8149, 7342});
        CHECK(rows[1].path.size() == 10U * 4U);
        CHECK(sha(rows[1].path) == "b464f12091144654c74871fe94fabf49e749053c2a21e58e865c39cebaf70d27");
        CHECK(r.e[94 * 128 + 72] == 4775);
        CHECK(r.e[35 * 128 + 69] == 6333);
        CHECK(report_pass_lines(g, r.e, rows) ==
              std::vector<std::string>{"passes = 2", "natural_gaps = 0", "passes_capped = 2",
                                       "pass_col_m = 477.5 633.3 633.3", "passes_walkable = 2"});
    }
    {
        const worldgen::stages::ElevationResult r = elev_api::elevation_run(31, 128, fixture("small").tuning, THREADS);
        const std::vector<PassRow> rows = rows_of(r);
        REQUIRE(rows.size() == 1U);
        CHECK(std::vector<std::int64_t>{rows[0].id, rows[0].range_id, rows[0].kind, rows[0].capped, rows[0].col_x,
                                        rows[0].col_y, rows[0].saddle_dm, rows[0].col_dm} ==
              std::vector<std::int64_t>{1, 2, 1, 0, 84, 67, 6243, 2243});
        CHECK(rows[0].path.size() == 21U * 4U);
        CHECK(sha(rows[0].path) == "ceb9bca4970eef67433bf47621586bf43373bf1c35fafe598c3131165454e571");
        REQUIRE(r.sites.size() == 1U);
        CHECK(r.sites[0].depth == Fx::ONE);  // lo = 0, uncapped
        CHECK(r.e[67 * 128 + 84] == 2223);
        CHECK(report_pass_lines(g, r.e, rows) ==
              std::vector<std::string>{"passes = 1", "natural_gaps = 0", "passes_capped = 0",
                                       "pass_col_m = 222.3 222.3 222.3", "passes_walkable = 1"});
    }
}

TEST_CASE("P1-00 10.3: the walkability measure on constructed layers") {
    // A flat land rectangle with an ocean border: every lowered pass is
    // walkable; a wall steeper than 35 deg across the band (S2 > 50204)
    // separates the two feet when it spans the whole region.
    const Grid g{120, 40};
    std::vector<std::int16_t> e(cells(g), 100);
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            if (x == 0 || y == 0 || x == g.width - 1 || y == g.height - 1) {
                e[static_cast<std::size_t>(y * g.width + x)] = -10;
            }
        }
    }
    std::vector<std::int64_t> path;
    for (std::int32_t x = 10; x <= 109; ++x) {
        path.push_back(20 * g.width + x);
    }
    CHECK(walkable_end_to_end(g, e, path));
    // A step of 300 dm between two columns gives S2 = 300^2 = 90000 on both
    // sides of the step (> 50204): an unwalkable wall from edge to edge.
    for (std::int32_t y = 1; y < g.height - 1; ++y) {
        for (std::int32_t x = 60; x < g.width - 1; ++x) {
            e[static_cast<std::size_t>(y * g.width + x)] = 400;
        }
    }
    CHECK(cell_s2(g, e, 59, 20) == 90000);
    CHECK(cell_s2(g, e, 60, 20) == 90000);
    CHECK(cell_s2(g, e, 58, 20) == 0);
    CHECK_FALSE(walkable_end_to_end(g, e, path));
    // A cell in both feet counts: a path short enough that the end discs overlap.
    CHECK(walkable_end_to_end(g, e, std::vector<std::int64_t>{20 * g.width + 10, 20 * g.width + 12}));
}

// --- Properties of 10.2 against the test's own implementation -----------------------------------

namespace {

struct PassWorld {
    std::uint64_t seed = 0;
    std::int32_t n = 0;
    elev_api::Fields f;
    elev_api::Ocean oc;
    elev_api::Heights h;
    std::vector<std::int64_t> h0;  // the test's own E5.2
    OwnLayout own;
    [[nodiscard]] PassGrids grids() const {
        return PassGrids{Grid{n, n}, h0, h.unit, h.mask, h.profile, f.nb, oc.mask, h.scale, h.floor.o_star, f.w_b};
    }
};

PassWorld pass_world(std::uint64_t seed, std::int32_t n, std::string_view fx) {
    const TuningPair& tp = fixture(fx);
    PassWorld w;
    w.seed = seed;
    w.n = n;
    w.f = elev_api::fields(seed, n, tp.tuning, THREADS);
    w.oc = elev_api::classify(w.f, tp.tuning);
    w.h = elev_api::heights(seed, w.f, w.oc, tp.tuning, THREADS);
    const std::size_t nn = cells(Grid{n, n});
    w.h0.assign(nn, 0);
    // E5.2 by the test: h0, unit, mask and profile per land cell.
    for (std::size_t i = 0; i < nn; ++i) {
        if (w.oc.mask[i] != 0U) {
            continue;
        }
        const auto x = static_cast<std::int32_t>(i % static_cast<std::size_t>(n));
        const auto y = static_cast<std::int32_t>(i / static_cast<std::size_t>(n));
        const CellHeights ch = e52_cell(seed, x, y, w.f.c[i], w.f.nb[i], w.f.ns[i], w.f.c_sea, w.f.s_land, w.f.w_b,
                                        w.h.floor.o_star, w.h.floor.beta_star, tp.values);
        w.h0[i] = ch.h0;
        REQUIRE(ch.unit == w.h.unit[i]);
        REQUIRE(ch.mask == w.h.mask[i]);
        REQUIRE(ch.profile == w.h.profile[i]);
    }
    w.own = own_pass_layout(seed, w.grids(), tp.values);
    return w;
}

// Rule counts seen by the property runs (10.2 "Rule coverage").
struct Coverage {
    std::map<std::string, int> rules;
    int capped = 0, uncapped = 0, two_on_range = 0, gaps = 0;
};

void check_pass_world(std::uint64_t seed, std::int32_t n, std::string_view fx, Coverage* cov) {
    CAPTURE(seed);
    CAPTURE(n);
    CAPTURE(std::string(fx));
    const TuningPair& tp = fixture(fx);
    const Values& v = tp.values;
    const PassWorld w = pass_world(seed, n, fx);
    const Grid g{n, n};
    const std::size_t nn = cells(g);
    const Values::PassInts pi = v.pass_ints(n);

    // E6.5: the stage's layout (inside elevation_heights) and the pass_layout
    // seam on the same grids both equal the test's own.
    compare_layout(g, w.h.passes, w.own);
    const PassGrids in = w.grids();
    const elev_api::PassLayout seam = elev_api::pass_layout(seed, g, in.h0, in.unit, in.mask, in.profile, in.nb,
                                                            in.ocean, in.scale, in.offset, in.w_b, tp.tuning);
    compare_layout(g, seam, w.own);

    // E7 by the test (ocean from the stage, which other tests check).
    CHECK(own_compose(in, w.own.factor, w.own.adj, w.h.h7, v) == w.h.h7);

    // Invariant 12.
    std::map<std::int32_t, std::int32_t> per_range;
    for (const OwnSite& s : w.own.sites) {
        const auto c = static_cast<std::size_t>(s.col);
        CHECK(w.own.flood.side[c] == 1U);
        CHECK(w.own.partner[c] == s.partner);
        CHECK(w.own.flood.side[static_cast<std::size_t>(s.partner)] == 2U);
        CHECK(d2of(g, s.col, s.partner) == 1);  // a 4-neighbour
        CHECK(s.dh == w.own.dh[c]);
        CHECK(s.dh == std::max(w.own.key[c], w.own.key[static_cast<std::size_t>(s.partner)]));
        const OwnRange& r = w.own.ranges[static_cast<std::size_t>(s.range - 1)];
        CHECK(std::find(r.divide.begin(), r.divide.end(), s.col) != r.divide.end());
        per_range[s.range] += 1;
        if (s.gap) {
            CHECK((s.maxm << 16) <= s.saddle);
        }
        if (cov != nullptr) {
            cov->gaps += s.gap ? 1 : 0;
            cov->capped += (!s.gap && s.capped) ? 1 : 0;
            cov->uncapped += (!s.gap && !s.capped) ? 1 : 0;
        }
    }
    for (const auto& [k, count] : per_range) {
        const OwnRange& r = w.own.ranges[static_cast<std::size_t>(k - 1)];
        CHECK(count <= r.slots);
        CHECK(r.slots <= v.i("pass_count_max"));
        if (cov != nullptr && count >= 2) {
            cov->two_on_range += 1;
        }
    }
    for (std::size_t a = 0; a < w.own.sites.size(); ++a) {
        for (std::size_t b = a + 1; b < w.own.sites.size(); ++b) {
            CHECK(d2of(g, w.own.sites[a].col, w.own.sites[b].col) >= pi.spacing_b * pi.spacing_b);
        }
    }
    if (cov != nullptr) {
        for (const OwnTry& t : w.own.tries) {
            cov->rules[t.rule] += 1;
        }
    }

    // Invariant 14: the cut at the crossing's highest point is at most the cap.
    for (const auto& s : w.h.passes.sites) {
        if (s.natural_gap) {
            continue;
        }
        std::int64_t top = 0;
        for (const auto c : s.axis) {
            const auto ci = static_cast<std::size_t>(c);
            top = std::max(top, elev_api::pass_lowered(w.h.scale, w.h.mask[ci], w.h.profile[ci], ONE - s.depth.raw(),
                                                       v.raw("foothill_fraction"), v.raw("pass_texture_keep")));
        }
        CHECK(top >= (s.maxm << 16) - v.raw("pass_max_cut_m"));
    }

    // Invariant 13 against the neutral run (the third version's E7).
    const std::string neutral = fx == "small" ? "small-neutral" : "shipped-neutral";
    const elev_api::Heights h0n = elev_api::heights(seed, w.f, w.oc, fixture(neutral).tuning, THREADS);
    const std::int64_t r = (w.h.scale >> 32) + 1;
    for (std::size_t i = 0; i < nn; ++i) {
        if (w.oc.mask[i] != 0U) {
            REQUIRE(w.h.h7[i] == h0n.h7[i]);
            REQUIRE(w.own.factor[i] == ONE);
            REQUIRE(w.own.adj[i] == 0);
            continue;
        }
        REQUIRE(w.h.h7[i] <= h0n.h7[i] + r);
        if (w.own.factor[i] == ONE && w.own.adj[i] == 0) {
            REQUIRE(w.h.h7[i] == h0n.h7[i]);
        }
        if (w.own.adj[i] != 0) {
            REQUIRE(w.h.h7[i] <= h0n.h7[i]);
        }
        REQUIRE(w.own.factor[i] >= 0);
        REQUIRE(w.own.factor[i] <= ONE);
    }

    // Invariants 10 and 15 and the layer.
    const elev_api::Fields fn = elev_api::fields(seed, n, fixture(neutral).tuning, THREADS);
    CHECK(elev_api::classify(fn, fixture(neutral).tuning).mask == w.oc.mask);
}

void check_final_talus_world(std::uint64_t seed, std::int32_t n, std::string_view fx) {
    CAPTURE(seed);
    CAPTURE(n);
    const TuningPair& tp = fixture(fx);
    const Values& v = tp.values;
    const Grid g{n, n};
    const elev_api::Fields f = elev_api::fields(seed, n, tp.tuning, THREADS);
    const elev_api::Ocean oc = elev_api::classify(f, tp.tuning);
    const elev_api::Heights h = elev_api::heights(seed, f, oc, tp.tuning, THREADS);
    elev_api::ErosionParams p;
    p.iterations = v.i("erosion_iterations");
    p.route_interval = v.i("erosion_route_interval");
    p.k = v.raw("erosion_k");
    p.diffusion = v.raw("erosion_diffusion");
    p.talus_slope = v.raw("talus_slope");
    p.talus_rate = v.raw("talus_rate");
    p.floor = v.raw("land_floor_m");
    p.key = key(seed, "erosion");
    const std::vector<std::int64_t> h8 = elev_api::erode(g, h.h7, oc.mask, p, THREADS);
    const Values::PassInts pi = v.pass_ints(n);
    const auto iters = static_cast<std::int32_t>(v.i("final_talus_iterations"));
    const std::vector<std::int64_t> h8t = elev_api::final_talus(g, h8, oc.mask, pi.drop_o, pi.drop_d,
                                                                v.raw("final_talus_rate"), v.raw("land_floor_m"), iters, THREADS);
    CHECK(h8t == own_final_talus(g, h8, oc.mask, pi.drop_o, pi.drop_d, v.raw("final_talus_rate"), v.raw("land_floor_m"),
                                 iters));
    // The layer's sign is the ocean mask of E4 (invariants 4 and 10).
    const std::vector<std::int16_t> e = elev_api::elevation_grid(seed, n, tp.tuning, THREADS);
    for (std::size_t i = 0; i < e.size(); ++i) {
        REQUIRE((e[i] < 0) == (oc.mask[i] != 0U));
    }
}

const std::vector<std::uint64_t>& coverage_seeds() {
    // The seeds of 4.4 that reach every rule the small-map fixture reaches,
    // a capped and an uncapped pass and two passes on one range.
    static const std::vector<std::uint64_t> s = {0, 2, 8, 9, 11, 20, 23, 30, 31, 38};
    return s;
}

void check_coverage(const Coverage& cov) {
    for (const char* rule : {"coast", "barrier", "side", "chord", "tarn_crossing", "col_tarn", "summit_top", "zone_p90",
                             "feet", "shoulders", "site"}) {
        CAPTURE(rule);
        CHECK(cov.rules.count(rule) == 1U);
    }
    CHECK(cov.rules.count("along_divide") + cov.rules.count("along_crest") >= 1U);  // rule (e)
    CHECK(cov.capped >= 1);
    CHECK(cov.uncapped >= 1);
    CHECK(cov.two_on_range >= 1);
}

}  // namespace

TEST_CASE("P1-01 10.2: E6.5 and E7 against the test's own implementation, invariants 10 to 14 (small fixture, "
          "N = 128, the rule-coverage seeds; shipped N = 128 seeds 0 to 3)") {
    Coverage cov;
    for (const std::uint64_t seed : coverage_seeds()) {
        check_pass_world(seed, 128, "small", &cov);
    }
    check_coverage(cov);
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        check_pass_world(seed, 128, "shipped", nullptr);
    }
    // A non-power-of-two size and a smaller map.
    check_pass_world(3, 48, "shipped", nullptr);
    check_pass_world(0, 64, "small", nullptr);
}

TEST_CASE("P1-01 10.2: E8.7 on the stage's E8 output equals plain Jacobi steps (N = 128, small fixture seeds 8, 31; "
          "shipped seed 0)") {
    check_final_talus_world(8, 128, "small");
    check_final_talus_world(31, 128, "small");
    check_final_talus_world(0, 128, "shipped");
}

TEST_CASE("P1-01 10.3: the divide flood and the Dijkstras do not depend on the queue structure (N = 128 fixtures)") {
    for (const auto& [seed, fx] : std::vector<std::pair<std::uint64_t, std::string>>{
             {8, "small"}, {31, "small"}, {9, "small"}, {0, "shipped"}}) {
        CAPTURE(seed);
        CAPTURE(fx);
        const PassWorld w = pass_world(seed, 128, fx);
        const Grid g{128, 128};
        const OwnLayout other = own_pass_layout(seed, w.grids(), fixture(fx).values, Queue::ordered_set);
        // The whole layout, with every flood and search on an ordered set.
        compare_layout(g, w.h.passes, other);
        if (!w.own.ran_divide) {
            continue;
        }
        CHECK(other.flood.parent == w.own.flood.parent);
        CHECK(other.flood.side == w.own.flood.side);
        CHECK(other.flood.level == w.own.flood.level);
        CHECK(other.range_search.dist == w.own.range_search.dist);
        CHECK(other.range_search.src == w.own.range_search.src);
        // The stage's divide flood seam gives the same parents and sides.
        const worldgen::stages::DivideFlood fl =
            elev_api::divide_flood(g, w.own.support, w.own.key, w.f.nb, w.h.floor.o_star);
        REQUIRE(fl.parent.size() == cells(g));
        for (std::size_t i = 0; i < cells(g); ++i) {
            if (w.own.flood.side[i] == 0U) {
                REQUIRE(fl.side[i] == 0U);  // unreached: side 0
                continue;
            }
            REQUIRE(static_cast<std::int64_t>(fl.parent[i]) == w.own.flood.parent[i]);
            REQUIRE(fl.side[i] == w.own.flood.side[i]);
        }
    }
}

TEST_CASE("P1-01 4.2, E6.5: pass_count_max = 0 places no site and leaves P = ONE and adj = 0") {
    for (const std::uint64_t seed : {8U, 31U}) {
        CAPTURE(seed);
        const PassWorld w = pass_world(seed, 128, "small-neutral");
        CHECK(w.h.passes.sites.empty());
        CHECK(w.h.passes.tries.empty());
        CHECK(w.h.passes.factor == std::vector<std::int64_t>(cells(Grid{128, 128}), ONE));
        CHECK(w.h.passes.adj == std::vector<std::int64_t>(cells(Grid{128, 128}), 0));
        for (const auto& r : w.h.passes.ranges) {
            CHECK(r.slots == 0);
        }
    }
}

// --- report: the pass keys from table `pass` (P1-00 10.3) ---------------------------------

namespace {

// A fixture world: a 120 x 40 elevation layer (flat land 10 m inside an
// ocean border, a wall of 40 m from x = 60 on), and the given SQL run on
// the finished file (rows of table `pass`).
std::string pass_fixture_world(const dbtest::TempDir& dir, const std::string& name, const std::string& sql,
                               std::vector<std::int16_t>* layer_out = nullptr) {
    const std::uint32_t w = 120;
    const std::uint32_t h = 40;
    std::vector<std::int16_t> e(static_cast<std::size_t>(w) * h, 100);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * w + x;
            if (x == 0 || y == 0 || x == w - 1 || y == h - 1) {
                e[i] = -10;
            } else if (x >= 60) {
                e[i] = 400;
            }
        }
    }
    std::vector<worldgen::stages::testing::FixtureLayer> layers;
    layers.push_back({"elevation", p004::grid_of(w, h, e), worldgen::db::Codec::delta_varint});
    const std::string out = dir.file(name);
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 3, layers).ok());
    if (!sql.empty()) {
        REQUIRE(worldgen::db::testing::execute_on_file(out, sql).ok());
    }
    if (layer_out != nullptr) {
        *layer_out = e;
    }
    return out;
}

// x'..' of u32 little-endian cell indices.
std::string path_blob(const std::vector<std::int64_t>& cells_) {
    static const char* d = "0123456789abcdef";
    std::string s = "x'";
    for (const std::int64_t c : cells_) {
        for (int k = 0; k < 4; ++k) {
            const auto b = static_cast<unsigned>((static_cast<std::uint64_t>(c) >> (8 * k)) & 0xFFU);
            s += d[b >> 4U];
            s += d[b & 15U];
        }
    }
    return s + "'";
}

}  // namespace

TEST_CASE("P1-00 10.3: report pass keys from the rows of table pass, against the test's own measure") {
    dbtest::TempDir dir("p101-pass-report");
    const Grid g{120, 40};
    // Row 1: a lowered, capped pass inside the flat part (walkable); row 2:
    // a lowered pass across the 300 dm wall (not walkable); row 3: a gap.
    std::vector<std::int64_t> p1;
    std::vector<std::int64_t> p2;
    for (std::int64_t x = 3; x <= 20; ++x) {
        p1.push_back(20 * 120 + x);
    }
    for (std::int64_t x = 10; x <= 110; ++x) {
        p2.push_back(20 * 120 + x);
    }
    const std::string sql = "INSERT INTO pass VALUES (1, 1, 1, 1, 10, 20, 900, 100, " + path_blob(p1) +
                            "); INSERT INTO pass VALUES (2, 1, 1, 0, 70, 20, 950, 120, " + path_blob(p2) +
                            "); INSERT INTO pass VALUES (3, 2, 0, 0, 5, 5, 300, 300, " + path_blob({5 * 120 + 5}) + ");";
    std::vector<std::int16_t> e;
    const std::string out = pass_fixture_world(dir, "w.db", sql, &e);
    auto wr = worldgen::db::WorldReader::open(out);
    REQUIRE(wr.ok());
    const auto rep = worldgen::inspect::build_report(wr.value());
    INFO("report error: ", rep.ok() ? std::string() : rep.error().message);
    REQUIRE(rep.ok());
    auto row = [](std::int64_t id, std::int64_t range, std::int64_t kind, std::int64_t capped, std::int64_t cx,
                  std::int64_t cy, const std::vector<std::int64_t>& path) {
        PassRow r = pass_row(id, range, kind == 0, capped == 1, cy * 120 + cx, 0, 0, path, 120);
        return r;
    };
    const std::vector<PassRow> rows = {row(1, 1, 1, 1, 10, 20, p1), row(2, 1, 1, 0, 70, 20, p2),
                                       row(3, 2, 0, 0, 5, 5, {5 * 120 + 5})};
    const std::vector<std::string> want = report_pass_lines(g, e, rows);
    CHECK(want == std::vector<std::string>{"passes = 2", "natural_gaps = 1", "passes_capped = 1",
                                           "pass_col_m = 10.0 40.0 40.0", "passes_walkable = 1"});
    std::string block;
    for (const std::string& l : want) {
        block += l + "\n";
    }
    const std::string& text = rep.value();
    CHECK(text.find("\npass = 3\n[layers]\n") != std::string::npos);
    const std::size_t at = text.find("\nedge_parallel_run_m = ");
    REQUIRE(at != std::string::npos);
    const std::size_t next = text.find('\n', at + 1);
    CHECK(text.substr(next + 1, block.size()) == block);

    // An empty table: zeros and n/a (lead's ruling).
    const std::string empty = pass_fixture_world(dir, "e.db", "");
    auto we = worldgen::db::WorldReader::open(empty);
    REQUIRE(we.ok());
    const auto re = worldgen::inspect::build_report(we.value());
    REQUIRE(re.ok());
    CHECK(re.value().find("\npasses = 0\nnatural_gaps = 0\npasses_capped = 0\npass_col_m = n/a n/a n/a\n"
                          "passes_walkable = 0\n") != std::string::npos);
}

TEST_CASE("P1-00 10.3: a malformed row of table pass is a corrupt_layer-class error naming the table") {
    dbtest::TempDir dir("p101-pass-bad");
    struct Bad {
        std::string what, sql;
    };
    const std::vector<Bad> bads = {
        {"path cell outside the layer", "INSERT INTO pass VALUES (1, 1, 1, 0, 10, 20, 900, 100, " +
                                            path_blob({20 * 120 + 10, 120 * 40}) + ")"},
        {"empty path", "INSERT INTO pass VALUES (1, 1, 1, 0, 10, 20, 900, 100, x'')"},
        {"path length not a multiple of 4", "INSERT INTO pass VALUES (1, 1, 1, 0, 10, 20, 900, 100, x'010203')"},
        {"col outside the layer", "INSERT INTO pass VALUES (1, 1, 1, 0, 120, 20, 900, 100, " +
                                      path_blob({20 * 120 + 10}) + ")"},
        {"negative col", "INSERT INTO pass VALUES (1, 1, 1, 0, -1, 20, 900, 100, " + path_blob({20 * 120 + 10}) + ")"},
        {"col_y outside the layer", "INSERT INTO pass VALUES (1, 1, 1, 0, 10, 40, 900, 100, " +
                                        path_blob({20 * 120 + 10}) + ")"},
        {"kind 2", "INSERT INTO pass VALUES (1, 1, 2, 0, 10, 20, 900, 100, " + path_blob({20 * 120 + 10}) + ")"},
        {"capped 2", "INSERT INTO pass VALUES (1, 1, 1, 2, 10, 20, 900, 100, " + path_blob({20 * 120 + 10}) + ")"},
        {"capped 1 on a natural gap", "INSERT INTO pass VALUES (1, 1, 0, 1, 10, 20, 900, 100, " +
                                          path_blob({20 * 120 + 10}) + ")"},
        {"a bad second row", "INSERT INTO pass VALUES (1, 1, 1, 0, 10, 20, 900, 100, " + path_blob({20 * 120 + 10}) +
                                 "); INSERT INTO pass VALUES (2, 1, 1, 0, 10, 20, 900, 100, x'')"},
    };
    int k = 0;
    for (const Bad& b : bads) {
        CAPTURE(b.what);
        const std::string out = pass_fixture_world(dir, "b" + std::to_string(k++) + ".db", b.sql);
        auto w = worldgen::db::WorldReader::open(out);
        REQUIRE(w.ok());
        const auto r = worldgen::inspect::build_report(w.value());
        REQUIRE_FALSE(r.ok());
        CHECK(r.code() == worldgen::db::ErrorCode::corrupt_layer);
        // P1-00 10.3: the error names table pass; the reason text is free.
        CHECK(r.error().message.rfind("corrupt table 'pass': ", 0) == 0);
    }
}

TEST_SUITE("elevation-slow" * doctest::skip()) {

TEST_CASE("P1-01 10.2 (slow): E6.5 properties, N = 128 seeds 0 to 63 with both fixtures; N = 512 seeds 0 to 3") {
    Coverage cov;
    for (std::uint64_t seed = 0; seed < 64; ++seed) {
        check_pass_world(seed, 128, "small", &cov);
        check_pass_world(seed, 128, "shipped", nullptr);
    }
    check_coverage(cov);
    // 4.4: 20 lowered passes in 19 worlds, 18 capped and 2 uncapped, no natural gap.
    CHECK(cov.capped == 18);
    CHECK(cov.uncapped == 2);
    CHECK(cov.gaps == 0);
    for (std::uint64_t seed = 0; seed < 4; ++seed) {
        check_pass_world(seed, 512, "shipped", nullptr);
        check_final_talus_world(seed, 512, "shipped");
    }
}

}  // TEST_SUITE
