#pragma once

// The monsters subsystem of spec P5-02 (docs/specs/p5-02-beasts-monsters.md):
// the beasts of each region (counts per region and species, their pull on
// the growth of settlements, 6.2), the named monsters (appearances, risings
// of the undead, raids, overruns, sleep, hunts and deaths, 6.3 and 6.4), the
// sites (lairs and frontier outposts, 6.6) and the second tension source
// (6.5). It runs in steps T1, T4, T6, Q1 and Q4 after renaming (6.1).
//
// Keyed draws only (the streams hist.beasts.*, hist.monsters.*,
// hist.sites.outpost, hist.names.monster and hist.names.site); integer
// arithmetic only; its members hold ids, never references into State, and
// are vectors in a fixed order. It changes State only through
// set_population, PeoplesSubsystem::note_reduction and
// PeoplesSubsystem::ruin (P5-02 15).

#include <array>
#include <cstddef>
#include <cstdint>
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
#include "worldgen/history/peoples.hpp"
#include "worldgen/history/renaming.hpp"
#include "worldgen/history/schedule.hpp"
#include "worldgen/stages/regions.hpp"

namespace worldgen::history {

class ConflictSubsystem;  // P5-07 7.3: the flight seam
struct DeepFlightPlan;
class AllianceView;       // P5-07 r2 7.6: the allies of a waking realm
class RelationsSubsystem;  // P5-07 r2 6.11: the aid's goodwill

// The bounds of P5-02 5.
inline constexpr std::int64_t MAX_BEAST_COUNT = 1000000;
inline constexpr std::int64_t MAX_MONSTERS = 100000;
inline constexpr std::int64_t MAX_SITES = 1000000;

// A habitat entry: (biome key, weight per mille).
struct HabitatWeight {
    std::string biome;
    std::int64_t weight = 0;

    friend bool operator==(const HabitatWeight&, const HabitatWeight&) = default;
};

// A beast definition (P5-02 4.1); id = 1, 2, ... in canonical id order.
struct BeastDef {
    std::int64_t id = 0;
    std::string key;
    std::string name;
    std::string plural;
    std::vector<HabitatWeight> habitat;
    std::int64_t density = 0;
    std::int64_t growth_permille = 1;
    std::int64_t menace = 0;
    std::int64_t food = 0;
    std::vector<std::string> bold_phrases;  // P5-03 13

    friend bool operator==(const BeastDef&, const BeastDef&) = default;
};

// A monster definition (P5-02 4.2); id = 1, 2, ... in canonical id order.
struct MonsterKindDef {
    std::int64_t id = 0;
    std::string key;
    std::string name;
    std::string noun;
    bool grouped = false;
    std::int64_t wake = 1;  // 1 wild, 2 dead, 3 deep (P5-07 4.1)
    std::vector<HabitatWeight> biomes;
    std::int64_t wake_rate = 0;
    std::string naming_culture;
    std::vector<std::string> name_forms;
    std::vector<std::string> lair_forms;
    std::int64_t might = 1;
    std::int64_t range_hops = 0;
    std::int64_t raid_permille = 0;
    std::int64_t toll_min_permille = 0;
    std::int64_t toll_max_permille = 0;
    std::int64_t kill_max = 0;
    std::vector<SettlementKind> overruns;
    std::int64_t overrun_permille = 0;
    std::int64_t active_years = 1;
    std::int64_t rewake_ppm = 0;
    std::int64_t lifespan_years = 0;
    std::int64_t max_alive = 0;
    bool flare = false;
    std::int64_t hoard_permille = 0;
    std::vector<std::string> raid_phrases;  // P5-03 9.5
    std::string slayer_epithet;             // P5-06 4.3

    friend bool operator==(const MonsterKindDef&, const MonsterKindDef&) = default;
};

// Definition history.monsters (P5-02 4.3), field for field.
struct MonstersTuning {
    std::int64_t pressure_low = 0, pressure_normal = 0, pressure_high = 0;
    std::int64_t living_permille = 0;
    std::int64_t undead_min_dead = 1, undead_window_years = 1;
    std::int64_t hunt_hops = 0, hunt_levy_permille = 0, hunt_permille = 0, hunt_memory_years = 1;
    std::int64_t slay_max_permille = 0, drive_permille = 0, drive_years = 0;
    std::int64_t displace_per_kpeople = 0, displace_max_permille = 0;
    std::int64_t beast_decline_permille = 1, people_floor = 1;
    std::int64_t drag_k = 0, drag_max_permille = 0, feed_k = 0, feed_max_permille = 0;
    std::int64_t bold_on_permille = 1, bold_off_permille = 0, bold_permille = 0, bold_toll_permille = 0;
    std::int64_t outpost_permille = 0, outpost_max = 0, outpost_quiet_years = 1;
    // (r1, 6.4.1) the raid target's weights. prey_weight is indexed by the
    // SettlementKind code (1 hamlet to 5 fortress); index 0 is unused.
    std::array<std::int64_t, 6> prey_weight{};
    std::int64_t prey_capital_permille = 0, prey_hop_permille = 0;
    std::int64_t prey_might_ref = 1, prey_might_max_permille = 0;
    // P5-03 9.1 to 9.4, 14.1 (generator 0.24.0). The defaults of a hand-made
    // tuning are the shipped values.
    std::int64_t raid_chain_years = 100, toll_over_permille = 250;
    std::int64_t hunt_raid_permille = 60, hunt_max_permille = 400;
    std::int64_t fear_raids = 3, fear_years = 50, fear_permille = 300;
    std::int64_t overrun_people_ref = 800;
    // P5-03a 6 (generator 0.25.0): the threshold's share of the world's
    // people. 0 in a hand-made tuning, where undead_min_dead is then the
    // whole threshold (the rule before 0.25.0); shipped 25.
    std::int64_t undead_dead_permille = 0;
    // P5-06 25.3 (generator 0.27.0): the region count above which the
    // population term shrinks; 0 (a hand-made tuning) leaves it out.
    std::int64_t undead_regions_ref = 0;
    // P5-07 4.2 (generator 0.29.0): the mountain depths. The defaults of a
    // hand-made tuning are inert (no mine deepens, none wakes); the rest are
    // the shipped starting values.
    std::array<std::int64_t, 3> deep_years{150, 500, 1000};
    std::int64_t deep_delve_permille = 0;
    std::array<std::int64_t, 3> deep_wake_ppm{0, 0, 0};
    std::int64_t deep_gate_levy_permille = 1000, deep_seal_max_permille = 800, deep_toll_permille = 400;
    std::int64_t deep_takes_max = 2, deep_reach_hops = 1;
    // P5-07 r2 4.2: the goblin deep warrens (6.10) and the allies' aid
    // (6.11). Inert in a hand-made tuning (no warren wakes, no ally aids).
    std::int64_t deep_warren_population_min = 600, deep_warren_ore_min = 1, deep_warren_wake_permille = 0;
    std::int64_t deep_aid_permille = 0, deep_aid_hops = 2, deep_aid_levy_permille = 500, deep_aid_max = 2;
    std::int64_t deep_aid_memory = 30;
    // P5-08 12.2 (generator 0.30.0): the deep's stir in quiet steps; 0 off.
    std::int64_t deep_stir_permille = 0;

    friend bool operator==(const MonstersTuning&, const MonstersTuning&) = default;
};

// The data of loaded content. FatalError "history: tuning: the content has
// no definition history.monsters" when it is absent.
[[nodiscard]] MonstersTuning monsters_tuning_from(const data::ContentSet& content);
[[nodiscard]] std::vector<BeastDef> beast_defs_from(const data::ContentSet& content);
[[nodiscard]] std::vector<MonsterKindDef> monster_kind_defs_from(const data::ContentSet& content);

// chance_ppb of P5-02 3: (u >> 32) < min(2^32, floor(max(0, p) 2^32 / 10^9)).
[[nodiscard]] bool chance_ppb(std::uint64_t u, std::int64_t p);

// P5-02 6.2.2: one year of a species' count B toward the capacity K.
[[nodiscard]] std::int64_t beast_year(std::int64_t count, std::int64_t capacity, std::int64_t growth_permille,
                                      std::int64_t decline_permille);

// Counts for the calibration sweep (P5-02 16): not stored in the world.
struct MonstersStats {
    std::int64_t bold = 0;
    std::int64_t raids = 0;
    std::int64_t overruns = 0;
    std::int64_t outposts = 0;
    std::int64_t fled = 0;  // P5-03 9.4
    // P5-07 (the mountain depths).
    std::int64_t deepened = 0;
    std::int64_t deep_woken = 0;
    std::int64_t deep_sealed = 0;
    std::int64_t holds_lost = 0;
    std::int64_t deep_flights = 0;
    // P5-07 r2.
    std::int64_t warrens_delved = 0;
    std::int64_t warrens_woken = 0;
    std::int64_t warrens_sealed = 0;
    std::int64_t aids = 0;
    std::int64_t wakings_aided = 0;
    // P5-08 12.2: stirs drawn in quiet steps and the wakings they forced.
    std::int64_t deep_stirred = 0;
    std::int64_t stir_wakings = 0;
};

class MonstersSubsystem final : public Subsystem, public TensionSource, public WildView, public MonsterNames {
public:
    // The shipped configuration: the data of `content`.
    MonstersSubsystem(PeoplesSubsystem& peoples, const data::ContentSet& content);
    // A test seam: hand-made data.
    MonstersSubsystem(PeoplesSubsystem& peoples, MonstersTuning tuning, std::vector<BeastDef> beasts,
                      std::vector<MonsterKindDef> kinds);
    MonstersSubsystem(const MonstersSubsystem&) = delete;
    MonstersSubsystem& operator=(const MonstersSubsystem&) = delete;

    [[nodiscard]] std::span<const Step> steps() const override;
    void run(StepContext& context) override;
    // 6.5: {T_d, pending_flare} when a flare is pending, else {0, 0}. Pure.
    TensionReport tension(const StepContext& context) override;
    // 7.1, 7.2.
    [[nodiscard]] std::int64_t growth_permille(std::int64_t region) const override;
    [[nodiscard]] bool shunned(std::int64_t region) const override;
    // 9.2, 9.3: the root of a monster's name; its name.
    [[nodiscard]] std::string monster_root(std::int64_t monster) const override;
    [[nodiscard]] const std::string& monster_name(std::int64_t monster) const;
    // P5-03 12.2: the region names in force (the renaming subsystem, set by
    // the history stage; without it a host is named for region.name).
    void set_region_names(const RenamingSubsystem* renaming) noexcept { renaming_ = renaming; }
    // P5-06 7.1, 7.7: the figures (set once by the stage; nullptr: no hero).
    void set_figures(const FigureView* figures) noexcept { figures_ = figures; }
    // P5-07 5, 7.3: the conflict subsystem for the deep's flight (set once by
    // the stage; nullptr: a last refuge taken by the deep has no plan).
    void set_conflict(ConflictSubsystem* conflict) noexcept { conflict_ = conflict; }
    // P5-07 r2 7.6: the alliance view and the relations for the allies' aid
    // at a waking (6.11), set once by the stage; with either unset no aid is
    // decided and no draw is made.
    void set_alliances(const AllianceView* alliances) noexcept { alliances_ = alliances; }
    void set_relations(RelationsSubsystem* relations) noexcept { relations_ = relations; }
    // P5-07 r2 7.6: the warren depth (0 to 3) of a settlement; 0 for an
    // unknown id.
    [[nodiscard]] std::int64_t warren_depth(std::int64_t settlement) const;
    // P5-07 7.1: the depth (0 to 3) of a mine; 0 for an unknown site or a
    // site of another kind.
    [[nodiscard]] std::int64_t mine_depth(std::int64_t site) const;
    // P5-07 7.2: the wake of a kind index (1 wild, 2 dead, 3 deep).
    [[nodiscard]] std::int64_t kind_wake(std::size_t kind) const { return kinds_[kind].wake; }
    // P5-06 3: const reads for the figures: the kind index of a monster
    // (into kinds()), the kinds, and P5-02 6.6.1's height cell of a region
    // (cells of standing settlements and open sites taken), none when no
    // free land cell is left.
    [[nodiscard]] std::size_t monster_kind_of(std::int64_t monster) const;
    [[nodiscard]] std::span<const MonsterKindDef> kinds() const noexcept { return kinds_; }
    struct HeightCell {
        std::int64_t x = 0, y = 0;
    };
    [[nodiscard]] std::optional<HeightCell> height_cell(const StepContext& context, std::int64_t region) const;
    // P5-04a 2, 21.4: the same height among the cells at Chebyshev distance
    // above `radius` from every cell of `avoid` (none kept clear when avoid
    // is empty or radius <= 0).
    [[nodiscard]] std::optional<HeightCell> height_cell(const StepContext& context, std::int64_t region,
                                                        std::span<const HeightCell> avoid, std::int64_t radius) const;
    // P5-03 11.2 (WildView): an open site of kind 1 or 2 stands on the
    // settlement (P5-04 6.12: a mine does not hold back a ruin's fading).
    [[nodiscard]] bool holds_site(std::int64_t settlement) const override;

    // P5-04 7.16: the site seam for the alliances subsystem's mines (kind 3).
    struct SiteSpec {
        std::int64_t id = 0, kind = 3;  // 3 mine (P5-04), 4 tower (P5-06)
        std::string name;
        std::int64_t region = 0, x = 0, y = 0;
        std::int64_t settlement = 0, founder_civ = 0, monster = 0;
        std::int64_t origin_year = 0, origin_event = 0;
    };
    [[nodiscard]] std::int64_t next_site_id() const noexcept { return static_cast<std::int64_t>(sites_.size()) + 1; }
    // FatalError unless spec.id == next_site_id() and spec.kind is 3 or 4.
    // end_site_seam: FatalError unless the site is an open mine or tower (kind
    // 3 or 4, P5-04a 21.4).
    void open_site(const SiteSpec& spec);

    // P5-08 7.3: a monster sent by a god. prepare_sent fixes the ids, the
    // kind, the lair's region and cell (no state change); create_sent makes
    // the monster (awake, born in the step's year, born_event `event`) and
    // its lair (kind 1), drawing their names with the streams and keys of a
    // wild appearance (engineer's reading: the names are drawn here, not in
    // prepare_sent, so that the registry is not touched by a pure call).
    // lair_region_for_sent: 6.4's search, the regions within `hops` borders
    // of `from_region` with no open lair and a free wild cell, by (hops, id),
    // the first; 0 none. living_count: the living monsters of a kind.
    struct SentMonster {
        std::int64_t monster = 0, lair = 0, region = 0;
        std::size_t kind = 0;
        SiteCell cell;
        std::string name, lair_name;
    };
    [[nodiscard]] SentMonster prepare_sent(const StepContext& context, std::size_t kind, std::int64_t region) const;
    void create_sent(StepContext& context, const SentMonster& sent, std::int64_t event);
    [[nodiscard]] std::int64_t lair_region_for_sent(const StepContext& context, std::int64_t from_region,
                                                    std::int64_t hops) const;
    [[nodiscard]] std::int64_t living_count(std::size_t kind) const { return living_of(kind); }
    void end_site_seam(std::int64_t site, std::int64_t year, std::int64_t event, std::int64_t ending);

    // 6.2.2: the beast rows of the run's end (year end_year - 1), called by
    // the stage once the engine has run. Idempotent.
    void finish(std::int64_t end_year);

    // The rows of the five tables of P5-02 11.2, in their write order.
    [[nodiscard]] std::vector<db::BeastRow> beast_rows() const;
    [[nodiscard]] std::vector<db::MonsterKindRow> monster_kind_rows() const;
    [[nodiscard]] std::vector<db::BeastPopulationRow> beast_population_rows() const;  // (region, beast, year)
    [[nodiscard]] std::vector<db::MonsterRow> monster_rows() const;
    [[nodiscard]] std::vector<db::SiteRow> site_rows() const;
    [[nodiscard]] const MonstersStats& stats() const noexcept { return stats_; }
    [[nodiscard]] const MonstersTuning& tuning() const noexcept { return tuning_; }

private:
    enum class MonsterStatus : std::uint8_t { awake = 1, asleep = 2, slain = 3, died = 4 };
    struct RaidMemory {
        std::int64_t civ = 0;
        std::int64_t year = 0;
        std::int64_t event = 0;
        std::vector<std::int64_t> years;  // P5-03 9.3: the raids on civ within hunt_memory_years, ascending
    };
    // P5-03 9.1, 9.4: the raids on one settlement within max(raid_chain_years,
    // fear_years), ascending.
    struct PlaceRaid {
        std::int64_t year = 0;
        std::int64_t event = 0;
    };
    struct PlaceMemory {
        std::int64_t settlement = 0;
        std::vector<PlaceRaid> raids;
    };
    struct Monster {
        std::int64_t id = 0;
        std::size_t kind = 0;  // index into kinds_
        std::string name;
        std::string root;
        MonsterStatus status = MonsterStatus::awake;
        std::int64_t born = 0, born_event = 0;
        std::int64_t lair = 0;  // site id
        std::vector<std::int64_t> hops;  // from the lair's region, index region id - 1, -1 unreachable
        std::int64_t waking_event = 0;
        std::int64_t sleeping_event = 0;
        std::int64_t drive_until = 0;
        std::int64_t hoard = 0;
        bool holds_settlement = false;
        std::vector<RaidMemory> raids;  // ascending civ
        std::vector<PlaceMemory> places;  // P5-03: ascending settlement
        bool placed = false;  // P5-03 12.2: a host named for its place (its root is the place)
        std::optional<std::int64_t> died;
        std::int64_t death_event = 0;
        std::int64_t slayer = 0;
        // P5-07 5: a deep horror's waking realm (0 for other kinds), the holds
        // it took (the first included), whether it was sealed, its deep_woken.
        std::int64_t waking_realm = 0;
        std::int64_t takes = 0;
        bool sealed = false;
        std::int64_t deep_event = 0;
        bool from_warren = false;  // P5-07 r2 5: it woke from a goblin deep warren
        bool sent = false;         // P5-08 6.4: sent by a god (outside the world cap)
    };
    struct Site {
        std::int64_t id = 0;
        std::int64_t kind = 1;  // 1 lair, 2 outpost, 3 mine (P5-04)
        std::string name;
        std::int64_t region = 0, x = 0, y = 0;
        std::int64_t settlement = 0, founder = 0, monster = 0;
        std::int64_t origin_year = 0, origin_event = 0;
        std::optional<std::int64_t> ended_year;
        std::int64_t ending_event = 0, ending = 0;
        std::int64_t hostile_year = 0;
        // P5-07 5: a mine's depth (0 to 3), its last delving_deepened (0
        // none), whether it woke a horror and whether it is sealed.
        std::int64_t depth = 0;
        std::int64_t deepen_event = 0;
        bool woken = false;
        bool sealed = false;
    };
    struct WindowEntry {
        std::int64_t year = 0;
        std::int64_t dead = 0;
        std::int64_t event = 0;
    };
    struct Cell {
        std::int64_t x = 0, y = 0;
    };

    void prepare(const StepContext& context);
    // 6.2.
    void beasts(StepContext& context, std::int64_t years);
    void write_beast_rows(std::int64_t year);
    void beasts_bold(StepContext& context);
    // 6.3.
    [[nodiscard]] std::int64_t living() const;
    [[nodiscard]] std::int64_t living_of(std::size_t kind) const;
    [[nodiscard]] bool has_lair(std::int64_t region) const;
    [[nodiscard]] bool caps_allow(std::size_t kind, std::int64_t region) const;
    void read_log(const StepContext& context);
    bool rise(StepContext& context);
    bool wild_appearance(StepContext& context);
    void create(StepContext& context, std::size_t kind, std::int64_t region, bool risen);
    // 6.4.
    void monsters_year(StepContext& context);
    void die_of_age(StepContext& context, std::int64_t monster);
    [[nodiscard]] std::int64_t prey_weight(SettlementKind kind, std::int64_t might, std::int64_t hops) const;
    [[nodiscard]] std::int64_t raid_target(StepContext& context, std::int64_t monster) const;
    void raid(StepContext& context, std::int64_t monster);
    void overrun(StepContext& context, std::int64_t monster, std::int64_t target, std::int64_t raid_event);
    // P5-03 9.4: the flight for fear after a raid on a standing target.
    void flight(StepContext& context, std::int64_t monster, std::int64_t target);
    // P5-03 9.1: the raids of a monster on a settlement still remembered.
    [[nodiscard]] std::vector<PlaceRaid> place_raids(std::int64_t monster, std::int64_t settlement) const;
    // P5-03 12.1, 12.2: a monster's name.
    [[nodiscard]] std::string wild_name(const StepContext& context, std::size_t kind, std::int64_t monster);
    [[nodiscard]] std::string place_name(const StepContext& context, std::size_t kind, std::int64_t monster,
                                         std::int64_t region, std::int64_t settlement, std::string& root);
    void hunts(StepContext& context, std::int64_t monster);
    void deaths_of_age_quiet(StepContext& context);
    // P5-07 6.2 to 6.7 (monsters_deep.cpp).
    void deepen(StepContext& context);
    void deep_waking(StepContext& context);
    // P5-07 r2 6.10: the warren delving (T4, Q4, after deepen).
    void warren_delving(StepContext& context);
    // 6.4 items 1 to 7 at one hold, a mine's (site > 0) or a warren's (site
    // 0); true when a horror woke.
    struct DeepHold {
        std::int64_t site = 0, settlement = 0, civ = 0, depth = 0, region = 0, cause = 0;
    };
    // P5-08 12.2: `certain` (a stir's flare-up) skips the wake draw.
    bool deep_wake_at(StepContext& context, const DeepHold& hold, bool certain);
    // P5-08 12.2: the stir (Q4, after the delving; at most one a step).
    void deep_stir(StepContext& context);
    // 6.4's candidate test for a mine (site > 0) or a warren (settlement t).
    [[nodiscard]] std::optional<DeepHold> mine_candidate(const StepContext& context, std::int64_t site) const;
    [[nodiscard]] std::optional<DeepHold> warren_candidate(const StepContext& context, std::int64_t settlement) const;
    // P5-07 r2 6.11: the allies that send aid to a waking of monster m at t.
    struct DeepAid {
        std::int64_t ally = 0, origin = 0, strength = 0;
    };
    [[nodiscard]] std::vector<DeepAid> deep_aid(const StepContext& context, std::int64_t monster, std::int64_t civ,
                                                const std::vector<std::int64_t>& hops) const;
    // 6.6: take(m, t, causes, plan); its hold_lost event, or 0 (and nothing
    // written) when t is its race's last refuge and the flight has no plan.
    // P5-08 12.7 A1: at a waking `plan` is the gate's plan (not recomputed);
    // nullptr: the take computes its own.
    std::int64_t deep_take(StepContext& context, std::int64_t monster, std::int64_t settlement,
                           const std::vector<std::int64_t>& causes, const DeepFlightPlan* plan);
    // 6.7: after a raid of a deep horror on t.
    void deep_after_raid(StepContext& context, std::int64_t monster, std::int64_t settlement, std::int64_t raid_event);
    // 6.5: the gate's odds for a hold of `people` of civilization c against kind k.
    [[nodiscard]] std::int64_t gate_odds(const StepContext& context, std::int64_t civ, std::int64_t people,
                                         std::size_t kind, std::int64_t aid) const;
    [[nodiscard]] std::int64_t prowess_of(std::int64_t race) const;
    [[nodiscard]] std::int64_t mining_of(std::int64_t race) const;
    [[nodiscard]] std::int64_t delving_of(std::int64_t race) const;
    [[nodiscard]] std::int64_t ally_join_of(std::int64_t race) const;
    void grow_warrens(std::size_t settlements);
    // 6.6 item 2: whether t is its race's last refuge.
    [[nodiscard]] bool deep_last_refuge(const StepContext& context, std::int64_t settlement) const;
    // 6.6.
    void outposts_left(StepContext& context);
    void outposts_raised(StepContext& context);
    [[nodiscard]] std::optional<Cell> wild_cell(const StepContext& context, std::int64_t region, bool height,
                                                std::span<const HeightCell> avoid = {}, std::int64_t radius = 0) const;
    void end_site(std::int64_t site, std::int64_t year, std::int64_t event, std::int64_t ending);
    // 9.
    [[nodiscard]] std::string lair_name(const StepContext& context, std::int64_t site, std::size_t kind,
                                        const std::string& root);
    [[nodiscard]] std::vector<std::int64_t> hops_from(std::int64_t region) const;
    Monster& monster(std::int64_t id);
    [[nodiscard]] const Monster& monster(std::int64_t id) const;
    Site& site(std::int64_t id);
    [[nodiscard]] std::int64_t lair_region(const Monster& m) const;
    [[nodiscard]] std::uint64_t key(std::size_t stream) const noexcept { return keys_[stream]; }

    PeoplesSubsystem& peoples_;
    MonstersTuning tuning_;
    std::vector<BeastDef> beasts_;
    std::vector<MonsterKindDef> kinds_;
    bool prepared_ = false;
    std::array<std::uint64_t, 16> keys_{};
    std::array<std::uint64_t, 9> deep_keys_{};  // P5-07 3: hist.deep.delve, wake, kind, gate, take; r2: aid,
                                                // warren_delve, warren_wake, warren_kind
    std::vector<std::pair<std::int64_t, std::int64_t>> delving_;    // (race id, delving_permille), ascending
    std::vector<std::pair<std::int64_t, std::int64_t>> ally_join_;  // (race id, ally_join_permille), ascending
    std::vector<std::uint8_t> warren_region_;  // per region: a warren may start there (6.10, 4.2)
    // P5-07 r2 5: per settlement (index id - 1) its warren depth, delver,
    // last warren_delved (0 none), woken and sealed.
    std::vector<std::int64_t> warren_depth_, warren_delver_, warren_event_;
    std::vector<std::uint8_t> warren_woken_, warren_sealed_;
    const AllianceView* alliances_ = nullptr;  // P5-07 r2 7.6
    RelationsSubsystem* relations_ = nullptr;   // P5-07 r2 7.6
    std::vector<std::pair<std::int64_t, std::int64_t>> mining_;  // (race id, mining_permille), ascending
    ConflictSubsystem* conflict_ = nullptr;      // P5-07 7.3  // P5-03: + hist.beasts.bold_kind, hist.monsters.fear; P5-06: hero_fall
    std::int64_t pressure_ = 1000;  // M
    std::int64_t threshold_ = 0;    // T_d
    std::int64_t region_count_ = 0;
    std::int64_t living_cap_ = 1;   // L_world
    std::int64_t map_size_ = 0;
    std::vector<std::uint8_t> islet_;                    // per region
    std::vector<std::vector<GeoView::Neighbour>> neighbours_;  // per region, ascending
    std::vector<std::int64_t> lair_count_;     // per region: open lairs
    std::vector<std::int64_t> shunned_count_;  // per region: open lairs on a settlement (7.2)
    std::vector<std::pair<std::int64_t, CultureNaming>> culture_naming_;  // (culture id, naming), ascending
    std::vector<std::pair<std::int64_t, std::string>> culture_keys_;      // (culture id, key), ascending
    std::vector<std::vector<std::uint32_t>> land_cells_;  // per region, row-major cell indexes with water 0
    std::vector<std::uint16_t> habitability_;
    std::vector<std::int16_t> elevation_;
    std::vector<std::pair<std::int64_t, std::int64_t>> prowess_;  // (race id, prowess_permille), ascending
    // Names (9): (culture key, culture id, naming) per monster kind, in kind order.
    std::vector<CultureNaming> kind_naming_;
    std::vector<std::int64_t> kind_culture_id_;
    stages::PlacenameData placenames_;
    // Beasts, index (region - 1) * species + (beast - 1).
    std::vector<std::int64_t> count_, capacity0_;
    std::vector<std::int64_t> habitat_kind_;  // monsters: index (region - 1) * kinds + kind index
    std::vector<std::int64_t> displaced_, growth_, menace_;  // per region
    std::vector<std::uint8_t> bold_;
    std::vector<std::int64_t> last_row_;  // per (region, beast): the count of its last row (0 none)
    std::vector<db::BeastPopulationRow> population_rows_;  // creation order
    bool beasts_updated_ = false;
    std::int64_t beast_phase_ = 0;
    std::int64_t beast_last_year_ = 0;
    bool finished_ = false;
    // Monsters and sites, index id - 1.
    std::vector<Monster> monsters_;
    std::vector<Site> sites_;
    std::vector<std::vector<WindowEntry>> windows_;  // per region
    std::int64_t cursor_ = 1;
    std::int64_t pending_flare_ = 0;
    // P5-08 12.2: the stirring candidate (a mine's site or a warren's
    // settlement, 0 none), its last deepening (the flare trigger), and the
    // first year of the current phase.
    std::int64_t stir_site_ = 0, stir_warren_ = 0, stir_event_ = 0;
    std::int64_t phase_seen_ = 0, phase_start_ = 0;
    std::array<std::uint64_t, 2> stir_keys_{};  // hist.deep.stir, hist.deep.warren_stir
    MonstersStats stats_;
    const RenamingSubsystem* renaming_ = nullptr;  // P5-03 12.2
    const FigureView* figures_ = nullptr;          // P5-06 7.1
    std::vector<std::int64_t> heroes_slain_;       // ruling G1: heroes slain in the T4 of heroes_slain_year_
    std::int64_t heroes_slain_year_ = -1;
    std::uint64_t name_key_ = 0;                   // P5-03 12.2: hist.names.monster
    std::vector<std::string> epithets_;            // P5-03 12.1: of the wild monsters' names, creation order
};

}  // namespace worldgen::history
