// Spec P5-04 (docs/specs/p5-04-alliances-trade.md r2) on fixture H16 (worldgen/tests/data/history-report/
// h16-alliances.toml, written by reference/p5_04_h16_fixture.py; its header comment says what each event is):
//   18.1 item 14  the sentences of every new template on a fixture event, {r:with} with one and three
//                 participants, {r:against} with none and one, the cause words; template_problem of the forms;
//   18.1 item 15  the reader: H16 loads; each message of 9.1 and 9.2 on a corrupt copy (rows changed by SQL); a
//                 schema 10 file refused;
//   18.1 item 16  report v7 at both levels: the header, [alliances], the civilization lines, [sites] with mines,
//                 [stats], the joined war rows kept out of the war lines (10.4), companions;
//   18.1 item 17  the timeline reads around a formation, a betrayal, a spurning and a lapse; sites_existing with
//                 a mine (through p5_04_alliances_api.hpp);
//   18.1 item 18  the battle kinds set in markers (allied_battle is a battle place) and the browser groups;
//   18.1 item 19  the mine marker (entry, glyph 19, MarkerOptions::sites off).
// The expected lines are hand-checked against the fixture's rows and equal the reference's report
// (p3_report_reference.py report at 0.26.0). Written from the spec before the implementation.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "../history_report_checks.hpp"
#include "../history_report_fixture.hpp"
#include "p5_03_relations_api.hpp"
#include "p5_04_alliances_api.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/inspect/history_report.hpp"

namespace {

namespace db = worldgen::db;
namespace h = worldgen::history;
namespace ins = worldgen::inspect;
namespace fs = std::filesystem;
using hrtest::HistoryLevel;

const char* const H16 = "h16-alliances";

bool present() { return fs::exists(fs::path(UNMAPPED_P3_07_FIXTURE_DIR) / (std::string(H16) + ".toml")); }

struct H16Fixture {
    hrtest::Fixture fx;
    std::vector<db::SettlementRaceRow> sr;
};

const H16Fixture*& current() {
    static const H16Fixture* f = nullptr;
    return f;
}

H16Fixture load_h16() {
    H16Fixture out;
    out.fx = hrtest::load_fixture(H16);
    const fs::path path = fs::path(UNMAPPED_P3_07_FIXTURE_DIR) / (std::string(H16) + ".toml");
    const toml::table doc = toml::parse(hrtest::detail::text_of_file(path), path.string());
    using hrtest::detail::i64;
    for (const toml::array* r : hrtest::detail::rows(doc, "settlement_race")) {
        db::SettlementRaceRow x;
        x.settlement = i64(*r->get(0));
        x.race = i64(*r->get(1));
        x.year = i64(*r->get(2));
        x.share = i64(*r->get(3));
        x.standing = i64(*r->get(4));
        out.sr.push_back(x);
    }
    return out;
}

void stage_biomes(worldgen::stages::StageContext& ctx) {
    for (const auto& r : current()->fx.biomes) ctx.write_biome(r);
}
void stage_regions(worldgen::stages::StageContext& ctx) {
    for (const auto& r : current()->fx.regions) ctx.write_region(r);
}
void stage_chronology(worldgen::stages::StageContext& ctx) {
    const hrtest::Fixture& f = current()->fx;
    for (const auto& r : f.race_rows) ctx.write_race(r);
    for (const auto& r : f.cultures) ctx.write_culture(r);
    for (const auto& r : f.gods) ctx.write_god(r);
    for (const auto& r : f.ages) ctx.write_age(r);
}
void stage_history(worldgen::stages::StageContext& ctx) {
    const hrtest::Fixture& f = current()->fx;
    for (const auto& r : f.kinds) ctx.write_event_kind(r);
    for (const auto& r : f.roles) ctx.write_event_kind_role(r);
    for (const auto& r : f.values) ctx.write_event_kind_value(r);
    for (const auto& r : f.phases) ctx.write_phase(r);
    for (const auto& r : f.events) ctx.write_event(r);
    for (const auto& r : f.participants) ctx.write_event_participant(r);
    for (const auto& r : f.causes) ctx.write_event_cause(r);
    for (const auto& r : f.event_values) ctx.write_event_value(r);
    for (const auto& r : f.civilizations) ctx.write_civilization(r);
    for (const auto& r : f.settlements) ctx.write_settlement(r);
    for (const auto& r : f.census) ctx.write_settlement_census(r);
    for (const auto& r : f.region_control) ctx.write_region_control(r);
    for (const auto& r : f.relations) ctx.write_relation(r);
    for (const auto& r : f.region_names) ctx.write_region_name(r);
    for (const auto& r : f.sites) ctx.write_site(r);
    for (const auto& r : current()->sr) ctx.write_settlement_race(r);
}

const std::string& h16_world() {
    static const std::string path = [] {
        static const H16Fixture f = load_h16();
        static dbtest::TempDir dir("alliances-h16");
        const std::string p = dir.file("h16-alliances.db");
        static const worldgen::data::ContentSet empty;
        worldgen::WorldParams wp;
        wp.seed = f.fx.seed;
        wp.map_size = f.fx.map_size;
        wp.content_hash = worldgen::data::content_hash(empty);
        wp.history.span = f.fx.span;
        wp.history.races = f.fx.races;
        wp.threads = 1;
        const worldgen::stages::StageInfo stages[] = {{"biomes", &stage_biomes},
                                                      {"regions", &stage_regions},
                                                      {"chronology", &stage_chronology},
                                                      {"history", &stage_history}};
        current() = &f;
        const db::Status s =
            worldgen::stages::testing::generate_world_with_stages(wp, empty, stages, p, db::Overwrite::replace);
        current() = nullptr;
        REQUIRE_MESSAGE(s.ok(), "fixture h16: " << (s.ok() ? "" : s.error().message));
        return p;
    }();
    return path;
}

std::string text(HistoryLevel level) {
    static std::map<int, std::string> cache;
    const int k = static_cast<int>(level);
    if (const auto it = cache.find(k); it != cache.end()) return it->second;
    return cache[k] = hrtest::report_of(h16_world(), level);
}

std::vector<std::string> between(const std::vector<std::string>& ls, const std::string& head, const std::string& next) {
    std::vector<std::string> out;
    bool in = false;
    for (const std::string& l : ls) {
        if (l == head) in = true;
        else if (in && l.rfind(next, 0) == 0) break;
        if (in) out.push_back(l);
    }
    return out;
}

std::string chronicle_line(const std::string& t, std::int64_t e) {
    const std::string tag = " e" + std::to_string(e) + " ";
    for (const std::string& l : hrtest::lines_of(t)) {
        if (!l.empty() && l[0] >= '0' && l[0] <= '9' && l.find(tag) != std::string::npos) return l;
    }
    return "";
}

std::string corrupt(const std::string& name, const std::string& sql) {
    static dbtest::TempDir dir("alliances-corrupt");
    const std::string path = dir.file(name + ".db");
    dbtest::write_file(path, dbtest::read_file(h16_world()));
    REQUIRE_MESSAGE(db::testing::execute_on_file(path, sql).ok(), "SQL rejected: " << sql);
    return path;
}

// `both`: read_history_tables refuses it too, with the same message. 9.1 places the relation row checks in
// HistoryView::load only (r3, the engineer's reading 12), so those cases test that reader alone.
void refused(const std::string& name, const std::string& sql, const std::string& message, bool both = true) {
    CAPTURE(name);
    const db::WorldReader r = hrtest::open_world(corrupt(name, sql));
    const auto e = eng::view_error(r);
    REQUIRE_MESSAGE(e.has_value(), "HistoryView::load accepted " << name);
    CHECK(e->code == db::ErrorCode::corrupt_layer);
    CHECK_MESSAGE(e->message.find(message) != std::string::npos, e->message);
    if (!both) return;
    const auto t = h::read_history_tables(r);
    REQUIRE_FALSE(t.ok());
    CHECK(t.error().message == e->message);
}

}  // namespace

TEST_SUITE("history") {

// --- 18.1 item 14: the templates on H16's events ------------------------------------------------------------

TEST_CASE("P5-04 8.3, 18.1 item 14: every new template rendered on H16 (with, against, the choices, cause words)") {
    if (!present()) {
        MESSAGE("h16-alliances.toml absent: skipped");
        return;
    }
    const db::WorldReader r = hrtest::open_world(h16_world());
    const std::map<std::int64_t, std::string> want = {
        {7, "The House of Aelin and the Court of Onsae swore the alliance their peoples had kept since the gods made "
            "them."},
        {23, "The Hold of Durn and the Kingdom of Cear opened trade."},
        {26, "The Hold of Durn opened the Mines of Kazad Rune in Kazad Fells."},
        {27, "The Hold of Durn sold arms forged in the Mines of Kazad Rune to the Kingdom of Cear."},
        {28, "The Hold of Durn opened the Delvings of Duldar in the Grey Fells."},
        {30, "After the outbreak of war, the Hold of Durn and the Kingdom of Cear swore to stand together against the "
             "Gub Horde."},
        {33, "The Kingdom of Cear came to the defence of the Hold of Durn against the Gub Horde."},
        {34, "The Hold of Durn, with the Kingdom of Cear, defeated the Gub Horde in the Grey Fells; 812 of the defeated "
             "and 140 of the victors fell."},
        {39, "The Hold of Durn kept the spoils of the Grey Fells from the Kingdom of Cear, its ally."},
        {46, "After the spoils of the Grey Fells were kept from it, the Kingdom of Cear broke its alliance with the Hold "
             "of Durn."},
        {47, "The Skreegg Horde and the Urarz Horde swore friendship and alliance."},
        {50, "The House of Aelin, with the Court of Onsae, defeated the Skreegg Horde in Aelin Vale; 40 of the defeated "
             "and 3 of the victors fell."},
        {53, "The House of Aelin gave the Court of Onsae its share of the spoils of Aelin Vale."},
        {55, "The Urarz Horde would not take up arms for the Skreegg Horde against the Kingdom of Cear."},
        {56, "After a call to arms refused, the alliance of the Skreegg Horde and the Urarz Horde was broken."},
        {58, "The Gub Horde and the Urarz Horde made common cause against the Hold of Durn."},
        {60, "The Urarz Horde broke faith and made war on the Gub Horde, an ally."},
        {61, "When war came, the trade between the Gub Horde and the Urarz Horde ended."},
        {63, "After long years of trade, the House of Aelin and the Kingdom of Cear swore friendship and alliance."},
        {64, "The alliance of the House of Aelin and the Kingdom of Cear lapsed, its cause long past."},
        {66, "The alliance of the House of Aelin and the Hold of Durn lapsed, for together they had grown too great."}};
    for (const auto& [e, s] : want) {
        CAPTURE(e);
        CHECK(eng::render(r, e) == s);
    }
}

TEST_CASE("P5-04 8.2: {r:with} with three participants; template_problem of with and against") {
    if (!present()) {
        MESSAGE("h16-alliances.toml absent: skipped");
        return;
    }
    // e34 with two more winner allies (seq 4 and 5, role 3, civilizations 1 and 2)
    const std::string p = corrupt("three-allies", "INSERT INTO event_participant VALUES (34, 4, 3, 3, 1);"
                                                  "INSERT INTO event_participant VALUES (34, 5, 3, 3, 2);");
    const db::WorldReader r = hrtest::open_world(p);
    if (const auto e = eng::view_error(r); e.has_value()) {
        // the reader may refuse an ally participant without its joined row (13 item 5 is an invariant, not a check)
        MESSAGE("three-allies refused by the reader: " << e->message);
    } else {
        CHECK(eng::render(r, 34) == "The Hold of Durn, with the Kingdom of Cear, the House of Aelin and the Court of "
                                    "Onsae, defeated the Gub Horde in the Grey Fells; 812 of the defeated and 140 of "
                                    "the victors fell.");
    }
    const h::EventCatalogue& cat = h::compiled_event_catalogue();
    const h::KindDef* ab = cat.find("allied_battle");
    REQUIRE(ab != nullptr);
    CHECK_FALSE(h::template_problem(*ab, "{winner_ally:with}").has_value());
    CHECK_FALSE(h::template_problem(*ab, "{loser_ally:against}").has_value());
    CHECK_FALSE(h::template_problem(*ab, "{winner:with}").has_value());
    CHECK(h::template_problem(*ab, "{winner_ally}").has_value());    // min 0: only all, with, against
    CHECK(h::template_problem(*ab, "x{nobody:with}").has_value());   // the role must exist
    CHECK(h::template_problem(*ab, "{nobody:against}").has_value());
}

// --- 18.1 item 15: the reader --------------------------------------------------------------------------------

TEST_CASE("P5-04 9.1, 9.2: H16 loads; each corruption refused with its message") {
    if (!present()) {
        MESSAGE("h16-alliances.toml absent: skipped");
        return;
    }
    {
        const db::WorldReader r = hrtest::open_world(h16_world());
        const auto e = eng::view_error(r);
        CHECK_MESSAGE(!e.has_value(), (e.has_value() ? e->message : std::string()));
    }
    // 9.1 relation rows (key: a_type a_id b_type b_id kind since; the reader's key format as P5-03's)
    refused("alliance-since", "UPDATE relation SET since_event = 23 WHERE kind = 'alliance' AND a_id = 3 AND b_id = 5;",
            "alliance since_event 23 is not an alliance_formed of the pair", false);
    refused("trade-since", "UPDATE relation SET since_event = 7 WHERE kind = 'trade' AND a_id = 3 AND b_id = 5;",
            "trade since_event 7 is not a trade_opened of the pair", false);
    refused("war-since", "UPDATE relation SET since_event = 23 WHERE kind = 'war' AND a_id = 5 AND since = 600;",
            "war since_event 23 is not a war_declared or an ally_joined_war of the pair", false);
    refused("alliance-until", "UPDATE relation SET until_event = 61 WHERE kind = 'alliance' AND a_id = 4 AND b_id = 7;",
            "until_event 61 does not end the alliance", false);
    // 9.1 event values
    refused("basis", "UPDATE event_value SET value = 5 WHERE event = 30 AND seq = 1;", "basis");
    refused("lapsed-reason", "UPDATE event_value SET value = 2 WHERE event = 64 AND seq = 1;", "reason");
    refused("trade-reason", "UPDATE event_value SET value = 3 WHERE event = 61 AND seq = 1;", "reason");
    refused("side", "UPDATE event_value SET value = 2 WHERE event = 33 AND seq = 1;", "side");
    refused("share", "UPDATE event_value SET value = 0 WHERE event = 53 AND seq = 1;", "share");
    refused("no-ally", "DELETE FROM event_participant WHERE event = 50 AND seq = 3;", "allied_battle without an ally");
    refused("spoils-war", "DELETE FROM event_cause WHERE event = 53 AND cause = 51;", "spoils without its war");
    // 9.2 the site kind mine (P5-02 11.4's line, amended)
    refused("site-kind", "UPDATE site SET kind = 5 WHERE id = 1;", "kind 5 is not 1, 2, 3 or 4");   // P5-06: 4 is a tower
    refused("mine-monster", "UPDATE site SET monster = 1 WHERE id = 1;", "mine with a monster");
    refused("mine-settlement", "UPDATE site SET settlement = 0 WHERE id = 1;", "mine without a settlement");
    refused("mine-civ", "UPDATE site SET founder_civ = 99 WHERE id = 1;", "unknown civilization 99");
    refused("mine-ending", "UPDATE site SET ending = 1 WHERE id = 2;", "ending 1 is not 0, 4 or 5 for a mine");
    refused("mine-origin", "UPDATE site SET origin_event = 23 WHERE id = 1;", "site");
}

TEST_CASE("P5-04 9.2: a schema 10 file is refused (readers refuse version 10 worlds)") {
    if (!present()) {
        MESSAGE("h16-alliances.toml absent: skipped");
        return;
    }
    const std::string path = corrupt("schema10", "PRAGMA user_version = 10; UPDATE meta SET value = '10' WHERE key = "
                                                 "'schema_version';");
    auto r = db::WorldReader::open(path);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("world schema version 10, this build reads 14") != std::string::npos);
    CHECK(db::SCHEMA_VERSION == p504api::SCHEMA_SPEC);
}

// --- 18.1 item 16: report v7 ---------------------------------------------------------------------------------

TEST_CASE("P5-04 10.1, 10.2: H16's header and [alliances] at both levels") {
    if (!present()) {
        MESSAGE("h16-alliances.toml absent: skipped");
        return;
    }
    const std::vector<std::string> s = hrtest::lines_of(text(HistoryLevel::summary));
    const std::vector<std::string> f = hrtest::lines_of(text(HistoryLevel::full));
    REQUIRE_FALSE(s.empty());
    CHECK(s.front() == "# worldgen history v11");
    CHECK(f.front() == "# worldgen history v11");
    const std::string count = "alliances: 6 formed, 1 standing at E; ended: 2 lapsed, 1 broken, 1 spurned, 1 betrayed, "
                              "0 with a realm";
    const std::string l7 = "  c1 The House of Aelin and c2 The Court of Onsae: 50-E, by origin, e7; joined 1, refused 0; "
                           "spoils 1 granted, 0 withheld";
    const std::string l30 = "  c3 The Hold of Durn and c5 The Kingdom of Cear: 480-630, against c4 The Gub Horde, "
                            "e30; joined 2, refused 0; spoils 0 granted, 2 withheld; spurned e46";
    const std::string l47 = "  c6 The Skreegg Horde and c7 The Urarz Horde: 650-800, in friendship, e47; joined 0, "
                            "refused 1; broken e56";
    const std::string l58 = "  c4 The Gub Horde and c7 The Urarz Horde: 900-1000, in fear of c3 The Hold of Durn, e58; betrayed e60";
    const std::string l63 = "  c1 The House of Aelin and c5 The Kingdom of Cear: 1100-1250, after trade, e63; lapsed e64";
    const std::string l65 = "  c1 The House of Aelin and c3 The Hold of Durn: 1300-1400, in friendship, e65; lapsed e66";
    const std::string calls = "calls to arms: 3 joined (3 in defence, 0 in attack), 1 refused, 0 withdrew; wars with an "
                              "ally: 3 of 7";
    const std::string spoils = "spoils: 3 owed, 1 granted, 2 withheld; 270 people owed, 10 granted";
    const std::string bloc = "largest bloc: 400 permille, c1 with c2, c3 in 1300";   // reading P1 (the sign)
    const std::string trade = "trade: 3 opened, 2 standing at E; broken 1 (war 1, cooled 0, out of reach 0); arms sold 1";
    CHECK(between(f, "[alliances]", "[figures]") ==   // P5-06 10.1: [figures] follows [alliances]
          std::vector<std::string>{"[alliances]", count, l7, l30, l47, l58, l63, l65, calls, spoils, bloc, trade,
                                   "  c3 The Hold of Durn and c5 The Kingdom of Cear: 400-E, e23; arms from c3",
                                   "  c1 The House of Aelin and c5 The Kingdom of Cear: 410-E, e24",
                                   "  c4 The Gub Horde and c7 The Urarz Horde: 420-1000, e25; broken e61"});
    // summary: standing at E, betrayed or spurned; no trade lines
    CHECK(between(s, "[alliances]", "[figures]") ==
          std::vector<std::string>{"[alliances]", count, l7, l30, l58, calls, spoils, bloc, trade});
    // block order: [peoples], [alliances], [stats]
    const auto at = [&](const std::string& head) { return std::find(f.begin(), f.end(), head) - f.begin(); };
    CHECK(at("[sites]") < at("[peoples]"));
    CHECK(at("[peoples]") < at("[alliances]"));
    CHECK(at("[alliances]") < at("[stats]"));
}

TEST_CASE("P5-04 10.3, 10.4: civilization lines; the joined war rows kept out of wars and adversaries") {
    if (!present()) {
        MESSAGE("h16-alliances.toml absent: skipped");
        return;
    }
    const std::string s = text(HistoryLevel::summary), f = text(HistoryLevel::full);
    const auto has = [](const std::vector<std::string>& b, const std::string& l) { return hrtest::contains_line(b, l); };
    const std::vector<std::string> f1 = hrtest::civ_block(f, 1);
    CHECK(has(f1, "  allies: 3"));
    const auto a1 = std::find(f1.begin(), f1.end(), "  allies: 3");
    REQUIRE(a1 != f1.end());
    REQUIRE(f1.end() - a1 == 5);   // P5-06 10.3: the rulers line closes the block
    CHECK(*(a1 + 1) == "    c2 The Court of Onsae: 50-E, by origin");
    CHECK(*(a1 + 2) == "    c5 The Kingdom of Cear: 1100-1250, after trade");
    CHECK(*(a1 + 3) == "    c3 The Hold of Durn: 1300-1400, in friendship");
    // P5-06 10.3: the rulers line follows the allies line
    const auto allies_last = [&](std::int64_t c) {
        std::vector<std::string> b = hrtest::civ_block(s, c);
        if (!b.empty() && b.back().rfind("  rulers: ", 0) == 0) b.pop_back();
        return b.back();
    };
    CHECK(allies_last(1) == "  allies: 3; at the end: c2 The Court of Onsae");
    CHECK(allies_last(2) == "  allies: 1; at the end: c1 The House of Aelin");
    CHECK(allies_last(3) == "  allies: 2");
    // 10.4: Cear joined two wars against the Gub Horde: its rows are no wars of its own
    CHECK(has(hrtest::civ_block(s, 5), "  wars: 1 declared, 1 defended; won 1, lost 0, drawn 1, open 0"));
    CHECK(has(hrtest::civ_block(s, 4), "  wars: 3 declared, 1 defended; won 0, lost 4, drawn 0, open 0"));
    CHECK(has(hrtest::civ_block(s, 2), "  wars: 0 declared, 0 defended; won 0, lost 0, drawn 0, open 0"));
    CHECK(has(hrtest::civ_block(s, 3), "    The Gub Horde (c4): 2 wars, won 2, lost 0, drawn 0, open 0, 600-630; now at "
                                        "peace"));
    const std::map<std::string, std::int64_t> st = hrtest::stats_of(f);
    CHECK(st.at("wars") == 7);
}

TEST_CASE("P5-04 10.6: [sites] counts mines; the mine lines at full") {
    if (!present()) {
        MESSAGE("h16-alliances.toml absent: skipped");
        return;
    }
    const std::vector<std::string> s = hrtest::lines_of(text(HistoryLevel::summary));
    const std::vector<std::string> f = hrtest::lines_of(text(HistoryLevel::full));
    CHECK(between(s, "[sites]", "[peoples]") ==
          std::vector<std::string>{"[sites]", "sites: 2; open 1; lairs 0, outposts 0, mines 2, towers 0"});
    CHECK(between(f, "[sites]", "[peoples]") ==
          std::vector<std::string>{
              "[sites]", "sites: 2; open 1; lairs 0, outposts 0, mines 2, towers 0",
              "  si1 The Mines of Kazad Rune: mine in Kazad Fells (rg3) at 30 30, since 450 e26, of The Hold of Durn "
              "(c3), at Kazad Rune (s3); open",
              "  si2 The Delvings of Duldar: mine in The Grey Fells (rg8) at 80 80, since 455 e28, of The Hold of Durn "
              "(c3), at Duldar (s8); ended 1505 e70 lost"});
}

TEST_CASE("P5-04 10.5: the [stats] keys appended at full, in order; none at summary") {
    if (!present()) {
        MESSAGE("h16-alliances.toml absent: skipped");
        return;
    }
    const std::vector<std::pair<std::string, std::int64_t>> want = {
        {"alliances_formed", 6}, {"alliances_standing", 1}, {"alliances_cross_race", 4}, {"alliances_origin", 1},
        {"alliances_lapsed", 2}, {"alliances_broken", 1}, {"alliances_betrayed", 1}, {"alliances_ended_with_realm", 0},
        {"alliances_years_median", 150}, {"allies_joined", 3}, {"allies_joined_defence", 3}, {"allies_refused", 1},
        {"allies_withdrew", 0}, {"wars_with_ally", 3}, {"allied_battles", 3}, {"bloc_share_max_permille", 400},
        {"bloc_over_800_years", 0}, {"trade_opened", 3}, {"trade_standing", 2}, {"trade_broken", 1},
        {"alliances_spurned", 1}, {"spoils_owed", 3}, {"spoils_granted", 1}, {"spoils_withheld", 2},
        {"spoils_granted_permille", 333}, {"spoils_people_owed", 270}, {"mines_opened", 2}, {"mines_open", 1},
        {"arms_sold", 1}, {"arms_buyers_at_end", 1}};
    const auto all = hrtest::stats_list(text(HistoryLevel::full));
    REQUIRE(all.size() >= want.size());
    // P5-06 10.4: the [figures] keys follow the alliances' (the block is the first want entry to the 30th)
    const auto first = std::find(all.begin(), all.end(), want.front());
    REQUIRE(first != all.end());
    REQUIRE(static_cast<std::size_t>(all.end() - first) >= want.size());
    CHECK(std::vector<std::pair<std::string, std::int64_t>>(first, first + static_cast<std::ptrdiff_t>(want.size())) ==
          want);
    const auto sum = hrtest::stats_of(text(HistoryLevel::summary));
    for (const auto& [k, v] : want) CHECK(sum.count(k) == 0U);
}

TEST_CASE("P5-04 10.3: companions: calls with their declaration, spoils with their war's end, betrayal both ways") {
    if (!present()) {
        MESSAGE("h16-alliances.toml absent: skipped");
        return;
    }
    const std::string s = text(HistoryLevel::summary);
    for (const auto& [a, b] : std::vector<std::pair<std::int64_t, std::int64_t>>{
             {32, 33}, {40, 41}, {48, 49}, {54, 55}, {37, 39}, {43, 45}, {51, 53}, {59, 60}}) {
        CAPTURE(a);
        CAPTURE(b);
        CHECK(chronicle_line(s, a).empty() == chronicle_line(s, b).empty());
    }
    // allied_battle is a battle kind (11.1): no summary candidate by itself, shown in its war's frame
    CHECK_FALSE(chronicle_line(s, 34).empty());
    CHECK(chronicle_line(s, 34).rfind("601 e34 allied_battle/35: ", 0) == 0U);
}

TEST_CASE("P5-04 10.2: the summary's alliance lines are capped at SUMMARY_ALLIANCES_MAX (10)") {
    CHECK(p504api::summary_alliances_max() == p504api::SUMMARY_ALLIANCES_MAX_SPEC);
}

// --- 18.1 item 17: the timeline ------------------------------------------------------------------------------

TEST_CASE("P5-04 12: alliances_at, allies_of, alliance_groups_at, trades_at around a formation, betrayal, spurning, "
          "lapse; sites_existing with mines") {
    if (!present()) {
        MESSAGE("h16-alliances.toml absent: skipped");
        return;
    }
    const db::WorldReader r = hrtest::open_world(h16_world());
    auto lh = h::load_history(r);
    REQUIRE_MESSAGE(lh.ok(), (lh.ok() ? "" : lh.error().message));
    const h::Timeline& t = lh.value().timeline;
    const auto pairs = [&](std::int64_t y) {
        std::vector<p504api::Pair> out;
        for (const auto& [a, b, since, ev] : p504api::alliances_at(t, y)) out.emplace_back(a, b);
        return out;
    };
    using P = p504api::Pair;
    using V = std::vector<P>;
    CHECK(pairs(49) == V{});
    CHECK(pairs(50) == V{P{1, 2}});
    CHECK(pairs(479) == V{P{1, 2}});
    CHECK(pairs(480) == V{P{1, 2}, P{3, 5}});   // the formation
    CHECK(pairs(629) == V{P{1, 2}, P{3, 5}});
    CHECK(pairs(630) == V{P{1, 2}});            // the spurning
    CHECK(pairs(999) == V{P{1, 2}, P{4, 7}});
    CHECK(pairs(1000) == V{P{1, 2}});           // the betrayal
    CHECK(pairs(1249) == V{P{1, 2}, P{1, 5}});
    CHECK(pairs(1250) == V{P{1, 2}});           // the lapse
    CHECK(pairs(1300) == V{P{1, 2}, P{1, 3}});
    CHECK(pairs(1999) == V{P{1, 2}});
    const auto a480 = p504api::alliances_at(t, 480);
    REQUIRE(a480.size() == 2U);
    CHECK(a480[1] == p504api::Alliance{3, 5, 480, 30});
    CHECK(p504api::allies_of(t, 1, 1300) == std::vector<std::int64_t>{2, 3});
    CHECK(p504api::allies_of(t, 3, 1300) == std::vector<std::int64_t>{1});
    CHECK(p504api::allies_of(t, 5, 600) == std::vector<std::int64_t>{3});
    CHECK(p504api::allies_of(t, 5, 630).empty());
    CHECK(p504api::allies_of(t, 4, 1000).empty());
    CHECK(p504api::alliance_groups_at(t, 1300) == std::vector<std::vector<std::int64_t>>{{1, 2, 3}});
    CHECK(p504api::alliance_groups_at(t, 480) == std::vector<std::vector<std::int64_t>>{{1, 2}, {3, 5}});
    CHECK(p504api::alliance_groups_at(t, 40).empty());
    CHECK(p504api::trades_at(t, 399) == V{});
    CHECK(p504api::trades_at(t, 420) == V{P{1, 5}, P{3, 5}, P{4, 7}});
    CHECK(p504api::trades_at(t, 1000) == V{P{1, 5}, P{3, 5}});
    const auto se = [&](std::int64_t y) {
        const auto s = t.sites_existing(y);
        return std::vector<std::int64_t>(s.begin(), s.end());
    };
    CHECK(se(449).empty());
    CHECK(se(455) == std::vector<std::int64_t>{1, 2});
    CHECK(t.site_at(2, 1504).kind == 3);
    CHECK(t.site_at(2, 1504).founder == 3);
}

// --- 18.1 items 18, 19: markers and the browser ----------------------------------------------------------------

TEST_CASE("P5-04 11.1, 11.3: allied_battle is a battle place; the mine entries at their cells, glyph 19, sites off") {
    if (!present()) {
        MESSAGE("h16-alliances.toml absent: skipped");
        return;
    }
    CHECK(ins::event_symbol("allied_battle") == ins::EventSymbol::battle);
    const db::WorldReader r = hrtest::open_world(h16_world());
    auto lh = h::load_history(r);
    REQUIRE(lh.ok());
    auto mi = ins::MarkerIndex::load(r, lh.value());
    REQUIRE_MESSAGE(mi.ok(), (mi.ok() ? "" : mi.error().message));
    ins::MarkerOptions on;
    const ins::YearMarkers m601 = mi.value().markers_at(lh.value(), 601, on);
    bool battle = false;
    for (const ins::PlaceMarker& p : m601.places) {
        if (std::find(p.events.begin(), p.events.end(), 34) != p.events.end()) battle = p.symbol == ins::EventSymbol::battle;
    }
    CHECK(battle);
    using M = p504api::Mine;
    CHECK(p504api::mines(mi.value().markers_at(lh.value(), 449, on)).empty());
    CHECK(p504api::mines(mi.value().markers_at(lh.value(), 455, on)) == std::vector<M>{M{1, 30, 30, 3}, M{2, 80, 80, 3}});
    CHECK(p504api::mines(mi.value().markers_at(lh.value(), 1504, on)) == std::vector<M>{M{1, 30, 30, 3}, M{2, 80, 80, 3}});
    CHECK(p504api::mines(mi.value().markers_at(lh.value(), 1505, on)) == std::vector<M>{M{1, 30, 30, 3}});
    ins::MarkerOptions off;
    p504api::set_sites(off, false);
    CHECK(p504api::mines(mi.value().markers_at(lh.value(), 1505, off)).empty());
    ins::GlyphKey k;
    k.base = p504api::MINE_GLYPH;
    const ins::Glyph g = ins::marker_glyph(k);   // FatalError before P5-04 (16..31 reserved)
    (void)g;
}

TEST_CASE("P5-04 11.2: browser groups of kinds 63 to 78; alliances = 7; ALL_GROUPS 0xFF") {
    CHECK(ins::ALL_GROUPS == p504api::ALL_GROUPS_SPEC);
    CHECK(ins::event_group_word(p504api::alliances_group()) == "alliances");
    const std::vector<std::pair<std::string, ins::EventGroup>> want = {
        {"alliance_formed", p504api::alliances_group()},   {"alliance_renewed", p504api::alliances_group()},
        {"alliance_lapsed", p504api::alliances_group()},   {"alliance_broken", p504api::alliances_group()},
        {"alliance_betrayed", p504api::alliances_group()}, {"ally_joined_war", p504api::alliances_group()},
        {"ally_refused", p504api::alliances_group()},      {"ally_withdrew", p504api::alliances_group()},
        {"allied_battle", ins::EventGroup::war},            {"trade_opened", p504api::alliances_group()},
        {"trade_broken", p504api::alliances_group()},      {"spoils_shared", p504api::alliances_group()},
        {"spoils_withheld", p504api::alliances_group()},   {"alliance_spurned", p504api::alliances_group()},
        {"mine_opened", ins::EventGroup::settlements},      {"arms_sold", p504api::alliances_group()}};
    for (const auto& [k, g] : want) {
        CAPTURE(k);
        CHECK(static_cast<int>(ins::event_group(k)) == static_cast<int>(g));
    }
    // every kind of the compiled catalogue is in a group other than `other` (P4-05 10.2 item 2), 78 of them
    const h::EventCatalogue& cat = h::compiled_event_catalogue();
    CHECK(cat.kinds.size() == 110U);
    for (const h::KindDef& k : cat.kinds) {
        CAPTURE(k.key);
        CHECK(ins::event_group(k.key) != ins::EventGroup::other);
    }
}

}  // TEST_SUITE
