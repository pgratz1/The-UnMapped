// Spec P3-07 14.1 items 1 to 4 and 7, with 22.6 and 23 item 5: escape_history_text and
// parse_history_level; plurals and spans; each block of section 6 on the fixtures against lines written
// here by hand (worked out from the fixture rows, not produced by the code under test); the computations
// of section 7 (peaks, milestones, the summary threshold and set, the land shares); the notes of 21.4,
// 22.2, 22.3 and 23 items 3, 4 and 4a; kinds the build does not compile (A1); and the digest API of 5.1.
//
// Written from the spec only. Readings where the spec is open (the test writer's report): milestone lines
// carry the note of history_note(view, event, c) (R1); the generations note needs c among the participants
// (R2).

#include <doctest/doctest.h>

#include <limits>

#include <cstdint>
#include <string>
#include <vector>

#include "history_report_checks.hpp"
#include "history_report_fixture.hpp"
#include "worldgen/history/digest.hpp"

namespace {

using hrtest::block;
using hrtest::civ_block;
using hrtest::contains_line;
using hrtest::HistoryLevel;
namespace wi = worldgen::inspect;

std::string text(const std::string& fixture, HistoryLevel level) {
    static std::map<std::pair<std::string, int>, std::string> cache;
    const auto key = std::make_pair(fixture, static_cast<int>(level));
    const auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    return cache[key] = hrtest::report_of(hrtest::fixture_world(fixture), level);
}
std::string summary(const std::string& f) { return text(f, HistoryLevel::summary); }
std::string full(const std::string& f) { return text(f, HistoryLevel::full); }

// The lines of block `head` that start with `prefix`.
std::vector<std::string> starting(const std::vector<std::string>& lines, const std::string& prefix) {
    std::vector<std::string> out;
    for (const std::string& l : lines) {
        if (l.rfind(prefix, 0) == 0) out.push_back(l);
    }
    return out;
}

// The chronicle event line of event `id`, or "".
std::string event_line(const std::string& report, std::int64_t id) {
    for (const std::string& l : block(report, "[chronicle]")) {
        if (hrtest::is_event_line(l) && hrtest::event_id_of(l) == id) return l;
    }
    return {};
}

}  // namespace

TEST_SUITE("history-report") {

TEST_CASE("P3-07 14.1 item 1: escape_history_text") {
    CHECK(wi::escape_history_text("") == "");
    CHECK(wi::escape_history_text("Voeledell (s1) {x} [y] ~!") == "Voeledell (s1) {x} [y] ~!");
    CHECK(wi::escape_history_text("a\\b") == "a\\\\b");
    CHECK(wi::escape_history_text("\\\\") == "\\\\\\\\");
    for (int b = 0; b < 0x20; ++b) {
        CAPTURE(b);
        const std::string in(1, static_cast<char>(b));
        static const char* const hex = "0123456789ABCDEF";
        CHECK(wi::escape_history_text(in) == std::string("\\x") + hex[b >> 4] + hex[b & 15]);
    }
    CHECK(wi::escape_history_text(std::string(1, '\x7f')) == "\\x7F");
    CHECK(wi::escape_history_text("\x80\x9f\xa0\xc3\xa9\xff") == "\\x80\\x9F\\xA0\\xC3\\xA9\\xFF");
    CHECK(wi::escape_history_text(std::string("a\0b", 3)) == "a\\x00b");
    CHECK(wi::escape_history_text(" trailing ") == " trailing ");  // the \x20 rule is a line rule, not escaping
    for (int b = 0x20; b < 0x7f; ++b) {
        if (b == 0x5c) continue;
        const std::string in(1, static_cast<char>(b));
        CHECK(wi::escape_history_text(in) == in);
    }
}

TEST_CASE("P3-07 4, 5.1: parse_history_level") {
    CHECK(wi::parse_history_level("summary") == HistoryLevel::summary);
    CHECK(wi::parse_history_level("full") == HistoryLevel::full);
    CHECK_FALSE(wi::parse_history_level("brief").has_value());
    CHECK_FALSE(wi::parse_history_level("").has_value());
    CHECK_FALSE(wi::parse_history_level("Full").has_value());
    CHECK_FALSE(wi::parse_history_level("full ").has_value());
    // r19 25.1: the header is `# worldgen history v2`, checked on the output text (the spec names no library
    // constant for it; see the H1 header case and history_report_v2_test.cpp)
}

TEST_CASE("P3-07 14.1 item 1: the trailing-space rule on a template ending in a space and on an empty sentence") {
    // P5-03a 9.2 (v6): the summary's milestones are a count, so the milestone lines are read at full
    const std::vector<std::string> c3 = civ_block(full("h1-war-and-ruin"), 3);
    CHECK(contains_line(c3, "    1900 e18 oath: Gutrot Clans swore an oath\\x20"));
    CHECK(contains_line(c3, "    1950 e19 silence:\\x20"));
    // in the chronicle the refs follow, so nothing is rewritten
    CHECK(event_line(full("h1-war-and-ruin"), 18) == "1900 e18 oath/99: Gutrot Clans swore an oath  {@rg4 civilization=c3}");
    CHECK(event_line(full("h1-war-and-ruin"), 19) == "1950 e19 silence/98:  {civilization=c3 flag=1}");
}

TEST_CASE("P3-07 14.1 items 1 and 7: escaping of a region name and a template literal; test kinds (A1)") {
    const std::string f = full("h1-war-and-ruin");
    CHECK(event_line(f, 4) == "100 e4 omen/30: A comet crossed the sky \\\\ twice. {}");
    CHECK(event_line(f, 7) ==
          "1500 e7 awakening/100: Krok awakened the Goblins in Gr\\xC3\\xA9y \\\\ Moor. {@rg3 race=ra2 creator=g2}");
    CHECK(contains_line(block(f, "[present]"), "  rg3 Gr\\xC3\\xA9y \\\\ Moor: Finen (c1) since 2005"));
    CHECK(contains_line(civ_block(f, 2), "  home: Gr\\xC3\\xA9y \\\\ Moor (rg3), grassland"));
    CHECK(event_line(f, 17) == "1800 e17 tally/10: Gutrot Clans counted -45 in the ledger. {keeper=c3 delta=-45}");
    CHECK(event_line(f, 35) == "2600 e35 landing/30: Gutrot Clans landed on Islet Rock. {@rg6 civilization=c3}");
}

TEST_CASE("P3-07 6.2, 6.3, 6.4, 6.5, r17 24.4 and 24.6 on H1: header, [settings], [ages], [phases] at full only") {
    const std::string s = summary("h1-war-and-ruin");
    const std::vector<std::string> lines = hrtest::lines_of(s);
    REQUIRE(lines.size() > 3U);
    CHECK(lines[0] == "# worldgen history v11");   // r19 25.1; v6 since P5-03a 9.1
    CHECK(lines[1] == "level = summary");
    CHECK(lines[2] == "[settings]");
    CHECK(block(s, "[settings]") == std::vector<std::string>{
                                        "seed = 101", "seed_phrase =", "map_size = 512", "history_span = 3000",
                                        "history_detail = normal", "races = elf,goblin", "civs_per_race = auto",
                                        "event_intensity = normal", "monster_pressure = normal",
                                        "divine_activity = normal", "divine_script =", "generator_version = 0.29.0"});
    CHECK(block(s, "[ages]") ==
          std::vector<std::string>{
              "a1 The Age of Elves: 0-1499, 1500 years; Elves (ra1) made by Fielie (g1, good); 1 civilization founded: "
              "1 at the awakening, 0 by secession",
              "a2 The Age of Goblins: 1500-2999, 1500 years; Goblins (ra2) made by Krok (g2, evil); 2 civilizations "
              "founded: 2 at the awakening, 0 by secession"});
    // 24.4: [phases] at full only; the chronicle before the civilization blocks
    CHECK(s.find("\n[phases]\n") == std::string::npos);
    CHECK(s.find("\n[chronicle]\n") < s.find("\n[civilizations]\n"));
    const std::string f = full("h1-war-and-ruin");
    CHECK(f.find("\n[ages]\n") < f.find("\n[phases]\n"));
    CHECK(f.find("\n[phases]\n") < f.find("\n[chronicle]\n"));
    CHECK(block(f, "[phases]") == std::vector<std::string>{
                                      "p1 a1 opening 0-149, 150 years; trigger e1; 4 events",
                                      "p2 a1 quiet 150-1499, 1350 years; 2 events",
                                      "p3 a2 opening 1500-1649, 150 years; trigger e7; 8 events",
                                      "p4 a2 quiet 1650-1999, 350 years; 6 events",
                                      "p5 a2 flare_up 2000-2049, 50 years; trigger e21; 12 events",
                                      "p6 a2 quiet 2050-2499, 450 years; 1 event",
                                      "p7 a2 flare_up 2500-2500, 1 year; trigger e34; 1 event",
                                      "p8 a2 quiet 2501-2899, 399 years; 1 event",
                                      "p9 a2 final 2900-2999, 100 years; 0 events"});
}

TEST_CASE("P3-07 6.6, 7.1-7.3 on H1: the three civilization blocks") {
    const std::string s = summary("h1-war-and-ruin");
    // c1: two runs at the peak of 3 regions (the first printed); a tie of salience 70 broken by id (e22
    // before e24); quiet_stretch milestones with 0 and 1 generations (elves: 500 years a generation)
    // c1 (24.5): wars line, adversaries at summary (same first year: no; f4 a reserved type), milestones: the
    // founding, the longest quiet stretch (e33, 450 years, one generation), then own deeds by salience with at most
    // two of a kind (two declarations of salience 70, then the settlement_founded of id 3 among three of 40)
    CHECK(civ_block(s, 1) ==
          std::vector<std::string>{
              "c1 Finen",
              "  race: Elf (ra1)",
              "  home: Elder Vale (rg1), grassland",
              "  founded: 0 in The Age of Elves (a1), e2",
              "  capital: Voeledell (s1)",
              "  peak regions: 3, 300-1699",
              "  peak population: 1920 in 2049",
              // r19 25.3 item 3: s5 (Old Ford) faded, counted apart; s3 was the Skib Horde's
              "  settlements: 3 founded, 3 held at the end, 0 ruined, 1 faded",
              "  wars: 2 declared, 0 defended; won 1, lost 0, drawn 1, open 0",
              // r19 25.4: war adversaries only, then one count line; 25.7 item 1: a war's last year is the year of its
              // ending (the truce of 2030, the conquest of 2010)
              "  adversaries: 3",
              "    Gutrot Clans (c3): 1 war, won 0, lost 0, drawn 1, open 0, 2003-2030; now at peace",
              "    The Skib Horde (c2): 1 war, won 1, lost 0, drawn 0, open 0, 2001-2010; now none",
              // P5-03a 9.2 (v6): no without-war line; the milestones as a count (the full block's eight) followed by
              // the quiet stretches only
              "  events: 19",
              "  fate: alive in 3000: 3 regions, 3 settlements, population 1920",
              // P5-04a 12.2 (v9): the count, then at most three lines of the realm's own deeds by salience (a quiet
              // stretch is no milestone of the summary)
              "  milestones: 8",
              "    2001 e22 war_declared: Finen declared war on the Skib Horde over Gr\\xC3\\xA9y \\\\ Moor. (Claimed "
              "since 1999.)",
              "    2010 e29 sacked: Finen sacked Skrisligg Pit of the Skib Horde; 150 died and 50 fled.",
              "    2010 e30 civ_conquered: The Skib Horde fell to Finen.",
               "  allies: 0", "  rulers: 0 reigns, 0 recorded"});   // P5-04 10.3 (v7): in every block
    // the relation rows stay at full in place of the adversary lines
    const std::vector<std::string> c1_full = civ_block(full("h1-war-and-ruin"), 1);
    REQUIRE(c1_full.size() > 15U);
    CHECK(std::vector<std::string>(c1_full.begin() + 9, c1_full.begin() + 15) ==
          std::vector<std::string>{"  relations: 5 (enmity 1, pact 1, peace 1, war 2)",
                                   "    pact: Finen (c1), f4, since 100, e4",
                                   "    enmity: Finen (c1), Gutrot Clans (c3), since 1600, e14",
                                   "    war: Finen (c1), The Skib Horde (c2), 2001-2009, e22 to e30",
                                   "    war: Finen (c1), Gutrot Clans (c3), 2003-2029, e24 to e31",
                                   "    peace: Finen (c1), Gutrot Clans (c3), since 2030, e31"});
    // c2: ended (last capital, fate with the sentence), lost its one war. r19 25.5: at summary an ended block leaves
    // out home, the peaks and events, and has at most four milestones: the founding, the ending (no quiet stretch:
    // c2 is in none), then the fill (own deeds first: its two foundings of salience 40, e9 before e12)
    CHECK(civ_block(s, 2) ==
          std::vector<std::string>{
              "c2 The Skib Horde",
              "  race: Goblin (ra2)",
              "  founded: 1500 in The Age of Goblins (a2), e8",
              "  last capital: none",
              "  settlements: 2 founded, 0 held at the end, 1 ruined",
              "  wars: 0 declared, 1 defended; won 0, lost 1, drawn 0, open 0",
              "  adversaries: 1",
              "    Finen (c1): 1 war, won 0, lost 1, drawn 0, open 0, 2001-2010; now none",
              "  fate: ended in 2010, e30: The Skib Horde fell to Finen.",
              // P5-03a 9.2 (v6): an ended realm's count is that of its full list (eight), no quiet stretch
              // P5-04a 12.2 (v9): the count of the full list, then the shown deeds
              "  milestones: 2",
              "    1500 e9 settlement_founded: The Skib Horde founded Skrisligg Pit in Gr\\xC3\\xA9y \\\\ Moor.",
              // P5-07 10 item 6 (v10): at most one shown per kind (e12 is a second founding)
              "  allies: 0", "  rulers: 0 reigns, 0 recorded"});   // P5-04 10.3 (v7)
    // the same realm at full keeps 24.5's block (eight milestones)
    const std::vector<std::string> c2_full = civ_block(full("h1-war-and-ruin"), 2);
    CHECK(contains_line(c2_full, "  home: Gr\\xC3\\xA9y \\\\ Moor (rg3), grassland"));
    CHECK(contains_line(c2_full, "  peak regions: 1, 1500-2004"));
    CHECK(contains_line(c2_full, "  peak population: 450 in 1649"));
    CHECK(contains_line(c2_full, "  events: 11"));
    CHECK(contains_line(c2_full, "    2010 e29 sacked: Finen sacked Skrisligg Pit of the Skib Horde; 150 died and 50 "
                                 "fled."));
    // c3: a one-year peak of regions, the earlier of two peak census years, its longest stretch (e33: 18
    // generations of 25 years) and not the other (e15)
    CHECK(civ_block(s, 3) ==
          std::vector<std::string>{
              "c3 Gutrot Clans",
              "  race: Goblin (ra2)",
              "  home: The Ashen Fields (rg4), temperate_forest",
              "  founded: 1500 in The Age of Goblins (a2), e10",
              "  capital: Ghashpit (s4)",
              "  peak regions: 2, 2600-2600",
              "  peak population: 1000 in 1999",
              "  settlements: 2 founded, 1 held at the end, 0 ruined",
              "  wars: 0 declared, 1 defended; won 0, lost 0, drawn 1, open 0",
              "  adversaries: 1",
              "    Finen (c1): 1 war, won 0, lost 0, drawn 1, open 0, 2003-2030; now at peace",
              "  events: 13",
              "  fate: alive in 3000: 1 region, 1 settlement, population 1000",
              "  milestones: 2",   // P5-04a 12.2 (v9)
              "    1500 e11 settlement_founded: Gutrot Clans founded Ghashpit in the Ashen Fields.",
               "  allies: 0", "  rulers: 0 reigns, 0 recorded"});   // P5-04 10.3 (v7)
    // the upland rule of 6.6 (4 x walk_cells < cells): Stone Teeth, 300 of 2000
    CHECK(contains_line(block(s, "[present]"), "regions: 6; held 4, free 2"));
}

TEST_CASE("P3-07 6.7, 21.4, 22.2 on H1: the chronicle, causes, :all, values, notes; no note on a quiet stretch") {
    const std::string f = full("h1-war-and-ruin");
    const std::string s = summary("h1-war-and-ruin");
    const std::vector<std::string> cf = block(f, "[chronicle]");
    const std::vector<std::string> cs = block(s, "[chronicle]");
    CHECK(std::vector<std::string>(cf.begin(), cf.begin() + 5) ==
          std::vector<std::string>{"threshold = 0", "events = 35", "shown = 35", "== a1 The Age of Elves: 0-1499",
                                   "-- p1 opening: 0-149, 150 years; trigger e1; 4 events"});
    CHECK(std::vector<std::string>(cs.begin(), cs.begin() + 3) ==
          std::vector<std::string>{"threshold = 20", "events = 35", "shown = 30"});
    // 24.3: claims (e14, e20, e23) are not in the summary chronicle; the declaration's note reads the claim
    CHECK(event_line(s, 14).empty());
    CHECK(event_line(s, 20).empty());
    CHECK(event_line(s, 23).empty());
    CHECK(event_line(s, 22) == event_line(f, 22));
    // 24.2: the last battle of e22's war (its only battle, e25) is shown through the war's frame
    CHECK(event_line(s, 25) == event_line(f, 25));
    CHECK(event_line(f, 3) == "0 e3 settlement_founded/40: Finen founded Voeledell in Elder Vale. {@rg1 @s1 "
                              "settlement=s1 founder=c1 population=200} [causes e2]");
    CHECK(event_line(f, 21) == "2000 e21 flare_up/50: In 2000 tensions flared into 50 years of upheaval. (The claim "
                               "of Finen to Gr\\xC3\\xA9y \\\\ Moor, held by the Skib Horde, came to a head.) "
                               "{years=50} [causes e20]");
    CHECK(event_line(f, 22) == "2001 e22 war_declared/70: Finen declared war on the Skib Horde over Gr\\xC3\\xA9y \\\\ "
                               "Moor. (Claimed since 1999.) {@rg3 attacker=c1 defender=c2} [causes e20, e21]");
    CHECK(event_line(f, 25) == "2004 e25 battle/30: Finen defeated the Skib Horde in Gr\\xC3\\xA9y \\\\ Moor; one of the "
                               "defeated and 12 of the victors fell. {@rg3 winner=c1 loser=c2 winner_dead=12 "
                               "loser_dead=1} [causes e22]");
    CHECK(event_line(f, 31) == "2030 e31 truce/50: Finen and Gutrot Clans made peace after 27 years of war, neither "
                               "the victor. {parties=c1 parties=c3 years=27} [causes e24]");
    // 22.2: no generations note on the chronicle line of a quiet stretch
    CHECK(event_line(f, 15) == "1650-1949 e15 quiet_stretch/95: For 300 years, from 1650 to 1949, the land was quiet, "
                               "and two realms kept the peace. {realm=c1 realm=c3 years=300 realms=2}");
    // a flare-up whose first cause is not a claim has no note; a one-year phase heading; plurals
    CHECK(event_line(f, 34) == "2500 e34 flare_up/50: In 2500 tensions flared into one year of upheaval. {years=1} "
                               "[causes e32]");
    CHECK(contains_line(cf, "-- p7 flare_up: 2500-2500, 1 year; trigger e34; 1 event"));
    CHECK(contains_line(cs, "-- p7 flare_up: 2500-2500, 1 year; trigger e34; 1 event"));
    CHECK(contains_line(cf, "== a2 The Age of Goblins: 1500-2999"));
    CHECK(contains_line(cf, "-- p9 final: 2900-2999, 100 years; 0 events"));
    CHECK(contains_line(cs, "-- p2 quiet: 150-1499, 1350 years; 2 events"));
    // summary: the two events of salience 10 are left out, one per phase (pl "1 more event")
    CHECK(event_line(s, 17).empty());
    CHECK(event_line(s, 32).empty());
    CHECK(starting(cs, "(") == std::vector<std::string>{"(1 more event not shown)", "(2 more events not shown)",
                                                       "(2 more events not shown)"});
    CHECK(starting(cf, "(").empty());
}

TEST_CASE("P3-07 6.8 on H1: [present] at both levels") {
    CHECK(block(summary("h1-war-and-ruin"), "[present]") ==
          std::vector<std::string>{"year: 3000",
                                   "civilizations alive: 2 of 3",
                                   "c1 Finen: 3 regions, 3 settlements, population 1920",
                                   "c3 Gutrot Clans: 1 region, 1 settlement, population 1000",
                                   "regions: 6; held 4, free 2",
                                   "settlements: 7; thriving 2, declining 3, ruined 1, faded 1",
                                   "settlement kinds: hamlet 2, village 2, town 1, city 1, fortress 1",
                                   "largest settlement: Voeledell (s1), population 1500",
                                   "ruins: 1",
                                   "  s3 Skrisligg Pit in Gr\\xC3\\xA9y \\\\ Moor (rg3): ruined 2010, e29 sacked by Finen (c1); "
                                   "last held by The Skib Horde (c2)",
                                   "hidden refuges: 0 of 0 founded by flight",   // P3-07 r21 26.3
                                   "open wars: 0",
                                   "standing claims: 0",
                                   "renamed regions: 0"});   // r21 26.2
    CHECK(block(full("h1-war-and-ruin"), "[present]") ==
          std::vector<std::string>{
              "year: 3000",
              "civilizations alive: 2 of 3",
              "c1 Finen: 3 regions, 3 settlements, population 1920",
              "c3 Gutrot Clans: 1 region, 1 settlement, population 1000",
              "regions: 6; held 4, free 2",
              "  rg1 Elder Vale: Finen (c1) since 0",
              "  rg2 Stone Teeth: Finen (c1) since 200",
              "  rg3 Gr\\xC3\\xA9y \\\\ Moor: Finen (c1) since 2005",
              "  rg4 The Ashen Fields: Gutrot Clans (c3) since 1500",
              "  rg5 Low Marsh: free",
              "  rg6 Islet Rock: free",
              "settlements: 7; thriving 2, declining 3, ruined 1, faded 1",
              "  s1 Voeledell: city, thriving, population 1500, Finen (c1), at 100 120 in Elder Vale (rg1), founded 0 "
              "by Finen (c1)",
              "  s2 Aelith: fortress, declining, population 300, Finen (c1), at 210 40 in Stone Teeth (rg2), founded "
              "200 by Finen (c1)",
              "  s3 Skrisligg Pit: hamlet, ruined, population 0, no owner, at 300 310 in Gr\\xC3\\xA9y \\\\ Moor (rg3), "
              "founded 1500 by The Skib Horde (c2), ruined 2010, e29, last held by The Skib Horde (c2)",
              "  s4 Ghashpit: town, thriving, population 1000, Gutrot Clans (c3), at 400 90 in The Ashen Fields (rg4), "
              "founded 1500 by Gutrot Clans (c3)",
              "  s5 Old Ford: village, faded, population 0, no owner, at 150 450 in Low Marsh (rg5), founded 300 by "
              "Finen (c1), last held by Finen (c1)",
              "  s6 Mossgate: village, declining, population 50, no owner, at 160 470 in Low Marsh (rg5), founded 1600 "
              "by Gutrot Clans (c3)",
              "  s7 Redwall: hamlet, declining, population 120, Finen (c1), at 320 330 in Gr\\xC3\\xA9y \\\\ Moor (rg3), "
              "founded 1510 by The Skib Horde (c2)",
              "settlement kinds: hamlet 2, village 2, town 1, city 1, fortress 1",
              "largest settlement: Voeledell (s1), population 1500",
              "ruins: 1",
              "  s3 Skrisligg Pit in Gr\\xC3\\xA9y \\\\ Moor (rg3): ruined 2010, e29 sacked by Finen (c1); last held by "
              "The Skib Horde (c2)",
              "hidden refuges: 0 of 0 founded by flight",   // P3-07 r21 26.3
              "open wars: 0",
              "standing claims: 0",
              "renamed regions: 0", "beasts:"});   // r21 26.2; P5-02 12.3 (full only; no beast rows)
}

TEST_CASE("P3-07 9 on H1: [stats] values worked out from the rows") {
    const auto st = hrtest::stats_of(full("h1-war-and-ruin"));   // r19 25.7 item 4: every key at full
    const std::vector<std::pair<std::string, std::int64_t>> want = {
        {"end_year", 3000}, {"ages", 2}, {"phases", 9}, {"phases_opening", 2}, {"phases_quiet", 4},
        {"phases_flare", 2}, {"phases_final", 1}, {"simulated_years", 451}, {"simulated_permille", 150},
        {"quiet_years", 2549}, {"flare_years", 51}, {"events", 35}, {"events_omen", 1}, {"events_landing", 1},
        {"events_caused", 17}, {"cause_links", 18}, {"events_per_1000_simulated_years", 77},
        {"summary_threshold", 20}, {"summary_events", 30}, {"civilizations", 3}, {"civilizations_alive", 2},
        {"civilizations_ended", 1}, {"civilizations_elf", 1}, {"civilizations_goblin", 2}, {"civilizations_dwarf", 0},
        {"civ_lifetime_max", 3000}, {"civ_regions_peak_max", 3}, {"civ_population_peak_max", 1920},
        {"settlements", 7}, {"settlements_standing", 5}, {"settlements_thriving", 2}, {"settlements_declining", 3},
        {"settlements_ruined", 1}, {"settlements_faded", 1}, {"settlements_hamlet", 2}, {"settlements_village", 2},
        {"settlements_town", 1}, {"settlements_city", 1}, {"settlements_fortress", 1}, {"settlements_unowned", 1},
        {"population", 2970}, {"population_settlement_max", 1500}, {"census_rows", 24}, {"regions", 6},
        {"regions_nonislet", 5}, {"regions_held", 4}, {"region_control_rows", 7}, {"regions_ever_held", 6},
        {"share_max_permille", 600}, {"share_max_civ", 1}, {"share_max_year", 300}, {"share_over_500_years", 1400},
        {"share_over_600_years", 0}, {"relations", 5}, {"relations_open", 3}, {"wars", 2}, {"wars_open", 0},
        {"population_elf", 1920}, {"population_goblin", 1000}, {"population_dwarf", 0},
        {"settlements_standing_elf", 3}, {"settlements_standing_goblin", 1}, {"regions_held_elf", 3},
        {"regions_held_goblin", 1}, {"civ_settlements_max", 3}, {"settlements_colonies", 0},
        {"civilizations_seceded", 0}, {"relations_enmity", 1}, {"relations_peace", 1}, {"war_years_total", 36},
        {"war_years_max", 27}, {"civilizations_conquered", 1}, {"settlements_sacked", 1}, {"settlements_captured", 1},
        {"regions_conquered", 1}, {"races_alive", 2}, {"wars_attacker_elf", 2}, {"wars_attacker_goblin", 0},
        {"wars_attacker_dwarf", 0}, {"wars_hollow", 1}, {"wars_hollow_permille", 500}, {"wars_same_goal_max", 1},
        {"wars_same_goal_unchanged_max", 1}, {"race_population_share_max_permille", 646},
        {"race_regions_share_max_permille", 750}, {"wars_unfound", 0}, {"refuges_held_no_destination", 0}};
    for (const auto& [k, v] : want) {
        CAPTURE(k);
        REQUIRE(st.count(k) == 1U);
        CHECK(st.at(k) == v);
    }
    // events_<key> in the stored catalogue's id order, the fixture's own ids (kinds found by key)
    std::vector<std::string> keys;
    for (const auto& [k, v] : hrtest::stats_list(full("h1-war-and-ruin"))) {
        if (k.rfind("events_", 0) == 0 && k != "events_caused" && k != "events_per_1000_simulated_years") {
            keys.push_back(k);
        }
    }
    CHECK(keys == std::vector<std::string>{"events_awakening", "events_civ_founded", "events_settlement_founded",
                                           "events_flare_up", "events_claim", "events_war_declared", "events_battle",
                                           "events_siege", "events_settlement_captured", "events_sacked",
                                           "events_civ_conquered", "events_truce", "events_settlement_abandoned",
                                           "events_ruin_faded", "events_quiet_stretch", "events_omen", "events_tally",
                                           "events_oath", "events_silence", "events_landing"});
}

TEST_CASE("P3-07 6.6, 6.8, 14.1 item 3 on H2: the minimal world, every 'none' form") {
    const std::string s = summary("h2-minimal");
    CHECK(civ_block(s, 1) == std::vector<std::string>{
                                 "c1 The Kingdom of Alird", "  race: Human (ra1)",
                                 "  home: Northeast Downs (rg1), grassland", "  founded: 0 in The Age of Humans (a1), e2",
                                 "  capital: none", "  peak regions: 0", "  peak population: 0",
                                 "  settlements: 0 founded, 0 held at the end, 0 ruined",
                                 "  wars: 0 declared, 0 defended; won 0, lost 0, drawn 0, open 0", "  adversaries: none",
                                 "  events: 1", "  fate: alive in 2000: 0 regions, 0 settlements, population 0",
                                 "  milestones: 0", "  allies: 0", "  rulers: 0 reigns, 0 recorded"});   // P5-04a 12.2 (v9): the count of the shown deeds; P5-04 10.3
    CHECK(contains_line(civ_block(full("h2-minimal"), 1),
                        "    0 e2 civ_founded: The Humans founded the Kingdom of Alird in Northeast Downs."));
    CHECK(block(s, "[phases]").empty());
    CHECK(contains_line(civ_block(full("h2-minimal"), 1), "  relations: none"));
    CHECK(block(full("h2-minimal"), "[phases]") == std::vector<std::string>{"p1 a1 opening 0-149, 150 years; trigger e1; 2 events",
                                                           "p2 a1 final 150-1999, 1850 years; 0 events"});
    CHECK(block(s, "[chronicle]") ==
          std::vector<std::string>{
              "threshold = 20", "events = 2", "shown = 2", "== a1 The Age of Humans: 0-1999",
              "-- p1 opening: 0-149, 150 years; trigger e1; 2 events",
              "0 e1 awakening/100: Wealstuld awakened the Humans in Northeast Downs. {@rg1 race=ra1 creator=g1}",
              "0 e2 civ_founded/80: The Humans founded the Kingdom of Alird in Northeast Downs. {@rg1 civilization=c1 "
              "race=ra1} [causes e1]",
              "-- p2 final: 150-1999, 1850 years; 0 events"});
    CHECK(block(s, "[present]") == std::vector<std::string>{
                                       "year: 2000", "civilizations alive: 1 of 1",
                                       "c1 The Kingdom of Alird: 0 regions, 0 settlements, population 0",
                                       "regions: 2; held 0, free 2",
                                       "settlements: 0; thriving 0, declining 0, ruined 0",
                                       "settlement kinds: hamlet 0, village 0, town 0, city 0, fortress 0",
                                       "largest settlement: none", "ruins: 0", "hidden refuges: 0 of 0 founded by flight",
                                       "open wars: 0", "standing claims: 0", "renamed regions: 0"});
    CHECK(block(full("h2-minimal"), "[present]") ==
          std::vector<std::string>{"year: 2000", "civilizations alive: 1 of 1",
                                   "c1 The Kingdom of Alird: 0 regions, 0 settlements, population 0",
                                   "regions: 2; held 0, free 2", "  rg1 Northeast Downs: free", "  rg2 Isle of Rest: free",
                                   "settlements: 0; thriving 0, declining 0, ruined 0",
                                   "settlement kinds: hamlet 0, village 0, town 0, city 0, fortress 0",
                                   "largest settlement: none", "ruins: 0", "hidden refuges: 0 of 0 founded by flight",
                                   "open wars: 0", "standing claims: 0", "renamed regions: 0", "beasts:"});   // P5-02 12.3
    const auto st = hrtest::stats_of(full("h2-minimal"));
    CHECK(st.at("simulated_years") == 2000);
    CHECK(st.at("simulated_permille") == 1000);
    CHECK(st.at("regions_nonislet") == 1);
    CHECK(st.at("share_max_permille") == 0);
    CHECK(st.at("share_max_civ") == 0);
    CHECK(st.at("share_max_year") == 0);
    CHECK(st.at("civ_population_peak_max") == 0);
    CHECK(st.at("population_settlement_max") == 0);
}

TEST_CASE("P3-07 7.5 on H3, H3b, H3c: islets, runs of 1,001 and 1,000 years, a handover, share_max ties") {
    const auto h3 = hrtest::stats_of(full("h3-runaway"));
    CHECK(h3.at("regions_nonislet") == 11);  // 1172 dry cells is not an islet; 1171 and 1000 are
    CHECK(h3.at("share_max_permille") == 909);
    CHECK(h3.at("share_max_civ") == 2);
    CHECK(h3.at("share_max_year") == 1001);
    CHECK(h3.at("share_over_800_years") == 1001);
    CHECK(h3.at("share_over_900_years") == 500);
    // a run belongs to one civilization: c1 above 500 for 1001 years, then c2 for 1999 (not 3000)
    CHECK(h3.at("share_over_500_years") == 1999);
    CHECK(h3.at("share_over_600_years") == 1001);
    CHECK(h3.at("share_over_700_years") == 1001);
    const auto h3b = hrtest::stats_of(full("h3b-runaway-1000"));
    CHECK(h3b.at("share_over_800_years") == 1000);
    CHECK(h3b.at("share_over_600_years") == 1000);
    CHECK(h3b.at("share_over_900_years") == 500);
    const auto h3c = hrtest::stats_of(full("h3c-share-ties"));
    CHECK(h3c.at("share_max_permille") == 500);
    CHECK(h3c.at("share_max_civ") == 2);  // c2 and c3 in year 0 (the smaller id), c1 only from 100
    CHECK(h3c.at("share_max_year") == 0);
    // the islets c1 holds are regions held, but not land in the shares
    CHECK(civ_block(summary("h3-runaway"), 1)[5] == "  peak regions: 11, 0-1000");
    CHECK(civ_block(summary("h3b-runaway-1000"), 1)[5] == "  peak regions: 11, 0-999");
    CHECK(civ_block(summary("h3-runaway"), 2)[5] == "  peak regions: 10, 1001-1500");
}

TEST_CASE("P3-07 7.4, 21.3, r17 24.3 on H4 and H5: the budget, saturation and the quiet stretch outside the threshold") {
    // H4: 1,000 notes of salience (37 i) mod 101 (e4 ... e1003), H2's two events (100 and 80) and a quiet stretch
    // (e3, salience 20). The notes alone have n(76) = 248 (14.1 item 4); r17's set always holds the quiet
    // stretch, which is no candidate: |F u C(76)| = 1 + 250 = 251 > 250 and |F u C(77)| <= 250, so T = 77.
    std::int64_t n76 = 2, n77 = 2;
    for (std::int64_t i = 1; i <= 1000; ++i) {
        n76 += (37 * i) % 101 >= 76 ? 1 : 0;
        n77 += (37 * i) % 101 >= 77 ? 1 : 0;
    }
    CHECK(n76 == 250);
    CHECK(n77 + 1 == 241);
    const std::string s4 = summary("h4-budget");
    const auto st4 = hrtest::stats_of(s4);
    CHECK(st4.at("summary_threshold") == 77);
    CHECK(st4.at("summary_events") == 241);
    const std::vector<std::string> c4 = block(s4, "[chronicle]");
    CHECK(std::vector<std::string>(c4.begin(), c4.begin() + 3) ==
          std::vector<std::string>{"threshold = 77", "events = 1003", "shown = 241"});
    CHECK(contains_line(c4, "(762 more events not shown)"));
    CHECK(hrtest::stats_of(full("h4-budget")).at("summary_threshold") == 77);  // also at full (9)
    CHECK(block(full("h4-budget"), "[chronicle]")[0] == "threshold = 0");
    std::size_t lines4 = 0;
    for (const std::string& l : c4) lines4 += hrtest::is_event_line(l) ? 1U : 0U;
    CHECK(lines4 == 241U);
    // 24.11 item 4: the quiet stretch is shown at summary, far below T
    CHECK(event_line(s4, 3) == "50-99 e3 quiet_stretch/20: For 50 years, from 50 to 99, the land was quiet; one realm "
                               "held it. {realm=c1 years=50 realms=1}");
    // a shown note: i = 3 has salience 111 mod 101 = 10 (hidden); i = 30 has 1110 mod 101 = 100 (shown)
    CHECK(event_line(s4, 3 + 3).empty());
    CHECK(event_line(s4, 3 + 30) == "100 e33 note/100: Note 30 was written down. {n=30}");
    CHECK(event_line(full("h4-budget"), 3 + 3) == "100 e6 note/10: Note three was written down. {n=3}");
    // H5: 300 heralds of salience 100 and the awakening: no t in [20, 100] keeps the set within 250; the floor
    // takes the age's own race first (24.3 item 3: the awakening and the founding) and the quiet stretch is in F
    const std::string s5 = summary("h5-saturated");
    const auto st5 = hrtest::stats_of(s5);
    CHECK(st5.at("summary_threshold") == 100);
    CHECK(st5.at("summary_events") == 303);
    const std::vector<std::string> c5 = block(s5, "[chronicle]");
    CHECK(std::vector<std::string>(c5.begin(), c5.begin() + 3) ==
          std::vector<std::string>{"threshold = 100", "events = 303", "shown = 303"});
    CHECK(starting(c5, "(").empty());
    CHECK_FALSE(event_line(s5, 2).empty());
    CHECK_FALSE(event_line(s5, 3).empty());
}

TEST_CASE("P3-07 21.4, 22.3, 23 items 3, 4, 4a on H6: the declaration and flare-up notes") {
    const std::string f = full("h6-notes");
    // 23 item 4a: a kind 39 claim with one settlement, on a region its claimant holds
    CHECK(event_line(f, 17) == "1120 e17 war_declared/70: Clan Kuushbam declared war on Finen over Snowscar. (The "
                               "claim, since 1100, was to Ghaldgruz Camp in Snowscar.) {@rg1 attacker=c3 defender=c1} "
                               "[causes e16]");
    // 4a with two settlements, a third realm's region, the same year, the second war of the pair
    CHECK(event_line(f, 24) == "1180 e24 war_declared/70: The Grork Horde declared war on Finen over Bellringer's "
                               "Forest. (The claim, made the same year, was to the Hollow Keep and Tarn in Bellringer's "
                               "Forest. The second war between them.) {@rg2 attacker=c2 defender=c1} [causes e23]");
    // 21.4 item 1: a kind 20 claim of the war's own region, the same year
    CHECK(event_line(f, 21) == "1160 e21 war_declared/70: The Grork Horde declared war on Finen over Greyhold. "
                               "(Claimed the same year.) {@rg6 attacker=c2 defender=c1} [causes e20]");
    // 23 item 3: the claim's region differs, an earlier year, no earlier war; then the same year, a second war
    CHECK(event_line(f, 28) == "1210 e28 war_declared/70: The Drolidrai Throne declared war on Clan Kuushbam over East "
                               "Wood. (The claim, since 1200, was to the Ashen Fields.) {@rg4 attacker=c4 defender=c3} "
                               "[causes e27]");
    CHECK(event_line(f, 31) == "1250 e31 war_declared/70: The Drolidrai Throne declared war on Clan Kuushbam over East "
                               "Wood. (The claim, made the same year, was to Woodward's Shaw. The second war between "
                               "them.) {@rg4 attacker=c4 defender=c3} [causes e30]");
    // ordinals count the pair in either role (e17 was c3 against c1)
    CHECK(event_line(f, 39) == "1410 e39 war_declared/70: Finen declared war on Clan Kuushbam over East Wood. (Claimed "
                               "the same year. The second war between them.) {@rg4 attacker=c1 defender=c3} [causes "
                               "e38]");
    CHECK(event_line(f, 42) == "1440 e42 war_declared/70: Finen declared war on Clan Kuushbam over East Wood. (Claimed "
                               "since 1430. The third war between them.) {@rg4 attacker=c1 defender=c3} [causes e41]");
    CHECK(event_line(f, 45) == "1500 e45 war_declared/70: Finen declared war on Clan Kuushbam over Snowscar. (Claimed "
                               "the same year. The fourth war between them.) {@rg1 attacker=c1 defender=c3} [causes "
                               "e44]");
    // 23 item 4: flare-ups after kind 39 claims of one and of three settlements (a The ... name inside)
    CHECK(event_line(f, 19) == "1150 e19 flare_up/50: In 1150 tensions flared into 50 years of upheaval. (The claim of "
                               "Clan Kuushbam to Ghaldgruz Camp of Finen in Snowscar came to a head.) {years=50} "
                               "[causes e16]");
    CHECK(event_line(f, 37) == "1400 e37 flare_up/50: In 1400 tensions flared into 50 years of upheaval. (The claim of "
                               "Clan Kuushbam to Elmstead, the Hollow Keep and Brackenford of Finen in Bellringer's "
                               "Forest came to a head.) {years=50} [causes e36]");
    // 22.3 / 22.6: a kind 20 claim with a The ... claimant, lower-cased in the note
    CHECK(event_line(f, 34) == "1300 e34 flare_up/50: In 1300 tensions flared into 50 years of upheaval. (The claim of "
                               "the Grork Horde to East Wood, held by Clan Kuushbam, came to a head.) {years=50} "
                               "[causes e33]");
    // history_note (21.4 API) returns the note without its parentheses
    CHECK(worldgen::history::history_note(worldgen::history::HistoryView::load(hrtest::open_world(
                                                                                   hrtest::fixture_world("h6-notes")))
                                              .value(),
                                          34) ==
          "The claim of the Grork Horde to East Wood, held by Clan Kuushbam, came to a head.");
}

TEST_CASE("P3-07 21.2, 21.3, 22.5a, 23 item 2 on H6: companions, searches, hollow wars") {
    const std::string s = summary("h6-notes");
    const auto st = hrtest::stats_of(full("h6-notes"));
    CHECK(st.at("summary_threshold") == 20);
    CHECK(st.at("summary_events") == 39);
    // r17 24.3: no claim at summary (ten claims, e16 ... e44); the flare-ups' and declarations' notes read them
    for (const std::int64_t claim : {16, 20, 23, 27, 30, 33, 36, 38, 41, 44}) {
        CAPTURE(claim);
        CHECK(event_line(s, claim).empty());
        CHECK_FALSE(event_line(full("h6-notes"), claim).empty());
    }
    // the open war's one battle is its last: shown through the war's frame (24.2)
    CHECK_FALSE(event_line(s, 46).empty());
    // the first search (salience 40) and its declaration are shown; the second (15) and its declaration not
    CHECK_FALSE(event_line(s, 39).empty());
    CHECK_FALSE(event_line(s, 40).empty());
    CHECK(event_line(s, 42).empty());
    CHECK(event_line(s, 43).empty());
    // companions below the minimum salience: the peace (18) and its cession of settlements (12) with their
    // declaration; the moved capital (10) and the refugees (15) with the sack
    CHECK(event_line(s, 25) == "1190 e25 peace/18: Finen sued the Grork Horde for peace after 10 years of war. "
                               "{winner=c2 loser=c1 years=10} [causes e24]");
    CHECK(event_line(s, 26) == "1190 e26 settlements_ceded/12: Finen gave up Tarn in Bellringer's Forest to the Grork "
                               "Horde. {@rg2 from=c1 to=c2 settlement=s3} [causes e25]");
    CHECK(event_line(s, 49) == "1511 e49 capital_moved/10: The seat of Clan Kuushbam moved to Kuush Hold. {@rg3 @s8 "
                               "civilization=c3 settlement=s8} [causes e48]");
    CHECK(event_line(s, 50) == "1512 e50 refugees_settled/15: The survivors of Kuush Pit fled into hiding and founded "
                               "Hidden Burrow in East Wood. {@rg4 @s9 settlement=s9 founder=c3 origin=s7 population=30} "
                               "[causes e48]");
    CHECK(contains_line(block(s, "[chronicle]"), "(4 more events not shown)"));  // p8: two claims, e42, e43
    // 21.5, 21.8, 23 item 2: e24's war ceded settlements (not hollow); two wars unfound; the open war sacked
    CHECK(st.at("wars") == 8);
    CHECK(st.at("wars_open") == 1);
    CHECK(st.at("wars_unfound") == 2);
    CHECK(st.at("wars_hollow") == 4);
    CHECK(st.at("wars_hollow_permille") == 666);
    CHECK(st.at("wars_same_goal_max") == 4);
    CHECK(st.at("war_years_total") == 10 + 10 + 10 + 10 + 10 + 10 + 5 + (2000 - 1500));
    CHECK(st.at("war_years_max") == 500);
    // 23 item 1: the kinds 39 and 40 lines in the catalogue's id order, after quiet_stretch
    std::vector<std::string> keys;
    for (const auto& [k, v] : hrtest::stats_list(full("h6-notes"))) {
        if (k.rfind("events_", 0) == 0) keys.push_back(k);
    }
    REQUIRE(keys.size() >= 18U);
    CHECK(std::vector<std::string>(keys.begin() + 15, keys.begin() + 18) ==
          std::vector<std::string>{"events_quiet_stretch", "events_claim_settlements", "events_settlements_ceded"});
    CHECK(st.at("events_claim_settlements") == 3);
    CHECK(st.at("events_settlements_ceded") == 1);
}

TEST_CASE("P3-07 21.8 item 5 with 22.5b on H7: a resettled region keeps its run, a change of holder breaks it") {
    const auto st = hrtest::stats_of(full("h7-runs"));
    CHECK(st.at("wars_same_goal_unchanged_max") == 2);
    CHECK(st.at("wars_same_goal_max") == 3);
    CHECK(st.at("wars_hollow") == 5);
    CHECK(st.at("wars_hollow_permille") == 1000);
}

TEST_CASE("P3-07 5.1: digest_history on H1 (peaks, counts, milestones, relations, threshold)") {
    const auto r = hrtest::open_world(hrtest::fixture_world("h1-war-and-ruin"));
    auto tables = worldgen::history::read_history_tables(r);
    REQUIRE(tables.ok());
    auto view = worldgen::history::HistoryView::load(r);
    REQUIRE(view.ok());
    const worldgen::history::HistoryDigest d = worldgen::history::digest_history(view.value(), tables.value());
    REQUIRE(d.civilizations.size() == 3U);
    const auto& c1 = d.civilizations[0];
    const auto& c2 = d.civilizations[1];
    const auto& c3 = d.civilizations[2];
    CHECK(c1.id == 1);
    CHECK(c2.id == 2);
    CHECK(c3.id == 3);
    CHECK(c1.peak_regions.count == 3);
    CHECK(c1.peak_regions.from_year == 300);
    CHECK(c1.peak_regions.to_year == 1699);
    CHECK(c3.peak_regions.count == 2);
    CHECK(c3.peak_regions.from_year == 2600);
    CHECK(c3.peak_regions.to_year == 2600);
    CHECK(c1.peak_population.population == 1920);
    CHECK(c1.peak_population.year == 2049);
    CHECK(c2.peak_population.population == 450);
    CHECK(c2.peak_population.year == 1649);
    CHECK(c3.peak_population.population == 1000);
    CHECK(c3.peak_population.year == 1999);
    CHECK(c1.settlements_founded == 3);
    CHECK(c1.settlements_held == 3);
    // r19 25.3 item 3 (reading): the digest's ruined count is the block's r, status 3 only; c1's s5 faded
    CHECK(c1.settlements_ruined == 0);
    CHECK(c1.regions_held == 3);
    CHECK(c1.population == 1920);
    CHECK(c1.events == 19);
    CHECK(c2.events == 11);
    CHECK(c3.events == 13);
    // r17 24.5 (MILESTONES 8)
    CHECK(c1.milestones == std::vector<std::int64_t>{2, 3, 22, 24, 27, 29, 30, 33});
    CHECK(c2.milestones == std::vector<std::int64_t>{8, 9, 12, 20, 22, 27, 29, 30});
    CHECK(c3.milestones == std::vector<std::int64_t>{10, 11, 13, 14, 18, 19, 24, 33});
    CHECK(c1.relations.size() == 5U);
    CHECK(c2.relations.size() == 1U);
    CHECK(c3.relations.size() == 3U);
    CHECK(d.summary_threshold == 20);
    // the stats of the digest are the [stats] block, in order
    std::vector<std::pair<std::string, std::int64_t>> stats;
    // P5-04 10.5: a value printed `-` (no ended alliance, no spoils owed) is the digest's INT64_MIN; stats_list
    // reads `-` as -1
    for (const auto& kv : d.stats) {
        stats.emplace_back(kv.first, kv.second == std::numeric_limits<std::int64_t>::min() ? -1 : kv.second);
    }
    CHECK(stats == hrtest::stats_list(full("h1-war-and-ruin")));   // r19 25.7 item 4: full's block
    // H2: fewer than six events of the civilization, and none of the peaks
    const auto r2 = hrtest::open_world(hrtest::fixture_world("h2-minimal"));
    auto t2 = worldgen::history::read_history_tables(r2);
    REQUIRE(t2.ok());
    auto v2 = worldgen::history::HistoryView::load(r2);
    REQUIRE(v2.ok());
    const auto d2 = worldgen::history::digest_history(v2.value(), t2.value());
    REQUIRE(d2.civilizations.size() == 1U);
    CHECK(d2.civilizations[0].milestones == std::vector<std::int64_t>{2});
    CHECK(d2.civilizations[0].peak_regions.count == 0);
    CHECK(d2.civilizations[0].peak_population.population == 0);
}

}  // TEST_SUITE

