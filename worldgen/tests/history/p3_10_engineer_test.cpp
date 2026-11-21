// Engineer-side checks of the P3-10 change (generator 0.19.0), written by the
// generator engineer while implementing; the change's tests proper are the
// test writer's (docs/specs/p3-10-followups.md section 7).

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/names.hpp"
#include "worldgen/history/schedule.hpp"
#include "worldgen/stages/regions.hpp"

#include "p3_04_engine_support.hpp"

namespace {

namespace wh = worldgen::history;
namespace ws = worldgen::stages;

const wh::KindDef& kind(std::string_view key) {
    const wh::KindDef* k = wh::compiled_event_catalogue().find(key);
    REQUIRE(k != nullptr);
    return *k;
}

}  // namespace

TEST_CASE("P3-10 engineer: kinds 37 and 38 (P3-04 r12 20.4)") {
    const wh::KindDef& outbreak = kind("plague_outbreak");
    CHECK(outbreak.id == 37);
    CHECK(outbreak.values == std::vector<std::string>{"dead", "population", "total", "reached"});
    const wh::KindDef& stretch = kind("quiet_stretch");
    CHECK(stretch.id == 38);
    CHECK(stretch.region_use == 0);
    CHECK(stretch.settlement_use == 0);
    REQUIRE(stretch.roles.size() == 1U);
    CHECK(stretch.roles[0].role == "realm");
    CHECK(stretch.roles[0].min_count == 1);
    CHECK(stretch.roles[0].max_count == 0);
}

TEST_CASE("P3-10 engineer: the {@causes:...} grammar (P3-04 r12 20.2)") {
    const wh::KindDef& k = kind("settlement_abandoned");
    CHECK_FALSE(wh::template_problem(k, "{@causes:famine=famine,plague=plague|Weakened by |, }{settlement} fell."));
    CHECK_FALSE(wh::template_problem(k, "{@causes:no_such_kind=x||}{settlement}."));  // keys need not be kinds
    CHECK_FALSE(wh::template_problem(k, "{@causes:famine=a b|x|}{settlement}."));
    for (const char* bad : {"{@causes:|a|b}", "{@causes:famine=|a|b}", "{@causes:Famine=x|a|b}",
                            "{@causes:famine=x,famine=y|a|b}", "{@causes:famine=x|a}", "{@causes:famine=x|a|b|c}",
                            "{@causes:famine=x,|a|b}", "{@causes:famine|a|b}", "{@causes:famine=x?|a|b}",
                            "{@causes:famine=x|a?|b}"}) {
        const std::optional<std::string> why = wh::template_problem(k, bad);
        REQUIRE(why.has_value());
        CHECK(why->rfind("unknown placeholder '{", 0) == 0);
    }
}

TEST_CASE("P3-10 engineer: name cores (P3-02 r10 12.2)") {
    CHECK(wh::name_core("skat") == "skat");
    CHECK(wh::name_core("gork-skreeg") == "gorkskreeg");
    CHECK(wh::name_core("ka'zz") == "kazz");
    CHECK(wh::within_one_edit("skat", "skat"));
    CHECK(wh::within_one_edit("slig", "sligg"));
    CHECK(wh::within_one_edit("sligg", "slig"));
    CHECK(wh::within_one_edit("kuz", "kaz"));
    CHECK(wh::within_one_edit("kuz", "ku"));
    CHECK(wh::within_one_edit("kuz", "akuz"));
    CHECK_FALSE(wh::within_one_edit("kuz", "kazz"));
    CHECK_FALSE(wh::within_one_edit("nili", "muli"));
    CHECK_FALSE(wh::within_one_edit("ab", "ba"));
    wh::NameCores cores;
    cores.add("kuz");
    CHECK(cores.near("kuzz"));
    CHECK(cores.near("kaz"));
    CHECK(cores.near("kuz"));
    CHECK_FALSE(cores.near("kazz"));
}

TEST_CASE("P3-10 engineer: V7 stems (P1-06 r6 15.4)") {
    const ws::PlaceSlotValues values = {{3, "Flooded"}, {0, "Weald"}, {7, "the Floods"}, {1, "holt"}, {2, "Ash"}};
    CHECK(ws::region_name_stems(values) == std::vector<std::string>{"flood", "flood", "ash"});
}

// --- P3-04 r13 20.1 / 20.6 item 0: at most 64 quiet_stretch participants ---------------------


namespace {

// Founds `n` civilizations at the first awakening, each with one hamlet of
// 100 + id people (so the most populous are the highest ids).
class ManyHandler : public worldgen::history::AwakeningHandler {
public:
    explicit ManyHandler(std::int64_t n) : n_(n) {}
    std::int64_t awakening_place(const worldgen::history::StepContext&, std::int64_t) override { return 1; }
    void awaken(worldgen::history::StepContext& ctx, std::int64_t race, std::int64_t awakening_event) override {
        namespace h = worldgen::history;
        if (!ctx.state().civs().empty()) {
            return;
        }
        std::int64_t culture = 0;
        for (const auto& r : ctx.inputs().races) {
            culture = r.id == race ? r.culture : culture;
        }
        for (std::int64_t c = 1; c <= n_; ++c) {
            h::EventSpec civ;
            civ.kind = "civ_founded";
            civ.year = ctx.year();
            civ.end_year = ctx.year();
            civ.region = 1;
            civ.participants = {{"civilization", h::EntityType::civilization, c}, {"race", h::EntityType::race, race}};
            civ.causes = {awakening_event};
            const std::int64_t e = ctx.append(civ);
            REQUIRE(ctx.state().found_civilization(race, culture, 1, "Civ " + std::to_string(c), e) == c);
            h::EventSpec set;
            set.kind = "settlement_founded";
            set.year = ctx.year();
            set.end_year = ctx.year();
            set.region = 1;
            set.settlement = c;
            set.participants = {{"settlement", h::EntityType::settlement, c}, {"founder", h::EntityType::civilization, c}};
            set.values = {100 + c};
            set.causes = {e};
            const std::int64_t es = ctx.append(set);
            REQUIRE(ctx.state().found_settlement((c - 1) % 8, (c - 1) / 8, 1, c, h::SettlementKind::hamlet, 100 + c,
                                                 "Town " + std::to_string(c), es) == c);
            ctx.state().set_capital(c, c);
        }
    }

private:
    std::int64_t n_;
};

}  // namespace

TEST_CASE("P3-10 engineer: quiet_stretch with 70 living realms lists the 64 most populous (P3-04 r13 20.1)") {
    namespace h = worldgen::history;
    ManyHandler handler(70);
    h::EmptyInterventionSource none;
    const h::EventCatalogue catalogue = h::event_catalogue(engine_test::shipped());
    h::EngineConfig cfg;
    cfg.awakening = &handler;
    cfg.interventions = &none;
    cfg.catalogue = &catalogue;
    const h::HistoryResult r =
        h::run_engine(engine_test::shipped(), engine_test::inputs_for({0}, 2000, "normal", {"elf"}), cfg);
    const std::int64_t kind = catalogue.find("quiet_stretch")->id;
    std::int64_t stretches = 0;
    for (const auto& e : r.events) {
        if (e.kind != kind) {
            continue;
        }
        ++stretches;
        std::vector<std::int64_t> ids;
        for (const auto& p : r.event_participants) {
            if (p.event == e.id) {
                ids.push_back(p.entity_id);
            }
        }
        REQUIRE(ids.size() == 64U);
        for (std::size_t i = 0; i < ids.size(); ++i) {
            CHECK(ids[i] == static_cast<std::int64_t>(i) + 7);  // ids 7 to 70, ascending
        }
        std::vector<std::int64_t> values;
        for (const auto& v : r.event_values) {
            if (v.event == e.id) {
                values.push_back(v.value);
            }
        }
        REQUIRE(values.size() == 2U);
        CHECK(values[1] == 70);
    }
    CHECK(stretches >= 1);
}

// --- P3-10 second round (r3; P3-04 r15, P3-06 r17) --------------------------------------------------

namespace {

std::string fatal_text(const auto& f) {
    try {
        f();
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return {};
}

}  // namespace

TEST_CASE("P3-10 r3 engineer: engine_quiet_min and the conflict seam checks (P3-04 r15 4.1, P3-06 r17 22.1, 22.7)") {
    namespace h = worldgen::history;
    h::EngineTuning e = h::engine_tuning_from(engine_test::shipped());
    CHECK(e.quiet_min_years == 50);
    CHECK(fatal_text([&] { h::check_engine_tuning(e); }).empty());
    e.quiet_min_years = 2 * e.quiet_step_years + 1;
    CHECK(fatal_text([&] { h::check_engine_tuning(e); }) ==
          "history: tuning: quiet_min_years must not be above 2 x quiet_step_years");
    h::ConflictTuning t = h::conflict_tuning_from(engine_test::shipped());
    CHECK(t.quarrel_min_settlements == 2);
    CHECK(fatal_text([&] { h::check_conflict_tuning(t); }).empty());
    t.strain_max = 1000;  // declare_min + declare_span (900) still fit
    t.quarrel_strain = 1001;
    CHECK(fatal_text([&] { h::check_conflict_tuning(t); }) ==
          "history: tuning: quarrel_strain must not be above strain_max");
    t = h::conflict_tuning_from(engine_test::shipped());
    t.quarrel_min_settlements = 1001;
    CHECK(fatal_text([&] { h::check_conflict_tuning(t); }) ==
          "history: tuning: conflict.quarrel_min_settlements outside its schema range");
    CHECK(h::ConflictTuning{}.quarrel_min_settlements == 1);  // a hand-made tuning: the rule off
    CHECK(h::ConflictRace{}.hidden_years == 300);
}

TEST_CASE("P3-10 r3 engineer: no quiet_stretch shorter than quiet_min_years (P3-04 r15 20.1 item 1a)") {
    namespace h = worldgen::history;
    ManyHandler handler(3);
    h::EmptyInterventionSource none;
    const h::EventCatalogue catalogue = h::event_catalogue(engine_test::shipped());
    h::EngineConfig cfg;
    cfg.awakening = &handler;
    cfg.interventions = &none;
    cfg.catalogue = &catalogue;
    const h::HistoryResult r =
        h::run_engine(engine_test::shipped(), engine_test::inputs_for({0}, 2000, "normal", {"elf"}), cfg);
    const std::int64_t kind = catalogue.find("quiet_stretch")->id;
    std::int64_t stretches = 0;
    for (const auto& e : r.events) {
        if (e.kind == kind) {
            ++stretches;
            CHECK(e.end_year - e.year + 1 >= 50);
        }
    }
    CHECK(stretches >= 1);
}

TEST_CASE("P3-10 r3 engineer: the end check tests aim and the lost goal before exhaustion (P3-06 21.6, 22.5)") {
    namespace h = worldgen::history;
    using D = h::EndDecision;
    const std::int64_t a = 3, b = 7;
    // 1. Aim: the attacker wins whatever the exhaustion and the score; a peace when b is exhausted.
    CHECK(h::end_decision(a, b, true, false, true, false, b, false) == D{true, a, false});
    CHECK(h::end_decision(a, b, true, false, false, true, 0, false) == D{true, a, true});
    CHECK(h::end_decision(a, b, true, false, false, false, b, false) == D{true, a, false});
    // 2. The goal lost: by the score, a peace when the score's loser is exhausted, a truce at 0.
    CHECK(h::end_decision(a, b, false, false, false, true, a, false) == D{true, a, true});
    CHECK(h::end_decision(a, b, false, false, true, false, a, false) == D{true, a, false});
    CHECK(h::end_decision(a, b, false, false, true, false, b, false) == D{true, b, true});
    CHECK(h::end_decision(a, b, false, false, true, true, 0, false) == D{true, 0, false});
    CHECK(h::end_decision(a, b, false, false, false, false, b, false) == D{true, b, false});
    // 3. b still stands in G: exhaustion, then no front or the length; else the war goes on.
    CHECK(h::end_decision(a, b, false, true, true, false, b, false) == D{true, b, true});
    CHECK(h::end_decision(a, b, false, true, true, false, a, false) == D{true, a, false});
    CHECK(h::end_decision(a, b, false, true, false, true, a, false) == D{true, a, true});
    CHECK(h::end_decision(a, b, false, true, false, false, a, true) == D{true, a, false});
    CHECK(h::end_decision(a, b, false, true, false, false, a, false) == D{});
}
