#pragma once

// Figures (spec docs/specs/p5-06-figures.md): the ruling line of every
// realm and the rulers the record names, disputed successions and exiles,
// generals named at declarations, heroes raised by monsters' raids, casters
// with their towers, books and blights, regnal numbers and epithets.
//
// FiguresSubsystem runs in steps T3, T4, T6 and Q4 after AlliancesSubsystem
// (6.1). It is the other subsystems' FigureView (7.1). Keyed draws only (the
// streams hist.figures.*, hist.names.person and hist.names.book); integer
// arithmetic only; members hold ids. It changes State only through
// open_relation and close_relation (kinds ruled, served, rival, parent),
// set_population and PeoplesSubsystem::note_reduction (a blight), and the
// site seam of MonstersSubsystem (towers, 7.8).

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/engine.hpp"
#include "worldgen/history/figures_view.hpp"
#include "worldgen/history/names.hpp"
#include "worldgen/stages/regions.hpp"

namespace worldgen::history {

class ConflictSubsystem;
class MonstersSubsystem;
class PeoplesSubsystem;

// The bounds of 5.
inline constexpr std::int64_t MAX_FIGURES = 100000;
inline constexpr std::int64_t MAX_BOOKS = 100000;

// Tempers (9.1 temper codes), in the order of temper_weights.
inline constexpr std::size_t TEMPERS = 5;

// One entry of history.figures.epithets (4.4, 6.10).
struct EpithetRule {
    std::int64_t role = 1;   // 1 ruler ... 5 caster
    std::int64_t deed = 0;   // index into the deed enum of 6.10 (0 founded_years ... 11 blights)
    std::int64_t min = 1;
    std::int64_t substantial = 0;
    std::string text;        // an epithet or {slayer}

    friend bool operator==(const EpithetRule&, const EpithetRule&) = default;
};

// Definition history.figures (4.4), field for field.
struct FiguresTuning {
    bool name_rulers = true;
    std::int64_t reign_min_permille = 500, reign_span_permille = 1000;
    std::int64_t accession_age_permille = 250, accession_span_permille = 250;
    std::array<std::int64_t, TEMPERS> temper_strain_permille{1000, 1300, 700, 1000, 1000};
    std::array<std::int64_t, TEMPERS> temper_allied_permille{1000, 1000, 1000, 2000, 500};
    std::array<std::int64_t, TEMPERS> temper_spoils_permille{0, 0, 0, -200, 200};
    std::int64_t short_reign_permille = 500, short_reign_split_permille = 100;
    std::int64_t exile_permille = 250, exile_hops = 3;
    std::int64_t general_scale_permille = 1000, skill_min = 100, skill_max = 300, career_permille = 300;
    std::int64_t fall_loser_permille = 150, fall_winner_permille = 30;
    std::int64_t general_age_permille = 300, general_age_span_permille = 300;
    std::int64_t hero_age_permille = 150, hero_age_span_permille = 200;
    std::int64_t life_min_permille = 700, life_span_permille = 500;
    std::int64_t hero_scale_permille = 1000, hero_hunt_permille = 30, hero_might_permille = 500;
    std::int64_t hero_fall_permille = 300;
    std::int64_t caster_ppm = 1500, caster_age_permille = 400, caster_age_span_permille = 300;
    std::int64_t book_permille = 10, books_max = 3, blight_ppm = 500, blight_toll_permille = 80;
    std::int64_t regnal_max = 9;
    std::vector<EpithetRule> epithets;
    std::int64_t epithet_repeat_max = 2;
    // P5-04a 14.2a (generator 0.28.0); the defaults keep 0.27.0's meaning.
    std::int64_t tower_spacing_permille = 0, hero_return_years = 100000;

    friend bool operator==(const FiguresTuning&, const FiguresTuning&) = default;
};

// The loaded definition. FatalError "history: tuning: the content has no
// definition history.figures" when it is absent.
[[nodiscard]] FiguresTuning figures_tuning_from(const data::ContentSet& content);

// The deed keys of 6.10 in enum order.
[[nodiscard]] std::span<const std::string_view> figure_deed_keys() noexcept;

// Counts for the calibration sweep: not stored in the world.
struct FiguresStats {
    std::int64_t reigns = 0, named = 0, disputes = 0, exiles = 0, appointments = 0, falls = 0;
    std::int64_t heroes = 0, casters = 0, books = 0, blights = 0, regnal = 0;
};

// A reign of the lines, for the calibration sweep's dumps (FX2): not stored.
struct ReignRecord {
    std::int64_t civ = 0, number = 0, start = 0, end = 0, temper = 0, figure = 0;
};

class FiguresSubsystem final : public Subsystem, public FigureView {
public:
    // The shipped configuration: the tuning and the race, culture and
    // monster fields of `content`.
    FiguresSubsystem(PeoplesSubsystem& peoples, const data::ContentSet& content);
    // A test seam: a hand-made tuning; the data fields are read from the
    // content at the first step.
    FiguresSubsystem(PeoplesSubsystem& peoples, FiguresTuning tuning);
    FiguresSubsystem(const FiguresSubsystem&) = delete;
    FiguresSubsystem& operator=(const FiguresSubsystem&) = delete;

    // 6.13 item 3: set once by the stage. Without the sites no tower rises;
    // without conflict no general is named and no exile has a host.
    void set_sites(MonstersSubsystem* monsters) noexcept { sites_ = monsters; }
    void set_conflict(const ConflictSubsystem* conflict) noexcept { conflict_ = conflict; }

    [[nodiscard]] std::span<const Step> steps() const override;  // T3, T4, T6, Q4
    void run(StepContext& context) override;

    // 7.1 (FigureView).
    [[nodiscard]] std::int64_t strain_permille(std::int64_t civ) const override;
    [[nodiscard]] std::int64_t allied_strain_permille(std::int64_t civ) const override;
    [[nodiscard]] std::int64_t spoils_permille(std::int64_t civ) const override;
    [[nodiscard]] std::int64_t temper(std::int64_t civ) const override;
    [[nodiscard]] std::int64_t split_bonus_permille(std::int64_t civ) const override;
    [[nodiscard]] General general_of(std::int64_t war, std::int64_t civ) const override;
    [[nodiscard]] Hero hero_of(std::int64_t civ) const override;
    [[nodiscard]] bool figure_exists(std::int64_t figure) const override;

    // The rows of tables figure and book (9.1, 9.3), ascending id.
    [[nodiscard]] std::vector<db::FigureRow> figure_rows() const;
    [[nodiscard]] std::vector<db::BookRow> book_rows() const;

    // Inspection (tests, the calibration sweep).
    [[nodiscard]] const FiguresTuning& tuning() const noexcept { return tuning_; }
    [[nodiscard]] const FiguresStats& stats() const noexcept { return stats_; }
    [[nodiscard]] const std::vector<ReignRecord>& reign_log() const noexcept { return reign_log_; }

private:
    struct Tally {
        std::int64_t wars_declared = 0, wars_won = 0, conquests = 0, betrayals = 0;
    };
    struct Figure {
        std::int64_t id = 0;
        std::string name, epithet, root;
        std::int64_t role = 1, race = 0, culture = 0, civ = 0;
        std::int64_t born = 0, office_from = 0, start_event = 0, planned_death = 0;
        std::optional<std::int64_t> died;
        std::int64_t death_event = 0, death_region = 0, death_settlement = 0, status = 1;
        std::int64_t reign_no = 0, regnal = 0, temper = 0, skill = 0;
        // Deeds (6.10).
        Tally tally;
        std::int64_t battles_won = 0, battles_lost = 0, slain = 0, slew_might = 0;
        std::size_t slew_kind = 0;  // the monster kind of slew_might (the last slain at a tie)
        std::int64_t books = 0, blights = 0;
        bool exiled = false, realm_fell = false, regnal_fallback = false;
        // Places and offices.
        std::int64_t host = 0;           // an exile's host (0 the wilds)
        std::int64_t host_region = 0;    // the host's capital region at the exile
        std::int64_t tower = 0;          // a caster's site
        std::int64_t tower_event = 0;    // its tower_founded
        std::int64_t last_book = 0;      // its last book_written
        std::int64_t first_command = 0;  // a general's first general_appointed year
        std::int64_t serving = 0;        // the war (declaration event) a general serves, 0 none
    };
    struct Reign {
        std::int64_t number = 0, start = 0, end = 0, temper = 0, age = 0, figure = 0;
        Tally tally;
    };
    struct Line {
        bool has = false;
        std::int64_t race = 0;
        std::size_t log_index = 0;  // the current reign's ReignRecord
        Reign reign;
        // 6.14: the distinct base names of the recorded rulers with their
        // counts, in the order of their first bearers.
        std::vector<std::pair<std::string, std::int64_t>> names;
    };
    struct KnownWar {
        std::int64_t attacker = 0, defender = 0, goal = 0;
        std::array<std::int64_t, 2> general{};  // per side (0 attacker, 1 defender), 0 none
        std::array<std::int64_t, 2> vacancy{};  // v_c
        std::array<bool, 2> vacant{};           // a general fell; no appointment since
    };
    struct RaceData {
        std::int64_t id = 0, lifespan = 1, generation = 1, awakening = 0;
        std::array<std::int64_t, TEMPERS> temper_weights{};
        std::int64_t mean_strain = 1000;  // 7.2's mean_r
        std::int64_t general = 0, hero = 0, magic = 0;
    };
    struct CultureData {
        std::int64_t id = 0;
        std::string key;
        CultureNaming naming;
    };
    struct BookState {
        std::int64_t id = 0;
        std::string title;
        std::int64_t author = 0, site = 0, year = 0, event = 0;
    };

    void prepare(const StepContext& context);
    [[nodiscard]] const RaceData& race(std::int64_t race_id) const;
    [[nodiscard]] const CultureData& culture(std::int64_t culture_id) const;
    [[nodiscard]] Figure& figure(std::int64_t id);
    [[nodiscard]] const Figure& figure(std::int64_t id) const;
    [[nodiscard]] Line& line(std::int64_t civ);
    [[nodiscard]] const Line* line_if(std::int64_t civ) const;
    [[nodiscard]] bool alive_visible(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] Tally& tally_of(std::int64_t civ);

    // 6.2.
    void read_log(StepContext& context);
    void on_founded(StepContext& context, std::int64_t civ, std::int64_t event);
    void on_war_declared(StepContext& context, std::int64_t event);
    void on_crisis(StepContext& context, std::int64_t event, std::int64_t last);
    void on_realm_end(StepContext& context, std::int64_t civ, std::int64_t event);
    void on_battle_led(StepContext& context, std::int64_t event);
    void on_raid(StepContext& context, std::int64_t event);
    // 6.3.
    [[nodiscard]] Reign draw_reign(const StepContext& context, std::int64_t civ, std::int64_t number,
                                   std::int64_t start) const;
    void start_reign(const StepContext& context, std::int64_t civ, std::int64_t number, std::int64_t start);
    void lines_tick(StepContext& context);
    void lines_quiet(StepContext& context);
    // 6.4, 6.11, 6.14: names the current reign of civ with `record` as its
    // start event (0: the caller appends the event and sets it); returns the figure.
    std::int64_t name_reign(StepContext& context, std::int64_t civ, std::int64_t record);
    std::int64_t new_figure(StepContext& context, std::int64_t role, std::int64_t civ);
    void draw_person_name(StepContext& context, std::int64_t figure_id);
    void life_draws(const StepContext& context, std::int64_t figure_id, std::int64_t age_permille,
                    std::int64_t age_span_permille);
    // 6.9.
    void die(StepContext& context, std::int64_t figure_id, std::int64_t how);
    void close_relations_of(StepContext& context, std::int64_t figure_id, std::int64_t event,
                            bool rivals_only = false);
    void deaths_of_age(StepContext& context);
    // 6.6.
    void appoint(StepContext& context, std::int64_t war, std::size_t side);
    void vacancies(StepContext& context);
    void stand_down(StepContext& context);
    // 6.8.
    void caster_rises(StepContext& context);
    void books(StepContext& context);
    void blights(StepContext& context);
    // 6.12, 6.10.
    void present(StepContext& context);
    void epithets(const StepContext& context);
    [[nodiscard]] std::int64_t deed_value(const StepContext& context, const Figure& f, std::int64_t deed) const;
    // 6.5's reach.
    [[nodiscard]] std::int64_t reach(const StepContext& context, std::int64_t from, std::int64_t to) const;

    PeoplesSubsystem& peoples_;
    MonstersSubsystem* sites_ = nullptr;
    const ConflictSubsystem* conflict_ = nullptr;
    FiguresTuning tuning_;
    bool prepared_ = false;
    std::array<std::uint64_t, 15> keys_{};
    std::vector<RaceData> races_;        // ascending race id
    std::vector<CultureData> cultures_;  // ascending culture id
    // Per monster kind (index of MonstersSubsystem::kinds()): might and slayer_epithet.
    std::vector<std::pair<std::int64_t, std::string>> monster_kinds_;
    std::int64_t end_year_ = 0;
    std::int64_t year_ = 0;  // the year of the last step (7.4's Y)

    std::vector<Figure> figures_;                  // index id - 1
    std::vector<Line> lines_;                      // index civ - 1
    std::map<std::int64_t, KnownWar> wars_;        // by war_declared id
    std::vector<std::int64_t> hero_;               // per civilization: the living hero
    std::vector<std::int64_t> caster_;             // per civilization: the living caster
    std::vector<BookState> books_;                 // index id - 1
    std::set<std::int64_t> libraries_;             // settlements with a library (6.8)
    stages::PlacenameData placenames_;             // tower names (P5-12 5.2)
    std::int64_t cursor_ = 1;
    bool ended_ = false;                           // 6.12 and 6.10 done
    FiguresStats stats_;
    std::vector<ReignRecord> reign_log_;
};

}  // namespace worldgen::history
