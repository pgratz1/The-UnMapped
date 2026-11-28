// The cults of the pantheon (spec docs/specs/p5-08-pantheon-cults.md 6.7 to
// 6.10): founding, spread, faith, suppression, hiding and endings, with
// their temples and sanctums.
//
// The open cult_in and faith_of rows are mirrored in the subsystem's own
// maps, so no loop runs over State::relations(); keys are copied before a
// relation is closed (6.12 item 1).

#include <algorithm>
#include <array>
#include <deque>
#include <string>
#include <utility>

#include "conflict_internal.hpp"
#include "pantheon_internal.hpp"
#include "worldgen/core/checked.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/figures.hpp"
#include "worldgen/history/monsters.hpp"
#include "worldgen/history/pantheon.hpp"

namespace worldgen::history {

using namespace conflict_detail;
using namespace pantheon_detail;

namespace {

constexpr std::int64_t FACTION_TYPE = static_cast<std::int64_t>(EntityType::faction);
constexpr std::int64_t CIV_TYPE = static_cast<std::int64_t>(EntityType::civilization);

RelationKey cult_key(std::int64_t k, std::int64_t c, std::string_view kind) {
    return RelationKey{FACTION_TYPE, k, CIV_TYPE, c, std::string(kind)};
}

Participant faction_role(std::int64_t k) {
    return Participant{"faction", EntityType::faction, k};
}

Participant god_role(std::int64_t g) {
    return Participant{"god", EntityType::god, g};
}

bool quiet(const StepContext& ctx) noexcept {
    return ctx.step() == Step::q4;
}

// Q4: min(1000, p x L).
std::int64_t over_step(const StepContext& ctx, std::int64_t p) {
    return quiet(ctx) ? std::min<std::int64_t>(1000, checked_mul(p, ctx.years())) : p;
}

}  // namespace

bool PantheonSubsystem::has_open_temple(std::int64_t settlement) const {
    return std::any_of(temples_.begin(), temples_.end(),
                       [settlement](const Temple& t) { return t.open && t.settlement == settlement; });
}

bool PantheonSubsystem::cult_in(std::int64_t k, std::int64_t c) const {
    return cult_in_.count({k, c}) != 0;
}

std::int64_t PantheonSubsystem::cult_realms(std::int64_t k) const {
    std::int64_t n = 0;
    for (auto it = cult_in_.lower_bound({k, 0}); it != cult_in_.end() && it->first.first == k; ++it) {
        ++n;
    }
    return n;
}

std::vector<std::int64_t> PantheonSubsystem::regions_by_hops(const StepContext& ctx, std::int64_t from,
                                                             std::int64_t hops) const {
    const GeoView& geo = ctx.geo();
    const auto nr = static_cast<std::int64_t>(geo.regions().size());
    std::vector<std::int64_t> d(static_cast<std::size_t>(nr), -1);
    if (from < 1 || from > nr) {
        return {};
    }
    std::deque<std::int64_t> queue{from};
    d[static_cast<std::size_t>(from - 1)] = 0;
    while (!queue.empty()) {
        const std::int64_t r = queue.front();
        queue.pop_front();
        const std::int64_t dr = d[static_cast<std::size_t>(r - 1)];
        if (dr >= hops) {
            continue;
        }
        for (const GeoView::Neighbour& nb : geo.region(r).neighbours) {
            std::int64_t& dn = d[static_cast<std::size_t>(nb.region - 1)];
            if (dn < 0) {
                dn = dr + 1;
                queue.push_back(nb.region);
            }
        }
    }
    std::vector<std::int64_t> out;
    for (std::int64_t h = 0; h <= hops; ++h) {
        for (std::int64_t r = 1; r <= nr; ++r) {
            if (d[static_cast<std::size_t>(r - 1)] == h) {
                out.push_back(r);
            }
        }
    }
    return out;
}

void PantheonSubsystem::end_temple(StepContext& ctx, Temple& t, std::int64_t ending, std::int64_t event) {
    sites_->end_site_seam(t.site, ctx.event_year(event), event, ending);
    t.open = false;
}

void PantheonSubsystem::end_cult(StepContext& ctx, std::int64_t k, std::int64_t ending, std::int64_t event) {
    Cult& cult = cults_[static_cast<std::size_t>(k - 1)];
    cult.status = 3;
    cult.ending = ending;
    cult.ended_year = ctx.event_year(event);
    cult.ending_event = event;
    if (god(cult.god).live_cult == k) {
        god(cult.god).live_cult = 0;
    }
    for (Temple& t : temples_) {
        if (t.open && t.cult == k) {
            end_temple(ctx, t, 7, event);
        }
    }
    stats_.ended += 1;
}

// --- 6.10 ------------------------------------------------------------------------------------------------------

void PantheonSubsystem::endings(StepContext& ctx) {
    const std::vector<std::pair<std::int64_t, std::int64_t>> ended = ended_realms_;
    for (const auto& [c, e] : ended) {
        std::vector<std::int64_t> cults;
        for (const auto& [key, ev] : cult_in_) {
            (void)ev;
            if (key.second == c) {
                cults.push_back(key.first);
            }
        }
        for (const std::int64_t k : cults) {
            ctx.state().close_relation(cult_key(k, c, "cult_in"), e);
            cult_in_.erase({k, c});
            latest_.erase({k, c});
        }
        if (const auto f = faith_of_.find(c); f != faith_of_.end()) {
            const std::int64_t k = f->second.first;
            faith_of_.erase(f);
            faith_since_.erase(c);
            ctx.state().close_relation(cult_key(k, c, "faith_of"), e);
        }
        for (const std::int64_t k : cults) {
            if (cults_[static_cast<std::size_t>(k - 1)].status == 1 && cult_realms(k) == 0) {
                end_cult(ctx, k, 2, e);
            }
        }
    }
    ended_realms_.clear();
}

void PantheonSubsystem::temples(StepContext& ctx) {
    if (sites_ == nullptr) {
        return;
    }
    const State& state = ctx.state();
    for (Temple& t : temples_) {
        if (!t.open) {
            continue;
        }
        const Settlement& s = state.settlement(t.settlement);
        std::int64_t ev = 0;
        if (!standing(s)) {
            ev = s.ruin_event;
        } else if (s.owner_civ != t.civ && !cult_in(t.cult, s.owner_civ)) {
            ev = s.owner_event;
        }
        if (ev == 0 || ev < t.origin_event) {
            continue;  // P5-04a 21.4 r3: an ending event older than the temple waits
        }
        end_temple(ctx, t, 4, ev);
    }
}

// --- 6.7 -------------------------------------------------------------------------------------------------------

void PantheonSubsystem::foundings(StepContext& ctx) {
    if (pending_acts_.empty()) {
        return;
    }
    const std::vector<PendingAct> acts = pending_acts_;
    std::vector<PendingAct> keep;
    const std::int64_t y = ctx.year();
    for (const PendingAct& a : acts) {
        const std::int64_t c = a.civ;
        const std::int64_t g = a.god;
        if (sites_ == nullptr || figures_ == nullptr || !alive_visible(ctx, c) || god(g).live_cult != 0) {
            continue;  // removed
        }
        if (quiet(ctx) && ctx.quiet_events_left(c) <= 0) {
            keep.push_back(a);  // it waits
            continue;
        }
        const std::int64_t factor = a.favourable ? 1000 : tuning_.cult_fear_permille;
        const std::int64_t p = floor_div(checked_mul(tuning_.cult_found_permille, factor), 1000);
        if (!chance_pm(ctx.draw(keys_[CULT_FOUND], word(a.event), 0, 0), p)) {
            continue;
        }
        const State& state = ctx.state();
        const auto seat_ok = [&](std::int64_t u) {
            if (u == 0) {
                return false;
            }
            const Settlement& s = state.settlement(u);
            return standing(s) && ctx.visible(s) && s.owner_civ == c;
        };
        std::int64_t t = 0;
        if (a.kind == MARK && seat_ok(a.settlement)) {
            t = a.settlement;
        } else if (seat_ok(state.civ(c).capital)) {
            t = state.civ(c).capital;
        }
        if (t != 0 && has_open_temple(t)) {
            t = 0;
        }
        if (t == 0) {
            const std::int64_t ns = settlement_count(state);
            for (std::int64_t u = 1; u <= ns; ++u) {
                if (seat_ok(u) && !has_open_temple(u) &&
                    (t == 0 || state.settlement(u).population > state.settlement(t).population)) {
                    t = u;
                }
            }
        }
        if (t == 0) {
            continue;  // no founding
        }
        std::int64_t f = 0;
        bool prophet = false;
        if (a.kind == CHAMPION && a.figure != 0 && figures_->champion_of(c) == a.figure) {
            f = a.figure;
        } else if (const std::int64_t caster = figures_->caster_of(c); caster != 0) {
            f = caster;
        } else if (const std::int64_t ruler = figures_->ruler_figure(c); ruler != 0) {
            f = ruler;
        } else {
            f = figures_->next_figure_id();
            prophet = true;
        }
        const auto k = static_cast<std::int64_t>(cults_.size()) + 1;
        if (k > MAX_FACTIONS) {
            fail("history: bound: factions exceed 100000");
        }
        const std::int64_t culture = state.civ(c).culture;
        std::string name = place_name(ctx, culture, "cult_forms", keys_[CULT_NAME], k, g);
        const std::int64_t s = sites_->next_site_id();
        std::string temple = place_name(ctx, culture, "temple_forms", keys_[SITE_NAME], s, g);
        const Settlement& seat = state.settlement(t);
        EventSpec e = point_event(ctx, "cult_founded", seat.region, t);
        e.participants = {faction_role(k), god_role(g), civ_role("civilization", c),
                          Participant{"founder", EntityType::figure, f}, Participant{"site", EntityType::site, s}};
        e.values = {k % 3};
        e.causes = {a.event};
        const std::int64_t ev = ctx.append(e);
        if (prophet) {
            figures_->raise_divine(ctx, f, 7, c, ev, "");
            stats_.prophets += 1;
        }
        MonstersSubsystem::SiteSpec spec;
        spec.id = s;
        spec.kind = 6;
        spec.name = std::move(temple);
        spec.region = seat.region;
        spec.x = seat.x;
        spec.y = seat.y;
        spec.settlement = t;
        spec.founder_civ = c;
        spec.origin_year = y;
        spec.origin_event = ev;
        sites_->open_site(spec);
        temples_.push_back(Temple{s, k, t, c, ev, true});
        Cult cult;
        cult.id = k;
        cult.name = std::move(name);
        cult.god = g;
        cult.civ = c;
        cult.founder = f;
        cult.seat = s;
        cult.seat_settlement = t;
        cult.founded_year = y;
        cult.founding_event = ev;
        cults_.push_back(std::move(cult));
        god(g).live_cult = k;
        cult_in_[{k, c}] = ev;
        latest_[{k, c}] = ev;
        ctx.state().open_relation(cult_key(k, c, "cult_in"), ev);
        stats_.cults_founded += 1;
    }
    pending_acts_ = std::move(keep);
}

// --- 6.8 -------------------------------------------------------------------------------------------------------

void PantheonSubsystem::spreads(StepContext& ctx) {
    if (alliances_ == nullptr || cults_.empty()) {
        return;
    }
    const std::int64_t nk = settled_cults_;  // E4: not the cults founded in this step
    const std::int64_t nc = civ_count(ctx.state());
    for (std::int64_t k = 1; k <= nk; ++k) {
        if (cults_[static_cast<std::size_t>(k - 1)].status != 1) {
            continue;
        }
        const std::int64_t g = cults_[static_cast<std::size_t>(k - 1)].god;
        for (std::int64_t a = 1; a <= nc; ++a) {
            if (!alive_visible(ctx, a) || cult_in(k, a)) {
                continue;
            }
            std::int64_t from = 0;
            std::int64_t link = 0;
            for (const auto& [c, ev] : alliances_->partners(a)) {  // ascending c
                if (cult_in(k, c)) {
                    from = c;
                    link = ev;
                    break;
                }
            }
            if (from == 0) {
                continue;
            }
            // P5-11 12.2: no spread back into a realm that put the cult down
            // within cult_return_years.
            if (const auto bar = suppressed_.find({k, a});
                bar != suppressed_.end() && bar->second > ctx.year() - tuning_.cult_return_years) {
                continue;
            }
            if (quiet(ctx) && ctx.quiet_events_left(a) <= 0) {
                continue;  // no draw
            }
            const std::int64_t camp_g = god(g).camp;
            const std::int64_t camp_a = civ_camp(ctx, a);
            const std::size_t aff = camp_g == camp_a ? 0 : (opposed_camps(camp_g, camp_a) ? 2 : 1);
            const std::int64_t p =
                over_step(ctx, floor_div(checked_mul(tuning_.cult_spread_permille, tuning_.affinity_permille[aff]), 1000));
            if (!chance_pm(ctx.draw(keys_[CULT_SPREAD], word(k), word(a), 0), p)) {
                continue;
            }
            EventSpec e = point_event(ctx, "cult_spread", home_region(ctx, a), 0);
            e.participants = {faction_role(k), god_role(g), civ_role("civilization", a), civ_role("from", from)};
            e.causes = {cults_[static_cast<std::size_t>(k - 1)].founding_event};
            if (link != 0 && link != e.causes.front()) {
                e.causes.push_back(link);
            }
            std::stable_sort(e.causes.begin(), e.causes.end());
            const std::int64_t ev = ctx.append(e);
            cult_in_[{k, a}] = ev;
            latest_[{k, a}] = ev;
            ctx.state().open_relation(cult_key(k, a, "cult_in"), ev);
            stats_.spreads += 1;
            break;  // at most one spread per cult per step
        }
    }
}

// --- 6.9 -------------------------------------------------------------------------------------------------------

void PantheonSubsystem::faiths(StepContext& ctx) {
    if (cult_in_.empty()) {
        return;
    }
    std::vector<std::pair<std::int64_t, std::int64_t>> pairs;
    for (const auto& [key, ev] : cult_in_) {
        (void)ev;
        pairs.push_back(key);
    }
    const std::int64_t y = ctx.year();
    const State& state = ctx.state();
    for (const auto& [k, c] : pairs) {
        if (k > settled_cults_ || !cult_in(k, c) || !alive_visible(ctx, c)) {
            continue;  // E4: a cult founded in this step waits for the next
        }
        if (cults_[static_cast<std::size_t>(k - 1)].status != 1) {
            continue;
        }
        const std::int64_t g = cults_[static_cast<std::size_t>(k - 1)].god;
        const std::int64_t founding_realm = cults_[static_cast<std::size_t>(k - 1)].civ;
        if (quiet(ctx) && ctx.quiet_events_left(c) <= 0) {
            continue;
        }
        const std::int64_t tau = temper_of(c);
        const auto temper_factor = [tau](const std::array<std::int64_t, 5>& t) {
            return tau >= 0 && tau < 5 ? t[static_cast<std::size_t>(tau)] : 1000;
        };
        const auto faith = faith_of_.find(c);
        // Faith (engineer's reading: the realm needs a standing capital, the
        // event's settlement).
        const std::int64_t capital = state.civ(c).capital;
        const bool capital_ok = capital != 0 && standing(state.settlement(capital)) &&
                                ctx.visible(state.settlement(capital));
        if (faith == faith_of_.end() && capital_ok && (regard(g, c) >= tuning_.adopt_regard || founding_realm == c)) {
            const std::int64_t p = over_step(
                ctx, floor_div(checked_mul(tuning_.adopt_permille, temper_factor(tuning_.temper_adopt_permille)), 1000));
            if (chance_pm(ctx.draw(keys_[ADOPT], word(k), word(c), 0), p)) {
                const Settlement& t = state.settlement(capital);
                const std::int64_t s = has_open_temple(capital) ? 0 : sites_->next_site_id();
                std::string temple;
                if (s != 0) {
                    temple = place_name(ctx, state.civ(c).culture, "temple_forms", keys_[SITE_NAME], s, g);
                }
                EventSpec e = point_event(ctx, "cult_adopted", t.region, capital);
                e.participants = {faction_role(k), god_role(g), civ_role("civilization", c)};
                if (s != 0) {
                    e.participants.push_back(Participant{"site", EntityType::site, s});
                }
                e.causes = {cult_in_.at({k, c})};
                const std::int64_t ev = ctx.append(e);
                if (s != 0) {
                    MonstersSubsystem::SiteSpec spec;
                    spec.id = s;
                    spec.kind = 6;
                    spec.name = std::move(temple);
                    spec.region = t.region;
                    spec.x = t.x;
                    spec.y = t.y;
                    spec.settlement = capital;
                    spec.founder_civ = c;
                    spec.origin_year = y;
                    spec.origin_event = ev;
                    sites_->open_site(spec);
                    temples_.push_back(Temple{s, k, capital, c, ev, true});
                }
                faith_of_[c] = {k, ev};
                faith_since_[c] = ctx.event_year(ev);
                latest_[{k, c}] = ev;
                ctx.state().open_relation(cult_key(k, c, "faith_of"), ev);
                add(g, c, tuning_.faith_points, ev, y);
                // The gods of the opposed camp, ascending, and the realm's
                // creator when its camp differs from g's.
                const std::int64_t camp_g = god(g).camp;
                std::int64_t creator = 0;
                for (const GodState& x : gods_) {
                    creator = x.race == state.civ(c).race ? x.id : creator;
                }
                bool creator_done = false;
                for (const GodState& x : gods_) {
                    if (opposed_camps(x.camp, camp_g)) {
                        add(x.id, c, -tuning_.rival_faith_points, ev, y);
                        creator_done = creator_done || x.id == creator;
                    }
                }
                if (creator != 0 && !creator_done && god(creator).camp != camp_g) {
                    add(creator, c, -tuning_.rival_faith_points, ev, y);
                }
                stats_.adopted += 1;
                continue;  // at most one of these per step
            }
        }
        // Suppression.
        if (faith != faith_of_.end() && faith->second.first == k) {
            continue;  // never the realm's own faith
        }
        bool hostile = opposed_camps(god(g).camp, civ_camp(ctx, c));
        if (!hostile && faith != faith_of_.end()) {
            const std::int64_t other_god = cults_[static_cast<std::size_t>(faith->second.first - 1)].god;
            hostile = opposed_camps(god(other_god).camp, god(g).camp);
        }
        if (!hostile) {
            continue;
        }
        const std::int64_t p = over_step(
            ctx, floor_div(checked_mul(tuning_.suppress_permille, temper_factor(tuning_.temper_suppress_permille)), 1000));
        if (!chance_pm(ctx.draw(keys_[SUPPRESS], word(k), word(c), 0), p)) {
            continue;
        }
        const bool last = cult_realms(k) == 1;
        bool hidden = false;
        std::int64_t sanctum_region = 0;
        MonstersSubsystem::HeightCell sanctum_cell;
        if (last && chance_pm(ctx.draw(keys_[HIDE], word(k), 0, 0), tuning_.hide_permille)) {
            const std::int64_t seat_region = state.settlement(cults_[static_cast<std::size_t>(k - 1)].seat_settlement).region;
            for (const std::int64_t r : regions_by_hops(ctx, seat_region, tuning_.sanctum_hops)) {
                if (const std::optional<MonstersSubsystem::HeightCell> cell = sites_->height_cell(ctx, r)) {
                    sanctum_region = r;
                    sanctum_cell = *cell;
                    hidden = true;
                    break;
                }
            }
        }
        const std::int64_t s2 = hidden ? sites_->next_site_id() : 0;
        std::string sanctum;
        if (hidden) {
            sanctum = place_name(ctx, state.civ(c).culture, "sanctum_forms", keys_[SITE_NAME], s2, g);
        }
        EventSpec e = point_event(ctx, "cult_suppressed", home_region(ctx, c), 0);
        e.participants = {faction_role(k), god_role(g), civ_role("civilization", c)};
        if (hidden) {
            e.participants.push_back(Participant{"sanctum", EntityType::site, s2});
        }
        e.values = {hidden ? 1 : 0};
        e.causes = {latest_.at({k, c})};
        const std::int64_t ev = ctx.append(e);
        suppressed_[{k, c}] = ctx.year();  // P5-11 12.2
        // 1. The realm's cult_in closes; its temples of the cult end (8).
        ctx.state().close_relation(cult_key(k, c, "cult_in"), ev);
        cult_in_.erase({k, c});
        latest_.erase({k, c});
        for (Temple& t : temples_) {
            if (t.open && t.cult == k && state.settlement(t.settlement).owner_civ == c) {
                end_temple(ctx, t, 8, ev);
            }
        }
        // 2. The last realm: hidden at a sanctum, or the cult ends.
        if (last) {
            if (hidden) {
                MonstersSubsystem::SiteSpec spec;
                spec.id = s2;
                spec.kind = 7;
                spec.name = std::move(sanctum);
                spec.region = sanctum_region;
                spec.x = sanctum_cell.x;
                spec.y = sanctum_cell.y;
                spec.settlement = 0;
                spec.founder_civ = c;
                spec.origin_year = y;
                spec.origin_event = ev;
                sites_->open_site(spec);
                Cult& cult = cults_[static_cast<std::size_t>(k - 1)];
                cult.status = 2;
                cult.sanctum = s2;
                stats_.hidden += 1;
            } else {
                end_cult(ctx, k, 1, ev);
            }
        }
        add(g, c, -tuning_.suppress_points, ev, y);
        stats_.suppressed += 1;
    }
}

// --- P5-11 r4 7.9: the faith reads of the artifacts ----------------------------------------------------

DivineView::Faith PantheonSubsystem::faith(std::int64_t civ) const {
    const auto f = faith_of_.find(civ);
    if (f == faith_of_.end()) {
        return {};
    }
    const auto since = faith_since_.find(civ);
    return Faith{f->second.first, since != faith_since_.end() ? since->second : 0, f->second.second};
}

std::int64_t PantheonSubsystem::faction_god(std::int64_t faction) const {
    if (faction < 1 || static_cast<std::size_t>(faction) > cults_.size()) {
        return 0;
    }
    return cults_[static_cast<std::size_t>(faction - 1)].god;
}

std::int64_t PantheonSubsystem::temple_faction(std::int64_t site) const {
    const auto it = std::find_if(temples_.begin(), temples_.end(), [site](const Temple& t) { return t.site == site; });
    return it != temples_.end() ? it->cult : 0;
}

}  // namespace worldgen::history
