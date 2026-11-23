// Spec P1-06 r11 section 17 (P5-12, generator 0.22.0; the P5-01 world critic's F2), tests 17.3 on generated
// worlds: V8 rejects a drawn attempt whose filter entry spans a join of the name's parts, and the region takes
// attempt t + 1; only that region's name changes. The names below are the test writer's reference
// (reference/p1_regions_reference.py with V8; p1-06-regions.txt's case shipped-512-phrase moves by it), checked
// against the stored region table of worlds generated up to regions:
//   - the phrase world at 512 ("the unmapped"): region 11 Tarden (Tar|den, tard) -> Lichenknoll Forest;
//   - 1024 defaults (suite regions-slow): seed 84 region 38 Tarden -> The Tainted Weald, seed 15 region 4
//     Wanegrass -> Kestrelhorn Reaches, seed 59 region 7 Pinegrove -> Larchwood, seed 91 region 16 Pinegrove ->
//     Archer's Grove (P1-06 r11 17.2's four, measured by the reference on the 0.21.0 sweep: exactly these four
//     names change in seeds 0 to 99);
//   - no stored name equals one of the four rejected names in those worlds.
// The unit cases of 17.3 (Night + wood, Knight's, Slag + wold, at_start and whole entries, the fallback) are the
// reference's check_rg11_r11; the library exposes no per-attempt seam for the parts. Written from the spec before
// the implementation (test-writer rule).

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "history/p3_03_chronology_support.hpp"

namespace {

std::vector<std::string> names_of(std::uint64_t seed, const char* phrase, std::int64_t size) {
    const std::string path = chrono_test::world(chrono_test::WorldKey{seed, phrase, size, 6000, "all", "regions", 1});
    const auto r = worldgen::db::WorldReader::open(path);
    REQUIRE(r.ok());
    std::vector<std::string> out;
    for (const auto& row : chrono_test::rows_of(r.value(), "region")) {
        out.push_back(row[1].bytes);
    }
    return out;
}

void check_renamed(std::uint64_t seed, const char* phrase, std::int64_t size, std::size_t region,
                   const std::string& old_name, const std::string& new_name) {
    CAPTURE(seed);
    CAPTURE(old_name);
    const std::vector<std::string> n = names_of(seed, phrase, size);
    REQUIRE(n.size() >= region);
    CHECK(n[region - 1] == new_name);
    for (const std::string& x : n) {
        CHECK(x != old_name);
    }
}

}  // namespace

TEST_SUITE("regions") {

TEST_CASE("P1-06 r11 17.1, 17.3: the phrase world at 512: Tar|den is rejected by V8, region 11 takes its next attempt") {
    const chrono_test::GoldenCase& g = chrono_test::golden().find("geo-phrase-512");
    check_renamed(g.seed, "the unmapped", 512, 11, "Tarden", "Lichenknoll Forest");
}

}  // TEST_SUITE

TEST_SUITE("regions-slow" * doctest::skip()) {

TEST_CASE("P1-06 r11 17.2, P5-12 N2: the four names V8 changes at 1024 defaults (seeds 84, 15, 59, 91)") {
    check_renamed(84, "", 1024, 38, "Tarden", "The Tainted Weald");
    check_renamed(15, "", 1024, 4, "Wanegrass", "Kestrelhorn Reaches");
    check_renamed(59, "", 1024, 7, "Pinegrove", "Larchwood");
    check_renamed(91, "", 1024, 16, "Pinegrove", "Archer's Grove");
}

}  // TEST_SUITE
