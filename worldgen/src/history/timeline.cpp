// The timeline of spec P4-01 (sections 4, 5.1, 5.2, 6).

#include "worldgen/history/timeline.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/db/schema.hpp"

namespace worldgen::history {

namespace {

std::string num(std::int64_t v) {
    return db::format_i64(v);
}

[[noreturn]] void timeline_fatal(const std::string& what) {
    throw FatalError("timeline: " + what);
}

constexpr std::int64_t TYPE_CIVILIZATION = static_cast<std::int64_t>(EntityType::civilization);
constexpr std::int64_t TYPE_SETTLEMENT = static_cast<std::int64_t>(EntityType::settlement);
constexpr std::int64_t TYPE_REGION = static_cast<std::int64_t>(EntityType::region);

// Appends id unless it is already the list's last element (6.1: events are
// visited in id order, so each list is strictly ascending, each event once).
void append_once(std::vector<std::int64_t>& list, std::int64_t id) {
    if (list.empty() || list.back() != id) {
        list.push_back(id);
    }
}

// The index id - 1 of an id in 1 .. n, or std::nullopt.
std::optional<std::size_t> index_of(std::int64_t id, std::size_t n) noexcept {
    if (id < 1 || static_cast<std::uint64_t>(id) > static_cast<std::uint64_t>(n)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(id - 1);
}

// The position of a region id in the ascending id list, or std::nullopt.
std::optional<std::size_t> position_of(const std::vector<std::int64_t>& ids, std::int64_t id) noexcept {
    const auto it = std::lower_bound(ids.begin(), ids.end(), id);
    if (it == ids.end() || *it != id) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(it - ids.begin());
}

// What the timeline relies on in the log, read separately from the tables:
// the message of history_reads_disagree (P3-07 5.2 item 4a) when an event is
// out of order, or a participant, place or cause of the log is not in the
// tables. Reached only when the file changed between the two reads.
std::optional<std::string> timeline_reads_disagree(const HistoryView& view, const HistoryTables& t) {
    const std::string changed = "corrupt table 'event': the file changed while it was read";
    std::vector<std::int64_t> region_ids;
    region_ids.reserve(t.regions.size());
    for (const RegionInfo& r : t.regions) {
        region_ids.push_back(r.id);
    }
    const std::size_t n = view.events().size();
    std::int64_t expected_id = 1;
    std::int64_t previous_year = 0;
    for (const HistoryView::Event& e : view.events()) {
        // The order the index relies on (P3-04 11.1, checked on the first read).
        if (e.row.id != expected_id || (expected_id > 1 && e.row.year < previous_year) ||
            e.row.end_year < e.row.year) {
            return changed;
        }
        ++expected_id;
        previous_year = e.row.year;
        if (e.row.region != 0 && !position_of(region_ids, e.row.region)) {
            return changed;
        }
        if (e.row.settlement != 0 && !index_of(e.row.settlement, t.settlements.size())) {
            return changed;
        }
        for (const db::EventParticipantRow& p : e.participants) {
            if ((p.entity_type == TYPE_CIVILIZATION && !index_of(p.entity_id, t.civilizations.size())) ||
                (p.entity_type == TYPE_SETTLEMENT && !index_of(p.entity_id, t.settlements.size())) ||
                (p.entity_type == TYPE_REGION && !position_of(region_ids, p.entity_id))) {
                return changed;
            }
        }
        for (const std::int64_t c : e.causes) {
            if (!index_of(c, n)) {
                return changed;
            }
        }
    }
    return std::nullopt;
}

}  // namespace

// --- Build (6.1) -------------------------------------------------------------

Timeline Timeline::build(const HistoryView& view, const HistoryTables& tables) {
    Timeline t;
    t.end_year_ = tables.end_year;

    // Regions and their control intervals (ordered by region, from_year).
    t.region_ids_.reserve(tables.regions.size());
    for (const RegionInfo& r : tables.regions) {
        if (!t.region_ids_.empty() && r.id <= t.region_ids_.back()) {
            timeline_fatal("region " + num(r.id) + " out of order");
        }
        t.region_ids_.push_back(r.id);
    }
    t.control_ = tables.region_control;
    t.control_offsets_.assign(t.region_ids_.size() + 1U, 0U);
    {
        std::size_t row = 0;
        for (std::size_t p = 0; p < t.region_ids_.size(); ++p) {
            t.control_offsets_[p] = row;
            while (row < t.control_.size() && t.control_[row].region == t.region_ids_[p]) {
                ++row;
            }
        }
        t.control_offsets_[t.region_ids_.size()] = row;
        if (row != t.control_.size()) {
            timeline_fatal("region_control row of region " + num(t.control_[row].region) + " out of order");
        }
    }
    // r3 16: the names by year.
    for (const RegionInfo& r : tables.regions) {
        t.region_names_.push_back(r.name);
    }
    t.names_ = tables.region_names;
    t.name_offsets_.assign(t.region_ids_.size() + 1U, 0U);
    {
        std::size_t row = 0;
        for (std::size_t p = 0; p < t.region_ids_.size(); ++p) {
            t.name_offsets_[p] = row;
            while (row < t.names_.size() && t.names_[row].region == t.region_ids_[p]) {
                ++row;
            }
        }
        t.name_offsets_[t.region_ids_.size()] = row;
        if (row != t.names_.size()) {
            timeline_fatal("region_name row of region " + num(t.names_[row].region) + " out of order");
        }
    }
    for (const db::RegionControlRow& r : t.control_) {
        t.change_years_.push_back(r.from_year);
        if (r.to_year) {
            t.change_years_.push_back(*r.to_year);
        }
    }
    // determinism-lint: allow(unstable-sort) -- plain integers: equal elements are identical, and unique() removes them
    std::sort(t.change_years_.begin(), t.change_years_.end());
    t.change_years_.erase(std::unique(t.change_years_.begin(), t.change_years_.end()), t.change_years_.end());

    // Settlements (ids 1 .. S) and their census rows (ordered by settlement, year).
    t.settlement_founded_.reserve(tables.settlements.size());
    for (const db::SettlementRow& s : tables.settlements) {
        if (s.id != static_cast<std::int64_t>(t.settlement_founded_.size()) + 1) {
            timeline_fatal("settlement " + num(s.id) + " out of order");
        }
        t.settlement_founded_.push_back(s.founded_year);
    }
    t.census_ = tables.census;
    t.census_offsets_.assign(t.settlement_founded_.size() + 1U, 0U);
    {
        std::size_t row = 0;
        for (std::size_t s = 0; s < t.settlement_founded_.size(); ++s) {
            t.census_offsets_[s] = row;
            const auto id = static_cast<std::int64_t>(s) + 1;
            while (row < t.census_.size() && t.census_[row].settlement == id) {
                ++row;
            }
            if (row == t.census_offsets_[s]) {
                timeline_fatal("settlement " + num(id) + " has no census row");
            }
        }
        t.census_offsets_[t.settlement_founded_.size()] = row;
        if (row != t.census_.size()) {
            timeline_fatal("census row of settlement " + num(t.census_[row].settlement) + " out of order");
        }
    }

    t.relations_ = tables.relations;

    // Civilizations (ids 1 .. C).
    for (const db::CivilizationRow& c : tables.civilizations) {
        if (c.id != static_cast<std::int64_t>(t.civ_founded_.size()) + 1) {
            timeline_fatal("civilization " + num(c.id) + " out of order");
        }
        t.civ_founded_.push_back(c.founded_year);
        t.civ_ended_.push_back(c.ended_year);
    }

    // Phases and ages (ascending id and start_year; they tile [0, E - 1]).
    for (const db::PhaseRow& p : tables.phases) {
        if (!t.phases_.empty() && p.start_year <= t.phases_.back().start_year) {
            timeline_fatal("phase " + num(p.id) + " out of order");
        }
        t.phases_.push_back(Span{p.start_year, p.end_year, p.id});
    }
    for (const db::AgeRow& a : tables.ages) {
        if (!t.ages_.empty() && a.start_year <= t.ages_.back().start_year) {
            timeline_fatal("age " + num(a.id) + " out of order");
        }
        t.ages_.push_back(Span{a.start_year, a.end_year, a.id});
    }

    // Events (index id - 1), per-entity lists and effects.
    const std::span<const HistoryView::Event> events = view.events();
    const std::size_t n = events.size();
    t.event_year_.reserve(n);
    t.event_end_.reserve(n);
    t.civ_events_.resize(t.civ_founded_.size());
    t.region_events_.resize(t.region_ids_.size());
    t.settlement_events_.resize(t.settlement_founded_.size());
    t.effects_.resize(n);
    const auto civ_list = [&t](std::int64_t c) -> std::vector<std::int64_t>& {
        const std::optional<std::size_t> i = index_of(c, t.civ_events_.size());
        if (!i) {
            timeline_fatal("no civilization " + num(c));
        }
        return t.civ_events_[*i];
    };
    const auto settlement_list = [&t](std::int64_t s) -> std::vector<std::int64_t>& {
        const std::optional<std::size_t> i = index_of(s, t.settlement_events_.size());
        if (!i) {
            timeline_fatal("no settlement " + num(s));
        }
        return t.settlement_events_[*i];
    };
    const auto region_list = [&t](std::int64_t r) -> std::vector<std::int64_t>& {
        const std::optional<std::size_t> p = position_of(t.region_ids_, r);
        if (!p) {
            timeline_fatal("no region " + num(r));
        }
        return t.region_events_[*p];
    };
    for (std::size_t i = 0; i < n; ++i) {
        const HistoryView::Event& e = events[i];
        const std::int64_t id = e.row.id;
        if (id != static_cast<std::int64_t>(i) + 1) {
            timeline_fatal("event " + num(id) + " out of order");
        }
        if (!t.event_year_.empty() && e.row.year < t.event_year_.back()) {
            timeline_fatal("event " + num(id) + " dated before the event before it");
        }
        if (e.row.end_year < e.row.year) {
            timeline_fatal("event " + num(id) + " ends before it starts");
        }
        t.event_year_.push_back(e.row.year);
        t.event_end_.push_back(e.row.end_year);
        if (e.row.end_year > e.row.year) {
            t.multi_year_.push_back(id);
        }
        if (e.row.region != 0) {
            append_once(region_list(e.row.region), id);
        }
        if (e.row.settlement != 0) {
            append_once(settlement_list(e.row.settlement), id);
        }
        for (const db::EventParticipantRow& p : e.participants) {
            if (p.entity_type == TYPE_CIVILIZATION) {
                append_once(civ_list(p.entity_id), id);
            } else if (p.entity_type == TYPE_SETTLEMENT) {
                append_once(settlement_list(p.entity_id), id);
            } else if (p.entity_type == TYPE_REGION) {
                append_once(region_list(p.entity_id), id);
            }
        }
        for (const std::int64_t c : e.causes) {
            const std::optional<std::size_t> ci = index_of(c, n);
            if (!ci) {
                timeline_fatal("no event " + num(c));
            }
            append_once(t.effects_[*ci], id);
        }
    }
    return t;
}

// --- Answers (6.2) -----------------------------------------------------------

void Timeline::require_year(std::int64_t year) const {
    if (!valid_year(year)) {
        timeline_fatal("year " + num(year) + " outside 0 to " + num(end_year_));
    }
}

std::size_t Timeline::region_position(std::int64_t region) const {
    const std::optional<std::size_t> p = position_of(region_ids_, region);
    if (!p) {
        timeline_fatal("no region " + num(region));
    }
    return *p;
}

RegionHolding Timeline::holder_at(std::size_t position, std::int64_t year) const {
    RegionHolding h;
    h.region = region_ids_[position];
    const auto first = control_.begin() + static_cast<std::ptrdiff_t>(control_offsets_[position]);
    const auto last = control_.begin() + static_cast<std::ptrdiff_t>(control_offsets_[position + 1U]);
    // The first row with from_year > Y; the row before it is the last with
    // from_year <= Y.
    const auto after = std::upper_bound(first, last, year, [](std::int64_t y, const db::RegionControlRow& r) {
        return y < r.from_year;
    });
    if (after == first) {
        return h;  // no row has started: free since the start (since 0)
    }
    const db::RegionControlRow& r = *(after - 1);
    if (!r.to_year || year < *r.to_year) {
        h.civ = r.civ;
        h.since = r.from_year;
        h.event = r.event;
    } else {
        h.since = *r.to_year;  // rows do not overlap: earlier rows end earlier
    }
    return h;
}

SettlementState Timeline::settlement_at(std::size_t index, std::int64_t year) const {
    SettlementState s;
    s.settlement = static_cast<std::int64_t>(index) + 1;
    if (settlement_founded_[index] > year) {
        return s;
    }
    const auto first = census_.begin() + static_cast<std::ptrdiff_t>(census_offsets_[index]);
    const auto last = census_.begin() + static_cast<std::ptrdiff_t>(census_offsets_[index + 1U]);
    const auto after = std::upper_bound(first, last, year, [](std::int64_t y, const db::SettlementCensusRow& r) {
        return y < r.year;
    });
    if (after == first) {
        return s;  // not reached on checked data: the first row is at founded_year
    }
    const db::SettlementCensusRow& r = *(after - 1);
    s.exists = true;
    s.owner = r.owner_civ;
    s.kind = r.kind;
    s.status = r.status;
    s.population = r.population;
    s.since = r.year;
    return s;
}

namespace {

// The id of the row with start_year <= Y <= end_year, or 0 (rows ascending
// start_year, disjoint).
template <class Row>
std::int64_t covering(const std::vector<Row>& rows, std::int64_t year) {
    const auto after = std::upper_bound(rows.begin(), rows.end(), year,
                                        [](std::int64_t y, const Row& r) { return y < r.start_year; });
    if (after == rows.begin()) {
        return 0;
    }
    const Row& r = *(after - 1);
    return year <= r.end_year ? r.id : 0;
}

}  // namespace

std::int64_t Timeline::phase_at(std::int64_t year) const {
    if (year == end_year_) {
        return 0;  // the present lies in no phase (section 4)
    }
    return covering(phases_, year);
}

std::int64_t Timeline::age_at(std::int64_t year) const {
    if (year == end_year_) {
        return ages_.empty() ? 0 : ages_.back().id;  // the present belongs to the last age
    }
    return covering(ages_, year);
}

void Timeline::events_at(std::int64_t year, std::vector<std::int64_t>& events,
                         std::vector<std::int64_t>& starting) const {
    events.clear();
    starting.clear();
    // The ids dated Y: a contiguous run, since year is non-decreasing with id.
    const auto lo = std::lower_bound(event_year_.begin(), event_year_.end(), year);
    const auto hi = std::upper_bound(lo, event_year_.end(), year);
    const auto first_id = static_cast<std::int64_t>(lo - event_year_.begin()) + 1;
    const auto last_id = static_cast<std::int64_t>(hi - event_year_.begin());  // inclusive
    for (std::int64_t id = first_id; id <= last_id; ++id) {
        starting.push_back(id);
    }
    // Merged, ascending, with the multi-year events begun before Y and not
    // yet ended.
    std::size_t s = 0;
    for (const std::int64_t id : multi_year_) {
        const auto i = static_cast<std::size_t>(id - 1);
        if (event_year_[i] >= year || event_end_[i] < year) {
            continue;
        }
        while (s < starting.size() && starting[s] < id) {
            events.push_back(starting[s]);
            ++s;
        }
        events.push_back(id);
    }
    while (s < starting.size()) {
        events.push_back(starting[s]);
        ++s;
    }
}

RegionHolding Timeline::region_holder(std::int64_t region, std::int64_t year) const {
    require_year(year);
    return holder_at(region_position(region), year);
}

std::string_view Timeline::region_name(std::int64_t region, std::int64_t year) const {
    require_year(year);
    const std::size_t p = region_position(region);
    const auto first = names_.begin() + static_cast<std::ptrdiff_t>(name_offsets_[p]);
    const auto last = names_.begin() + static_cast<std::ptrdiff_t>(name_offsets_[p + 1U]);
    const auto after = std::upper_bound(first, last, year, [](std::int64_t y, const db::RegionNameRow& r) {
        return y < r.from_year;
    });
    if (after == first) {
        return region_names_[p];
    }
    return (after - 1)->name;
}

SettlementState Timeline::settlement_state(std::int64_t settlement, std::int64_t year) const {
    require_year(year);
    const std::optional<std::size_t> i = index_of(settlement, settlement_founded_.size());
    if (!i) {
        timeline_fatal("no settlement " + num(settlement));
    }
    return settlement_at(*i, year);
}

void Timeline::year_state(std::int64_t year, YearState& out) const {
    require_year(year);
    out.year = year;
    out.phase = phase_at(year);
    out.age = age_at(year);
    out.regions.resize(region_ids_.size());
    for (std::size_t p = 0; p < region_ids_.size(); ++p) {
        out.regions[p] = holder_at(p, year);
    }
    out.settlements.resize(settlement_founded_.size());
    for (std::size_t s = 0; s < settlement_founded_.size(); ++s) {
        out.settlements[s] = settlement_at(s, year);
    }
    out.relations.clear();
    for (std::size_t i = 0; i < relations_.size(); ++i) {
        const db::RelationRow& r = relations_[i];
        if (r.since <= year && (!r.until || year < *r.until)) {
            out.relations.push_back(i);
        }
    }
    out.civilizations.clear();
    for (std::size_t c = 0; c < civ_founded_.size(); ++c) {
        if (civ_founded_[c] <= year && (!civ_ended_[c] || year < *civ_ended_[c])) {
            out.civilizations.push_back(static_cast<std::int64_t>(c) + 1);
        }
    }
    events_at(year, out.events, out.events_starting);
}

YearState Timeline::year_state(std::int64_t year) const {
    YearState out;
    year_state(year, out);
    return out;
}

std::optional<std::int64_t> Timeline::previous_event_year(std::int64_t year) const {
    require_year(year);
    const auto lo = std::lower_bound(event_year_.begin(), event_year_.end(), year);
    if (lo == event_year_.begin()) {
        return std::nullopt;
    }
    return *(lo - 1);
}

std::optional<std::int64_t> Timeline::next_event_year(std::int64_t year) const {
    require_year(year);
    const auto hi = std::upper_bound(event_year_.begin(), event_year_.end(), year);
    if (hi == event_year_.end()) {
        return std::nullopt;
    }
    return *hi;
}

std::span<const std::int64_t> Timeline::events_of_civilization(std::int64_t civ) const {
    const std::optional<std::size_t> i = index_of(civ, civ_events_.size());
    if (!i) {
        timeline_fatal("no civilization " + num(civ));
    }
    return civ_events_[*i];
}

std::span<const std::int64_t> Timeline::events_of_region(std::int64_t region) const {
    return region_events_[region_position(region)];
}

std::span<const std::int64_t> Timeline::events_of_settlement(std::int64_t settlement) const {
    const std::optional<std::size_t> i = index_of(settlement, settlement_events_.size());
    if (!i) {
        timeline_fatal("no settlement " + num(settlement));
    }
    return settlement_events_[*i];
}

std::span<const std::int64_t> Timeline::effects(std::int64_t event) const {
    const std::optional<std::size_t> i = index_of(event, effects_.size());
    if (!i) {
        timeline_fatal("no event " + num(event));
    }
    return effects_[*i];
}

// --- load_history (5.2) ------------------------------------------------------

db::Result<LoadedHistory> load_history(const db::WorldReader& world) {
    try {
        db::Result<HistoryTables> tables = read_history_tables(world);
        if (!tables.ok()) {
            return tables.error();
        }
        db::Result<HistoryView> view = HistoryView::load(world);
        if (!view.ok()) {
            return view.error();
        }
        if (const std::optional<std::string> why = history_reads_disagree(view.value(), tables.value())) {
            return db::Error{db::ErrorCode::corrupt_layer, *why};
        }
        if (const std::optional<std::string> why = timeline_reads_disagree(view.value(), tables.value())) {
            return db::Error{db::ErrorCode::corrupt_layer, *why};
        }
        Timeline timeline = Timeline::build(view.value(), tables.value());
        return LoadedHistory{std::move(tables.value()), std::move(view.value()), std::move(timeline)};
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while building the timeline"};
    }
}

}  // namespace worldgen::history
