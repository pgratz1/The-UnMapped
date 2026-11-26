// Spec P5-03a (docs/specs/p5-03a-relations-followups.md r1) tests 14 items 1 to 9, 12 and 13: the eleven new
// history.relations fields and undead_dead_permille with their bounds, absence and the checks relations_peace_cap
// and relations_settlers (10.1, 10.2); the formulas of 3.2, 5.1, 6, 8 and 9.3 on values computed by hand from the
// spec (the same values as the reference's check_units_025); the rules on the rows of the fixtures relations-small
// (with meetings by word after 0 years) and zero: the invariants of P5-03 19 as amended and 14 item 12 (a) to (c)
// (p5_03a_checks.hpp), the expulsion gap, the settlers' range, meetings by word, I0 with the new zero tuning; and
// the seams of 3.5 and 13 (credit, DispositionView::band and met) and A2 through the adapter
// (p5_03a_relations_api.hpp: those cases FAIL by name until UNMAPPED_P503A_API is defined at integration).
//
// The state of the subsystem (memories, contributions, strife and word clocks) is private, so 14 items 2 to 5 are
// checked by hand in the reference (p5_03_relations_reference.py check_stub_units_025, on stub worlds) and here
// through their visible effects: the rows, the turns' causes, the meetings' `sealed` value.
//
// Written from the spec before the implementation (test-writer rule).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../p3_01_history_support.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_03a_checks.hpp"
#include "p5_03a_formulas.hpp"
#include "p5_03a_relations_api.hpp"
#include "p5_03a_relations_data.hpp"
#include "worldgen/history/data_checks.hpp"

namespace {

namespace wd = worldgen::data;
namespace h = worldgen::history;
using history_data::Files;
using namespace history_test;

const std::vector<std::uint64_t>& stress_seeds() {
    static const std::vector<std::uint64_t> v = {0U, 1U, 2U, 3U};
    return v;
}

std::int64_t count_kind(const p503check::World& w, const std::string& key) {
    const p503check::Index x(w);
    return static_cast<std::int64_t>(x.events_of(key).size());
}

Files edited(const std::string& path, const std::string& from, const std::string& to) {
    Files f = p503a::files_025();
    std::string& t = f[path];
    const std::size_t at = t.find(from);
    REQUIRE_MESSAGE(at != std::string::npos, from << " not in " << path);
    t.replace(at, from.size(), to);
    return f;
}

Files without(const std::string& path, const std::string& name) {
    Files f = p503a::files_025();
    std::string& t = f[path];
    const std::size_t a = t.find("\n" + name + " = ");
    REQUIRE(a != std::string::npos);
    t.erase(a, t.find('\n', a + 1) - a);
    return f;
}

std::string line(const std::string& name, std::int64_t v) { return name + " = " + std::to_string(v); }

const wd::Record& record_of(const wd::ContentSet& c, const char* id) {
    const wd::Definition* d = p503check::definition(c, "history", id);
    REQUIRE(d != nullptr);
    return d->record;
}

void one_check_failure(const Files& f, const std::string& check, const std::string& message) {
    const Load l = load(f);
    INFO(dump(l));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 1U);
    CHECK(l.errors[0].path == "history/relations.toml");
    CHECK(l.errors[0].message == "check '" + check + "' failed for 'history.relations': " + message);
}

}  // namespace

TEST_SUITE("history") {

// --- 14 item 1: data ------------------------------------------------------------------------------------

TEST_CASE("P5-03a 10.1: history.relations: the 0.24.0 fields (shelter 90, settlers 50), then eleven new, in order") {
    const Load l = load(p503a::files_025());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    CHECK(h::check_history_data(*l.content).empty());
    const wd::Record& r = record_of(*l.content, "relations");
    REQUIRE(r.size() == p503a::relations_fields().size());
    REQUIRE(r.size() == 37U);   // 23 of P5-03 and 11 of 10.1
    for (std::size_t i = 0; i < r.size(); ++i) {
        CAPTURE(i);
        CHECK(r.name_at(i) == p503a::relations_fields()[i].name);
        CHECK(r.get_int(p503a::relations_fields()[i].name) == p503a::relations_fields()[i].shipped);
    }
    CHECK(r.name_at(22) == "expel_share_max");
    CHECK(r.get_int("shelter_memory") == 90);
    CHECK(r.get_int("conquest_settlers_permille") == 50);
}

TEST_CASE("P5-03a 10.1: each new field required and one step outside its range refused; the bounds load") {
    const std::string p = "history/relations.toml";
    for (const p503data::RelField& f : p503a::new_relations_fields()) {
        const std::string name(f.name);
        CAPTURE(name);
        const Load gone = load(without(p, name));
        REQUIRE_FALSE(gone.ok());
        CHECK(dump(gone).find("history.relations." + name) != std::string::npos);
        for (const std::int64_t v : {f.lo - 1, f.hi + 1}) {
            const Load e = load(edited(p, line(name, f.shipped), line(name, v)));
            REQUIRE_FALSE(e.ok());
            CHECK(dump(e).find("history.relations." + name) != std::string::npos);
        }
        // the bounds: peace_memory_max's 2000 and the span's 1000 fail the named checks (tested below)
        if (name != "peace_memory_max" && name != "conquest_settlers_span_permille") {
            CHECK(load(edited(p, line(name, f.shipped), line(name, f.lo))).ok());
            CHECK(load(edited(p, line(name, f.shipped), line(name, f.hi))).ok());
        }
    }
}

TEST_CASE("P5-03a 10.1: relations_peace_cap and relations_settlers at and one past their boundaries") {
    const std::string p = "history/relations.toml";
    CHECK(load(edited(p, "peace_memory_max = 300", "peace_memory_max = 500")).ok());   // = memory_max
    one_check_failure(edited(p, "peace_memory_max = 300", "peace_memory_max = 501"), "relations_peace_cap",
                      "peace_memory_max 501 is above memory_max 500");
    CHECK(load(edited(p, "conquest_settlers_span_permille = 400", "conquest_settlers_span_permille = 950")).ok());
    one_check_failure(edited(p, "conquest_settlers_span_permille = 400", "conquest_settlers_span_permille = 951"),
                      "relations_settlers", "conquest settlers 50 + 951 exceed 1000");
    one_check_failure(edited(p, "conquest_settlers_permille = 50", "conquest_settlers_permille = 601"),
                      "relations_settlers", "conquest settlers 601 + 400 exceed 1000");
}

TEST_CASE("P5-03a 10.2: undead_min_dead 1000 and undead_dead_permille int(0, 1000) 25 after overrun_people_ref") {
    const Load l = load(p503a::files_025());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const wd::Record& r = record_of(*l.content, "monsters");
    REQUIRE(r.size() >= 3U);
    CHECK(r.name_at(r.size() - 3U) == "overrun_people_ref");
    CHECK(r.name_at(r.size() - 2U) == "undead_dead_permille");
    CHECK(r.get_int("undead_dead_permille") == 28);
    CHECK(r.get_int("undead_min_dead") == 1000);
    const std::string p = "history/monsters.toml";
    CHECK_FALSE(load(without(p, "undead_dead_permille")).ok());
    for (const std::int64_t v : {-1, 1001}) CHECK_FALSE(load(edited(p, line("undead_dead_permille", 28), line("undead_dead_permille", v))).ok());
    for (const std::int64_t v : {0, 1000}) CHECK(load(edited(p, line("undead_dead_permille", 28), line("undead_dead_permille", v))).ok());
}

TEST_CASE("P5-03a 10.1, 10.2: the shipped game/data and both fixture roots hold the 0.25.0 fields") {
    for (const std::filesystem::path& root :
         {source_root() / "game" / "data", source_root() / "worldgen" / "tests" / "data" / "geo",
          source_root() / "worldgen" / "tests" / "data" / "geo-small"}) {
        CAPTURE(root.string());
        const Load l = load_disk(root);
        REQUIRE_MESSAGE(l.ok(), dump(l));
        CHECK(h::check_history_data(*l.content).empty());
        const wd::Record& r = record_of(*l.content, "relations");
        CHECK(r.size() == p503a::relations_fields().size());
        for (const p503data::RelField& f : p503a::new_relations_fields()) CHECK(r.find(std::string(f.name)) != nullptr);
        CHECK(record_of(*l.content, "monsters").find("undead_dead_permille") != nullptr);
    }
}

// --- the formulas ------------------------------------------------------------------------------------------

TEST_CASE("P5-03a 3.2: the peace gain: ticks, a quiet step, the cap below, at and above it") {
    CHECK(rel_a::peace_gain(0, 1, 60, 300) == 600);
    CHECK(rel_a::peace_gain(-20000, 25, 60, 300) == -5000);
    CHECK(rel_a::peace_gain(299700, 1, 60, 300) == 300000);   // up to the cap
    CHECK(rel_a::peace_gain(300000, 1, 60, 300) == 300000);   // at it
    CHECK(rel_a::peace_gain(310000, 1, 60, 300) == 310000);   // above it (by other sources): unchanged
    CHECK(rel_a::peace_gain(-7000, 100, 0, 300) == -7000);
    // 3.2's prose with the shipped values (60, cap 300, decay 20): a good-neutral pair (B -150) at peace reaches
    // D = 150 after 750 counted years; a neutral-evil pair (B -250) stays wary at +50
    std::int64_t m = 0, y = 0;
    while (rel_f::disposition(-150, m) < 150) {
        m = rel_a::peace_gain(rel_f::decay(m, 1, 20), 1, 60, 300);
        ++y;
    }
    CHECK(y == 750);
    m = 0;
    for (int i = 0; i < 5000; ++i) m = rel_a::peace_gain(rel_f::decay(m, 1, 20), 1, 60, 300);
    CHECK(rel_f::disposition(-250, m) == 50);
    // a quiet step of 25 years equals 25 ticks while below the cap
    std::int64_t a = -40000, b = -40000;
    for (int i = 0; i < 25; ++i) a = rel_a::peace_gain(a, 1, 60, 300);
    b = rel_a::peace_gain(b, 25, 60, 300);
    CHECK(a == b);
}

TEST_CASE("P5-03a 5.1: settlers 50 + pick(u, 401): 50 at u = 0, 450 at the largest u; a one-race town at 450") {
    CHECK(rel_a::settlers(50, 400, 0U) == 50);
    CHECK(rel_a::settlers(50, 400, ~std::uint64_t{0}) == 450);
    CHECK(rel_a::settlers(100, 0, ~std::uint64_t{0}) == 100);
    CHECK(rel_f::conquer({{1, 1000}}, 2, 450) == rel_f::Shares{{1, 550}, {2, 450}});
    CHECK(rel_f::conquest_share(200, 450) == 200 + 800 * 450 / 1000);
}

TEST_CASE("P5-03a 8 A1: a host of 1 taking 2000 newcomers keeps its ruling race at share 1") {
    const rel_f::Shares raw = rel_f::mix({{1, 1000}}, 1, {{2, 1000}}, 2000);
    CHECK(raw == rel_f::Shares{{2, 1000}});   // 6.3.2's split alone drops the ruling race
    CHECK(rel_a::keep_ruling(raw, 1) == rel_f::Shares{{1, 1}, {2, 999}});
    CHECK(rel_a::keep_ruling({{2, 500}, {3, 500}}, 1) == rel_f::Shares{{1, 1}, {2, 499}, {3, 500}});   // ties: smaller id
    CHECK(rel_a::keep_ruling({{1, 3}, {2, 997}}, 1) == rel_f::Shares{{1, 3}, {2, 997}});
    CHECK(rel_a::keep_ruling({{2, 1000}}, 5) == rel_f::Shares{{2, 999}, {5, 1}});
}

TEST_CASE("P5-03a 6: the undead threshold below and above undead_min_dead x 1000 div undead_dead_permille; with M") {
    // 1000 x 1000 div 25 = 40000: T is the floor up to P_W 40039, P_W x 25 div 1000 above
    CHECK(rel_a::undead_threshold(1000, 25, 0, 1000) == 1000);
    CHECK(rel_a::undead_threshold(1000, 25, 39999, 1000) == 1000);
    CHECK(rel_a::undead_threshold(1000, 25, 40000, 1000) == 1000);
    CHECK(rel_a::undead_threshold(1000, 25, 40040, 1000) == 1001);
    CHECK(rel_a::undead_threshold(1000, 25, 160000, 1000) == 4000);   // the spec's world of 160,000: as at 0.24.0
    CHECK(rel_a::undead_threshold(1000, 25, 160000, 1600) == 2500);
    CHECK(rel_a::undead_threshold(1000, 25, 40000, 500) == 2000);
    CHECK(rel_a::undead_threshold(1000, 25, 160000, 0) == 4000000);
    CHECK(rel_a::undead_threshold(1000, 0, 1000000, 1000) == 1000);
}

TEST_CASE("P5-03a 9.3: the summary's ruins: 15 all; 17 the union of 5 oldest, 5 largest, 5 latest; ties by id") {
    std::vector<rel_a::Ruin> r;
    for (std::int64_t i = 1; i <= 17; ++i) r.push_back({i, 1000 + 10 * i, 100 + (37 * i) % 17 * 10});
    const auto [ids, more] = rel_a::summary_ruins(r);
    // oldest 1 to 5, latest 13 to 17; sizes 100 + (37 i mod 17) x 10 = 100 + (3 i mod 17) x 10: the five largest
    // are i = 11 (260), 5 (250), 16 (240), 10 (230), 4 (220)
    CHECK(ids == std::vector<std::int64_t>{1, 2, 3, 4, 5, 10, 11, 13, 14, 15, 16, 17});
    CHECK(more == 5);
    const auto [ids15, more15] = rel_a::summary_ruins(std::vector<rel_a::Ruin>(r.begin(), r.begin() + 15));
    CHECK(ids15.size() == 15U);
    CHECK(more15 == 0);
    std::vector<rel_a::Ruin> same;
    for (std::int64_t i = 1; i <= 17; ++i) same.push_back({i, 500, 10});
    CHECK(rel_a::summary_ruins(same).first == std::vector<std::int64_t>{1, 2, 3, 4, 5, 13, 14, 15, 16, 17});
}

// --- rows of the fixtures (14 items 5 to 7, 12, 13) ------------------------------------------------------------

TEST_CASE("P5-03a 14 item 12: relations-small: P5-03's invariants as amended and (a), (b), (c), 10.4, 5.1 hold") {
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U, 7U}) {
        CAPTURE(seed);
        const p503check::World w = p503run::stage_fixture(seed, p503api::relations_small_content());
        CHECK(p503check::violations(w) == std::vector<std::string>{});
        CHECK(p503a_check::violations(w) == std::vector<std::string>{});
    }
}

TEST_CASE("P5-03a 14 item 12: the shipped data on generated worlds (seeds 42 and 0 at 512)") {
    for (const std::uint64_t seed : {42U, 0U}) {
        CAPTURE(seed);
        const p503check::World w = p503run::world_rows(p503run::world_path(seed, 512));
        CHECK(p503a_check::violations(w) == std::vector<std::string>{});
    }
}

TEST_CASE("P5-03a 4.2: meetings by word: sealed 1 only when no open meeting; meet_sealed_hops -1 never") {
    // relations-small meets by word after 0 years within 3 borders; with meet_hops -1 every meeting is by word
    const wd::ContentSet word_only =
        p503api::with_history(p503api::relations_small_content(), "relations", {{"meet_hops", p503api::V(-1)}});
    const wd::ContentSet never = p503api::with_history(p503api::relations_small_content(), "relations",
                                                       {{"meet_hops", p503api::V(-1)}, {"meet_sealed_hops", p503api::V(-1)}});
    const wd::ContentSet open_only =
        p503api::with_history(p503api::relations_small_content(), "relations", {{"meet_sealed_hops", p503api::V(-1)}});
    std::int64_t sealed_seen = 0;
    for (const std::uint64_t seed : stress_seeds()) {
        CAPTURE(seed);
        const p503check::World a = p503run::stage_fixture(seed, word_only);
        const p503check::Index xa(a);
        for (const std::int64_t e : xa.events_of("peoples_met")) {
            CHECK(xa.val(e, 2) == 1);
            ++sealed_seen;
        }
        CHECK(p503a_check::violations(a) == std::vector<std::string>{});
        CHECK(count_kind(p503run::stage_fixture(seed, never), "peoples_met") == 0);
        const p503check::World b = p503run::stage_fixture(seed, open_only);
        const p503check::Index xb(b);
        for (const std::int64_t e : xb.events_of("peoples_met")) CHECK(xb.val(e, 2) == 0);
    }
    CHECK(sealed_seen > 0);
}

TEST_CASE("P5-03a 5.3: no realm expels twice within expel_gap_years; gap 100000: at most one expulsion a realm") {
    const wd::ContentSet once =
        p503api::with_history(p503api::relations_small_content(), "relations", {{"expel_gap_years", p503api::V(100000)}});
    std::int64_t expelled = 0;
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U, 7U}) {
        CAPTURE(seed);
        const p503check::World w = p503run::stage_fixture(seed, once);
        const p503check::Index x(w);
        std::map<std::int64_t, std::int64_t> per;
        for (const std::int64_t e : x.events_of("people_expelled")) ++per[x.part(e, 2)];
        for (const auto& [c, k] : per) {
            CAPTURE(c);
            CHECK(k == 1);
        }
        expelled += static_cast<std::int64_t>(per.size());
        CHECK(p503a_check::violations(w) == std::vector<std::string>{});
    }
    CHECK(expelled > 0);
}

TEST_CASE("P5-03a 5.1: the settlers' span 0 gives the least share exactly on every owner change to another people") {
    const wd::ContentSet span0 = p503api::with_history(
        p503api::relations_small_content(), "relations",
        {{"conquest_settlers_span_permille", p503api::V(0)}, {"conquest_settlers_permille", p503api::V(600)}});
    for (const std::uint64_t seed : stress_seeds()) {
        CAPTURE(seed);
        CHECK(p503a_check::violations(p503run::stage_fixture(seed, span0)) == std::vector<std::string>{});
    }
}

TEST_CASE("P5-03a 10.1 (13): I0 with the 0.25.0 zero tuning: no event of kinds 57 to 62, no race_relation row") {
    for (const std::uint64_t seed : stress_seeds()) {
        CAPTURE(seed);
        const p503check::World w = p503run::stage_fixture(seed, p503api::zero_content());
        for (const char* k : {"peoples_met", "disposition_turned", "refugees_arrived", "settlement_mixed",
                              "standing_changed", "people_expelled"}) {
            CAPTURE(std::string(k));
            CHECK(count_kind(w, k) == 0);
        }
        CHECK(w.rows.race_relation.empty());
    }
}

// --- the seams (3.5, 13, A2) ------------------------------------------------------------------------------

TEST_CASE("P5-03a 8 A2: dispositions valid from construction: D = B, no hatred event, before the first step") {
    const wd::ContentSet& c = conflict_test::shipped();
    const eng::HistoryInputs in = p503run::stress_inputs(1, c);
    cfl::Conflict cp(c);
    const auto rel = p503a_api::constructed(*cp.peoples.sub, *cp.sub, c, in);
    REQUIRE(rel != nullptr);
    const p503check::World w = p503check::world_of_run(p503api::RTables{}, in, c);
    const p503check::Index x(w);
    for (const p503check::RaceInfo& a : w.races) {
        for (const p503check::RaceInfo& b : w.races) {
            if (!a.present || !b.present || b.id <= a.id) continue;
            CAPTURE(a.id);
            CAPTURE(b.id);
            CHECK(rel->disposition(a.id, b.id) == p503check::base_of(w, x, a.id, b.id));
            CHECK(rel->hatred_event(a.id, b.id) == 0);
        }
    }
}

TEST_CASE("P5-03a 13(b): DispositionView::band and met after a run agree with the rows; band of B before a meeting") {
    const wd::ContentSet& c = p503api::relations_small_content();
    const eng::HistoryInputs in = p503run::stress_inputs(1, c);
    const p503api::RelConfig cfg(c, {});
    const p503check::World w = p503check::world_of_run(cfg.run(c, in), in, c);
    const p503check::Index x(w);
    const auto prows = p503check::pair_rows(w);
    const h::DispositionView& v = *cfg.relations;
    const std::int64_t hm = p503api::tuning_int(c, "relations", "hatred_max");
    const std::int64_t fm = p503api::tuning_int(c, "relations", "friend_min");
    for (const p503check::RaceInfo& a : w.races) {
        for (const p503check::RaceInfo& b : w.races) {
            if (!a.present || !b.present || b.id <= a.id) continue;
            CAPTURE(a.id);
            CAPTURE(b.id);
            const auto it = prows.find({a.id, b.id});
            CHECK(p503a_api::view_met(v, a.id, b.id) == (it != prows.end()));
            CHECK(p503a_api::view_met(v, b.id, a.id) == (it != prows.end()));
            const std::int64_t want = it != prows.end() ? p503check::at(*it->second.back(), 4)
                                                        : rel_f::plain_band(p503check::base_of(w, x, a.id, b.id), hm, fm);
            CHECK(p503a_api::view_band(v, a.id, b.id) == want);
            CHECK(p503a_api::view_band(v, b.id, a.id) == want);
        }
    }
}

TEST_CASE("P5-03a 3.5: credit after a run: nothing on an unmet pair; units x 1000 clamped on a met one") {
    const wd::ContentSet& c = p503api::relations_small_content();
    const eng::HistoryInputs in = p503run::stress_inputs(1, c);
    const p503api::RelConfig cfg(c, {});
    const p503check::World w = p503check::world_of_run(cfg.run(c, in), in, c);
    h::RelationsSubsystem& r = *cfg.relations;
    const std::int64_t mmax = p503api::tuning_int(c, "relations", "memory_max");
    const std::int64_t mmin = p503api::tuning_int(c, "relations", "memory_min");
    bool met_tested = false, unmet_tested = false;
    for (const p503check::RaceInfo& a : w.races) {
        for (const p503check::RaceInfo& b : w.races) {
            if (!a.present || !b.present || b.id <= a.id) continue;
            CAPTURE(a.id);
            CAPTURE(b.id);
            const std::int64_t d0 = r.disposition(a.id, b.id), m0 = r.memory(a.id, b.id);
            if (!r.met(a.id, b.id)) {
                p503a_api::credit(r, a.id, b.id, 100, 1);
                CHECK(r.disposition(a.id, b.id) == d0);
                CHECK(r.memory(a.id, b.id) == 0);
                unmet_tested = true;
                continue;
            }
            p503a_api::credit(r, b.id, a.id, 7, 1);   // either race order
            CHECK(r.memory(a.id, b.id) == std::clamp(m0 + 7000, mmin * 1000, mmax * 1000));
            p503a_api::credit(r, a.id, b.id, -100000, 1);
            CHECK(r.memory(a.id, b.id) == mmin * 1000);
            CHECK(r.disposition(a.id, b.id) == r.base(a.id, b.id) + mmin);
            p503a_api::credit(r, a.id, b.id, 100000, 1);
            CHECK(r.memory(a.id, b.id) == mmax * 1000);
            met_tested = true;
        }
    }
    CHECK(met_tested);
    (void)unmet_tested;
}

}  // TEST_SUITE
