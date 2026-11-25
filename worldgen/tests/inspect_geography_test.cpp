// The [geography] section of report v2, spec P1-00 section 10.3 with
// appendix C items 6 to 10: the number formats, each statistic on tiny
// hand-computed grids, the empty-set cases, the hydrology lines on the
// P1-02 5.1 worked example and on hand-made rivers, the omission of the
// hydrology lines, the failures that make report exit 3, and the whole
// section and report text byte for byte.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "db_test_support.hpp"
#include "p0_04_reference.hpp"
#include "p1_00_geography_reference.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/math.hpp"
#include "worldgen/core/wide.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/inspect/geography.hpp"
#include "worldgen/inspect/render.hpp"
#include "worldgen/inspect/report.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using worldgen::FatalError;
using worldgen::Fx;
using worldgen::U128;
using worldgen::db::Codec;
using worldgen::db::LayerGrid;
using worldgen::inspect::GeographyLayers;
using worldgen::inspect::HullPoint;
using worldgen::inspect::HydrologyCells;
using worldgen::inspect::metres1;
using worldgen::inspect::ratio4;
using worldgen::stages::testing::FixtureLayer;
using Cells = std::vector<std::int16_t>;

constexpr std::uint64_t U64_MAX = std::numeric_limits<std::uint64_t>::max();
constexpr std::int64_t I64_MIN = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t I64_MAX = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t ONE_RAW = std::int64_t{1} << 32;
constexpr std::int64_t SQRT2_RAW = 6074001000;  // P1-00 amendment A1
constexpr std::int64_t ORTHO = 16 * ONE_RAW;
constexpr std::int64_t DIAG = 16 * SQRT2_RAW;

// Builds a W x H grid from rows of characters: '#' land (value land), '.'
// ocean (value ocean).
Cells shape(const std::vector<std::string>& rows, std::int16_t land = 10, std::int16_t ocean = -10) {
    Cells c;
    for (const std::string& r : rows) {
        for (const char ch : r) {
            c.push_back(ch == '#' ? land : ocean);
        }
    }
    return c;
}

std::string line(std::string_view key, std::string_view value) {
    return std::string(key) + " = " + std::string(value) + "\n";
}

// The [geography] section of an elevation grid alone.
std::string section_of(const LayerGrid& elevation) {
    GeographyLayers layers;
    layers.elevation = &elevation;
    std::string problem;
    const auto s = worldgen::inspect::geography_section(layers, &problem);
    INFO("problem: ", problem);
    REQUIRE(s.has_value());
    return *s;
}

std::string section_of(std::uint32_t w, std::uint32_t h, const Cells& e) {
    return section_of(p100::grid(w, h, e));
}

// Reference coast_ratio as defined by P1-00 10.3 and appendix C item 10:
// Pq = isqrt_u128(4 PI.raw A 2^32), A = 256 land; value RHA(coastline 2^64 /
// Pq), computed here on U128 with the core's wide arithmetic.
std::uint64_t ref_pq(std::uint64_t land) {
    const U128 a = worldgen::mul_u64_wide(4U * static_cast<std::uint64_t>(Fx::PI.raw()), 256U * land);
    return worldgen::isqrt_u128(a << 32U);
}
std::int64_t ref_coast_ratio_raw(std::uint64_t coastline, std::uint64_t land) {
    const std::uint64_t pq = ref_pq(land);
    const auto qr = worldgen::divmod_u128_u64(U128{coastline, 0}, pq);
    std::uint64_t q = qr.quotient.lo;
    REQUIRE(qr.quotient.hi == 0U);
    if (qr.remainder >= pq - qr.remainder) {  // 2r >= pq without overflow
        ++q;
    }
    return static_cast<std::int64_t>(q);
}

// Reference path lengths D along flow_dir (P1-00 10.3), by walking.
std::vector<std::int64_t> ref_paths(const Cells& e, const std::vector<std::uint8_t>& dir, std::uint32_t w,
                                    std::uint32_t h) {
    static constexpr int DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static constexpr int DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    std::vector<std::int64_t> d(e.size(), 0);
    for (std::size_t i = 0; i < e.size(); ++i) {
        std::int64_t x = static_cast<std::int64_t>(i % w);
        std::int64_t y = static_cast<std::int64_t>(i / w);
        std::int64_t sum = 0;
        for (std::size_t steps = 0; e[static_cast<std::size_t>(y * w + x)] >= 0; ++steps) {
            REQUIRE(steps < e.size());
            const std::uint8_t k = dir[static_cast<std::size_t>(y * w + x)];
            REQUIRE(k < 8);
            sum += (k % 2 == 0) ? ORTHO : DIAG;
            x += DX[k];
            y += DY[k];
            REQUIRE((x >= 0 && y >= 0 && x < std::int64_t{w} && y < std::int64_t{h}));
        }
        d[i] = sum;
    }
    return d;
}

// Hand-made hydrology on a small map.
struct Hydro {
    std::uint32_t w = 0;
    std::uint32_t h = 0;
    Cells e;
    std::vector<std::uint8_t> water;
    std::vector<std::uint8_t> flow_dir;
    std::vector<std::uint32_t> flow_accum;
    std::vector<std::uint8_t> river;
    std::vector<std::uint32_t> lake;

    [[nodiscard]] HydrologyCells cells() const { return HydrologyCells{water, flow_dir, flow_accum, river, lake}; }
    [[nodiscard]] worldgen::inspect::HydrologyResult stats() const {
        return worldgen::inspect::hydrology_stats(e, cells(), w, h);
    }
};

Hydro worked() {
    return Hydro{p100::WX,
                 p100::WY,
                 p100::WORKED_ELEVATION,
                 p100::WORKED_WATER,
                 p100::WORKED_FLOW_DIR,
                 p100::WORKED_FLOW_ACCUM,
                 p100::WORKED_RIVER,
                 p100::WORKED_LAKE};
}

// The section of a Hydro with all five hydrology layers, or the problem.
struct SectionResult {
    std::optional<std::string> text;
    std::string problem;
};
SectionResult section_of(const Hydro& hy) {
    const LayerGrid e = p100::grid(hy.w, hy.h, hy.e);
    const LayerGrid water = p100::grid(hy.w, hy.h, hy.water);
    const LayerGrid dir = p100::grid(hy.w, hy.h, hy.flow_dir);
    const LayerGrid acc = p100::grid(hy.w, hy.h, hy.flow_accum);
    const LayerGrid river = p100::grid(hy.w, hy.h, hy.river);
    const LayerGrid lake = p100::grid(hy.w, hy.h, hy.lake);
    GeographyLayers layers{&e, &water, &dir, &acc, &river, &lake};
    SectionResult r;
    r.text = worldgen::inspect::geography_section(layers, &r.problem);
    return r;
}

bool has(std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; }

// The five lines that follow max_step_m (P1-00 10.3, added 2026-11-15).
std::string slope_lines(std::string_view p999, std::string_view over, std::string_view p50, std::string_view flat,
                        std::string_view run) {
    return line("step_p999_m", p999) + line("slope_over", over) + line("slope_p50_above_500m", p50) +
           line("lowland_flat_share", flat) + line("edge_parallel_run_m", run);
}

}  // namespace

// --- Number formats (10.3) -------------------------------------------------------------

TEST_CASE("geography 10.3: ratio4") {
    CHECK(ratio4(0, 1) == "0.0000");
    CHECK(ratio4(1, 3) == "0.3333");
    CHECK(ratio4(2, 3) == "0.6667");
    CHECK(ratio4(1, 2) == "0.5000");
    CHECK(ratio4(5, 5) == "1.0000");
    CHECK(ratio4(3, 2) == "1.5000");
    CHECK(ratio4(12345678, 1) == "12345678.0000");
    // RHA: 1/20000 = 0.5 units of 10^-4 rounds up; 1/20001 rounds down.
    CHECK(ratio4(1, 20000) == "0.0001");
    CHECK(ratio4(1, 20001) == "0.0000");
    CHECK(ratio4(3, 20000) == "0.0002");  // 1.5 -> 2
    CHECK(ratio4(9999, 10000) == "0.9999");
    CHECK(ratio4(19999, 20000) == "1.0000");  // 0.99995 -> 1.0000
    // Zero denominator (appendix C item 6).
    CHECK(ratio4(0, 0) == "n/a");
    CHECK(ratio4(7, 0) == "n/a");
    CHECK(worldgen::inspect::NOT_AVAILABLE == "n/a");
    // 128-bit exactness: a 10^4 overflows 64 bits.
    CHECK(ratio4(U64_MAX, U64_MAX) == "1.0000");
    CHECK(ratio4(U64_MAX / 3, U64_MAX) == "0.3333");
    CHECK(ratio4(U64_MAX - 1, U64_MAX) == "1.0000");
    CHECK(ratio4(U64_MAX, 10000) == "1844674407370955.1615");  // the largest value that fits
    // The result must fit 64 bits (header contract).
    CHECK_THROWS_AS((void)ratio4(U64_MAX, 1), FatalError);
}

TEST_CASE("geography 10.3: metres1") {
    CHECK(metres1(0) == "0.0");
    CHECK(metres1(5) == "0.5");
    CHECK(metres1(10) == "1.0");
    CHECK(metres1(-125) == "-12.5");
    CHECK(metres1(-5) == "-0.5");
    CHECK(metres1(-10) == "-1.0");
    CHECK(metres1(24000) == "2400.0");
    CHECK(metres1(32767) == "3276.7");
    CHECK(metres1(-32768) == "-3276.8");
    CHECK(metres1(I64_MAX) == "922337203685477580.7");
    CHECK(metres1(I64_MIN) == "-922337203685477580.8");
}

TEST_CASE("geography 10.3: the percentile rank rule") {
    using worldgen::inspect::percentile_rank;
    // n = 1: every rank is 0.
    for (const std::uint32_t p : {0U, 1U, 50U, 99U, 100U}) {
        CHECK(percentile_rank(1, p) == 0U);
    }
    // n = 10: floor(p n / 100), at most 9.
    CHECK(percentile_rank(10, 0) == 0U);
    CHECK(percentile_rank(10, 1) == 0U);
    CHECK(percentile_rank(10, 10) == 1U);
    CHECK(percentile_rank(10, 25) == 2U);
    CHECK(percentile_rank(10, 50) == 5U);
    CHECK(percentile_rank(10, 75) == 7U);
    CHECK(percentile_rank(10, 90) == 9U);
    CHECK(percentile_rank(10, 99) == 9U);
    CHECK(percentile_rank(10, 100) == 9U);
    CHECK(percentile_rank(100, 100) == 99U);
    CHECK(percentile_rank(100, 99) == 99U);
    CHECK(percentile_rank(7, 50) == 3U);
    CHECK(percentile_rank(3, 34) == 1U);
    CHECK(percentile_rank(3, 33) == 0U);
    CHECK(percentile_rank(std::uint64_t{2048} * 2048, 1) == 41943U);  // floor(4194304 / 100)
    CHECK_THROWS_AS((void)percentile_rank(0, 50), FatalError);
    CHECK_THROWS_AS((void)percentile_rank(10, 101), FatalError);
}

// --- Elevation statistics on hand-made grids ----------------------------------------------

TEST_CASE("geography 10.3: a square island") {
    const Cells& e = p100::SQUARE_ELEVATION;
    const auto c = worldgen::inspect::count_land_ocean(e, p100::SQ, p100::SQ);
    CHECK(c.cells == 25U);
    CHECK(c.land == 9U);
    CHECK(c.ocean == 16U);
    const auto m = worldgen::inspect::count_landmasses(e, p100::SQ, p100::SQ);
    CHECK(m.count == 1U);
    CHECK(m.largest_cells == 9U);
    CHECK(worldgen::inspect::coastline_m(e, p100::SQ, p100::SQ) == 192U);
    // coast_ratio 1.1284 for a 3 x 3 square: 192 / (2 sqrt(pi 2304)).
    const auto ratio = worldgen::inspect::coast_ratio(192, 9);
    REQUIRE(ratio.has_value());
    CHECK(ratio->to_decimal(4) == "1.1284");
    CHECK(ratio->raw() == ref_coast_ratio_raw(192, 9));
    CHECK(worldgen::inspect::circle_perimeter_raw(9) == ref_pq(9));
    CHECK(worldgen::inspect::land_hull_area2(e, p100::SQ, p100::SQ) == 18U);
    CHECK(ratio4(18, 18) == "1.0000");
    CHECK(section_of(p100::SQ, p100::SQ, e) == p100::SQUARE_GEOGRAPHY);
}

TEST_CASE("geography 10.3: a ring has solidity below 1") {
    // 7 x 7: a 5 x 5 block of land with an ocean cell in the middle.
    const Cells e = shape({".......", ".#####.", ".#####.", ".##.##.", ".#####.", ".#####.", "......."});
    // Land 24, ocean 25. Coastline: 20 outer edges + 4 around the hole =
    // 24 pairs, 384 m. coast_ratio: Pq for A = 256 x 24, value 384 x 2^64 /
    // Pq = 1.38197 -> 1.3820. Hull: the rows span x 1..5, so the hull is the
    // square [1, 6] x [1, 6], 2H = 50, solidity ratio4(48, 50) = 0.9600.
    // Five keys of 2026-11-15: every land cell is 10, so every cell step is
    // 0 (step_p999 0.0, rank 23 of 24). Slopes with e' = 0 on the ocean:
    // corners (1,1) (5,1) (1,5) (5,5) have (gx, gy) = (+-10, +-10), S2 200;
    // the other border cells and the four cells beside the hole (3,2) (2,3)
    // (4,3) (3,4) have one difference of 10, S2 100; (2,2) (4,2) (2,4) (4,4)
    // have all four neighbours land, S2 0, flat. All 24 cells are lowland:
    // ratio4(4, 24) = 0.1667; nothing steep; nothing >= 500 m. Edge runs:
    // land rows 1..5 span x 1..5 (the hole does not change the outermost
    // land), so every side has t = 1 on 5 lines: 80.
    CHECK(worldgen::inspect::coastline_m(e, 7, 7) == 384U);
    CHECK(worldgen::inspect::land_hull_area2(e, 7, 7) == 50U);
    CHECK(worldgen::inspect::coast_ratio(384, 24)->raw() == ref_coast_ratio_raw(384, 24));
    const std::string expected = "[geography]\n" + line("cells", "49") + line("land_cells", "24") +
                                 line("ocean_cells", "25") + line("land_fraction", "0.4898") +  // 0.489796
                                 line("landmasses", "1") + line("largest_landmass_share", "1.0000") +
                                 line("coastline_m", "384") + line("coast_ratio", "1.3820") +
                                 line("land_solidity", "0.9600") +
                                 line("elevation_m", "1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0") +
                                 line("depth_m", "1.0 1.0 1.0") +
                                 line("land_above_m", "50:0.0000 500:0.0000 1000:0.0000 1500:0.0000 2000:0.0000") +
                                 line("max_step_m", "0.0") +
                                 slope_lines("0.0", "35:0.0000 45:0.0000", "n/a", "0.1667", "80");
    CHECK(section_of(7, 7, e) == expected);
}

TEST_CASE("geography 10.3: two islands") {
    // 8 x 5: a 3 x 2 island and a single cell.
    const Cells e = shape({"........", ".###....", ".###....", "......#.", "........"}, 30);
    // Land 7, ocean 33. Landmasses 2, the largest 6: ratio4(6, 7) = 0.857142
    // -> 0.8571. Coastline 2 (3 + 2) + 4 = 14 pairs, 224 m; coast_ratio
    // 224 / (2 sqrt(pi 1792)) = 1.49269 -> 1.4927. Hull points: rows 1 and 2
    // give x 1 and 4, row 3 gives x 6 and 7; hull (1,1) (4,1) (7,3) (7,4)
    // (6,4) (1,3); shoelace: -3 + 5 + 7 + 4 + 14 - 2 = 25 = 2H; solidity
    // ratio4(14, 25) = 0.5600.
    // Five keys: all land 30, cell steps 0 (rank 6 of 7). Slopes: the six
    // cells of the 3 x 2 island have (gx, gy) (+-30, +-30) S2 1800 at its
    // corners and (0, +-30) S2 900 at (2,1), (2,2); the islet (6,3) has
    // ocean on all four sides, gx = gy = 0 - 0, S2 0: flat. Lowland 7, flat 1:
    // ratio4(1, 7) = 0.1429. Edge runs: west t = (-, 1, 1, 6, -): range 5,
    // run 3; east 7 - x_r = (-, 4, 4, 1, -): 3; north t on columns 1, 2, 3 =
    // 1 (columns 4, 5 empty), column 6 = 3: 3; south 4 - 2 = 2 on columns
    // 1..3: 3. 16 x 3 = 48.
    const auto m = worldgen::inspect::count_landmasses(e, 8, 5);
    CHECK(m.count == 2U);
    CHECK(m.largest_cells == 6U);
    CHECK(worldgen::inspect::land_hull_area2(e, 8, 5) == 25U);
    const std::string expected = "[geography]\n" + line("cells", "40") + line("land_cells", "7") +
                                 line("ocean_cells", "33") + line("land_fraction", "0.1750") +
                                 line("landmasses", "2") + line("largest_landmass_share", "0.8571") +
                                 line("coastline_m", "224") + line("coast_ratio", "1.4927") +
                                 line("land_solidity", "0.5600") +
                                 line("elevation_m", "3.0 3.0 3.0 3.0 3.0 3.0 3.0 3.0 3.0") +
                                 line("depth_m", "1.0 1.0 1.0") +
                                 line("land_above_m", "50:0.0000 500:0.0000 1000:0.0000 1500:0.0000 2000:0.0000") +
                                 line("max_step_m", "0.0") +
                                 slope_lines("0.0", "35:0.0000 45:0.0000", "n/a", "0.1429", "48");
    CHECK(section_of(8, 5, e) == expected);
}

TEST_CASE("geography 10.3: the hull of a concave L-shape") {
    const Cells e = shape({".....", ".#...", ".#...", ".###.", "....."});
    // Hull points per row (x_l, y), (x_l, y+1), (x_r+1, y), (x_r+1, y+1):
    // row 1: (1,1) (1,2) (2,1) (2,2); row 2: (1,2) (1,3) (2,2) (2,3);
    // row 3: (1,3) (1,4) (4,3) (4,4).
    const std::vector<HullPoint> want = {{1, 1}, {1, 2}, {2, 1}, {2, 2}, {1, 2}, {1, 3},
                                         {2, 2}, {2, 3}, {1, 3}, {1, 4}, {4, 3}, {4, 4}};
    CHECK(worldgen::inspect::land_hull_points(e, 5, 5) == want);
    // Hull (1,1) (2,1) (4,3) (4,4) (1,4): shoelace -1 + 2 + 4 + 12 - 3 = 14 =
    // 2H (area 7 against 5 land cells); solidity ratio4(10, 14) = 0.714285
    // -> 0.7143.
    CHECK(worldgen::inspect::land_hull_area2(e, 5, 5) == 14U);
    // Coastline: (1,1) 3 + (1,2) 2 + (1,3) 2 + (2,3) 2 + (3,3) 3 = 12, 192 m;
    // coast_ratio 192 / (2 sqrt(pi 1280)) = 1.51388 -> 1.5139.
    CHECK(worldgen::inspect::coastline_m(e, 5, 5) == 192U);
    const std::string s = section_of(5, 5, e);
    CHECK(has(s, "\nland_solidity = 0.7143\n"));
    CHECK(has(s, "\ncoast_ratio = 1.5139\n"));
    CHECK(has(s, "\nlandmasses = 1\n"));
}

TEST_CASE("geography 10.3: diagonal-only contact, 8-connected land and 4-connected coastline pairs") {
    // Land at (1, 1) e = 10 and (2, 2) e = 500 only.
    Cells e = shape({"....", ".#..", "..#.", "...."});
    e[2 * 4 + 2] = 500;
    // One landmass of 2 cells (8-connected through the diagonal).
    const auto m = worldgen::inspect::count_landmasses(e, 4, 4);
    CHECK(m.count == 1U);
    CHECK(m.largest_cells == 2U);
    // Each land cell has 4 ocean 4-neighbours; diagonal pairs do not count:
    // 8 pairs, 128 m.
    CHECK(worldgen::inspect::coastline_m(e, 4, 4) == 128U);
    // No 4-adjacent land pair: max step 0, although |500 - 10| = 490 on the
    // diagonal.
    CHECK(worldgen::inspect::max_land_step(e, 4, 4) == 0U);
    // Hull: (1,1) (2,1) (3,2) (3,3) (2,3) (1,2), 2H = 6; solidity ratio4(4, 6)
    // = 0.6667. coast_ratio 128 / (2 sqrt(pi 512)) = 1.59577 -> 1.5958.
    CHECK(worldgen::inspect::land_hull_area2(e, 4, 4) == 6U);
    // elevation_m over {10, 500}: ranks 0 0 0 0 1 1 1 1 1.
    // Five keys: no 4-adjacent land pair, so both cell steps are 0 (rank
    // min(1, floor(1998 / 1000)) = 1): 0.0. Slopes: each land cell's four
    // neighbours are ocean (e' 0), so gx = gy = 0 and S2 = 0 for both, even
    // the 50 m spike (central differences do not see the cell itself): both
    // lowland and flat, 1.0000. Edge runs: west t = (-, 1, 2, -): 2; east
    // (-, 2, 1, -): 2; north t(1) = 1, t(2) = 2: 2; south 2, 1: 2. 32.
    const std::string expected = "[geography]\n" + line("cells", "16") + line("land_cells", "2") +
                                 line("ocean_cells", "14") + line("land_fraction", "0.1250") +
                                 line("landmasses", "1") + line("largest_landmass_share", "1.0000") +
                                 line("coastline_m", "128") + line("coast_ratio", "1.5958") +
                                 line("land_solidity", "0.6667") +
                                 line("elevation_m", "1.0 1.0 1.0 1.0 50.0 50.0 50.0 50.0 50.0") +
                                 line("depth_m", "1.0 1.0 1.0") +
                                 line("land_above_m", "50:0.5000 500:0.0000 1000:0.0000 1500:0.0000 2000:0.0000") +
                                 line("max_step_m", "0.0") +
                                 slope_lines("0.0", "35:0.0000 45:0.0000", "n/a", "1.0000", "32");
    CHECK(section_of(4, 4, e) == expected);
    // The anti-diagonal and a 4-connected pair for comparison.
    const Cells anti = shape({"....", "..#.", ".#..", "...."});
    CHECK(worldgen::inspect::count_landmasses(anti, 4, 4).count == 1U);
    const Cells apart = shape({"#.#", "...", "#.#"});
    CHECK(worldgen::inspect::count_landmasses(apart, 3, 3).count == 4U);
    CHECK(worldgen::inspect::count_landmasses(apart, 3, 3).largest_cells == 1U);
}

TEST_CASE("geography 10.3: percentiles and land_above_m on a known multiset") {
    // Ten land values (with repeats) and five ocean depths, in scrambled
    // positions of a 5 x 3 grid.
    const Cells e = {5000, -1, 120, 500, 499, -300, 20000, 120, 15000, -2, 10000, -3000, 1, 30000, -1};
    // Land sorted: 1 120 120 499 500 5000 10000 15000 20000 30000 (n = 10).
    // Ranks for p 0 1 10 25 50 75 90 99 100: 0 0 1 2 5 7 9 9 9.
    const std::vector<std::uint32_t> ps = {0, 1, 10, 25, 50, 75, 90, 99, 100};
    const std::vector<std::int64_t> want = {1, 1, 120, 120, 5000, 15000, 30000, 30000, 30000};
    CHECK(worldgen::inspect::land_percentiles(e, ps) == want);
    // Ocean depths sorted: 1 1 2 300 3000 (n = 5); ranks for 0 50 100: 0 2 4.
    const std::vector<std::uint32_t> qs = {0, 50, 100};
    CHECK(worldgen::inspect::ocean_depth_percentiles(e, qs) == std::vector<std::int64_t>{1, 2, 3000});
    // land_above_m: >= 500: 500 5000 10000 15000 20000 30000 = 6; >= 5000: 5;
    // >= 10000: 4; >= 15000: 3; >= 20000: 2.
    CHECK(worldgen::inspect::land_at_or_above(e, 500) == 6U);
    CHECK(worldgen::inspect::land_at_or_above(e, 5000) == 5U);
    CHECK(worldgen::inspect::land_at_or_above(e, 10000) == 4U);
    CHECK(worldgen::inspect::land_at_or_above(e, 15000) == 3U);
    CHECK(worldgen::inspect::land_at_or_above(e, 20000) == 2U);
    CHECK(worldgen::inspect::land_at_or_above(e, 30001) == 0U);
    // Only land counts: a threshold at or below 0 counts the land cells.
    CHECK(worldgen::inspect::land_at_or_above(e, -5000) == 10U);
    const std::string s = section_of(5, 3, e);
    CHECK(has(s, "\nelevation_m = 0.1 0.1 12.0 12.0 500.0 1500.0 3000.0 3000.0 3000.0\n"));
    CHECK(has(s, "\ndepth_m = 0.1 0.2 300.0\n"));
    CHECK(has(s, "\nland_above_m = 50:0.6000 500:0.5000 1000:0.4000 1500:0.3000 2000:0.2000\n"));
    // Order of ps is kept.
    CHECK(worldgen::inspect::land_percentiles(e, std::vector<std::uint32_t>{100, 0, 50}) ==
          std::vector<std::int64_t>{30000, 1, 5000});
    // No land / no ocean: empty.
    CHECK(worldgen::inspect::land_percentiles(Cells{-1, -2}, ps).empty());
    CHECK(worldgen::inspect::ocean_depth_percentiles(Cells{1, 2}, qs).empty());
}

TEST_CASE("geography 10.3: max_step_m over 4-adjacent land pairs") {
    // Ocean neighbours do not count, however deep; diagonal pairs do not
    // count; the largest orthogonal land difference does.
    const Cells e = {
        100, 3100, -30000,  //
        50,  -1,   3000,    //
        1,   900,  2900,    //
    };
    // Pairs: (0,0)-(1,0) 3000; (0,0)-(0,1) 50; (0,1)-(0,2) 49; (0,2)-(1,2)
    // 899; (1,2)-(2,2) 2000; (2,1)-(2,2) 100. Largest 3000 dm = 300.0 m.
    CHECK(worldgen::inspect::max_land_step(e, 3, 3) == 3000U);
    CHECK(has(section_of(3, 3, e), "\nmax_step_m = 300.0\n"));
    // The widest i16 step: 32767 against 1.
    const Cells wide = {32767, 1, -32768};
    CHECK(worldgen::inspect::max_land_step(wide, 3, 1) == 32766U);
    const std::string s = section_of(3, 1, wide);
    CHECK(has(s, "\nmax_step_m = 3276.6\n"));
    CHECK(has(s, "\ndepth_m = 3276.8 3276.8 3276.8\n"));
    CHECK(has(s, "\nelevation_m = 0.1 0.1 0.1 0.1 3276.7 3276.7 3276.7 3276.7 3276.7\n"));
    CHECK(has(s, "\nland_above_m = 50:0.5000 500:0.5000 1000:0.5000 1500:0.5000 2000:0.5000\n"));
}

TEST_CASE("geography 10.3: coast_ratio as defined, and on large maps") {
    using worldgen::inspect::circle_perimeter_raw;
    using worldgen::inspect::coast_ratio;
    CHECK(!coast_ratio(0, 0).has_value());
    CHECK(!coast_ratio(64, 0).has_value());
    CHECK(coast_ratio(0, 5)->raw() == 0);
    CHECK(circle_perimeter_raw(0) == 0U);
    // Small cases against the U128 reference and the int64 form of the spec,
    // from_ratio(coastline 2^32, Pq).
    for (const std::uint64_t land : {1U, 2U, 7U, 9U, 30U, 100U, 1000U}) {
        for (const std::uint64_t coast : {16U, 64U, 192U, 1600U, 100000U}) {
            INFO("land ", land, " coast ", coast);
            const std::uint64_t pq = ref_pq(land);
            CHECK(circle_perimeter_raw(land) == pq);
            const auto r = coast_ratio(coast, land);
            REQUIRE(r.has_value());
            CHECK(r->raw() == ref_coast_ratio_raw(coast, land));
            CHECK(r->raw() ==
                  Fx::from_ratio(static_cast<std::int64_t>(coast) << 32, static_cast<std::int64_t>(pq)).raw());
        }
    }
    // A single cell: 4 edges, 64 m against 2 sqrt(256 pi) = 56.72 m: 1.1284.
    CHECK(coast_ratio(64, 1)->to_decimal(4) == "1.1284");
    // A 2048 map (appendix C item 10, 128-bit): land 2^21 cells and a
    // coastline beyond 2^31 m, where the int64 form would overflow.
    const std::uint64_t land = std::uint64_t{1} << 21;
    const std::uint64_t coast = (std::uint64_t{1} << 31) + 12345U;
    CHECK(circle_perimeter_raw(land) == ref_pq(land));
    CHECK(coast_ratio(coast, land)->raw() == ref_coast_ratio_raw(coast, land));
    CHECK(coast_ratio(16U * 2U * 2048U * 2048U, std::uint64_t{2048} * 2048)->raw() ==
          ref_coast_ratio_raw(16U * 2U * 2048U * 2048U, std::uint64_t{2048} * 2048));
}

TEST_CASE("geography 10.3: convex_hull_area2") {
    using worldgen::inspect::convex_hull_area2;
    CHECK(convex_hull_area2(std::vector<HullPoint>{}) == 0U);
    CHECK(convex_hull_area2(std::vector<HullPoint>{{1, 1}}) == 0U);
    CHECK(convex_hull_area2(std::vector<HullPoint>{{1, 1}, {5, 5}}) == 0U);
    CHECK(convex_hull_area2(std::vector<HullPoint>{{0, 0}, {1, 1}, {2, 2}, {3, 3}}) == 0U);  // collinear
    CHECK(convex_hull_area2(std::vector<HullPoint>{{0, 0}, {4, 0}, {0, 3}}) == 12U);
    // Duplicates, interior points and points on edges do not change it.
    CHECK(convex_hull_area2(std::vector<HullPoint>{
              {0, 0}, {2, 0}, {4, 0}, {4, 4}, {0, 4}, {2, 2}, {0, 0}, {4, 4}, {0, 2}, {1, 3}}) == 32U);
    // Order of input does not matter.
    CHECK(convex_hull_area2(std::vector<HullPoint>{{4, 4}, {0, 0}, {0, 4}, {4, 0}}) == 32U);
    // Negative coordinates and the documented range [-2^29, 2^29].
    const std::int64_t m = std::int64_t{1} << 29;
    CHECK(convex_hull_area2(std::vector<HullPoint>{{-m, -m}, {m, -m}, {m, m}, {-m, m}}) == std::uint64_t{1} << 61);
    CHECK_THROWS_AS((void)convex_hull_area2(std::vector<HullPoint>{{0, 0}, {m + 1, 0}, {0, 1}}), FatalError);
    CHECK_THROWS_AS((void)convex_hull_area2(std::vector<HullPoint>{{0, 0}, {1, 0}, {0, -m - 1}}), FatalError);
}

// --- Empty-set cases (appendix C item 6) ---------------------------------------------------

TEST_CASE("geography appendix C 6: no land") {
    const std::string expected = "[geography]\n" + line("cells", "9") + line("land_cells", "0") +
                                 line("ocean_cells", "9") + line("land_fraction", "0.0000") +
                                 line("landmasses", "0") + line("largest_landmass_share", "n/a") +
                                 line("coastline_m", "0") + line("coast_ratio", "n/a") +
                                 line("land_solidity", "n/a") + line("elevation_m", "n/a") +
                                 line("depth_m", "0.5 0.5 0.5") +
                                 line("land_above_m", "50:n/a 500:n/a 1000:n/a 1500:n/a 2000:n/a") +
                                 line("max_step_m", "0.0") +
                                 // No land: step_p999_m is n/a (10.3 "Cell step"); slope_over
                                 // is "35:" ratio4(0, 0) and "45:" ratio4(0, 0), each n/a (C 6);
                                 // no cell >= 500 m; lowland denominator 0; no land, run 0.
                                 slope_lines("n/a", "35:n/a 45:n/a", "n/a", "n/a", "0");
    CHECK(section_of(3, 3, Cells(9, -5)) == expected);
    const auto m = worldgen::inspect::count_landmasses(Cells(9, -5), 3, 3);
    CHECK(m.count == 0U);
    CHECK(m.largest_cells == 0U);
    CHECK(worldgen::inspect::land_hull_area2(Cells(9, -5), 3, 3) == 0U);
    CHECK(worldgen::inspect::land_hull_points(Cells(9, -5), 3, 3).empty());
}

TEST_CASE("geography appendix C 6: no ocean") {
    // 2 x 2 land, e = 10: coast_ratio 0.0000 (land and no ocean).
    const std::string expected = "[geography]\n" + line("cells", "4") + line("land_cells", "4") +
                                 line("ocean_cells", "0") + line("land_fraction", "1.0000") +
                                 line("landmasses", "1") + line("largest_landmass_share", "1.0000") +
                                 line("coastline_m", "0") + line("coast_ratio", "0.0000") +
                                 line("land_solidity", "1.0000") +
                                 line("elevation_m", "1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0") +
                                 line("depth_m", "n/a") +
                                 line("land_above_m", "50:0.0000 500:0.0000 1000:0.0000 1500:0.0000 2000:0.0000") +
                                 line("max_step_m", "0.0") +
                                 // Steps 0. Slopes: 2 x 2, xa = 0, xb = 1 for every cell, gx =
                                 // (10 - 10) x 2 = 0, gy likewise: S2 0, 4 of 4 lowland cells
                                 // flat. Each side has t = 0 on 2 lines: 32.
                                 slope_lines("0.0", "35:0.0000 45:0.0000", "n/a", "1.0000", "32");
    CHECK(section_of(2, 2, Cells(4, 10)) == expected);
}

TEST_CASE("geography appendix C 6: a single cell") {
    SUBCASE("land") {
        // Hull of the cell's corners is the unit square, 2H = 2: 1.0000.
        const std::string expected = "[geography]\n" + line("cells", "1") + line("land_cells", "1") +
                                     line("ocean_cells", "0") + line("land_fraction", "1.0000") +
                                     line("landmasses", "1") + line("largest_landmass_share", "1.0000") +
                                     line("coastline_m", "0") + line("coast_ratio", "0.0000") +
                                     line("land_solidity", "1.0000") +
                                     line("elevation_m", "0.7 0.7 0.7 0.7 0.7 0.7 0.7 0.7 0.7") +
                                     line("depth_m", "n/a") +
                                     line("land_above_m", "50:0.0000 500:0.0000 1000:0.0000 1500:0.0000 2000:0.0000") +
                                     line("max_step_m", "0.0") +
                                     // n_L 1: step 0. xa = xb, ya = yb: gx = gy = 0, S2 0, flat
                                     // lowland 1 of 1. Each side t = (0): run 1, 16 m.
                                     slope_lines("0.0", "35:0.0000 45:0.0000", "n/a", "1.0000", "16");
        CHECK(section_of(1, 1, Cells{7}) == expected);
        CHECK(worldgen::inspect::land_hull_area2(Cells{7}, 1, 1) == 2U);
    }
    SUBCASE("ocean") {
        const std::string expected = "[geography]\n" + line("cells", "1") + line("land_cells", "0") +
                                     line("ocean_cells", "1") + line("land_fraction", "0.0000") +
                                     line("landmasses", "0") + line("largest_landmass_share", "n/a") +
                                     line("coastline_m", "0") + line("coast_ratio", "n/a") +
                                     line("land_solidity", "n/a") + line("elevation_m", "n/a") +
                                     line("depth_m", "0.7 0.7 0.7") +
                                     line("land_above_m", "50:n/a 500:n/a 1000:n/a 1500:n/a 2000:n/a") +
                                     line("max_step_m", "0.0") +
                                     slope_lines("n/a", "35:n/a 45:n/a", "n/a", "n/a", "0");
        CHECK(section_of(1, 1, Cells{-7}) == expected);
    }
    SUBCASE("elevation 0: neither land nor ocean (appendix C item 3)") {
        const std::string expected = "[geography]\n" + line("cells", "1") + line("land_cells", "0") +
                                     line("ocean_cells", "0") + line("land_fraction", "0.0000") +
                                     line("landmasses", "0") + line("largest_landmass_share", "n/a") +
                                     line("coastline_m", "0") + line("coast_ratio", "n/a") +
                                     line("land_solidity", "n/a") + line("elevation_m", "n/a") +
                                     line("depth_m", "n/a") +
                                     line("land_above_m", "50:n/a 500:n/a 1000:n/a 1500:n/a 2000:n/a") +
                                     line("max_step_m", "0.0") +
                                     // Elevation 0 is not land: as with no land.
                                     slope_lines("n/a", "35:n/a 45:n/a", "n/a", "n/a", "0");
        CHECK(section_of(1, 1, Cells{0}) == expected);
    }
}

TEST_CASE("geography appendix C 3: elevation 0 separates land from ocean") {
    // 3 x 1: land 5, zero, ocean -5. The land and ocean cells are not
    // 4-adjacent, so there is no coastline pair; 0 is in no count.
    const Cells e = {5, 0, -5};
    const auto c = worldgen::inspect::count_land_ocean(e, 3, 1);
    CHECK(c.cells == 3U);
    CHECK(c.land == 1U);
    CHECK(c.ocean == 1U);
    CHECK(worldgen::inspect::coastline_m(e, 3, 1) == 0U);
    // A zero between two land cells splits them into two landmasses (and is
    // no max_step pair).
    const Cells split = {5, 0, 900};
    CHECK(worldgen::inspect::count_landmasses(split, 3, 1).count == 2U);
    CHECK(worldgen::inspect::max_land_step(split, 3, 1) == 0U);
    const std::string s = section_of(3, 1, e);
    CHECK(has(s, "\nland_cells = 1\nocean_cells = 1\nland_fraction = 0.3333\n"));
    CHECK(has(s, "\ncoastline_m = 0\ncoast_ratio = 0.0000\n"));
}

// --- Hydrology statistics ------------------------------------------------------------------

TEST_CASE("geography 10.3: flow path lengths on the P1-02 worked example") {
    const Hydro hy = worked();
    const auto fp = worldgen::inspect::flow_path_lengths(hy.e, hy.flow_dir, hy.w, hy.h);
    REQUIRE(!fp.bad_cell.has_value());
    REQUIRE(fp.d.size() == hy.e.size());
    CHECK(fp.d == ref_paths(hy.e, hy.flow_dir, hy.w, hy.h));
    CHECK(worldgen::inspect::ORTHOGONAL_STEP_RAW == ORTHO);
    CHECK(worldgen::inspect::DIAGONAL_STEP_RAW == DIAG);
    CHECK(worldgen::inspect::SQRT2_RAW == SQRT2_RAW);
    // Hand values: (4,5) S to the ocean: 16 m. (3,4) SE to (4,5): 16 sqrt2 +
    // 16. The lake outlet (2,3) SE to (3,4): 2 x 16 sqrt2 + 16. Ocean 0.
    CHECK(fp.d[5 * 8 + 4] == ORTHO);
    CHECK(fp.d[4 * 8 + 3] == 165903492736);
    CHECK(fp.d[3 * 8 + 2] == 263087508736);
    CHECK(fp.d[0] == 0);
    CHECK(fp.d[6 * 8 + 7] == 0);
    // (3,3) W to the outlet (2,3), then on: 16 + 2 x 16 sqrt2 + 16.
    CHECK(fp.d[3 * 8 + 3] == 2 * ORTHO + 2 * DIAG);
}

TEST_CASE("geography 10.3, appendix C 8: hydrology on the P1-02 worked example") {
    const Hydro hy = worked();
    const auto r = hy.stats();
    REQUIRE(r.stats.has_value());
    CHECK(!r.bad_cell.has_value());
    const auto& s = *r.stats;
    CHECK(s.lakes == 1U);
    CHECK(s.lake_cells == 4U);
    CHECK(s.largest_lake_cells == 4U);
    CHECK(s.river_cells == 2U);
    CHECK(s.river_sources == 0U);
    CHECK(s.river_mouths == 1U);
    CHECK(s.major_rivers == 0U);
    CHECK(s.longest_river_raw == 165903492736);  // D(3, 4); see p1_00_geography_reference.hpp
    CHECK(Fx::from_raw(s.longest_river_raw).round_int() == 39);
    CHECK(s.largest_basin_cells == 10U);
    // The whole section, byte for byte.
    const SectionResult sec = section_of(hy);
    INFO("problem: ", sec.problem);
    REQUIRE(sec.text.has_value());
    CHECK(*sec.text == p100::worked_geography());
}

namespace {

// 5 x 3: ocean around a row of land (1,1) e 10, (2,1) e 20, (3,1) e 30, all
// draining west: (3,1) -> (2,1) -> (1,1) -> ocean (0,1). flow_accum 3, 2, 1.
// (1,1) and (2,1) are river cells (class 1), (3,1) dry land.
Hydro single_river() {
    Hydro hy;
    hy.w = 5;
    hy.h = 3;
    hy.e = {-5, -5, -5, -5, -5, -5, 10, 20, 30, -5, -5, -5, -5, -5, -5};
    hy.water = {3, 3, 3, 3, 3, 3, 1, 1, 0, 3, 3, 3, 3, 3, 3};
    hy.flow_dir = {8, 8, 8, 8, 8, 8, 4, 4, 4, 8, 8, 8, 8, 8, 8};
    hy.flow_accum = {0, 0, 0, 0, 0, 0, 3, 2, 1, 0, 0, 0, 0, 0, 0};
    hy.river = {0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0};
    hy.lake = std::vector<std::uint32_t>(15, 0);
    return hy;
}

}  // namespace

TEST_CASE("geography 10.3: a single river") {
    SUBCASE("source below a dry cell") {
        const Hydro hy = single_river();
        const auto r = hy.stats();
        REQUIRE(r.stats.has_value());
        const auto& s = *r.stats;
        CHECK(s.lakes == 0U);
        CHECK(s.lake_cells == 0U);
        CHECK(s.largest_lake_cells == 0U);
        CHECK(s.river_cells == 2U);
        // (2,1) is fed only by (3,1), which is dry: a source. (1,1) is fed by
        // the river cell (2,1): not a source.
        CHECK(s.river_sources == 1U);
        CHECK(s.river_mouths == 1U);  // (1,1) -> ocean (0,1)
        CHECK(s.major_rivers == 0U);  // class 1
        CHECK(s.longest_river_raw == 2 * ORTHO);  // D(2,1) = 32 m
        CHECK(s.largest_basin_cells == 3U);       // (1,1)
        const SectionResult sec = section_of(hy);
        REQUIRE(sec.text.has_value());
        const std::string want = line("lakes", "0") + line("lake_cells", "0") + line("lake_share_of_land", "0.0000") +
                                 line("largest_lake_cells", "0") + line("river_cells", "2") +
                                 line("river_share_of_land", "0.6667") + line("river_sources", "1") +
                                 line("river_mouths", "1") + line("major_rivers", "0") +
                                 line("longest_river_m", "32") + line("largest_basin_share", "1.0000");
        // Five keys: steps 10 for all three land cells (rank 2 of 3): 1.0.
        // Slopes on the 5 x 3 grid (e' 0 on the ocean): (1,1) gx 20 - 0,
        // (2,1) 30 - 10, (3,1) 0 - 20, gy 0 - 0: S2 400 each, not flat,
        // lowland 3: 0.0000. Edge runs: west t = (-, 1, -): 1; east (-, 1,
        // -): 1; north t = 1 on columns 1..3: 3; south 2 - 1 = 1 on them: 3.
        // 48.
        CHECK(has(*sec.text, "\nmax_step_m = 1.0\n" +
                                 slope_lines("1.0", "35:0.0000 45:0.0000", "n/a", "0.0000", "48") + want));
        CHECK(sec.text->size() >= want.size());
        CHECK(sec.text->substr(sec.text->size() - want.size()) == want);
    }
    SUBCASE("a major river: class 4 at the mouth") {
        Hydro hy = single_river();
        hy.river[6] = 4;
        CHECK(hy.stats().stats->major_rivers == 1U);
        hy.river[6] = 3;
        CHECK(hy.stats().stats->major_rivers == 0U);
        hy.river[6] = 15;
        CHECK(hy.stats().stats->major_rivers == 1U);
        // Only the mouth's class counts.
        hy.river[6] = 1;
        hy.river[7] = 9;
        CHECK(hy.stats().stats->major_rivers == 0U);
    }
    SUBCASE("the river reaches the dry cell's position") {
        Hydro hy = single_river();
        hy.water[8] = 1;
        hy.river[8] = 1;
        const auto s = *hy.stats().stats;
        CHECK(s.river_cells == 3U);
        CHECK(s.river_sources == 1U);  // now (3,1)
        CHECK(s.longest_river_raw == 3 * ORTHO);
    }
    SUBCASE("a diagonal mouth rounds 16 sqrt2 = 22.63 m to 23") {
        // 3 x 3: one river cell in the middle draining NE to the ocean corner.
        Hydro hy;
        hy.w = 3;
        hy.h = 3;
        hy.e = {-5, -5, -5, -5, 10, -5, -5, -5, -5};
        hy.water = {3, 3, 3, 3, 1, 3, 3, 3, 3};
        hy.flow_dir = {8, 8, 8, 8, 7, 8, 8, 8, 8};
        hy.flow_accum = {0, 0, 0, 0, 1, 0, 0, 0, 0};
        hy.river = {0, 0, 0, 0, 5, 0, 0, 0, 0};
        hy.lake = std::vector<std::uint32_t>(9, 0);
        const auto s = *hy.stats().stats;
        CHECK(s.longest_river_raw == DIAG);
        CHECK(s.river_sources == 1U);
        CHECK(s.river_mouths == 1U);
        CHECK(s.major_rivers == 1U);
        const SectionResult sec = section_of(hy);
        REQUIRE(sec.text.has_value());
        CHECK(has(*sec.text, "\nlongest_river_m = 23\n"));
    }
}

TEST_CASE("geography appendix C 8: a river fed by a lake") {
    // 6 x 3: (4,1) e 40 and (3,1) e 30 form lake 1, draining W through the
    // outlet (3,1) into river cells (2,1) and (1,1), then the ocean.
    Hydro hy;
    hy.w = 6;
    hy.h = 3;
    hy.e = {-5, -5, -5, -5, -5, -5, -5, 10, 20, 30, 40, -5, -5, -5, -5, -5, -5, -5};
    hy.water = {3, 3, 3, 3, 3, 3, 3, 1, 1, 2, 2, 3, 3, 3, 3, 3, 3, 3};
    hy.flow_dir = {8, 8, 8, 8, 8, 8, 8, 4, 4, 4, 4, 8, 8, 8, 8, 8, 8, 8};
    hy.flow_accum = {0, 0, 0, 0, 0, 0, 0, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0};
    hy.river = {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    hy.lake = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0};
    const auto s = *hy.stats().stats;
    CHECK(s.lakes == 1U);
    CHECK(s.lake_cells == 2U);
    CHECK(s.largest_lake_cells == 2U);
    CHECK(s.river_cells == 2U);
    // (2,1) is fed by the lake cell (3,1): no source at all.
    CHECK(s.river_sources == 0U);
    CHECK(s.river_mouths == 1U);
    // Measured from the river cell at the outlet, (2,1): 32 m, not from the
    // lake (D(4,1) = 64 m).
    CHECK(s.longest_river_raw == 2 * ORTHO);
    CHECK(s.largest_basin_cells == 4U);
    const SectionResult sec = section_of(hy);
    REQUIRE(sec.text.has_value());
    CHECK(has(*sec.text, "\nlakes = 1\nlake_cells = 2\nlake_share_of_land = 0.5000\nlargest_lake_cells = 2\n"
                         "river_cells = 2\nriver_share_of_land = 0.5000\nriver_sources = 0\nriver_mouths = 1\n"
                         "major_rivers = 0\nlongest_river_m = 32\nlargest_basin_share = 1.0000\n"));
}

TEST_CASE("geography 10.3: lakes is the largest lake value, largest_lake_cells the biggest lake") {
    // 6 x 3 land row, lake ids 3, 3, 1 (and no lake 2): lakes = 3.
    Hydro hy;
    hy.w = 6;
    hy.h = 3;
    hy.e = {-5, -5, -5, -5, -5, -5, -5, 10, 20, 30, 40, -5, -5, -5, -5, -5, -5, -5};
    hy.water = {3, 3, 3, 3, 3, 3, 3, 0, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3};
    hy.flow_dir = {8, 8, 8, 8, 8, 8, 8, 4, 4, 4, 4, 8, 8, 8, 8, 8, 8, 8};
    hy.flow_accum = {0, 0, 0, 0, 0, 0, 0, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0};
    hy.river = std::vector<std::uint8_t>(18, 0);
    hy.lake = {0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 3, 0, 0, 0, 0, 0, 0, 0};
    const auto s = *hy.stats().stats;
    CHECK(s.lakes == 3U);
    CHECK(s.lake_cells == 3U);
    CHECK(s.largest_lake_cells == 2U);
    CHECK(s.river_cells == 0U);
    CHECK(s.river_sources == 0U);
    CHECK(s.river_mouths == 0U);
    CHECK(s.longest_river_raw == 0);  // no river cells: maximum over nothing
    CHECK(s.largest_basin_cells == 4U);
}

TEST_CASE("geography 10.3: two rivers joining") {
    // 6 x 4, land x 1..4, y 1..2:
    //   (1,1) SE -> (2,2); (3,1) SW -> (2,2); (2,1) S -> (2,2); (2,2) S -> ocean;
    //   (4,1) E, (1,2) W, (3,2) S, (4,2) S -> ocean.
    // Rivers: (1,1) and (3,1) (class 1), joining at (2,2) (class 2); (2,1)
    // is dry land draining into the junction.
    Hydro hy;
    hy.w = 6;
    hy.h = 4;
    const std::int16_t o = -5;
    hy.e = {o, o, o, o, o, o, o, 10, 11, 12, 13, o, o, 14, 9, 15, 16, o, o, o, o, o, o, o};
    hy.water = {3, 3, 3, 3, 3, 3, 3, 1, 0, 1, 0, 3, 3, 0, 1, 0, 0, 3, 3, 3, 3, 3, 3, 3};
    hy.flow_dir = {8, 8, 8, 8, 8, 8, 8, 1, 2, 3, 0, 8, 8, 4, 2, 2, 2, 8, 8, 8, 8, 8, 8, 8};
    hy.flow_accum = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 4, 1, 1, 0, 0, 0, 0, 0, 0, 0};
    hy.river = {0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    hy.lake = std::vector<std::uint32_t>(24, 0);
    const auto s = *hy.stats().stats;
    CHECK(s.river_cells == 3U);
    CHECK(s.river_sources == 2U);  // (1,1) and (3,1); (2,2) is fed by rivers
    CHECK(s.river_mouths == 1U);   // (2,2)
    CHECK(s.major_rivers == 0U);
    // D(1,1) = D(3,1) = 16 sqrt2 + 16 = 38.63 m -> 39.
    CHECK(s.longest_river_raw == DIAG + ORTHO);
    CHECK(s.largest_basin_cells == 4U);  // (2,2): itself, (1,1), (2,1), (3,1)
    const SectionResult sec = section_of(hy);
    REQUIRE(sec.text.has_value());
    CHECK(has(*sec.text, "\nriver_cells = 3\nriver_share_of_land = 0.3750\nriver_sources = 2\nriver_mouths = 1\n"
                         "major_rivers = 0\nlongest_river_m = 39\nlargest_basin_share = 0.5000\n"));
}

// --- Omission and failures (appendix C item 9) ------------------------------------------------

TEST_CASE("geography appendix C 9: hydrology lines are omitted when any of the five layers is absent") {
    const Hydro hy = worked();
    const LayerGrid e = p100::grid(hy.w, hy.h, hy.e);
    const LayerGrid water = p100::grid(hy.w, hy.h, hy.water);
    const LayerGrid dir = p100::grid(hy.w, hy.h, hy.flow_dir);
    const LayerGrid acc = p100::grid(hy.w, hy.h, hy.flow_accum);
    const LayerGrid river = p100::grid(hy.w, hy.h, hy.river);
    const LayerGrid lake = p100::grid(hy.w, hy.h, hy.lake);
    // A bad hydrology layer that would fail if it were checked.
    const LayerGrid bad = p100::grid<std::uint16_t>(2, 1, {1, 2});
    for (int missing = 0; missing < 5; ++missing) {
        INFO("missing layer ", missing);
        GeographyLayers layers{&e, &water, &dir, &acc, &river, &lake};
        const LayerGrid** slots[5] = {&layers.water, &layers.flow_dir, &layers.flow_accum, &layers.river,
                                      &layers.lake};
        *slots[missing] = nullptr;
        CHECK(!layers.has_hydrology());
        std::string problem;
        const auto text = worldgen::inspect::geography_section(layers, &problem);
        REQUIRE(text.has_value());
        CHECK(*text == p100::WORKED_GEOGRAPHY_ELEVATION_LINES);
        // The others are not checked (appendix C item 9).
        *slots[(missing + 1) % 5] = &bad;
        CHECK(!worldgen::inspect::geography_layers_problem(layers).has_value());
        CHECK(worldgen::inspect::geography_section(layers, &problem) ==
              std::optional<std::string>(std::string(p100::WORKED_GEOGRAPHY_ELEVATION_LINES)));
    }
}

TEST_CASE("geography appendix C 9: a flow cycle, a step off the map, code 8 on land") {
    auto fault = [](const Hydro& hy, std::uint64_t cell) {
        const auto fp = worldgen::inspect::flow_path_lengths(hy.e, hy.flow_dir, hy.w, hy.h);
        CHECK(fp.d.empty());
        CHECK(fp.bad_cell == std::optional<std::uint64_t>(cell));
        const auto r = hy.stats();
        CHECK(!r.stats.has_value());
        CHECK(r.bad_cell == std::optional<std::uint64_t>(cell));
        const SectionResult sec = section_of(hy);
        CHECK(!sec.text.has_value());
        CHECK(has(sec.problem, "flow_dir"));
        return sec.problem;
    };
    SUBCASE("a cycle; the lowest bad cell feeds it") {
        // (1,1) E -> (2,1) E -> (3,1) W -> (2,1): cycle {(2,1), (3,1)}. The
        // lowest cell that does not reach the ocean is (1,1), index 6.
        Hydro hy = single_river();
        hy.flow_dir[6] = 0;
        hy.flow_dir[7] = 0;
        hy.flow_dir[8] = 4;
        const std::string p = fault(hy, 6);
        CHECK(has(p, "(1, 1)"));
    }
    SUBCASE("a two-cell cycle in the middle of the map, with good cells before it") {
        Hydro hy = single_river();
        hy.flow_dir[7] = 0;  // (2,1) E -> (3,1), (3,1) W -> (2,1); (1,1) is fine
        fault(hy, 7);
    }
    SUBCASE("a step off the map") {
        // 3 x 1: (0,0) land with W, off the map; (1,0) E to the ocean (2,0).
        Hydro hy;
        hy.w = 3;
        hy.h = 1;
        hy.e = {5, 6, -5};
        hy.water = {0, 0, 3};
        hy.flow_dir = {4, 0, 8};
        hy.flow_accum = {1, 1, 0};
        hy.river = {0, 0, 0};
        hy.lake = {0, 0, 0};
        const std::string p = fault(hy, 0);
        CHECK(has(p, "(0, 0)"));
        // Also off the south, north and east edges.
        for (const std::uint8_t k : {std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{6}, std::uint8_t{7},
                                     std::uint8_t{5}, std::uint8_t{3}}) {
            INFO("code ", int{k});
            hy.flow_dir[0] = k;
            fault(hy, 0);
        }
        // (0,0) E to the ocean (1,0) is fine; (2,0) E leaves the map.
        hy.e = {5, -5, 6};
        hy.flow_dir = {0, 8, 0};
        fault(hy, 2);
    }
    SUBCASE("code 8 or above 8 on a non-ocean cell") {
        Hydro hy = single_river();
        hy.flow_dir[8] = 8;
        fault(hy, 8);
        hy.flow_dir[8] = 9;
        fault(hy, 8);
        hy.flow_dir[8] = 255;
        fault(hy, 8);
        // A cell with elevation 0 is not ocean (appendix C items 3, 9).
        Hydro z = single_river();
        z.e[8] = 0;
        z.flow_dir[8] = 8;
        fault(z, 8);
    }
    SUBCASE("ocean cells may hold any code") {
        Hydro hy = single_river();
        hy.flow_dir[0] = 4;  // would leave the map
        hy.flow_dir[5] = 0;  // points at land
        hy.flow_dir[10] = 200;
        const auto r = hy.stats();
        REQUIRE(r.stats.has_value());
        CHECK(r.stats->river_mouths == 1U);
    }
}

TEST_CASE("geography appendix C 9: layers of the wrong type or size") {
    using worldgen::inspect::geography_layers_problem;
    const Hydro hy = worked();
    const LayerGrid e = p100::grid(hy.w, hy.h, hy.e);
    const LayerGrid water = p100::grid(hy.w, hy.h, hy.water);
    const LayerGrid dir = p100::grid(hy.w, hy.h, hy.flow_dir);
    const LayerGrid acc = p100::grid(hy.w, hy.h, hy.flow_accum);
    const LayerGrid river = p100::grid(hy.w, hy.h, hy.river);
    const LayerGrid lake = p100::grid(hy.w, hy.h, hy.lake);
    auto names = [](const std::optional<std::string>& p, const char* layer) {
        return p.has_value() && p->find(layer) != std::string::npos;
    };
    CHECK(!geography_layers_problem(GeographyLayers{&e, &water, &dir, &acc, &river, &lake}).has_value());
    // elevation absent, or not i16.
    CHECK(geography_layers_problem(GeographyLayers{}).has_value());
    const LayerGrid e_u16 = worldgen::db::make_layer_grid(worldgen::db::ElementType::u16, hy.w, hy.h);
    const LayerGrid e_i32 = worldgen::db::make_layer_grid(worldgen::db::ElementType::i32, hy.w, hy.h);
    CHECK(names(geography_layers_problem(GeographyLayers{&e_u16}), "elevation"));
    CHECK(names(geography_layers_problem(GeographyLayers{&e_i32, &water, &dir, &acc, &river, &lake}), "elevation"));
    // Each hydrology layer of the wrong type.
    const LayerGrid u16 = worldgen::db::make_layer_grid(worldgen::db::ElementType::u16, hy.w, hy.h);
    const LayerGrid u8 = worldgen::db::make_layer_grid(worldgen::db::ElementType::u8, hy.w, hy.h);
    CHECK(names(geography_layers_problem(GeographyLayers{&e, &u16, &dir, &acc, &river, &lake}), "water"));
    CHECK(names(geography_layers_problem(GeographyLayers{&e, &water, &u16, &acc, &river, &lake}), "flow_dir"));
    CHECK(names(geography_layers_problem(GeographyLayers{&e, &water, &dir, &u16, &river, &lake}), "flow_accum"));
    CHECK(names(geography_layers_problem(GeographyLayers{&e, &water, &dir, &acc, &u16, &lake}), "river"));
    CHECK(names(geography_layers_problem(GeographyLayers{&e, &water, &dir, &acc, &river, &u8}), "lake"));
    // Each of the wrong size (one row short, or transposed).
    const LayerGrid water_short = worldgen::db::make_layer_grid(worldgen::db::ElementType::u8, hy.w, hy.h - 1);
    const LayerGrid acc_t = worldgen::db::make_layer_grid(worldgen::db::ElementType::u32, hy.h, hy.w);
    const LayerGrid lake_t = worldgen::db::make_layer_grid(worldgen::db::ElementType::u32, hy.h, hy.w);
    CHECK(names(geography_layers_problem(GeographyLayers{&e, &water_short, &dir, &acc, &river, &lake}), "water"));
    CHECK(names(geography_layers_problem(GeographyLayers{&e, &water, &dir, &acc_t, &river, &lake}), "flow_accum"));
    CHECK(names(geography_layers_problem(GeographyLayers{&e, &water, &dir, &acc, &river, &lake_t}), "lake"));
    // geography_section reports the same problem.
    std::string problem;
    CHECK(!worldgen::inspect::geography_section(GeographyLayers{&e, &water, &dir, &acc, &u16, &lake}, &problem)
               .has_value());
    CHECK(has(problem, "river"));
    // hydrology_stats on wrong sizes is a contract violation.
    Hydro short_hy = worked();
    short_hy.lake.pop_back();
    CHECK_THROWS_AS((void)short_hy.stats(), FatalError);
}

// --- The section through build_report on fixture worlds -------------------------------------

namespace {

std::string report_of(const std::string& path) {
    auto w = worldgen::db::WorldReader::open(path);
    REQUIRE(w.ok());
    auto r = worldgen::inspect::build_report(w.value());
    INFO("report: ", r.ok() ? std::string() : r.error().message);
    REQUIRE(r.ok());
    return r.value();
}

worldgen::db::Result<std::string> try_report(const std::string& path) {
    auto w = worldgen::db::WorldReader::open(path);
    REQUIRE(w.ok());
    return worldgen::inspect::build_report(w.value());
}

std::string meta_lines(std::uint64_t seed, std::string_view stages) {
    std::string s;
    for (const auto& [k, v] : p004::meta_rows(seed, {}, stages)) {
        s += v.empty() ? k + " =\n" : k + " = " + v + "\n";
    }
    return s;
}

}  // namespace

TEST_CASE("report v2: the whole report of a world with elevation only") {
    dbtest::TempDir dir("p100-report-square");
    const std::string out = dir.file("w.db");
    const LayerGrid g = p100::grid(p100::SQ, p100::SQ, p100::SQUARE_ELEVATION);
    std::vector<FixtureLayer> layers;
    layers.push_back({"elevation", g, Codec::delta_varint});
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 5, layers).ok());
    const std::string expected = "# worldgen report v2\n[meta]\n" + meta_lines(5, "test_fixture") +
                                 "[tables]\nage = 0\nbeast = 0\nbeast_population = 0\nbiome = 0\nbook = 0\ncivilization = 0\nclimate = 0\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\nfigure = 0\ngod = 0\nlayer = 1\nmeta = 17\nmonster = 0\nmonster_kind = 0\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nbook = 0\nrace_relation = 0\nregion = 0\nregion_biome = 0\nbook = 0\nregion_border = 0\nregion_control = 0\nregion_name = 0\nrelation = 0\nsettlement = 0\nsettlement_census = 0\nsettlement_race = 0\nsite = 0\n[layers]\n"
                                 "elevation i16 5x5 codec=1 bytes=" +
                                 std::to_string(worldgen::db::encode_layer_blob(g).size()) +
                                 " min=-300 max=20000\n" + p100::with_empty_pass_lines(p100::SQUARE_GEOGRAPHY);
    CHECK(report_of(out) == expected);
}

TEST_CASE("report v2: the whole report of the P1-02 worked example") {
    dbtest::TempDir dir("p100-report-worked");
    const std::string out = dir.file("w.db");
    const auto layers = p100::worked_example_layers();
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 7, layers).ok());
    auto layer_line = [](const FixtureLayer& l, const char* fmt, const char* lo, const char* hi) {
        return l.name + " " + fmt + " 8x7 codec=1 bytes=" + std::to_string(worldgen::db::encode_layer_blob(l.grid).size()) +
               " min=" + lo + " max=" + hi + "\n";
    };
    const std::string expected =
        "# worldgen report v2\n[meta]\n" + meta_lines(7, "test_fixture") + "[tables]\nage = 0\nbeast = 0\nbeast_population = 0\nbiome = 0\nbook = 0\ncivilization = 0\nclimate = 0\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\nfigure = 0\ngod = 0\nlayer = 8\nmeta = 17\nmonster = 0\nmonster_kind = 0\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nbook = 0\nrace_relation = 0\nregion = 0\nregion_biome = 0\nbook = 0\nregion_border = 0\nregion_control = 0\nregion_name = 0\nrelation = 0\nsettlement = 0\nsettlement_census = 0\nsettlement_race = 0\nsite = 0\n[layers]\n" +
        layer_line(layers[0], "i16", "-5", "80") + layer_line(layers[1], "u8", "0", "3") +
        layer_line(layers[2], "u16", "0", "8") + layer_line(layers[3], "u8", "0", "8") +
        layer_line(layers[4], "u32", "0", "10") + layer_line(layers[5], "u8", "0", "2") +
        layer_line(layers[6], "u32", "0", "1") + layer_line(layers[7], "i16", "-22", "48") +
        p100::with_empty_pass_lines(p100::worked_geography());
    CHECK(report_of(out) == expected);
}

TEST_CASE("report v2: no [geography] without a layer named elevation") {
    dbtest::TempDir dir("p100-report-noelev");
    const std::string out = dir.file("w.db");
    std::vector<FixtureLayer> layers;
    layers.push_back({"water", p100::grid<std::uint8_t>(2, 1, {0, 3}), Codec::delta_varint});
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 1, layers).ok());
    const std::string r = report_of(out);
    CHECK(!has(r, "[geography]"));
    CHECK(r.substr(0, 21) == "# worldgen report v2\n");
}

TEST_CASE("report v2, appendix C 9: unusable geography layers are corrupt_layer errors") {
    dbtest::TempDir dir("p100-report-bad");
    auto check_fails = [&](const std::string& name, std::vector<FixtureLayer> layers, const char* layer) {
        const std::string out = dir.file(name);
        REQUIRE(worldgen::stages::testing::write_fixture_world(out, 0, layers).ok());
        const auto r = try_report(out);
        REQUIRE(!r.ok());
        CHECK(r.code() == worldgen::db::ErrorCode::corrupt_layer);
        INFO("message: ", r.error().message);
        CHECK(has(r.error().message, layer));
        return r.error().message;
    };
    SUBCASE("elevation not i16") {
        std::vector<FixtureLayer> layers;
        layers.push_back({"elevation", p100::grid<std::uint16_t>(2, 1, {1, 2}), Codec::delta_varint});
        check_fails("u16.db", layers, "elevation");
    }
    SUBCASE("a flow cycle") {
        auto layers = p100::worked_example_layers();
        auto& dir_cells = std::get<std::vector<std::uint8_t>>(layers[3].grid.values);
        // (5,2) SE -> (6,3); make (6,3) point back NW to (5,2): a cycle.
        dir_cells[3 * 8 + 6] = 5;
        // Lowest cell that does not reach the ocean: (5,2), index 21.
        const std::string m = check_fails("cycle.db", layers, "flow_dir");
        CHECK(has(m, "(5, 2)"));
    }
    SUBCASE("code 8 on land") {
        auto layers = p100::worked_example_layers();
        std::get<std::vector<std::uint8_t>>(layers[3].grid.values)[1 * 8 + 1] = 8;
        const std::string m = check_fails("code8.db", layers, "flow_dir");
        CHECK(has(m, "(1, 1)"));
    }
    SUBCASE("a hydrology layer of the wrong type") {
        auto layers = p100::worked_example_layers();
        layers[6].grid = worldgen::db::make_layer_grid(worldgen::db::ElementType::u16, 8, 7);  // lake
        check_fails("lake.db", layers, "lake");
    }
    SUBCASE("a hydrology layer of the wrong size") {
        auto layers = p100::worked_example_layers();
        layers[4].grid = worldgen::db::make_layer_grid(worldgen::db::ElementType::u32, 8, 6);  // flow_accum
        check_fails("acc.db", layers, "flow_accum");
    }
}

TEST_CASE("report v2: [geography] and the geography renders do not depend on the codec") {
    dbtest::TempDir dir("p100-report-codec");
    const std::string raw_db = dir.file("raw.db");
    const std::string varint_db = dir.file("varint.db");
    REQUIRE(worldgen::stages::testing::write_fixture_world(raw_db, 3, p100::worked_example_layers(Codec::raw)).ok());
    REQUIRE(worldgen::stages::testing::write_fixture_world(varint_db, 3,
                                                           p100::worked_example_layers(Codec::delta_varint))
                .ok());
    const std::string a = report_of(raw_db);
    const std::string b = report_of(varint_db);
    const std::size_t pa = a.find("[geography]\n");
    const std::size_t pb = b.find("[geography]\n");
    REQUIRE(pa != std::string::npos);
    REQUIRE(pb != std::string::npos);
    CHECK(a.substr(pa) == b.substr(pb));
    CHECK(a.substr(pa) == p100::with_empty_pass_lines(p100::worked_geography()));
    CHECK(has(a, "elevation i16 8x7 codec=0 "));
    CHECK(has(b, "elevation i16 8x7 codec=1 "));
    // Renders of the decoded layers.
    auto ra = worldgen::db::WorldReader::open(raw_db);
    auto rb = worldgen::db::WorldReader::open(varint_db);
    REQUIRE(ra.ok());
    REQUIRE(rb.ok());
    for (const char* name : {"elevation", "water", "water_depth", "flow_dir", "flow_accum", "river", "lake",
                             "coast_dist"}) {
        INFO("layer ", name);
        auto la = ra.value().read_layer(name);
        auto lb = rb.value().read_layer(name);
        REQUIRE(la.ok());
        REQUIRE(lb.ok());
        CHECK(la.value().codec == Codec::raw);
        CHECK(lb.value().codec == Codec::delta_varint);
        const auto pal = worldgen::inspect::auto_palette(name, la.value().grid);
        CHECK(pal == worldgen::inspect::registered_palette(name));
        CHECK(worldgen::inspect::render_rgb(la.value().grid, pal, 2) ==
              worldgen::inspect::render_rgb(lb.value().grid, pal, 2));
    }
}
