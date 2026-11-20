// The peoples subsystem of the history (spec P3-05 sections 7 to 10 and 13):
// awakenings, growth, foundings, pressures, abandonment, ruin, secession.
//
// Rules of P3-04 6.9 as 13.1 states them: every loop over civilizations or
// settlements runs over ids up to a count taken before it and re-fetches by
// id after any change; no reference into State is kept across a founding or
// in a member (members hold ids); draws are keyed by ids and the year.

#include "worldgen/history/peoples.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <string>
#include <tuple>
#include <utility>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/history/draw.hpp"
#include "worldgen/history/schedule.hpp"

namespace worldgen::history {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::string int_text(std::int64_t v) {
    return db::format_i64(v);
}

// The twelve streams of 3.3, in registration order.
enum Stream : std::size_t {
    HOME = 0,
    CIVS,
    FOUND,
    OUTWARD,
    FAMINE,
    FAMINE_TOLL,
    PLAGUE,
    PLAGUE_TOLL,
    PLAGUE_SPREAD,
    ABANDON,
    CRISIS,
    SPLIT,
};
constexpr std::array<std::string_view, 12> STREAM_NAMES = {
    streams::hist_peoples_home,         streams::hist_peoples_civs,        streams::hist_peoples_found,
    streams::hist_peoples_outward,      streams::hist_peoples_famine,      streams::hist_peoples_famine_toll,
    streams::hist_peoples_plague,       streams::hist_peoples_plague_toll, streams::hist_peoples_plague_spread,
    streams::hist_peoples_abandon,      streams::hist_peoples_crisis,      streams::hist_peoples_split,
};

constexpr std::array<Step, 6> STEPS = {Step::t1, Step::t2, Step::t4, Step::q1, Step::q3, Step::q4};

constexpr std::int64_t FAR = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t ONE_RAW = std::int64_t{1} << 32;

bool standing(const Settlement& s) noexcept {
    return s.status == SettlementStatus::thriving || s.status == SettlementStatus::declining;
}

bool quiet_step(const StepContext& ctx) noexcept {
    return ctx.step() == Step::q1 || ctx.step() == Step::q3 || ctx.step() == Step::q4;
}

std::uint64_t word(std::int64_t v) noexcept {
    return static_cast<std::uint64_t>(v);
}

// chance_pm and chance_ppm of 3.3.
bool chance_scaled(std::uint64_t u, std::int64_t p, std::int64_t scale) {
    const std::int64_t raw = std::min(ONE_RAW, floor_div(checked_mul(std::max<std::int64_t>(0, p), ONE_RAW), scale));
    return chance(u, Fx::from_raw(raw));
}
bool chance_pm(std::uint64_t u, std::int64_t p) {
    return chance_scaled(u, p, 1000);
}
bool chance_ppm(std::uint64_t u, std::int64_t p) {
    return chance_scaled(u, p, 1000000);
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

std::string_view became_kind(SettlementKind k) noexcept {
    switch (k) {
        case SettlementKind::village: return "became_village";
        case SettlementKind::town: return "became_town";
        case SettlementKind::city: return "became_city";
        case SettlementKind::hamlet:
        case SettlementKind::fortress: break;
    }
    return "";
}

std::int64_t code(SettlementKind k) noexcept {
    return static_cast<std::int64_t>(k);
}

// slot(v, id): the member of id (index id - 1), growing the vector with 0.
std::int64_t& slot(std::vector<std::int64_t>& v, std::int64_t id) {
    if (id < 1) {
        fail("history: peoples: id " + int_text(id) + " below 1");
    }
    const auto at = static_cast<std::size_t>(id - 1);
    if (v.size() <= at) {
        v.resize(at + 1, 0);
    }
    return v[at];
}

std::int64_t get(const std::vector<std::int64_t>& v, std::int64_t id) noexcept {
    const auto at = static_cast<std::size_t>(id - 1);
    return id >= 1 && at < v.size() ? v[at] : 0;
}

std::int64_t settlement_count(const State& state) noexcept {
    return static_cast<std::int64_t>(state.settlements().size());
}

std::int64_t civ_count(const State& state) noexcept {
    return static_cast<std::int64_t>(state.civs().size());
}

}  // namespace

// --- Setup ------------------------------------------------------------------------------------

PeoplesSubsystem::PeoplesSubsystem() : view_(*this) {}

std::span<const Step> PeoplesSubsystem::steps() const {
    return STEPS;
}

void PeoplesSubsystem::prepare(const StepContext& ctx) {
    if (prepared_) {
        return;
    }
    tuning_ = peoples_tuning_from(ctx.content());
    const std::int64_t islet = engine_tuning_from(ctx.content()).islet_max_dry_cells;
    const HistoryInputs& in = ctx.inputs();
    geo_.emplace(in, ctx.content(), tuning_, islet);
    // P3-03 8.2: the registry from the stored names (as the stub, P3-04 7.2).
    for (const db::RegionRow& r : in.regions) {
        registry_.add(r.name);
    }
    for (const db::AgeRow& a : in.ages) {
        registry_.add(a.name);
    }
    for (const db::GodRow& g : in.gods) {
        registry_.add(g.name);
    }
    for (std::size_t i = 0; i < STREAM_NAMES.size(); ++i) {
        keys_[i] = stream_key(in.seed, STREAM_NAMES[i]);
    }
    switch (in.params.event_intensity) {
        case data::EventIntensity::calm: intensity_ = tuning_.intensity_calm; break;
        case data::EventIntensity::normal: intensity_ = tuning_.intensity_normal; break;
        case data::EventIntensity::turbulent: intensity_ = tuning_.intensity_turbulent; break;
    }
    prepared_ = true;
}

const PeoplesGeo& PeoplesSubsystem::geo() const {
    if (!geo_) {
        fail("history: peoples: used before the first step");
    }
    return *geo_;
}

std::string PeoplesSubsystem::next_name(const StepContext& ctx, std::int64_t race, NameKind kind,
                                        std::int64_t entity) {
    const PeoplesRace& r = geo().race(race);
    return make_name(r.naming, r.culture_key, kind, ctx.inputs().seed, word(r.culture), word(entity), registry_);
}

void PeoplesSubsystem::run(StepContext& ctx) {
    prepare(ctx);
    switch (ctx.step()) {
        case Step::t1:
            end_empty_civilizations(ctx);
            grow_years(ctx, 1, true);
            abandon_small(ctx);
            change_kinds(ctx);
            break;
        case Step::t2:
        case Step::q4: foundings(ctx); break;
        case Step::t4: pressures(ctx); break;
        case Step::q1:
            // 9.2: the yearly growth and recovery of T1 L times (invariant 9:
            // a quiet step of L years equals L tick years without pressures),
            // then abandonment and kind changes once.
            grow_years(ctx, ctx.years(), true);
            abandon_small(ctx);
            change_kinds(ctx);
            break;
        case Step::q3: fade_ruins(ctx); break;
        default: break;
    }
}

// --- Shares and lookups (5.2) ----------------------------------------------------------------------

std::int64_t PeoplesSubsystem::race_of_civ(const State& state, std::int64_t civ) const {
    return state.civ(civ).race;
}

std::int64_t PeoplesSubsystem::cap_of(const State& state, const Settlement& s) const {
    return kind_cap(geo().race(race_of_civ(state, s.owner_civ)), s.kind, tuning_);
}

std::vector<std::int64_t> PeoplesSubsystem::shares(const State& state) const {
    const PeoplesGeo& g = geo();
    const std::int64_t n = settlement_count(state);
    std::vector<std::int64_t> weight_of_region(static_cast<std::size_t>(g.region_count()) + 1, 0);
    std::vector<std::int64_t> weight(static_cast<std::size_t>(n), 0);
    for (std::int64_t id = 1; id <= n; ++id) {
        const Settlement& s = state.settlement(id);
        if (!standing(s)) {
            continue;
        }
        const std::int64_t w = state.civ(s.owner_civ).capital == id ? tuning_.capital_weight : 1000;
        weight[static_cast<std::size_t>(id - 1)] = w;
        weight_of_region[static_cast<std::size_t>(s.region)] += w;
    }
    std::vector<std::int64_t> out(static_cast<std::size_t>(n), 0);
    for (std::int64_t id = 1; id <= n; ++id) {
        const Settlement& s = state.settlement(id);
        if (!standing(s)) {
            continue;
        }
        const std::int64_t c = g.capacity(s.region, race_of_civ(state, s.owner_civ));
        out[static_cast<std::size_t>(id - 1)] =
            std::max<std::int64_t>(1, floor_div(checked_mul(c, weight[static_cast<std::size_t>(id - 1)]),
                                                weight_of_region[static_cast<std::size_t>(s.region)]));
    }
    return out;
}

std::int64_t PeoplesSubsystem::share_now(const State& state, std::int64_t settlement) const {
    if (settlement < 1 || settlement > settlement_count(state)) {
        return 0;
    }
    return shares(state)[static_cast<std::size_t>(settlement - 1)];
}

// --- Awakening (7) --------------------------------------------------------------------------------

std::vector<std::int64_t> PeoplesSubsystem::homes(const StepContext& ctx, std::int64_t race) {
    prepare(ctx);
    if (homes_serial_ == static_cast<std::int64_t>(ctx.serial()) && homes_race_ == race) {
        return homes_;
    }
    const PeoplesGeo& g = geo();
    const State& state = ctx.state();
    const std::int64_t nr = g.region_count();
    if (nr < 1) {
        fail("history: peoples: no region");
    }
    // 7.1: (sigma desc, draw asc, id asc).
    std::vector<std::tuple<std::int64_t, std::uint64_t, std::int64_t>> keyed;
    for (std::int64_t r = 1; r <= nr; ++r) {
        keyed.emplace_back(-g.score(r, race), ctx.draw(key(HOME), word(race), word(r), 0), r);
    }
    std::stable_sort(keyed.begin(), keyed.end());  // keys are distinct (the region id)
    std::vector<std::int64_t> order;
    for (const auto& k : keyed) {
        order.push_back(std::get<2>(k));
    }
    const auto non_islet = [&g](std::int64_t r) { return !g.region(r).islet; };
    const auto free = [&state](std::int64_t r) { return state.region(r).owner == 0; };
    const auto eligible = [&](std::int64_t r) {
        return non_islet(r) && free(r) && g.capacity(r, race) >= tuning_.home_min_capacity;
    };
    // 7.2: how many.
    std::int64_t alive = 0;
    for (std::int64_t c = 1; c <= civ_count(state); ++c) {
        alive += state.civ(c).ended_year ? 0 : 1;
    }
    std::int64_t eligible_count = 0;
    for (const std::int64_t r : order) {
        eligible_count += eligible(r) ? 1 : 0;
    }
    std::int64_t n = 1;
    if (const std::optional<std::int64_t> setting = ctx.inputs().params.civs_per_race) {
        n = *setting;
    } else {
        n = 1 + (chance_pm(ctx.draw(key(CIVS), word(race), 0, 0), tuning_.civs_two_permille) ? 1 : 0);
        if (eligible_count < tuning_.regions_per_civ * alive + 2) {
            n = 1;
        }
    }
    std::vector<std::int64_t> out;
    // Civilization 1: the first eligible, else free non-islet, else non-islet, else any region.
    const std::array<std::function<bool(std::int64_t)>, 4> sets = {
        eligible, [&](std::int64_t r) { return non_islet(r) && free(r); }, non_islet, [](std::int64_t) { return true; }};
    for (const auto& in_set : sets) {
        for (const std::int64_t r : order) {
            if (out.empty() && in_set(r)) {
                out.push_back(r);
            }
        }
    }
    // Civilizations 2 to n: eligible, not a home yet, home_min_hops from every earlier home.
    for (std::int64_t j = 2; j <= n; ++j) {
        std::optional<std::int64_t> next;
        for (const std::int64_t r : order) {
            if (next || !eligible(r) || std::find(out.begin(), out.end(), r) != out.end()) {
                continue;
            }
            bool apart = true;
            for (const std::int64_t h : out) {
                const std::int64_t d = g.hops(h)[static_cast<std::size_t>(r - 1)];
                apart = apart && (d < 0 || d >= tuning_.home_min_hops);
            }
            if (apart) {
                next = r;
            }
        }
        if (!next) {
            break;
        }
        out.push_back(*next);
    }
    homes_serial_ = static_cast<std::int64_t>(ctx.serial());
    homes_race_ = race;
    homes_ = out;
    return out;
}

void PeoplesSubsystem::awaken(StepContext& ctx, std::int64_t race, std::int64_t awakening_event) {
    const std::vector<std::int64_t> list = homes(ctx, race);
    const PeoplesGeo& g = geo();
    const PeoplesRace& r = g.race(race);
    State& state = ctx.state();
    for (const std::int64_t home : list) {
        // 1. The civilization.
        const std::int64_t c = civ_count(state) + 1;
        std::string civ_name = next_name(ctx, race, NameKind::civilization, c);
        EventSpec founded = point_event(ctx, "civ_founded", home, 0);
        founded.participants = {Participant{"civilization", EntityType::civilization, c},
                                Participant{"race", EntityType::race, race}};
        founded.causes = {awakening_event};
        const std::int64_t civ_event = ctx.append(founded);
        if (state.found_civilization(race, r.culture, home, std::move(civ_name), civ_event) != c) {
            fail("history: peoples: the civilization id is not the expected one");
        }
        slot(hunger_, c) = 0;
        if (state.region(home).owner == 0) {
            state.set_region_owner(home, c, civ_event);
        }
        // 2. The site (relaxed).
        const std::optional<SiteCell> site = g.site(home, race, state.settlements(), true);
        if (!site) {
            fail("history: peoples: no settlement site in region " + int_text(home));
        }
        // 3. The settlement.
        const std::int64_t s = settlement_count(state) + 1;
        std::string settlement_name = next_name(ctx, race, NameKind::settlement, s);
        const SettlementKind kind = r.builds_kind(SettlementKind::fortress) && g.region(home).upland
                                        ? SettlementKind::fortress
                                        : ladder_kind(r, tuning_.founding_population, tuning_);
        EventSpec settled = point_event(ctx, "settlement_founded", home, s);
        settled.participants = {Participant{"settlement", EntityType::settlement, s},
                                Participant{"founder", EntityType::civilization, c}};
        settled.values = {tuning_.founding_population};
        settled.causes = {civ_event};
        const std::int64_t settlement_event = ctx.append(settled);
        if (state.found_settlement(site->x, site->y, home, c, kind, tuning_.founding_population,
                                   std::move(settlement_name), settlement_event) != s) {
            fail("history: peoples: the settlement id is not the expected one");
        }
        state.set_capital(c, s);
    }
    // Audit A3: the cached homes describe the state before this awakening.
    homes_serial_ = -1;
    homes_race_ = 0;
    homes_.clear();
}

std::int64_t PeoplesAwakening::awakening_place(const StepContext& context, std::int64_t race) {
    return peoples_.homes(context, race).front();
}

void PeoplesAwakening::awaken(StepContext& context, std::int64_t race, std::int64_t awakening_event) {
    peoples_.awaken(context, race, awakening_event);
}

// --- T1 and Q1: growth, recovery, abandonment, kinds (8.2, 8.3, 8.5 (b), 9.1, 9.2) ------------------

void PeoplesSubsystem::end_empty_civilizations(StepContext& ctx) {
    State& state = ctx.state();
    const std::int64_t nc = civ_count(state);
    const std::int64_t ns = settlement_count(state);
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (state.civ(c).ended_year || !ctx.visible(state.civ(c))) {
            continue;
        }
        bool any = false;
        std::int64_t last = 0;  // the settlement ruined last while c owned it, ties to the larger id
        for (std::int64_t s = 1; s <= ns; ++s) {
            const Settlement& t = state.settlement(s);
            any = any || (standing(t) && t.owner_civ == c);
            if (get(ruined_owner_, s) == c &&
                (last == 0 || std::make_pair(get(ruined_year_, s), s) > std::make_pair(get(ruined_year_, last), last))) {
                last = s;
            }
        }
        if (any) {
            continue;
        }
        if (last == 0) {
            fail("history: peoples: civilization " + int_text(c) + " has no standing settlement and none was ruined");
        }
        end_civilization(ctx, c, state.settlement(last).region, last, get(ruin_event_, last));
    }
}

void PeoplesSubsystem::grow_years(StepContext& ctx, std::int64_t years, bool recover_each_year) {
    State& state = ctx.state();
    const std::int64_t n = settlement_count(state);
    // The shares do not change while only populations do.
    const std::vector<std::int64_t> share = shares(state);
    for (std::int64_t year = 0; year < years; ++year) {
        for (std::int64_t id = 1; id <= n; ++id) {
            const Settlement& s = state.settlement(id);
            if (!standing(s) || !ctx.visible(s)) {
                continue;
            }
            const PeoplesRace& r = geo().race(race_of_civ(state, s.owner_civ));
            const std::int64_t p = s.population;
            const std::int64_t next = grow_population(p, share[static_cast<std::size_t>(id - 1)], r.growth_permille,
                                                      cap_of(state, s), tuning_.decline_permille);
            if (next != p) {
                state.set_population(id, next);
            }
            if (recover_each_year) {
                const Settlement& t = state.settlement(id);
                if (t.status == SettlementStatus::declining &&
                    checked_mul(t.population, 1000) >= checked_mul(tuning_.recover_permille, get(before_, id))) {
                    state.set_settlement_status(id, SettlementStatus::thriving, t.founding_event);
                }
            }
        }
    }
}

std::int64_t PeoplesSubsystem::abandon_events(const State& state, std::int64_t settlement) const {
    const Settlement& s = state.settlement(settlement);
    const std::int64_t c = s.owner_civ;
    bool others = false;
    for (std::int64_t id = 1; id <= settlement_count(state); ++id) {
        const Settlement& t = state.settlement(id);
        others = others || (id != settlement && standing(t) && t.owner_civ == c);
    }
    const bool capital = state.civ(c).capital == settlement;
    return 1 + (capital && others ? 1 : 0) + (others ? 0 : 1);
}

void PeoplesSubsystem::abandon_small(StepContext& ctx) {
    State& state = ctx.state();
    const std::int64_t n = settlement_count(state);
    const bool quiet = quiet_step(ctx);
    for (std::int64_t id = 1; id <= n; ++id) {
        const Settlement& s = state.settlement(id);
        if (!standing(s) || !ctx.visible(s) || s.population >= tuning_.abandon_min) {
            continue;
        }
        if (quiet && ctx.quiet_events_left(s.owner_civ) < abandon_events(state, id)) {
            continue;  // waits for the next step (8.5)
        }
        const std::int64_t last = get(last_reduction_, id);
        abandon(ctx, id, last != 0 ? last : s.founding_event);
    }
}

void PeoplesSubsystem::change_kinds(StepContext& ctx) {
    State& state = ctx.state();
    const std::int64_t n = settlement_count(state);
    const bool quiet = quiet_step(ctx);
    for (std::int64_t id = 1; id <= n; ++id) {
        const Settlement& s = state.settlement(id);
        if (!standing(s) || !ctx.visible(s) || s.kind == SettlementKind::fortress) {
            continue;
        }
        const std::int64_t owner = s.owner_civ;
        const std::int64_t p = s.population;
        const SettlementKind k = s.kind;
        const SettlementKind next = ladder_kind(geo().race(race_of_civ(state, owner)), p, tuning_);
        const bool up = code(next) > code(k);
        const bool down = code(next) < code(k) &&
                          checked_mul(p, 1000) < checked_mul(kind_min(k, tuning_), tuning_.kind_shrink_permille);
        if (!up && !down) {
            continue;
        }
        if (quiet && ctx.quiet_events_left(owner) <= 0) {
            continue;  // deferred (10.5)
        }
        EventSpec e = point_event(ctx, up ? became_kind(next) : "settlement_dwindled", s.region, id);
        e.participants = {Participant{"settlement", EntityType::settlement, id},
                          Participant{"civilization", EntityType::civilization, owner}};
        e.values = {p};
        if (up) {
            e.causes = {s.founding_event};
        } else if (const std::int64_t last = get(last_reduction_, id); last != 0) {
            e.causes = {last};
        }
        const std::int64_t ev = ctx.append(e);
        state.set_settlement_kind(id, next, ev);
    }
}

// --- Abandonment and ruin (8.5) ---------------------------------------------------------------------

void PeoplesSubsystem::ruin(StepContext& ctx, std::int64_t settlement, std::int64_t event) {
    prepare(ctx);
    State& state = ctx.state();
    const Settlement& s0 = state.settlement(settlement);
    if (!standing(s0) || s0.owner_civ == 0) {
        fail("history: peoples: ruin: settlement " + int_text(settlement) + " is not standing");
    }
    const std::int64_t c = s0.owner_civ;
    const std::int64_t p = s0.population;
    const std::int64_t region = s0.region;
    const std::int64_t sx = s0.x;
    const std::int64_t sy = s0.y;
    const std::int64_t race = race_of_civ(state, c);
    const std::int64_t n = settlement_count(state);
    // 2. The survivors join the nearest other standing settlement of c.
    std::int64_t nearest = 0;
    std::int64_t nearest_d = 0;
    for (std::int64_t id = 1; id <= n; ++id) {
        const Settlement& t = state.settlement(id);
        if (id == settlement || !standing(t) || t.owner_civ != c) {
            continue;
        }
        const std::int64_t d = (t.x - sx) * (t.x - sx) + (t.y - sy) * (t.y - sy);
        if (nearest == 0 || d < nearest_d) {
            nearest = id;
            nearest_d = d;
        }
    }
    if (nearest != 0) {
        const Settlement& t = state.settlement(nearest);
        const std::int64_t room =
            std::max<std::int64_t>(0, checked_sub(checked_mul(2, geo().capacity(t.region, race)), t.population));
        state.set_population(nearest, checked_add(t.population, std::min(p, room)));
    }
    // 3. The ruin.
    state.set_population(settlement, 0);
    state.set_settlement_status(settlement, SettlementStatus::ruined, event);
    state.set_settlement_owner(settlement, 0, event);
    slot(ruined_owner_, settlement) = c;
    slot(ruined_year_, settlement) = ctx.year();
    slot(ruin_event_, settlement) = event;
    // 4. The region is released when c has no other standing settlement in it.
    bool left_in_region = false;
    std::int64_t capital = 0;  // 5: the most populous other standing settlement, then smaller id
    for (std::int64_t id = 1; id <= n; ++id) {
        const Settlement& t = state.settlement(id);
        if (!standing(t) || t.owner_civ != c) {
            continue;
        }
        left_in_region = left_in_region || t.region == region;
        if (capital == 0 || t.population > state.settlement(capital).population) {
            capital = id;
        }
    }
    if (state.region(region).owner == c && !left_in_region) {
        state.set_region_owner(region, 0, event);
    }
    // 5. The capital moves.
    if (state.civ(c).capital == settlement && capital != 0) {
        const Settlement& t = state.settlement(capital);
        EventSpec e = point_event(ctx, "capital_moved", t.region, capital);
        e.participants = {Participant{"civilization", EntityType::civilization, c},
                          Participant{"settlement", EntityType::settlement, capital}};
        e.causes = {event};
        (void)ctx.append(e);
        state.set_capital(c, capital);
    }
}

void PeoplesSubsystem::end_civilization(StepContext& ctx, std::int64_t civ, std::int64_t region,
                                        std::int64_t settlement, std::int64_t cause) {
    State& state = ctx.state();
    EventSpec e = point_event(ctx, "civ_ended", region, settlement);
    e.participants = {Participant{"civilization", EntityType::civilization, civ}};
    e.causes = {cause};
    const std::int64_t ev = ctx.append(e);
    state.end_civilization(civ, ev);
    const std::int64_t nr = geo().region_count();
    for (std::int64_t r = 1; r <= nr; ++r) {
        if (state.region(r).owner == civ) {
            state.set_region_owner(r, 0, ev);
        }
    }
}

void PeoplesSubsystem::abandon(StepContext& ctx, std::int64_t settlement, std::int64_t cause) {
    State& state = ctx.state();
    const Settlement& s = state.settlement(settlement);
    const std::int64_t c = s.owner_civ;
    const std::int64_t region = s.region;
    // 1. The event.
    EventSpec e = point_event(ctx, "settlement_abandoned", region, settlement);
    e.participants = {Participant{"settlement", EntityType::settlement, settlement},
                      Participant{"civilization", EntityType::civilization, c}};
    e.values = {s.population};
    e.causes = {cause};
    const std::int64_t ev = ctx.append(e);
    // 2 to 5.
    ruin(ctx, settlement, ev);
    // 6. A people with no settlement left ends.
    for (std::int64_t id = 1; id <= settlement_count(state); ++id) {
        const Settlement& t = state.settlement(id);
        if (standing(t) && t.owner_civ == c) {
            return;
        }
    }
    end_civilization(ctx, c, region, settlement, ev);
}

// --- T2 and Q4: foundings (9.3) -------------------------------------------------------------------

std::optional<std::int64_t> PeoplesSubsystem::outward_target(const StepContext& ctx, std::int64_t civ,
                                                             bool require_free, std::vector<std::int64_t>* all) const {
    const PeoplesGeo& g = geo();
    const State& state = ctx.state();
    const Civilization& c = state.civ(civ);
    if (c.capital == 0) {
        return std::nullopt;
    }
    const std::int64_t race = c.race;
    const PeoplesRace& r = g.race(race);
    const std::int64_t reach = tuning_.reach_base + floor_div(r.expansion * tuning_.reach_per_expansion, 1000);
    const std::int64_t smin =
        std::max(tuning_.expand_min_score,
                 floor_div(checked_mul(g.score(c.home_region, race), tuning_.expand_relative_permille), 1000));
    const std::vector<std::int64_t>& from_capital = g.hops(state.settlement(c.capital).region);
    const std::int64_t nr = g.region_count();
    std::vector<std::int64_t> cost(static_cast<std::size_t>(nr) + 1, FAR);
    for (std::int64_t r0 = 1; r0 <= nr; ++r0) {
        if (state.region(r0).owner != civ) {
            continue;
        }
        for (const PeoplesNeighbour& nb : g.neighbours(r0)) {
            const std::int64_t r1 = nb.region;
            const std::int64_t owner = state.region(r1).owner;
            const bool owner_ok = require_free ? owner == 0 : (owner != 0 && owner != civ);
            const std::int64_t d = from_capital[static_cast<std::size_t>(r1 - 1)];
            if (!owner_ok || g.region(r1).islet || d < 0 || d > reach || g.score(r1, race) < smin ||
                g.capacity(r1, race) < tuning_.expand_min_capacity) {
                continue;
            }
            std::int64_t& k = cost[static_cast<std::size_t>(r1)];
            k = std::min(k, nb.cost);
        }
    }
    std::optional<std::int64_t> best;
    std::int64_t best_value = 0;
    for (std::int64_t r1 = 1; r1 <= nr; ++r1) {
        const std::int64_t k = cost[static_cast<std::size_t>(r1)];
        if (k == FAR) {
            continue;
        }
        if (all != nullptr) {
            all->push_back(r1);
        }
        const std::int64_t value = g.score(r1, race) - checked_mul(tuning_.cost_penalty, k);
        if (!best || value > best_value) {
            best = r1;
            best_value = value;
        }
    }
    return best;
}

void PeoplesSubsystem::foundings(StepContext& ctx) {
    State& state = ctx.state();
    const PeoplesGeo& g = geo();
    const bool quiet = ctx.step() == Step::q4;
    const std::int64_t years = ctx.years();
    const std::vector<std::int64_t> share = shares(state);  // the snapshot at the step's start
    const std::int64_t nc = civ_count(state);
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (state.civ(c).ended_year || !ctx.visible(state.civ(c))) {
            continue;
        }
        const std::int64_t race = state.civ(c).race;
        const std::int64_t capital = state.civ(c).capital;
        const PeoplesRace& r = g.race(race);
        // 1, 2. The source: (not the capital, pressure, P, -id) largest among the ready.
        std::int64_t source = 0;
        std::tuple<bool, std::int64_t, std::int64_t, std::int64_t> source_key{};
        const std::int64_t ns = settlement_count(state);
        for (std::int64_t id = 1; id <= ns; ++id) {
            const Settlement& s = state.settlement(id);
            if (s.owner_civ != c || !standing(s) || !ctx.visible(s)) {
                continue;
            }
            const std::int64_t p = s.population;
            const std::int64_t pressure =
                std::max(floor_div(checked_mul(p, 1000), share[static_cast<std::size_t>(id - 1)]),
                         floor_div(checked_mul(p, 1000), std::max<std::int64_t>(1, cap_of(state, s))));  // audit A1: a cap of 0 is legal data
            if (pressure < tuning_.found_pressure_permille) {
                continue;
            }
            const auto k = std::make_tuple(id != capital, pressure, p, -id);
            if (source == 0 || k > source_key) {
                source = id;
                source_key = k;
            }
        }
        if (source == 0) {
            continue;
        }
        const std::int64_t source_region = state.settlement(source).region;
        // 3. Targets.
        std::optional<std::pair<std::int64_t, SiteCell>> inside;
        if (state.region(source_region).owner == c) {
            std::int64_t in_region = 0;
            for (std::int64_t id = 1; id <= ns; ++id) {
                const Settlement& t = state.settlement(id);
                in_region += (standing(t) && t.region == source_region) ? 1 : 0;
            }
            if (in_region < tuning_.region_slots_max) {
                if (const std::optional<SiteCell> site = g.site(source_region, race, state.settlements(), false)) {
                    inside = std::make_pair(source_region, *site);
                }
            }
        }
        std::optional<std::pair<std::int64_t, SiteCell>> outward;
        if (const std::optional<std::int64_t> r1 = outward_target(ctx, c, true, nullptr)) {
            if (const std::optional<SiteCell> site = g.site(*r1, race, state.settlements(), false)) {
                outward = std::make_pair(*r1, *site);
            }
        }
        // 4. Land hunger.
        if (!inside && !outward) {
            slot(hunger_, c) = checked_add(get(hunger_, c), years);
            continue;
        }
        slot(hunger_, c) = 0;
        // 5. The chance.
        std::int64_t p = checked_mul(floor_div(checked_mul(tuning_.found_rate_permille, r.expansion), 1000), years);
        if (quiet) {
            p = floor_div(checked_mul(p, tuning_.quiet_found_permille), 1000);
        }
        if (!chance_pm(ctx.draw(key(FOUND), word(c), 0, 0), std::min<std::int64_t>(1000, p))) {
            continue;
        }
        // 6. The quiet budget.
        if (quiet && ctx.quiet_events_left(c) <= 0) {
            continue;
        }
        // 7. Colonists.
        const std::int64_t source_p = state.settlement(source).population;
        std::int64_t colonists =
            std::max(tuning_.colonists_min, floor_div(checked_mul(source_p, tuning_.colonist_permille), 1000));
        if (source_p - colonists < 2 * tuning_.colonists_min) {
            continue;
        }
        // 8. The choice.
        std::pair<std::int64_t, SiteCell> target = inside ? *inside : *outward;
        if (inside && outward) {
            target = chance_pm(ctx.draw(key(OUTWARD), word(c), 0, 0), r.expansion) ? *outward : *inside;
        }
        // 9. The founding.
        const std::int64_t region = target.first;
        colonists = std::min(colonists, g.capacity(region, race));
        state.set_population(source, source_p - colonists);
        const std::int64_t s = settlement_count(state) + 1;
        std::string name = next_name(ctx, race, NameKind::settlement, s);
        const SettlementKind kind = r.builds_kind(SettlementKind::fortress) && g.region(region).upland
                                        ? SettlementKind::fortress
                                        : ladder_kind(r, colonists, tuning_);
        const bool was_free = state.region(region).owner == 0;
        EventSpec e = point_event(ctx, "colony_founded", region, s);
        e.participants = {Participant{"settlement", EntityType::settlement, s},
                          Participant{"founder", EntityType::civilization, c},
                          Participant{"origin", EntityType::settlement, source}};
        e.values = {colonists};
        if (was_free) {
            e.salience = tuning_.colony_new_region_salience;
        }
        const std::int64_t ev = ctx.append(e);
        if (state.found_settlement(target.second.x, target.second.y, region, c, kind, colonists, std::move(name),
                                   ev) != s) {
            fail("history: peoples: the settlement id is not the expected one");
        }
        if (was_free) {
            state.set_region_owner(region, c, ev);
        }
    }
}

// --- T4: pressures (10) -----------------------------------------------------------------------------

std::int64_t PeoplesSubsystem::strike(StepContext& ctx, std::int64_t settlement, std::string_view kind, std::int64_t toll,
                              std::int64_t cause, std::optional<std::int64_t> salience) {
    State& state = ctx.state();
    const Settlement& s = state.settlement(settlement);
    const std::int64_t p = s.population;
    const std::int64_t dead = floor_div(checked_mul(p, toll), 1000);
    const bool was_declining = s.status == SettlementStatus::declining;
    const SettlementKind settlement_kind = s.kind;
    const std::int64_t owner = s.owner_civ;
    const std::int64_t region = s.region;
    if (!was_declining) {
        slot(before_, settlement) = p;
    }
    const std::int64_t after = p - dead;
    state.set_population(settlement, after);
    EventSpec e = point_event(ctx, kind, region, settlement);
    e.participants = {Participant{"settlement", EntityType::settlement, settlement},
                      Participant{"civilization", EntityType::civilization, owner}};
    e.values = {dead, after};
    if (cause != 0) {
        e.causes = {cause};
    }
    e.salience = salience;
    const std::int64_t ev = ctx.append(e);
    state.set_settlement_status(settlement, SettlementStatus::declining, ev);
    slot(last_reduction_, settlement) = ev;
    // 8.5 (a).
    if (after < tuning_.abandon_min) {
        abandon(ctx, settlement, ev);
    } else if (was_declining && after < tuning_.abandon_frail_max &&
               chance_pm(ctx.draw(key(ABANDON), word(settlement), 0, 0),
                         tuning_.abandon_permille.of(settlement_kind))) {
        abandon(ctx, settlement, ev);
    }
    return ev;
}

void PeoplesSubsystem::pressures(StepContext& ctx) {
    State& state = ctx.state();
    const PeoplesGeo& g = geo();
    const std::vector<std::int64_t> share = shares(state);  // the snapshot at the step's start
    const std::int64_t n = settlement_count(state);
    const std::int64_t famine_rate = floor_div(checked_mul(tuning_.famine_ppm, intensity_), 1000);
    const std::int64_t famine_range = tuning_.famine_toll_max - tuning_.famine_toll_min + 1;
    const std::int64_t plague_range = tuning_.plague_toll_max - tuning_.plague_toll_min + 1;
    std::vector<std::uint8_t> struck(static_cast<std::size_t>(n) + 1, 0);
    for (std::int64_t id = 1; id <= n; ++id) {
        {
            const Settlement& s = state.settlement(id);
            if (!standing(s) || !ctx.visible(s)) {
                continue;
            }
            // 1. Famine.
            const std::int64_t crowding =
                floor_div(checked_mul(s.population, 1000), share[static_cast<std::size_t>(id - 1)]);
            if (crowding >= tuning_.famine_crowding_permille &&
                chance_ppm(ctx.draw(key(FAMINE), word(id), 0, 0), famine_rate)) {
                const std::int64_t toll =
                    tuning_.famine_toll_min +
                    static_cast<std::int64_t>(pick(ctx.draw(key(FAMINE_TOLL), word(id), 0, 0), word(famine_range)));
                (void)strike(ctx, id, "famine", toll, 0, std::nullopt);
                struck[static_cast<std::size_t>(id)] = 1;
            }
        }
        // 2. Plague, if still standing.
        const Settlement& s = state.settlement(id);
        if (!standing(s)) {
            continue;
        }
        const SettlementKind kind = s.kind;
        const std::int64_t region = s.region;
        const std::int64_t rate = floor_div(checked_mul(tuning_.plague_ppm.of(kind), intensity_), 1000);
        if (!chance_ppm(ctx.draw(key(PLAGUE), word(id), 0, 0), rate)) {
            continue;
        }
        const std::int64_t toll =
            tuning_.plague_toll_min +
            static_cast<std::int64_t>(pick(ctx.draw(key(PLAGUE_TOLL), word(id), 0, 0), word(plague_range)));
        const bool small = kind == SettlementKind::hamlet || kind == SettlementKind::village;
        const std::int64_t outbreak =
            strike(ctx, id, "plague", toll, 0,
                   small ? std::optional<std::int64_t>(tuning_.plague_small_salience) : std::nullopt);
        struck[static_cast<std::size_t>(id)] = 1;
        // The spread, to the owner's settlements in the region and its open neighbours.
        const std::int64_t owner = state.settlement(id).owner_civ;  // 0 if the outbreak emptied it
        if (owner == 0) {
            continue;
        }
        std::vector<std::int64_t> regions{region};
        for (const PeoplesNeighbour& nb : g.neighbours(region)) {
            regions.push_back(nb.region);
        }
        for (std::int64_t u = 1; u <= n; ++u) {
            const Settlement& t = state.settlement(u);
            if (u == id || struck[static_cast<std::size_t>(u)] != 0 || !standing(t) || !ctx.visible(t) ||
                t.owner_civ != owner || std::find(regions.begin(), regions.end(), t.region) == regions.end()) {
                continue;
            }
            if (!chance_pm(ctx.draw(key(PLAGUE_SPREAD), word(id), word(u), 0), tuning_.plague_spread_permille)) {
                continue;
            }
            const std::int64_t base =
                tuning_.plague_toll_min +
                static_cast<std::int64_t>(pick(ctx.draw(key(PLAGUE_TOLL), word(u), 1, 0), word(plague_range)));
            (void)strike(ctx, u, "plague_spread", floor_div(checked_mul(base, tuning_.plague_spread_toll_permille), 1000),
                   outbreak, std::nullopt);
            struck[static_cast<std::size_t>(u)] = 1;
        }
    }
    // 10.3: succession crises.
    const std::int64_t nc = civ_count(state);
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (state.civ(c).ended_year || !ctx.visible(state.civ(c))) {
            continue;
        }
        const PeoplesRace& r = g.race(state.civ(c).race);
        const std::int64_t rate = floor_div(
            checked_mul(floor_div(checked_mul(tuning_.crisis_per_generation_permille, 1000), r.generation_years),
                        intensity_),
            1000);
        if (!chance_ppm(ctx.draw(key(CRISIS), word(c), 0, 0), rate)) {
            continue;
        }
        const std::int64_t capital = state.civ(c).capital;
        const Settlement& cap = state.settlement(capital);
        EventSpec e = point_event(ctx, "succession_crisis", cap.region, capital);
        e.participants = {Participant{"civilization", EntityType::civilization, c}};
        crisis(ctx, c, ctx.append(e));
    }
}

void PeoplesSubsystem::crisis(StepContext& ctx, std::int64_t c, std::int64_t event) {
    State& state = ctx.state();
    const PeoplesGeo& g = geo();
    const std::int64_t ns = settlement_count(state);
    const std::int64_t nr = g.region_count();
    std::vector<std::int64_t> mine;
    for (std::int64_t id = 1; id <= ns; ++id) {
        const Settlement& s = state.settlement(id);
        if (s.owner_civ == c && standing(s) && ctx.visible(s)) {
            mine.push_back(id);
        }
    }
    std::vector<std::int64_t> held;
    for (std::int64_t r = 1; r <= nr; ++r) {
        if (state.region(r).owner == c) {
            held.push_back(r);
        }
    }
    const std::int64_t capital = state.civ(c).capital;
    std::vector<std::int64_t> part;
    std::vector<std::int64_t> leaving;
    if (static_cast<std::int64_t>(mine.size()) >= tuning_.split_min_settlements &&
        static_cast<std::int64_t>(held.size()) >= tuning_.split_min_regions &&
        chance_pm(ctx.draw(key(SPLIT), word(c), 0, 0), tuning_.split_permille)) {
        // Hop distances within c's held regions.
        const auto within = [&](std::int64_t from) {
            std::vector<std::int64_t> d(static_cast<std::size_t>(nr) + 1, FAR);
            std::vector<std::int64_t> queue{from};
            d[static_cast<std::size_t>(from)] = 0;
            for (std::size_t head = 0; head < queue.size(); ++head) {
                const std::int64_t u = queue[head];
                for (const PeoplesNeighbour& v : g.neighbours(u)) {
                    std::int64_t& dv = d[static_cast<std::size_t>(v.region)];
                    if (dv == FAR && state.region(v.region).owner == c) {
                        dv = d[static_cast<std::size_t>(u)] + 1;
                        queue.push_back(v.region);
                    }
                }
            }
            return d;
        };
        const std::vector<std::int64_t> d_cap = within(state.settlement(capital).region);
        std::vector<std::int64_t> pop_of(static_cast<std::size_t>(nr) + 1, 0);
        for (const std::int64_t id : mine) {
            pop_of[static_cast<std::size_t>(state.settlement(id).region)] += state.settlement(id).population;
        }
        std::int64_t far = held.front();
        for (const std::int64_t r : held) {
            const auto k = [&](std::int64_t x) {
                return std::make_tuple(d_cap[static_cast<std::size_t>(x)], pop_of[static_cast<std::size_t>(x)], -x);
            };
            if (k(r) > k(far)) {
                far = r;
            }
        }
        const std::vector<std::int64_t> d_far = within(far);
        for (const std::int64_t r : held) {
            if (d_far[static_cast<std::size_t>(r)] < d_cap[static_cast<std::size_t>(r)]) {
                part.push_back(r);
            }
        }
        for (const std::int64_t id : mine) {
            if (std::find(part.begin(), part.end(), state.settlement(id).region) != part.end()) {
                leaving.push_back(id);
            }
        }
    }
    if (!leaving.empty() && leaving.size() < mine.size()) {
        // Secession.
        std::int64_t new_capital = leaving.front();
        for (const std::int64_t id : leaving) {
            if (state.settlement(id).population > state.settlement(new_capital).population) {
                new_capital = id;
            }
        }
        const std::int64_t home = state.settlement(new_capital).region;
        const std::int64_t race = state.civ(c).race;
        const std::int64_t culture = state.civ(c).culture;
        const std::int64_t c2 = civ_count(state) + 1;
        std::string name = next_name(ctx, race, NameKind::civilization, c2);
        EventSpec e = point_event(ctx, "civ_seceded", home, new_capital);
        e.participants = {Participant{"civilization", EntityType::civilization, c2},
                          Participant{"from", EntityType::civilization, c}};
        e.values = {static_cast<std::int64_t>(leaving.size())};
        e.causes = {event};
        const std::int64_t ev = ctx.append(e);
        if (state.found_civilization(race, culture, home, std::move(name), ev) != c2) {
            fail("history: peoples: the civilization id is not the expected one");
        }
        slot(hunger_, c2) = 0;
        state.set_capital(c2, new_capital);
        for (const std::int64_t id : leaving) {
            state.set_settlement_owner(id, c2, ev);
        }
        for (const std::int64_t r : part) {
            state.set_region_owner(r, c2, ev);
        }
        return;
    }
    // No split: the capital loses people and declines.
    const Settlement& cap = state.settlement(capital);
    const std::int64_t p = cap.population;
    if (cap.status == SettlementStatus::thriving) {
        slot(before_, capital) = p;
    }
    state.set_population(capital, p - floor_div(checked_mul(p, tuning_.crisis_loss_permille), 1000));
    state.set_settlement_status(capital, SettlementStatus::declining, event);
    slot(last_reduction_, capital) = event;
}

// --- Q3: ruins fade (8.6) -------------------------------------------------------------------------

void PeoplesSubsystem::fade_ruins(StepContext& ctx) {
    State& state = ctx.state();
    const std::int64_t n = settlement_count(state);
    for (std::int64_t id = 1; id <= n; ++id) {
        const Settlement& s = state.settlement(id);
        if (s.status != SettlementStatus::ruined || !ctx.visible(s) || !s.ruined_year ||
            ctx.year() - *s.ruined_year < tuning_.ruin_fade_years) {
            continue;
        }
        if (ctx.quiet_events_left(0) <= 0) {
            return;  // the rest wait
        }
        EventSpec e = point_event(ctx, "ruin_faded", s.region, id);
        e.participants = {Participant{"settlement", EntityType::settlement, id}};
        e.causes = {s.ruin_event};
        const std::int64_t ev = ctx.append(e);
        state.set_settlement_status(id, SettlementStatus::faded, ev);
    }
}

// --- The seam (13) -----------------------------------------------------------------------------------

void PeoplesSubsystem::note_reduction(std::int64_t settlement, std::int64_t event) {
    slot(last_reduction_, settlement) = event;
}

std::int64_t PeoplesView::score(std::int64_t region, std::int64_t race) const {
    return owner_.geo().score(region, race);
}

std::int64_t PeoplesView::capacity(std::int64_t region, std::int64_t race) const {
    return owner_.geo().capacity(region, race);
}

std::span<const PeoplesNeighbour> PeoplesView::neighbours(std::int64_t region) const {
    return owner_.geo().neighbours(region);
}

std::int64_t PeoplesView::share(const StepContext& context, std::int64_t settlement) const {
    return owner_.share_now(context.state(), settlement);
}

std::int64_t PeoplesView::population(const StepContext& context, std::int64_t civ) const {
    const State& state = context.state();
    std::int64_t sum = 0;
    for (std::int64_t id = 1; id <= settlement_count(state); ++id) {
        const Settlement& s = state.settlement(id);
        sum += (standing(s) && s.owner_civ == civ) ? s.population : 0;
    }
    return sum;
}

std::vector<std::int64_t> PeoplesView::regions(const StepContext& context, std::int64_t civ) const {
    std::vector<std::int64_t> out;
    for (std::int64_t r = 1; r <= owner_.geo().region_count(); ++r) {
        if (context.state().region(r).owner == civ) {
            out.push_back(r);
        }
    }
    return out;
}

std::vector<std::int64_t> PeoplesView::settlements(const StepContext& context, std::int64_t civ) const {
    const State& state = context.state();
    std::vector<std::int64_t> out;
    for (std::int64_t id = 1; id <= settlement_count(state); ++id) {
        const Settlement& s = state.settlement(id);
        if (standing(s) && s.owner_civ == civ) {
            out.push_back(id);
        }
    }
    return out;
}

std::vector<std::int64_t> PeoplesView::wanted_held(const StepContext& context, std::int64_t civ) const {
    std::vector<std::int64_t> out;
    (void)owner_.outward_target(context, civ, false, &out);
    return out;
}

std::int64_t PeoplesView::hunger(std::int64_t civ) const {
    return get(owner_.hunger_, civ);
}

std::int64_t PeoplesView::last_reduction(std::int64_t settlement) const {
    return get(owner_.last_reduction_, settlement);
}

}  // namespace worldgen::history
