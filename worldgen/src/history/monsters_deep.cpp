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
#include "worldgen/history/relations.hpp"

namespace worldgen::history {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::uint64_t word(std::int64_t v) noexcept {
    return static_cast<std::uint64_t>(v);
}

// The nine streams of P5-07 3 (r2), in the order of deep_keys_.
enum DeepStream : std::size_t { DELVE = 0, WAKE_DEEP, KIND, GATE, TAKE, AID, WARREN_DELVE, WARREN_WAKE, WARREN_KIND };

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

// --- 6.10 (r2) Goblin deep warrens: the delving ----------------------------------------------------------

std::int64_t MonstersSubsystem::delving_of(std::int64_t race) const {
    return value_of(delving_, race, 0);
}

std::int64_t MonstersSubsystem::ally_join_of(std::int64_t race) const {
    return value_of(ally_join_, race, 0);
}

std::int64_t MonstersSubsystem::warren_depth(std::int64_t settlement) const {
    if (settlement < 1 || static_cast<std::size_t>(settlement) > warren_depth_.size()) {
        return 0;
    }
    return warren_depth_[static_cast<std::size_t>(settlement - 1)];
}

void MonstersSubsystem::grow_warrens(std::size_t settlements) {
    if (warren_depth_.size() < settlements) {
        warren_depth_.resize(settlements, 0);
        warren_delver_.resize(settlements, 0);
        warren_event_.resize(settlements, 0);
        warren_woken_.resize(settlements, 0);
        warren_sealed_.resize(settlements, 0);
    }
}

void MonstersSubsystem::warren_delving(StepContext& ctx) {
    const State& state = ctx.state();
    const bool in_quiet = ctx.step() == Step::q4;
    const std::int64_t y = ctx.year();
    const auto n = static_cast<std::int64_t>(state.settlements().size());
    const auto n_civs = static_cast<std::int64_t>(state.civs().size());
    grow_warrens(static_cast<std::size_t>(n));
    // The realms with a live warren (a standing settlement of theirs that
    // they delved to depth 1 or more and that is not sealed), kept up to date
    // within the loop, so the first candidate by id wins (14).
    std::vector<std::uint8_t> live(static_cast<std::size_t>(n_civs) + 1, 0);
    for (std::int64_t u = 1; u <= n; ++u) {
        const auto ui = static_cast<std::size_t>(u - 1);
        const Settlement& su = state.settlement(u);
        const std::int64_t c = warren_delver_[ui];
        if (standing(su) && c >= 1 && c <= n_civs && su.owner_civ == c && warren_depth_[ui] >= 1 &&
            warren_sealed_[ui] == 0) {
            live[static_cast<std::size_t>(c)] = 1;
        }
    }
    for (std::int64_t t = 1; t <= n; ++t) {
        const auto ti = static_cast<std::size_t>(t - 1);
        const Settlement& st = state.settlement(t);
        if (!standing(st) || !ctx.visible(st)) {
            continue;
        }
        const std::int64_t c = st.owner_civ;
        if (c < 1 || c > n_civs || state.civ(c).ended_year || !ctx.visible(state.civ(c))) {
            continue;
        }
        const std::int64_t race = state.civ(c).race;
        const std::int64_t delving = delving_of(race);
        if (mining_of(race) != 0 || delving <= 0) {
            continue;
        }
        const std::int64_t d = warren_depth_[ti];
        if (warren_woken_[ti] != 0 || warren_sealed_[ti] != 0 || d >= 3) {
            continue;
        }
        if (d == 0) {
            if (st.population < tuning_.deep_warren_population_min || st.region < 1 || st.region > region_count_ ||
                warren_region_[static_cast<std::size_t>(st.region - 1)] == 0 || live[static_cast<std::size_t>(c)] != 0) {
                continue;
            }
        } else if (warren_delver_[ti] != c) {
            continue;  // a warren taken by another realm is frozen
        }
        if (st.owner_event < 1) {
            continue;
        }
        if (y - ctx.event_year(st.owner_event) < tuning_.deep_years[static_cast<std::size_t>(d)]) {
            continue;
        }
        std::int64_t p = floor_div(checked_mul(tuning_.deep_delve_permille, delving), 1000);
        if (in_quiet) {
            p = std::min<std::int64_t>(1000, checked_mul(p, ctx.years()));
            if (ctx.quiet_events_left(c) <= 0) {
                continue;  // it waits (no draw)
            }
        }
        if (!chance_pm(ctx.draw(deep_keys_[WARREN_DELVE], word(t), word(d + 1), 0), p)) {
            continue;
        }
        const std::int64_t cause = warren_event_[ti] != 0 ? warren_event_[ti] : st.owner_event;
        const std::int64_t region = st.region;
        EventSpec e = point_event(ctx, "warren_delved", region, t);
        e.participants = {Participant{"civilization", EntityType::civilization, c}};
        e.values = {d + 1, (t + d + 1) % 3};
        e.causes = {cause};
        const std::int64_t ev = ctx.append(e);
        warren_depth_[ti] = d + 1;
        warren_delver_[ti] = c;
        warren_event_[ti] = ev;
        live[static_cast<std::size_t>(c)] = 1;
        ++stats_.warrens_delved;
    }
}

// --- 6.4 The waking, 6.5 the gate, 6.11 (r2) the allies' aid ----------------------------------------------

std::int64_t MonstersSubsystem::gate_odds(const StepContext& ctx, std::int64_t civ, std::int64_t people,
                                          std::size_t kind, std::int64_t aid) const {
    const std::int64_t levy = floor_div(checked_mul(people, tuning_.deep_gate_levy_permille), 1000);
    const std::int64_t s =
        checked_add(floor_div(checked_mul(levy, prowess_of(ctx.state().civ(civ).race)), 1000), aid);
    if (s <= 0) {
        return 0;
    }
    return std::min(tuning_.deep_seal_max_permille,
                    floor_div(checked_mul(s, 1000), checked_add(s, kinds_[kind].might)));
}

std::vector<MonstersSubsystem::DeepAid> MonstersSubsystem::deep_aid(const StepContext& ctx, std::int64_t m,
                                                                    std::int64_t c,
                                                                    const std::vector<std::int64_t>& hops) const {
    std::vector<DeepAid> out;
    if (alliances_ == nullptr || relations_ == nullptr || tuning_.deep_aid_permille <= 0 || tuning_.deep_aid_max <= 0) {
        return out;
    }
    const State& state = ctx.state();
    const auto n_civs = static_cast<std::int64_t>(state.civs().size());
    const std::span<const std::int64_t> view = alliances_->allies(c);
    const std::vector<std::int64_t> allies(view.begin(), view.end());  // a local copy, ascending
    const auto n = static_cast<std::int64_t>(state.settlements().size());
    for (const std::int64_t a : allies) {
        if (static_cast<std::int64_t>(out.size()) >= tuning_.deep_aid_max) {
            break;
        }
        if (a < 1 || a > n_civs || a == c || state.civ(a).ended_year || !ctx.visible(state.civ(a))) {
            continue;
        }
        // q: a's standing visible settlement within deep_aid_hops of the hold,
        // the largest by population, ties the smaller id.
        std::int64_t q = 0;
        std::int64_t best = -1;
        for (std::int64_t id = 1; id <= n; ++id) {
            const Settlement& sq = state.settlement(id);
            if (sq.owner_civ != a || !standing(sq) || !ctx.visible(sq) || sq.region < 1 || sq.region > region_count_) {
                continue;
            }
            const std::int64_t h = hops[static_cast<std::size_t>(sq.region - 1)];
            if (h < 0 || h > tuning_.deep_aid_hops) {
                continue;
            }
            if (sq.population > best) {
                best = sq.population;
                q = id;
            }
        }
        if (q == 0) {
            continue;  // no draw
        }
        const std::int64_t p = floor_div(checked_mul(tuning_.deep_aid_permille, ally_join_of(state.civ(a).race)), 1000);
        if (!chance_pm(ctx.draw(deep_keys_[AID], word(m), word(a), 0), p)) {
            continue;
        }
        const std::int64_t levy = floor_div(checked_mul(best, tuning_.deep_aid_levy_permille), 1000);
        out.push_back(DeepAid{a, q, floor_div(checked_mul(levy, prowess_of(state.civ(a).race)), 1000)});
    }
    return out;
}

bool MonstersSubsystem::deep_wake_at(StepContext& ctx, const DeepHold& hold) {
    const State& state = ctx.state();
    const bool warren = hold.site == 0;
    const std::int64_t c = hold.civ;
    const std::int64_t t = hold.settlement;
    const std::int64_t region = hold.region;
    const std::int64_t key_a = warren ? t : hold.site;
    const std::int64_t ppm = tuning_.deep_wake_ppm[static_cast<std::size_t>(hold.depth - 1)];
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
        return false;  // no draw
    }
    // 2. The waking chance (r2: a warren's at deep_warren_wake_permille of a mine's).
    std::int64_t p = floor_div(checked_mul(ppm, pressure_), 1000);
    if (warren) {
        p = floor_div(checked_mul(p, tuning_.deep_warren_wake_permille), 1000);
    }
    p = std::min(PPM, p);
    if (!chance_ppm(ctx.draw(deep_keys_[warren ? WARREN_WAKE : WAKE_DEEP], word(key_a), 0, 0), p)) {
        return false;
    }
    // 3. The kind.
    if (total <= 0) {
        return false;
    }
    const std::size_t k = room[weighted(ctx.draw(deep_keys_[warren ? WARREN_KIND : KIND], word(key_a), 0, 0), weights)];
    // 4. The monster, its name, the allies' aid and the gate (decided before any event).
    const auto m = static_cast<std::int64_t>(monsters_.size()) + 1;
    if (m > MAX_MONSTERS) {
        fail("history: bound: monsters exceed 100000");
    }
    Monster mon;
    mon.id = m;
    mon.kind = k;
    mon.name = wild_name(ctx, k, m);
    mon.root = realm_root(mon.name, kinds_[k].name_forms);
    mon.hops = hops_from(region);
    const std::vector<DeepAid> aid = deep_aid(ctx, m, c, mon.hops);
    std::int64_t aid_strength = 0;
    for (const DeepAid& a : aid) {
        aid_strength = checked_add(aid_strength, a.strength);
    }
    const std::int64_t people = state.settlement(t).population;
    const std::int64_t odds = gate_odds(ctx, c, people, k, aid_strength);
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
    // 6. The waking (r2: no site at a warren, the goblin wordings 3 to 5).
    EventSpec e = point_event(ctx, "deep_woken", region, t);
    e.participants = {Participant{"monster", EntityType::monster, m},
                      Participant{"civilization", EntityType::civilization, c}};
    if (!warren) {
        e.participants.push_back(Participant{"site", EntityType::site, hold.site});
    }
    e.participants.push_back(Participant{"lair", EntityType::site, l});
    e.values = {hold.depth, m % 3 + (warren ? 3 : 0)};
    e.causes = {hold.cause};
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
    mon.waking_event = ev;
    mon.waking_realm = c;
    mon.deep_event = ev;
    mon.from_warren = warren;
    monsters_.push_back(std::move(mon));
    if (warren) {
        warren_woken_[static_cast<std::size_t>(t - 1)] = 1;
        ++stats_.warrens_woken;
    } else {
        site(hold.site).woken = true;
    }
    ++stats_.deep_woken;
    // 6.11: one ally_aided_hold per ally of the aid, ascending; the goodwill
    // between different races, with the aid event just appended.
    std::vector<std::int64_t> causes{ev};
    const std::int64_t race_c = state.civ(c).race;
    for (const DeepAid& a : aid) {
        EventSpec g = point_event(ctx, "ally_aided_hold", region, t);
        g.participants = {Participant{"ally", EntityType::civilization, a.ally},
                          Participant{"partner", EntityType::civilization, c},
                          Participant{"monster", EntityType::monster, m},
                          Participant{"origin", EntityType::settlement, a.origin}};
        g.values = {(m + a.ally) % 3};
        g.causes = {ev};
        const std::int64_t aid_event = ctx.append(g);
        causes.push_back(aid_event);
        ++stats_.aids;
        const std::int64_t race_a = ctx.state().civ(a.ally).race;
        if (race_a != race_c) {
            relations_->credit(race_a, race_c, tuning_.deep_aid_memory, aid_event);
        }
    }
    if (!aid.empty()) {
        ++stats_.wakings_aided;
    }
    // 7. The gate's outcome.
    if (!falls || !deep_take(ctx, m, t, causes)) {
        if (falls) {
            fail("history: monsters: the fall of settlement " + db::format_i64(t) + " found no flight");
        }
        EventSpec g = point_event(ctx, warren ? "warren_sealed" : "deep_sealed", region, t);
        g.participants = {Participant{"civilization", EntityType::civilization, c},
                          Participant{"monster", EntityType::monster, m}};
        if (!warren) {
            g.participants.push_back(Participant{"site", EntityType::site, hold.site});
        }
        g.participants.push_back(Participant{"lair", EntityType::site, l});
        g.values = {(m + 1) % 3};
        g.causes = causes;
        const std::int64_t sealed = ctx.append(g);
        Monster& w = monster(m);
        w.status = MonsterStatus::asleep;
        w.sleeping_event = sealed;
        w.sealed = true;
        if (warren) {
            warren_sealed_[static_cast<std::size_t>(t - 1)] = 1;
            ++stats_.warrens_sealed;
        } else {
            site(hold.site).sealed = true;
        }
        ++stats_.deep_sealed;
    }
    return true;
}

void MonstersSubsystem::deep_waking(StepContext& ctx) {
    const State& state = ctx.state();
    const auto n_civs = static_cast<std::int64_t>(state.civs().size());
    const auto n = static_cast<std::int64_t>(sites_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        if (site(id).kind != MINE || site(id).ended_year || site(id).woken || site(id).sealed || site(id).depth < 1) {
            continue;
        }
        const std::int64_t d = site(id).depth;
        if (tuning_.deep_wake_ppm[static_cast<std::size_t>(d - 1)] <= 0) {
            continue;
        }
        const std::int64_t c = site(id).founder;
        if (c < 1 || c > n_civs || state.civ(c).ended_year || !ctx.visible(state.civ(c))) {
            continue;
        }
        const std::int64_t t = site(id).settlement;
        if (!standing(state.settlement(t)) || !ctx.visible(state.settlement(t)) || state.settlement(t).owner_civ != c) {
            continue;
        }
        // Ids and values only: the lair grows the site list.
        if (deep_wake_at(ctx, DeepHold{id, t, c, d, site(id).region, site(id).deepen_event})) {
            return;  // at most one waking a step
        }
    }
    // r2 6.10: the warrens, after every mine candidate.
    const auto ns = static_cast<std::int64_t>(state.settlements().size());
    grow_warrens(static_cast<std::size_t>(ns));
    for (std::int64_t t = 1; t <= ns; ++t) {
        const auto ti = static_cast<std::size_t>(t - 1);
        const std::int64_t d = warren_depth_[ti];
        if (d < 1 || warren_woken_[ti] != 0 || warren_sealed_[ti] != 0 ||
            tuning_.deep_wake_ppm[static_cast<std::size_t>(d - 1)] <= 0) {
            continue;
        }
        const Settlement& st = state.settlement(t);
        const std::int64_t c = warren_delver_[ti];
        if (!standing(st) || !ctx.visible(st) || st.owner_civ != c || c < 1 || c > n_civs || state.civ(c).ended_year ||
            !ctx.visible(state.civ(c))) {
            continue;
        }
        if (deep_wake_at(ctx, DeepHold{0, t, c, d, st.region, warren_event_[ti]})) {
            return;
        }
    }
}

// --- 6.6 The take ----------------------------------------------------------------------------------------

bool MonstersSubsystem::deep_take(StepContext& ctx, std::int64_t id, std::int64_t t,
                                  const std::vector<std::int64_t>& causes) {
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
    e.values = {dead, survivors, (id + t) % 3 + (monster(id).from_warren ? 3 : 0)};
    e.causes = causes;
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
    (void)deep_take(ctx, id, t, std::vector<std::int64_t>{raid_event});
}

}  // namespace worldgen::history
