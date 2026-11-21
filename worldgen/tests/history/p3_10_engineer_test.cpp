// Engineer-side checks of the P3-10 change (generator 0.19.0), written by the
// generator engineer while implementing; the change's tests proper are the
// test writer's (docs/specs/p3-10-followups.md section 7).

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <vector>

#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/names.hpp"
#include "worldgen/stages/regions.hpp"

namespace {

namespace wh = worldgen::history;
namespace ws = worldgen::stages;

const wh::KindDef& kind(std::string_view key) {
    const wh::KindDef* k = wh::compiled_event_catalogue().find(key);
    REQUIRE(k != nullptr);
    return *k;
}

}  // namespace

TEST_CASE("P3-10 engineer: kinds 37 and 38 (P3-04 r12 20.4)") {
    const wh::KindDef& outbreak = kind("plague_outbreak");
    CHECK(outbreak.id == 37);
    CHECK(outbreak.values == std::vector<std::string>{"dead", "population", "total", "reached"});
    const wh::KindDef& stretch = kind("quiet_stretch");
    CHECK(stretch.id == 38);
    CHECK(stretch.region_use == 0);
    CHECK(stretch.settlement_use == 0);
    REQUIRE(stretch.roles.size() == 1U);
    CHECK(stretch.roles[0].role == "realm");
    CHECK(stretch.roles[0].min_count == 1);
    CHECK(stretch.roles[0].max_count == 0);
}

TEST_CASE("P3-10 engineer: the {@causes:...} grammar (P3-04 r12 20.2)") {
    const wh::KindDef& k = kind("settlement_abandoned");
    CHECK_FALSE(wh::template_problem(k, "{@causes:famine=famine,plague=plague|Weakened by |, }{settlement} fell."));
    CHECK_FALSE(wh::template_problem(k, "{@causes:no_such_kind=x||}{settlement}."));  // keys need not be kinds
    CHECK_FALSE(wh::template_problem(k, "{@causes:famine=a b|x|}{settlement}."));
    for (const char* bad : {"{@causes:|a|b}", "{@causes:famine=|a|b}", "{@causes:Famine=x|a|b}",
                            "{@causes:famine=x,famine=y|a|b}", "{@causes:famine=x|a}", "{@causes:famine=x|a|b|c}",
                            "{@causes:famine=x,|a|b}", "{@causes:famine|a|b}", "{@causes:famine=x?|a|b}",
                            "{@causes:famine=x|a?|b}"}) {
        const std::optional<std::string> why = wh::template_problem(k, bad);
        REQUIRE(why.has_value());
        CHECK(why->rfind("unknown placeholder '{", 0) == 0);
    }
}

TEST_CASE("P3-10 engineer: name cores (P3-02 r10 12.2)") {
    CHECK(wh::name_core("skat") == "skat");
    CHECK(wh::name_core("gork-skreeg") == "gorkskreeg");
    CHECK(wh::name_core("ka'zz") == "kazz");
    CHECK(wh::within_one_edit("skat", "skat"));
    CHECK(wh::within_one_edit("slig", "sligg"));
    CHECK(wh::within_one_edit("sligg", "slig"));
    CHECK(wh::within_one_edit("kuz", "kaz"));
    CHECK(wh::within_one_edit("kuz", "ku"));
    CHECK(wh::within_one_edit("kuz", "akuz"));
    CHECK_FALSE(wh::within_one_edit("kuz", "kazz"));
    CHECK_FALSE(wh::within_one_edit("nili", "muli"));
    CHECK_FALSE(wh::within_one_edit("ab", "ba"));
    wh::NameCores cores;
    cores.add("kuz");
    CHECK(cores.near("kuzz"));
    CHECK(cores.near("kaz"));
    CHECK(cores.near("kuz"));
    CHECK_FALSE(cores.near("kazz"));
}

TEST_CASE("P3-10 engineer: V7 stems (P1-06 r6 15.4)") {
    const ws::PlaceSlotValues values = {{3, "Flooded"}, {0, "Weald"}, {7, "the Floods"}, {1, "holt"}, {2, "Ash"}};
    CHECK(ws::region_name_stems(values) == std::vector<std::string>{"flood", "flood", "ash"});
}
