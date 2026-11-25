// Spec P5-06 (docs/specs/p5-06-figures.md r3) 18.2 and 18.3: the invariants of 13 that rows show
// (p5_06_figures_checks.hpp, the reference's invariant_violations) on the shipped data's generated worlds (seeds 0
// to 9 at 512; in the suite "figures-slow" seeds 0 to 19 at 1024 and civs4 seeds 0 to 4), on figures-small (18.3:
// every kind of 79 to 92 happens, regnal numbers up to III), no ruin_faded and no status 4 on every world
// (invariant 14), determinism (a world twice, at one and at four threads).
//
// I0 (7.11: the zero tuning against the configuration without the subsystem) and the resume test (P3-04 16.3) need
// a configuration of the stage the spec does not name: they are the engineer's (see p5_06_figures_api.hpp) and the
// reference's (`p5_06_figures_reference.py fixtures`, `i0-world`). Every case starts with
// P506_REQUIRE_INTEGRATED(). Written from the spec before the implementation.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "p5_03_relations_runs.hpp"
#include "p5_06_figures_api.hpp"
#include "p5_06_figures_checks.hpp"
#include "p5_06_figures_runs.hpp"

namespace {

namespace db = worldgen::db;
using p506run::figures_small;
using p506run::generated;

std::string counts_line(const p506check::World& w) {
    std::string out;
    for (const auto& [k, n] : p506check::figure_kind_counts(w)) out += (out.empty() ? "" : ", ") + k + " " + std::to_string(n);
    return out;
}

void check_world(const std::string& path, const std::string& label) {
    CAPTURE(label);
    const db::WorldReader r = chrono_test::open_world(path);
    const p506check::World w = p506check::world_of_file(r);
    CHECK(p506check::violations(w) == std::vector<std::string>{});
    MESSAGE(label << ": " << w.figs.size() << " figures; " << counts_line(w));
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-06 18.2: the invariants of 13 (and 14: no ruin fades) on the shipped data's worlds, seeds 0 to 9 at 512") {
    P506_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        check_world(p503run::world_path(seed, 512), "seed " + std::to_string(seed) + " at 512");
    }
}

TEST_CASE("P5-06 18.3: figures-small: every kind of 79 to 92 happens, regnal numbers reach III; the invariants hold") {
    P506_REQUIRE_INTEGRATED();
    std::map<std::string, std::int64_t> seen;
    std::int64_t regnal_max = 0;
    for (const std::uint64_t seed : {42U, 0U, 3U}) {
        const std::string path = generated(seed, 512, figures_small(), "small");
        check_world(path, "figures-small seed " + std::to_string(seed));
        const db::WorldReader r = chrono_test::open_world(path);
        const p506check::World w = p506check::world_of_file(r);
        for (const auto& [k, n] : p506check::figure_kind_counts(w)) seen[k] += n;
        for (const auto& [id, f] : w.figs) regnal_max = std::max(regnal_max, f.regnal);
    }
    for (const auto& [k, n] : seen) {
        CAPTURE(k);
        CHECK(n > 0);
    }
    CHECK(regnal_max == 3);   // regnal_max 3 is the highest number (6.14)
}

TEST_CASE("P5-06 14: determinism: a world twice gives the same file; one and four threads") {
    P506_REQUIRE_INTEGRATED();
    const std::string a = generated(5, 512, figures_small(), "det-a");
    const std::string b = generated(5, 512, figures_small(), "det-b");
    CHECK(dbtest::read_file(a) == dbtest::read_file(b));
    const std::string four = generated(5, 512, figures_small(), "det-4", 0, 4);
    CHECK(dbtest::read_file(a) == dbtest::read_file(four));
}

}  // TEST_SUITE

TEST_SUITE("figures-slow" * doctest::skip()) {

TEST_CASE("P5-06 18.2: the invariants of 13 at 1024, seeds 0 to 19") {
    P506_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        check_world(p503run::world_path(seed, 1024), "seed " + std::to_string(seed) + " at 1024");
    }
}

TEST_CASE("P5-06 18.2: the invariants of 13 on civs4 worlds at 1024, seeds 0 to 4") {
    P506_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 5; ++seed) {
        check_world(generated(seed, 1024, conflict_test::shipped(), "civs4", 4), "civs4 seed " + std::to_string(seed));
    }
}

}  // TEST_SUITE
