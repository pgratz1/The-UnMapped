// Spec P3-07 r17 (section 24, readability) on the fixtures H8 frames, H9 realms and H10 floor
// (worldgen/tests/data/history-report/), against lines written here from the fixture rows: the war of an event
// and the war frames of 24.1, the units and summary set of 24.2 and 24.3, the blocks of 24.4 to 24.7 (secessions,
// the wars line of every outcome, adversaries, milestones, [ages], ruins, refuges, open wars, standing claims).
// 24.11 item 2 (civs4 orphans) is in history_report_worlds_test.cpp; item 11 (the invariants on every fixture and
// world) in history_report_fixtures_test.cpp and history_report_worlds_test.cpp.
//
// Written from the spec only. Reading R10 (the test writer's report): a sack below salience 20, shown only as part 4
// of a consequence's unit, does not bring its own consequences (24.2: "Companions are not followed further"), so
// 24.10 item 1's third rule is read for the other sacks.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "history_report_checks.hpp"
#include "history_report_fixture.hpp"

namespace {

using hrtest::block;
using hrtest::civ_block;
using hrtest::contains_line;
using hrtest::HistoryLevel;

std::string text(const std::string& fixture, HistoryLevel level) {
    static std::map<std::pair<std::string, int>, std::string> cache;
    const auto key = std::make_pair(fixture, static_cast<int>(level));
    const auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    return cache[key] = hrtest::report_of(hrtest::fixture_world(fixture), level);
}
std::string summary(const std::string& f) { return text(f, HistoryLevel::summary); }
std::string full(const std::string& f) { return text(f, HistoryLevel::full); }

std::set<std::int64_t> range(std::int64_t a, std::int64_t b) {
    std::set<std::int64_t> out;
    for (std::int64_t i = a; i <= b; ++i) out.insert(i);
    return out;
}

// The lines of a civilization block from `head` (exclusive) to the next line not indented by four spaces.
std::vector<std::string> sub_lines(const std::vector<std::string>& civ, const std::string& head) {
    std::vector<std::string> out;
    bool in = false;
    for (const std::string& l : civ) {
        if (l.rfind(head, 0) == 0) {
            in = true;
            continue;
        }
        if (in && l.rfind("    ", 0) != 0) break;
        if (in) out.push_back(l);
    }
    return out;
}

}  // namespace

TEST_SUITE("history-report") {

TEST_CASE("P3-07 r17 24.1, 24.11 item 1 on H8: war(e) for every event and the last battles") {
    const auto r = hrtest::open_world(hrtest::fixture_world("h8-frames"));
    const hrtest::Wars w(r);
    // war A: e16 (three battles e17-e19, siege, sack, consequences, peace, tribute); war B: e26 (battles e27, e28,
    // conquest e29); war C: e30 (battle, siege, sack, consequences, peace); war D: e37 (siege, search)
    std::map<std::int64_t, std::int64_t> want;
    for (const std::int64_t e : range(16, 25)) want[e] = 16;
    for (const std::int64_t e : range(26, 29)) want[e] = 26;
    for (const std::int64_t e : range(30, 36)) want[e] = 30;
    for (const std::int64_t e : range(37, 39)) want[e] = 37;
    for (std::int64_t e = 1; e <= 39; ++e) {
        CAPTURE(e);
        const auto got = w.war(e);
        if (want.count(e) != 0U) {
            REQUIRE(got.has_value());
            CHECK(*got == want.at(e));
        } else {
            CHECK_FALSE(got.has_value());  // e15: a capital moved after an abandonment has no war
        }
    }
    CHECK(w.last_battle(16) == std::optional<std::int64_t>(19));
    CHECK(w.last_battle(26) == std::optional<std::int64_t>(28));
    CHECK(w.last_battle(30) == std::optional<std::int64_t>(31));
    CHECK_FALSE(w.last_battle(37).has_value());
    CHECK(w.excluded(37));
    CHECK_FALSE(w.excluded(16));
}

TEST_CASE("P3-07 r17 24.2, 24.3, 24.11 items 1 and 6 on H8: units, the summary set, battles at both levels") {
    const std::string s = summary("h8-frames");
    const std::set<std::int64_t> shown = hrtest::shown_ids(s);
    std::set<std::int64_t> want = range(1, 13);
    for (const std::int64_t e : {15, 16, 19, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 33, 35, 36, 38}) want.insert(e);
    CHECK(shown == want);
    // the sack e21 (75) brings its consequences e22, e23 and its war's frame: the declaration e16, the last battle
    // e19, the ending e24 and its tribute e25; not the battles e17, e18 nor the siege e20
    for (const std::int64_t e : {17, 18, 20}) CHECK(shown.count(e) == 0U);
    // the refugees e35 (65), alone a candidate in war C, bring their sack e33 and its war's frame e30, e31, e36, but
    // not the sack's other consequence e34 nor the siege e32
    CHECK(shown.count(34) == 0U);
    CHECK(shown.count(32) == 0U);
    // the capital moved after an abandonment (e15) brings nothing: the abandonment e14 is not shown
    CHECK(shown.count(14) == 0U);
    // war D ended in a repeated search (salience 15, 22.5a): its siege e38 (25) is shown, its declaration is not
    // brought in
    CHECK(shown.count(37) == 0U);
    CHECK(shown.count(39) == 0U);
    // B: the conquest brings the declaration's last battle e28, not e27
    CHECK(shown.count(27) == 0U);
    CHECK(block(s, "[chronicle]")[2] == "shown = 30");
    // every battle at full (24.11 item 6)
    const std::set<std::int64_t> all = hrtest::shown_ids(full("h8-frames"));
    for (const std::int64_t e : {17, 18, 19, 27, 28, 31}) CHECK(all.count(e) == 1U);
    const hrtest::Wars w(hrtest::open_world(hrtest::fixture_world("h8-frames")));
    CHECK(w.orphans(shown) == 0);
}

TEST_CASE("P3-07 r17 24.7 with r19 25.3, 24.11 item 10 on H8: ruins by sack and by abandonment, refuges") {
    const std::vector<std::string> p = block(summary("h8-frames"), "[present]");
    // P3-07 r21 26.3 (v3): no refuge_hidden in this fixture, so both refuges are counted and none is listed
    const std::vector<std::string> tail(p.end() - 8, p.end());
    CHECK(tail == std::vector<std::string>{
                      "ruins: 3",
                      "  s3 Branfield in Bran Downs (rg2): ruined 1005, e21 sacked by Alird (c1); last held by "
                      "Brannock (c2)",
                      "  s7 Dunwick in Dun Moor (rg4): ruined 1203, e33 sacked by Alird (c1); last held by Dunmark (c4)",
                      "  s9 Old Alird in Alird Vale (rg1): ruined 500, e14 abandoned; last held by Alird (c1)",
                      "hidden refuges: 0 of 2 founded by flight",
                      "open wars: 0",
                      "standing claims: 0",
                      "renamed regions: 0"});
    CHECK(contains_line(block(full("h8-frames"), "[present]"),
                        "  s9 Old Alird: hamlet, ruined, population 0, no owner, at 90 90 in Alird Vale (rg1), founded 0 "
                        "by Alird (c1), ruined 500, e14, last held by Alird (c1)"));
}

TEST_CASE("P3-07 r17 24.6, 24.11 item 9 on H9: [ages] separates awakenings and secessions") {
    CHECK(block(summary("h9-realms"), "[ages]") ==
          std::vector<std::string>{
              "a1 The Age of Humans: 0-1499, 1500 years; Humans (ra1) made by Wealstuld (g1, neutral); 4 civilizations "
              "founded: 4 at the awakening, 0 by secession",
              "a2 The Age of Orcs: 1500-2999, 1500 years; Orcs (ra2) made by Kruu (g2, evil); 4 civilizations founded: "
              "1 at the awakening, 3 by secession"});
}

TEST_CASE("P3-07 r17 24.5, 24.11 items 7 and 8 on H9: the wars line, adversaries, milestones") {
    const std::string s = summary("h9-realms");
    const std::vector<std::string> c1 = civ_block(s, 1);
    // one war of each outcome: won by peace (W1), by war_won (W2), by conquest (W3); lost (W4); truce (W5), search
    // (W6), a third party's conquest of the defender (W7): drawn; open (W8)
    CHECK(contains_line(c1, "  wars: 7 declared, 1 defended; won 3, lost 1, drawn 3, open 1"));
    // adversaries by (first since, entity type, id): c2, c3 and f1 all from 300; every state word. r19 25.4: the
    // adversaries without a war row (f1: none, Mog: enmity) are one count line after the war lines; 25.7 item 1: a
    // closed war's last year is its until (the year of its ending event)
    CHECK(sub_lines(c1, "  adversaries: 7") ==
          std::vector<std::string>{
              "    Brannock (c2): 3 wars, won 1, lost 1, drawn 1, open 0, 300-1310; now at peace",
              "    Cendrel (c3): 2 wars, won 1, lost 0, drawn 1, open 0, 300-510; now enmity",
              "    Dunmark (c4): 1 war, won 1, lost 0, drawn 0, open 0, 1200-1210; now none",
              "    Skab (c6): 1 war, won 0, lost 0, drawn 0, open 1, 2600-2999; now at war",
              "    Grork (c5): 1 war, won 0, lost 0, drawn 1, open 0, 2700-2800; now none"});
    // (P5-03a 9.2, v6: no `without war:` line for f1 and Mog)
    // milestones: no quiet stretch; four sacks of salience 75 give two (e17, e18: ties broken by id); two
    // declarations of 70 (e15, e16) of four; then the two endings of 60 and the conquest. P5-03a 9.2 (v6): the list
    // at full, its count at summary (no quiet stretch: no line)
    // P5-04a 12.2 (v9): n = the own events of salience 40 or more that are no quiet stretch, then the three weightiest
    CHECK(contains_line(c1, "  milestones: 16"));
    CHECK(sub_lines(c1, "  milestones:") ==
          std::vector<std::string>{
              // P5-07 10 item 6 (v10): at most one per kind, the first in rank order (the declaration e15, a sack e17)
              "    300 e15 war_declared: Alird declared war on Brannock over Bran Downs. (Claimed the same year.)",
              "    305 e17 sacked: Alird sacked Branmoor of Brannock; 100 died and 0 fled.",
              "    1210 e33 civ_conquered: Dunmark fell to Alird."});
    CHECK(sub_lines(civ_block(full("h9-realms"), 1), "  milestones:") ==
          std::vector<std::string>{
              "    0 e2 civ_founded: The Humans founded Alird in Alird Vale.",
              "    300 e15 war_declared: Alird declared war on Brannock over Bran Downs. (Claimed the same year.)",
              "    300 e16 war_declared: Alird declared war on Cendrel over Cen Wold.",
              "    305 e17 sacked: Alird sacked Branmoor of Brannock; 100 died and 0 fled.",
              "    306 e18 sacked: Alird sacked Branlea of Brannock; 100 died and 0 fled.",
              "    320 e19 peace: Brannock sued Alird for peace after 20 years of war.",
              "    320 e20 war_won: Alird prevailed over Cendrel after 20 years of war.",
              "    1210 e33 civ_conquered: Dunmark fell to Alird."});
    // the other side of the same rows
    CHECK(contains_line(civ_block(s, 2), "  wars: 1 declared, 2 defended; won 1, lost 1, drawn 1, open 0"));
    CHECK(contains_line(civ_block(s, 2), "    Alird (c1): 3 wars, won 1, lost 1, drawn 1, open 0, 300-1310; now at peace"));
    CHECK(contains_line(civ_block(s, 3), "  wars: 0 declared, 2 defended; won 0, lost 1, drawn 1, open 0"));
    // the longest quiet stretch with its generations note: c2 the 100-year one (4 generations), not the other;
    // c3 its only one (no new generation)
    // P5-04a 12.2 (v9): no quiet-stretch line at summary; the stretches stay at full (and in the chronicle)
    CHECK(sub_lines(civ_block(full("h9-realms"), 2), "  milestones:").size() > 0U);
    CHECK_FALSE(contains_line(civ_block(s, 2),
                        "    2300-2399 e50 quiet_stretch: For 100 years, from 2300 to 2399, the land was quiet; one realm "
                        "held it. (Brannock: 4 new generations of rulers.)"));
    CHECK_FALSE(contains_line(civ_block(s, 2),
                              "    2200-2209 e49 quiet_stretch: For 10 years, from 2200 to 2209, the land was quiet, and "
                              "two realms kept the peace. (Brannock: no new generation of rulers.)"));
    CHECK_FALSE(contains_line(civ_block(s, 3),
                        "    2200-2209 e49 quiet_stretch: For 10 years, from 2200 to 2209, the land was quiet, and two "
                        "realms kept the peace. (Cendrel: no new generation of rulers.)"));
    // seceded from; last capital on ended realms; a realm with fewer than eight events
    const std::vector<std::string> c6 = civ_block(s, 6);
    REQUIRE(c6.size() > 5U);
    CHECK(c6[4] == "  seceded from: Grork (c5), e39");
    CHECK(c6[5] == "  capital: Deep Hole (s11)");
    CHECK(civ_block(s, 8)[4] == "  seceded from: Skab (c6), e44");
    // r19 25.5: an ended block at summary has no home line, so `last capital` is its fourth line
    CHECK(civ_block(s, 4)[3] == "  last capital: none");
    CHECK(civ_block(s, 5)[3] == "  last capital: Grorkden (s9)");
    CHECK(civ_block(full("h9-realms"), 4)[4] == "  last capital: none");
    // the ended Dunmark: its milestones (founding, ending, then its own deeds by salience) at full; P5-03a 9.2 (v6):
    // at summary the count of the full list ("an ended realm's n counts its full list") and no line
    const std::vector<std::string> f4 = sub_lines(civ_block(full("h9-realms"), 4), "  milestones:");
    for (const char* l : {"    0 e8 civ_founded: The Humans founded Dunmark in Dun Moor.",
                          "    0 e9 settlement_founded: Dunmark founded Dunwick in Dun Moor.",
                          "    800 e29 settlement_captured: Dunmark took Branford from Cendrel; 0 died.",
                          "    1210 e33 civ_conquered: Dunmark fell to Alird."}) {
        CHECK(contains_line(f4, l));
    }
    CHECK(contains_line(civ_block(s, 4), "  milestones: 2"));   // P5-04a 12.2 (v9)
    CHECK(sub_lines(civ_block(s, 4), "  milestones:") ==
          std::vector<std::string>{"    0 e9 settlement_founded: Dunmark founded Dunwick in Dun Moor.",
                                   "    800 e29 settlement_captured: Dunmark took Branford from Cendrel; 0 died."});
    CHECK(sub_lines(civ_block(full("h9-realms"), 8), "  milestones:") ==
          std::vector<std::string>{"    1800 e44 civ_seceded: Ruk broke away from Skab with one settlement.",
                                   "    1800 e45 settlement_founded: Ruk founded Rukhold in Ruk Crag.",
                                   "    2500 e51 war_declared: Mog declared war on Ruk over Ruk Crag."});
    CHECK(contains_line(civ_block(s, 8), "  milestones: 1"));   // P5-04a 12.2 (v9)
    // the relation rows are kept at full, the adversary lines not
    const std::vector<std::string> f1 = civ_block(full("h9-realms"), 1);
    CHECK(contains_line(f1, "  relations: 12 (enmity 2, pact 1, peace 1, war 8)"));
    CHECK(std::find_if(f1.begin(), f1.end(), [](const std::string& l) { return l.rfind("  adversaries:", 0) == 0; }) ==
          f1.end());
}

TEST_CASE("P3-07 r17 24.3 item 7, 24.7, 24.11 items 3 and 10 on H9: claims, ruins, a refuge, open wars") {
    const std::string s = summary("h9-realms");
    // no claim at summary; every claim at full
    for (const std::int64_t claim : {14, 21, 24, 27, 46, 47, 48, 55}) {
        CAPTURE(claim);
        CHECK(hrtest::shown_ids(s).count(claim) == 0U);
        CHECK(hrtest::shown_ids(full("h9-realms")).count(claim) == 1U);
    }
    const std::vector<std::string> p = block(s, "[present]");
    const auto at = std::find(p.begin(), p.end(), "ruins: 5");
    REQUIRE(at != p.end());
    CHECK(std::vector<std::string>(at, p.end()) ==
          std::vector<std::string>{
              "ruins: 5",
              "  s3 Branmoor in Bran Downs (rg2): ruined 305, e17 sacked by Alird (c1); last held by Brannock (c2)",
              "  s4 Branlea in Bran Downs (rg2): ruined 306, e18 sacked by Alird (c1); last held by Brannock (c2)",
              // captured by Cendrel, then by Dunmark, then sacked: last held by Dunmark
              "  s5 Branford in Bran Downs (rg2): ruined 1205, e32 sacked by Alird (c1); last held by Dunmark (c4)",
              "  s7 Cenwick in Cen Wold (rg3): ruined 900, e30 abandoned; last held by Cendrel (c3)",
              "  s10 Skabpit in Skab Hollow (rg6): ruined 2650, e53 sacked by Alird (c1); last held by Skab (c6)",
              "hidden refuges: 0 of 1 founded by flight",   // r21 26.3: no refuge_hidden in this fixture
              "open wars: 2",
              "  e51 Mog (c7) against Ruk (c8) over Ruk Crag (rg8), since 2500",
              "  e52 Alird (c1) against Skab (c6) over Skab Hollow (rg6), since 2600",
              // pursued (e14), closed by a later war in the same direction (e21) and in the other (e24), a side
              // ended (e27), replaced by a later claim of the pair (e46): none listed; e47 (kind 39, two
              // settlements), e48 (the reverse pair, which replaces nothing) and e55 stand
              "standing claims: 3",
              "  e47 2000 Skab (c6) on Mog (c7): Mog Heath (rg7), for Mogfort (s12), Mogbrook (s13)",
              "  e48 2100 Mog (c7) on Skab (c6): Skab Hollow (rg6)",
              "  e55 2700 Cendrel (c3) on Brannock (c2): Bran Downs (rg2)",
              "renamed regions: 0"});   // r21 26.2
}

TEST_CASE("P3-07 r17 24.3 item 3, 24.11 item 5 on H10: the age floor takes the age's own race first") {
    const std::string s = summary("h10-floor");
    CHECK(hrtest::stats_of(s).at("summary_threshold") == 91);   // a SUMMARY_STATS key (r19 25.7 item 4)
    std::set<std::int64_t> want = {1, 2, 3, 4, 5, 33, 34};
    for (const std::int64_t e : range(18, 32)) want.insert(e);  // the fifteen raids of the age's race, then 3 others
    for (const std::int64_t e : range(35, 52)) want.insert(e);  // the floor of the orc age: 18 chants
    CHECK(hrtest::shown_ids(s) == want);
}

}  // TEST_SUITE
