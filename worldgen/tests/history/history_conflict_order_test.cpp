// Spec P3-06 r14 21.6 (with r15's readings and r17 22.5 item 1): the order of a war's end checks. At each check of
// 7.7 (after 7.3 in T6) and at 7.8's end of an active phase (Q4), for an open war of attacker a and defender b
// over G with o = G's owner at the check:
//   1. aim achieved (o = a and b has no standing settlement in G): a wins, `peace` when b is exhausted, else
//      `war_won`; no last-battle rule;
//   2. goal lost (o != a, b neither holds G nor stands in it): the war ends now by the score (20.15 items 1 and 3:
//      the last-battle rule; `peace` only when the loser is exhausted, else `war_won`; 0 a `truce`);
//   3. otherwise 7.7 items 1 (exhaustion), 3 (no front) and 4 (length).
// So aim and goal-lost are decided before exhaustion. Validation's D1 (docs/reviews/p3-10-followups-round2-
// validation.md, 1024 default seed 73, e914 in 5063) is a check where aim held while exhaustion also held, with
// the score not for the attacker: aim gives the attacker a `peace`, exhaustion first a truce.
//
// No golden world reaches such a check except seed 73 (the test writer's scan of every golden case with the
// reference's check instrumented, and 96 synthetic and stress runs with exhaustion_permille 20 to 600: none). When
// the attacker gains G by its own arms (the usual way), it captured b's last settlement in G in the same T6, so
// the score is for the attacker and both orders give the same ending. The order shows only when G passes to the
// attacker some other way (another war's cost, a secession, a third party) while the score is level or for b.
//
// The cases here make that happen through the engine's public seam: a probe between peoples and conflict in T6
// (or Q4) passes every standing settlement of b in G, and G, to a (aim) or to a third civilization (goal lost), for
// every open war it can, once per war; a probe after conflict reads the closed war (dead, pop0, taken_people,
// last_battle_winner: P3-06 9's public ConflictWar) and the state, and the test compares the ending event with
// 21.6. Exhaustion is computed as 7.7 item 1 with r9 20.6 item 1: dead x 1000 >= exhaustion_permille x max(1,
// pop0 x muster_permille div 1000). The reach checks require at least one aim case in which exhaustion first
// (T6) or the score first (Q4) would give another ending.
//
// The mirrored goal-lost case: in T6, ending by the score with the loser exhausted gives the same winner and
// kind as 7.7 item 1 (both exhausted, or the exhausted side behind: the score's outcome, `peace`; the exhausted
// side leading: it wins, `war_won`, and the loser is not exhausted), so item 2's place before item 1 is not
// observable from the rows; the case checks item 2's rule itself (ends now, the score with the last-battle rule,
// the kind). In Q4 the kind is observable (a plain quiet ending is `war_won`; item 2 writes `peace` when the
// loser is exhausted), but no war reaches a quiet step with an exhausted side (each T6 check would have ended
// it), so only the winner rule is reached there.
//
// Written from the spec text alone (P3-06 7.7, 7.8, 20.15, 21.6, 22.5), before the fix, for the P3-10 second
// round's D1.

#include "p3_06_conflict_support.hpp"

#if UNMAPPED_HAVE_PEOPLES && UNMAPPED_HAVE_CONFLICT

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "worldgen/history/conflict.hpp"

namespace {

using namespace conflict_test;
namespace h = worldgen::history;

enum class Mode { aim, lost };

struct Injected {
    std::int64_t declared = 0, a = 0, b = 0, g = 0, third = 0, year = 0;
    std::size_t index = 0;
    bool closed = false;     // closed in the probe's step
    bool at_check = false;   // after the step G is as injected and b is out of G
    std::int64_t s = 0, lbw = 0;
    bool ex_a = false, ex_b = false;
};

struct Result {
    Tables t;
    std::vector<Injected> wars;
    std::int64_t open_after = 0;  // injected wars still open after a T6 probe step with the injection standing
};

std::map<std::int64_t, std::int64_t> muster_of_race(const wd::ContentSet& c) {
    std::map<std::int64_t, std::int64_t> out;
    const wd::KindContent* race = c.find_kind("race");
    REQUIRE(race != nullptr);
    for (const wd::Definition& d : race->definitions) out[d.record.get_int("order")] = d.record.get_int("muster_permille");
    return out;
}

bool standing(const h::Settlement& s) {
    return s.status == h::SettlementStatus::thriving || s.status == h::SettlementStatus::declining;
}

bool exhausted(const h::ConflictWar& w, std::size_t side, std::int64_t permille, std::int64_t muster) {
    return w.dead[side] * 1000 >= permille * std::max<std::int64_t>(1, w.pop0[side] * muster / 1000);
}

Result probe_run(const wd::ContentSet& content, const eng::HistoryInputs& in, h::Step step, Mode mode) {
    ConflictConfig cfg(content);
    {
        eng::KindDef k;
        k.id = static_cast<std::int64_t>(cfg.catalogue.kinds.size()) + 1;
        k.key = "test_change";
        k.caused = false;
        k.region_use = 0;
        k.settlement_use = 0;
        // no civilization role, so no quiet budget is charged (P3-04 W9)
        k.roles = {eng::RoleDef{1, "settlement", eng::EntityType::settlement, 0, 1}};
        k.salience = 10;
        k.templ = "The land changed hands.";
        cfg.catalogue.kinds.push_back(k);
    }
    const h::ConflictSubsystem* sub = cfg.pair->sub.get();
    const std::int64_t permille = conflict_int(content, "exhaustion_permille");
    const std::map<std::int64_t, std::int64_t> muster = muster_of_race(content);
    Result out;
    std::set<std::int64_t> done;  // declared events already injected
    std::vector<std::size_t> now;  // indices into out.wars injected in the current step
    cfg.add_between({step}, [&](eng::StepContext& ctx) {
        now.clear();
        h::State& st = ctx.state();
        std::set<std::int64_t> goals;
        std::int64_t ev = 0;  // one probe event per step (a quiet step's events without a civilization share a budget)
        for (std::size_t i = 0; i < sub->wars().size(); ++i) {
            const h::ConflictWar w = sub->wars()[i];  // a copy: the state changes below
            // a war declared in this year's T2 is neither fought nor checked in this T6 (the reference's reading
            // of 7.3 and 7.7): it is probed from the next step on
            if (!w.open || done.count(w.declared) != 0 || goals.count(w.goal) != 0 || w.start == ctx.year()) continue;
            const std::int64_t a = w.attacker, b = w.defender, g = w.goal;
            if (st.civ(a).ended_year || st.civ(b).ended_year) continue;
            const std::int64_t cap = st.civ(b).capital;
            if (cap == 0) continue;
            std::vector<std::int64_t> in_g;
            std::int64_t elsewhere = 0;  // b's first standing settlement outside G: its capital if G held it
            for (const h::Settlement& s : st.settlements()) {
                if (!standing(s) || s.owner_civ != b) continue;
                if (s.region == g) {
                    in_g.push_back(s.id);
                } else if (elsewhere == 0) {
                    elsewhere = s.id;
                }
            }
            if (elsewhere == 0) continue;
            std::int64_t to = a;
            if (mode == Mode::lost) {
                to = 0;
                for (const h::Civilization& c : st.civs()) {
                    if (!c.ended_year && c.id != a && c.id != b && c.capital != 0) {
                        to = c.id;
                        break;
                    }
                }
                if (to == 0) continue;
            }
            if (mode == Mode::aim && st.region(g).owner == a && in_g.empty()) continue;  // already aim: not a probe
            if (step == h::Step::q4) {
                // in a quiet step every event is charged to a budget (P3-04 W9): the transfer names the war's
                // declaration instead of a probe event
                ev = w.declared;
            } else if (ev == 0) {
                eng::EventSpec e;
                e.kind = "test_change";
                e.year = ctx.year();
                e.end_year = ctx.year();
                ev = ctx.append(e);
            }
            for (const std::int64_t s : in_g) st.set_settlement_owner(s, to, ev);
            st.set_region_owner(g, to, ev);
            if (st.settlement(cap).region == g) st.set_capital(b, elsewhere);
            done.insert(w.declared);
            goals.insert(g);
            Injected x;
            x.declared = w.declared;
            x.a = a;
            x.b = b;
            x.g = g;
            x.third = mode == Mode::lost ? to : 0;
            x.year = ctx.year();
            x.index = i;
            now.push_back(out.wars.size());
            out.wars.push_back(x);
        }
    });
    cfg.add_after({step}, [&](eng::StepContext& ctx) {
        const h::State& st = ctx.state();
        for (const std::size_t k : now) {
            Injected& x = out.wars[k];
            const h::ConflictWar& w = sub->wars()[x.index];
            REQUIRE(w.declared == x.declared);
            const std::int64_t o = st.region(x.g).owner;
            bool b_in = false;
            for (const h::Settlement& s : st.settlements()) b_in = b_in || (standing(s) && s.owner_civ == x.b && s.region == x.g);
            x.at_check = !b_in && !st.civ(x.a).ended_year && !st.civ(x.b).ended_year &&
                         (mode == Mode::aim ? o == x.a : (o != x.a && o != x.b));
            x.closed = !w.open;
            if (x.at_check && !x.closed && step == h::Step::t6) ++out.open_after;
            x.s = (w.dead[1] - w.dead[0]) + (w.taken_people[0] - w.taken_people[1]);
            x.lbw = w.last_battle_winner;
            x.ex_a = exhausted(w, 0, permille, muster.at(st.civ(x.a).race));
            x.ex_b = exhausted(w, 1, permille, muster.at(st.civ(x.b).race));
        }
        now.clear();
    });
    out.t = cfg.run(content, in);
    return out;
}

// The ending of an injected war: the `peace`, `war_won` or `truce` of its year whose participants are a and b.
struct Ending {
    std::int64_t id = 0, kind = 0, winner = 0, loser = 0;
};

Ending ending_of(const Tables& t, const Index& x, const Injected& w) {
    Ending out;
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0), k = at(e, 3);
        if (at(e, 1) != w.year || (k != k_peace && k != k_war_won && k != k_truce)) continue;
        std::set<std::int64_t> who;
        for (const auto& [seq, ids] : x.roles.count(id) ? x.roles.at(id) : std::map<std::int64_t, std::vector<std::int64_t>>{}) {
            (void)seq;
            who.insert(ids.begin(), ids.end());
        }
        if (who != std::set<std::int64_t>{w.a, w.b}) continue;
        REQUIRE_MESSAGE(out.id == 0, "two endings of c" << w.a << " and c" << w.b << " in " << w.year);
        out.id = id;
        out.kind = k;
        if (k != k_truce) {
            out.winner = x.role1(id, 1);
            out.loser = x.role1(id, 2);
        }
    }
    return out;
}

// 21.6 item 1: a wins; `peace` when b is exhausted, else `war_won`.
Ending expect_aim(const Injected& w) {
    return Ending{0, w.ex_b ? k_peace : k_war_won, w.a, w.b};
}

// 21.6 item 2 (and 20.15 items 1, 3, 4): the score with the last-battle rule; `peace` only when the loser is
// exhausted; 0 or a last-battle reversal a truce.
Ending expect_by_score(const Injected& w) {
    std::int64_t win = w.s > 0 ? w.a : (w.s < 0 ? w.b : 0);
    if (win != 0 && w.lbw == (win == w.a ? w.b : w.a)) win = 0;
    if (win == 0) return Ending{0, k_truce, 0, 0};
    const bool loser_ex = win == w.a ? w.ex_b : w.ex_a;
    return Ending{0, loser_ex ? k_peace : k_war_won, win, win == w.a ? w.b : w.a};
}

// What 7.7 item 1 (exhaustion) would give if it were tested first, or nothing when no side is exhausted.
bool exhaustion_first(const Injected& w, Ending& e) {
    if (!w.ex_a && !w.ex_b) return false;
    const std::int64_t fav = w.s > 0 ? w.a : (w.s < 0 ? w.b : 0);
    const std::int64_t only = w.ex_a && w.ex_b ? 0 : (w.ex_a ? w.a : w.b);
    if (only == 0 || fav == 0 || only != fav) {
        e = expect_by_score(w);
        if (e.kind != k_truce) e.kind = k_peace;
        return true;
    }
    std::int64_t win = only;  // 20.15 item 2: the exhausted side that leads wins (`war_won`); last-battle rule
    if (w.lbw == (win == w.a ? w.b : w.a)) win = 0;
    e = win == 0 ? Ending{0, k_truce, 0, 0} : Ending{0, k_war_won, win, win == w.a ? w.b : w.a};
    return true;
}

std::string text(const Ending& e) {
    const char* k = e.kind == k_peace ? "peace" : (e.kind == k_war_won ? "war_won" : (e.kind == k_truce ? "truce" : "none"));
    return std::string(k) + " winner c" + std::to_string(e.winner) + " loser c" + std::to_string(e.loser);
}

bool same(const Ending& a, const Ending& b) { return a.kind == b.kind && a.winner == b.winner && a.loser == b.loser; }

struct Tally {
    std::int64_t checked = 0, differs = 0, score_zero = 0, score_for_b = 0, differs_zero = 0, differs_for_b = 0;
    void add(const Tally& o) {
        checked += o.checked;
        differs += o.differs;
        score_zero += o.score_zero;
        score_for_b += o.score_for_b;
        differs_zero += o.differs_zero;
        differs_for_b += o.differs_for_b;
    }
};

// Checks every injected war that the probe saw as reaching its check with the injection standing, and counts
// those in which the other order would have given another ending.
Tally check_endings(const Result& r, Mode mode, h::Step step) {
    const Index x(r.t);
    Tally n;
    for (const Injected& w : r.wars) {
        if (!w.at_check || !w.closed) continue;
        CAPTURE(w.declared);
        CAPTURE(w.year);
        CAPTURE(w.s);
        CAPTURE(w.lbw);
        CAPTURE(w.ex_a);
        CAPTURE(w.ex_b);
        const Ending got = ending_of(r.t, x, w);
        REQUIRE(got.id != 0);
        const Ending want = mode == Mode::aim ? expect_aim(w) : expect_by_score(w);
        CHECK_MESSAGE(same(got, want), "e" << got.id << ": " << text(got) << ", 21.6 gives " << text(want));
        ++n.checked;
        n.score_zero += w.s == 0 ? 1 : 0;
        n.score_for_b += w.s < 0 ? 1 : 0;
        if (mode == Mode::aim) {
            Ending other;
            bool differs = false;
            if (step == h::Step::t6) {
                differs = exhaustion_first(w, other) && !same(other, want);
            } else {
                // 7.8 by the score first: a plain quiet ending (`war_won`), the last-battle rule
                other = expect_by_score(w);
                if (other.kind != k_truce) other.kind = k_war_won;
                differs = !same(other, want);
            }
            n.differs += differs ? 1 : 0;
            n.differs_zero += differs && w.s == 0 ? 1 : 0;
            n.differs_for_b += differs && w.s < 0 ? 1 : 0;
        }
    }
    return n;
}

const wd::ContentSet& bled_content() {
    // the stress tuning with exhaustion_permille 1: nearly every side is exhausted at its first check
    static const wd::ContentSet c = with_conflict({{"exhaustion_permille", I64(1)}}, conflict_test::stress_content());
    return c;
}

const wd::ContentSet& worn_content() {
    // exhaustion_permille 30: some sides are exhausted at a later check, often one side only
    static const wd::ContentSet c = with_conflict({{"exhaustion_permille", I64(30)}}, conflict_test::stress_content());
    return c;
}

eng::HistoryInputs stress_inputs(const char* name, const wd::ContentSet& content) {
    const CCase& g = conflict_case(name);
    return inputs_of(make_synthetic(g.seed), g.seed, settings_of(g), content);
}

const std::vector<const char*>& order_cases() {
    static const std::vector<const char*> v = {"stress-0", "stress-1", "stress-2", "stress-3", "stress-4", "stress-5",
                                               "stress-6", "stress-7", "stress-9", "stress-12", "stress-13", "stress-14"};
    return v;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-06 21.6 order (T6): aim before exhaustion; a wins, peace when b is exhausted, with the score level or for b") {
    Tally all;
    for (const wd::ContentSet* content : {&conflict_test::stress_content(), &worn_content(), &bled_content()}) {
        for (const char* name : order_cases()) {
            CAPTURE(std::string(name));
            CAPTURE(conflict_int(*content, "exhaustion_permille"));
            const Result r = probe_run(*content, stress_inputs(name, *content), h::Step::t6, Mode::aim);
            CHECK(r.open_after == 0);  // 21.6 item 1 ends the war at the check
            all.add(check_endings(r, Mode::aim, h::Step::t6));
        }
    }
    MESSAGE("T6 aim checks " << all.checked << " (score 0: " << all.score_zero << ", for the defender: "
                              << all.score_for_b << "); exhaustion first would differ in " << all.differs
                              << " (score 0: " << all.differs_zero << ", for the defender: " << all.differs_for_b << ")");
    CHECK(all.checked >= 10);
    CHECK(all.differs_for_b >= 1);  // the order is reached with the score for the defender
}

TEST_CASE("P3-06 21.6 order (T6, mirrored): a goal lost to a third civilization ends at the check by the score") {
    Tally all;
    for (const wd::ContentSet* content : {&conflict_test::stress_content(), &bled_content()}) {
        for (const char* name : order_cases()) {
            CAPTURE(std::string(name));
            const Result r = probe_run(*content, stress_inputs(name, *content), h::Step::t6, Mode::lost);
            CHECK(r.open_after == 0);  // 21.6 item 2 ends the war at the check
            const Tally n = check_endings(r, Mode::lost, h::Step::t6);
            all.checked += n.checked;
        }
    }
    MESSAGE("T6 goal-lost checks " << all.checked);
    CHECK(all.checked >= 10);
}

TEST_CASE("P3-06 21.6 order (Q4, 7.8): aim before the quiet ending by the score; goal lost by the score") {
    Tally aim, lost;
    for (const char* name : order_cases()) {
        CAPTURE(std::string(name));
        const Result ra = probe_run(conflict_test::stress_content(), stress_inputs(name, conflict_test::stress_content()), h::Step::q4, Mode::aim);
        aim.add(check_endings(ra, Mode::aim, h::Step::q4));
        const Result rl = probe_run(conflict_test::stress_content(), stress_inputs(name, conflict_test::stress_content()), h::Step::q4, Mode::lost);
        lost.checked += check_endings(rl, Mode::lost, h::Step::q4).checked;
    }
    MESSAGE("Q4 aim checks " << aim.checked << ", the score first would differ in " << aim.differs << " (score 0: "
                              << aim.score_zero << ", for the defender: " << aim.score_for_b << "); goal-lost checks "
                              << lost.checked);
    CHECK(aim.checked >= 3);
    CHECK(aim.differs >= 1);
    CHECK(lost.checked >= 3);
}

// The engineer's seam of the fix (P3-10 fix3): end_decision(attacker, defender, aim, defender_there, ex_a, ex_b,
// by_score, over), by_score the score's winner after 20.15 item 3's last-battle rule (0: a truce), over no front or
// the length reached; the result: ends, winner (0 a truce), sued (`peace`, else `war_won`). Expectations from the
// spec: the T6 checks the probes above never reach, a level score (by_score 0) with a side exhausted, and the
// rest of 21.6's order.
TEST_CASE("P3-06 21.6 order (seam): end_decision with a level score and an exhausted side, and every item's place") {
    using D = h::EndDecision;
    const std::int64_t a = 4, b = 9;
    for (const bool over : {false, true}) {
        CAPTURE(over);
        // 21.6 item 1 (aim): a wins whatever the score, the exhaustion and the length; `peace` when b is exhausted.
        CHECK(h::end_decision(a, b, true, false, true, false, 0, over) == D{true, a, false});
        CHECK(h::end_decision(a, b, true, false, false, true, 0, over) == D{true, a, true});
        CHECK(h::end_decision(a, b, true, false, true, true, 0, over) == D{true, a, true});
        CHECK(h::end_decision(a, b, true, false, false, false, 0, over) == D{true, a, false});
        CHECK(h::end_decision(a, b, true, false, true, true, b, over) == D{true, a, true});  // D1, seed 73
        CHECK(h::end_decision(a, b, true, false, true, false, b, over) == D{true, a, false});
        // 21.6 item 2 (goal lost): the score now; a level score is a truce whoever is exhausted.
        CHECK(h::end_decision(a, b, false, false, true, false, 0, over) == D{true, 0, false});
        CHECK(h::end_decision(a, b, false, false, false, true, 0, over) == D{true, 0, false});
        CHECK(h::end_decision(a, b, false, false, true, true, 0, over) == D{true, 0, false});
        CHECK(h::end_decision(a, b, false, false, false, false, 0, over) == D{true, 0, false});
        // item 2 with a winner: `peace` only when the loser is exhausted (20.15 item 4)
        CHECK(h::end_decision(a, b, false, false, true, true, b, over) == D{true, b, true});
        CHECK(h::end_decision(a, b, false, false, false, true, b, over) == D{true, b, false});
        CHECK(h::end_decision(a, b, false, false, false, false, a, over) == D{true, a, false});
        // 21.6 item 3 -> 7.7 item 1 with a level score: "if s = 0 ... the war ends with s's outcome", a truce
        CHECK(h::end_decision(a, b, false, true, true, false, 0, over) == D{true, 0, false});
        CHECK(h::end_decision(a, b, false, true, false, true, 0, over) == D{true, 0, false});
        CHECK(h::end_decision(a, b, false, true, true, true, 0, over) == D{true, 0, false});
        // 7.7 item 1 with a winner: both exhausted, the score's outcome, `peace` (the loser is exhausted)
        CHECK(h::end_decision(a, b, false, true, true, true, a, over) == D{true, a, true});
        // 20.15 item 2: the one exhausted side leads: it wins, `war_won`
        CHECK(h::end_decision(a, b, false, true, false, true, b, over) == D{true, b, false});
    }
    // 7.7 items 3 and 4 (no front, length) without exhaustion: the score, `war_won`; a level score a truce
    CHECK(h::end_decision(a, b, false, true, false, false, b, true) == D{true, b, false});
    CHECK(h::end_decision(a, b, false, true, false, false, 0, true) == D{true, 0, false});
    // nothing holds: the war goes on
    CHECK(h::end_decision(a, b, false, true, false, false, a, false) == D{});
    CHECK(h::end_decision(a, b, false, true, false, false, 0, false) == D{});
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES && UNMAPPED_HAVE_CONFLICT
