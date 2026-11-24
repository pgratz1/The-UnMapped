// The war of spec P3-06 sections 7.3 to 7.7: battles, sieges, sackings and
// captures, conquest, the end of a war, peace, truce and cession.
//
// The wars of a step are visited in declaration order by index (no war is
// declared in T6 or Q4); settlement and relation changes are applied to id
// lists taken before them (P3-06 8.3 item 1).

#include <algorithm>
#include <array>
#include <optional>
#include <tuple>
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
        if (f != Fought::ended && wars_[i].open) {
            withdrawals(ctx, i);  // P5-04 7.9, before 7.7 item 1
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
    // 6.2: the target (no draw; chosen before the search and the battle draw).
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
    // r10 20.17 item 1: y's settlements there all concealed: x must find them.
    // r14 21.7 item 2: a region where this war has fought a battle needs no
    // search for the rest of the war.
    const std::vector<std::int64_t> there = seen_settlements(ctx, y, region);
    const bool fought_here = std::binary_search(w0.fought.begin(), w0.fought.end(), region);
    const bool all_concealed =
        !fought_here && !there.empty() && std::all_of(there.begin(), there.end(), [&](std::int64_t id) {
            return concealed(ctx.state(), ctx.state().settlement(id));
        });
    if (all_concealed) {
        if (!chance_pm(ctx.draw(keys_[SEARCH], word(w0.declared), word(region), 0), tuning_.find_permille)) {
            ConflictWar& w = wars_[i];
            w.unfound += 1;
            if (w.unfound >= tuning_.search_years) {
                abandon_search(ctx, i, x, y, region);
                return Fought::ended;
            }
            return Fought::no_battle;
        }
        wars_[i].unfound = 0;
    }
    if (!chance_pm(ctx.draw(keys_[BATTLE], word(w0.declared), 0, 0), tuning_.battle_permille)) {
        return Fought::no_battle;
    }
    // 6.3: the defence (r9: with home defence).
    const auto [def, pass_id] = defence_at(ctx, y, target);
    const bool at_pass = pass_id != 0;
    // 7.3 items 3 to 6; r9 20.6 item 2: the local forces. P5-04 7.7: each
    // side is its principal and its open allies of that side.
    struct Member {
        std::int64_t civ = 0;
        std::int64_t force = 0;
        std::size_t ally = 0;  // index into the war's allies
    };
    const std::int64_t side_x = x == w0.attacker ? 1 : 0;  // the ally side code of x's side
    std::vector<Member> allies_x;
    std::vector<Member> allies_y;
    std::int64_t attack = local_force(ctx, x, region);
    std::int64_t defend = local_force(ctx, y, region);
    for (std::size_t k = 0; k < w0.allies.size(); ++k) {
        const ConflictAlly& m = w0.allies[k];
        if (!m.open) {
            continue;
        }
        const std::int64_t f =
            floor_div(checked_mul(local_force(ctx, m.civ, region), tuning_.ally_effort_permille), 1000);
        if (f <= 0) {
            continue;
        }
        (m.side == side_x ? allies_x : allies_y).push_back(Member{m.civ, f, k});
        if (m.side == side_x) {
            attack = checked_add(attack, f);
        } else {
            defend = checked_add(defend, f);
        }
    }
    const auto by_id = [](const Member& a, const Member& b) { return a.civ < b.civ; };
    std::stable_sort(allies_x.begin(), allies_x.end(), by_id);
    std::stable_sort(allies_y.begin(), allies_y.end(), by_id);
    const bool allied_battle = !allies_x.empty() || !allies_y.empty();
    const std::int64_t p = battle_permille_of(attack, defend, def);
    const bool x_wins = chance_pm(ctx.draw(keys_[BATTLE_WIN], word(w0.declared), 0, 0), p);
    const std::int64_t winner = x_wins ? x : y;
    const std::int64_t loser = x_wins ? y : x;
    const std::int64_t f_winner = x_wins ? attack : defend;
    const std::int64_t f_loser = x_wins ? defend : attack;
    std::int64_t dead_loser = loser_dead_of(tuning_, f_winner, f_loser);  // r10 20.16
    // r7 (P3-08): the victors lose loss_winner of the smaller force.
    std::int64_t dead_winner =
        floor_div(checked_mul(std::min(f_winner, f_loser), tuning_.loss_winner_permille), 1000);
    // r22 24.5: each side's toll times a keyed factor in [1000 - s, 1000 + s]
    // permille (no draw when s = 0).
    if (const std::int64_t s = tuning_.toll_spread_permille; s > 0) {
        const auto n = static_cast<std::uint64_t>(2 * s + 1);
        const std::int64_t v_loser =
            1000 - s + static_cast<std::int64_t>(pick(ctx.draw(keys_[TOLL], word(w0.declared), 0, 0), n));
        const std::int64_t v_winner =
            1000 - s + static_cast<std::int64_t>(pick(ctx.draw(keys_[TOLL], word(w0.declared), 0, 1), n));
        dead_loser = floor_div(checked_mul(dead_loser, v_loser), 1000);
        dead_winner = floor_div(checked_mul(dead_winner, v_winner), 1000);
    }
    const std::vector<Member>& allies_w = x_wins ? allies_x : allies_y;
    const std::vector<Member>& allies_l = x_wins ? allies_y : allies_x;
    EventSpec e = point_event(ctx, allied_battle ? "allied_battle" : (at_pass ? "battle_at_pass" : "battle"), region, 0);
    e.participants = {civ_role("winner", winner), civ_role("loser", loser)};
    for (const Member& m : allies_w) {
        e.participants.push_back(civ_role("winner_ally", m.civ));
    }
    for (const Member& m : allies_l) {
        e.participants.push_back(civ_role("loser_ally", m.civ));
    }
    e.values = {dead_winner, dead_loser};
    if (at_pass || allied_battle) {
        e.values.push_back(pass_id);
    }
    e.causes = {w0.declared};
    const std::int64_t ev = ctx.append(e);
    // P5-04 7.7: a side's dead split over its allies in ascending id by
    // force (floor), the rest to the principal; winners first.
    std::array<std::int64_t, 2> side_dead{};
    const auto split = [&](const std::vector<Member>& members, std::int64_t principal, std::int64_t total,
                           std::int64_t force_total) {
        std::int64_t given = 0;
        std::int64_t taken = 0;
        for (const Member& m : members) {
            const std::int64_t share = floor_div(checked_mul(total, m.force), std::max<std::int64_t>(1, force_total));
            given = checked_add(given, share);
            const std::int64_t d = casualties(ctx, m.civ, share, ev);
            ConflictAlly& rec = wars_[i].allies[m.ally];
            rec.dead = checked_add(rec.dead, d);
            taken = checked_add(taken, d);
        }
        const std::int64_t d = casualties(ctx, principal, total - given, ev);
        return std::pair<std::int64_t, std::int64_t>{d, checked_add(taken, d)};
    };
    const auto [dw, side_w] = split(allies_w, winner, dead_winner, f_winner);
    const auto [dl, side_l] = split(allies_l, loser, dead_loser, f_loser);
    side_dead[0] = side_w;
    side_dead[1] = side_l;
    {
        ConflictWar& w = wars_[i];
        w.dead[side(w, winner)] = checked_add(w.dead[side(w, winner)], dw);
        w.dead[side(w, loser)] = checked_add(w.dead[side(w, loser)], dl);
        w.won[side(w, winner)] += 1;
        w.last_battle_winner = winner;
        w.last_event = ev;
        w.initiative = winner;
        const auto at = std::lower_bound(w.fought.begin(), w.fought.end(), region);
        if (at == w.fought.end() || *at != region) {
            w.fought.insert(at, region);
        }
    }
    if (x_wins && !seen_settlements(ctx, y, region).empty()) {  // r9: hidden ones are never besieged
        siege(ctx, i, x, y, region, attack, side_dead[0], ev);  // P5-04 7.7: the side's force and dead
    }
    return Fought::battle;
}

void ConflictSubsystem::siege(StepContext& ctx, std::size_t i, std::int64_t x, std::int64_t y, std::int64_t region,
                              std::int64_t attack, std::int64_t dead_x, std::int64_t battle_event) {
    const State& state = ctx.state();
    std::int64_t t = 0;
    for (const std::int64_t id : seen_settlements(ctx, y, region)) {
        if (t == 0 || state.settlement(id).population > state.settlement(t).population) {
            t = id;
        }
    }
    const std::int64_t population = state.settlement(t).population;
    const SettlementKind kind = state.settlement(t).kind;
    const bool capital = state.civ(y).capital == t;
    const ConflictRace& ry = race_of_civ(ctx, y);
    // r9 20.7 item 2: the garrison with home defence.
    const std::int64_t garrison = garrison_of(tuning_, population, ry.prowess, kind, capital, ry.home_defense,
                                              peoples_.view().biome_term(region, ry.id));
    const std::int64_t left = std::max<std::int64_t>(0, attack - dead_x);
    const std::int64_t ps = siege_permille_of(left, garrison);
    const std::int64_t w = wars_[i].declared;
    bool fell = chance_pm(ctx.draw(keys_[SIEGE], word(w), word(t), 0), ps);
    std::optional<Flight> flight;
    std::int64_t held_reason = 0;  // r10 20.22: 1 nowhere to flee, 2 too few to flee
    if (fell && tuning_.last_refuge != 0 && last_refuge_of(state, y)) {
        // r9 20.1: a fallen last refuge flees when it can, else the guard holds.
        fell = false;
        if (tuning_.refuge_flight != 0) {
            flight = flight_plan(ctx, x, y, t);
            fell = flight && flight->people >= peoples_.tuning().abandon_min;
        }
        if (!fell) {
            held_reason = flight ? 2 : 1;
        }
    }
    EventSpec e = point_event(ctx, held_reason != 0 ? "refuge_held" : "siege", region, t);
    e.participants = {civ_role("attacker", x), civ_role("defender", y)};
    e.values = {held_reason != 0 ? held_reason : (fell ? 1 : 0)};
    e.causes = {battle_event};
    const std::int64_t ev = ctx.append(e);
    wars_[i].last_event = ev;
    if (fell && flight) {
        flee(ctx, i, x, y, t, ev, *flight);
    } else if (fell) {
        fall(ctx, i, x, y, t, ev);
    }
}

// --- The flight of a last refuge (r9 20.1) -----------------------------------------------------------

std::optional<ConflictSubsystem::Flight> ConflictSubsystem::flight_plan(const StepContext& ctx, std::int64_t x,
                                                                       std::int64_t y, std::int64_t t) const {
    const State& state = ctx.state();
    const PeoplesView& view = peoples_.view();
    const PeoplesGeo& g = peoples_.geo();
    const std::int64_t race = state.civ(y).race;
    const std::int64_t from = state.settlement(t).region;
    const std::int64_t p = state.settlement(t).population;
    const std::int64_t survivors = p - floor_div(checked_mul(p, tuning_.sack_toll_permille), 1000);
    // P3-06 r14 21.3 (audit A1): a copy of the row, since view.hops(r) below
    // may fill other rows of the lazily filled cache.
    const std::vector<std::int64_t> from_t = view.hops(from);
    const std::vector<std::int64_t> px = presence(ctx, x);
    const std::int64_t far = tuning_.march_hops + 1;
    // (-h_x(D), -sigma(D, r_y), D) ascending.
    std::vector<std::tuple<std::int64_t, std::int64_t, std::int64_t>> order;
    const std::int64_t nr = g.region_count();
    for (std::int64_t d = 1; d <= nr; ++d) {
        const std::int64_t hd = from_t[static_cast<std::size_t>(d - 1)];
        if (state.region(d).owner != 0 || g.region(d).islet || d == from || hd < 0 || hd > tuning_.flight_hops ||
            view.capacity(d, race) < peoples_.tuning().expand_min_capacity) {
            continue;
        }
        std::int64_t h = far;
        for (const std::int64_t r : px) {
            const std::int64_t k = view.hops(r)[static_cast<std::size_t>(d - 1)];
            if (k >= 0) {
                h = std::min(h, k);
            }
        }
        order.emplace_back(-h, -view.score(d, race), d);
    }
    std::stable_sort(order.begin(), order.end());  // keys are distinct (the region id)
    for (const auto& [neg_h, neg_score, d] : order) {
        (void)neg_h;
        (void)neg_score;
        if (const std::optional<SiteCell> site = view.site(ctx, d, race)) {
            const std::int64_t n = std::min(survivors, checked_mul(2, view.capacity(d, race)));
            return Flight{d, *site, n};
        }
    }
    return std::nullopt;
}

void ConflictSubsystem::flee(StepContext& ctx, std::size_t i, std::int64_t x, std::int64_t y, std::int64_t t,
                             std::int64_t siege_event, const Flight& plan) {
    State& state = ctx.state();
    if (!standing(state.settlement(t)) || state.settlement(t).owner_civ != y) {
        fail("history: conflict: flight from settlement " + int_text(t) + ", which is not standing");
    }
    const std::int64_t region = state.settlement(t).region;
    const std::int64_t race = state.civ(y).race;
    // 1. The sack (no sack draw: a refuge that falls is always sacked).
    const std::int64_t p = state.settlement(t).population;
    {
        ConflictWar& war = wars_[i];  // r10 20.15 item 1
        war.taken_people[side(war, x)] = checked_add(war.taken_people[side(war, x)], p);
        war.plunder[side(war, x)] = checked_add(war.plunder[side(war, x)], p);  // P5-04 7.15
    }
    const std::int64_t dead = floor_div(checked_mul(p, tuning_.sack_toll_permille), 1000);
    state.set_population(t, p - dead);
    EventSpec e = point_event(ctx, "sacked", region, t);
    e.participants = {civ_role("attacker", x), civ_role("defender", y)};
    e.values = {dead, p - dead};
    e.causes = {siege_event};
    if (state.civ(y).capital == t) {
        e.salience = tuning_.capital_sacked_salience;
    }
    const std::int64_t sack = ctx.append(e);
    // 2. The refuge. Ids only from here: found_settlement grows the list.
    const std::int64_t n = std::min(p - dead, checked_mul(2, peoples_.view().capacity(plan.region, race)));
    const std::int64_t h = settlement_count(state) + 1;
    std::string name = peoples_.settlement_name(ctx, race, h);
    const SettlementKind kind = peoples_.view().founding_kind(race, plan.region, plan.site, n);
    EventSpec f = point_event(ctx, "refugees_settled", plan.region, h);
    f.participants = {Participant{"settlement", EntityType::settlement, h},
                      Participant{"founder", EntityType::civilization, y},
                      Participant{"origin", EntityType::settlement, t}};
    f.values = {n};
    f.causes = {sack};
    const std::int64_t settled = ctx.append(f);
    if (state.found_settlement(plan.site.x, plan.site.y, plan.region, y, kind, n, std::move(name), settled) != h) {
        fail("history: conflict: the settlement id is not the expected one");
    }
    if (PeopleLedger* ledger = peoples_.ledger(); ledger != nullptr && n >= 1) {
        ledger->moved(state, t, h, n, settled);  // P5-03 7.2
    }
    state.set_region_owner(plan.region, y, settled);
    // r17 22.2: the hidden years of the founder's race.
    const std::int64_t hidden_years = race_of_civ(ctx, y).hidden_years;
    const std::int64_t until = checked_add(ctx.year(), hidden_years);
    hidden_until_[h] = until;
    // r22 24.4: the refuge's hidden years within the history, as a span
    // (kind 42) right after its refugees_settled.
    if (hidden_years >= 1) {
        EventSpec r = point_event(ctx, "refuge_hidden", plan.region, h);
        r.end_year = std::min(until, ctx.inputs().params.span) - 1;
        r.participants = {Participant{"settlement", EntityType::settlement, h},
                          Participant{"founder", EntityType::civilization, y}};
        r.causes = {settled};
        (void)ctx.append(r);
    }
    // r14 21.10 item 1, r17 22.3: y lies low while its refuge stands, is its
    // own and is hidden; peoples holds the record and the one predicate
    // (P3-05 r15 22.7a).
    peoples_.lie_low(y, h, until);
    // 3. The ruin of t; its destination is h, which is full when n was capped.
    state.set_population(t, state.settlement(t).population - n);
    peoples_.ruin(ctx, t, sack);
    // 4. The war.
    {
        ConflictWar& war = wars_[i];
        war.taken[side(war, x)] += 1;
        war.last_event = settled;
    }
    add_grudge(x, y, tuning_.capture_grudge);
    wronged_[Pair{y, x}].insert(region);
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
    {
        ConflictWar& war = wars_[i];  // r10 20.15 item 1: the people taken, before the toll
        war.taken_people[side(war, x)] = checked_add(war.taken_people[side(war, x)], state.settlement(t).population);
    }
    const std::int64_t sack_pm =
        std::min<std::int64_t>(1000, checked_add(tuning_.sack_base, floor_div(checked_mul(h, tuning_.sack_hostility), 1000)));
    const bool sack = chance_pm(ctx.draw(keys_[SACK], word(w), word(t), 0), sack_pm);
    std::int64_t ev = 0;
    if (sack) {
        const std::int64_t p = state.settlement(t).population;
        {
            ConflictWar& war = wars_[i];  // P5-04 7.15: the people at the fall, before the toll
            war.plunder[side(war, x)] = checked_add(war.plunder[side(war, x)], p);
        }
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
        refugees(ctx, y, t, excess, ev);
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
    wronged_[Pair{y, x}].insert(region);  // r9 20.5
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

void ConflictSubsystem::refugees(StepContext& ctx, std::int64_t civ, std::int64_t from, std::int64_t people,
                                 std::int64_t cause) {
    if (people <= 0) {
        return;
    }
    PeopleLedger* ledger = peoples_.ledger();  // P5-03 7.2
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
        if (ledger != nullptr) {
            ledger->displaced(state, from, 0, people, cause);
        }
        return;  // lost
    }
    const std::int64_t p = state.settlement(nearest).population;
    const std::int64_t cap = peoples_.view().capacity(state.settlement(nearest).region, race_of_civ(ctx, civ).id);
    const std::int64_t room = std::max<std::int64_t>(0, checked_sub(checked_mul(2, cap), p));
    const std::int64_t placed = std::min(people, room);
    state.set_population(nearest, checked_add(p, placed));
    if (ledger != nullptr) {
        if (placed >= 1) {
            ledger->moved(state, from, nearest, placed, cause);
        }
        if (people - placed >= 1) {
            ledger->displaced(state, from, 0, people - placed, cause);
        }
    }
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
    const std::size_t nw = wars_.size();
    for (std::size_t k = 0; k < nw; ++k) {
        ConflictWar& w = wars_[k];
        // P5-04 7.11: an ended ally leaves with the ending event.
        for (ConflictAlly& m : w.allies) {
            if (w.open && m.open && m.civ == y) {
                m.open = false;
                m.closed_by = 4;
                m.close_event = ev;
            }
        }
        if (w.open && (w.attacker == y || w.defender == y)) {
            close_allies_at_side_end(ctx, k, y, ev);
            wars_[k].open = false;  // over, no peace event
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
    // r10 20.20 item 1: a race's last refuge keeps at least abandon_min.
    if (last_refuge_of(state, civ)) {
        dead = std::max<std::int64_t>(0, std::min(dead, total - peoples_.tuning().abandon_min));
    }
    if (total == 0 || dead <= 0) {
        return 0;
    }
    const std::vector<std::int64_t> loss = casualty_losses(pops, dead);
    for (std::size_t k = 0; k < ids.size(); ++k) {
        // r14 21.1 (generator 0.19.0): a reduction is noted only where people were lost.
        if (loss[k] != 0) {
            state.set_population(ids[k], pops[k] - loss[k]);
            peoples_.note_reduction(ids[k], event);
        }
    }
    return std::min(dead, total);
}

// --- The end of a war (7.7) ---------------------------------------------------------------------------

EndDecision end_decision(std::int64_t attacker, std::int64_t defender, bool aim, bool defender_there, bool ex_a,
                         bool ex_b, std::int64_t by_score, bool over) noexcept {
    // 20.15 item 4: a peace only when the loser is exhausted.
    const auto loser_exhausted = [&](std::int64_t winner) {
        return winner != 0 && (winner == attacker ? ex_b : (winner == defender && ex_a));
    };
    // 21.6 item 1 (r17 22.5 item 1): aim achieved; no last-battle rule.
    if (aim) {
        return EndDecision{true, attacker, ex_b};
    }
    // 21.6 item 2: the goal lost to no one or to a third party: by the score.
    if (!defender_there) {
        return EndDecision{true, by_score, loser_exhausted(by_score)};
    }
    // 21.6 item 3: 7.7 items 1, 3 and 4.
    if (ex_a || ex_b || over) {
        return EndDecision{true, by_score, loser_exhausted(by_score)};
    }
    return EndDecision{};
}

void ConflictSubsystem::end_checks(StepContext& ctx, std::size_t i, bool no_front) {
    const ConflictWar w = wars_[i];
    const State& state = ctx.state();
    // r9 20.6 item 1: the losses a side bears scale with its muster.
    const bool ex_a = exhausted(ctx, w, 0);
    const bool ex_b = exhausted(ctx, w, 1);
    // r10 20.15: the score of dead and people taken; the winner of the last
    // battle never loses an ending decided by the score.
    const std::int64_t by_score = unless_last_battle(w, score_winner(w));
    // r14 21.6 (generator 0.19.0; replaces 7.7 item 2), in its order: 1.
    // aim achieved (r17 22.5 item 1), 2. the goal lost to no one or to a
    // third party (the defender neither holds it nor has a standing
    // settlement in it), 3. 7.7 items 1, 3 and 4 (end_decision).
    bool defender_there = state.region(w.goal).owner == w.defender;
    const std::int64_t ns = settlement_count(state);
    for (std::int64_t id = 1; id <= ns; ++id) {
        const Settlement& s = state.settlement(id);
        defender_there = defender_there || (s.owner_civ == w.defender && standing(s) && s.region == w.goal);
    }
    const bool over = no_front || ctx.year() - w.start >= tuning_.war_max_years;
    const EndDecision d =
        end_decision(w.attacker, w.defender, aim_achieved(state, w), defender_there, ex_a, ex_b, by_score, over);
    if (d.ends) {
        (void)end_war(ctx, i, d.winner, d.sued);
    }
}

bool ConflictSubsystem::aim_achieved(const State& state, const ConflictWar& w) {
    return state.region(w.goal).owner == w.attacker && standing_settlements_in(state, w.defender, w.goal).empty();
}

bool ConflictSubsystem::end_war(StepContext& ctx, std::size_t i, std::int64_t winner, bool sued) {
    State& state = ctx.state();
    const ConflictWar w = wars_[i];
    const bool quiet = ctx.step() == Step::q4;
    const std::int64_t years = ctx.year() - w.start;
    const std::int64_t a = w.attacker;
    const std::int64_t b = w.defender;
    std::int64_t loser = 0;
    bool ceding = false;
    bool foothold = false;  // r17 22.5 item 3.2
    Cost cost;
    EventSpec e = point_event(ctx, winner == 0 ? "truce" : (sued ? "peace" : "war_won"), 0, 0);
    e.values = {years};
    e.causes = {w.last_event};
    if (winner != 0) {
        loser = winner == a ? b : a;
        const std::int64_t loser_capital = state.civ(loser).capital;
        ceding = winner == a && state.region(w.goal).owner == loser &&
                 (loser_capital == 0 || state.settlement(loser_capital).region != w.goal);
        // r17 22.5 item 3.2: the foothold cession, when the attacker already
        // holds G and the defender stands there, its capital elsewhere.
        if (!ceding && winner == a && state.region(w.goal).owner == winner) {
            const std::vector<std::int64_t> there = standing_settlements_in(state, loser, w.goal);
            foothold = !there.empty() && std::find(there.begin(), there.end(), loser_capital) == there.end();
        }
        if (!ceding && !foothold) {
            cost = cost_plan(ctx, winner, loser);  // r9 20.3
        }
        const bool costs = ceding || foothold || cost.region != 0 || cost.tribute > 0;
        if (quiet && (ctx.quiet_events_left(winner) < 1 || (costs && ctx.quiet_events_left(loser) < 1))) {
            return false;  // waits for the next step, still open (7.8, r9 20.3)
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
    close_war(ctx, i, ev);
    wars_[i].winner = winner;  // P5-04 7.15
    note_futile(ctx, w);  // r9 20.4, at the end event (r10 20.21 item 6)
    if (ceding) {
        cede(ctx, w.goal, loser, winner, ev);  // cession of the goal (7.7)
    } else if (foothold) {
        cede(ctx, w.goal, loser, winner, ev, false);  // r17 22.5 item 3.2
    } else if (cost.region != 0) {
        cede(ctx, cost.region, loser, winner, ev);  // r9 20.3 item 1
    } else if (cost.tribute > 0) {
        // r9 20.3 item 2: tribute, taken from the loser as casualties.
        EventSpec t = point_event(ctx, "tribute", 0, 0);
        t.participants = {civ_role("payer", loser), civ_role("payee", winner)};
        t.values = {cost.tribute};
        t.causes = {ev};
        const std::int64_t tev = ctx.append(t);
        (void)casualties(ctx, loser, cost.tribute, tev);
        wars_[i].tribute_people = cost.tribute;  // P5-04 7.15
        wars_[i].tribute_event = tev;
    }
    close_allies(ctx, i, ev);  // P5-04 7.10: after the costs
    return true;
}

void ConflictSubsystem::close_war(StepContext& ctx, std::size_t i, std::int64_t ev) {
    State& state = ctx.state();
    const std::int64_t a = wars_[i].attacker;
    const std::int64_t b = wars_[i].defender;
    state.close_relation(civ_relation(a, b, "war"), ev);
    state.open_relation(symmetric_relation(a, b, "peace"), ev);
    last_peace_[ordered(a, b)] = Peace{ctx.year(), ev};
    add_grudge(a, b, tuning_.war_grudge);
    drop_pair(a, b);
    wars_[i].open = false;
    wars_[i].end_event = ev;  // P5-04 7.6
}

// P5-04 7.10: the open allies leave at the war's end, each with a peace.
void ConflictSubsystem::close_allies(StepContext& ctx, std::size_t i, std::int64_t ev) {
    State& state = ctx.state();
    const std::size_t n = wars_[i].allies.size();
    for (std::size_t k = 0; k < n; ++k) {
        const ConflictAlly m = wars_[i].allies[k];
        if (!m.open) {
            continue;
        }
        const std::int64_t enemy = m.side == 0 ? wars_[i].attacker : wars_[i].defender;
        state.close_relation(civ_relation(m.civ, enemy, "war"), ev);
        state.open_relation(symmetric_relation(m.civ, enemy, "peace"), ev);
        last_peace_[ordered(m.civ, enemy)] = Peace{ctx.year(), ev};
        add_grudge(m.civ, enemy, tuning_.war_grudge);
        drop_pair(m.civ, enemy);
        ConflictAlly& rec = wars_[i].allies[k];
        rec.open = false;
        rec.closed_by = 1;
        rec.close_event = ev;
    }
}

// P5-04 7.11: a principal ended; the war is over with no peace. Each open
// ally's war row is closed with the ended side's event (already closed
// when the ended side is the ally's enemy).
void ConflictSubsystem::close_allies_at_side_end(StepContext& ctx, std::size_t i, std::int64_t ended,
                                                 std::int64_t ev) {
    State& state = ctx.state();
    ConflictWar& w0 = wars_[i];
    const std::int64_t survivor = ended == w0.attacker ? w0.defender : w0.attacker;
    w0.winner = state.civ(survivor).ended_year ? 0 : survivor;
    w0.end_event = ev;
    const std::size_t n = w0.allies.size();
    for (std::size_t k = 0; k < n; ++k) {
        const ConflictAlly m = wars_[i].allies[k];
        if (!m.open) {
            continue;
        }
        const std::int64_t enemy = m.side == 0 ? wars_[i].attacker : wars_[i].defender;
        const RelationKey key = civ_relation(m.civ, enemy, "war");
        if (state.relations().count(key) != 0) {
            state.close_relation(key, ev);
        }
        ConflictAlly& rec = wars_[i].allies[k];
        rec.open = false;
        rec.closed_by = 3;
        rec.close_event = ev;
    }
}

// P5-04 7.9: a bled-out ally leaves the war with a peace of its own.
void ConflictSubsystem::withdrawals(StepContext& ctx, std::size_t i) {
    State& state = ctx.state();
    const std::size_t n = wars_[i].allies.size();
    for (std::size_t k = 0; k < n; ++k) {
        const ConflictAlly m = wars_[i].allies[k];
        if (!m.open) {
            continue;
        }
        const std::int64_t base = floor_div(checked_mul(m.pop0, race_of_civ(ctx, m.civ).muster), 1000);
        if (checked_mul(m.dead, 1000) < checked_mul(tuning_.exhaustion_permille, std::max<std::int64_t>(1, base))) {
            continue;
        }
        const ConflictWar& w = wars_[i];
        const std::int64_t enemy = m.side == 0 ? w.attacker : w.defender;
        const std::int64_t partner = m.side == 0 ? w.defender : w.attacker;
        EventSpec e = point_event(ctx, "ally_withdrew", 0, 0);
        e.participants = {civ_role("ally", m.civ), civ_role("partner", partner), civ_role("enemy", enemy)};
        e.causes = {w.last_event};
        const std::int64_t ev = ctx.append(e);
        state.close_relation(civ_relation(m.civ, enemy, "war"), ev);
        state.open_relation(symmetric_relation(m.civ, enemy, "peace"), ev);
        last_peace_[ordered(m.civ, enemy)] = Peace{ctx.year(), ev};
        add_grudge(m.civ, enemy, tuning_.war_grudge);
        drop_pair(m.civ, enemy);
        ConflictAlly& rec = wars_[i].allies[k];
        rec.open = false;
        rec.closed_by = 2;
        rec.close_event = ev;
    }
}

// r10 20.17 item 2: the searcher gives up; a truce without cost (T6 only).
void ConflictSubsystem::abandon_search(StepContext& ctx, std::size_t i, std::int64_t x, std::int64_t y,
                                       std::int64_t region) {
    const ConflictWar w = wars_[i];
    EventSpec e = point_event(ctx, "search_abandoned", region, 0);
    e.participants = {civ_role("searcher", x), civ_role("hidden", y)};
    e.values = {ctx.year() - w.start};
    e.causes = {w.last_event};
    // r14 21.7 item 3: only the first search_abandoned of the log keeps the
    // base salience.
    if (search_written_) {
        e.salience = tuning_.search_repeat_salience;
    }
    const std::int64_t ev = ctx.append(e);
    search_written_ = true;
    close_war(ctx, i, ev);
    close_allies(ctx, i, ev);  // P5-04 7.10
    note_futile(ctx, w);  // 20.4: the goal unchanged by an unfound war counts
    // r14 21.7 item 1: R is weary for every claimant against y for
    // search_weary_years, until its holder changes.
    if (tuning_.search_weary_years > 0) {
        searched_until_[Pair{y, region}] =
            Searched{checked_add(ctx.year(), tuning_.search_weary_years), ctx.state().region(region).holder,
                     ctx.state().region(region).holder_changes};
    }
}

// 7.7's cession (and r9 20.3 item 1's): every standing settlement of the
// loser in the region (r13 20.27: visible, hidden or created earlier in the
// step; a stated exception to P3-04 6.2's visibility) passes to the winner,
// its excess over 2 C to the loser's nearest other settlement;
// wronged(loser, winner) gains the region.
void ConflictSubsystem::cede(StepContext& ctx, std::int64_t region, std::int64_t loser, std::int64_t winner,
                             std::int64_t cause, bool move_region) {
    State& state = ctx.state();
    // 8.3 item 1: the id list before any change, each re-fetched by id.
    const std::vector<std::int64_t> ids = standing_settlements_in(state, loser, region);
    // r19 23.3: the foothold cession (the region stays) is kind 40
    // settlements_ceded, naming every settlement that passes, ascending id.
    EventSpec c = point_event(ctx, move_region ? "region_ceded" : "settlements_ceded", region, 0);
    c.participants = {civ_role("from", loser), civ_role("to", winner)};
    if (!move_region) {
        if (ids.empty()) {
            fail("history: conflict: a foothold cession of region " + int_text(region) + " passes no settlement");
        }
        for (std::size_t k = 0; k < ids.size() && k + 2 < MAX_PARTICIPANTS; ++k) {
            c.participants.push_back(Participant{"settlement", EntityType::settlement, ids[k]});
        }
    }
    c.causes = {cause};
    const std::int64_t cev = ctx.append(c);
    const std::int64_t room = checked_mul(2, peoples_.view().capacity(region, race_of_civ(ctx, winner).id));
    for (const std::int64_t id : ids) {
        const std::int64_t p = state.settlement(id).population;
        const std::int64_t excess = std::max<std::int64_t>(0, p - room);
        if (excess > 0) {
            state.set_population(id, p - excess);
            refugees(ctx, loser, id, excess, cev);
        }
        state.set_settlement_owner(id, winner, cev);
    }
    if (move_region) {
        state.set_region_owner(region, winner, cev);
    }
    wronged_[Pair{loser, winner}].insert(region);  // r9 20.5
}

// r9 20.3: the loser's best non-capital region in the winner's front, else
// a tribute (none from a last refuge).
ConflictSubsystem::Cost ConflictSubsystem::cost_plan(const StepContext& ctx, std::int64_t winner,
                                                     std::int64_t loser) const {
    const State& state = ctx.state();
    const std::int64_t capital = state.civ(loser).capital;
    const std::int64_t capital_region = capital != 0 ? state.settlement(capital).region : 0;
    std::vector<FrontEntry> candidates;
    for (const FrontEntry& e : live_front(ctx, winner, loser)) {
        if (state.region(e.region).owner == loser && e.region != capital_region) {
            candidates.push_back(e);
        }
    }
    Cost cost;
    if (!candidates.empty()) {
        cost.region = best_target(ctx, race_of_civ(ctx, winner).id, candidates).region;
        return cost;
    }
    if (last_refuge_of(state, loser)) {
        return cost;
    }
    std::int64_t people = 0;
    for (const std::int64_t id : seen_settlements(ctx, loser, 0)) {
        people = checked_add(people, state.settlement(id).population);
    }
    cost.tribute = floor_div(checked_mul(people, tuning_.tribute_permille), 1000);
    return cost;
}

// r9 20.4: a war over G that leaves G's holder (r14 21.8) as it was at the
// declaration.
void ConflictSubsystem::note_futile(const StepContext& ctx, const ConflictWar& w) {
    const std::int64_t owner = ctx.state().region(w.goal).holder;
    if (owner != w.goal_owner) {
        return;
    }
    const std::int64_t count = futile_count(ctx, w.defender, w.goal);
    futile_[Pair{w.defender, w.goal}] = Futile{checked_add(count, 1), owner, ctx.state().region(w.goal).holder_changes};
}

}  // namespace worldgen::history
