#pragma once

// The timeline of spec P4-01: a read-only index over the history tables of a
// world, built once, that answers what the world was like at a year Y in
// [0, E] (section 4) and, independent of Y, the events of a civilization,
// region or settlement and the events an event caused. Integer arithmetic
// only; every list in ascending id or year (section 9).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <tuple>
#include <vector>

#include "worldgen/db/reader.hpp"
#include "worldgen/db/result.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/history/event_catalogue.hpp"

namespace worldgen::history {

// The holder of a region at a year (section 4).
struct RegionHolding {
    std::int64_t region = 0;
    std::int64_t civ = 0;    // 0: free
    std::int64_t since = 0;  // from_year of the interval; free: the largest to_year <= Y, or 0
    std::int64_t event = 0;  // the interval's event; 0 when free
    friend bool operator==(const RegionHolding&, const RegionHolding&) = default;
};

// The state of a settlement at a year (section 4): the census row of largest
// year <= Y. Every field but `settlement` is 0 when it does not exist yet.
struct SettlementState {
    std::int64_t settlement = 0;
    bool exists = false;
    std::int64_t owner = 0, kind = 0, status = 0, population = 0, since = 0;
    // P5-02 13 (P4-01 r4): 0 while standing at Y, else settlement.ending.
    std::int64_t ending = 0;
    friend bool operator==(const SettlementState&, const SettlementState&) = default;
};

// P5-02 13: a site at a year (open iff ended_year is NULL or Y < ended_year).
struct SiteState {
    std::int64_t site;
    std::int64_t kind, region, x, y, settlement, monster, founder;
    bool open;
    std::int64_t ending;
    friend bool operator==(const SiteState&, const SiteState&) = default;
};

// P5-02 13: a monster at a year (half-open: the state at the end of Y).
struct MonsterState {
    std::int64_t monster;
    std::int64_t kind;
    bool alive, awake;
    std::int64_t lair_site, region;
    friend bool operator==(const MonsterState&, const MonsterState&) = default;
};

// P5-03 18 (P4-01 r5): a race pair's disposition at a year, and a race's
// share and standing in a settlement.
struct RaceRelationState {
    std::int64_t race_a, race_b;
    bool met;
    std::int64_t disposition, band;
    friend bool operator==(const RaceRelationState&, const RaceRelationState&) = default;
};
struct RaceShare {
    std::int64_t race, share, standing;
    friend bool operator==(const RaceShare&, const RaceShare&) = default;
};

// P5-04 12: an alliance in force at a year.
struct AllianceState {
    std::int64_t a, b, since, since_event;
    friend bool operator==(const AllianceState&, const AllianceState&) = default;
};

// P5-06 12: a figure at a year. status at Y: 0 not yet born; while alive 1,
// or 2 from the year of its exiled event; once dead the stored status.
struct FigureState {
    std::int64_t id, role, civ, status;
    bool alive;
    friend bool operator==(const FigureState&, const FigureState&) = default;
};

// Everything that depends on the year (section 4).
struct YearState {
    std::int64_t year = 0, phase = 0, age = 0;
    std::vector<RegionHolding> regions;        // one per HistoryTables::regions row, ascending id
    std::vector<SettlementState> settlements;  // one per settlement, index id - 1
    std::vector<std::size_t> relations;        // indexes into Timeline::relations(), ascending
    std::vector<std::int64_t> civilizations;   // alive at Y, ascending id
    std::vector<std::int64_t> events;          // year <= Y <= end_year, ascending id
    std::vector<std::int64_t> events_starting; // year == Y, ascending id
    friend bool operator==(const YearState&, const YearState&) = default;
};

class Timeline {
public:
    // Preconditions: both from one world and checked (load_history). The
    // timeline owns copies of what it indexes and keeps no reference into
    // either argument. FatalError "timeline: <what>" if an id it indexes is
    // out of range (a caller's bug; load_history makes sure it is not).
    [[nodiscard]] static Timeline build(const HistoryView& view, const HistoryTables& tables);

    [[nodiscard]] std::int64_t end_year() const noexcept { return end_year_; }
    [[nodiscard]] bool valid_year(std::int64_t y) const noexcept { return y >= 0 && y <= end_year_; }

    // At year Y. FatalError "timeline: year <Y> outside 0 to <E>" when
    // !valid_year(Y); "timeline: no region <r>", "timeline: no settlement
    // <s>", "timeline: no civilization <c>", "timeline: no event <e>" for an
    // id the world does not have.
    [[nodiscard]] RegionHolding region_holder(std::int64_t region, std::int64_t year) const;
    // P4-01 r3 16 (P5-12): the name of the region at year Y, the name of its
    // region_name row with the largest from_year <= Y, else region.name. A
    // view into the timeline's own storage.
    [[nodiscard]] std::string_view region_name(std::int64_t region, std::int64_t year) const;
    [[nodiscard]] SettlementState settlement_state(std::int64_t settlement, std::int64_t year) const;
    [[nodiscard]] YearState year_state(std::int64_t year) const;
    // Reuses out's buffers; the same content as the returning overload.
    void year_state(std::int64_t year, YearState& out) const;
    // P5-02 13 (P4-01 r4): the sites with origin_year <= Y, ascending id; a
    // site, a monster and a beast count at Y (the beast_population row with
    // the largest year <= Y, else 0). FatalError "timeline: no site <s>",
    // "timeline: no monster <m>", "timeline: no beast <b>", "timeline: no
    // region <r>" for an id the world does not have.
    [[nodiscard]] std::span<const std::int64_t> sites_existing(std::int64_t year) const;
    [[nodiscard]] SiteState site_at(std::int64_t site, std::int64_t year) const;
    [[nodiscard]] MonsterState monster_at(std::int64_t monster, std::int64_t year) const;
    [[nodiscard]] std::int64_t beast_count(std::int64_t region, std::int64_t beast, std::int64_t year) const;
    [[nodiscard]] std::size_t site_count() const noexcept { return sites_.size(); }
    [[nodiscard]] std::size_t monster_count() const noexcept { return monsters_.size(); }
    // P5-03 18: the pair's disposition at Y (met iff its peoples_met has year
    // <= Y; then the race_relation row of the largest year <= Y; else met
    // false, disposition and band 0); symmetric in its arguments. A standing
    // settlement's shares at Y (per race the last row with year <= Y and
    // share > 0, ascending race); empty when it does not stand at Y.
    // FatalError "timeline: no race <r>" for a race the world does not have.
    [[nodiscard]] RaceRelationState race_relation_at(std::int64_t race_a, std::int64_t race_b, std::int64_t year) const;
    [[nodiscard]] std::vector<RaceShare> settlement_races(std::int64_t settlement, std::int64_t year) const;
    // P5-04 12: the relation rows of kind alliance in force at Y (since <=
    // Y < until), ascending (a, b); the allies of civ at Y, ascending; the
    // connected components of the alliance graph among the civilizations
    // alive at Y with an ally, each ascending, ordered by smallest id; the
    // trade rows in force at Y, ascending (a, b).
    [[nodiscard]] std::vector<AllianceState> alliances_at(std::int64_t year) const;
    [[nodiscard]] std::vector<std::int64_t> allies_of(std::int64_t civ, std::int64_t year) const;
    [[nodiscard]] std::vector<std::vector<std::int64_t>> alliance_groups_at(std::int64_t year) const;
    [[nodiscard]] std::vector<std::pair<std::int64_t, std::int64_t>> trades_at(std::int64_t year) const;
    // P5-06 12: the recorded ruler of civ at Y (office_from <= Y < its ruled
    // row's until, or <= E when the row is open), 0 none; the figures with
    // born <= Y < died (or <= E when alive at the end), ascending id; the
    // generals of civ at Y (served rows with since <= Y < until, or <= E when
    // open), ascending id; a figure at Y (alive by the same rule). FatalError "timeline: no figure <f>", "timeline: no civilization
    // <c>" for an id the world does not have.
    [[nodiscard]] std::int64_t ruler_at(std::int64_t civ, std::int64_t year) const;
    [[nodiscard]] std::vector<std::int64_t> figures_alive(std::int64_t year) const;
    [[nodiscard]] std::vector<std::int64_t> generals_of(std::int64_t civ, std::int64_t year) const;
    [[nodiscard]] FigureState figure_at(std::int64_t figure, std::int64_t year) const;
    [[nodiscard]] std::size_t figure_count() const noexcept { return figures_.size(); }
    [[nodiscard]] std::optional<std::int64_t> previous_event_year(std::int64_t year) const;
    [[nodiscard]] std::optional<std::int64_t> next_event_year(std::int64_t year) const;

    // Independent of Y. Spans into the timeline's own storage, valid while it
    // lives (and across a move of it).
    [[nodiscard]] std::span<const std::int64_t> holder_change_years() const noexcept { return change_years_; }
    [[nodiscard]] std::span<const db::RelationRow> relations() const noexcept { return relations_; }
    [[nodiscard]] std::span<const std::int64_t> events_of_civilization(std::int64_t civ) const;
    [[nodiscard]] std::span<const std::int64_t> events_of_region(std::int64_t region) const;
    [[nodiscard]] std::span<const std::int64_t> events_of_settlement(std::int64_t settlement) const;
    [[nodiscard]] std::span<const std::int64_t> effects(std::int64_t event) const;
    // P5-06 12: the events naming figure `figure`, ascending: as a
    // participant, and through {r:under} (figure_event_lists).
    [[nodiscard]] std::span<const std::int64_t> events_of_figure(std::int64_t figure) const;

    // The region ids, ascending (one per HistoryTables::regions row).
    [[nodiscard]] std::span<const std::int64_t> region_ids() const noexcept { return region_ids_; }
    [[nodiscard]] std::size_t civilization_count() const noexcept { return civ_founded_.size(); }
    [[nodiscard]] std::size_t settlement_count() const noexcept { return settlement_founded_.size(); }
    [[nodiscard]] std::size_t event_count() const noexcept { return event_year_.size(); }

private:
    struct Span {  // a phase or age row: its years (inclusive) and id
        std::int64_t start_year = 0, end_year = 0, id = 0;
    };

    void require_year(std::int64_t year) const;
    [[nodiscard]] std::size_t region_position(std::int64_t region) const;
    [[nodiscard]] RegionHolding holder_at(std::size_t position, std::int64_t year) const;
    [[nodiscard]] SettlementState settlement_at(std::size_t index, std::int64_t year) const;
    [[nodiscard]] std::int64_t phase_at(std::int64_t year) const;
    [[nodiscard]] std::int64_t age_at(std::int64_t year) const;
    void events_at(std::int64_t year, std::vector<std::int64_t>& events,
                   std::vector<std::int64_t>& starting) const;

    std::int64_t end_year_ = 0;
    // Regions: ids ascending; region_control rows of position p are
    // control_[control_offsets_[p]] .. control_[control_offsets_[p + 1] - 1],
    // ascending from_year.
    std::vector<std::int64_t> region_ids_;
    std::vector<db::RegionControlRow> control_;
    std::vector<std::size_t> control_offsets_;
    std::vector<std::int64_t> change_years_;
    // r3 16: per region position its geographic name, and its region_name
    // rows names_[name_offsets_[p]] .. names_[name_offsets_[p + 1] - 1],
    // ascending from_year.
    std::vector<std::string> region_names_;
    std::vector<db::RegionNameRow> names_;
    std::vector<std::size_t> name_offsets_;
    // Settlements (index id - 1): founded year; census rows likewise by offsets.
    std::vector<std::int64_t> settlement_founded_;
    std::vector<db::SettlementCensusRow> census_;
    std::vector<std::size_t> census_offsets_;
    std::vector<db::RelationRow> relations_;
    // Civilizations (index id - 1).
    std::vector<std::int64_t> civ_founded_;
    std::vector<std::optional<std::int64_t>> civ_ended_;
    // Phases and ages, ascending start_year (they tile [0, E - 1]).
    std::vector<Span> phases_;
    std::vector<Span> ages_;
    // Events (index id - 1): start and end year; the ids with end_year > year.
    std::vector<std::int64_t> event_year_;
    std::vector<std::int64_t> event_end_;
    std::vector<std::int64_t> multi_year_;
    // Per-entity event lists (civ and settlement index id - 1, region by
    // position) and the effects (index event id - 1); each strictly ascending.
    std::vector<std::vector<std::int64_t>> civ_events_;
    std::vector<std::vector<std::int64_t>> region_events_;
    std::vector<std::vector<std::int64_t>> settlement_events_;
    std::vector<std::vector<std::int64_t>> effects_;
    // P5-02 13: sites and monsters (index id - 1), the site ids and their
    // origin years (ascending: creation order), per monster its awake
    // transitions (year, awake) in event order, and the beast rows in
    // primary-key order with per (region position, beast) offsets.
    std::vector<db::SiteRow> sites_;
    std::vector<std::int64_t> site_ids_;
    std::vector<std::int64_t> site_origin_;
    std::vector<db::MonsterRow> monsters_;
    std::vector<std::vector<std::pair<std::int64_t, bool>>> monster_awake_;
    std::vector<std::vector<std::int64_t>> monster_lairs_;  // per monster: its lair site ids, ascending
    std::vector<db::BeastPopulationRow> beast_rows_;
    std::size_t beast_kinds_ = 0;
    std::vector<std::size_t> beast_offsets_;  // (position * beast_kinds_ + beast - 1), size + 1 entries
    std::vector<std::int64_t> settlement_ending_;  // index id - 1
    // P5-03 18: the race ids (ascending), each pair's meeting year (by
    // (a, b), ascending) and the rows of the two tables in primary-key order,
    // the share rows per settlement by offsets (size + 1 entries).
    std::vector<std::int64_t> race_ids_;
    std::vector<std::tuple<std::int64_t, std::int64_t, std::int64_t>> met_years_;
    std::vector<db::RaceRelationRow> race_relations_;
    std::vector<db::SettlementRaceRow> settlement_races_;
    std::vector<std::size_t> share_offsets_;
    // P5-06 12: the figure rows (index id - 1), per figure the year of its
    // first exiled event (-1 none) and its events; per civilization (index
    // id - 1) its rulers as (office_from, end, figure) ascending office_from
    // (end exclusive: until, or E + 1 for an open row),
    // and its served rows as (since, end, figure) in row order.
    struct Term {
        std::int64_t from = 0, to = 0, figure = 0;
    };
    std::vector<db::FigureRow> figures_;
    std::vector<std::int64_t> figure_exiled_;
    std::vector<std::vector<std::int64_t>> figure_events_;
    std::vector<std::vector<Term>> civ_rulers_;
    std::vector<std::vector<Term>> civ_generals_;
};

// Everything a reader of history needs, read once (P4-00 V3). Movable: the
// timeline holds no reference into the other two.
struct LoadedHistory {
    HistoryTables tables;
    HistoryView view;
    Timeline timeline;
};

// P4-01 5.2, first failure returned: read_history_tables (its errors
// unchanged), HistoryView::load (its errors unchanged), history_reads_disagree
// (corrupt_layer), then Timeline::build. std::bad_alloc: out_of_memory, "out
// of memory while building the timeline". Never throws for a bad file.
[[nodiscard]] db::Result<LoadedHistory> load_history(const db::WorldReader& world);

}  // namespace worldgen::history
