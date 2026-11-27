// Spec P5-06 (docs/specs/p5-06-figures.md r3) test 18.1 item 13: the reader checks of 9.4 (figure_tables_problem),
// each on a corrupt copy of a generated figures-small world (rows changed by SQL), refused by HistoryView::load and
// read_history_tables alike with the same first message (code corrupt_layer); a version 11 world is refused. The
// messages are the spec's tables, the row numbers those of the base world. Written from the spec before the
// implementation; every case starts with P506_REQUIRE_INTEGRATED().

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "p5_06_figures_api.hpp"
#include "p5_06_figures_checks.hpp"
#include "p5_06_figures_runs.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/history/digest.hpp"

namespace {

namespace db = worldgen::db;
namespace h = worldgen::history;
std::string n(std::int64_t v) { return std::to_string(v); }

const std::string& base_path() {
    static const std::string p = p506run::generated(42, 512, p506run::figures_small(), "small");
    return p;
}

struct Base {
    p506check::World w;
    std::int64_t ruler = 0, general = 0, claimant = 0, caster = 0, alive = 0, last_figure = 0;
    std::int64_t battle_led = 0, figure_died = 0, last_book = 0, last_book_event = 0;
};
const Base& base() {
    static const Base b = [] {
        Base out;
        const db::WorldReader r = chrono_test::open_world(base_path());
        out.w = p506check::world_of_file(r);
        for (const auto& [id, f] : out.w.figs) {
            if (f.role == 1 && out.ruler == 0) out.ruler = id;
            if (f.role == 2 && out.claimant == 0) out.claimant = id;
            if (f.role == 3 && out.general == 0) out.general = id;
            if (f.role == 5 && out.caster == 0) out.caster = id;
            if (f.death_event == 0 && out.alive == 0) out.alive = id;
            out.last_figure = id;
        }
        const auto bl = out.w.of("battle_led");
        const auto fd = out.w.of("figure_died");
        out.battle_led = bl.empty() ? 0 : bl.front();
        out.figure_died = fd.empty() ? 0 : fd.front();
        if (!out.w.books.empty()) {
            out.last_book = out.w.books.back().at(0).integer;
            out.last_book_event = out.w.books.back().at(5).integer;
        }
        REQUIRE(out.ruler != 0);
        REQUIRE(out.general != 0);
        REQUIRE(out.caster != 0);
        REQUIRE(out.battle_led != 0);
        REQUIRE(out.last_book != 0);
        return out;
    }();
    return b;
}

std::string corrupt(const std::string& name, const std::string& sql) {
    static dbtest::TempDir dir("figures-corrupt");
    const std::string path = dir.file(name + ".db");
    dbtest::write_file(path, dbtest::read_file(base_path()));
    REQUIRE_MESSAGE(worldgen::db::testing::execute_on_file(path, sql).ok(), "SQL rejected: " << sql);
    return path;
}

void refused(const std::string& name, const std::string& sql, const std::string& message) {
    CAPTURE(name);
    const db::WorldReader r = chrono_test::open_world(corrupt(name, sql));
    const auto e = eng::view_error(r);
    REQUIRE_MESSAGE(e.has_value(), "HistoryView::load accepted " << name);
    CHECK(e->code == db::ErrorCode::corrupt_layer);
    CHECK(e->message == message);
    const auto t = h::read_history_tables(r);
    REQUIRE_FALSE(t.ok());
    CHECK(t.error().message == e->message);
}

std::string fig(std::int64_t id, const std::string& set) {
    return "UPDATE figure SET " + set + " WHERE id = " + n(id) + ";";
}
std::string F(std::int64_t id) { return "corrupt table 'figure': row " + n(id) + ": "; }

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-06 9.4: the generated figures-small world loads; each damaged figure row is refused with its message") {
    P506_REQUIRE_INTEGRATED();
    REQUIRE_FALSE(eng::view_error(chrono_test::open_world(base_path())).has_value());
    const Base& b = base();
    const auto& f1 = b.w.figs.at(b.ruler);
    const std::int64_t last = b.last_figure;
    refused("F1", fig(last, "id = " + n(last + 100)), F(last + 100) + "id " + n(last + 100) + " is not " + n(last));
    refused("F2", fig(b.ruler, "name = 'x1'"), F(b.ruler) + "name is not a name");
    refused("F3", fig(b.ruler, "epithet = ' the Old'"), F(b.ruler) + "epithet is not an epithet");
    refused("F4", fig(b.ruler, "role = 8"), F(b.ruler) + "role 8 outside 1 to 7");
    refused("F5", fig(b.ruler, "race = 99"), F(b.ruler) + "unknown race 99");
    refused("F6", fig(b.ruler, "culture = 99"), F(b.ruler) + "unknown culture 99");
    refused("F7", fig(b.ruler, "civ = 9999"), F(b.ruler) + "unknown civilization 9999");
    refused("F8", fig(b.ruler, "start_event = 99999999"), F(b.ruler) + "start_event 99999999 is not an event");
    refused("F9", fig(b.ruler, "born = office_from + 1"),
            F(b.ruler) + "born " + n(f1.office_from + 1) + " after office_from " + n(f1.office_from));
    refused("F12", fig(b.ruler, "status = 7"), F(b.ruler) + "status 7 outside 1 to 6");
    refused("F13", fig(b.alive, "death_region = 1"), F(b.alive) + "end columns disagree");
    refused("F18", fig(b.ruler, "reign_no = 0"), F(b.ruler) + "reign_no 0 of a ruler");
    refused("F18b", fig(b.general, "reign_no = 1"), F(b.general) + "reign_no 1 of a general");
    refused("F20", fig(b.ruler, "regnal = 1"), F(b.ruler) + "regnal 1 outside 0 or 2 to 39");
    refused("F21", fig(b.general, "regnal = 2"), F(b.general) + "regnal 2 of a general");
    refused("F24", fig(b.ruler, "temper = 5"), F(b.ruler) + "temper 5 outside 0 to 4");
    refused("F25", fig(b.general, "temper = 1"), F(b.general) + "temper 1 of a general");
    refused("F26", fig(b.general, "skill = 0"), F(b.general) + "skill 0 outside 1 to 1000");
    refused("F27", fig(b.ruler, "skill = 5"), F(b.ruler) + "skill 5 of a ruler");
}

TEST_CASE("P5-06 9.4: book, relation and event rows refused with their messages; a version 11 world refused") {
    P506_REQUIRE_INTEGRATED();
    const Base& b = base();
    const std::string B = "corrupt table 'book': row " + n(b.last_book) + ": ";
    refused("K2", "UPDATE book SET title = '' WHERE id = " + n(b.last_book) + ";", B + "title is not a name");
    refused("K3", "UPDATE book SET author = " + n(b.ruler) + " WHERE id = " + n(b.last_book) + ";",
            B + "author " + n(b.ruler) + " is not a caster");
    refused("E5", "DELETE FROM book WHERE id = " + n(b.last_book) + ";",
            "corrupt table 'event': row " + n(b.last_book_event) + ": book_written without its book row");
    refused("E2", "DELETE FROM event_participant WHERE event = " + n(b.battle_led) + " AND role IN (5, 6);",
            "corrupt table 'event': row " + n(b.battle_led) + ": battle_led without a general");
    if (b.figure_died != 0) {
        refused("E3", "UPDATE event_value SET value = 3 WHERE event = " + n(b.figure_died) + " AND seq = 1;",
                "corrupt table 'event': row " + n(b.figure_died) + ": value how 3 outside 0 to 2");
    }
    // a version 11 world: P0-02's reader refuses it at open
    const std::string p11 = corrupt("v11", "PRAGMA user_version = 11; UPDATE meta SET value = '11' WHERE key = "
                                           "'schema_version';");
    auto r = db::WorldReader::open(p11);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("world schema version 11, this build reads 14") != std::string::npos);
}

}  // TEST_SUITE
