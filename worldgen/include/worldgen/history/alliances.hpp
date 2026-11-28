#pragma once

// Alliances, trade and the dwarves' mines and arms (spec
// docs/specs/p5-04-alliances-trade.md): realms swear alliances, renew them,
// let them lapse, break them and betray them; an ally is called to arms at a
// declaration and is owed a share of the spoils of a won war; realms in
// reach open trade; dwarven realms open mines on reachable ore and sell the
// arms forged there.
//
// AlliancesSubsystem runs in steps T2, T6 and Q4 after RelationsSubsystem
// (6.1). It is conflict's AllianceView (7.1). Keyed draws only (streams
// hist.alliances.form, .join, .trade, .spoils and .mine); integer arithmetic
// only; members hold ids. It changes State only through open_relation and
// close_relation (kinds alliance and trade), ConflictSubsystem::join_war and
// add_grudge_seam, and the site seam of MonstersSubsystem (7.16).

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/engine.hpp"
#include "worldgen/history/peoples.hpp"

namespace worldgen::history {

class MonstersSubsystem;
class RelationsSubsystem;

// Definition history.alliances (4.2), field for field.
struct AlliancesTuning {
    std::int64_t ally_hops = 3, ally_min_disposition = -300, ally_full_disposition = 300;
    std::int64_t ally_friend_disposition = 150, ally_break_disposition = -350;
    std::int64_t allies_max = 3, ally_after_war_years = 100, foe_years = 100, fear_share = 300;
    std::int64_t form_foe_permille = 40, form_fear_permille = 30, form_trade_permille = 20;
    std::int64_t form_friend_permille = 10, form_max_permille = 60;
    std::int64_t bloc_share_max = 500, bloc_lapse_share = 700;
    std::int64_t term_years = 150, refusals_to_break = 2;
    std::int64_t defend_permille = 700, attack_permille = 200, hostility_full = 500, ally_wars_max = 2;
    std::int64_t ally_memory = 30, renew_memory = 15, join_memory = 30, refuse_memory = 40;
    std::int64_t betray_memory = 150, ally_war_memory = 75;
    std::int64_t trade_hops = 3, trade_min_disposition = -250, trade_break_margin = 100;
    std::int64_t trade_after_war_years = 50, trade_permille = 15, trade_max = 4;
    std::int64_t trade_credit_years = 50, trade_memory = 20, trade_ally_years = 100;
    std::int64_t spoils_plunder_permille = 250, spoils_floor_permille = 100, spoils_cap_permille = 400;
    std::int64_t spoils_grant_permille = 450, spoils_warmth_permille = 300, spoils_part_permille = 500;
    std::int64_t spoils_need_permille = 400, spoils_grant_min = 100, spoils_grant_max = 950;
    std::int64_t share_memory = 25, withheld_memory = 50, withheld_grudge = 120, withheld_to_break = 2;
    std::int64_t mine_permille = 20, mine_ore_min = 1, mine_settlement_min = 200;
    std::int64_t arms_trade_permille = 2000;
    std::vector<std::string> mine_forms{"The Mines of {settlement}", "The Delvings of {settlement}"};
    // P5-04a 14.1 (generator 0.28.0). The defaults of a hand-made tuning keep
    // 0.26.0's meaning (no large share, no soured draw, no factor below 1000).
    std::int64_t spoils_large_share = 1000000, spoils_large_part_permille = 1001;
    std::int64_t soured_renew_permille = 1000, soured_join_permille = 1000;
    // P5-08 12.5 (generator 0.30.0): written renewals per alliance row; 0 no cap.
    std::int64_t renew_written_max = 0;
    std::int64_t weary_years = 0, join_weary_permille = 1000, join_busy_permille = 0;
    std::int64_t join_fear_floor = 0, join_warmth_floor = 0, grasping_join_permille = 1000;
    std::int64_t join_claim_permille = 1000, excused_memory = 0, refuse_grudge = 0;
    std::int64_t betray_cooled_disposition = 0, betray_weak_permille = 0;

    friend bool operator==(const AlliancesTuning&, const AlliancesTuning&) = default;
};

// The loaded definition. FatalError "history: tuning: the content has no
// definition history.alliances" when it is absent.
[[nodiscard]] AlliancesTuning alliances_tuning_from(const data::ContentSet& content);

// Basis codes (8.1).
enum class AllianceBasis : std::int64_t { origin = 0, common_enemy = 1, fear = 2, friendship = 3, trade = 4 };

// Counts for the calibration sweep: not stored in the world.
struct AlliancesStats {
    std::int64_t formed = 0, origin = 0, renewed = 0, lapsed = 0, broken = 0, spurned = 0, betrayed = 0;
    std::int64_t joined = 0, refused = 0, owed = 0, granted = 0, withheld = 0;
    std::int64_t trades = 0, trades_broken = 0, mines = 0, arms = 0;
    // P5-04a: soured alliances, spurnings at once, excused refusals, quiet renewals.
    std::int64_t soured = 0, spurned_at_once = 0, excused = 0, renewed_quietly = 0;
    std::int64_t renewals_capped = 0;  // P5-08 12.5: quiet renewals made by the cap
};

class AlliancesSubsystem final : public Subsystem, public AllianceView {
public:
    // The shipped configuration: the tuning and race fields of `content`.
    AlliancesSubsystem(PeoplesSubsystem& peoples, ConflictSubsystem& conflict, RelationsSubsystem& relations,
                       const data::ContentSet& content);
    // A test seam: a hand-made tuning; the race fields are read from the
    // content at the first step.
    AlliancesSubsystem(PeoplesSubsystem& peoples, ConflictSubsystem& conflict, RelationsSubsystem& relations,
                       AlliancesTuning tuning);
    AlliancesSubsystem(const AlliancesSubsystem&) = delete;
    AlliancesSubsystem& operator=(const AlliancesSubsystem&) = delete;

    // 7.16: the sites of the world (set once by the stage); without it no
    // mine opens.
    void set_sites(MonstersSubsystem* monsters) noexcept { sites_ = monsters; }
    // P5-06 7.1, 7.3: the figures (set once by the stage; nullptr: none).
    void set_figures(const FigureView* figures) noexcept { figures_ = figures; }

    [[nodiscard]] std::span<const Step> steps() const override;  // T2, T6, Q4
    void run(StepContext& context) override;

    // 7.1 (AllianceView).
    [[nodiscard]] bool allied(std::int64_t a, std::int64_t b) const override;
    [[nodiscard]] std::span<const std::int64_t> allies(std::int64_t c) const override;
    [[nodiscard]] std::int64_t trade_partners(std::int64_t c) const override;
    [[nodiscard]] bool forged(std::int64_t c) const override;
    [[nodiscard]] std::int64_t forge_depth(std::int64_t c) const override;  // P5-07 7.1
    [[nodiscard]] bool arms(std::int64_t c) const override;
    [[nodiscard]] std::int64_t arms_event(std::int64_t c) const override;  // P5-04a 9.2
    [[nodiscard]] std::vector<std::pair<std::int64_t, std::int64_t>> partners(std::int64_t c) const override;  // P5-08 7.4
    [[nodiscard]] std::int64_t forge_site(std::int64_t c) const override;  // P5-11 7.4

    // Inspection (tests).
    [[nodiscard]] const AlliancesTuning& tuning() const noexcept { return tuning_; }
    [[nodiscard]] const AlliancesStats& stats() const noexcept { return stats_; }

private:
    using Pair = std::pair<std::int64_t, std::int64_t>;
    struct Alliance {
        std::int64_t formed_year = 0, formed_event = 0;
        std::int64_t basis = 0, foe = 0;
        bool origin = false;
        // P5-04a 6.1, 6.2: the last written renewal (the formation at first),
        // the next due year of the renewal chain, and the latest joint war
        // and granted share between the pair (0 none).
        std::int64_t renew_year = 0, renew_event = 0;
        std::int64_t written_renewals = 0;  // P5-08 12.5
        std::int64_t next_due = 0;
        std::int64_t fought_event = 0, shared_event = 0;
        std::int64_t refusals = 0;  // P5-04a 4.3: counted refusals only
        // P5-04a 3.2: withheld shares over the alliance's life and the soured
        // record (the first withholding: its event, year, region and the
        // wronged ally; event 0 none) with its two flags.
        std::int64_t withheld = 0;
        std::int64_t soured_event = 0, soured_year = 0, soured_region = 0, soured_ally = 0;
        bool renewal_due = false, call_due = false;
        // P5-04a 3.4: a spurning at once that waits for a quiet step's budget
        // (the spoils_withheld event, its year, region and the wronged ally).
        std::int64_t pending_event = 0, pending_year = 0, pending_region = 0, pending_ally = 0;
    };
    struct Trade {
        std::int64_t opened_year = 0, opened_event = 0, next_credit = 0;
    };
    struct Joined {
        std::int64_t war = 0, ally = 0, partner = 0, enemy = 0, event = 0;
        bool credited = false, settled = false;
    };
    struct Mine {
        std::int64_t site = 0, settlement = 0, event = 0;
    };
    struct RaceData {
        std::int64_t id = 0;
        std::vector<std::int64_t> origin_allies;  // race ids, ascending
        std::int64_t join = 0, mining = 0, muster = 1000;
    };
    // The reasons of a pair (6.2), each with its cause event (0 absent).
    struct Reasons {
        std::int64_t enemy_event = 0, enemy = 0;
        std::int64_t fear_event = 0, fear = 0;
        std::int64_t trade_event = 0;
        std::int64_t friend_event = 0;
        [[nodiscard]] bool any() const noexcept {
            return enemy_event != 0 || fear_event != 0 || trade_event != 0 || friend_event != 0;
        }
    };
    struct Declaration {
        std::int64_t event = 0, year = 0, attacker = 0, defender = 0;
    };
    // The per-step cache of 6.2.
    struct Presence {
        std::vector<std::int64_t> regions;                     // P(c), ascending
        std::vector<std::pair<std::int64_t, std::int64_t>> near;  // (region, hops) within the reach radius
    };

    void prepare(const StepContext& context);
    void build_cache(const StepContext& context);
    void drop_closed(const StepContext& context);
    [[nodiscard]] const RaceData& race(std::int64_t race_id) const;
    [[nodiscard]] std::int64_t race_of(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] bool alive_visible(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] bool free_civ(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] std::int64_t hops(std::int64_t x, std::int64_t y) const;  // -1 beyond the radius
    [[nodiscard]] bool in_reach(std::int64_t x, std::int64_t y, std::int64_t limit) const;
    [[nodiscard]] std::int64_t d(const StepContext& context, std::int64_t x, std::int64_t y) const;
    [[nodiscard]] std::int64_t warmth(const StepContext& context, std::int64_t x, std::int64_t y) const;
    [[nodiscard]] std::int64_t share_of(std::int64_t c) const;
    [[nodiscard]] std::int64_t bloc(std::int64_t c) const;
    [[nodiscard]] bool bloc_test(std::int64_t x, std::int64_t y) const;
    [[nodiscard]] bool lately_at_war(const StepContext& context, std::int64_t x, std::int64_t y,
                                     std::int64_t years) const;
    [[nodiscard]] Reasons reasons(const StepContext& context, std::int64_t x, std::int64_t y) const;
    [[nodiscard]] bool quiet_ok(const StepContext& context, std::int64_t civ) const;
    [[nodiscard]] bool is_seller(std::int64_t c) const;
    void rebuild_allies();
    void open_alliance(StepContext& context, std::int64_t x, std::int64_t y, std::int64_t event, Alliance a);
    void close_alliance(StepContext& context, const Pair& p, std::int64_t event);
    void close_trade(StepContext& context, const Pair& p, std::int64_t event);
    [[nodiscard]] std::int64_t append_pair_event(StepContext& context, std::string_view kind, const Pair& p,
                                                 std::int64_t value, std::vector<std::int64_t> causes);

    // Rules.
    void read_log(StepContext& context);
    void betrayal(StepContext& context, const Declaration& w);
    void calls(StepContext& context, const Declaration& w);
    void cooling(StepContext& context);
    void blocs(StepContext& context);
    void renewals(StepContext& context);
    // P5-04a 3.4 to 3.6: alliance_spurned of `p` by the wronged ally over the
    // share `share_event` (year, region), with reason and years.
    void spurn(StepContext& context, const Pair& p, std::int64_t reason, std::int64_t ally, std::int64_t share_event,
               std::int64_t share_year, std::int64_t region);
    void pending_spurns(StepContext& context);
    void on_withheld(StepContext& context, std::int64_t principal, std::int64_t ally, std::int64_t event,
                     std::int64_t region, std::int64_t year, bool large);
    [[nodiscard]] std::int64_t formation_band(const StepContext& context, std::int64_t x, std::int64_t y) const;
    void origin_alliances(StepContext& context);
    void formations(StepContext& context);
    void trade_breaks(StepContext& context);
    void trade_openings(StepContext& context);
    void trade_credits(StepContext& context);
    void joined_ends(StepContext& context);
    void spoils(StepContext& context, Joined& j);
    void mine_closings(StepContext& context);
    void mine_openings(StepContext& context);
    void arms_sales(StepContext& context);

    PeoplesSubsystem& peoples_;
    ConflictSubsystem& conflict_;
    RelationsSubsystem& relations_;
    MonstersSubsystem* sites_ = nullptr;
    const FigureView* figures_ = nullptr;  // P5-06 7.1
    AlliancesTuning tuning_;
    bool prepared_ = false;
    std::uint64_t form_key_ = 0, join_key_ = 0, trade_key_ = 0, spoils_key_ = 0, mine_key_ = 0, renew_key_ = 0;
    std::vector<RaceData> races_;  // ascending race id
    std::vector<std::int64_t> ore_reach_;  // per region, index id - 1

    std::map<Pair, Alliance> alliances_;
    std::set<Pair> betrayed_;
    std::map<Pair, Trade> trades_;
    std::vector<Joined> joined_;
    std::map<std::int64_t, std::int64_t> spoils_given_;  // per war: the shares given so far (6.11)
    std::map<Pair, Mine> mines_;                         // (civilization, region)
    std::set<std::int64_t> mined_;                       // settlements that ever had a mine
    std::map<Pair, std::int64_t> arms_sold_;             // (seller, buyer) -> its arms_sold event
    std::set<std::pair<Pair, bool>> closed_now_;         // ruling A9: (pair, alliance) closed in this year
    std::int64_t closed_year_ = -1;                      // the year closed_now_ holds (audit A3)
    std::vector<Declaration> declarations_;              // every war_declared read, ascending id
    std::int64_t cursor_ = 1;
    std::map<std::int64_t, std::vector<std::int64_t>> allies_of_;  // ascending, rebuilt at each change

    // The step's cache (6.2).
    std::int64_t radius_ = 0;
    std::map<std::int64_t, Presence> presence_;  // alive visible civilizations
    std::map<std::int64_t, std::int64_t> share_;  // alive civilizations
    std::set<std::int64_t> sellers_at_start_;
    std::vector<std::pair<std::int64_t, std::int64_t>> feared_;  // (civilization, bloc share >= fear_share)
    AlliancesStats stats_;
};

}  // namespace worldgen::history
