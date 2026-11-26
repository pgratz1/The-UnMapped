#pragma once

// Race relations, mixed settlements and refugee migrations (spec
// docs/specs/p5-03-race-relations.md part A): the disposition of every pair
// of races present (base, memory, bands and their turns, meetings), the
// share and standing of each race in every settlement, the people ledger
// that turns P3's lost people into refugees seeking shelter with another
// people, and expulsions.
//
// RelationsSubsystem runs in steps T1, T4, T6, Q1 and Q4 after monsters
// (6.1; P5-03a 3.6). It is the people ledger of PeoplesSubsystem and the disposition
// view of ConflictSubsystem (7.1, 7.2). Keyed draws only (streams
// hist.relations.expel and, P5-03a 5.1, hist.relations.settlers); integer arithmetic only; members hold ids. It
// changes State only through set_population and
// PeoplesSubsystem::note_reduction (6.6).

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/engine.hpp"
#include "worldgen/history/peoples.hpp"

namespace worldgen::history {

// Definition history.relations (4.2), field for field.
struct RelationsTuning {
    std::int64_t hatred_max = -500, friend_min = 150, band_margin = 50;
    std::int64_t memory_min = -250, memory_max = 250, memory_decay_per_century = 40;
    std::int64_t war_memory = 25, conquest_memory = 50, sack_memory = 10, capture_memory = 5;
    std::int64_t expel_memory = 30, shelter_memory = 15;
    std::int64_t meet_hops = 3, migrate_hops = 2, migrate_min = 40, welcome_min = -200;
    std::int64_t subject_years = 200, conquest_settlers_permille = 100;
    std::int64_t drift_permille_per_century = 100, fade_permille = 20, mixed_min_permille = 100;
    std::int64_t expel_permille = 30, expel_share_max = 500;
    // P5-03a 10.1 (generator 0.25.0). The defaults of a hand-made tuning
    // switch the new rules off (no peace, no common enemy, no meeting by
    // word, no settlers' span, no gap), so a 0.24.0 test tuning keeps its
    // meaning.
    std::int64_t peace_years = 100, peace_memory_per_century = 0, peace_memory_max = 0, contact_hops = 1;
    std::int64_t foe_years = 100, foe_war_memory = 0, foe_monster_memory = 0;
    std::int64_t meet_sealed_hops = -1, meet_sealed_years = 300;
    std::int64_t conquest_settlers_span_permille = 0, expel_gap_years = 0;
    // P5-04a 14.2 (generator 0.28.0); the defaults are inert.
    std::int64_t turn_cooldown_years = 0, expel_settled_years = 100000, expel_fresh_years = 0;

    friend bool operator==(const RelationsTuning&, const RelationsTuning&) = default;
};

// The loaded definition. FatalError "history: tuning: the content has no
// definition history.relations" when it is absent.
[[nodiscard]] RelationsTuning relations_tuning_from(const data::ContentSet& content);

// One stance of a race toward another (4.1), by race key.
struct RaceStance {
    std::string race;  // the race the stance is toward
    std::int64_t value = 0;
    friend bool operator==(const RaceStance&, const RaceStance&) = default;
};

// The stances of race `key` in the content (empty when it has none).
[[nodiscard]] std::vector<RaceStance> race_stances_from(const data::ContentSet& content, std::string_view key);

// Standing codes (6.3) and bands (6.2.3).
enum class Standing : std::int64_t { subject = 0, tolerated = 1, accepted = 2, ruling = 3 };
enum class Band : std::int64_t { hatred = 0, wariness = 1, friendship = 2 };

// 6.3.2's largest-remainder split: `total` over `weights` (non-negative,
// sum > 0) as shares_i = w_i x total div W, the remainder one each in the
// order (w_i x total mod W descending, index ascending). Test seam.
[[nodiscard]] std::vector<std::int64_t> largest_remainder(std::span<const std::int64_t> weights, std::int64_t total);

// Counts for the calibration sweep: not stored in the world.
struct RelationsStats {
    std::int64_t displaced_groups = 0, displaced_people = 0;
    std::int64_t lost_groups = 0, lost_people = 0;  // groups with no host (or too small), and the rest of a group
    std::int64_t arrivals = 0, arrived_people = 0;
    std::int64_t expulsions = 0;
};

class RelationsSubsystem final : public Subsystem, public PeopleLedger, public DispositionView {
public:
    // The shipped configuration: the tuning and race stances of `content`.
    RelationsSubsystem(PeoplesSubsystem& peoples, const ConflictSubsystem& conflict, const data::ContentSet& content);
    // A test seam: a hand-made tuning; the stances are still read from the
    // content at the first step.
    RelationsSubsystem(PeoplesSubsystem& peoples, const ConflictSubsystem& conflict, RelationsTuning tuning);
    // P5-03a 8 (A2): the shipped configuration with B(a, b) and the pairs
    // computed at construction from the inputs and the content, so that
    // disposition and hatred_event are valid before the first step.
    RelationsSubsystem(PeoplesSubsystem& peoples, const ConflictSubsystem& conflict, const data::ContentSet& content,
                       const HistoryInputs& inputs);
    RelationsSubsystem(const RelationsSubsystem&) = delete;
    RelationsSubsystem& operator=(const RelationsSubsystem&) = delete;

    [[nodiscard]] std::span<const Step> steps() const override;
    void run(StepContext& context) override;

    // 6.4.1 (PeopleLedger): record only.
    void moved(const State& state, std::int64_t from, std::int64_t to, std::int64_t people,
               std::int64_t cause) override;
    void displaced(const State& state, std::int64_t from, std::int64_t race, std::int64_t people,
                   std::int64_t cause) override;
    // 7.1 (DispositionView). FatalError "history: relations: dispositions
    // read before the first step" before the subsystem's first step.
    [[nodiscard]] std::int64_t disposition(std::int64_t race_a, std::int64_t race_b) const override;
    [[nodiscard]] std::int64_t hatred_event(std::int64_t race_a, std::int64_t race_b) const override;
    // P5-03a 13 (b): the pair's band code (the plain band of B before a
    // meeting) and whether it has met.
    [[nodiscard]] std::int64_t band(std::int64_t race_a, std::int64_t race_b) const override;
    [[nodiscard]] bool met(std::int64_t race_a, std::int64_t race_b) const override;
    // P5-04 7.13: the pair's last disposition_turned (0 none).
    [[nodiscard]] std::int64_t turn_event(std::int64_t race_a, std::int64_t race_b) const override;
    // P5-04 6.8: the pair's peoples_met event (0 when unmet or unknown).
    [[nodiscard]] std::int64_t met_event(std::int64_t race_a, std::int64_t race_b) const;

    // P5-03a 3.5: the one path by which an event moves a pair's memory. When
    // the pair has met: M += units x 1000, clamped to its bounds, and the
    // event is kept as a contribution of the sign of units (none for 0).
    void credit(std::int64_t race_a, std::int64_t race_b, std::int64_t units, std::int64_t event);

    // 6.7: the rows of race_relation and settlement_race of year E - 1,
    // called by the stage once the engine has run. Idempotent.
    void finish(std::int64_t end_year);
    // The rows, each in its primary-key order.
    [[nodiscard]] std::vector<db::RaceRelationRow> race_relation_rows() const;
    [[nodiscard]] std::vector<db::SettlementRaceRow> settlement_race_rows() const;

    // Inspection (tests).
    [[nodiscard]] const RelationsTuning& tuning() const noexcept { return tuning_; }
    [[nodiscard]] const RelationsStats& stats() const noexcept { return stats_; }
    // B(a, b) of 6.2.1 (0 for an unknown race).
    [[nodiscard]] std::int64_t base(std::int64_t race_a, std::int64_t race_b) const;
    // The pair's memory in milli-units, 0 when unmet or unknown.
    [[nodiscard]] std::int64_t memory(std::int64_t race_a, std::int64_t race_b) const;
    // The band of 6.3.5 (the pair's band if met, else the plain band of B).
    [[nodiscard]] Band band_of(std::int64_t race_a, std::int64_t race_b) const;
    struct Share {
        std::int64_t race = 0;
        std::int64_t share = 0;
        Standing standing = Standing::ruling;
        friend bool operator==(const Share&, const Share&) = default;
    };
    // The entries the subsystem holds for a settlement (ascending race).
    [[nodiscard]] std::vector<Share> shares(std::int64_t settlement) const;

private:
    struct Pair {
        std::int64_t a = 0, b = 0;
        std::int64_t base = 0;
        std::int64_t memory = 0;  // milli-units
        bool met = false;
        std::int64_t met_year = 0, met_event = 0;
        Band band = Band::wariness;
        std::int64_t turn_year = 0, turn_event = 0;
        std::int64_t hatred_event = 0;
        // P5-03a 3.5: the last at most 8 contributions of each sign.
        std::vector<std::int64_t> warmer, colder;
        std::int64_t strife_year = 0;  // P5-03a 3.2
        std::int64_t word_since = -1;  // P5-03a 4.2; -1 unset (P5-04a 11 item 7)
    };
    struct Entry {
        std::int64_t race = 0;
        std::int64_t share = 0;
        Standing standing = Standing::ruling;
        std::int64_t since_year = 0, since_event = 0;
        bool conquered = false;
        bool mixed_told = false;
    };
    struct Place {
        std::int64_t owner = 0;   // o_s
        std::int64_t ruling = 0;  // q_s
        std::vector<Entry> entries;  // ascending race
    };
    struct Record {
        std::int64_t from = 0, to = 0, race = 0, people = 0, cause = 0;
        std::int64_t to_before = 0;  // P_to - people at the call (moves)
        std::int64_t owner = 0;      // o_from at the call
        std::vector<Entry> entries;  // from's entries at the call
    };
    struct RowValue {
        std::int64_t year = 0, a = 0, b = 0;  // (year, disposition, band) or (year, share, standing)
    };
    struct RaceRows {
        std::int64_t race = 0;
        std::vector<RowValue> rows;
    };

    void prepare(const StepContext& context);
    void prepare_from(const HistoryInputs& inputs, const data::ContentSet& content);
    // P5-03a 4.1: hops on the border graph from `region` (index region - 1;
    // -1 unreachable), cached per source region.
    [[nodiscard]] const std::vector<std::int64_t>& border_hops(const GeoView& geo, std::int64_t region);
    [[nodiscard]] const std::vector<std::int64_t>& within_contact(const GeoView& geo, std::int64_t region);
    // Per region (index region - 1): the bits (race index) of the races
    // present, over the alive visible civilizations (P5-03a 3.2).
    [[nodiscard]] std::vector<std::uint64_t> race_presence(const StepContext& context) const;
    [[nodiscard]] std::size_t race_index(std::int64_t race) const;
    void know_settlements(const State& state);
    [[nodiscard]] Pair* pair_of(std::int64_t a, std::int64_t b);
    [[nodiscard]] const Pair* pair_of(std::int64_t a, std::int64_t b) const;
    [[nodiscard]] std::int64_t d_of(const Pair& p) const;
    [[nodiscard]] Band plain_band(std::int64_t d) const;
    [[nodiscard]] Standing target(const StepContext& context, std::int64_t ruling, const Entry& e) const;
    [[nodiscard]] std::int64_t race_of_civ(const State& state, std::int64_t civ) const;
    [[nodiscard]] Place& place(std::int64_t settlement);
    // 6.3.2.
    void mix(const StepContext& context, std::int64_t settlement, std::span<const Entry> group, std::int64_t people,
             std::int64_t before, std::int64_t event);
    void fill_with(Place& p, std::int64_t keep_race, std::int64_t keep_share);
    // Steps.
    void pending_moves(const StepContext& context);
    void pending_displacements(StepContext& context);
    void read_log(const StepContext& context);
    void owner_changes(const StepContext& context);
    void expulsions(StepContext& context);
    void decay(std::int64_t years);
    void war_foes(const StepContext& context);
    void peace(const StepContext& context);
    void monster_foe(const StepContext& context, std::int64_t event);
    void turns(StepContext& context);
    void meetings(StepContext& context);
    void drift(const StepContext& context);
    void standings(StepContext& context);
    void mixed(StepContext& context);
    void phase_rows(std::int64_t year);
    void write_pair_row(Pair& p, std::int64_t year);
    void write_settlement_rows(const State& state, std::int64_t year);
    [[nodiscard]] std::vector<std::int64_t> presence(const StepContext& context, std::int64_t civ) const;

    PeoplesSubsystem& peoples_;
    const ConflictSubsystem& conflict_;
    RelationsTuning tuning_;
    bool prepared_ = false;
    bool finished_ = false;
    std::uint64_t expel_key_ = 0;
    std::uint64_t settlers_key_ = 0;
    std::vector<std::int64_t> race_ids_;      // every race row, ascending
    std::vector<std::int64_t> race_camp_;     // same index
    std::vector<std::int64_t> race_awake_;    // same index: awakening year, -1 none
    std::vector<std::int64_t> base_;          // [i * races + j]
    std::vector<Pair> pairs_;                 // the races present, ascending (a, b)
    std::vector<std::vector<RowValue>> pair_rows_;  // per pair
    std::vector<Place> places_;               // index settlement - 1
    std::vector<std::vector<RaceRows>> place_rows_;  // per settlement, ascending race
    std::vector<std::uint8_t> ruined_;        // per settlement: seen ruined (no more rows)
    std::vector<Record> moves_, displacements_;
    std::int64_t cursor_ = 1;
    // P5-03a 3.3: per (civilization or monster, race) the last year and
    // event; the credited (race_a, race_b, enemy) triples.
    struct Seen {
        std::int64_t year = 0, event = 0;
    };
    std::map<std::pair<std::int64_t, std::int64_t>, Seen> civ_foes_, monster_foes_;
    std::set<std::tuple<std::int64_t, std::int64_t, std::int64_t>> civ_credited_, monster_credited_;
    std::vector<std::uint8_t> at_war_pairs_;  // per pair: an open war joins its races this step
    std::vector<std::vector<std::int64_t>> border_hops_;  // per source region, empty until asked
    std::vector<std::vector<std::int64_t>> within_;       // per region: within contact_hops (3.2)
    std::map<std::int64_t, std::int64_t> last_expulsion_;  // per realm: the year of its last expulsion (5.3)
    std::int64_t last_drift_ = 0;
    std::int64_t last_phase_ = 0;
    std::int64_t last_year_ = 0;  // the last year a step ran (for finish)
    RelationsStats stats_;
};

}  // namespace worldgen::history
