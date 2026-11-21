// Spec P3-04 r12 (generator 0.19.0, P3-10, docs/specs/p3-10-followups.md), tests 20.6:
//   20.4  kinds 37 plague_outbreak and 38 quiet_stretch in the compiled catalogue, with the shipped
//         saliences and templates (data of P3-05 r12 22.1 and P3-04 r12 20.1);
//   20.1  one world-level quiet_stretch per quiet phase row on the stub world cases and the ForcedTension
//         cases (participants every civilization alive at the row's first step in ascending id as `realm`,
//         realms their number, years = end_year - year + 1 = the row's length; no quiet_era event);
//   20.2  the placeholder {@causes:...}: lists, first occurrence kept, joins as {role:all}, empty when no cause
//         maps, the name after it judged on the text rendered so far, every malformed body of 20.2;
//   20.3  thousands separators in {value} and #, never in years, the choice by the value.
// Written from the spec before the implementation (test-writer rule): the catalogue, quiet-stretch and
// rendering cases need no new API (render_event, template_problem, the compiled catalogue) and fail until
// the implementation of r12 lands. The expected texts are the spec's, written here by hand; the reference
// (reference/p3_engine_reference.py, check_render_r12 and check_quiet_r12) checks the same cases.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "p3_04_engine_golden_support.hpp"
#include "worldgen/db/testing.hpp"

namespace {

using namespace engine_golden;


bool null_cell(const Cell& c) { return c.type == db::ValueType::null; }

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

// One rendering case: a test kind with `templ` (roles civilization and settlement, value `n`), its event with
// value `n`, and causes: events of the test cause kinds named in `causes` (keys test_r12_k_<name>).
struct CCase {
    std::string templ;
    std::vector<std::string> causes;  // cause kind names, in cause id order
    std::int64_t n = 0;
    std::vector<std::pair<std::string, std::int64_t>> vals = {};  // the kind's values; empty: {n}
};

// The test cause kinds (uncaused, no role, no value), one per name, keys test_r12_k_<name>.
const std::vector<std::string> CAUSE_KINDS = {"famine", "plague", "spread", "battle", "tribute", "colony", "other"};

struct BuiltC {
    std::string path;
    std::vector<std::int64_t> case_event;  // the event id of each case
};

// A copy of the 512 world (geo-seed-42-512) with the test kinds, the cause events and one event per case,
// all dated in the last year; civilization 1 renamed `The Warrens of Zeez`, settlement 1 `Glathenddell`.
BuiltC build_causes(const std::string& file, const std::vector<CCase>& cases) {
    static dbtest::TempDir dir("engine-r12");
    const std::string src = world_of(golden().world("geo-seed-42-512"));
    const db::WorldReader base = open_world(src);
    std::int64_t eid = static_cast<std::int64_t>(rows_of(base, "event").size());
    std::int64_t kid = static_cast<std::int64_t>(rows_of(base, "event_kind").size());
    const std::int64_t phase = static_cast<std::int64_t>(rows_of(base, "phase").size());
    const std::int64_t region = rows_of(base, "settlement").at(0).at(4).integer;
    std::string sql =
        "UPDATE civilization SET name = 'The Warrens of Zeez' WHERE id = 1;"
        "UPDATE settlement SET name = 'Glathenddell' WHERE id = 1;";
    std::map<std::string, std::int64_t> cause_kind;
    for (const std::string& c : CAUSE_KINDS) {
        cause_kind[c] = ++kid;
        sql += "INSERT INTO event_kind VALUES (" + std::to_string(kid) + ", " + q("test_r12_k_" + c) +
               ", 0, 0, 0, 10, 'x');";
    }
    BuiltC out;
    for (std::size_t i = 0; i < cases.size(); ++i) {
        std::vector<std::int64_t> cause_ids;
        for (const std::string& c : cases[i].causes) {
            cause_ids.push_back(++eid);
            sql += "INSERT INTO event VALUES (" + std::to_string(eid) + ", 5999, 5999, " +
                   std::to_string(cause_kind.at(c)) + ", " + std::to_string(phase) + ", 0, 0, 10);";
        }
        const std::string k = std::to_string(++kid);
        sql += "INSERT INTO event_kind VALUES (" + k + ", " + q("test_r12_" + std::to_string(i)) + ", 0, 1, 1, 30, " +
               q(cases[i].templ) + ");";
        sql += "INSERT INTO event_kind_role VALUES (" + k + ", 1, 'civilization', 3, 1, 1);";
        sql += "INSERT INTO event_kind_role VALUES (" + k + ", 2, 'settlement', 4, 1, 1);";
        const std::vector<std::pair<std::string, std::int64_t>> vals =
            cases[i].vals.empty() ? std::vector<std::pair<std::string, std::int64_t>>{{"n", cases[i].n}} : cases[i].vals;
        for (std::size_t v = 0; v < vals.size(); ++v) {
            sql += "INSERT INTO event_kind_value VALUES (" + k + ", " + std::to_string(v + 1) + ", " + q(vals[v].first) +
                   ");";
        }
        const std::string e = std::to_string(++eid);
        sql += "INSERT INTO event VALUES (" + e + ", 5999, 5999, " + k + ", " + std::to_string(phase) + ", " +
               std::to_string(region) + ", 1, 30);";
        sql += "INSERT INTO event_participant VALUES (" + e + ", 1, 1, 3, 1);";
        sql += "INSERT INTO event_participant VALUES (" + e + ", 2, 2, 4, 1);";
        for (std::size_t v = 0; v < vals.size(); ++v) {
            sql += "INSERT INTO event_value VALUES (" + e + ", " + std::to_string(v + 1) + ", " +
                   std::to_string(vals[v].second) + ");";
        }
        for (const std::int64_t c : cause_ids) {
            sql += "INSERT INTO event_cause VALUES (" + e + ", " + std::to_string(c) + ");";
        }
        out.case_event.push_back(eid);
    }
    out.path = dir.file(file);
    dbtest::write_file(out.path, dbtest::read_file(src));
    const auto s = worldgen::db::testing::execute_on_file(out.path, sql);
    REQUIRE_MESSAGE(s.ok(), "SQL rejected");
    return out;
}

std::vector<std::string> render_c(const std::string& file, const std::vector<CCase>& cases) {
    const BuiltC b = build_causes(file, cases);
    const db::WorldReader r = open_world(b.path);
    const auto err = eng::view_error(r);
    REQUIRE_MESSAGE(!err.has_value(), "HistoryView::load refused the test world");
    std::vector<std::string> out;
    for (const std::int64_t e : b.case_event) {
        out.push_back(eng::render(r, e));
    }
    return out;
}

const std::string M =
    "test_r12_k_famine=famine,test_r12_k_plague=plague,test_r12_k_spread=plague,test_r12_k_battle=war,"
    "test_r12_k_tribute=tribute";

// 20.1 on the rows of one world: one quiet_stretch per quiet phase row with a living civilization (r15 20.1 item
// 1a: and at least quiet_min_years long; a shorter row has none).
void check_quiet_stretches(const Tables& t) {
    const std::int64_t qmin = shipped_quiet_min_years();
    std::int64_t k38 = 0, k4 = 0;
    for (const Row& k : t.event_kind) {
        if (k.at(1).bytes == "quiet_stretch") k38 = at(k, 0);
        if (k.at(1).bytes == "quiet_era") k4 = at(k, 0);
    }
    REQUIRE(k38 == 38);
    REQUIRE(k4 == 4);
    std::map<std::int64_t, std::vector<std::int64_t>> parts;  // event -> participant ids (participant seq order)
    std::map<std::int64_t, std::vector<std::int64_t>> roles;
    for (const Row& p : t.event_participant) {
        parts[at(p, 0)].push_back(at(p, 4));
        roles[at(p, 0)].push_back(at(p, 2) * 10 + at(p, 3));  // role seq 1, type civilization
    }
    std::map<std::int64_t, std::map<std::int64_t, std::int64_t>> vals;
    for (const Row& v : t.event_value) vals[at(v, 0)][at(v, 1)] = at(v, 2);
    std::set<std::int64_t> caused;
    for (const Row& c : t.event_cause) caused.insert(at(c, 0));
    std::map<std::int64_t, std::vector<const Row*>> by_phase;
    for (const Row& e : t.event) {
        CHECK_MESSAGE(at(e, 3) != k4, "event " << at(e, 0) << " is a quiet_era (not written from 0.19.0)");
        if (at(e, 3) == k38) by_phase[at(e, 4)].push_back(&e);
    }
    std::int64_t rows = 0;
    for (const Row& ph : t.phase) {
        if (at(ph, 2) != 2) continue;
        ++rows;
        CAPTURE(at(ph, 0));
        std::vector<std::int64_t> alive;
        for (const Row& c : t.civilization) {
            const bool ended_before = !null_cell(c.at(8)) && at(c, 8) < at(ph, 3);
            if (at(c, 6) <= at(ph, 3) && !ended_before) alive.push_back(at(c, 0));
        }
        const auto& mine = by_phase[at(ph, 0)];
        if (alive.empty() || at(ph, 4) - at(ph, 3) + 1 < qmin) {
            CHECK(mine.empty());
            continue;
        }
        REQUIRE(mine.size() == 1U);
        const Row& e = *mine[0];
        CHECK(at(e, 1) == at(ph, 3));
        CHECK(at(e, 2) == at(ph, 4));
        CHECK(at(e, 5) == 0);
        CHECK(at(e, 6) == 0);
        CHECK(at(e, 7) == 20);
        CHECK(caused.count(at(e, 0)) == 0U);
        CHECK(parts[at(e, 0)] == alive);
        CHECK(roles[at(e, 0)] == std::vector<std::int64_t>(alive.size(), 13));
        CHECK(vals[at(e, 0)] == std::map<std::int64_t, std::int64_t>{{1, at(ph, 4) - at(ph, 3) + 1},
                                                                    {2, static_cast<std::int64_t>(alive.size())}});
    }
    CHECK(rows > 0);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 r12 20.4: kinds 37 plague_outbreak and 38 quiet_stretch in the compiled catalogue; 4 and 12 kept") {
    const eng::EventCatalogue& c = eng::compiled();
    REQUIRE(c.kinds.size() == 38U);
    const eng::KindDef& k37 = c.kinds[36];
    CHECK(k37.id == 37);
    CHECK(k37.key == "plague_outbreak");
    CHECK_FALSE(k37.caused);
    CHECK(k37.region_use == 1);
    CHECK(k37.settlement_use == 1);
    CHECK(roles_of(k37) == "1:settlement:4:1-1;2:civilization:3:1-1;");
    CHECK(k37.values == std::vector<std::string>{"dead", "population", "total", "reached"});
    const eng::KindDef& k38 = c.kinds[37];
    CHECK(k38.id == 38);
    CHECK(k38.key == "quiet_stretch");
    CHECK_FALSE(k38.caused);
    CHECK(k38.region_use == 0);
    CHECK(k38.settlement_use == 0);
    CHECK(roles_of(k38) == "1:realm:3:1-0;");
    CHECK(k38.values == std::vector<std::string>{"years", "realms"});
    // kinds 4 and 12 keep their structure (8.2: a shipped kind is never edited or removed)
    CHECK(c.find("quiet_era") == &c.kinds[3]);
    CHECK(c.kinds[3].values == std::vector<std::string>{"years", "generations"});
    CHECK(c.find("plague") == &c.kinds[11]);
    CHECK(c.kinds[11].values == std::vector<std::string>{"dead", "population"});
}

TEST_CASE("P3-04 r12 20.1, P3-05 r12 22.1: the shipped entries of kinds 37 and 38 (salience, template)") {
    const eng::EventCatalogue c = eng::catalogue_of(shipped());
    const eng::KindDef* k37 = c.find("plague_outbreak");
    const eng::KindDef* k38 = c.find("quiet_stretch");
    REQUIRE(k37 != nullptr);
    REQUIRE(k38 != nullptr);
    CHECK(k37->salience == 45);
    CHECK(k37->templ ==
          "Plague broke out in {settlement}{reached?| and spread to one other settlement| and spread to # other "
          "settlements}; {total} died{reached?| in all| in all}.");
    CHECK(k38->salience == 20);
    CHECK(k38->templ ==
          "For {years} {years?year|years}, from {@year} to {@end_year}, the land was quiet{realms?|; one realm "
          "held it|, and # realms kept the peace}.");
    CHECK_FALSE(eng::template_problem(*k37, k37->templ).has_value());
    CHECK_FALSE(eng::template_problem(*k38, k38->templ).has_value());
}

TEST_CASE("P3-04 r12 20.2: {@causes:...} is valid for any kind; every malformed body is an unknown placeholder") {
    eng::KindDef k;
    k.id = 99;
    k.key = "test_r12";
    k.values = {"n"};
    for (const char* ok : {"{@causes:a=x|Weakened by |, }", "{@causes:famine=famine,plague=plague|W |, }x",
                           "{@causes:nokind=word|a|b}", "{@causes:a=x y,b_2=z|, w=|.}", "{@causes:a=x||}"}) {
        CAPTURE(ok);
        CHECK_FALSE(eng::template_problem(k, ok).has_value());
    }
    for (const char* body : {"@causes:a=x|||", "@causes:a=x|w", "@causes:a=x", "@causes:|a|b", "@causes:a=|a|b",
                             "@causes:a=x,a=y|a|b", "@causes:Ab=x|a|b", "@causes:1a=x|a|b", "@causes:a-b=x|a|b",
                             "@causes:a=x=y|a|b", "@causes:a=x,|a|b", "@causes:a=x?|a|b", "@causes:a=x|a?|b",
                             "@causes", "@causes:"}) {
        CAPTURE(body);
        CHECK(eng::template_problem(k, std::string("ab{") + body + "}") ==
              std::optional<std::string>(std::string("unknown placeholder '{") + body + "}' at byte 2"));
    }
}

TEST_CASE("P3-04 r12 20.2, 20.6 item 2: {@causes:...} renders the causes' words, joined, with the two words") {
    const std::string tail = "{settlement} dwindled to {n} souls.";
    const std::string head = "{@causes:" + M + "|Weakened by |, }";
    const std::vector<CCase> cases = {
        {head + tail, {"famine", "plague"}, 412},
        {head + tail, {"plague", "spread"}, 412},                       // both map to plague: one word
        {head + tail, {"battle", "famine", "tribute"}, 412},            // three words
        {head + tail, {"tribute", "famine"}, 412},                      // ascending cause id, not entry order
        {head + tail, {"colony", "other"}, 412},                        // no mapped cause: empty
        {head + tail, {}, 412},                                         // no cause: empty
        {head + "{civilization} moved.", {"battle"}, 0},                // mid-sentence after ", "
        {head + "{civilization} moved.", {"other"}, 0},                 // empty at offset 0: sentence start
        {"{@causes:test_r12_k_battle=The War|The |: }x", {"battle"}, 0},  // literals, no name rule
    };
    CHECK(render_c("causes.db", cases) == std::vector<std::string>{
                                             "Weakened by famine and plague, Glathenddell dwindled to 412 souls.",
                                             "Weakened by plague, Glathenddell dwindled to 412 souls.",
                                             "Weakened by war, famine and tribute, Glathenddell dwindled to 412 souls.",
                                             "Weakened by tribute and famine, Glathenddell dwindled to 412 souls.",
                                             "Glathenddell dwindled to 412 souls.",
                                             "Glathenddell dwindled to 412 souls.",
                                             "Weakened by war, the Warrens of Zeez moved.",
                                             "The Warrens of Zeez moved.",
                                             "The The War: x",
                                         });
}

TEST_CASE("P3-04 r12 20.3, 20.6 item 3: thousands separators in {value} and #; not in years; the choice by value") {
    std::vector<CCase> cases;
    const std::vector<std::int64_t> vs = {0, 999, 1000, 1155, 25000, 1000000, -1000, -999, 52691};
    for (const std::int64_t v : vs) cases.push_back({"{n}", {}, v});
    for (const std::int64_t v : vs) cases.push_back({"{n?zero|one|# x}", {}, v});
    cases.push_back({"{@year}", {}, 0});
    cases.push_back({"{n?one|many}", {}, 1000});
    const std::vector<std::string> got = render_c("separators.db", cases);
    const std::vector<std::string> want = {"0", "999", "1,000", "1,155", "25,000", "1,000,000", "-1,000", "-999",
                                           "52,691", "zero", "999 x", "1,000 x", "1,155 x", "25,000 x",
                                           "1,000,000 x", "-1,000 x", "-999 x", "52,691 x", "5999", "many"};
    CHECK(got == want);
}

TEST_CASE("P3-05 r12 22.1, 22.8 item 1: the outbreak's sentence with 2, 1 and 0 spreads; P3-04 r12 20.1's quiet line") {
    const std::string t37 =
        "Plague broke out in {settlement}{reached?| and spread to one other settlement| and spread to # other "
        "settlements}; {total} died{reached?| in all| in all}.";
    const std::string t38 =
        "For {years} {years?year|years}, from {@year} to {@end_year}, the land was quiet{realms?|; one realm "
        "held it|, and # realms kept the peace}.";
    using V = std::vector<std::pair<std::string, std::int64_t>>;
    const V k37 = {{"dead", 600}, {"population", 5400}, {"total", 1020}, {"reached", 2}};
    const std::vector<CCase> cases = {
        {t37, {}, 0, k37},
        {t37, {}, 0, V{{"dead", 600}, {"population", 5400}, {"total", 900}, {"reached", 1}}},
        {t37, {}, 0, V{{"dead", 120}, {"population", 1080}, {"total", 120}, {"reached", 0}}},
        {t37, {}, 0, V{{"dead", 1084}, {"population", 9000}, {"total", 1084}, {"reached", 3}}},
        {t38, {}, 0, V{{"years", 1924}, {"realms", 1}}},
        {t38, {}, 0, V{{"years", 25}, {"realms", 7}}},
        {t38, {}, 0, V{{"years", 1}, {"realms", 2}}},
    };
    CHECK(render_c("k37.db", cases) ==
          std::vector<std::string>{
              "Plague broke out in Glathenddell and spread to 2 other settlements; 1,020 died in all.",
              // reached 1: P3-05 r14 22.1, "in all" whenever another settlement was reached
              "Plague broke out in Glathenddell and spread to one other settlement; 900 died in all.",
              "Plague broke out in Glathenddell; 120 died.",
              "Plague broke out in Glathenddell and spread to 3 other settlements; 1,084 died in all.",
              "For 1,924 years, from 5999 to 5999, the land was quiet; one realm held it.",
              "For 25 years, from 5999 to 5999, the land was quiet, and 7 realms kept the peace.",
              "For 1 year, from 5999 to 5999, the land was quiet, and 2 realms kept the peace.",
          });
}

TEST_CASE("P3-04 r12 20.1, 20.6 item 1: the stub world geo-seed-42-512 has one quiet_stretch per quiet row") {
    check_quiet_stretches(tables_of_world(open_world(stub_world_of(golden().world("geo-seed-42-512")))));
}

}  // TEST_SUITE

TEST_SUITE("history-slow" * doctest::skip()) {

TEST_CASE("P3-04 r12 20.1, 20.6 item 1: every stub world case and both ForcedTension cases: quiet stretches") {
    for (const GoldenWorld& g : golden().worlds) {
        if (g.tension == "none") {
            CAPTURE(g.name);
            check_quiet_stretches(tables_of_world(open_world(stub_world_of(g))));
        }
    }
    const GoldenWorld& base = golden().world("example-42-1024");
    const db::WorldReader part = open_world(world_of(base, 1, "chronology"));
    const eng::HistoryInputs in = eng::inputs_from_world(part, shipped());
    for (const auto& [p, v] : {std::pair<std::int64_t, std::int64_t>{500, 1000}, {1, 5000}}) {
        CAPTURE(p);
        Config cfg;
        cfg.handler = eng::make_stub();
        cfg.catalogue = eng::catalogue_of(shipped());
        cfg.add_tension(eng::forced_tension(p, v, 1));
        check_quiet_stretches(eng::run_engine(shipped(), in, cfg.get()));
    }
}

}  // TEST_SUITE
