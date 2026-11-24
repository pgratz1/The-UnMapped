// ViewerModel: the browser of civilizations and events (spec P4-05 5.2 to
// 5.4). Every text, list, link target and place comes from worldgen
// (BrowserIndex, MarkerIndex, the timeline); this file converts them into
// Godot values and keeps the selection, the filter, follow and the path.

#include <algorithm>
#include <chrono>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "viewer/viewer_model.hpp"
#include "worldgen/inspect/layer_catalogue.hpp"

namespace unmapped {

namespace {

godot::String to_godot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

std::string to_utf8(const godot::String& text) {
    const godot::CharString utf8 = text.utf8();
    return {utf8.get_data(), static_cast<std::size_t>(utf8.length())};
}

int64_t now_nsec() {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// The number of characters (code points) of UTF-8 text in bytes [from, to).
int64_t code_points(const std::string& text, std::size_t from, std::size_t to) {
    int64_t n = 0;
    to = std::min(to, text.size());
    for (std::size_t i = from; i < to; ++i) {
        n += (static_cast<unsigned char>(text[i]) & 0xC0U) != 0x80U ? 1 : 0;
    }
    return n;
}

godot::PackedInt64Array packed(const std::vector<std::int64_t>& values) {
    godot::PackedInt64Array out;
    out.resize(static_cast<int64_t>(values.size()));
    std::copy(values.begin(), values.end(), out.ptrw());
    return out;
}

const char* scope_word(BrowserScope scope) {
    switch (scope) {
        case BrowserScope::entity: return "entity";
        case BrowserScope::place: return "place";
        case BrowserScope::all: break;
    }
    return "all";
}

const char* match_kind_word(std::int64_t type) {
    switch (type) {
        case BROWSER_CIVILIZATION: return "civilization";
        case BROWSER_SETTLEMENT: return "settlement";
        case BROWSER_REGION: return "region";
        default: break;
    }
    return "";
}

}  // namespace

godot::Dictionary ViewerModel::linked_dictionary(const worldgen::history::LinkedText& text) const {
    // 5.2: offsets in characters, so that a RichTextLabel can use them.
    godot::Dictionary out;
    out["text"] = to_godot(text.text);
    godot::Array links;
    std::size_t byte = 0;
    int64_t chars = 0;
    for (const auto& link : text.links) {
        chars += code_points(text.text, byte, link.start);
        byte = link.start;
        const int64_t length = code_points(text.text, link.start, std::size_t{link.start} + link.length);
        godot::Dictionary l;
        l["start"] = chars;
        l["length"] = length;
        l["target"] = to_godot(browser_target(link.type, link.id));
        l["tagged"] = link.tagged;
        links.push_back(l);
    }
    out["links"] = links;
    return out;
}

void ViewerModel::rebuild_mask() {
    const int64_t start = now_nsec();
    const BrowserHighlight& h = browser_.highlight();
    mask_.clear();
    mask_width_ = 0;
    mask_height_ = 0;
    const worldgen::db::LayerGrid* grid = h.dim ? layer_grid("region") : nullptr;
    if (grid != nullptr) {
        // keep[v] = 1 for the regions not dimmed.
        std::int64_t max_region = 0;
        for (const std::int64_t r : h.regions) {
            max_region = std::max(max_region, r);
        }
        std::vector<std::uint8_t> keep(static_cast<std::size_t>(max_region) + 1U, 0);
        for (const std::int64_t r : h.regions) {
            if (r >= 0) {
                keep[static_cast<std::size_t>(r)] = 1;
            }
        }
        mask_width_ = grid->width;
        mask_height_ = grid->height;
        mask_.resize(static_cast<std::size_t>(grid->cell_count()));
        std::visit(
            [&](const auto& cells) {
                for (std::size_t i = 0; i < cells.size() && i < mask_.size(); ++i) {
                    const auto v = cells[i];
                    bool kept = false;
                    if constexpr (std::is_signed_v<std::decay_t<decltype(v)>>) {
                        kept = v >= 0 && static_cast<std::uint64_t>(v) < keep.size() &&
                               keep[static_cast<std::size_t>(v)] != 0;
                    } else {
                        kept = static_cast<std::uint64_t>(v) < keep.size() && keep[static_cast<std::size_t>(v)] != 0;
                    }
                    mask_[i] = kept ? 0 : 1;
                }
            },
            grid->values);
    }
    ++mask_version_;
    const int64_t took = now_nsec() - start;
    ++browser_timings_.mask_rebuilds;
    browser_timings_.last_mask_nsec = took;
    browser_timings_.slowest_mask_nsec = std::max(browser_timings_.slowest_mask_nsec, took);
}

void ViewerModel::refresh_browser(bool list) {
    if (!browser_.available()) {
        return;
    }
    const int64_t start = now_nsec();
    bool list_changed = false;
    bool highlight_changed = false;
    try {
        list_changed = list && browser_.refresh_list(history_.history(), history_.year());
        bool regions_changed = false;
        highlight_changed = browser_.refresh_highlight(history_, regions_changed);
        if (highlight_changed && regions_changed) {
            rebuild_mask();
        }
    } catch (const std::exception& e) {
        godot::UtilityFunctions::push_error(godot::String("ViewerModel: browser: ") + e.what());
        return;
    }
    const int64_t computed = now_nsec();
    year_steps_.browser = computed - start;
    browser_timings_.last_year_nsec = year_steps_.browser;
    if (in_year_change_) {
        browser_timings_.slowest_year_nsec = std::max(browser_timings_.slowest_year_nsec, year_steps_.browser);
    }
    if (list_changed) {
        ++browser_timings_.list_changes;
        emit_signal("event_list_changed");
    }
    if (highlight_changed) {
        ++browser_timings_.highlight_changes;
        emit_signal("highlight_changed");
    }
    year_steps_.browser_signal = now_nsec() - computed;
}

void ViewerModel::selection_changed(bool list) {
    refresh_browser(list);
    // P4-06 5.4: a selection or filter change recomputes the stops of an
    // open session; the span is kept.
    if (playback_.active()) {
        refresh_playback_source();
    }
    if (browser_.available()) {
        emit_signal("browser_selection_changed");
    }
}

void ViewerModel::restore_path(const BrowserPathEntry& entry) {
    browser_.selection = entry.selection;
    browser_.filter.scope = entry.scope;
    set_year(entry.year);
    selection_changed(true);
}

bool ViewerModel::get_browser_available() const {
    return history_.markers_available() && browser_.available();
}

godot::String ViewerModel::get_browser_error_code() const {
    return to_godot(browser_.error_code());
}

godot::String ViewerModel::get_browser_error_message() const {
    return to_godot(browser_.error_message());
}

godot::PackedStringArray ViewerModel::get_event_groups() const {
    godot::PackedStringArray out;
    if (!get_browser_available()) {
        return out;
    }
    for (std::uint32_t code = 0; code < 32U; ++code) {
        if ((worldgen::inspect::ALL_GROUPS & (std::uint32_t{1} << code)) != 0U) {
            out.push_back(to_godot(worldgen::inspect::event_group_word(static_cast<worldgen::inspect::EventGroup>(code))));
        }
    }
    return out;
}

godot::Array ViewerModel::get_civilization_list() const {
    godot::Array out;
    if (!get_browser_available()) {
        return out;
    }
    for (const auto& c : browser_.index().civilizations()) {
        godot::Dictionary row;
        row["id"] = c.id;
        row["name"] = to_godot(c.name);
        row["race"] = c.race;
        row["race_name"] = to_godot(c.race_name);
        row["colour"] = history_.colour_of(c.id);
        row["founded_year"] = c.founded_year;
        row["ended_year"] = c.ended_year;
        row["fate"] = c.ended_year >= 0 ? "ended" : "alive";
        row["events"] = c.events;
        out.push_back(row);
    }
    return out;
}

godot::Array ViewerModel::get_civilization_block(int64_t civilization) const {
    godot::Array out;
    if (!get_browser_available() || browser_.civilization(civilization) == nullptr) {
        return out;
    }
    try {
        for (const auto& line : browser_.index().block(history_.history(), civilization, browser_.filter.level)) {
            out.push_back(linked_dictionary(line));
        }
    } catch (const std::exception& e) {
        godot::UtilityFunctions::push_error(godot::String("ViewerModel.get_civilization_block: ") + e.what());
        return {};
    }
    return out;
}

godot::PackedInt64Array ViewerModel::get_event_ids() const {
    return get_browser_available() ? packed(browser_.ids()) : godot::PackedInt64Array();
}

godot::PackedStringArray ViewerModel::get_event_rows() const {
    godot::PackedStringArray out;
    if (!get_browser_available()) {
        return out;
    }
    out.resize(static_cast<int64_t>(browser_.ids().size()));
    int64_t i = 0;
    for (const std::int64_t id : browser_.ids()) {
        out.set(i++, to_godot(browser_.row(id)));
    }
    return out;
}

godot::Dictionary ViewerModel::get_event_detail(int64_t event) const {
    godot::Dictionary out;
    if (!get_browser_available() || browser_.event(event) == nullptr) {
        return out;
    }
    try {
        const worldgen::history::LoadedHistory& history = history_.history();
        const worldgen::inspect::EventDetail d = browser_.index().event_detail(history, event);
        out["id"] = d.entry.id;
        out["year"] = d.entry.year;
        out["end_year"] = d.entry.end_year;
        out["row"] = to_godot(browser_.row(event));
        out["kind_key"] = to_godot(d.kind_key);
        out["group"] = to_godot(worldgen::inspect::event_group_word(d.entry.group));
        out["salience"] = d.entry.salience;
        out["summary"] = d.entry.summary;
        out["sentence"] = linked_dictionary(d.sentence);
        out["note"] = to_godot(d.note);
        godot::Array participants;
        for (const auto& p : d.participants) {
            godot::Dictionary row;
            row["role"] = to_godot(p.role);
            row["target"] = to_godot(browser_target(p.type, p.id));
            row["name"] = to_godot(p.name);
            participants.push_back(row);
        }
        out["participants"] = participants;
        godot::Array values;
        for (const auto& [key, value] : d.values) {
            godot::Dictionary row;
            row["key"] = to_godot(key);
            row["value"] = value;
            values.push_back(row);
        }
        out["values"] = values;
        const auto events = [this](const std::vector<std::int64_t>& ids) {
            godot::Array rows;
            for (const std::int64_t id : ids) {
                godot::Dictionary row;
                row["target"] = to_godot(browser_target(0, id));
                row["row"] = to_godot(browser_.row(id));
                rows.push_back(row);
            }
            return rows;
        };
        out["causes"] = events(d.causes);
        out["effects"] = events(d.effects);
        const worldgen::inspect::MarkerIndex* markers = history_.marker_index();
        const auto place = markers != nullptr ? markers->event_place(history, event) : std::nullopt;
        out["place"] = place ? godot::Vector2i(static_cast<int32_t>((*place)[0]), static_cast<int32_t>((*place)[1]))
                             : godot::Vector2i(-1, -1);
        out["settlement"] = d.entry.settlement;
        out["region"] = d.entry.region;
        // The heading's links (3): the event's settlement and region, by the
        // names the sentence uses (the region's at the event's year).
        const std::string* settlement =
            d.entry.settlement != 0 ? history.view.name(worldgen::history::EntityType::settlement, d.entry.settlement)
                                    : nullptr;
        out["settlement_target"] = d.entry.settlement != 0 ? to_godot(browser_target(BROWSER_SETTLEMENT, d.entry.settlement))
                                                           : godot::String();
        out["settlement_name"] = settlement != nullptr ? to_godot(*settlement) : godot::String();
        out["region_target"] =
            d.entry.region != 0 ? to_godot(browser_target(BROWSER_REGION, d.entry.region)) : godot::String();
        out["region_name"] = d.entry.region != 0 ? to_godot(history.timeline.region_name(d.entry.region, d.entry.year))
                                                 : godot::String();
    } catch (const std::exception& e) {
        godot::UtilityFunctions::push_error(godot::String("ViewerModel.get_event_detail: ") + e.what());
        return {};
    }
    return out;
}

bool ViewerModel::set_browser_scope(const godot::String& scope) {
    if (!get_browser_available()) {
        return false;
    }
    BrowserScope s = BrowserScope::all;
    if (scope == "all") {
        s = BrowserScope::all;
    } else if (scope == "entity") {
        s = BrowserScope::entity;
    } else if (scope == "place") {
        s = BrowserScope::place;
    } else {
        return false;
    }
    browser_.filter.scope = s;
    selection_changed(true);
    return true;
}

bool ViewerModel::set_browser_group(const godot::String& group) {
    if (!get_browser_available()) {
        return false;
    }
    int code = -2;
    if (group == "all") {
        code = -1;
    }
    for (std::uint32_t c = 0; c < 32U && code == -2; ++c) {
        if ((worldgen::inspect::ALL_GROUPS & (std::uint32_t{1} << c)) != 0U &&
            to_godot(worldgen::inspect::event_group_word(static_cast<worldgen::inspect::EventGroup>(c))) == group) {
            code = static_cast<int>(c);
        }
    }
    if (code == -2) {
        return false;
    }
    browser_.filter.group = code;
    selection_changed(true);
    return true;
}

bool ViewerModel::set_browser_level(const godot::String& level) {
    if (!get_browser_available()) {
        return false;
    }
    const auto parsed = worldgen::inspect::parse_history_level(to_utf8(level));
    if (!parsed) {
        return false;
    }
    browser_.filter.level = *parsed;
    selection_changed(true);
    return true;
}

bool ViewerModel::set_browser_years(int64_t from, int64_t to) {
    const int64_t end = history_.end_year();
    if (!get_browser_available() || from < -1 || to < -1 || from > end || to > end) {
        return false;
    }
    browser_.filter.from = from;
    browser_.filter.to = to;
    selection_changed(true);
    return true;
}

bool ViewerModel::set_browser_this_year(bool on) {
    if (!get_browser_available()) {
        return false;
    }
    browser_.filter.this_year = on;
    selection_changed(true);
    return true;
}

bool ViewerModel::select_civilization(int64_t civilization) {
    if (!get_browser_available() || (civilization != 0 && browser_.civilization(civilization) == nullptr)) {
        return false;
    }
    browser_.push_path(history_.year());
    BrowserSelection& s = browser_.selection;
    s = {};
    if (civilization != 0) {
        s.entity_type = BROWSER_CIVILIZATION;
        s.entity_id = civilization;
    }
    browser_.filter.scope = civilization != 0 ? BrowserScope::entity : BrowserScope::all;
    browser_.follow = civilization != 0;
    selection_changed(true);
    return true;
}

bool ViewerModel::select_event(int64_t event) {
    const worldgen::inspect::EventEntry* e = get_browser_available() ? browser_.event(event) : nullptr;
    if (e == nullptr) {
        return false;
    }
    browser_.push_path(history_.year());
    browser_.selection.event = event;
    set_year(e->year);
    selection_changed(false);
    return true;
}

bool ViewerModel::select_place(int64_t type, int64_t id) {
    if (!get_browser_available()) {
        return false;
    }
    const auto& timeline = history_.history().timeline;
    const bool valid = (type == BROWSER_SETTLEMENT && id >= 1 && static_cast<std::uint64_t>(id) <= timeline.settlement_count()) ||
                       (type == BROWSER_REGION && id >= 1 && history_.holding(id).has_value());
    if (!valid) {
        return false;
    }
    browser_.push_path(history_.year());
    browser_.selection.place_type = type;
    browser_.selection.place_id = id;
    browser_.selection.event = 0;
    browser_.filter.scope = BrowserScope::place;
    selection_changed(true);
    return true;
}

bool ViewerModel::clear_selection() {
    if (!get_browser_available()) {
        return false;
    }
    browser_.push_path(history_.year());
    browser_.selection = {};
    browser_.filter.scope = BrowserScope::all;
    browser_.follow = false;
    selection_changed(true);
    return true;
}

bool ViewerModel::follow_link(const godot::String& target) {
    std::string prefix;
    std::int64_t id = 0;
    if (!get_browser_available() || !parse_browser_target(to_utf8(target), prefix, id)) {
        return false;
    }
    // 5.2: c, s, rg and e; ra, g and (until section 9) m, si, f, fa are not
    // followed.
    if (prefix == "c") {
        return id != 0 && select_civilization(id);
    }
    if (prefix == "s") {
        return select_place(BROWSER_SETTLEMENT, id);
    }
    if (prefix == "rg") {
        return select_place(BROWSER_REGION, id);
    }
    if (prefix == "e") {
        return select_event(id);
    }
    return false;
}

bool ViewerModel::can_follow_link(const godot::String& target) const {
    std::string prefix;
    std::int64_t id = 0;
    return get_browser_available() && parse_browser_target(to_utf8(target), prefix, id) &&
           (prefix == "c" || prefix == "s" || prefix == "rg" || prefix == "e");
}

bool ViewerModel::browser_back() {
    if (!get_browser_available()) {
        return false;
    }
    const auto entry = browser_.back(history_.year());
    if (!entry) {
        return false;
    }
    restore_path(*entry);
    return true;
}

bool ViewerModel::browser_forward() {
    if (!get_browser_available()) {
        return false;
    }
    const auto entry = browser_.forward(history_.year());
    if (!entry) {
        return false;
    }
    restore_path(*entry);
    return true;
}

bool ViewerModel::can_browser_back() const {
    return get_browser_available() && browser_.can_back();
}

bool ViewerModel::can_browser_forward() const {
    return get_browser_available() && browser_.can_forward();
}

bool ViewerModel::set_follow(bool on) {
    if (!get_browser_available()) {
        return false;
    }
    if (browser_.follow != on) {
        browser_.follow = on;
        emit_signal("browser_selection_changed");
    }
    return true;
}

bool ViewerModel::get_follow() const {
    return get_browser_available() && browser_.follow;
}

bool ViewerModel::follow_first() {
    if (!get_browser_available() || browser_.ids().empty()) {
        return false;
    }
    return select_event(browser_.ids().front());
}

godot::Array ViewerModel::search_names(const godot::String& query) const {
    godot::Array out;
    if (!get_browser_available()) {
        return out;
    }
    try {
        for (const auto& m : browser_.index().search(history_.history(), to_utf8(query), 50)) {
            godot::Dictionary row;
            row["target"] = to_godot(browser_target(m.type, m.id));
            row["name"] = to_godot(m.name);
            row["kind"] = match_kind_word(m.type);
            out.push_back(row);
        }
    } catch (const std::exception& e) {
        godot::UtilityFunctions::push_error(godot::String("ViewerModel.search_names: ") + e.what());
        return {};
    }
    return out;
}

godot::Dictionary ViewerModel::get_selection() const {
    godot::Dictionary out;
    if (!get_browser_available()) {
        return out;
    }
    const BrowserSelection& s = browser_.selection;
    const BrowserFilter& f = browser_.filter;
    out["entity_type"] = s.entity_type;
    out["entity_id"] = s.entity_id;
    out["event"] = s.event;
    out["place_type"] = s.place_type;
    out["place_id"] = s.place_id;
    out["scope"] = scope_word(f.scope);
    out["group"] = f.group < 0 ? godot::String("all")
                               : to_godot(worldgen::inspect::event_group_word(
                                     static_cast<worldgen::inspect::EventGroup>(f.group)));
    out["level"] = f.level == worldgen::inspect::HistoryLevel::full ? "full" : "summary";
    out["from"] = f.from;
    out["to"] = f.to;
    out["this_year"] = f.this_year;
    out["follow"] = browser_.follow;
    return out;
}

godot::Dictionary ViewerModel::get_highlight() const {
    godot::Dictionary out;
    if (!get_browser_available()) {
        return out;
    }
    const BrowserHighlight& h = browser_.highlight();
    out["civilization"] = h.civilization;
    out["regions"] = packed(h.regions);
    out["settlements"] = packed(h.settlements);
    out["place"] = godot::Vector2i(static_cast<int32_t>(h.place[0]), static_cast<int32_t>(h.place[1]));
    out["place_event"] = h.place_event;
    out["place_settlement"] = h.place_settlement;
    out["dim"] = h.dim;
    return out;
}

godot::String ViewerModel::get_place_text() const {
    if (!get_browser_available()) {
        return {};
    }
    const BrowserSelection& s = browser_.selection;
    if (s.place_type == BROWSER_SETTLEMENT && s.place_id != 0) {
        // The P4-04 hover text of the settlement's entry at Y.
        for (std::size_t i = 0; i < marker_order_.size(); ++i) {
            const MarkerRef& ref = marker_order_[i];
            if ((ref.type == worldgen::inspect::MarkerType::standing || ref.type == worldgen::inspect::MarkerType::ruin ||
                 ref.type == worldgen::inspect::MarkerType::faded) &&
                history_.markers().settlements[ref.index].settlement == s.place_id) {
                return describe_marker(static_cast<int64_t>(i));
            }
        }
        const std::string* name =
            history_.history().view.name(worldgen::history::EntityType::settlement, s.place_id);
        return to_godot((name != nullptr ? *name : "settlement " + std::to_string(s.place_id)) +
                        ": not on the map in " + std::to_string(history_.year()));
    }
    if (s.place_type == BROWSER_REGION && s.place_id != 0) {
        const auto name = history_.region_name(s.place_id);
        return to_godot((name ? *name : "region " + std::to_string(s.place_id)) + history_.holding_text(s.place_id));
    }
    return {};
}

int64_t ViewerModel::get_event_row_at_year(int64_t year) const {
    if (!get_browser_available()) {
        return -1;
    }
    return static_cast<int64_t>(browser_.first_row_at_or_after(year));
}

int64_t ViewerModel::get_region_at(int64_t x, int64_t y) {
    const godot::String saved_code = error_code_;
    const godot::String saved_message = error_message_;
    const worldgen::db::LayerGrid* grid = has_world() ? layer_grid("region") : nullptr;
    error_code_ = saved_code;
    error_message_ = saved_message;
    if (grid == nullptr) {
        return -1;
    }
    if (x < 0 || y < 0 || x >= int64_t{grid->width} || y >= int64_t{grid->height}) {
        return 0;
    }
    std::int64_t value = 0;
    return worldgen::inspect::cell_value(*grid, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), value)
               ? value
               : 0;
}

void ViewerModel::bind_browser_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    using godot::MethodInfo;
    ClassDB::bind_method(D_METHOD("get_browser_available"), &ViewerModel::get_browser_available);
    ClassDB::bind_method(D_METHOD("get_browser_error_code"), &ViewerModel::get_browser_error_code);
    ClassDB::bind_method(D_METHOD("get_browser_error_message"), &ViewerModel::get_browser_error_message);
    ClassDB::bind_method(D_METHOD("get_event_groups"), &ViewerModel::get_event_groups);
    ClassDB::bind_method(D_METHOD("get_civilization_list"), &ViewerModel::get_civilization_list);
    ClassDB::bind_method(D_METHOD("get_civilization_block", "civilization"), &ViewerModel::get_civilization_block);
    ClassDB::bind_method(D_METHOD("get_event_ids"), &ViewerModel::get_event_ids);
    ClassDB::bind_method(D_METHOD("get_event_rows"), &ViewerModel::get_event_rows);
    ClassDB::bind_method(D_METHOD("get_event_detail", "event"), &ViewerModel::get_event_detail);
    ClassDB::bind_method(D_METHOD("set_browser_scope", "scope"), &ViewerModel::set_browser_scope);
    ClassDB::bind_method(D_METHOD("set_browser_group", "group"), &ViewerModel::set_browser_group);
    ClassDB::bind_method(D_METHOD("set_browser_level", "level"), &ViewerModel::set_browser_level);
    ClassDB::bind_method(D_METHOD("set_browser_years", "from", "to"), &ViewerModel::set_browser_years);
    ClassDB::bind_method(D_METHOD("set_browser_this_year", "on"), &ViewerModel::set_browser_this_year);
    ClassDB::bind_method(D_METHOD("select_civilization", "civilization"), &ViewerModel::select_civilization);
    ClassDB::bind_method(D_METHOD("select_event", "event"), &ViewerModel::select_event);
    ClassDB::bind_method(D_METHOD("select_place", "type", "id"), &ViewerModel::select_place);
    ClassDB::bind_method(D_METHOD("clear_selection"), &ViewerModel::clear_selection);
    ClassDB::bind_method(D_METHOD("follow_link", "target"), &ViewerModel::follow_link);
    ClassDB::bind_method(D_METHOD("can_follow_link", "target"), &ViewerModel::can_follow_link);
    ClassDB::bind_method(D_METHOD("browser_back"), &ViewerModel::browser_back);
    ClassDB::bind_method(D_METHOD("browser_forward"), &ViewerModel::browser_forward);
    ClassDB::bind_method(D_METHOD("can_browser_back"), &ViewerModel::can_browser_back);
    ClassDB::bind_method(D_METHOD("can_browser_forward"), &ViewerModel::can_browser_forward);
    ClassDB::bind_method(D_METHOD("set_follow", "on"), &ViewerModel::set_follow);
    ClassDB::bind_method(D_METHOD("get_follow"), &ViewerModel::get_follow);
    ClassDB::bind_method(D_METHOD("follow_first"), &ViewerModel::follow_first);
    ClassDB::bind_method(D_METHOD("search_names", "query"), &ViewerModel::search_names);
    ClassDB::bind_method(D_METHOD("get_selection"), &ViewerModel::get_selection);
    ClassDB::bind_method(D_METHOD("get_highlight"), &ViewerModel::get_highlight);
    ClassDB::bind_method(D_METHOD("get_place_text"), &ViewerModel::get_place_text);
    ClassDB::bind_method(D_METHOD("get_event_row_at_year", "year"), &ViewerModel::get_event_row_at_year);
    ClassDB::bind_method(D_METHOD("get_region_at", "x", "y"), &ViewerModel::get_region_at);
    ADD_SIGNAL(MethodInfo("browser_selection_changed"));
    ADD_SIGNAL(MethodInfo("event_list_changed"));
    ADD_SIGNAL(MethodInfo("highlight_changed"));
}

}  // namespace unmapped
