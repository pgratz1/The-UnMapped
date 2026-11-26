// Race relations, mixed settlements and refugee migrations (spec
// docs/specs/p5-03-race-relations.md sections 4 to 8).
//
// Rules of P3-04 6.9 as P5-03 6.6 states them: no founding, no relation;
// State changes only by set_population and PeoplesSubsystem::note_reduction;
// every loop over settlements or civilizations runs over ids up to a count
// taken before it and re-fetches by id after any change; members hold ids.
// The one draw (expulsions) is keyed by settlement and race ids and the
// step's year.

#include "worldgen/history/relations.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/history/draw.hpp"

namespace worldgen::history {

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::uint64_t word(std::int64_t v) noexcept {
    return static_cast<std::uint64_t>(v);
}

constexpr std::array<Step, 5> STEPS = {Step::t1, Step::t4, Step::t6, Step::q1, Step::q4};
constexpr std::int64_t CIV_TYPE = static_cast<std::int64_t>(EntityType::civilization);
constexpr std::int64_t ONE_RAW = std::int64_t{1} << 32;
constexpr std::size_t MAX_CONTRIBUTIONS = 8;

bool chance_pm(std::uint64_t u, std::int64_t p) {
    const std::int64_t raw = std::min(ONE_RAW, floor_div(checked_mul(std::max<std::int64_t>(0, p), ONE_RAW), 1000));
    return chance(u, Fx::from_raw(raw));
}

bool standing_settlement(const Settlement& s) noexcept {
    return s.status == SettlementStatus::thriving || s.status == SettlementStatus::declining;
}

bool quiet(const StepContext& ctx) noexcept {
    return ctx.step() == Step::q1 || ctx.step() == Step::q4;
}

std::int64_t settlement_count(const State& state) noexcept {
    return static_cast<std::int64_t>(state.settlements().size());
}

std::int64_t civ_count(const State& state) noexcept {
    return static_cast<std::int64_t>(state.civs().size());
}

bool at_war(const State& state, std::int64_t a, std::int64_t b) {
    return state.relations().count(RelationKey{CIV_TYPE, a, CIV_TYPE, b, "war"}) != 0 ||
           state.relations().count(RelationKey{CIV_TYPE, b, CIV_TYPE, a, "war"}) != 0;
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

// Causes ascending without repeats and zeros.
std::vector<std::int64_t> causes_of(std::vector<std::int64_t> c) {
    c.erase(std::remove(c.begin(), c.end(), std::int64_t{0}), c.end());
    std::stable_sort(c.begin(), c.end());
    c.erase(std::unique(c.begin(), c.end()), c.end());
    return c;
}

}  // namespace

// --- Data ---------------------------------------------------------------------------------------------

RelationsTuning relations_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::HISTORY_KIND, data::HISTORY_RELATIONS_ID);
    if (d == nullptr) {
        fail("history: tuning: the content has no definition history.relations");
    }
    const data::Record& r = d->record;
    RelationsTuning t;
    t.hatred_max = r.get_int("hatred_max");
    t.friend_min = r.get_int("friend_min");
    t.band_margin = r.get_int("band_margin");
    t.memory_min = r.get_int("memory_min");
    t.memory_max = r.get_int("memory_max");
    t.memory_decay_per_century = r.get_int("memory_decay_per_century");
    t.war_memory = r.get_int("war_memory");
    t.conquest_memory = r.get_int("conquest_memory");
    t.sack_memory = r.get_int("sack_memory");
    t.capture_memory = r.get_int("capture_memory");
    t.expel_memory = r.get_int("expel_memory");
    t.shelter_memory = r.get_int("shelter_memory");
    t.meet_hops = r.get_int("meet_hops");
    t.migrate_hops = r.get_int("migrate_hops");
    t.migrate_min = r.get_int("migrate_min");
    t.welcome_min = r.get_int("welcome_min");
    t.subject_years = r.get_int("subject_years");
    t.conquest_settlers_permille = r.get_int("conquest_settlers_permille");
    t.drift_permille_per_century = r.get_int("drift_permille_per_century");
    t.fade_permille = r.get_int("fade_permille");
    t.mixed_min_permille = r.get_int("mixed_min_permille");
    t.expel_permille = r.get_int("expel_permille");
    t.expel_share_max = r.get_int("expel_share_max");
    t.peace_years = r.get_int("peace_years");  // P5-03a 10.1
    t.peace_memory_per_century = r.get_int("peace_memory_per_century");
    t.peace_memory_max = r.get_int("peace_memory_max");
    t.contact_hops = r.get_int("contact_hops");
    t.foe_years = r.get_int("foe_years");
    t.foe_war_memory = r.get_int("foe_war_memory");
    t.foe_monster_memory = r.get_int("foe_monster_memory");
    t.meet_sealed_hops = r.get_int("meet_sealed_hops");
    t.meet_sealed_years = r.get_int("meet_sealed_years");
    t.conquest_settlers_span_permille = r.get_int("conquest_settlers_span_permille");
    t.expel_gap_years = r.get_int("expel_gap_years");
    t.turn_cooldown_years = r.get_int("turn_cooldown_years");
    t.expel_settled_years = r.get_int("expel_settled_years");
    t.expel_fresh_years = r.get_int("expel_fresh_years");
    return t;
}

std::vector<RaceStance> race_stances_from(const data::ContentSet& content, std::string_view key) {
    std::vector<RaceStance> out;
    const data::Definition* d = content.find(data::RACE_KIND, key);
    if (d == nullptr || d->record.find("stances") == nullptr) {
        return out;
    }
    for (const data::Value& v : d->record.get_array("stances")) {
        const data::Record& r = v.as_record();
        out.push_back(RaceStance{r.get_string("race"), r.get_int("value")});
    }
    return out;
}

std::vector<std::int64_t> largest_remainder(std::span<const std::int64_t> weights, std::int64_t total) {
    std::int64_t sum = 0;
    for (const std::int64_t w : weights) {
        sum = checked_add(sum, w);
    }
    std::vector<std::int64_t> out(weights.size(), 0);
    if (sum <= 0) {
        return out;
    }
    std::vector<std::pair<std::int64_t, std::size_t>> rest;  // (remainder, index)
    std::int64_t given = 0;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        const std::int64_t c = checked_mul(weights[i], total);
        out[i] = floor_div(c, sum);
        given = checked_add(given, out[i]);
        rest.emplace_back(c - checked_mul(out[i], sum), i);
    }
    std::stable_sort(rest.begin(), rest.end(), [](const auto& x, const auto& y) {
        return x.first != y.first ? x.first > y.first : x.second < y.second;
    });
    for (std::size_t k = 0; given < total && k < rest.size(); ++k) {
        ++out[rest[k].second];
        ++given;
    }
    return out;
}

// --- Setup --------------------------------------------------------------------------------------------

RelationsSubsystem::RelationsSubsystem(PeoplesSubsystem& peoples, const ConflictSubsystem& conflict,
                                       const data::ContentSet& content)
    : RelationsSubsystem(peoples, conflict, relations_tuning_from(content)) {}

RelationsSubsystem::RelationsSubsystem(PeoplesSubsystem& peoples, const ConflictSubsystem& conflict,
                                       RelationsTuning tuning)
    : peoples_(peoples), conflict_(conflict), tuning_(tuning) {}

RelationsSubsystem::RelationsSubsystem(PeoplesSubsystem& peoples, const ConflictSubsystem& conflict,
                                       const data::ContentSet& content, const HistoryInputs& inputs)
    : RelationsSubsystem(peoples, conflict, relations_tuning_from(content)) {
    prepare_from(inputs, content);  // P5-03a 8 (A2)
}

std::span<const Step> RelationsSubsystem::steps() const {
    return STEPS;
}

void RelationsSubsystem::prepare(const StepContext& ctx) {
    if (!prepared_) {
        prepare_from(ctx.inputs(), ctx.content());
    }
}

void RelationsSubsystem::prepare_from(const HistoryInputs& in, const data::ContentSet& content) {
    if (prepared_) {
        return;
    }
    expel_key_ = stream_key(in.seed, streams::hist_relations_expel);
    settlers_key_ = stream_key(in.seed, streams::hist_relations_settlers);
    const ConflictCamp& camp = conflict_.camp_table();
    for (const db::RaceRow& r : in.races) {  // id order
        race_ids_.push_back(r.id);
        race_camp_.push_back(r.camp);
        race_awake_.push_back(r.awakening_year ? *r.awakening_year : -1);
    }
    const std::size_t n = race_ids_.size();
    if (n > 64U) {
        fail("history: bound: races exceeds 64");  // P5-03a 3.2's bit sets
    }
    // 6.2.1: B(a, b) = camp + floor_div(stance(a->b) + stance(b->a), 2).
    std::vector<std::vector<RaceStance>> stances;
    for (const db::RaceRow& r : in.races) {
        stances.push_back(race_stances_from(content, r.key));
    }
    const auto stance = [&](std::size_t i, std::size_t j) {
        for (const RaceStance& s : stances[i]) {
            if (s.race == in.races[j].key) {
                return s.value;
            }
        }
        return std::int64_t{0};
    };
    base_.assign(n * n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            const std::int64_t c = camp.of(race_camp_[i], race_camp_[j]);
            base_[i * n + j] = i == j ? c : checked_add(c, floor_div(checked_add(stance(i, j), stance(j, i)), 2));
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (in.races[i].present == 0) {
            continue;
        }
        for (std::size_t j = i + 1; j < n; ++j) {
            if (in.races[j].present == 0) {
                continue;
            }
            Pair p;
            p.a = race_ids_[i];
            p.b = race_ids_[j];
            p.base = base_[i * n + j];
            p.band = plain_band(p.base);
            pairs_.push_back(std::move(p));
        }
    }
    pair_rows_.assign(pairs_.size(), {});
    at_war_pairs_.assign(pairs_.size(), 0);
    prepared_ = true;
}

void RelationsSubsystem::run(StepContext& ctx) {
    prepare(ctx);
    const State& state = ctx.state();
    know_settlements(state);  // 6.3.1, every step, first
    pending_moves(ctx);
    const Step step = ctx.step();
    if (step == Step::t1 || step == Step::q1) {
        if (ctx.phase_id() != last_phase_) {
            if (last_phase_ != 0) {
                phase_rows(ctx.year() - 1);
            }
            last_phase_ = ctx.phase_id();
            drift(ctx);
            standings(ctx);
        }
        pending_displacements(ctx);
    } else if (step == Step::t4) {
        pending_displacements(ctx);
    } else if (step == Step::t6 || step == Step::q4) {
        read_log(ctx);
        owner_changes(ctx);
        if (step == Step::t6) {
            expulsions(ctx);
        }
        pending_displacements(ctx);
        war_foes(ctx);  // P5-03a 3.6
        decay(ctx.years());
        peace(ctx);
        turns(ctx);
        meetings(ctx);
    }
    mixed(ctx);
    // 6.7 (a reading): the rows of a step at its year, a quiet step's at its
    // start, as the engine's census rows (P3-04 6.7).
    last_year_ = ctx.year();
    write_settlement_rows(ctx.state(), last_year_);
}

// --- Lookups ------------------------------------------------------------------------------------------

RelationsSubsystem::Pair* RelationsSubsystem::pair_of(std::int64_t a, std::int64_t b) {
    if (a > b) {
        std::swap(a, b);
    }
    for (Pair& p : pairs_) {
        if (p.a == a && p.b == b) {
            return &p;
        }
    }
    return nullptr;
}

const RelationsSubsystem::Pair* RelationsSubsystem::pair_of(std::int64_t a, std::int64_t b) const {
    if (a > b) {
        std::swap(a, b);
    }
    for (const Pair& p : pairs_) {
        if (p.a == a && p.b == b) {
            return &p;
        }
    }
    return nullptr;
}

std::int64_t RelationsSubsystem::base(std::int64_t a, std::int64_t b) const {
    const auto ia = std::lower_bound(race_ids_.begin(), race_ids_.end(), a);
    const auto ib = std::lower_bound(race_ids_.begin(), race_ids_.end(), b);
    if (ia == race_ids_.end() || *ia != a || ib == race_ids_.end() || *ib != b) {
        return 0;
    }
    const auto i = static_cast<std::size_t>(ia - race_ids_.begin());
    const auto j = static_cast<std::size_t>(ib - race_ids_.begin());
    return base_[i * race_ids_.size() + j];
}

std::int64_t RelationsSubsystem::d_of(const Pair& p) const {
    return checked_add(p.base, p.memory / 1000);  // truncated toward zero (6.2.1)
}

Band RelationsSubsystem::plain_band(std::int64_t d) const {
    if (d <= tuning_.hatred_max) {
        return Band::hatred;
    }
    if (d >= tuning_.friend_min) {
        return Band::friendship;
    }
    return Band::wariness;
}

std::int64_t RelationsSubsystem::disposition(std::int64_t a, std::int64_t b) const {
    if (!prepared_) {
        fail("history: relations: dispositions read before the first step");
    }
    if (a == b) {
        return base(a, a);
    }
    const Pair* p = pair_of(a, b);
    return p != nullptr ? d_of(*p) : base(a, b);
}

std::int64_t RelationsSubsystem::hatred_event(std::int64_t a, std::int64_t b) const {
    const Pair* p = a != b ? pair_of(a, b) : nullptr;
    return p != nullptr ? p->hatred_event : 0;
}

std::int64_t RelationsSubsystem::memory(std::int64_t a, std::int64_t b) const {
    const Pair* p = a != b ? pair_of(a, b) : nullptr;
    return p != nullptr ? p->memory : 0;
}

bool RelationsSubsystem::met(std::int64_t a, std::int64_t b) const {
    const Pair* p = a != b ? pair_of(a, b) : nullptr;
    return p != nullptr && p->met;
}

std::int64_t RelationsSubsystem::band(std::int64_t a, std::int64_t b) const {
    return static_cast<std::int64_t>(band_of(a, b));
}

Band RelationsSubsystem::band_of(std::int64_t a, std::int64_t b) const {
    const Pair* p = a != b ? pair_of(a, b) : nullptr;
    if (p != nullptr && p->met) {
        return p->band;
    }
    return plain_band(a == b ? base(a, a) : base(a, b));
}

std::vector<RelationsSubsystem::Share> RelationsSubsystem::shares(std::int64_t settlement) const {
    std::vector<Share> out;
    if (settlement < 1 || static_cast<std::size_t>(settlement) > places_.size()) {
        return out;
    }
    for (const Entry& e : places_[static_cast<std::size_t>(settlement - 1)].entries) {
        out.push_back(Share{e.race, e.share, e.standing});
    }
    return out;
}

std::int64_t RelationsSubsystem::race_of_civ(const State& state, std::int64_t civ) const {
    return civ >= 1 && civ <= civ_count(state) ? state.civ(civ).race : 0;
}

RelationsSubsystem::Place& RelationsSubsystem::place(std::int64_t settlement) {
    if (settlement < 1 || static_cast<std::size_t>(settlement) > places_.size()) {
        fail("history: relations: settlement " + db::format_i64(settlement) + " is not known");
    }
    return places_[static_cast<std::size_t>(settlement - 1)];
}

// 6.3.5: the target standing of entry e under the ruling race.
Standing RelationsSubsystem::target(const StepContext& ctx, std::int64_t ruling, const Entry& e) const {
    if (e.race == ruling) {
        return Standing::ruling;
    }
    if (e.conquered && ctx.year() - e.since_year < tuning_.subject_years) {
        return Standing::subject;
    }
    switch (band_of(ruling, e.race)) {
        case Band::hatred: return Standing::subject;
        case Band::wariness: return Standing::tolerated;
        case Band::friendship: return Standing::accepted;
    }
    return Standing::tolerated;
}

// --- Shares (6.3) --------------------------------------------------------------------------------------

void RelationsSubsystem::know_settlements(const State& state) {
    const std::int64_t n = settlement_count(state);
    for (auto id = static_cast<std::int64_t>(places_.size()) + 1; id <= n; ++id) {
        const Settlement& s = state.settlement(id);
        Place p;
        p.owner = s.founder_civ;
        p.ruling = race_of_civ(state, s.founder_civ);
        Entry e;
        e.race = p.ruling;
        e.share = 1000;
        e.standing = Standing::ruling;
        e.since_year = s.founded_year;
        e.since_event = s.founding_event;
        p.entries.push_back(e);
        places_.push_back(std::move(p));
        place_rows_.emplace_back();
        ruined_.push_back(0);
    }
}

// Scales every entry but `keep_race` to fill 1000 - keep_share by the
// largest-remainder rule (weights their shares); zero shares are dropped.
void RelationsSubsystem::fill_with(Place& p, std::int64_t keep_race, std::int64_t keep_share) {
    std::vector<std::int64_t> weights;
    for (const Entry& e : p.entries) {
        weights.push_back(e.race == keep_race ? 0 : e.share);
    }
    const std::vector<std::int64_t> shares = largest_remainder(weights, 1000 - keep_share);
    for (std::size_t i = 0; i < p.entries.size(); ++i) {
        p.entries[i].share = p.entries[i].race == keep_race ? keep_share : shares[i];
    }
    p.entries.erase(std::remove_if(p.entries.begin(), p.entries.end(), [](const Entry& e) { return e.share <= 0; }),
                    p.entries.end());
}

void RelationsSubsystem::mix(const StepContext& ctx, std::int64_t settlement, std::span<const Entry> group,
                             std::int64_t people, std::int64_t before, std::int64_t event) {
    Place& p = place(settlement);
    std::vector<std::int64_t> races;
    for (const Entry& e : p.entries) {
        races.push_back(e.race);
    }
    for (const Entry& g : group) {
        races.push_back(g.race);
    }
    std::stable_sort(races.begin(), races.end());
    races.erase(std::unique(races.begin(), races.end()), races.end());
    const auto share_in = [](std::span<const Entry> list, std::int64_t race) {
        for (const Entry& e : list) {
            if (e.race == race) {
                return e.share;
            }
        }
        return std::int64_t{0};
    };
    std::vector<std::int64_t> c;
    for (const std::int64_t r : races) {
        c.push_back(checked_add(checked_mul(share_in(p.entries, r), before), checked_mul(share_in(group, r), people)));
    }
    std::vector<std::int64_t> shares = largest_remainder(c, 1000);
    // P5-03a 8 (A1): the ruling race, when it was in S, keeps at least 1;
    // the 1 comes from the largest share (then the smaller race id).
    const bool ruling_in_s =
        std::any_of(p.entries.begin(), p.entries.end(), [&p](const Entry& e) { return e.race == p.ruling; });
    if (ruling_in_s) {
        const auto qi = static_cast<std::size_t>(std::lower_bound(races.begin(), races.end(), p.ruling) - races.begin());
        if (shares[qi] == 0) {
            std::size_t big = 0;
            for (std::size_t i = 1; i < shares.size(); ++i) {
                big = shares[i] > shares[big] ? i : big;  // races ascend: ties keep the smaller id
            }
            --shares[big];
            ++shares[qi];
        }
    }
    std::vector<Entry> next;
    for (std::size_t i = 0; i < races.size(); ++i) {
        if (shares[i] <= 0) {
            continue;
        }
        const auto it = std::find_if(p.entries.begin(), p.entries.end(),
                                     [&](const Entry& e) { return e.race == races[i]; });
        Entry e;
        if (it != p.entries.end()) {
            e = *it;
        } else {
            e.race = races[i];
            e.since_year = ctx.year();
            e.since_event = event;
            e.conquered = false;
            e.standing = target(ctx, p.ruling, e);
        }
        e.share = shares[i];
        next.push_back(e);
    }
    if (!next.empty()) {
        p.entries = std::move(next);
    }
}

void RelationsSubsystem::pending_moves(const StepContext& ctx) {
    const State& state = ctx.state();
    std::vector<Record> moves = std::move(moves_);
    moves_.clear();
    for (const Record& r : moves) {
        if (r.to < 1 || r.to > settlement_count(state) || !standing_settlement(state.settlement(r.to))) {
            continue;  // the destination fell meanwhile; its people went with it
        }
        mix(ctx, r.to, r.entries, r.people, r.to_before, r.cause);
    }
}

// 6.3.3.
void RelationsSubsystem::owner_changes(const StepContext& ctx) {
    const State& state = ctx.state();
    const std::int64_t n = static_cast<std::int64_t>(places_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        const Settlement& s = state.settlement(id);
        Place& p = place(id);
        if (!standing_settlement(s) || s.owner_civ == p.owner || s.owner_civ == 0) {
            continue;
        }
        p.owner = s.owner_civ;
        const std::int64_t q = race_of_civ(state, s.owner_civ);
        if (q == p.ruling) {
            continue;
        }
        const std::int64_t event = s.owner_event;
        // 1. The old ruling people become subjects.
        for (Entry& e : p.entries) {
            if (e.race == p.ruling) {
                e.standing = Standing::subject;
                e.conquered = true;
                e.since_year = ctx.year();
                e.since_event = event;
            }
        }
        // 2. The conquerors' people settle among them.
        std::int64_t x = 0;
        for (const Entry& e : p.entries) {
            x = e.race == q ? e.share : x;
        }
        // P5-03a 5.1: the settlers' share sigma is drawn in
        // [conquest_settlers_permille, + conquest_settlers_span_permille].
        const std::uint64_t u = ctx.draw(settlers_key_, word(id), word(s.owner_civ), 0);
        const std::int64_t sigma = checked_add(
            tuning_.conquest_settlers_permille,
            static_cast<std::int64_t>(pick(u, word(checked_add(tuning_.conquest_settlers_span_permille, 1)))));
        const std::int64_t share_q = checked_add(x, floor_div(checked_mul(1000 - x, sigma), 1000));
        if (std::none_of(p.entries.begin(), p.entries.end(), [q](const Entry& e) { return e.race == q; })) {
            Entry e;
            e.race = q;
            p.entries.push_back(e);
            std::stable_sort(p.entries.begin(), p.entries.end(),
                             [](const Entry& a, const Entry& b) { return a.race < b.race; });
        }
        fill_with(p, q, share_q);
        p.ruling = q;
        for (Entry& e : p.entries) {
            if (e.race == q) {
                e.standing = Standing::ruling;
                e.since_year = ctx.year();
                e.since_event = event;
                e.conquered = false;
            }
        }
        // 3. Every other entry's standing under the new rulers.
        for (Entry& e : p.entries) {
            if (e.race != q && !(e.conquered && e.since_event == event)) {
                e.standing = target(ctx, q, e);
            }
        }
    }
}

// 6.3.4.
void RelationsSubsystem::drift(const StepContext& ctx) {
    const std::int64_t delta = ctx.year() - last_drift_;
    last_drift_ = ctx.year();
    const State& state = ctx.state();
    const std::int64_t n = static_cast<std::int64_t>(places_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        Place& p = place(id);
        if (!standing_settlement(state.settlement(id)) || p.entries.size() < 2U) {
            continue;
        }
        std::int64_t gained = 0;
        for (Entry& e : p.entries) {
            if (e.race == p.ruling) {
                continue;
            }
            const std::int64_t lost = std::min(
                e.share, floor_div(checked_mul(checked_mul(e.share, tuning_.drift_permille_per_century), delta), 100000));
            e.share -= lost;
            gained += lost;
        }
        for (Entry& e : p.entries) {
            if (e.race != p.ruling && e.share < tuning_.fade_permille) {
                gained += e.share;
                e.share = 0;
            }
        }
        for (Entry& e : p.entries) {
            if (e.race == p.ruling) {
                e.share += gained;
            }
        }
        const std::int64_t q = p.ruling;
        p.entries.erase(std::remove_if(p.entries.begin(), p.entries.end(),
                                       [q](const Entry& e) { return e.race != q && e.share <= 0; }),
                        p.entries.end());
    }
}

// 6.3.5.
void RelationsSubsystem::standings(StepContext& ctx) {
    const std::int64_t n = static_cast<std::int64_t>(places_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        const Settlement& s = ctx.state().settlement(id);
        if (!standing_settlement(s)) {
            continue;
        }
        const std::int64_t region = s.region;
        const std::size_t count = place(id).entries.size();
        for (std::size_t i = 0; i < count; ++i) {
            const Place& p = place(id);
            const Entry& e = p.entries[i];
            if (e.race == p.ruling) {
                continue;
            }
            const Standing t = target(ctx, p.ruling, e);
            if (t == e.standing) {
                continue;
            }
            if (ctx.step() == Step::q1 && ctx.quiet_events_left(p.owner) <= 0) {
                continue;  // it waits for the next phase start
            }
            const Pair* pr = pair_of(p.ruling, e.race);
            const std::int64_t turn = pr != nullptr ? pr->turn_event : 0;
            const std::int64_t cause = turn > e.since_event ? turn : e.since_event;
            const bool by_conquest = e.conquered && ctx.year() - e.since_year < tuning_.subject_years;
            EventSpec ev = point_event(ctx, "standing_changed", region, id);
            ev.participants = {Participant{"settlement", EntityType::settlement, id},
                               Participant{"civilization", EntityType::civilization, p.owner},
                               Participant{"race", EntityType::race, e.race}};
            ev.values = {static_cast<std::int64_t>(t), e.share};
            ev.causes = causes_of({cause});
            const std::int64_t at = ctx.append(ev);
            Entry& w = place(id).entries[i];
            w.standing = t;
            w.since_event = at;
            w.since_year = ctx.year();
            if (!(by_conquest && t == Standing::subject)) {
                w.conquered = false;
            }
        }
    }
}

// 6.3.6.
void RelationsSubsystem::mixed(StepContext& ctx) {
    const std::int64_t n = static_cast<std::int64_t>(places_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        const Settlement& s = ctx.state().settlement(id);
        if (!standing_settlement(s) || !ctx.visible(s)) {
            continue;
        }
        const std::int64_t region = s.region;
        const std::size_t count = place(id).entries.size();
        for (std::size_t i = 0; i < count; ++i) {
            const Place& p = place(id);
            const Entry& e = p.entries[i];
            if (e.race == p.ruling || e.share < tuning_.mixed_min_permille || e.mixed_told) {
                continue;
            }
            if (quiet(ctx) && ctx.quiet_events_left(p.owner) <= 0) {
                continue;  // it waits
            }
            EventSpec ev = point_event(ctx, "settlement_mixed", region, id);
            ev.participants = {Participant{"settlement", EntityType::settlement, id},
                               Participant{"civilization", EntityType::civilization, p.owner},
                               Participant{"race", EntityType::race, e.race},
                               Participant{"ruling", EntityType::race, p.ruling}};
            // P5-03a 5.2: the entry's standing, and whether it outnumbers the
            // ruling race.
            std::int64_t ruling_share = 0;
            for (const Entry& q : p.entries) {
                ruling_share = q.race == p.ruling ? q.share : ruling_share;
            }
            ev.values = {e.share, static_cast<std::int64_t>(e.standing), e.share > ruling_share ? 1 : 0};
            ev.causes = causes_of({e.since_event});
            (void)ctx.append(ev);
            place(id).entries[i].mixed_told = true;
        }
    }
}

// --- The people ledger (6.4) ---------------------------------------------------------------------------

void RelationsSubsystem::moved(const State& state, std::int64_t from, std::int64_t to, std::int64_t people,
                               std::int64_t cause) {
    know_settlements(state);  // 6.3.1's bookkeeping, at the call
    Record r;
    r.from = from;
    r.to = to;
    r.people = people;
    r.cause = cause;
    r.to_before = state.settlement(to).population - people;
    r.owner = state.settlement(from).owner_civ;
    r.entries = place(from).entries;
    moves_.push_back(std::move(r));
}

void RelationsSubsystem::displaced(const State& state, std::int64_t from, std::int64_t race, std::int64_t people,
                                   std::int64_t cause) {
    know_settlements(state);
    Record r;
    r.from = from;
    r.race = race;
    r.people = people;
    r.cause = cause;
    r.owner = state.settlement(from).owner_civ;
    r.entries = place(from).entries;
    displacements_.push_back(std::move(r));
}

void RelationsSubsystem::pending_displacements(StepContext& ctx) {
    std::vector<Record> records = std::move(displacements_);
    displacements_.clear();
    for (const Record& r : records) {
        // 1. Groups.
        std::vector<std::pair<std::int64_t, std::int64_t>> groups;  // (race, people), ascending race
        if (r.race != 0) {
            groups.emplace_back(r.race, r.people);
        } else {
            std::vector<std::int64_t> weights;
            for (const Entry& e : r.entries) {
                weights.push_back(e.share);
            }
            const std::vector<std::int64_t> n = largest_remainder(weights, r.people);
            for (std::size_t i = 0; i < r.entries.size(); ++i) {
                groups.emplace_back(r.entries[i].race, n[i]);
            }
        }
        ++stats_.displaced_groups;
        stats_.displaced_people = checked_add(stats_.displaced_people, r.people);
        for (const auto& [g, n_g] : groups) {
            if (n_g < tuning_.migrate_min) {
                stats_.lost_people = checked_add(stats_.lost_people, n_g);
                continue;  // they scattered
            }
            // 2, 3. The host.
            const State& state = ctx.state();
            const Settlement& from = state.settlement(r.from);
            const std::int64_t fx = from.x;
            const std::int64_t fy = from.y;
            const std::vector<std::int64_t>& hops = peoples_.view().hops(from.region);
            std::int64_t best = 0;
            std::tuple<std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t> best_key{};
            std::int64_t best_room = 0;
            const std::int64_t ns = settlement_count(state);
            for (std::int64_t u = 1; u <= ns; ++u) {
                const Settlement& s = state.settlement(u);
                if (u == r.from || !standing_settlement(s) || !ctx.visible(s)) {
                    continue;
                }
                const std::int64_t c = s.owner_civ;
                if (c < 1 || state.civ(c).ended_year || !ctx.visible(state.civ(c)) ||
                    peoples_.lying_low(state, c, ctx.year()) || c == r.owner || (r.owner != 0 && at_war(state, c, r.owner))) {
                    continue;
                }
                const std::int64_t h = s.region >= 1 && static_cast<std::size_t>(s.region) <= hops.size()
                                           ? hops[static_cast<std::size_t>(s.region - 1)]
                                           : -1;
                if (h < 0 || h > tuning_.migrate_hops) {
                    continue;
                }
                const std::int64_t rc = state.civ(c).race;
                const std::int64_t d = disposition(rc, g);
                if (d < tuning_.welcome_min) {
                    continue;
                }
                const std::int64_t room = std::max<std::int64_t>(
                    0, checked_sub(checked_mul(2, peoples_.view().capacity(s.region, rc)), s.population));
                if (room < tuning_.migrate_min) {
                    continue;
                }
                const std::int64_t dx = s.x - fx;
                const std::int64_t dy = s.y - fy;
                const auto key = std::make_tuple(h, rc == g ? 0 : 1, -d, checked_add(checked_mul(dx, dx), checked_mul(dy, dy)), u);
                if (best == 0 || key < best_key) {
                    best = u;
                    best_key = key;
                    best_room = room;
                }
            }
            if (best == 0) {
                ++stats_.lost_groups;
                stats_.lost_people = checked_add(stats_.lost_people, n_g);
                continue;  // no host: lost
            }
            // 4. The arrival.
            const Settlement& host = state.settlement(best);
            const std::int64_t c_u = host.owner_civ;
            if (quiet(ctx) && ctx.quiet_events_left(c_u) <= 0) {
                stats_.lost_people = checked_add(stats_.lost_people, n_g);
                continue;  // the budget is spent: lost
            }
            know_settlements(state);
            const std::int64_t taken = std::min(n_g, best_room);
            const std::int64_t p_u = host.population;
            const std::int64_t region = host.region;
            const Place& hp = place(best);
            Entry probe;
            probe.race = g;
            const Standing st = target(ctx, hp.ruling, probe);
            EventSpec ev = point_event(ctx, "refugees_arrived", region, best);
            ev.participants = {Participant{"settlement", EntityType::settlement, best},
                               Participant{"civilization", EntityType::civilization, c_u},
                               Participant{"origin", EntityType::settlement, r.from},
                               Participant{"race", EntityType::race, g}};
            ev.values = {taken, static_cast<std::int64_t>(st)};
            // P5-03a 7.3: an abandonment's own causes come with it.
            std::vector<std::int64_t> causes{r.cause};
            if (r.cause >= 1 && r.cause <= ctx.last_event_id()) {
                const std::string_view ck = ctx.event_kind(r.cause);
                if (ck == "settlement_abandoned" || ck == "settlement_abandoned_to") {
                    const std::vector<std::int64_t> more = ctx.event_causes(r.cause);
                    causes.insert(causes.end(), more.begin(), more.end());
                }
            }
            ev.causes = causes_of(std::move(causes));
            const std::int64_t at = ctx.append(ev);
            ctx.state().set_population(best, checked_add(p_u, taken));
            Entry arrived;
            arrived.race = g;
            arrived.share = 1000;
            const std::array<Entry, 1> group{arrived};
            mix(ctx, best, group, taken, p_u, at);
            ++stats_.arrivals;
            stats_.arrived_people = checked_add(stats_.arrived_people, taken);
            stats_.lost_people = checked_add(stats_.lost_people, n_g - taken);
        }
    }
}

// --- Expulsions (6.5) ----------------------------------------------------------------------------------

void RelationsSubsystem::expulsions(StepContext& ctx) {
    const std::int64_t n = static_cast<std::int64_t>(places_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        const State& state = ctx.state();
        const Settlement& s = state.settlement(id);
        if (!standing_settlement(s) || !ctx.visible(s)) {
            continue;
        }
        const std::int64_t o = place(id).owner;
        if (o < 1 || state.civ(o).ended_year || !ctx.visible(state.civ(o)) || peoples_.lying_low(state, o, ctx.year())) {
            continue;
        }
        // P5-03a 5.3: the gap, tested before any draw.
        if (const auto last = last_expulsion_.find(o);
            last != last_expulsion_.end() && ctx.year() - last->second < tuning_.expel_gap_years) {
            continue;
        }
        const std::int64_t pop = s.population;
        const std::int64_t region = s.region;
        const std::size_t count = place(id).entries.size();
        for (std::size_t i = 0; i < count; ++i) {
            const Place& p = place(id);
            const Entry& e = p.entries[i];
            if (e.race == p.ruling || static_cast<std::int64_t>(e.standing) > 1 ||
                band_of(p.ruling, e.race) != Band::hatred || e.share > tuning_.expel_share_max) {
                continue;
            }
            // P5-04a 8: a long-settled entry only after a fresh turn into hatred (before the draw).
            if (ctx.year() - e.since_year >= tuning_.expel_settled_years) {
                const Pair* held = pair_of(p.ruling, e.race);
                const std::int64_t hatred = held != nullptr ? held->hatred_event : 0;
                if (hatred == 0 || ctx.event_year(hatred) < ctx.year() - tuning_.expel_fresh_years) {
                    continue;
                }
            }
            const std::int64_t people = floor_div(checked_mul(pop, e.share), 1000);
            if (people < tuning_.migrate_min ||
                !chance_pm(ctx.draw(expel_key_, word(id), word(e.race), 0), tuning_.expel_permille)) {
                continue;
            }
            const std::int64_t g = e.race;
            const Pair* pr = pair_of(p.ruling, g);
            const std::int64_t turn = pr != nullptr ? pr->turn_event : 0;
            EventSpec ev = point_event(ctx, "people_expelled", region, id);
            ev.participants = {Participant{"settlement", EntityType::settlement, id},
                               Participant{"civilization", EntityType::civilization, o},
                               Participant{"race", EntityType::race, g}};
            ev.values = {people, static_cast<std::int64_t>(e.standing), ctx.year() - e.since_year};  // P5-03a 5.3
            ev.causes = causes_of({e.since_event, turn > e.since_event ? turn : 0});
            const std::int64_t at = ctx.append(ev);
            last_expulsion_[o] = ctx.year();
            ctx.state().set_population(id, pop - people);
            peoples_.note_reduction(id, at);
            Place& w = place(id);
            w.entries.erase(w.entries.begin() + static_cast<std::ptrdiff_t>(i));
            fill_with(w, 0, 0);
            displaced(ctx.state(), id, g, people, at);
            ++stats_.expulsions;
            break;  // at most one a year
        }
    }
}

// --- Dispositions (6.2) ----------------------------------------------------------------------------------

void RelationsSubsystem::read_log(const StepContext& ctx) {
    const std::int64_t last = ctx.last_event_id();
    const State& state = ctx.state();
    for (std::int64_t e = cursor_; e <= last; ++e) {
        const std::string_view kind = ctx.event_kind(e);
        std::int64_t x = 0;
        std::int64_t y = 0;
        std::int64_t ry = 0;
        std::int64_t delta = 0;
        if (kind == "war_won" || kind == "peace") {
            x = ctx.event_participant(e, "winner");
            y = ctx.event_participant(e, "loser");
            delta = -tuning_.war_memory;
        } else if (kind == "truce") {
            const std::vector<std::int64_t> parties = ctx.event_participants(e, "parties");
            x = parties.size() >= 1U ? parties[0] : 0;
            y = parties.size() >= 2U ? parties[1] : 0;
            delta = -tuning_.war_memory;
        } else if (kind == "civ_conquered") {
            x = ctx.event_participant(e, "civilization");
            y = ctx.event_participant(e, "conqueror");
            delta = -tuning_.conquest_memory;
        } else if (kind == "sacked") {
            x = ctx.event_participant(e, "attacker");
            y = ctx.event_participant(e, "defender");
            delta = -tuning_.sack_memory;
        } else if (kind == "settlement_captured") {
            x = ctx.event_participant(e, "attacker");
            y = ctx.event_participant(e, "defender");
            delta = -tuning_.capture_memory;
        } else if (kind == "people_expelled") {
            x = ctx.event_participant(e, "civilization");
            ry = ctx.event_participant(e, "race");
            delta = -tuning_.expel_memory;
        } else if (kind == "refugees_arrived") {
            x = ctx.event_participant(e, "civilization");
            ry = ctx.event_participant(e, "race");
            delta = tuning_.shelter_memory;
        } else {
            if (kind == "monster_raid" || kind == "settlement_overrun") {
                monster_foe(ctx, e);  // P5-03a 3.3
            }
            continue;
        }
        const std::int64_t rx = race_of_civ(state, x);
        if (ry == 0) {
            ry = race_of_civ(state, y);
        }
        if (rx == 0 || ry == 0 || rx == ry) {
            continue;
        }
        credit(rx, ry, delta, e);  // P5-03a 3.5
    }
    cursor_ = last + 1;
}

std::int64_t RelationsSubsystem::met_event(std::int64_t a, std::int64_t b) const {
    const Pair* p = a != b ? pair_of(a, b) : nullptr;
    return p != nullptr && p->met ? p->met_event : 0;
}

std::int64_t RelationsSubsystem::turn_event(std::int64_t a, std::int64_t b) const {
    const Pair* p = a != b ? pair_of(a, b) : nullptr;
    return p != nullptr ? p->turn_event : 0;
}

void RelationsSubsystem::credit(std::int64_t a, std::int64_t b, std::int64_t units, std::int64_t event) {
    Pair* p = a != b ? pair_of(a, b) : nullptr;
    if (p == nullptr || !p->met) {
        return;
    }
    p->memory = std::clamp(checked_add(p->memory, checked_mul(units, 1000)), checked_mul(tuning_.memory_min, 1000),
                           checked_mul(tuning_.memory_max, 1000));
    if (units == 0) {
        return;  // reading: a contribution of no sign is kept by neither list
    }
    std::vector<std::int64_t>& list = units > 0 ? p->warmer : p->colder;
    list.push_back(event);
    if (list.size() > MAX_CONTRIBUTIONS) {
        list.erase(list.begin());
    }
}

// P5-03a 3.3, a monster: raid or overrun e of monster m on a realm of race r.
void RelationsSubsystem::monster_foe(const StepContext& ctx, std::int64_t e) {
    const std::int64_t m = ctx.event_participant(e, "monster");
    const std::int64_t r = race_of_civ(ctx.state(), ctx.event_participant(e, "civilization"));
    if (m == 0 || r == 0) {
        return;
    }
    const std::int64_t y = ctx.year();
    for (const std::int64_t r2 : race_ids_) {  // ascending
        if (r2 == r) {
            continue;
        }
        const auto seen = monster_foes_.find({m, r2});
        if (seen == monster_foes_.end() || y - seen->second.year >= tuning_.foe_years || !met(r, r2)) {
            continue;
        }
        const auto key = std::make_tuple(std::min(r, r2), std::max(r, r2), m);
        if (monster_credited_.count(key) != 0) {
            continue;
        }
        credit(r, r2, tuning_.foe_monster_memory, e);
        monster_credited_.insert(key);
    }
    monster_foes_[{m, r}] = Seen{ctx.event_year(e), e};
}

// P5-03a 3.3, in war; and the strife of 3.2: which pairs an open war joins.
void RelationsSubsystem::war_foes(const StepContext& ctx) {
    const State& state = ctx.state();
    const std::int64_t y = ctx.year();
    std::fill(at_war_pairs_.begin(), at_war_pairs_.end(), std::uint8_t{0});
    for (const auto& [key, rel] : state.relations()) {  // ascending key
        if (key.kind != "war" || key.a_type != CIV_TYPE || key.b_type != CIV_TYPE) {
            continue;
        }
        const std::int64_t ra = race_of_civ(state, key.a_id);
        const std::int64_t rb = race_of_civ(state, key.b_id);
        if (ra == 0 || rb == 0 || ra == rb) {
            continue;
        }
        // P5-03a audit A2: at war with several realms of one race, the larger opening event.
        for (const auto& k : {std::pair<std::int64_t, std::int64_t>{key.a_id, rb}, std::pair<std::int64_t, std::int64_t>{key.b_id, ra}}) {
            const auto found = civ_foes_.find(k);
            if (found != civ_foes_.end() && found->second.year == y) {
                found->second.event = std::max(found->second.event, rel.event);
            } else {
                civ_foes_[k] = Seen{y, rel.event};
            }
        }
        if (Pair* p = pair_of(ra, rb)) {
            at_war_pairs_[static_cast<std::size_t>(p - pairs_.data())] = 1;
        }
    }
    // For each z ascending, its entries (ascending race).
    auto it = civ_foes_.begin();
    while (it != civ_foes_.end()) {
        const std::int64_t z = it->first.first;
        auto end = it;
        while (end != civ_foes_.end() && end->first.first == z) {
            ++end;
        }
        for (auto i = it; i != end; ++i) {
            for (auto j = std::next(i); j != end; ++j) {
                const std::int64_t r1 = i->first.second;
                const std::int64_t r2 = j->first.second;
                const Seen& s1 = i->second;
                const Seen& s2 = j->second;
                if (y - s1.year >= tuning_.foe_years || y - s2.year >= tuning_.foe_years ||
                    (s1.year != y && s2.year != y) || !met(r1, r2)) {
                    continue;
                }
                const auto credited = std::make_tuple(r1, r2, z);
                if (civ_credited_.count(credited) != 0) {
                    continue;
                }
                credit(r1, r2, tuning_.foe_war_memory, std::max(s1.event, s2.event));
                civ_credited_.insert(credited);
            }
        }
        it = end;
    }
}

// P5-03a 4.1: breadth-first on every border, neighbours ascending.
const std::vector<std::int64_t>& RelationsSubsystem::border_hops(const GeoView& geo, std::int64_t region) {
    const std::size_t n = geo.regions().size();
    if (border_hops_.size() != n) {
        border_hops_.assign(n, {});
    }
    std::vector<std::int64_t>& out = border_hops_[static_cast<std::size_t>(region - 1)];
    if (!out.empty()) {
        return out;
    }
    out.assign(n, -1);
    std::vector<std::int64_t> queue{region};
    out[static_cast<std::size_t>(region - 1)] = 0;
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const std::int64_t r = queue[head];
        const std::int64_t h = out[static_cast<std::size_t>(r - 1)];
        for (const GeoView::Neighbour& nb : geo.region(r).neighbours) {
            std::int64_t& d = out[static_cast<std::size_t>(nb.region - 1)];
            if (d < 0) {
                d = h + 1;
                queue.push_back(nb.region);
            }
        }
    }
    return out;
}

// The regions within contact_hops of `region` on the border graph (itself
// included), ascending; cached per region.
const std::vector<std::int64_t>& RelationsSubsystem::within_contact(const GeoView& geo, std::int64_t region) {
    const std::size_t n = geo.regions().size();
    if (within_.size() != n) {
        within_.assign(n, {});
    }
    std::vector<std::int64_t>& out = within_[static_cast<std::size_t>(region - 1)];
    if (!out.empty()) {
        return out;
    }
    std::vector<std::pair<std::int64_t, std::int64_t>> queue{{region, 0}};  // (region, hops)
    std::vector<std::int64_t> seen{region};
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const auto [r, h] = queue[head];
        if (h >= tuning_.contact_hops) {
            continue;
        }
        for (const GeoView::Neighbour& nb : geo.region(r).neighbours) {
            if (std::find(seen.begin(), seen.end(), nb.region) == seen.end()) {
                seen.push_back(nb.region);
                queue.emplace_back(nb.region, h + 1);
            }
        }
    }
    std::stable_sort(seen.begin(), seen.end());
    out = std::move(seen);
    return out;
}

std::size_t RelationsSubsystem::race_index(std::int64_t race) const {
    return static_cast<std::size_t>(std::lower_bound(race_ids_.begin(), race_ids_.end(), race) - race_ids_.begin());
}

// P5-03a 3.2: per region the races present (P3-06 5.1's presence of every
// alive visible civilization).
std::vector<std::uint64_t> RelationsSubsystem::race_presence(const StepContext& ctx) const {
    const State& state = ctx.state();
    std::vector<std::uint64_t> bits(ctx.geo().regions().size(), 0);
    const auto present = [&](std::int64_t civ) {
        if (civ < 1 || civ > civ_count(state)) {
            return false;
        }
        const Civilization& c = state.civ(civ);
        return !c.ended_year && ctx.visible(c);
    };
    const auto mark = [&](std::int64_t region, std::int64_t civ) {
        if (region >= 1 && static_cast<std::size_t>(region) <= bits.size() && present(civ)) {
            bits[static_cast<std::size_t>(region - 1)] |= std::uint64_t{1} << race_index(state.civ(civ).race);
        }
    };
    const auto nr = static_cast<std::int64_t>(bits.size());
    for (std::int64_t r = 1; r <= nr; ++r) {
        mark(r, state.region(r).owner);
    }
    const std::int64_t ns = settlement_count(state);
    for (std::int64_t id = 1; id <= ns; ++id) {
        const Settlement& s = state.settlement(id);
        if (standing_settlement(s) && ctx.visible(s)) {
            mark(s.region, s.owner_civ);
        }
    }
    return bits;
}

// P5-03a 3.2: long peace between neighbours.
void RelationsSubsystem::peace(const StepContext& ctx) {
    if (tuning_.peace_memory_per_century == 0 || tuning_.peace_memory_max == 0) {
        // No gain is possible; the strife years are still kept.
        for (std::size_t i = 0; i < pairs_.size(); ++i) {
            if (pairs_[i].met && at_war_pairs_[i] != 0) {
                pairs_[i].strife_year = ctx.year();
            }
        }
        return;
    }
    const std::int64_t y = ctx.year();
    const std::vector<std::uint64_t> bits = race_presence(ctx);
    // near[k]: the races present within contact_hops of a region of race k.
    std::vector<std::uint64_t> near(race_ids_.size(), 0);
    const auto nr = static_cast<std::int64_t>(bits.size());
    for (std::int64_t r = 1; r <= nr; ++r) {
        const std::uint64_t here = bits[static_cast<std::size_t>(r - 1)];
        if (here == 0) {
            continue;
        }
        std::uint64_t around = 0;
        for (const std::int64_t t : within_contact(ctx.geo(), r)) {
            around |= bits[static_cast<std::size_t>(t - 1)];
        }
        for (std::size_t k = 0; k < near.size(); ++k) {
            if ((here >> k) & 1U) {
                near[k] |= around;
            }
        }
    }
    const std::int64_t g = floor_div(checked_mul(checked_mul(ctx.years(), tuning_.peace_memory_per_century), 1000), 100);
    const std::int64_t cap = checked_mul(tuning_.peace_memory_max, 1000);
    for (std::size_t i = 0; i < pairs_.size(); ++i) {
        Pair& p = pairs_[i];
        if (!p.met) {
            continue;
        }
        if (at_war_pairs_[i] != 0) {
            p.strife_year = y;
            continue;
        }
        const bool contact = ((near[race_index(p.a)] >> race_index(p.b)) & 1U) != 0;
        if (!contact || y - p.strife_year < tuning_.peace_years || p.memory >= cap) {
            continue;
        }
        p.memory = std::min(cap, checked_add(p.memory, g));
    }
}

void RelationsSubsystem::decay(std::int64_t years) {
    const std::int64_t d = floor_div(checked_mul(checked_mul(years, tuning_.memory_decay_per_century), 1000), 100);
    for (Pair& p : pairs_) {
        if (p.memory > 0) {
            p.memory = std::max<std::int64_t>(0, p.memory - d);
        } else if (p.memory < 0) {
            p.memory = std::min<std::int64_t>(0, p.memory + d);
        }
    }
}

void RelationsSubsystem::turns(StepContext& ctx) {
    const std::int64_t h = tuning_.hatred_max;
    const std::int64_t f = tuning_.friend_min;
    const std::int64_t m = tuning_.band_margin;
    for (std::size_t i = 0; i < pairs_.size(); ++i) {
        const Pair& p = pairs_[i];
        if (!p.met) {
            continue;
        }
        const std::int64_t d = d_of(p);
        Band next = p.band;
        if (p.band == Band::wariness && d <= h) {
            next = Band::hatred;
        } else if (p.band == Band::wariness && d >= f) {
            next = Band::friendship;
        } else if (p.band == Band::hatred && d > h + m) {
            next = Band::wariness;
        } else if (p.band == Band::friendship && d < f - m) {
            next = Band::wariness;
        }
        if (next == p.band) {
            continue;
        }
        const bool warmer = static_cast<std::int64_t>(next) > static_cast<std::int64_t>(p.band);
        // P5-04a 7.2: no turn within turn_cooldown_years of the last unless a
        // fresh contribution of the turn's direction.
        if (p.turn_event != 0 && ctx.year() - p.turn_year < tuning_.turn_cooldown_years &&
            (warmer ? p.warmer : p.colder).empty()) {
            continue;
        }
        if (ctx.step() == Step::q4 && ctx.quiet_events_left(0) <= 0) {
            continue;  // it waits
        }
        // P5-03a 3.5: a warmer turn cites the warmer contributions (else the
        // meeting), a colder one the colder (else the last turn, else the
        // meeting).
        std::vector<std::int64_t> causes = warmer ? p.warmer : p.colder;
        if (causes.empty()) {
            causes = {warmer ? p.met_event : (p.turn_event != 0 ? p.turn_event : p.met_event)};
        }
        EventSpec ev = point_event(ctx, "disposition_turned", 0, 0);
        ev.participants = {Participant{"race_a", EntityType::race, p.a}, Participant{"race_b", EntityType::race, p.b}};
        // P5-04a 7.1: 0 into hatred, 1 a thaw, 2 a decline, 3 into friendship.
        const std::int64_t change = next == Band::hatred       ? 0
                                    : next == Band::friendship ? 3
                                    : p.band == Band::hatred   ? 1
                                                               : 2;
        ev.values = {d, static_cast<std::int64_t>(next), change};
        ev.causes = causes_of(std::move(causes));
        const std::int64_t at = ctx.append(ev);
        Pair& w = pairs_[i];
        w.band = next;
        w.turn_year = ctx.year();
        w.turn_event = at;
        if (next == Band::hatred) {
            w.hatred_event = at;
        }
        w.warmer.clear();
        w.colder.clear();
        write_pair_row(w, ctx.year());
    }
}

std::vector<std::int64_t> RelationsSubsystem::presence(const StepContext& ctx, std::int64_t civ) const {
    // P3-06 5.1: the regions c holds and the regions of its standing visible
    // settlements, ascending.
    const State& state = ctx.state();
    std::vector<std::int64_t> out;
    const auto nr = static_cast<std::int64_t>(ctx.geo().regions().size());
    for (std::int64_t r = 1; r <= nr; ++r) {
        if (state.region(r).owner == civ) {
            out.push_back(r);
        }
    }
    const std::int64_t ns = settlement_count(state);
    for (std::int64_t id = 1; id <= ns; ++id) {
        const Settlement& s = state.settlement(id);
        if (s.owner_civ == civ && standing_settlement(s) && ctx.visible(s)) {
            out.push_back(s.region);
        }
    }
    std::stable_sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

void RelationsSubsystem::meetings(StepContext& ctx) {
    if (tuning_.meet_hops < 0 && tuning_.meet_sealed_hops < 0) {
        return;
    }
    const State& state = ctx.state();
    const std::int64_t nc = civ_count(state);
    using Key = std::tuple<std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t>;  // (h, Rx, Ry, x, y)
    // The presence of each alive visible civilization, once per step (6.2.4).
    std::vector<std::vector<std::int64_t>> presences(static_cast<std::size_t>(nc) + 1);
    bool presences_known = false;
    for (std::size_t i = 0; i < pairs_.size(); ++i) {
        if (pairs_[i].met) {
            continue;
        }
        if (!presences_known) {
            for (std::int64_t c = 1; c <= nc; ++c) {
                const Civilization& civ = state.civ(c);
                if (!civ.ended_year && ctx.visible(civ)) {
                    presences[static_cast<std::size_t>(c)] = presence(ctx, c);
                }
            }
            presences_known = true;
        }
        const std::int64_t a = pairs_[i].a;
        const std::int64_t b = pairs_[i].b;
        std::vector<std::int64_t> xs, ys;  // civilizations of race a and b
        for (std::int64_t c = 1; c <= nc; ++c) {
            const Civilization& civ = state.civ(c);
            if (civ.ended_year || !ctx.visible(civ) || (civ.race != a && civ.race != b)) {
                continue;
            }
            (civ.race == a ? xs : ys).push_back(c);
        }
        if (xs.empty() || ys.empty()) {
            pairs_[i].word_since = -1;  // P5-03a 4.2: no key (P5-04a 11 item 7: -1 unset)
            continue;
        }
        // The smallest key on a graph given by its hops lists.
        const auto smallest = [&](const auto& hops_of) -> std::optional<Key> {
            std::optional<Key> best;
            for (const std::int64_t x : xs) {
                for (const std::int64_t rx : presences[static_cast<std::size_t>(x)]) {
                    const std::vector<std::int64_t>& hops = hops_of(rx);
                    for (const std::int64_t y : ys) {
                        for (const std::int64_t ry : presences[static_cast<std::size_t>(y)]) {
                            const std::int64_t h = hops[static_cast<std::size_t>(ry - 1)];
                            if (h < 0) {
                                continue;
                            }
                            const Key key{h, rx, ry, x, y};
                            if (!best || key < *best) {
                                best = key;
                            }
                        }
                    }
                }
            }
            return best;
        };
        std::optional<Key> found;
        bool sealed = false;
        if (tuning_.meet_hops >= 0) {
            const std::optional<Key> open =
                smallest([this](std::int64_t r) -> const std::vector<std::int64_t>& { return peoples_.view().hops(r); });
            if (open && std::get<0>(*open) <= tuning_.meet_hops) {
                found = open;
            }
        }
        if (!found) {
            // P5-03a 4.2: meeting by word across closed borders.
            std::optional<Key> word_key;
            if (tuning_.meet_sealed_hops >= 0) {
                word_key = smallest(
                    [this, &ctx](std::int64_t r) -> const std::vector<std::int64_t>& { return border_hops(ctx.geo(), r); });
            }
            if (!word_key || std::get<0>(*word_key) > tuning_.meet_sealed_hops) {
                pairs_[i].word_since = -1;
                continue;
            }
            if (pairs_[i].word_since < 0) {
                pairs_[i].word_since = ctx.year();
            }
            if (ctx.year() - pairs_[i].word_since < tuning_.meet_sealed_years) {
                continue;
            }
            found = word_key;
            sealed = true;
        }
        if (ctx.step() == Step::q4 && ctx.quiet_events_left(0) <= 0) {
            continue;  // it waits
        }
        const auto [h, rx, ry, x, y] = *found;
        (void)h;
        const Civilization& cx = state.civ(x);
        const Civilization& cy = state.civ(y);
        const bool x_younger = cx.founded_year != cy.founded_year ? cx.founded_year > cy.founded_year : x > y;
        const std::int64_t region = x_younger ? rx : ry;
        const std::int64_t cause = x_younger ? cx.founding_event : cy.founding_event;
        const Band b0 = plain_band(pairs_[i].base);
        EventSpec ev = point_event(ctx, "peoples_met", region, 0);
        ev.participants = {Participant{"race_a", EntityType::race, a}, Participant{"race_b", EntityType::race, b}};
        ev.values = {pairs_[i].base, static_cast<std::int64_t>(b0), sealed ? 1 : 0};
        ev.causes = causes_of({cause});
        const std::int64_t at = ctx.append(ev);
        Pair& w = pairs_[i];
        w.met = true;
        w.met_year = ctx.year();
        w.met_event = at;
        w.memory = 0;
        w.band = b0;
        w.strife_year = ctx.year();  // P5-03a 3.2
        w.warmer.clear();
        w.colder.clear();
        write_pair_row(w, ctx.year());
    }
}

// --- Rows (6.7) ------------------------------------------------------------------------------------------

namespace {

// The rules of 6.7 for one key's rows: nothing when (a, b) equals the last
// row; a row of the last row's year replaces it (and is dropped when it then
// equals the row before, or when it is a first row of share 0).
template <class Row>
void put_row(std::vector<Row>& rows, std::int64_t year, std::int64_t a, std::int64_t b, bool drop_zero_first) {
    if (!rows.empty() && rows.back().a == a && rows.back().b == b) {
        return;
    }
    if (!rows.empty() && rows.back().year == year) {
        rows.pop_back();
        if (!rows.empty() && rows.back().a == a && rows.back().b == b) {
            return;
        }
    }
    if (rows.empty() && drop_zero_first && a == 0) {
        return;
    }
    rows.push_back(Row{year, a, b});
}

}  // namespace

void RelationsSubsystem::write_pair_row(Pair& p, std::int64_t year) {
    const auto i = static_cast<std::size_t>(&p - pairs_.data());
    put_row(pair_rows_[i], year, d_of(p), static_cast<std::int64_t>(p.band), false);
}

void RelationsSubsystem::phase_rows(std::int64_t year) {
    for (Pair& p : pairs_) {
        if (p.met) {
            write_pair_row(p, year);
        }
    }
}

void RelationsSubsystem::write_settlement_rows(const State& state, std::int64_t year) {
    const std::int64_t n = static_cast<std::int64_t>(places_.size());
    for (std::int64_t id = 1; id <= n; ++id) {
        const auto i = static_cast<std::size_t>(id - 1);
        if (ruined_[i] != 0) {
            continue;
        }
        if (!standing_settlement(state.settlement(id))) {
            ruined_[i] = 1;  // no row after the ruin
            continue;
        }
        std::vector<RaceRows>& rows = place_rows_[i];
        const std::vector<Entry>& entries = places_[i].entries;
        // Races that left: a row of share 0.
        for (RaceRows& r : rows) {
            const bool present =
                std::any_of(entries.begin(), entries.end(), [&r](const Entry& e) { return e.race == r.race; });
            if (!present) {
                put_row(r.rows, year, 0, 0, true);
            }
        }
        for (const Entry& e : entries) {
            auto it = std::lower_bound(rows.begin(), rows.end(), e.race,
                                       [](const RaceRows& r, std::int64_t race) { return r.race < race; });
            if (it == rows.end() || it->race != e.race) {
                it = rows.insert(it, RaceRows{e.race, {}});
            }
            put_row(it->rows, year, e.share, static_cast<std::int64_t>(e.standing), true);
        }
    }
}

void RelationsSubsystem::finish(std::int64_t end_year) {
    if (finished_ || !prepared_) {
        return;
    }
    (void)last_year_;
    phase_rows(end_year - 1);
    finished_ = true;
}

std::vector<db::RaceRelationRow> RelationsSubsystem::race_relation_rows() const {
    std::vector<db::RaceRelationRow> out;
    for (std::size_t i = 0; i < pairs_.size(); ++i) {  // ascending (a, b)
        for (const RowValue& r : pair_rows_[i]) {
            out.push_back(db::RaceRelationRow{pairs_[i].a, pairs_[i].b, r.year, r.a, r.b});
        }
    }
    return out;
}

std::vector<db::SettlementRaceRow> RelationsSubsystem::settlement_race_rows() const {
    std::vector<db::SettlementRaceRow> out;
    for (std::size_t i = 0; i < place_rows_.size(); ++i) {
        for (const RaceRows& r : place_rows_[i]) {
            for (const RowValue& v : r.rows) {
                out.push_back(db::SettlementRaceRow{static_cast<std::int64_t>(i) + 1, r.race, v.year, v.a, v.b});
            }
        }
    }
    return out;
}

}  // namespace worldgen::history
