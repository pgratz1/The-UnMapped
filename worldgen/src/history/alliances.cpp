// Alliances, trade and the dwarves' mines and arms (spec
// docs/specs/p5-04-alliances-trade.md sections 4 to 6).
//
// P3-04 6.9 as 6.10 states it: every pass that opens or closes a relation
// works on a copy of the keys taken before it; members hold ids only; every
// draw is keyed by ids and the year.

#include "worldgen/history/alliances.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "conflict_internal.hpp"
#include "worldgen/core/checked.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/history/monsters.hpp"
#include "worldgen/history/relations.hpp"

namespace worldgen::history {

using namespace conflict_detail;

namespace {

constexpr std::array<Step, 3> STEPS = {Step::t2, Step::t6, Step::q4};

std::int64_t clamp_i(std::int64_t v, std::int64_t lo, std::int64_t hi) noexcept {
    return std::min(hi, std::max(lo, v));
}

std::vector<std::int64_t> sorted_unique(std::vector<std::int64_t> v) {
    v.erase(std::remove(v.begin(), v.end(), std::int64_t{0}), v.end());
    std::stable_sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

RelationKey alliance_key(std::int64_t a, std::int64_t b) {
    return symmetric_relation(a, b, "alliance");
}

RelationKey trade_key(std::int64_t a, std::int64_t b) {
    return symmetric_relation(a, b, "trade");
}

}  // namespace

AlliancesTuning alliances_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::HISTORY_KIND, data::HISTORY_ALLIANCES_ID);
    if (d == nullptr) {
        fail("history: tuning: the content has no definition history.alliances");
    }
    const data::Record& r = d->record;
    AlliancesTuning t;
    t.ally_hops = r.get_int("ally_hops");
    t.ally_min_disposition = r.get_int("ally_min_disposition");
    t.ally_full_disposition = r.get_int("ally_full_disposition");
    t.ally_friend_disposition = r.get_int("ally_friend_disposition");
    t.ally_break_disposition = r.get_int("ally_break_disposition");
    t.allies_max = r.get_int("allies_max");
    t.ally_after_war_years = r.get_int("ally_after_war_years");
    t.foe_years = r.get_int("foe_years");
    t.fear_share = r.get_int("fear_share");
    t.form_foe_permille = r.get_int("form_foe_permille");
    t.form_fear_permille = r.get_int("form_fear_permille");
    t.form_trade_permille = r.get_int("form_trade_permille");
    t.form_friend_permille = r.get_int("form_friend_permille");
    t.form_max_permille = r.get_int("form_max_permille");
    t.bloc_share_max = r.get_int("bloc_share_max");
    t.bloc_lapse_share = r.get_int("bloc_lapse_share");
    t.term_years = r.get_int("term_years");
    t.refusals_to_break = r.get_int("refusals_to_break");
    t.defend_permille = r.get_int("defend_permille");
    t.attack_permille = r.get_int("attack_permille");
    t.hostility_full = r.get_int("hostility_full");
    t.ally_wars_max = r.get_int("ally_wars_max");
    t.ally_memory = r.get_int("ally_memory");
    t.renew_memory = r.get_int("renew_memory");
    t.join_memory = r.get_int("join_memory");
    t.refuse_memory = r.get_int("refuse_memory");
    t.betray_memory = r.get_int("betray_memory");
    t.ally_war_memory = r.get_int("ally_war_memory");
    t.trade_hops = r.get_int("trade_hops");
    t.trade_min_disposition = r.get_int("trade_min_disposition");
    t.trade_break_margin = r.get_int("trade_break_margin");
    t.trade_after_war_years = r.get_int("trade_after_war_years");
    t.trade_permille = r.get_int("trade_permille");
    t.trade_max = r.get_int("trade_max");
    t.trade_credit_years = r.get_int("trade_credit_years");
    t.trade_memory = r.get_int("trade_memory");
    t.trade_ally_years = r.get_int("trade_ally_years");
    t.spoils_plunder_permille = r.get_int("spoils_plunder_permille");
    t.spoils_floor_permille = r.get_int("spoils_floor_permille");
    t.spoils_cap_permille = r.get_int("spoils_cap_permille");
    t.spoils_grant_permille = r.get_int("spoils_grant_permille");
    t.spoils_warmth_permille = r.get_int("spoils_warmth_permille");
    t.spoils_part_permille = r.get_int("spoils_part_permille");
    t.spoils_need_permille = r.get_int("spoils_need_permille");
    t.spoils_grant_min = r.get_int("spoils_grant_min");
    t.spoils_grant_max = r.get_int("spoils_grant_max");
    t.share_memory = r.get_int("share_memory");
    t.withheld_memory = r.get_int("withheld_memory");
    t.withheld_grudge = r.get_int("withheld_grudge");
    t.withheld_to_break = r.get_int("withheld_to_break");
    t.mine_permille = r.get_int("mine_permille");
    t.mine_ore_min = r.get_int("mine_ore_min");
    t.mine_settlement_min = r.get_int("mine_settlement_min");
    t.arms_trade_permille = r.get_int("arms_trade_permille");
    t.mine_forms.clear();
    for (const data::Value& v : r.get_array("mine_forms")) {
        t.mine_forms.push_back(v.as_string());
    }
    t.spoils_large_share = r.get_int("spoils_large_share");
    t.spoils_large_part_permille = r.get_int("spoils_large_part_permille");
    t.soured_renew_permille = r.get_int("soured_renew_permille");
    t.renew_written_max = r.get_int("renew_written_max");  // P5-08 12.5
    t.soured_join_permille = r.get_int("soured_join_permille");
    t.weary_years = r.get_int("weary_years");
    t.join_weary_permille = r.get_int("join_weary_permille");
    t.join_busy_permille = r.get_int("join_busy_permille");
    t.join_fear_floor = r.get_int("join_fear_floor");
    t.join_warmth_floor = r.get_int("join_warmth_floor");
    t.grasping_join_permille = r.get_int("grasping_join_permille");
    t.join_claim_permille = r.get_int("join_claim_permille");
    t.excused_memory = r.get_int("excused_memory");
    t.refuse_grudge = r.get_int("refuse_grudge");
    t.betray_cooled_disposition = r.get_int("betray_cooled_disposition");
    t.betray_weak_permille = r.get_int("betray_weak_permille");
    return t;
}

// --- Setup ----------------------------------------------------------------------------------------

AlliancesSubsystem::AlliancesSubsystem(PeoplesSubsystem& peoples, ConflictSubsystem& conflict,
                                       RelationsSubsystem& relations, const data::ContentSet& content)
    : AlliancesSubsystem(peoples, conflict, relations, alliances_tuning_from(content)) {}

AlliancesSubsystem::AlliancesSubsystem(PeoplesSubsystem& peoples, ConflictSubsystem& conflict,
                                       RelationsSubsystem& relations, AlliancesTuning tuning)
    : peoples_(peoples), conflict_(conflict), relations_(relations), tuning_(std::move(tuning)) {
    if (tuning_.mine_forms.empty()) {
        fail("history: alliances: mine_forms is empty");
    }
    radius_ = std::max<std::int64_t>({0, tuning_.ally_hops, tuning_.trade_hops + 1});
}

std::span<const Step> AlliancesSubsystem::steps() const {
    return STEPS;
}

void AlliancesSubsystem::prepare(const StepContext& ctx) {
    if (prepared_) {
        return;
    }
    const HistoryInputs& in = ctx.inputs();
    form_key_ = stream_key(in.seed, streams::hist_alliances_form);
    join_key_ = stream_key(in.seed, streams::hist_alliances_join);
    trade_key_ = stream_key(in.seed, streams::hist_alliances_trade);
    spoils_key_ = stream_key(in.seed, streams::hist_alliances_spoils);
    mine_key_ = stream_key(in.seed, streams::hist_alliances_mine);
    renew_key_ = stream_key(in.seed, streams::hist_alliances_renew);
    for (const db::RaceRow& row : in.races) {
        const data::Definition* d = ctx.content().find(data::RACE_KIND, row.key);
        if (d == nullptr) {
            fail("history: alliances: race '" + row.key + "' is not in the content");
        }
        RaceData r;
        r.id = row.id;
        r.join = d->record.get_int("ally_join_permille");
        r.mining = d->record.get_int("mining_permille");
        r.muster = d->record.get_int("muster_permille");
        for (const data::Value& v : d->record.get_array("origin_allies")) {
            for (const db::RaceRow& other : in.races) {
                if (other.key == v.as_string()) {
                    r.origin_allies.push_back(other.id);
                }
            }
        }
        std::stable_sort(r.origin_allies.begin(), r.origin_allies.end());
        races_.push_back(std::move(r));
    }
    std::stable_sort(races_.begin(), races_.end(), [](const RaceData& a, const RaceData& b) { return a.id < b.id; });
    for (const db::RegionRow& row : in.regions) {
        ore_reach_.push_back(row.ore_reach_cells);
    }
    prepared_ = true;
}

const AlliancesSubsystem::RaceData& AlliancesSubsystem::race(std::int64_t race_id) const {
    for (const RaceData& r : races_) {
        if (r.id == race_id) {
            return r;
        }
    }
    fail("history: alliances: race " + int_text(race_id) + " does not exist");
}

std::int64_t AlliancesSubsystem::race_of(const StepContext& ctx, std::int64_t civ) const {
    return ctx.state().civ(civ).race;
}

bool AlliancesSubsystem::alive_visible(const StepContext& ctx, std::int64_t civ) const {
    const Civilization& c = ctx.state().civ(civ);
    return !c.ended_year && ctx.visible(c);
}

bool AlliancesSubsystem::free_civ(const StepContext& ctx, std::int64_t civ) const {
    return alive_visible(ctx, civ) && !conflict_.lying_low(ctx, civ);
}

bool AlliancesSubsystem::quiet_ok(const StepContext& ctx, std::int64_t civ) const {
    return ctx.step() != Step::q4 || ctx.quiet_events_left(civ) > 0;
}

// --- AllianceView (7.1) -----------------------------------------------------------------------------

bool AlliancesSubsystem::allied(std::int64_t a, std::int64_t b) const {
    return a != b && alliances_.count(ordered(a, b)) != 0;
}

std::span<const std::int64_t> AlliancesSubsystem::allies(std::int64_t c) const {
    const auto it = allies_of_.find(c);
    if (it == allies_of_.end()) {
        return {};
    }
    return it->second;
}

std::int64_t AlliancesSubsystem::trade_partners(std::int64_t c) const {
    std::int64_t n = 0;
    for (const auto& [p, t] : trades_) {
        (void)t;
        n += (p.first == c || p.second == c) ? 1 : 0;
    }
    return n;
}

std::vector<std::pair<std::int64_t, std::int64_t>> AlliancesSubsystem::partners(std::int64_t c) const {
    std::map<std::int64_t, std::int64_t> by_partner;
    for (const auto& [p, t] : trades_) {
        if (p.first == c || p.second == c) {
            by_partner[p.first == c ? p.second : p.first] = t.opened_event;
        }
    }
    for (const auto& [p, a] : alliances_) {  // the alliance's event when both
        if (p.first == c || p.second == c) {
            by_partner[p.first == c ? p.second : p.first] = a.origin ? 0 : a.formed_event;
        }
    }
    return std::vector<std::pair<std::int64_t, std::int64_t>>(by_partner.begin(), by_partner.end());
}

bool AlliancesSubsystem::is_seller(std::int64_t c) const {
    const auto it = mines_.lower_bound(Pair{c, 0});
    return it != mines_.end() && it->first.first == c;
}

bool AlliancesSubsystem::forged(std::int64_t c) const {
    return is_seller(c);
}

std::int64_t AlliancesSubsystem::forge_depth(std::int64_t c) const {
    if (sites_ == nullptr) {
        return 0;
    }
    std::int64_t d = 0;
    for (auto it = mines_.lower_bound(Pair{c, 0}); it != mines_.end() && it->first.first == c; ++it) {
        d = std::max(d, sites_->mine_depth(it->second.site));
    }
    return d;
}

std::int64_t AlliancesSubsystem::forge_site(std::int64_t c) const {
    if (sites_ == nullptr) {
        return 0;
    }
    std::int64_t best = 0;
    std::int64_t depth = -1;
    for (auto it = mines_.lower_bound(Pair{c, 0}); it != mines_.end() && it->first.first == c; ++it) {
        const std::int64_t s = it->second.site;
        const std::int64_t d = sites_->mine_depth(s);
        if (d > depth || (d == depth && s < best)) {
            best = s;
            depth = d;
        }
    }
    return best;
}

std::int64_t AlliancesSubsystem::arms_event(std::int64_t c) const {
    if (!arms(c)) {
        return 0;
    }
    std::int64_t seller = 0;
    std::int64_t event = 0;
    for (const auto& [p, t] : trades_) {  // ascending pair
        (void)t;
        const std::int64_t s = p.first == c ? p.second : (p.second == c ? p.first : 0);
        if (s == 0 || !is_seller(s) || (seller != 0 && s > seller)) {
            continue;
        }
        if (const auto it = arms_sold_.find(Pair{s, c}); it != arms_sold_.end()) {
            seller = s;
            event = it->second;
        }
    }
    return event;
}

bool AlliancesSubsystem::arms(std::int64_t c) const {
    if (is_seller(c)) {
        return false;
    }
    for (const auto& [p, t] : trades_) {
        (void)t;
        if ((p.first == c && is_seller(p.second)) || (p.second == c && is_seller(p.first))) {
            return true;
        }
    }
    return false;
}

void AlliancesSubsystem::rebuild_allies() {
    allies_of_.clear();
    for (const auto& [p, a] : alliances_) {
        (void)a;
        allies_of_[p.first].push_back(p.second);
        allies_of_[p.second].push_back(p.first);
    }
    for (auto& [c, list] : allies_of_) {
        (void)c;
        std::stable_sort(list.begin(), list.end());
    }
}

// --- The cache (6.2) ---------------------------------------------------------------------------------

void AlliancesSubsystem::build_cache(const StepContext& ctx) {
    const State& state = ctx.state();
    presence_.clear();
    share_.clear();
    sellers_at_start_.clear();
    const std::int64_t nc = civ_count(state);
    const std::int64_t nr = static_cast<std::int64_t>(ctx.geo().regions().size());
    const PeoplesGeo& geo = peoples_.geo();
    std::vector<std::vector<std::int64_t>> by_civ(static_cast<std::size_t>(nc) + 1);
    for (std::int64_t r = 1; r <= nr; ++r) {
        const std::int64_t o = state.region(r).owner;
        if (o >= 1 && o <= nc) {
            by_civ[static_cast<std::size_t>(o)].push_back(r);
        }
    }
    const std::int64_t ns = settlement_count(state);
    for (std::int64_t id = 1; id <= ns; ++id) {
        const Settlement& s = state.settlement(id);
        if (s.owner_civ >= 1 && s.owner_civ <= nc && standing(s) && ctx.visible(s)) {
            by_civ[static_cast<std::size_t>(s.owner_civ)].push_back(s.region);
        }
    }
    std::vector<std::int64_t> dist(static_cast<std::size_t>(nr) + 1, -1);
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (state.civ(c).ended_year) {
            continue;
        }
        share_.emplace(c, conflict_.share(ctx, c));
        if (!ctx.visible(state.civ(c))) {
            continue;
        }
        Presence p;
        p.regions = sorted_unique(by_civ[static_cast<std::size_t>(c)]);
        // A breadth-first search from P(c) over the open borders, to the
        // radius: the smallest PeoplesView::hops over the presence.
        std::vector<std::int64_t> queue = p.regions;
        for (const std::int64_t r : queue) {
            dist[static_cast<std::size_t>(r)] = 0;
        }
        for (std::size_t head = 0; head < queue.size(); ++head) {
            const std::int64_t u = queue[head];
            const std::int64_t du = dist[static_cast<std::size_t>(u)];
            if (du >= radius_) {
                continue;
            }
            for (const PeoplesNeighbour& v : geo.neighbours(u)) {
                std::int64_t& dv = dist[static_cast<std::size_t>(v.region)];
                if (dv < 0) {
                    dv = du + 1;
                    queue.push_back(v.region);
                }
            }
        }
        for (const std::int64_t r : queue) {
            p.near.emplace_back(r, dist[static_cast<std::size_t>(r)]);
            dist[static_cast<std::size_t>(r)] = -1;
        }
        std::stable_sort(p.near.begin(), p.near.end());
        presence_.emplace(c, std::move(p));
    }
    for (const auto& [key, m] : mines_) {
        (void)m;
        sellers_at_start_.insert(key.first);
    }
    feared_.clear();
    for (const auto& [c, s] : share_) {
        (void)s;
        const std::int64_t bs = bloc(c);
        if (bs >= tuning_.fear_share) {
            feared_.emplace_back(c, bs);
        }
    }
}

std::int64_t AlliancesSubsystem::hops(std::int64_t x, std::int64_t y) const {
    const auto px = presence_.find(x);
    const auto py = presence_.find(y);
    if (px == presence_.end() || py == presence_.end()) {
        return -1;
    }
    std::int64_t best = -1;
    for (const std::int64_t r : py->second.regions) {
        const auto it = std::lower_bound(px->second.near.begin(), px->second.near.end(),
                                         std::pair<std::int64_t, std::int64_t>{r, -1});
        if (it != px->second.near.end() && it->first == r && (best < 0 || it->second < best)) {
            best = it->second;
        }
    }
    return best;
}

bool AlliancesSubsystem::in_reach(std::int64_t x, std::int64_t y, std::int64_t limit) const {
    if (limit < 0) {
        return false;
    }
    const std::int64_t h = hops(x, y);
    return h >= 0 && h <= limit;
}

std::int64_t AlliancesSubsystem::d(const StepContext& ctx, std::int64_t x, std::int64_t y) const {
    return conflict_.disposition_of(ctx, x, y);
}

std::int64_t AlliancesSubsystem::warmth(const StepContext& ctx, std::int64_t x, std::int64_t y) const {
    const std::int64_t v = d(ctx, x, y);
    if (v < tuning_.ally_min_disposition) {
        return 0;
    }
    return clamp_i(floor_div(checked_mul(v - tuning_.ally_min_disposition, 1000),
                             tuning_.ally_full_disposition - tuning_.ally_min_disposition),
                   0, 1000);
}

std::int64_t AlliancesSubsystem::share_of(std::int64_t c) const {
    const auto it = share_.find(c);
    return it == share_.end() ? 0 : it->second;
}

std::int64_t AlliancesSubsystem::bloc(std::int64_t c) const {
    std::int64_t s = share_of(c);
    for (const std::int64_t z : allies(c)) {
        s = checked_add(s, share_of(z));
    }
    return std::min<std::int64_t>(1000, s);
}

// 6.3: the blocs after forming.
bool AlliancesSubsystem::bloc_test(std::int64_t x, std::int64_t y) const {
    const auto rest = [this](std::int64_t c) {
        std::int64_t s = share_of(c);
        for (const std::int64_t z : allies(c)) {
            s = checked_add(s, share_of(z));
        }
        return s;
    };
    return checked_add(bloc(x), rest(y)) <= tuning_.bloc_share_max &&
           checked_add(bloc(y), rest(x)) <= tuning_.bloc_share_max;
}

bool AlliancesSubsystem::lately_at_war(const StepContext& ctx, std::int64_t x, std::int64_t y,
                                       std::int64_t years) const {
    if (conflict_.at_war(x, y)) {
        return true;
    }
    const auto lp = conflict_.last_peace(x, y);
    return lp && lp->first > ctx.year() - years;
}

// 6.2's reasons of the pair (x, y), x < y.
AlliancesSubsystem::Reasons AlliancesSubsystem::reasons(const StepContext& ctx, std::int64_t x,
                                                        std::int64_t y) const {
    Reasons out;
    const std::int64_t year = ctx.year();
    // 1. A common enemy: the latest declaration between z and one of x, y.
    for (auto it = declarations_.rbegin(); it != declarations_.rend(); ++it) {
        const Declaration& w = *it;
        std::int64_t side = 0;
        std::int64_t z = 0;
        if (w.attacker == x || w.attacker == y) {
            side = w.attacker;
            z = w.defender;
        } else if (w.defender == x || w.defender == y) {
            side = w.defender;
            z = w.attacker;
        } else {
            continue;
        }
        if (z == x || z == y) {
            continue;
        }
        if (!(conflict_.war_open(w.event) || w.year > year - tuning_.foe_years)) {
            continue;
        }
        if (!alive_visible(ctx, z)) {
            continue;
        }
        const std::int64_t other = side == x ? y : x;
        if (d(ctx, other, z) < 0 && in_reach(other, z, tuning_.ally_hops)) {
            out.enemy_event = w.event;
            out.enemy = z;
            break;
        }
    }
    // 2. Fear of a great realm or bloc.
    std::int64_t best_bs = -1;
    for (const auto& [z, bs] : feared_) {  // the bloc shares of the step's start, ascending id
        if (z == x || z == y || !alive_visible(ctx, z) || allied(z, x) || allied(z, y)) {
            continue;
        }
        if (bs <= best_bs) {
            continue;
        }
        if (in_reach(x, z, tuning_.ally_hops) && in_reach(y, z, tuning_.ally_hops) && d(ctx, x, z) < 0 &&
            d(ctx, y, z) < 0) {
            best_bs = bs;
            out.fear = z;
        }
    }
    if (out.fear != 0) {
        const State& state = ctx.state();
        const auto nr = static_cast<std::int64_t>(ctx.geo().regions().size());
        for (std::int64_t r = 1; r <= nr; ++r) {
            if (state.region(r).owner == out.fear) {
                out.fear_event = std::max(out.fear_event, state.region(r).owner_event);
            }
        }
        if (out.fear_event == 0) {  // reading A10: a feared realm that holds no region
            out.fear_event = state.civ(out.fear).founding_event;
        }
    }
    // 3. Long trade.
    if (const auto t = trades_.find(Pair{x, y}); t != trades_.end() && t->second.opened_year <= year - tuning_.trade_ally_years) {
        out.trade_event = t->second.opened_event;
    }
    // 4. Friendship.
    if (d(ctx, x, y) >= tuning_.ally_friend_disposition) {
        const std::int64_t younger = ctx.state().civ(std::max(x, y)).founding_event;
        const std::int64_t rx = race_of(ctx, x);
        const std::int64_t ry = race_of(ctx, y);
        std::int64_t cause = younger;
        if (rx != ry && relations_.band(rx, ry) == 2) {
            cause = std::max(cause, relations_.turn_event(rx, ry));
        }
        out.friend_event = cause;
    }
    return out;
}

// --- Relations and events -------------------------------------------------------------------------

void AlliancesSubsystem::open_alliance(StepContext& ctx, std::int64_t x, std::int64_t y, std::int64_t event,
                                       Alliance a) {
    ctx.state().open_relation(alliance_key(x, y), event);
    conflict_.forgive(x, y);  // reading: claims are set aside at the oath
    a.formed_year = ctx.year();
    a.formed_event = event;
    a.renew_year = ctx.year();
    a.renew_event = event;
    a.next_due = checked_add(ctx.year(), tuning_.term_years);  // P5-04a 6.1: d_1
    alliances_[ordered(x, y)] = a;
    rebuild_allies();
    relations_.credit(race_of(ctx, x), race_of(ctx, y), tuning_.ally_memory, event);
    ++stats_.formed;
    stats_.origin += a.origin ? 1 : 0;
}

void AlliancesSubsystem::close_alliance(StepContext& ctx, const Pair& p, std::int64_t event) {
    ctx.state().close_relation(alliance_key(p.first, p.second), event);
    closed_now_.insert(std::pair<Pair, bool>{p, true});
    alliances_.erase(p);
    rebuild_allies();
}

void AlliancesSubsystem::close_trade(StepContext& ctx, const Pair& p, std::int64_t event) {
    ctx.state().close_relation(trade_key(p.first, p.second), event);
    closed_now_.insert(std::pair<Pair, bool>{p, false});
    trades_.erase(p);
    arms_sold_.erase(Pair{p.first, p.second});
    arms_sold_.erase(Pair{p.second, p.first});
    ++stats_.trades_broken;
}

std::int64_t AlliancesSubsystem::append_pair_event(StepContext& ctx, std::string_view kind, const Pair& p,
                                                   std::int64_t value, std::vector<std::int64_t> causes) {
    const bool trade = kind == "trade_broken";
    EventSpec e = point_event(ctx, kind, 0, 0);
    e.participants = {civ_role(trade ? "partner_a" : "ally_a", p.first), civ_role(trade ? "partner_b" : "ally_b", p.second)};
    e.values = {value};
    e.causes = sorted_unique(std::move(causes));
    return ctx.append(e);
}

// Alliances and trades whose relation an ended side or a conquest closed are
// dropped with no event (6.7, 6.8).
void AlliancesSubsystem::drop_closed(const StepContext& ctx) {
    const auto& open = ctx.state().relations();
    bool changed = false;
    for (auto it = alliances_.begin(); it != alliances_.end();) {
        if (open.count(alliance_key(it->first.first, it->first.second)) == 0) {
            it = alliances_.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed) {
        rebuild_allies();
    }
    for (auto it = trades_.begin(); it != trades_.end();) {
        if (open.count(trade_key(it->first.first, it->first.second)) == 0) {
            arms_sold_.erase(Pair{it->first.first, it->first.second});
            arms_sold_.erase(Pair{it->first.second, it->first.first});
            it = trades_.erase(it);
        } else {
            ++it;
        }
    }
}

// --- Run (6.1) -----------------------------------------------------------------------------------------

void AlliancesSubsystem::run(StepContext& ctx) {
    prepare(ctx);
    // Ruling A9, keyed by year (audit A3: the relation table's key holds the year, so no pair reopens a row in
    // the year it closed, whatever the step).
    if (ctx.year() != closed_year_) {
        closed_now_.clear();
        closed_year_ = ctx.year();
    }
    drop_closed(ctx);
    switch (ctx.step()) {
        case Step::t2:
            build_cache(ctx);
            pending_spurns(ctx);
            read_log(ctx);
            cooling(ctx);
            blocs(ctx);
            renewals(ctx);
            origin_alliances(ctx);
            formations(ctx);
            trade_breaks(ctx);
            trade_openings(ctx);
            trade_credits(ctx);
            mine_closings(ctx);
            mine_openings(ctx);
            arms_sales(ctx);
            break;
        case Step::t6:
            joined_ends(ctx);
            mine_closings(ctx);
            break;
        case Step::q4:
            build_cache(ctx);
            pending_spurns(ctx);
            read_log(ctx);  // no declaration happens in a quiet step; the cursor only moves
            joined_ends(ctx);
            cooling(ctx);
            blocs(ctx);
            renewals(ctx);
            trade_breaks(ctx);
            trade_openings(ctx);
            trade_credits(ctx);
            mine_closings(ctx);
            mine_openings(ctx);
            arms_sales(ctx);
            break;
        default: break;
    }
}

// --- The log: betrayals, trade broken by war, calls (6.5, 6.6, 6.8) ------------------------------------

void AlliancesSubsystem::read_log(StepContext& ctx) {
    const std::int64_t last = ctx.last_event_id();
    std::vector<Declaration> fresh;
    for (std::int64_t e = cursor_; e <= last; ++e) {
        if (ctx.event_kind(e) == "war_declared") {
            fresh.push_back(Declaration{e, ctx.event_year(e), ctx.event_participant(e, "attacker"),
                                        ctx.event_participant(e, "defender")});
        }
    }
    cursor_ = last + 1;
    for (const Declaration& w : fresh) {
        declarations_.push_back(w);
        if (ctx.step() != Step::t2) {
            continue;  // declarations are T2 only (P3-06 7.8)
        }
        betrayal(ctx, w);
        const Pair p = ordered(w.attacker, w.defender);
        if (trades_.count(p) != 0) {
            const std::int64_t ev = append_pair_event(ctx, "trade_broken", p, 0, {w.event});
            close_trade(ctx, p, ev);
        }
        calls(ctx, w);
    }
    // Declarations older than every reason's window are no longer read.
    while (!declarations_.empty() && declarations_.front().year <= ctx.year() - tuning_.foe_years &&
           !conflict_.war_open(declarations_.front().event)) {
        declarations_.erase(declarations_.begin());
    }
}

void AlliancesSubsystem::betrayal(StepContext& ctx, const Declaration& w) {
    const Pair p = ordered(w.attacker, w.defender);
    const auto it = alliances_.find(p);
    if (it == alliances_.end()) {
        return;
    }
    // P5-04a 5: the motive, the first that holds.
    std::int64_t motive = 3;
    if (figures_ != nullptr && figures_->temper(w.attacker) == 3) {
        motive = 0;
    } else if (d(ctx, w.attacker, w.defender) < tuning_.betray_cooled_disposition) {
        motive = 1;
    } else if (checked_mul(conflict_.force(ctx, w.defender), 1000) <
               checked_mul(tuning_.betray_weak_permille, conflict_.force(ctx, w.attacker))) {
        motive = 2;
    }
    EventSpec e = point_event(ctx, "alliance_betrayed", 0, 0);
    e.participants = {civ_role("betrayer", w.attacker), civ_role("betrayed", w.defender)};
    e.values = {motive, ctx.year() - it->second.formed_year};
    e.causes = sorted_unique({w.event, it->second.renew_event});
    const std::int64_t ev = ctx.append(e);
    close_alliance(ctx, p, ev);
    betrayed_.insert(p);
    relations_.credit(race_of(ctx, w.attacker), race_of(ctx, w.defender), -tuning_.betray_memory, ev);
    ++stats_.betrayed;
}

void AlliancesSubsystem::calls(StepContext& ctx, const Declaration& w) {
    const State& state = ctx.state();
    const ConflictTuning& ct = conflict_.tuning();
    const std::int64_t year = ctx.year();
    for (std::int64_t side = 0; side <= 1; ++side) {
        const std::int64_t partner = side == 0 ? w.defender : w.attacker;
        const std::int64_t enemy = side == 0 ? w.attacker : w.defender;
        const std::span<const std::int64_t> list = allies(partner);
        const std::vector<std::int64_t> called(list.begin(), list.end());
        for (const std::int64_t z : called) {
            const auto al = alliances_.find(ordered(z, partner));
            if (al == alliances_.end() || !free_civ(ctx, z) || z == w.attacker || z == w.defender ||
                allied(z, enemy) || conflict_.at_war(z, enemy) || conflict_.at_war(z, partner) ||
                !conflict_.war_open(w.event) || !conflict_.reaches(ctx, z, enemy) ||
                ctx.year() < checked_add(state.civ(enemy).founded_year, ct.shelter_years) ||
                conflict_.open_wars_of(z) >= tuning_.ally_wars_max) {
                continue;
            }
            const std::int64_t dz = d(ctx, z, enemy);
            if (side == 1 && (!conflict_.truce_over(ctx, z, enemy) || dz >= 0)) {
                continue;
            }
            const std::int64_t join = race(race_of(ctx, z)).join;
            // P5-04a 4.2: the factors f0 to f7 (f3, f4, f6 and f7 for a defence call only).
            const std::pair<std::int64_t, std::int64_t> war_end = conflict_.last_war_end(z);
            std::array<std::int64_t, 8> f{};
            f.fill(1000);
            f[0] = join;
            if (war_end.second != 0 && war_end.first >= year - tuning_.weary_years) {
                f[1] = tuning_.join_weary_permille;
            }
            f[2] = std::max<std::int64_t>(0, 1000 - checked_mul(tuning_.join_busy_permille, conflict_.open_wars_of(z)));
            if (figures_ != nullptr && figures_->temper(z) == 3) {
                f[5] = tuning_.grasping_join_permille;
            }
            const bool soured_call = side == 0 && al->second.call_due && al->second.soured_ally == z;
            std::int64_t p = 0;
            if (side == 0) {
                const std::int64_t sigma = std::min<std::int64_t>(
                    1000, floor_div(checked_mul(checked_add(conflict_.force(ctx, z), conflict_.force(ctx, partner)), 1000),
                                    std::max<std::int64_t>(1, conflict_.force(ctx, enemy))));
                f[3] = tuning_.join_fear_floor + floor_div(checked_mul(1000 - tuning_.join_fear_floor, sigma), 1000);
                f[4] = tuning_.join_warmth_floor +
                       floor_div(checked_mul(1000 - tuning_.join_warmth_floor, warmth(ctx, z, partner)), 1000);
                if (conflict_.has_claim(z, partner)) {
                    f[6] = tuning_.join_claim_permille;
                }
                if (soured_call) {
                    f[7] = tuning_.soured_join_permille;
                }
                p = tuning_.defend_permille;
                for (const std::int64_t fk : f) {
                    p = floor_div(checked_mul(p, fk), 1000);
                }
            } else {
                const std::int64_t h = std::min<std::int64_t>(1000, floor_div(checked_mul(-dz, 1000), tuning_.hostility_full));
                p = floor_div(checked_mul(tuning_.attack_permille, join), 1000);
                p = floor_div(checked_mul(p, h), 1000);
                for (const std::size_t k : {std::size_t{1}, std::size_t{2}, std::size_t{5}}) {
                    p = floor_div(checked_mul(p, f[k]), 1000);
                }
            }
            if (soured_call) {
                alliances_.at(ordered(z, partner)).call_due = false;  // 3.5: cleared after the call
            }
            const std::int64_t renewal = al->second.renew_event;
            const bool origin = al->second.origin;
            const std::int64_t soured_share = al->second.soured_event;
            if (chance_pm(ctx.draw(join_key_, word(w.event), word(z), 0), p)) {
                EventSpec e = point_event(ctx, "ally_joined_war", 0, 0);
                e.participants = {civ_role("ally", z), civ_role("partner", partner), civ_role("enemy", enemy)};
                e.values = {side};
                e.causes = sorted_unique({w.event, renewal});
                const std::int64_t ev = ctx.append(e);
                conflict_.join_war(ctx, w.event, z, side, ev);
                joined_.push_back(Joined{w.event, z, partner, enemy, ev, false, false});
                relations_.credit(race_of(ctx, z), race_of(ctx, partner), tuning_.join_memory, ev);
                alliances_.at(ordered(z, partner)).fought_event = ev;  // 6.2 item 1
                ++stats_.joined;
                continue;
            }
            if (side != 0 || origin) {
                continue;  // an origin ally that stays out, and an attack call refused, write nothing
            }
            // 4.2: the motive, the smallest factor below 1000 among codes 1 to 7 (the smaller code at a tie).
            std::int64_t motive = 0;
            for (std::size_t k = 1; k < f.size(); ++k) {
                if (f[k] < 1000 && (motive == 0 || f[k] < f[static_cast<std::size_t>(motive)])) {
                    motive = static_cast<std::int64_t>(k);
                }
            }
            const bool excused = motive == 1 || motive == 2;
            Alliance& a = alliances_.at(ordered(z, partner));
            const std::int64_t count = excused ? 0 : a.refusals + 1;
            std::vector<std::int64_t> causes{w.event, renewal};
            if (motive == 1) {
                causes.push_back(war_end.second);
            } else if (motive == 6) {
                causes.push_back(conflict_.claim_event(z, partner));
            } else if (motive == 7) {
                causes.push_back(soured_share);
            }
            EventSpec e = point_event(ctx, "ally_refused", 0, 0);
            e.participants = {civ_role("ally", z), civ_role("partner", partner), civ_role("enemy", enemy)};
            e.values = {motive, count};
            e.causes = sorted_unique(std::move(causes));
            const std::int64_t ev = ctx.append(e);
            ++stats_.refused;
            if (excused) {
                relations_.credit(race_of(ctx, z), race_of(ctx, partner), -tuning_.excused_memory, ev);
                ++stats_.excused;
                continue;
            }
            relations_.credit(race_of(ctx, z), race_of(ctx, partner), -tuning_.refuse_memory, ev);
            conflict_.add_grudge_seam(z, partner, tuning_.refuse_grudge);
            Alliance& live = alliances_.at(ordered(z, partner));
            live.refusals = count;
            if (live.refusals >= tuning_.refusals_to_break) {
                const Pair pr = ordered(z, partner);
                const std::int64_t b = append_pair_event(ctx, "alliance_broken", pr, 0, {ev});
                close_alliance(ctx, pr, b);
                ++stats_.broken;
            }
        }
    }
}

// --- Keeping an alliance (6.7) ---------------------------------------------------------------------------

void AlliancesSubsystem::spurn(StepContext& ctx, const Pair& p, std::int64_t reason, std::int64_t ally,
                               std::int64_t share_event, std::int64_t share_year, std::int64_t region) {
    const Alliance a = alliances_.at(p);
    const std::int64_t other = ally == p.first ? p.second : p.first;
    EventSpec e = point_event(ctx, "alliance_spurned", region, 0);
    e.participants = {civ_role("ally", ally), civ_role("principal", other)};
    e.values = {reason, ctx.year() - share_year};
    e.causes = sorted_unique({share_event, a.renew_event});
    const std::int64_t ev = ctx.append(e);
    close_alliance(ctx, p, ev);
    ++stats_.spurned;
    stats_.spurned_at_once += reason == 2 ? 1 : 0;
}

// P5-04a 3.4: a spurning at once that waited for a quiet step's budget.
void AlliancesSubsystem::pending_spurns(StepContext& ctx) {
    std::vector<Pair> keys;
    for (const auto& [p, a] : alliances_) {
        if (a.pending_event != 0) {
            keys.push_back(p);
        }
    }
    for (const Pair& p : keys) {
        const auto it = alliances_.find(p);
        if (it == alliances_.end() || !quiet_ok(ctx, it->second.pending_ally)) {
            continue;
        }
        const Alliance a = it->second;
        spurn(ctx, p, 2, a.pending_ally, a.pending_event, a.pending_year, a.pending_region);
    }
}

// P5-04a 3.4: after a spoils_withheld `event` of principal to ally on an open
// alliance not of origin (its grievance already decided by the caller).
void AlliancesSubsystem::on_withheld(StepContext& ctx, std::int64_t principal, std::int64_t ally, std::int64_t event,
                                     std::int64_t region, std::int64_t year, bool large) {
    const Pair p = ordered(principal, ally);
    Alliance& a = alliances_.at(p);
    const bool at_once = large || checked_add(a.withheld, 1) >= tuning_.withheld_to_break;
    a.withheld = checked_add(a.withheld, 1);
    if (!at_once) {
        // Reading: the soured record is the first withholding's; the flags are set at every souring.
        if (a.soured_event == 0) {
            a.soured_event = event;
            a.soured_year = year;
            a.soured_region = region;
            a.soured_ally = ally;
        }
        a.renewal_due = true;
        a.call_due = true;
        ++stats_.soured;
        return;
    }
    if (!quiet_ok(ctx, ally)) {
        a.pending_event = event;
        a.pending_year = year;
        a.pending_region = region;
        a.pending_ally = ally;
        return;
    }
    spurn(ctx, p, 2, ally, event, year, region);
}

void AlliancesSubsystem::cooling(StepContext& ctx) {
    std::vector<Pair> keys;
    for (const auto& [p, a] : alliances_) {
        (void)a;
        keys.push_back(p);
    }
    for (const Pair& p : keys) {
        const auto it = alliances_.find(p);
        if (it == alliances_.end() || it->second.origin || it->second.pending_event != 0 ||
            d(ctx, p.first, p.second) >= tuning_.ally_break_disposition) {
            continue;
        }
        if (it->second.soured_event == 0) {
            if (!quiet_ok(ctx, p.first)) {
                continue;
            }
            const std::int64_t ev = append_pair_event(ctx, "alliance_broken", p, 1, {it->second.renew_event});
            close_alliance(ctx, p, ev);
            ++stats_.broken;
        } else {
            const Alliance a = it->second;
            if (!quiet_ok(ctx, a.soured_ally)) {
                continue;
            }
            spurn(ctx, p, 1, a.soured_ally, a.soured_event, a.soured_year, a.soured_region);
        }
    }
}

void AlliancesSubsystem::blocs(StepContext& ctx) {
    const std::int64_t nc = civ_count(ctx.state());
    for (std::int64_t c = 1; c <= nc; ++c) {
        while (bloc(c) > tuning_.bloc_lapse_share && !allies(c).empty()) {
            std::int64_t pick_z = 0;
            for (const std::int64_t z : allies(c)) {  // the smallest share, then the larger id
                if (pick_z == 0 || share_of(z) < share_of(pick_z) ||
                    (share_of(z) == share_of(pick_z) && z > pick_z)) {
                    pick_z = z;
                }
            }
            const Pair p = ordered(c, pick_z);
            if (!quiet_ok(ctx, p.first)) {
                break;
            }
            const std::int64_t ev = append_pair_event(ctx, "alliance_lapsed", p, 1, {alliances_.at(p).renew_event});
            close_alliance(ctx, p, ev);
            ++stats_.lapsed;
        }
    }
}

// P5-04a 6: the renewal chain, the soured renewal, notable renewals only.
void AlliancesSubsystem::renewals(StepContext& ctx) {
    const std::int64_t year = ctx.year();
    std::vector<Pair> keys;
    for (const auto& [p, a] : alliances_) {
        (void)a;
        keys.push_back(p);
    }
    for (const Pair& p : keys) {
        const auto it = alliances_.find(p);
        if (it == alliances_.end() || year < it->second.next_due || it->second.pending_event != 0) {
            continue;
        }
        Alliance& a = it->second;
        const std::int64_t due = a.next_due;
        if (a.origin) {
            a.next_due = checked_add(due, tuning_.term_years);
            continue;
        }
        if (a.renewal_due) {
            // 3.5: the soured renewal.
            if (!quiet_ok(ctx, a.soured_ally)) {
                continue;  // it waits
            }
            if (!chance_pm(ctx.draw(renew_key_, word(p.first), word(p.second), word(due)),
                           tuning_.soured_renew_permille)) {
                const Alliance copy = a;
                spurn(ctx, p, 0, copy.soured_ally, copy.soured_event, copy.soured_year, copy.soured_region);
                continue;
            }
            a.renewal_due = false;
        }
        const Reasons r = reasons(ctx, p.first, p.second);
        if (!r.any()) {
            if (!quiet_ok(ctx, p.first)) {
                continue;  // the lapse waits
            }
            const std::int64_t ev = append_pair_event(ctx, "alliance_lapsed", p, 0, {a.renew_event});
            close_alliance(ctx, p, ev);
            ++stats_.lapsed;
            continue;
        }
        const std::int64_t basis = r.enemy_event != 0 ? 1 : (r.fear_event != 0 ? 2 : (r.trade_event != 0 ? 4 : 3));
        const std::int64_t foe = basis == 1 ? r.enemy : (basis == 2 ? r.fear : 0);
        const std::int64_t basis_event =
            basis == 1 ? r.enemy_event : (basis == 2 ? r.fear_event : (basis == 4 ? r.trade_event : r.friend_event));
        // 6.2: notable since R (the formation or the last written renewal).
        std::vector<std::int64_t> notable;
        if (a.fought_event > a.renew_event) {
            notable.push_back(a.fought_event);
        }
        if (a.shared_event > a.renew_event) {
            notable.push_back(a.shared_event);
        }
        if (basis != a.basis) {
            notable.push_back(basis_event);
        }
        a.next_due = checked_add(due, tuning_.term_years);
        // P5-08 12.5: past the cap a notable renewal is made quietly (R unchanged).
        const bool capped = !notable.empty() && tuning_.renew_written_max > 0 &&
                            a.written_renewals >= tuning_.renew_written_max;
        if (capped) {
            relations_.credit_units(race_of(ctx, p.first), race_of(ctx, p.second), tuning_.renew_memory);
            ++stats_.renewed_quietly;
            ++stats_.renewals_capped;
            continue;
        }
        if (notable.empty() || !quiet_ok(ctx, p.first)) {
            // A renewal with nothing new (or without a quiet step's budget) writes nothing.
            // P5-07 7.5: no event of this step to cite; units only.
            relations_.credit_units(race_of(ctx, p.first), race_of(ctx, p.second), tuning_.renew_memory);
            ++stats_.renewed_quietly;
            continue;
        }
        EventSpec e = point_event(ctx, "alliance_renewed", 0, 0);
        e.participants = {civ_role("ally_a", p.first), civ_role("ally_b", p.second)};
        if (foe != 0) {
            e.participants.push_back(civ_role("foe", foe));
        }
        e.values = {basis};
        notable.push_back(a.renew_event);
        e.causes = sorted_unique(std::move(notable));
        const std::int64_t ev = ctx.append(e);
        Alliance& live = alliances_.at(p);
        live.renew_year = year;
        live.renew_event = ev;
        live.written_renewals += 1;
        live.basis = basis;
        live.foe = foe;
        relations_.credit(race_of(ctx, p.first), race_of(ctx, p.second), tuning_.renew_memory, ev);
        ++stats_.renewed;
    }
}

std::int64_t AlliancesSubsystem::formation_band(const StepContext& ctx, std::int64_t x, std::int64_t y) const {
    const std::int64_t rx = race_of(ctx, x);
    const std::int64_t ry = race_of(ctx, y);
    return rx == ry ? 2 : relations_.band(rx, ry);  // P5-04a 10 item 4: 2 for one race
}

// --- Origin alliances and formations (6.3, 6.4) -------------------------------------------------------

void AlliancesSubsystem::origin_alliances(StepContext& ctx) {
    std::vector<std::int64_t> civs;
    for (const auto& [c, p] : presence_) {
        (void)p;
        if (free_civ(ctx, c)) {
            civs.push_back(c);
        }
    }
    for (std::size_t i = 0; i < civs.size(); ++i) {
        for (std::size_t j = i + 1; j < civs.size(); ++j) {
            const std::int64_t x = civs[i];
            const std::int64_t y = civs[j];
            const RaceData& rx = race(race_of(ctx, x));
            const RaceData& ry = race(race_of(ctx, y));
            if (!std::binary_search(rx.origin_allies.begin(), rx.origin_allies.end(), ry.id) ||
                !std::binary_search(ry.origin_allies.begin(), ry.origin_allies.end(), rx.id) || allied(x, y) ||
                betrayed_.count(Pair{x, y}) != 0 || conflict_.at_war(x, y) ||
                static_cast<std::int64_t>(allies(x).size()) >= tuning_.allies_max ||
                static_cast<std::int64_t>(allies(y).size()) >= tuning_.allies_max || !bloc_test(x, y) ||
                closed_now_.count({Pair{x, y}, true}) != 0) {
                continue;
            }
            EventSpec e = point_event(ctx, "alliance_formed", 0, 0);
            e.participants = {civ_role("ally_a", x), civ_role("ally_b", y)};
            e.values = {0, tuning_.term_years, formation_band(ctx, x, y)};
            e.causes = {ctx.state().civ(y).founding_event};
            const std::int64_t ev = ctx.append(e);
            Alliance a;
            a.basis = 0;
            a.origin = true;
            open_alliance(ctx, x, y, ev, a);
        }
    }
}

void AlliancesSubsystem::formations(StepContext& ctx) {
    if (tuning_.ally_hops < 0) {
        return;
    }
    std::vector<std::int64_t> civs;
    for (const auto& [c, p] : presence_) {
        (void)p;
        if (free_civ(ctx, c)) {
            civs.push_back(c);
        }
    }
    for (std::size_t i = 0; i < civs.size(); ++i) {
        for (std::size_t j = i + 1; j < civs.size(); ++j) {
            const std::int64_t x = civs[i];
            const std::int64_t y = civs[j];
            if (allied(x, y) || betrayed_.count(Pair{x, y}) != 0 || closed_now_.count({Pair{x, y}, true}) != 0 ||
                static_cast<std::int64_t>(allies(x).size()) >= tuning_.allies_max ||
                static_cast<std::int64_t>(allies(y).size()) >= tuning_.allies_max ||
                !in_reach(x, y, tuning_.ally_hops) || lately_at_war(ctx, x, y, tuning_.ally_after_war_years)) {
                continue;
            }
            const std::int64_t dxy = d(ctx, x, y);
            if (dxy < tuning_.ally_min_disposition) {
                continue;
            }
            const Reasons r = reasons(ctx, x, y);
            const std::int64_t m = (r.enemy_event != 0 ? tuning_.form_foe_permille : 0) +
                                   (r.fear_event != 0 ? tuning_.form_fear_permille : 0) +
                                   (r.trade_event != 0 ? tuning_.form_trade_permille : 0) +
                                   (r.friend_event != 0 ? tuning_.form_friend_permille : 0);
            const std::int64_t p =
                std::min(tuning_.form_max_permille, floor_div(checked_mul(m, warmth(ctx, x, y)), 1000));
            if (p <= 0 || !bloc_test(x, y)) {
                continue;
            }
            if (!chance_pm(ctx.draw(form_key_, word(x), word(y), 0), p)) {
                continue;
            }
            const std::int64_t basis =
                r.enemy_event != 0 ? 1 : (r.fear_event != 0 ? 2 : (r.trade_event != 0 ? 4 : 3));
            EventSpec e = point_event(ctx, "alliance_formed", 0, 0);
            e.participants = {civ_role("ally_a", x), civ_role("ally_b", y)};
            const std::int64_t foe = basis == 1 ? r.enemy : (basis == 2 ? r.fear : 0);
            if (foe != 0) {
                e.participants.push_back(civ_role("foe", foe));
            }
            e.values = {basis, tuning_.term_years, formation_band(ctx, x, y)};
            // P5-04a 10 item 2: the cause of the basis reason only.
            const std::int64_t basis_event = basis == 1   ? r.enemy_event
                                             : basis == 2 ? r.fear_event
                                             : basis == 4 ? r.trade_event
                                                          : r.friend_event;
            e.causes = sorted_unique({basis_event});
            const std::int64_t ev = ctx.append(e);
            Alliance a;
            a.basis = basis;
            a.foe = foe;
            open_alliance(ctx, x, y, ev, a);
        }
    }
}

// --- Trade (6.8, 6.9) ---------------------------------------------------------------------------------------

void AlliancesSubsystem::trade_breaks(StepContext& ctx) {
    std::vector<Pair> keys;
    for (const auto& [p, t] : trades_) {
        (void)t;
        keys.push_back(p);
    }
    for (const Pair& p : keys) {
        const auto it = trades_.find(p);
        if (it == trades_.end()) {
            continue;
        }
        std::int64_t reason = -1;
        if (d(ctx, p.first, p.second) < tuning_.trade_min_disposition - tuning_.trade_break_margin) {
            reason = 1;
        } else if (!in_reach(p.first, p.second, tuning_.trade_hops + 1)) {
            reason = 2;
        }
        if (reason < 0 || !quiet_ok(ctx, p.first)) {
            continue;
        }
        const std::int64_t ev = append_pair_event(ctx, "trade_broken", p, reason, {it->second.opened_event});
        close_trade(ctx, p, ev);
    }
}

void AlliancesSubsystem::trade_openings(StepContext& ctx) {
    if (tuning_.trade_hops < 0) {
        return;
    }
    const bool quiet = ctx.step() == Step::q4;
    std::vector<std::int64_t> civs;
    for (const auto& [c, p] : presence_) {
        (void)p;
        if (free_civ(ctx, c)) {
            civs.push_back(c);
        }
    }
    for (std::size_t i = 0; i < civs.size(); ++i) {
        for (std::size_t j = i + 1; j < civs.size(); ++j) {
            const std::int64_t x = civs[i];
            const std::int64_t y = civs[j];
            if (trades_.count(Pair{x, y}) != 0 || closed_now_.count({Pair{x, y}, false}) != 0 ||
                trade_partners(x) >= tuning_.trade_max ||
                trade_partners(y) >= tuning_.trade_max || !in_reach(x, y, tuning_.trade_hops) ||
                lately_at_war(ctx, x, y, tuning_.trade_after_war_years)) {
                continue;
            }
            const std::int64_t dxy = d(ctx, x, y);
            if (dxy < tuning_.trade_min_disposition || !quiet_ok(ctx, x)) {
                continue;
            }
            const std::int64_t wt = clamp_i(floor_div(checked_mul(dxy - tuning_.trade_min_disposition, 1000),
                                                      tuning_.ally_full_disposition - tuning_.trade_min_disposition),
                                            0, 1000);
            std::int64_t p = floor_div(checked_mul(tuning_.trade_permille, wt), 1000);
            if (quiet) {
                p = std::min<std::int64_t>(1000, checked_mul(p, ctx.years()));
            }
            const bool sx = sellers_at_start_.count(x) != 0;
            const bool sy = sellers_at_start_.count(y) != 0;
            if (sx != sy) {
                p = std::min<std::int64_t>(1000, floor_div(checked_mul(p, tuning_.arms_trade_permille), 1000));
            }
            if (!chance_pm(ctx.draw(trade_key_, word(x), word(y), 0), p)) {
                continue;
            }
            std::int64_t cause = ctx.state().civ(y).founding_event;
            if (const auto lp = conflict_.last_peace(x, y)) {
                cause = std::max(cause, lp->second);
            }
            const std::int64_t rx = race_of(ctx, x);
            const std::int64_t ry = race_of(ctx, y);
            if (rx != ry) {
                cause = std::max(cause, relations_.met_event(rx, ry));
            }
            EventSpec e = point_event(ctx, "trade_opened", 0, 0);
            e.participants = {civ_role("partner_a", x), civ_role("partner_b", y)};
            e.causes = {cause};
            const std::int64_t ev = ctx.append(e);
            ctx.state().open_relation(trade_key(x, y), ev);
            trades_[Pair{x, y}] = Trade{ctx.year(), ev, checked_add(ctx.year(), tuning_.trade_credit_years)};
            ++stats_.trades;
        }
    }
}

void AlliancesSubsystem::trade_credits(StepContext& ctx) {
    const std::int64_t year = ctx.year();
    const bool quiet = ctx.step() == Step::q4;
    for (auto& [p, t] : trades_) {
        if (year < t.next_credit) {
            continue;
        }
        const std::int64_t k =
            quiet ? 1 + floor_div(year + ctx.years() - 1 - t.next_credit, tuning_.trade_credit_years) : 1;
        relations_.credit(race_of(ctx, p.first), race_of(ctx, p.second), checked_mul(tuning_.trade_memory, k),
                          t.opened_event);
        t.next_credit = checked_add(t.next_credit, checked_mul(k, tuning_.trade_credit_years));
    }
}

// --- Ends of joined wars and spoils (6.9, 6.11) ---------------------------------------------------------

void AlliancesSubsystem::joined_ends(StepContext& ctx) {
    for (Joined& j : joined_) {
        if (j.settled && j.credited) {
            continue;
        }
        if (!j.settled && !conflict_.war_open(j.war)) {
            spoils(ctx, j);
        }
        if (!j.credited && (!conflict_.war_open(j.war) || conflict_.ally_left(j.war, j.ally))) {
            const ConflictAlly rec = conflict_.ally_record(j.war, j.ally);
            const std::int64_t ev = rec.close_event != 0 ? rec.close_event : conflict_.war_end_event(j.war);
            if (ev != 0) {
                relations_.credit(race_of(ctx, j.ally), race_of(ctx, j.enemy), -tuning_.ally_war_memory, ev);
            }
            j.credited = true;
        }
    }
}

void AlliancesSubsystem::spoils(StepContext& ctx, Joined& j) {
    const State& state = ctx.state();
    const WarSpoils s = conflict_.war_spoils(j.war);
    const ConflictAlly rec = conflict_.ally_record(j.war, j.ally);
    const std::int64_t v = checked_add(s.tribute_people,
                                       floor_div(checked_mul(s.plunder, tuning_.spoils_plunder_permille), 1000));
    const std::string_view end_kind = s.end_event != 0 ? ctx.event_kind(s.end_event) : std::string_view{};
    // Spec 13 item 11: the end event's own winner is the principal; a
    // conquest of the enemy by a third realm owes nothing.
    // P5-04a 11 item 1: a conquest owes only when the principal conquered the enemy itself.
    const bool cited = end_kind == "peace" || end_kind == "war_won" ||
                       (end_kind == "civ_conquered" && ctx.event_participant(s.end_event, "conqueror") == s.winner &&
                        ctx.event_participant(s.end_event, "civilization") == j.enemy);
    if (s.winner == 0 || s.winner != j.partner || (rec.closed_by != 1 && rec.closed_by != 3) || state.civ(j.ally).ended_year ||
        state.civ(s.winner).ended_year || rec.dead <= 0 || v <= 0 || !cited) {
        j.settled = true;
        return;
    }
    const std::int64_t part = clamp_i(floor_div(checked_mul(rec.dead, 1000), std::max<std::int64_t>(1, s.side_dead)),
                                      tuning_.spoils_floor_permille, tuning_.spoils_cap_permille);
    const std::int64_t given = spoils_given_[j.war];
    const std::int64_t share = std::min(floor_div(checked_mul(v, part), 1000), v - given);
    if (share <= 0) {
        j.settled = true;
        return;
    }
    if (!quiet_ok(ctx, s.winner)) {
        return;  // waits for the next T6 or Q4
    }
    const std::int64_t base = floor_div(checked_mul(s.winner_pop0, race(race_of(ctx, s.winner)).muster), 1000);
    const std::int64_t denom = std::max<std::int64_t>(
        1, floor_div(checked_mul(conflict_.tuning().exhaustion_permille, std::max<std::int64_t>(1, base)), 1000));
    const std::int64_t ex = std::min<std::int64_t>(1000, floor_div(checked_mul(s.winner_dead, 1000), denom));
    std::int64_t g = tuning_.spoils_grant_permille;
    g = checked_add(g, floor_div(checked_mul(tuning_.spoils_warmth_permille, warmth(ctx, s.winner, j.ally)), 1000));
    g = checked_add(g, floor_div(checked_mul(tuning_.spoils_part_permille, part), 1000));
    g = checked_sub(g, floor_div(checked_mul(tuning_.spoils_need_permille, ex), 1000));
    if (figures_ != nullptr) {  // P5-06 7.3: the principal's temper, inside the clamp
        g = checked_add(g, figures_->spoils_permille(s.winner));
    }
    g = clamp_i(g, tuning_.spoils_grant_min, tuning_.spoils_grant_max);
    const bool granted = chance_pm(ctx.draw(spoils_key_, word(j.war), word(j.ally), 0), g);
    // P5-04a 3.3, 3.4: the grievance of a withheld share on an open alliance not of origin.
    const auto open = alliances_.find(ordered(s.winner, j.ally));
    const bool on_alliance = open != alliances_.end() && !open->second.origin;
    const bool large = share >= tuning_.spoils_large_share || part >= tuning_.spoils_large_part_permille;
    std::int64_t grievance = 0;
    if (!granted && on_alliance) {
        grievance = large || checked_add(open->second.withheld, 1) >= tuning_.withheld_to_break ? 2 : 1;
    }
    EventSpec e = point_event(ctx, granted ? "spoils_shared" : "spoils_withheld", s.goal_region, 0);
    e.participants = {civ_role("principal", s.winner), civ_role("ally", j.ally), civ_role("enemy", s.loser)};
    e.values = {share};
    if (!granted) {
        e.values.push_back(grievance);
    }
    e.causes = sorted_unique({s.end_event, j.event});
    const std::int64_t ev = ctx.append(e);
    spoils_given_[j.war] = checked_add(given, share);
    ++stats_.owed;
    if (granted) {
        relations_.credit(race_of(ctx, s.winner), race_of(ctx, j.ally), tuning_.share_memory, ev);
        if (const auto it = alliances_.find(ordered(s.winner, j.ally)); it != alliances_.end()) {
            it->second.shared_event = ev;  // 6.2 item 2
        }
        ++stats_.granted;
    } else {
        relations_.credit(race_of(ctx, s.winner), race_of(ctx, j.ally), -tuning_.withheld_memory, ev);
        conflict_.add_grudge_seam(s.winner, j.ally, tuning_.withheld_grudge);
        ++stats_.withheld;
        if (on_alliance) {
            on_withheld(ctx, s.winner, j.ally, ev, s.goal_region, ctx.year(), large);
        }
    }
    j.settled = true;
}

// --- Mines and arms (6.12, 6.13) --------------------------------------------------------------------------

void AlliancesSubsystem::mine_closings(StepContext& ctx) {
    if (sites_ == nullptr) {
        return;
    }
    const State& state = ctx.state();
    // Open mines ascending by site id.
    std::vector<std::pair<std::int64_t, Pair>> order;
    for (const auto& [key, m] : mines_) {
        order.emplace_back(m.site, key);
    }
    std::stable_sort(order.begin(), order.end());
    for (const auto& [site, key] : order) {
        const Mine m = mines_.at(key);
        const std::int64_t c = key.first;
        const std::int64_t r = key.second;
        const Settlement t = state.settlement(m.settlement);
        std::int64_t ending = 0;
        std::int64_t ev = 0;
        if (state.civ(c).ended_year) {
            ending = 5;
            ev = state.civ(c).ending_event;
        } else if (!standing(t)) {
            ending = 4;
            ev = t.ruin_event;
        } else if (t.owner_civ != c) {
            ending = 4;
            ev = t.owner_event;
        } else if (state.region(r).owner != c) {
            ending = 4;
            ev = state.region(r).owner_event;
        }
        if (ending == 0) {
            continue;
        }
        // Reading: the ended year is the ending event's year (Y in every
        // case 6.12 names; a town lost after this step's Q4 closes the mine
        // at the next step with the event's own year, as the reader checks).
        sites_->end_site_seam(site, ctx.event_year(ev), ev, ending);
        mines_.erase(key);
    }
}

void AlliancesSubsystem::mine_openings(StepContext& ctx) {
    if (sites_ == nullptr || tuning_.mine_permille <= 0) {
        return;
    }
    const State& state = ctx.state();
    const bool quiet = ctx.step() == Step::q4;
    const std::int64_t nc = civ_count(state);
    const auto nr = static_cast<std::int64_t>(ctx.geo().regions().size());
    const std::int64_t ns = settlement_count(state);
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (!free_civ(ctx, c)) {
            continue;
        }
        const std::int64_t mining = race(race_of(ctx, c)).mining;
        if (mining <= 0) {
            continue;
        }
        for (std::int64_t r = 1; r <= nr; ++r) {
            if (state.region(r).owner != c || ore_reach_[static_cast<std::size_t>(r - 1)] < tuning_.mine_ore_min ||
                mines_.count(Pair{c, r}) != 0) {
                continue;
            }
            std::int64_t t = 0;
            for (std::int64_t id = 1; id <= ns; ++id) {
                const Settlement& s = state.settlement(id);
                if (s.owner_civ != c || s.region != r || !standing(s) || mined_.count(id) != 0) {  // A12: hidden too
                    continue;
                }
                if (t == 0 || s.population > state.settlement(t).population) {
                    t = id;
                }
            }
            if (t == 0 || state.settlement(t).population < tuning_.mine_settlement_min || !quiet_ok(ctx, c)) {
                continue;
            }
            std::int64_t p = floor_div(checked_mul(tuning_.mine_permille, mining), 1000);
            if (quiet) {
                p = std::min<std::int64_t>(1000, checked_mul(p, ctx.years()));
            }
            if (!chance_pm(ctx.draw(mine_key_, word(c), word(r), 0), p)) {
                continue;
            }
            const std::int64_t id = sites_->next_site_id();
            const Settlement town = state.settlement(t);
            EventSpec e = point_event(ctx, "mine_opened", r, t);
            e.participants = {civ_role("civilization", c), Participant{"site", EntityType::site, id}};
            e.causes = {std::max(state.region(r).owner_event, town.founding_event)};
            const std::int64_t ev = ctx.append(e);
            const std::string& form = tuning_.mine_forms[static_cast<std::size_t>((id - 1) %
                                                                                  static_cast<std::int64_t>(tuning_.mine_forms.size()))];
            std::string place = town.name;
            std::string name = form;
            const std::size_t at = name.find("{settlement}");
            if (at != 0 && place.rfind("The ", 0) == 0) {  // mid-sentence (P1-06 r3 14.7)
                place[0] = 't';
            }
            if (at != std::string::npos) {
                name.replace(at, std::string_view("{settlement}").size(), place);
            }
            MonstersSubsystem::SiteSpec spec;
            spec.id = id;
            spec.kind = 3;
            spec.name = std::move(name);
            spec.region = r;
            spec.x = town.x;
            spec.y = town.y;
            spec.settlement = t;
            spec.founder_civ = c;
            spec.monster = 0;
            spec.origin_year = ctx.year();
            spec.origin_event = ev;
            sites_->open_site(spec);
            mines_[Pair{c, r}] = Mine{id, t, ev};
            mined_.insert(t);
            ++stats_.mines;
        }
    }
}

void AlliancesSubsystem::arms_sales(StepContext& ctx) {
    std::vector<Pair> keys;
    for (const auto& [p, t] : trades_) {
        (void)t;
        keys.push_back(p);
    }
    for (const Pair& p : keys) {
        const bool sx = is_seller(p.first);
        const bool sy = is_seller(p.second);
        if (sx == sy) {
            continue;
        }
        const std::int64_t s = sx ? p.first : p.second;
        const std::int64_t b = sx ? p.second : p.first;
        if (arms_sold_.count(Pair{s, b}) != 0 || !quiet_ok(ctx, s)) {
            continue;
        }
        // The forge: the seller's smallest-id open mine.
        Mine forge;
        for (auto it = mines_.lower_bound(Pair{s, 0}); it != mines_.end() && it->first.first == s; ++it) {
            if (forge.site == 0 || it->second.site < forge.site) {
                forge = it->second;
            }
        }
        EventSpec e = point_event(ctx, "arms_sold", 0, 0);
        e.participants = {civ_role("seller", s), civ_role("buyer", b), Participant{"site", EntityType::site, forge.site}};
        e.causes = sorted_unique({trades_.at(p).opened_event, forge.event});
        const std::int64_t ev = ctx.append(e);
        arms_sold_.emplace(Pair{s, b}, ev);
        relations_.credit(race_of(ctx, s), race_of(ctx, b), tuning_.trade_memory, ev);
        ++stats_.arms;
    }
}

}  // namespace worldgen::history
