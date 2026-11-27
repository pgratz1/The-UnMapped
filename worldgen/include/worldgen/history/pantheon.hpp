#pragma once

// The pantheon, divine interventions and cults (spec
// docs/specs/p5-08-pantheon-cults.md): each god's regard for each realm from
// the deeds its domain cares about, the gods' acts (bless, curse, champion,
// monster, mark) in T5 of tick years, the gods' flare-up of a quiet age, and
// the cults that grow from an act (founding, spread, faith, suppression,
// hiding, endings).
//
// PantheonSubsystem runs in T4 and Q4 after FiguresSubsystem (6.12 item 3);
// it is the engine's intervention source and applier, the third tension
// source, and the DivineView of peoples and conflict (7.1). Keyed draws only
// (the streams hist.pantheon.*); integer arithmetic only; members hold ids.
// It changes State only through set_population and
// PeoplesSubsystem::note_reduction (a pestilence), open_relation and
// close_relation (kinds cult_in, faith_of), and the seams of the monsters
// (sites, sent monsters) and the figures (champions, prophets).

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/engine.hpp"
#include "worldgen/stages/regions.hpp"

namespace worldgen::history {

class AllianceView;
class ConflictSubsystem;
class FiguresSubsystem;
class MonstersSubsystem;
class PeoplesSubsystem;

// 7.1: what peoples and conflict read of the gods' acts. Unset: 0, 0, false.
class DivineView {
public:
    virtual ~DivineView() = default;
    // [-900, 1000]: the effects of c in force at `year` and a sacred shrine
    // at `settlement`.
    [[nodiscard]] virtual std::int64_t growth_permille(std::int64_t civ, std::int64_t settlement,
                                                       std::int64_t year) const = 0;
    // [-500, 500].
    [[nodiscard]] virtual std::int64_t force_permille(std::int64_t civ, std::int64_t year) const = 0;
    // A profane shrine's region in its years.
    [[nodiscard]] virtual bool shunned(std::int64_t region, std::int64_t year) const = 0;
};

// The bound of 5 (H2).
inline constexpr std::int64_t MAX_FACTIONS = 100000;

// 4.3: one deed. who: 0 kin, 1 any, 2 foe.
struct GodDeed {
    std::string kind, role;
    std::int64_t points = 0;
    std::int64_t who = 1;
    std::string against;

    friend bool operator==(const GodDeed&, const GodDeed&) = default;
};

// The fields of 4.1 from favour on (a god definition, or the tuning's
// creator record). bless: 0 plenty, 1 valour; curse: 0 pestilence, 1
// withering, 2 defeat.
struct GodActs {
    std::array<std::int64_t, 3> favour{500, 300, 200};  // bless, champion, mark
    std::array<std::int64_t, 3> wrath{700, 0, 300};     // curse, monster, mark
    std::int64_t bless = 0;
    std::int64_t curse = 0;
    std::vector<std::string> monsters;  // monster definition ids
    std::vector<GodDeed> deeds;

    friend bool operator==(const GodActs&, const GodActs&) = default;
};

// A god definition (4.1), in canonical id order.
struct PantheonGodDef {
    std::string key, name_culture;
    std::int64_t camp = 1;    // 0 good, 1 neutral, 2 evil
    std::int64_t domain = 2;  // 2 war ... 9 ruin
    std::string title;
    GodActs acts;

    friend bool operator==(const PantheonGodDef&, const PantheonGodDef&) = default;
};

// Definition history.pantheon (4.2), field for field. The defaults are the
// code defaults: inert (no act, no cult, no flare).
struct PantheonTuning {
    std::array<std::int64_t, 3> activity_permille{300, 1000, 2500};  // distant, normal, meddling
    std::int64_t regard_max = 400, regard_fade_per_century = 20, regard_memory_years = 300;
    std::int64_t contributions_max = 8, act_threshold = 100, act_permille = 0;
    std::int64_t god_rest_years = 30, pair_rest_years = 100;
    std::int64_t flare_regard = 200, flare_permille = 0;
    std::int64_t bless_years = 50, curse_years = 50;
    std::int64_t plenty_growth_permille = 300, withering_growth_permille = 500;
    std::int64_t valour_permille = 50, defeat_permille = 60, pestilence_toll_permille = 120;
    std::int64_t send_hops = 2, sacred_growth_permille = 200, profane_years = 300;
    std::string champion_epithet = "Chosen of {god}";
    std::int64_t cult_found_permille = 0, cult_fear_permille = 500, cult_spread_permille = 20;
    std::array<std::int64_t, 3> affinity_permille{1000, 500, 100};  // same, near, opposed
    std::int64_t adopt_regard = 50, adopt_permille = 15, suppress_permille = 10;
    std::array<std::int64_t, 5> temper_adopt_permille{1000, 500, 1500, 700, 1000};
    std::array<std::int64_t, 5> temper_suppress_permille{1000, 1500, 500, 1000, 1300};
    std::int64_t faith_points = 60, rival_faith_points = 40, suppress_points = 120;
    std::int64_t hide_permille = 400, sanctum_hops = 3;
    GodActs creator;

    friend bool operator==(const PantheonTuning&, const PantheonTuning&) = default;
};

// The loaded definition; FatalError "history: tuning: the content has no
// definition history.pantheon" when it is absent.
[[nodiscard]] PantheonTuning pantheon_tuning_from(const data::ContentSet& content);
// The god definitions in canonical id order (none: empty).
[[nodiscard]] std::vector<PantheonGodDef> pantheon_god_defs_from(const data::ContentSet& content);

// Counts for the calibration sweep: not stored in the world.
struct PantheonStats {
    std::array<std::int64_t, 5> acts{};  // bless, curse, champion, monster, mark
    std::int64_t flares = 0, flares_acted = 0, dead = 0;
    std::int64_t cults_founded = 0, spreads = 0, adopted = 0, suppressed = 0, hidden = 0, ended = 0;
    std::int64_t prophets = 0;
};

class PantheonSubsystem final : public Subsystem,
                                public TensionSource,
                                public InterventionSource,
                                public InterventionApplier,
                                public DivineView {
public:
    // The shipped configuration: the tuning and the god definitions of
    // `content`.
    PantheonSubsystem(PeoplesSubsystem& peoples, const data::ContentSet& content);
    // A test seam: a hand-made tuning and god definitions.
    PantheonSubsystem(PeoplesSubsystem& peoples, PantheonTuning tuning, std::vector<PantheonGodDef> gods);
    PantheonSubsystem(const PantheonSubsystem&) = delete;
    PantheonSubsystem& operator=(const PantheonSubsystem&) = delete;

    // 6.12 item 3: set once by the stage. Without the sites no monster is
    // sent and no shrine, temple or sanctum is opened (no mark, no cult);
    // without the figures no champion and no founder.
    void set_sites(MonstersSubsystem* monsters) noexcept { sites_ = monsters; }
    void set_figures(FiguresSubsystem* figures) noexcept { figures_ = figures; }
    void set_alliances(const AllianceView* alliances) noexcept { alliances_ = alliances; }
    // 6.3 champion eligibility: open wars (nullptr: no war counts).
    void set_conflict(const ConflictSubsystem* conflict) noexcept { conflict_ = conflict; }

    [[nodiscard]] std::span<const Step> steps() const override;  // T4, Q4
    void run(StepContext& context) override;
    // 6.6 (pure).
    TensionReport tension(const StepContext& context) override;
    // 6.3, the source (pure).
    std::vector<Intervention> interventions(const StepContext& context) override;
    // 6.3, 6.4, the applier.
    void apply(StepContext& context, const Intervention& intervention) override;

    // 7.1.
    [[nodiscard]] std::int64_t growth_permille(std::int64_t civ, std::int64_t settlement,
                                               std::int64_t year) const override;
    [[nodiscard]] std::int64_t force_permille(std::int64_t civ, std::int64_t year) const override;
    [[nodiscard]] bool shunned(std::int64_t region, std::int64_t year) const override;

    // The rows of table faction (9), ascending id.
    [[nodiscard]] std::vector<db::FactionRow> faction_rows() const;
    // 8.1: the stage's write check.
    [[nodiscard]] bool faction_exists(std::int64_t faction) const noexcept;

    // Inspection (tests, the calibration sweep).
    [[nodiscard]] const PantheonTuning& tuning() const noexcept { return tuning_; }
    [[nodiscard]] const PantheonStats& stats() const noexcept { return stats_; }
    // R(g, c), 0 when the pair has no entry.
    [[nodiscard]] std::int64_t regard(std::int64_t god, std::int64_t civ) const;

private:
    enum ActKind : std::size_t { BLESS = 0, CURSE = 1, CHAMPION = 2, MONSTER = 3, MARK = 4 };
    struct Contribution {
        std::int64_t event = 0, year = 0, points = 0;
    };
    struct PairState {
        std::int64_t regard = 0;
        std::vector<Contribution> contributions;  // append order
        std::optional<std::int64_t> last_act;
    };
    struct GodState {
        std::int64_t id = 0, camp = 1, race = 0;  // race: the creator's race, 0 for the pantheon
        std::string name;
        const GodActs* acts = nullptr;
        std::vector<std::size_t> monster_kinds;  // indexes of MonstersSubsystem::kinds(), data order
        std::optional<std::int64_t> last_act;
        std::int64_t acts_count = 0;
        std::int64_t live_cult = 0;  // a cult of status 1 or 2, 0 none
    };
    struct Effect {
        std::int64_t kind = 0;  // 1 plenty, 2 withering, 3 valour, 4 defeat
        std::int64_t from = 0, until = 0, event = 0;
    };
    struct Shrine {
        std::int64_t site = 0, region = 0, settlement = 0, origin = 0;
        bool profane = false;
    };
    struct PendingAct {
        std::int64_t event = 0, god = 0, civ = 0;
        std::size_t kind = BLESS;
        bool favourable = true;
        std::int64_t figure = 0;      // a champion
        std::int64_t settlement = 0;  // a mark at a settlement
    };
    struct Cult {
        std::int64_t id = 0;
        std::string name;
        std::int64_t god = 0, civ = 0, founder = 0, seat = 0, seat_settlement = 0;
        std::int64_t founded_year = 0, founding_event = 0;
        std::int64_t status = 1;  // 1 present, 2 hidden, 3 ended
        std::optional<std::int64_t> ended_year;
        std::int64_t ending_event = 0, ending = 0;
        std::int64_t sanctum = 0;
    };
    struct Temple {
        std::int64_t site = 0, cult = 0, settlement = 0, civ = 0, origin_event = 0;
        bool open = true;
    };
    struct Candidate {
        std::int64_t god = 0, civ = 0, regard = 0;
    };
    struct Place {
        std::int64_t region = 0, x = 0, y = 0, settlement = 0;
    };

    void prepare(const StepContext& context);
    [[nodiscard]] const GodState& god(std::int64_t id) const;
    [[nodiscard]] GodState& god(std::int64_t id);
    [[nodiscard]] std::int64_t race_camp(std::int64_t race) const;
    [[nodiscard]] std::int64_t civ_camp(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] bool kin(const StepContext& context, std::int64_t g, std::int64_t c) const;
    [[nodiscard]] bool foe(const StepContext& context, std::int64_t g, std::int64_t c) const;
    [[nodiscard]] bool alive_visible(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] std::int64_t activity() const noexcept { return activity_; }

    // 6.2.
    void read_log(StepContext& context);
    void add(std::int64_t g, std::int64_t c, std::int64_t points, std::int64_t event, std::int64_t year);
    void fade(const StepContext& context);
    // 6.3, 6.4.
    [[nodiscard]] bool is_candidate(const StepContext& context, std::int64_t g, std::int64_t c, const PairState& p,
                                    std::int64_t min_regard) const;
    [[nodiscard]] std::optional<Candidate> best_candidate(const StepContext& context, std::int64_t min_regard) const;
    [[nodiscard]] std::vector<std::int64_t> ranked_causes(const StepContext& context, const PairState& p) const;
    [[nodiscard]] std::array<std::int64_t, 3> kind_weights(const StepContext& context, std::int64_t g, std::int64_t c,
                                                           bool favourable) const;
    [[nodiscard]] bool eligible(const StepContext& context, std::size_t kind, std::int64_t g, std::int64_t c) const;
    [[nodiscard]] bool champion_work(const StepContext& context, std::int64_t c) const;
    [[nodiscard]] std::int64_t home_region(const StepContext& context, std::int64_t c) const;
    [[nodiscard]] std::int64_t capital_region(const StepContext& context, std::int64_t c) const;
    [[nodiscard]] std::vector<std::size_t> sendable_kinds(std::int64_t g) const;
    [[nodiscard]] std::int64_t lair_region(const StepContext& context, std::int64_t c) const;
    [[nodiscard]] std::optional<Place> mark_place(const StepContext& context, std::int64_t c, std::int64_t lead) const;
    [[nodiscard]] std::int64_t act_region(const StepContext& context, std::size_t kind, std::int64_t g,
                                          std::int64_t c, std::int64_t lead) const;
    void spend(std::int64_t g, std::int64_t c, std::int64_t year);
    // 6.6.
    void flare(const StepContext& context);
    // 6.7 to 6.10 (pantheon_cults.cpp).
    void endings(StepContext& context);
    void temples(StepContext& context);
    void foundings(StepContext& context);
    void spreads(StepContext& context);
    void faiths(StepContext& context);
    void end_cult(StepContext& context, std::int64_t k, std::int64_t ending, std::int64_t event);
    void end_temple(StepContext& context, Temple& t, std::int64_t ending, std::int64_t event);
    [[nodiscard]] bool has_open_temple(std::int64_t settlement) const;
    [[nodiscard]] bool cult_in(std::int64_t k, std::int64_t c) const;
    [[nodiscard]] std::int64_t cult_realms(std::int64_t k) const;
    [[nodiscard]] bool opposed_camps(std::int64_t a, std::int64_t b) const noexcept;
    [[nodiscard]] std::vector<std::int64_t> regions_by_hops(const StepContext& context, std::int64_t from,
                                                            std::int64_t hops) const;
    // 4.4: a name of `field` of culture `culture` with {god} = the god's name.
    [[nodiscard]] std::string place_name(const StepContext& context, std::int64_t culture, std::string_view field,
                                         std::uint64_t stream, std::int64_t id, std::int64_t g);
    [[nodiscard]] std::int64_t temper_of(std::int64_t civ) const;

    PeoplesSubsystem& peoples_;
    MonstersSubsystem* sites_ = nullptr;
    FiguresSubsystem* figures_ = nullptr;
    const AllianceView* alliances_ = nullptr;
    const ConflictSubsystem* conflict_ = nullptr;
    PantheonTuning tuning_;
    std::vector<PantheonGodDef> defs_;
    bool prepared_ = false;
    std::array<std::uint64_t, 11> keys_{};
    std::int64_t activity_ = 1000;  // A
    std::int64_t threshold_ = 0;    // T_d
    std::vector<std::pair<std::int64_t, std::int64_t>> race_camps_;  // (race id, camp), ascending
    std::vector<GodState> gods_;                                     // index id - 1
    // 6.2: (kind key, (god, deed index)) in ascending god, then deed index.
    std::map<std::string, std::vector<std::pair<std::int64_t, std::size_t>>, std::less<>> deeds_by_kind_;
    std::map<std::pair<std::int64_t, std::int64_t>, PairState> pairs_;  // (god, civ)
    std::vector<std::int64_t> drop_next_;  // realms whose ending was read: dropped at the next step
    std::vector<std::vector<Effect>> effects_;  // per civilization (index id - 1)
    std::vector<Shrine> shrines_;               // ascending site
    std::set<std::int64_t> shrine_settlements_;
    std::vector<PendingAct> pending_acts_;      // ascending event
    std::vector<Cult> cults_;                   // index id - 1
    std::vector<Temple> temples_;               // ascending site
    std::map<std::pair<std::int64_t, std::int64_t>, std::int64_t> cult_in_;  // (cult, civ) -> its opening event
    std::map<std::pair<std::int64_t, std::int64_t>, std::int64_t> latest_;   // (cult, civ) -> its latest cult event
    std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> faith_of_;  // civ -> (cult, cult_adopted)
    std::vector<std::pair<std::int64_t, std::int64_t>> ended_realms_;  // (civ, ending event) read this step
    std::int64_t pending_flare_ = 0;
    std::array<std::int64_t, 3> flared_{};  // (god, civ, deed event), 0s none
    std::int64_t phase_seen_ = 0, phase_start_ = 0;
    std::vector<std::pair<std::int64_t, std::string>> culture_keys_;  // (culture id, key), ascending
    stages::PlacenameData placenames_;
    std::int64_t cursor_ = 1;
    PantheonStats stats_;
};

}  // namespace worldgen::history
