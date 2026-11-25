// Spec P5-03 (docs/specs/p5-03-race-relations.md r1) test 24.1 item 9 and the catalogue of 8.1 and 11.3: kinds 56
// to 62 appended after 55 with their uses, roles and values; the shipped entries pass template_problem with the
// saliences of 8.2 and 11.3; the form `raid`, `{@beast:bold}` and the four-word choice, with their template_problem
// messages; and the sentence of every new template (and of kinds 48 and 53) on events added by SQL to a copy of the
// generated 512 world of seed 42 (as P5-02's render test does). The phrase of a raid and of bold beasts is the one of
// number ((event id - 1) mod n) of the stored monster_kind.raid_phrases or beast.bold_phrases (9.5, 13), read from
// the world, so the expectation follows the rule, not the data.
//
// Written from the spec before the implementation. Library names: eng::compiled, catalogue_of, template_problem,
// view_error, render (all older than P5-03).

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "p5_03_relations_formulas.hpp"
#include "p5_03_relations_runs.hpp"
#include "worldgen/db/testing.hpp"

namespace {

namespace wd = worldgen::data;
namespace db = worldgen::db;

std::string n(std::int64_t v) { return std::to_string(v); }

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
std::vector<std::string> split_bar(const std::string& s) { return engine_test::split(s, '|'); }

struct Scene {
    std::string path;
    std::int64_t region = 0, year = 0, m1 = 0, met = 0;
    std::map<std::string, std::int64_t> ev;
    std::map<std::int64_t, std::string> plural;   // race id -> plural
    std::string raid_phrases, bold_phrases;       // troll's and wolf's, as stored
};

const Scene& scene() {
    static const Scene sc = [] {
        static dbtest::TempDir dir("relations-render");
        const std::string src = p503run::world_path(42, 512);
        const db::WorldReader base = chrono_test::open_world(src);
        Scene s;
        std::int64_t eid = static_cast<std::int64_t>(chrono_test::rows_of(base, "event").size());
        const std::int64_t phase = static_cast<std::int64_t>(chrono_test::rows_of(base, "phase").size());
        const auto st = chrono_test::rows_of(base, "settlement");
        s.region = st.at(0).at(4).integer;
        const std::int64_t cx = st.at(0).at(2).integer, cy = st.at(0).at(3).integer;
        s.year = std::stoll(base.meta("history_end_year").value_or("6000")) - 1;
        const std::int64_t m0 = static_cast<std::int64_t>(chrono_test::rows_of(base, "monster").size());
        const std::int64_t s0 = static_cast<std::int64_t>(chrono_test::rows_of(base, "site").size());
        s.m1 = m0 + 1;
        std::map<std::string, std::int64_t> kid;
        for (const auto& k : chrono_test::rows_of(base, "event_kind")) kid[k.at(1).bytes] = k.at(0).integer;
        for (const auto& r : chrono_test::rows_of(base, "race")) s.plural[r.at(0).integer] = r.at(3).bytes;
        for (const auto& k : chrono_test::rows_of(base, "monster_kind")) {
            if (k.at(1).bytes == "troll") s.raid_phrases = k.at(7).bytes;
        }
        for (const auto& b : chrono_test::rows_of(base, "beast")) {
            if (b.at(1).bytes == "wolf") s.bold_phrases = b.at(6).bytes;
        }
        for (const auto& e : chrono_test::rows_of(base, "event")) {
            if (e.at(3).integer == kid["peoples_met"] && s.met == 0) s.met = e.at(0).integer;
        }
        const std::string Y = n(s.year), R = n(s.region), P = n(phase);
        std::string sql =
            "UPDATE civilization SET name = 'The Snat Horde' WHERE id = 1;"
            "UPDATE civilization SET name = 'The Stinrild League' WHERE id = 2;"
            "UPDATE settlement SET name = 'Drutnot Hole' WHERE id = 1;"
            "UPDATE settlement SET name = 'Nutskrank' WHERE id = 2;"
            "UPDATE region SET name = 'King''s Fold' WHERE id = " + R + ";"
            "UPDATE region_name SET name = 'King''s Fold' WHERE region = " + R + ";";
        auto add = [&](const std::string& label, const std::string& key, std::int64_t region, std::int64_t settlement,
                       const std::vector<std::tuple<std::int64_t, std::int64_t, std::int64_t>>& parts,
                       const std::vector<std::int64_t>& vals, const std::vector<std::int64_t>& causes) {
            REQUIRE_MESSAGE(kid.count(key) == 1U, "kind " << key << " not in the world's catalogue");
            const std::string e = n(++eid);
            s.ev[label] = eid;
            sql += "INSERT INTO event VALUES (" + e + ", " + Y + ", " + Y + ", " + n(kid.at(key)) + ", " + P + ", " +
                   n(region) + ", " + n(settlement) + ", 30);";
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
        const std::int64_t r = s.region;
        add("sack", "sacked", r, 2, {{1, 3, 1}, {2, 3, 2}}, {10, 20}, {});
        add("capture", "settlement_captured", r, 2, {{1, 3, 1}, {2, 3, 2}}, {10, 20}, {});
        add("44", "monster_appeared", r, 0, {{1, 8, s.m1}, {2, 9, s0 + 1}}, {}, {});
        for (const char* l : {"48a", "48b", "48c", "48d"}) {
            add(l, "monster_raid", r, 1, {{1, 8, s.m1}, {2, 3, 1}}, {41, 0}, {s.ev["44"]});
        }
        add("53a", "beasts_bold", r, 1, {{1, 4, 1}, {2, 3, 1}}, {5, 41}, {});
        add("53b", "beasts_bold", r, 1, {{1, 4, 1}, {2, 3, 1}}, {5, 0}, {});
        add("56", "settlement_fled", r, 1, {{1, 8, s.m1}, {2, 3, 1}}, {}, {s.ev["48a"]});
        add("58a", "disposition_turned", 0, 0, {{1, 1, 1}, {2, 1, 2}}, {-600, 0}, {s.ev["sack"]});
        add("58b", "disposition_turned", 0, 0, {{1, 1, 1}, {2, 1, 2}}, {300, 2}, {});
        add("58c", "disposition_turned", 0, 0, {{1, 1, 1}, {2, 1, 2}}, {0, 1}, {s.ev["58a"]});
        for (std::int64_t k = 0; k <= 3; ++k) {
            add("59-" + n(k), "refugees_arrived", r, 1, {{1, 4, 1}, {2, 3, 1}, {3, 4, 2}, {4, 1, 1}}, {240, k},
                {s.ev["sack"]});
        }
        // P5-03a 10.4: kind 60's standing and majority, kind 62's standing and years
        add("60", "settlement_mixed", r, 1, {{1, 4, 1}, {2, 3, 1}, {3, 1, 1}, {4, 1, 2}}, {300, 0, 0},
            {s.ev["capture"]});
        add("60b", "settlement_mixed", r, 1, {{1, 4, 1}, {2, 3, 1}, {3, 1, 1}, {4, 1, 2}}, {700, 2, 1}, {});
        for (std::int64_t k = 0; k <= 2; ++k) {
            add("61-" + n(k), "standing_changed", r, 1, {{1, 4, 1}, {2, 3, 1}, {3, 1, 1}}, {k, 300}, {s.ev["capture"]});
        }
        add("62", "people_expelled", r, 1, {{1, 4, 1}, {2, 3, 1}, {3, 1, 1}}, {310, 1, 77}, {s.ev["capture"]});
        add("62b", "people_expelled", r, 1, {{1, 4, 1}, {2, 3, 1}, {3, 1, 1}}, {45, 0, 1}, {s.ev["capture"]});
        const std::string M1 = n(s.m1), S1 = n(s0 + 1);
        sql += "INSERT INTO monster VALUES (" + M1 + ", 'Old Grakh', 4, " + S1 + ", " + Y + ", " + n(s.ev["44"]) +
               ", NULL, 0, 0, 1, 0);";
        sql += "INSERT INTO site VALUES (" + S1 + ", 'Grakh''s Hollow', 1, " + R + ", " + n(cx) + ", " + n(cy) +
               ", 0, 0, " + M1 + ", " + Y + ", " + n(s.ev["44"]) + ", NULL, 0, 0);";
        s.path = dir.file("scene.db");
        dbtest::write_file(s.path, dbtest::read_file(src));
        REQUIRE_MESSAGE(worldgen::db::testing::execute_on_file(s.path, sql).ok(), "SQL rejected");
        return s;
    }();
    return sc;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-03 8.1, 11.3: kinds 56 to 62 appended after 55, their uses, roles and values") {
    const auto& c = eng::compiled();
    REQUIRE(c.kinds.size() == 78U)  /* P5-04 8.1 (0.26.0): kinds 63 to 78 */;
    struct K {
        std::int64_t id;
        const char* key;
        bool caused;
        std::int64_t region_use, settlement_use;
        const char* roles;
        const char* values;
    };
    for (const K& k : {K{56, "settlement_fled", true, 1, 1, "1:monster:8:1-1;2:civilization:3:1-1;", ""},
                       // P5-03a 10.4 (0.25.0): sealed after band
                       K{57, "peoples_met", true, 1, 0, "1:race_a:1:1-1;2:race_b:1:1-1;", "disposition,band,sealed"},
                       K{58, "disposition_turned", true, 0, 0, "1:race_a:1:1-1;2:race_b:1:1-1;", "disposition,band"},
                       K{59, "refugees_arrived", true, 1, 1,
                         "1:settlement:4:1-1;2:civilization:3:1-1;3:origin:4:1-1;4:race:1:1-1;", "people,standing"},
                       K{60, "settlement_mixed", true, 1, 1,
                         "1:settlement:4:1-1;2:civilization:3:1-1;3:race:1:1-1;4:ruling:1:1-1;",
                         "share,standing,majority"},   // P5-03a 10.4
                       K{61, "standing_changed", true, 1, 1, "1:settlement:4:1-1;2:civilization:3:1-1;3:race:1:1-1;",
                         "standing,share"},
                       K{62, "people_expelled", true, 1, 1, "1:settlement:4:1-1;2:civilization:3:1-1;3:race:1:1-1;",
                         "people,standing,years"}}) {   // P5-03a 10.4
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

TEST_CASE("P5-03 8.2, 11.3: the shipped entries of kinds 56 to 62 pass template_problem; salience as listed") {
    const auto cat = eng::catalogue_of(conflict_test::shipped());
    for (const auto& [key, sal] : std::vector<std::pair<std::string, std::int64_t>>{
             {"settlement_fled", 45}, {"peoples_met", 25}, {"disposition_turned", 55}, {"refugees_arrived", 25},
             {"settlement_mixed", 25}, {"standing_changed", 15}, {"people_expelled", 40}}) {
        CAPTURE(key);
        const eng::KindDef* k = cat.find(key);
        REQUIRE(k != nullptr);
        CHECK(k->salience == sal);
        CHECK_FALSE(eng::template_problem(*k, k->templ).has_value());
    }
    // 9.5, 13: the templates of kinds 48 and 53
    CHECK(cat.find("monster_raid")->templ == "{monster} {monster:raid} {@settlement}; {dead?no one died|one died|# died}.");
    CHECK(cat.find("beasts_bold")->templ ==
          "In {@region} the {@beast} {@beast:bold} {settlement}; {dead?no one|one|#} died.");
}

TEST_CASE("P5-03 9.5, 13, 8.2: template_problem of form raid, {@beast:bold} and the four-word choice") {
    const auto& c = eng::compiled();
    const eng::KindDef* raid = c.find("monster_raid");
    const eng::KindDef* battle = c.find("battle");
    const eng::KindDef* bold = c.find("beasts_bold");
    const eng::KindDef* arrived = c.find("refugees_arrived");
    REQUIRE(raid != nullptr);
    REQUIRE(battle != nullptr);
    REQUIRE(bold != nullptr);
    REQUIRE(arrived != nullptr);
    CHECK_FALSE(eng::template_problem(*raid, "{monster:raid} it.").has_value());
    CHECK(eng::template_problem(*raid, "{civilization:raid} it.") ==
          std::optional<std::string>("form 'raid' needs a monster role at byte 0"));
    CHECK(eng::template_problem(*battle, "Then {winner:raid} won.") ==
          std::optional<std::string>("form 'raid' needs a monster role at byte 5"));
    CHECK_FALSE(eng::template_problem(*bold, "The {@beast} {@beast:bold} it.").has_value());
    // "as {@beast}'s" (13): the same message for a kind without a value `beast` (reading R-B1: the placeholder's
    // own text is not compared)
    const auto p = eng::template_problem(*raid, "They {@beast:bold} it.");
    REQUIRE(p.has_value());
    CHECK(p->find("needs a kind with a value 'beast' at byte 5") != std::string::npos);
    CHECK_FALSE(eng::template_problem(*arrived, "{standing?a|b|c|d}").has_value());
    CHECK_FALSE(eng::template_problem(*arrived, "{standing?a|b|c}").has_value());
}

TEST_CASE("P5-03 8.2, 9.5, 11.3, 13: the sentence of every new kind and of kinds 48 and 53") {
    const Scene& s = scene();
    const db::WorldReader r = chrono_test::open_world(s.path);
    const auto err = eng::view_error(r);
    REQUIRE_MESSAGE(!err.has_value(), "HistoryView::load refused the scene: " << (err ? err->message : ""));
    auto at = [&](const std::string& label) { return eng::render(r, s.ev.at(label)); };
    const std::string A = s.plural.at(1), B = s.plural.at(2);
    const std::vector<std::string> raid = split_bar(s.raid_phrases), bold = split_bar(s.bold_phrases);
    REQUIRE(raid.size() == 3U);
    REQUIRE(bold.size() == 2U);
    for (const char* l : {"48a", "48b", "48c", "48d"}) {
        CAPTURE(l);
        const std::string ph = raid[rel_f::phrase_index(s.ev.at(l), raid.size())];
        CHECK(at(l) == "Old Grakh " + ph + " Drutnot Hole; 41 died.");
    }
    CHECK(at("53a") == "In King's Fold the wolves " + bold[rel_f::phrase_index(s.ev.at("53a"), 2)] +
                           " Drutnot Hole; 41 died.");
    CHECK(at("53b") == "In King's Fold the wolves " + bold[rel_f::phrase_index(s.ev.at("53b"), 2)] +
                           " Drutnot Hole; no one died.");
    CHECK(at("56") == "For fear of Old Grakh, the people of Drutnot Hole left their homes.");
    CHECK(at("58a") == "After the sacks, the " + A + " and the " + B + " came to hate one another.");
    CHECK(at("58b") == "The " + A + " and the " + B + " became friends.");
    // P5-03a 3.5: a turn citing the last turn reads "long years apart"
    CHECK(at("58c") == "After long years apart, the " + A + " and the " + B + " grew wary of one another.");
    CHECK(at("59-0") == "After the sack, 240 " + A + " from Nutskrank found shelter in Drutnot Hole, held as subjects "
                        "by the Snat Horde.");
    CHECK(at("59-1") == "After the sack, 240 " + A + " from Nutskrank found shelter in Drutnot Hole, tolerated by the "
                        "Snat Horde.");
    CHECK(at("59-2") == "After the sack, 240 " + A + " from Nutskrank found shelter in Drutnot Hole, welcomed by the "
                        "Snat Horde.");
    CHECK(at("59-3") == "After the sack, 240 " + A + " from Nutskrank found shelter in Drutnot Hole, among their own "
                        "kin of the Snat Horde.");
    // P5-03a 5.2
    CHECK(at("60") == "After the conquest, in Drutnot Hole the " + B + " ruled over the " + A + ".");
    CHECK(at("60b") == "In Drutnot Hole the " + B + " lived as equals with the " + A + ", who outnumbered them.");
    CHECK(at("61-0") == "In Drutnot Hole, the Snat Horde held the " + A + " as subjects.");
    CHECK(at("61-1") == "In Drutnot Hole, the Snat Horde tolerated the " + A + ".");
    CHECK(at("61-2") == "In Drutnot Hole, the Snat Horde accepted the " + A + " as equals.");
    // P5-03a 5.3
    CHECK(at("62") == "Hating the " + A + ", the Snat Horde drove them out of Drutnot Hole, where it had tolerated them "
                      "for 77 years; 310 left.");
    CHECK(at("62b") == "Hating the " + A + ", the Snat Horde drove them out of Drutnot Hole, where it had held them as "
                       "subjects for one year; 45 left.");   // P3-11's number words in the clause of {years}
    // a meeting of the world itself (57): its band's alternative
    REQUIRE(s.met != 0);
    const std::string met = eng::render(r, s.met);
    CHECK(met.rfind("The ", 0) == 0U);
    CHECK((met.find(" first met the ") != std::string::npos || met.find(" first had word of the ") != std::string::npos));
    CHECK((met.find(", and hated them from the first.") != std::string::npos ||
           met.find(", and were wary of them.") != std::string::npos ||
           met.find(", and were glad of them.") != std::string::npos));
}

}  // TEST_SUITE
