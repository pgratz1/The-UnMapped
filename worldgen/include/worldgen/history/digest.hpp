#pragma once

// The history digest of spec P3-07 (section 5.1): the tables the history
// report computes from and their checks (5.3), the computations of section 7
// and the amendments of 21 to 23 (peaks, milestones, the summary set, the
// [stats] values), and the note written after a sentence (21.4, 22.2, 23).
// Shared by the history report and, later, P4's browser. Integer arithmetic
// only; every list in a fixed order (12).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/db/reader.hpp"
#include "worldgen/db/result.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/event_catalogue.hpp"

namespace worldgen::history {

// --- Constants of format v1 (P3-07 section 8, 21.3 item 8) -----------------

inline constexpr std::int64_t SUMMARY_MIN_SALIENCE = 20;
inline constexpr std::int64_t SUMMARY_BUDGET = 250;
inline constexpr std::int64_t SUMMARY_AGE_FLOOR = 20;
inline constexpr std::size_t MILESTONES = 8;          // r17 24.9 (was 6)
inline constexpr std::int64_t MILESTONE_KIND_MAX = 2;  // r17 24.5
inline constexpr std::int64_t ISLET_MAX_DRY_CELLS = 1172;
inline constexpr std::array<std::int64_t, 5> SHARE_LEVELS = {500, 600, 700, 800, 900};

// --- Constants of format v2 (P3-07 r19 25.11) --------------------------------

inline constexpr std::size_t SUMMARY_CALAMITY_PER_PHASE = 2;  // 25.2
inline constexpr std::size_t MILESTONES_ENDED = 4;            // 25.5
// P5-02 12.4 (format v4): the living monsters listed in [monsters] at summary.
// P5-02 r1 12.9: the summary line limit L = SUMMARY_LINES_MAX +
// SUMMARY_LINES_PER_CIV * max(0, C - SUMMARY_LINES_CIVS), and the least
// event budget B.
inline constexpr std::int64_t SUMMARY_LINES_MAX = 790;
inline constexpr std::int64_t SUMMARY_LINES_PER_CIV = 40;
inline constexpr std::int64_t SUMMARY_LINES_CIVS = 16;
inline constexpr std::int64_t SUMMARY_BUDGET_MIN = 100;
inline constexpr std::size_t SUMMARY_MONSTER_LINES = 2;
// 25.7 item 4: the [stats] keys printed at summary, in section 9's order.
// r21 26.5 item 2 (format v3): and regions_renamed, refuges_hidden.
inline constexpr std::array<std::string_view, 17> SUMMARY_STATS = {
    "end_year",           "events",           "summary_threshold",    "summary_events",       "civilizations",
    "civilizations_alive", "civilizations_ended", "settlements_standing", "settlements_ruined", "population",
    "wars",               "wars_open",        "wars_hollow_permille", "share_max_permille",   "races_alive",
    "regions_renamed",    "refuges_hidden"};

// --- Tables (5.1, 5.3) ------------------------------------------------------

// A region row reduced to the columns of 3.1.
struct RegionInfo {
    std::int64_t id = 0;
    std::string name;
    std::int64_t biome = 0;
    std::int64_t cells = 0;
    std::int64_t lake_cells = 0;
    std::int64_t walk_cells = 0;
};

// The tables the report computes from, read once and checked. Rows in
// primary-key order.
struct HistoryTables {
    std::int64_t end_year = 0;  // E = meta.history_end_year
    std::string stages;         // meta.stages
    // The twelve [settings] lines of 6.3: (report key, meta value).
    std::vector<std::pair<std::string, std::string>> settings;
    std::vector<RegionInfo> regions;
    std::vector<std::pair<std::int64_t, std::string>> biome_keys;  // (biome.id, biome.key), ascending id
    std::vector<db::RaceRow> races;
    std::vector<db::GodRow> gods;
    std::vector<db::AgeRow> ages;
    std::vector<db::PhaseRow> phases;
    std::vector<db::CivilizationRow> civilizations;
    std::vector<db::SettlementRow> settlements;
    std::vector<db::SettlementCensusRow> census;
    std::vector<db::RegionControlRow> region_control;
    std::vector<db::RelationRow> relations;
    // Schema 8 (P3-07 r21 26.6): table region_name, primary-key order.
    std::vector<db::RegionNameRow> region_names;
    // Schema 9 (P5-02 11.4): tables beast, monster_kind, beast_population,
    // monster and site, primary-key order.
    std::vector<db::BeastRow> beasts;
    std::vector<db::MonsterKindRow> monster_kinds;
    std::vector<db::BeastPopulationRow> beast_populations;
    std::vector<db::MonsterRow> monsters;
    std::vector<db::SiteRow> sites;
    std::int64_t event_rows = 0;
    std::int64_t cause_rows = 0;
};

// Reads the tables and runs, first failure reported, the checks of P3-07
// 5.2 items 1 to 3: the history check (history_missing, or corrupt_layer
// "corrupt table 'phase': ..." / "corrupt table 'event': ..."), the world
// report's checks of P3-03 9.1 and P3-04 11.1 (their messages unchanged),
// then the checks of 5.3 (corrupt_layer, "corrupt table '<t>': row <key>:
// <reason>"). Reader errors are passed on.
//
// Checks beyond 5.3 (the engineer's, so that no line of the report is
// undefined): the shapes of tables biome and god, before region; a
// settlement or census population above 2147483647 (7.6); after the
// relation checks, a god's camp outside 0 to 2 ("row <id>: camp <c>") and a
// race whose creator_god is not a god ("corrupt table 'race': row <id>:
// creator_god <g> is not a god").
[[nodiscard]] db::Result<HistoryTables> read_history_tables(const db::WorldReader& world);

// The two reads of a file (read_history_tables, then HistoryView::load) are
// separate snapshots: std::nullopt when they agree on everything the digest,
// the report and the timeline index in one by a value of the other, else the
// message "corrupt table 'event': the file changed while it was read"
// (P3-07 5.2 item 4a, P4-01 5.1).
[[nodiscard]] std::optional<std::string> history_reads_disagree(const HistoryView& view,
                                                                const HistoryTables& tables);

// --- Digest (section 7) -----------------------------------------------------

struct RegionPeak {
    std::int64_t count = 0, from_year = 0, to_year = 0;  // 7.1; to_year inclusive; count 0: none
};
struct PopulationPeak {
    std::int64_t population = 0, year = 0;  // 7.2; population 0: none
};
struct CivilizationDigest {
    std::int64_t id = 0;
    RegionPeak peak_regions;
    PopulationPeak peak_population;
    // r19 25.3 item 3: ruined counts status 3 only, faded status 4.
    std::int64_t settlements_founded = 0, settlements_held = 0, settlements_ruined = 0, settlements_faded = 0;
    // At E: open region_control rows, the population of the standing
    // settlements it owns, the events with it as participant.
    std::int64_t regions_held = 0, population = 0, events = 0;
    std::vector<std::int64_t> milestones;  // 24.5 (replaces 7.3), ascending event id
    // r19 25.5: at most MILESTONES_ENDED of the same choice, for the summary
    // block of an ended civilization (empty for an alive one).
    std::vector<std::int64_t> milestones_ended;
    std::vector<std::size_t> relations;    // indexes into HistoryTables::relations, in 6.6 order
    // r17 24.5: the civilization of the `from` role of a civ_seceded
    // founding event (0: not seceded); the wars line.
    std::int64_t seceded_from = 0;
    std::int64_t wars_declared = 0, wars_defended = 0;
    std::int64_t wars_won = 0, wars_lost = 0, wars_drawn = 0, wars_open = 0;
    // One adversary of 24.5: the other end of the civilization's relation
    // rows, in the order (first since, type, id).
    struct Adversary {
        std::int64_t type = 0, id = 0;
        std::int64_t wars = 0, won = 0, lost = 0, drawn = 0, open = 0;
        // over the war rows, meaningful when wars > 0; r19 25.7 item 1: last is
        // the largest of E - 1 (open row) and until (closed row).
        std::int64_t first = 0, last = 0;
        std::string state;                 // "at war", "at peace", "enmity" or "none"
    };
    std::vector<Adversary> adversaries;
};
struct HistoryDigest {
    std::vector<CivilizationDigest> civilizations;  // id order
    std::int64_t summary_threshold = 0;             // T of 21.3 item 3
    std::int64_t summary_events = 0;                // the size of the summary set
    std::vector<std::uint8_t> summary;              // index id - 1: 1 when in the summary set (24.3)
    // The inputs of 24.3 item 4's threshold, kept so that the set can be
    // chosen again with another budget (P5-02 12.9): per event (index
    // id - 1) 1 when in the floor set, and the highest salience of a
    // candidate whose unit holds it (-1 none); summary_at_least[t] the
    // non-floor events with that salience >= t (t <= 100), and the floor size.
    std::vector<std::uint8_t> summary_floor;
    std::vector<std::int64_t> summary_entered;
    std::array<std::int64_t, 102> summary_at_least{};
    std::int64_t summary_floor_size = 0;
    std::int64_t summary_budget = 0;                // the budget the set was chosen with
    // r17 24.7: per settlement (index id - 1) the owner_civ of its census
    // row of largest year with status 1 or 2, 0 when none; the standing
    // claims, ascending id.
    std::vector<std::int64_t> last_held_by;
    std::vector<std::int64_t> standing_claims;
    // r19 25.9 item 1: per settlement (index id - 1) the end_year of the
    // refuge_hidden with it as settlement, smallest id first; -1 when none.
    std::vector<std::int64_t> refuge_hidden_end;
    std::vector<std::pair<std::string, std::int64_t>> stats;  // section 9 with 21.5, 21.8, 22, 23, in order
};

// Pure, does not fail: everything it relies on was checked by
// read_history_tables and HistoryView::load on the same world.
[[nodiscard]] HistoryDigest digest_history(const HistoryView& view, const HistoryTables& tables);

// P5-02 12.9: B = min(SUMMARY_BUDGET, max(SUMMARY_BUDGET_MIN, L - O - F)),
// with L of `civilizations` rows (and `lines_max` for SUMMARY_LINES_MAX, a
// test seam), O = `outside_lines` (the summary's lines outside
// [chronicle]) and F = 4 + ages + 2 * phases.
[[nodiscard]] std::int64_t summary_budget(std::int64_t civilizations, std::int64_t outside_lines, std::int64_t ages,
                                          std::int64_t phases, std::int64_t lines_max = SUMMARY_LINES_MAX);

// 24.3 item 4 with the budget B: chooses the summary set of `digest` again
// (summary, summary_threshold, summary_events and their [stats] values).
void choose_summary_set(HistoryDigest& digest, std::int64_t budget);

// --- Notes (21.4, 22.2, 23) -------------------------------------------------

// The note written after the sentence of event `event_id`, without its
// parentheses, or an empty string when the event has none:
//   war_declared: from its cause c of kind claim or claim_settlements with
//                 the smallest id: for a claim_settlements (P3-07 r15 23
//                 item 4a) "The claim, since <year>, was to <settlements> in
//                 <region>." or "The claim, made the same year, was to
//                 <settlements> in <region>."; for a claim "Claimed since
//                 <year>." or "Claimed the same year." when its region is the
//                 war's, else (23 item 3) "The claim, since <year>, was to
//                 <region>." or "The claim, made the same year, was to
//                 <region>." (r19 25.8: every year is that of root(c), c's
//                 own claim cause with the smallest id, else c's); then,
//                 from the second war between the same
//                 two civilizations, " The <ordinal> war between them.";
//   flare_up:     "The claim of <claimant> to <region>, held by <holder>,
//                 came to a head." when its first cause is a claim (P3-07
//                 r12 22.3; every name mid-sentence), or "The claim of
//                 <claimant> to <settlements> of <holder> in <region> came
//                 to a head." when it is a claim_settlements (r14 23 item 4);
//   quiet_stretch, with `civilization` one of its realms (22.2; 0 at the
//                 chronicle: no note): "<C>: no new generation of rulers.",
//                 "<C>: one new generation of rulers." or "<C>: <n> new
//                 generations of rulers.".
// Kinds are found by key in the stored catalogue. Never throws on a loaded
// view; an id not in the log has no note.
[[nodiscard]] std::string history_note(const HistoryView& view, std::int64_t event_id,
                                       std::int64_t civilization = 0);

// ordinal(n) of 21.4 item 3 for n >= 2: "second" .. "tenth", then "11th",
// "21st", "22nd", "23rd", "101st", "111th".
[[nodiscard]] std::string history_ordinal(std::int64_t n);

// The word of a phase kind (P3-07 report, P4-03 9.5): 1 "opening", 2 "quiet",
// 3 "flare_up", 4 "final"; anything else empty.
[[nodiscard]] std::string_view phase_kind_word(std::int64_t kind) noexcept;

}  // namespace worldgen::history
