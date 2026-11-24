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
    // r9 20.9 (P3-09, generator 0.18.0). The defaults of a hand-made tuning
    // leave 0.17.0's rules (no flight, no weariness, tribute or revenge,
    // levies from every reachable settlement, no odds minimum, the full
    // truce), except that a truce shortens by nothing only below aggression
    // 999.
    // r17 22.2: hidden_years is a race field (ConflictRace::hidden_years).
    std::int64_t refuge_flight = 0, flight_hops = 1, weary_wars = 0, tribute_permille = 0;
    // r14 21.9 (generator 0.19.0): odds_min is a race field (odds_min_permille).
    std::int64_t revenge_aggression = 0, levy_hops = 16, odds_floor = 0;
    std::int64_t truce_break_aggression = 999, enmity_hops = 1;
    // r10 20.19 (P3-09 r3): the kill bound, the search for a concealed people.
    std::int64_t kill_max_permille = 10000, find_permille = 1000, search_years = 100;
    // r14 21.7 (generator 0.19.0): the weary years after a failed search (0:
    // the rule off) and the salience of every search_abandoned after the
    // first of a world (a hand-made tuning: 40, the shipped base salience).
    std::int64_t search_weary_years = 0, search_repeat_salience = 40;
    // r15 21.12 item 2: the strain of an awakening quarrel (x 1000 in strain).
    std::int64_t quarrel_strain = 600;
    // r17 22.1: the standing settlements a civilization needs before its
    // awakening quarrel claim is pursued (a hand-made tuning: 1, the rule off).
    std::int64_t quarrel_min_settlements = 1;
    // r22 24.2 (generator 0.21.0): the least strength at which the awakening
    // quarrel claim is pursued; r22 24.5: the spread of the keyed factor on
    // each side's battle toll. A hand-made tuning: 0 and 0, both rules off.
    std::int64_t quarrel_strength_permille = 0;
    std::int64_t toll_spread_permille = 0;
    std::int64_t capital_sacked_salience = 0, capital_captured_salience = 0;
    // P5-04 4.3 (generator 0.26.0). The defaults of a hand-made tuning keep
    // 0.25.0's rules while no alliance, trade or mine exists (7.14).
    std::int64_t allied_strain_permille = 250, ally_deterrence_permille = 300, ally_effort_permille = 1000;
    std::int64_t trade_strength_permille = 50, trade_strength_max = 3;
    std::int64_t forge_strength_permille = 50, arms_strength_permille = 100;
};

// The loaded definition. FatalError "history: tuning: the content has no
// definition history.conflict" if absent.
[[nodiscard]] ConflictTuning conflict_tuning_from(const data::ContentSet& content);

// The schema ranges and named checks of 12.1 on a tuning not read through
// the loader (16.4), in the table's order: "history: tuning:
// conflict.<field> outside its schema range" (record fields as
// conflict.<record>.<field>), then "history: tuning: claim_strain must not be
// above declare_min", "history: tuning: declare_min + declare_span must
// not be above strain_max" and (r9) "history: tuning: enmity_hops must not
// be below march_hops".
void check_conflict_tuning(const ConflictTuning& tuning);

// --- Pure rules (test seams; 4, 5.1, 5.2, 6, 7.4, 7.7) -------------------------------------------

// What P3-06 reads of a race: its row's camp and the race data's fields.
struct ConflictRace {
    std::int64_t id = 0;
    std::string key;
    std::int64_t camp = 0;        // 0 good, 1 neutral, 2 evil (P3-03 race.camp)
    std::int64_t aggression = 0;  // P3-01
    std::int64_t prowess = 1000;  // prowess_permille (12.2)
    // r9 20.12 (P3-01 r9 3.2, generator 0.18.0).
    std::int64_t muster = 1000;        // muster_permille (20.6)
    std::int64_t conceal_min = 1001;   // 20.7; 1001 never
    std::int64_t home_defense = 0;     // home_defense_permille (20.7)
    // r14 21.9, 21.12 (P3-01 r11 3.5, generator 0.19.0).
    std::int64_t odds_min = 0;            // odds_min_permille
    std::int64_t awakening_quarrel = 0;   // 1: a quarrel at its awakening
    // r17 22.2 (P3-01 r13 3.6): the years a fled refuge of the race stays
    // hidden and its civilization lies low.
    std::int64_t hidden_years = 300;
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
// settlements and its race's prowess (and, r9 20.6 item 1, muster: levy x
// prowess div 1000 x muster div 1000; the three-argument form is muster 1000).
[[nodiscard]] std::int64_t force_of(const ConflictTuning& t, std::span<const std::pair<std::int64_t, SettlementKind>> settlements,
                                    std::int64_t prowess);
[[nodiscard]] std::int64_t force_of(const ConflictTuning& t, std::span<const std::pair<std::int64_t, SettlementKind>> settlements,
                                    std::int64_t prowess, std::int64_t muster);
// r9 20.6 item 3: the odds of a declaration, from a's force, b's force
// weighted by the defence of the claimed region (fb = force(b) x def div
// 1000, computed by the caller) and a's effective aggression g'.
[[nodiscard]] std::int64_t declare_odds(const ConflictTuning& t, std::int64_t force_a, std::int64_t weighted_force_b,
                                        std::int64_t aggression);
// r9: the declaration chance of 7.2 item 3 for strain s (tension units) at
// those odds: 0 below declare_min.
[[nodiscard]] std::int64_t declare_chance(const ConflictTuning& t, std::int64_t s, std::int64_t odds);
// r9 20.6 item 4: the years of a's truce after a peace (aggression g_a).
[[nodiscard]] std::int64_t truce_length(const ConflictTuning& t, std::int64_t aggression);
// cover(R) of 6.3 (rough and the four shares, per mille).
[[nodiscard]] std::int64_t cover_of(const PeoplesRegion& region, std::span<const std::int64_t> cover_biomes);
// def(R) of 6.3.
struct DefenceTerms {
    std::int64_t cover = 0;
    bool pass = false;   // R_from != R, walk_pairs = 0, pass_id > 0
    bool river = false;  // R_from != R, a large-river border
    bool fort = false;   // the defender has a standing visible fortress in R
    // r9 20.7 item 2: the defender's race's home_defense_permille and its
    // biome term B(R, r_y); adds home_defense x max(0, B) div 1000.
    std::int64_t home_defense = 0;
    std::int64_t biome_term = 0;
};
[[nodiscard]] std::int64_t defence_of(const ConflictTuning& t, const DefenceTerms& terms);
// The win chance p of 7.3 item 3.
[[nodiscard]] std::int64_t battle_permille_of(std::int64_t attack, std::int64_t defence_force, std::int64_t def);
// The garrison G and the fall chance ps of 7.3 item 7.
[[nodiscard]] std::int64_t garrison_of(const ConflictTuning& t, std::int64_t population, std::int64_t prowess,
                                       SettlementKind kind, bool capital);
// r9 20.7 item 2: the garrison with home defence, G x (1000 + home_defense x
// max(0, B) div 1000) div 1000 after the walls.
[[nodiscard]] std::int64_t garrison_of(const ConflictTuning& t, std::int64_t population, std::int64_t prowess,
                                       SettlementKind kind, bool capital, std::int64_t home_defense,
                                       std::int64_t biome_term);
[[nodiscard]] std::int64_t siege_permille_of(std::int64_t attack_left, std::int64_t garrison);
// 7.4: the losses of each settlement (populations in ascending id) for
// `dead`, already capped at the total; the remainder from the most populous
// after the shares (then the first), at most its population.
[[nodiscard]] std::vector<std::int64_t> casualty_losses(std::span<const std::int64_t> populations, std::int64_t dead);
// The war score of 7.7 (0.16.0 to r9): positive the attacker leads.
[[nodiscard]] std::int64_t war_score(std::int64_t won_a, std::int64_t won_b, std::int64_t taken_a, std::int64_t taken_b);
// r10 20.15 item 1: s = (dead(b) - dead(a)) + (taken(a) - taken(b)), with
// taken the people of the settlements each side took; positive a leads.
[[nodiscard]] std::int64_t war_score_people(std::int64_t dead_a, std::int64_t dead_b, std::int64_t taken_a,
                                            std::int64_t taken_b);
// r10 20.16: the defeated's dead, min(F_L x loss_loser, F_W x kill_max) div 1000.
[[nodiscard]] std::int64_t loser_dead_of(const ConflictTuning& t, std::int64_t force_winner, std::int64_t force_loser);

// The war's end check of 7.7 in T6 as 21.6 orders it (r17 22.5): 1. aim
// achieved, 2. the goal lost to no one or to a third party, 3. 7.7 items 1,
// 3 and 4 (exhaustion, then no front or the length). `by_score` is the
// score's winner after the last-battle rule (0 a truce); `ex_a`, `ex_b` the
// sides' exhaustion; `over` no front or the war's length reached.
struct EndDecision {
    bool ends = false;
    std::int64_t winner = 0;  // 0: a truce
    bool sued = false;        // peace (the loser is exhausted), else war_won
    friend bool operator==(const EndDecision&, const EndDecision&) = default;
};
[[nodiscard]] EndDecision end_decision(std::int64_t attacker, std::int64_t defender, bool aim, bool defender_there,
                                       bool ex_a, bool ex_b, std::int64_t by_score, bool over) noexcept;

// P5-03 7.1: what conflict reads of the peoples' dispositions
// (RelationsSubsystem). D(a, b) is symmetric, D(a, a) the camp value.
class DispositionView {
public:
    virtual ~DispositionView() = default;
    [[nodiscard]] virtual std::int64_t disposition(std::int64_t race_a, std::int64_t race_b) const = 0;
    // The pair's last disposition_turned into band 0 (hatred), 0 when none.
    [[nodiscard]] virtual std::int64_t hatred_event(std::int64_t race_a, std::int64_t race_b) const = 0;
    // P5-03a 13 (b), for P5-04 (conflict does not read them): the pair's band
    // code (0 hatred, 1 wariness, 2 friendship; the plain band of B before a
    // meeting) and whether it has met. The defaults serve a view that keeps
    // no bands.
    [[nodiscard]] virtual std::int64_t band(std::int64_t race_a, std::int64_t race_b) const {
        (void)race_a;
        (void)race_b;
        return 1;
    }
    [[nodiscard]] virtual bool met(std::int64_t race_a, std::int64_t race_b) const {
        (void)race_a;
        (void)race_b;
        return false;
    }
    // P5-04 7.13: the pair's last disposition_turned, 0 when none.
    [[nodiscard]] virtual std::int64_t turn_event(std::int64_t race_a, std::int64_t race_b) const {
        (void)race_a;
        (void)race_b;
        return 0;
    }
};

// P5-04 7.1: what conflict reads of alliances, trade and arms
// (AlliancesSubsystem). With no view set, or a view with no alliance, no
// trade and no mine, conflict is as at 0.25.0.
class AllianceView {
public:
    virtual ~AllianceView() = default;
    [[nodiscard]] virtual bool allied(std::int64_t a, std::int64_t b) const = 0;           // an open alliance
    [[nodiscard]] virtual std::span<const std::int64_t> allies(std::int64_t c) const = 0;  // ascending
    [[nodiscard]] virtual std::int64_t trade_partners(std::int64_t c) const = 0;           // open trades of c
    [[nodiscard]] virtual bool forged(std::int64_t c) const = 0;                           // c has an open mine
    [[nodiscard]] virtual bool arms(std::int64_t c) const = 0;                             // c buys arms
};

// P5-04 7.6: an ally that joined a war, in join order.
struct ConflictAlly {
    std::int64_t civ = 0;
    std::int64_t side = 0;  // 0 the defender's, 1 the attacker's
    std::int64_t join_event = 0;
    std::int64_t dead = 0;
    std::int64_t pop0 = 0;
    bool open = true;
    // How the record closed: 0 open, 1 the war's end, 2 withdrew (7.9), 3 a
    // side's end (7.11), 4 the ally's own end; and the closing event.
    std::int64_t closed_by = 0;
    std::int64_t close_event = 0;
};

// P5-04 7.15: what a won war leaves to share.
struct WarSpoils {
    std::int64_t winner = 0, loser = 0;  // truce, both ended: 0, 0
    std::int64_t end_event = 0, goal_region = 0;
    std::int64_t tribute_people = 0, tribute_event = 0, plunder = 0;  // the winner's plunder
    std::int64_t winner_dead = 0, winner_pop0 = 0;                    // the principal's own
    std::int64_t side_dead = 0;                                       // the winner's side (7.8)
};

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
    std::int64_t goal_owner = 0;  // r9 20.4: the goal's owner at the declaration
    bool open = true;
    // Per side, index 0 the attacker, 1 the defender.
    std::array<std::int64_t, 2> won{};
    std::array<std::int64_t, 2> taken{};
    std::array<std::int64_t, 2> dead{};
    std::array<std::int64_t, 2> pop0{};
    // r10 (20.15, 20.17): people of the settlements taken (before the toll),
    // the winner of the war's latest battle (0 none), failed searches in a row.
    std::array<std::int64_t, 2> taken_people{};
    std::int64_t last_battle_winner = 0;
    std::int64_t unfound = 0;
    // r14 21.7 item 2: the regions this war fought a battle in, ascending.
    std::vector<std::int64_t> fought;
    // P5-04 7.6, 7.15: the joined allies in join order; the war's ending
    // event (0 while open); the winner of its peace or war_won, or the
    // survivor of a principal's end (0 a truce or none); per principal the
    // people of the settlements it sacked before the toll; the tribute.
    std::vector<ConflictAlly> allies;
    std::int64_t end_event = 0;
    std::int64_t winner = 0;
    std::array<std::int64_t, 2> plunder{};
    std::int64_t tribute_people = 0, tribute_event = 0;
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
    // P5-03 7.1: the peoples' dispositions (set once by the stage; nullptr:
    // the camp table, as at 0.23.0) and the camp table they start from.
    void set_dispositions(const DispositionView* view) noexcept { dispositions_ = view; }
    // P5-04 7.1: alliances, trade and arms (set once by the stage; nullptr:
    // none, as at 0.25.0).
    void set_alliances(const AllianceView* view) noexcept { alliances_ = view; }

    // P5-04 7.13: const reads with the live state of the step.
    [[nodiscard]] std::int64_t disposition_of(const StepContext& context, std::int64_t a, std::int64_t b) const;
    // Any open war relation between a and b, either way (principal or joined).
    [[nodiscard]] bool at_war(std::int64_t a, std::int64_t b) const;
    // The wars with civ as a side and its open joined records.
    [[nodiscard]] std::int64_t open_wars_of(std::int64_t civ) const;
    [[nodiscard]] std::int64_t force(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] bool truce_over(const StepContext& context, std::int64_t a, std::int64_t b) const;
    // 5.1: non-islet regions held x 1000 div N.
    [[nodiscard]] std::int64_t share(const StepContext& context, std::int64_t civ) const;
    // The pair's last peace (year, event), or none.
    [[nodiscard]] std::optional<std::pair<std::int64_t, std::int64_t>> last_peace(std::int64_t a, std::int64_t b) const;
    // 5.1's front(x, y) with front_hops(x, y) is not empty.
    [[nodiscard]] bool reaches(const StepContext& context, std::int64_t x, std::int64_t y) const;
    // The war of declaration event `war`; FatalError when there is none.
    [[nodiscard]] bool war_open(std::int64_t war) const;
    [[nodiscard]] std::int64_t war_end_event(std::int64_t war) const;
    // The ally's joined record of the war is closed.
    [[nodiscard]] bool ally_left(std::int64_t war, std::int64_t ally) const;
    // The joined record of `ally` in `war` (FatalError when none).
    [[nodiscard]] const ConflictAlly& ally_record(std::int64_t war, std::int64_t ally) const;
    // 7.15: fatal while the war is open.
    [[nodiscard]] WarSpoils war_spoils(std::int64_t war) const;
    [[nodiscard]] std::int64_t ally_dead(std::int64_t war, std::int64_t ally) const;
    // 7.15: grudge{a, b} += units (a != b, units >= 0).
    void add_grudge_seam(std::int64_t a, std::int64_t b, std::int64_t units);
    // 7.6: ally joins the war of declaration event `war` against the side
    // it is not called by; `event` its ally_joined_war.
    void join_war(StepContext& context, std::int64_t war, std::int64_t ally, std::int64_t side, std::int64_t event);
    [[nodiscard]] const ConflictCamp& camp_table() const noexcept { return tuning_.camp; }

    // Inspection (tests; not used by the engine).
    [[nodiscard]] const std::vector<ConflictWar>& wars() const noexcept { return wars_; }
    [[nodiscard]] std::int64_t strain(std::int64_t a, std::int64_t b) const;
    [[nodiscard]] std::int64_t grudge(std::int64_t a, std::int64_t b) const;
    [[nodiscard]] std::optional<std::pair<std::int64_t, std::int64_t>> claim(std::int64_t a, std::int64_t b) const;
    // r9 (20.1, 20.4, 20.5): the year until which a fled refuge is hidden
    // (0 none), the futile count of (defender, region) as read now (0 when
    // the recorded holder is not the region's holder, r14 21.8), wronged(a, b).
    [[nodiscard]] std::int64_t hidden_until(std::int64_t settlement) const;
    [[nodiscard]] std::int64_t futile_count(const StepContext& context, std::int64_t defender, std::int64_t region) const;
    [[nodiscard]] std::vector<std::int64_t> wronged(std::int64_t a, std::int64_t b) const;
    // r14 21.7 item 1: (hidden, region) is search-weary now (Y below the year
    // set by its last search_abandoned, and the region's holder unchanged).
    [[nodiscard]] bool search_weary(const StepContext& context, std::int64_t hidden, std::int64_t region) const;
    // r17 22.3: civ lies low now (PeoplesSubsystem::lying_low, the one
    // definition: its latest refuge stands, is its own, and is still hidden).
    [[nodiscard]] bool lying_low(const StepContext& context, std::int64_t civ) const;
    // r17 22.1: the open awakening quarrels, civilization -> its neighbour.
    [[nodiscard]] const std::map<std::int64_t, std::int64_t>& quarrel_with() const noexcept { return quarrel_with_; }
    // r17 22.1 item 3: a's claim on b is its quarrel claim and a has fewer
    // than quarrel_min_settlements standing visible settlements, or (r22
    // 24.2) strength(a, b) is below quarrel_strength_permille.
    [[nodiscard]] bool quarrel_held_back(const StepContext& context, std::int64_t a, std::int64_t b) const;
    // r22 24.2: 20.6 item 3's odds of a's claim on b without the aggression
    // discount: min(1000, force(a) x 1000 div max(1, fb x declare_ratio div 1000)).
    [[nodiscard]] std::int64_t strength_of(const StepContext& context, std::int64_t a, std::int64_t b) const;
    // r14 21.12: the civilizations whose awakening quarrel waits for a people.
    [[nodiscard]] const std::set<std::int64_t>& quarrel_pending() const noexcept { return quarrel_pending_; }

private:
    struct Claim {
        std::int64_t event = 0;
        std::int64_t region = 0;
    };
    struct Peace {
        std::int64_t year = 0;
        std::int64_t event = 0;
    };
    struct Futile {
        std::int64_t count = 0;
        std::int64_t owner = 0;  // r14 21.8: the holder at the declaration
        std::int64_t epoch = 0;  // ruling A: the region's holder_changes when stored
    };
    // r14 21.7 item 1: a region weary for every claimant after a failed search.
    struct Searched {
        std::int64_t until = 0;
        std::int64_t holder = 0;  // hold(R) when it was set
        std::int64_t epoch = 0;   // ruling A: the region's holder_changes when set
    };
    // 20.1: where the survivors of a fallen last refuge would flee.
    struct Flight {
        std::int64_t region = 0;
        SiteCell site;
        std::int64_t people = 0;  // n of item 2
    };
    // 20.3: what a lost war costs the loser when the goal is not ceded.
    struct Cost {
        std::int64_t region = 0;   // a region to cede, 0 none
        std::int64_t tribute = 0;  // people, when no region
    };

    void prepare(const StepContext& context);
    [[nodiscard]] const ConflictRace& race_of_civ(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] bool alive_visible(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] std::vector<std::int64_t> alive_civs(const StepContext& context) const;
    [[nodiscard]] std::vector<std::int64_t> presence(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] std::vector<std::int64_t> visible_settlements(const StepContext& context, std::int64_t civ,
                                                                std::int64_t region) const;
    // r9: hidden (20.1) and concealed (20.7) settlements; the visible
    // settlements that are not hidden.
    [[nodiscard]] bool concealed(const State& state, const Settlement& s) const;
    [[nodiscard]] bool hidden(const StepContext& context, std::int64_t settlement) const;
    [[nodiscard]] std::vector<std::int64_t> seen_settlements(const StepContext& context, std::int64_t civ,
                                                             std::int64_t region) const;
    [[nodiscard]] bool last_refuge_of(const State& state, std::int64_t civ) const;
    // P5-04 7.5: the trade, forge and arms factors.
    [[nodiscard]] std::int64_t strengthened(std::int64_t civ, std::int64_t f) const;
    [[nodiscard]] bool allied(std::int64_t a, std::int64_t b) const;
    [[nodiscard]] std::size_t war_index(std::int64_t war) const;
    // P5-04 7.9, 7.10, 7.11.
    void withdrawals(StepContext& context, std::size_t war);
    void close_allies(StepContext& context, std::size_t war, std::int64_t event);
    void close_allies_at_side_end(StepContext& context, std::size_t war, std::int64_t ended, std::int64_t event);
    // r9 20.6 item 2: the local force at a battlefield, divided by open wars.
    [[nodiscard]] std::int64_t local_force(const StepContext& context, std::int64_t civ, std::int64_t region) const;
    [[nodiscard]] std::int64_t front_hops(std::int64_t a, std::int64_t b) const;
    [[nodiscard]] std::int64_t effective_aggression(std::int64_t a, std::int64_t b, std::int64_t aggression) const;
    // 6.3 (with r9's home defence) of entry e against defender y: def and the pass id (0 none).
    [[nodiscard]] std::pair<std::int64_t, std::int64_t> defence_at(const StepContext& context, std::int64_t y,
                                                                   const FrontEntry& e) const;
    [[nodiscard]] std::int64_t odds_of(const StepContext& context, std::int64_t a, std::int64_t b) const;
    // 20.6 item 3's fb for a's claim on b: force(b) x def(G) div 1000.
    [[nodiscard]] std::int64_t claim_defence_force(const StepContext& context, std::int64_t a, std::int64_t b) const;
    // 5.3: the claim event of a on b over region (kind 20 when b owns it,
    // else kind 39 naming b's seen settlements there), with extra causes.
    [[nodiscard]] EventSpec claim_event(const StepContext& context, std::int64_t a, std::int64_t b,
                                        std::int64_t region, std::span<const std::int64_t> extra_causes) const;
    [[nodiscard]] bool weary(const StepContext& context, std::int64_t defender, std::int64_t region) const;
    [[nodiscard]] std::vector<FrontEntry> allowed_goals(const StepContext& context, std::int64_t b,
                                                        std::span<const FrontEntry> front) const;
    [[nodiscard]] std::optional<Flight> flight_plan(const StepContext& context, std::int64_t x, std::int64_t y,
                                                    std::int64_t t) const;
    [[nodiscard]] Cost cost_plan(const StepContext& context, std::int64_t winner, std::int64_t loser) const;
    [[nodiscard]] bool attacking(std::int64_t a) const;
    [[nodiscard]] std::int64_t target_key_value(const StepContext& context, std::int64_t race,
                                                const FrontEntry& e) const;
    [[nodiscard]] FrontEntry best_target(const StepContext& context, std::int64_t race,
                                         std::span<const FrontEntry> front) const;
    [[nodiscard]] std::vector<FrontEntry> live_front(const StepContext& context, std::int64_t x, std::int64_t y) const;
    [[nodiscard]] std::optional<std::pair<TensionReport, Pair>> best_claim(const StepContext& context) const;

    // Steps (8.1, 8.2).
    void housekeeping(StepContext& context);
    // r14 21.12: the awakening quarrels of this T2, before the strain update.
    void awakening_quarrels(StepContext& context);
    void decay_grudges(std::int64_t years);
    void update_strain(StepContext& context, std::int64_t years, std::int64_t permille);
    void declarations(StepContext& context);
    void declare(StepContext& context, std::int64_t a, std::int64_t b);
    void fight(StepContext& context);
    void quiet_ends(StepContext& context);

    // War (conflict_war.cpp; 7.3 to 7.7).
    enum class Fought : std::uint8_t { no_front, no_battle, battle, ended };
    Fought battle(StepContext& context, std::size_t war);
    void siege(StepContext& context, std::size_t war, std::int64_t x, std::int64_t y, std::int64_t region,
               std::int64_t attack, std::int64_t dead_x, std::int64_t battle_event);
    void fall(StepContext& context, std::size_t war, std::int64_t x, std::int64_t y, std::int64_t settlement,
              std::int64_t siege_event);
    void flee(StepContext& context, std::size_t war, std::int64_t x, std::int64_t y, std::int64_t settlement,
              std::int64_t siege_event, const Flight& plan);
    // `move_region` false: r17 22.5 item 3's foothold cession (the winner
    // already holds the region; its owner is left as it is).
    void cede(StepContext& context, std::int64_t region, std::int64_t loser, std::int64_t winner, std::int64_t cause,
              bool move_region = true);
    void note_futile(const StepContext& context, const ConflictWar& w);
    void conquer(StepContext& context, std::int64_t y, std::int64_t x, std::int64_t cause, std::int64_t region);
    std::int64_t casualties(StepContext& context, std::int64_t civ, std::int64_t dead, std::int64_t event);
    void refugees(StepContext& context, std::int64_t civ, std::int64_t from_settlement, std::int64_t people,
                  std::int64_t cause);
    void end_checks(StepContext& context, std::size_t war, bool no_front);
    // 7.7: ends the war with winner (0 a truce); `sued` writes a peace (the
    // loser is exhausted), else a war_won (r10 20.15 item 4); false when a
    // quiet step's budget does not allow it (the war stays open).
    bool end_war(StepContext& context, std::size_t war, std::int64_t winner, bool sued);
    // r10 20.17 item 2: the war ends unfound, as a truce without cost.
    void abandon_search(StepContext& context, std::size_t war, std::int64_t x, std::int64_t y, std::int64_t region);
    // The war's relation changes, last_peace, grudge and strains after its end event.
    void close_war(StepContext& context, std::size_t war, std::int64_t event);
    [[nodiscard]] std::int64_t score_winner(const ConflictWar& w) const;
    // r17 22.5 item 1: the attacker holds the goal and the defender has no
    // standing settlement in it (visible or not, hidden ones included).
    [[nodiscard]] static bool aim_achieved(const State& state, const ConflictWar& w);
    // 7.7 item 1: side s (0 the attacker, 1 the defender) is exhausted.
    [[nodiscard]] bool exhausted(const StepContext& context, const ConflictWar& w, std::size_t s) const;
    // r10 20.15 item 3: an ending decided by the score never makes the winner
    // of the war's last battle the loser (a truce, 0, instead).
    [[nodiscard]] std::int64_t unless_last_battle(const ConflictWar& w, std::int64_t winner) const;
    void add_grudge(std::int64_t a, std::int64_t b, std::int64_t amount);
    void drop_pair(std::int64_t a, std::int64_t b);  // strains 0, claims dropped, both directions
    void close_relations_of(StepContext& context, std::int64_t civ, std::int64_t event);

    PeoplesSubsystem& peoples_;
    ConflictTuning tuning_;
    bool prepared_ = false;
    std::array<std::uint64_t, 7> keys_{};
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
    // r9 (P3-09): fled refuges (settlement -> year found again), futile wars
    // per (defender, region), regions a was wronged of by b.
    std::map<std::int64_t, std::int64_t> hidden_until_;
    std::map<Pair, Futile> futile_;
    std::map<Pair, std::set<std::int64_t>> wronged_;
    std::int64_t last_t2_phase_ = 0;
    // r14 (P3-10, generator 0.19.0): failed searches per (hidden, region);
    // whether a search_abandoned is in the log (21.7); the civilizations
    // seen by the awakening quarrel and those whose quarrel waits (21.12).
    std::map<Pair, Searched> searched_until_;
    bool search_written_ = false;
    std::set<std::int64_t> quarrel_seen_;
    std::set<std::int64_t> quarrel_pending_;
    // r17 22.1 item 1: a -> b while a's awakening quarrel with b is open.
    std::map<std::int64_t, std::int64_t> quarrel_with_;
    const DispositionView* dispositions_ = nullptr;  // P5-03 7.1
    const AllianceView* alliances_ = nullptr;        // P5-04 7.1
};

}  // namespace worldgen::history
