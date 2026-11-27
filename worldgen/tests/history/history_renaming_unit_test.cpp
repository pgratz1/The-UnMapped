// Spec P3-04 r21 23.2 and 23.5 items 2 and 3 (P5-12, generator 0.22.0): kind 43 region_renamed in the compiled
// catalogue and the shipped catalogue (caused, region always, settlement optional, role 1 namer civilization
// 0-1, no value, salience 60 and the template of P5-12 7.1); template_problem accepts {@region_former} only in a
// kind with region_use 1 (the message of {@region}'s form). The rules of section 6 (windows, chance keys,
// choice, caps, quiet budget, names, the written event) are exercised on hand-made histories by the reference
// (reference/p5_12_renaming_reference.py check: check_units, check_subsystem_units) and on the golden worlds and
// fixtures by history_renaming_golden_test.cpp; the public seam has no way to feed a hand-made log to the
// subsystem. Written from the specs before the implementation (test-writer rule).

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>

#include "p3_04_engine_support.hpp"

namespace {

using namespace engine_test;

std::string roles_text(const eng::KindDef& k) {
    std::string out;
    for (const auto& r : k.roles) {
        out += std::to_string(r.seq) + ":" + r.role + ":" + std::to_string(static_cast<int>(r.type)) + ":" +
               std::to_string(r.min_count) + "-" + std::to_string(r.max_count) + ";";
    }
    return out;
}

const char* const KIND43_TEMPLATE =
    "{@causes:civ_conquered=the fall of a realm,sacked=the sack,battle=the slaughter,battle_at_pass=the "
    "slaughter,allied_battle=the slaughter,battle_led=the slaughter,plague_outbreak=the plague,became_city=the rise of a realm's seat,settlement_overrun=the ravage,hold_lost=the fall of a hold to the deep|After |, "
    "}{@region_former} became known as "
    "{@region}.";

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 r21 23.2: kind 43 region_renamed in the compiled and the shipped catalogue") {
    const auto& c = eng::compiled();
    REQUIRE(c.kinds.size() == 110U)  /* P5-04 8.1 (0.26.0): kinds 63 to 78 */;   // P5-02 10.1
    const auto& k = c.kinds[42];
    CHECK(k.id == 43);
    CHECK(k.key == "region_renamed");
    CHECK(k.caused);
    CHECK(k.region_use == 1);
    CHECK(k.settlement_use == 2);
    CHECK(roles_text(k) == "1:namer:3:0-1;");
    CHECK(k.values.empty());
    const auto cat = eng::catalogue_of(shipped());
    const auto* s = cat.find("region_renamed");
    REQUIRE(s != nullptr);
    CHECK(s->salience == 60);
    CHECK(s->templ == KIND43_TEMPLATE);
    CHECK_FALSE(eng::template_problem(*s, s->templ).has_value());
}

TEST_CASE("P3-04 r21 23.1, 23.5 item 2: {@region_former} needs a kind with a region") {
    const auto cat = eng::catalogue_of(shipped());
    const auto* tribute = cat.find("tribute");      // region_use 0
    const auto* claim = cat.find("claim");          // region_use 1
    const auto* k43 = cat.find("region_renamed");
    REQUIRE(tribute != nullptr);
    REQUIRE(claim != nullptr);
    REQUIRE(k43 != nullptr);
    CHECK(eng::template_problem(*tribute, "{payer} paid {@region_former}.") ==
          std::optional<std::string>("{@region_former} needs a kind with a region at byte 13"));
    CHECK_FALSE(eng::template_problem(*claim, "{claimant} claimed {@region_former}, now {@region}.").has_value());
    CHECK_FALSE(eng::template_problem(*k43, "{@region_former} and {namer:all}.").has_value());
    // {namer} alone: a role that may have no participant (min 0)
    CHECK(eng::template_problem(*k43, "{namer} named {@region}.") ==
          std::optional<std::string>("role 'namer' may have no participant at byte 0"));
    CHECK(eng::template_problem(*k43, "{@region_formerly} was renamed.") ==
          std::optional<std::string>("unknown placeholder '{@region_formerly}' at byte 0"));
    // a kind with region_use 2 (optional): the test kind of the engine support
    eng::KindDef opt = *claim;
    opt.region_use = 2;
    CHECK(eng::template_problem(opt, "x {@region_former}") ==
          std::optional<std::string>("{@region_former} needs a kind with a region at byte 2"));
}

}  // TEST_SUITE
