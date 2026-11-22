// Spec P3-04 r10 (generator 0.17.0, P3-08), tests 16.1 item 10 (r10): the
// three-word value form {value?w0|w1|w2} with # for the value (8.4, 8.5),
// and names inside a sentence (8.5; P1-06 r3 14.7): a leading "The " of a
// name appended mid-sentence is written "the ", at a sentence start (offset
// 0 or after ". ") it keeps its capital, in {role:all} each name is judged
// at its own offset, and the stored names never change.
//
// Every case goes through render_event on a copy of a generated 512 world
// (geo-seed-42-512) to which this test adds, by SQL, test kinds holding the
// templates (each its own kind) and one event per case, and renames some
// entities to the names of P1-06 r3 14.7's examples. The expected texts are
// the spec's examples, written here by hand. They need no new API: every
// case fails until the renderer of r10 is in place.

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "p3_04_engine_support.hpp"
#include "worldgen/db/testing.hpp"

namespace {

using namespace engine_test;

// A role of a test kind: name, entity type code (P3-04 9.2), min and max count.
struct TRole {
    std::string role;
    int type;
    int min_count;
    int max_count;
};

// One test case: a kind (template, roles, values, region/settlement use) and its event.
struct TCase {
    std::string templ;
    std::vector<TRole> roles;
    std::vector<std::string> values;
    // participants: (role seq, entity type, entity id), in participant seq order
    std::vector<std::tuple<int, int, std::int64_t>> parts;
    std::vector<std::int64_t> vals;
    std::int64_t region = 0;      // the event's region (0: none; region_use 1 when > 0)
    std::int64_t settlement = 0;  // the event's settlement (settlement_use 1 when > 0; the region is then
                                  // the settlement's own, read from the world)
};

constexpr int RACE = 1, GOD = 2, CIV = 3, SET = 4, REG = 5;

std::string q(const std::string& s) {
    std::string out = "'";
    for (const char c : s) {
        out += c;
        if (c == '\'') out += '\'';
    }
    return out + "'";
}

// The entities renamed for the cases (P1-06 r3 14.7, P3-04 r10 8.5).
const char* const RENAMES =
    "UPDATE god SET name = 'Fielie' WHERE id = 1;"
    "UPDATE civilization SET name = 'The Skib Horde' WHERE id = 1;"
    "UPDATE civilization SET name = 'The Hold of Durgarin' WHERE id = 2;"
    "UPDATE civilization SET name = 'The Court of Saene' WHERE id = 3;"
    "UPDATE civilization SET name = 'The Warrens of Bonk-Gitch' WHERE id = 4;"
    "UPDATE civilization SET name = 'Clan Durgarin' WHERE id = 5;"
    "UPDATE settlement SET name = 'Makmoudin Deep' WHERE id = 1;"
    "UPDATE settlement SET name = 'Thebes' WHERE id = 2;"
    "UPDATE settlement SET name = 'There Hollow' WHERE id = 3;"
    "UPDATE settlement SET name = 'The' WHERE id = 4;"
    "UPDATE settlement SET name = 'THE Mob' WHERE id = 5;"
    "UPDATE region SET name = 'The A' WHERE id = 1;"
    "UPDATE region SET name = 'The B' WHERE id = 2;"
    "UPDATE region SET name = 'The C' WHERE id = 3;"
    "UPDATE region SET name = 'The Scarred Downs' WHERE id = 4;"
    "UPDATE region SET name = 'The Grove of the Hunt' WHERE id = 5;"
    "UPDATE region SET name = 'West Wood' WHERE id = 6;"
    "UPDATE region SET name = 'The Ashen Downs' WHERE id = 7;";

struct Built {
    std::string path;
    std::int64_t first_event = 0;  // the event id of case 0
};

// A copy of the 512 world with RENAMES and one kind and one event per case.
Built build(const std::string& file, const std::vector<TCase>& cases) {
    static dbtest::TempDir dir("engine-render-r10");
    const std::string src = world_of(golden().world("geo-seed-42-512"));
    const db::WorldReader base = open_world(src);
    const std::int64_t n = static_cast<std::int64_t>(rows_of(base, "event").size());
    const std::int64_t k0 = static_cast<std::int64_t>(rows_of(base, "event_kind").size()) + 1;
    const std::int64_t phase = static_cast<std::int64_t>(rows_of(base, "phase").size());
    REQUIRE(rows_of(base, "civilization").size() >= 5U);
    REQUIRE(rows_of(base, "settlement").size() >= 5U);
    const auto settlements = rows_of(base, "settlement");
    std::string sql = RENAMES;
    for (std::size_t i = 0; i < cases.size(); ++i) {
        TCase c = cases[i];
        if (c.settlement > 0) {
            c.region = settlements.at(static_cast<std::size_t>(c.settlement - 1)).at(4).integer;  // its region
        }
        const std::string kid = std::to_string(k0 + static_cast<std::int64_t>(i));
        const std::string eid = std::to_string(n + 1 + static_cast<std::int64_t>(i));
        sql += "INSERT INTO event_kind VALUES (" + kid + ", " + q("test_r10_" + std::to_string(i)) + ", 0, " +
               (c.region > 0 ? "1" : "0") + ", " + (c.settlement > 0 ? "1" : "0") + ", 30, " + q(c.templ) + ");";
        for (std::size_t r = 0; r < c.roles.size(); ++r) {
            sql += "INSERT INTO event_kind_role VALUES (" + kid + ", " + std::to_string(r + 1) + ", " +
                   q(c.roles[r].role) + ", " + std::to_string(c.roles[r].type) + ", " +
                   std::to_string(c.roles[r].min_count) + ", " + std::to_string(c.roles[r].max_count) + ");";
        }
        for (std::size_t v = 0; v < c.values.size(); ++v) {
            sql += "INSERT INTO event_kind_value VALUES (" + kid + ", " + std::to_string(v + 1) + ", " +
                   q(c.values[v]) + ");";
        }
        sql += "INSERT INTO event VALUES (" + eid + ", 5999, 5999, " + kid + ", " + std::to_string(phase) + ", " +
               std::to_string(c.region) + ", " + std::to_string(c.settlement) + ", 30);";
        for (std::size_t p = 0; p < c.parts.size(); ++p) {
            const auto& [rs, type, id] = c.parts[p];
            sql += "INSERT INTO event_participant VALUES (" + eid + ", " + std::to_string(p + 1) + ", " +
                   std::to_string(rs) + ", " + std::to_string(type) + ", " + std::to_string(id) + ");";
        }
        for (std::size_t v = 0; v < c.vals.size(); ++v) {
            sql += "INSERT INTO event_value VALUES (" + eid + ", " + std::to_string(v + 1) + ", " +
                   std::to_string(c.vals[v]) + ");";
        }
    }
    const std::string path = dir.file(file);
    dbtest::write_file(path, dbtest::read_file(src));
    const auto s = worldgen::db::testing::execute_on_file(path, sql);
    REQUIRE_MESSAGE(s.ok(), "SQL rejected");
    return Built{path, n + 1};
}

std::vector<std::string> render_cases(const std::string& file, const std::vector<TCase>& cases) {
    const Built b = build(file, cases);
    const db::WorldReader r = open_world(b.path);
    const auto err = eng::view_error(r);
    REQUIRE_MESSAGE(!err.has_value(), "HistoryView::load refused the test world");
    std::vector<std::string> out;
    for (std::size_t i = 0; i < cases.size(); ++i) {
        out.push_back(eng::render(r, b.first_event + static_cast<std::int64_t>(i)));
    }
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 r10 8.4, 8.5, 16.1 item 10: the three-word value form with 0, 1, 2 and -1; # in the chosen word only") {
    const TRole none{"x", CIV, 0, 0};
    std::vector<TCase> cases;
    for (const std::int64_t v : {std::int64_t{0}, std::int64_t{1}, std::int64_t{2}, std::int64_t{-1}, std::int64_t{12}}) {
        cases.push_back(TCase{"x{fled?| and one fled| and # fled}.", {none}, {"fled"}, {}, {v}});
    }
    for (const std::int64_t v : {std::int64_t{0}, std::int64_t{1}, std::int64_t{5}}) {
        cases.push_back(TCase{"{n?#a|#b|#c}", {none}, {"n"}, {}, {v}});
    }
    // In the two-word form # is an ordinary byte (as before r10).
    for (const std::int64_t v : {std::int64_t{1}, std::int64_t{3}}) {
        cases.push_back(TCase{"{n?#|##}", {none}, {"n"}, {}, {v}});
    }
    const std::vector<std::string> got = render_cases("three-word.db", cases);
    // P3-04 r17 21.1 (0.20.0): a value from one to nine in # renders as a word
    CHECK(got == std::vector<std::string>{"x.", "x and one fled.", "x and two fled.", "x and -1 fled.", "x and 12 fled.",
                                          "0a", "oneb", "fivec", "#", "##"});
}

TEST_CASE("P3-04 r10 8.4: a body with four words is an unknown placeholder; the three-word form is valid") {
    eng::KindDef k;
    k.id = 99;
    k.key = "test_r10";
    k.caused = false;
    k.region_use = 0;
    k.settlement_use = 0;
    k.roles = {};
    k.values = {"n"};
    CHECK_FALSE(eng::template_problem(k, "{n?a|b|c}").has_value());
    CHECK_FALSE(eng::template_problem(k, "{n?|one|#}").has_value());
    CHECK(eng::template_problem(k, "{n?a|b|c|d}") == std::optional<std::string>("unknown placeholder '{n?a|b|c|d}' at byte 0"));
    CHECK(eng::template_problem(k, "x {m?a|b|c}") == std::optional<std::string>("unknown placeholder '{m?a|b|c}' at byte 2"));
    // The shipped templates of 0.17.0 that use the form parse (P3-04 r10 4.2, P3-05 r6 11, P3-06 r7 11).
    for (const ReportEntry& e : shipped_report()) {
        CAPTURE(e.kind);
        const eng::KindDef* kd = eng::compiled().find(e.kind);
        REQUIRE(kd != nullptr);
        CHECK_FALSE(eng::template_problem(*kd, e.templ).has_value());
    }
}

TEST_CASE("P3-04 r10 8.5, P1-06 r3 14.7: names inside a sentence, the examples of 8.5 and 14.7") {
    const std::vector<TCase> cases = {
        // 0 awakening (region The Scarred Downs)
        {"{creator} awakened the {race:plural} in {@region}.", {{"race", RACE, 1, 1}, {"creator", GOD, 1, 1}}, {},
         {{1, RACE, 1}, {2, GOD, 1}}, {}, 4},
        // 1 war: a The name at the start keeps its capital, the two later ones do not
        {"{attacker} declared war on {defender} over {@region}.", {{"attacker", CIV, 1, 1}, {"defender", CIV, 1, 1}},
         {}, {{1, CIV, 1}, {2, CIV, 2}}, {}, 5},
        // 2 the leading The is a template literal
        {"The {race:plural} founded {civilization} in {@region}.", {{"civilization", CIV, 1, 1}, {"race", RACE, 1, 1}},
         {}, {{1, CIV, 4}, {2, RACE, 2}}, {}, 6},
        // 3 {parties:all}
        {"{parties:all} made peace after {years} {years?year|years} of war, neither the victor.",
         {{"parties", CIV, 2, 2}}, {"years"}, {{1, CIV, 1}, {1, CIV, 3}}, {12}},
        // 4 sentence start
        {"{civilization} came to an end.", {{"civilization", CIV, 1, 1}}, {}, {{1, CIV, 1}}, {}},
        // 5 siege: {@settlement} and a mid-sentence The name after "of"
        {"{attacker} besieged {@settlement} of {defender}; it {fell?fell|held}.",
         {{"attacker", CIV, 1, 1}, {"defender", CIV, 1, 1}}, {"fell"}, {{1, CIV, 5}, {2, CIV, 4}}, {1}, 1, 1},
        // 6 claim: after ", "
        {"{claimant} laid claim to {@region}, held by {holder}.", {{"claimant", CIV, 1, 1}, {"holder", CIV, 1, 1}}, {},
         {{1, CIV, 5}, {2, CIV, 3}}, {}, 7},
    };
    const std::vector<std::string> got = render_cases("names.db", cases);
    CHECK(got == std::vector<std::string>{
                     "Fielie awakened the Elves in the Scarred Downs.",
                     "The Skib Horde declared war on the Hold of Durgarin over the Grove of the Hunt.",
                     "The Goblins founded the Warrens of Bonk-Gitch in West Wood.",
                     "The Skib Horde and the Court of Saene made peace after 12 years of war, neither the victor.",
                     "The Skib Horde came to an end.",
                     "Clan Durgarin besieged Makmoudin Deep of the Warrens of Bonk-Gitch; it fell.",
                     "Clan Durgarin laid claim to the Ashen Downs, held by the Court of Saene.",
                 });
}

TEST_CASE("P3-04 r10 8.5: only a leading 'The ' exactly; '. ' starts a sentence; other separators do not") {
    const TRole s1{"s", SET, 1, 1};
    const TRole c1{"civilization", CIV, 1, 1};
    std::vector<TCase> cases;
    // Thebes, There Hollow, The (alone) and THE Mob are unchanged mid-sentence.
    for (const std::int64_t id : {std::int64_t{2}, std::int64_t{3}, std::int64_t{4}, std::int64_t{5}}) {
        cases.push_back(TCase{"x {s}", {s1}, {}, {{1, SET, id}}, {}});
    }
    cases.push_back(TCase{"It ended. {civilization} was no more.", {c1}, {}, {{1, CIV, 1}}, {}});
    for (const char* sep : {": ", "; ", "(", ", ", "x and "}) {
        cases.push_back(TCase{std::string("a") + sep + "{civilization}", {c1}, {}, {{1, CIV, 1}}, {}});
    }
    // {role:all} with three The names at the start and mid-sentence (regions The A, The B, The C).
    const TRole places{"places", REG, 0, 3};
    cases.push_back(TCase{"{places:all} met.", {places}, {}, {{1, REG, 1}, {1, REG, 2}, {1, REG, 3}}, {}});
    cases.push_back(TCase{"Met: {places:all}.", {places}, {}, {{1, REG, 1}, {1, REG, 2}, {1, REG, 3}}, {}});
    // {role:plural}, {role:adjective}, values, ?-words and literals are copied as they are.
    cases.push_back(TCase{"x {r:plural} {r:adjective} {n?The one|The many} The", {{"r", RACE, 1, 1}}, {"n"},
                          {{1, RACE, 1}}, {2}});
    const std::vector<std::string> got = render_cases("rules.db", cases);
    CHECK(got == std::vector<std::string>{"x Thebes", "x There Hollow", "x The", "x THE Mob",
                                          "It ended. The Skib Horde was no more.", "a: the Skib Horde",
                                          "a; the Skib Horde", "a(the Skib Horde", "a, the Skib Horde",
                                          "ax and the Skib Horde", "The A, the B and the C met.",
                                          "Met: the A, the B and the C.", "x Elves Elven The many The"});
}

TEST_CASE("P3-04 r10 8.5: rendering changes no stored name") {
    const std::vector<TCase> cases = {
        {"x {civilization} {@region}", {{"civilization", CIV, 1, 1}}, {}, {{1, CIV, 1}}, {}, 4}};
    const Built b = build("stored.db", cases);
    const db::WorldReader r = open_world(b.path);
    CHECK(eng::render(r, b.first_event) == "x the Skib Horde the Scarred Downs");
    // The tables keep the capital (P3-04 r10 8.5: stored names and display names never change).
    CHECK(rows_of(r, "civilization").at(0).at(1).bytes == "The Skib Horde");
    CHECK(rows_of(r, "region").at(3).at(1).bytes == "The Scarred Downs");
    // A sentence starting with the name keeps it.
    const std::vector<TCase> start = {{"{civilization}.", {{"civilization", CIV, 1, 1}}, {}, {{1, CIV, 1}}, {}}};
    CHECK(render_cases("start.db", start) == std::vector<std::string>{"The Skib Horde."});
}

}  // TEST_SUITE
