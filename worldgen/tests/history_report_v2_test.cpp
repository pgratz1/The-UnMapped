// Spec P3-07 r19 section 25 (the history report's format v2, P5-01, generator 0.21.0), the tests of 25.13 items 1 to
// 10 on the fixture H11 (worldgen/tests/data/history-report/h11-v2.toml; 25.13 item 11 calls it "H9 v2", and H9 is
// r17's h9-realms), against lines written here from the fixture rows and the spec, cross-checked with the test
// writer's reference (reference/p3_report_reference.py, check_fixture_facts_v2). The H11 rows: two ages (humans
// 0-1499, orcs 1500-2999); p2 holds five calamity candidates of saliences 45 (e26 plague, e27 plague_outbreak), 40
// (e25 succession_crisis) and 25 (e23, e24 famines), p1 one (e15 famine); 300 chants of salience 100 in p6 make T =
// 100; the flare-up e38 (p4) has two declarations in its phase, e44 (p5) none; Arn (c1) declares on Bel (c2) in 1650
// citing the fresh claim e39 (caused by the claim e31 of 1300 on Bel Moor), peace in 1656, and on Cor (c3) in 1660
// citing the fresh kind 39 claim e42 (caused by the claim e32 of 1400); Esk (c5) ends in 1200; the claim e37 of Fen
// (c6) on Dun (c4) is dropped by e45 (kind 41); refuges s16 (kind 42 to 2999 = E - 1), s17 (to 1990) and s18 (none).
//
// Written from the spec only, before the implementation (test-writer rule): every case fails until the report is v2.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
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
std::string summary() { return text("h11-v2", HistoryLevel::summary); }
std::string full() { return text("h11-v2", HistoryLevel::full); }

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

// The lines from the line equal to `head` (inclusive), `n` of them.
std::vector<std::string> from(const std::vector<std::string>& lines, const std::string& head, std::size_t n) {
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (lines[i] == head) {
            const std::size_t end = i + n < lines.size() ? i + n : lines.size();
            return std::vector<std::string>(lines.begin() + static_cast<std::ptrdiff_t>(i),
                                            lines.begin() + static_cast<std::ptrdiff_t>(end));
        }
    }
    return {};
}

std::string event_line(const std::string& report, std::int64_t id) {
    for (const std::string& l : block(report, "[chronicle]")) {
        if (hrtest::is_event_line(l) && hrtest::event_id_of(l) == id) return l;
    }
    return {};
}

}  // namespace

TEST_SUITE("history-report") {

TEST_CASE("P3-07 r19 25.13 item 1: the header is `# worldgen history v2` at both levels") {
    // the spec names no library constant for the header: the text is checked
    for (const std::string& t : {summary(), full()}) {
        const std::vector<std::string> lines = hrtest::lines_of(t);
        REQUIRE(!lines.empty());
        CHECK(lines[0] == "# worldgen history v2");
    }
    CHECK(hrtest::lines_of(text("h2-minimal", HistoryLevel::summary))[0] == "# worldgen history v2");
    CHECK(hrtest::lines_of(text("h2-minimal", HistoryLevel::full))[0] == "# worldgen history v2");
}

TEST_CASE("P3-07 r19 25.13 item 2: at most two calamity candidates per phase (25.2)") {
    const auto st = hrtest::stats_of(summary());
    CHECK(st.at("summary_threshold") == 100);
    const std::set<std::int64_t> shown = hrtest::shown_ids(summary());
    // p2: the two of 45 stay (e26 before e27 by id; both in the age floor, which takes them before the foundings of
    // 40); e25 (40), e23 and e24 (25) are no candidates, so neither the floor nor any threshold shows them
    CHECK(shown.count(26) == 1U);
    CHECK(shown.count(27) == 1U);
    for (const std::int64_t e : {23, 24, 25}) {
        CAPTURE(e);
        CHECK(shown.count(e) == 0U);
    }
    // no phase shows more than two calamities (25.12 item 1)
    const auto r = hrtest::open_world(hrtest::fixture_world("h11-v2"));
    const hrtest::Wars w(r);
    CHECK(hrtest::v2_summary_violations(w.view(), shown) == std::vector<std::string>{});
}

TEST_CASE("P3-07 r19 25.13 item 3: ruins in words; faded settlements apart (25.3)") {
    const std::vector<std::string> p = block(summary(), "[present]");
    CHECK(from(p, "ruins: 6", 7) ==
          std::vector<std::string>{
              "ruins: 6",
              "  s3 Belmoor in Bel Moor (rg3): ruined 1800, e47 sacked by Arn (c1); last held by Bel (c2)",
              "  s6 Eskby in Esk Heath (rg7): ruined 1190, e29 abandoned; last held by Esk (c5)",
              "  s9 Dunlea in The Holt of Yews (rg6): ruined 1820, e49 abandoned; last held by Dun (c4)",
              // a sack with no attacker participant
              "  s10 Dunsike in Dun Sike (rg9): ruined 1810, e48 sacked; last held by Dun (c4)",
              "  s14 Fenwold in Fen Hollow (rg5): ruined 1830, e50 abandoned, survivors to Fenmouth (s7); last held by "
              "Fen (c6)",
              // a kind the spec does not name: its key
              "  s15 Grokmire in Grok Fell (rg8): ruined 1850, e52 swallowed; last held by Grok (c7)"});
    CHECK(from(block(full(), "[present]"), "ruins: 6", 7) == from(p, "ruins: 6", 7));
    // Esk (c5) founded Eskby (ruined) and Eskmere (faded); Dun (c4) three, two ruined
    CHECK(contains_line(civ_block(summary(), 5), "  settlements: 2 founded, 0 held at the end, 1 ruined, 1 faded"));
    CHECK(contains_line(civ_block(full(), 5), "  settlements: 2 founded, 0 held at the end, 1 ruined, 1 faded"));
    CHECK(contains_line(civ_block(summary(), 4), "  settlements: 3 founded, 1 held at the end, 2 ruined, 0 faded"));
    // the full settlement line keeps e<ruin_event> and adds nothing (25.3 item 2)
    CHECK(contains_line(block(full(), "[present]"),
                        "  s10 Dunsike: hamlet, ruined, population 0, no owner, at 100 100 in Dun Sike (rg9), founded "
                        "310 by Dun (c4), ruined 1810, e48, last held by Dun (c4)"));
}

TEST_CASE("P3-07 r19 25.13 item 4: adversary lines for a war only, one count line for the others (25.4)") {
    // Arn: Bel and Cor at war; Dun and Fen in enmity, Esk none (its enmity closed at its end)
    CHECK(sub_lines(civ_block(summary(), 1), "  adversaries: 5") ==
          std::vector<std::string>{"    Bel (c2): 1 war, won 1, lost 0, drawn 0, open 0, 1650-1656; now at peace",
                                   "    Cor (c3): 1 war, won 0, lost 0, drawn 0, open 1, 1660-2999; now at war",
                                   "    without war: 3 (enmity 2, none 1)"});
    // Dun: only a no-war adversary: the count line and the without-war line
    const std::vector<std::string> c4 = civ_block(summary(), 4);
    CHECK(contains_line(c4, "  adversaries: 1"));
    CHECK(sub_lines(c4, "  adversaries: 1") == std::vector<std::string>{"    without war: 1 (enmity 1, none 0)"});
    // Grok: none
    CHECK(contains_line(civ_block(summary(), 7), "  adversaries: none"));
    // full keeps its relation rows
    CHECK(contains_line(civ_block(full(), 1), "  relations: 6 (enmity 3, peace 1, war 2)"));
}

TEST_CASE("P3-07 r19 25.13 item 5: an ended realm in short at summary, unchanged at full (25.5)") {
    CHECK(civ_block(summary(), 5) ==
          std::vector<std::string>{
              "c5 Esk", "  race: Human (ra1)", "  founded: 0 in The Age of Humans (a1), e10",
              "  last capital: Eskmere (s11)", "  settlements: 2 founded, 0 held at the end, 1 ruined, 1 faded",
              "  wars: 0 declared, 0 defended; won 0, lost 0, drawn 0, open 0", "  adversaries: 1",
              "    without war: 1 (enmity 0, none 1)", "  fate: ended in 1200, e30: Esk came to an end.", "  milestones:",
              // four: the founding, the ending, the longest quiet stretch, then the fill (its own founding of Eskby)
              "    0 e10 civ_founded: The Humans founded Esk in Esk Heath.",
              "    0 e11 settlement_founded: Esk founded Eskby in Esk Heath.",
              "    150-1499 e16 quiet_stretch: For 1,350 years, from 150 to 1499, the land was quiet, and six realms "
              "kept the peace. (Esk: 54 new generations of rulers.)",
              "    1200 e30 civ_ended: Esk came to an end."});
    const std::vector<std::string> f5 = civ_block(full(), 5);
    CHECK(contains_line(f5, "  home: Esk Heath (rg7), grassland"));
    CHECK(contains_line(f5, "  peak regions: 1, 0-1199"));
    CHECK(contains_line(f5, "  peak population: 200 in 600"));
    CHECK(contains_line(f5, "  events: 6"));
    CHECK(sub_lines(f5, "  milestones:").size() == 6U);   // all six events of E_c (MILESTONES 8 at full)
    // an alive block is unchanged at summary
    CHECK(contains_line(civ_block(summary(), 1), "  home: Arn Vale (rg1), grassland"));
    CHECK(contains_line(civ_block(summary(), 1), "  events: 13"));
}

TEST_CASE("P3-07 r19 25.13 item 6: every flare-up with a war in its phase is shown, at T = 100 too (25.6)") {
    const std::string s = summary();
    CHECK(event_line(s, 38) == "1650 e38 flare_up/50: In 1650 tensions flared into 50 years of upheaval. (The claim of "
                               "Arn to Bel Moor, held by Bel, came to a head.) {years=50} [causes e31]");
    CHECK(event_line(s, 44).empty());   // p5 has no war_declared; 50 < T
    CHECK_FALSE(event_line(full(), 44).empty());
}

TEST_CASE("P3-07 r19 25.13 item 7: one end year per war; colonies and kind changes before calamities; [stats] (25.7)") {
    // the war ended by the peace of 1656: 1650-1656 in the adversary line, 1650-1655 in the full relation line
    CHECK(contains_line(civ_block(summary(), 2),
                        "    Arn (c1): 1 war, won 0, lost 1, drawn 0, open 0, 1650-1656; now at peace"));
    CHECK(contains_line(civ_block(full(), 1), "    war: Arn (c1), Bel (c2), 1650-1655, e40 to e41"));
    // Dun, no war: the fill key (own, cal, salience, id) takes its colonies, then its claim, sack and abandonment
    // (own 0, cal 0) before the plague e26 and the outbreak e27 of 45 and the crisis e25 of 40 (cal 1)
    CHECK(sub_lines(civ_block(summary(), 4), "  milestones:") ==
          std::vector<std::string>{
              "    0 e8 civ_founded: The Humans founded Dun in the Holt of Yews.",
              "    0 e9 settlement_founded: Dun founded Dunholm in the Holt of Yews.",
              "    150-1499 e16 quiet_stretch: For 1,350 years, from 150 to 1499, the land was quiet, and six realms "
              "kept the peace. (Dun: 54 new generations of rulers.)",
              "    300 e18 colony_founded: Dun sent settlers from Dunholm to found Dunlea in the Holt of Yews.",
              "    310 e19 colony_founded: Dun sent settlers from Dunholm to found Dunsike in Dun Sike.",
              "    1600 e37 claim: Fen laid claim to the Holt of Yews, held by Dun.",
              "    1810 e48 sacked: Dunsike of Dun was sacked; 50 died and 94 fled.",
              "    1820 e49 settlement_abandoned: Dunlea was abandoned; no one survived."});
    // [stats] at summary: exactly the 15 keys of SUMMARY_STATS, in section 9's order, with full's values
    std::vector<std::string> keys;
    for (const auto& [k, v] : hrtest::stats_list(summary())) keys.push_back(k);
    CHECK(keys == std::vector<std::string>{"end_year", "events", "summary_threshold", "summary_events", "civilizations",
                                           "civilizations_alive", "civilizations_ended", "settlements_standing",
                                           "settlements_ruined", "population", "share_max_permille", "wars",
                                           "wars_open", "races_alive", "wars_hollow_permille"});
    const auto sf = hrtest::stats_of(full());
    for (const auto& [k, v] : hrtest::stats_list(summary())) {
        CAPTURE(k);
        CHECK(sf.at(k) == v);
    }
    CHECK(sf.count("events_claim_dropped") == 1U);
    CHECK(sf.at("events_claim_dropped") == 1);
    CHECK(sf.at("events_refuge_hidden") == 2);
    CHECK(hrtest::stats_list(full()).size() > 15U);
}

TEST_CASE("P3-07 r19 25.13 item 8: the claim note dates the grievance from the root claim (25.8)") {
    const std::string f = full();
    CHECK(event_line(f, 40) == "1650 e40 war_declared/70: Arn declared war on Bel over Bel Downs. (Claimed since 1300.) "
                               "{@rg2 attacker=c1 defender=c2} [causes e39]");
    // a fresh claim of kind 39: 23 item 4a's form with the fresh claim's settlements and region, the root's year
    CHECK(event_line(f, 43) == "1660 e43 war_declared/70: Arn declared war on Cor over Fen Hollow. (The claim, since "
                               "1400, was to Corbridge in Fen Hollow.) {@rg5 attacker=c1 defender=c3} [causes e42]");
    // a claim on another region than the war's (no fresh claim, as a 0.20.0 world has) keeps 23 item 3's form (H6)
    CHECK(event_line(text("h6-notes", HistoryLevel::full), 28) ==
          "1210 e28 war_declared/70: The Drolidrai Throne declared war on Clan Kuushbam over East Wood. (The claim, since "
          "1200, was to the Ashen Fields.) {@rg4 attacker=c4 defender=c3} [causes e27]");
    // the fresh claims are claims: never in the summary chronicle (24.3), every one at full
    CHECK(event_line(summary(), 39).empty());
    CHECK(event_line(summary(), 42).empty());
    CHECK_FALSE(event_line(f, 39).empty());
}

TEST_CASE("P3-07 r19 25.13 item 9: refuges marked hidden or found; dropped claims are not standing (25.9)") {
    const std::vector<std::string> p = block(summary(), "[present]");
    CHECK(from(p, "refuges: 3", 4) ==
          std::vector<std::string>{
              "refuges: 3",
              "  s16 Hidden Hole in Dun Sike (rg9): Grok (c7), population 150, founded 1850 by flight from Grokmire "
              "(s15), e53; hidden",
              "  s17 Found Hole in Bel Moor (rg3): Grok (c7), population 150, founded 1860 by flight from Belmoor (s3), "
              "e55; found 1991",
              "  s18 Plain Hole in Grok Fell (rg8): Grok (c7), population 100, founded 1870 by flight from Fenwold "
              "(s14), e57"});
    // e37 is closed by its claim_dropped (test (iv)); e31 and e32 by the declarations of their pairs (ii); the fresh
    // claims are pursued; e58 stands
    CHECK(from(p, "standing claims: 1", 2) ==
          std::vector<std::string>{"standing claims: 1", "  e58 2000 Cor (c3) on Arn (c1): Arn Vale (rg1)"});
    CHECK(from(block(full(), "[present]"), "standing claims: 1", 2) == from(p, "standing claims: 1", 2));
    // kinds 41 and 42 are in the full chronicle only
    CHECK(event_line(summary(), 45).empty());
    CHECK(event_line(summary(), 54).empty());
    CHECK(event_line(full(), 45) == "1720 e45 claim_dropped/15: Fen let the claim against Dun over the Holt of Yews "
                                    "lapse. {@rg6 claimant=c6 holder=c4} [causes e37]");
    CHECK(event_line(full(), 54) == "1850-2999 e54 refuge_hidden/10: Hidden Hole lay hidden from 1850 to 2999. {@rg9 "
                                    "@s16 settlement=s16 founder=c7} [causes e53]");
}

TEST_CASE("P3-07 r19 25.13 item 10: the invariants of 25.12 on H11") {
    const auto r = hrtest::open_world(hrtest::fixture_world("h11-v2"));
    hrtest::check_invariants(r, summary(), full());
}

}  // TEST_SUITE
