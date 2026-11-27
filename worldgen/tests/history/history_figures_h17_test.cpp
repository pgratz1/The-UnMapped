// Spec P5-06 (docs/specs/p5-06-figures.md r3) 18.1 item 14: report v8 on fixture H17 (worldgen/tests/data/
// history-report/h17-figures.toml, written by reference/p5_06_h17_fixture.py; its docstring says what each figure
// event is): the header, [figures] at both levels (the count line, the figure lines with deeds and fates, the figures
// of note, the books line), the rulers: lines of [civilizations] (a realm with none, a seceded and conquered realm,
// Aldric and Aldric II), [sites] with a library, the monster slain by a hero, the sentences of the new kinds, the
// [stats] keys of 10.5, no ", faded 0". The expected lines are hand-checked against the fixture's rows and equal the
// reference's report (p3_report_reference.py at 0.27.0). The fixture also passes every reader check of 9.4 (the
// reference's reader_problems; HistoryView::load and read_history_tables here).

#include <doctest/doctest.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "../history_report_checks.hpp"
#include "../history_report_fixture.hpp"
#include "worldgen/history/digest.hpp"

namespace {

namespace fs = std::filesystem;
using hrtest::HistoryLevel;

const char* const H17 = "h17-figures";

bool present() { return fs::exists(fs::path(UNMAPPED_P3_07_FIXTURE_DIR) / (std::string(H17) + ".toml")); }

const std::string& text(HistoryLevel level) {
    static std::map<int, std::string> cache;
    const int k = static_cast<int>(level);
    if (const auto it = cache.find(k); it == cache.end()) {
        cache[k] = hrtest::report_of(hrtest::fixture_world(H17), level);
    }
    return cache[k];
}

std::vector<std::string> lines(HistoryLevel level) { return hrtest::lines_of(text(level)); }

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-06 18.1 item 14: H17 loads through both history readers (9.4)") {
    if (!present()) {
        MESSAGE("h17-figures.toml absent: skipped");
        return;
    }
    const worldgen::db::WorldReader r = hrtest::open_world(hrtest::fixture_world(H17));
    const auto t = worldgen::history::read_history_tables(r);
    REQUIRE_MESSAGE(t.ok(), (t.ok() ? std::string() : t.error().message));
    CHECK(t.value().figures.size() == 20U);
    CHECK(t.value().books.size() == 2U);
}

TEST_CASE("P5-06 10.1, 10.2, 18.1 item 14: H17 [figures] at full and at summary") {
    if (!present()) {
        MESSAGE("h17-figures.toml absent: skipped");
        return;
    }
    const std::vector<std::string> f = lines(HistoryLevel::full);
    const std::vector<std::string> s = lines(HistoryLevel::summary);
    REQUIRE_FALSE(f.empty());
    REQUIRE_FALSE(s.empty());
    CHECK(f[0] == "# worldgen history v11");
    CHECK(s[0] == "# worldgen history v11");
    const std::vector<std::string> want_full = {
        "[figures]",
        "figures: 20; rulers 13, claimants 2, generals 2, heroes 2, casters 1; alive at the end 7",
        "  f1 Aldric the Founder: ruler of The Kingdom of Cear (c5), Human, 300-380, reigned 300-380 (reign 1, "
        "steady); died 380 e24",
        "  f2 Dain the Victorious: ruler of The Hold of Durn (c3), Dwarven, 520-640, reigned 590-640 (reign 13, "
        "grasping); 1 won; died 640 e49",
        "  f3 Aldric II: ruler of The Kingdom of Cear (c5), Human, 765-820, reigned 798-820 (reign 4, warlike); 1 war "
        "declared; died 820 e64",
        "  f4 Osric the Old: ruler of The Kingdom of Cear (c5), Human, 795-900, reigned 820-900 (reign 5, just); died "
        "900 e68",
        "  f5 Edric the Old: ruler of The March of Edric (c8), Human, 795-870, reigned 820-870 (reign 1, warlike); "
        "died 870 e66",
        "  f6 Grishnak the Faithless: ruler of The Urarz Horde (c7), Goblin, 970-1030, reigned 990-1030 (reign 40, "
        "grasping); 1 war declared, 1 won; died 1030 e73",
        "  f7 Kili Trollbane: hero of The Hold of Durn (c3), Dwarven, 1290-1500; slew Skarr (m1); died 1500 e91",
        "  f8 Faelan: hero of The House of Aelin (c1), Elven, 1200-1340; slain by Saelith (m2) 1340 e83",
        "  f9 Iensath the Accursed: caster of The House of Aelin (c1), Elven, 1100-E; 2 books, a blight; alive",
        "  f10 Cenric the Conqueror: ruler of The Kingdom of Cear (c5), Human, 1535-1610, reigned 1560-1610 (reign 9, "
        "steady); 1 war declared, 1 won, 1 realm conquered; died 1610 e103",
        "  f11 Ulf: general of The Kingdom of Cear (c5), Human, 1545-1650; 2 battles led, 1 won; died 1650 e108",
        "  f12 Garth: general of The March of Edric (c8), Human, 1540-1572; 1 battle led, 0 won; fell in battle 1572 "
        "e100 in Edric March (rg11)",
        "  f13 Godric the Last: ruler of The March of Edric (c8), Human, 1555-1575, reigned 1570-1575 (reign 20, "
        "steady); fell with the realm 1575 e102",
        "  f14 Harold the Old: ruler of The Kingdom of Cear (c5), Human, 1590-1660, reigned 1615-1660 (reign 11, "
        "warlike); 1 war declared; died 1660 e109",
        "  f15 Thrain the Old: ruler of The Hold of Durn (c3), Dwarven, 1780-E, reigned 1850-E (reign 52, steady); "
        "alive",
        "  f16 Borin the Exile: claimant of The Hold of Durn (c3), Dwarven, 1800-E; in exile with The House of Aelin "
        "(c1)",
        "  f17 Elarion: ruler of The House of Aelin (c1), Elven, 1600-E, reigned 1900-E (reign 14, peaceable); alive",
        "  f18 Ilythe the Exile: claimant of The House of Aelin (c1), Elven, 1650-E; in the wilds",
        "  f19 Wulfric: ruler of The Kingdom of Cear (c5), Human, 1955-E, reigned 1980-E (reign 13, steady); alive",
        "  f20 Uglak: ruler of The Urarz Horde (c7), Goblin, 1975-E, reigned 1990-E (reign 90, grasping); alive",
        "books: 2; The Lay of the First Stars by Iensath the Accursed (f9) at The Leaf-House of Iensath (si5), 1370 e86; "
        "On the Speech of Trees by Iensath the Accursed (f9) at The Leaf-House of Iensath (si5), 1380 e87"};
    CHECK(hrtest::block(text(HistoryLevel::full), "[figures]") ==
          std::vector<std::string>(want_full.begin() + 1, want_full.end()));
    const std::vector<std::string> sb = hrtest::block(text(HistoryLevel::summary), "[figures]");
    REQUIRE(sb.size() == 9U);   // the count line, SUMMARY_FIGURES_MAX figures of note, "and 14 more", the books line
    CHECK(sb[0] == want_full[1]);
    CHECK(sb[7] == "  and 14 more");
    CHECK(sb[8] == "books: 2");
    CHECK(worldgen::history::SUMMARY_FIGURES_MAX == 6U);
}

TEST_CASE("P5-06 10.3, 10.4, 18.1 item 14: H17 rulers lines, [sites] with a library, a monster slain by a hero") {
    if (!present()) {
        MESSAGE("h17-figures.toml absent: skipped");
        return;
    }
    const std::string& f = text(HistoryLevel::full);
    const std::string& s = text(HistoryLevel::summary);
    const std::vector<std::string> c5 = hrtest::civ_block(f, 5);
    CHECK(hrtest::contains_line(c5, "  rulers: 13 reigns, 6 recorded; first Aldric the Founder (f1); at the end "
                                    "Wulfric (f19)"));
    CHECK(hrtest::contains_line(c5, "    Aldric II (f3): 798-820, reign 4, warlike"));
    CHECK(hrtest::contains_line(c5, "    Wulfric (f19): 1980-E, reign 13, steady"));
    // a conquered realm: no "at the end"; a realm with no recorded ruler
    CHECK(hrtest::contains_line(hrtest::civ_block(f, 8), "  rulers: 20 reigns, 2 recorded; first Edric the Old (f5)"));
    CHECK(hrtest::contains_line(hrtest::civ_block(f, 2), "  rulers: 0 reigns, 0 recorded"));
    // summary: the count line only
    const std::vector<std::string> s5 = hrtest::civ_block(s, 5);
    CHECK(hrtest::contains_line(s5, "  rulers: 13 reigns, 6 recorded; first Aldric the Founder (f1); at the end "
                                    "Wulfric (f19)"));
    CHECK_FALSE(hrtest::contains_line(s5, "    Aldric II (f3): 798-820, reign 4, warlike"));
    const std::vector<std::string> sites = hrtest::block(f, "[sites]");
    REQUIRE_FALSE(sites.empty());
    CHECK(sites[0] == "sites: 5; open 3; lairs 2, outposts 0, mines 2, towers 1");
    CHECK(hrtest::contains_line(sites, "  si5 The Leaf-House of Iensath: library in Aelin Vale (rg1) at 10 10, since "
                                       "1360 e85, of The House of Aelin (c1), by Iensath the Accursed (f9), at "
                                       "Aelinost (s1); open"));
    CHECK(hrtest::contains_line(hrtest::block(f, "[monsters]"),
                                "  m1 Skarr: Troll, slain by Kili Trollbane (f7) of The Hold of Durn (c3), lair The "
                                "Troll-Holes of Skarr (si3) in Skarr Crags (rg12), since 1300 e77, raids 1, killed 20, "
                                "hoard 0, ended 1350 e84"));
    // 25.1 (d): no ", faded 0" in a world without a faded row
    CHECK(f.find(", faded 0") == std::string::npos);
    CHECK(f.find(", 0 faded") == std::string::npos);
}

TEST_CASE("P5-06 8.2, 18.1 items 12 and 14: H17's sentences of the new kinds and of {r:under}") {
    if (!present()) {
        MESSAGE("h17-figures.toml absent: skipped");
        return;
    }
    const std::string& f = text(HistoryLevel::full);
    for (const char* want :
         {"Iensath the Accursed of the House of Aelin founded the Leaf-House of Iensath in Aelin Vale.",
          "Iensath the Accursed wrote the Lay of the First Stars at the Leaf-House of Iensath.",
          "Iensath the Accursed wrote On the Speech of Trees at the Leaf-House of Iensath.",
          "The workings of Iensath the Accursed at the Leaf-House of Iensath went awry, and a blight fell on Aelin "
          "Vale; 60 died.",
          "Osric the Old took the throne of the Kingdom of Cear against the claim of Edric the Old, who broke away "
          "with the far lands.",
          "Borin the Exile, who had claimed the throne of the Hold of Durn, went into exile with the House of Aelin.",
          "Ilythe the Exile, who had claimed the throne of the House of Aelin, fled into the wilds.",
          "Ulf again took command of the armies of the Kingdom of Cear against the Skreegg Horde.",
          "Kili Trollbane of the Hold of Durn hunted down and slew Skarr",
          "Faelan of the House of Aelin was slain hunting Saelith",
          "The Kingdom of Cear under Aldric II declared war on the Skreegg Horde over Skree Moor.",
          "The March of Edric under Godric the Last fell to the Kingdom of Cear."}) {
        CAPTURE(want);
        CHECK(f.find(want) != std::string::npos);
    }
}

TEST_CASE("P5-06 10.5, 18.1 item 14: H17 [stats] keys of 10.5 at full, none at summary") {
    if (!present()) {
        MESSAGE("h17-figures.toml absent: skipped");
        return;
    }
    const std::map<std::string, std::int64_t> st = hrtest::stats_of(text(HistoryLevel::full));
    const std::map<std::string, std::int64_t> want = {
        {"figures", 20},           {"figures_rulers", 13},  {"figures_claimants", 2},       {"figures_generals", 2},
        {"figures_heroes", 2},     {"figures_casters", 1},  {"figures_alive", 7},           {"figures_epithet", 13},
        {"epithet_bearers_max", 4}, {"reigns", 189},        {"rulers_recorded", 13},        {"rulers_by_deed", 5},
        {"rulers_regnal", 1},      {"successions_disputed", 3}, {"exiles", 2},             {"generals_appointed", 3},
        {"battles_led", 2},        {"generals_fallen", 1},  {"heroes_risen", 2},            {"monsters_slain_by_heroes", 1},
        {"heroes_slain", 1},       {"towers", 1},           {"books", 2},                   {"blights", 1},
        {"figure_age_median", 75}, {"summary_major_calamities", 0}};
    for (const auto& [k, v] : want) {
        CAPTURE(k);
        REQUIRE(st.count(k) == 1U);
        CHECK(st.at(k) == v);
    }
    CHECK(hrtest::stats_of(text(HistoryLevel::summary)).count("figures") == 0U);
}

}  // TEST_SUITE
