// Spec P3-04 r17 section 21 (P3-11, generator 0.20.0), tests 21.3:
//   21.1  values one to nine render as lower-case words in {value} and in the # of a chosen three-word form;
//         0, negative values and 10 to 999 as digits, 1,000 and above with separators; years stay digits; the
//         two-word form's # is an ordinary byte; ?-words are the template's own; the six rows of 21.1's table;
//   21.3 item 2  no shipped template puts a value at a sentence start;
//   21.2  kinds 39 claim_settlements and 40 settlements_ceded in the compiled catalogue; template_problem
//         accepts their templates and {settlement}, refuses {@settlement}; {settlement:all} with one, two and
//         three participants, each name under 8.5's names rule.
// Written from the spec before the implementation (test-writer rule), through render_event, template_problem and
// the compiled catalogue; the expected texts are the spec's, written by hand. The reference checks the same cases
// (reference/p3_engine_reference.py, check_render_r17).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <tuple>
#include <string>
#include <utility>
#include <vector>

#include "p3_04_engine_golden_support.hpp"
#include "worldgen/db/testing.hpp"

namespace {

using namespace engine_golden;

std::string q(const std::string& s) {
    std::string out = "'";
    for (const char c : s) {
        out += c;
        if (c == '\'') out += '\'';
    }
    return out + "'";
}

std::string roles_of(const eng::KindDef& k) {
    std::string out;
    for (const eng::RoleDef& r : k.roles) {
        out += std::to_string(r.seq) + ":" + r.role + ":" + std::to_string(static_cast<int>(r.type)) + ":" +
               std::to_string(r.min_count) + "-" + std::to_string(r.max_count) + ";";
    }
    return out;
}

// One event to render: a kind (by key from the world's catalogue, or a test kind with `templ` and values `vals`,
// roles civilization 1 and settlement 1), its participants (role seq, entity type, id) and values.
struct Ev {
    std::string kind;   // a catalogue key; empty: a test kind with templ and vals
    std::string templ;  // the test kind's template
    std::vector<std::pair<std::string, std::int64_t>> vals;
    std::vector<std::tuple<std::int64_t, std::int64_t, std::int64_t>> parts;  // (role seq, type, id)
    std::int64_t region = 0;
    std::int64_t year = 5999;
};

// A copy of the 512 world geo-seed-42-512 with the events appended in the last year; civilizations 1 and 2 named
// The Snat Horde and The Stinrild League, settlements 1 to 3 Drutnot Hole, Nutskrank and The Grey Fort, the
// events' region King's Fold.
std::vector<std::string> render_all(const std::string& file, const std::vector<Ev>& evs) {
    static dbtest::TempDir dir("engine-r17");
    const std::string src = world_of(golden().world("geo-seed-42-512"));
    const db::WorldReader base = open_world(src);
    std::int64_t eid = static_cast<std::int64_t>(rows_of(base, "event").size());
    std::int64_t kid = static_cast<std::int64_t>(rows_of(base, "event_kind").size());
    const std::int64_t phase = static_cast<std::int64_t>(rows_of(base, "phase").size());
    const std::int64_t region = rows_of(base, "settlement").at(0).at(4).integer;
    std::map<std::string, std::int64_t> by_key, caused, settlement_use;
    for (const auto& k : rows_of(base, "event_kind")) {
        by_key[k.at(1).bytes] = k.at(0).integer;
        caused[k.at(1).bytes] = k.at(2).integer;
        settlement_use[k.at(1).bytes] = k.at(4).integer;
    }
    std::string sql =
        "UPDATE civilization SET name = 'The Snat Horde' WHERE id = 1;"
        "UPDATE civilization SET name = 'The Stinrild League' WHERE id = 2;"
        "UPDATE settlement SET name = 'Drutnot Hole' WHERE id = 1;"
        "UPDATE settlement SET name = 'Nutskrank' WHERE id = 2;"
        "UPDATE settlement SET name = 'The Grey Fort' WHERE id = 3;"
        "UPDATE region SET name = 'King''s Fold' WHERE id = " + std::to_string(region) + ";"
        // P5-12: this region is renamed in the world (rg11, 3311); the appended events come after the renaming
        // (N(r, e), P3-04 r21 23.1), so its region_name rows carry the test's name too
        "UPDATE region_name SET name = 'King''s Fold' WHERE region = " + std::to_string(region) + ";";
    std::vector<std::int64_t> ids;
    for (std::size_t i = 0; i < evs.size(); ++i) {
        const Ev& ev = evs[i];
        std::string k;
        if (ev.kind.empty()) {
            k = std::to_string(++kid);
            sql += "INSERT INTO event_kind VALUES (" + k + ", " + q("test_r17_" + std::to_string(i)) + ", 0, 1, 1, 30, " +
                   q(ev.templ) + ");";
            sql += "INSERT INTO event_kind_role VALUES (" + k + ", 1, 'civilization', 3, 1, 1);";
            sql += "INSERT INTO event_kind_role VALUES (" + k + ", 2, 'settlement', 4, 1, 1);";
            for (std::size_t v = 0; v < ev.vals.size(); ++v) {
                sql += "INSERT INTO event_kind_value VALUES (" + k + ", " + std::to_string(v + 1) + ", " +
                       q(ev.vals[v].first) + ");";
            }
        } else {
            REQUIRE_MESSAGE(by_key.count(ev.kind) == 1U, "kind " << ev.kind << " not in the world's catalogue");
            k = std::to_string(by_key.at(ev.kind));
        }
        const std::string e = std::to_string(++eid);
        const std::int64_t settlement = (ev.kind.empty() || settlement_use.at(ev.kind) == 1) ? 1 : 0;
        sql += "INSERT INTO event VALUES (" + e + ", " + std::to_string(ev.year) + ", " + std::to_string(ev.year) +
               ", " + k + ", " + std::to_string(phase) + ", " + std::to_string(ev.region ? ev.region : region) + ", " +
               std::to_string(settlement) + ", 30);";
        std::vector<std::tuple<std::int64_t, std::int64_t, std::int64_t>> parts = ev.parts;
        if (ev.kind.empty() && parts.empty()) parts = {{1, 3, 1}, {2, 4, 1}};
        for (std::size_t p = 0; p < parts.size(); ++p) {
            sql += "INSERT INTO event_participant VALUES (" + e + ", " + std::to_string(p + 1) + ", " +
                   std::to_string(std::get<0>(parts[p])) + ", " + std::to_string(std::get<1>(parts[p])) + ", " +
                   std::to_string(std::get<2>(parts[p])) + ");";
        }
        for (std::size_t v = 0; v < ev.vals.size(); ++v) {
            sql += "INSERT INTO event_value VALUES (" + e + ", " + std::to_string(v + 1) + ", " +
                   std::to_string(ev.vals[v].second) + ");";
        }
        // a caused kind needs a cause: the world's first event
        if (!ev.kind.empty() && caused.at(ev.kind) == 1) sql += "INSERT INTO event_cause VALUES (" + e + ", 1);";
        ids.push_back(eid);
    }
    const std::string path = dir.file(file);
    dbtest::write_file(path, dbtest::read_file(src));
    const auto s = worldgen::db::testing::execute_on_file(path, sql);
    REQUIRE_MESSAGE(s.ok(), "SQL rejected");
    const db::WorldReader r = open_world(path);
    const auto err = eng::view_error(r);
    REQUIRE_MESSAGE(!err.has_value(), "HistoryView::load refused the test world");
    std::vector<std::string> out;
    for (const std::int64_t id : ids) out.push_back(eng::render(r, id));
    return out;
}

Ev val(const std::string& templ, std::int64_t n) { return Ev{"", templ, {{"n", n}}, {}}; }

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 r17 21.1, 21.3 item 1: one to nine as words in {value} and #; digits otherwise; years unchanged") {
    std::vector<Ev> evs;
    const std::vector<std::int64_t> vs = {0, 1, 2, 9, 10, 11, 999, 1000, -3};
    for (const std::int64_t v : vs) evs.push_back(val("{n}", v));
    for (const std::int64_t v : vs) evs.push_back(val("{n?zero|single|# x}", v));
    evs.push_back(val("{@year}", 3));
    evs.push_back(val("{n?one|#}", 1));
    evs.push_back(val("{n?one|#}", 4));   // the two-word form's # is an ordinary byte (8.4)
    evs.push_back(val("{n?|one|#}", 4));
    evs.push_back(val("{n?|one|#}", 1));  // the template's own word
    evs.push_back(val("{n} died.", 7));   // capitalised since P5-03 (lead ruling 1)
    // P5-03 lead ruling 1 (0.24.0): a first byte a to z is capitalised (r17 21.1 said a number word is never
    // capitalised; the ruling makes it the general rule)
    const std::vector<std::string> want = {
        "0", "One", "Two", "Nine", "10", "11", "999", "1,000", "-3",
        "Zero", "Single", "Two x", "Nine x", "10 x", "11 x", "999 x", "1,000 x", "-3 x",
        "5999", "One", "#", "Four", "One", "Seven died."};
    CHECK(render_all("numbers.db", evs) == want);
}

TEST_CASE("P3-04 r17 21.3 item 1: {@year} 5 renders 5 (years are never words)") {
    // an event dated in year 5, appended after the world's events (HistoryView may refuse a log out of year order;
    // this case stands alone so that a refusal is visible as such)
    Ev y5 = val("{@year}-{@end_year} {n}", 5);
    y5.year = 5;
    CHECK(render_all("year5.db", {y5}) == std::vector<std::string>{"5-5 five"});
}

TEST_CASE("P3-04 r17 21.1: the six rows of 21.1's table with the shipped templates") {
    using P = std::tuple<std::int64_t, std::int64_t, std::int64_t>;
    const std::int64_t CIV = 3, STL = 4;
    const std::vector<Ev> evs = {
        {"civ_seceded", "", {{"settlements", 1}}, {P{1, CIV, 2}, P{2, CIV, 1}}},
        {"plague_outbreak", "", {{"dead", 30}, {"population", 100}, {"total", 1084}, {"reached", 2}},
         {P{1, STL, 1}, P{2, CIV, 1}}},
        {"quiet_stretch", "", {{"years", 30}, {"realms", 4}}, {P{1, CIV, 1}, P{1, CIV, 2}, P{1, CIV, 3}, P{1, CIV, 4}}},
        {"peace", "", {{"years", 3}}, {P{1, CIV, 1}, P{2, CIV, 2}}},
        {"battle", "", {{"winner_dead", 1}, {"loser_dead", 7}}, {P{1, CIV, 1}, P{2, CIV, 2}}},
        {"famine", "", {{"dead", 8}, {"population", 400}}, {P{1, STL, 1}, P{2, CIV, 1}}},
    };
    const std::vector<std::string> got = render_all("table.db", evs);
    REQUIRE(got.size() == 6U);
    CHECK(got[0] == "The Stinrild League broke away from the Snat Horde with one settlement.");
    CHECK(got[1] == "Plague broke out in Drutnot Hole and spread to two other settlements; 1,084 died in all.");
    CHECK(got[2].find(", and four realms kept the peace.") != std::string::npos);
    CHECK(got[3].find("after three years of war.") != std::string::npos);
    CHECK(got[4].find("; seven of the defeated and one of the victors fell.") != std::string::npos);
    CHECK(got[5].find("; eight died.") != std::string::npos);
}

TEST_CASE("P3-04 r17 21.3 item 2: no shipped template puts a value at byte 0 or directly after \". \"") {
    const eng::EventCatalogue c = eng::catalogue_of(shipped());
    for (const eng::KindDef& k : c.kinds) {
        CAPTURE(k.key);
        std::vector<std::size_t> starts = {0};
        for (std::size_t i = 0; i + 2 < k.templ.size(); ++i) {
            if (k.templ[i] == '.' && k.templ[i + 1] == ' ') starts.push_back(i + 2);
        }
        for (const std::size_t at : starts) {
            if (at >= k.templ.size() || k.templ[at] != '{' || (at + 1 < k.templ.size() && k.templ[at + 1] == '{')) {
                continue;
            }
            const std::string body = k.templ.substr(at + 1, k.templ.find('}', at) - at - 1);
            const std::string v = body.substr(0, body.find('?'));
            const bool value = std::find(k.values.begin(), k.values.end(), v) != k.values.end();
            const std::size_t bars = static_cast<std::size_t>(std::count(body.begin(), body.end(), '|'));
            // P5-04a 3.6 (lead ruling, integration): a three-word choice is a number clause only when its last word
            // is `#` alone (`{n?none|one|#}`); a word choice such as alliance_spurned's `{reason?Still bitter|Long
            // resentful|Outraged}` or hero_rose's `{quiet?...|When, after # quiet years, }` is allowed at byte 0.
            const bool number_clause =
                bars == 2U && body.size() >= 2U && body.compare(body.size() - 2U, 2U, "|#") == 0;
            CHECK_FALSE((value && (body.find('?') == std::string::npos || number_clause)));
        }
    }
}

TEST_CASE("P3-04 r17 21.2: kinds 39 claim_settlements and 40 settlements_ceded in the compiled catalogue") {
    const eng::EventCatalogue& c = eng::compiled();
    REQUIRE(c.kinds.size() >= 40U);  // 42 from 0.21.0 (P3-04 r19 22.2: kinds 41, 42 appended)
    const eng::KindDef& k39 = c.kinds[38];
    CHECK(k39.id == 39);
    CHECK(k39.key == "claim_settlements");
    CHECK(k39.caused);
    CHECK(k39.region_use == 1);
    CHECK(k39.settlement_use == 0);
    CHECK(roles_of(k39) == "1:claimant:3:1-1;2:holder:3:1-1;3:settlement:4:1-0;");
    CHECK(k39.values.empty());
    const eng::KindDef& k40 = c.kinds[39];
    CHECK(k40.id == 40);
    CHECK(k40.key == "settlements_ceded");
    CHECK(k40.caused);
    CHECK(k40.region_use == 1);
    CHECK(k40.settlement_use == 0);
    CHECK(roles_of(k40) == "1:from:3:1-1;2:to:3:1-1;3:settlement:4:1-0;");
    CHECK(k40.values.empty());
    // kinds 20 and 29 keep their structure (8.2)
    CHECK(roles_of(*c.find("claim")) == "1:claimant:3:1-1;2:holder:3:1-1;");
    CHECK(roles_of(*c.find("region_ceded")) == "1:from:3:1-1;2:to:3:1-1;");
}

TEST_CASE("P3-04 r17 21.2, 21.3 item 3: the shipped entries and template_problem of kinds 39 and 40") {
    const eng::EventCatalogue c = eng::catalogue_of(shipped());
    const eng::KindDef* k39 = c.find("claim_settlements");
    const eng::KindDef* k40 = c.find("settlements_ceded");
    REQUIRE(k39 != nullptr);
    REQUIRE(k40 != nullptr);
    CHECK(k39->salience == 35);
    CHECK(k39->templ == "{claimant} laid claim to {settlement:all} of {holder} in {@region}.");
    CHECK(k40->salience == 55);
    CHECK(k40->templ == "{from} gave up {settlement:all} in {@region} to {to}.");
    for (const eng::KindDef* k : {k39, k40}) {
        CAPTURE(k->key);
        CHECK_FALSE(eng::template_problem(*k, k->templ).has_value());
        CHECK_FALSE(eng::template_problem(*k, "{settlement}.").has_value());   // min count 1: the first participant
        CHECK(eng::template_problem(*k, "x {@settlement}").has_value());       // settlement_use 0
    }
}

TEST_CASE("P3-04 r17 21.3 item 3: {settlement:all} with one, two and three settlements, names mid-sentence") {
    using P = std::tuple<std::int64_t, std::int64_t, std::int64_t>;
    const std::int64_t CIV = 3, STL = 4;
    std::vector<Ev> evs;
    for (std::int64_t n = 1; n <= 3; ++n) {
        Ev claim{"claim_settlements", "", {}, {P{1, CIV, 1}, P{2, CIV, 2}}};
        Ev ceded{"settlements_ceded", "", {}, {P{1, CIV, 2}, P{2, CIV, 1}}};
        for (std::int64_t s = 1; s <= n; ++s) {
            claim.parts.emplace_back(3, STL, s);
            ceded.parts.emplace_back(3, STL, s);
        }
        evs.push_back(claim);
        evs.push_back(ceded);
    }
    CHECK(render_all("all.db", evs) ==
          std::vector<std::string>{
              "The Snat Horde laid claim to Drutnot Hole of the Stinrild League in King's Fold.",
              "The Stinrild League gave up Drutnot Hole in King's Fold to the Snat Horde.",
              "The Snat Horde laid claim to Drutnot Hole and Nutskrank of the Stinrild League in King's Fold.",
              "The Stinrild League gave up Drutnot Hole and Nutskrank in King's Fold to the Snat Horde.",
              "The Snat Horde laid claim to Drutnot Hole, Nutskrank and the Grey Fort of the Stinrild League in "
              "King's Fold.",
              "The Stinrild League gave up Drutnot Hole, Nutskrank and the Grey Fort in King's Fold to the Snat Horde.",
          });
}

}  // TEST_SUITE
