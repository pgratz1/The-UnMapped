// Artifacts r2 (spec docs/specs/p5-11-artifacts.md 6.11, 7.7, 8.4): an evil
// god's artifact kept by a realm may turn its reigning ruler's temper; (r4
// 6.13) so may one enshrined at an open temple of the realm's faith whose
// god is the artifact's; (r3 6.12, 7.8, 8.5) an evil artifact borne by a
// general or hero may turn its bearer. The pass runs at the end of every
// step of the subsystem, after the log and the seats: its lifting part in
// T3, T4, T6 and Q4, its draws (rulers, then bearers) in T3 and Q4.
//
// One keyed draw per evaluated period, at the period's own year t_j (not
// the step's), so the outcome does not depend on which step evaluates it.
// Civilizations ascending, their kept artifacts ascending id, periods
// ascending. No artifact moves here; figures change only through
// FiguresSubsystem::corrupt_reign and lift_corruption.

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "artifacts_internal.hpp"
#include "conflict_internal.hpp"
#include "worldgen/core/checked.hpp"
#include "worldgen/history/artifacts.hpp"
#include "worldgen/history/draw.hpp"
#include "worldgen/history/figures.hpp"
#include "worldgen/history/monsters.hpp"
#include "worldgen/history/pantheon.hpp"

namespace worldgen::history {

using namespace conflict_detail;
using namespace artifacts_detail;

namespace {

constexpr std::int64_t EVIL_CAMP = 2;
constexpr std::int64_t WARLIKE = 1;
constexpr std::int64_t GRASPING = 3;
constexpr std::int64_t GENERAL_ROLE = 3;
constexpr std::int64_t HERO_ROLE = 4;
constexpr std::int64_t SITE_TEMPLE = 6;
// r4 6.13: the key offset of a faith exposure's period (j <= 10,000).
constexpr std::int64_t FAITH_KEY_OFFSET = 1000000;

std::int64_t camp_in(std::span<const std::pair<std::int64_t, std::int64_t>> camps, std::int64_t id) {
    const auto it = std::lower_bound(camps.begin(), camps.end(), std::pair<std::int64_t, std::int64_t>{id, -1});
    return it != camps.end() && it->first == id ? it->second : -1;
}

}  // namespace

// 6.11: evil when a.god is a god of camp 2; nothing else counts.
bool ArtifactsSubsystem::evil(const Artifact& a) const {
    return a.god != 0 && camp_in(god_camps_, a.god) == EVIL_CAMP;
}

std::int64_t ArtifactsSubsystem::god_camp(std::int64_t god) const {
    return camp_in(god_camps_, god);
}

// 6.13: a enshrined at an open temple s whose faction k is of a's god, and
// k is c's faith: the faith's cult_adopted (and its year in *since).
std::int64_t ArtifactsSubsystem::faith_exposure(std::int64_t c, std::int64_t a, std::int64_t* since) const {
    if (divine_ == nullptr || sites_ == nullptr) {
        return 0;
    }
    const Artifact& art = artifact(a);
    const Holding& h = art.holdings.back();
    if (h.state != ENSHRINED || !evil(art) || !open_site_of_kind(h.site, SITE_TEMPLE)) {
        return 0;
    }
    const std::int64_t k = divine_->temple_faction(h.site);
    if (k == 0 || divine_->faction_god(k) != art.god) {
        return 0;
    }
    const DivineView::Faith faith = divine_->faith(c);
    if (faith.faction != k || faith.event == 0) {
        return 0;
    }
    if (since != nullptr) {
        *since = faith.since;
    }
    return faith.event;
}

bool ArtifactsSubsystem::exposes(std::int64_t c, std::int64_t a) const {
    const Holding& h = open_holding(a);
    return (h.state == KEPT && h.holder == c) || faith_exposure(c, a, nullptr) != 0;
}

std::int64_t ArtifactsSubsystem::civ_camp(const StepContext& ctx, std::int64_t civ) const {
    const std::int64_t camp = camp_in(race_camps_, ctx.state().civ(civ).race);
    if (camp < 0 || camp > 2) {
        fail("history: artifacts: civilization " + int_text(civ) + " has no camp");
    }
    return camp;
}

void ArtifactsSubsystem::corruption(StepContext& ctx) {
    if (figures_ == nullptr) {
        return;  // no reigns (a test seam)
    }
    lift_corruptions(ctx);
    if (ctx.step() == Step::t3 || ctx.step() == Step::q4) {
        corruption_draws(ctx);
        bearer_draws(ctx);  // r3 6.1: after the rulers' draws
    }
}

// The corruption of an alive c ends when the artifact that turned it no
// longer exposes c: not kept by c (any move of 6.7 in this step) and (r4)
// not enshrined at an open temple of c's faith. No event. Then (6.13) the
// faith exposures that have ended are erased.
void ArtifactsSubsystem::lift_corruptions(const StepContext& ctx) {
    const std::int64_t nc = civ_count(ctx.state());
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (!civ_alive(ctx, c)) {
            continue;
        }
        const std::int64_t a = figures_->reign_brief(c).corrupt_artifact;
        if (a == 0) {
            continue;
        }
        if (!exposes(c, a)) {
            figures_->lift_corruption(c);
            stats_.lifted += 1;
        }
    }
    for (auto it = exposures_.begin(); it != exposures_.end();) {  // (artifact, civ) order
        const auto [a, c] = it->first;
        if (!civ_alive(ctx, c) || faith_exposure(c, a, nullptr) == 0) {
            it = exposures_.erase(it);
        } else {
            ++it;
        }
    }
}

void ArtifactsSubsystem::corruption_draws(StepContext& ctx) {
    const std::int64_t nc = civ_count(ctx.state());
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (!civ_alive(ctx, c) || figures_->reign_brief(c).number == 0) {
            continue;
        }
        // Reading (Part M 9): in Q4 the figures have run their lines to the
        // step's end, so c's reign may have begun after Y, the year an event
        // 117 would carry; its periods wait for the next pass (the keyed draw
        // gives the same result there).
        if (ctx.step() == Step::q4 && figures_->reign_brief(c).start > ctx.year()) {
            continue;
        }
        // 6.13: c's exposures in ascending artifact id over both kinds (an
        // artifact is in one holding, so of one kind at a time). Nothing
        // moves in the pass, so this is r2's walk over the kept ids.
        const auto n = static_cast<std::int64_t>(artifacts_.size());
        for (std::int64_t a = 1; a <= n; ++a) {
            if (!evil(artifact(a))) {
                continue;
            }
            const Holding& h = open_holding(a);
            bool more = true;
            if (h.state == KEPT && h.holder == c) {
                more = corrupt_periods(ctx, c, a);
            } else if (faith_exposure(c, a, nullptr) != 0) {
                more = faith_periods(ctx, c, a);
            }
            if (!more) {
                break;  // Q4 without budget: c's periods wait for the next pass
            }
        }
    }
}

// The periods t_j = f + j * T of a's open (kept) holding with t_j <= Y not
// yet evaluated, ascending. Members are re-read by index after every append.
bool ArtifactsSubsystem::corrupt_periods(StepContext& ctx, std::int64_t c, std::int64_t a) {
    const std::int64_t y = ctx.year();
    const std::int64_t period = tuning_.corrupt_years;
    const std::size_t hi = artifact(a).holdings.size() - 1;
    const std::int64_t from = artifact(a).holdings[hi].from_year;
    for (std::int64_t j = artifact(a).holdings[hi].periods + 1;; ++j) {
        const std::int64_t t = checked_add(from, checked_mul(j, period));
        if (t > y) {
            return true;
        }
        if (figures_->reign_brief(c).corrupt_event != 0) {
            artifact(a).holdings[hi].periods = j;  // a corruption in force: no draw
            continue;
        }
        if (ctx.step() == Step::q4 && ctx.quiet_events_left(c) <= 0) {
            return false;
        }
        const std::int64_t p = floor_div(
            checked_mul(tuning_.corrupt_permille, tuning_.corrupt_camp_permille[static_cast<std::size_t>(civ_camp(ctx, c))]),
            1000);
        // D_at(hist.artifacts.corrupt; t_j; a, c, j): the period's own year,
        // which may lie before the step (draw_at refuses it), so the keyed
        // draw is called directly.
        const auto u = static_cast<std::int64_t>(pick(draw(keys_[CORRUPT], word(t), word(a), word(c), word(j)), 1000));
        artifact(a).holdings[hi].periods = j;
        stats_.periods += 1;
        if (u < p) {
            const std::int64_t warlike = floor_div(checked_mul(p, tuning_.corrupt_warlike_permille), 1000);
            corrupt(ctx, c, a, u < warlike ? WARLIKE : GRASPING, 0);
        }
    }
}

// 6.13: the periods t_j = f + j * T of c's faith exposure to a, f = max(the
// holding's from_year, the faith's year); keyed (a, c, 1,000,000 + j) on
// stream 128. A changed origin (a new holding or a new faith) starts the
// count again.
bool ArtifactsSubsystem::faith_periods(StepContext& ctx, std::int64_t c, std::int64_t a) {
    const std::int64_t y = ctx.year();
    const std::int64_t period = tuning_.corrupt_years;
    std::int64_t since = 0;
    const std::int64_t faith_event = faith_exposure(c, a, &since);
    const std::int64_t origin = std::max(open_holding(a).from_year, since);
    const std::pair<std::int64_t, std::int64_t> key{a, c};
    {
        auto it = exposures_.find(key);
        if (it == exposures_.end() || it->second.first != origin) {
            exposures_[key] = {origin, 0};
        }
    }
    for (std::int64_t j = exposures_.at(key).second + 1;; ++j) {
        if (j >= FAITH_KEY_OFFSET) {
            fail("history: artifacts: faith exposure of c" + int_text(c) + " to ar" + int_text(a) +
                 " exceeds its periods");
        }
        const std::int64_t t = checked_add(origin, checked_mul(j, period));
        if (t > y) {
            return true;
        }
        if (figures_->reign_brief(c).corrupt_event != 0) {
            exposures_.at(key).second = j;  // a corruption in force: no draw
            continue;
        }
        if (ctx.step() == Step::q4 && ctx.quiet_events_left(c) <= 0) {
            return false;
        }
        const std::int64_t p = floor_div(
            checked_mul(tuning_.corrupt_permille, tuning_.corrupt_camp_permille[static_cast<std::size_t>(civ_camp(ctx, c))]),
            1000);
        const auto u = static_cast<std::int64_t>(
            pick(draw(keys_[CORRUPT], word(t), word(a), word(c), word(checked_add(FAITH_KEY_OFFSET, j))), 1000));
        exposures_.at(key).second = j;
        stats_.faith_periods += 1;
        if (u < p) {
            const std::int64_t warlike = floor_div(checked_mul(p, tuning_.corrupt_warlike_permille), 1000);
            corrupt(ctx, c, a, u < warlike ? WARLIKE : GRASPING, faith_event);
            stats_.corrupted_by_faith += 1;
        }
    }
}

// 8.4: artifact_corrupted at seat(c) (its region), else c's home region; then
// the reign is turned (7.7).
void ArtifactsSubsystem::corrupt(StepContext& ctx, std::int64_t c, std::int64_t a, std::int64_t temper,
                                 std::int64_t faith_event) {
    const std::int64_t seat = seat_of(c);
    const std::int64_t region = seat != 0 ? ctx.state().settlement(seat).region : ctx.state().civ(c).home_region;
    const Artifact& art = artifact(a);
    EventSpec spec = point_event(ctx, "artifact_corrupted", region, seat);
    spec.participants = {artifact_role(a), civ_role("civilization", c), Participant{"god", EntityType::god, art.god}};
    spec.values = {temper};
    spec.causes = {art.holdings.back().event, art.made_event};
    if (faith_event != 0) {
        spec.causes.push_back(faith_event);  // r4 6.13: the faith's cult_adopted
    }
    std::stable_sort(spec.causes.begin(), spec.causes.end());
    spec.causes.erase(std::unique(spec.causes.begin(), spec.causes.end()), spec.causes.end());
    const std::int64_t e = ctx.append(spec);
    figures_->corrupt_reign(ctx, c, a, temper, e);
    stats_.corrupted += 1;
    stats_.corrupted_warlike += temper == WARLIKE ? 1 : 0;
}

// --- Bearers (r3 6.12) -----------------------------------------------------------------------------------

// The open holdings of state 2 of evil artifacts, ascending artifact id.
void ArtifactsSubsystem::bearer_draws(StepContext& ctx) {
    const auto n = static_cast<std::int64_t>(artifacts_.size());
    for (std::int64_t a = 1; a <= n; ++a) {
        if (evil(artifact(a)) && open_holding(a).state == BORNE) {
            bearer_periods(ctx, a);
        }
    }
}

// The periods t_j = f + j * Tb of a's borne holding; champions, the dead and
// the already corrupted are not tested. No camp factor (L-20).
void ArtifactsSubsystem::bearer_periods(StepContext& ctx, std::int64_t a) {
    const std::size_t hi = artifact(a).holdings.size() - 1;
    const std::int64_t f = artifact(a).holdings[hi].holder;
    const FigureView::Brief b = figures_->brief(f);
    if (!b.alive || (b.role != GENERAL_ROLE && b.role != HERO_ROLE) || bearer_corrupt_.count(f) != 0) {
        return;
    }
    const std::int64_t y = ctx.year();
    const std::int64_t period = tuning_.corrupt_bearer_years;
    const std::int64_t from = artifact(a).holdings[hi].from_year;
    for (std::int64_t j = artifact(a).holdings[hi].periods + 1;; ++j) {
        const std::int64_t t = checked_add(from, checked_mul(j, period));
        if (t > y) {
            return;
        }
        if (ctx.step() == Step::q4 && ctx.quiet_events_left(b.civ) <= 0) {
            return;  // periods unchanged
        }
        // D_at(hist.artifacts.corrupt_bearer; t_j; a, f, j), called directly
        // as the rulers' draw.
        const auto u =
            static_cast<std::int64_t>(pick(draw(keys_[CORRUPT_BEARER], word(t), word(a), word(f), word(j)), 1000));
        artifact(a).holdings[hi].periods = j;
        stats_.bearer_periods += 1;
        if (u < tuning_.corrupt_bearer_permille) {
            corrupt_bearer(ctx, f, b.civ, a, b.role);
            return;
        }
    }
}

// 8.5: bearer_corrupted at seat(c)'s region (c alive), else c's home region;
// then figures applies the role's consequence (7.8).
void ArtifactsSubsystem::corrupt_bearer(StepContext& ctx, std::int64_t f, std::int64_t c, std::int64_t a,
                                        std::int64_t role) {
    const std::int64_t seat = civ_alive(ctx, c) ? seat_of(c) : 0;
    const std::int64_t region = seat != 0 ? ctx.state().settlement(seat).region : ctx.state().civ(c).home_region;
    const Artifact& art = artifact(a);
    EventSpec spec = point_event(ctx, "bearer_corrupted", region, 0);
    spec.participants = {artifact_role(a), figure_role("figure", f), civ_role("civilization", c),
                         Participant{"god", EntityType::god, art.god}};
    spec.values = {role};
    spec.causes = {art.holdings.back().event, art.made_event};
    std::stable_sort(spec.causes.begin(), spec.causes.end());
    spec.causes.erase(std::unique(spec.causes.begin(), spec.causes.end()), spec.causes.end());
    const std::int64_t e = ctx.append(spec);
    bearer_corrupt_[f] = {e, a};
    figures_->corrupt_bearer(ctx, f, e);
    stats_.generals_corrupted += role == GENERAL_ROLE ? 1 : 0;
    stats_.heroes_outlawed += role == HERO_ROLE ? 1 : 0;
}

// 7.8: a living general's bearer_corrupted while he bears its artifact.
std::int64_t ArtifactsSubsystem::bearer_corruption(std::int64_t f) const {
    const auto it = bearer_corrupt_.find(f);
    if (it == bearer_corrupt_.end() || figures_ == nullptr) {
        return 0;
    }
    const FigureView::Brief b = figures_->brief(f);
    if (!b.alive || b.role != GENERAL_ROLE) {
        return 0;
    }
    const Holding& h = open_holding(it->second.second);
    return h.state == BORNE && h.holder == f ? it->second.first : 0;
}

}  // namespace worldgen::history
