// Spec P5-07 (docs/specs/p5-07-mountain-depths.md r1) test 16 items 11 and 12 on fixture H19
// (worldgen/tests/data/history-report/h19-v10.toml, written by reference/p5_07_h19_fixture.py; the spec's "H18",
// named H19 since P5-04a holds H18: reading D-R1): report v10 at both levels: the header (10 item 1); the sentences
// of kinds 93 to 96 and of the changed civ_ended (8.2); `sealed in` (10 item 2); the mine lines' depth and sealing (10
// item 3); the ruins `lost to` (10 item 4); `; soured e<id>` (10 item 5); the milestones at most one per kind (10 item
// 6); the [stats] keys of 10 item 7 in order; the `allies:` lines' report text in v9's form (12 item 4). Then the
// timeline reads of 11.1 (mine_depth_at, sealed_year, the monster state at the waking and sealing years), the browser
// groups and the markers' hold_lost symbol, on H19.
//
// The expected lines are the reference's (`p5_07_h19_fixture.py ... --lines`: p3_report_reference at 0.29.0),
// hand-checked against the fixture's rows (the toll 1206/1810 is 8.2's sample; the per-kind milestones: war_declared
// e20, refugees_settled e41, settlement_founded e6 where v9 would print e20, e34 and e41). One case runs before
// integration (the row invariants of p5_07_checks.hpp on the fixture's rows, read without a history reader); the
// others start with P507_REQUIRE_INTEGRATED() (a 0.28.0 reader refuses wake 3). All skip while the file is absent.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../history_report_checks.hpp"
#include "../history_report_fixture.hpp"
#include "p3_04_engine_api.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_07_api.hpp"
#include "p5_07_checks.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/inspect/history_browser.hpp"
#include "worldgen/inspect/markers.hpp"

namespace {

namespace fs = std::filesystem;
namespace db = worldgen::db;
namespace h = worldgen::history;
namespace in = worldgen::inspect;
using hrtest::HistoryLevel;

const char* const H19 = "h19-v10";

bool present() { return fs::exists(fs::path(UNMAPPED_P3_07_FIXTURE_DIR) / (std::string(H19) + ".toml")); }

const std::string& text(HistoryLevel level) {
    static std::map<int, std::string> cache;
    const int k = static_cast<int>(level);
    if (const auto it = cache.find(k); it == cache.end()) cache[k] = hrtest::report_of(hrtest::fixture_world(H19), level);
    return cache[k];
}
std::vector<std::string> lines(HistoryLevel level) { return hrtest::lines_of(text(level)); }
bool has(const std::vector<std::string>& ls, const std::string& l) { return std::find(ls.begin(), ls.end(), l) != ls.end(); }
std::string starting(const std::vector<std::string>& ls, const std::string& head) {
    for (const std::string& l : ls) {
        if (l.rfind(head, 0) == 0) return l;
    }
    return "";
}

#define SKIP_UNLESS_PRESENT()                                     \
    if (!present()) {                                             \
        MESSAGE("h19-v10.toml absent: skipped");                  \
        return;                                                   \
    }
#define SKIP_UNLESS_READY()                                       \
    P507_REQUIRE_INTEGRATED();                                    \
    SKIP_UNLESS_PRESENT()

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-07 13: the row invariants (p5_07_checks.hpp) hold on H19's rows; its stats are the reference's") {
    // Runs before integration if the fixture builder writes the rows (no history reader): it checks the invariants'
    // code against a hand-made history the reference also passes (p5_07_h19_fixture.py --lines).
    SKIP_UNLESS_PRESENT();
    const db::WorldReader r = hrtest::open_world(hrtest::fixture_world(H19));
    const p503check::World w = p503check::world_of_file(r, conflict_test::shipped());
    p507_check::Tuning t{};
    t.mining = {{"dwarf", 1000}, {"elf", 0}, {"goblin", 0}};
    t.deep_years = {150, 300, 500};   // the fixture's mines deepen after 160, 480 and 1050 worked years
    const std::vector<std::string> v = p507_check::violations(w, t);
    // H19 is a report fixture: it has no region_border rows, so item 5's reach of the later take is not shown; and
    // before integration the fixture builder derives settlement.ending without 6.6's rule (hold_lost -> 2)
    std::vector<std::string> other;
    for (const std::string& s : v) {
        const bool ending = !p507api::INTEGRATED && s.find("with ending 2") != std::string::npos;
        if (s.find("out of reach") == std::string::npos && !ending) other.push_back(s);
    }
    CHECK(other == std::vector<std::string>{});
    const std::vector<std::pair<std::string, std::int64_t>> want = {
        {"mines_deepened", 7}, {"mines_deepest", 1}, {"deep_woken", 3}, {"deep_sealed", 1}, {"holds_lost", 3},
        {"deep_flights", 1}, {"realms_lost_to_deep", 1}, {"deep_alive", 3}};
    CHECK(p507_check::stats(w) == want);
    CHECK(p507_check::mine_depth_at(w, 1, 1449) == 2);
    CHECK(p507_check::mine_depth_at(w, 1, 1450) == 3);
}

TEST_CASE("P5-07 10 item 1: H19 loads through both readers; the header is v10 at both levels") {
    SKIP_UNLESS_READY();
    const db::WorldReader r = hrtest::open_world(hrtest::fixture_world(H19));
    CHECK_FALSE(eng::view_error(r).has_value());
    const auto t = h::read_history_tables(r);
    REQUIRE_MESSAGE(t.ok(), (t.ok() ? std::string() : t.error().message));
    REQUIRE_FALSE(lines(HistoryLevel::full).empty());
    CHECK(lines(HistoryLevel::full)[0] == p507api::REPORT_HEADER_SPEC);
    CHECK(lines(HistoryLevel::summary)[0] == p507api::REPORT_HEADER_SPEC);
}

TEST_CASE("P5-07 8.2: the sentences of kinds 93 to 96 and of the changed civ_ended (full chronicle)") {
    SKIP_UNLESS_READY();
    const std::vector<std::string> f = lines(HistoryLevel::full);
    for (const char* l : {
             "560 e16 delving_deepened/15: By hammer and lamp, the Hold of Durn delved the Mines of Gimlin below the "
             "reach of daylight.",
             "900 e26 delving_deepened/15: Following a rich seam, the Hold of Durn delved the Mines of Gimlin into the "
             "roots of the mountain.",
             "1450 e36 delving_deepened/15: Year upon year, the Hold of Durn delved the Mines of Gimlin into the oldest "
             "dark beneath the world.",
             "1150 e29 deep_woken/60: In the deepest halls under Kazad Rune, the Hold of Durn woke Bolgrim the "
             "Deep-Coiled, a wyrm of the lightless deep.",
             "1200 e31 deep_woken/60: Where the last seam ran out beneath Kazmar, Clan Nirgurn broke into the old dark "
             "and woke the Swarm Beneath Durak, a host of eyeless things from under the stone.",
             "1500 e37 deep_woken/60: Delving too deep beneath Gimlin, the Hold of Durn woke Ugrak the Unquenched, a "
             "fire that walks in the roots of the world.",
             "1150 e30 deep_sealed/50: With stone and rune, the Hold of Durn shut Bolgrim the Deep-Coiled away below "
             "the Mines of Kazad Rune, and the deep ways were closed.",
             "1200 e32 hold_lost/70: The Swarm Beneath Durak broke out of the deeps into Kazmar of Clan Nirgurn; 480 "
             "died, and 720 fled.",
             "1600 e40 hold_lost/70: After the raids, Ugrak the Unquenched broke out of the deeps into Kazad Rune of the "
             "Hold of Durn; 1,206 died, and 1,810 fled.",
             "1201 e33 civ_ended/85: After the loss of its last hold to the deep, Clan Nirgurn came to an end.",
             "1600 e41 refugees_settled/65: The survivors of Kazad Rune fled into hiding and founded Brunhal in Brun "
             "Heights."}) {
        const std::string head(l);
        CAPTURE(head);
        const std::string got = starting(f, head.substr(0, head.find(' ', head.find(' ') + 1) + 1));
        CHECK(got.rfind(head, 0) == 0);
    }
}

TEST_CASE("P5-07 10 items 2 to 5: sealed in, a mine's depth and sealing, lost to, soured") {
    SKIP_UNLESS_READY();
    const std::vector<std::string> f = lines(HistoryLevel::full);
    CHECK(has(f, "  m1 Bolgrim the Deep-Coiled: Ymrach, sealed in The Coils of Bolgrim (si4), lair The Coils of Bolgrim "
                 "(si4) in Kazad Fells (rg2), since 1150 e29, raids 0, killed 0, hoard 0"));
    CHECK(has(f, "monsters: 3; awake 2, asleep 1, slain 0, died 0"));   // a sealed monster counts as asleep
    CHECK(has(f, "  si1 The Mines of Gimlin: mine in Iron Downs (rg5) at 50 50, since 400 e13, of The Hold of Durn "
                 "(c2), at Gimlin (s5), depth 3; ended 1500 e38 lost"));
    CHECK(has(f, "  si3 The Mines of Kazad Rune: mine in Kazad Fells (rg2) at 20 20, since 450 e15, of The Hold of Durn "
                 "(c2), at Kazad Rune (s2), depth 2, sealed 1150 e30; ended 1600 e40 lost"));
    CHECK(has(f, "  si5 The Warrens of Durak: lair in The Grey Fells (rg3) at 30 30, since 1200 e31, at Kazmar (s3); open"));
    for (const HistoryLevel lv : {HistoryLevel::summary, HistoryLevel::full}) {
        const std::vector<std::string> ls = lines(lv);
        CHECK(has(ls, "  s2 Kazad Rune in Kazad Fells (rg2): ruined 1600, e40 lost to Ugrak the Unquenched (m3); last "
                      "held by The Hold of Durn (c2)"));
        CHECK(has(ls, "  s3 Kazmar in The Grey Fells (rg3): ruined 1200, e32 lost to The Swarm Beneath Durak (m2); last "
                      "held by Clan Nirgurn (c3)"));
        CHECK(has(ls, "  c1 The House of Aelin and c4 The Gub Horde: 700-E, in friendship, e19; renewed 8; joined 1, "
                      "refused 0; spoils 0 granted, 1 withheld; soured e25"));
    }
}

TEST_CASE("P5-07 10 item 6, 12 item 4: milestones at most one per kind; the allies: lines in v9's form") {
    SKIP_UNLESS_READY();
    const std::vector<std::string> c2 = hrtest::civ_block(text(HistoryLevel::summary), 2);
    const auto at = std::find(c2.begin(), c2.end(), "  milestones: 5");
    REQUIRE(at != c2.end());
    REQUIRE(c2.end() - at >= 4);
    CHECK(*(at + 1) == "    100 e6 settlement_founded: The Hold of Durn founded Kazad Rune in Kazad Fells.");
    CHECK(*(at + 2) == "    800 e20 war_declared: The Hold of Durn declared war on the Gub Horde over Gor Heath.");
    CHECK(*(at + 3) == "    1600 e41 refugees_settled: The survivors of Kazad Rune fled into hiding and founded Brunhal "
                       "in Brun Heights.");
    const std::vector<std::string> s = lines(HistoryLevel::summary);
    CHECK(has(s, "  allies: 1; at the end: c4 The Gub Horde"));
    CHECK(has(s, "  allies: 1; at the end: c1 The House of Aelin"));
    const std::vector<std::string> f = lines(HistoryLevel::full);
    CHECK(has(f, "    c4 The Gub Horde: 700-E, in friendship"));
}

TEST_CASE("P5-07 10 item 7: the eight [stats] keys appended in order, with H19's values") {
    SKIP_UNLESS_READY();
    const auto st = hrtest::stats_list(text(HistoryLevel::full));
    REQUIRE(st.size() >= 8U);
    const std::vector<std::pair<std::string, std::int64_t>> want = {
        {"mines_deepened", 7}, {"mines_deepest", 1}, {"deep_woken", 3}, {"deep_sealed", 1}, {"holds_lost", 3},
        {"deep_flights", 1}, {"realms_lost_to_deep", 1}, {"deep_alive", 3}};
    CHECK(std::vector<std::pair<std::string, std::int64_t>>(st.end() - 8, st.end()) == want);
}

TEST_CASE("P5-07 11.1, 16 item 12: the timeline's mine_depth_at and sealed_year, the monster state; groups; symbol") {
    SKIP_UNLESS_READY();
    const db::WorldReader r = hrtest::open_world(hrtest::fixture_world(H19));
    auto lh = h::load_history(r);
    REQUIRE_MESSAGE(lh.ok(), (lh.ok() ? "" : lh.error().message));
    const h::Timeline& t = lh.value().timeline;
    // si1: depth 1 in 560, 2 in 900, 3 in 1450
    for (const auto& [y, d] : std::vector<std::pair<std::int64_t, std::int64_t>>{
             {0, 0}, {559, 0}, {560, 1}, {899, 1}, {900, 2}, {1449, 2}, {1450, 3}, {1999, 3}}) {
        CAPTURE(y);
        CHECK(p507api::mine_depth_at(t, 1, y) == d);
    }
    CHECK(p507api::mine_depth_at(t, 4, 1999) == 0);   // a lair
    CHECK(p507api::sealed_year(t, 1) == 1150);
    CHECK(p507api::sealed_year(t, 2) == -1);
    // P5-02 13 with 94 and 95: awake from the waking, asleep from the sealing (the state at the end of Y)
    CHECK_FALSE(t.monster_at(1, 1149).alive);
    CHECK(t.monster_at(1, 1150).alive);
    CHECK_FALSE(t.monster_at(1, 1150).awake);   // woken and sealed in 1150
    CHECK(t.monster_at(2, 1200).awake);
    CHECK(t.monster_at(3, 1999).awake);
    CHECK(in::event_group("delving_deepened") == in::EventGroup::settlements);
    for (const char* k : {"deep_woken", "deep_sealed", "hold_lost"}) {
        CAPTURE(k);
        CHECK(in::event_group(k) == in::EventGroup::monsters);
    }
    CHECK(in::ALL_GROUPS == 0x1FFU);
    CHECK(in::event_symbol("hold_lost") == in::EventSymbol::sack);
    CHECK(in::event_symbol("deep_woken") == in::EventSymbol::plain);
    CHECK(in::event_symbol("deep_sealed") == in::EventSymbol::plain);
    CHECK(in::event_symbol("delving_deepened") == in::EventSymbol::plain);
}

}  // TEST_SUITE
