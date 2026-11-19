// Property and determinism tests of spec P3-03: 14.2 items 13 to 15 and
// 14.3 items 16 and 17. The schedule through awakening_years against the
// test's own C2/C3 and the invariants of section 10 (2 and 3); packed
// hand-made shares; invariants 1 and 4 to 9 on the stage's rows for the
// two fixture data roots with synthetic region names (some colliding);
// thread counts; resume from a --until regions world.
//
// The full sweeps of 14.2 (seeds 0-999) are in the doctest suite
// "chronology-slow" (skipped by default: worldgen-tests -ts=chronology-slow
// --no-skip); the default suite runs the first 50 (item 13) and 100 (item
// 14) seeds of the same loops.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <tuple>
#include <cstdint>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "p3_03_chronology_support.hpp"

namespace {

using namespace chrono_test;
using chrono_api::Params;

// Item 13 for seeds [0, seeds).
void sweep_shipped(std::uint64_t seeds) {
    const Tuning t = shipped_tuning();
    const std::vector<std::vector<std::string>> subsets = all_subsets();
    std::uint64_t failures = 0;
    for (std::uint64_t seed = 0; seed < seeds; ++seed) {
        for (const std::int64_t span : {2000, 2001, 6000, 11999, 12000}) {
            const std::vector<OwnStep> full = own_shipped_schedule(seed, span, six());
            std::map<std::int64_t, std::int64_t> full_year;
            for (const OwnStep& s : full) {
                full_year[s.id] = s.year;
            }
            for (const std::vector<std::string>& sub : subsets) {
                const auto [ids, shares] = present_of(sub);
                const std::vector<std::int64_t> y = chrono_api::awakening_years(seed, span, ids, shares, t);
                const std::vector<OwnStep> own = own_schedule(seed, span, ids, shares, t.jitter_raw, t.gap);
                const auto p = static_cast<std::int64_t>(ids.size());
                const std::int64_t g = own_gap(span, p, t.gap);
                const std::int64_t jmax = own_jmax(span, t.jitter_raw);
                bool ok = y == years_of(own) && y.size() == ids.size() && y.front() == 0;
                for (std::size_t i = 1; ok && i < y.size(); ++i) {
                    ok = y[i] - y[i - 1] >= g                                // invariant 2
                         && std::abs(y[i] - own[i].base) <= jmax              // invariant 3
                         && !own[i].clamped                                  // no clamp with the shipped data
                         && y[i] == full_year[ids[i]];                       // a race's year is its own (C3)
                }
                ok = ok && y.back() <= span - g;
                if (!ok) {
                    ++failures;
                    if (failures <= 5) {
                        MESSAGE("schedule differs: seed " << seed << " span " << span << " races " << joined(sub));
                    }
                }
            }
        }
    }
    CHECK(failures == 0U);
}

// A test-owned keyed sequence for hand-made data (not a library stream).
std::uint64_t test_word(std::uint64_t seed, std::uint64_t k) {
    return names_api::draw(worldgen::stream_key(seed, "test.chronology.packed"), 0, 0, 0, k);
}

// Item 14 for seeds [0, seeds): 2 to 6 races, shares in [0, 1) sorted
// (strictly increasing after the first 0), gap 1 to 2000, jitter 0 to 0.05.
void sweep_packed(std::uint64_t seeds) {
    std::uint64_t failures = 0;
    std::uint64_t clamped = 0;
    for (std::uint64_t seed = 0; seed < seeds; ++seed) {
        const std::size_t n = 2 + static_cast<std::size_t>(test_word(seed, 0) % 5);
        std::set<std::int64_t> raw;
        std::uint64_t k = 1;
        while (raw.size() < n - 1) {
            raw.insert(1 + static_cast<std::int64_t>(test_word(seed, k++) >> 33));  // in [1, 2^31]: packed in [0, 0.5]
        }
        std::vector<std::int64_t> shares = {0};
        shares.insert(shares.end(), raw.begin(), raw.end());
        std::vector<std::int64_t> ids;
        for (std::size_t i = 0; i < n; ++i) {
            ids.push_back(static_cast<std::int64_t>(i + 1));
        }
        const std::int64_t gap = 1 + static_cast<std::int64_t>(test_word(seed, 100) % 2000);
        const std::int64_t jitter = static_cast<std::int64_t>(test_word(seed, 101) % 214748366);  // 0 .. raw 0.05
        const std::int64_t span = 2000 + static_cast<std::int64_t>(test_word(seed, 102) % 10001);
        const Tuning t{jitter, gap, "The Age of {plural}"};
        const std::vector<std::int64_t> y = chrono_api::awakening_years(seed, span, ids, shares, t);
        const std::vector<OwnStep> own = own_schedule(seed, span, ids, shares, jitter, gap);
        const std::int64_t g = own_gap(span, static_cast<std::int64_t>(n), gap);
        bool ok = y == years_of(own) && y.front() == 0 && y.back() <= span - g;
        for (std::size_t i = 1; ok && i < y.size(); ++i) {
            ok = y[i] > y[i - 1] && y[i] - y[i - 1] >= g;
        }
        for (const OwnStep& s : own) {
            clamped += s.clamped ? 1U : 0U;
        }
        if (!ok) {
            ++failures;
            if (failures <= 5) {
                MESSAGE("packed schedule fails: seed " << seed);
            }
        }
    }
    CHECK(failures == 0U);
    CHECK(clamped > 0U);  // the sweep does exercise the clamp
}

// Invariants 1 and 4 to 8 of section 10 on rows (the data are the shipped
// races, as in both fixture roots).
void check_invariants(const Rows& r, const Params& p, const std::vector<std::string>& regions) {
    // (2, the years, is checked by the caller against own_schedule.)
    const std::vector<const history_data::Race*> listed = races_in_order();
    // 1: race rows
    REQUIRE(r.races.size() == listed.size());
    std::vector<std::string> present;
    for (std::size_t i = 0; i < r.races.size(); ++i) {
        const RaceRow& x = r.races[i];
        const history_data::Race& d = *listed[i];
        CHECK(x.id == static_cast<std::int64_t>(i + 1));
        CHECK(x.id == d.order);
        CHECK(x.key == d.id);
        CHECK(x.name == d.name);
        CHECK(x.plural == d.plural);
        CHECK(x.adjective == d.adjective);
        CHECK(x.camp == camp_code(d.camp));
        CHECK(x.lifespan_years == d.lifespan_years);
        CHECK(x.generation_years == d.generation_years);
        CHECK(x.culture == culture_number(d.culture));
        const bool in = std::find(p.races.begin(), p.races.end(), std::string(d.id)) != p.races.end();
        CHECK(x.present == (in ? 1 : 0));
        CHECK(x.awakening_year.has_value() == in);
        CHECK(x.creator_god == x.id);
        if (in) {
            present.push_back(x.key);
        }
    }
    CHECK(present == p.races);
    // 4: ages tile [0, E)
    REQUIRE(r.ages.size() == p.races.size());
    for (std::size_t i = 0; i < r.ages.size(); ++i) {
        const AgeRow& a = r.ages[i];
        CHECK(a.id == static_cast<std::int64_t>(i + 1));
        CHECK(a.race == race_listing(p.races[i]).order);
        CHECK(a.start_year == r.races[static_cast<std::size_t>(a.race - 1)].awakening_year.value_or(-1));
        CHECK(a.end_year == (i + 1 < r.ages.size() ? r.ages[i + 1].start_year - 1 : p.span - 1));
        CHECK(a.end_year >= a.start_year);
    }
    CHECK(r.ages.front().start_year == 0);
    // 5: gods
    REQUIRE(r.gods.size() == listed.size());
    for (std::size_t i = 0; i < r.gods.size(); ++i) {
        CHECK(r.gods[i].id == static_cast<std::int64_t>(i + 1));
        CHECK(r.gods[i].camp == camp_code(listed[i]->camp));
    }
    // 6: cultures, canonical id order, race = the id of its race; D3
    REQUIRE(r.cultures.size() == 6U);
    for (std::size_t i = 0; i < r.cultures.size(); ++i) {
        const CultureRow& c = r.cultures[i];
        CHECK(c.id == static_cast<std::int64_t>(i + 1));
        CHECK(c.key == names_test::CULTURE_IDS[i]);
        std::int64_t race = 0;
        for (const history_data::CultureListing& l : history_data::culture_listings()) {
            if (l.id == c.key) {
                CHECK(c.name == l.name);
                race = race_listing(l.race).order;
            }
        }
        CHECK(c.race == race);
        CHECK(r.races[static_cast<std::size_t>(c.race - 1)].culture == c.id);
    }
    // 7: race_biome = the listed entries, (race, biome) order
    std::vector<RaceBiomeRow> want;
    for (const history_data::Race* d : listed) {
        for (const history_data::RaceBiome& b : d->biomes) {
            want.push_back(RaceBiomeRow{d->order, biome_code(b.biome), b.weight});
        }
    }
    std::sort(want.begin(), want.end(), [](const RaceBiomeRow& a, const RaceBiomeRow& b) {
        return a.race != b.race ? a.race < b.race : a.biome < b.biome;
    });
    CHECK(r.race_biomes == want);
    CHECK(r.race_biomes.size() == 45U);
    for (const RaceBiomeRow& b : r.race_biomes) {
        CHECK(b.biome != 1);  // ocean
        CHECK(b.biome != 2);  // lake
        CHECK(b.biome != 8);  // blighted_waste
    }
    // 8: names
    std::set<std::string> all(regions.begin(), regions.end());
    CHECK(all.size() == regions.size());
    for (const AgeRow& a : r.ages) {
        CHECK(all.insert(a.name).second);
        CHECK(a.name.size() <= 64U);
        const std::string base = own_age_text("The Age of {plural}",
                                              r.races[static_cast<std::size_t>(a.race - 1)].plural, "");
        CHECK((a.name == base || a.name.rfind(base + " ", 0) == 0));
    }
    for (const GodRow& g : r.gods) {
        CHECK(all.insert(g.name).second);
        CHECK(!g.name.empty());
        CHECK(g.name.size() <= 64U);
        for (const char ch : g.name) {
            CHECK(((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == ' ' || ch == '\'' || ch == '-'));
        }
    }
}

// Region name lists for the fixture runs: placeholder names, and names that
// collide with the shipped ages and with gods of the seed.
std::vector<std::string> region_list(std::uint64_t seed, const Rows& clean) {
    std::vector<std::string> out = {"North Downs", "North Downs II", "East Wood"};
    if (seed % 2 == 0) {
        out.push_back("The Age of Elves");
        out.push_back("The Age of Orcs");
    }
    if (seed % 3 == 0 && !clean.gods.empty()) {
        out.push_back(clean.gods[seed % clean.gods.size()].name);
    }
    return out;
}

}  // namespace

TEST_SUITE("chronology") {

TEST_CASE("P3-03 14.2 item 13: shipped races, seeds 0-49, five spans, 63 subsets: invariants 2, 3 and C3") {
    sweep_shipped(50);
}

TEST_CASE("P3-03 14.2 item 14: packed hand-made shares, seeds 0-99: strict order, gap G, last age >= G") {
    sweep_packed(100);
}

TEST_CASE("P3-03 14.2 item 15: invariants 1, 4 to 9 through the stage entry on the two fixture data roots") {
    dbtest::TempDir dir("chronology-fixture");
    const std::vector<std::pair<const char*, const wd::ContentSet*>> roots = {{"geo", &fixture_geo()},
                                                                             {"geo-small", &fixture_geo_small()}};
    const std::vector<std::vector<std::string>> subsets = {six(), {"elf", "orc"}, {"goblin", "faerie"}, {"human"}};
    int n = 0;
    for (const auto& [label, content] : roots) {
        for (std::uint64_t seed = 0; seed < 16; ++seed) {
            const std::vector<std::string>& races = subsets[seed % subsets.size()];
            const std::int64_t span = seed % 3 == 0 ? 2000 : (seed % 3 == 1 ? 6000 : 12000);
            const Params p = params_with(span, races);
            CAPTURE(label);
            CAPTURE(seed);
            const Rows clean = chrono_api::stage(*content, p, seed, {});
            const std::vector<std::string> regions = region_list(seed, clean);
            const std::string path = dir.file("w" + std::to_string(n++) + ".db");
            REQUIRE(run_pipeline(path, *content, seed, p, regions).empty());
            const worldgen::db::WorldReader r = open_world(path);
            const Rows rows = rows_of_world(r);
            check_invariants(rows, p, regions);
            // 2: the stored years are C3's
            std::vector<std::int64_t> y;
            for (const AgeRow& a : rows.ages) {
                y.push_back(a.start_year);
            }
            CHECK(y == years_of(own_shipped_schedule(seed, span, races)));
            // the stage seam with the stored region names gives the stored rows
            CHECK(chrono_api::stage(*content, p, seed, region_names_of(r)) == rows);
            // 9: the eight keys, canonical
            for (const auto& [k, v] : chrono_api::meta_entries(p)) {
                CHECK(meta_or(r, k) == v);
            }
            CHECK(meta_or(r, "stages") == "regions,chronology");
            // the fixture roots give the shipped data's rows
            CHECK(chrono_api::stage(shipped(), p, seed, regions) == rows);
        }
    }
}

TEST_CASE("P3-03 14.3 item 16: thread counts 1, 2 and 5 give identical chronology tables") {
    dbtest::TempDir dir("chronology-threads");
    std::map<std::string, std::string> first;
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        CAPTURE(threads);
        const std::string path = dir.file("t" + std::to_string(threads) + ".db");
        REQUIRE(run_pipeline(path, shipped(), 11, Params{}, {"North Downs", "Fielie"}, threads).empty());
        const std::map<std::string, std::string> h = detail_tables(open_world(path));
        if (first.empty()) {
            first = h;
        }
        for (const std::string& t : five_tables()) {
            CHECK(h.at(t) == first.at(t));
        }
        CHECK(dbtest::read_file(path) == dbtest::read_file(dir.file("t1.db")));
    }
}

TEST_CASE("P3-03 14.3 item 17, H8: --until regions, read back, then chronology_stage equals one run") {
    for (const auto& [seed, span, races] : std::vector<std::tuple<std::uint64_t, std::int64_t, std::string>>{
             {42, 6000, "all"}, {7, 2000, "all"}}) {
        CAPTURE(seed);
        const std::string geo = world(WorldKey{seed, "", 512, span, races, "regions", 1});
        const std::string full = world(WorldKey{seed, "", 512, span, races, "", 1});
        const worldgen::db::WorldReader g = open_world(geo);
        const worldgen::db::WorldReader f = open_world(full);
        // The geography-only world: five empty tables, the eight keys present.
        for (const std::string& t : five_tables()) {
            CHECK(rows_of(g, t).empty());
        }
        CHECK(meta_or(g, "stages") == "elevation,hydrology,climate,biomes,resources,regions");
        CHECK(meta_or(f, "stages") == "elevation,hydrology,climate,biomes,resources,regions,chronology");
        const Params p = chrono_api::params_from_meta(g);
        CHECK(p == params_with(span, races_of_text(races)));
        CHECK(chrono_api::params_from_meta(f) == p);
        const Rows resumed = chrono_api::stage(shipped(), p, std::stoull(meta_or(g, "seed")), region_names_of(g));
        CHECK(resumed == rows_of_world(f));
        // And the geography is the same in both (the stage reads no layer and writes none).
        const std::map<std::string, std::string> hg = detail_tables(g);
        const std::map<std::string, std::string> hf = detail_tables(f);
        for (const char* t : {"region", "region_border", "region_biome", "biome", "climate", "pass", "layer"}) {
            CAPTURE(t);
            CHECK(hg.at(t) == hf.at(t));
        }
    }
}

}  // TEST_SUITE

TEST_SUITE("chronology-slow" * doctest::skip()) {

TEST_CASE("P3-03 14.2 item 13 (full): seeds 0-999, five spans, 63 subsets (315,000 schedules)") { sweep_shipped(1000); }

TEST_CASE("P3-03 14.2 item 14 (full): packed hand-made shares, seeds 0-999") { sweep_packed(1000); }

TEST_CASE("P3-03 14.3 item 16 (worlds): 512 seed 42 with threads 1, 2 and 5: identical files") {
    const std::string a = world(WorldKey{42, "", 512, 6000, "all", "", 1});
    const std::string b = world(WorldKey{42, "", 512, 6000, "all", "", 2});
    const std::string c = world(WorldKey{42, "", 512, 6000, "all", "", 5});
    CHECK(dbtest::read_file(a) == dbtest::read_file(b));
    CHECK(dbtest::read_file(a) == dbtest::read_file(c));
}

}  // TEST_SUITE
