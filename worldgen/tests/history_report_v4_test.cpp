// Spec P5-02 (docs/specs/p5-02-beasts-monsters.md r0) section 12 (the history report's format v4, generator
// 0.23.0), test 19.1 item 15 on the fixture H13 (worldgen/tests/data/history-report/h13-v4.toml), against lines
// written here from the fixture rows and the spec; the whole text is also compared with the reference's
// (p3-07-report.txt, history_report_fixtures_test.cpp, and here).
//
// The H13 rows (E = 2000): Old Grakh (m1, a troll) appears in Cor Fells in 500 (e15, lair si1), raids Dunton in
// 600 (e16: 30 dead, plunder 1), whose people leave it in 601 (e17, ending 5), and is slain by the Kingdom of Bel
// in 700 (e18, 25 hunters). Saelith the Black (m2, a dragon) appears in the quiet step of 975 (e20, lair si2),
// starts the flare-up of 1000 (e21, phase 3's trigger), raids Arnwick in 1300 (e22: 100 dead, plunder 32),
// overruns it (e23: 700 escaped; lair si3 at its cell; si2 ends 3 moved) and sleeps in 1400 (e24). Outposts si4
// (Arn Watch, open) and si5 (Bel Tower, left in 1700 when its frontier fell quiet, e33). Corford is abandoned after
// plague (e12, ending 3, survivors to Arnton), Belham after famine (e14, ending 4), Eldstead sacked in 1500 (e29,
// ending 1) in Eld Wood, which is renamed The Red Wood in 1600 (e32). The wolves grow bold in Bel Downs (e19).
//
// Written by the test writer from the spec before the implementation.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "history_report_checks.hpp"
#include "history_report_fixture.hpp"
#include "worldgen/inspect/history_report.hpp"

namespace {

using hrtest::block;
using hrtest::contains_line;
using hrtest::HistoryLevel;

std::string text(HistoryLevel level) {
    static std::map<int, std::string> cache;
    const auto it = cache.find(static_cast<int>(level));
    if (it != cache.end()) return it->second;
    return cache[static_cast<int>(level)] = hrtest::report_of(hrtest::fixture_world("h13-v4"), level);
}

std::string chronicle_line(const std::string& report, std::int64_t id) {
    const std::string tag = " e" + std::to_string(id) + " ";
    for (const std::string& l : hrtest::lines_of(report)) {
        const std::size_t sp = l.find(' ');
        if (sp != std::string::npos && l.compare(sp, tag.size(), tag) == 0) return l;
    }
    return "";
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-02 12.1: the header is `# worldgen history v4` at both levels; the text equals the reference's") {
    for (const HistoryLevel level : {HistoryLevel::summary, HistoryLevel::full}) {
        const std::string t = text(level);
        CHECK(hrtest::lines_of(t).front() == "# worldgen history v4");
        const std::string name = level == HistoryLevel::full ? "full" : "summary";
        CHECK(worldgen::to_hex(worldgen::sha256(t)) == hrtest::golden_case("h13-v4").report.at(name));
    }
}

TEST_CASE("P5-02 12.3: the ruins list: overrun by a monster, abandoned after plague or famine, then called") {
    const std::vector<std::string> ruins = block(text(HistoryLevel::full), "[present]");
    CHECK(contains_line(ruins, "  s2 Arnwick in Arn Vale (rg1): ruined 1300, e23 overrun by Saelith the Black (m2); "
                               "last held by The Kingdom of Arn (c1)"));
    CHECK(contains_line(ruins, "  s3 Corford in Cor Fells (rg3): ruined 400, e12 abandoned after plague, survivors to "
                               "Arnton (s1); last held by The Kingdom of Arn (c1)"));
    CHECK(contains_line(ruins, "  s5 Belham in Bel Downs (rg2): ruined 450, e14 abandoned after famine; last held by "
                               "The Kingdom of Bel (c2)"));
    CHECK(contains_line(ruins, "  s6 Dunton in Dun Heath (rg4): ruined 601, e17 abandoned; last held by The Kingdom "
                               "of Bel (c2)"));
    CHECK(contains_line(ruins, "  s7 Eldstead in The Red Wood (rg5), then called Eld Wood: ruined 1500, e29 sacked by "
                               "The Kingdom of Bel (c2); last held by The Kingdom of Arn (c1)"));
    // the same lines at summary (ruins are listed at both levels)
    const std::vector<std::string> s = block(text(HistoryLevel::summary), "[present]");
    CHECK(contains_line(s, "  s2 Arnwick in Arn Vale (rg1): ruined 1300, e23 overrun by Saelith the Black (m2); last "
                           "held by The Kingdom of Arn (c1)"));
}

TEST_CASE("P5-02 12.3: `beasts:` the world totals at E, every species, at full only, the last line of [present]") {
    const std::vector<std::string> p = block(text(HistoryLevel::full), "[present]");
    REQUIRE_FALSE(p.empty());
    CHECK(p.back() == "beasts: bears 19, wolves 185");
    for (const std::string& l : block(text(HistoryLevel::summary), "[present]")) CHECK(l.rfind("beasts:", 0) != 0U);
}

TEST_CASE("P5-02 12.4: [monsters] after [present]: every monster at full; the living ones by hoard at summary") {
    const std::vector<std::string> full = block(text(HistoryLevel::full), "[monsters]");
    CHECK(full == std::vector<std::string>{
                      "monsters: 2; awake 0, asleep 1, slain 1, died 0",
                      "  m1 Old Grakh: Troll, slain by The Kingdom of Bel (c2), lair Grakh's Hollow (si1) in Cor Fells "
                      "(rg3), since 500 e15, raids 1, killed 30, hoard 1, ended 700 e18",
                      "  m2 Saelith the Black: Dragon, asleep, lair Saelith's Deep (si3) in Arn Vale (rg1), since 975 "
                      "e20, raids 1, killed 100, hoard 32"});
    const std::vector<std::string> summary = block(text(HistoryLevel::summary), "[monsters]");
    CHECK(summary == std::vector<std::string>{
                         "monsters: 2; awake 0, asleep 1, slain 1, died 0",
                         "  m2 Saelith the Black: Dragon, asleep, lair Saelith's Deep (si3) in Arn Vale (rg1), since "
                         "975 e20, raids 1, killed 100, hoard 32"});   // one alive: no `and <x> more` line
    // the block order: [present], [monsters], [sites], [stats]
    const std::vector<std::string> lines = hrtest::lines_of(text(HistoryLevel::full));
    std::vector<std::string> heads;
    for (const std::string& l : lines) {
        if (!l.empty() && l[0] == '[') heads.push_back(l);
    }
    REQUIRE(heads.size() >= 4U);
    CHECK(std::vector<std::string>(heads.end() - 4, heads.end()) ==
          std::vector<std::string>{"[present]", "[monsters]", "[sites]", "[stats]"});
}

TEST_CASE("P5-02 12.5: [sites] after [monsters]: every site at full, the count line only at summary") {
    CHECK(block(text(HistoryLevel::full), "[sites]") ==
          std::vector<std::string>{
              "sites: 5; open 2; lairs 3, outposts 2",
              "  si1 Grakh's Hollow: lair in Cor Fells (rg3) at 35 35, since 500 e15; ended 700 e18 slain",
              "  si2 The Lair of Saelith: lair in Cor Fells (rg3) at 36 36, since 975 e20; ended 1300 e23 moved",
              "  si3 Saelith's Deep: lair in Arn Vale (rg1) at 14 12, since 1300 e23, at Arnwick (s2); open",
              "  si4 Arn Watch: outpost in Arn Vale (rg1) at 12 12, since 1460 e27, of The Kingdom of Arn (c1); open",
              "  si5 Bel Tower: outpost in Bel Downs (rg2) at 22 22, since 1470 e28, of The Kingdom of Bel (c2); ended "
              "1700 e33 quiet"});
    CHECK(block(text(HistoryLevel::summary), "[sites]") ==
          std::vector<std::string>{"sites: 5; open 2; lairs 3, outposts 2"});
}

TEST_CASE("P5-02 12.6: the [stats] keys appended at the end, full only; the summary's keys unchanged") {
    const std::vector<std::pair<std::string, std::int64_t>> st = hrtest::stats_list(text(HistoryLevel::full));
    const std::vector<std::pair<std::string, std::int64_t>> want = {
        {"beasts", 2},          {"monster_kinds", 6},        {"beast_populations", 6}, {"monsters", 2},
        {"sites", 5},           {"monsters_alive", 1},       {"monsters_slain", 1},    {"monsters_died", 0},
        {"monster_raids", 2},   {"monster_killed", 130},     {"settlements_overrun", 1}, {"undead_risen", 0},
        {"outposts", 2},        {"outposts_open", 1},        {"beasts_bold", 1},       {"settlements_left", 3},
        {"regions_shunned", 1}};
    REQUIRE(st.size() >= want.size());
    CHECK(std::vector<std::pair<std::string, std::int64_t>>(st.end() - static_cast<std::ptrdiff_t>(want.size()),
                                                            st.end()) == want);
    const auto m = hrtest::stats_of(text(HistoryLevel::full));
    CHECK(m.at("events_monster_appeared") == 2);
    CHECK(m.at("events_outpost_abandoned") == 1);
    const auto s = hrtest::stats_of(text(HistoryLevel::summary));
    for (const auto& [k, v] : want) CHECK_MESSAGE(s.count(k) == 0U, k);
}

TEST_CASE("P5-02 12.2, 12.7, 10.3: the chronicle names monsters and sites; the monster's flare-up note") {
    const std::string full = text(HistoryLevel::full);
    CHECK(chronicle_line(full, 21) ==
          "1000 e21 flare_up/50: In 1000 tensions flared into 50 years of upheaval. (The waking of Saelith the Black, "
          "a dragon of the old world, stirred the land.) {years=50} [causes e20]");
    CHECK(chronicle_line(full, 15) ==
          "500 e15 monster_appeared/45: Old Grakh, a troll of the hills, made its lair in Cor Fells. {@rg3 monster=m1 "
          "lair=si1}");
    CHECK(chronicle_line(full, 19) ==
          "800 e19 beasts_bold/15: In Bel Downs the wolves grew bold, and travellers from Belford went in fear; three "
          "died on the roads. {@rg2 @s4 settlement=s4 civilization=c2 beast=2 dead=3}");
    CHECK(chronicle_line(full, 33) ==
          "1700 e33 outpost_abandoned/10: The Kingdom of Bel abandoned Bel Tower, the frontier having fallen quiet. "
          "{@rg2 civilization=c2 site=si5 reason=0}");
}

TEST_CASE("P5-02 12.8: the summary grows by at most 5 lines of [monsters] and 2 of [sites]") {
    CHECK(block(text(HistoryLevel::summary), "[monsters]").size() + 1U <= 5U);   // the head line included
    CHECK(block(text(HistoryLevel::summary), "[sites]").size() + 1U <= 2U);
}

}  // TEST_SUITE
