#pragma once

// Expansion and conflict (spec P3-06): dispositions and grudges, strain and
// claims between neighbours, the tension that triggers flare-ups, wars
// with their battles, sieges, sackings and captures, peace and cession, and
// the conquest of a civilization.
//
// ConflictSubsystem runs in steps T2, T6 and Q4 of the engine, after
// PeoplesSubsystem (P3-04 6.4, P3-06 8.2), and is the engine's one tension
// source. Everything is integer arithmetic; every random decision is a
// keyed draw on one of the five hist.conflict.* streams (3.3), keyed by
// civilization ids, the war's declaration event id and settlement ids.
// Members hold ids only (8.3 item 1).

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
#include "worldgen/history/engine.hpp"
#include "worldgen/history/peoples.hpp"

namespace worldgen::history {

// --- Tuning (12.1) -------------------------------------------------------------------------

// A value per settlement kind (levy_permille, walls_permille).
struct ConflictByKind {
    std::int64_t hamlet = 0;
    std::int64_t village = 0;
    std::int64_t town = 0;
    std::int64_t city = 0;
    std::int64_t fortress = 0;

    [[nodiscard]] std::int64_t of(SettlementKind kind) const noexcept;
};

// The camp table of 4.1 by camp pair.
struct ConflictCamp {
    std::int64_t good_good = 0, good_neutral = 0, good_evil = 0;
    std::int64_t neutral_neutral = 0, neutral_evil = 0, evil_evil = 0;

    // camp_table[min(ka, kb)][max(ka, kb)] for camps 0 good, 1 neutral, 2
    // evil. FatalError "history: conflict: camp <k> is not 0, 1 or 2".
    [[nodiscard]] std::int64_t of(std::int64_t ka, std::int64_t kb) const;
};

// The definition history.conflict, field for field (short names as 12.1
// states them for loss_*, sack_toll, capture_toll).
struct ConflictTuning {
    ConflictCamp camp;
    std::int64_t enmity_max = 0;
    std::int64_t border_friction = 0, want_weight = 0, hunger_full = 1;
    std::int64_t fear_share = 0, fear_weight = 0;
    std::int64_t tick_strain_permille = 0, quiet_strain_permille = 0, strain_max = 1;
    std::int64_t claim_strain = 1, declare_min = 0, declare_span = 1, declare_permille = 0, declare_ratio = 1;
    std::int64_t shelter_years = 0, march_hops = 1, cost_penalty = 0;
    ConflictByKind levy_permille;
    std::int64_t terrain_defense = 0, pass_defense = 0, river_defense = 0, fort_defense = 0;
    std::int64_t battle_permille = 0, loss_loser_permille = 0, loss_winner_permille = 0;
    std::int64_t garrison_permille = 0;
    ConflictByKind walls_permille{1, 1, 1, 1, 1};
    std::int64_t capital_walls = 0;
    std::int64_t sack_base = 0, sack_hostility = 0, sack_toll_permille = 0, capture_toll_permille = 0;
    std::int64_t exhaustion_permille = 1, war_max_years = 1, truce_years = 0;
    std::int64_t war_grudge = 0, capture_grudge = 0, secession_grudge = 0, grudge_decay_per_century = 0;
    std::int64_t last_refuge = 0;
    std::int64_t capital_sacked_salience = 0, capital_captured_salience = 0;
};

// The loaded definition. FatalError "history: tuning: the content has no
// definition history.conflict" if absent.
[[nodiscard]] ConflictTuning conflict_tuning_from(const data::ContentSet& content);

// The schema ranges and named checks of 12.1 on a tuning not read through
// the loader (16.4), in the table's order: "history: tuning:
// conflict.<field> outside its schema range" (record fields as
// conflict.<record>.<field>), then "history: tuning: claim_strain must not be
// above declare_min" and "history: tuning: declare_min + declare_span must
// not be above strain_max".
void check_conflict_tuning(const ConflictTuning& tuning);

// --- Pure rules (test seams; 4, 5.1, 5.2, 6, 7.4, 7.7) -------------------------------------------

// What P3-06 reads of a race: its row's camp and the race data's fields.
struct ConflictRace {
    std::int64_t id = 0;
    std::string key;
    std::int64_t camp = 0;        // 0 good, 1 neutral, 2 evil (P3-03 race.camp)
    std::int64_t aggression = 0;  // P3-01
    std::int64_t prowess = 1000;  // prowess_permille (12.2)
};

// One entry of front(x, y) (5.1): the region R of y's presence, the cost of
// reaching it and the region the army enters from (= R for a shared region).
struct FrontEntry {
    std::int64_t region = 0;
    std::int64_t cost = 0;
    std::int64_t from = 0;
    friend bool operator==(const FrontEntry&, const FrontEntry&) = default;
};

// The open borders with their costs, index region id - 1 (P3-05 4.4).
using ConflictGraph = std::vector<std::vector<PeoplesNeighbour>>;

// The cheapest paths of at most `hops` open borders from the regions of
// `from` (ascending, distinct): per region (index id - 1) the smallest
// (cost, last region), cost -1 when unreachable; a region of `from` has (0,
// itself).
struct ReachTable {
    std::vector<std::int64_t> cost;
    std::vector<std::int64_t> last;
};
[[nodiscard]] ReachTable reach_from(const ConflictGraph& graph, std::span<const std::int64_t> from, std::int64_t hops);
// front(x, y) of 5.1 from x's reach table and presence and y's presence
// (both ascending): the reachable entries in ascending region.
[[nodiscard]] std::vector<FrontEntry> front_of(const ReachTable& reach_x, std::span<const std::int64_t> presence_x,
                                               std::span<const std::int64_t> presence_y);
[[nodiscard]] std::vector<FrontEntry> front_of(const ConflictGraph& graph, std::span<const std::int64_t> presence_x,
                                               std::span<const std::int64_t> presence_y, std::int64_t hops);

// d(a, b) = camp(a, b) - grudge, h = max(0, -d) (4.1).
[[nodiscard]] std::int64_t disposition(const ConflictTuning& t, std::int64_t camp_a, std::int64_t camp_b,
                                       std::int64_t grudge);
[[nodiscard]] std::int64_t hostility(std::int64_t disposition) noexcept;
// want, share, fear and drive of 5.1.
[[nodiscard]] std::int64_t want_of(const ConflictTuning& t, bool wants_held_land, std::int64_t hunger);
[[nodiscard]] std::int64_t share_of(std::int64_t non_islet_held, std::int64_t non_islet_regions);
[[nodiscard]] std::int64_t fear_of(const ConflictTuning& t, std::int64_t share);
[[nodiscard]] std::int64_t drive_of(const ConflictTuning& t, std::int64_t disposition, std::int64_t want,
                                    std::int64_t fear);
// rate(a, b) of 5.2, left to right; k >= 1.
[[nodiscard]] std::int64_t strain_rate(std::int64_t aggression, std::int64_t drive, std::int64_t contact_cost);
// The strain after one update of 5.2 (years, the step's permille, the
// intensity I), capped at strain_max x 1000.
[[nodiscard]] std::int64_t strain_after(const ConflictTuning& t, std::int64_t strain, std::int64_t rate,
                                        std::int64_t years, std::int64_t permille, std::int64_t intensity);
// The declaration chance of 7.2 item 3 (s the strain in tension units).
[[nodiscard]] std::int64_t declare_permille(const ConflictTuning& t, std::int64_t s, std::int64_t force_a,
                                            std::int64_t force_b, std::int64_t aggression_a);
// force(c) of 6.1 from (population, kind) of its standing visible
// settlements and its race's prowess.
[[nodiscard]] std::int64_t force_of(const ConflictTuning& t, std::span<const std::pair<std::int64_t, SettlementKind>> settlements,
                                    std::int64_t prowess);
// cover(R) of 6.3 (rough and the four shares, per mille).
[[nodiscard]] std::int64_t cover_of(const PeoplesRegion& region, std::span<const std::int64_t> cover_biomes);
// def(R) of 6.3.
struct DefenceTerms {
    std::int64_t cover = 0;
    bool pass = false;   // R_from != R, walk_pairs = 0, pass_id > 0
    bool river = false;  // R_from != R, a large-river border
    bool fort = false;   // the defender has a standing visible fortress in R
};
[[nodiscard]] std::int64_t defence_of(const ConflictTuning& t, const DefenceTerms& terms);
// The win chance p of 7.3 item 3.
[[nodiscard]] std::int64_t battle_permille_of(std::int64_t attack, std::int64_t defence_force, std::int64_t def);
// The garrison G and the fall chance ps of 7.3 item 7.
[[nodiscard]] std::int64_t garrison_of(const ConflictTuning& t, std::int64_t population, std::int64_t prowess,
                                       SettlementKind kind, bool capital);
[[nodiscard]] std::int64_t siege_permille_of(std::int64_t attack_left, std::int64_t garrison);
// 7.4: the losses of each settlement (populations in ascending id) for
// `dead`, already capped at the total; the remainder from the most populous
// after the shares (then the first), at most its population.
[[nodiscard]] std::vector<std::int64_t> casualty_losses(std::span<const std::int64_t> populations, std::int64_t dead);
// The war score of 7.7: positive the attacker leads.
[[nodiscard]] std::int64_t war_score(std::int64_t won_a, std::int64_t won_b, std::int64_t taken_a, std::int64_t taken_b);

// --- The subsystem (13) ----------------------------------------------------------------------

// A war (7.1), keyed by its war_declared event id.
struct ConflictWar {
    std::int64_t declared = 0;  // the war_declared event: the war's key for draws
    std::int64_t attacker = 0;
    std::int64_t defender = 0;
    std::int64_t goal = 0;
    std::int64_t start = 0;
    std::int64_t last_event = 0;
    std::int64_t initiative = 0;
    bool open = true;
    // Per side, index 0 the attacker, 1 the defender.
    std::array<std::int64_t, 2> won{};
    std::array<std::int64_t, 2> taken{};
    std::array<std::int64_t, 2> dead{};
    std::array<std::int64_t, 2> pop0{};
};

class ConflictSubsystem final : public Subsystem, public TensionSource {
public:
    using Pair = std::pair<std::int64_t, std::int64_t>;

    // Reads history.conflict (and checks it); history.peoples' intensity
    // and the race data are read at the first step.
    ConflictSubsystem(PeoplesSubsystem& peoples, const data::ContentSet& content);
    // Test seam: a hand-made tuning, checked by check_conflict_tuning.
    ConflictSubsystem(PeoplesSubsystem& peoples, const ConflictTuning& tuning);
    ConflictSubsystem(const ConflictSubsystem&) = delete;
    ConflictSubsystem& operator=(const ConflictSubsystem&) = delete;

    [[nodiscard]] std::span<const Step> steps() const override;  // T2, T6, Q4
    void run(StepContext& context) override;
    // 5.4: the largest strain among claimed pairs that could fight; reads
    // only (8.3 item 2).
    TensionReport tension(const StepContext& context) override;

    [[nodiscard]] const ConflictTuning& tuning() const noexcept { return tuning_; }

    // Inspection (tests; not used by the engine).
    [[nodiscard]] const std::vector<ConflictWar>& wars() const noexcept { return wars_; }
    [[nodiscard]] std::int64_t strain(std::int64_t a, std::int64_t b) const;
    [[nodiscard]] std::int64_t grudge(std::int64_t a, std::int64_t b) const;
    [[nodiscard]] std::optional<std::pair<std::int64_t, std::int64_t>> claim(std::int64_t a, std::int64_t b) const;

private:
    struct Claim {
        std::int64_t event = 0;
        std::int64_t region = 0;
    };
    struct Peace {
        std::int64_t year = 0;
        std::int64_t event = 0;
    };

    void prepare(const StepContext& context);
    [[nodiscard]] const ConflictRace& race_of_civ(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] bool alive_visible(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] std::vector<std::int64_t> alive_civs(const StepContext& context) const;
    [[nodiscard]] std::vector<std::int64_t> presence(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] std::vector<std::int64_t> visible_settlements(const StepContext& context, std::int64_t civ,
                                                                std::int64_t region) const;
    [[nodiscard]] std::int64_t force(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] std::int64_t open_wars_of(std::int64_t civ) const;
    [[nodiscard]] bool at_war(std::int64_t a, std::int64_t b) const;
    [[nodiscard]] bool attacking(std::int64_t a) const;
    [[nodiscard]] bool truce_over(const StepContext& context, std::int64_t a, std::int64_t b) const;
    [[nodiscard]] std::int64_t disposition_of(const StepContext& context, std::int64_t a, std::int64_t b) const;
    [[nodiscard]] std::int64_t target_key_value(const StepContext& context, std::int64_t race,
                                                const FrontEntry& e) const;
    [[nodiscard]] FrontEntry best_target(const StepContext& context, std::int64_t race,
                                         std::span<const FrontEntry> front) const;
    [[nodiscard]] std::vector<FrontEntry> live_front(const StepContext& context, std::int64_t x, std::int64_t y) const;
    [[nodiscard]] std::optional<std::pair<TensionReport, Pair>> best_claim(const StepContext& context) const;

    // Steps (8.1, 8.2).
    void housekeeping(StepContext& context);
    void decay_grudges(std::int64_t years);
    void update_strain(StepContext& context, std::int64_t years, std::int64_t permille);
    void declarations(StepContext& context);
    void declare(StepContext& context, std::int64_t a, std::int64_t b);
    void fight(StepContext& context);
    void quiet_ends(StepContext& context);

    // War (conflict_war.cpp; 7.3 to 7.7).
    enum class Fought : std::uint8_t { no_front, no_battle, battle };
    Fought battle(StepContext& context, std::size_t war);
    void siege(StepContext& context, std::size_t war, std::int64_t x, std::int64_t y, std::int64_t region,
               std::int64_t attack, std::int64_t dead_x, std::int64_t battle_event);
    void fall(StepContext& context, std::size_t war, std::int64_t x, std::int64_t y, std::int64_t settlement,
              std::int64_t siege_event);
    void conquer(StepContext& context, std::int64_t y, std::int64_t x, std::int64_t cause, std::int64_t region);
    std::int64_t casualties(StepContext& context, std::int64_t civ, std::int64_t dead, std::int64_t event);
    void refugees(StepContext& context, std::int64_t civ, std::int64_t from_settlement, std::int64_t people);
    void end_checks(StepContext& context, std::size_t war, bool no_front);
    // 7.7: ends the war with winner (0 a truce); false when a quiet step's
    // budget does not allow it (the war stays open).
    bool end_war(StepContext& context, std::size_t war, std::int64_t winner);
    [[nodiscard]] std::int64_t score_winner(const ConflictWar& w) const;
    void add_grudge(std::int64_t a, std::int64_t b, std::int64_t amount);
    void drop_pair(std::int64_t a, std::int64_t b);  // strains 0, claims dropped, both directions
    void close_relations_of(StepContext& context, std::int64_t civ, std::int64_t event);

    PeoplesSubsystem& peoples_;
    ConflictTuning tuning_;
    bool prepared_ = false;
    std::array<std::uint64_t, 5> keys_{};
    std::int64_t intensity_ = 1000;
    std::vector<ConflictRace> races_;          // race row order (ascending id)
    ConflictGraph graph_;                      // open borders, index region - 1
    std::map<Pair, std::size_t> border_;       // (min, max) region pair -> index of its first border row
    std::vector<std::int64_t> cover_biomes_;   // biome ids of temperate_forest, wet_woodland, fen, marsh
    std::int64_t non_islet_ = 0;

    // Members (3.2), keyed by ids.
    std::map<Pair, std::int64_t> strain_;
    std::map<Pair, Claim> claims_;
    std::map<Pair, std::int64_t> grudge_;       // (min, max)
    std::map<Pair, Peace> last_peace_;          // (min, max)
    std::vector<ConflictWar> wars_;             // declaration order
    std::set<Pair> enmity_opened_;
    std::set<std::int64_t> seen_;
    std::map<Pair, std::int64_t> contact_;      // this step's contact cost k, set in each strain update
    std::int64_t last_t2_phase_ = 0;
};

}  // namespace worldgen::history
