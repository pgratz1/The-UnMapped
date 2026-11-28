#include "viewer/browser_state.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <new>
#include <utility>

#include "viewer/history_state.hpp"
#include "worldgen/db/result.hpp"

namespace unmapped {

namespace {

std::int64_t steady_nsec() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

const std::string EMPTY;

}  // namespace

std::string browser_target(std::int64_t type, std::int64_t id) {
    if (type == 0) {
        return "e" + std::to_string(id);
    }
    return worldgen::inspect::history_entity_tag(type, id);
}

bool parse_browser_target(std::string_view target, std::string& prefix, std::int64_t& id) {
    std::size_t i = 0;
    while (i < target.size() && target[i] >= 'a' && target[i] <= 'z') {
        ++i;
    }
    if (i == 0 || i == target.size() || target.size() - i > 18) {
        return false;
    }
    std::int64_t value = 0;
    for (std::size_t k = i; k < target.size(); ++k) {
        if (target[k] < '0' || target[k] > '9') {
            return false;
        }
        value = value * 10 + (target[k] - '0');
    }
    prefix.assign(target.substr(0, i));
    id = value;
    return true;
}

void BrowserState::reset() {
    index_.reset();
    error_code_.clear();
    error_message_.clear();
    load_usec_ = 0;
    rows_.clear();
    ids_.clear();
    scratch_.clear();
    last_select_nsec_ = 0;
    highlight_ = {};
    scratch_highlight_ = {};
    back_.clear();
    forward_.clear();
    selection = {};
    filter = {};
    follow = false;
}

void BrowserState::load(const worldgen::history::LoadedHistory& history) {
    reset();
    const std::int64_t start = steady_nsec();
    try {
        auto built = worldgen::inspect::BrowserIndex::build(history);
        if (!built.ok()) {
            error_code_ = worldgen::db::error_code_name(built.error().code);
            error_message_ = built.error().message;
            return;
        }
        index_.emplace(std::move(built).value());
        rows_.reserve(index_->events().size());
        for (const auto& e : index_->events()) {
            rows_.push_back(index_->row_text(history, e.id));
        }
    } catch (const std::bad_alloc&) {
        reset();
        error_code_ = "out_of_memory";
        error_message_ = "out of memory while building the browser";
        return;
    } catch (const std::exception& e) {
        // A FatalError: the index and the history disagree (a library bug).
        reset();
        error_code_ = "corrupt_layer";
        error_message_ = e.what();
        return;
    }
    load_usec_ = (steady_nsec() - start) / 1000;
}

const worldgen::inspect::EventEntry* BrowserState::event(std::int64_t id) const noexcept {
    if (!index_) {
        return nullptr;
    }
    const auto events = index_->events();
    const auto it = std::lower_bound(events.begin(), events.end(), id,
                                     [](const worldgen::inspect::EventEntry& e, std::int64_t key) { return e.id < key; });
    return it != events.end() && it->id == id ? &*it : nullptr;
}

const worldgen::inspect::CivilizationEntry* BrowserState::civilization(std::int64_t id) const noexcept {
    if (!index_) {
        return nullptr;
    }
    const auto civs = index_->civilizations();
    const auto it = std::lower_bound(civs.begin(), civs.end(), id,
                                     [](const worldgen::inspect::CivilizationEntry& c, std::int64_t key) {
                                         return c.id < key;
                                     });
    return it != civs.end() && it->id == id ? &*it : nullptr;
}

const worldgen::inspect::FigureEntry* BrowserState::figure(std::int64_t id) const noexcept {
    if (!index_) {
        return nullptr;
    }
    const auto figures = index_->figures();
    const auto it = std::lower_bound(figures.begin(), figures.end(), id,
                                     [](const worldgen::inspect::FigureEntry& f, std::int64_t key) { return f.id < key; });
    return it != figures.end() && it->id == id ? &*it : nullptr;
}

const std::string& BrowserState::row(std::int64_t id) const noexcept {
    const worldgen::inspect::EventEntry* e = event(id);
    if (e == nullptr) {
        return EMPTY;
    }
    const auto i = static_cast<std::size_t>(e - index_->events().data());
    return i < rows_.size() ? rows_[i] : EMPTY;
}

bool BrowserState::event_filter(std::int64_t year, worldgen::inspect::EventFilter& out) const {
    out = {};
    switch (filter.scope) {
        case BrowserScope::all:
            break;
        case BrowserScope::entity:
            // Civilizations and (P5-06 11.3) figures; monsters and sites get
            // their own fields later (section 9).
            if (selection.entity_id == 0) {
                return false;
            }
            if (selection.entity_type == BROWSER_CIVILIZATION) {
                out.civilization = selection.entity_id;
            } else if (selection.entity_type == BROWSER_FIGURE) {
                out.figure = selection.entity_id;
            } else if (selection.entity_type != BROWSER_GOD && selection.entity_type != BROWSER_CULT) {
                return false;
            }
            break;
        case BrowserScope::place:
            if (selection.place_type == BROWSER_SETTLEMENT && selection.place_id != 0) {
                out.settlement = selection.place_id;
            } else if (selection.place_type == BROWSER_REGION && selection.place_id != 0) {
                out.region = selection.place_id;
            } else {
                return false;
            }
            break;
    }
    if (filter.this_year) {
        out.from_year = year;
        out.to_year = year;
    } else {
        out.from_year = filter.from;
        out.to_year = filter.to;
    }
    out.groups = filter.group < 0 ? worldgen::inspect::ALL_GROUPS : (std::uint32_t{1} << filter.group);
    out.summary_only = filter.level == worldgen::inspect::HistoryLevel::summary;
    return true;
}

bool BrowserState::refresh_list(const worldgen::history::LoadedHistory& history, std::int64_t year) {
    if (!index_) {
        const bool changed = !ids_.empty();
        ids_.clear();
        return changed;
    }
    const std::int64_t start = steady_nsec();
    worldgen::inspect::EventFilter f;
    if (event_filter(year, f)) {
        index_->select(history, f, scratch_);
        if (filter.scope == BrowserScope::entity &&
            (selection.entity_type == BROWSER_GOD || selection.entity_type == BROWSER_CULT)) {
            // P5-08: worldgen's EventFilter has no god or faction field; keep
            // the events that name the selected god or cult as a participant.
            const std::int64_t type = selection.entity_type;
            const std::int64_t id = selection.entity_id;
            std::erase_if(scratch_, [&history, type, id](std::int64_t e) {
                const auto* event = history.view.event(e);
                if (event == nullptr) {
                    return true;
                }
                return std::none_of(event->participants.begin(), event->participants.end(), [type, id](const auto& p) {
                    return p.entity_type == type && p.entity_id == id;
                });
            });
        }
    } else {
        scratch_.clear();
    }
    last_select_nsec_ = steady_nsec() - start;
    if (scratch_ == ids_) {
        return false;
    }
    std::swap(scratch_, ids_);
    return true;
}

std::optional<std::int64_t> BrowserState::follow_target(std::int64_t year, int direction) const {
    if (ids_.empty() || (direction != 1 && direction != -1)) {
        return std::nullopt;
    }
    // From a selected event in the list: its neighbour in the list.
    if (selection.event != 0) {
        const auto it = std::lower_bound(ids_.begin(), ids_.end(), selection.event);
        if (it != ids_.end() && *it == selection.event) {
            if (direction > 0) {
                return it + 1 != ids_.end() ? std::optional<std::int64_t>(*(it + 1)) : std::nullopt;
            }
            return it != ids_.begin() ? std::optional<std::int64_t>(*(it - 1)) : std::nullopt;
        }
    }
    // Otherwise: +1 the first id with year > Y, -1 the last id with year < Y.
    if (direction > 0) {
        for (const std::int64_t id : ids_) {
            const auto* e = event(id);
            if (e != nullptr && e->year > year) {
                return id;
            }
        }
        return std::nullopt;
    }
    for (auto it = ids_.rbegin(); it != ids_.rend(); ++it) {
        const auto* e = event(*it);
        if (e != nullptr && e->year < year) {
            return *it;
        }
    }
    return std::nullopt;
}

std::size_t BrowserState::first_row_at_or_after(std::int64_t year) const {
    const auto it = std::partition_point(ids_.begin(), ids_.end(), [this, year](std::int64_t id) {
        const auto* e = event(id);
        return e != nullptr && e->year < year;
    });
    return static_cast<std::size_t>(it - ids_.begin());
}

bool BrowserState::refresh_highlight(const HistoryState& history, bool& regions_changed) {
    BrowserHighlight& h = scratch_highlight_;
    h.civilization = 0;
    h.regions.clear();
    h.settlements.clear();
    h.dim = false;
    h.place = {-1, -1};
    h.place_event = 0;
    h.place_settlement = 0;
    if (index_ && history.loaded()) {
        const std::int64_t year = history.year();
        if (selection.entity_type == BROWSER_CIVILIZATION && selection.entity_id != 0) {
            const std::int64_t civ = selection.entity_id;
            h.civilization = civ;
            for (const auto& holding : history.state().regions) {
                if (holding.civ == civ) {
                    h.regions.push_back(holding.region);
                }
            }
            h.dim = true;
            for (const auto& s : history.markers().settlements) {
                if ((s.status == 1 || s.status == 2) && s.owner == civ) {
                    h.settlements.push_back(s.settlement);
                }
            }
            std::sort(h.settlements.begin(), h.settlements.end());
        }
        if (selection.place_type == BROWSER_REGION && selection.place_id != 0) {
            // 5.4: a region's land is drawn as a held set of one region.
            h.regions.assign(1, selection.place_id);
            h.dim = true;
        } else if (selection.place_type == BROWSER_SETTLEMENT && selection.place_id != 0) {
            h.place_settlement = selection.place_id;
        }
        if (selection.event != 0) {
            const auto* e = event(selection.event);
            const worldgen::inspect::MarkerIndex* markers = history.marker_index();
            if (e != nullptr && markers != nullptr && e->year <= year && year <= e->end_year) {
                if (const auto cell = markers->event_place(history.history(), e->id)) {
                    h.place = *cell;
                    h.place_event = e->id;
                }
            }
        }
    }
    regions_changed = h.dim != highlight_.dim || h.regions != highlight_.regions;
    if (h == highlight_) {
        return false;
    }
    std::swap(scratch_highlight_, highlight_);
    return true;
}

void BrowserState::push_path(std::int64_t year) {
    back_.push_back(BrowserPathEntry{selection, filter.scope, year});
    if (back_.size() > PATH_LIMIT) {
        back_.erase(back_.begin());
    }
    forward_.clear();
}

std::optional<BrowserPathEntry> BrowserState::back(std::int64_t year) {
    if (back_.empty()) {
        return std::nullopt;
    }
    BrowserPathEntry entry = back_.back();
    back_.pop_back();
    forward_.push_back(BrowserPathEntry{selection, filter.scope, year});
    if (forward_.size() > PATH_LIMIT) {
        forward_.erase(forward_.begin());
    }
    return entry;
}

std::optional<BrowserPathEntry> BrowserState::forward(std::int64_t year) {
    if (forward_.empty()) {
        return std::nullopt;
    }
    BrowserPathEntry entry = forward_.back();
    forward_.pop_back();
    back_.push_back(BrowserPathEntry{selection, filter.scope, year});
    if (back_.size() > PATH_LIMIT) {
        back_.erase(back_.begin());
    }
    return entry;
}

}  // namespace unmapped
