// The mountain depths (spec docs/specs/p5-07-mountain-depths.md sections 4
// to 7): the deepening of mines (6.2), the waking of a deep horror (6.4),
// the gate (6.5), the take of a hold (6.6) and a horror's later takes
// (6.7). Part of MonstersSubsystem; its state is in the Site and Monster
// members (ids only).
//
// P3-04 6.9 as P5-07 7.4 states it: every loop over sites runs over ids up
// to a count taken before it; at most one waking a step, after which the
// loop ends; a settlement is founded only through the conflict subsystem's
// deep_flight, after which nothing held is read. Draws are keyed by site,
// monster and settlement ids and depth levels, never by list positions.

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/draw.hpp"
#include "worldgen/history/monsters.hpp"

namespace worldgen::history {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::uint64_t word(std::int64_t v) noexcept {
    return static_cast<std::uint64_t>(v);
}

// The five streams of P5-07 3, in the order of deep_keys_.
enum DeepStream : std::size_t { DELVE = 0, WAKE_DEEP, KIND, GATE, TAKE };

constexpr std::int64_t ONE_RAW = std::int64_t{1} << 32;
constexpr std::int64_t PPM = 1000000;
constexpr std::int64_t DEEP = 3;  // MonsterKindDef::wake of a deep kind
constexpr std::int64_t MINE = 3;  // site kind

bool chance_scaled(std::uint64_t u, std::int64_t p, std::int64_t scale) {
    const std::int64_t raw = std::min(ONE_RAW, floor_div(checked_mul(std::max<std::int64_t>(0, p), ONE_RAW), scale));
    return chance(u, Fx::from_raw(raw));
}
bool chance_pm(std::uint64_t u, std::int64_t p) {
    return chance_scaled(u, p, 1000);
}
bool chance_ppm(std::uint64_t u, std::int64_t p) {
    return chance_scaled(u, p, PPM);
}

bool standing(const Settlement& s) noexcept {
    return s.status == SettlementStatus::thriving || s.status == SettlementStatus::declining;
}

EventSpec point_event(const StepContext& ctx, std::string_view kind, std::int64_t region, std::int64_t settlement) {
    EventSpec e;
    e.kind = kind;
    e.year = ctx.year();
    e.end_year = ctx.year();
    e.region = region;
    e.settlement = settlement;
    return e;
}

std::int64_t value_of(const std::vector<std::pair<std::int64_t, std::int64_t>>& list, std::int64_t race,
                      std::int64_t absent) {
    for (const auto& [id, v] : list) {
        if (id == race) {
            return v;
        }
    }
    return absent;
}

}  // namespace

std::int64_t MonstersSubsystem::mine_depth(std::int64_t id) const {
    if (id < 1 || static_cast<std::size_t>(id) > sites_.size()) {
        return 0;
    }
    const Site& t = sites_[static_cast<std::size_t>(id - 1)];
    return t.kind == MINE ? t.depth : 0;
}

std::int64_t MonstersSubsystem::prowess_of(std::int64_t race) const {
    return value_of(prowess_, race, 1000);
}

std::int64_t MonstersSubsystem::mining_of(std::int64_t race) const {
    return value_of(mining_, race, 0);
}

bool MonstersSubsystem::deep_last_refuge(const StepContext& ctx, std::int64_t settlement) const {
    const State& state = ctx.state();
    const std::int64_t c = state.settlement(settlement).owner_civ;
    // The one predicate (P3-05 r12 22.7): t is standing and owned by c, so
    // c's race's last refuge is t.
    return c >= 1 && peoples_.race_last_refuge(PeoplesSubsystem::SettlementsNow{state, nullptr, nullptr}, c);
}

// --- 6.2 Depth ------------------------------------------------------------------------------------------

void MonstersSubsystem::deepen(StepContext& ctx) {
    const State& state = ctx.state();
    const bool in_quiet = ctx.step() == Step::q4;
    const std::int64_t y = ctx.year();
    const auto n = static_cast<std::int64_t>(sites_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        const Site& s = site(id);
        if (s.kind != MINE || s.ended_year || s.sealed || s.depth >= 3) {
            continue;
        }
        const std::int64_t c = s.founder;
        if (c < 1 || c > static_cast<std::int64_t>(state.civs().size()) || state.civ(c).ended_year ||
            !ctx.visible(state.civ(c))) {
            continue;
        }
        const Settlement& t = state.settlement(s.settlement);
        if (!standing(t) || !ctx.visible(t) || t.owner_civ != c) {
            continue;
        }
        const std::int64_t mining = mining_of(state.civ(c).race);
        if (mining <= 0) {
            continue;
        }
        const std::int64_t d = s.depth;
        if (y - s.origin_year < tuning_.deep_years[static_cast<std::size_t>(d)]) {
            continue;  // index d: the worked years for depth d + 1
        }
        std::int64_t p = floor_div(checked_mul(tuning_.deep_delve_permille, mining), 1000);
        if (in_quiet) {
            p = std::min<std::int64_t>(1000, checked_mul(p, ctx.years()));
            if (ctx.quiet_events_left(c) <= 0) {
                continue;  // it waits (no draw)
            }
        }
        if (!chance_pm(ctx.draw(deep_keys_[DELVE], word(id), word(d + 1), 0), p)) {
            continue;
        }
        const std::int64_t cause = s.deepen_event != 0 ? s.deepen_event : s.origin_event;
        EventSpec e = point_event(ctx, "delving_deepened", s.region, s.settlement);
        e.participants = {Participant{"civilization", EntityType::civilization, c},
                          Participant{"site", EntityType::site, id}};
        e.values = {d + 1, (id + d + 1) % 3};
        e.causes = {cause};
        const std::int64_t ev = ctx.append(e);
        Site& w = site(id);
        w.depth = d + 1;
        w.deepen_event = ev;
        ++stats_.deepened;
    }
}

// --- 6.4 The waking, 6.5 the gate ----------------------------------------------------------------------

std::int64_t MonstersSubsystem::gate_odds(const StepContext& ctx, std::int64_t civ, std::int64_t people,
                                          std::size_t kind) const {
    const std::int64_t levy = floor_div(checked_mul(people, tuning_.deep_gate_levy_permille), 1000);
    const std::int64_t s = floor_div(checked_mul(levy, prowess_of(ctx.state().civ(civ).race)), 1000);
    if (s <= 0) {
        return 0;
    }
    return std::min(tuning_.deep_seal_max_permille,
                    floor_div(checked_mul(s, 1000), checked_add(s, kinds_[kind].might)));
}

void MonstersSubsystem::deep_waking(StepContext& ctx) {
    const State& state = ctx.state();
    const auto n = static_cast<std::int64_t>(sites_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        if (site(id).kind != MINE || site(id).ended_year || site(id).woken || site(id).sealed || site(id).depth < 1) {
            continue;
        }
        // Ids and values only: the lair below grows the site list.
        const std::int64_t d = site(id).depth;
        const std::int64_t region = site(id).region;
        const std::int64_t deepen_event = site(id).deepen_event;
        const std::int64_t ppm = tuning_.deep_wake_ppm[static_cast<std::size_t>(d - 1)];
        if (ppm <= 0) {
            continue;
        }
        const std::int64_t c = site(id).founder;
        if (c < 1 || c > static_cast<std::int64_t>(state.civs().size()) || state.civ(c).ended_year ||
            !ctx.visible(state.civ(c))) {
            continue;
        }
        const std::int64_t t = site(id).settlement;
        if (!standing(state.settlement(t)) || !ctx.visible(state.settlement(t)) || state.settlement(t).owner_civ != c) {
            continue;
        }
        // 1. The deep kinds with room, ascending id.
        std::vector<std::size_t> room;
        std::vector<std::int64_t> weights;
        std::int64_t total = 0;
        for (std::size_t k = 0; k < kinds_.size(); ++k) {
            if (kinds_[k].wake == DEEP && living_of(k) < kinds_[k].max_alive) {
                room.push_back(k);
                weights.push_back(kinds_[k].wake_rate);
                total = checked_add(total, kinds_[k].wake_rate);
            }
        }
        if (room.empty()) {
            continue;  // no draw
        }
        // 2. The waking chance.
        const std::int64_t p = std::min(PPM, floor_div(checked_mul(ppm, pressure_), 1000));
        if (!chance_ppm(ctx.draw(deep_keys_[WAKE_DEEP], word(id), 0, 0), p)) {
            continue;
        }
        // 3. The kind.
        if (total <= 0) {
            continue;
        }
        const std::size_t k = room[weighted(ctx.draw(deep_keys_[KIND], word(id), 0, 0), weights)];
        // 4. The monster, its name and the gate (decided before any event).
        const auto m = static_cast<std::int64_t>(monsters_.size()) + 1;
        if (m > MAX_MONSTERS) {
            fail("history: bound: monsters exceed 100000");
        }
        Monster mon;
        mon.id = m;
        mon.kind = k;
        mon.name = wild_name(ctx, k, m);
        mon.root = realm_root(mon.name, kinds_[k].name_forms);
        const std::int64_t people = state.settlement(t).population;
        const std::int64_t odds = gate_odds(ctx, c, people, k);
        const auto u = static_cast<std::int64_t>(pick(ctx.draw(deep_keys_[GATE], word(m), 0, 0), 1000));
        bool falls = u >= odds;
        if (falls && deep_last_refuge(ctx, t)) {
            // 6.5: a fall that cannot happen (no flight plan) becomes a seal.
            const std::int64_t dead = floor_div(checked_mul(people, tuning_.deep_toll_permille), 1000);
            falls = conflict_ != nullptr && conflict_->deep_flight_plan(ctx, c, t, people - dead, region).has_value();
        }
        // 5. The lair.
        const auto l = static_cast<std::int64_t>(sites_.size()) + 1;
        if (l > MAX_SITES) {
            fail("history: bound: sites exceed 1000000");
        }
        std::string lair = lair_name(ctx, l, k, mon.root);
        const std::int64_t tx = state.settlement(t).x;
        const std::int64_t ty = state.settlement(t).y;
        // 6. The waking.
        EventSpec e = point_event(ctx, "deep_woken", region, t);
        e.participants = {Participant{"monster", EntityType::monster, m},
                          Participant{"civilization", EntityType::civilization, c},
                          Participant{"site", EntityType::site, id}, Participant{"lair", EntityType::site, l}};
        e.values = {d, m % 3};
        e.causes = {deepen_event};
        const std::int64_t ev = ctx.append(e);
        Site x;
        x.id = l;
        x.kind = 1;
        x.name = std::move(lair);
        x.region = region;
        x.x = tx;
        x.y = ty;
        x.settlement = falls ? t : 0;
        x.monster = m;
        x.origin_year = ctx.year();
        x.origin_event = ev;
        sites_.push_back(std::move(x));
        ++lair_count_[static_cast<std::size_t>(region - 1)];
        if (falls) {
            ++shunned_count_[static_cast<std::size_t>(region - 1)];
        }
        mon.status = MonsterStatus::awake;
        mon.born = ctx.year();
        mon.born_event = ev;
        mon.lair = l;
        mon.hops = hops_from(region);
        mon.waking_event = ev;
        mon.waking_realm = c;
        mon.deep_event = ev;
        monsters_.push_back(std::move(mon));
        site(id).woken = true;
        ++stats_.deep_woken;
        // 7. The gate's outcome.
        if (!falls || !deep_take(ctx, m, t, ev)) {
            if (falls) {
                fail("history: monsters: the fall of settlement " + db::format_i64(t) + " found no flight");
            }
            EventSpec g = point_event(ctx, "deep_sealed", region, t);
            g.participants = {Participant{"civilization", EntityType::civilization, c},
                              Participant{"monster", EntityType::monster, m},
                              Participant{"site", EntityType::site, id}, Participant{"lair", EntityType::site, l}};
            g.values = {(m + 1) % 3};
            g.causes = {ev};
            const std::int64_t sealed = ctx.append(g);
            Monster& w = monster(m);
            w.status = MonsterStatus::asleep;
            w.sleeping_event = sealed;
            w.sealed = true;
            site(id).sealed = true;
            ++stats_.deep_sealed;
        }
        return;  // at most one waking a step
    }
}

// --- 6.6 The take ----------------------------------------------------------------------------------------

bool MonstersSubsystem::deep_take(StepContext& ctx, std::int64_t id, std::int64_t t, std::int64_t cause) {
    State& state = ctx.state();
    const std::int64_t c = monster(id).waking_realm;
    if (!standing(state.settlement(t)) || state.settlement(t).owner_civ != c) {
        fail("history: monsters: take of settlement " + db::format_i64(t) + ", which is not standing");
    }
    const std::int64_t p = state.settlement(t).population;
    const std::int64_t dead = floor_div(checked_mul(p, tuning_.deep_toll_permille), 1000);
    const std::int64_t survivors = p - dead;
    // 2. Last refuge: a flight, else no take.
    std::optional<DeepFlightPlan> plan;
    if (deep_last_refuge(ctx, t)) {
        if (conflict_ != nullptr) {
            plan = conflict_->deep_flight_plan(ctx, c, t, survivors, lair_region(monster(id)));
        }
        if (!plan) {
            return false;
        }
    }
    // 3. The event; the open mine at t, if any.
    std::int64_t mine = 0;
    for (const Site& x : sites_) {  // ascending id
        if (mine == 0 && x.kind == MINE && !x.ended_year && x.settlement == t) {
            mine = x.id;
        }
    }
    EventSpec e = point_event(ctx, "hold_lost", state.settlement(t).region, t);
    e.participants = {Participant{"monster", EntityType::monster, id},
                      Participant{"civilization", EntityType::civilization, c},
                      Participant{"lair", EntityType::site, monster(id).lair}};
    if (mine != 0) {
        e.participants.push_back(Participant{"site", EntityType::site, mine});
    }
    e.values = {dead, survivors, (id + t) % 3};
    e.causes = {cause};
    const std::int64_t ev = ctx.append(e);
    if (dead > 0) {
        state.set_population(t, survivors);
    }
    // 4. The flight (the hidden refuge, the ruin), else the ruin by the ledger.
    if (plan) {
        conflict_->deep_flight(ctx, *plan, ev);
        ++stats_.deep_flights;
    } else {
        peoples_.ruin(ctx, t, ev);
    }
    // 5.
    Monster& m = monster(id);
    m.takes += 1;
    m.holds_settlement = true;
    ++stats_.holds_lost;
    return true;
}

// --- 6.7 The horror's years: a later take after a raid ---------------------------------------------------

void MonstersSubsystem::deep_after_raid(StepContext& ctx, std::int64_t id, std::int64_t t, std::int64_t raid_event) {
    const State& state = ctx.state();
    if (monster(id).sealed || monster(id).waking_realm == 0) {
        return;
    }
    const std::int64_t c = monster(id).waking_realm;
    const std::int64_t region = state.settlement(t).region;
    if (!standing(state.settlement(t)) || state.settlement(t).owner_civ != c || state.civ(c).ended_year ||
        region < 1 || region > region_count_) {
        return;
    }
    const std::int64_t h = monster(id).hops[static_cast<std::size_t>(region - 1)];
    if (h < 0 || h > tuning_.deep_reach_hops || monster(id).takes > tuning_.deep_takes_max) {
        return;
    }
    const std::int64_t overrun_permille = kinds_[monster(id).kind].overrun_permille;
    const std::int64_t base = std::min<std::int64_t>(1000, floor_div(checked_mul(overrun_permille, pressure_), 1000));
    const std::int64_t f_p = std::min<std::int64_t>(
        1000,
        floor_div(checked_mul(tuning_.overrun_people_ref, 1000), std::max<std::int64_t>(1, state.settlement(t).population)));
    if (!chance_pm(ctx.draw(deep_keys_[TAKE], word(id), word(t), 0), floor_div(checked_mul(base, f_p), 1000))) {
        return;
    }
    (void)deep_take(ctx, id, t, raid_event);
}

}  // namespace worldgen::history
