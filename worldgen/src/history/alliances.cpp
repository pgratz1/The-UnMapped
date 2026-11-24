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

bool AlliancesSubsystem::is_seller(std::int64_t c) const {
    const auto it = mines_.lower_bound(Pair{c, 0});
    return it != mines_.end() && it->first.first == c;
}

bool AlliancesSubsystem::forged(std::int64_t c) const {
    return is_seller(c);
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
        if (out.fear_event == 0) {
            out.fear = 0;
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
    a.formed_year = ctx.year();
    a.formed_event = event;
    a.renew_year = ctx.year();
    a.renew_event = event;
    alliances_[ordered(x, y)] = a;
    rebuild_allies();
    relations_.credit(race_of(ctx, x), race_of(ctx, y), tuning_.ally_memory, event);
    ++stats_.formed;
    stats_.origin += a.origin ? 1 : 0;
}

void AlliancesSubsystem::close_alliance(StepContext& ctx, const Pair& p, std::int64_t event) {
    ctx.state().close_relation(alliance_key(p.first, p.second), event);
    alliances_.erase(p);
    rebuild_allies();
}

void AlliancesSubsystem::close_trade(StepContext& ctx, const Pair& p, std::int64_t event) {
    ctx.state().close_relation(trade_key(p.first, p.second), event);
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
    drop_closed(ctx);
    switch (ctx.step()) {
        case Step::t2:
            build_cache(ctx);
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
    EventSpec e = point_event(ctx, "alliance_betrayed", 0, 0);
    e.participants = {civ_role("betrayer", w.attacker), civ_role("betrayed", w.defender)};
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
            std::int64_t p = 0;
            if (side == 0) {
                const std::int64_t sigma = std::min<std::int64_t>(
                    1000, floor_div(checked_mul(checked_add(conflict_.force(ctx, z), conflict_.force(ctx, partner)), 1000),
                                    std::max<std::int64_t>(1, conflict_.force(ctx, enemy))));
                p = floor_div(checked_mul(tuning_.defend_permille, join), 1000);
                p = floor_div(checked_mul(p, warmth(ctx, z, partner)), 1000);
                p = floor_div(checked_mul(p, sigma), 1000);
            } else {
                const std::int64_t h = std::min<std::int64_t>(1000, floor_div(checked_mul(-dz, 1000), tuning_.hostility_full));
                p = floor_div(checked_mul(tuning_.attack_permille, join), 1000);
                p = floor_div(checked_mul(p, h), 1000);
            }
            const std::int64_t renewal = al->second.renew_event;
            const bool origin = al->second.origin;
            if (chance_pm(ctx.draw(join_key_, word(w.event), word(z), 0), p)) {
                EventSpec e = point_event(ctx, "ally_joined_war", 0, 0);
                e.participants = {civ_role("ally", z), civ_role("partner", partner), civ_role("enemy", enemy)};
                e.values = {side};
                e.causes = sorted_unique({w.event, renewal});
                const std::int64_t ev = ctx.append(e);
                conflict_.join_war(ctx, w.event, z, side, ev);
                joined_.push_back(Joined{w.event, z, partner, enemy, ev, false, false});
                relations_.credit(race_of(ctx, z), race_of(ctx, partner), tuning_.join_memory, ev);
                ++stats_.joined;
                continue;
            }
            if (side != 0 || origin) {
                continue;  // an origin ally that stays out, and an attack call refused, write nothing
            }
            EventSpec e = point_event(ctx, "ally_refused", 0, 0);
            e.participants = {civ_role("ally", z), civ_role("partner", partner), civ_role("enemy", enemy)};
            e.causes = sorted_unique({w.event, renewal});
            const std::int64_t ev = ctx.append(e);
            ++stats_.refused;
            relations_.credit(race_of(ctx, z), race_of(ctx, partner), -tuning_.refuse_memory, ev);
            Alliance& a = alliances_.at(ordered(z, partner));
            a.refusals += 1;
            if (a.refusals >= tuning_.refusals_to_break) {
                const Pair pr = ordered(z, partner);
                const std::int64_t b = append_pair_event(ctx, "alliance_broken", pr, 0, {ev});
                close_alliance(ctx, pr, b);
                ++stats_.broken;
            }
        }
    }
}

// --- Keeping an alliance (6.7) ---------------------------------------------------------------------------

void AlliancesSubsystem::spurn(StepContext& ctx, const Pair& p, std::int64_t reason) {
    const Alliance a = alliances_.at(p);
    const std::int64_t wronged = a.withheld_ally;
    const std::int64_t other = wronged == p.first ? p.second : p.first;
    EventSpec e = point_event(ctx, "alliance_spurned", a.withheld_region, 0);
    e.participants = {civ_role("ally", wronged), civ_role("principal", other)};
    e.values = {reason};
    e.causes = sorted_unique({a.withheld_event, a.renew_event});
    const std::int64_t ev = ctx.append(e);
    close_alliance(ctx, p, ev);
    ++stats_.spurned;
}

void AlliancesSubsystem::cooling(StepContext& ctx) {
    std::vector<Pair> keys;
    for (const auto& [p, a] : alliances_) {
        (void)a;
        keys.push_back(p);
    }
    for (const Pair& p : keys) {
        const auto it = alliances_.find(p);
        if (it == alliances_.end() || it->second.origin ||
            d(ctx, p.first, p.second) >= tuning_.ally_break_disposition) {
            continue;
        }
        if (it->second.withheld == 0) {
            if (!quiet_ok(ctx, p.first)) {
                continue;
            }
            const std::int64_t ev = append_pair_event(ctx, "alliance_broken", p, 1, {it->second.renew_event});
            close_alliance(ctx, p, ev);
            ++stats_.broken;
        } else {
            if (!quiet_ok(ctx, it->second.withheld_ally)) {
                continue;
            }
            spurn(ctx, p, 1);
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

void AlliancesSubsystem::renewals(StepContext& ctx) {
    const std::int64_t year = ctx.year();
    const std::int64_t last = ctx.step() == Step::q4 ? year + ctx.years() - 1 : year;
    std::vector<Pair> keys;
    for (const auto& [p, a] : alliances_) {
        (void)a;
        keys.push_back(p);
    }
    for (const Pair& p : keys) {
        const auto it = alliances_.find(p);
        if (it == alliances_.end() || last < checked_add(it->second.renew_year, tuning_.term_years)) {
            continue;
        }
        Alliance& a = it->second;
        if (a.origin) {
            a.renew_year = year;
            a.withheld = 0;
            continue;
        }
        if (a.withheld >= tuning_.withheld_to_break) {
            if (quiet_ok(ctx, a.withheld_ally)) {
                spurn(ctx, p, 0);
            }
            continue;
        }
        if (!quiet_ok(ctx, p.first)) {
            continue;
        }
        const Reasons r = reasons(ctx, p.first, p.second);
        if (!r.any()) {
            const std::int64_t ev = append_pair_event(ctx, "alliance_lapsed", p, 0, {a.renew_event});
            close_alliance(ctx, p, ev);
            ++stats_.lapsed;
            continue;
        }
        const std::int64_t basis = r.enemy_event != 0 ? 1 : (r.fear_event != 0 ? 2 : (r.trade_event != 0 ? 4 : 3));
        EventSpec e = point_event(ctx, "alliance_renewed", 0, 0);
        e.participants = {civ_role("ally_a", p.first), civ_role("ally_b", p.second)};
        if (basis == 1 || basis == 2) {
            e.participants.push_back(civ_role("foe", basis == 1 ? r.enemy : r.fear));
        }
        e.values = {basis};
        e.causes = sorted_unique({r.enemy_event, r.fear_event, r.trade_event, r.friend_event, a.renew_event});
        const std::int64_t ev = ctx.append(e);
        Alliance& live = alliances_.at(p);
        live.renew_year = year;
        live.renew_event = ev;
        live.withheld = 0;
        relations_.credit(race_of(ctx, p.first), race_of(ctx, p.second), tuning_.renew_memory, ev);
        ++stats_.renewed;
    }
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
                static_cast<std::int64_t>(allies(y).size()) >= tuning_.allies_max || !bloc_test(x, y)) {
                continue;
            }
            EventSpec e = point_event(ctx, "alliance_formed", 0, 0);
            e.participants = {civ_role("ally_a", x), civ_role("ally_b", y)};
            e.values = {0};
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
            if (allied(x, y) || betrayed_.count(Pair{x, y}) != 0 ||
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
            e.values = {basis};
            e.causes = sorted_unique({r.enemy_event, r.fear_event, r.trade_event, r.friend_event});
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
            if (trades_.count(Pair{x, y}) != 0 || trade_partners(x) >= tuning_.trade_max ||
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
    const bool cited = end_kind == "peace" || end_kind == "war_won" || end_kind == "civ_conquered";
    if (s.winner == 0 || s.winner != j.partner || rec.closed_by != 1 || state.civ(j.ally).ended_year ||
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
    g = clamp_i(g, tuning_.spoils_grant_min, tuning_.spoils_grant_max);
    const bool granted = chance_pm(ctx.draw(spoils_key_, word(j.war), word(j.ally), 0), g);
    EventSpec e = point_event(ctx, granted ? "spoils_shared" : "spoils_withheld", s.goal_region, 0);
    e.participants = {civ_role("principal", s.winner), civ_role("ally", j.ally), civ_role("enemy", s.loser)};
    e.values = {share};
    e.causes = sorted_unique({s.end_event, j.event});
    const std::int64_t ev = ctx.append(e);
    spoils_given_[j.war] = checked_add(given, share);
    ++stats_.owed;
    if (granted) {
        relations_.credit(race_of(ctx, s.winner), race_of(ctx, j.ally), tuning_.share_memory, ev);
        ++stats_.granted;
    } else {
        relations_.credit(race_of(ctx, s.winner), race_of(ctx, j.ally), -tuning_.withheld_memory, ev);
        conflict_.add_grudge_seam(s.winner, j.ally, tuning_.withheld_grudge);
        if (const auto it = alliances_.find(ordered(s.winner, j.ally)); it != alliances_.end()) {
            it->second.withheld += 1;
            it->second.withheld_event = ev;
            it->second.withheld_region = s.goal_region;
            it->second.withheld_ally = j.ally;
        }
        ++stats_.withheld;
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
        sites_->end_site_seam(site, ctx.year(), ev, ending);
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
                if (s.owner_civ != c || s.region != r || !standing(s) || !ctx.visible(s) || mined_.count(id) != 0) {
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
            if (place.rfind("The ", 0) == 0) {
                place[0] = 't';
            }
            std::string name = form;
            const std::size_t at = name.find("{settlement}");
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
        arms_sold_.insert(Pair{s, b});
        relations_.credit(race_of(ctx, s), race_of(ctx, b), tuning_.trade_memory, ev);
        ++stats_.arms;
    }
}

}  // namespace worldgen::history
