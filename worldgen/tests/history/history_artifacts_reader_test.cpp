// Spec P5-11 (docs/specs/p5-11-artifacts.md r1) test 16 item 9: each reader check of 9 on a corrupt copy of an
// artifacts-small world, in both readers (HistoryView::load and read_history_tables) with the same first message. 9
// gives the message's frame `corrupt table '<t>': row <k>: <reason>` (and `holding <k> of artifact <a>: <reason>` for
// artifact_holding) but not every reason's words, so the tests compare that prefix. A schema 14 world is refused at
// open with the version message. Every case starts with P511_REQUIRE_INTEGRATED().

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "p3_04_engine_api.hpp"
#include "p5_11_property_common.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/history/digest.hpp"

namespace {

namespace db = worldgen::db;
namespace h = worldgen::history;
using p511_check::I;
std::string n(std::int64_t v) { return std::to_string(v); }

struct Base {
    std::string path;
    std::int64_t regalia_kind = 0, relic_kind = 0, arms_kind = 0;
    std::int64_t moved = 0;     // an artifact with two holdings or more
    std::int64_t smith = 0, smith_art = 0, god_art = 0, taken = 0;
};
const Base& base() {
    static const Base b = [] {
        Base out;
        for (std::uint64_t seed = 0; seed < 10 && out.path.empty(); ++seed) {
            const std::string path = p511api::generated(seed, 512, p511api::artifacts_small(), "small");
            const p511_check::World w = artifacts_prop::rows_of(path);
            Base x;
            for (const auto& [k, r] : w.kind) {
                if (I(*r, 4) == p511api::REGALIA && x.regalia_kind == 0) x.regalia_kind = k;
                if (I(*r, 4) == p511api::RELIC && x.relic_kind == 0) x.relic_kind = k;
                if (I(*r, 4) == p511api::ARMS && x.arms_kind == 0) x.arms_kind = k;
            }
            for (const auto& [a, r] : w.art) {
                if (w.hold.at(a).size() >= 2U && x.moved == 0) x.moved = a;
                if (I(*r, 4) == p511api::SMITH_MADE && x.smith_art == 0) {
                    x.smith_art = a;
                    x.smith = I(*r, 5);
                }
                if (I(*r, 4) == p511api::GOD_GIVEN && x.god_art == 0) x.god_art = a;
            }
            if (!w.of("artifact_taken").empty()) x.taken = w.of("artifact_taken")[0];
            if (x.moved && x.smith_art && x.god_art && x.taken && x.regalia_kind && x.relic_kind && x.arms_kind) {
                x.path = path;
                out = x;
            }
        }
        REQUIRE_MESSAGE(!out.path.empty(),
                        "no artifacts-small world of seeds 0 to 9 has a moved, a smith-made, a god-given and a taken artifact");
        return out;
    }();
    return b;
}

std::string corrupt(const std::string& name, const std::string& sql) {
    static dbtest::TempDir dir("artifacts-corrupt");
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
void refused_prefix(const std::string& name, const std::string& sql, const std::string& prefix) {
    const std::string m = first_message(name, sql);
    CAPTURE(m);
    CHECK(m.rfind(prefix, 0) == 0U);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-11 9: the artifact_kind rows") {
    P511_REQUIRE_INTEGRATED();
    const Base& b = base();
    const std::string rk = n(b.regalia_kind), lk = n(b.relic_kind), ak = n(b.arms_kind);
    refused_prefix("K1", "UPDATE artifact_kind SET class = 6 WHERE id = " + ak + ";",
                   "corrupt table 'artifact_kind': row " + ak + ": ");
    refused_prefix("K2", "UPDATE artifact_kind SET battle_permille = 10 WHERE id = " + rk + ";",
                   "corrupt table 'artifact_kind': row " + rk + ": ");
    refused_prefix("K3", "UPDATE artifact_kind SET battle_permille = 501 WHERE id = " + ak + ";",
                   "corrupt table 'artifact_kind': row " + ak + ": ");
    refused_prefix("K4", "UPDATE artifact_kind SET hunt_permille = 10 WHERE id = " + lk + ";",
                   "corrupt table 'artifact_kind': row " + lk + ": ");
    refused_prefix("K5", "UPDATE artifact_kind SET hunt_permille = 1001 WHERE id = " + ak + ";",
                   "corrupt table 'artifact_kind': row " + ak + ": ");
    refused_prefix("K6", "UPDATE artifact_kind SET claim_permille = 1001 WHERE id = " + ak + ";",
                   "corrupt table 'artifact_kind': row " + ak + ": ");
    refused_prefix("K7", "UPDATE artifact_kind SET id = id + 100 WHERE id = 1;", "corrupt table '");
}

TEST_CASE("P5-11 9: the artifact rows") {
    P511_REQUIRE_INTEGRATED();
    const Base& b = base();
    const std::string a = n(b.smith_art), g = n(b.god_art);
    const std::string pa = "corrupt table 'artifact': row " + a + ": ", pg = "corrupt table 'artifact': row " + g + ": ";
    refused_prefix("A1", "UPDATE artifact SET kind = 999 WHERE id = " + a + ";", pa);
    refused_prefix("A2", "UPDATE artifact SET civ = 999 WHERE id = " + a + ";", pa);
    refused_prefix("A3", "UPDATE artifact SET made_event = occasion_event WHERE id = " + a + ";", pa);
    refused_prefix("A4", "UPDATE artifact SET made_year = made_year + 1 WHERE id = " + a + ";", pa);
    refused_prefix("A5", "UPDATE artifact SET maker_kind = 5 WHERE id = " + a + ";", pa);
    refused_prefix("A6", "UPDATE artifact SET maker_kind = 3 WHERE id = " + a + ";", pa);
    refused_prefix("A7", "UPDATE artifact SET deep = 1 WHERE id = " + g + ";", pg);
    refused_prefix("A8", "UPDATE artifact SET god = 0 WHERE id = " + g + ";", pg);
    refused_prefix("A9", "UPDATE artifact SET maker_civ = maker_civ + 1 WHERE id = " + a + ";", pa);
    refused_prefix("A10", "UPDATE artifact SET occasion_event = 1 WHERE id = " + a + ";", pa);
    refused_prefix("A11", "UPDATE artifact SET status = (status % 6) + 1 WHERE id = " + a + ";", pa);
    refused_prefix("A12", "UPDATE artifact SET id = id + 1000000 WHERE id = " + a + ";", "corrupt table '");
}

TEST_CASE("P5-11 9: the artifact_holding rows") {
    P511_REQUIRE_INTEGRATED();
    const Base& b = base();
    const std::string a = n(b.moved);
    const std::string pre = "corrupt table 'artifact_holding': row ";
    refused_prefix("H1", "DELETE FROM artifact_holding WHERE artifact = " + a + " AND seq = 1;", pre);
    refused_prefix("H2", "UPDATE artifact_holding SET to_year = NULL WHERE artifact = " + a + " AND seq = 1;", pre);
    refused_prefix("H3", "UPDATE artifact_holding SET until_event = 0 WHERE artifact = " + a + " AND seq = 1;", pre);
    refused_prefix("H4", "UPDATE artifact_holding SET state = 7 WHERE artifact = " + a + " AND seq = 1;", pre);
    refused_prefix("H5", "UPDATE artifact_holding SET holder_type = 8 WHERE artifact = " + a + " AND seq = 1;", pre);
    refused_prefix("H6", "UPDATE artifact_holding SET holder = 99999 WHERE artifact = " + a + " AND seq = 1;", pre);
    refused_prefix("H7", "UPDATE artifact_holding SET from_year = from_year + 1 WHERE artifact = " + a + " AND seq = 1;",
                   pre);
    refused_prefix("H8", "UPDATE artifact_holding SET event = 1 WHERE artifact = " + a + " AND seq = 1;", pre);
    refused_prefix("H9", "UPDATE artifact_holding SET state = 1, holder_type = 3, region = 1 WHERE artifact = " + a +
                             " AND seq = 1;", pre);
    refused_prefix("H10", "UPDATE artifact_holding SET to_year = 5 WHERE artifact = " + a +
                              " AND seq = (SELECT MAX(seq) FROM artifact_holding WHERE artifact = " + a + ");", pre);
    // the frame of 9's holding reasons
    const std::string m = first_message("H11", "UPDATE artifact_holding SET state = 7 WHERE artifact = " + a +
                                                   " AND seq = 1;");
    CHECK(m.find("holding 1 of artifact " + a + ": ") != std::string::npos);
}

TEST_CASE("P5-11 9: the figure rows (role 8 smith) and the event values and participants of 111 to 116") {
    P511_REQUIRE_INTEGRATED();
    const Base& b = base();
    refused_prefix("R1", "UPDATE figure SET role = 9 WHERE id = " + n(b.smith) + ";",
                   "corrupt table 'figure': row " + n(b.smith) + ": ");
    refused_prefix("R2", "UPDATE figure SET start_event = 1 WHERE id = " + n(b.smith) + ";",
                   "corrupt table 'figure': row " + n(b.smith) + ": ");
    refused_prefix("V1", "UPDATE event_value SET value = 5 WHERE event = " + n(b.taken) + " AND seq = 1;", "corrupt table '");
    refused_prefix("V2", "UPDATE event_value SET value = 0 WHERE event = " + n(b.taken) + " AND seq = 1;", "corrupt table '");
    refused_prefix("P1", "UPDATE event_participant SET entity_id = 99999 WHERE event = " + n(b.taken) +
                             " AND entity_type = 10;", "corrupt table '");
    refused_prefix("P2", "DELETE FROM event_participant WHERE event = (SELECT made_event FROM artifact WHERE id = " +
                             n(b.smith_art) + ") AND role = 3;", "corrupt table '");
}

TEST_CASE("P5-11 9: a schema 14 world (as 0.30.0 writes) is refused with the version message") {
    P511_REQUIRE_INTEGRATED();
    const std::string p14 = corrupt("v14", "PRAGMA user_version = 14; UPDATE meta SET value = '14' WHERE key = "
                                           "'schema_version';");
    auto r = db::WorldReader::open(p14);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("world schema version 14, this build reads 15") != std::string::npos);
}

}  // TEST_SUITE
