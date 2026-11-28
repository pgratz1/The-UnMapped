// Artifacts (spec docs/specs/p5-11-artifacts.md): the moves of 6.7, read
// from the log, and the quiet moves of 6.8.
//
// In each row "each" is in ascending artifact id over a copy of the holder's
// list taken before the row's moves; a move that writes an artifact event
// writes it at once, then makes the holding with that event; a quiet move
// uses the triggering event as the holding's event.

#include <string_view>
#include <vector>

#include "artifacts_internal.hpp"
#include "conflict_internal.hpp"
#include "worldgen/history/artifacts.hpp"
#include "worldgen/history/draw.hpp"
#include "worldgen/history/figures.hpp"
#include "worldgen/history/monsters.hpp"

namespace worldgen::history {

using namespace conflict_detail;
using namespace artifacts_detail;

namespace {

constexpr std::int64_t SITE_TOWER = 4;
constexpr std::int64_t SITE_TEMPLE = 6;
constexpr std::int64_t ENDING_SUPPRESSED = 8;  // P5-08 9: a temple ended by its cult's suppression

bool ruined(const Settlement& s) noexcept {
    return s.status == SettlementStatus::ruined || s.status == SettlementStatus::faded;
}

}  // namespace

void ArtifactsSubsystem::moves(StepContext& ctx, std::int64_t e, std::string_view k) {
    if (artifacts_.empty()) {
        return;
    }
    if (k == "general_appointed") {
        on_general(ctx, e);
    } else if (k == "figure_died") {
        on_figure_died(ctx, e);
    } else if (k == "fell_in_battle") {
        on_fell(ctx, e);
    } else if (k == "hero_slain") {
        on_hero_slain(ctx, e);
    } else if (k == "monster_slain_by_hero") {
        on_monster_slain(ctx, e, true);
    } else if (k == "monster_slain") {
        on_monster_slain(ctx, e, false);
    } else if (k == "monster_died") {
        on_monster_died(ctx, e);
    } else if (k == "monster_raid") {
        on_raid(ctx, e);
    } else if (k == "hold_lost") {
        on_hold_lost(ctx, e);
    } else if (k == "sacked") {
        on_sacked(ctx, e, true);
    } else if (k == "settlement_captured") {
        on_sacked(ctx, e, false);
    } else if (k == "civ_conquered") {
        on_realm_end(ctx, e, ctx.event_participant(e, "civilization"), ctx.event_participant(e, "conqueror"));
    } else if (k == "civ_ended") {
        on_realm_end(ctx, e, ctx.event_participant(e, "civilization"), 0);
    } else if (k == "peace" || k == "war_won") {
        on_war_end(ctx, e);
    } else if (k == "cult_suppressed") {
        on_cult_suppressed(ctx, e);
    } else if (k == "hero_rose") {
        on_hero_rose(ctx, e);
    }
}

// r3 M15: a newly risen hero takes up the smallest-id evil arms, then
// armour, its realm keeps; only while corrupt_bearer_permille is above 0 (I2).
void ArtifactsSubsystem::on_hero_rose(StepContext& ctx, std::int64_t e) {
    if (tuning_.corrupt_bearer_permille <= 0) {
        return;
    }
    const std::int64_t f = ctx.event_participant(e, "figure");
    const std::int64_t c = ctx.event_participant(e, "civilization");
    if (!figure_alive(f)) {
        return;
    }
    for (const std::int64_t cls : {static_cast<std::int64_t>(ARMS), static_cast<std::int64_t>(ARMOUR)}) {
        if (bears_class(f, cls)) {
            continue;
        }
        for (const std::int64_t a : held_by(KEPT, c)) {  // a copy, ascending id
            if (kind_of(artifact(a)).artifact_class == cls && evil(artifact(a))) {
                move(a, ctx.event_year(e), e, holder(BORNE, FIGURE_TYPE, f));
                stats_.quiet_moves += 1;
                stats_.taken_up_by_heroes += 1;
                break;
            }
        }
    }
}

// M1: a new general takes up the smallest-id arms, then armour, its realm keeps.
void ArtifactsSubsystem::on_general(StepContext& ctx, std::int64_t e) {
    const std::int64_t f = ctx.event_participant(e, "figure");
    const std::int64_t c = ctx.event_participant(e, "civilization");
    if (!figure_alive(f)) {
        return;
    }
    for (const std::int64_t cls : {static_cast<std::int64_t>(ARMS), static_cast<std::int64_t>(ARMOUR)}) {
        if (bears_class(f, cls)) {
            continue;
        }
        // L-28 ("the dark blade calls"): a kept evil artifact of the class
        // first, then any other; smallest id first within each.
        std::int64_t pick_id = 0;
        for (const std::int64_t a : held_by(KEPT, c)) {  // ascending id
            if (kind_of(artifact(a)).artifact_class != cls) {
                continue;
            }
            if (evil(artifact(a))) {
                pick_id = a;
                break;
            }
            if (pick_id == 0) {
                pick_id = a;
            }
        }
        if (pick_id != 0) {
            move(pick_id, ctx.event_year(e), e, holder(BORNE, FIGURE_TYPE, f));
            stats_.quiet_moves += 1;
        }
    }
}

// M2: a bearer's death of age.
void ArtifactsSubsystem::on_figure_died(StepContext& ctx, std::int64_t e) {
    const std::int64_t f = ctx.event_participant(e, "figure");
    const std::int64_t c = figure_civ(f);
    const FigureView::Brief b = figures_ != nullptr ? figures_->brief(f) : FigureView::Brief{};
    for (const std::int64_t a : held_by(BORNE, f)) {
        const Artifact& art = artifact(a);
        if (kind_of(art).artifact_class == STAFF && art.maker == f && art.maker_kind == 3) {
            const std::int64_t s = figures_ != nullptr ? figures_->caster_site(f) : 0;
            if (open_site_of_kind(s, SITE_TOWER)) {
                move(a, ctx.event_year(e), e, placed(ENSHRINED, site_place(s, true)));
                stats_.quiet_moves += 1;
                continue;
            }
        }
        bool bury = true;
        if (alive_visible(ctx, c)) {
            bury = chance_pm(ctx.draw(keys_[BURY], word(a), word(f), 0), tuning_.burial_permille);
            if (!bury) {
                move(a, ctx.event_year(e), e, holder(KEPT, CIV_TYPE, c));
                stats_.quiet_moves += 1;
                continue;
            }
        }
        Emit x{"artifact_buried", b.death_region, b.death_settlement,
               {artifact_role(a), figure_role("figure", f), civ_role("civilization", c)}, {}, 0};
        const std::int64_t ev = emit(ctx, x, e);
        move(a, ctx.event_year(ev), ev, placed(BURIED, grave_place(ctx, f)));
    }
}

// M3: a general fallen in battle.
void ArtifactsSubsystem::on_fell(StepContext& ctx, std::int64_t e) {
    const std::int64_t f = ctx.event_participant(e, "figure");
    const std::int64_t c = ctx.event_participant(e, "civilization");
    const std::int64_t enemy = ctx.event_participant(e, "enemy");
    const std::int64_t region = ctx.event_region(e);
    for (const std::int64_t a : held_by(BORNE, f)) {
        if (civ_alive(ctx, enemy) &&
            chance_pm(ctx.draw(keys_[TAKE], word(e), word(a), 0), tuning_.battle_take_permille)) {
            Emit x{"artifact_taken", region, ctx.event_settlement(e),
                   {artifact_role(a), civ_role("taker", enemy), civ_role("from", c), figure_role("figure", f)},
                   {1}, 0};
            const std::int64_t ev = emit(ctx, x, e);
            move(a, ctx.event_year(ev), ev, holder(KEPT, CIV_TYPE, enemy));
        } else {
            Emit x{"artifact_lost", region, 0, {artifact_role(a), civ_role("civilization", c), figure_role("figure", f)},
                   {}, 0};
            const std::int64_t ev = emit(ctx, x, e);
            move(a, ctx.event_year(ev), ev, placed(LOST, field_place(region)));
        }
    }
}

// M4: a hero slain by a monster.
void ArtifactsSubsystem::on_hero_slain(StepContext& ctx, std::int64_t e) {
    const std::int64_t f = ctx.event_participant(e, "figure");
    const std::int64_t c = ctx.event_participant(e, "civilization");
    const std::int64_t m = ctx.event_participant(e, "monster");
    const std::int64_t lair = monster_lair(m);
    const std::optional<MonstersSubsystem::SiteBrief> lb =
        sites_ != nullptr && lair != 0 ? sites_->site_brief(lair) : std::nullopt;
    const std::int64_t region = lb ? lb->region : ctx.event_region(e);
    for (const std::int64_t a : held_by(BORNE, f)) {
        Emit x{"artifact_hoarded", region, 0,
               {artifact_role(a), monster_role(m), civ_role("civilization", c), figure_role("figure", f)}, {1}, 0};
        const std::int64_t ev = emit(ctx, x, e);
        move(a, ctx.event_year(ev), ev, holder(HOARDED, MONSTER_TYPE, m));
    }
}

// M5: a hoard's keeper slain.
void ArtifactsSubsystem::on_monster_slain(StepContext& ctx, std::int64_t e, bool by_hero) {
    const std::int64_t m = ctx.event_participant(e, "monster");
    const std::int64_t c = ctx.event_participant(e, "slayer");
    const std::int64_t h = by_hero ? ctx.event_participant(e, "hero") : 0;
    const std::int64_t lair = monster_lair(m);
    const std::optional<MonstersSubsystem::SiteBrief> lb =
        sites_ != nullptr && lair != 0 ? sites_->site_brief(lair) : std::nullopt;
    const std::int64_t region = lb ? lb->region : ctx.event_region(e);
    for (const std::int64_t a : held_by(HOARDED, m)) {
        const std::int64_t cls = kind_of(artifact(a)).artifact_class;
        const bool bear = (cls == ARMS || cls == ARMOUR) && h != 0 && figure_alive(h) && !bears_class(h, cls);
        Emit x{"artifact_recovered", region, 0, {artifact_role(a), civ_role("civilization", c)}, {1}, 0};
        if (h != 0) {
            x.participants.push_back(figure_role("figure", h));
        }
        x.participants.push_back(monster_role(m));
        const std::int64_t ev = emit(ctx, x, e);
        move(a, ctx.event_year(ev), ev, bear ? holder(BORNE, FIGURE_TYPE, h) : holder(KEPT, CIV_TYPE, c));
    }
}

// M6: a keeper's death of age; its hoard stays in its lair.
void ArtifactsSubsystem::on_monster_died(StepContext& ctx, std::int64_t e) {
    const std::int64_t m = ctx.event_participant(e, "monster");
    const std::int64_t lair = monster_lair(m);
    for (const std::int64_t a : held_by(HOARDED, m)) {
        const Holding h = lair != 0 ? placed(LOST, site_place(lair, false)) : placed(LOST, field_place(ctx.event_region(e)));
        move(a, ctx.event_year(e), e, h);
        stats_.quiet_moves += 1;
    }
}

// M7: a raid on a seat may carry off a treasure.
void ArtifactsSubsystem::on_raid(StepContext& ctx, std::int64_t e) {
    const std::int64_t m = ctx.event_participant(e, "monster");
    const std::int64_t c = ctx.event_participant(e, "civilization");
    const std::int64_t t = ctx.event_settlement(e);
    if (t == 0 || t != seat_of(c)) {
        return;
    }
    const std::vector<std::int64_t> kept = held_by(KEPT, c);
    if (kept.empty() || !chance_pm(ctx.draw(keys_[RAID], word(e), 0, 0), tuning_.raid_take_permille)) {
        return;
    }
    const std::int64_t a = kept.front();
    Emit x{"artifact_hoarded", ctx.state().settlement(t).region, t,
           {artifact_role(a), monster_role(m), civ_role("civilization", c)}, {2}, 0};
    const std::int64_t ev = emit(ctx, x, e);
    move(a, ctx.event_year(ev), ev, holder(HOARDED, MONSTER_TYPE, m));
}

// M8: a hold lost to the deep at the seat.
void ArtifactsSubsystem::on_hold_lost(StepContext& ctx, std::int64_t e) {
    const std::int64_t m = ctx.event_participant(e, "monster");
    const std::int64_t c = ctx.event_participant(e, "civilization");
    const std::int64_t t = ctx.event_settlement(e);
    if (t == 0 || t != seat_of(c)) {
        return;
    }
    for (const std::int64_t a : held_by(KEPT, c)) {
        Emit x{"artifact_hoarded", ctx.state().settlement(t).region, t,
               {artifact_role(a), monster_role(m), civ_role("civilization", c)}, {3}, 0};
        const std::int64_t ev = emit(ctx, x, e);
        move(a, ctx.event_year(ev), ev, holder(HOARDED, MONSTER_TYPE, m));
    }
}

// M9 (sack) and M10 (capture) of the seat.
void ArtifactsSubsystem::on_sacked(StepContext& ctx, std::int64_t e, bool sack) {
    const std::int64_t x_civ = ctx.event_participant(e, "attacker");
    const std::int64_t d = ctx.event_participant(e, "defender");
    const std::int64_t t = ctx.event_settlement(e);
    if (t == 0 || t != seat_of(d)) {
        return;
    }
    const std::int64_t take = sack ? tuning_.sack_take_permille : tuning_.capture_take_permille;
    for (const std::int64_t a : held_by(KEPT, d)) {
        if (civ_alive(ctx, x_civ) && chance_pm(ctx.draw(keys_[TAKE], word(e), word(a), 0), take)) {
            Emit x{"artifact_taken", ctx.event_region(e), t,
                   {artifact_role(a), civ_role("taker", x_civ), civ_role("from", d)}, {sack ? 2 : 3}, 0};
            const std::int64_t ev = emit(ctx, x, e);
            move(a, ctx.event_year(ev), ev, holder(KEPT, CIV_TYPE, x_civ));
        } else if (sack && ruined(ctx.state().settlement(t))) {
            Emit x{"artifact_lost", ctx.state().settlement(t).region, t,
                   {artifact_role(a), civ_role("civilization", d)}, {}, 0};
            const std::int64_t ev = emit(ctx, x, e);
            move(a, ctx.event_year(ev), ev, placed(LOST, ruin_place(ctx, t)));
        }
    }
}

// M12: the treasure of a realm that ends with no living conqueror.
void ArtifactsSubsystem::lose_with_realm(StepContext& ctx, std::int64_t e, std::int64_t c, std::int64_t a) {
    const std::int64_t t = seat_of(c);
    if (t != 0) {
        const Settlement& s = ctx.state().settlement(t);
        const std::int64_t owner = s.owner_civ;
        if (!ruined(s) && owner != 0 && owner != c && civ_alive(ctx, owner)) {
            Emit x{"artifact_taken", ctx.event_region(e), ctx.event_settlement(e),
                   {artifact_role(a), civ_role("taker", owner), civ_role("from", c)}, {4}, 0};
            const std::int64_t ev = emit(ctx, x, e);
            move(a, ctx.event_year(ev), ev, holder(KEPT, CIV_TYPE, owner));
            return;
        }
        // Reading 7: a seat that stands with no living other owner keeps it as a ruin would.
        Emit x{"artifact_lost", s.region, t, {artifact_role(a), civ_role("civilization", c)}, {}, 0};
        const std::int64_t ev = emit(ctx, x, e);
        move(a, ctx.event_year(ev), ev, placed(LOST, ruin_place(ctx, t)));
        return;
    }
    const std::int64_t home = ctx.state().civ(c).home_region;
    Emit x{"artifact_lost", home, 0, {artifact_role(a), civ_role("civilization", c)}, {}, 0};
    const std::int64_t ev = emit(ctx, x, e);
    move(a, ctx.event_year(ev), ev, placed(LOST, field_place(home)));
}

// M11 (conquest, k the conqueror) and M12 (k = 0).
void ArtifactsSubsystem::on_realm_end(StepContext& ctx, std::int64_t e, std::int64_t c, std::int64_t k) {
    if (c == 0) {
        return;
    }
    for (const std::int64_t a : held_by(KEPT, c)) {
        if (k != 0 && civ_alive(ctx, k)) {
            Emit x{"artifact_taken", ctx.event_region(e), ctx.event_settlement(e),
                   {artifact_role(a), civ_role("taker", k), civ_role("from", c)}, {4}, 0};
            const std::int64_t ev = emit(ctx, x, e);
            move(a, ctx.event_year(ev), ev, holder(KEPT, CIV_TYPE, k));
        } else {
            lose_with_realm(ctx, e, c, a);
        }
    }
}

// M13: a won war brings home the winner's treasure held by the loser.
void ArtifactsSubsystem::on_war_end(StepContext& ctx, std::int64_t e) {
    const std::int64_t w = ctx.event_participant(e, "winner");
    const std::int64_t l = ctx.event_participant(e, "loser");
    if (w == 0 || l == 0 || !civ_alive(ctx, w)) {
        return;
    }
    const auto n = static_cast<std::int64_t>(artifacts_.size());
    for (std::int64_t a = 1; a <= n; ++a) {
        if (artifact(a).civ != w) {
            continue;
        }
        const Holding& h = open_holding(a);
        const bool held = (h.state == KEPT && h.holder == l) ||
                          (h.state == BORNE && figure_alive(h.holder) && figure_civ(h.holder) == l);
        if (!held) {
            continue;
        }
        Emit x{"artifact_recovered", civ_region(ctx, w), 0,
               {artifact_role(a), civ_role("civilization", w), civ_role("from", l)}, {2}, 0};
        const std::int64_t ev = emit(ctx, x, e);
        move(a, ctx.event_year(ev), ev, holder(KEPT, CIV_TYPE, w));
    }
}

// M14: a cult's temple ended by its suppression; its relics go to the
// sanctum, else to the realm.
void ArtifactsSubsystem::on_cult_suppressed(StepContext& ctx, std::int64_t e) {
    if (sites_ == nullptr) {
        return;
    }
    const std::int64_t c = ctx.event_participant(e, "civilization");
    const std::int64_t sanctum = ctx.event_participant(e, "sanctum");
    const auto n = static_cast<std::int64_t>(artifacts_.size());
    for (std::int64_t a = 1; a <= n; ++a) {
        const Holding& h = open_holding(a);
        if (h.state != ENSHRINED) {
            continue;
        }
        const std::optional<MonstersSubsystem::SiteBrief> b = sites_->site_brief(h.site);
        if (!b || b->kind != SITE_TEMPLE || b->ending != ENDING_SUPPRESSED || b->ending_event != e) {
            continue;
        }
        const Holding to = sanctum != 0 ? placed(ENSHRINED, site_place(sanctum, true)) : holder(KEPT, CIV_TYPE, c);
        move(a, ctx.event_year(e), e, to);
        stats_.quiet_moves += 1;
    }
}

}  // namespace worldgen::history
