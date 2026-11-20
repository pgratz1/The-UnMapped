// Expansion and conflict (spec P3-06 sections 4 to 6, 7.2, 7.8 and 8):
// tuning, dispositions and grudges, contact, strain, claims, the tension
// source, declarations and the end of wars in quiet steps. The war itself
// (7.3 to 7.7) is in conflict_war.cpp.
//
// Rules of P3-04 6.9 as P3-06 8.3 states them: loops over civilizations,
// settlements and regions run over ids up to a count taken before them;
// relation changes are collected first and applied after; members hold ids
// only; every draw is keyed by ids and the year.

#include "worldgen/history/conflict.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <tuple>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/schema.hpp"
#include "conflict_internal.hpp"
#include "worldgen/history/draw.hpp"

namespace worldgen::history {

namespace conflict_detail {

[[noreturn]] void fail(const std::string& message) {
    throw FatalError(message);
}

std::string int_text(std::int64_t v) {
    return db::format_i64(v);
}

bool standing(const Settlement& s) noexcept {
    return s.status == SettlementStatus::thriving || s.status == SettlementStatus::declining;
}

std::uint64_t word(std::int64_t v) noexcept {
    return static_cast<std::uint64_t>(v);
}

bool chance_pm(std::uint64_t u, std::int64_t p) {
    constexpr std::int64_t ONE_RAW = std::int64_t{1} << 32;
    const std::int64_t raw = std::min(ONE_RAW, floor_div(checked_mul(std::max<std::int64_t>(0, p), ONE_RAW), 1000));
    return chance(u, Fx::from_raw(raw));
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

Participant civ_role(std::string_view role, std::int64_t civ) {
    return Participant{role, EntityType::civilization, civ};
}

std::int64_t settlement_count(const State& state) noexcept {
    return static_cast<std::int64_t>(state.settlements().size());
}

std::int64_t civ_count(const State& state) noexcept {
    return static_cast<std::int64_t>(state.civs().size());
}

ConflictSubsystem::Pair ordered(std::int64_t a, std::int64_t b) noexcept {
    return a < b ? ConflictSubsystem::Pair{a, b} : ConflictSubsystem::Pair{b, a};
}

RelationKey civ_relation(std::int64_t a, std::int64_t b, std::string_view kind) {
    const std::int64_t civ = static_cast<std::int64_t>(EntityType::civilization);
    return RelationKey{civ, a, civ, b, std::string(kind)};
}

RelationKey symmetric_relation(std::int64_t a, std::int64_t b, std::string_view kind) {
    const ConflictSubsystem::Pair p = ordered(a, b);
    return civ_relation(p.first, p.second, kind);
}

}  // namespace conflict_detail

using namespace conflict_detail;

namespace {

// The five streams of 3.3, in registration order.
constexpr std::array<std::string_view, 5> STREAM_NAMES = {
    streams::hist_conflict_declare, streams::hist_conflict_battle, streams::hist_conflict_battle_win,
    streams::hist_conflict_siege, streams::hist_conflict_sack,
};

constexpr std::array<Step, 3> STEPS = {Step::t2, Step::t6, Step::q4};

constexpr std::int64_t BOUND_2_40 = std::int64_t{1} << 40;

void field_in(bool ok, const std::string& field) {
    if (!ok) {
        fail("history: tuning: conflict." + field + " outside its schema range");
    }
}

bool in(std::int64_t v, std::int64_t lo, std::int64_t hi) noexcept {
    return v >= lo && v <= hi;
}

}  // namespace

// --- Tuning (12.1) -------------------------------------------------------------------------------

std::int64_t ConflictByKind::of(SettlementKind kind) const noexcept {
    switch (kind) {
        case SettlementKind::hamlet: return hamlet;
        case SettlementKind::village: return village;
        case SettlementKind::town: return town;
        case SettlementKind::city: return city;
        case SettlementKind::fortress: return fortress;
    }
    return 0;
}

std::int64_t ConflictCamp::of(std::int64_t ka, std::int64_t kb) const {
    if (!in(ka, 0, 2) || !in(kb, 0, 2)) {
        fail("history: conflict: camp " + int_text(in(ka, 0, 2) ? kb : ka) + " is not 0, 1 or 2");
    }
    const std::int64_t lo = std::min(ka, kb);
    const std::int64_t hi = std::max(ka, kb);
    if (lo == 0) {
        return hi == 0 ? good_good : (hi == 1 ? good_neutral : good_evil);
    }
    if (lo == 1) {
        return hi == 1 ? neutral_neutral : neutral_evil;
    }
    return evil_evil;
}

ConflictTuning conflict_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::HISTORY_KIND, data::HISTORY_CONFLICT_ID);
    if (d == nullptr) {
        fail("history: tuning: the content has no definition history.conflict");
    }
    const data::Record& r = d->record;
    const auto by_kind = [](const data::Record& k) {
        return ConflictByKind{k.get_int("hamlet"), k.get_int("village"), k.get_int("town"), k.get_int("city"),
                              k.get_int("fortress")};
    };
    ConflictTuning t;
    const data::Record& camp = r.get_record("camp");
    t.camp = ConflictCamp{camp.get_int("good_good"),       camp.get_int("good_neutral"), camp.get_int("good_evil"),
                          camp.get_int("neutral_neutral"), camp.get_int("neutral_evil"), camp.get_int("evil_evil")};
    t.enmity_max = r.get_int("enmity_max");
    t.border_friction = r.get_int("border_friction");
    t.want_weight = r.get_int("want_weight");
    t.hunger_full = r.get_int("hunger_full");
    t.fear_share = r.get_int("fear_share");
    t.fear_weight = r.get_int("fear_weight");
    t.tick_strain_permille = r.get_int("tick_strain_permille");
    t.quiet_strain_permille = r.get_int("quiet_strain_permille");
    t.strain_max = r.get_int("strain_max");
    t.claim_strain = r.get_int("claim_strain");
    t.declare_min = r.get_int("declare_min");
    t.declare_span = r.get_int("declare_span");
    t.declare_permille = r.get_int("declare_permille");
    t.declare_ratio = r.get_int("declare_ratio");
    t.shelter_years = r.get_int("shelter_years");
    t.march_hops = r.get_int("march_hops");
    t.cost_penalty = r.get_int("cost_penalty");
    t.levy_permille = by_kind(r.get_record("levy_permille"));
    t.terrain_defense = r.get_int("terrain_defense");
    t.pass_defense = r.get_int("pass_defense");
    t.river_defense = r.get_int("river_defense");
    t.fort_defense = r.get_int("fort_defense");
    t.battle_permille = r.get_int("battle_permille");
    t.loss_loser_permille = r.get_int("loss_loser_permille");
    t.loss_winner_permille = r.get_int("loss_winner_permille");
    t.garrison_permille = r.get_int("garrison_permille");
    t.walls_permille = by_kind(r.get_record("walls_permille"));
    t.capital_walls = r.get_int("capital_walls");
    t.sack_base = r.get_int("sack_base");
    t.sack_hostility = r.get_int("sack_hostility");
    t.sack_toll_permille = r.get_int("sack_toll_permille");
    t.capture_toll_permille = r.get_int("capture_toll_permille");
    t.exhaustion_permille = r.get_int("exhaustion_permille");
    t.war_max_years = r.get_int("war_max_years");
    t.truce_years = r.get_int("truce_years");
    t.war_grudge = r.get_int("war_grudge");
    t.capture_grudge = r.get_int("capture_grudge");
    t.secession_grudge = r.get_int("secession_grudge");
    t.grudge_decay_per_century = r.get_int("grudge_decay_per_century");
    t.last_refuge = r.get_int("last_refuge");
    t.capital_sacked_salience = r.get_int("capital_sacked_salience");
    t.capital_captured_salience = r.get_int("capital_captured_salience");
    return t;
}

void check_conflict_tuning(const ConflictTuning& t) {
    const std::pair<std::string_view, std::int64_t> camp[] = {
        {"good_good", t.camp.good_good},         {"good_neutral", t.camp.good_neutral},
        {"good_evil", t.camp.good_evil},         {"neutral_neutral", t.camp.neutral_neutral},
        {"neutral_evil", t.camp.neutral_evil},   {"evil_evil", t.camp.evil_evil}};
    for (const auto& [name, v] : camp) {
        field_in(in(v, -1000, 1000), "camp." + std::string(name));
    }
    field_in(in(t.enmity_max, -1000, 1000), "enmity_max");
    field_in(in(t.border_friction, 0, 2000), "border_friction");
    field_in(in(t.want_weight, 0, 2000), "want_weight");
    field_in(in(t.hunger_full, 1, 100000), "hunger_full");
    field_in(in(t.fear_share, 0, 999), "fear_share");
    field_in(in(t.fear_weight, 0, 2000), "fear_weight");
    field_in(in(t.tick_strain_permille, 0, 1000), "tick_strain_permille");
    field_in(in(t.quiet_strain_permille, 0, 1000), "quiet_strain_permille");
    field_in(in(t.strain_max, 1, 1000000), "strain_max");
    field_in(in(t.claim_strain, 1, 1000000), "claim_strain");
    field_in(in(t.declare_min, 0, 1000000), "declare_min");
    field_in(in(t.declare_span, 1, 1000000), "declare_span");
    field_in(in(t.declare_permille, 0, 1000), "declare_permille");
    field_in(in(t.declare_ratio, 1, 10000), "declare_ratio");
    field_in(in(t.shelter_years, 0, 1000), "shelter_years");
    field_in(in(t.march_hops, 1, 4), "march_hops");
    field_in(in(t.cost_penalty, 0, 10000), "cost_penalty");
    const auto kinds = [](const ConflictByKind& k) {
        return std::array<std::pair<std::string_view, std::int64_t>, 5>{
            {{"hamlet", k.hamlet}, {"village", k.village}, {"town", k.town}, {"city", k.city}, {"fortress", k.fortress}}};
    };
    for (const auto& [name, v] : kinds(t.levy_permille)) {
        field_in(in(v, 0, 1000), "levy_permille." + std::string(name));
    }
    field_in(in(t.terrain_defense, 0, 10000), "terrain_defense");
    field_in(in(t.pass_defense, 0, 10000), "pass_defense");
    field_in(in(t.river_defense, 0, 10000), "river_defense");
    field_in(in(t.fort_defense, 0, 10000), "fort_defense");
    field_in(in(t.battle_permille, 0, 1000), "battle_permille");
    field_in(in(t.loss_loser_permille, 0, 1000), "loss_loser_permille");
    field_in(in(t.loss_winner_permille, 0, 1000), "loss_winner_permille");
    field_in(in(t.garrison_permille, 0, 1000), "garrison_permille");
    for (const auto& [name, v] : kinds(t.walls_permille)) {
        field_in(in(v, 1, 100000), "walls_permille." + std::string(name));
    }
    field_in(in(t.capital_walls, 0, 100000), "capital_walls");
    field_in(in(t.sack_base, 0, 1000), "sack_base");
    field_in(in(t.sack_hostility, 0, 2000), "sack_hostility");
    field_in(in(t.sack_toll_permille, 0, 1000), "sack_toll_permille");
    field_in(in(t.capture_toll_permille, 0, 1000), "capture_toll_permille");
    field_in(in(t.exhaustion_permille, 1, 1000), "exhaustion_permille");
    field_in(in(t.war_max_years, 1, 1000), "war_max_years");
    field_in(in(t.truce_years, 0, 10000), "truce_years");
    field_in(in(t.war_grudge, 0, 2000), "war_grudge");
    field_in(in(t.capture_grudge, 0, 2000), "capture_grudge");
    field_in(in(t.secession_grudge, 0, 2000), "secession_grudge");
    field_in(in(t.grudge_decay_per_century, 0, 10000), "grudge_decay_per_century");
    field_in(in(t.last_refuge, 0, 1), "last_refuge");
    field_in(in(t.capital_sacked_salience, 0, 100), "capital_sacked_salience");
    field_in(in(t.capital_captured_salience, 0, 100), "capital_captured_salience");
    if (t.claim_strain > t.declare_min) {
        fail("history: tuning: claim_strain must not be above declare_min");
    }
    if (t.declare_min + t.declare_span > t.strain_max) {
        fail("history: tuning: declare_min + declare_span must not be above strain_max");
    }
}

// --- Pure rules ----------------------------------------------------------------------------------

ReachTable reach_from(const ConflictGraph& graph, std::span<const std::int64_t> from, std::int64_t hops) {
    const std::size_t n = graph.size();
    ReachTable t{std::vector<std::int64_t>(n, -1), std::vector<std::int64_t>(n, 0)};
    for (const std::int64_t r : from) {
        if (r < 1 || static_cast<std::size_t>(r) > n) {
            fail("history: conflict: region " + int_text(r) + " does not exist");
        }
        t.cost[static_cast<std::size_t>(r - 1)] = 0;
        t.last[static_cast<std::size_t>(r - 1)] = r;
    }
    // Paths of at most i borders, i = 1 .. hops: the smallest (cost, last)
    // over every path, lexicographically (5.1).
    for (std::int64_t i = 0; i < hops; ++i) {
        ReachTable next = t;
        for (std::size_t u = 0; u < n; ++u) {
            const std::int64_t cu = t.cost[u];
            if (cu < 0) {
                continue;
            }
            const auto m = static_cast<std::int64_t>(u) + 1;
            for (const PeoplesNeighbour& nb : graph[u]) {
                const auto v = static_cast<std::size_t>(nb.region - 1);
                const std::int64_t c = checked_add(cu, nb.cost);
                if (next.cost[v] < 0 || std::pair(c, m) < std::pair(next.cost[v], next.last[v])) {
                    next.cost[v] = c;
                    next.last[v] = m;
                }
            }
        }
        t = std::move(next);
    }
    return t;
}

std::vector<FrontEntry> front_of(const ReachTable& reach_x, std::span<const std::int64_t> presence_x,
                                 std::span<const std::int64_t> presence_y) {
    std::vector<FrontEntry> out;
    for (const std::int64_t r : presence_y) {
        if (std::binary_search(presence_x.begin(), presence_x.end(), r)) {
            out.push_back(FrontEntry{r, 1, r});
            continue;
        }
        const auto i = static_cast<std::size_t>(r - 1);
        if (i < reach_x.cost.size() && reach_x.cost[i] >= 0) {
            out.push_back(FrontEntry{r, reach_x.cost[i], reach_x.last[i]});
        }
    }
    return out;
}

std::vector<FrontEntry> front_of(const ConflictGraph& graph, std::span<const std::int64_t> presence_x,
                                 std::span<const std::int64_t> presence_y, std::int64_t hops) {
    return front_of(reach_from(graph, presence_x, hops), presence_x, presence_y);
}

std::int64_t disposition(const ConflictTuning& t, std::int64_t camp_a, std::int64_t camp_b, std::int64_t grudge) {
    return checked_sub(t.camp.of(camp_a, camp_b), grudge);
}

std::int64_t hostility(std::int64_t d) noexcept {
    return std::max<std::int64_t>(0, -d);
}

std::int64_t want_of(const ConflictTuning& t, bool wants_held_land, std::int64_t hunger) {
    if (!wants_held_land) {
        return 0;
    }
    return floor_div(checked_mul(t.want_weight, std::min(hunger, t.hunger_full)), t.hunger_full);
}

std::int64_t share_of(std::int64_t non_islet_held, std::int64_t non_islet_regions) {
    return non_islet_regions > 0 ? floor_div(checked_mul(non_islet_held, 1000), non_islet_regions) : 0;
}

std::int64_t fear_of(const ConflictTuning& t, std::int64_t share) {
    return floor_div(checked_mul(t.fear_weight, std::max<std::int64_t>(0, share - t.fear_share)), 1000 - t.fear_share);
}

std::int64_t drive_of(const ConflictTuning& t, std::int64_t d, std::int64_t want, std::int64_t fear) {
    return std::max<std::int64_t>(0, checked_add(checked_add(checked_sub(t.border_friction, d), want), fear));
}

std::int64_t strain_rate(std::int64_t g, std::int64_t drive, std::int64_t k) {
    if (k < 1) {
        fail("history: conflict: contact cost " + int_text(k) + " below 1");
    }
    std::int64_t v = floor_div(checked_mul(g, g), 1000);
    v = floor_div(checked_mul(v, drive), 1000);
    v = floor_div(checked_mul(v, floor_div(1000, k)), 1000);
    return v;
}

std::int64_t strain_after(const ConflictTuning& t, std::int64_t strain, std::int64_t rate, std::int64_t years,
                          std::int64_t permille, std::int64_t intensity) {
    const std::int64_t add =
        floor_div(checked_mul(checked_mul(checked_mul(rate, years), permille), intensity), 1000);
    return std::min(checked_add(strain, add), checked_mul(t.strain_max, 1000));
}

std::int64_t declare_permille(const ConflictTuning& t, std::int64_t s, std::int64_t force_a, std::int64_t force_b,
                              std::int64_t g) {
    if (s < t.declare_min) {
        return 0;
    }
    const std::int64_t ramp = std::min<std::int64_t>(1000, floor_div(checked_mul(s - t.declare_min, 1000), t.declare_span));
    std::int64_t p = floor_div(checked_mul(t.declare_permille, ramp), 1000);
    std::int64_t need = floor_div(checked_mul(force_b, t.declare_ratio), 1000);
    need = floor_div(checked_mul(need, 1000 - g), 1000);
    const std::int64_t odds = std::min<std::int64_t>(1000, floor_div(checked_mul(force_a, 1000), std::max<std::int64_t>(1, need)));
    p = floor_div(checked_mul(p, odds), 1000);
    return p;
}

std::int64_t force_of(const ConflictTuning& t, std::span<const std::pair<std::int64_t, SettlementKind>> settlements,
                      std::int64_t prowess) {
    std::int64_t levy = 0;
    for (const auto& [p, kind] : settlements) {
        levy = checked_add(levy, floor_div(checked_mul(p, t.levy_permille.of(kind)), 1000));
    }
    return floor_div(checked_mul(levy, prowess), 1000);
}

std::int64_t cover_of(const PeoplesRegion& region, std::span<const std::int64_t> cover_biomes) {
    std::int64_t c = region.rough;
    for (const auto& [biome, share] : region.shares) {
        if (std::find(cover_biomes.begin(), cover_biomes.end(), biome) != cover_biomes.end()) {
            c = checked_add(c, share);
        }
    }
    return std::min<std::int64_t>(1000, c);
}

std::int64_t defence_of(const ConflictTuning& t, const DefenceTerms& d) {
    std::int64_t def = checked_add(1000, floor_div(checked_mul(t.terrain_defense, d.cover), 1000));
    if (d.pass) {
        def = checked_add(def, t.pass_defense);
    }
    if (d.river) {
        def = checked_add(def, t.river_defense);
    }
    if (d.fort) {
        def = checked_add(def, t.fort_defense);
    }
    return def;
}

std::int64_t battle_permille_of(std::int64_t attack, std::int64_t defence_force, std::int64_t def) {
    const std::int64_t d = floor_div(checked_mul(defence_force, def), 1000);
    const std::int64_t sum = checked_add(attack, d);
    return sum > 0 ? floor_div(checked_mul(attack, 1000), sum) : 500;
}

std::int64_t garrison_of(const ConflictTuning& t, std::int64_t population, std::int64_t prowess, SettlementKind kind,
                         bool capital) {
    const std::int64_t walls = checked_add(t.walls_permille.of(kind), capital ? t.capital_walls : 0);
    std::int64_t g = floor_div(checked_mul(population, t.garrison_permille), 1000);
    g = floor_div(checked_mul(g, prowess), 1000);
    return floor_div(checked_mul(g, walls), 1000);
}

std::int64_t siege_permille_of(std::int64_t attack_left, std::int64_t garrison) {
    const std::int64_t sum = checked_add(attack_left, garrison);
    return sum > 0 ? floor_div(checked_mul(attack_left, 1000), sum) : 0;
}

std::vector<std::int64_t> casualty_losses(std::span<const std::int64_t> populations, std::int64_t dead) {
    std::vector<std::int64_t> loss(populations.size(), 0);
    std::int64_t total = 0;
    for (const std::int64_t p : populations) {
        total = checked_add(total, p);
    }
    if (total == 0 || dead <= 0) {
        return loss;
    }
    dead = std::min(dead, total);
    std::int64_t given = 0;
    for (std::size_t i = 0; i < populations.size(); ++i) {
        loss[i] = floor_div(checked_mul(dead, populations[i]), total);
        given += loss[i];
    }
    const std::int64_t rest = dead - given;
    if (rest > 0) {
        std::size_t big = 0;
        for (std::size_t i = 1; i < populations.size(); ++i) {
            if (populations[i] - loss[i] > populations[big] - loss[big]) {
                big = i;
            }
        }
        loss[big] += std::min(rest, populations[big] - loss[big]);
    }
    return loss;
}

std::int64_t war_score(std::int64_t won_a, std::int64_t won_b, std::int64_t taken_a, std::int64_t taken_b) {
    return checked_add(checked_sub(won_a, won_b), checked_mul(2, checked_sub(taken_a, taken_b)));
}

// --- Setup ----------------------------------------------------------------------------------------

ConflictSubsystem::ConflictSubsystem(PeoplesSubsystem& peoples, const data::ContentSet& content)
    : peoples_(peoples), tuning_(conflict_tuning_from(content)) {
    check_conflict_tuning(tuning_);
}

ConflictSubsystem::ConflictSubsystem(PeoplesSubsystem& peoples, const ConflictTuning& tuning)
    : peoples_(peoples), tuning_(tuning) {
    check_conflict_tuning(tuning_);
}

std::span<const Step> ConflictSubsystem::steps() const {
    return STEPS;
}

void ConflictSubsystem::prepare(const StepContext& ctx) {
    if (prepared_) {
        return;
    }
    const HistoryInputs& in = ctx.inputs();
    for (std::size_t i = 0; i < STREAM_NAMES.size(); ++i) {
        keys_[i] = stream_key(in.seed, STREAM_NAMES[i]);
    }
    const PeoplesTuning peoples_tuning = peoples_tuning_from(ctx.content());
    switch (in.params.event_intensity) {
        case data::EventIntensity::calm: intensity_ = peoples_tuning.intensity_calm; break;
        case data::EventIntensity::normal: intensity_ = peoples_tuning.intensity_normal; break;
        case data::EventIntensity::turbulent: intensity_ = peoples_tuning.intensity_turbulent; break;
    }
    for (const db::RaceRow& row : in.races) {
        const data::Definition* d = ctx.content().find(data::RACE_KIND, row.key);
        if (d == nullptr) {
            fail("history: conflict: race '" + row.key + "' is not in the content");
        }
        races_.push_back(ConflictRace{row.id, row.key, row.camp, d->record.get_int("aggression"),
                                      d->record.get_int("prowess_permille")});
    }
    const PeoplesGeo& g = peoples_.geo();
    const std::int64_t nr = g.region_count();
    graph_.assign(static_cast<std::size_t>(nr), {});
    for (std::int64_t r = 1; r <= nr; ++r) {
        const std::span<const PeoplesNeighbour> nb = g.neighbours(r);
        graph_[static_cast<std::size_t>(r - 1)].assign(nb.begin(), nb.end());
        non_islet_ += g.region(r).islet ? 0 : 1;
    }
    for (std::size_t i = 0; i < in.region_borders.size(); ++i) {
        const db::RegionBorderRow& b = in.region_borders[i];
        (void)border_.try_emplace(ordered(b.a, b.b), i);
    }
    for (const std::string_view key : {"temperate_forest", "wet_woodland", "fen", "marsh"}) {
        for (const db::BiomeRow& b : in.biomes) {
            if (b.key == key) {
                cover_biomes_.push_back(b.id);
            }
        }
    }
    prepared_ = true;
}

// --- Lookups -------------------------------------------------------------------------------------

const ConflictRace& ConflictSubsystem::race_of_civ(const StepContext& ctx, std::int64_t civ) const {
    const std::int64_t race = ctx.state().civ(civ).race;
    for (const ConflictRace& r : races_) {
        if (r.id == race) {
            return r;
        }
    }
    fail("history: conflict: race " + int_text(race) + " does not exist");
}

bool ConflictSubsystem::alive_visible(const StepContext& ctx, std::int64_t civ) const {
    const Civilization& c = ctx.state().civ(civ);
    return !c.ended_year && ctx.visible(c);
}

std::vector<std::int64_t> ConflictSubsystem::alive_civs(const StepContext& ctx) const {
    std::vector<std::int64_t> out;
    const std::int64_t n = civ_count(ctx.state());
    for (std::int64_t c = 1; c <= n; ++c) {
        if (alive_visible(ctx, c)) {
            out.push_back(c);
        }
    }
    return out;
}

std::vector<std::int64_t> ConflictSubsystem::presence(const StepContext& ctx, std::int64_t civ) const {
    const State& state = ctx.state();
    const std::int64_t nr = static_cast<std::int64_t>(graph_.size());
    std::vector<std::uint8_t> in_presence(static_cast<std::size_t>(nr) + 1, 0);
    for (std::int64_t r = 1; r <= nr; ++r) {
        in_presence[static_cast<std::size_t>(r)] = state.region(r).owner == civ ? 1 : 0;
    }
    const std::int64_t ns = settlement_count(state);
    for (std::int64_t id = 1; id <= ns; ++id) {
        const Settlement& s = state.settlement(id);
        if (s.owner_civ == civ && standing(s) && ctx.visible(s)) {
            in_presence[static_cast<std::size_t>(s.region)] = 1;
        }
    }
    std::vector<std::int64_t> out;
    for (std::int64_t r = 1; r <= nr; ++r) {
        if (in_presence[static_cast<std::size_t>(r)] != 0) {
            out.push_back(r);
        }
    }
    return out;
}

std::vector<std::int64_t> ConflictSubsystem::visible_settlements(const StepContext& ctx, std::int64_t civ,
                                                                 std::int64_t region) const {
    const State& state = ctx.state();
    std::vector<std::int64_t> out;
    const std::int64_t ns = settlement_count(state);
    for (std::int64_t id = 1; id <= ns; ++id) {
        const Settlement& s = state.settlement(id);
        if (s.owner_civ == civ && standing(s) && ctx.visible(s) && (region == 0 || s.region == region)) {
            out.push_back(id);
        }
    }
    return out;
}

std::int64_t ConflictSubsystem::force(const StepContext& ctx, std::int64_t civ) const {
    std::vector<std::pair<std::int64_t, SettlementKind>> list;
    for (const std::int64_t id : visible_settlements(ctx, civ, 0)) {
        const Settlement& s = ctx.state().settlement(id);
        list.emplace_back(s.population, s.kind);
    }
    const std::int64_t f = force_of(tuning_, list, race_of_civ(ctx, civ).prowess);
    if (f > BOUND_2_40) {
        fail("history: bound: conflict force of civilization " + int_text(civ) + " exceeds 2^40");
    }
    return f;
}

std::int64_t ConflictSubsystem::open_wars_of(std::int64_t civ) const {
    std::int64_t n = 0;
    for (const ConflictWar& w : wars_) {
        n += (w.open && (w.attacker == civ || w.defender == civ)) ? 1 : 0;
    }
    return n;
}

bool ConflictSubsystem::at_war(std::int64_t a, std::int64_t b) const {
    return std::any_of(wars_.begin(), wars_.end(), [a, b](const ConflictWar& w) {
        return w.open && ((w.attacker == a && w.defender == b) || (w.attacker == b && w.defender == a));
    });
}

bool ConflictSubsystem::attacking(std::int64_t a) const {
    return std::any_of(wars_.begin(), wars_.end(), [a](const ConflictWar& w) { return w.open && w.attacker == a; });
}

bool ConflictSubsystem::truce_over(const StepContext& ctx, std::int64_t a, std::int64_t b) const {
    const auto it = last_peace_.find(ordered(a, b));
    return it == last_peace_.end() || ctx.year() >= checked_add(it->second.year, tuning_.truce_years);
}

std::int64_t ConflictSubsystem::disposition_of(const StepContext& ctx, std::int64_t a, std::int64_t b) const {
    return disposition(tuning_, race_of_civ(ctx, a).camp, race_of_civ(ctx, b).camp, grudge(a, b));
}

std::int64_t ConflictSubsystem::strain(std::int64_t a, std::int64_t b) const {
    const auto it = strain_.find(Pair{a, b});
    return it == strain_.end() ? 0 : it->second;
}

std::int64_t ConflictSubsystem::grudge(std::int64_t a, std::int64_t b) const {
    const auto it = grudge_.find(ordered(a, b));
    return it == grudge_.end() ? 0 : it->second;
}

std::optional<std::pair<std::int64_t, std::int64_t>> ConflictSubsystem::claim(std::int64_t a, std::int64_t b) const {
    const auto it = claims_.find(Pair{a, b});
    if (it == claims_.end()) {
        return std::nullopt;
    }
    return std::pair(it->second.event, it->second.region);
}

void ConflictSubsystem::add_grudge(std::int64_t a, std::int64_t b, std::int64_t amount) {
    std::int64_t& g = grudge_[ordered(a, b)];
    g = checked_add(g, amount);
}

void ConflictSubsystem::drop_pair(std::int64_t a, std::int64_t b) {
    strain_[Pair{a, b}] = 0;
    strain_[Pair{b, a}] = 0;
    claims_.erase(Pair{a, b});
    claims_.erase(Pair{b, a});
}

// The value part of the target key of 5.3 and 6.2: sigma(R, r) - cost_penalty x cost.
std::int64_t ConflictSubsystem::target_key_value(const StepContext&, std::int64_t race, const FrontEntry& e) const {
    return checked_sub(peoples_.view().score(e.region, race), checked_mul(tuning_.cost_penalty, e.cost));
}

// 6.2's key: the smallest (-(sigma - cost_penalty x cost), R).
FrontEntry ConflictSubsystem::best_target(const StepContext& ctx, std::int64_t race,
                                          std::span<const FrontEntry> front) const {
    FrontEntry best = front.front();
    std::pair<std::int64_t, std::int64_t> best_key{-target_key_value(ctx, race, best), best.region};
    for (const FrontEntry& e : front) {
        const std::pair<std::int64_t, std::int64_t> k{-target_key_value(ctx, race, e), e.region};
        if (k < best_key) {
            best = e;
            best_key = k;
        }
    }
    return best;
}

std::vector<FrontEntry> ConflictSubsystem::live_front(const StepContext& ctx, std::int64_t x, std::int64_t y) const {
    const std::vector<std::int64_t> px = presence(ctx, x);
    const std::vector<std::int64_t> py = presence(ctx, y);
    return front_of(graph_, px, py, tuning_.march_hops);
}

// --- Run --------------------------------------------------------------------------------------------

void ConflictSubsystem::run(StepContext& ctx) {
    prepare(ctx);
    switch (ctx.step()) {
        case Step::t2:
            housekeeping(ctx);
            decay_grudges(1);
            update_strain(ctx, 1, tuning_.tick_strain_permille);
            declarations(ctx);
            last_t2_phase_ = ctx.phase_id();
            break;
        case Step::t6:
            housekeeping(ctx);
            fight(ctx);
            break;
        case Step::q4:
            housekeeping(ctx);
            quiet_ends(ctx);
            decay_grudges(ctx.years());
            update_strain(ctx, ctx.years(), tuning_.quiet_strain_permille);
            break;
        default: break;
    }
}

// --- Housekeeping (8.1) ---------------------------------------------------------------------------

void ConflictSubsystem::housekeeping(StepContext& ctx) {
    State& state = ctx.state();
    // 1. Relations, wars and claims of ended civilizations.
    std::vector<std::pair<RelationKey, std::int64_t>> to_close;
    for (const auto& [key, open] : state.relations()) {
        (void)open;
        if (key.a_type != static_cast<std::int64_t>(EntityType::civilization) ||
            key.b_type != static_cast<std::int64_t>(EntityType::civilization)) {
            continue;
        }
        const Pair p = ordered(key.a_id, key.b_id);
        const Civilization& lo = state.civ(p.first);
        const Civilization& hi = state.civ(p.second);
        if (lo.ended_year) {
            to_close.emplace_back(key, lo.ending_event);
        } else if (hi.ended_year) {
            to_close.emplace_back(key, hi.ending_event);
        }
    }
    for (const auto& [key, event] : to_close) {
        state.close_relation(key, event);
    }
    for (ConflictWar& w : wars_) {
        if (w.open && (state.civ(w.attacker).ended_year || state.civ(w.defender).ended_year)) {
            w.open = false;
        }
    }
    for (auto it = claims_.begin(); it != claims_.end();) {
        if (state.civ(it->first.first).ended_year || state.civ(it->first.second).ended_year) {
            it = claims_.erase(it);
        } else {
            ++it;
        }
    }
    // 2. Newly seen civilizations: the secession grudge.
    const std::vector<std::int64_t> alive = alive_civs(ctx);
    for (const std::int64_t c : alive) {
        if (!seen_.insert(c).second) {
            continue;
        }
        const std::int64_t founding = state.civ(c).founding_event;
        if (ctx.event_kind(founding) == "civ_seceded") {
            const std::int64_t parent = ctx.event_participant(founding, "from");
            if (parent != 0) {
                add_grudge(parent, c, tuning_.secession_grudge);
            }
        }
    }
    // 3. Enmities.
    std::vector<Pair> to_open;
    for (std::size_t i = 0; i < alive.size(); ++i) {
        for (std::size_t j = i + 1; j < alive.size(); ++j) {
            const Pair p{alive[i], alive[j]};
            if (enmity_opened_.count(p) != 0) {
                continue;
            }
            if (tuning_.camp.of(race_of_civ(ctx, p.first).camp, race_of_civ(ctx, p.second).camp) <=
                tuning_.enmity_max) {
                to_open.push_back(p);
            }
        }
    }
    for (const Pair& p : to_open) {
        state.open_relation(symmetric_relation(p.first, p.second, "enmity"), state.civ(p.second).founding_event);
        enmity_opened_.insert(p);
    }
}

void ConflictSubsystem::decay_grudges(std::int64_t years) {
    const std::int64_t d = floor_div(checked_mul(years, tuning_.grudge_decay_per_century), 100);
    for (auto& [key, g] : grudge_) {
        (void)key;
        g = std::max<std::int64_t>(0, g - d);
    }
}

// --- Strain and claims (5.1 to 5.3) ---------------------------------------------------------------

void ConflictSubsystem::update_strain(StepContext& ctx, std::int64_t years, std::int64_t permille) {
    const State& state = ctx.state();
    const PeoplesView& view = peoples_.view();
    const PeoplesGeo& g = peoples_.geo();
    const bool quiet = ctx.step() == Step::q4;
    const std::vector<std::int64_t> alive = alive_civs(ctx);
    // The snapshot at the start of the update (5.3, 17).
    struct Snapshot {
        std::vector<std::int64_t> presence;
        std::vector<std::int64_t> held;
        std::vector<std::int64_t> wanted;
        std::int64_t share = 0;
        ReachTable reach;
    };
    std::map<std::int64_t, Snapshot> snap;
    const std::int64_t nr = g.region_count();
    for (const std::int64_t c : alive) {
        Snapshot s;
        s.presence = presence(ctx, c);
        std::int64_t non_islet_held = 0;
        for (std::int64_t r = 1; r <= nr; ++r) {
            if (state.region(r).owner == c) {
                s.held.push_back(r);
                non_islet_held += g.region(r).islet ? 0 : 1;
            }
        }
        s.wanted = view.wanted_held(ctx, c);  // once per civilization (8.4 item 7)
        s.share = share_of(non_islet_held, non_islet_);
        s.reach = reach_from(graph_, s.presence, tuning_.march_hops);
        snap.emplace(c, std::move(s));
    }
    contact_.clear();
    std::map<Pair, std::vector<FrontEntry>> fronts;
    for (const std::int64_t a : alive) {
        const Snapshot& sa = snap.at(a);
        for (const std::int64_t b : alive) {
            if (a == b) {
                continue;
            }
            const Snapshot& sb = snap.at(b);
            std::vector<FrontEntry> fr = front_of(sa.reach, sa.presence, sb.presence);
            if (fr.empty()) {
                continue;
            }
            std::int64_t k = fr.front().cost;
            for (const FrontEntry& e : fr) {
                k = std::min(k, e.cost);
            }
            contact_[Pair{a, b}] = k;
            fronts.emplace(Pair{a, b}, std::move(fr));
            if (at_war(a, b) || !truce_over(ctx, a, b)) {
                continue;
            }
            bool wants = false;
            for (const std::int64_t r : sa.wanted) {
                wants = wants || std::binary_search(sb.held.begin(), sb.held.end(), r);
            }
            const std::int64_t want = want_of(tuning_, wants, view.hunger(a));
            const std::int64_t fear = fear_of(tuning_, sb.share);
            const std::int64_t drive = drive_of(tuning_, disposition_of(ctx, a, b), want, fear);
            const std::int64_t rate = strain_rate(race_of_civ(ctx, a).aggression, drive, k);
            const std::int64_t next = strain_after(tuning_, strain(a, b), rate, years, permille, intensity_);
            if (next > BOUND_2_40) {
                fail("history: bound: conflict strain of civilization " + int_text(a) + " against " + int_text(b) +
                     " exceeds 2^40");
            }
            strain_[Pair{a, b}] = next;
        }
    }
    // Claims, in ascending (a, b) among the pairs in contact.
    for (const auto& [key, fr] : fronts) {
        const auto [a, b] = key;
        if (claims_.count(key) != 0 || at_war(a, b) || !truce_over(ctx, a, b) ||
            strain(a, b) < checked_mul(tuning_.claim_strain, 1000)) {
            continue;
        }
        if (quiet && ctx.quiet_events_left(a) <= 0) {
            continue;
        }
        const std::int64_t race = race_of_civ(ctx, a).id;
        const std::vector<std::int64_t>& wanted = snap.at(a).wanted;
        FrontEntry pick_entry = fr.front();
        std::tuple<std::int64_t, std::int64_t, std::int64_t> best_key{};
        bool first = true;
        for (const FrontEntry& e : fr) {
            const bool wanted_held = std::binary_search(wanted.begin(), wanted.end(), e.region) &&
                                     ctx.state().region(e.region).owner == b;
            const std::tuple<std::int64_t, std::int64_t, std::int64_t> k{wanted_held ? 0 : 1,
                                                                         -target_key_value(ctx, race, e), e.region};
            if (first || k < best_key) {
                pick_entry = e;
                best_key = k;
                first = false;
            }
        }
        const std::int64_t region = pick_entry.region;
        std::int64_t cause = 0;
        if (ctx.state().region(region).owner == b) {
            cause = ctx.state().region(region).owner_event;
        } else {
            std::int64_t best = 0;
            for (const std::int64_t id : visible_settlements(ctx, b, region)) {
                if (best == 0 || ctx.state().settlement(id).population > ctx.state().settlement(best).population) {
                    best = id;
                }
            }
            cause = best != 0 ? ctx.state().settlement(best).founding_event : 0;
        }
        EventSpec e = point_event(ctx, "claim", region, 0);
        e.participants = {civ_role("claimant", a), civ_role("holder", b)};
        if (cause != 0) {
            e.causes = {cause};
        }
        claims_[key] = Claim{ctx.append(e), region};
    }
}

// --- Tension (5.4) --------------------------------------------------------------------------------

std::optional<std::pair<TensionReport, ConflictSubsystem::Pair>> ConflictSubsystem::best_claim(
    const StepContext& ctx) const {
    std::optional<std::pair<TensionReport, Pair>> best;
    std::int64_t best_value = 0;
    for (const auto& [key, c] : claims_) {
        const auto [a, b] = key;
        if (ctx.state().civ(a).ended_year || ctx.state().civ(b).ended_year || contact_.count(key) == 0 ||
            at_war(a, b) || !truce_over(ctx, a, b)) {
            continue;
        }
        const std::int64_t v = floor_div(strain(a, b), 1000);
        if (v > best_value) {
            best_value = v;
            best = std::pair(TensionReport{v, c.event}, key);
        }
    }
    return best;
}

TensionReport ConflictSubsystem::tension(const StepContext& ctx) {
    if (!prepared_) {
        return TensionReport{};
    }
    const std::optional<std::pair<TensionReport, Pair>> best = best_claim(ctx);
    return best ? best->first : TensionReport{};
}

// --- Declarations (7.2) ---------------------------------------------------------------------------

void ConflictSubsystem::declarations(StepContext& ctx) {
    const State& state = ctx.state();
    std::optional<Pair> flare;
    if (ctx.phase_kind() == PhaseKind::flare && ctx.phase_id() != last_t2_phase_) {
        if (const std::optional<std::pair<TensionReport, Pair>> best = best_claim(ctx)) {
            flare = best->second;
        }
    }
    const std::vector<std::int64_t> alive = alive_civs(ctx);
    for (const std::int64_t a : alive) {
        if (attacking(a)) {
            continue;
        }
        std::vector<std::int64_t> candidates;
        for (const auto& [key, c] : claims_) {
            (void)c;
            const std::int64_t b = key.second;
            if (key.first != a || !alive_visible(ctx, b) || contact_.count(key) == 0 || at_war(a, b) ||
                !truce_over(ctx, a, b) || ctx.year() < checked_add(state.civ(b).founded_year, tuning_.shelter_years)) {
                continue;
            }
            candidates.push_back(b);
        }
        if (candidates.empty()) {
            continue;
        }
        if (flare && flare->first == a &&
            std::find(candidates.begin(), candidates.end(), flare->second) != candidates.end()) {
            declare(ctx, a, flare->second);
            continue;
        }
        std::int64_t b = candidates.front();
        for (const std::int64_t c : candidates) {
            if (strain(a, c) > strain(a, b)) {
                b = c;
            }
        }
        const std::int64_t s = floor_div(strain(a, b), 1000);
        if (s < tuning_.declare_min) {
            continue;
        }
        const std::int64_t p = declare_permille(tuning_, s, force(ctx, a), force(ctx, b), race_of_civ(ctx, a).aggression);
        if (chance_pm(ctx.draw(keys_[DECLARE], word(a), word(b), 0), p)) {
            declare(ctx, a, b);
        }
    }
}

void ConflictSubsystem::declare(StepContext& ctx, std::int64_t a, std::int64_t b) {
    State& state = ctx.state();
    const Claim claim_of = claims_.at(Pair{a, b});
    std::int64_t goal = claim_of.region;
    if (state.region(goal).owner != b && visible_settlements(ctx, b, goal).empty()) {
        const std::vector<FrontEntry> fr = live_front(ctx, a, b);
        if (!fr.empty()) {
            goal = best_target(ctx, race_of_civ(ctx, a).id, fr).region;
        }
    }
    EventSpec e = point_event(ctx, "war_declared", goal, 0);
    e.participants = {civ_role("attacker", a), civ_role("defender", b)};
    e.causes = {claim_of.event};
    if (const auto lp = last_peace_.find(ordered(a, b)); lp != last_peace_.end()) {
        e.causes.push_back(lp->second.event);
        std::sort(e.causes.begin(), e.causes.end());
    }
    const std::int64_t ev = ctx.append(e);
    const RelationKey peace = symmetric_relation(a, b, "peace");
    if (state.relations().count(peace) != 0) {
        state.close_relation(peace, ev);
    }
    state.open_relation(civ_relation(a, b, "war"), ev);
    drop_pair(a, b);
    ConflictWar w;
    w.declared = ev;
    w.attacker = a;
    w.defender = b;
    w.goal = goal;
    w.start = ctx.year();
    w.last_event = ev;
    w.initiative = a;
    w.pop0 = {peoples_.view().population(ctx, a), peoples_.view().population(ctx, b)};
    wars_.push_back(w);
}

// --- Quiet steps (7.8) ------------------------------------------------------------------------------

void ConflictSubsystem::quiet_ends(StepContext& ctx) {
    const std::size_t n = wars_.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (wars_[i].open) {
            (void)end_war(ctx, i, score_winner(wars_[i]));
        }
    }
}

std::int64_t ConflictSubsystem::score_winner(const ConflictWar& w) const {
    const std::int64_t s = war_score(w.won[0], w.won[1], w.taken[0], w.taken[1]);
    return s > 0 ? w.attacker : (s < 0 ? w.defender : 0);
}

}  // namespace worldgen::history
