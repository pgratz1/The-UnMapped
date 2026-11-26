// Spec P5-07 (docs/specs/p5-07-mountain-depths.md r2) test 16 items 10 and (r2) 18: the reader checks of 9 (r2: the
// warren and aid item, on a depths-small world with a warren at depth 2, a warren's waking and an aid), each on a
// corrupt copy
// of a generated depths-small world (rows changed by SQL), refused by HistoryView::load and read_history_tables alike
// with the same first message (code corrupt_layer, `corrupt table '<t>': row <k>: <reason>`); a schema 12 world (as
// 0.28.0 writes) refused at open with the version message. The row numbers are those of the base world; each
// corruption is built so that only one check fails. Where the spec gives no row or no bound, the reading is named
// (D-R10 of the reference: `hold_lost <e> did not ruin its settlement` is on the event's row; `dead` and `survivors`
// have no upper bound, so only the message's start is compared). Every case starts with P507_REQUIRE_INTEGRATED().

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "p5_07_api.hpp"
#include "p5_07_checks.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/history/digest.hpp"

namespace {

namespace db = worldgen::db;
namespace h = worldgen::history;
using p503check::at;
std::string n(std::int64_t v) { return std::to_string(v); }

// The first depths-small world of seeds 0 to 9 at 512 with a mine at depth 2, a sealing and a hold lost.
struct Base {
    std::string path;
    std::int64_t delve1 = 0, delve2 = 0, mine = 0, woken = 0, sealed = 0, lost = 0, lost_settlement = 0;
    std::int64_t deep_monster = 0, deep_kind = 0, wild_kind = 0, founder = 0, other_civ = 0, wild_monster = 0;
};
const Base& base() {
    static const Base b = [] {
        Base out;
        for (std::uint64_t seed = 0; seed < 10 && out.path.empty(); ++seed) {
            const std::string path = p507api::generated(seed, 512, p507api::depths_small(), "small");
            const db::WorldReader r = chrono_test::open_world(path);
            const p503check::World w = p503check::world_of_file(r, p507api::depths_small());
            const p503check::Index x(w);
            const auto dd = x.events_of("delving_deepened");
            const auto se = x.events_of("deep_sealed");
            const auto hl = x.events_of("hold_lost");
            std::int64_t d1 = 0, d2 = 0;
            for (const std::int64_t e : dd) {
                if (x.val(e, 0) == 2 && d2 == 0) {
                    d2 = e;
                    for (const std::int64_t y : dd) {
                        if (x.part(y, 2) == x.part(e, 2) && x.val(y, 0) == 1) d1 = y;
                    }
                }
            }
            if (d2 == 0 || se.empty() || hl.empty()) continue;
            out.path = path;
            out.delve1 = d1;
            out.delve2 = d2;
            out.mine = x.part(d2, 2);
            out.founder = x.part(d2, 1);
            out.sealed = se.front();
            out.woken = x.cause_list(out.sealed).front();
            out.deep_monster = x.part(out.sealed, 2);
            out.lost = hl.front();
            out.lost_settlement = at(*x.ev.at(out.lost), 6);
            for (const eng::Row& k : w.rows.t.monster_kind) {
                if (at(k, 5) == p507api::WAKE_DEEP && out.deep_kind == 0) out.deep_kind = at(k, 0);
                if (at(k, 5) == 1 && out.wild_kind == 0) out.wild_kind = at(k, 0);
            }
            for (const auto& [c, row] : x.civ) {
                if (c != out.founder && out.other_civ == 0) out.other_civ = c;
            }
            for (const eng::Row& m : w.rows.t.monster) {
                if (at(m, 2) <= 6 && out.wild_monster == 0) out.wild_monster = at(m, 0);
            }
        }
        REQUIRE_MESSAGE(!out.path.empty(), "no depths-small world of seeds 0 to 9 has a depth 2, a sealing and a fall");
        return out;
    }();
    return b;
}

// r2: the first depths-small world of seeds 0 to 9 at 512 with a warren at depth 2 and a warren's waking; its first
// aid (0 when none).
struct Base2 {
    std::string path;
    std::int64_t w1 = 0, w2 = 0, warren = 0, delver = 0, other_civ = 0, woken = 0, woken_settlement = 0, sealed = 0,
                 aid = 0;
};
const Base2& base2() {
    static const Base2 b = [] {
        Base2 out;
        for (std::uint64_t seed = 0; seed < 10 && out.path.empty(); ++seed) {
            const std::string path = p507api::generated(seed, 512, p507api::depths_small(), "small");
            const p503check::World w = p503check::world_of_file(chrono_test::open_world(path), p507api::depths_small());
            const p503check::Index x(w);
            std::int64_t w2 = 0, w1 = 0, wk = 0;
            for (const std::int64_t e : x.events_of("warren_delved")) {
                if (x.val(e, 0) != 2 || w2 != 0) continue;
                const std::int64_t t = at(*x.ev.at(e), 6);
                for (const std::int64_t y : x.events_of("warren_delved")) {
                    if (at(*x.ev.at(y), 6) == t && x.val(y, 0) == 1) w1 = y;
                }
                w2 = e;
            }
            for (const std::int64_t e : x.events_of("deep_woken")) {
                if (x.part(e, 3) == 0 && wk == 0) wk = e;
            }
            if (w2 == 0 || wk == 0) continue;
            out.path = path;
            out.w1 = w1;
            out.w2 = w2;
            out.warren = at(*x.ev.at(w2), 6);
            out.delver = x.part(w2, 1);
            out.woken = wk;
            out.woken_settlement = at(*x.ev.at(wk), 6);
            for (const std::int64_t e : x.events_of("warren_sealed")) {
                if (out.sealed == 0) out.sealed = e;
            }
            for (const std::int64_t e : x.events_of("ally_aided_hold")) {
                if (out.aid == 0) out.aid = e;
            }
            for (const auto& [c, row] : x.civ) {
                if (c != out.delver && out.other_civ == 0) out.other_civ = c;
            }
        }
        REQUIRE_MESSAGE(!out.path.empty(), "no depths-small world of seeds 0 to 9 has a warren at depth 2 and a waking");
        return out;
    }();
    return b;
}

std::string corrupt_of(const std::string& from, const std::string& name, const std::string& sql) {
    static dbtest::TempDir dir("depths-corrupt");
    const std::string path = dir.file(name + ".db");
    dbtest::write_file(path, dbtest::read_file(from));
    REQUIRE_MESSAGE(worldgen::db::testing::execute_on_file(path, sql).ok(), "SQL rejected: " << sql);
    return path;
}

std::string corrupt(const std::string& name, const std::string& sql) {
    static dbtest::TempDir dir("depths-corrupt-r1");
    const std::string path = dir.file(name + ".db");
    dbtest::write_file(path, dbtest::read_file(base().path));
    REQUIRE_MESSAGE(worldgen::db::testing::execute_on_file(path, sql).ok(), "SQL rejected: " << sql);
    return path;
}

std::string message_of(const std::string& path, const std::string& name) {
    CAPTURE(name);
    const db::WorldReader r = chrono_test::open_world(path);
    const auto e = eng::view_error(r);
    REQUIRE_MESSAGE(e.has_value(), "HistoryView::load accepted " << name);
    CHECK(e->code == db::ErrorCode::corrupt_layer);
    const auto t = h::read_history_tables(r);
    REQUIRE_FALSE(t.ok());
    CHECK(t.error().message == e->message);
    return e->message;
}
void refused2(const std::string& name, const std::string& sql, const std::string& message) {
    CHECK(message_of(corrupt_of(base2().path, name, sql), name) == message);
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
    CHECK_MESSAGE(m.rfind(prefix, 0) == 0, m);
}

std::string value(std::int64_t e, std::int64_t seq, std::int64_t v) {
    return "UPDATE event_value SET value = " + n(v) + " WHERE event = " + n(e) + " AND seq = " + n(seq) + ";";
}
std::string E(std::int64_t e) { return "corrupt table 'event': row " + n(e) + ": "; }

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-07 9: the base world loads; monster_kind and monster rows refused with their messages") {
    P507_REQUIRE_INTEGRATED();
    const Base& b = base();
    REQUIRE_FALSE(eng::view_error(chrono_test::open_world(b.path)).has_value());
    refused("K1", "UPDATE monster_kind SET wake = 4 WHERE id = " + n(b.deep_kind) + ";",
            "corrupt table 'monster_kind': row " + n(b.deep_kind) + ": wake 4 not 1 to 3");
    refused("K0", "UPDATE monster_kind SET wake = 0 WHERE id = " + n(b.deep_kind) + ";",
            "corrupt table 'monster_kind': row " + n(b.deep_kind) + ": wake 0 not 1 to 3");
    // a deep monster born of another event; a wild-kind monster born of a deep_woken
    if (b.wild_monster != 0) {
        const std::string sql = "UPDATE monster SET born_event = (SELECT born_event FROM monster WHERE id = " +
                                n(b.wild_monster) + ") WHERE id = " + n(b.deep_monster) + ";";
        const std::string m = first_message("M1", sql);
        CHECK_MESSAGE(m.find("corrupt table 'monster': row " + n(b.deep_monster) + ": born_event ") == 0U, m);
        CHECK_MESSAGE(m.find("is not the waking of this monster") != std::string::npos, m);
    }
}

TEST_CASE("P5-07 9: the event checks of kinds 93 to 96 refused with their messages") {
    P507_REQUIRE_INTEGRATED();
    const Base& b = base();
    // values in the ranges of 8.1 (P5-03 16.4's form)
    refused("V1", value(b.delve1, 2, 3), E(b.delve1) + "value telling 3 outside 0 to 2");
    refused("V2", value(b.sealed, 1, -1), E(b.sealed) + "value telling -1 outside 0 to 2");
    refused("V3", value(b.woken, 1, 4), E(b.woken) + "value depth 4 outside 1 to 3");
    refused_prefix("V4", value(b.lost, 1, -5), E(b.lost) + "value dead -5 outside 0 to ");
    refused_prefix("V5", value(b.lost, 2, -1), E(b.lost) + "value survivors -1 outside 0 to ");
    // depth order: the depth-2 event made depth 3 lacks depth 2 before it; the depth-1 event made 2 lacks depth 1
    refused("D1", value(b.delve1, 1, 2), E(b.delve1) + "depth 2 without depth 1");
    refused("D2", value(b.delve2, 1, 1), E(b.delve2) + "depth 1 again");
    // a deepening of another civilization's mine; the waking of another site
    refused("S1",
            "UPDATE event_participant SET entity_id = " + n(b.other_civ) + " WHERE event = " + n(b.delve1) +
                " AND seq = 1;",
            E(b.delve1) + "site " + n(b.mine) + " is not a mine of " + n(b.other_civ));
    // a second waking of one site: the fall's waking names the sealed one's site
    const p503check::World w = p503check::world_of_file(chrono_test::open_world(b.path), p507api::depths_small());
    const p503check::Index x(w);
    const std::int64_t fall_woken = x.cause_list(b.lost).front();
    if (x.kind_of(fall_woken) == "deep_woken" && x.part(fall_woken, 3) != x.part(b.woken, 3) && fall_woken > b.woken &&
        x.part(fall_woken, 2) == x.part(b.woken, 2)) {
        refused("W2",
                "UPDATE event_participant SET entity_id = " + n(x.part(b.woken, 3)) + " WHERE event = " +
                    n(fall_woken) + " AND seq = 3;",
                E(fall_woken) + "a second waking of site " + n(x.part(b.woken, 3)));
    }
    // a sealing and a first take without their waking
    refused("C1",
            "UPDATE event_cause SET cause = " + n(b.delve1) + " WHERE event = " + n(b.sealed) + " AND cause = " +
                n(b.woken) + ";",
            E(b.sealed) + "deep_sealed without its waking");
    if (x.kind_of(fall_woken) == "deep_woken") {
        refused("C2",
                // integration: the waking's own cause (its last delving), which lies before the take; b.delve1 can
                // lie after it, and then the earlier check `cause not below event` fires first
                "UPDATE event_cause SET cause = " + n(x.cause_list(fall_woken).front()) + " WHERE event = " +
                    n(b.lost) + " AND cause = " + n(fall_woken) + ";",
                E(b.lost) + "hold_lost without its waking");
    }
    // a hold_lost that is not its settlement's ruin event (reading D-R10: on the event's row)
    refused("R1",
            // integration: an earlier event of the same settlement, with the ending it derives (5), so that the
            // settlement row's own ending check (which runs before the event checks) passes
            "UPDATE settlement SET ending = 5, ruin_event = (SELECT MAX(id) FROM event WHERE id < " + n(b.lost) +
                " AND settlement = " + n(b.lost_settlement) + ") WHERE id = " + n(b.lost_settlement) + ";",
            E(b.lost) + "hold_lost " + n(b.lost) + " did not ruin its settlement");
}

TEST_CASE("P5-07 r2 9: the warren and aid checks refused with their messages") {
    P507_REQUIRE_INTEGRATED();
    const Base2& b = base2();
    REQUIRE_FALSE(eng::view_error(chrono_test::open_world(b.path)).has_value());
    // values of 97 to 99 in 8.1's ranges; 94's telling 0 to 5
    refused2("WV1", value(b.w1, 2, 3), E(b.w1) + "value telling 3 outside 0 to 2");
    refused2("WV2", value(b.w2, 1, 4), E(b.w2) + "value depth 4 outside 1 to 3");
    refused2("WV3", value(b.woken, 2, 6), E(b.woken) + "value telling 6 outside 0 to 5");
    // the depth chain per settlement (93's messages)
    refused2("WD1", value(b.w1, 1, 2), E(b.w1) + "depth 2 without depth 1");
    refused2("WD2", value(b.w2, 1, 1), E(b.w2) + "depth 1 again");
    // all of one settlement name one civilization
    refused2("WC1",
             "UPDATE event_participant SET entity_id = " + n(b.other_civ) + " WHERE event = " + n(b.w2) + " AND seq = 1;",
             E(b.w2) + "warren " + n(b.warren) + " delved by two realms");
    // a waking without a site whose settlement has no warren of its civilization: the waking's civilization changed
    refused2("WK1",
             "UPDATE event_participant SET entity_id = " + n(b.other_civ) + " WHERE event = " + n(b.woken) +
                 " AND seq = 2;",
             E(b.woken) + "waking of settlement " + n(b.woken_settlement) + " without a warren");
    if (b.sealed != 0) {
        const p503check::World w = p503check::world_of_file(chrono_test::open_world(b.path), p507api::depths_small());
        const p503check::Index x(w);
        const std::int64_t wk = x.cause_list(b.sealed).front();
        refused2("WS1",
                 "UPDATE event_cause SET cause = " + n(x.cause_list(wk).front()) + " WHERE event = " + n(b.sealed) +
                     " AND cause = " + n(wk) + ";",
                 E(b.sealed) + "warren_sealed without its waking");
    } else {
        MESSAGE("no warren_sealed in the base world: its check not run");
    }
    if (b.aid != 0) {
        const p503check::World w = p503check::world_of_file(chrono_test::open_world(b.path), p507api::depths_small());
        const p503check::Index x(w);
        // ally = partner
        refused2("WA1",
                 "UPDATE event_participant SET entity_id = " + n(x.part(b.aid, 2)) + " WHERE event = " + n(b.aid) +
                     " AND seq = 1;",
                 E(b.aid) + "ally_aided_hold " + n(b.aid) + " does not answer its waking");
    } else {
        MESSAGE("no ally_aided_hold in the base world: its checks not run");
    }
}

TEST_CASE("P5-07 9: a schema 12 world (as 0.28.0 writes) is refused with the version message") {
    P507_REQUIRE_INTEGRATED();
    const std::string p12 = corrupt("v12", "PRAGMA user_version = 12; UPDATE meta SET value = '12' WHERE key = "
                                           "'schema_version';");
    auto r = db::WorldReader::open(p12);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("world schema version 12, this build reads 13") != std::string::npos);
}

}  // TEST_SUITE
