// Artifacts r2 (spec docs/specs/p5-11-artifacts.md 6.11, 7.7, 8.4): an evil
// god's artifact kept by a realm may turn its reigning ruler's temper. The
// pass runs at the end of every step of the subsystem, after the log and the
// seats: its lifting part in T3, T4, T6 and Q4, its draws in T3 and Q4.
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

namespace worldgen::history {

using namespace conflict_detail;
using namespace artifacts_detail;

namespace {

constexpr std::int64_t EVIL_CAMP = 2;
constexpr std::int64_t WARLIKE = 1;
constexpr std::int64_t GRASPING = 3;

std::int64_t camp_in(std::span<const std::pair<std::int64_t, std::int64_t>> camps, std::int64_t id) {
    const auto it = std::lower_bound(camps.begin(), camps.end(), std::pair<std::int64_t, std::int64_t>{id, -1});
    return it != camps.end() && it->first == id ? it->second : -1;
}

}  // namespace

// 6.11: evil when a.god is a god of camp 2; nothing else counts.
bool ArtifactsSubsystem::evil(const Artifact& a) const {
    return a.god != 0 && camp_in(god_camps_, a.god) == EVIL_CAMP;
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
    }
}

// The corruption of an alive c ends when the artifact that turned it is no
// longer kept by c (any move of 6.7 in this step). No event.
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
        const Holding& h = open_holding(a);
        if (h.state != KEPT || h.holder != c) {
            figures_->lift_corruption(c);
            stats_.lifted += 1;
        }
    }
}

void ArtifactsSubsystem::corruption_draws(StepContext& ctx) {
    const std::int64_t nc = civ_count(ctx.state());
    for (std::int64_t c = 1; c <= nc; ++c) {
        if (!civ_alive(ctx, c) || figures_->reign_brief(c).number == 0) {
            continue;
        }
        const std::vector<std::int64_t> kept = held_by(KEPT, c);  // a copy, ascending id
        for (const std::int64_t a : kept) {
            if (!evil(artifact(a))) {
                continue;
            }
            if (!corrupt_periods(ctx, c, a)) {
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
            corrupt(ctx, c, a, u < warlike ? WARLIKE : GRASPING);
        }
    }
}

// 8.4: artifact_corrupted at seat(c) (its region), else c's home region; then
// the reign is turned (7.7).
void ArtifactsSubsystem::corrupt(StepContext& ctx, std::int64_t c, std::int64_t a, std::int64_t temper) {
    const std::int64_t seat = seat_of(c);
    const std::int64_t region = seat != 0 ? ctx.state().settlement(seat).region : ctx.state().civ(c).home_region;
    const Artifact& art = artifact(a);
    EventSpec spec = point_event(ctx, "artifact_corrupted", region, seat);
    spec.participants = {artifact_role(a), civ_role("civilization", c), Participant{"god", EntityType::god, art.god}};
    spec.values = {temper};
    spec.causes = {art.holdings.back().event, art.made_event};
    std::stable_sort(spec.causes.begin(), spec.causes.end());
    spec.causes.erase(std::unique(spec.causes.begin(), spec.causes.end()), spec.causes.end());
    const std::int64_t e = ctx.append(spec);
    figures_->corrupt_reign(ctx, c, a, temper, e);
    stats_.corrupted += 1;
    stats_.corrupted_warlike += temper == WARLIKE ? 1 : 0;
}

}  // namespace worldgen::history
