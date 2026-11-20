// The war of spec P3-06 sections 7.3 to 7.7: battles, sieges, sackings and
// captures, conquest, the end of a war, peace, truce and cession.
//
// The wars of a step are visited in declaration order by index (no war is
// declared in T6 or Q4); settlement and relation changes are applied to id
// lists taken before them (P3-06 8.3 item 1).

#include <algorithm>
#include <string>
#include <utility>

#include "conflict_internal.hpp"
#include "worldgen/core/checked.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/draw.hpp"

namespace worldgen::history {

using namespace conflict_detail;

namespace {

std::size_t side(const ConflictWar& w, std::int64_t civ) noexcept {
    return civ == w.attacker ? 0 : 1;
}

}  // namespace

// --- T6 (7.3) -----------------------------------------------------------------------------------

void ConflictSubsystem::fight(StepContext& ctx) {
    const std::size_t n = wars_.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (!wars_[i].open || wars_[i].start == ctx.year()) {
            continue;  // a war has no battle in its declaration year (7.3, r2)
        }
        const Fought f = battle(ctx, i);
        if (wars_[i].open) {
            end_checks(ctx, i, f == Fought::no_front);
        }
    }
}

ConflictSubsystem::Fought ConflictSubsystem::battle(StepContext& ctx, std::size_t i) {
    const ConflictWar w0 = wars_[i];
    std::int64_t x = w0.initiative;
    std::int64_t y = x == w0.attacker ? w0.defender : w0.attacker;
    std::vector<FrontEntry> fr = live_front(ctx, x, y);
    if (fr.empty()) {
        std::swap(x, y);
        fr = live_front(ctx, x, y);
        if (fr.empty()) {
            return Fought::no_front;
        }
    }
    if (!chance_pm(ctx.draw(keys_[BATTLE], word(w0.declared), 0, 0), tuning_.battle_permille)) {
        return Fought::no_battle;
    }
    // 6.2: the target.
    FrontEntry target{};
    bool goal_found = false;
    if (x == w0.attacker) {
        for (const FrontEntry& e : fr) {
            if (e.region == w0.goal) {
                target = e;
                goal_found = true;
            }
        }
    }
    if (!goal_found) {
        target = best_target(ctx, race_of_civ(ctx, x).id, fr);
    }
    const std::int64_t region = target.region;
    // 6.3: the defence.
    DefenceTerms terms;
    terms.cover = cover_of(peoples_.geo().region(region), cover_biomes_);
    std::int64_t pass_id = 0;
    if (target.from != region) {
        const auto it = border_.find(ordered(target.from, region));
        if (it != border_.end()) {
            const db::RegionBorderRow& b = ctx.inputs().region_borders[it->second];
            terms.pass = b.walk_pairs == 0 && b.pass_id > 0;
            pass_id = terms.pass ? b.pass_id : 0;
            terms.river = b.length_m > 0 &&
                          checked_mul(checked_mul(b.river_pairs, 16), 1000) >=
                              checked_mul(peoples_.tuning().river_border_share_permille, b.length_m);
        }
    }
    for (const std::int64_t id : visible_settlements(ctx, y, region)) {
        terms.fort = terms.fort || ctx.state().settlement(id).kind == SettlementKind::fortress;
    }
    const std::int64_t def = defence_of(tuning_, terms);
    // 7.3 items 3 to 6.
    const std::int64_t attack = floor_div(force(ctx, x), std::max<std::int64_t>(1, open_wars_of(x)));
    const std::int64_t defend = floor_div(force(ctx, y), std::max<std::int64_t>(1, open_wars_of(y)));
    const std::int64_t p = battle_permille_of(attack, defend, def);
    const bool x_wins = chance_pm(ctx.draw(keys_[BATTLE_WIN], word(w0.declared), 0, 0), p);
    const std::int64_t winner = x_wins ? x : y;
    const std::int64_t loser = x_wins ? y : x;
    const std::int64_t f_winner = x_wins ? attack : defend;
    const std::int64_t f_loser = x_wins ? defend : attack;
    const std::int64_t dead_loser = floor_div(checked_mul(f_loser, tuning_.loss_loser_permille), 1000);
    const std::int64_t dead_winner = floor_div(checked_mul(f_winner, tuning_.loss_winner_permille), 1000);
    EventSpec e = point_event(ctx, terms.pass ? "battle_at_pass" : "battle", region, 0);
    e.participants = {civ_role("winner", winner), civ_role("loser", loser)};
    e.values = {dead_winner, dead_loser};
    if (terms.pass) {
        e.values.push_back(pass_id);
    }
    e.causes = {w0.declared};
    const std::int64_t ev = ctx.append(e);
    const std::int64_t dw = casualties(ctx, winner, dead_winner, ev);
    const std::int64_t dl = casualties(ctx, loser, dead_loser, ev);
    {
        ConflictWar& w = wars_[i];
        w.dead[side(w, winner)] = checked_add(w.dead[side(w, winner)], dw);
        w.dead[side(w, loser)] = checked_add(w.dead[side(w, loser)], dl);
        w.won[side(w, winner)] += 1;
        w.last_event = ev;
        w.initiative = winner;
    }
    if (x_wins && !visible_settlements(ctx, y, region).empty()) {
        siege(ctx, i, x, y, region, attack, dw, ev);
    }
    return Fought::battle;
}

void ConflictSubsystem::siege(StepContext& ctx, std::size_t i, std::int64_t x, std::int64_t y, std::int64_t region,
                              std::int64_t attack, std::int64_t dead_x, std::int64_t battle_event) {
    const State& state = ctx.state();
    std::int64_t t = 0;
    for (const std::int64_t id : visible_settlements(ctx, y, region)) {
        if (t == 0 || state.settlement(id).population > state.settlement(t).population) {
            t = id;
        }
    }
    const std::int64_t population = state.settlement(t).population;
    const SettlementKind kind = state.settlement(t).kind;
    const bool capital = state.civ(y).capital == t;
    const std::int64_t garrison = garrison_of(tuning_, population, race_of_civ(ctx, y).prowess, kind, capital);
    const std::int64_t left = std::max<std::int64_t>(0, attack - dead_x);
    const std::int64_t ps = siege_permille_of(left, garrison);
    const std::int64_t w = wars_[i].declared;
    bool fell = chance_pm(ctx.draw(keys_[SIEGE], word(w), word(t), 0), ps);
    if (fell && tuning_.last_refuge != 0) {
        // The last settlement of the last civilization of its race never falls.
        const std::int64_t race = state.civ(y).race;
        bool other_civ = false;
        const std::int64_t nc = civ_count(state);
        for (std::int64_t c = 1; c <= nc; ++c) {
            other_civ = other_civ || (c != y && !state.civ(c).ended_year && state.civ(c).race == race);
        }
        std::int64_t standing_count = 0;
        const std::int64_t ns = settlement_count(state);
        for (std::int64_t id = 1; id <= ns; ++id) {
            standing_count += (state.settlement(id).owner_civ == y && standing(state.settlement(id))) ? 1 : 0;
        }
        fell = other_civ || standing_count != 1;
    }
    EventSpec e = point_event(ctx, "siege", region, t);
    e.participants = {civ_role("attacker", x), civ_role("defender", y)};
    e.values = {fell ? 1 : 0};
    e.causes = {battle_event};
    const std::int64_t ev = ctx.append(e);
    wars_[i].last_event = ev;
    if (fell) {
        fall(ctx, i, x, y, t, ev);
    }
}

// --- Falls (7.5) and conquest (7.6) -----------------------------------------------------------------

void ConflictSubsystem::fall(StepContext& ctx, std::size_t i, std::int64_t x, std::int64_t y, std::int64_t t,
                             std::int64_t siege_event) {
    State& state = ctx.state();
    if (!standing(state.settlement(t)) || state.settlement(t).owner_civ == 0) {
        fail("history: conflict: ruin of settlement " + int_text(t) + ", which is not standing");
    }
    const std::int64_t region = state.settlement(t).region;
    const bool capital = state.civ(y).capital == t;
    const std::int64_t w = wars_[i].declared;
    const std::int64_t h = hostility(disposition_of(ctx, x, y));
    const std::int64_t sack_pm =
        std::min<std::int64_t>(1000, checked_add(tuning_.sack_base, floor_div(checked_mul(h, tuning_.sack_hostility), 1000)));
    const bool sack = chance_pm(ctx.draw(keys_[SACK], word(w), word(t), 0), sack_pm);
    std::int64_t ev = 0;
    if (sack) {
        const std::int64_t p = state.settlement(t).population;
        const std::int64_t dead = floor_div(checked_mul(p, tuning_.sack_toll_permille), 1000);
        state.set_population(t, p - dead);
        EventSpec e = point_event(ctx, "sacked", region, t);
        e.participants = {civ_role("attacker", x), civ_role("defender", y)};
        e.values = {dead, p - dead};
        e.causes = {siege_event};
        if (capital) {
            e.salience = tuning_.capital_sacked_salience;
        }
        ev = ctx.append(e);
        peoples_.ruin(ctx, t, ev);
    } else {
        const std::int64_t p = state.settlement(t).population;
        const std::int64_t dead = floor_div(checked_mul(p, tuning_.capture_toll_permille), 1000);
        const std::int64_t after = p - dead;
        const std::int64_t room = checked_mul(2, peoples_.view().capacity(region, race_of_civ(ctx, x).id));
        const std::int64_t excess = std::max<std::int64_t>(0, after - room);
        EventSpec e = point_event(ctx, "settlement_captured", region, t);
        e.participants = {civ_role("attacker", x), civ_role("defender", y)};
        e.values = {dead, excess};
        e.causes = {siege_event};
        if (capital) {
            e.salience = tuning_.capital_captured_salience;
        }
        ev = ctx.append(e);
        state.set_population(t, after - excess);
        refugees(ctx, y, t, excess);
        state.set_settlement_owner(t, x, ev);
        // The conquest of the region when y keeps no standing settlement there.
        bool left_in_region = false;
        std::int64_t next_capital = 0;  // most populous standing settlement of y, then smaller id
        const std::int64_t ns = settlement_count(state);
        for (std::int64_t id = 1; id <= ns; ++id) {
            const Settlement& s = state.settlement(id);
            if (s.owner_civ != y || !standing(s)) {
                continue;
            }
            left_in_region = left_in_region || s.region == region;
            if (next_capital == 0 || s.population > state.settlement(next_capital).population) {
                next_capital = id;
            }
        }
        if (state.region(region).owner == y && !left_in_region) {
            state.set_region_owner(region, x, ev);
        }
        if (capital && next_capital != 0) {
            const std::int64_t cap_region = state.settlement(next_capital).region;
            EventSpec m = point_event(ctx, "capital_moved", cap_region, next_capital);
            m.participants = {civ_role("civilization", y),
                              Participant{"settlement", EntityType::settlement, next_capital}};
            m.causes = {ev};
            (void)ctx.append(m);
            state.set_capital(y, next_capital);
        }
    }
    {
        ConflictWar& war = wars_[i];
        war.taken[side(war, x)] += 1;
        war.last_event = ev;
    }
    add_grudge(x, y, tuning_.capture_grudge);
    // 7.5 item 5: end what was emptied, in the same step (8.4 item 3).
    bool any = false;
    const std::int64_t ns = settlement_count(state);
    for (std::int64_t id = 1; id <= ns; ++id) {
        any = any || (state.settlement(id).owner_civ == y && standing(state.settlement(id)));
    }
    if (!any) {
        conquer(ctx, y, x, ev, region);
    }
}

void ConflictSubsystem::refugees(StepContext& ctx, std::int64_t civ, std::int64_t from, std::int64_t people) {
    if (people <= 0) {
        return;
    }
    State& state = ctx.state();
    const std::int64_t fx = state.settlement(from).x;
    const std::int64_t fy = state.settlement(from).y;
    std::int64_t nearest = 0;
    std::int64_t nearest_d = 0;
    const std::int64_t ns = settlement_count(state);
    for (std::int64_t id = 1; id <= ns; ++id) {
        const Settlement& t = state.settlement(id);
        if (id == from || t.owner_civ != civ || !standing(t)) {
            continue;
        }
        const std::int64_t dx = t.x - fx;
        const std::int64_t dy = t.y - fy;
        const std::int64_t d = checked_add(checked_mul(dx, dx), checked_mul(dy, dy));
        if (nearest == 0 || d < nearest_d) {
            nearest = id;
            nearest_d = d;
        }
    }
    if (nearest == 0) {
        return;  // lost
    }
    const std::int64_t p = state.settlement(nearest).population;
    const std::int64_t cap = peoples_.view().capacity(state.settlement(nearest).region, race_of_civ(ctx, civ).id);
    const std::int64_t room = std::max<std::int64_t>(0, checked_sub(checked_mul(2, cap), p));
    state.set_population(nearest, checked_add(p, std::min(people, room)));
}

void ConflictSubsystem::close_relations_of(StepContext& ctx, std::int64_t civ, std::int64_t event) {
    State& state = ctx.state();
    std::vector<RelationKey> keys;
    for (const auto& [key, open] : state.relations()) {
        (void)open;
        const bool civs = key.a_type == static_cast<std::int64_t>(EntityType::civilization) &&
                          key.b_type == static_cast<std::int64_t>(EntityType::civilization);
        if (civs && (key.a_id == civ || key.b_id == civ)) {
            keys.push_back(key);
        }
    }
    for (const RelationKey& key : keys) {
        state.close_relation(key, event);
    }
}

void ConflictSubsystem::conquer(StepContext& ctx, std::int64_t y, std::int64_t x, std::int64_t cause,
                                std::int64_t region) {
    State& state = ctx.state();
    EventSpec e = point_event(ctx, "civ_conquered", region, 0);
    e.participants = {civ_role("civilization", y), civ_role("conqueror", x)};
    e.causes = {cause};
    const std::int64_t ev = ctx.append(e);
    state.end_civilization(y, ev);
    const std::int64_t nr = static_cast<std::int64_t>(graph_.size());
    for (std::int64_t r = 1; r <= nr; ++r) {
        if (state.region(r).owner == y) {
            state.set_region_owner(r, 0, ev);
        }
    }
    close_relations_of(ctx, y, ev);
    for (ConflictWar& w : wars_) {
        if (w.open && (w.attacker == y || w.defender == y)) {
            w.open = false;  // over, no peace event
        }
    }
    for (auto it = claims_.begin(); it != claims_.end();) {
        if (it->first.first == y || it->first.second == y) {
            it = claims_.erase(it);
        } else {
            ++it;
        }
    }
}

// --- Casualties (7.4) -------------------------------------------------------------------------------

std::int64_t ConflictSubsystem::casualties(StepContext& ctx, std::int64_t civ, std::int64_t dead, std::int64_t event) {
    State& state = ctx.state();
    const std::vector<std::int64_t> ids = visible_settlements(ctx, civ, 0);
    std::vector<std::int64_t> pops;
    std::int64_t total = 0;
    for (const std::int64_t id : ids) {
        pops.push_back(state.settlement(id).population);
        total = checked_add(total, pops.back());
    }
    if (total == 0 || dead <= 0) {
        return 0;
    }
    const std::vector<std::int64_t> loss = casualty_losses(pops, dead);
    for (std::size_t k = 0; k < ids.size(); ++k) {
        if (loss[k] != 0) {
            state.set_population(ids[k], pops[k] - loss[k]);
        }
        peoples_.note_reduction(ids[k], event);
    }
    return std::min(dead, total);
}

// --- The end of a war (7.7) ---------------------------------------------------------------------------

void ConflictSubsystem::end_checks(StepContext& ctx, std::size_t i, bool no_front) {
    const ConflictWar w = wars_[i];
    const State& state = ctx.state();
    const auto exhausted = [&](std::size_t s) {
        return checked_mul(w.dead[s], 1000) >= checked_mul(tuning_.exhaustion_permille, std::max<std::int64_t>(1, w.pop0[s]));
    };
    const bool ex_a = exhausted(0);
    const bool ex_b = exhausted(1);
    const std::int64_t by_score = score_winner(w);
    if (ex_a || ex_b) {
        const std::int64_t tired = ex_a ? w.attacker : w.defender;
        if ((ex_a && ex_b) || by_score == 0 || tired != by_score) {
            (void)end_war(ctx, i, by_score);
        } else {
            (void)end_war(ctx, i, 0);  // the exhausted side leads: a truce
        }
        return;
    }
    // Aim achieved: the defender neither holds the goal nor has a standing settlement in it.
    bool defender_there = state.region(w.goal).owner == w.defender;
    const std::int64_t ns = settlement_count(state);
    for (std::int64_t id = 1; id <= ns; ++id) {
        const Settlement& s = state.settlement(id);
        defender_there = defender_there || (s.owner_civ == w.defender && standing(s) && s.region == w.goal);
    }
    if (!defender_there) {
        (void)end_war(ctx, i, w.attacker);
        return;
    }
    if (no_front || ctx.year() - w.start >= tuning_.war_max_years) {
        (void)end_war(ctx, i, by_score);
    }
}

bool ConflictSubsystem::end_war(StepContext& ctx, std::size_t i, std::int64_t winner) {
    State& state = ctx.state();
    const ConflictWar w = wars_[i];
    const bool quiet = ctx.step() == Step::q4;
    const std::int64_t years = ctx.year() - w.start;
    const std::int64_t a = w.attacker;
    const std::int64_t b = w.defender;
    std::int64_t loser = 0;
    bool ceding = false;
    EventSpec e = point_event(ctx, winner != 0 ? "peace" : "truce", 0, 0);
    e.values = {years};
    e.causes = {w.last_event};
    if (winner != 0) {
        loser = winner == a ? b : a;
        const std::int64_t loser_capital = state.civ(loser).capital;
        ceding = winner == a && state.region(w.goal).owner == loser &&
                 (loser_capital == 0 || state.settlement(loser_capital).region != w.goal);
        if (quiet && (ctx.quiet_events_left(winner) < 1 || (ceding && ctx.quiet_events_left(loser) < 1))) {
            return false;  // waits for the next step, still open (7.8)
        }
        e.participants = {civ_role("winner", winner), civ_role("loser", loser)};
    } else {
        const Pair p = ordered(a, b);
        if (quiet && ctx.quiet_events_left(p.first) < 1) {
            return false;
        }
        e.participants = {civ_role("parties", p.first), civ_role("parties", p.second)};
    }
    const std::int64_t ev = ctx.append(e);
    state.close_relation(civ_relation(a, b, "war"), ev);
    state.open_relation(symmetric_relation(a, b, "peace"), ev);
    last_peace_[ordered(a, b)] = Peace{ctx.year(), ev};
    add_grudge(a, b, tuning_.war_grudge);
    drop_pair(a, b);
    wars_[i].open = false;
    if (!ceding) {
        return true;
    }
    // Cession of the goal.
    const std::int64_t goal = w.goal;
    EventSpec c = point_event(ctx, "region_ceded", goal, 0);
    c.participants = {civ_role("from", loser), civ_role("to", winner)};
    c.causes = {ev};
    const std::int64_t cev = ctx.append(c);
    const std::int64_t room = checked_mul(2, peoples_.view().capacity(goal, race_of_civ(ctx, winner).id));
    for (const std::int64_t id : visible_settlements(ctx, loser, goal)) {
        const std::int64_t p = state.settlement(id).population;
        const std::int64_t excess = std::max<std::int64_t>(0, p - room);
        if (excess > 0) {
            state.set_population(id, p - excess);
            refugees(ctx, loser, id, excess);
        }
        state.set_settlement_owner(id, winner, cev);
    }
    state.set_region_owner(goal, winner, cev);
    return true;
}

}  // namespace worldgen::history
