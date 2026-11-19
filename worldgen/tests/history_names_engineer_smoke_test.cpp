// Engineer's smoke test of the name generator (spec P3-02): the worked
// example of 4.6 only. The spec's tests (section 9) are the test writer's.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>

#include "worldgen/core/hash.hpp"
#include "worldgen/history/draw.hpp"
#include "worldgen/history/names.hpp"

namespace {

using worldgen::history::CultureNaming;
using worldgen::history::NameKind;

CultureNaming elven() {
    CultureNaming n;
    n.onsets = {"", "l", "th", "s", "f", "n", "m", "r", "v", "gl", "c", "el", "ith"};
    n.nuclei = {"a", "e", "i", "ae", "ie", "o"};
    n.codas = {"", "", "l", "n", "th", "s", "r", "nd", "ll", "nn"};
    n.patterns = {"ONONC", "ONONC", "ONCON", "ONONON", "NCONC"};
    n.min_letters = 4;
    n.max_letters = 11;
    n.forbidden = {"aea", "iei", "iae", "eae", "aa", "ii", "ee", "thth", "lll", "nnn", "ea", "ia"};
    n.god_forms = {"{root}"};
    n.civilization_forms = {"The Court of {root}", "The Realm of {root}", "{root}"};
    n.settlement_forms = {"{root}", "{root}", "{root}", "{root}dell"};
    return n;
}

}  // namespace

TEST_CASE("P3-02 engineer smoke: worked example 4.6 (seed 0, elven settlement 1)") {
    const std::uint64_t key = worldgen::stream_key(0, "hist.names.settlement");
    CHECK(key == 0x969C418DDF3C5EDCULL);
    CHECK(worldgen::history::draw(key, 0, 2, 1, 0) == 0xAB9A949FA699954CULL);
    CHECK(worldgen::history::draw(key, 0, 2, 1, 6) == 0x7347EA42EAED2BB2ULL);

    const CultureNaming n = elven();
    const auto a0 = worldgen::history::name_attempt(n, NameKind::settlement, 0, 2, 1, 0);
    CHECK(a0.valid);
    CHECK(a0.root == "iensath");
    CHECK(a0.name == "Iensath");

    worldgen::history::NameRegistry reg;
    CHECK(worldgen::history::make_name(n, "elven", NameKind::settlement, 0, 2, 1, reg) == "Iensath");
    CHECK(worldgen::history::make_name(n, "elven", NameKind::settlement, 0, 2, 1, reg) == "Focoth");
}
