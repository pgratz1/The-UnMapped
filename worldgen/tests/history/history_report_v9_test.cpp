// Spec P5-04a (docs/specs/p5-04a-relations-alliances-followups.md r2) test 16 items 10 to 12: report v9 on fixture
// H18 (worldgen/tests/data/history-report/h18-v9.toml, written by reference/p5_04a_h18_fixture.py; the spec's "H17",
// named H18 since P5-06 holds H17), both levels: the header (12.1); the sentences of the changed templates on its
// events (3.6, 4.4, 5, 6.2, 7.1, 9.1, 9.2, 10 item 4); the [alliances] lines of 12.3 (the foe named, `renewed <k>` by
// 6.3, spoils by id) and the trade lines of 12.5 (`; ended`); the `allies:` lines at summary with names; one line per
// same-day group (12.4); the milestones at summary (12.2); the [stats] keys of 12.6. Then the reader checks of 14.4 on
// corrupt copies of H18 (both readers, the same first message: 11 item 2).
//
// The expected lines are the reference's (`p5_04a_h18_fixture.py ... --lines`: p3_engine_reference.render at 0.28.0
// and p5_04_alliances_reference's alliance_line_v9, trade_line_v9, civ_alliance_lines_v9, stats_028), hand-checked
// against the fixture's rows (the renewal counts: (end - 1 - since) div 150). The milestone lines are checked for
// their count line and shape only (the reference has no v9 report; P3-07's `own` set is the digest's). Every case
// starts with P504A_REQUIRE_INTEGRATED() (a 0.27.0 reader refuses the fixture's reason 2) and skips while the file is
// absent.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <tuple>
#include <string>
#include <vector>

#include "../history_report_checks.hpp"
#include "../history_report_fixture.hpp"
#include "p3_04_engine_api.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_04a_api.hpp"
#include "p5_04a_checks.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/history/digest.hpp"

namespace {

namespace fs = std::filesystem;
namespace db = worldgen::db;
namespace h = worldgen::history;
using hrtest::HistoryLevel;

const char* const H18 = "h18-v9";

bool present() { return fs::exists(fs::path(UNMAPPED_P3_07_FIXTURE_DIR) / (std::string(H18) + ".toml")); }

const std::string& text(HistoryLevel level) {
    static std::map<int, std::string> cache;
    const int k = static_cast<int>(level);
    if (const auto it = cache.find(k); it == cache.end()) cache[k] = hrtest::report_of(hrtest::fixture_world(H18), level);
    return cache[k];
}
std::vector<std::string> lines(HistoryLevel level) { return hrtest::lines_of(text(level)); }

bool has(const std::vector<std::string>& ls, const std::string& l) { return std::find(ls.begin(), ls.end(), l) != ls.end(); }

// The chronicle line of event e: "<year> e<id> <key>/<salience>: <sentence>".
std::string event_line(const std::vector<std::string>& ls, std::int64_t year, std::int64_t e) {
    const std::string head = std::to_string(year) + " e" + std::to_string(e) + " ";
    for (const std::string& l : ls) {
        if (l.rfind(head, 0) == 0) return l;
    }
    return "";
}

#define SKIP_UNLESS_READY()                                       \
    P504A_REQUIRE_INTEGRATED();                                   \
    if (!present()) {                                             \
        MESSAGE("h18-v9.toml absent: skipped");                   \
        return;                                                   \
    }

std::string corrupt(const std::string& label, const std::string& sql) {
    const std::string path = hrtest::fixture_copy(H18, label);
    REQUIRE_MESSAGE(db::testing::execute_on_file(path, sql).ok(), "SQL rejected: " << sql);
    return path;
}

void refused(const std::string& label, const std::string& sql, const std::string& message) {
    CAPTURE(label);
    const db::WorldReader r = hrtest::open_world(corrupt(label, sql));
    const auto e = eng::view_error(r);
    REQUIRE_MESSAGE(e.has_value(), "HistoryView::load accepted " << label);
    CHECK(e->code == db::ErrorCode::corrupt_layer);
    CHECK(e->message == message);
    const auto t = h::read_history_tables(r);
    REQUIRE_FALSE(t.ok());
    CHECK(t.error().message == e->message);
}

std::string set_value(std::int64_t e, std::int64_t seq, std::int64_t v) {
    return "UPDATE event_value SET value = " + std::to_string(v) + " WHERE event = " + std::to_string(e) +
           " AND seq = " + std::to_string(seq) + ";";
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-04a 16 item 13: the row invariants (p5_04a_checks.hpp) hold on H18's rows; its values cover the codes") {
    // Runs before integration: the rows are read as stored (no history reader), so this checks the invariants'
    // code against a hand-made history the reference also passes (p5_04a_h18_fixture.py --lines).
    if (!present()) {
        MESSAGE("h18-v9.toml absent: skipped");
        return;
    }
    const db::WorldReader r = hrtest::open_world(hrtest::fixture_world(H18));
    const p503check::World w = p503check::world_of_file(r, conflict_test::shipped());
    const p504a_check::Tuning t{};   // the spec's shipped values (refusals_to_break 2, weary_years 50, ...)
    CHECK(p504a_check::alliance_violations(w, t) == std::vector<std::string>{});
    CHECK(p504a_check::relation_violations(w, t) == std::vector<std::string>{});
    const auto cov = p504a_check::value_coverage(w);
    CHECK(cov.at("ally_refused.motive") == std::set<std::int64_t>{2, 3, 4});
    CHECK(cov.at("spoils_withheld.grievance") == std::set<std::int64_t>{1, 2});
    CHECK(cov.at("alliance_spurned.reason") == std::set<std::int64_t>{0, 2});
    CHECK(cov.at("disposition_turned.change") == std::set<std::int64_t>{1, 2});
}

TEST_CASE("P5-04a 12.1, 16 item 12: H18 loads through both readers; the header is v9 at both levels") {
    SKIP_UNLESS_READY();
    const db::WorldReader r = hrtest::open_world(hrtest::fixture_world(H18));
    CHECK_FALSE(eng::view_error(r).has_value());
    const auto t = h::read_history_tables(r);
    REQUIRE_MESSAGE(t.ok(), (t.ok() ? std::string() : t.error().message));
    REQUIRE_FALSE(lines(HistoryLevel::full).empty());
    CHECK(lines(HistoryLevel::full)[0] == p504a_api::REPORT_HEADER_SPEC);
    CHECK(lines(HistoryLevel::summary)[0] == p504a_api::REPORT_HEADER_SPEC);
}

TEST_CASE("P5-04a 3.6, 4.4, 5, 6.2, 7.1, 9.2, 10 item 4: the changed templates on H18's events (full)") {
    SKIP_UNLESS_READY();
    const std::vector<std::string> f = lines(HistoryLevel::full);
    const std::vector<std::tuple<std::int64_t, std::int64_t, std::string>> want = {
        {605, 42, "spoils_withheld/35: The Hold of Durn kept the spoils of the Grey Fells, 200 captives, from the "
                  "Kingdom of Cear, its ally, which did not forget it."},
        {630, 43, "alliance_renewed/10: After the war they fought together, the Hold of Durn and the Kingdom of Cear "
                  "renewed their alliance against the Gub Horde."},
        {700, 47, "alliance_formed/50: After the outbreak of war, the House of Aelin and the Gub Horde swore to stand "
                  "together against the Skreegg Horde, though their peoples hated one another."},
        {800, 49, "ally_refused/30: Fearing the strength of the foe, the Urarz Horde would not take up arms for the "
                  "Skreegg Horde against the Kingdom of Cear, and its ally did not forget it."},
        {802, 50, "battle/30: With new-bought arms, the Kingdom of Cear defeated the Skreegg Horde in Skree Moor; 120 "
                  "of the defeated and 40 of the victors fell."},
        {820, 53, "ally_refused/30: Grown cold toward its ally, the Urarz Horde would not take up arms for the Skreegg "
                  "Horde against the Hold of Durn."},
        {1000, 60, "ally_refused/30: With a war of its own to fight, the Hold of Durn would not take up arms for the "
                   "Kingdom of Cear against the Gub Horde."},
        {1000, 61, "disposition_turned/55: After long years of peace, the Elves and the Goblins put aside their hatred "
                   "of one another."},
        {1152, 67, "allied_battle/35: The Kingdom of Cear, with the House of Aelin, defeated the Skreegg Horde in Cear "
                   "Downs; 300 of the defeated and 30 of the victors fell."},
        {1200, 71, "disposition_turned/55: After long years apart, the Elves and the Dwarves grew wary of one another."},
        {1250, 73, "alliance_spurned/50: Still bitter that the spoils of Cear Downs had been kept from it 90 years "
                   "before, the House of Aelin broke its alliance with the Kingdom of Cear."},
        {1310, 80, "alliance_spurned/50: Outraged that the spoils of Gor Heath had been kept from it, the Urarz Horde "
                   "broke its alliance with the Gub Horde."},
        {1600, 87, "alliance_betrayed/65: Seeing its ally weak, the Skreegg Horde broke an alliance of 200 years with "
                   "the Gub Horde."}};
    for (const auto& [year, e, tail] : want) {
        CAPTURE(e);
        // at full a chronicle line ends with its participants, values and causes (P3-07): the sentence is its prefix
        const std::string l = event_line(f, year, e);
        CHECK(l.rfind(std::to_string(year) + " e" + std::to_string(e) + " " + tail, 0) == 0U);
    }
}

TEST_CASE("P5-04a 12.3, 12.5, 6.3, 11 item 9: H18's [alliances] and trade lines at full") {
    SKIP_UNLESS_READY();
    const std::vector<std::string> f = lines(HistoryLevel::full);
    for (const std::string& l : std::vector<std::string>{
             "  c1 The House of Aelin and c2 The Court of Onsae: 50-E, by origin, e7; renewed 12",
             "  c3 The Hold of Durn and c5 The Kingdom of Cear: 480-E, against c4 The Gub Horde, e33; renewed 10; joined "
             "1, refused 1; spoils 0 granted, 1 withheld",
             "  c6 The Skreegg Horde and c7 The Urarz Horde: 650-820, in friendship, e44; renewed 1; joined 0, refused "
             "2; broken e54",
             "  c1 The House of Aelin and c4 The Gub Horde: 700-850, against c6 The Skreegg Horde, e47; lapsed e56",
             "  c4 The Gub Horde and c7 The Urarz Horde: 900-1310, in fear of c3 The Hold of Durn, e57; renewed 2; "
             "joined 1, refused 0; spoils 0 granted, 1 withheld; spurned e80",
             "  c1 The House of Aelin and c5 The Kingdom of Cear: 1100-1250, after trade, e64; joined 1, refused 0; "
             "spoils 0 granted, 1 withheld; spurned e73",
             "  c4 The Gub Horde and c6 The Skreegg Horde: 1400-1600, in friendship, e81; renewed 1; betrayed e87",
             "  c3 The Hold of Durn and c5 The Kingdom of Cear: 400-E, e27; arms from c3",
             "  c1 The House of Aelin and c5 The Kingdom of Cear: 410-E, e28",
             "  c5 The Kingdom of Cear and c7 The Urarz Horde: 420-1800, e29; ended e91"}) {
        CAPTURE(l);
        CHECK(has(f, l));
    }
    for (const std::string& l : f) CHECK(l.find("against a common enemy against") == std::string::npos);
}

TEST_CASE("P5-04a 12.3: H18's allies: lines at summary name the allies at the end") {
    SKIP_UNLESS_READY();
    const std::string s = text(HistoryLevel::summary);
    const std::map<std::int64_t, std::string> want = {
        {1, "  allies: 3; at the end: c2 The Court of Onsae"}, {2, "  allies: 1; at the end: c1 The House of Aelin"},
        {3, "  allies: 1; at the end: c5 The Kingdom of Cear"}, {4, "  allies: 3"},
        {5, "  allies: 2; at the end: c3 The Hold of Durn"},    {6, "  allies: 2"}};
    for (const auto& [c, l] : want) {
        CAPTURE(c);
        CHECK(has(hrtest::civ_block(s, c), l));
    }
}

TEST_CASE("P5-04a 12.4: one chronicle line per same-day group of settlement_mixed (full)") {
    SKIP_UNLESS_READY();
    const std::vector<std::string> f = lines(HistoryLevel::full);
    // reading (12.4 at full): the group line keeps the full level's suffix after the sentence; the prefix is checked
    const auto starts = [&](const std::string& p) {
        return std::any_of(f.begin(), f.end(), [&](const std::string& l) { return l.rfind(p, 0) == 0; });
    };
    CHECK(starts("1500 e82, e83 settlement_mixed/25: In Aelinost and Elmere the Elves lived as equals with the Dwarves."));
    CHECK(starts("1500 e84, e85 settlement_mixed/25: In Kazad Rune and Kazad Low the Dwarves tolerated the Elves."));
    CHECK(event_line(f, 1500, 83).empty());
    CHECK(event_line(f, 1500, 85).empty());
}

TEST_CASE("P5-04a 12.2: milestones at summary: the count and the three weightiest, no quiet stretch") {
    SKIP_UNLESS_READY();
    const std::vector<std::string> c3 = hrtest::civ_block(text(HistoryLevel::summary), 3);
    const auto m = std::find_if(c3.begin(), c3.end(), [](const std::string& l) { return l.rfind("  milestones: ", 0) == 0; });
    REQUIRE(m != c3.end());
    CHECK(std::stoll(m->substr(14)) >= 5);   // c3's own events of salience >= 40 (declarations, wins, a conquest)
    REQUIRE(std::distance(m, c3.end()) > 3);
    for (int i = 1; i <= 3; ++i) {
        const std::string& l = *(m + i);
        CAPTURE(l);
        CHECK(l.rfind("    ", 0) == 0U);
        CHECK(l.find(" e") != std::string::npos);
        CHECK(l.find("quiet_stretch") == std::string::npos);
    }
    CHECK((m + 4)->rfind("    ", 0) != 0U);   // three, not more
    for (const std::string& l : lines(HistoryLevel::summary)) CHECK(l.find("quiet_stretch:") == std::string::npos);
}

TEST_CASE("P5-04a 12.6: the [stats] keys of v9 at full") {
    SKIP_UNLESS_READY();
    const auto st = hrtest::stats_of(text(HistoryLevel::full));
    const std::vector<std::pair<std::string, std::int64_t>> want = {
        {"alliances_renewals", 26}, {"alliances_renewals_written", 1}, {"alliances_soured", 2},
        {"alliances_spurned_at_once", 1}, {"allies_refused_excused", 1}, {"refusal_motive_0", 0},
        {"refusal_motive_1", 0}, {"refusal_motive_2", 1}, {"refusal_motive_3", 1}, {"refusal_motive_4", 1},
        {"refusal_motive_5", 0}, {"refusal_motive_6", 0}, {"refusal_motive_7", 0}, {"betrayal_motive_0", 0},
        {"betrayal_motive_1", 0}, {"betrayal_motive_2", 1}, {"betrayal_motive_3", 0}, {"battles_with_arms", 1},
        {"dispositions_thawed", 1}};
    for (const auto& [k, v] : want) {
        CAPTURE(k);
        REQUIRE(st.count(k) == 1U);
        CHECK(st.at(k) == v);
    }
    // in the spec's order (12.6)
    const auto list = hrtest::stats_list(text(HistoryLevel::full));
    std::vector<std::string> order;
    for (const auto& [k, v] : list) {
        for (const auto& [kk, vv] : want) {
            if (k == kk) order.push_back(k);
        }
    }
    std::vector<std::string> spec;
    for (const auto& [k, v] : want) spec.push_back(k);
    CHECK(order == spec);
}

TEST_CASE("P5-04a 14.4, 11 item 2: each range and message on a corrupt copy of H18, in both readers") {
    SKIP_UNLESS_READY();
    const std::string R = "corrupt table 'event': row ";
    refused("motive-8", set_value(49, 1, 8), R + "49: value motive 8 outside 0 to 7");
    refused("motive-neg", set_value(49, 1, -1), R + "49: value motive -1 outside 0 to 7");
    refused("count-101", set_value(49, 2, 101), R + "49: value count 101 outside 0 to 100");
    refused("grievance-3", set_value(42, 2, 3), R + "42: value grievance 3 outside 0 to 2");
    refused("reason-3", set_value(80, 1, 3), R + "80: value reason 3 outside 0 to 2");
    refused("years-E", set_value(73, 2, 2000), R + "73: value years 2000 outside 0 to 1999");
    refused("betrayal-motive-4", set_value(87, 1, 4), R + "87: value motive 4 outside 0 to 3");
    refused("betrayal-years", set_value(87, 2, -1), R + "87: value years -1 outside 0 to 1999");
    refused("change-4", set_value(61, 3, 4), R + "61: value change 4 outside 0 to 3");
    refused("band-3", set_value(47, 3, 3), R + "47: value band 3 outside 0 to 2");
    // term >= 1 (no upper bound given): the message's upper end is the reader's; the prefix is checked
    {
        const db::WorldReader r = hrtest::open_world(corrupt("term-0", set_value(47, 2, 0)));
        const auto e = eng::view_error(r);
        REQUIRE(e.has_value());
        CHECK(e->message.rfind(R + "47: value term 0 outside 1 to ", 0) == 0U);
    }
    refused("motive-7-no-share", set_value(49, 1, 7), R + "49: refusal over spoils without its share");
    refused("excused-motive-3", set_value(60, 1, 3), R + "60: excused refusal with motive 3");
    refused("at-once-no-share", set_value(79, 2, 1), R + "80: spurned at once without its share");
}

}  // TEST_SUITE
