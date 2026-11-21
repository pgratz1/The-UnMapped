#pragma once

// The peoples of the history (spec P3-05): where each race awakens and how
// many civilizations it founds, region scores and capacities, settlement
// sites, growth, foundings, the random pressures famine, plague and
// succession crisis, abandonment, ruin and its fading, and secession.
//
// PeoplesSubsystem runs in steps T1, T2, T4, Q1, Q3 and Q4 of the engine
// (P3-04 6.4); PeoplesAwakening is the engine's awakening handler and
// delegates to it, so that both share one name registry and one geography.
// Everything is integer arithmetic; every random decision is a keyed draw
// on one of the twelve hist.peoples.* streams (3.3).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/history/engine.hpp"
#include "worldgen/history/inputs.hpp"
#include "worldgen/history/names.hpp"

namespace worldgen::history {

// --- Tuning (12.1) -------------------------------------------------------------------

// A value per settlement kind (abandon_permille, plague_ppm).
struct PeoplesByKind {
    std::int64_t hamlet = 0;
    std::int64_t village = 0;
    std::int64_t town = 0;
    std::int64_t city = 0;
    std::int64_t fortress = 0;

    [[nodiscard]] std::int64_t of(SettlementKind kind) const noexcept;
};

// The definition history.peoples, field for field.
struct PeoplesTuning {
    std::int64_t score_site = 0, score_biome = 0, score_upland = 0, score_coast = 0;
    std::int64_t ore_reach_full = 1, coast_full = 1;
    std::int64_t rough_base = 0, rough_upland = 0;
    std::int64_t people_per_kcell = 0, capacity_floor = 0, capital_weight = 1000, founding_population = 0;
    std::int64_t village_min = 0, town_min = 0, city_min = 0, city_max = 0, fortress_max = 0;
    std::int64_t kind_shrink_permille = 0;
    std::int64_t site_spacing = 1, region_slots_max = 1;
    std::int64_t mouth_class_min = 1, port_radius = 0, port_mouth_bonus = 0, port_coast_bonus = 0;
    std::int64_t decline_permille = 1, recover_permille = 0;
    std::int64_t colonist_permille = 1, colonists_min = 1;
    std::int64_t abandon_min = 1, abandon_frail_max = 1;
    PeoplesByKind abandon_permille;
    std::int64_t found_pressure_permille = 1, found_rate_permille = 0, quiet_found_permille = 0;
    std::int64_t reach_base = 0, reach_per_expansion = 0;
    std::int64_t expand_min_score = 0, expand_relative_permille = 0, expand_min_capacity = 0;
    std::int64_t cost_penalty = 0, pass_cost = 1, river_cost = 0, river_border_share_permille = 0;
    std::int64_t civs_two_permille = 0, regions_per_civ = 0, home_min_hops = 1, home_min_capacity = 0;
    std::int64_t regions_per_extra_civ = 1, civs_extra_max = 0;  // P3-05 r8 21 item 5 (generator 0.18.0)
    std::int64_t intensity_calm = 0, intensity_normal = 0, intensity_turbulent = 0;
    std::int64_t famine_crowding_permille = 0, famine_ppm = 0, famine_toll_min = 0, famine_toll_max = 0;
    PeoplesByKind plague_ppm;
    std::int64_t plague_toll_min = 0, plague_toll_max = 0;
    std::int64_t plague_spread_permille = 0, plague_spread_toll_permille = 0;
    std::int64_t crisis_per_generation_permille = 0, crisis_loss_permille = 0;
    std::int64_t split_min_settlements = 2, split_min_regions = 2, split_permille = 0;
    std::int64_t ruin_fade_years = 1;
    std::int64_t colony_new_region_salience = 0, plague_small_salience = 0;
    std::int64_t plague_major_dead = 0, crisis_small_salience = 0;  // r6 (generator 0.17.0)
};

// The loaded definition (validated by the loader). FatalError "history:
// tuning: the content has no definition history.peoples" if absent.
[[nodiscard]] PeoplesTuning peoples_tuning_from(const data::ContentSet& content);

// --- Races and regions (4.1, 4.2, 5.1) ---------------------------------------------------

// What P3-05 reads of one race: its row, its race_biome rows and the race
// definition's fields that are not echoed in the world.
struct PeoplesRace {
    std::int64_t id = 0;
    std::string key;
    std::int64_t culture = 0;  // culture row id
    std::string culture_key;
    CultureNaming naming;
    std::int64_t w_fertility = 0, w_forage = 0, w_timber = 0, w_ore = 0, w_fresh_water = 0;  // site_weights
    std::int64_t upland = 0, coast = 0, growth_permille = 0, expansion = 0, density_permille = 1;
    std::int64_t generation_years = 1;
    std::int64_t cell_ore = 0, cell_height = 0;  // r6: cell_weights (P3-01 r8)
    std::array<std::uint8_t, 6> builds{};      // index: SettlementKind code (1 to 5); 1 if built
    std::vector<db::RaceBiomeRow> biomes;      // its race_biome rows, ascending biome

    [[nodiscard]] bool builds_kind(SettlementKind kind) const noexcept;
    // Some ladder kind (hamlet, village, town, city) is built.
    [[nodiscard]] bool has_ladder() const noexcept;
    // r6: cell_weights not both 0 (6.2, 8.4).
    [[nodiscard]] bool has_cell_term() const noexcept;
};

// The race of row `race` with the content's definitions. FatalError
// "history: peoples: race '<key>' or culture '<key>' is not in the content".
[[nodiscard]] PeoplesRace peoples_race(const db::RaceRow& race, std::span<const db::RaceBiomeRow> race_biomes,
                                       std::span<const db::CultureRow> cultures, const data::ContentSet& content);

// The per-region quantities of 4.1.
struct PeoplesRegion {
    std::int64_t id = 0;
    std::int64_t cells = 0, dry = 0, walk_cells = 0, coast_cells = 0;
    bool islet = false;
    bool upland = false;   // 4 x walk_cells < cells
    std::int64_t land = 0;      // cells of non-water biomes
    std::int64_t rough = 0;     // per mille of dry ground not walkable
    std::int64_t ore_m = 0;     // 0 to 255
    std::int64_t coast_m = 0;   // per mille
    std::int64_t fertility_mean = 0, forage_mean = 0, timber_mean = 0, fresh_water_mean = 0;
    std::int64_t seed_x = 0, seed_y = 0, x_min = 0, y_min = 0, x_max = 0, y_max = 0;
    std::int64_t elev_max_dm = 0;  // r6: the region row's largest dry-cell elevation (6.2)
    std::vector<std::pair<std::int64_t, std::int64_t>> shares;  // (biome code, share_b per mille), ascending biome
};

// 4.1 for one region row; `of_region` are its region_biome rows, `biomes`
// the biome table (for the water flag; a biome code without a row counts as
// land).
[[nodiscard]] PeoplesRegion peoples_region(const db::RegionRow& row, std::span<const db::RegionBiomeRow> of_region,
                                           std::span<const db::BiomeRow> biomes, const PeoplesTuning& tuning,
                                           std::int64_t islet_max_dry_cells);

// The biome term B(R, r) of 4.2, in [-1000, 1000]; P3-06 20.7 reads it too.
[[nodiscard]] std::int64_t region_biome_term(const PeoplesRegion& region, const PeoplesRace& race);
// sigma(R, r) of 4.2, in [-3000, 4000] with the shipped multipliers.
[[nodiscard]] std::int64_t region_score(const PeoplesRegion& region, const PeoplesRace& race,
                                        const PeoplesTuning& tuning);
// The fit q = min(1000, max(0, sigma)).
[[nodiscard]] std::int64_t region_fit(std::int64_t score) noexcept;
// C(R, r) of 5.1. FatalError "history: bound: capacity of region <R> for
// race <r> exceeds 2147483647" when it would not fit 2^31 - 1.
[[nodiscard]] std::int64_t region_capacity(const PeoplesRegion& region, const PeoplesRace& race,
                                           const PeoplesTuning& tuning);

// 4.4: the cost of an open border, nullopt for a barrier.
[[nodiscard]] std::optional<std::int64_t> border_cost(const db::RegionBorderRow& border, const PeoplesTuning& tuning);

// --- Settlement kinds (5.2, 8.1) -----------------------------------------------------------

// ladder(r, P): the highest ladder kind the race builds with min <= P; if
// none, its lowest ladder kind; fortress for a race without a ladder kind.
[[nodiscard]] SettlementKind ladder_kind(const PeoplesRace& race, std::int64_t population, const PeoplesTuning& tuning);
// The minimum population of a ladder kind (hamlet 0).
[[nodiscard]] std::int64_t kind_min(SettlementKind kind, const PeoplesTuning& tuning) noexcept;
// kind_cap(s): fortress_max for a fortress, else the top of the highest
// ladder kind the race builds.
[[nodiscard]] std::int64_t kind_cap(const PeoplesRace& race, SettlementKind kind, const PeoplesTuning& tuning);
// The yearly growth rule of 9.1 for one settlement: P toward its share.
[[nodiscard]] std::int64_t grow_population(std::int64_t population, std::int64_t share, std::int64_t growth_permille,
                                           std::int64_t cap, std::int64_t decline_permille);

// --- Geography (sections 4 to 6) ------------------------------------------------------------

struct PeoplesNeighbour {
    std::int64_t region = 0;  // ascending
    std::int64_t cost = 0;
};

struct SiteCell {
    std::int64_t x = 0;
    std::int64_t y = 0;
    friend bool operator==(const SiteCell&, const SiteCell&) = default;
};

// Everything P3-05 derives from the stored geography and the races, once
// per run. Scores and capacities for every (region, race); open borders;
// hop distances (cached on first use); mouths and ports; site candidates
// per (region, race) sorted by (value desc, cell index asc), cached on
// first use. Read-only apart from those caches; single-threaded.
class PeoplesGeo {
public:
    // FatalError "history: peoples: layer '<name>' is not <N> x <N> u8 or
    // u16" for a layer of another shape, "history: peoples: race <r> has
    // no generation_years above 0".
    PeoplesGeo(const HistoryInputs& inputs, const data::ContentSet& content, const PeoplesTuning& tuning,
               std::int64_t islet_max_dry_cells);

    [[nodiscard]] std::int64_t region_count() const noexcept { return static_cast<std::int64_t>(regions_.size()); }
    // FatalError "history: peoples: region <R> does not exist".
    [[nodiscard]] const PeoplesRegion& region(std::int64_t id) const;
    // FatalError "history: peoples: race <r> does not exist".
    [[nodiscard]] const PeoplesRace& race(std::int64_t id) const;
    [[nodiscard]] std::int64_t score(std::int64_t region, std::int64_t race) const;
    [[nodiscard]] std::int64_t capacity(std::int64_t region, std::int64_t race) const;
    // B(R, r) of 4.2 (P3-06 20.7: concealment and home defence).
    [[nodiscard]] std::int64_t biome_term(std::int64_t region, std::int64_t race) const;
    // The regions that are not islets (P3-05 21 item 5).
    [[nodiscard]] std::int64_t non_islet_count() const noexcept;
    [[nodiscard]] std::span<const PeoplesNeighbour> neighbours(std::int64_t region) const;
    // Breadth-first hop distances from `from` over open borders, index
    // region id - 1, -1 when unreachable.
    [[nodiscard]] const std::vector<std::int64_t>& hops(std::int64_t from) const;
    [[nodiscard]] std::int64_t map_size() const noexcept { return n_; }
    [[nodiscard]] bool is_mouth(std::int64_t x, std::int64_t y) const;
    [[nodiscard]] bool near_mouth(std::int64_t x, std::int64_t y) const;
    [[nodiscard]] bool near_sea(std::int64_t x, std::int64_t y) const;
    // v(c, r) of 6.2 item 2: habitability(c) + port(c, r) + (r6) the cell
    // term (cell_ore x ore(c) + cell_height x hgt(c)) div 1000.
    [[nodiscard]] std::int64_t site_value(std::int64_t x, std::int64_t y, std::int64_t race) const;
    // r6 (8.4): walk_area(c) > 0.
    [[nodiscard]] bool walkable(std::int64_t x, std::int64_t y) const;
    // The site of 6.2 in `region` for `race` against the given settlements
    // (statuses 1 to 3 keep their spacing; with `relaxed`, item 3: any cell
    // no settlement stands on). nullopt when there is none (item 4 is the
    // caller's).
    [[nodiscard]] std::optional<SiteCell> site(std::int64_t region, std::int64_t race,
                                               std::span<const Settlement> settlements, bool relaxed) const;

private:
    [[nodiscard]] std::size_t race_index(std::int64_t race) const;
    [[nodiscard]] std::size_t cell(std::int64_t x, std::int64_t y) const;
    [[nodiscard]] const std::vector<std::uint32_t>& candidates(std::int64_t region, std::int64_t race) const;

    PeoplesTuning tuning_;
    std::int64_t n_ = 0;
    std::vector<PeoplesRegion> regions_;
    std::vector<PeoplesRace> races_;                 // race row order (ascending id)
    std::vector<std::int64_t> score_;                // [race index * regions + region - 1]
    std::vector<std::int64_t> capacity_;
    std::vector<std::int64_t> biome_term_;           // B (4.2), same index
    std::vector<std::vector<PeoplesNeighbour>> neighbours_;
    // One row per region, each empty until computed. The outer vector is
    // sized once, at construction, and never resized, so a reference
    // returned by hops() stays valid for the life of the geography (which
    // ConflictSubsystem::local_force relies on while it holds one row);
    // a caller that holds a row across further hops() calls may still copy
    // it (P3-06 r14 21.3).
    mutable std::vector<std::vector<std::int64_t>> hops_;
    std::vector<std::uint16_t> region_layer_;
    std::vector<std::uint8_t> water_;
    std::vector<std::uint16_t> habitability_;
    std::vector<std::int16_t> elevation_;   // r6
    std::vector<std::uint16_t> ore_;        // r6
    std::vector<std::uint16_t> walk_area_;  // r6
    std::vector<std::uint8_t> mouth_;
    std::vector<std::uint8_t> near_mouth_;
    std::vector<std::uint8_t> near_sea_;
    std::vector<std::vector<std::uint32_t>> cells_of_region_;  // land candidates (water 0), ascending index
    mutable std::vector<std::vector<std::uint32_t>> candidates_;  // [race index * regions + region - 1]
    mutable std::vector<std::uint8_t> candidates_ready_;
};

// --- The subsystem and its seam for P3-06 (13) ------------------------------------------------

class PeoplesSubsystem;

// The read-only view P3-06 uses. Valid during any step of the run of its
// subsystem, after the first awakening. The functions that read the state
// take the step's context (spec 13 lists them without it; a member may not
// hold a reference into State between steps, 13.1 item 1).
class PeoplesView {
public:
    [[nodiscard]] std::int64_t score(std::int64_t region, std::int64_t race) const;       // sigma (4.2)
    [[nodiscard]] std::int64_t capacity(std::int64_t region, std::int64_t race) const;    // C (5.1)
    [[nodiscard]] std::span<const PeoplesNeighbour> neighbours(std::int64_t region) const;  // open borders (4.4)
    // C_s now (5.2); 0 if the settlement is not standing.
    [[nodiscard]] std::int64_t share(const StepContext& context, std::int64_t settlement) const;
    // Sum of P over its standing settlements.
    [[nodiscard]] std::int64_t population(const StepContext& context, std::int64_t civ) const;
    // Held regions, ascending.
    [[nodiscard]] std::vector<std::int64_t> regions(const StepContext& context, std::int64_t civ) const;
    // Standing settlements, ascending.
    [[nodiscard]] std::vector<std::int64_t> settlements(const StepContext& context, std::int64_t civ) const;
    // Regions held by another civilization that pass 9.3's outward tests but
    // for being held, ascending.
    [[nodiscard]] std::vector<std::int64_t> wanted_held(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] std::int64_t hunger(std::int64_t civ) const;              // 9.3 item 4
    [[nodiscard]] std::int64_t last_reduction(std::int64_t settlement) const;  // 10.4; 0 none
    // r8 (21 item 1, generator 0.18.0): the site of 6.2 in `region` for `race`
    // against the step's settlements, not relaxed; the kind of 8.4 (r6) a
    // settlement of `population` founded on that site would have; the hop
    // distances of 4.4 from `region` (index region id - 1, -1 unreachable);
    // the biome term B of 4.2.
    [[nodiscard]] std::optional<SiteCell> site(const StepContext& context, std::int64_t region,
                                               std::int64_t race) const;
    [[nodiscard]] SettlementKind founding_kind(std::int64_t race, std::int64_t region, const SiteCell& site,
                                               std::int64_t population) const;
    [[nodiscard]] const std::vector<std::int64_t>& hops(std::int64_t region) const;
    [[nodiscard]] std::int64_t biome_term(std::int64_t region, std::int64_t race) const;

private:
    friend class PeoplesSubsystem;
    explicit PeoplesView(const PeoplesSubsystem& owner) : owner_(owner) {}
    const PeoplesSubsystem& owner_;
};

// The peoples subsystem (steps T1, T2, T4, Q1, Q3, Q4). One subsystem
// serves one run of the engine. Its members hold ids, never references into
// State (13.1 item 1).
class PeoplesSubsystem final : public Subsystem {
public:
    PeoplesSubsystem();
    PeoplesSubsystem(const PeoplesSubsystem&) = delete;
    PeoplesSubsystem& operator=(const PeoplesSubsystem&) = delete;

    [[nodiscard]] std::span<const Step> steps() const override;
    void run(StepContext& context) override;

    [[nodiscard]] const PeoplesView& view() const noexcept { return view_; }
    // P3-06 after casualties: the cause of a later abandonment.
    void note_reduction(std::int64_t settlement, std::int64_t event);
    // 8.5 items 2 to 5 with `event` as the ruin event (r1); not item 6. The
    // settlement must be standing and owned. FatalError "history: peoples:
    // ruin: settlement <s> is not standing".
    void ruin(StepContext& context, std::int64_t settlement, std::int64_t event);
    // r8 (21 item 1): the name of settlement `settlement_id` of `race`, drawn
    // as a colony's (next_name, NameKind::settlement, through the name
    // filter), from the one registry.
    [[nodiscard]] std::string settlement_name(const StepContext& context, std::int64_t race,
                                              std::int64_t settlement_id);

    // The homes of an awakening of `race` now (7.1, 7.2): no state change.
    // Cached within one step for awakening_place and awaken; awaken clears
    // the cache, so a later call in the same step sees the new state.
    [[nodiscard]] std::vector<std::int64_t> homes(const StepContext& context, std::int64_t race);
    // 7.3 for each home.
    void awaken(StepContext& context, std::int64_t race, std::int64_t awakening_event);

    // The geography (after the first call of homes or run). FatalError
    // "history: peoples: used before the first step" otherwise.
    [[nodiscard]] const PeoplesGeo& geo() const;
    [[nodiscard]] const PeoplesTuning& tuning() const noexcept { return tuning_; }

    // The settlements a rule reads: the state, or (the plague plan, 10.2) a
    // copy of the populations and of which settlements of the owner stand,
    // indexed by id.
    struct SettlementsNow {
        const State& state;
        const std::vector<std::int64_t>* pop = nullptr;
        const std::vector<std::uint8_t>* up_mask = nullptr;
        // Standing and owned by `owner` (the copy: standing in it).
        [[nodiscard]] bool up(std::int64_t id, std::int64_t owner) const;
        [[nodiscard]] std::int64_t population(std::int64_t id) const;
    };
    // P3-05 r12 22.7 (the one last-refuge predicate, P3-06 r14 21.4): `civ`
    // has exactly one standing settlement in `now` and no other
    // civilization of its race is alive (ended_year read from the state).
    // P3-06 calls it with the live state (SettlementsNow{state}).
    [[nodiscard]] bool race_last_refuge(const SettlementsNow& now, std::int64_t civ) const;

private:
    friend class PeoplesView;

    void prepare(const StepContext& context);
    // T1 (and one year of Q1 without the end-of-step rules).
    void end_empty_civilizations(StepContext& context);
    void grow_years(StepContext& context, std::int64_t years, bool recover_each_year);
    void abandon_small(StepContext& context);
    void change_kinds(StepContext& context);
    void foundings(StepContext& context);
    void pressures(StepContext& context);
    void fade_ruins(StepContext& context);

    [[nodiscard]] std::int64_t share_now(const State& state, std::int64_t settlement) const;
    [[nodiscard]] std::vector<std::int64_t> shares(const State& state) const;  // index id - 1; 0 not standing
    [[nodiscard]] std::int64_t race_of_civ(const State& state, std::int64_t civ) const;
    [[nodiscard]] std::int64_t cap_of(const State& state, const Settlement& s) const;
    std::int64_t strike(StepContext& context, std::int64_t settlement, std::string_view kind, std::int64_t toll,
                std::int64_t cause, std::optional<std::int64_t> salience);
    void abandon(StepContext& context, std::int64_t settlement, std::int64_t cause);
    // 8.5: the events an abandonment of s would append.
    [[nodiscard]] std::int64_t abandon_events(const State& state, std::int64_t settlement) const;
    void end_civilization(StepContext& context, std::int64_t civ, std::int64_t region, std::int64_t settlement,
                          std::int64_t cause);
    // 10.3: the split test (r6: made before the crisis event is written).
    struct SplitPlan {
        std::vector<std::int64_t> part;     // the seceding regions
        std::vector<std::int64_t> leaving;  // the settlements that leave
        std::size_t mine = 0;               // c's standing visible settlements
    };
    [[nodiscard]] SplitPlan plan_split(const StepContext& context, std::int64_t civ) const;
    void crisis(StepContext& context, std::int64_t civ, std::int64_t event, const SplitPlan& plan);
    // r6 (10.2): the total dead of an outbreak and its spreads, planned.
    [[nodiscard]] std::int64_t planned_plague_dead(const StepContext& context, std::int64_t origin, std::int64_t toll,
                                                   const std::vector<std::uint8_t>& struck) const;
    // The outcome of a blow of `toll` per mille (10.4 with 8.5 (a)).
    struct StrikeResult {
        std::int64_t dead = 0;
        std::int64_t after = 0;
        bool abandoned = false;
    };
    [[nodiscard]] StrikeResult strike_result(const StepContext& context, const SettlementsNow& now,
                                             std::int64_t settlement, std::int64_t toll) const;
    // 10.2: the toll of a spread from origin to target, nullopt if its draw fails.
    [[nodiscard]] std::optional<std::int64_t> spread_toll(const StepContext& context, std::int64_t origin,
                                                          std::int64_t target) const;
    // Another standing settlement of the settlement's owner exists.
    [[nodiscard]] bool other_standing(const SettlementsNow& now, std::int64_t settlement) const;
    // r9 (21 item 6): the standing settlement is its race's last refuge
    // (race_last_refuge of its owner: then it is the one standing settlement).
    [[nodiscard]] bool settlement_last_refuge(const SettlementsNow& now, std::int64_t settlement) const;
    // The owner's nearest other standing settlement (squared distance, then
    // id), 0 if none.
    [[nodiscard]] std::int64_t nearest_standing(const SettlementsNow& now, std::int64_t settlement) const;
    // 8.5 item 2: max(0, 2 C(R_t, race) - P_t).
    [[nodiscard]] std::int64_t refugee_room(const SettlementsNow& now, std::int64_t destination,
                                            std::int64_t race) const;
    // 8.4: the kind of a new settlement (r6: the fortress cell rule).
    [[nodiscard]] SettlementKind founding_kind(std::int64_t race, std::int64_t region, const SiteCell& site,
                                               std::int64_t population) const;
    [[nodiscard]] std::optional<std::int64_t> outward_target(const StepContext& context, std::int64_t civ,
                                                             bool require_free, std::vector<std::int64_t>* all) const;
    [[nodiscard]] std::string next_name(const StepContext& context, std::int64_t race, NameKind kind,
                                        std::int64_t entity);
    [[nodiscard]] std::uint64_t key(std::size_t stream) const noexcept { return keys_[stream]; }

    PeoplesTuning tuning_;
    std::optional<PeoplesGeo> geo_;
    NameRegistry registry_;
    NameFilter filter_;  // P3-02 r5: the world's name filter
    std::array<std::uint64_t, 12> keys_{};
    std::int64_t intensity_ = 1000;
    bool prepared_ = false;
    std::int64_t homes_serial_ = -1;          // the step serial the cached homes were computed in; -1 none
    std::int64_t homes_race_ = 0;
    std::vector<std::int64_t> homes_;
    // Members keyed by id (index id - 1).
    std::vector<std::int64_t> before_;          // per settlement: P just before the first blow
    std::vector<std::int64_t> last_reduction_;  // per settlement: event, 0 none
    std::vector<std::int64_t> decline_start_;   // r6, per settlement: the blow that last began its decline
    std::vector<std::int64_t> ruined_owner_;    // per settlement: the owner when it was ruined
    std::vector<std::int64_t> ruined_year_;     // per settlement: the year it was ruined
    std::vector<std::int64_t> ruin_event_;      // per settlement: its ruin event
    std::vector<std::int64_t> hunger_;          // per civilization
    PeoplesView view_;
};

// The awakening handler of P3-05 (7): the homes of 7.1 and 7.2 and the
// placement of 7.3, through its subsystem.
class PeoplesAwakening final : public AwakeningHandler {
public:
    explicit PeoplesAwakening(PeoplesSubsystem& peoples) : peoples_(peoples) {}
    std::int64_t awakening_place(const StepContext& context, std::int64_t race) override;
    void awaken(StepContext& context, std::int64_t race, std::int64_t awakening_event) override;

private:
    PeoplesSubsystem& peoples_;
};

}  // namespace worldgen::history
