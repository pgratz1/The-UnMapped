#pragma once

// The const reads the other history subsystems make of the figures (spec
// docs/specs/p5-06-figures.md 7.1). Declared apart from figures.hpp so that
// conflict, peoples, monsters and alliances can hold a FigureView without
// including the subsystem.

#include <cstdint>

namespace worldgen::history {

// Valid from construction: with nothing named every factor is neutral. With
// no view set, every reader is as at 0.26.0.
class FigureView {
public:
    virtual ~FigureView() = default;
    // 7.2: the strain factor of civ's current reign, normalized by its race
    // (1000 with no reign).
    [[nodiscard]] virtual std::int64_t strain_permille(std::int64_t civ) const = 0;
    // 7.3: the factor on strain toward an ally (1000 with no reign).
    [[nodiscard]] virtual std::int64_t allied_strain_permille(std::int64_t civ) const = 0;
    // 7.3: added to the grant of a share of spoils (0 with no reign; may be negative).
    [[nodiscard]] virtual std::int64_t spoils_permille(std::int64_t civ) const = 0;
    // P5-04a 2: the temper code of civ's current reign (0 steady, 1 warlike,
    // 2 peaceable, 3 grasping, 4 just); -1 with no reign.
    [[nodiscard]] virtual std::int64_t temper(std::int64_t civ) const {
        (void)civ;
        return -1;
    }
    // 7.4: added to split_permille at a crisis of civ in year `year` (P5-04a
    // 11 item 8, P5-06 audit A3: the crisis year, ctx.year() of peoples' step).
    [[nodiscard]] virtual std::int64_t split_bonus_permille(std::int64_t civ, std::int64_t year) const = 0;
    // 7.6: the serving general of civ in the war of declaration event `war`;
    // figure 0: none (skill 0).
    struct General {
        std::int64_t figure = 0, skill = 0;
    };
    [[nodiscard]] virtual General general_of(std::int64_t war, std::int64_t civ) const = 0;
    // 7.7: civ's living hero; figure 0: none (every term 0). fall_permille
    // is the chance that a failed hunt kills the hero (engineer's reading:
    // the tuning hero_fall_permille, which monsters applies).
    struct Hero {
        std::int64_t figure = 0, hunt_permille = 0, might_permille = 0, fall_permille = 0;
    };
    [[nodiscard]] virtual Hero hero_of(std::int64_t civ) const = 0;
    // 8.5: the stage's write check.
    [[nodiscard]] virtual bool figure_exists(std::int64_t figure) const = 0;
    // P5-08 7.5: civ's living champion (role 6), living caster and reigning
    // recorded ruler; 0 none. Defaults for views that keep no figures.
    [[nodiscard]] virtual std::int64_t champion_of(std::int64_t civ) const {
        (void)civ;
        return 0;
    }
    [[nodiscard]] virtual std::int64_t caster_of(std::int64_t civ) const {
        (void)civ;
        return 0;
    }
    [[nodiscard]] virtual std::int64_t ruler_figure(std::int64_t civ) const {
        (void)civ;
        return 0;
    }
    // P5-11 7.3: a figure's role, civilization, whether it lives (status 1
    // or 2) and its place of death (all 0 for an unknown id); the open kind 4
    // site whose master is the caster, or whose last master it was when it
    // has died (0 none). Defaults for views that keep no figures.
    struct Brief {
        std::int64_t role = 0, civ = 0;
        bool alive = false;
        std::int64_t death_region = 0, death_settlement = 0;
    };
    [[nodiscard]] virtual Brief brief(std::int64_t figure) const {
        (void)figure;
        return {};
    }
    [[nodiscard]] virtual std::int64_t caster_site(std::int64_t figure) const {
        (void)figure;
        return 0;
    }
};

}  // namespace worldgen::history
