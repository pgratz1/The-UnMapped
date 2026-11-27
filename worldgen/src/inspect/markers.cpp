// Settlement and place markers (spec P4-04 sections 3 to 6).

#include "worldgen/inspect/markers.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/inspect/geography.hpp"

namespace worldgen::inspect {

namespace {

std::string num(std::int64_t v) {
    return db::format_i64(v);
}

[[noreturn]] void markers_fatal(const std::string& what) {
    throw FatalError("markers: " + what);
}

constexpr std::int64_t TYPE_CIVILIZATION = static_cast<std::int64_t>(history::EntityType::civilization);

// The index id - 1 of an id in 1 .. n, or std::nullopt.
std::optional<std::size_t> index_of(std::int64_t id, std::size_t n) noexcept {
    if (id < 1 || static_cast<std::uint64_t>(id) > static_cast<std::uint64_t>(n)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(id - 1);
}

bool standing_status(std::int64_t status) noexcept {
    return status == 1 || status == 2;
}

// --- Table pass (3.5) -------------------------------------------------------

// The rows of table pass, checked with the report's row checks against a
// map_size x map_size grid; std::nullopt and the error in `error` on failure.
std::optional<std::vector<db::PassRow>> read_passes(const db::WorldReader& world, db::Error& error) {
    db::Result<std::vector<std::vector<db::ColumnValue>>> rows = world.table_rows(db::PASS_TABLE);
    if (!rows.ok()) {
        error = rows.error();
        return std::nullopt;
    }
    const std::uint32_t size = world.map_size();
    const std::uint64_t cells = std::uint64_t{size} * std::uint64_t{size};
    std::vector<db::PassRow> passes;
    passes.reserve(rows.value().size());
    for (const std::vector<db::ColumnValue>& row : rows.value()) {
        PassRecord record;
        if (std::optional<std::string> why = read_pass_row(row, size, size, cells, record)) {
            error = db::Error{db::ErrorCode::corrupt_layer, "corrupt table 'pass': " + *why};
            return std::nullopt;
        }
        // read_pass_row checked 9 columns, integers 0..7 and a blob path.
        db::PassRow p;
        p.id = row[0].integer;
        p.range_id = row[1].integer;
        p.kind = row[2].integer;
        p.capped = row[3].integer;
        p.col_x = row[4].integer;
        p.col_y = row[5].integer;
        p.saddle_dm = row[6].integer;
        p.col_dm = row[7].integer;
        p.path.assign(row[8].bytes.begin(), row[8].bytes.end());
        passes.push_back(std::move(p));
    }
    return passes;
}

// --- Capital runs (3.2) -----------------------------------------------------

// One kind that sets a capital: its key and the role naming the civilization.
struct CapitalKind {
    std::string_view key;
    std::string_view role;
};
constexpr std::array<CapitalKind, 3> CAPITAL_KINDS = {{
    {"settlement_founded", "founder"},
    {"civ_seceded", "civilization"},
    {"capital_moved", "civilization"},
}};

// For each kind id (index id - 1) the seq of the role that names the
// civilization whose capital the event sets, 0 when the kind sets none.
std::vector<std::int64_t> capital_role_seqs(const history::EventCatalogue& catalogue) {
    std::vector<std::int64_t> seqs(catalogue.kinds.size(), 0);
    for (const CapitalKind& ck : CAPITAL_KINDS) {
        const history::KindDef* kind = catalogue.find(ck.key);
        if (kind == nullptr) {
            continue;  // a kind the world lacks contributes nothing
        }
        const history::RoleDef* role = kind->find_role(ck.role);
        if (role == nullptr) {
            continue;
        }
        if (const std::optional<std::size_t> k = index_of(kind->id, seqs.size())) {
            seqs[*k] = role->seq;
        }
    }
    return seqs;
}

}  // namespace

// --- Symbols (3.4) -----------------------------------------------------------

EventSymbol event_symbol(std::string_view kind_key) noexcept {
    if (kind_key == "sacked" || kind_key == "settlement_captured" || kind_key == "hold_lost") {  // P5-07 11.1
        return EventSymbol::sack;
    }
    if (kind_key == "siege" || kind_key == "refuge_held") {
        return EventSymbol::siege;
    }
    if (kind_key == "battle" || kind_key == "battle_at_pass" || kind_key == "allied_battle" ||
        kind_key == "battle_led") {  // P5-04 11.1, P5-06 11.1
        return EventSymbol::battle;
    }
    if (kind_key == "plague" || kind_key == "plague_outbreak" || kind_key == "plague_spread") {
        return EventSymbol::plague;
    }
    return EventSymbol::plain;
}

std::string_view mine_depth_word(std::int64_t depth) noexcept {
    switch (depth) {
        case 1: return "below daylight";
        case 2: return "to the mountain's roots";
        case 3: return "to the old deep";
        default: return {};
    }
}

std::string_view event_symbol_word(EventSymbol symbol) noexcept {
    switch (symbol) {
        case EventSymbol::plain: return "plain";
        case EventSymbol::plague: return "plague";
        case EventSymbol::battle: return "battle";
        case EventSymbol::siege: return "siege";
        case EventSymbol::sack: return "sack";
    }
    return {};
}

Rgb event_symbol_colour(EventSymbol symbol) noexcept {
    switch (symbol) {
        case EventSymbol::sack: return Rgb{200, 0, 200};
        case EventSymbol::siege: return Rgb{255, 150, 0};
        case EventSymbol::battle: return Rgb{230, 30, 30};
        case EventSymbol::plague: return Rgb{0, 190, 120};  // P4-05 12.1 F5
        case EventSymbol::plain: break;
    }
    return Rgb{255, 255, 255};
}

// --- MarkerIndex --------------------------------------------------------------

db::Result<MarkerIndex> MarkerIndex::load(const db::WorldReader& world, const history::LoadedHistory& history) {
    try {
        MarkerIndex index;
        const history::HistoryTables& tables = history.tables;
        const history::HistoryView& view = history.view;
        index.end_year_ = history.timeline.end_year();

        // Table region: the seed cells, ids equal in order to the history's.
        db::Result<std::vector<db::RegionRow>> regions = db::read_region_rows(world);
        if (!regions.ok()) {
            return regions.error();
        }
        // Table pass.
        db::Error pass_error;
        std::optional<std::vector<db::PassRow>> passes = read_passes(world, pass_error);
        if (!passes) {
            return pass_error;
        }
        index.passes_ = std::move(*passes);
        bool same_regions = regions.value().size() == tables.regions.size();
        for (std::size_t i = 0; same_regions && i < tables.regions.size(); ++i) {
            same_regions = regions.value()[i].id == tables.regions[i].id;
        }
        if (!same_regions) {
            return db::Error{db::ErrorCode::corrupt_layer,
                             "corrupt table 'region': the file changed while it was read"};
        }
        // Region ids ascending (primary-key order) with their seed cells.
        std::vector<std::int64_t> region_ids;
        std::vector<std::pair<std::int64_t, std::int64_t>> region_seeds;
        region_ids.reserve(regions.value().size());
        region_seeds.reserve(regions.value().size());
        for (const db::RegionRow& r : regions.value()) {
            region_ids.push_back(r.id);
            region_seeds.emplace_back(r.seed_x, r.seed_y);
        }

        // Settlements (ids 1 .. n, checked by load_history).
        const std::size_t settlement_count = tables.settlements.size();
        if (settlement_count != history.timeline.settlement_count()) {
            markers_fatal("settlement count " + num(static_cast<std::int64_t>(settlement_count)) +
                          " differs from the timeline's");
        }
        index.settlement_x_.reserve(settlement_count);
        index.settlement_y_.reserve(settlement_count);
        index.settlement_founded_.reserve(settlement_count);
        index.ruined_year_.assign(settlement_count, -1);
        index.ruin_event_.assign(settlement_count, 0);
        for (std::size_t i = 0; i < settlement_count; ++i) {
            const db::SettlementRow& s = tables.settlements[i];
            if (s.id != static_cast<std::int64_t>(i) + 1) {
                markers_fatal("settlement " + num(s.id) + " out of order");
            }
            index.settlement_x_.push_back(s.x);
            index.settlement_y_.push_back(s.y);
            index.settlement_founded_.push_back(s.founded_year);
            index.ruin_event_[i] = s.ruin_event;
        }
        // 3.3: the ruined year from the first census row with status 3.
        for (const db::SettlementCensusRow& c : tables.census) {
            if (c.status != 3) {
                continue;
            }
            const std::optional<std::size_t> i = index_of(c.settlement, settlement_count);
            if (!i) {
                markers_fatal("census row of settlement " + num(c.settlement) + ": no such settlement");
            }
            // Census rows are in (settlement, year) order: the first one wins.
            if (index.ruined_year_[*i] < 0) {
                index.ruined_year_[*i] = c.year;
            }
        }

        // Civilizations (ids 1 .. n): the timeline's alive rule.
        const std::size_t civ_count = tables.civilizations.size();
        index.civ_founded_.reserve(civ_count);
        index.civ_ended_.reserve(civ_count);
        for (std::size_t i = 0; i < civ_count; ++i) {
            const db::CivilizationRow& c = tables.civilizations[i];
            if (c.id != static_cast<std::int64_t>(i) + 1) {
                markers_fatal("civilization " + num(c.id) + " out of order");
            }
            index.civ_founded_.push_back(c.founded_year);
            index.civ_ended_.push_back(c.ended_year ? *c.ended_year : -1);
        }

        // One pass over the log: capital runs (3.2), the ruin_faded fallback
        // (3.3) and each event's place and symbol (3.4).
        const history::EventCatalogue& catalogue = view.catalogue();
        const std::vector<std::int64_t> capital_roles = capital_role_seqs(catalogue);
        std::vector<EventSymbol> kind_symbols(catalogue.kinds.size(), EventSymbol::plain);
        std::int64_t ruin_faded_kind = 0;
        for (std::size_t k = 0; k < catalogue.kinds.size(); ++k) {
            kind_symbols[k] = event_symbol(catalogue.kinds[k].key);
            if (catalogue.kinds[k].key == "ruin_faded" && ruin_faded_kind == 0) {
                ruin_faded_kind = catalogue.kinds[k].id;
            }
        }
        std::vector<std::vector<CapitalEntry>> runs(civ_count);
        // The ruin event found through ruin_faded, per settlement: from the
        // first ruin_faded event (smallest id) naming it, its smallest cause.
        std::vector<std::uint8_t> faded_seen(settlement_count, 0);
        std::vector<std::int64_t> faded_cause(settlement_count, 0);
        const std::span<const history::HistoryView::Event> events = view.events();
        index.event_places_.resize(events.size());
        for (std::size_t e = 0; e < events.size(); ++e) {
            const history::HistoryView::Event& ev = events[e];
            const db::EventRow& row = ev.row;
            if (row.id != static_cast<std::int64_t>(e) + 1) {
                markers_fatal("event " + num(row.id) + " out of order");
            }
            const std::optional<std::size_t> kind = index_of(row.kind, catalogue.kinds.size());
            if (!kind) {
                markers_fatal("event " + num(row.id) + ": no kind " + num(row.kind));
            }
            std::optional<std::size_t> settlement;
            if (row.settlement != 0) {
                settlement = index_of(row.settlement, settlement_count);
                if (!settlement) {
                    markers_fatal("event " + num(row.id) + ": no settlement " + num(row.settlement));
                }
            }
            // 3.4: the place.
            EventPlace& place = index.event_places_[e];
            place.symbol = kind_symbols[*kind];
            if (settlement) {
                place.placed = true;
                place.x = index.settlement_x_[*settlement];
                place.y = index.settlement_y_[*settlement];
            } else if (row.region != 0) {
                const auto it = std::lower_bound(region_ids.begin(), region_ids.end(), row.region);
                if (it == region_ids.end() || *it != row.region) {
                    markers_fatal("event " + num(row.id) + ": no region " + num(row.region));
                }
                const auto p = static_cast<std::size_t>(it - region_ids.begin());
                place.placed = true;
                place.x = region_seeds[p].first;
                place.y = region_seeds[p].second;
            }
            // 3.2: a capital set by this event.
            const std::int64_t role = capital_roles[*kind];
            if (role != 0 && settlement) {
                for (const db::EventParticipantRow& part : ev.participants) {
                    if (part.role != role || part.entity_type != TYPE_CIVILIZATION) {
                        continue;
                    }
                    const std::optional<std::size_t> c = index_of(part.entity_id, civ_count);
                    if (!c) {
                        markers_fatal("event " + num(row.id) + ": no civilization " + num(part.entity_id));
                    }
                    runs[*c].push_back(CapitalEntry{row.year, row.id, row.settlement});
                }
            }
            // 3.3: the ruin_faded fallback.
            if (row.kind == ruin_faded_kind && settlement && faded_seen[*settlement] == 0) {
                faded_seen[*settlement] = 1;
                if (!ev.causes.empty()) {
                    faded_cause[*settlement] = *std::min_element(ev.causes.begin(), ev.causes.end());
                }
            }
        }
        for (std::size_t i = 0; i < settlement_count; ++i) {
            if (index.ruin_event_[i] == 0) {
                index.ruin_event_[i] = faded_cause[i];
            }
        }
        index.run_offsets_.reserve(civ_count + 1U);
        index.run_offsets_.push_back(0);
        for (std::vector<CapitalEntry>& run : runs) {
            // Events are in id order with non-decreasing year (load_history),
            // so each run is ascending in both, as the binary search needs.
            index.capitals_.insert(index.capitals_.end(), run.begin(), run.end());
            index.run_offsets_.push_back(index.capitals_.size());
        }
        return index;
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while building the markers"};
    }
}

void MarkerIndex::check_history(const history::LoadedHistory& history) const {
    if (history.tables.settlements.size() != settlement_x_.size() ||
        history.timeline.settlement_count() != settlement_x_.size()) {
        markers_fatal("the history has " + num(static_cast<std::int64_t>(history.tables.settlements.size())) +
                      " settlements, the index " + num(static_cast<std::int64_t>(settlement_x_.size())));
    }
    if (history.view.events().size() != event_places_.size() ||
        history.tables.civilizations.size() != civ_founded_.size() || history.timeline.end_year() != end_year_) {
        markers_fatal("the history is not the one the index was loaded from");
    }
}

std::size_t MarkerIndex::settlement_index(std::int64_t settlement) const {
    const std::optional<std::size_t> i = index_of(settlement, settlement_x_.size());
    if (!i) {
        markers_fatal("no settlement " + num(settlement));
    }
    return *i;
}

std::int64_t MarkerIndex::run_settlement(std::size_t c, std::int64_t year) const {
    const auto first = capitals_.begin() + static_cast<std::ptrdiff_t>(run_offsets_[c]);
    const auto last = capitals_.begin() + static_cast<std::ptrdiff_t>(run_offsets_[c + 1U]);
    const auto after =
        std::upper_bound(first, last, year, [](std::int64_t y, const CapitalEntry& e) { return y < e.year; });
    if (after == first) {
        return 0;
    }
    return (after - 1)->settlement;
}

std::int64_t MarkerIndex::capital_from(std::size_t c, std::int64_t year, bool alive,
                                       std::span<const history::SettlementState> states) const {
    if (!alive) {
        return 0;
    }
    const std::int64_t s = run_settlement(c, year);
    const std::optional<std::size_t> i = index_of(s, states.size());
    if (!i) {
        return 0;
    }
    const history::SettlementState& st = states[*i];
    const auto civ = static_cast<std::int64_t>(c) + 1;
    if (!st.exists || !standing_status(st.status) || st.owner != civ) {
        return 0;
    }
    return s;
}

void MarkerIndex::markers_at(const history::LoadedHistory& history, const history::YearState& state,
                             const MarkerOptions& options, YearMarkers& out) const {
    check_history(history);
    const std::int64_t year = state.year;
    if (year < 0 || year > end_year_) {
        markers_fatal("year " + num(year) + " outside 0 to " + num(end_year_));
    }
    if (state.settlements.size() != settlement_x_.size()) {
        markers_fatal("the year state has " + num(static_cast<std::int64_t>(state.settlements.size())) +
                      " settlements, the index " + num(static_cast<std::int64_t>(settlement_x_.size())));
    }
    out.year = year;

    // 3.2: the capital of each civilization alive at Y (index id - 1; 0 none).
    std::vector<std::int64_t> capital_of(civ_founded_.size(), 0);
    for (const std::int64_t civ : state.civilizations) {
        const std::optional<std::size_t> c = index_of(civ, civ_founded_.size());
        if (!c) {
            markers_fatal("no civilization " + num(civ));
        }
        capital_of[*c] = capital_from(*c, year, true, state.settlements);
    }

    // 3.1 and 3.3: settlement entries, ascending id.
    out.settlements.clear();
    for (std::size_t i = 0; i < state.settlements.size(); ++i) {
        const history::SettlementState& st = state.settlements[i];
        // P5-06 25.1 (e): a ruin OLD_RUIN_YEARS or more before Y is an old
        // ruin, an entry of type faded, shown only with the option.
        const bool old_ruin = st.exists && st.status == 3 && ruined_year_[i] >= 0 &&
                              year - ruined_year_[i] >= OLD_RUIN_YEARS;
        if (!st.exists || ((st.status == 4 || old_ruin) && !options.faded)) {
            continue;
        }
        SettlementMarker m;
        m.settlement = static_cast<std::int64_t>(i) + 1;
        m.x = settlement_x_[i];
        m.y = settlement_y_[i];
        m.kind = st.kind;
        m.status = st.status;
        m.owner = st.owner;
        m.population = st.population;
        m.since = st.since;
        m.founded = settlement_founded_[i];
        if (standing_status(st.status)) {
            const std::optional<std::size_t> c = index_of(st.owner, capital_of.size());
            m.capital = c && capital_of[*c] == m.settlement;
        } else {
            m.ruined_year = ruined_year_[i];
            m.ruin_event = ruin_event_[i];
            m.old_ruin = old_ruin;
        }
        out.settlements.push_back(m);
    }

    // 3.4: places, grouped by cell in ascending smallest event id.
    out.places.clear();
    if (options.events) {
        for (const std::int64_t id : state.events) {
            const EventPlace& ep = place_of(id);  // P4-05 4.6: the one place rule
            if (!ep.placed) {
                continue;
            }
            // At most a handful of places a year: a linear search by cell.
            PlaceMarker* found = nullptr;
            for (PlaceMarker& p : out.places) {
                if (p.x == ep.x && p.y == ep.y) {
                    found = &p;
                    break;
                }
            }
            if (found == nullptr) {
                out.places.push_back(PlaceMarker{ep.x, ep.y, ep.symbol, {id}});
            } else {
                found->events.push_back(id);
                if (static_cast<std::uint8_t>(ep.symbol) > static_cast<std::uint8_t>(found->symbol)) {
                    found->symbol = ep.symbol;
                }
            }
        }
    }

    // P5-04 11.3: the mines existing at Y, and P5-06 11.4 the towers,
    // ascending id.
    out.sites.clear();
    if (options.sites) {
        for (const std::int64_t id : history.timeline.sites_existing(year)) {
            const history::SiteState s = history.timeline.site_at(id, year);
            // P5-08 11.1: shrines, temples and sanctums by their options.
            const bool shown = s.kind == 3 || s.kind == 4 || (s.kind == 5 && options.shrines) ||
                               ((s.kind == 6 || s.kind == 7) && options.temples);
            if (shown && s.open) {
                out.sites.push_back(SiteMarker{id, s.x, s.y, s.kind, s.founder, s.settlement});
            }
        }
        std::stable_sort(out.sites.begin(), out.sites.end(),
                         [](const SiteMarker& a, const SiteMarker& b) { return a.site < b.site; });
    }

    // 3.5: passes, ascending id.
    out.passes.clear();
    if (options.passes) {
        for (const db::PassRow& p : passes_) {
            out.passes.push_back(PassMarker{p.id, p.col_x, p.col_y, p.kind});
        }
    }
}

YearMarkers MarkerIndex::markers_at(const history::LoadedHistory& history, std::int64_t year,
                                    const MarkerOptions& options) const {
    const history::YearState state = history.timeline.year_state(year);  // FatalError for a bad year
    YearMarkers out;
    markers_at(history, state, options, out);
    return out;
}

std::int64_t MarkerIndex::capital(const history::LoadedHistory& history, std::int64_t civ, std::int64_t year) const {
    check_history(history);
    const std::optional<std::size_t> c = index_of(civ, civ_founded_.size());
    if (!c) {
        markers_fatal("no civilization " + num(civ));
    }
    if (year < 0 || year > end_year_) {
        markers_fatal("year " + num(year) + " outside 0 to " + num(end_year_));
    }
    const bool alive = civ_founded_[*c] <= year && (civ_ended_[*c] < 0 || year < civ_ended_[*c]);
    if (!alive) {
        return 0;
    }
    const std::int64_t s = run_settlement(*c, year);
    if (s == 0 || !index_of(s, settlement_x_.size())) {
        return 0;
    }
    const history::SettlementState st = history.timeline.settlement_state(s, year);
    if (!st.exists || !standing_status(st.status) || st.owner != civ) {
        return 0;
    }
    return s;
}

const MarkerIndex::EventPlace& MarkerIndex::place_of(std::int64_t event) const {
    const std::optional<std::size_t> e = index_of(event, event_places_.size());
    if (!e) {
        markers_fatal("no event " + num(event));
    }
    return event_places_[*e];
}

std::optional<std::array<std::int64_t, 2>> MarkerIndex::event_place(const history::LoadedHistory& history,
                                                                    std::int64_t event) const {
    check_history(history);
    const EventPlace& ep = place_of(event);
    if (!ep.placed) {
        return std::nullopt;
    }
    return std::array<std::int64_t, 2>{ep.x, ep.y};
}

std::int64_t MarkerIndex::ruined_year(std::int64_t settlement) const {
    return ruined_year_[settlement_index(settlement)];
}

std::int64_t MarkerIndex::ruin_event(std::int64_t settlement) const {
    return ruin_event_[settlement_index(settlement)];
}

// --- Glyphs (section 5) --------------------------------------------------------

namespace {

enum class Shape : std::uint8_t { disc, square, diamond };

std::int32_t iabs(std::int32_t v) noexcept {
    return v < 0 ? -v : v;
}

// The predicates of 5.1; r < 0 holds no cell.
bool in_shape(Shape shape, std::int32_t r, std::int32_t dx, std::int32_t dy) noexcept {
    if (r < 0) {
        return false;
    }
    switch (shape) {
        case Shape::disc: return dx * dx + dy * dy <= r * r + r;
        case Shape::square: return std::max(iabs(dx), iabs(dy)) <= r;
        case Shape::diamond: return iabs(dx) + iabs(dy) <= r;
    }
    return false;
}

bool in_disc(std::int32_t r, std::int32_t dx, std::int32_t dy) noexcept {
    return in_shape(Shape::disc, r, dx, dy);
}

class GlyphGrid {
public:
    explicit GlyphGrid(std::int32_t radius)
        : radius_(radius), side_(2 * radius + 1),
          cells_(static_cast<std::size_t>(side_) * static_cast<std::size_t>(side_), GLYPH_NONE) {}

    [[nodiscard]] std::int32_t radius() const noexcept { return radius_; }
    [[nodiscard]] std::uint8_t at(std::int32_t dx, std::int32_t dy) const noexcept { return cells_[pos(dx, dy)]; }
    void set(std::int32_t dx, std::int32_t dy, std::uint8_t c) noexcept { cells_[pos(dx, dy)] = c; }

    // The halo rule of 5.1: a 0 cell with an 8-neighbour of class `of`
    // (ink; P4-05 F3: fill for the faded cross).
    void halo(std::uint8_t of = GLYPH_INK) {
        std::vector<std::uint8_t> next = cells_;
        for (std::int32_t dy = -radius_; dy <= radius_; ++dy) {
            for (std::int32_t dx = -radius_; dx <= radius_; ++dx) {
                if (at(dx, dy) != GLYPH_NONE) {
                    continue;
                }
                bool near_ink = false;
                for (std::int32_t ny = dy - 1; ny <= dy + 1 && !near_ink; ++ny) {
                    for (std::int32_t nx = dx - 1; nx <= dx + 1 && !near_ink; ++nx) {
                        if ((nx != dx || ny != dy) && iabs(nx) <= radius_ && iabs(ny) <= radius_ &&
                            at(nx, ny) == of) {
                            near_ink = true;
                        }
                    }
                }
                if (near_ink) {
                    next[pos(dx, dy)] = GLYPH_HALO;
                }
            }
        }
        cells_ = std::move(next);
    }

    [[nodiscard]] Glyph take() && { return Glyph{radius_, std::move(cells_)}; }

private:
    [[nodiscard]] std::size_t pos(std::int32_t dx, std::int32_t dy) const noexcept {
        return static_cast<std::size_t>(dy + radius_) * static_cast<std::size_t>(side_) +
               static_cast<std::size_t>(dx + radius_);
    }

    std::int32_t radius_;
    std::int32_t side_;
    std::vector<std::uint8_t> cells_;
};

// 5.2.
Glyph settlement_glyph(std::uint8_t kind, bool declining, bool capital) {
    Shape shape = Shape::disc;
    std::int32_t r = 1;
    switch (kind) {
        case 1: shape = Shape::disc; r = 1; break;
        case 2: shape = Shape::disc; r = 2; break;
        case 3: shape = Shape::square; r = 2; break;
        case 4: shape = Shape::square; r = 3; break;
        default: shape = Shape::diamond; r = 3; break;  // 5, checked by the caller
    }
    const std::int32_t o = r + 1 + (capital ? 1 : 0);
    GlyphGrid g(o + 1);
    for (std::int32_t dy = -g.radius(); dy <= g.radius(); ++dy) {
        for (std::int32_t dx = -g.radius(); dx <= g.radius(); ++dx) {
            if (in_shape(shape, r, dx, dy)) {
                g.set(dx, dy, declining && in_shape(shape, r - 1, dx, dy) ? GLYPH_INTERIOR : GLYPH_FILL);
            } else if (in_shape(shape, o, dx, dy)) {
                g.set(dx, dy, GLYPH_INK);
            }
        }
    }
    if (kind == 4) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                g.set(dx, dy, GLYPH_INK);
            }
        }
    }
    g.halo();
    return std::move(g).take();
}

// P4-05 12.1 F3 (replaces P4-04 5.3): the ruin, a thick X of radius 4,
// ink where max(|dx|, |dy|) <= 3 and ||dx| - |dy|| <= 1, then the halo
// rule (33 ink, 44 halo).
Glyph ruin_glyph() {
    GlyphGrid g(4);
    for (std::int32_t dy = -3; dy <= 3; ++dy) {
        for (std::int32_t dx = -3; dx <= 3; ++dx) {
            if (iabs(iabs(dx) - iabs(dy)) <= 1) {
                g.set(dx, dy, GLYPH_INK);
            }
        }
    }
    g.halo();
    return std::move(g).take();
}

// F3: the faded ruin, radius 3, fill where |dx| = |dy| <= 2, then a halo on
// every 0 cell with an 8-neighbour of class fill (9 fill, 36 halo).
Glyph faded_glyph() {
    GlyphGrid g(3);
    for (std::int32_t d = -2; d <= 2; ++d) {
        g.set(d, d, GLYPH_FILL);
        g.set(d, -d, GLYPH_FILL);
    }
    g.halo(GLYPH_FILL);
    return std::move(g).take();
}

// 5.3: the pass chevron.
Glyph pass_glyph() {
    GlyphGrid g(3);
    for (std::int32_t dx = -2; dx <= 2; ++dx) {
        g.set(dx, iabs(dx) - 1, GLYPH_INK);
    }
    g.halo();
    return std::move(g).take();
}

// 5.4: the place ring.
Glyph ring_glyph() {
    GlyphGrid g(11);
    for (std::int32_t dy = -11; dy <= 11; ++dy) {
        for (std::int32_t dx = -11; dx <= 11; ++dx) {
            if ((in_disc(10, dx, dy) && !in_disc(9, dx, dy)) || (in_disc(8, dx, dy) && !in_disc(7, dx, dy))) {
                g.set(dx, dy, GLYPH_INK);
            } else if (in_disc(9, dx, dy) && !in_disc(8, dx, dy)) {
                g.set(dx, dy, GLYPH_FILL);
            }
        }
    }
    g.halo();
    return std::move(g).take();
}

// P5-04 11.3: the mine, a pick and a hammer crossed in the 7 x 7 cell: the
// two diagonals of radius 2 with a head at each upper end, then the halo on
// every 0 cell with an 8-neighbour of class fill.
Glyph mine_glyph() {
    GlyphGrid g(3);
    for (std::int32_t d = -2; d <= 2; ++d) {
        g.set(d, d, GLYPH_FILL);
        g.set(d, -d, GLYPH_FILL);
    }
    g.set(-1, -2, GLYPH_FILL);
    g.set(-2, -1, GLYPH_FILL);
    g.set(1, -2, GLYPH_FILL);
    g.set(2, -1, GLYPH_FILL);
    g.halo(GLYPH_FILL);
    return std::move(g).take();
}

// P5-06 11.4: the tower, a narrow upright tower with a pointed top in the
// 7 x 7 cell: fill where dx = 0 and -1 <= dy <= 1; ink where |dx| = 1 and
// -1 <= dy <= 2, or dx = 0 and dy is -2 or 2; then the halo rule (20 halo,
// 3 fill, 10 ink).
Glyph tower_glyph() {
    GlyphGrid g(3);
    for (std::int32_t dy = -1; dy <= 1; ++dy) {
        g.set(0, dy, GLYPH_FILL);
    }
    for (std::int32_t dy = -1; dy <= 2; ++dy) {
        g.set(-1, dy, GLYPH_INK);
        g.set(1, dy, GLYPH_INK);
    }
    g.set(0, -2, GLYPH_INK);
    g.set(0, 2, GLYPH_INK);
    g.halo();
    return std::move(g).take();
}

// P5-08 11.1: the shrine, a standing cross: fill where dx = 0 and
// -2 <= dy <= 2 or dy = -1 and |dx| = 1; then the halo rule on fill.
Glyph shrine_glyph() {
    GlyphGrid g(3);
    for (std::int32_t dy = -2; dy <= 2; ++dy) {
        g.set(0, dy, GLYPH_FILL);
    }
    g.set(-1, -1, GLYPH_FILL);
    g.set(1, -1, GLYPH_FILL);
    g.halo(GLYPH_FILL);
    return std::move(g).take();
}

// P5-08 11.1: the temple, a house with a pointed roof: ink at (0, -2),
// (-1, -1), (1, -1), and the sides |dx| = 2 for 0 <= dy <= 2 with the roof
// ends (-2, 0) and (2, 0); fill where |dx| <= 1 and 0 <= dy <= 2 and at
// (0, -1); then the halo rule.
Glyph temple_glyph() {
    GlyphGrid g(3);
    g.set(0, -2, GLYPH_INK);
    g.set(-1, -1, GLYPH_INK);
    g.set(1, -1, GLYPH_INK);
    g.set(0, -1, GLYPH_FILL);
    for (std::int32_t dy = 0; dy <= 2; ++dy) {
        g.set(-2, dy, GLYPH_INK);
        g.set(2, dy, GLYPH_INK);
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
            g.set(dx, dy, GLYPH_FILL);
        }
    }
    g.halo();
    return std::move(g).take();
}

// P5-08 11.1: the sanctum, a diamond: ink where |dx| + |dy| = 2, fill where
// |dx| + |dy| <= 1; then the halo rule.
Glyph sanctum_glyph() {
    GlyphGrid g(3);
    for (std::int32_t dy = -2; dy <= 2; ++dy) {
        for (std::int32_t dx = -2; dx <= 2; ++dx) {
            const std::int32_t d = iabs(dx) + iabs(dy);
            if (d == 2) {
                g.set(dx, dy, GLYPH_INK);
            } else if (d < 2) {
                g.set(dx, dy, GLYPH_FILL);
            }
        }
    }
    g.halo();
    return std::move(g).take();
}

std::string key_text(GlyphKey key) {
    return "base " + num(key.base) + (key.declining ? " declining" : "") + (key.capital ? " capital" : "");
}

}  // namespace

Glyph marker_glyph(GlyphKey key) {
    if (key.base >= 1 && key.base <= 5) {
        return settlement_glyph(key.base, key.declining, key.capital);
    }
    if (key.base == GLYPH_MINE && !key.declining && !key.capital) {
        return mine_glyph();  // P5-04 11.3
    }
    if (key.base == GLYPH_TOWER && !key.declining && !key.capital) {
        return tower_glyph();  // P5-06 11.4
    }
    if (key.base == GLYPH_SHRINE && !key.declining && !key.capital) {
        return shrine_glyph();  // P5-08 11.1
    }
    if (key.base == GLYPH_TEMPLE && !key.declining && !key.capital) {
        return temple_glyph();
    }
    if (key.base == GLYPH_SANCTUM && !key.declining && !key.capital) {
        return sanctum_glyph();
    }
    if (key.declining || key.capital || (key.base < 8 || key.base > 11)) {
        throw FatalError("marker_glyph: no glyph for " + key_text(key));
    }
    switch (key.base) {
        case 8: return ruin_glyph();
        case 9: return faded_glyph();
        case 10: return pass_glyph();
        default: return ring_glyph();  // 11
    }
}

GlyphKey glyph_of(const SettlementMarker& marker) noexcept {
    GlyphKey key;
    if (standing_status(marker.status)) {
        key.base = (marker.kind >= 1 && marker.kind <= 5) ? static_cast<std::uint8_t>(marker.kind) : std::uint8_t{0};
        key.declining = marker.status == 2;
        key.capital = marker.capital;
    } else if (marker.status == 3) {
        key.base = marker.old_ruin ? 9 : 8;  // P5-06 25.1 (e)
    } else if (marker.status == 4) {
        key.base = 9;
    }
    return key;
}

MarkerType settlement_marker_type(const SettlementMarker& marker) noexcept {
    if (standing_status(marker.status)) {
        return MarkerType::standing;
    }
    return marker.status == 4 || (marker.status == 3 && marker.old_ruin) ? MarkerType::faded : MarkerType::ruin;
}

std::array<std::int64_t, 2> site_glyph_offset(const SiteMarker& site) noexcept {
    if (site.kind == 4) {
        return site.settlement != 0 ? std::array<std::int64_t, 2>{-SITE_GLYPH_OFFSET, -SITE_GLYPH_OFFSET}
                                    : std::array<std::int64_t, 2>{0, 0};
    }
    if (site.kind == 5) {  // P5-08 11.1
        return site.settlement != 0 ? std::array<std::int64_t, 2>{SITE_GLYPH_OFFSET, SITE_GLYPH_OFFSET}
                                    : std::array<std::int64_t, 2>{0, 0};
    }
    if (site.kind == 6) {
        return {-SITE_GLYPH_OFFSET, SITE_GLYPH_OFFSET};
    }
    if (site.kind == 7) {
        return {0, 0};
    }
    return {SITE_GLYPH_OFFSET, -SITE_GLYPH_OFFSET};
}

std::uint8_t site_glyph_base(const SiteMarker& site) noexcept {
    switch (site.kind) {
        case 3: return GLYPH_MINE;
        case 4: return GLYPH_TOWER;
        case 5: return GLYPH_SHRINE;  // P5-08 11.1
        case 6: return GLYPH_TEMPLE;
        case 7: return GLYPH_SANCTUM;
        default: return 0;
    }
}

std::string_view shrine_word(bool profane) noexcept {
    return profane ? "cursed by" : "holy to";
}

// --- Drawing (section 6) ---------------------------------------------------------

namespace {

[[noreturn]] void draw_fatal(const std::string& what) {
    throw FatalError("draw_markers: " + what);
}

// The glyphs drawn so far, built once per key in one draw_markers call.
class GlyphCache {
public:
    const Glyph& get(GlyphKey key) {
        const std::size_t slot = slot_of(key);
        if (!glyphs_[slot]) {
            glyphs_[slot] = marker_glyph(key);
        }
        return *glyphs_[slot];
    }

private:
    // 1..5 x declining x capital at 0..19, then 8..11 at 20..23; anything
    // else is refused by marker_glyph before a slot is needed.
    static std::size_t slot_of(GlyphKey key) {
        if (key.base >= 1 && key.base <= 5) {
            return static_cast<std::size_t>(key.base - 1U) * 4U + (key.declining ? 2U : 0U) + (key.capital ? 1U : 0U);
        }
        if (key.base >= 8 && key.base <= 11 && !key.declining && !key.capital) {
            return 20U + static_cast<std::size_t>(key.base - 8U);
        }
        if (key.base == GLYPH_MINE && !key.declining && !key.capital) {
            return 24U;
        }
        if (key.base == GLYPH_TOWER && !key.declining && !key.capital) {
            return 25U;  // P5-06 11.4
        }
        if (key.base >= GLYPH_SHRINE && key.base <= GLYPH_SANCTUM && !key.declining && !key.capital) {
            return 26U + static_cast<std::size_t>(key.base - GLYPH_SHRINE);  // P5-08 11.1
        }
        (void)marker_glyph(key);  // throws its FatalError
        draw_fatal("no glyph for base " + num(key.base));
    }
    std::array<std::optional<Glyph>, 29> glyphs_;
};

class Canvas {
public:
    Canvas(std::span<std::uint8_t> rgb, std::uint32_t width, std::uint32_t height, std::uint32_t scale)
        : rgb_(rgb), width_(width), height_(height), scale_(scale) {}

    // One glyph at cell (x, y), fill colour `fill` (6.2).
    void draw(const Glyph& glyph, std::int64_t x, std::int64_t y, Rgb fill) {
        const std::int32_t r = glyph.radius;
        // A glyph wholly outside the canvas is skipped before x + dx is formed.
        if (x < -std::int64_t{r} || y < -std::int64_t{r} || x > std::int64_t{width_} + r ||
            y > std::int64_t{height_} + r) {
            return;
        }
        const auto side = static_cast<std::size_t>(2 * r + 1);
        for (std::int32_t dy = -r; dy <= r; ++dy) {
            const std::int64_t cy = y + dy;
            if (cy < 0 || cy >= std::int64_t{height_}) {
                continue;
            }
            for (std::int32_t dx = -r; dx <= r; ++dx) {
                const std::int64_t cx = x + dx;
                if (cx < 0 || cx >= std::int64_t{width_}) {
                    continue;
                }
                const std::uint8_t cls = glyph.cells[static_cast<std::size_t>(dy + r) * side +
                                                     static_cast<std::size_t>(dx + r)];
                if (cls == GLYPH_HALO) {
                    put(cx, cy, MARKER_HALO);
                } else if (cls == GLYPH_FILL) {
                    put(cx, cy, fill);
                } else if (cls == GLYPH_INK) {
                    put(cx, cy, MARKER_INK);
                }
            }
        }
    }

private:
    // The k x k block of cell (cx, cy), inside the canvas.
    void put(std::int64_t cx, std::int64_t cy, Rgb c) {
        const std::size_t k = scale_;
        const std::size_t row_pixels = static_cast<std::size_t>(width_) * k;
        const std::size_t px0 = static_cast<std::size_t>(cx) * k;
        const std::size_t py0 = static_cast<std::size_t>(cy) * k;
        for (std::size_t py = py0; py < py0 + k; ++py) {
            for (std::size_t px = px0; px < px0 + k; ++px) {
                const std::size_t at = (py * row_pixels + px) * 3U;
                rgb_[at] = c.r;
                rgb_[at + 1U] = c.g;
                rgb_[at + 2U] = c.b;
            }
        }
    }

    std::span<std::uint8_t> rgb_;
    std::uint32_t width_;
    std::uint32_t height_;
    std::uint32_t scale_;
};

}  // namespace

void draw_markers(std::span<std::uint8_t> rgb, std::uint32_t width, std::uint32_t height, std::uint32_t scale,
                  const YearMarkers& markers, std::span<const Rgb> colours) {
    if (scale < MIN_RENDER_SCALE || scale > MAX_RENDER_SCALE) {
        draw_fatal("scale " + num(scale) + " outside 1 to 16");
    }
    // 3 (width k) (height k), in 64 bits: width and height are at most
    // 2^32 - 1 and k at most 16, so each side fits in 36 bits; the product
    // is checked against overflow before it is compared.
    const std::uint64_t w = std::uint64_t{width} * scale;
    const std::uint64_t h = std::uint64_t{height} * scale;
    const bool overflow = w != 0 && h > (std::uint64_t{0xFFFFFFFFFFFFFFFFULL} / 3U) / w;
    if (overflow || w * h * 3U != static_cast<std::uint64_t>(rgb.size())) {
        draw_fatal("buffer of " + db::format_u64(rgb.size()) + " bytes, expected 3 x " + db::format_u64(w) + " x " +
                   db::format_u64(h));
    }
    // Owner colours, checked before anything is drawn.
    for (const SettlementMarker& m : markers.settlements) {
        if (standing_status(m.status) && m.owner != 0 && !index_of(m.owner, colours.size())) {
            draw_fatal("no colour for civilization " + num(m.owner));
        }
    }
    Canvas canvas(rgb, width, height, scale);
    GlyphCache glyphs;
    // (1) passes.
    for (const PassMarker& p : markers.passes) {
        canvas.draw(glyphs.get(GlyphKey{10, false, false}), p.x, p.y, MARKER_INK);
    }
    // (2) places.
    for (const PlaceMarker& p : markers.places) {
        canvas.draw(glyphs.get(GlyphKey{11, false, false}), p.x, p.y, event_symbol_colour(p.symbol));
    }
    // (3) to (6): faded, ruins, standing non-capitals, capitals.
    const auto pass_of = [](const SettlementMarker& m) -> int {
        if (m.status == 4 || (m.status == 3 && m.old_ruin)) return 3;  // P5-06 25.1 (e): old ruins with the faded
        if (m.status == 3) return 4;
        if (standing_status(m.status)) return m.capital ? 6 : 5;
        return 0;
    };
    for (int pass = 3; pass <= 6; ++pass) {
        if (pass == 5) {  // P5-04 11.3: sites, between ruins and standing settlements
            // P5-06 11.4: and the towers, each at its offset.
            for (const SiteMarker& s : markers.sites) {
                const std::uint8_t base = site_glyph_base(s);
                if (base == 0) {
                    continue;
                }
                const std::array<std::int64_t, 2> d = site_glyph_offset(s);
                const Rgb fill = base == GLYPH_TOWER     ? MARKER_TOWER
                                 : base == GLYPH_SHRINE  ? MARKER_SHRINE
                                 : base == GLYPH_TEMPLE  ? MARKER_TEMPLE
                                 : base == GLYPH_SANCTUM ? MARKER_SANCTUM
                                                         : MARKER_MINE;
                canvas.draw(glyphs.get(GlyphKey{base, false, false}), s.x + d[0], s.y + d[1], fill);
            }
        }
        for (const SettlementMarker& m : markers.settlements) {
            if (pass_of(m) != pass) {
                continue;
            }
            Rgb fill = MARKER_FADED;
            if (standing_status(m.status)) {
                fill = m.owner == 0 ? MARKER_NO_OWNER : colours[static_cast<std::size_t>(m.owner - 1)];
            }
            canvas.draw(glyphs.get(glyph_of(m)), m.x, m.y, fill);
        }
    }
}

}  // namespace worldgen::inspect
