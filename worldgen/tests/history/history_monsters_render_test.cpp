// Spec P5-02 (docs/specs/p5-02-beasts-monsters.md r0) 10 and 11.4, 21.3; tests 19.1 items 13 and 14: the compiled
// kinds 44 to 55 (10.1), their shipped entries (10.2) and template_problem (form `kind`, `{@beast}`, 10.3), the
// sentences of every new kind on hand-made rows added by SQL to a copy of a generated 512 world (display names of
// entity types 8 and 9, the form kind, {@beast}, the three-word values, the names-inside rule on `The Host of
// ...`), and the reader's checks of 11.4 and 21.3 (A1, A2): each a corrupt copy whose HistoryView::load and
// read_history_tables give `corrupt table '<table>': row <key>: <reason>` (code corrupt_layer).
//
// Written from the spec before the implementation (test-writer rule). Library names used: eng::compiled,
// catalogue_of, template_problem, view_error, render (all older than P5-02), and the five tables of schema 9 by
// name through SQL. Composite keys print as `(<a>, <b>, <c>)`, as region_name's do (P3-07 26.6; reading R2).

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "p3_04_engine_golden_support.hpp"
#include "p3_04_engine_support.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/history/digest.hpp"

namespace {

using namespace engine_golden;
namespace h = worldgen::history;

std::string q(const std::string& s) {
    std::string out = "'";
    for (const char c : s) {
        out += c;
        if (c == '\'') out += '\'';
    }
    return out + "'";
}

std::string roles_of(const eng::KindDef& k) {
    std::string s;
    for (const eng::RoleDef& r : k.roles) {
        s += std::to_string(r.seq) + ":" + r.role + ":" + std::to_string(static_cast<std::int64_t>(r.type)) + ":" +
             std::to_string(r.min_count) + "-" + std::to_string(r.max_count) + ";";
    }
    return s;
}

std::string values_of(const eng::KindDef& k) {
    std::string s;
    for (const std::string& v : k.values) s += (s.empty() ? "" : ",") + v;
    return s;
}

std::string n(std::int64_t v) { return std::to_string(v); }

// The rows the test adds to a copy of geo-seed-42-512 (generated at 0.23.0; its own monster and site rows, if
// any, keep their ids and the added ones follow). Region R = settlement 1's, named King's Fold; civilizations 1
// and 2 The Snat Horde and The Stinrild League; settlements 1 and 2 Drutnot Hole and Nutskrank.
struct Scene {
    std::string path;
    std::int64_t region = 0, year = 0, first_event = 0, m0 = 0, s0 = 0;
    std::map<std::string, std::int64_t> ev;   // label -> event id
};

struct Added {
    std::string sql;
    std::int64_t eid = 0;
};

const Scene& scene() {
    static const Scene sc = [] {
        static dbtest::TempDir dir("monsters-render");
        const std::string src = world_of(golden().world("geo-seed-42-512"));
        const db::WorldReader base = open_world(src);
        Scene s;
        std::int64_t eid = static_cast<std::int64_t>(rows_of(base, "event").size());
        s.first_event = eid + 1;
        const std::int64_t phase = static_cast<std::int64_t>(rows_of(base, "phase").size());
        const auto st = rows_of(base, "settlement");
        s.region = st.at(0).at(4).integer;
        const std::int64_t x1 = st.at(0).at(2).integer, y1 = st.at(0).at(3).integer;
        const std::int64_t x2 = st.at(1).at(2).integer, y2 = st.at(1).at(3).integer;
        s.year = std::stoll(base.meta("history_end_year").value_or("6000")) - 1;
        s.m0 = static_cast<std::int64_t>(rows_of(base, "monster").size());
        s.s0 = static_cast<std::int64_t>(rows_of(base, "site").size());
        std::map<std::string, std::int64_t> kid;
        for (const auto& k : rows_of(base, "event_kind")) kid[k.at(1).bytes] = k.at(0).integer;
        const std::string Y = n(s.year), R = n(s.region), P = n(phase);
        const std::int64_t m1 = s.m0 + 1, m2 = s.m0 + 2, m3 = s.m0 + 3, m4 = s.m0 + 4;
        const std::int64_t s1 = s.s0 + 1, s2 = s.s0 + 2, s3 = s.s0 + 3, s4 = s.s0 + 4, s5 = s.s0 + 5, s6 = s.s0 + 6;
        std::string sql =
            "UPDATE civilization SET name = 'The Snat Horde' WHERE id = 1;"
            "UPDATE civilization SET name = 'The Stinrild League' WHERE id = 2;"
            "UPDATE settlement SET name = 'Drutnot Hole' WHERE id = 1;"
            "UPDATE settlement SET name = 'Nutskrank' WHERE id = 2;"
            "UPDATE region SET name = 'King''s Fold' WHERE id = " + R + ";"
            "UPDATE region_name SET name = 'King''s Fold' WHERE region = " + R + ";";
        // ev(label, kind, settlement, parts "seq:role:type:id", values, causes)
        auto add = [&](const std::string& label, const std::string& key, std::int64_t settlement,
                       const std::vector<std::tuple<std::int64_t, std::int64_t, std::int64_t>>& parts,
                       const std::vector<std::int64_t>& vals, const std::vector<std::int64_t>& causes) {
            REQUIRE_MESSAGE(kid.count(key) == 1U, "kind " << key << " not in the world's catalogue");
            const std::string e = n(++eid);
            s.ev[label] = eid;
            sql += "INSERT INTO event VALUES (" + e + ", " + Y + ", " + Y + ", " + n(kid.at(key)) + ", " + P + ", " + R +
                   ", " + n(settlement) + ", 30);";
            for (std::size_t i = 0; i < parts.size(); ++i) {
                sql += "INSERT INTO event_participant VALUES (" + e + ", " + n(static_cast<std::int64_t>(i + 1)) + ", " +
                       n(std::get<0>(parts[i])) + ", " + n(std::get<1>(parts[i])) + ", " + n(std::get<2>(parts[i])) +
                       ");";
            }
            for (std::size_t i = 0; i < vals.size(); ++i) {
                sql += "INSERT INTO event_value VALUES (" + e + ", " + n(static_cast<std::int64_t>(i + 1)) + ", " +
                       n(vals[i]) + ");";
            }
            for (const std::int64_t c : causes) sql += "INSERT INTO event_cause VALUES (" + e + ", " + n(c) + ");";
        };
        add("battle", "battle", 0, {{1, 3, 1}, {2, 3, 2}}, {10, 20}, {1});
        add("44", "monster_appeared", 0, {{1, 8, m1}, {2, 9, s1}}, {}, {});
        add("45", "undead_rose", 0, {{1, 8, m2}, {2, 9, s2}}, {}, {s.ev["battle"]});
        add("46", "monster_woke", 0, {{1, 8, m2}}, {}, {s.ev["45"]});
        add("47", "monster_slept", 0, {{1, 8, m1}, {2, 9, s1}}, {}, {s.ev["44"]});
        add("48a", "monster_raid", 1, {{1, 8, m1}, {2, 3, 1}}, {0, 3}, {s.ev["44"]});
        add("48b", "monster_raid", 1, {{1, 8, m1}, {2, 3, 1}}, {1, 3}, {s.ev["44"]});
        add("48c", "monster_raid", 1, {{1, 8, m1}, {2, 3, 1}}, {41, 3}, {s.ev["44"]});
        add("49", "settlement_overrun", 2, {{1, 8, m2}, {2, 3, 1}, {3, 9, s3}}, {41}, {s.ev["48c"]});
        add("44b", "monster_appeared", 0, {{1, 8, m3}, {2, 9, s5}}, {}, {});
        add("47b", "monster_slept", 0, {{1, 8, m3}, {2, 9, s5}}, {}, {s.ev["44b"]});
        add("50", "monster_slain", 0, {{1, 8, m3}, {2, 3, 2}}, {30}, {s.ev["48c"]});
        add("51", "monster_driven_off", 0, {{1, 8, m1}, {2, 3, 1}}, {}, {s.ev["48c"]});
        add("44c", "monster_appeared", 0, {{1, 8, m4}, {2, 9, s6}}, {}, {});
        add("52", "monster_died", 0, {{1, 8, m4}}, {}, {s.ev["44c"]});
        add("53", "beasts_bold", 1, {{1, 4, 1}, {2, 3, 1}}, {5, 3}, {});
        add("54", "outpost_built", 0, {{1, 3, 1}, {2, 3, 2}, {3, 9, s4}}, {}, {s.ev["battle"]});
        add("55a", "outpost_abandoned", 0, {{1, 3, 1}, {2, 9, s4}}, {0}, {});
        add("55b", "outpost_abandoned", 0, {{1, 3, 1}, {2, 9, s4}}, {1}, {});
        add("55c", "outpost_abandoned", 0, {{1, 3, 1}, {2, 9, s4}}, {2}, {});
        // monster: id, name, kind, lair_site, born, born_event, died, death_event, slain_by, status, hoard
        sql += "INSERT INTO monster VALUES (" + n(m1) + ", 'Old Grakh', 4, " + n(s1) + ", " + Y + ", " + n(s.ev["44"]) +
               ", NULL, 0, 0, 1, 9);";
        sql += "INSERT INTO monster VALUES (" + n(m2) + ", 'The Host of Edric', 5, " + n(s3) + ", " + Y + ", " +
               n(s.ev["45"]) + ", NULL, 0, 0, 2, 0);";
        sql += "INSERT INTO monster VALUES (" + n(m3) + ", 'Saelith the Black', 1, " + n(s5) + ", " + Y + ", " +
               n(s.ev["44b"]) + ", " + Y + ", " + n(s.ev["50"]) + ", 2, 3, 0);";
        sql += "INSERT INTO monster VALUES (" + n(m4) + ", 'Uguurz the Glutton', 3, " + n(s6) + ", " + Y + ", " +
               n(s.ev["44c"]) + ", " + Y + ", " + n(s.ev["52"]) + ", 0, 4, 0);";
        // site: id, name, kind, region, x, y, settlement, founder_civ, monster, origin_year, origin_event, ended_year,
        // ending_event, ending
        auto site = [&](std::int64_t id, const std::string& name, std::int64_t kind, std::int64_t x, std::int64_t y,
                        std::int64_t settlement, std::int64_t founder, std::int64_t monster, const std::string& origin) {
            sql += "INSERT INTO site VALUES (" + n(id) + ", " + q(name) + ", " + n(kind) + ", " + R + ", " + n(x) + ", " +
                   n(y) + ", " + n(settlement) + ", " + n(founder) + ", " + n(monster) + ", " + Y + ", " +
                   n(s.ev[origin]) + ", NULL, 0, 0);";
        };
        site(s1, "Grakh's Hollow", 1, x1, y1, 0, 0, m1, "44");
        site(s2, "The Barrows of Edric", 1, x1, y1, 0, 0, m2, "45");
        site(s3, "Edric's Howe", 1, x2, y2, 2, 0, m2, "49");
        site(s4, "Snat Watch", 2, x1, y1, 0, 1, 0, "54");
        site(s5, "The Lair of Saelith", 1, x1, y1, 0, 0, m3, "44b");
        site(s6, "The Pit of Uguurz", 1, x1, y1, 0, 0, m4, "44c");
        s.path = dir.file("scene.db");
        dbtest::write_file(s.path, dbtest::read_file(src));
        REQUIRE_MESSAGE(worldgen::db::testing::execute_on_file(s.path, sql).ok(), "SQL rejected");
        return s;
    }();
    return sc;
}

// A copy of the scene with `sql` applied.
std::string corrupt(const std::string& name, const std::string& sql) {
    static dbtest::TempDir dir("monsters-corrupt");
    const std::string path = dir.file(name + ".db");
    dbtest::write_file(path, dbtest::read_file(scene().path));
    REQUIRE_MESSAGE(worldgen::db::testing::execute_on_file(path, sql).ok(), "SQL rejected: " << sql);
    return path;
}

void refused(const std::string& name, const std::string& sql, const std::string& message) {
    CAPTURE(name);
    const std::string path = corrupt(name, sql);
    const db::WorldReader r = open_world(path);
    const auto e = eng::view_error(r);
    REQUIRE_MESSAGE(e.has_value(), "HistoryView::load accepted " << name);
    CHECK(e->code == db::ErrorCode::corrupt_layer);
    CHECK(e->message == message);
    const auto t = h::read_history_tables(r);   // 11.4: one function serves both reads
    REQUIRE_FALSE(t.ok());
    CHECK(t.code() == db::ErrorCode::corrupt_layer);
    CHECK(t.error().message == message);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-02 10.1: kinds 44 to 55 appended in the table's order, their uses, roles and values") {
    const auto& c = eng::compiled();
    REQUIRE(c.kinds.size() == 92U)  /* P5-04 8.1 (0.26.0): kinds 63 to 78 */;
    struct K {
        std::int64_t id;
        const char* key;
        bool caused;
        std::int64_t region_use, settlement_use;
        const char* roles;
        const char* values;
    };
    for (const K& k : {K{44, "monster_appeared", false, 1, 0, "1:monster:8:1-1;2:lair:9:1-1;", ""},
                       K{45, "undead_rose", true, 1, 2, "1:monster:8:1-1;2:lair:9:1-1;", ""},
                       K{46, "monster_woke", true, 1, 0, "1:monster:8:1-1;", ""},
                       K{47, "monster_slept", true, 1, 0, "1:monster:8:1-1;2:lair:9:1-1;", ""},
                       K{48, "monster_raid", true, 1, 1, "1:monster:8:1-1;2:civilization:3:1-1;", "dead,plunder"},
                       K{49, "settlement_overrun", true, 1, 1,
                         "1:monster:8:1-1;2:civilization:3:1-1;3:lair:9:1-1;", "survivors"},
                       K{50, "monster_slain", true, 1, 0, "1:monster:8:1-1;2:slayer:3:1-1;", "hunters"},
                       K{51, "monster_driven_off", true, 1, 0, "1:monster:8:1-1;2:civilization:3:1-1;", ""},
                       K{52, "monster_died", true, 1, 0, "1:monster:8:1-1;", ""},
                       K{53, "beasts_bold", false, 1, 1, "1:settlement:4:1-1;2:civilization:3:1-1;", "beast,dead"},
                       K{54, "outpost_built", true, 1, 0, "1:civilization:3:1-1;2:enemy:3:1-1;3:site:9:1-1;", ""},
                       K{55, "outpost_abandoned", false, 1, 0, "1:civilization:3:1-1;2:site:9:1-1;", "reason"}}) {
        CAPTURE(k.key);
        const eng::KindDef& d = c.kinds[static_cast<std::size_t>(k.id - 1)];
        CHECK(d.id == k.id);
        CHECK(d.key == k.key);
        CHECK(d.caused == k.caused);
        CHECK(d.region_use == k.region_use);
        CHECK(d.settlement_use == k.settlement_use);
        CHECK(roles_of(d) == k.roles);
        CHECK(values_of(d) == k.values);
    }
}

TEST_CASE("P5-02 10.2: the shipped entries of kinds 44 to 55 pass template_problem; salience as listed") {
    const auto cat = eng::catalogue_of(shipped());
    for (const auto& [key, sal] : std::vector<std::pair<std::string, std::int64_t>>{
             {"monster_appeared", 45}, {"undead_rose", 50}, {"monster_woke", 25}, {"monster_slept", 10},
             {"monster_raid", 15}, {"settlement_overrun", 50}, {"monster_slain", 55}, {"monster_driven_off", 30},
             {"monster_died", 30}, {"beasts_bold", 15}, {"outpost_built", 12}, {"outpost_abandoned", 10}}) {
        CAPTURE(key);
        const eng::KindDef* k = cat.find(key);
        REQUIRE(k != nullptr);
        CHECK(k->salience == sal);
        CHECK_FALSE(eng::template_problem(*k, k->templ).has_value());
    }
}

TEST_CASE("P5-02 10.3: template_problem of the form kind and {@beast}, where the race-role form check is") {
    const auto& c = eng::compiled();
    const eng::KindDef* raid = c.find("monster_raid");
    const eng::KindDef* battle = c.find("battle");
    const eng::KindDef* bold = c.find("beasts_bold");
    const eng::KindDef* slain = c.find("monster_slain");
    REQUIRE(raid != nullptr);
    REQUIRE(battle != nullptr);
    REQUIRE(bold != nullptr);
    REQUIRE(slain != nullptr);
    CHECK_FALSE(eng::template_problem(*raid, "{monster:kind} raided {@settlement}.").has_value());
    CHECK(eng::template_problem(*raid, "{civilization:kind} raided.") ==
          std::optional<std::string>("form 'kind' needs a monster role at byte 0"));
    CHECK(eng::template_problem(*battle, "Then {winner:kind} won.") ==
          std::optional<std::string>("form 'kind' needs a monster role at byte 5"));
    CHECK(eng::template_problem(*slain, "{monster:plural} fell.") ==
          std::optional<std::string>("form 'plural' needs a race role at byte 0"));
    CHECK_FALSE(eng::template_problem(*bold, "The {@beast} of {@region}.").has_value());
    CHECK(eng::template_problem(*raid, "The {@beast} came.") ==
          std::optional<std::string>("{@beast} needs a kind with a value 'beast' at byte 4"));
    CHECK(eng::template_problem(*raid, "{@beasts}.") ==
          std::optional<std::string>("unknown placeholder '{@beasts}' at byte 0"));
    // D2 (the test writer's reading): kind 48 has no role settlement; 10.2's text {settlement} is refused
    CHECK(eng::template_problem(*raid, "{monster} fell upon {settlement}; {dead?no one died|one died|# died}.") ==
          std::optional<std::string>("unknown placeholder '{settlement}' at byte 20"));
}

TEST_CASE("P5-02 10.2, 10.3: the sentence of every new kind (names of types 8 and 9, kind, @beast, names inside)") {
    const Scene& s = scene();
    const db::WorldReader r = open_world(s.path);
    const auto err = eng::view_error(r);
    REQUIRE_MESSAGE(!err.has_value(), "HistoryView::load refused the scene: " << (err ? err->message : ""));
    auto at = [&](const char* label) { return eng::render(r, s.ev.at(label)); };
    CHECK(at("44") == "Old Grakh, a troll of the hills, made its lair in King's Fold.");
    // P5-03a 7.2 (0.25.0): kinds 45 and 49 with {monster:its} (its for a kind of grouped 0, their for 1, read from
    // the stored monster_kind row) and {monster:where} (the host is not named for King's Fold: " in King's Fold")
    std::map<std::int64_t, std::string> its;
    for (const auto& k : rows_of(r, "monster_kind")) its[k.at(0).integer] = k.at(4).integer != 0 ? "their" : "its";
    CHECK(at("45") == "After the slaughter, the Host of Edric rose from " + its.at(5) + " graves in King's Fold.");
    CHECK(at("46") == "The Host of Edric woke again in King's Fold.");
    CHECK(at("47") == "Old Grakh sank into sleep in Grakh's Hollow.");
    CHECK(at("47b") == "Saelith the Black sank into sleep in the Lair of Saelith.");
    // P5-03 9.5 (0.24.0): the raid phrase ((event id - 1) mod 3) of the troll's raid_phrases; 48a, b, c are three
    // consecutive events
    // consecutive events (their ids follow the base world's events, so the phrase is chosen by id here)
    // (the troll's raid_phrases in game/data/monsters/troll.toml order)
    const std::vector<std::string> raid = {"came down by night on", "fell upon", "prowled the edges of"};
    const auto phrase = [&](const char* label) {
        return raid[static_cast<std::size_t>((s.ev.at(label) - 1) % 3)];
    };
    CHECK(at("48a") == "Old Grakh " + phrase("48a") + " Drutnot Hole; no one died.");
    CHECK(at("48b") == "Old Grakh " + phrase("48b") + " Drutnot Hole; one died.");
    CHECK(at("48c") == "Old Grakh " + phrase("48c") + " Drutnot Hole; 41 died.");
    CHECK(at("49") == "The Host of Edric drove the people from Nutskrank and made it " + its.at(5) + " lair; 41 escaped.");
    CHECK(at("50") == "The Stinrild League hunted down and slew Saelith the Black, a dragon of the old world, in King's "
                      "Fold.");
    CHECK(at("51") == "The Snat Horde drove Old Grakh back to its lair in King's Fold.");
    CHECK(at("52") == "Uguurz the Glutton, an ogre, died of great age in King's Fold.");
    // P5-03 13 (0.24.0): kind 53's template with the wolves' bold phrase ((event id - 1) mod 2)
    const std::string bold = (s.ev.at("53") - 1) % 2 == 0 ? "hunted the roads from" : "grew bold and stalked the herds of";
    CHECK(at("53") == "In King's Fold the wolves " + bold + " Drutnot Hole; three died.");
    CHECK(at("54") == "The Snat Horde raised Snat Watch in King's Fold against the Stinrild League.");
    CHECK(at("55a") == "The Snat Horde abandoned Snat Watch, the frontier having fallen quiet.");
    CHECK(at("55b") == "The Snat Horde abandoned Snat Watch, the land being lost.");
    CHECK(at("55c") == "The Snat Horde abandoned Snat Watch, its realm having fallen.");
}

TEST_CASE("P5-02 11.4: the scene loads; each damaged site row is refused with its message, in 11.4's order") {
    const Scene& s = scene();
    REQUIRE_FALSE(eng::view_error(open_world(s.path)).has_value());
    const std::string s1 = n(s.s0 + 1), s3 = n(s.s0 + 3), s4 = n(s.s0 + 4), s6 = n(s.s0 + 6);
    const std::string Y = n(s.year), E = n(s.year + 1);
    const std::string head = "corrupt table 'site': row ";
    refused("site-id", "UPDATE site SET id = " + n(s.s0 + 16) + " WHERE id = " + s6 + ";",
            head + n(s.s0 + 16) + ": id " + n(s.s0 + 16) + " is not " + s6);
    refused("site-kind", "UPDATE site SET kind = 5 WHERE id = " + s1 + ";", head + s1 + ": kind 5 is not 1, 2, 3 or 4");   // P5-04 9.2: 3 a mine; P5-06: 4 a tower
    refused("site-region", "UPDATE site SET region = 999999 WHERE id = " + s1 + ";",
            head + s1 + ": unknown region 999999");
    refused("site-cell", "UPDATE site SET x = 512, y = 0 WHERE id = " + s1 + ";",
            head + s1 + ": cell 512 0 outside the map");
    refused("site-settlement", "UPDATE site SET settlement = 999999 WHERE id = " + s3 + ";",
            head + s3 + ": unknown settlement 999999");
    refused("site-not-at-cell", "UPDATE site SET settlement = 1 WHERE id = " + s3 + ";",
            head + s3 + ": settlement 1 is not at the site's cell");
    refused("site-monster", "UPDATE site SET monster = 999999 WHERE id = " + s1 + ";",
            head + s1 + ": monster 999999 is not a monster row");
    refused("site-civ", "UPDATE site SET founder_civ = 9999 WHERE id = " + s4 + ";",
            head + s4 + ": unknown civilization 9999");
    refused("site-origin-year", "UPDATE site SET origin_year = " + E + " WHERE id = " + s1 + ";",
            head + s1 + ": origin_year " + E + " outside 0 to " + Y);
    refused("site-origin-event", "UPDATE site SET origin_event = " + n(s.ev.at("battle")) + " WHERE id = " + s1 + ";",
            head + s1 + ": origin_event " + n(s.ev.at("battle")) + " is not an event of its year naming this site");
    refused("site-end-columns", "UPDATE site SET ended_year = " + Y + " WHERE id = " + s1 + ";",
            head + s1 + ": end columns disagree");
    refused("site-ended-before", "UPDATE site SET ended_year = " + n(s.year - 1) + ", ending_event = " +
                                     n(s.ev.at("50")) + ", ending = 1 WHERE id = " + s1 + ";",
            head + s1 + ": ended_year " + n(s.year - 1) + " before origin_year");
    refused("site-ending-event", "UPDATE site SET ended_year = " + Y + ", ending_event = 1, ending = 1 WHERE id = " +
                                     s1 + ";",
            head + s1 + ": ending_event 1 is not an event of year " + Y);
    refused("site-ending", "UPDATE site SET ended_year = " + Y + ", ending_event = " + n(s.ev.at("50")) +
                               ", ending = 7 WHERE id = " + s1 + ";",
            head + s1 + ": ending 7 outside 0 to 6");
    // the two reasons 11.4 names without a text (A3): only the table and row are checked
    for (const auto& [label, sql, row] : std::vector<std::tuple<std::string, std::string, std::string>>{
             {"site-lair-founder", "UPDATE site SET founder_civ = 1 WHERE id = " + s1 + ";", s1},
             {"site-outpost-monster", "UPDATE site SET monster = " + n(s.m0 + 1) + " WHERE id = " + s4 + ";", s4}}) {
        CAPTURE(label);
        const auto e = eng::view_error(open_world(corrupt(label, sql)));
        REQUIRE(e.has_value());
        CHECK(e->code == db::ErrorCode::corrupt_layer);
        CHECK(e->message.rfind(head + row + ": ", 0) == 0U);
    }
}

TEST_CASE("P5-02 11.4: each damaged monster row is refused with its message") {
    const Scene& s = scene();
    const std::string m1 = n(s.m0 + 1), m3 = n(s.m0 + 3);
    const std::string Y = n(s.year);
    const std::string head = "corrupt table 'monster': row ";
    refused("monster-kind", "UPDATE monster SET kind = 7 WHERE id = " + m1 + ";", head + m1 + ": unknown kind 7");
    refused("monster-name", "UPDATE monster SET name = '' WHERE id = " + m1 + ";", head + m1 + ": name is not a name");
    refused("monster-lair", "UPDATE monster SET lair_site = " + n(s.s0 + 4) + " WHERE id = " + m1 + ";",
            head + m1 + ": lair_site " + n(s.s0 + 4) + " is not a lair of this monster");
    refused("monster-born", "UPDATE monster SET born_event = " + n(s.ev.at("battle")) + " WHERE id = " + m1 + ";",
            head + m1 + ": born_event " + n(s.ev.at("battle")) + " is not the appearance of this monster in year " + Y);
    refused("monster-status", "UPDATE monster SET status = 5 WHERE id = " + m1 + ";",
            head + m1 + ": status 5 outside 1 to 4");
    refused("monster-end-columns", "UPDATE monster SET died = " + Y + " WHERE id = " + m1 + ";",
            head + m1 + ": end columns disagree");
    for (const auto& [label, sql] : std::vector<std::pair<std::string, std::string>>{
             {"monster-slayer", "UPDATE monster SET slain_by = 1 WHERE id = " + m3 + ";"},
             {"monster-slain-event", "UPDATE monster SET death_event = " + n(s.ev.at("52")) + " WHERE id = " + m3 + ";"},
             {"monster-hoard", "UPDATE monster SET hoard = -1 WHERE id = " + m1 + ";"}}) {
        CAPTURE(label);
        const auto e = eng::view_error(open_world(corrupt(label, sql)));
        REQUIRE(e.has_value());
        CHECK(e->code == db::ErrorCode::corrupt_layer);
        CHECK(e->message.rfind(head, 0) == 0U);
    }
}

TEST_CASE("P5-02 11.4: beast, monster_kind, beast_population, settlement.ending and the event checks") {
    const Scene& s = scene();
    const std::string R = n(s.region), Y = n(s.year), E = n(s.year + 1);
    refused("beast-id", "UPDATE beast SET id = 9 WHERE id = 5;", "corrupt table 'beast': row 9: id 9 is not 5");
    refused("beast-key", "UPDATE beast SET key = 'Wolf' WHERE id = 5;",
            "corrupt table 'beast': row 5: key is not a definition id");
    refused("beast-name", "UPDATE beast SET name = '' WHERE id = 5;", "corrupt table 'beast': row 5: name is not a name");
    refused("beast-plural", "UPDATE beast SET plural = 'Wolves' WHERE id = 5;",
            "corrupt table 'beast': row 5: plural is not a plural");
    // R3 (reading): the range reasons as "<column> <v> outside <lo> to <hi>"
    refused("beast-menace", "UPDATE beast SET menace = 1001 WHERE id = 5;",
            "corrupt table 'beast': row 5: menace 1001 outside 0 to 1000");
    refused("beast-food", "UPDATE beast SET food = -1 WHERE id = 5;",
            "corrupt table 'beast': row 5: food -1 outside 0 to 1000");
    refused("kind-noun", "UPDATE monster_kind SET noun = 'troll' WHERE id = 4;",
            "corrupt table 'monster_kind': row 4: noun is not a noun");
    refused("kind-might", "UPDATE monster_kind SET might = 0 WHERE id = 4;",
            "corrupt table 'monster_kind': row 4: might 0 outside 1 to 10000000");
    for (const auto& [label, sql] : std::vector<std::pair<std::string, std::string>>{
             {"kind-grouped", "UPDATE monster_kind SET grouped = 2 WHERE id = 4;"},
             {"kind-wake", "UPDATE monster_kind SET wake = 3 WHERE id = 4;"}}) {
        CAPTURE(label);
        const auto e = eng::view_error(open_world(corrupt(label, sql)));
        REQUIRE(e.has_value());
        CHECK(e->message.rfind("corrupt table 'monster_kind': row 4: ", 0) == 0U);
    }
    const std::string bp = "corrupt table 'beast_population': row (";
    refused("bp-region", "INSERT INTO beast_population VALUES (999999, 1, 3, 5);",
            bp + "999999, 1, 3): unknown region 999999");
    refused("bp-beast", "INSERT INTO beast_population VALUES (" + R + ", 9, 3, 5);",
            bp + R + ", 9, 3): unknown beast 9");
    refused("bp-year", "INSERT INTO beast_population VALUES (" + R + ", 1, " + E + ", 5);",
            bp + R + ", 1, " + E + "): year " + E + " outside 0 to " + Y);
    refused("bp-count", "INSERT INTO beast_population VALUES (" + R + ", 1, 3, 1000001);",
            bp + R + ", 1, 3): count 1000001 outside 0 to 1000000");
    refused("bp-repeat", "INSERT INTO beast_population VALUES (" + R + ", 1, 3, 777777);"
                         "INSERT INTO beast_population VALUES (" + R + ", 1, 4, 777777);",
            bp + R + ", 1, 4): count equals the previous row of this region and beast");
    // settlement 4 of geo-seed-42-512 is faded after its sacking (ending 1, derived through its ruin_faded's cause,
    // r1 25 item 3); at 0.24.0 settlement 1 (the witness at 0.23.0) stands to the end
    // 0.27.0: s4 of the 512 world (recalibrated monsters, P5-06) stands to the end (expected ending 0), so any
    // other code is the corruption
    refused("ending", "UPDATE settlement SET ending = 1 WHERE id = 4;",
            "corrupt table 'settlement': row 4: ending 1 is not its ruin's ending 4");
    // 0.28.0 (P5-04a integration): s4 of the 512 world is ruined now (a ruin whose cause gives ending 4), so the
    // expected ending in the reader's message is 4; the message describes the corruption (1 against 4) correctly.
    // events: a participant of type 8 with no row; a kind 44 without its site; a value beast with no row (R4)
    {
        const auto e = eng::view_error(open_world(corrupt(
            "event-monster", "UPDATE event_participant SET entity_id = 999999 WHERE event = " + n(s.ev.at("48a")) +
                                 " AND seq = 1;")));
        REQUIRE(e.has_value());
        CHECK(e->message.rfind("corrupt table 'event': row " + n(s.ev.at("48a")) + ": ", 0) == 0U);
        CHECK(e->message.find("monster 999999 has no row") != std::string::npos);
    }
    {
        const std::int64_t extra = s.first_event + static_cast<std::int64_t>(s.ev.size());
        const auto k44 = [&] {
            for (const auto& k : rows_of(open_world(s.path), "event_kind")) {
                if (k.at(1).bytes == "monster_appeared") return k.at(0).integer;
            }
            return std::int64_t{0};
        }();
        const std::string ph = n(static_cast<std::int64_t>(rows_of(open_world(s.path), "phase").size()));
        refused("event-44-no-site",
                "INSERT INTO event VALUES (" + n(extra) + ", " + Y + ", " + Y + ", " + n(k44) + ", " + ph + ", " + R +
                    ", 0, 45);INSERT INTO event_participant VALUES (" + n(extra) + ", 1, 1, 8, " + n(s.m0 + 1) +
                    ");INSERT INTO event_participant VALUES (" + n(extra) + ", 2, 2, 9, " + n(s.s0 + 1) + ");",
                "corrupt table 'event': row " + n(extra) + ": monster_appeared without its site row");
    }
    {
        const auto e = eng::view_error(open_world(corrupt(
            "event-beast", "UPDATE event_value SET value = 9 WHERE event = " + n(s.ev.at("53")) + " AND seq = 1;")));
        REQUIRE(e.has_value());
        CHECK(e->code == db::ErrorCode::corrupt_layer);
        CHECK(e->message.find("beast") != std::string::npos);
    }
}

TEST_CASE("P5-02 21.3 A2: an event of kind 43 without its region_name row (the linear check keeps P3-07 26.6's text)") {
    const Scene& s = scene();
    const db::WorldReader r = open_world(s.path);
    const auto rows = rows_of(r, "region_name");
    if (rows.empty()) {
        MESSAGE("geo-seed-42-512 has no renaming at 0.23.0; A2 is covered by history_renaming_readers_test.cpp");
        return;
    }
    const std::string e = n(rows.front().at(4).integer);
    refused("a2", "DELETE FROM region_name WHERE event = " + e + ";",
            "corrupt table 'event': row " + e + ": region_renamed without a region_name row");
}

}  // TEST_SUITE
