// Invariants of the regions stage as property tests (spec P1-06 section 6
// invariants 1 to 6, 10.2).
//
// Each world's layer and tables come from the stage (seam regions_stage);
// every invariant is checked by the test's own code: invariant 5 by the
// test's own RG2 to RG13 (its own Dijkstra on a std::priority_queue, its
// own merge on the region graph, its own border rows and pass crossings,
// p1_06_regions_support.hpp), the others directly on the output (a BFS per
// region for connectivity, the G2 labels for the landmass, recounts of
// every column; r3: the names of invariant 6 by the test's own RG11).
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
    // Invariant 8 (r2): each pass attached to at most one row, to one exactly
    // when its path crosses a border.
    {
        std::int64_t total = 0;
        for (const auto& b : out.borders) {
            total += b.passes;
        }
        std::vector<std::int64_t> reg(out.region.begin(), out.region.end());
        std::int64_t crossing = 0;
        for (const auto& p : ci.passes) {
            crossing += own_pass_attachment(g, reg, in.water, p).key ? 1 : 0;
        }
        if (total > static_cast<std::int64_t>(ci.passes.size()) || total != crossing) {
            fail("invariant 8: passes attached");
        }
    }
    // Invariant 6 (as restated by P1-06 r7 15.7 item 4, generator 0.19.0):
    // names unique, no numeral; for each land noun in use exactly one region,
    // the one with the most cells (ties to the smaller id), is named its base
    // name; every other name one of its land's forms filled from the
    // region's pools (common, land, its traits; weight > 0), valid by V1 to
    // V6, and equal to no keeper's name. Bases, lands and traits by the
    // test's own RG11 1 to 5. (V7 is checked by the golden comparison of the
    // test's own RG11, which applies it.)
    {
        std::set<std::string> names;
        std::map<std::string, std::pair<std::int64_t, std::int64_t>> best;  // land noun -> (cells, -id)
        for (std::size_t k = 0; k < out.regions.size() && k < own.bases.size(); ++k) {
            const auto& row = out.regions[k];
            const std::string noun = region_names::land_noun(own.bases[k]);
            auto it = best.find(noun);
            if (it == best.end() || std::make_pair(row.cells, -row.id) > it->second) {
                best[noun] = std::make_pair(row.cells, -row.id);
            }
        }
        std::set<std::string> base_set;  // the keepers' names
        for (std::size_t k = 0; k < out.regions.size() && k < own.bases.size(); ++k) {
            if (best[region_names::land_noun(own.bases[k])].second == -out.regions[k].id) {
                base_set.insert(own.bases[k]);
            }
        }
        const region_names::PlaceData& pd = region_names::shipped();
        const region_names::NameTuning nt{};
        for (std::size_t k = 0; k < out.regions.size(); ++k) {
            const auto& row = out.regions[k];
            if (!names.insert(row.name).second) {
                fail("duplicate name");
            }
            for (const std::string& w : words_of(row.name)) {
                if (w == "I" || is_numeral_from_2(w)) {
                    fail("a Roman numeral in a name: " + row.name);
                }
            }
            if (k >= own.bases.size()) {
                fail("no own base name");
                continue;
            }
            const std::string& base = own.bases[k];
            if (best[region_names::land_noun(base)].second == -row.id) {
                if (row.name != base) {
                    fail("the keeper does not hold its base name: " + row.name);
                }
                continue;
            }
            if (base_set.count(row.name) != 0U) {
                fail("a drawn name equals a keeper's name: " + row.name);
            }
            const auto td = region_names::trait_defs(pd, region_names::traits_of(own.trait_rows[k], nt));
            if (!region_names::parses_as_land_name(row.name, *own.lands[k], pd.common(), td, nt)) {
                fail("not a valid name of its land: " + row.name);
            }
        }
    }
    for (const auto& row : out.regions) {
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
