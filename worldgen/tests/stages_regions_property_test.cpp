// Invariants of the regions stage as property tests (spec P1-06 section 6
// invariants 1 to 6, 10.2).
//
// Each world's layer and tables come from the stage (seam regions_stage);
// every invariant is checked by the test's own code: invariant 5 by the
// test's own RG2 to RG13 (its own Dijkstra on a std::priority_queue, its
// own merge on the region graph, its own border rows and pass crossings,
// p1_06_regions_support.hpp), the others directly on the output (a BFS per
// region for connectivity, the G2 labels for the landmass, recounts of
// every column, the name grammar).
//
// Seeds 0 to 63 at N = 128 with the shipped and the test-only tuning of
// 10.2 run in the suite "regions-slow":
//   worldgen-tests -ts=regions-slow --no-skip

#include <doctest/doctest.h>

#include <cstdint>
#include <deque>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "p1_05_resources_support.hpp"
#include "p1_06_regions_api.hpp"
#include "p1_06_regions_support.hpp"

using namespace region_test;

namespace {

const Overrides T128 = {{"seed_tile_m", "256"}, {"seed_spacing_m", "400"}, {"region_min_area_m2", "65536"}};

std::vector<std::string> words_of(const std::string& s) { return split(s, ' '); }

bool is_numeral_from_2(const std::string& s) {
    for (std::int64_t k = 2; k < 4000; ++k) {
        if (own_roman(k) == s) {
            return true;
        }
    }
    return false;
}

void check_invariants(const std::string& label, const CaseInputs& ci, const Overrides& o, std::uint64_t seed) {
    CAPTURE(label);
    const region_api::Inputs& in = ci.in;
    const Grid g = in.grid;
    const std::size_t n = g.cells();
    const Values v = values(o);
    const region_api::Output out = region_api::stage(in, definitions(), ci.passes, tuning(o), seed, 3);
    std::map<std::string, std::int64_t> bad;
    auto fail = [&](const std::string& what) {
        if (bad[what]++ == 0) {
            MESSAGE(label << ": " << what);
        }
    };
    // Invariant 1.
    REQUIRE(out.region.size() == n);
    for (std::size_t i = 0; i < n; ++i) {
        if ((out.region[i] > 0) != is_land(in.water[i])) {
            fail("region > 0 exactly on land");
        }
    }
    // Invariant 2.
    const auto R = static_cast<std::int64_t>(out.regions.size());
    std::vector<std::int64_t> first(static_cast<std::size_t>(R) + 1, -1);
    std::vector<std::vector<std::size_t>> cells(static_cast<std::size_t>(R) + 1);
    for (std::size_t i = 0; i < n; ++i) {
        const std::int64_t r = out.region[i];
        if (r > R) {
            fail("region id above R");
            continue;
        }
        if (r > 0) {
            if (first[uz(r)] < 0) {
                first[uz(r)] = static_cast<std::int64_t>(i);
            }
            cells[uz(r)].push_back(i);
        }
    }
    for (std::int64_t r = 1; r <= R; ++r) {
        if (out.regions[uz(r - 1)].id != r) {
            fail("rows not in id order 1 .. R");
        }
        if (first[uz(r)] < 0) {
            fail("an id without cells");
        }
        if (r > 1 && first[uz(r)] <= first[uz(r - 1)]) {
            fail("ids not increasing with the smallest cell index");
        }
    }
    // Invariant 3: 8-connected, one landmass, contains its seed (a site cell).
    std::int64_t nlm = 0;
    const std::vector<std::int64_t> lm = own_landmass(g, in.water, nlm);
    for (const auto& row : out.regions) {
        const auto& cs = cells[uz(row.id)];
        if (cs.empty()) {
            continue;
        }
        const std::size_t s = g.index(static_cast<std::int32_t>(row.seed_x), static_cast<std::int32_t>(row.seed_y));
        if (out.region[s] != row.id) {
            fail("seed outside its region");
        }
        if (in.water[s] != WATER_DRY) {
            fail("seed not a site cell");
        }
        if (lm[s] != row.landmass) {
            fail("landmass is not the seed's G2 label");
        }
        std::set<std::size_t> seen{cs[0]};
        std::deque<std::size_t> q{cs[0]};
        while (!q.empty()) {
            const std::size_t c = q.front();
            q.pop_front();
            if (lm[c] != row.landmass) {
                fail("region on two landmasses");
            }
            const auto cc = static_cast<worldgen::geo::CellIndex>(c);
            for (std::size_t d = 0; d < 8; ++d) {
                const std::int32_t x = g.x_of(cc) + DX[d], y = g.y_of(cc) + DY[d];
                if (x < 0 || y < 0 || x >= g.width || y >= g.height) {
                    continue;
                }
                const std::size_t m = g.index(x, y);
                if (out.region[m] == row.id && seen.insert(m).second) {
                    q.push_back(m);
                }
            }
        }
        if (seen.size() != cs.size()) {
            fail("region not 8-connected");
        }
    }
    // Invariant 4: at least min_cells, unless no 4-adjacent neighbour.
    std::set<std::int64_t> has_neighbour;
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            const std::uint16_t a = out.region[g.index(x, y)];
            for (const auto& [xx, yy] : {std::pair{x + 1, y}, std::pair{x, y + 1}}) {
                if (xx < g.width && yy < g.height) {
                    const std::uint16_t b = out.region[g.index(xx, yy)];
                    if (a != 0 && b != 0 && a != b) {
                        has_neighbour.insert(a);
                        has_neighbour.insert(b);
                    }
                }
            }
        }
    }
    for (const auto& row : out.regions) {
        if (row.cells < v.min_cells() && has_neighbour.count(row.id) != 0U) {
            fail("small region with a neighbour");
        }
        if (row.cells != static_cast<std::int64_t>(cells[uz(row.id)].size())) {
            fail("cells column");
        }
    }
    // The border rows are exactly the neighbouring pairs, a < b.
    std::set<std::pair<std::int64_t, std::int64_t>> keys;
    for (const auto& b : out.borders) {
        if (b.a >= b.b || b.a < 1 || b.b > R) {
            fail("border key");
        }
        keys.insert({b.a, b.b});
    }
    if (keys.size() != out.borders.size()) {
        fail("duplicate border row");
    }
    std::set<std::int64_t> in_rows;
    for (const auto& k : keys) {
        in_rows.insert(k.first);
        in_rows.insert(k.second);
    }
    if (in_rows != has_neighbour) {
        fail("border rows are not the neighbouring pairs");
    }
    // Biome rows sum to cells.
    std::map<std::int64_t, std::int64_t> bsum;
    for (const auto& b : out.biomes) {
        bsum[b.region] += b.cells;
        if (b.cells <= 0) {
            fail("biome row with no cells");
        }
    }
    for (const auto& row : out.regions) {
        if (bsum[row.id] != row.cells) {
            fail("biome rows do not sum to cells");
        }
    }
    // Invariant 5: every column equals the test's own computation.
    const Own own = own_regions(in, ci.passes, v, seed);
    if (!(own.out.region == out.region)) {
        fail("layer differs from the test's own stage");
    }
    if (!(own.out.regions == out.regions)) {
        fail("table region differs from the test's own stage");
    }
    if (!(own.out.borders == out.borders)) {
        fail("table region_border differs from the test's own stage");
    }
    if (!(own.out.biomes == out.biomes)) {
        fail("table region_biome differs from the test's own stage");
    }
    // Invariant 6: unique names of data words; numerals from II.
    std::set<std::string> names;
    std::set<std::string> sector_words(v.names.begin(), v.names.begin() + 9);
    std::set<std::string> nouns{v.names[9]};
    for (const auto& b : definitions().biomes) {
        nouns.insert(b.region_noun);
    }
    for (const auto& row : out.regions) {
        if (!names.insert(row.name).second) {
            fail("duplicate name");
        }
        const std::vector<std::string> w = words_of(row.name);
        if (w.size() < 2 || w.size() > 3 || sector_words.count(w[0]) == 0U || nouns.count(w[1]) == 0U ||
            (w.size() == 3 && !is_numeral_from_2(w[2]))) {
            fail("name grammar: " + row.name);
        }
        if (row.biome < 1 || own.out.regions.empty()) {
            fail("biome");
        }
    }
    CHECK(bad.empty());
}

CaseInputs world_case(std::uint64_t seed) {
    const resource_api::Inputs ri = resource_test::world_inputs(seed, 128, false);
    const resource_api::Layers l =
        resource_api::layers(ri, resource_test::definitions(), resource_test::tuning(), seed, 4);
    return CaseInputs{from_resources(ri, l), world_passes(seed, 128, false)};
}

}  // namespace

TEST_CASE("P1-06 6, 10.2: invariants 1 to 6 on the golden cases up to N = 128") {
    for (const GoldenCase& c : golden()) {
        if (c.n > 128) {
            continue;
        }
        check_invariants(c.name, case_inputs(c), c.overrides, c.seed);
    }
}

TEST_CASE("P1-06 6, 10.2: invariants 1 to 6 at N = 128, seeds 0 to 3, test-only tuning") {
    for (std::uint64_t seed = 0; seed < 4; ++seed) {
        check_invariants("seed " + std::to_string(seed), world_case(seed), T128, seed);
    }
}

TEST_SUITE("regions-slow" * doctest::skip()) {

TEST_CASE("P1-06 6, 10.2 (slow): invariants 1 to 6 at N = 128, seeds 0 to 63, shipped and test-only tuning") {
    for (std::uint64_t seed = 0; seed < 64; ++seed) {
        const CaseInputs ci = world_case(seed);
        check_invariants("shipped seed " + std::to_string(seed), ci, {}, seed);
        check_invariants("test seed " + std::to_string(seed), ci, T128, seed);
    }
}

}  // TEST_SUITE
