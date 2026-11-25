// Spec P5-03a (docs/specs/p5-03a-relations-followups.md r1) test 14 item 10 (and the catalogue of 10.4): the forms
// {m:its}, {m:aside} and {m:where} of a monster role (7.1) with their template_problem; the six monster templates
// of 7.2 on events added by SQL to a copy of the generated 512 world of seed 42 (as the P5-02 and P5-03 render
// tests do): a wild kind and a grouped one, a host of the dead named for its region (where and aside empty), one
// named for a settlement, one named for its region's geographic name after a renaming and one named for the name in
// force; and the new values of kinds 57, 60 and 62 in the compiled catalogue. The kind of each monster (grouped,
// wake, noun) is read from the stored monster_kind rows, so the expectations follow the rule, not the data.
//
// Written from the spec before the implementation. Library names: eng::compiled, catalogue_of, template_problem,
// view_error, render (all older than P5-03a).

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "p5_04_alliances_data.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_03a_relations_data.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/history/event_catalogue.hpp"

namespace {

namespace db = worldgen::db;

std::string n(std::int64_t v) { return std::to_string(v); }
std::string q(const std::string& s) {
    std::string out = "'";
    for (const char c : s) {
        out += c;
        if (c == '\'') out += '\'';
    }
    return out + "'";
}

struct Kind {
    bool grouped = false;
    bool dead = false;
    std::string noun;
};

struct Scene {
    std::string path;
    std::map<std::string, std::int64_t> ev;
    std::map<std::string, Kind> kind;   // by key
};

// Region R (settlement 1's) is named The Red Heath; region R2 (another settlement's) has the geographic name Grey
// Fells and is renamed The Ash Downs in year Y by a region_renamed event before the monster events (so its name at
// each event and in force at the event's year agree). Civilizations 1 and 2 are the Snat Horde and the Stinrild
// League, settlement 1 Drutnot Hole.
const Scene& scene() {
    static const Scene sc = [] {
        static dbtest::TempDir dir("relations-monster-forms");
        const std::string src = p503run::world_path(42, 512);
        const db::WorldReader base = chrono_test::open_world(src);
        Scene s;
        std::int64_t eid = static_cast<std::int64_t>(chrono_test::rows_of(base, "event").size());
        const std::int64_t phase = static_cast<std::int64_t>(chrono_test::rows_of(base, "phase").size());
        const auto st = chrono_test::rows_of(base, "settlement");
        const std::int64_t R = st.at(0).at(4).integer;
        std::int64_t R2 = 0, x2 = 0, y2 = 0;
        // R2 is a region the world never renamed (the scene's own renaming is then its only region_name row; at
        // 0.25.0 the first other region of seed 42 has a renaming whose event would lose its row)
        std::set<std::int64_t> renamed;
        for (const auto& row : chrono_test::rows_of(base, "region_name")) renamed.insert(row.at(0).integer);
        for (const auto& row : st) {
            if (row.at(4).integer != R && R2 == 0 && renamed.count(row.at(4).integer) == 0U) {
                R2 = row.at(4).integer;
                x2 = row.at(2).integer;
                y2 = row.at(3).integer;
            }
        }
        REQUIRE_MESSAGE(R2 != 0, "the world has settlements in one region only");
        const std::int64_t x1 = st.at(0).at(2).integer, y1 = st.at(0).at(3).integer;
        const std::int64_t Y = std::stoll(base.meta("history_end_year").value_or("6000")) - 1;
        const std::int64_t m0 = static_cast<std::int64_t>(chrono_test::rows_of(base, "monster").size());
        const std::int64_t s0 = static_cast<std::int64_t>(chrono_test::rows_of(base, "site").size());
        std::map<std::string, std::int64_t> kid, mkid;
        for (const auto& k : chrono_test::rows_of(base, "event_kind")) kid[k.at(1).bytes] = k.at(0).integer;
        for (const auto& k : chrono_test::rows_of(base, "monster_kind")) {
            mkid[k.at(1).bytes] = k.at(0).integer;
            s.kind[k.at(1).bytes] = Kind{k.at(4).integer != 0, k.at(5).integer == 2, k.at(3).bytes};
        }
        REQUIRE(mkid.count("undead") == 1U);
        REQUIRE(mkid.count("troll") == 1U);
        REQUIRE(mkid.count("wyvern") == 1U);
        std::string sql =
            "UPDATE civilization SET name = 'The Snat Horde' WHERE id = 1;"
            "UPDATE civilization SET name = 'The Stinrild League' WHERE id = 2;"
            "UPDATE settlement SET name = 'Drutnot Hole' WHERE id = 1;"
            "UPDATE region SET name = 'The Red Heath' WHERE id = " + n(R) + ";"
            "UPDATE region_name SET name = 'The Red Heath' WHERE region = " + n(R) + ";"
            "UPDATE region SET name = 'Grey Fells' WHERE id = " + n(R2) + ";"
            "DELETE FROM region_name WHERE region = " + n(R2) + ";";
        auto add = [&](const std::string& label, const std::string& key, std::int64_t region, std::int64_t settlement,
                       const std::vector<std::tuple<std::int64_t, std::int64_t, std::int64_t>>& parts,
                       const std::vector<std::int64_t>& vals, const std::vector<std::int64_t>& causes) {
            REQUIRE_MESSAGE(kid.count(key) == 1U, "kind " << key << " not in the world's catalogue");
            const std::string e = n(++eid);
            s.ev[label] = eid;
            sql += "INSERT INTO event VALUES (" + e + ", " + n(Y) + ", " + n(Y) + ", " + n(kid.at(key)) + ", " +
                   n(phase) + ", " + n(region) + ", " + n(settlement) + ", 30);";
            for (std::size_t i = 0; i < parts.size(); ++i) {
                sql += "INSERT INTO event_participant VALUES (" + e + ", " + n(static_cast<std::int64_t>(i + 1)) +
                       ", " + n(std::get<0>(parts[i])) + ", " + n(std::get<1>(parts[i])) + ", " +
                       n(std::get<2>(parts[i])) + ");";
            }
            for (std::size_t i = 0; i < vals.size(); ++i) {
                sql += "INSERT INTO event_value VALUES (" + e + ", " + n(static_cast<std::int64_t>(i + 1)) + ", " +
                       n(vals[i]) + ");";
            }
            for (const std::int64_t c : causes) sql += "INSERT INTO event_cause VALUES (" + e + ", " + n(c) + ");";
        };
        add("sack", "sacked", R, 2, {{1, 3, 1}, {2, 3, 2}}, {10, 20}, {});
        add("rename", "region_renamed", R2, 0, {}, {}, {s.ev["sack"]});
        sql += "INSERT INTO region_name VALUES (" + n(R2) + ", " + n(Y) + ", 'The Ash Downs', 1, " + n(s.ev["rename"]) +
               ");";
        // monsters m1 to m6: born event (monster_appeared or undead_rose), lair site, the test event, the row
        struct M {
            std::string label, name, kind;
            std::int64_t region;
        };
        const std::vector<M> ms = {{"dead-region", "The Dead of the Red Heath", "undead", R},
                                   {"dead-settlement", "The Host of Drutnot Hole", "undead", R},
                                   {"dead-old-name", "The Dead of Grey Fells", "undead", R2},
                                   {"dead-new-name", "The Restless of the Ash Downs", "undead", R2},
                                   {"wyvern", "The Screamers of Nux", "wyvern", R},
                                   {"troll", "Old Grakh", "troll", R}};
        for (std::size_t i = 0; i < ms.size(); ++i) {
            const M& m = ms[i];
            const std::int64_t mid = m0 + 1 + static_cast<std::int64_t>(i), sid = s0 + 1 + static_cast<std::int64_t>(i);
            const bool dead = s.kind.at(m.kind).dead;
            add("born-" + m.label, dead ? "undead_rose" : "monster_appeared", m.region, 0, {{1, 8, mid}, {2, 9, sid}},
                {}, dead ? std::vector<std::int64_t>{s.ev["sack"]} : std::vector<std::int64_t>{});
            const std::int64_t cx = m.region == R ? x1 : x2, cy = m.region == R ? y1 : y2;
            sql += "INSERT INTO site VALUES (" + n(sid) + ", " + q(m.name + "'s Lair") + ", 1, " + n(m.region) + ", " +
                   n(cx) + ", " + n(cy) + ", 0, 0, " + n(mid) + ", " + n(Y) + ", " + n(s.ev["born-" + m.label]) +
                   ", NULL, 0, 0);";
        }
        // the test events, after every birth
        const std::int64_t m1 = m0 + 1, m2 = m0 + 2, m3 = m0 + 3, m4 = m0 + 4, m5 = m0 + 5, m6 = m0 + 6;
        add("raid", "monster_raid", R, 1, {{1, 8, m6}, {2, 3, 1}}, {41, 0}, {s.ev["born-troll"]});
        add("slain-dead", "monster_slain", R, 0, {{1, 8, m1}, {2, 3, 1}}, {30}, {s.ev["raid"]});
        add("died-settlement", "monster_died", R, 0, {{1, 8, m2}}, {}, {s.ev["born-dead-settlement"]});
        add("driven-old", "monster_driven_off", R2, 0, {{1, 8, m3}, {2, 3, 1}}, {}, {s.ev["raid"]});
        add("died-new", "monster_died", R2, 0, {{1, 8, m4}}, {}, {s.ev["born-dead-new-name"]});
        add("slain-wyvern", "monster_slain", R, 0, {{1, 8, m5}, {2, 3, 2}}, {12}, {s.ev["raid"]});
        // the overrun's lair is a site of its own, opened by the overrun (the reader's origin rule; P5-02 8.3)
        add("overrun", "settlement_overrun", R, 1, {{1, 8, m6}, {2, 3, 1}, {3, 9, s0 + 7}}, {41}, {s.ev["raid"]});
        sql += "INSERT INTO site VALUES (" + n(s0 + 7) + ", 'Grakh''s Hold', 1, " + n(R) + ", " + n(x1) + ", " +
               n(y1) + ", 1, 0, " + n(m6) + ", " + n(Y) + ", " + n(s.ev["overrun"]) + ", NULL, 0, 0);";
        // monster rows: id, name, kind, lair_site, born, born_event, died, death_event, slain_by, status, hoard
        auto mrow = [&](std::int64_t mid, const M& m, const std::string& death, std::int64_t slayer, std::int64_t status) {
            const std::int64_t sid = s0 + (mid - m0);
            sql += "INSERT INTO monster VALUES (" + n(mid) + ", " + q(m.name) + ", " + n(mkid.at(m.kind)) + ", " +
                   n(sid) + ", " + n(Y) + ", " + n(s.ev["born-" + m.label]) + ", " +
                   (death.empty() ? std::string("NULL, 0") : n(Y) + ", " + n(s.ev[death])) + ", " + n(slayer) + ", " +
                   n(status) + ", 0);";   // (the lairs stay open, as in P5-02's render scene)
        };
        mrow(m1, ms[0], "slain-dead", 1, 3);
        mrow(m2, ms[1], "died-settlement", 0, 4);
        mrow(m3, ms[2], "", 0, 1);
        mrow(m4, ms[3], "died-new", 0, 4);
        mrow(m5, ms[4], "slain-wyvern", 2, 3);
        mrow(m6, ms[5], "", 0, 1);
        s.path = dir.file("scene.db");
        dbtest::write_file(s.path, dbtest::read_file(src));
        REQUIRE_MESSAGE(worldgen::db::testing::execute_on_file(s.path, sql).ok(), "SQL rejected");
        return s;
    }();
    return sc;
}

std::string values_of(const eng::KindDef& k) {
    std::string out;
    for (const std::string& v : k.values) out += (out.empty() ? "" : ",") + v;
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-03a 10.4: kinds 57, 60 and 62 gain sealed; standing, majority; standing, years (seq after the old)") {
    const auto& c = eng::compiled();
    REQUIRE(c.kinds.size() == 78U)  /* P5-04 8.1 (0.26.0): kinds 63 to 78 */;
    CHECK(values_of(c.kinds[56]) == "disposition,band,sealed");
    CHECK(values_of(c.kinds[59]) == "share,standing,majority");
    CHECK(values_of(c.kinds[61]) == "people,standing,years");
    CHECK(values_of(c.kinds[57]) == "disposition,band");   // 58 unchanged
    CHECK(values_of(c.kinds[58]) == "people,standing");    // 59 unchanged
}

TEST_CASE("P5-03a 7.1: template_problem of the forms its, aside and where") {
    const auto& c = eng::compiled();
    const eng::KindDef* slain = c.find("monster_slain");
    const eng::KindDef* battle = c.find("battle");
    REQUIRE(slain != nullptr);
    REQUIRE(battle != nullptr);
    for (const char* f : {"its", "aside", "where"}) {
        CAPTURE(std::string(f));
        CHECK_FALSE(eng::template_problem(*slain, std::string("{monster:") + f + "}.").has_value());
        CHECK(eng::template_problem(*slain, std::string("{slayer:") + f + "}.") ==
              std::optional<std::string>(std::string("form '") + f + "' needs a monster role at byte 0"));
        CHECK(eng::template_problem(*battle, std::string("Then {winner:") + f + "} won.") ==
              std::optional<std::string>(std::string("form '") + f + "' needs a monster role at byte 5"));
    }
    CHECK(eng::template_problem(*slain, "{monster:there}.") ==
          std::optional<std::string>("unknown placeholder '{monster:there}' at byte 0"));
}

TEST_CASE("P5-03a 7.2, 10.3: the shipped monster templates and those of 57, 58, 59, 60, 62 are the spec's") {
    const auto cat = eng::catalogue_of(conflict_test::shipped());
    for (const auto& [key, templ] : p503a::templates()) {
        CAPTURE(key);
        const eng::KindDef* k = cat.find(key);
        REQUIRE(k != nullptr);
        CHECK(k->templ == p504data::with_allied_cause(templ));   // P5-04 11.1: allied_battle after battle_at_pass
        CHECK_FALSE(eng::template_problem(*k, k->templ).has_value());
    }
}

TEST_CASE("P5-03a 7.1, 7.2: its, aside and where in the six monster sentences") {
    const Scene& s = scene();
    const db::WorldReader r = chrono_test::open_world(s.path);
    const auto err = eng::view_error(r);
    REQUIRE_MESSAGE(!err.has_value(), "HistoryView::load refused the scene: " << (err ? err->message : ""));
    auto at = [&](const std::string& label) { return eng::render(r, s.ev.at(label)); };
    auto its = [&](const std::string& k) { return std::string(s.kind.at(k).grouped ? "their" : "its"); };
    const std::string wyv = s.kind.at("wyvern").noun, troll = s.kind.at("troll").noun;
    REQUIRE_FALSE(s.kind.at("wyvern").dead);
    REQUIRE(s.kind.at("undead").dead);
    // 44 with its/their; a grouped kind
    CHECK(at("born-wyvern") == "The Screamers of Nux, " + wyv + ", made " + its("wyvern") + " lair in the Red Heath.");
    CHECK(at("born-troll") == "Old Grakh, " + troll + ", made " + its("troll") + " lair in the Red Heath.");
    // 45: a host named for its region: where empty (F8.3: one name for the place)
    CHECK(at("born-dead-region") == "After the sack, the Dead of the Red Heath rose from " + its("undead") + " graves.");
    // named for a settlement: where " in " + the region
    CHECK(at("born-dead-settlement") ==
          "After the sack, the Host of Drutnot Hole rose from " + its("undead") + " graves in the Red Heath.");
    // 50: dead, named for its region: no aside, no where; wild: ", <noun>," and where
    CHECK(at("slain-dead") == "The Snat Horde hunted down and slew the Dead of the Red Heath.");
    CHECK(at("slain-wyvern") == "The Stinrild League hunted down and slew the Screamers of Nux, " + wyv +
                                    ", in the Red Heath.");
    // 52: named for a settlement: where; named for the name in force at the event's year after a renaming: none
    CHECK(at("died-settlement") == "The Host of Drutnot Hole died of great age in the Red Heath.");
    CHECK(at("died-new") == "The Restless of the Ash Downs died of great age.");
    // 51: named for the geographic name of a renamed region: where, with the name in force
    CHECK(at("driven-old") ==
          "The Snat Horde drove the Dead of Grey Fells back to " + its("undead") + " lair in the Ash Downs.");
    // 49 with its/their
    CHECK(at("overrun") == "Old Grakh drove the people from Drutnot Hole and made it " + its("troll") +
                               " lair; 41 escaped.");
}

TEST_CASE("P5-03a 7.1: {m:where} links the region exactly as {@region}; its and aside link nothing") {
    const Scene& s = scene();
    const db::WorldReader r = chrono_test::open_world(s.path);
    const auto view = worldgen::history::HistoryView::load(r);
    REQUIRE(view.ok());
    const worldgen::history::LinkedText t = worldgen::history::render_event_linked(view.value(), s.ev.at("driven-old"));
    std::int64_t region_links = 0;
    for (const worldgen::history::TextLink& l : t.links) {
        if (l.type == 5) {
            ++region_links;
            CHECK(t.text.substr(l.start, l.length) == "the Ash Downs");
        }
    }
    CHECK(region_links == 1);
}

}  // TEST_SUITE
