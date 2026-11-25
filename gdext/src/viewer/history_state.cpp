#include "viewer/history_state.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "worldgen/db/result.hpp"
#include "worldgen/history/digest.hpp"

namespace unmapped {

namespace {

// A row of a table held in ascending id order, by binary search; null when
// absent.
template <class Row>
const Row* find_row(const std::vector<Row>& rows, std::int64_t id) noexcept {
    const auto it = std::lower_bound(rows.begin(), rows.end(), id,
                                     [](const Row& row, std::int64_t key) { return row.id < key; });
    return it != rows.end() && it->id == id ? &*it : nullptr;
}

std::uint32_t rgb_word(worldgen::inspect::Rgb c) noexcept {
    return (static_cast<std::uint32_t>(c.r) << 16U) | (static_cast<std::uint32_t>(c.g) << 8U) |
           static_cast<std::uint32_t>(c.b);
}

std::string phase_text(const worldgen::db::PhaseRow& phase) {
    return "phase p" + std::to_string(phase.id) + " " +
           std::string(worldgen::history::phase_kind_word(phase.kind)) + ", " + std::to_string(phase.start_year) +
           "-" + std::to_string(phase.end_year);
}

constexpr std::uint8_t GLYPH_RUIN = 8;
constexpr std::uint8_t GLYPH_FADED = 9;
constexpr std::uint8_t GLYPH_PASS = 10;
constexpr std::uint8_t GLYPH_RING = 11;

}  // namespace

std::string marker_glyph_name(worldgen::inspect::GlyphKey key) {
    switch (key.base) {
        case GLYPH_RUIN: return key.declining || key.capital ? std::string() : std::string("ruin");
        case GLYPH_FADED: return key.declining || key.capital ? std::string() : std::string("faded");
        case GLYPH_PASS: return key.declining || key.capital ? std::string() : std::string("pass");
        case GLYPH_RING: return key.declining || key.capital ? std::string() : std::string("ring");
        case worldgen::inspect::GLYPH_MINE:  // P5-04 11.3
            return key.declining || key.capital ? std::string() : std::string("mine");
        default: break;
    }
    const std::string_view word = worldgen::history::settlement_kind_word(key.base);
    if (word.empty()) {
        return {};
    }
    std::string name(word);
    if (key.declining) {
        name += " declining";
    }
    if (key.capital) {
        name += " capital";
    }
    return name;
}

std::optional<worldgen::inspect::GlyphKey> marker_glyph_key(std::string_view name) {
    for (const std::uint8_t base : {GLYPH_RUIN, GLYPH_FADED, GLYPH_PASS, GLYPH_RING, worldgen::inspect::GLYPH_MINE}) {
        if (name == marker_glyph_name(worldgen::inspect::GlyphKey{base, false, false})) {
            return worldgen::inspect::GlyphKey{base, false, false};
        }
    }
    for (std::uint8_t kind = 1; kind <= 5; ++kind) {
        for (const bool declining : {false, true}) {
            for (const bool capital : {false, true}) {
                const worldgen::inspect::GlyphKey key{kind, declining, capital};
                if (name == marker_glyph_name(key)) {
                    return key;
                }
            }
        }
    }
    return std::nullopt;
}

void HistoryState::reset() {
    status_ = HistoryStatus::none;
    error_code_.clear();
    error_message_.clear();
    history_.reset();
    colours_.clear();
    base_.reset();
    year_ = 0;
    state_ = {};
    holders_.clear();
    drawn_holders_.clear();
    drawn_valid_ = false;
    markers_.reset();
    markers_error_code_.clear();
    markers_error_message_.clear();
    markers_enabled_ = true;
    marker_options_ = {};
    markers_current_ = {};
    markers_scratch_ = {};
    last_marker_step_nsec_ = 0;
}

void HistoryState::load(const worldgen::db::WorldReader& reader) {
    reset();
    try {
        auto loaded = worldgen::history::load_history(reader);
        if (!loaded.ok()) {
            if (loaded.error().code != worldgen::db::ErrorCode::history_missing) {
                status_ = HistoryStatus::failed;
                error_code_ = worldgen::db::error_code_name(loaded.error().code);
                error_message_ = loaded.error().message;
            }
            return;
        }
        history_.emplace(std::move(loaded).value());
    } catch (const std::bad_alloc&) {
        reset();
        status_ = HistoryStatus::failed;
        error_code_ = "out_of_memory";
        error_message_ = "out of memory while reading the history";
        return;
    }
    try {
        colours_ = worldgen::inspect::civilization_colours(history_->tables);
        status_ = HistoryStatus::loaded;
        const std::int64_t end = history_->timeline.end_year();
        year_ = end + 1;  // so that set_year(E) computes the state
        if (!set_year(end)) {
            throw std::runtime_error("the end year is not a year of the timeline");
        }
    } catch (const std::bad_alloc&) {
        reset();
        status_ = HistoryStatus::failed;
        error_code_ = "out_of_memory";
        error_message_ = "out of memory while reading the history";
        return;
    } catch (const std::exception& e) {
        // A FatalError of civilization_colours (P4-03 3.1 step 4).
        reset();
        status_ = HistoryStatus::failed;
        error_code_ = "corrupt_layer";
        error_message_ = e.what();
        return;
    }
    // P4-04 8.1: the marker index; its failure leaves the history loaded.
    try {
        auto index = worldgen::inspect::MarkerIndex::load(reader, *history_);
        if (index.ok()) {
            markers_.emplace(std::move(index).value());
        } else {
            markers_error_code_ = worldgen::db::error_code_name(index.error().code);
            markers_error_message_ = index.error().message;
        }
    } catch (const std::bad_alloc&) {
        markers_.reset();
        markers_error_code_ = "out_of_memory";
        markers_error_message_ = "out of memory while building the markers";
    } catch (const std::exception& e) {
        // A FatalError: the history and the index disagree (a library bug).
        markers_.reset();
        markers_error_code_ = "corrupt_layer";
        markers_error_message_ = e.what();
    }
    try {
        update_markers();
    } catch (const std::exception& e) {
        markers_.reset();
        markers_current_ = {};
        markers_error_code_ = "corrupt_layer";
        markers_error_message_ = e.what();
    }
}

namespace {

// The marker sets of two years are the same when their lists are (the
// year itself is no difference, P4-04 8.1).
bool same_markers(const worldgen::inspect::YearMarkers& a, const worldgen::inspect::YearMarkers& b) {
    return a.sites == b.sites && a.settlements == b.settlements && a.places == b.places && a.passes == b.passes;
}

}  // namespace

bool HistoryState::update_markers() {
    const auto start = std::chrono::steady_clock::now();
    if (markers_available() && markers_enabled_) {
        markers_->markers_at(*history_, state_, marker_options_, markers_scratch_);
    } else {
        markers_scratch_.year = loaded() ? year_ : 0;
        markers_scratch_.sites.clear();
        markers_scratch_.settlements.clear();
        markers_scratch_.places.clear();
        markers_scratch_.passes.clear();
    }
    const bool changed = !same_markers(markers_scratch_, markers_current_);
    if (changed) {
        std::swap(markers_scratch_, markers_current_);
    } else {
        markers_current_.year = markers_scratch_.year;
    }
    last_marker_step_nsec_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();
    return changed;
}

std::int64_t HistoryState::end_year() const noexcept {
    return loaded() ? history_->timeline.end_year() : -1;
}

bool HistoryState::valid_year(std::int64_t y) const noexcept {
    return loaded() && history_->timeline.valid_year(y);
}

bool HistoryState::set_year(std::int64_t y) {
    if (!valid_year(y)) {
        return false;
    }
    if (y == year_) {
        return true;
    }
    year_ = y;
    const auto& timeline = history_->timeline;
    timeline.year_state(year_, state_);
    holders_ = worldgen::inspect::territory_holders(timeline, year_);
    return true;
}

const worldgen::db::CivilizationRow* HistoryState::civilization(std::int64_t id) const noexcept {
    return loaded() ? find_row(history_->tables.civilizations, id) : nullptr;
}

const worldgen::db::RaceRow* HistoryState::race(std::int64_t id) const noexcept {
    return loaded() ? find_row(history_->tables.races, id) : nullptr;
}

const worldgen::db::AgeRow* HistoryState::age(std::int64_t id) const noexcept {
    return loaded() ? find_row(history_->tables.ages, id) : nullptr;
}

const worldgen::db::PhaseRow* HistoryState::phase(std::int64_t id) const noexcept {
    return loaded() ? find_row(history_->tables.phases, id) : nullptr;
}

std::int64_t HistoryState::colour_of(std::int64_t civilization) const noexcept {
    if (!loaded() || civilization < 1 || static_cast<std::uint64_t>(civilization) > colours_.size()) {
        return -1;
    }
    return rgb_word(colours_[static_cast<std::size_t>(civilization - 1)]);
}

std::optional<worldgen::history::RegionHolding> HistoryState::holding(std::int64_t region) const {
    if (!loaded()) {
        return std::nullopt;
    }
    const auto& regions = state_.regions;
    const auto it = std::lower_bound(
        regions.begin(), regions.end(), region,
        [](const worldgen::history::RegionHolding& h, std::int64_t key) { return h.region < key; });
    if (it == regions.end() || it->region != region) {
        return std::nullopt;
    }
    return *it;
}

std::optional<std::string> HistoryState::region_name(std::int64_t region) const {
    if (region <= 0 || !holding(region)) {  // holding: the timeline has the region
        return std::nullopt;
    }
    return std::string(history_->timeline.region_name(region, year_));
}

std::string HistoryState::holding_text(std::int64_t region) const {
    if (region <= 0) {
        return {};
    }
    const auto h = holding(region);
    if (!h) {
        return {};
    }
    if (h->civ != 0) {
        const worldgen::db::CivilizationRow* civ = civilization(h->civ);
        return "   |   held by " + (civ != nullptr ? civ->name : "civilization " + std::to_string(h->civ)) +
               " since " + std::to_string(h->since);
    }
    return h->since > 0 ? "   |   free since " + std::to_string(h->since) : std::string("   |   free");
}

std::int64_t HistoryState::regions_held() const noexcept {
    std::int64_t n = 0;
    for (const auto& h : state_.regions) {
        n += h.civ != 0 ? 1 : 0;
    }
    return n;
}

std::int64_t HistoryState::settlements_standing() const noexcept {
    std::int64_t n = 0;
    for (const auto& s : state_.settlements) {
        n += s.exists && (s.status == 1 || s.status == 2) ? 1 : 0;
    }
    return n;
}

std::int64_t HistoryState::wars() const noexcept {
    if (!loaded()) {
        return 0;
    }
    const auto relations = history_->timeline.relations();
    std::int64_t n = 0;
    for (const std::size_t i : state_.relations) {
        n += i < relations.size() && relations[i].kind == "war" ? 1 : 0;
    }
    return n;
}

std::optional<std::int64_t> HistoryState::previous_event_year() const {
    return loaded() ? history_->timeline.previous_event_year(year_) : std::nullopt;
}

std::optional<std::int64_t> HistoryState::next_event_year() const {
    return loaded() ? history_->timeline.next_event_year(year_) : std::nullopt;
}

std::optional<std::int64_t> HistoryState::previous_change_year() const {
    if (!loaded()) {
        return std::nullopt;
    }
    const auto years = history_->timeline.holder_change_years();
    const auto it = std::lower_bound(years.begin(), years.end(), year_);  // first >= Y
    if (it == years.begin()) {
        return std::nullopt;
    }
    return *(it - 1);
}

std::optional<std::int64_t> HistoryState::next_change_year() const {
    if (!loaded()) {
        return std::nullopt;
    }
    const auto years = history_->timeline.holder_change_years();
    const auto it = std::upper_bound(years.begin(), years.end(), year_);  // first > Y
    if (it == years.end()) {
        return std::nullopt;
    }
    return *it;
}

std::vector<YearMark> HistoryState::year_marks() const {
    std::vector<YearMark> marks;
    if (!loaded()) {
        return marks;
    }
    // Rank by kind for the ties of 5.4; each list is already in ascending
    // id order, which a stable sort keeps within a year and kind.
    std::vector<std::pair<int, YearMark>> ranked;
    for (const auto& race : history_->tables.races) {
        if (race.present != 0 && race.awakening_year) {
            ranked.push_back({0, YearMark{*race.awakening_year, "awakening",
                                          "awakening of the " + race.plural + ", " +
                                              std::to_string(*race.awakening_year)}});
        }
    }
    for (const auto& phase : history_->tables.phases) {
        ranked.push_back({1, YearMark{phase.start_year, "phase", phase_text(phase)}});
    }
    const std::int64_t end = end_year();
    ranked.push_back({2, YearMark{end, "present", "the present, " + std::to_string(end)}});
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        return a.second.year != b.second.year ? a.second.year < b.second.year : a.first < b.first;
    });
    marks.reserve(ranked.size());
    for (auto& entry : ranked) {
        marks.push_back(std::move(entry.second));
    }
    return marks;
}

std::vector<worldgen::inspect::LegendEntry> HistoryState::legend(
    std::span<const worldgen::inspect::BiomeRecord> biomes) const {
    using worldgen::inspect::LegendEntry;
    std::vector<LegendEntry> out;
    if (!loaded()) {
        return out;
    }
    // 1. Races in ascending id with a civilization alive at Y, each with its
    // civilizations alive at Y in ascending id.
    for (const auto& race : history_->tables.races) {
        bool heading = false;
        for (const std::int64_t id : state_.civilizations) {
            const worldgen::db::CivilizationRow* civ = civilization(id);
            if (civ == nullptr || civ->race != race.id) {
                continue;
            }
            if (!heading) {
                out.push_back(LegendEntry{race.plural, {}});
                heading = true;
            }
            const std::int64_t colour = colour_of(id);
            out.push_back(LegendEntry{civ->name, {static_cast<std::uint32_t>(colour < 0 ? 0 : colour)}});
        }
    }
    // 2 to 4. The three borders (P4-03 r4 6.4).
    out.push_back(LegendEntry{"border between holders", {0x000000U}});
    out.push_back(LegendEntry{"border inside a realm", {}});
    out.push_back(LegendEntry{"border in free land", {}});
    // 5. Free land: the regions composite's rows after its border row and
    // before its last three rows (the land biomes). 6. Water: its last three
    // rows (ocean, lake, river) under their own heading.
    std::vector<LegendEntry> rows = worldgen::inspect::composite_legend(worldgen::inspect::Composite::regions, biomes);
    std::size_t first = 0;
    while (first < rows.size() && rows[first].label != "region border") {
        ++first;
    }
    first = first < rows.size() ? first + 1U : 0U;
    constexpr std::size_t WATER_ROWS = 3;
    const std::size_t water = rows.size() >= first + WATER_ROWS ? rows.size() - WATER_ROWS : rows.size();
    out.push_back(LegendEntry{"free land", {}});
    for (std::size_t i = first; i < water; ++i) {
        out.push_back(std::move(rows[i]));
    }
    out.push_back(LegendEntry{"water", {}});
    for (std::size_t i = water; i < rows.size(); ++i) {
        out.push_back(std::move(rows[i]));
    }
    return out;
}

std::string_view HistoryState::legend_tooltip(std::string_view label) noexcept {
    // P4-03 r4 6.4: the longer explanation of the three border rows.
    if (label == "border between holders") {
        return "black, between regions of different holders";
    }
    if (label == "border inside a realm") {
        return "the realm's colour, darker";
    }
    if (label == "border in free land") {
        return "the land's colour, darker";
    }
    return {};
}

std::optional<std::string> HistoryState::build_base(const TerritoryLayers& layers,
                                                    std::span<const worldgen::inspect::BiomeColour> biome_table) {
    if (base_) {
        return std::nullopt;
    }
    if (layers.elevation == nullptr) {
        return std::string("the world has no layer 'elevation'");
    }
    if (layers.biome == nullptr) {
        return std::string("the world has no layer 'biome'");
    }
    if (layers.region == nullptr) {
        return std::string("the world has no layer 'region'");
    }
    if (auto problem = worldgen::inspect::composite_regions_problem(*layers.elevation, layers.water, layers.river,
                                                                    layers.flow_dir, *layers.biome, *layers.region)) {
        return problem;
    }
    base_.emplace(worldgen::inspect::TerritoryBase::build(*layers.elevation, layers.water, layers.river,
                                                          layers.flow_dir, *layers.biome, *layers.region,
                                                          biome_table));
    drawn_valid_ = false;
    return std::nullopt;
}

bool HistoryState::needs_draw() const noexcept {
    return !drawn_valid_ || drawn_holders_ != holders_;
}

void HistoryState::draw(std::span<std::uint8_t> out) {
    if (!base_ || !loaded()) {
        return;
    }
    worldgen::inspect::render_territory(*base_, history_->timeline, colours_, year_, 1U, out);
    drawn_holders_ = holders_;
    drawn_valid_ = true;
}

}  // namespace unmapped
