// Spec P3-04 r19 section 22 (P5-01, generator 0.21.0), tests 22.4:
//   22.1  the numbers of one clause of a template (cut at the literal "; " and ", ") render all as words when every
//         number of the clause is from 1 to 9, else all in digits; the seven rows of 22.1's table with the shipped
//         templates; one number of each range as 21.1; 2 and 10, 2 and 0, 2 and 3; "; " and ", " inside a ?-word
//         do not cut, "," without a space and ";" at the end do not cut, " and " is no cut;
//   22.2  kinds 41 claim_dropped and 42 refuge_hidden in the compiled catalogue; their shipped entries;
//         template_problem accepts both templates and refuses {@settlement} in kind 41; their sentences;
//   22.3  flare_min_years in SC6 (the stub engine with ForcedTension, Q 25): a flare-up of 5 years before the
//         final phase is skipped at m 10 and the quiet row runs to the interval's end; kept at m 1 and without the
//         field; a crossing with 40 years left gives min(F, 40); at E 2003 / 2004 the scaled m is 3 (2 skipped, 3
//         kept); engine_flare_min and the field's range with their messages; every flare-up row at least m long;
//   P3-05 r18 24  became_city names its region, "The" lower-cased mid-sentence.
// Expected texts are the spec's, written by hand; the schedules were checked by the reference
// (reference/p3_engine_reference.py check_render_r19, check_flare_r19). Written from the spec before the
// implementation (test-writer rule).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "p3_04_engine_golden_support.hpp"
#include "p3_04_engine_support.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/history/schedule.hpp"

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
    for (const auto& r : k.roles) {
        out += std::to_string(r.seq) + ":" + r.role + ":" + std::to_string(static_cast<int>(r.type)) + ":" +
               std::to_string(r.min_count) + "-" + std::to_string(r.max_count) + ";";
    }
    return out;
}

// One event to render (as history_engine_r17_test.cpp): a kind of the world's catalogue by key, or a test kind with
// `templ` and the values `vals` (roles civilization 1 and settlement 1); participants (role seq, entity type, id).
struct Ev {
    std::string kind;
    std::string templ;
    std::vector<std::pair<std::string, std::int64_t>> vals;
    std::vector<std::tuple<std::int64_t, std::int64_t, std::int64_t>> parts;
    std::int64_t year = 5999;
    std::int64_t end_year = -1;  // -1: the year
};

// A copy of geo-seed-42-512 with the events appended in the last phase; civilizations 1 and 2 named The Snat Horde
// and The Stinrild League, settlements 1 to 3 Drutnot Hole, Nutskrank and The Grey Fort, the events' region (that
// of settlement 1) King's Fold, unless region_name says otherwise.
std::vector<std::string> render_all(const std::string& file, const std::vector<Ev>& evs,
                                    const std::string& region_name = "King's Fold") {
    using namespace engine_golden;
    static dbtest::TempDir dir("engine-r19");
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
        "UPDATE region SET name = " + q(region_name) + " WHERE id = " + std::to_string(region) + ";"
        // P5-12: the region may be renamed in the world; the appended events come after its renamings (N(r, e),
        // P3-04 r21 23.1), so its region_name rows carry the test's name too
        "UPDATE region_name SET name = " + q(region_name) + " WHERE region = " + std::to_string(region) + ";";
    std::vector<std::int64_t> ids;
    for (std::size_t i = 0; i < evs.size(); ++i) {
        const Ev& ev = evs[i];
        std::string k;
        if (ev.kind.empty()) {
            k = std::to_string(++kid);
            sql += "INSERT INTO event_kind VALUES (" + k + ", " + q("test_r19_" + std::to_string(i)) + ", 0, 1, 1, 30, " +
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
        const std::int64_t end_year = ev.end_year < 0 ? ev.year : ev.end_year;
        sql += "INSERT INTO event VALUES (" + e + ", " + std::to_string(ev.year) + ", " + std::to_string(end_year) +
               ", " + k + ", " + std::to_string(phase) + ", " + std::to_string(region) + ", " +
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
        if (!ev.kind.empty() && caused.at(ev.kind) == 1) sql += "INSERT INTO event_cause VALUES (" + e + ", 1);";
        ids.push_back(eid);
    }
    const std::string path = dir.file(file);
    dbtest::write_file(path, dbtest::read_file(src));
    const auto s = worldgen::db::testing::execute_on_file(path, sql);
    REQUIRE_MESSAGE(s.ok(), "SQL rejected");
    const db::WorldReader r = open_world(path);
    const auto err = eng::view_error(r);
    REQUIRE_MESSAGE(!err.has_value(), "HistoryView::load refused the test world: " << (err ? err->message : ""));
    std::vector<std::string> out;
    for (const std::int64_t id : ids) out.push_back(eng::render(r, id));
    return out;
}

// A test kind with two values a and b.
Ev two(const std::string& templ, std::int64_t a, std::int64_t b) { return Ev{"", templ, {{"a", a}, {"b", b}}, {}}; }

bool ends_with(const std::string& s, const std::string& tail) {
    return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

// --- 22.3: the stub engine with ForcedTension(period, 5000, 1), one race, normal ---------------------------------

using PhaseRows = std::vector<std::tuple<std::int64_t, std::int64_t, std::int64_t>>;  // kind, start, end

PhaseRows flare_run(std::int64_t span, std::int64_t period, std::int64_t m, bool with_field = true) {
    using namespace engine_test;
    EngineTuning tun;
    tun.flare_min_years = m;
    const wd::ContentSet c = with_field ? with_engine(tun) : with_engine_no_flare_min(tun);
    const eng::HistoryInputs in = inputs_for({0}, span);
    Config cfg;
    cfg.handler = eng::make_stub();
    cfg.catalogue = eng::catalogue_of(c);
    cfg.add_tension(eng::forced_tension(period, 5000, 1));
    const Tables t = eng::run_engine(c, in, cfg.get());
    PhaseRows out;
    for (const Row& ph : t.phase) out.emplace_back(at(ph, 2), at(ph, 3), at(ph, 4));
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 r19 22.1: the seven rows of 22.1's table with the shipped templates") {
    using P = std::tuple<std::int64_t, std::int64_t, std::int64_t>;
    const std::int64_t CIV = 3, STL = 4;
    Ev quiet{"quiet_stretch", "", {{"years", 683}, {"realms", 4}}, {P{1, CIV, 1}, P{1, CIV, 2}, P{1, CIV, 3}}};
    quiet.year = 5000;
    quiet.end_year = 5682;
    const std::vector<Ev> evs = {
        {"battle", "", {{"winner_dead", 7}, {"loser_dead", 24}}, {P{1, CIV, 1}, P{2, CIV, 2}}},
        {"battle", "", {{"winner_dead", 3}, {"loser_dead", 8}}, {P{1, CIV, 1}, P{2, CIV, 2}}},
        {"battle", "", {{"winner_dead", 1}, {"loser_dead", 24}}, {P{1, CIV, 1}, P{2, CIV, 2}}},
        {"sacked", "", {{"dead", 8}, {"survivors", 19}}, {P{1, CIV, 1}, P{2, CIV, 2}}},
        {"plague_outbreak", "", {{"dead", 30}, {"population", 100}, {"total", 1084}, {"reached", 2}},
         {P{1, STL, 1}, P{2, CIV, 1}}},
        quiet,
        {"settlement_abandoned_to", "", {{"settled", 12}, {"lost", 3}}, {P{1, STL, 1}, P{2, CIV, 1}, P{3, STL, 2}}},
    };
    const std::vector<std::string> got = render_all("table.db", evs);
    REQUIRE(got.size() == 7U);
    CHECK(ends_with(got[0], "; 24 of the defeated and 7 of the victors fell."));
    CHECK(ends_with(got[1], "; eight of the defeated and three of the victors fell."));
    CHECK(ends_with(got[2], "; 24 of the defeated and one of the victors fell."));
    CHECK(ends_with(got[3], "; 8 died and 19 fled."));
    CHECK(got[4] == "Plague broke out in Drutnot Hole and spread to two other settlements; 1,084 died in all.");
    CHECK(got[5] == "For 683 years, from 5000 to 5682, the land was quiet, and four realms kept the peace.");
    CHECK(ends_with(got[6], " 12 survivors went to Nutskrank, and 3 were lost."));
}

TEST_CASE("P3-04 r19 22.1, 22.4 item 1: one number per clause as 21.1; the clause rule on hand-made templates") {
    std::vector<Ev> evs;
    for (const std::int64_t v : {0, 1, 9, 10, 1000, -3}) evs.push_back(Ev{"", "{a} souls", {{"a", v}}, {}});
    const std::vector<std::pair<std::string, std::pair<std::int64_t, std::int64_t>>> cases = {
        {"{a} and {b}", {2, 10}},                       // 2 10
        {"{a} and {b}", {2, 0}},                        // 2 0
        {"{a} and {b}", {2, 3}},                        // two three
        {"{a}, {b}", {2, 10}},                          // a cut: two, 10
        {"{a}; {b}", {2, 10}},                          // a cut: two; 10
        {"{a},{b}", {2, 10}},                           // no space: no cut
        {"{a};{b}", {2, 10}},                           // no space: no cut
        {"{a} and {b};", {2, 3}},                       // ";" at the end
        {"{a?none|one|#}{b?|x|; #}", {2, 10}},          // "; " inside a ?-word: one clause
        {"{a?none|one|#}{b?|x|, #}", {2, 10}},          // ", " inside a ?-word: one clause
        {"{a?none|one|#} and {b?none|one|#}", {2, 10}},
        {"{a?none|one|# and #}; {b}", {4, 10}},         // two #s of one chosen word
        {"{a?none|one|# fell} and {b}", {12, 3}},
        {"{a?none|single|#} and {b}", {1, 10}},         // the chosen word has no #: no number
        {"{a?none|single|#} and {b}", {0, 3}},
        {"{a?one|many} and {b}", {12, 3}},              // a two-word form: no number
        {"In {@year} {a} fell", {3, 0}},                // a year is no number
    };
    for (const auto& [t, v] : cases) evs.push_back(two(t, v.first, v.second));
    // P5-03 lead ruling 1 (0.24.0): a first byte a to z is capitalised
    const std::vector<std::string> want = {
        "0 souls", "One souls", "Nine souls", "10 souls", "1,000 souls", "-3 souls",
        "2 and 10", "2 and 0", "Two and three", "Two, 10", "Two; 10", "2,10", "2;10", "Two and three;", "2; 10",
        "2, 10", "2 and 10", "Four and four; 10", "12 fell and 3", "Single and 10", "None and three",
        "Many and three", "In 5999 three fell"};
    const std::vector<std::string> got = render_all("clauses.db", evs);
    REQUIRE(got.size() == want.size());
    for (std::size_t i = 0; i < want.size(); ++i) {
        CAPTURE(i);
        CHECK(got[i] == want[i]);
    }
}

TEST_CASE("P3-04 r19 22.2: kinds 41 claim_dropped and 42 refuge_hidden in the compiled catalogue") {
    const auto& c = eng::compiled();
    REQUIRE(c.kinds.size() == 62U);  // P3-04 r21 23.2 (0.22.0): kind 43 appended; P5-02 10.1: 44 to 55
    const auto& k41 = c.kinds[40];
    CHECK(k41.id == 41);
    CHECK(k41.key == "claim_dropped");
    CHECK(k41.caused);
    CHECK(k41.region_use == 1);
    CHECK(k41.settlement_use == 0);
    CHECK(roles_of(k41) == "1:claimant:3:1-1;2:holder:3:1-1;");
    CHECK(k41.values.empty());
    const auto& k42 = c.kinds[41];
    CHECK(k42.id == 42);
    CHECK(k42.key == "refuge_hidden");
    CHECK(k42.caused);
    CHECK(k42.region_use == 1);
    CHECK(k42.settlement_use == 1);
    CHECK(roles_of(k42) == "1:settlement:4:1-1;2:founder:3:1-1;");
    CHECK(k42.values.empty());
}

TEST_CASE("P3-04 r19 22.2, 22.4 item 2: the shipped entries of kinds 41 and 42, template_problem and the sentences") {
    using P = std::tuple<std::int64_t, std::int64_t, std::int64_t>;
    const auto c = eng::catalogue_of(shipped());
    const auto* k41 = c.find("claim_dropped");
    const auto* k42 = c.find("refuge_hidden");
    REQUIRE(k41 != nullptr);
    REQUIRE(k42 != nullptr);
    CHECK(k41->salience == 15);
    CHECK(k41->templ == "{claimant} let the claim against {holder} over {@region} lapse.");
    CHECK(k42->salience == 10);
    CHECK(k42->templ == "{@settlement} lay hidden from {@year} to {@end_year}.");
    CHECK_FALSE(eng::template_problem(*k41, k41->templ).has_value());
    CHECK_FALSE(eng::template_problem(*k42, k42->templ).has_value());
    CHECK(eng::template_problem(*k41, "{claimant} {@settlement}") ==
          std::optional<std::string>("{@settlement} needs a kind with a settlement at byte 11"));
    CHECK_FALSE(eng::template_problem(*k42, "{settlement} of {founder} in {@region}.").has_value());
    Ev rh{"refuge_hidden", "", {}, {P{1, 4, 1}, P{2, 3, 1}}};
    rh.year = 5891;
    rh.end_year = 5990;
    const std::vector<std::string> got =
        render_all("k41.db", {Ev{"claim_dropped", "", {}, {P{1, 3, 1}, P{2, 3, 2}}}, rh}, "Bellringer's Forest");
    REQUIRE(got.size() == 2U);
    CHECK(got[0] == "The Snat Horde let the claim against the Stinrild League over Bellringer's Forest lapse.");
    CHECK(got[1] == "Drutnot Hole lay hidden from 5891 to 5990.");
}

TEST_CASE("P3-05 r18 24: became_city names its region without the figure; The lower-cased mid-sentence") {
    using P = std::tuple<std::int64_t, std::int64_t, std::int64_t>;
    const auto c = eng::catalogue_of(shipped());
    const auto* k9 = c.find("became_city");
    REQUIRE(k9 != nullptr);
    CHECK(k9->salience == 45);
    CHECK(k9->templ == "{settlement} in {@region} grew into a city.");
    CHECK_FALSE(eng::template_problem(*k9, k9->templ).has_value());
    const std::vector<std::string> got =
        // roles settlement and civilization (both 1 to 1): the view refuses an event without its civilization
        render_all("city.db", {Ev{"became_city", "", {{"population", 8017}}, {P{1, 4, 1}, P{2, 3, 1}}},
                               Ev{"became_city", "", {{"population", 8017}}, {P{1, 4, 3}, P{2, 3, 1}}}},
                   "The Holt of Yews");
    REQUIRE(got.size() == 2U);
    CHECK(got[0] == "Drutnot Hole in the Holt of Yews grew into a city.");
    CHECK(got[1] == "The Grey Fort in the Holt of Yews grew into a city.");
}

TEST_CASE("P3-04 r19 22.3: a flare-up of 5 years before the final phase is skipped at flare_min_years 10") {
    // E 6006, one race, normal: opening 0-149, the quiet interval 150-5554, final 5555-6005; ForcedTension(5550)
    // fires only at the step ending in 5550, which leaves 5 years (22.4 item 3).
    const PhaseRows m10 = flare_run(6006, 5550, 10);
    CHECK(m10 == PhaseRows{{1, 0, 149}, {2, 150, 5554}, {4, 5555, 6005}});
    const PhaseRows m1 = flare_run(6006, 5550, 1);
    CHECK(m1 == PhaseRows{{1, 0, 149}, {2, 150, 5549}, {3, 5550, 5554}, {4, 5555, 6005}});
    // Hand-made tuning (tuning built in code, 22.3): flare_min_years 1, as before. The field is required in data
    // (P3-01 r15 3.8), so a record without it is refused content, not hand-made tuning; the default is that of
    // the tuning built in code.
    CHECK(worldgen::history::EngineTuning{}.flare_min_years == 1);
}

TEST_CASE("P3-04 r19 22.3: a crossing with 40 years left gives min(F, 40); the scaled minimum at E 2003, 2004 is 3") {
    // E 6017: the quiet interval 150-5564; ForcedTension(5525): 40 years left, F 50
    CHECK(flare_run(6017, 5525, 10) == PhaseRows{{1, 0, 149}, {2, 150, 5524}, {3, 5525, 5564}, {4, 5565, 6016}});
    // E 2003: the quiet interval 50-1851; ForcedTension(1850) leaves 2 years: m = max(1, (10 x 2003 + 3000) div 6000)
    // = 3, so no flare-up; at m 1 a 2-year one
    CHECK(flare_run(2003, 1850, 10) == PhaseRows{{1, 0, 49}, {2, 50, 1851}, {4, 1852, 2002}});
    CHECK(flare_run(2003, 1850, 1) == PhaseRows{{1, 0, 49}, {2, 50, 1849}, {3, 1850, 1851}, {4, 1852, 2002}});
    // E 2004: 3 years left: kept
    CHECK(flare_run(2004, 1850, 10) == PhaseRows{{1, 0, 49}, {2, 50, 1849}, {3, 1850, 1852}, {4, 1853, 2003}});
}

TEST_CASE("P3-04 r19 22.3 invariant: every flare-up row is at least the scaled flare_min_years long") {
    for (const std::int64_t span : {2000, 2003, 4000, 6000, 6006, 9000}) {
        const std::int64_t m = std::max<std::int64_t>(1, (10 * span + 3000) / 6000);
        for (const std::int64_t period : {25, 50, 75, 125, 175, 333, 1000}) {
            CAPTURE(span);
            CAPTURE(period);
            for (const auto& [kind, a, b] : flare_run(span, period, 10)) {
                if (kind == 3) CHECK(b - a + 1 >= m);
            }
        }
    }
}

TEST_CASE("P3-04 r19 22.3, P3-01 r15 3.8: engine_flare_min and the range of flare_min_years, with their messages") {
    using namespace engine_test;
    const eng::HistoryInputs in = inputs_for({0, 2074}, 6000);
    auto stage_error = [&](const wd::ContentSet& c) { return fatal_of([&] { (void)eng::history_stage(c, in); }); };
    EngineTuning t;
    t.flare_min_years = 30;  // = sparse flare_years: passes
    CHECK(stage_error(with_engine(t)) == "<no error>");
    t.flare_min_years = 31;
    CHECK(stage_error(with_engine(t)) == "history: tuning: sparse: flare_min_years must not be above flare_years");
    t.flare_min_years = 51;  // above sparse and normal: the first level named
    CHECK(stage_error(with_engine(t)) == "history: tuning: sparse: flare_min_years must not be above flare_years");
    t = EngineTuning{};
    t.sparse.flare_years = 80;
    t.normal.flare_years = 80;
    t.flare_min_years = 76;  // only dense (75) fails
    CHECK(stage_error(with_engine(t)) == "history: tuning: dense: flare_min_years must not be above flare_years");
    t = EngineTuning{};
    t.flare_min_years = 0;
    CHECK(stage_error(with_engine(t)) == "history: tuning: flare_min_years outside its schema range");
    t.flare_min_years = 1001;
    CHECK(stage_error(with_engine(t)) == "history: tuning: flare_min_years outside its schema range");
    // the named check comes after engine_quiet_min (22.3)
    t = EngineTuning{};
    t.flare_min_years = 31;
    t.quiet_min_years = 51;
    CHECK(stage_error(with_engine(t)) == "history: tuning: quiet_min_years must not be above 2 x quiet_step_years");
}

}  // TEST_SUITE
