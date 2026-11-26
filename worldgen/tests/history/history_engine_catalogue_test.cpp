// Spec P3-04 section 8 (r1), tests 16.1 items 9 to 11: the compiled
// catalogue against 8.2 transcribed here, its keys, event_catalogue on the
// shipped data, template_problem on every template case of the golden file
// (every problem of 8.4 once), the named check report_events through the
// loader (every message once), the P3-01 unknown-id message with three
// ids, and render_event (8.5) on hand-made events added by SQL to a copy of
// a generated world: :all with 0 to 3 participants, ? with 0, 1, 2, a
// negative value, {{ and }}, and a kind (6) this build does not compile,
// read from the world's catalogue tables only (r1). HistoryView::load's
// corrupt cases are error values with code corrupt_layer and message
// `corrupt table '<table>': <detail>` (r1; the detail is the engineer's).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "p3_04_engine_support.hpp"
#include "worldgen/db/testing.hpp"

namespace {

using namespace engine_test;
using eng::EntityType;

// The test kind of the golden template cases (golden header): id 99,
// test_optional, caused 0, region_use 2, settlement_use 2, roles 1 actor
// civilization 0-0, 2 people race 1-1, 3 place region 1-2, value count.
eng::KindDef test_optional() {
    eng::KindDef k;
    k.id = 99;
    k.key = "test_optional";
    k.caused = false;
    k.region_use = 2;
    k.settlement_use = 2;
    k.roles = {eng::RoleDef{1, "actor", EntityType::civilization, 0, 0},
               eng::RoleDef{2, "people", EntityType::race, 1, 1}, eng::RoleDef{3, "place", EntityType::region, 1, 2}};
    k.values = {"count"};
    return k;
}

const eng::KindDef& compiled_kind(const std::string& key) {
    const eng::KindDef* k = eng::compiled().find(key);
    REQUIRE_MESSAGE(k != nullptr, "no compiled kind " << key);
    return *k;
}

std::string roles_text(const eng::KindDef& k) {
    std::string s;
    for (const eng::RoleDef& r : k.roles) {
        s += std::to_string(r.seq) + ":" + r.role + ":" + std::to_string(static_cast<std::int64_t>(r.type)) + ":" +
             std::to_string(r.min_count) + "-" + std::to_string(r.max_count) + ";";
    }
    return s;
}

// A copy of a generated 512 world with `sql` applied; returns the path.
std::string world_with(const std::string& name, const std::string& sql) {
    static dbtest::TempDir dir("engine-render");
    const std::string src = world_of(golden().world("geo-seed-42-512"));
    const std::string path = dir.file(name);
    dbtest::write_file(path, dbtest::read_file(src));
    if (!sql.empty()) {
        const auto s = worldgen::db::testing::execute_on_file(path, sql);
        REQUIRE_MESSAGE(s.ok(), "SQL rejected: " << sql);
    }
    return path;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 8.2, 16.1 item 9: the compiled catalogue is 8.2 transcribed; keys distinct and of the id grammar") {
    const eng::EventCatalogue& c = eng::compiled();
    // P3-05 appends kinds 6-19 (generator 0.15.0); its own tests check them.
    REQUIRE(c.kinds.size() >= 5U);
    struct Want {
        std::int64_t id;
        const char* key;
        bool caused;
        std::int64_t region, settlement;
        const char* roles;
        std::vector<std::string> values;
    };
    const std::vector<Want> want = {
        {1, "awakening", false, 1, 0, "1:race:1:1-1;2:creator:2:1-1;", {}},
        {2, "civ_founded", true, 1, 0, "1:civilization:3:1-1;2:race:1:1-1;", {}},
        {3, "settlement_founded", false, 1, 1, "1:settlement:4:1-1;2:founder:3:1-1;", {"population"}},
        {4, "quiet_era", false, 1, 0, "1:civilization:3:1-1;", {"years", "generations"}},
        {5, "flare_up", true, 0, 0, "", {"years"}},
    };
    for (std::size_t i = 0; i < want.size(); ++i) {
        const eng::KindDef& k = c.kinds[i];
        CAPTURE(k.key);
        CHECK(k.id == want[i].id);
        CHECK(k.key == want[i].key);
        CHECK(k.caused == want[i].caused);
        CHECK(k.region_use == want[i].region);
        CHECK(k.settlement_use == want[i].settlement);
        CHECK(roles_text(k) == want[i].roles);
        CHECK(k.values == want[i].values);
        CHECK(k.salience == 0);
        CHECK(k.templ.empty());
        CHECK(c.find(k.key) == &k);
        std::vector<std::string> keys = k.values;
        for (const eng::RoleDef& r : k.roles) {
            keys.push_back(r.role);
        }
        keys.push_back(k.key);
        for (const std::string& s : keys) {
            bool ok = !s.empty() && s[0] >= 'a' && s[0] <= 'z';
            for (const char ch : s) {
                ok = ok && ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_');
            }
            CHECK_MESSAGE(ok, s);
        }
        std::sort(keys.begin(), keys.end() - 1);
        CHECK(std::adjacent_find(keys.begin(), keys.end() - 1) == keys.end() - 1);
    }
    CHECK(c.find("duel") == nullptr);  // ("battle" is a kind of P3-06 since 0.16.0)
    // Entity type and phase kind codes (6.1, 9.2).
    CHECK(static_cast<std::int64_t>(EntityType::race) == 1);
    CHECK(static_cast<std::int64_t>(EntityType::god) == 2);
    CHECK(static_cast<std::int64_t>(EntityType::civilization) == 3);
    CHECK(static_cast<std::int64_t>(EntityType::settlement) == 4);
    CHECK(static_cast<std::int64_t>(EntityType::region) == 5);
    CHECK(static_cast<std::int64_t>(eng::PhaseKind::flare) == 3);
    CHECK(static_cast<std::int64_t>(eng::SettlementKind::fortress) == 5);
    CHECK(static_cast<std::int64_t>(eng::SettlementStatus::faded) == 4);
}

TEST_CASE("P3-04 8.1, 16.1 item 9: event_catalogue(shipped) carries the salience and templates of 4.2") {
    const eng::EventCatalogue c = eng::catalogue_of(shipped());
    const std::vector<ReportEntry> r = shipped_report();
    REQUIRE(c.kinds.size() >= r.size());  // P3-05 appends kinds 6-19 (0.15.0), checked by its tests
    for (std::size_t i = 0; i < r.size(); ++i) {
        CAPTURE(r[i].kind);
        CHECK(c.kinds[i].key == r[i].kind);
        CHECK(c.kinds[i].salience == r[i].salience);
        CHECK(c.kinds[i].templ == r[i].templ);
        CHECK(roles_text(c.kinds[i]) == roles_text(eng::compiled().kinds[i]));
    }
    // The fixture roots carry the same data (4.1, 4.2).
    for (const wd::ContentSet* f : {&chrono_test::fixture_geo(), &chrono_test::fixture_geo_small()}) {
        const eng::EventCatalogue fc = eng::catalogue_of(*f);
        REQUIRE(fc.kinds.size() == c.kinds.size());
        for (std::size_t i = 0; i < c.kinds.size(); ++i) {
            CHECK(fc.kinds[i].templ == c.kinds[i].templ);
            CHECK(fc.kinds[i].salience == c.kinds[i].salience);
        }
    }
}

TEST_CASE("P3-04 8.4, 16.1 item 10, 16.4: template_problem on every template case of the golden file") {
    const eng::KindDef opt = test_optional();
    std::size_t n = 0;
    for (const GoldenTemplate& t : golden().templates) {
        CAPTURE(t.kind);
        CAPTURE(t.text);
        const eng::KindDef& k = t.kind == "test_optional" ? opt : compiled_kind(t.kind);
        const std::optional<std::string> got = eng::template_problem(k, t.text);
        CHECK(got.value_or("ok") == t.result);
        ++n;
    }
    CHECK(n >= 35U);
    // Every problem of 8.4 is among the cases.
    std::string all;
    for (const GoldenTemplate& t : golden().templates) {
        all += t.result + "\n";
    }
    for (const char* p : {"unmatched } at byte", "unclosed placeholder at byte", "unknown placeholder '{",
                          "needs a race role at byte", "{@region} needs a kind with a region",
                          "{@settlement} needs a kind with a settlement", "may have no participant at byte",
                          "the template is empty", "non-ASCII byte at byte"}) {
        CHECK_MESSAGE(all.find(p) != std::string::npos, p);
    }
}

TEST_CASE("P3-04 4.2, 16.1 item 11: the named check report_events through the loader, every message once") {
    const std::string report =
        "[history.report]\n"
        "events = [\n"
        "  { kind = \"awakening\", salience = 100, template = \"{creator} awakened the {race:plural} in {@region}.\" },\n"
        "  { kind = \"duel\", salience = 10, template = \"x\" },\n"
        "  { kind = \"awakening\", salience = 100, template = \"again\" },\n"
        "  { kind = \"civ_founded\", salience = 80, template = \"{race:plural\" },\n"
        "  { kind = \"settlement_founded\", salience = 40, template = \"\" },\n"
        "  { kind = \"quiet_era\", salience = 20, template = \"{civilization}\" },\n"
        "]\n";
    wd::MemoryDataRoot root = chrono_test::shipped_memory_root();
    bool placed = false;
    for (wd::MemoryDirectory& d : root.directories) {
        if (d.name != "history") {
            continue;
        }
        for (wd::MemoryEntry& f : d.entries) {
            if (f.name == "report.toml") {
                f.bytes = report;
                placed = true;
            }
        }
        if (!placed) {
            d.entries.push_back(wd::MemoryEntry{"report.toml", report, false});
            placed = true;
        }
    }
    REQUIRE(placed);
    const auto r = wd::load_content_from_memory(wd::builtin_kinds(), root);
    REQUIRE_FALSE(r.ok());
    std::vector<std::string> got;
    for (const wd::LoadError& e : r.errors) {
        got.push_back(e.message);
        CHECK(e.path == "history/report.toml");
        CHECK(e.line == 1U);  // the definition's table header
    }
    // The check reports all its messages (the loader sorts them, P0-03 9).
    // P0-03 7.5: a per-definition check failure is reported as
    // "check '<name>' failed for '<kind>.<id>': <message>".
    for (const std::string& m : golden().report_events) {
        CAPTURE(m);
        const std::string full = "check 'report_events' failed for 'history.report': " + m;
        CHECK(std::find(got.begin(), got.end(), full) != got.end());
    }
    CHECK(got.size() == golden().report_events.size());
}

TEST_CASE("P3-04 4.2 (P3-01 8.1 item 4, P3-06 12.1): an unknown history id lists the registered ids") {
    wd::MemoryDataRoot root = chrono_test::shipped_memory_root();
    for (wd::MemoryDirectory& d : root.directories) {
        if (d.name == "history") {
            // P5-02 4.3: monsters is registered from 0.23.0; a future id (deities) is not
            d.entries.push_back(wd::MemoryEntry{"deities.toml", "[history.deities]\nx = 1\n", false});
        }
    }
    const auto r = wd::load_content_from_memory(wd::builtin_kinds(), root);
    REQUIRE_FALSE(r.ok());
    const std::string text = chrono_test::errors_text(r.errors);
    CAPTURE(text);
    // P5-12 4: renaming registered from 0.22.0
    CHECK(text.find("expected one of 'alliances', 'chronology', 'conflict', 'engine', 'figures', 'monsters', 'peoples', 'relations', 'renaming', 'report'") !=
          std::string::npos);
}

TEST_CASE("P3-04 8.5 r1: render_event of hand-made events of a kind this build does not compile") {
    const std::string src = world_of(golden().world("geo-seed-42-512"));
    const db::WorldReader base = open_world(src);
    const std::int64_t n = static_cast<std::int64_t>(rows_of(base, "event").size());
    const std::int64_t last_phase = static_cast<std::int64_t>(rows_of(base, "phase").size());
    const std::string region1 = rows_of(base, "region").at(0).at(1).bytes;
    const auto races = rows_of(base, "race");
    // The next kind id after the compiled ones (6 at P3-04; 20 since P3-05 appended 6-19).
    const std::string kid = std::to_string(rows_of(base, "event_kind").size() + 1);
    // Kind <kid> `test_omen`: uncaused, region always, no settlement; role people (race, 0-unbounded),
    // value count. Events n+1 ... n+4 at year 5999 with 0, 1, 2, 3 people and counts 0, 1, 2, -5.
    std::string sql =
        "INSERT INTO event_kind VALUES (" + kid + ", 'test_omen', 0, 1, 0, 30, "
        "'[{people:all}] saw an omen over {@region} in {@year}-{@end_year}: {count} {count?star|stars} {{!}}');"
        "INSERT INTO event_kind_role VALUES (" + kid + ", 1, 'people', 1, 0, 0);"
        "INSERT INTO event_kind_value VALUES (" + kid + ", 1, 'count');";
    const std::vector<std::int64_t> counts = {0, 1, 2, -5};
    for (std::int64_t k = 0; k < 4; ++k) {
        const std::string id = std::to_string(n + 1 + k);
        sql += "INSERT INTO event VALUES (" + id + ", 5999, 5999, " + kid + ", " + std::to_string(last_phase) + ", 1, 0, 30);";
        for (std::int64_t p = 1; p <= k; ++p) {
            sql += "INSERT INTO event_participant VALUES (" + id + ", " + std::to_string(p) + ", 1, 1, " +
                   std::to_string(p) + ");";
        }
        sql += "INSERT INTO event_value VALUES (" + id + ", 1, " + std::to_string(counts[static_cast<std::size_t>(k)]) +
               ");";
    }
    const std::string path = world_with("omen.db", sql);
    const db::WorldReader r = open_world(path);
    REQUIRE_FALSE(eng::view_error(r).has_value());
    const std::string tail = " saw an omen over " + region1 + " in 5999-5999: ";
    const std::string a = races.at(0).at(2).bytes, b = races.at(1).at(2).bytes, c = races.at(2).at(2).bytes;
    CHECK(eng::render(r, n + 1) == "[]" + tail + "0 stars {!}");
    // P3-04 r17 21.1 (0.20.0): one to nine as words
    CHECK(eng::render(r, n + 2) == "[" + a + "]" + tail + "one star {!}");
    CHECK(eng::render(r, n + 3) == "[" + a + " and " + b + "]" + tail + "two stars {!}");
    CHECK(eng::render(r, n + 4) == "[" + a + ", " + b + " and " + c + "]" + tail + "-5 stars {!}");
    // The stored catalogue, not the compiled one: kind <kid> is read back.
    const eng::EventCatalogue cat = eng::catalogue_of_world(r);
    REQUIRE(std::to_string(cat.kinds.size()) == kid);
    CHECK(cat.kinds.back().key == "test_omen");
    CHECK(cat.kinds.back().salience == 30);
    // An id outside the log is fatal (a caller's bug).
    for (const std::int64_t bad : {std::int64_t{0}, n + 5, std::int64_t{-1}}) {
        const std::string e = fatal_of([&] { (void)eng::render(r, bad); });
        CHECK(e.rfind("render_event: event " + std::to_string(bad) + ": ", 0) == 0);
    }
}

TEST_CASE("P3-04 8.5 r1: render_event on forms plural, adjective and name of a race role; a stored template wins") {
    // The stored awakening template replaced: rendering follows the world, not the loaded data (A1).
    const std::string path = world_with(
        "forms.db", "UPDATE event_kind SET template = '{race} {race:name} {race:plural} {race:adjective} {creator:all} "
                    "{creator:name} {@region}' WHERE id = 1;");
    const db::WorldReader r = open_world(path);
    const auto race1 = rows_of(r, "race").at(0);
    const auto god1 = rows_of(r, "god").at(0);
    const auto ev1 = rows_of(r, "event").at(0);
    std::string region;
    for (const auto& reg : rows_of(r, "region")) {
        if (reg.at(0).integer == ev1.at(5).integer) {
            region = reg.at(1).bytes;
        }
    }
    CHECK(eng::render(r, 1) == race1.at(2).bytes + " " + race1.at(2).bytes + " " + race1.at(3).bytes + " " +
                                   race1.at(4).bytes + " " + god1.at(1).bytes + " " + god1.at(1).bytes + " " + region);
}

TEST_CASE("P3-04 8.5 r1: HistoryView::load refuses corrupt history tables with an error value, never FatalError") {
    struct Case {
        const char* label;
        std::string sql;
        const char* table;  // "" when the table named is the engineer's choice
    };
    const std::vector<Case> cases = {
        {"template with a problem", "UPDATE event_kind SET template = '{race' WHERE id = 1;", "event_kind"},
        {"template of a role the kind lacks", "UPDATE event_kind SET template = '{ruler}' WHERE id = 2;", "event_kind"},
        {"role row of a missing kind", "INSERT INTO event_kind_role VALUES (100, 1, 'x', 1, 1, 1);", ""},
        {"event of a missing kind", "UPDATE event SET kind = 100 WHERE id = 1;", ""},
        {"participant of a missing civilization", "UPDATE event_participant SET entity_id = 99 WHERE event = 2 AND seq = 1;", ""},
        {"participant role seq not of the kind", "UPDATE event_participant SET role = 7 WHERE event = 1 AND seq = 2;", ""},
        {"participant entity type not of the role", "UPDATE event_participant SET entity_type = 3 WHERE event = 1 AND seq = 1;", ""},
        {"a value missing", "DELETE FROM event_value WHERE event = 3;", ""},
        {"a cause not below its event", "INSERT INTO event_cause VALUES (2, 3);", ""},
        {"a region that does not exist", "UPDATE event SET region = 999 WHERE id = 1;", ""},
    };
    for (const Case& c : cases) {
        CAPTURE(std::string_view(c.label));
        const std::string path = world_with(std::string("corrupt-") + std::to_string(&c - cases.data()) + ".db", c.sql);
        auto opened = db::WorldReader::open(path);
        REQUIRE(opened.ok());
        const db::WorldReader& r = opened.value();
        std::optional<eng::ViewError> e;
        CHECK_NOTHROW(e = eng::view_error(r));
        REQUIRE(e.has_value());
        CAPTURE(e->message);
        CHECK(e->code == db::ErrorCode::corrupt_layer);
        CHECK(e->message.rfind("corrupt table '", 0) == 0);
        if (std::string(c.table).size() > 0) {
            CHECK(e->message.rfind(std::string("corrupt table '") + c.table + "': ", 0) == 0);
        }
    }
    // The uncorrupted copy loads.
    const db::WorldReader ok = open_world(world_with("clean.db", ""));
    CHECK_FALSE(eng::view_error(ok).has_value());
}

}  // TEST_SUITE
