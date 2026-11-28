#pragma once

// Artifacts (spec docs/specs/p5-11-artifacts.md): a few dozen named works at
// most per world, each made in answer to an occasion of conflict or of a god
// (6.3), by a smith at a forge, a caster at its tower or a god (6.4), and
// moved from its making through a chain of holdings read from the log (6.7):
// kept, borne, hoarded, lost, buried, enshrined. Artifacts are never
// destroyed.
//
// ArtifactsSubsystem runs in T3, T4, T6 and Q4, last of the subsystems (6.1);
// it is the ArtifactView of conflict and monsters (7.1). Keyed draws only
// (the streams hist.artifacts.* and hist.names.artifact); integer arithmetic
// only; members hold ids. It changes State not at all: it founds nothing,
// opens no relation and changes no population, status or owner; it creates
// figures only through FiguresSubsystem::raise_maker (6.10).

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/engine.hpp"
#include "worldgen/history/names.hpp"

namespace worldgen::history {

class AllianceView;
class DivineView;
class FiguresSubsystem;
class FigureView;
class MonstersSubsystem;
class PeoplesSubsystem;

// 7.1: what conflict and monsters read of the artifacts. Unset (tests, the
// I0 build): every read 0.
class ArtifactView {
public:
    virtual ~ArtifactView() = default;
    // [0, force_max_permille]: the battle bonus of the artifacts borne by figure.
    [[nodiscard]] virtual std::int64_t battle_permille(std::int64_t figure) const = 0;
    // The artifact_made of the smallest-id contributor of battle_permille; 0 none.
    [[nodiscard]] virtual std::int64_t battle_artifact(std::int64_t figure) const = 0;
    // [0, hunt_max_permille].
    [[nodiscard]] virtual std::int64_t hunt_permille(std::int64_t figure) const = 0;
    // [0, claim_max_permille]: the strain of realm `home` toward realm
    // `holder` for its artifacts that holder keeps or its living figures bear.
    [[nodiscard]] virtual std::int64_t claim_permille(std::int64_t home, std::int64_t who) const = 0;
    // The largest holding event among them; 0 when no claim.
    [[nodiscard]] virtual std::int64_t claim_event(std::int64_t home, std::int64_t who) const = 0;
    // hoard_hunt_permille when monster hoards an artifact of civ (its home, or
    // kept by civ or borne by a figure of civ before the hoard), else 0.
    [[nodiscard]] virtual std::int64_t hoard_motive_permille(std::int64_t monster, std::int64_t civ) const = 0;
    // r3 7.8: the figure's bearer_corrupted when it is a living general that
    // bears its corrupting artifact now, else 0 (heroes: 0).
    [[nodiscard]] virtual std::int64_t bearer_corruption(std::int64_t figure) const {
        (void)figure;
        return 0;
    }
    // r3 7.8: corrupt_sack_permille, the sack term of a corrupted general
    // (read only when bearer_corruption is not 0).
    [[nodiscard]] virtual std::int64_t corrupt_sack_permille() const { return 0; }
};

// The bound of 5 (H2).
inline constexpr std::int64_t MAX_ARTIFACTS = 100000;

// Classes (4.1), codes 1 to 5 in this order.
inline constexpr std::size_t ARTIFACT_CLASS_COUNT = 5;
enum ArtifactClass : std::int64_t { ARMS = 1, ARMOUR = 2, REGALIA = 3, RELIC = 4, STAFF = 5 };

// A definition of kind artifact (4.1), in canonical id order (kind id =
// index + 1).
struct ArtifactKindDef {
    std::string key, title, noun;
    std::int64_t artifact_class = ARMS;
    std::int64_t weight = 1;
    std::int64_t battle_permille = 0, hunt_permille = 0, claim_permille = 0;

    friend bool operator==(const ArtifactKindDef&, const ArtifactKindDef&) = default;
};

// 4.4: an occasion. bearer: 0 realm, 1 general, 2 figure, 3 site; classes in
// the order arms, armour, regalia, relic, staff; (r4 4.4a) god_camp: -1 any,
// else the camp code 0 good, 1 neutral, 2 evil.
struct ArtifactOccasion {
    std::string kind, role;
    std::int64_t chance_permille = 0;
    std::int64_t bearer = 0;
    std::array<std::int64_t, ARTIFACT_CLASS_COUNT> classes{};
    std::int64_t god_camp = -1;

    friend bool operator==(const ArtifactOccasion&, const ArtifactOccasion&) = default;
};

// Definition history.artifacts (4.3), field for field. The defaults are the
// code defaults: no occasion (inert), the others their starts.
struct ArtifactsTuning {
    std::vector<ArtifactOccasion> occasions;
    std::int64_t forge_chance_permille = 2000, realm_rest_years = 50, world_max = 40;
    std::int64_t deep_metal_depth = 3, deep_metal_permille = 30;
    std::int64_t force_max_permille = 150, hunt_max_permille = 200, claim_max_permille = 1000;
    std::int64_t burial_permille = 300, battle_take_permille = 500, sack_take_permille = 700;
    std::int64_t capture_take_permille = 500, raid_take_permille = 30, hoard_hunt_permille = 50;
    // r2 4.3 (6.11): the period, the chance, its factor by the keeping
    // realm's camp (good, neutral, evil) and the warlike share.
    std::int64_t corrupt_years = 25, corrupt_permille = 150;
    std::array<std::int64_t, 3> corrupt_camp_permille{1000, 1000, 600};
    std::int64_t corrupt_warlike_permille = 500;
    // r3 4.3 (6.12): the bearer's period and chance, and the sack term of a
    // corrupted general (7.8).
    std::int64_t corrupt_bearer_years = 10, corrupt_bearer_permille = 300, corrupt_sack_permille = 500;

    friend bool operator==(const ArtifactsTuning&, const ArtifactsTuning&) = default;
};

// The loaded definition; FatalError "history: tuning: the content has no
// definition history.artifacts" when it is absent.
[[nodiscard]] ArtifactsTuning artifacts_tuning_from(const data::ContentSet& content);
// The artifact definitions in canonical id order (none: empty).
[[nodiscard]] std::vector<ArtifactKindDef> artifact_kind_defs_from(const data::ContentSet& content);

// Counts for the calibration sweep: not stored in the world.
struct ArtifactsStats {
    std::int64_t made = 0, smiths = 0, commissioned = 0, casters = 0, gods = 0, deep = 0;
    std::array<std::int64_t, ARTIFACT_CLASS_COUNT> by_class{};
    std::int64_t taken = 0, lost = 0, hoarded = 0, recovered = 0, buried = 0, quiet_moves = 0;
    // r2 6.11: events 117 (and those warlike), corruptions lifted by a move
    // of the artifact, periods evaluated.
    std::int64_t corrupted = 0, corrupted_warlike = 0, lifted = 0, periods = 0;
    // r3 6.12: events 118 by role, bearer periods evaluated, M15 take-ups;
    // r4 6.13: events 117 by faith, faith periods evaluated.
    std::int64_t generals_corrupted = 0, heroes_outlawed = 0, bearer_periods = 0, taken_up_by_heroes = 0;
    std::int64_t corrupted_by_faith = 0, faith_periods = 0;
};

class ArtifactsSubsystem final : public Subsystem, public ArtifactView {
public:
    // The shipped configuration: the tuning and the artifact definitions of
    // `content`.
    ArtifactsSubsystem(PeoplesSubsystem& peoples, const data::ContentSet& content);
    // A test seam: a hand-made tuning and kinds.
    ArtifactsSubsystem(PeoplesSubsystem& peoples, ArtifactsTuning tuning, std::vector<ArtifactKindDef> kinds);
    ArtifactsSubsystem(const ArtifactsSubsystem&) = delete;
    ArtifactsSubsystem& operator=(const ArtifactsSubsystem&) = delete;

    // 6.10 item 3: set once by the stage. Without the figures no smith,
    // caster or bearer exists (every making by a smith is skipped); without
    // the alliances no forge; without the sites no lair, temple or tower.
    void set_figures(FiguresSubsystem* figures) noexcept { figures_ = figures; }
    void set_alliances(const AllianceView* alliances) noexcept { alliances_ = alliances; }
    void set_sites(const MonstersSubsystem* monsters) noexcept { sites_ = monsters; }
    // r4 7.9: the pantheon's faith reads; without it no faith exposure.
    void set_divine(const DivineView* divine) noexcept { divine_ = divine; }

    [[nodiscard]] std::span<const Step> steps() const override;  // T3, T4, T6, Q4
    void run(StepContext& context) override;

    // 7.1 (ArtifactView).
    [[nodiscard]] std::int64_t battle_permille(std::int64_t figure) const override;
    [[nodiscard]] std::int64_t battle_artifact(std::int64_t figure) const override;
    [[nodiscard]] std::int64_t hunt_permille(std::int64_t figure) const override;
    [[nodiscard]] std::int64_t claim_permille(std::int64_t home, std::int64_t who) const override;
    [[nodiscard]] std::int64_t claim_event(std::int64_t home, std::int64_t who) const override;
    [[nodiscard]] std::int64_t hoard_motive_permille(std::int64_t monster, std::int64_t civ) const override;
    [[nodiscard]] std::int64_t bearer_corruption(std::int64_t figure) const override;
    [[nodiscard]] std::int64_t corrupt_sack_permille() const override { return tuning_.corrupt_sack_permille; }

    // The rows of tables artifact_kind, artifact and artifact_holding (9).
    [[nodiscard]] std::vector<db::ArtifactKindRow> artifact_kind_rows() const;
    [[nodiscard]] std::vector<db::ArtifactRow> artifact_rows() const;
    [[nodiscard]] std::vector<db::ArtifactHoldingRow> artifact_holding_rows() const;  // (artifact, seq)

    // Inspection (tests, the calibration sweep).
    [[nodiscard]] const ArtifactsTuning& tuning() const noexcept { return tuning_; }
    [[nodiscard]] const ArtifactsStats& stats() const noexcept { return stats_; }

private:
    enum HoldState : std::int64_t { KEPT = 1, BORNE = 2, HOARDED = 3, LOST = 4, BURIED = 5, ENSHRINED = 6 };
    struct Holding {
        std::int64_t from_year = 0;
        std::optional<std::int64_t> to_year;
        std::int64_t state = KEPT, holder_type = 0, holder = 0;
        std::int64_t region = 0, settlement = 0, site = 0, x = 0, y = 0;
        std::int64_t event = 0, until_event = 0;
        // r2 5, 6.11: the periods of a kept holding evaluated so far (0 at its
        // opening); used for holdings of state 1 of an evil artifact, and
        // (r3 6.12) of state 2 of an evil artifact borne by a general or hero.
        std::int64_t periods = 0;
    };
    struct Artifact {
        std::int64_t id = 0;
        std::string name;
        std::int64_t kind = 0, civ = 0, maker_kind = 1, maker = 0, god = 0, maker_civ = 0, maker_site = 0;
        std::int64_t made_year = 0, made_event = 0, occasion_event = 0, deep = 0;
        std::vector<Holding> holdings;  // seq order; the last is open
    };
    struct CultureData {
        std::int64_t id = 0;
        std::string key;
        CultureNaming naming;
    };
    // The place columns of a holding of state 4 to 6 (6.2).
    struct Place {
        std::int64_t holder_type = 0, holder = 0, region = 0, settlement = 0, site = 0, x = 0, y = 0;
    };
    // The participants of an artifact event to write, its kind and place.
    struct Emit {
        std::string_view kind;
        std::int64_t region = 0, settlement = 0;
        std::vector<Participant> participants;
        std::vector<std::int64_t> values;
        std::int64_t cause = 0;
    };

    void prepare(const StepContext& context);
    [[nodiscard]] const CultureData& culture(std::int64_t culture_id) const;
    [[nodiscard]] Artifact& artifact(std::int64_t id);
    [[nodiscard]] const Artifact& artifact(std::int64_t id) const;
    [[nodiscard]] const ArtifactKindDef& kind_of(const Artifact& a) const;
    [[nodiscard]] bool alive_visible(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] bool civ_alive(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] std::int64_t seat_of(std::int64_t civ) const;
    void refresh_seats(const StepContext& context);

    // 6.7, 6.3.
    void read_log(StepContext& context);
    void moves(StepContext& context, std::int64_t event, std::string_view kind);
    void making(StepContext& context, std::int64_t event, std::string_view kind);
    [[nodiscard]] bool try_make(StepContext& context, std::int64_t event, const ArtifactOccasion& o, std::int64_t c);

    // 6.4: the forge of c (pure); a role's first participant of a type.
    [[nodiscard]] std::int64_t forge(std::int64_t civ) const;
    [[nodiscard]] std::int64_t forge_site(std::int64_t civ) const;
    [[nodiscard]] static std::int64_t first_of_type(const StepContext& context, std::int64_t event,
                                                    std::string_view kind, EntityType type);
    [[nodiscard]] bool open_site_of_kind(std::int64_t site, std::int64_t kind) const;
    [[nodiscard]] std::int64_t caster_site(std::int64_t caster) const;
    [[nodiscard]] std::string draw_name(const StepContext& context, std::int64_t culture_id, std::int64_t id,
                                        const ArtifactKindDef& kind);

    // Holdings (6.2).
    [[nodiscard]] const Holding& open_holding(std::int64_t id) const;
    [[nodiscard]] std::vector<std::int64_t> held_by(std::int64_t state, std::int64_t who) const;
    [[nodiscard]] bool bears_class(std::int64_t figure, std::int64_t artifact_class) const;
    [[nodiscard]] bool figure_alive(std::int64_t figure) const;
    [[nodiscard]] std::int64_t figure_civ(std::int64_t figure) const;
    void open(std::int64_t id, std::int64_t year, Holding h);
    void move(std::int64_t id, std::int64_t year, std::int64_t event, Holding h);
    [[nodiscard]] static Holding holder(std::int64_t state, std::int64_t type, std::int64_t id);
    [[nodiscard]] static Holding placed(std::int64_t state, const Place& p);
    [[nodiscard]] Place field_place(std::int64_t region) const;
    [[nodiscard]] Place ruin_place(const StepContext& context, std::int64_t settlement) const;
    [[nodiscard]] Place site_place(std::int64_t site, bool with_settlement) const;
    [[nodiscard]] Place grave_place(const StepContext& context, std::int64_t figure) const;
    // Writes the artifact event `e` (causes: the triggering event) and
    // returns its id; in Q4 without budget for its first civilization, writes
    // nothing and returns `trigger` (a quiet move, 6.8).
    std::int64_t emit(StepContext& context, const Emit& e, std::int64_t trigger);
    [[nodiscard]] std::int64_t civ_region(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] std::int64_t monster_lair(std::int64_t monster) const;

    // The rows of 6.7.
    void on_general(StepContext& context, std::int64_t e);
    void on_figure_died(StepContext& context, std::int64_t e);
    void on_fell(StepContext& context, std::int64_t e);
    void on_hero_slain(StepContext& context, std::int64_t e);
    void on_monster_slain(StepContext& context, std::int64_t e, bool by_hero);
    void on_monster_died(StepContext& context, std::int64_t e);
    void on_raid(StepContext& context, std::int64_t e);
    void on_hold_lost(StepContext& context, std::int64_t e);
    void on_sacked(StepContext& context, std::int64_t e, bool sack);
    void on_realm_end(StepContext& context, std::int64_t e, std::int64_t c, std::int64_t k);
    void on_war_end(StepContext& context, std::int64_t e);
    void on_cult_suppressed(StepContext& context, std::int64_t e);
    void on_hero_rose(StepContext& context, std::int64_t e);  // r3 M15
    void lose_with_realm(StepContext& context, std::int64_t e, std::int64_t c, std::int64_t a);

    // r2 6.11 (artifacts_corrupt.cpp): the corruption pass at the end of a
    // step, its lifting part, its draws, and whether an artifact is evil.
    void corruption(StepContext& context);
    void lift_corruptions(const StepContext& context);
    void corruption_draws(StepContext& context);
    // Returns false when c has no quiet-step budget left (its periods wait).
    bool corrupt_periods(StepContext& context, std::int64_t civ, std::int64_t artifact);
    // r4 6.13: likewise for the faith exposure of c to the enshrined a.
    bool faith_periods(StepContext& context, std::int64_t civ, std::int64_t artifact);
    void corrupt(StepContext& context, std::int64_t civ, std::int64_t artifact, std::int64_t temper,
                 std::int64_t faith_event);
    [[nodiscard]] bool evil(const Artifact& a) const;
    [[nodiscard]] std::int64_t god_camp(std::int64_t god) const;  // -1 unknown
    // r4 6.13: the faith exposure of c to the evil artifact a (enshrined at an
    // open temple of c's faith whose god is a's): the faith's cult_adopted,
    // else 0; `since` gets the faith's year.
    [[nodiscard]] std::int64_t faith_exposure(std::int64_t civ, std::int64_t artifact, std::int64_t* since) const;
    // r2 6.11 and r4 6.13: whether a exposes c now (kept, or by faith).
    [[nodiscard]] bool exposes(std::int64_t civ, std::int64_t artifact) const;
    // r3 6.12 (artifacts_corrupt.cpp): the bearers' draws.
    void bearer_draws(StepContext& context);
    void bearer_periods(StepContext& context, std::int64_t artifact);
    void corrupt_bearer(StepContext& context, std::int64_t figure, std::int64_t civ, std::int64_t artifact,
                        std::int64_t role);
    [[nodiscard]] std::int64_t civ_camp(const StepContext& context, std::int64_t civ) const;

    PeoplesSubsystem& peoples_;
    FiguresSubsystem* figures_ = nullptr;
    const AllianceView* alliances_ = nullptr;
    const MonstersSubsystem* sites_ = nullptr;
    const DivineView* divine_ = nullptr;
    ArtifactsTuning tuning_;
    std::vector<ArtifactKindDef> kinds_;  // index kind id - 1
    bool prepared_ = false;
    std::array<std::uint64_t, 8> keys_{};
    std::vector<std::pair<std::int64_t, std::int64_t>> god_camps_;   // r2 6.11: (god id, camp), ascending
    std::vector<std::pair<std::int64_t, std::int64_t>> race_camps_;  // (race id, camp), ascending
    std::vector<CultureData> cultures_;  // ascending culture id
    std::vector<std::pair<std::int64_t, std::int64_t>> region_seeds_;  // per region (index id - 1): (seed_x, seed_y)
    std::vector<Artifact> artifacts_;     // index id - 1
    // Per civilization (index id - 1): the year of its last making (none:
    // nullopt), its seat (6.2) and whether it has ended (as at the last
    // refresh, for the view).
    std::vector<std::optional<std::int64_t>> last_made_;
    std::vector<std::int64_t> seat_;
    std::vector<char> ended_;
    // The holder indexes (5): kept by civilization, borne by figure, hoarded
    // by monster, each holder id to its artifact ids ascending.
    std::map<std::int64_t, std::vector<std::int64_t>> kept_, borne_, hoarded_;
    // r4 6.13: the faith exposures, (artifact, civ) to (origin year, periods).
    std::map<std::pair<std::int64_t, std::int64_t>, std::pair<std::int64_t, std::int64_t>> exposures_;
    // r3 5: per corrupted figure, its bearer_corrupted and the artifact.
    std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> bearer_corrupt_;
    std::int64_t cursor_ = 1;
    ArtifactsStats stats_;
};

}  // namespace worldgen::history
