// Spec P5-08 (docs/specs/p5-08-pantheon-cults.md r1) test 16 item 10: each reader check of 9 on a corrupt copy of a
// pantheon-small world, in both readers (HistoryView::load and read_history_tables) with the same first message.
// Where 9 gives the reason's words they are compared whole (`domain <d> not 2 to 9 for a pantheon god`, `faction <k>
// hidden without a sanctum`, `kind <k> not 1 to 7`); elsewhere the message's prefix `corrupt table '<t>': row <k>: `
// (reading P-R15 of the reference). A schema 13 world is refused at open with the version message. Every case starts
// with P508_REQUIRE_INTEGRATED().

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "p3_04_engine_api.hpp"
#include "p5_08_property_common.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/history/digest.hpp"

namespace {

namespace db = worldgen::db;
namespace h = worldgen::history;
using p508_check::I;
std::string n(std::int64_t v) { return std::to_string(v); }

struct Base {
    std::string path;
    std::int64_t faction = 0, hidden_faction = 0, temple = 0, shrine = 0, sanctum = 0, champion = 0, prophet = 0;
    std::int64_t pantheon_god = 0, curse = 0, mark = 0, spread = 0;
};
const Base& base() {
    static const Base b = [] {
        Base out;
        for (std::uint64_t seed = 0; seed < 10 && out.path.empty(); ++seed) {
            const std::string path = p508api::generated(seed, 512, p508api::pantheon_small(), "small");
            const p508_check::World w = pantheon_prop::rows_of(path);
            Base x;
            for (const auto& [k, f] : w.faction) {
                if (x.faction == 0) x.faction = k;
                if (I(*f, 12) == p508api::STATUS_HIDDEN && x.hidden_faction == 0) x.hidden_faction = k;
            }
            for (const auto& [s, r] : w.site) {
                if (I(*r, 2) == p508api::SITE_TEMPLE && x.temple == 0) x.temple = s;
                if (I(*r, 2) == p508api::SITE_SHRINE && x.shrine == 0) x.shrine = s;
                if (I(*r, 2) == p508api::SITE_SANCTUM && x.sanctum == 0) x.sanctum = s;
            }
            for (const auto& [f, r] : w.figure) {
                if (I(*r, 3) == p508api::ROLE_CHAMPION && x.champion == 0) x.champion = f;
                if (I(*r, 3) == p508api::ROLE_PROPHET && x.prophet == 0) x.prophet = f;
            }
            x.pantheon_god = static_cast<std::int64_t>(w.rows("race").size()) + 1;
            if (!w.of("divine_curse").empty()) x.curse = w.of("divine_curse")[0];
            if (!w.of("divine_mark").empty()) x.mark = w.of("divine_mark")[0];
            if (!w.of("cult_spread").empty()) x.spread = w.of("cult_spread")[0];
            if (x.faction && x.temple && x.shrine && x.champion && x.curse) {
                x.path = path;
                out = x;
            }
        }
        REQUIRE_MESSAGE(!out.path.empty(), "no pantheon-small world of seeds 0 to 9 has a cult, a shrine and a champion");
        return out;
    }();
    return b;
}

std::string corrupt(const std::string& name, const std::string& sql) {
    static dbtest::TempDir dir("pantheon-corrupt");
    const std::string path = dir.file(name + ".db");
    dbtest::write_file(path, dbtest::read_file(base().path));
    REQUIRE_MESSAGE(worldgen::db::testing::execute_on_file(path, sql).ok(), "SQL rejected: " << sql);
    return path;
}

std::string first_message(const std::string& name, const std::string& sql) {
    CAPTURE(name);
    const db::WorldReader r = chrono_test::open_world(corrupt(name, sql));
    const auto e = eng::view_error(r);
    REQUIRE_MESSAGE(e.has_value(), "HistoryView::load accepted " << name);
    CHECK(e->code == db::ErrorCode::corrupt_layer);
    const auto t = h::read_history_tables(r);
    REQUIRE_FALSE(t.ok());
    CHECK(t.error().message == e->message);
    return e->message;
}
void refused(const std::string& name, const std::string& sql, const std::string& message) {
    CHECK(first_message(name, sql) == message);
}
void refused_prefix(const std::string& name, const std::string& sql, const std::string& prefix) {
    const std::string m = first_message(name, sql);
    CAPTURE(m);
    CHECK(m.rfind(prefix, 0) == 0U);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-08 9: the god rows") {
    P508_REQUIRE_INTEGRATED();
    const Base& b = base();
    const std::string g = n(b.pantheon_god);
    refused("G1", "UPDATE god SET domain = 1 WHERE id = " + g + ";",
            "corrupt table 'god': row " + g + ": domain 1 not 2 to 9 for a pantheon god");
    refused("G2", "UPDATE god SET domain = 10 WHERE id = " + g + ";",
            "corrupt table 'god': row " + g + ": domain 10 not 2 to 9 for a pantheon god");
    refused_prefix("G3", "UPDATE god SET race = 1 WHERE id = " + g + ";", "corrupt table 'god': row " + g + ": ");
    refused_prefix("G4", "UPDATE god SET domain = 2 WHERE id = 1;", "corrupt table 'god': row 1: ");
    refused_prefix("G5", "UPDATE god SET race = 2 WHERE id = 1;", "corrupt table 'god': row 1: ");
    refused_prefix("G6", "UPDATE god SET camp = 3 WHERE id = " + g + ";", "corrupt table 'god': row " + g + ": ");
    // a missing god row may be met first as the participant of an event that names it (integration)
    refused_prefix("G7", "DELETE FROM god WHERE id = " + g + ";", "corrupt table '");
}

TEST_CASE("P5-08 9: the faction rows") {
    P508_REQUIRE_INTEGRATED();
    const Base& b = base();
    const std::string k = n(b.faction), pre = "corrupt table 'faction': row " + n(b.faction) + ": ";
    refused_prefix("F1", "UPDATE faction SET kind = 2 WHERE id = " + k + ";", pre);
    refused_prefix("F2", "UPDATE faction SET god = 999 WHERE id = " + k + ";", pre);
    refused_prefix("F3", "UPDATE faction SET civ = 999 WHERE id = " + k + ";", pre);
    refused_prefix("F4", "UPDATE faction SET founder = 99999 WHERE id = " + k + ";", pre);
    refused_prefix("F5", "UPDATE faction SET seat = " + n(b.shrine) + " WHERE id = " + k + ";", pre);
    refused_prefix("F6", "UPDATE faction SET founding_event = 1 WHERE id = " + k + ";", pre);
    refused_prefix("F7", "UPDATE faction SET status = 4 WHERE id = " + k + ";", pre);
    refused_prefix("F8", "UPDATE faction SET status = 3, ending = 0, ended_year = NULL WHERE id = " + k + ";", pre);
    refused_prefix("F9", "UPDATE faction SET status = 1, ending = 1 WHERE id = " + k + ";", pre);
    refused_prefix("F10", "UPDATE faction SET id = id + 100 WHERE id = " + k + ";", "corrupt table '");
    refused("F11", "UPDATE faction SET status = 2, ending = 0, ended_year = NULL, ending_event = 0 WHERE id = " + k +
                       " AND status <> 2;",
            pre + "faction " + k + " hidden without a sanctum");
}

TEST_CASE("P5-08 9: the site, figure and monster rows") {
    P508_REQUIRE_INTEGRATED();
    const Base& b = base();
    refused("S1", "UPDATE site SET kind = 8 WHERE id = " + n(b.shrine) + ";",
            "corrupt table 'site': row " + n(b.shrine) + ": kind 8 not 1 to 7");
    refused_prefix("S2", "UPDATE site SET ending = 4, ending_event = origin_event, ended_year = origin_year WHERE id = " +
                             n(b.shrine) + ";",
                   "corrupt table 'site': row " + n(b.shrine) + ": ending 4 is not ");
    refused_prefix("S3", "UPDATE site SET origin_event = 1 WHERE id = " + n(b.temple) + ";",
                   "corrupt table 'site': row " + n(b.temple) + ": ");
    refused_prefix("S4", "UPDATE site SET ending = 5, ending_event = origin_event, ended_year = origin_year WHERE id = " +
                             n(b.temple) + ";",
                   "corrupt table 'site': row " + n(b.temple) + ": ending 5 is not ");
    refused_prefix("S5", "UPDATE site SET origin_event = 1 WHERE id = " + n(b.shrine) + ";",
                   "corrupt table 'site': row " + n(b.shrine) + ": ");
    if (b.sanctum != 0) {
        refused_prefix("S6", "UPDATE site SET origin_event = 1 WHERE id = " + n(b.sanctum) + ";",
                       "corrupt table 'site': row " + n(b.sanctum) + ": ");
    }
    refused_prefix("R1", "UPDATE figure SET role = 8 WHERE id = " + n(b.champion) + ";",
                   "corrupt table 'figure': row " + n(b.champion) + ": ");
    refused_prefix("R2", "UPDATE figure SET start_event = 1 WHERE id = " + n(b.champion) + ";",
                   "corrupt table 'figure': row " + n(b.champion) + ": ");
    if (b.prophet != 0) {
        refused_prefix("R3", "UPDATE figure SET start_event = 1 WHERE id = " + n(b.prophet) + ";",
                       "corrupt table 'figure': row " + n(b.prophet) + ": ");
    }
}

TEST_CASE("P5-08 9: event values and the faction order of cult events") {
    P508_REQUIRE_INTEGRATED();
    const Base& b = base();
    const p508_check::World w = pantheon_prop::rows_of(b.path);
    const std::int64_t eff = w.value_seq.at(p508api::K_CURSE).at("effect");
    refused_prefix("V1", "UPDATE event_value SET value = 4 WHERE event = " + n(b.curse) + " AND seq = " + n(eff) + ";",
                   "corrupt table 'event_value': row (" + n(b.curse) + ", ");   // its key (event, seq)
    const std::int64_t ds = w.value_seq.at(p508api::K_CURSE).at("divine_script");
    refused_prefix("V2", "UPDATE event_value SET value = 2 WHERE event = " + n(b.curse) + " AND seq = " + n(ds) + ";",
                   "corrupt table 'event_value': row (" + n(b.curse) + ", ");
    if (b.spread != 0) {
        refused_prefix("V3", "UPDATE event_participant SET entity_id = 99999 WHERE event = " + n(b.spread) + " AND seq = 1;",
                       "corrupt table '");
    }
}

TEST_CASE("P5-08 9: a schema 13 world (as 0.29.0 writes) is refused with the version message") {
    P508_REQUIRE_INTEGRATED();
    const std::string p13 = corrupt("v13", "PRAGMA user_version = 13; UPDATE meta SET value = '13' WHERE key = "
                                           "'schema_version';");
    auto r = db::WorldReader::open(p13);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("world schema version 13, this build reads 14") != std::string::npos);
}

}  // TEST_SUITE
