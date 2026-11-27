// Spec P5-04a (docs/specs/p5-04a-relations-alliances-followups.md r2) test 16 items 2 to 8, 10 and 15 on
// hand-checked values: large shares at the bounds +- 1 (3.3), each factor of a call alone with its motive, ties to
// the smaller code, motive 0 only with every factor at 1000, the excused codes and the chance with floors at each
// step (4.2, 4.3), the betrayal's motives in order at their bounds (5), the renewal chain and the report's count with
// a closing in a due year's step (6.1, 6.3), each turn's change and the cool-down at turn_cooldown_years - 1 and at
// it with and without a fresh contribution (7), expulsions held at expel_settled_years - 1 and at it with the hatred
// turn inside and outside expel_fresh_years (8), tower spacing at the bound - 1 and at it (21.4), hero_rose's quiet
// at hero_return_years - 1 and at it (21.5), the eight-word choice (8.5). These check the functions of
// p5_04a_checks.hpp (the invariants' arithmetic) and run before integration; the same values are in the reference's
// check_units_028 (p5_04_alliances_reference.py units).
//
// With UNMAPPED_P504A_API (integration): the compiled catalogue's ten value rows (14.4), template_problem of the
// choice of 2 to 8 words and 9 words, and of {m:died} (8.5), the generator version. (16 item 15, `the Old` at
// reign_permille 1479 and 1480 with two earlier bearers, is in the reference's units, p5_06_figures_reference.py
// units, with P5-06's epithet_of; its data are checked in history_alliances_p504a_data_test.cpp.)

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "p5_04a_api.hpp"
#include "p5_04a_checks.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/version.hpp"

namespace {

namespace h = worldgen::history;
using p504a_f::factors;
using p504a_f::motive;

const p504a_f::JoinTuning T{};   // the shipped values of 4.4: 600, 250, 300, 400, 600, 500, soured 500

std::array<std::int64_t, 8> f_of(std::int64_t join = 1000, bool weary = false, std::int64_t wars = 0,
                                 std::int64_t sigma = 1000, std::int64_t warmth = 1000, std::int64_t temper = -1,
                                 bool claim = false, bool soured = false) {
    return factors(T, join, weary, wars, sigma, warmth, temper, claim, soured);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-04a 3.3: large by share and by part at the bound +- 1; part 1001 never") {
    CHECK_FALSE(p504a_f::large(599, 299, 600, 300));
    CHECK(p504a_f::large(600, 0, 600, 300));
    CHECK(p504a_f::large(601, 0, 600, 300));
    CHECK(p504a_f::large(1, 300, 600, 300));
    CHECK_FALSE(p504a_f::large(1, 299, 600, 300));
    CHECK_FALSE(p504a_f::large(1, 1000, 1000000, 1001));   // 14.1's code defaults: never large
    CHECK(p504a_f::large(1, 0, 1, 1001));                  // alliances-small: spoils_large_share 1
}

TEST_CASE("P5-04a 4.2: each factor alone gives its motive; motive 0 only with every factor at 1000") {
    CHECK(f_of() == std::array<std::int64_t, 8>{1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000});
    CHECK(motive(f_of()) == 0);
    CHECK(motive(f_of(200)) == 0);   // f0 (its people's reluctance) is never a named motive
    CHECK(f_of(1000, true)[1] == 600);
    CHECK(motive(f_of(1000, true)) == 1);
    CHECK(f_of(1000, false, 1)[2] == 750);
    CHECK(motive(f_of(1000, false, 1)) == 2);
    CHECK(f_of(1000, false, 5)[2] == 0);                 // max(0, ...)
    CHECK(f_of(1000, false, 0, 0)[3] == 300);            // join_fear_floor
    CHECK(f_of(1000, false, 0, 500)[3] == 650);          // 300 + 700 x 500 div 1000
    CHECK(motive(f_of(1000, false, 0, 0)) == 3);
    CHECK(f_of(1000, false, 0, 1000, 0)[4] == 400);      // join_warmth_floor
    CHECK(f_of(1000, false, 0, 1000, 999)[4] == 999);    // 400 + 600 x 999 div 1000 = 999
    CHECK(motive(f_of(1000, false, 0, 1000, 999)) == 4);
    CHECK(f_of(1000, false, 0, 1000, 1000, 3)[5] == 600);
    CHECK(f_of(1000, false, 0, 1000, 1000, 1)[5] == 1000);   // warlike: no factor
    CHECK(f_of(1000, false, 0, 1000, 1000, -1)[5] == 1000);  // no reign
    CHECK(motive(f_of(1000, false, 0, 1000, 1000, 3)) == 5);
    CHECK(f_of(1000, false, 0, 1000, 1000, -1, true)[6] == 500);
    CHECK(motive(f_of(1000, false, 0, 1000, 1000, -1, true)) == 6);
    CHECK(f_of(1000, false, 0, 1000, 1000, -1, false, true)[7] == 500);
    CHECK(motive(f_of(1000, false, 0, 1000, 1000, -1, false, true)) == 7);
}

TEST_CASE("P5-04a 4.2: ties go to the smaller code; the smallest factor names the motive") {
    CHECK(motive(f_of(1000, true, 0, 1000, 1000, 3)) == 1);                 // f1 = f5 = 600
    CHECK(motive(f_of(1000, false, 0, 1000, 1000, -1, true, true)) == 6);   // f6 = f7 = 500
    CHECK(motive(f_of(1000, true, 0, 0)) == 3);                             // f3 300 under f1 600
    CHECK(motive(f_of(1000, true, 1, 1000, 1000, 3, true, true)) == 6);     // 600, 750, 600, 500, 500
}

TEST_CASE("P5-04a 4.2: the chance is floored at each of the eight steps; 4.3: motives 1 and 2 are excused") {
    const auto f = f_of(500, true, 0, 500);
    CHECK(p504a_f::chance(1000, f) == 1000 * 500 / 1000 * 600 / 1000 * 650 / 1000);
    CHECK(p504a_f::chance(1000, f) == 195);
    CHECK(p504a_f::chance(999, f_of(333)) == 332);   // 999 x 333 div 1000 = 332, then x 1000 div 1000 seven times
    for (std::int64_t m = 0; m < 8; ++m) {
        CAPTURE(m);
        CHECK(p504a_f::excused(m) == (m == 1 || m == 2));
    }
}

TEST_CASE("P5-04a 5: the betrayal's motive, the first that holds, at its bounds") {
    CHECK(p504a_f::betrayal_motive(3, -500, 100, 10, 0, 500) == 0);   // grasping before everything
    CHECK(p504a_f::betrayal_motive(1, -1, 100, 10, 0, 500) == 1);     // d < betray_cooled_disposition 0
    CHECK(p504a_f::betrayal_motive(1, 0, 100, 100, 0, 500) == 3);     // d = 0 is not cooled
    CHECK(p504a_f::betrayal_motive(-1, 10, 100, 49, 0, 500) == 2);    // 49 000 < 50 000
    CHECK(p504a_f::betrayal_motive(-1, 10, 100, 50, 0, 500) == 3);    // 50 000 = 50 000: land
    CHECK(p504a_f::betrayal_motive(-1, 10, 100, 1, 0, 0) == 3);       // code default betray_weak_permille 0
}

TEST_CASE("P5-04a 6.1, 6.3: the renewal chain does not drift; the report's count, a closing at a due year counts") {
    CHECK(p504a_f::due_years(480, 150, 1000) == std::vector<std::int64_t>{630, 780, 930});
    CHECK(p504a_f::due_years(480, 150, 629).empty());
    CHECK(p504a_f::renewal_count(480, 150, 2000) == 10);   // 630, 780, ..., 1980
    CHECK(p504a_f::renewal_count(480, 150, 631) == 1);     // closed in the step of its due year 630
    CHECK(p504a_f::renewal_count(480, 150, 630) == 0);     // closed the year before
    CHECK(p504a_f::renewal_count(480, 150, 480) == 0);
    CHECK(p504a_f::renewal_count(50, 150, 2000) == 12);    // H16's origin alliance, standing at E 2000
}

TEST_CASE("P5-04a 7.1: change for each band move; 7.2: the cool-down at 299 and at 300, with a fresh cause") {
    using p504a_f::FRIENDSHIP;
    using p504a_f::HATRED;
    using p504a_f::WARINESS;
    CHECK(p504a_f::turn_change(WARINESS, HATRED) == 0);
    CHECK(p504a_f::turn_change(HATRED, WARINESS) == 1);
    CHECK(p504a_f::turn_change(FRIENDSHIP, WARINESS) == 2);
    CHECK(p504a_f::turn_change(WARINESS, FRIENDSHIP) == 3);
    CHECK_FALSE(p504a_f::turn_allowed(1299, 1000, true, 300, false));
    CHECK(p504a_f::turn_allowed(1300, 1000, true, 300, false));
    CHECK(p504a_f::turn_allowed(1299, 1000, true, 300, true));
    CHECK(p504a_f::turn_allowed(10, 0, false, 300, false));   // reading R3: never turned, no cool-down
    CHECK(p504a_f::turn_allowed(1000, 1000, true, 0, false)); // relations-small: turn_cooldown_years 0
}

TEST_CASE("P5-04a 8: expulsions held at 799 and 800 years, a turn into hatred inside and outside 150 years") {
    CHECK(p504a_f::expel_allowed(2000, 1201, std::nullopt, 800, 150));   // held 799: the old rule
    CHECK_FALSE(p504a_f::expel_allowed(2000, 1200, std::nullopt, 800, 150));   // held 800, no turn (reading R4)
    CHECK(p504a_f::expel_allowed(2000, 1200, 1850, 800, 150));          // the turn at Y - 150
    CHECK_FALSE(p504a_f::expel_allowed(2000, 1200, 1849, 800, 150));    // one year too early
    CHECK(p504a_f::expel_allowed(2000, 0, std::nullopt, 100000, 0));    // code defaults: inert
}

TEST_CASE("P5-04a 21.4: D = permille x W div 1000; Chebyshev distance at D and D + 1; 21.5: quiet at 99 and 100") {
    CHECK(p504a_f::tower_spacing(30, 1024) == 30);
    CHECK(p504a_f::tower_spacing(30, 512) == 15);
    CHECK(p504a_f::tower_spacing(30, 2048) == 61);
    CHECK(p504a_f::tower_spacing(0, 1024) == 0);
    CHECK(p504a_f::chebyshev(100, 100, 130, 110) == 30);   // within D = 30: refused
    CHECK(p504a_f::chebyshev(100, 100, 131, 70) == 31);    // D + 1: allowed
    CHECK(p504a_f::hero_quiet(99, 100) == 0);
    CHECK(p504a_f::hero_quiet(100, 100) == 100);
    CHECK(p504a_f::hero_quiet(234, 100) == 234);
    CHECK(p504a_f::hero_quiet(0, 100) == 0);   // no earlier raid: g = 0
}

TEST_CASE("P5-04a 8.5: the choice of 2 to 8 words: each value, a value above n - 1, a negative value") {
    CHECK(p504a_f::choice_index(8, 0) == 0U);
    CHECK(p504a_f::choice_index(8, 6) == 6U);
    CHECK(p504a_f::choice_index(8, 7) == 7U);
    CHECK(p504a_f::choice_index(8, 8) == 7U);
    CHECK(p504a_f::choice_index(8, -1) == 7U);
    CHECK(p504a_f::choice_index(5, 4) == 4U);
    CHECK(p504a_f::choice_index(5, 9) == 4U);
    CHECK(p504a_f::choice_index(4, 3) == 3U);
    CHECK(p504a_f::choice_index(3, 1) == 1U);
    CHECK(p504a_f::choice_index(3, 2) == 2U);
    CHECK(p504a_f::choice_index(2, 1) == 0U);   // {years?year|years}: 1 year
    CHECK(p504a_f::choice_index(2, 5) == 1U);
}

TEST_CASE("P5-04a 14.4: the compiled catalogue appends ten values to seven kinds; header: generator 0.28.0") {
    P504A_REQUIRE_INTEGRATED();
    CHECK(worldgen::GENERATOR_VERSION == std::string_view(p504a_api::GENERATOR_SPEC));
    const h::EventCatalogue& c = h::compiled_event_catalogue();
    CHECK(static_cast<std::int64_t>(c.kinds.size()) == p504a_api::KINDS_SPEC);
    const std::vector<std::pair<std::string, std::vector<std::string>>> want = {
        {"disposition_turned", {"disposition", "band", "change"}},
        {"alliance_formed", {"basis", "term", "band"}},
        {"alliance_betrayed", {"motive", "years"}},
        {"ally_refused", {"motive", "count"}},
        {"spoils_withheld", {"share", "grievance"}},
        {"alliance_spurned", {"reason", "years"}},
        {"hero_rose", {"quiet"}}};
    for (const auto& [key, values] : want) {
        CAPTURE(key);
        const h::KindDef* k = c.find(key);
        REQUIRE(k != nullptr);
        CHECK(k->values == values);
    }
    CHECK(c.find("ally_refused")->id == 69);
    CHECK(c.find("alliance_spurned")->id == 76);
    CHECK(c.find("hero_rose")->id == 87);
}

TEST_CASE("P5-04a 8.5: template_problem of the choice of 2 to 8 words, of 9 words, and of {m:died}") {
    P504A_REQUIRE_INTEGRATED();
    const h::EventCatalogue& c = h::compiled_event_catalogue();
    const h::KindDef* refused = c.find("ally_refused");
    const h::KindDef* died = c.find("monster_died");
    REQUIRE(refused != nullptr);
    REQUIRE(died != nullptr);
    CHECK_FALSE(h::template_problem(*refused, "{motive?a|b}").has_value());
    CHECK_FALSE(h::template_problem(*refused, "{motive?a|b|c|d|e}").has_value());
    CHECK_FALSE(h::template_problem(*refused, "{motive?a|b|c|d|e|f|g|h}").has_value());
    CHECK_FALSE(h::template_problem(*refused, "{motive?a|b|c|d|e|f|g|h|i}").has_value());  // P5-08 12.3: up to nine
    CHECK(h::template_problem(*refused, "{motive?a|b|c|d|e|f|g|h|i|j}") ==
          std::optional<std::string>("choice of 10 words at byte 0"));
    CHECK(h::template_problem(*refused, "x {motive?a|b|c|d|e|f|g|h|i|j|k}") ==
          std::optional<std::string>("choice of 11 words at byte 2"));
    CHECK_FALSE(h::template_problem(*died, "{monster}{monster:aside} {monster:died}{monster:where}.").has_value());
    // P5-03a 7.1's monster forms' problem for a role that is not a monster
    CHECK(h::template_problem(*refused, "{ally:died}") ==
          std::optional<std::string>("form 'died' needs a monster role at byte 0"));
}

}  // TEST_SUITE
