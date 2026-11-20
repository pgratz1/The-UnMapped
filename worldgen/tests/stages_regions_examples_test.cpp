// The worked examples of the regions stage and the unit tests of its steps
// (spec P1-06 5.1 and 10.1 tests 1 to 3, 5; P0-04 amendment 18 (e)).
//
// Every example of 5.1 is run through the stage's seams
// (p1_06_regions_api.hpp) and, as a check of the test's own code, through
// the test's own steps (p1_06_regions_support.hpp). The expected values are
// those printed in the spec; each was reproduced by the test writer's
// reference (worldgen/tests/reference/p1_regions_reference.py check).

#include <doctest/doctest.h>

#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "p1_06_regions_api.hpp"
#include "p1_06_regions_support.hpp"
#include "p3_08_switch.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"

using namespace region_test;

namespace {

// A W x H grid printed as rows of numbers, top (y = 0) to bottom.
std::vector<std::string> rows_of(const std::vector<std::int64_t>& v, std::int32_t w, std::int32_t h) {
    std::vector<std::string> out;
    for (std::int32_t y = 0; y < h; ++y) {
        std::string s;
        for (std::int32_t x = 0; x < w; ++x) {
            s += (x ? " " : "") + std::to_string(v[static_cast<std::size_t>(y * w + x)]);
        }
        out.push_back(s);
    }
    return out;
}

constexpr std::int32_t FW = 9, FH = 5;
const Grid FG{FW, FH};

struct FloodCase {
    std::vector<std::uint8_t> water = std::vector<std::uint8_t>(FW * FH, WATER_DRY);
    std::vector<std::uint8_t> river = std::vector<std::uint8_t>(FW * FH, 0);
    std::vector<std::int64_t> wt = std::vector<std::int64_t>(FW * FH, ONE);
    std::vector<std::int64_t> seeds;
};

std::int64_t at(std::int32_t x, std::int32_t y) { return static_cast<std::int64_t>(FG.index(x, y)); }

// Runs the seam and the test's own flood; both must give `owners` and the
// cost rows named in `costs` (row index, text).
void check_flood(const FloodCase& f, const std::vector<std::string>& owners,
                 const std::vector<std::pair<int, std::string>>& costs) {
    const region_api::Flood got = region_api::flood(FG, f.wt, f.water, f.river, f.seeds, tuning());
    CHECK(rows_of(got.owner, FW, FH) == owners);
    for (const auto& [row, text] : costs) {
        CAPTURE(row);
        CHECK(rows_of(got.cost, FW, FH)[static_cast<std::size_t>(row)] == text);
    }
    const Values v = values();
    const OwnFlood own = own_flood(FG, f.wt, f.water, border_river(f.water, f.river, v.river_border_class), f.seeds, v);
    CHECK(rows_of(own.owner, FW, FH) == owners);
    for (const auto& [row, text] : costs) {
        CHECK(rows_of(own.cost, FW, FH)[static_cast<std::size_t>(row)] == text);
    }
}

const std::vector<std::string> FLAT_OWNERS(5, "1 1 1 1 1 2 2 2 2");
const std::vector<std::string> SPLIT_AT_3(5, "1 1 1 1 2 2 2 2 2");

}  // namespace

// --- RG3 -----------------------------------------------------------------------------------------

TEST_CASE("P1-06 5.1 RG3: step weights for hand 600 and 300 dm; the step costs 544 and 769") {
    const region_api::Tuning t = tuning();
    CHECK(region_api::step_weight(0, 600, t) == 14602888806);
    CHECK(region_api::step_weight(0, 300, t) == 9448928051);
    CHECK(region_api::step_weight(0, 0, t) == ONE);
    const Values v = values();
    CHECK(own_step_weight(0, 600, v) == 14602888806);
    CHECK(own_step_weight(0, 300, v) == 9448928051);
    const std::int64_t w = 14602888806;
    CHECK(Fx::from_raw(w * 160).round_int() == 544);
    CHECK(worldgen::mul(Fx::from_raw(w), Fx::from_raw(D160)).round_int() == 769);
    CHECK(D160 == 971840160000);
    CHECK(worldgen::mul_int(Fx::SQRT2, 160).raw() == D160);
    // nf = 1 + amp nz over nz in [-1, 1] with the largest amplitude 0.9 (7: [0.1, 1.9]).
    const region_api::Tuning t9 = tuning({{"cost_noise_amp", "0.9"}});
    CHECK(region_api::step_weight(-ONE, 0, t9) == ONE - fx_of("0.9"));
    CHECK(region_api::step_weight(ONE, 0, t9) == ONE + fx_of("0.9"));
    // A shipped amplitude of 0.5 and hand 250 dm: (1 + 0.5 nz)(1 + 1).
    CHECK(region_api::step_weight(ONE / 2, 250, t) == worldgen::mul(Fx::from_raw(ONE + ONE / 4), Fx::from_int(2)).raw());
}

TEST_CASE("P1-06 RG3: the cost noise is fbm_m of geo.regions.cost at the cell centre (own fbm2, a lattice)") {
    const region_api::Tuning t = tuning();
    const Values v = values();
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{42}}) {
        const std::uint64_t key = worldgen::stream_key(seed, STREAM_COST);
        for (std::int32_t y = 0; y < 300; y += 37) {
            for (std::int32_t x = 0; x < 300; x += 41) {
                if (region_api::cost_noise(seed, x, y, t) != own_cost_noise(key, x, y, v)) {
                    CAPTURE(seed);
                    CAPTURE(x);
                    CAPTURE(y);
                    CHECK(region_api::cost_noise(seed, x, y, t) == own_cost_noise(key, x, y, v));
                    return;
                }
            }
        }
    }
}

// --- RG7 -----------------------------------------------------------------------------------------

TEST_CASE("P1-06 5.1 RG7, flat: owners split between columns 4 and 5; row 2 costs") {
    FloodCase f;
    f.seeds = {at(1, 2), at(8, 2)};
    check_flood(f, FLAT_OWNERS, {{2, "160 0 160 320 480 480 320 160 0"}});
}

TEST_CASE("P1-06 5.1 RG7, ridge: hand 600 dm in column 3 moves the border onto the ridge's east foot") {
    FloodCase f;
    f.seeds = {at(1, 2), at(8, 2)};
    for (std::int32_t y = 0; y < FH; ++y) {
        f.wt[FG.index(3, y)] = 14602888806;
    }
    check_flood(f, SPLIT_AT_3,
                {{0, "386 320 386 930 772 612 452 386 320"},
                 {1, "226 160 226 770 706 546 386 226 160"},
                 {2, "160 0 160 704 640 480 320 160 0"},
                 {3, "226 160 226 770 706 546 386 226 160"},
                 {4, "386 320 386 930 772 612 452 386 320"}});
}

TEST_CASE("P1-06 5.1 RG7, large river: column 3 of class 6 is A's and nobody crosses; class 5 is the flat case") {
    for (const std::uint8_t cls : {std::uint8_t{6}, std::uint8_t{5}}) {
        CAPTURE(int{cls});
        FloodCase f;
        f.seeds = {at(1, 2), at(8, 2)};
        for (std::int32_t y = 0; y < FH; ++y) {
            f.water[FG.index(3, y)] = WATER_RIVER;
            f.river[FG.index(3, y)] = cls;
        }
        if (cls == 6) {
            check_flood(f, SPLIT_AT_3, {{2, "160 0 160 20320 640 480 320 160 0"}});
        } else {
            check_flood(f, FLAT_OWNERS, {{2, "160 0 160 320 480 480 320 160 0"}});
        }
    }
}

TEST_CASE("P1-06 5.1 RG7, diagonal river of class 7: the corner rule keeps (3, 0) B's") {
    FloodCase f;
    f.seeds = {at(0, 4), at(8, 0)};
    for (const auto& [x, y] : std::vector<std::pair<int, int>>{{2, 0}, {3, 1}, {4, 2}, {5, 3}, {6, 4}}) {
        f.water[FG.index(x, y)] = WATER_RIVER;
        f.river[FG.index(x, y)] = 7;
    }
    check_flood(f,
                {"1 1 1 2 2 2 2 2 2", "1 1 1 1 2 2 2 2 2", "1 1 1 1 1 2 2 2 2", "1 1 1 1 1 2 2 2 2",
                 "1 1 1 1 1 1 2 2 2"},
                {{0, "640 706 20772 800 640 480 320 160 0"}, {4, "0 160 320 480 640 800 20772 706 640"}});
    // The diagonal step (2, 1) -> (3, 0) between (2, 0) and (3, 1) costs 226 + 20000.
    const Values v = values();
    CHECK(own_step_cost(FG, FG.index(2, 1), FG.index(3, 0), 7, f.wt, f.water, border_river(f.water, f.river, 6), v) ==
          20226);
}

TEST_CASE("P1-06 5.1 RG7, lake: columns 3 to 5, rows 1 to 3; a step onto the lake costs 320, diagonally 452") {
    FloodCase f;
    f.seeds = {at(1, 2), at(8, 2)};
    for (std::int32_t y = 1; y <= 3; ++y) {
        for (std::int32_t x = 3; x <= 5; ++x) {
            f.water[FG.index(x, y)] = WATER_LAKE;
        }
    }
    check_flood(f, FLAT_OWNERS, {{2, "160 0 160 480 800 640 320 160 0"}});
    const Values v = values();
    const std::vector<std::uint8_t> none(FW * FH, 0);
    CHECK(own_step_cost(FG, FG.index(2, 2), FG.index(3, 2), 0, f.wt, f.water, none, v) == 320);
    CHECK(own_step_cost(FG, FG.index(2, 1), FG.index(3, 2), 1, f.wt, f.water, none, v) == 452);
}

TEST_CASE("P1-06 RG7: equal costs keep the owner whose entry came first; only land is flooded") {
    // A 3 x 1 strip with seeds at both ends: the middle is reached at 160
    // from both; seed 1's entry is popped first, so it is seed 1's.
    const Grid g{3, 1};
    const std::vector<std::uint8_t> water(3, WATER_DRY), river(3, 0);
    const std::vector<std::int64_t> wt(3, ONE);
    const region_api::Flood a = region_api::flood(g, wt, water, river, std::vector<std::int64_t>{0, 2}, tuning());
    CHECK(a.owner == std::vector<std::int64_t>{1, 1, 2});
    const region_api::Flood b = region_api::flood(g, wt, water, river, std::vector<std::int64_t>{2, 0}, tuning());
    CHECK(b.owner == std::vector<std::int64_t>{2, 1, 1});
    // Ocean between two islands: each island is its own seed's.
    const std::vector<std::uint8_t> w2 = {WATER_DRY, WATER_OCEAN, WATER_DRY};
    const region_api::Flood c = region_api::flood(g, wt, w2, river, std::vector<std::int64_t>{0, 2}, tuning());
    CHECK(c.owner == std::vector<std::int64_t>{1, 0, 2});
}

// --- RG5 -----------------------------------------------------------------------------------------

TEST_CASE("P1-06 5.1 RG5: five candidates; the river at x = 168 lets c5 stand; without it c5 is rejected") {
    const Grid g{200, 200};
    std::vector<std::uint8_t> water(g.cells(), WATER_DRY), river(g.cells(), 0);
    for (std::int32_t y = 0; y < 200; ++y) {
        water[g.index(168, y)] = WATER_RIVER;
        river[g.index(168, y)] = 6;
    }
    auto cell = [&](std::int32_t x, std::int32_t y) { return static_cast<std::int64_t>(g.index(x, y)); };
    // Equal S; h(c2) > h(c1) (5.1).
    const std::vector<region_api::Candidate> cands = {
        {cell(100, 100), 200, 50, 1}, {cell(160, 100), 200, 50, 2}, {cell(150, 175), 180, 50, 7},
        {cell(40, 40), 150, 50, 3},   {cell(175, 120), 90, 50, 9},
    };
    const std::vector<std::int64_t> want = {cell(160, 100), cell(150, 175), cell(40, 40), cell(175, 120)};
    CHECK(region_api::seeds(cands, g, water, river, tuning()) == want);
    const std::vector<std::uint8_t> no_river(g.cells(), 0);
    const std::vector<std::uint8_t> dry(g.cells(), WATER_DRY);
    CHECK(region_api::seeds(cands, g, dry, no_river, tuning()) ==
          std::vector<std::int64_t>{cell(160, 100), cell(150, 175), cell(40, 40)});
    // The river is a border only from river_border_class on.
    CHECK(region_api::seeds(cands, g, water, river, tuning({{"river_border_class", "7"}})) ==
          std::vector<std::int64_t>{cell(160, 100), cell(150, 175), cell(40, 40)});
    // The test's own selection on the same order.
    const Values v = values();
    const std::vector<std::int64_t> order = {cell(160, 100), cell(100, 100), cell(150, 175), cell(40, 40),
                                             cell(175, 120)};
    CHECK(own_select(g, order, border_river(water, river, 6), v) == want);
    // Crossing tests of 5.1.
    const std::vector<std::uint8_t> b = border_river(water, river, 6);
    CHECK(region_api::crosses(g, b, cell(160, 100), cell(175, 120)));
    CHECK(region_api::crosses(g, b, cell(150, 175), cell(175, 120)));
    CHECK_FALSE(region_api::crosses(g, b, cell(160, 100), cell(100, 100)));
    CHECK(own_crosses(g, b, 160, 100, 175, 120));
    // Distances of 5.1 in cells^2 (near <=> < 5625).
    CHECK(10 * 10 + 75 * 75 == 5725);
    CHECK(15 * 15 + 20 * 20 == 625);
    CHECK(25 * 25 + 55 * 55 == 3650);
    CHECK(own_rha(150, 20) == 8);
    CHECK(own_rha(-150, 20) == -8);
}

TEST_CASE("P1-06 RG5: the diagonal slip between two corner river cells is a crossing; one corner is not") {
    const Grid g{10, 10};
    std::vector<std::uint8_t> b(g.cells(), 0);
    b[g.index(4, 3)] = 1;
    b[g.index(3, 4)] = 1;
    auto cell = [&](std::int32_t x, std::int32_t y) { return static_cast<std::int64_t>(g.index(x, y)); };
    CHECK(region_api::crosses(g, b, cell(3, 3), cell(4, 4)));
    CHECK(region_api::crosses(g, b, cell(3, 3), cell(5, 5)));
    CHECK(region_api::crosses(g, b, cell(4, 4), cell(3, 3)));
    b[g.index(3, 4)] = 0;
    CHECK_FALSE(region_api::crosses(g, b, cell(3, 3), cell(4, 4)));
    CHECK_FALSE(own_crosses(g, b, 3, 3, 4, 4));
    // A seed exactly one spacing away is not near (strict <): 75 cells = 1200 m.
    const Grid wide{80, 1};
    const std::vector<std::uint8_t> ww(80, WATER_DRY), wr(80, 0);
    const std::vector<region_api::Candidate> two = {{0, 10, 0, 1}, {75, 9, 0, 1}};
    CHECK(region_api::seeds(two, wide, ww, wr, tuning()).size() == 2U);
    const std::vector<region_api::Candidate> close = {{0, 10, 0, 1}, {74, 9, 0, 1}};
    CHECK(region_api::seeds(close, wide, ww, wr, tuning()).size() == 1U);
}

TEST_CASE("P1-06 site order: habitability, then the lower surface, then the larger hash, then the index") {
    // All far apart for a spacing of 160 m (10 cells): a 200-cell strip.
    const Grid strip{200, 1};
    const std::vector<std::uint8_t> sw(200, WATER_DRY), sr(200, 0);
    const region_api::Tuning t = tuning({{"seed_spacing_m", "160"}, {"seed_tile_m", "256"}});
    const std::vector<region_api::Candidate> c = {
        {0, 5, 10, 9}, {20, 5, 9, 1}, {40, 7, 50, 0}, {60, 5, 9, 4}, {80, 5, 9, 4},
    };
    CHECK(region_api::seeds(c, strip, sw, sr, t) == std::vector<std::int64_t>{40, 60, 80, 20, 0});
}

// --- RG8 -----------------------------------------------------------------------------------------

namespace {

// The grid of the RG8 example (reference merge_example_grid): regions 1
// (500 cells), 2 (120), 3 (90), 4 (60, an island); n(1, 2) = 30, n(1, 3) =
// 12 of which 10 touch river cells of class 6, n(2, 3) = 8.
struct MergeGrid {
    Grid g{32, 40};
    std::vector<std::int64_t> owner = std::vector<std::int64_t>(32 * 40, 0);
    std::vector<std::uint8_t> water = std::vector<std::uint8_t>(32 * 40, WATER_OCEAN);
    std::vector<std::uint8_t> river = std::vector<std::uint8_t>(32 * 40, 0);
};

MergeGrid merge_grid() {
    MergeGrid m;
    auto put = [&](int x0, int x1, int y0, int y1, std::int64_t o) {
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                m.owner[m.g.index(x, y)] = o;
                m.water[m.g.index(x, y)] = WATER_DRY;
            }
        }
    };
    put(0, 24, 0, 19, 1);
    put(25, 28, 0, 19, 2);
    put(15, 24, 20, 20, 2);
    put(25, 29, 20, 25, 2);
    put(3, 14, 20, 20, 3);
    put(3, 21, 21, 21, 3);
    put(3, 14, 22, 25, 3);
    put(3, 13, 26, 26, 3);
    put(0, 9, 33, 38, 4);
    for (int x = 3; x < 13; ++x) {
        m.water[m.g.index(x, 20)] = WATER_RIVER;
        m.river[m.g.index(x, 20)] = 6;
    }
    return m;
}

std::vector<std::int64_t> cells_per(const std::vector<std::int64_t>& owner) {
    std::vector<std::int64_t> c(5, 0);
    for (const std::int64_t o : owner) {
        c[static_cast<std::size_t>(o)] += o != 0 ? 1 : 0;
    }
    return {c[1], c[2], c[3], c[4]};
}

}  // namespace

TEST_CASE("P1-06 5.1 RG8: region 3 merges into 2 (more non-river border) although its longest border is with 1") {
    const MergeGrid m = merge_grid();
    CHECK(cells_per(m.owner) == std::vector<std::int64_t>{500, 120, 90, 60});
    const std::vector<std::uint8_t> b = border_river(m.water, m.river, 6);
    const PairCounts pc = own_pair_counts(m.g, m.owner, b);
    CHECK(pc.at({1, 2}) == std::array<std::int64_t, 2>{30, 30});
    CHECK(pc.at({1, 3}) == std::array<std::int64_t, 2>{12, 2});
    CHECK(pc.at({2, 3}) == std::array<std::int64_t, 2>{8, 8});
    CHECK(pc.size() == 3U);
    // min_cells 100: region_min_area_m2 = 25600.
    const region_api::Tuning t = tuning({{"region_min_area_m2", "25600"}});
    const std::vector<std::int64_t> got = region_api::merge(m.g, m.owner, m.water, m.river, 4, 100, t);
    CHECK(cells_per(got) == std::vector<std::int64_t>{500, 210, 0, 60});
    std::vector<std::int64_t> want = m.owner;
    for (std::int64_t& o : want) {
        o = o == 3 ? 2 : o;
    }
    CHECK(got == want);
    std::int64_t merges = 0;
    CHECK(own_merge(m.g, m.owner, b, 4, 100, &merges) == got);
    CHECK(merges == 1);
    // On the graph: n(1, 2) becomes 42, nr 32 (5.1); nothing more merges.
    const auto f = own_merge_graph({{1, 500}, {2, 120}, {3, 90}, {4, 60}},
                                   {{{1, 2}, {30, 30}}, {{1, 3}, {12, 2}}, {{2, 3}, {8, 8}}}, 100);
    CHECK(f == std::map<std::int64_t, std::int64_t>{{1, 1}, {2, 2}, {3, 2}, {4, 4}});
    // Region 4 (60 cells, no neighbour) stays whatever the minimum.
    const std::vector<std::int64_t> big = region_api::merge(m.g, m.owner, m.water, m.river, 4, 100000, t);
    CHECK(cells_per(big)[3] == 60);
    CHECK(cells_per(big)[0] + cells_per(big)[1] + cells_per(big)[2] == 710);
    // min_cells 0: nothing merges.
    CHECK(region_api::merge(m.g, m.owner, m.water, m.river, 4, 0, t) == m.owner);
}

TEST_CASE("P1-06 RG8: ties go to the smaller (cells, number) and the neighbour with the smaller number") {
    // Graph-level: regions 2 and 3 both 10 cells; 2 merges first; its two
    // neighbours 1 and 4 have equal (nr, n): 1 is taken.
    const auto f = own_merge_graph({{1, 100}, {2, 10}, {3, 10}, {4, 100}},
                                   {{{1, 2}, {5, 5}}, {{2, 4}, {5, 5}}, {{3, 4}, {6, 6}}}, 50);
    CHECK(f == std::map<std::int64_t, std::int64_t>{{1, 1}, {2, 1}, {3, 4}, {4, 4}});
    // On a strip through the seam: 1 1 1 2 3 3 3 with min 2: region 2 (one
    // cell) has equal borders with 1 and 3 and goes to 1.
    const Grid g{7, 1};
    const std::vector<std::int64_t> owner = {1, 1, 1, 2, 3, 3, 3};
    const std::vector<std::uint8_t> water(7, WATER_DRY), river(7, 0);
    CHECK(region_api::merge(g, owner, water, river, 3, 2, tuning()) == std::vector<std::int64_t>{1, 1, 1, 1, 3, 3, 3});
}

// --- RG11 ----------------------------------------------------------------------------------------

TEST_CASE("P1-06 5.1 RG11: base names at N = 1024, centroid (500, 480), r_c 123; sectors (r3: no numerals)") {
    const region_api::Tuning t = tuning();
    const Values v = values();
    CHECK(v.name_central_frac == 515396076);
    CHECK(worldgen::mul_int(Fx::from_raw(v.name_central_frac), 1024).round_int() == 123);
    const std::vector<region_api::NameInput> in = {{520, 470, 4, false}, {800, 500, 3, false}, {200, 200, 7, false},
                                                   {510, 900, 5, false}, {790, 560, 3, false}, {500, 100, 3, true}};
    // r3 (RG11 4): the base names; the second `East Wood` is not the keeper
    // (which keeps the base name is decided by cells, and the other draws:
    // stages_regions_names_test.cpp). Before r3 it was `East Wood II`.
    const std::vector<std::string> want = {"Central Downs", "East Wood", "Northwest Peaks",
                                           "South Marsh",   "East Wood", "North Isle"};
    CHECK(own_name_list(in, 500, 480, 123, v, nouns_of(definitions())) == want);
    (void)t;
    // Sector checks of 5.1 (outside the central disc).
    const std::vector<std::pair<std::pair<int, int>, int>> sec = {
        {{10, 4}, 3}, {{10, 5}, 4}, {{-3, -7}, 8}, {{0, -9}, 1}, {{4, 10}, 5}, {{5, 12}, 4}};
    for (const auto& [d, s] : sec) {
        CAPTURE(d.first);
        CAPTURE(d.second);
        CHECK(region_api::sector(d.first, d.second, 0) == s);
        CHECK(own_sector(d.first, d.second, 0) == s);
    }
    // Boundaries of 29 ay <= 12 ax and the central disc (strict).
    CHECK(region_api::sector(29, 12, 0) == 3);
    CHECK(region_api::sector(29, 13, 0) == 4);
    CHECK(region_api::sector(-12, 29, 0) == 5);
    CHECK(region_api::sector(-13, -29, 0) == 8);
    CHECK(region_api::sector(-29, 1, 0) == 7);
    CHECK(region_api::sector(13, -29, 0) == 2);
    CHECK(region_api::sector(-13, 29, 0) == 6);
    CHECK(region_api::sector(3, 4, 5) != 0);
    CHECK(region_api::sector(3, 4, 6) == 0);
    // (r3, 10.5: the names no longer use numerals; `roman` keeps its own
    // tests in worldgen/core.)
}

TEST_CASE("P1-06 RG11 (r3): a repeated base name has no numeral; the words come from the tuning") {
    const Overrides o = {{"names", "Mid/Nor/Norost/Ost/Sudost/Sud/Sudwest/West/Norwest/Holm/Hoch"}};
    std::vector<region_api::NameInput> in;
    for (int k = 0; k < 12; ++k) {
        in.push_back({900, 480, 3, false});
    }
    in.push_back({500, 100, 3, true});
    const std::vector<std::string> got = own_name_list(in, 500, 480, 123, values(o), nouns_of(definitions()));
    REQUIRE(got.size() == 13U);
    for (std::size_t k = 0; k < 12; ++k) {
        CHECK(got[k] == "Ost Wood");
    }
    CHECK(got[12] == "Nor Holm");
}

// --- RG12 ----------------------------------------------------------------------------------------

TEST_CASE("P1-06 5.1 RG12: the 4 x 3 border row and the pass crossings (pass_id 3, passes 2)") {
    const Grid g{4, 3};
    const std::vector<std::uint16_t> region = {1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2};
    const std::vector<std::int64_t> s = {10, 20, 30, 10, 10, 50, 40, 10, 10, 25, 25, 10};
    std::vector<std::uint16_t> walk(12, 1);
    walk[g.index(1, 1)] = 0;
    walk[g.index(2, 1)] = 0;
    std::vector<std::uint8_t> water(12, WATER_DRY), river(12, 0);
    water[g.index(2, 0)] = WATER_RIVER;
    river[g.index(2, 0)] = 6;
    auto path = [&](const std::vector<std::pair<std::int32_t, std::int32_t>>& c) { return path_bytes(g, c); };
    const std::vector<region_api::Pass> passes = {
        {3, path({{0, 1}, {1, 0}, {2, 1}, {3, 1}}), 1, 0}, {5, path({{1, 2}, {2, 2}}), 2, 2},
        {6, path({{0, 0}, {1, 0}, {1, 0}}), 1, 0}};
    const region_api::BorderRow want{1, 2, 48, 25, 1, 2, 2, 1, 6, 3, 2};
    CHECK(region_api::borders(g, region, s, walk, water, river, passes, tuning()) ==
          std::vector<region_api::BorderRow>{want});
    std::vector<std::int64_t> reg64(region.begin(), region.end());
    std::int64_t crossings = 0;
    CHECK(own_borders(g, reg64, s, walk, water, river, border_river(water, river, 6), passes, &crossings) ==
          std::vector<region_api::BorderRow>{want});
    CHECK(crossings == 2);
    // The step sequence of pass 3 with its corners (1, 1) and (2, 0).
    CHECK(own_pass_steps(g, path_cells(passes[0].path), water) == std::vector<std::int64_t>{4, 5, 1, 2, 6, 7});
    // Without passes: pass_id 0, passes 0.
    const region_api::BorderRow none{1, 2, 48, 25, 1, 2, 2, 1, 6, 0, 0};
    CHECK(region_api::borders(g, region, s, walk, water, river, {}, tuning()) ==
          std::vector<region_api::BorderRow>{none});
    // 5.1 (ruling): with (2, 0) of class 4, river_pairs 0 and river_class_max 4.
    std::vector<std::uint8_t> river4 = river;
    river4[g.index(2, 0)] = 4;
    const region_api::BorderRow c4{1, 2, 48, 25, 1, 2, 2, 0, 4, 0, 0};
    CHECK(region_api::borders(g, region, s, walk, water, river4, {}, tuning()) ==
          std::vector<region_api::BorderRow>{c4});
    // river_border_class 7: the class-6 cell is no longer a border river,
    // but river_class_max still reports it.
    const region_api::BorderRow r7{1, 2, 48, 25, 1, 2, 2, 0, 6, 0, 0};
    CHECK(region_api::borders(g, region, s, walk, water, river, {}, tuning({{"river_border_class", "7"}})) ==
          std::vector<region_api::BorderRow>{r7});
}

TEST_CASE("P1-06 RG12: the second corner when the first is ocean; a diagonal step with no land corner crosses nothing") {
    const Grid g{4, 3};
    std::vector<std::uint8_t> water(12, WATER_DRY);
    for (const std::size_t i : {g.index(0, 0), g.index(2, 0), g.index(1, 1)}) {
        water[i] = WATER_OCEAN;
    }
    const std::vector<std::int64_t> p = {static_cast<std::int64_t>(g.index(0, 1)), static_cast<std::int64_t>(g.index(1, 0)),
                                         static_cast<std::int64_t>(g.index(2, 1)), static_cast<std::int64_t>(g.index(3, 1))};
    // (0,1)->(1,0): corners (1,1) ocean, (0,0) ocean: none. (1,0)->(2,1):
    // corners (2,0) ocean, (1,1) ocean: none.
    CHECK(own_pass_steps(g, p, water) == p);
    std::vector<std::uint8_t> w2(12, WATER_DRY);
    w2[g.index(2, 0)] = WATER_OCEAN;
    CHECK(own_pass_steps(g, p, w2) ==
          std::vector<std::int64_t>{4, 5, 1, 5, 6, 7});  // (1,0)->(2,1) takes the second corner (1, 1)
}

// --- Composite (P0-04 amendment 18 (e)) ------------------------------------------------------------

TEST_CASE("P0-04 amendment 18 (e): the 3 x 2 example: black cells (1, 0) and (0, 1)") {
    const Grid g{3, 2};
    const std::vector<std::uint16_t> region = {1, 1, 2, 1, 2, 2};
    CHECK(border_cells(g, region) == std::vector<bool>{false, true, false, true, false, false});
    // An ocean neighbour (0) is not a border.
    const Grid s{3, 1};
    CHECK(border_cells(s, std::vector<std::uint16_t>{1, 0, 2}) == std::vector<bool>{false, false, false});
}

// --- r2 (section 13) -------------------------------------------------------------------------------

TEST_CASE("P1-06 5.1 RG12 attachment (r2), 10.1 test 9: the crossing nearest the col; ties to the smaller j; none") {
    const Grid g{9, 1};
    const std::vector<std::uint16_t> region = {1, 1, 1, 2, 2, 2, 3, 3, 3};
    const std::vector<std::int64_t> s(9, 0);
    const std::vector<std::uint16_t> walk(9, 0);
    const std::vector<std::uint8_t> water(9, WATER_DRY), river(9, 0);
    std::vector<std::pair<std::int32_t, std::int32_t>> cells;
    for (std::int32_t x = 0; x < 9; ++x) {
        cells.emplace_back(x, 0);
    }
    const std::vector<std::uint8_t> path = path_bytes(g, cells);
    auto rows_with = [&](const std::vector<region_api::Pass>& p) {
        return region_api::borders(g, region, s, walk, water, river, p, tuning());
    };
    auto pass_cols = [](const std::vector<region_api::BorderRow>& rows) {
        std::vector<std::pair<std::int64_t, std::int64_t>> out;
        for (const auto& r : rows) {
            out.emplace_back(r.pass_id, r.passes);
        }
        return out;
    };
    using PC = std::vector<std::pair<std::int64_t, std::int64_t>>;
    // Col (5, 0), q = 5: d = 2 and 0, attached to (2, 3).
    CHECK(pass_cols(rows_with({{1, path, 5, 0}})) == PC{{0, 0}, {1, 1}});
    // Col (4, 0), q = 4: d = 1 and 1, the smaller j wins: (1, 2).
    CHECK(pass_cols(rows_with({{1, path, 4, 0}})) == PC{{1, 1}, {0, 0}});
    // Three passes: two on (2, 3), one on (1, 2); Sum of passes = 3 (invariant 8).
    CHECK(pass_cols(rows_with({{1, path, 5, 0}, {2, path, 4, 0}, {3, path, 6, 0}})) == PC{{2, 1}, {1, 2}});
    // A path inside one region: attached to none.
    const std::vector<std::uint16_t> one(9, 1);
    const std::vector<region_api::Pass> inside = {{1, path, 1, 0}};
    CHECK(region_api::borders(g, one, s, walk, water, river, inside, tuning()).empty());
    const std::vector<std::uint16_t> two = {1, 1, 1, 1, 1, 1, 1, 1, 2};
    const std::vector<region_api::Pass> short_path = {{1, path_bytes(g, {{0, 0}, {1, 0}, {2, 0}}), 1, 0}};
    CHECK(pass_cols(region_api::borders(g, two, s, walk, water, river, short_path, tuning())) == PC{{0, 0}});
    // The test's own attachment on the same strip.
    std::vector<std::int64_t> reg64(region.begin(), region.end());
    CHECK(own_pass_attachment(g, reg64, water, {1, path, 5, 0}).key == std::make_pair(std::int64_t{2}, std::int64_t{3}));
    CHECK(own_pass_attachment(g, reg64, water, {1, path, 4, 0}).key == std::make_pair(std::int64_t{1}, std::int64_t{2}));
    // A col on the path twice: its first position (q = 4 on the way out).
    std::vector<std::pair<std::int32_t, std::int32_t>> back = cells;
    for (std::int32_t x = 7; x >= 4; --x) {
        back.emplace_back(x, 0);
    }
    CHECK(pass_cols(rows_with({{1, path_bytes(g, back), 4, 0}})) == PC{{1, 1}, {0, 0}});
    // The 4 x 3 example: pass 3 (col (1, 0), q = 1) crosses at j = 1, d = 0.
    const Grid g4{4, 3};
    const std::vector<std::uint16_t> r4 = {1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2};
    const std::vector<std::uint8_t> w4(12, WATER_DRY);
    std::vector<std::int64_t> r464(r4.begin(), r4.end());
    const Attachment a = own_pass_attachment(g4, r464, w4, {3, path_bytes(g4, {{0, 1}, {1, 0}, {2, 1}, {3, 1}}), 1, 0});
    REQUIRE(a.found.size() == 1U);
    CHECK(a.found[0].d == 0);
    CHECK(a.found[0].j == 1);
}

TEST_CASE("P1-06 5.1 RG11 (r2), 10.1 test 9: Mere for regions at least half lake, Fells for upland; precedence") {
    const region_api::Tuning t = tuning();
    const Values v = values();
    // Mere (b): 120 of 200 cells lake; Fells (c): forest-dominated upland;
    // alpine upland keeps Peaks; an upland region half lake is Mere (b
    // before c); an isle is Isle whatever its lakes (a first).
    const std::vector<region_api::NameInput> in = {
        {520, 470, 4, false},
        {700, 300, 3, false, 200, 120, 80},
        {210, 700, 3, false, 100, 0, 24},
        {230, 720, 7, false, 100, 0, 24},
        {230, 720, 3, false, 100, 50, 0},
        {500, 100, 3, true, 10, 10, 0},
        {800, 480, 6, false, 100, 0, 0},
        {800, 480, 9, false, 100, 0, 0},
        {800, 480, 4, false, 100, 0, 25},
        {800, 480, 4, false, 100, 49, 24},
    };
    const std::vector<std::string> want = {"Central Downs", "Northeast Mere", "Southwest Fells", "Southwest Peaks",
                                           "Southwest Mere", "North Isle",     "East Moor",       "East Heath",
                                           "East Downs",     "East Fells"};
#if !UNMAPPED_P3_08_API
    // The r2 seam (no repeated base name here, so r3 names these regions the
    // same; the r3 seam takes more inputs: stages_regions_names_test.cpp).
    CHECK(region_api::names(in, 500, 480, 1024, t, definitions()) == want);
#endif
    CHECK(own_name_list(in, 500, 480, 123, v, nouns_of(definitions())) == want);
    // upland_own_noun empty: the alpine upland is Fells too; the Fells word from the tuning.
    const Overrides o = {{"upland_own_noun", "none"},
                         {"names", "Central/North/Northeast/East/Southeast/South/Southwest/West/Northwest/Isle/Hoch"}};
    const std::vector<region_api::NameInput> alp = {{230, 720, 7, false, 100, 0, 24}};
#if !UNMAPPED_P3_08_API
    CHECK(region_api::names(alp, 500, 480, 1024, tuning(o), definitions()) == std::vector<std::string>{"Southwest Hoch"});
#endif
    CHECK(own_name_list(alp, 500, 480, 123, values(o), nouns_of(definitions())) ==
          std::vector<std::string>{"Southwest Hoch"});
}
