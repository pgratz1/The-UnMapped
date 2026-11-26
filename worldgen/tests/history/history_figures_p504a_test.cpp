// Spec P5-04a (docs/specs/p5-04a-relations-alliances-followups.md r2) test 16 items 16 and 19 (figures, 21.4 and
// 21.5): every kind 4 site of a realm opened more than D = tower_spacing_permille x W div 1000 (Chebyshev) from every
// earlier kind 4 site of its realm, open or ended (a library at its city excepted as the later one); every ended tower
// ended by 21.4's table (5 its realm's ending event; 4 its city's ruin or change of owner, or its region's change of
// owner) in the ending event's own year; no book after its tower's end; every hero_rose's quiet equal to 21.5's g
// when g >= hero_return_years, else 0 (p5_04a_checks.hpp: figure_violations; the reference's p5_06_figures_reference.py
// violations_028), on the shipped data's worlds (seeds 0 to 9 at 512; "figures-slow" seeds 0 to 19 at 1024) and on
// P5-06's figures-small, where towers end (TW1 reported). The tower rule's own cases (a free cell at D and D + 1, no
// free cell, each ending) are the reference's replay (p5_06_figures_reference.py, height_cell, tower_endings): their
// inputs are not reachable from outside the subsystem. Every case starts with P504A_REQUIRE_INTEGRATED().

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "p5_03_relations_runs.hpp"
#include "p5_04a_api.hpp"
#include "p5_04a_checks.hpp"
#include "p5_06_figures_runs.hpp"

namespace {

namespace db = worldgen::db;
namespace wd = worldgen::data;

struct Result {
    std::vector<std::string> violations;
    std::int64_t towers = 0, ended = 0, quiet_heroes = 0;
};

Result check(const std::string& path, const wd::ContentSet& content) {
    const db::WorldReader r = chrono_test::open_world(path);
    const p503check::World w = p503check::world_of_file(r, content);
    const std::int64_t width = std::stoll(r.meta("param.map_size").value_or("0"));
    Result out;
    out.violations = p504a_check::figure_violations(w, chrono_test::rows_of(r, "book"), width,
                                                    p504a_check::tuning_of(content));
    for (const auto& s : w.rows.t.site) {
        if (s.at(2).integer != 4) continue;
        ++out.towers;
        if (s.at(11).type != db::ValueType::null) ++out.ended;
    }
    const p504check::Rows rows(w);
    for (const std::int64_t h : rows.of("hero_rose")) out.quiet_heroes += rows.value(h, "quiet") > 0 ? 1 : 0;
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-04a 21.4, 21.5: towers apart and ended, books, hero_rose's quiet on the shipped worlds, seeds 0 to 9") {
    P504A_REQUIRE_INTEGRATED();
    std::int64_t towers = 0, ended = 0, quiet = 0;
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        CAPTURE(seed);
        const Result x = check(p503run::world_path(seed, 512), conflict_test::shipped());
        CHECK(x.violations == std::vector<std::string>{});
        towers += x.towers;
        ended += x.ended;
        quiet += x.quiet_heroes;
    }
    MESSAGE("seeds 0 to 9 at 512: " << towers << " kind 4 sites, " << ended << " ended; " << quiet
                                    << " hero_rose with quiet > 0");
}

TEST_CASE("P5-04a 21.4: figures-small: the invariants hold and some tower or library ends") {
    P504A_REQUIRE_INTEGRATED();
    std::int64_t ended = 0;
    for (const std::uint64_t seed : {42U, 0U, 3U}) {
        CAPTURE(seed);
        const Result x = check(p506run::generated(seed, 512, p506run::figures_small(), "small"), p506run::figures_small());
        CHECK(x.violations == std::vector<std::string>{});
        ended += x.ended;
    }
    CHECK(ended > 0);
}

}  // TEST_SUITE

TEST_SUITE("figures-slow" * doctest::skip()) {

TEST_CASE("P5-04a 21.4, 21.5: the figures invariants at 1024, seeds 0 to 19") {
    P504A_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        CAPTURE(seed);
        CHECK(check(p503run::world_path(seed, 1024), conflict_test::shipped()).violations == std::vector<std::string>{});
    }
}

}  // TEST_SUITE
