#include "viewer/history_state.hpp"

#include <algorithm>
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

}  // namespace

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
    fill_.clear();
    drawn_holders_.clear();
    drawn_valid_ = false;
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
    } catch (const std::exception& e) {
        // A FatalError of civilization_colours (P4-03 3.1 step 4).
        reset();
        status_ = HistoryStatus::failed;
        error_code_ = "corrupt_layer";
        error_message_ = e.what();
    }
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
    fill_ = worldgen::inspect::territory_fill(timeline, colours_, year_);
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
    // 2, 3. The two borders.
    out.push_back(LegendEntry{"border between holders", {0x000000U}});
    out.push_back(LegendEntry{"border within a realm", {}});
    // 4. Free land: the regions composite's rows without its border row.
    out.push_back(LegendEntry{"free land", {}});
    for (auto& row : worldgen::inspect::composite_legend(worldgen::inspect::Composite::regions, biomes)) {
        if (row.label != "region border") {
            out.push_back(std::move(row));
        }
    }
    return out;
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
    worldgen::inspect::render_territory(*base_, fill_, holders_, 1U, out);
    drawn_holders_ = holders_;
    drawn_valid_ = true;
}

}  // namespace unmapped
