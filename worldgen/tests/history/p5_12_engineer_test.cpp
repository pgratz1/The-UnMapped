// The engineer's checks of P5-12 helpers (the independent tests are the
// test writer's): the fused-suffix rule (P3-02 r19 16.1), V8 at joins
// (P1-06 r11 17.1), the realm root (P5-12 5.3), the form filler (5.1), the
// caps (6) and settlement_row (P3-07 r21 26.7).

#include <doctest/doctest.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "worldgen/history/names.hpp"
#include "worldgen/history/renaming.hpp"
#include "worldgen/inspect/history_report.hpp"

namespace wh = worldgen::history;

TEST_CASE("p5-12 engineer: fused tails and doubled suffixes") {
    const std::vector<std::string> forms = {"{root}", "{root}ford", "{root}ton", "{root}bury",
                                            "{root}wick", "{root}ham", "{root}stead"};
    const std::vector<std::string> tails = wh::fused_tails(forms);
    CHECK(tails == std::vector<std::string>{"ford", "ton", "bury", "wick", "ham", "stead"});
    CHECK(wh::fused_tails({"{root} Ford", "{root}ford"}) == std::vector<std::string>{"ford"});
    CHECK(wh::tail_first_syllable("bury") == "bur");
    CHECK(wh::tail_first_syllable("stead") == "stead");
    CHECK(wh::tail_first_syllable("ford") == "ford");
    CHECK(wh::tail_first_syllable("dell") == "dell");
    CHECK(wh::doubled_suffix("sudstead", "stead", tails));
    CHECK(wh::doubled_suffix("galwick", "wick", tails));
    CHECK(wh::doubled_suffix("ston", "ton", tails));
    CHECK(wh::doubled_suffix("dabur", "bury", tails));
    CHECK(wh::doubled_suffix("canasham", "ford", tails));
    CHECK(wh::doubled_suffix("deas", "stead", tails));
    CHECK(wh::doubled_suffix("bant", "ton", tails));
    CHECK_FALSE(wh::doubled_suffix("sud", "stead", tails));
    CHECK_FALSE(wh::doubled_suffix("gal", "wick", tails));
    CHECK_FALSE(wh::doubled_suffix("bea", "ford", tails));
}

TEST_CASE("p5-12 engineer: V8 tests joins only") {
    wh::NameFilter f;
    f.anywhere = {"tard", "nig", "negr"};
    f.at_start = {"tarde", "slag"};
    f.whole = {"mark"};
    CHECK(wh::name_filtered_at_joins(f, "Tarden", {3}));
    CHECK(wh::name_filtered_at_joins(f, "Pinegrove", {4}));
    CHECK_FALSE(wh::name_filtered_at_joins(f, "Nightwood", {5}));
    CHECK_FALSE(wh::name_filtered_at_joins(f, "Knight's Rest", {6}));
    CHECK(wh::name_filtered_at_joins(f, "Tardeholt", {3}));
    CHECK_FALSE(wh::name_filtered_at_joins(f, "Slagwold", {4}));
    CHECK(wh::name_filtered_at_joins(f, "Mark", {2}));
    CHECK_FALSE(wh::name_filtered_at_joins(f, "Tarden", {}));
}

TEST_CASE("p5-12 engineer: realm roots and forms") {
    const std::vector<std::string> human = {"The Kingdom of {root}", "The {root} League", "The Duchy of {root}"};
    CHECK(wh::realm_root("The Kingdom of Cear", human) == "Cear");
    CHECK(wh::realm_root("The Cear League", human) == "Cear");
    CHECK(wh::realm_root("The Kingdom of Cear II", human) == "Cear II");
    CHECK(wh::realm_root("Clan Uk'ghaam", human) == "Clan Uk'ghaam");
    const wh::RenamedName a = wh::fill_rename_form("{name}land", "Tar", "Wood");
    CHECK(a.name == "Tarland");
    CHECK(a.joins == std::vector<std::size_t>{3});
    const wh::RenamedName b = wh::fill_rename_form("The {noun} of the Slain", "", "Heath");
    CHECK(b.name == "The Heath of the Slain");
    CHECK(b.joins == std::vector<std::size_t>{4, 9});
    CHECK(wh::renaming_cap(12, 150) == 1);
    CHECK(wh::renaming_cap(49, 150) == 7);
    CHECK(wh::renaming_cap(200, 150) == 30);
}

TEST_CASE("p5-12 engineer: settlement_row") {
    const std::vector<std::int64_t> v = {7, 8, 9};
    const std::span<const std::int64_t> s(v);
    CHECK(worldgen::inspect::settlement_row(s, 0) == nullptr);
    CHECK(*worldgen::inspect::settlement_row(s, 1) == 7);
    CHECK(*worldgen::inspect::settlement_row(s, 3) == 9);
    CHECK(worldgen::inspect::settlement_row(s, 4) == nullptr);
}
