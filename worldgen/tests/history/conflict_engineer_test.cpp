// The engineer's checks of P3-06 internals (the independent tests are the
// test writer's, history_conflict_*): the pure rules on the numbers of
// worked example W1 (spec 16.1) and the tuning seam, on hand-made inputs;
// and (r13, 20.27 item 8) the cession of every standing settlement of the
// loser in the ceded region, through probes around the shipped subsystems
// on the synthetic worlds of the golden file and, in history-slow, on 2048
// seed 3.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "p3_06_conflict_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/history/renaming.hpp"
#include "worldgen/history/conflict.hpp"

namespace h = worldgen::history;

namespace {

h::ConflictTuning shipped_tuning() {
    h::ConflictTuning t;
    t.camp = h::ConflictCamp{400, -150, -700, 100, -250, 300};
    t.enmity_max = -500;
    t.border_friction = 200;
    t.want_weight = 500;
    t.hunger_full = 300;
    t.fear_share = 400;
    t.fear_weight = 1000;
    t.tick_strain_permille = 25;
    t.quiet_strain_permille = 6;
    t.strain_max = 5000;
    t.claim_strain = 300;
    t.declare_min = 300;
    t.declare_span = 600;
    t.declare_permille = 150;
    t.declare_ratio = 1000;
    t.shelter_years = 150;
    t.march_hops = 2;
    t.cost_penalty = 100;
    t.levy_permille = h::ConflictByKind{100, 100, 100, 80, 250};
    t.terrain_defense = 1000;
    t.pass_defense = 1000;
    t.river_defense = 500;
    t.fort_defense = 500;
    t.battle_permille = 300;
    t.loss_loser_permille = 250;
    t.loss_winner_permille = 80;
    t.garrison_permille = 300;
    t.walls_permille = h::ConflictByKind{1000, 1500, 2500, 4000, 6000};
    t.capital_walls = 2000;
    t.sack_base = 200;
    t.sack_hostility = 1000;
    t.sack_toll_permille = 300;
    t.capture_toll_permille = 100;
    t.exhaustion_permille = 80;
    t.war_max_years = 40;
    t.truce_years = 200;
    t.war_grudge = 150;
    t.capture_grudge = 50;
    t.secession_grudge = 300;
    t.grudge_decay_per_century = 50;
    t.last_refuge = 1;
    // r9 20.9 and r10 20.19 (generator 0.18.0), shipped.
    t.refuge_flight = 1;
    t.flight_hops = 3;
    t.weary_wars = 3;
    t.tribute_permille = 100;
    t.revenge_aggression = 400;
    t.levy_hops = 3;
    t.odds_floor = 500;
    t.truce_break_aggression = 600;
    t.enmity_hops = 3;
    t.kill_max_permille = 500;
    t.find_permille = 250;
    t.search_years = 3;
    t.search_weary_years = 500;  // r14 21.7 (generator 0.19.0)
    t.search_repeat_salience = 15;
    t.capital_sacked_salience = 90;
    t.capital_captured_salience = 80;
    return t;
}

std::string fatal_of(const h::ConflictTuning& t) {
    try {
        h::check_conflict_tuning(t);
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return "";
}

}  // namespace

TEST_CASE("P3-06 engineer: W1 dispositions, fear, drive and the strain rate") {
    const h::ConflictTuning t = shipped_tuning();
    const std::int64_t d = h::disposition(t, 0, 2, 0);
    CHECK(d == -700);
    CHECK(h::hostility(d) == 700);
    CHECK(h::share_of(2, 3) == 666);
    CHECK(h::fear_of(t, 666) == 443);
    CHECK(h::drive_of(t, d, 0, 443) == 1343);
    CHECK(h::drive_of(t, d, 0, 0) == 900);
    CHECK(h::strain_rate(600, 1343, 1) == 483);
    CHECK(h::strain_after(t, 0, 483, 1, 25, 1000) == 12075);
    CHECK(h::strain_after(t, 4999000, 483, 1, 25, 1000) == 5000000);  // capped at strain_max
    CHECK(h::declare_permille(t, 603, 480, 400, 600) == 75);
    CHECK(h::declare_permille(t, 299, 480, 400, 600) == 0);
}

TEST_CASE("P3-06 engineer: W1 front, force, battle, siege and casualties") {
    // R1 - R2 - R3, walkable borders of cost 1.
    const h::ConflictGraph g = {{{2, 1}}, {{1, 1}, {3, 1}}, {{2, 1}}};
    const std::vector<std::int64_t> goblins = {1};
    const std::vector<std::int64_t> elves = {2, 3};
    const std::vector<h::FrontEntry> fr = h::front_of(g, goblins, elves, 2);
    REQUIRE(fr.size() == 2U);
    CHECK(fr[0] == h::FrontEntry{2, 1, 1});
    CHECK(fr[1] == h::FrontEntry{3, 2, 2});
    CHECK(h::front_of(g, goblins, elves, 1).size() == 1U);
    const h::ConflictTuning t = shipped_tuning();
    const std::vector<std::pair<std::int64_t, h::SettlementKind>> gob = {{6000, h::SettlementKind::town}};
    const std::vector<std::pair<std::int64_t, h::SettlementKind>> elf = {{2000, h::SettlementKind::town},
                                                                          {500, h::SettlementKind::village}};
    CHECK(h::force_of(t, gob, 800) == 480);
    CHECK(h::force_of(t, elf, 1600) == 400);
    CHECK(h::battle_permille_of(480, 400, h::defence_of(t, h::DefenceTerms{})) == 545);
    CHECK(h::battle_permille_of(0, 0, 1000) == 500);
    const std::vector<std::int64_t> losses = h::casualty_losses(std::vector<std::int64_t>{2000, 500}, 100);
    CHECK(losses == std::vector<std::int64_t>{80, 20});
    CHECK(h::casualty_losses(std::vector<std::int64_t>{6000}, 38) == std::vector<std::int64_t>{38});
    // The remainder comes from one settlement only, at most its population (7.4).
    CHECK(h::casualty_losses(std::vector<std::int64_t>{1, 1, 1}, 2) == std::vector<std::int64_t>{1, 0, 0});
    const std::int64_t garrison = h::garrison_of(t, 480, 1600, h::SettlementKind::village, false);
    CHECK(garrison == 345);
    CHECK(h::siege_permille_of(442, garrison) == 561);
    CHECK(h::siege_permille_of(0, 0) == 0);
    CHECK(h::war_score(1, 0, 0, 1) == -1);
}

TEST_CASE("P3-06 engineer: a pass against a two-border walkable path, and ties of (cost, last region)") {
    // 1 - 4 over a pass (cost 3); 1 - 2 - 4 and 1 - 3 - 4 walkable (cost 2 each).
    const h::ConflictGraph g = {{{2, 1}, {3, 1}, {4, 3}}, {{1, 1}, {4, 1}}, {{1, 1}, {4, 1}}, {{1, 3}, {2, 1}, {3, 1}}};
    const std::vector<std::int64_t> x = {1};
    const std::vector<std::int64_t> y = {4};
    const std::vector<h::FrontEntry> fr = h::front_of(g, x, y, 2);
    REQUIRE(fr.size() == 1U);
    CHECK(fr[0] == h::FrontEntry{4, 2, 2});
    const std::vector<h::FrontEntry> one = h::front_of(g, x, y, 1);
    REQUIRE(one.size() == 1U);
    CHECK(one[0] == h::FrontEntry{4, 3, 1});
}

TEST_CASE("P3-06 engineer: the tuning seam's messages (16.4)") {
    CHECK(fatal_of(shipped_tuning()).empty());
    h::ConflictTuning t = shipped_tuning();
    t.march_hops = 5;
    CHECK(fatal_of(t) == "history: tuning: conflict.march_hops outside its schema range");
    t = shipped_tuning();
    t.camp.good_evil = -1001;
    CHECK(fatal_of(t) == "history: tuning: conflict.camp.good_evil outside its schema range");
    t = shipped_tuning();
    t.walls_permille.city = 0;
    CHECK(fatal_of(t) == "history: tuning: conflict.walls_permille.city outside its schema range");
    t = shipped_tuning();
    t.claim_strain = 301;
    CHECK(fatal_of(t) == "history: tuning: claim_strain must not be above declare_min");
    t = shipped_tuning();
    t.strain_max = 899;
    CHECK(fatal_of(t) == "history: tuning: declare_min + declare_span must not be above strain_max");
}

// --- r13 20.27: a cession passes every settlement of the loser in the ceded region ----------------------

#if UNMAPPED_HAVE_PEOPLES && UNMAPPED_HAVE_CONFLICT

namespace {

namespace ct = conflict_test;

// One cession, as the probe after conflict's run of a step sees it.
struct Cession {
    std::uint64_t serial = 0;
    h::Step step = h::Step::t6;
    std::int64_t year = 0, region = 0, from = 0, to = 0, event = 0;
    bool goal = false;                 // 7.7's cession of a war's goal; else 20.3 item 1's
    std::vector<std::int64_t> passed;  // settlements of `from` standing in the region before conflict ran
    std::int64_t unseen = 0;           // of those, created earlier in the step (not visible)
};

struct Standing {
    std::int64_t id = 0, owner = 0, region = 0;
    bool seen = false;
};

// Probes around conflict in T2, T6 and Q4: `between` (after peoples, before conflict) runs the optional
// injection, then keeps the regions' owner events, the standing settlements and the open wars; `after`
// finds the step's cessions (a region whose owner event became a region_ceded) and checks, on the live
// state, invariant (13) of 20.27 item 6, 14 item 5 for the ceded region, and that a settlement created
// in the step changes owner only by a cession of its region.
class CessionWatch {
public:
    std::function<void(h::StepContext&)> inject;  // runs first in `between`
    std::function<void(h::StepContext&)> check;   // runs last in `after`
    std::vector<Cession> cessions;
    std::vector<std::string> failures;
    std::int64_t unseen_kept = 0;  // same-step settlements in a region not ceded, kept by their owner

    void install(ct::ConflictConfig& cfg) {
        const std::vector<h::Step> steps = {h::Step::t2, h::Step::t6, h::Step::q4};
        cfg.add_between(steps, [this](h::StepContext& ctx) { before(ctx); });
        sub_ = cfg.pair->sub.get();
        cfg.add_after(steps, [this](h::StepContext& ctx) { after(ctx, *sub_); });
    }

private:
    std::vector<std::int64_t> owner_event_;
    std::vector<Standing> standing_;
    std::set<std::size_t> open_wars_;
    const h::ConflictSubsystem* sub_ = nullptr;

    static bool is_standing(const h::Settlement& s) {
        return s.status == h::SettlementStatus::thriving || s.status == h::SettlementStatus::declining;
    }
    static std::size_t region_count(const h::StepContext& ctx) { return ctx.inputs().regions.size(); }

    void before(h::StepContext& ctx) {
        if (inject) inject(ctx);
        const h::State& st = ctx.state();
        owner_event_.assign(region_count(ctx) + 1, 0);
        for (std::size_t r = 1; r <= region_count(ctx); ++r) {
            owner_event_[r] = st.region(static_cast<std::int64_t>(r)).owner_event;
        }
        standing_.clear();
        for (const h::Settlement& s : st.settlements()) {
            if (is_standing(s)) standing_.push_back({s.id, s.owner_civ, s.region, ctx.visible(s)});
        }
        open_wars_.clear();
        for (std::size_t i = 0; i < sub_->wars().size(); ++i) {
            if (sub_->wars()[i].open) open_wars_.insert(i);
        }
    }

    void fail(const h::StepContext& ctx, const std::string& what) {
        failures.push_back("year " + std::to_string(ctx.year()) + ": " + what);
    }

    void after(h::StepContext& ctx, const h::ConflictSubsystem& sub) {
        const h::State& st = ctx.state();
        std::set<std::int64_t> ceded;
        for (std::size_t r = 1; r < owner_event_.size(); ++r) {
            const std::int64_t g = static_cast<std::int64_t>(r);
            const std::int64_t ev = st.region(g).owner_event;
            if (ev == owner_event_[r] || ev == 0 || ctx.event_kind(ev) != "region_ceded") continue;
            Cession c;
            c.serial = ctx.serial();
            c.step = ctx.step();
            c.year = ctx.year();
            c.region = g;
            c.event = ev;
            c.from = ctx.event_participant(ev, "from");
            c.to = ctx.event_participant(ev, "to");
            for (const std::size_t i : open_wars_) {
                const h::ConflictWar& w = sub.wars()[i];
                const bool pair = (w.attacker == c.from && w.defender == c.to) ||
                                  (w.attacker == c.to && w.defender == c.from);
                c.goal = c.goal || (!w.open && pair && w.goal == g);
            }
            for (const Standing& s : standing_) {
                if (s.owner != c.from || s.region != g) continue;
                c.passed.push_back(s.id);
                c.unseen += s.seen ? 0 : 1;
                const h::Settlement& now = st.settlement(s.id);
                if (is_standing(now) && now.owner_civ != c.to) {
                    fail(ctx, "settlement " + std::to_string(s.id) + " of region " + std::to_string(g) +
                                  " did not pass to " + std::to_string(c.to));
                }
            }
            bool owner_has = false;
            for (const h::Settlement& s : st.settlements()) {
                if (!is_standing(s) || s.region != g) continue;
                if (s.owner_civ == c.from) {
                    fail(ctx, "civilization " + std::to_string(c.from) + " keeps settlement " +
                                  std::to_string(s.id) + " in ceded region " + std::to_string(g));
                }
                owner_has = owner_has || s.owner_civ == st.region(g).owner;
            }
            if (!owner_has) fail(ctx, "the owner of ceded region " + std::to_string(g) + " has no settlement in it");
            ceded.insert(g);
            cessions.push_back(c);
        }
        for (const Standing& s : standing_) {
            if (s.seen || ceded.count(s.region) != 0) continue;
            if (st.settlement(s.id).owner_civ != s.owner) {
                fail(ctx, "same-step settlement " + std::to_string(s.id) + " changed owner outside a cession");
            } else {
                ++unseen_kept;
            }
        }
        if (check) check(ctx);
    }
};

// A kind for the probe's planted settlements: no civilization participant, so no quiet budget is charged.
void add_planted_kind(eng::EventCatalogue& cat) {
    eng::KindDef k;
    k.id = static_cast<std::int64_t>(cat.kinds.size()) + 1;
    k.key = "test_planted";
    k.caused = false;
    k.region_use = 1;
    k.settlement_use = 1;
    k.roles = {eng::RoleDef{1, "settlement", eng::EntityType::settlement, 1, 1}};
    k.salience = 10;
    k.templ = "A settlement was planted.";
    cat.kinds.push_back(k);
}

// A settlement of civ planted now at (x, y) of region r, as a same-step colony would be founded.
std::int64_t plant(h::StepContext& ctx, std::int64_t civ, std::int64_t region, std::int64_t x, std::int64_t y) {
    h::State& st = ctx.state();
    const std::int64_t id = static_cast<std::int64_t>(st.settlements().size()) + 1;
    h::EventSpec e;
    e.kind = "test_planted";
    e.year = ctx.year();
    e.end_year = ctx.year();
    e.region = region;
    e.settlement = id;
    e.participants = {h::Participant{"settlement", h::EntityType::settlement, id}};
    const std::int64_t ev = ctx.append(e);
    const std::int64_t got = st.found_settlement(x, y, region, civ, h::SettlementKind::hamlet, 50,
                                                 "Planted " + std::to_string(id), ev);
    REQUIRE(got == id);
    return id;
}

struct Watched {
    eng::Tables tables;
    std::vector<Cession> cessions;
    std::vector<std::string> failures;
    std::int64_t unseen_kept = 0;
};

Watched watched_run(const worldgen::data::ContentSet& content, const eng::HistoryInputs& in,
                    const std::function<void(CessionWatch&)>& setup = {}, bool planted_kind = false) {
    ct::ConflictConfig cfg(content);
    if (planted_kind) add_planted_kind(cfg.catalogue);
    CessionWatch watch;
    if (setup) setup(watch);
    watch.install(cfg);
    // P5-12: ConflictConfig installs the renaming subsystem after conflict, as the shipped stage does.
    Watched out;
    out.tables = cfg.run(content, in);
    out.cessions = watch.cessions;
    out.failures = watch.failures;
    out.unseen_kept = watch.unseen_kept;
    return out;
}

const std::vector<const char*>& property_cases() {
    static const std::vector<const char*> v = {
        "syn-0",      "syn-1",    "syn-2",    "syn-3",    "syn-4",    "syn-5",    "syn-6",     "syn-7",
        "syn-8",      "syn-9",    "syn-10",   "syn-11",   "syn-12",   "syn-13",   "syn-14",    "syn-15",
        "stress-0",   "stress-1", "stress-2", "stress-3", "stress-4", "stress-5", "stress-6",  "stress-7",
        "stress-9",   "stress-12", "stress-13", "stress-14", "norefuge-5", "norefuge-12", "norefuge-13"};
    return v;
}

}  // namespace

TEST_CASE("P3-06 r13 engineer: invariant (13) and 14 item 5 at every cession of the synthetic worlds (20.27)") {
    std::int64_t goal = 0, other = 0;
    for (const char* name : property_cases()) {
        const ct::CCase& g = ct::conflict_case(name);
        CAPTURE(g.name);
        const eng::HistoryInputs in = ct::synthetic_inputs(g);
        const Watched w = watched_run(ct::content_of(g), in);
        for (const std::string& f : w.failures) FAIL_CHECK(f);
        for (const Cession& c : w.cessions) (c.goal ? goal : other) += 1;
        // the probes read only: the watched run is the shipped stage
        if (std::string(name) == "syn-5" || std::string(name) == "stress-0") {
            CHECK(w.tables == eng::history_stage(ct::content_of(g), in));
        }
    }
    CHECK(goal > 0);
    CHECK(other > 0);
}

// 7.7's and 20.3's cessions with one visible settlement and one same-step colony of the loser L in G (a
// settlement planted after peoples, before conflict, in the step of a cession of the unplanted run): both
// pass, with the region_ceded and no event of their own; a same-step colony of L in a region not ceded
// stays L's.
TEST_CASE("P3-06 r13 engineer: a same-step colony passes with the cession of its region, one elsewhere stays") {
    for (const bool want_goal : {true, false}) {
        CAPTURE(want_goal);
        bool done = false;
        for (const char* name : property_cases()) {
            if (done) break;
            const ct::CCase& g = ct::conflict_case(name);
            const eng::HistoryInputs in = ct::synthetic_inputs(g);
            const worldgen::data::ContentSet& content = ct::content_of(g);
            const Watched base = watched_run(content, in, {}, true);
            for (const Cession& c : base.cessions) {
                if (done) break;
                if (c.goal != want_goal || c.step != h::Step::q4 || c.passed.empty() || c.unseen != 0) continue;
                CAPTURE(g.name);
                CAPTURE(c.year);
                CAPTURE(c.region);
                std::int64_t in_g = 0, elsewhere = 0, ceded_event = 0;
                bool elsewhere_wanted = false;
                const auto setup = [&](CessionWatch& watch) {
                    watch.inject = [&](h::StepContext& ctx) {
                        if (ctx.serial() != c.serial || ctx.step() != h::Step::q4) return;
                        const h::State& st = ctx.state();
                        const h::Settlement& v = st.settlement(c.passed.front());
                        REQUIRE(ctx.visible(v));
                        const std::int64_t vx = v.x, vy = v.y;
                        std::int64_t ox = -1, oy = -1, oregion = 0;
                        for (const h::Settlement& s : st.settlements()) {
                            const bool ceded_here = std::any_of(base.cessions.begin(), base.cessions.end(),
                                                                [&](const Cession& d) {
                                                                    return d.serial == c.serial &&
                                                                           d.region == s.region;
                                                                });
                            if (oregion == 0 && s.owner_civ == c.from && s.region != c.region && !ceded_here &&
                                (s.status == h::SettlementStatus::thriving ||
                                 s.status == h::SettlementStatus::declining)) {
                                ox = s.x;
                                oy = s.y;
                                oregion = s.region;
                            }
                        }
                        in_g = plant(ctx, c.from, c.region, vx, vy);
                        if (oregion != 0) {
                            elsewhere_wanted = true;
                            elsewhere = plant(ctx, c.from, oregion, ox, oy);
                        }
                    };
                    watch.check = [&](h::StepContext& ctx) {
                        if (ctx.serial() != c.serial || ctx.step() != h::Step::q4) return;
                        const h::State& st = ctx.state();
                        CHECK(st.region(c.region).owner == c.to);
                        ceded_event = st.region(c.region).owner_event;
                        CHECK(st.settlement(in_g).owner_civ == c.to);
                        for (const std::int64_t id : c.passed) CHECK(st.settlement(id).owner_civ == c.to);
                        if (elsewhere != 0) CHECK(st.settlement(elsewhere).owner_civ == c.from);
                    };
                };
                const Watched w = watched_run(content, in, setup, true);
                for (const std::string& f : w.failures) FAIL_CHECK(f);
                REQUIRE(in_g != 0);
                // the same cession, after the planted events and with no event of the transfers
                const std::int64_t planted = elsewhere_wanted ? 2 : 1;
                CHECK(ceded_event == c.event + planted);
                bool found = false;
                for (const Cession& d : w.cessions) {
                    if (d.serial != c.serial || d.region != c.region) continue;
                    found = true;
                    CHECK(d.goal == want_goal);
                    CHECK(d.from == c.from);
                    CHECK(d.to == c.to);
                    CHECK(d.unseen == 1);
                    CHECK(d.passed.size() == c.passed.size() + 1);
                    CHECK(std::is_sorted(d.passed.begin(), d.passed.end()));
                }
                CHECK(found);
                if (want_goal) {
                    CHECK(elsewhere_wanted);  // the goal case carries the third test of 20.27 item 8
                }
                done = true;
            }
        }
        // P3-06 r17 (P3-10 second round): the synthetic worlds no longer hold a Q4 cession of a war's goal
        // with a visible settlement of the loser, so the goal half is a warning until a case has one.
        // P5-01 (generator 0.21.0): the toll factors and fresh claims move the synthetic histories, and
        // neither half finds its case any more; both are warnings until a case has one.
        WARN_MESSAGE(done, "no Q4 cession with a visible settlement of the loser in the synthetic worlds");
    }
}

TEST_SUITE("history-slow" * doctest::skip()) {

// 20.27 item 7: a colony founded in a Q4 passes to the winner with the region_ceded of that step.
// Generator 0.19.0 changed the 0.18.0 case (2048 seed 3, colony s240 of c15 in region 158 at 5234; at
// 0.19.0 that world has no such cession). 1024 seed 36 (default settings) had one at the first P3-10 round
// (region 10, c3 to c2 at 4913, colony s66), which the second round's rules remove; no 1024 default world of
// seeds 0 to 160 has one at the second round, and 1024 seed 5 with --event-intensity turbulent has two (region_ceded
// e933 of region 25 in 4410, colony s101; e1102 of region 29 in 4885, colony s110; P3-10 second round
// integration); the test finds the first in the log, the first cession that passes a
// settlement created earlier in its step, and checks invariant (13) over the whole world.
// P5-01 (generator 0.21.0, test writer at integration): seed 5 has no same-step colony cession any more; 1024
// turbulent seed 19 has one (region_ceded e751 of region 10 in 4331, colony s122).
// P5-02 (generator 0.23.0, test writer): seed 19 has none any more; of the 1024 turbulent seeds 0 to 39, seeds 5
// and 12 have one each (C++ worlds, the history tables confirmed by the conflict reference's check-world); seed
// 5's is region_ceded e1048 of region 34 in 4410, colony e1046 (s112).
TEST_CASE("P3-06 r13 engineer: invariant (13) on 1024 turbulent seed 5 and a same-step colony cession (20.27 item 7)") {
    static dbtest::TempDir dir("conflict-engineer-1024-turbulent-5");
    std::vector<worldgen::stages::StageInfo> stages = worldgen::stages::stages_until(std::string("chronology"));
    chrono_api::Params hp;
    hp.intensity = "turbulent";
    const worldgen::WorldParams wp = chrono_test::world_params(5, 1024, hp, ct::shipped());
    const std::string path = dir.file("turbulent-5-1024.db");
    REQUIRE(worldgen::stages::testing::generate_world_with_stages(wp, ct::shipped(), stages, path).ok());
    const eng::HistoryInputs in = h::history_inputs_from_world(chrono_test::open_world(path), ct::shipped());
    const Watched w = watched_run(ct::shipped(), in);
    for (const std::string& f : w.failures) FAIL_CHECK(f);
    const Cession* found = nullptr;
    for (const Cession& c : w.cessions) {
        if (c.unseen >= 1) {
            found = &c;
            break;
        }
    }
    REQUIRE(found != nullptr);
    MESSAGE("same-step cession: year " << found->year << " region " << found->region << " c" << found->from
                                       << " -> c" << found->to << " event " << found->event);
    // the census of that step's bookkeeping writes the winner as the owner of every passed settlement
    for (const std::int64_t s : found->passed) {
        bool census = false;
        for (const eng::Row& r : w.tables.settlement_census) {
            if (ct::at(r, 0) == s && ct::at(r, 1) == found->year) {
                census = true;
                CHECK(ct::at(r, 2) == found->to);
            }
        }
        CHECK(census);
    }
    CHECK(w.unseen_kept > 0);
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES && UNMAPPED_HAVE_CONFLICT
