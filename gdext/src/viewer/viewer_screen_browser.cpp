// ViewerScreen: the browser pane (spec P4-05 section 6). Presentation only:
// every list, text, link target and highlight comes from the model.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <initializer_list>
#include <utility>

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "viewer/viewer_screen.hpp"

namespace unmapped {

namespace {

int64_t now_nsec() {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// The civilization list's swatch: a 14 x 14 square of one colour.
constexpr int32_t SWATCH = 14;

godot::Ref<godot::ImageTexture> swatch_icon(int64_t colour) {
    godot::PackedByteArray data;
    data.resize(int64_t{SWATCH} * SWATCH * 3);
    uint8_t* out = data.ptrw();
    for (int64_t i = 0; i < int64_t{SWATCH} * SWATCH; ++i) {
        out[i * 3] = static_cast<uint8_t>((colour >> 16) & 0xFF);
        out[i * 3 + 1] = static_cast<uint8_t>((colour >> 8) & 0xFF);
        out[i * 3 + 2] = static_cast<uint8_t>(colour & 0xFF);
    }
    const godot::Ref<godot::Image> image =
        godot::Image::create_from_data(SWATCH, SWATCH, false, godot::Image::FORMAT_RGB8, data);
    return godot::ImageTexture::create_from_image(image);
}

godot::String years_text(int64_t from, int64_t to) {
    return to < 0 ? godot::String::num_int64(from) + "-"
                  : godot::String::num_int64(from) + "-" + godot::String::num_int64(to);
}

constexpr const char* SCOPES[] = {"all", "entity", "place"};
constexpr const char* SCOPE_LABELS[] = {"All", "Selected", "Place"};

}  // namespace

std::vector<godot::Control*> ViewerScreen::browser_controls() const {
    return {browser_back_,  browser_forward_, browser_search_, event_scope_,    event_group_, event_all_,
            event_from_,    event_to_,        event_this_year_, follow_toggle_, follow_first_};
}

void ViewerScreen::wire_browser() {
    civ_list_->set_column_title(0, "Civilization");
    civ_list_->set_column_title(1, "Race");
    civ_list_->set_column_title(2, "Years");
    civ_list_->set_column_title(3, "Fate");
    civ_list_->set_column_expand_ratio(0, 3);
    civ_list_->set_column_expand_ratio(1, 1);
    civ_list_->set_column_expand_ratio(2, 1);
    civ_list_->set_column_expand_ratio(3, 1);
    for (int32_t c = 0; c < 4; ++c) {
        civ_list_->set_column_clip_content(c, true);
    }
    for (std::size_t i = 0; i < 3; ++i) {
        event_scope_->add_item(SCOPE_LABELS[i], static_cast<int32_t>(i));
    }
    // Narrow year fields (four digits), so that the pane keeps its 380 px
    // minimum at 1280 x 800 (6.1).
    for (godot::SpinBox* box : {event_from_, event_to_}) {
        box->get_line_edit()->add_theme_constant_override("minimum_character_width", 3);
    }
    browser_detail_->set_use_bbcode(false);
    browser_detail_->set_meta_underline(true);
    browser_search_->connect("text_changed", callable_mp(this, &ViewerScreen::on_search_changed));
    search_results_->connect("item_selected", callable_mp(this, &ViewerScreen::on_search_result_selected));
    civ_list_->connect("item_selected", callable_mp(this, &ViewerScreen::on_civ_list_selected));
    event_list_->connect("item_selected", callable_mp(this, &ViewerScreen::on_event_row_selected));
    event_scope_->connect("item_selected", callable_mp(this, &ViewerScreen::on_scope_selected));
    event_group_->connect("item_selected", callable_mp(this, &ViewerScreen::on_group_selected));
    event_all_->connect("toggled", callable_mp(this, &ViewerScreen::on_event_all_toggled));
    event_from_->connect("value_changed", callable_mp(this, &ViewerScreen::on_event_years_changed));
    event_to_->connect("value_changed", callable_mp(this, &ViewerScreen::on_event_years_changed));
    event_this_year_->connect("toggled", callable_mp(this, &ViewerScreen::on_this_year_toggled));
    follow_toggle_->connect("toggled", callable_mp(this, &ViewerScreen::on_follow_toggled));
    follow_first_->connect("pressed", callable_mp(this, &ViewerScreen::on_follow_first_pressed));
    browser_back_->connect("pressed", callable_mp(this, &ViewerScreen::on_browser_back_pressed));
    browser_forward_->connect("pressed", callable_mp(this, &ViewerScreen::on_browser_forward_pressed));
    browser_detail_->connect("meta_clicked", callable_mp(this, &ViewerScreen::on_detail_meta_clicked));
    map_view_->connect("cell_clicked", callable_mp(this, &ViewerScreen::on_cell_clicked));
    model_->connect("browser_selection_changed", callable_mp(this, &ViewerScreen::on_browser_selection_changed));
    model_->connect("event_list_changed", callable_mp(this, &ViewerScreen::on_event_list_changed));
    model_->connect("highlight_changed", callable_mp(this, &ViewerScreen::on_highlight_changed));
    browser_world_closed();
}

void ViewerScreen::browser_world_opened() {
    const bool available = model_->get_browser_available();
    updating_browser_ = true;
    // The group control: "All kinds", then the model's group words.
    event_group_->clear();
    event_group_->add_item("All kinds", 0);
    group_words_ = model_->get_event_groups();
    for (int64_t i = 0; i < group_words_.size(); ++i) {
        event_group_->add_item(group_words_[i], static_cast<int32_t>(i + 1));
    }
    const int64_t end = available ? model_->get_end_year() : 0;
    for (godot::SpinBox* box : {event_from_, event_to_}) {
        box->set_min(0.0);
        box->set_max(static_cast<double>(end));
    }
    event_from_->set_value_no_signal(0.0);
    event_to_->set_value_no_signal(static_cast<double>(end));
    browser_search_->clear();
    updating_browser_ = false;

    // The civilization list, ascending id (the report's order).
    civ_list_->clear();
    swatches_.clear();
    const godot::Array civs = model_->get_civilization_list();
    godot::TreeItem* root = civs.is_empty() ? nullptr : civ_list_->create_item();
    for (int64_t i = 0; i < civs.size(); ++i) {
        const godot::Dictionary c = civs[i];
        godot::TreeItem* row = civ_list_->create_item(root);
        const int64_t colour = c["colour"];
        auto found = swatches_.find(colour);
        if (found == swatches_.end()) {
            found = swatches_.emplace(colour, swatch_icon(colour < 0 ? 0 : colour)).first;
        }
        row->set_icon(0, found->second);
        row->set_text(0, c["name"]);
        row->set_tooltip_text(0, godot::String(c["name"]) + " (" + godot::String::num_int64(c["events"]) + " events)");
        row->set_text(1, c["race_name"]);
        row->set_text(2, years_text(c["founded_year"], c["ended_year"]));
        row->set_text(3, c["fate"]);
        row->set_metadata(0, c["id"]);
    }
    // 6.1: a world with history opens on Browser; without, on Legend with
    // the Browser tab disabled.
    right_pane_->set_tab_disabled(0, !available);
    right_pane_->set_current_tab(available ? 0 : 1);
    on_event_list_changed();
    on_browser_selection_changed();
}

void ViewerScreen::browser_world_closed() {
    civ_list_->clear();
    swatches_.clear();
    search_results_->clear();
    search_rows_.clear();
    event_list_->clear();
    event_ids_ = godot::PackedInt64Array();
    browser_detail_->clear();
    event_group_->clear();
    group_words_ = godot::PackedStringArray();
    mask_version_shown_ = -1;
    map_view_->set_mask({}, 0, 0);
    map_view_->set_frames({}, -1);
    map_view_->set_event_place(godot::Vector2i(-1, -1));
    right_pane_->set_tab_disabled(0, true);
    right_pane_->set_current_tab(1);
    update_browser_controls();
}

void ViewerScreen::update_browser_controls() {
    const bool available = model_->get_browser_available();
    updating_browser_ = true;
    const godot::Dictionary sel = model_->get_selection();
    browser_back_->set_disabled(!model_->can_browser_back());
    browser_forward_->set_disabled(!model_->can_browser_forward());
    browser_search_->set_editable(available);
    event_scope_->set_disabled(!available);
    event_group_->set_disabled(!available);
    event_all_->set_disabled(!available);
    event_from_->set_editable(available);
    event_to_->set_editable(available);
    event_this_year_->set_disabled(!available);
    follow_toggle_->set_disabled(!available);
    follow_first_->set_disabled(!available || model_->get_event_ids().is_empty());
    follow_toggle_->set_pressed_no_signal(model_->get_follow());
    event_all_->set_pressed_no_signal(available && sel.get("level", "") == godot::Variant("full"));
    event_this_year_->set_pressed_no_signal(available && bool(sel.get("this_year", false)));
    const godot::String scope = sel.get("scope", "all");
    for (int32_t i = 0; i < 3; ++i) {
        if (scope == SCOPES[i]) {
            event_scope_->select(i);
        }
    }
    const godot::String group = sel.get("group", "all");
    int32_t group_item = 0;
    for (int64_t i = 0; i < group_words_.size(); ++i) {
        if (group_words_[i] == group) {
            group_item = static_cast<int32_t>(i + 1);
        }
    }
    if (event_group_->get_item_count() > group_item) {
        event_group_->select(group_item);
    }
    // The civilization list shows the selected civilization.
    const int64_t civ = int64_t(sel.get("entity_type", 0)) == 3 ? int64_t(sel.get("entity_id", 0)) : 0;
    godot::TreeItem* root = civ_list_->get_root();
    godot::TreeItem* selected = civ_list_->get_selected();
    if (root != nullptr) {
        godot::TreeItem* want = nullptr;
        for (godot::TreeItem* row = root->get_first_child(); row != nullptr; row = row->get_next()) {
            if (int64_t(row->get_metadata(0)) == civ) {
                want = row;
            }
        }
        if (want == nullptr) {
            if (selected != nullptr) {
                civ_list_->deselect_all();
            }
        } else if (want != selected) {
            want->select(0);
            civ_list_->scroll_to_item(want);
        }
    }
    // The event list shows the selected event.
    const int64_t event = sel.get("event", 0);
    int64_t row = -1;
    const int64_t* ids = event_ids_.ptr();
    const int64_t n = event_ids_.size();
    if (event != 0 && n > 0) {
        const int64_t* it = std::lower_bound(ids, ids + n, event);
        row = it != ids + n && *it == event ? it - ids : -1;
    }
    if (row >= 0) {
        if (!event_list_->is_selected(static_cast<int32_t>(row))) {
            event_list_->select(static_cast<int32_t>(row));
        }
        event_list_->call_deferred("ensure_current_is_visible");
    } else {
        event_list_->deselect_all();
    }
    updating_browser_ = false;
}

void ViewerScreen::on_browser_selection_changed() {
    update_browser_controls();
    update_year_bar(false);  // the event steps follow the list (5.3)
    show_browser_detail();
}

void ViewerScreen::on_event_list_changed() {
    const int64_t start = now_nsec();
    event_ids_ = model_->get_event_ids();
    const godot::PackedStringArray rows = model_->get_event_rows();
    event_list_->clear();
    for (int64_t i = 0; i < rows.size(); ++i) {
        const int32_t item = event_list_->add_item(rows[i]);
        event_list_->set_item_tooltip(item, rows[i]);
    }
    update_browser_controls();
    // After layout: a list rebuilt just now has no item positions yet.
    callable_mp(this, &ViewerScreen::scroll_event_list).call_deferred();
    last_list_rebuild_nsec_ = now_nsec() - start;
    year_steps_.list = last_list_rebuild_nsec_;
}

void ViewerScreen::scroll_event_list() {
    // 6.3: a year change scrolls the list to the first row of year >= Y,
    // leaving the selection alone.
    if (event_ids_.is_empty() || !model_->has_history()) {
        return;
    }
    const int64_t row = std::min(model_->get_event_row_at_year(model_->get_year()), event_ids_.size() - 1);
    if (row < 0) {
        return;
    }
    godot::VScrollBar* bar = event_list_->get_v_scroll_bar();
    if (bar != nullptr) {
        bar->set_value(static_cast<double>(event_list_->get_item_rect(static_cast<int32_t>(row)).position.y));
    }
}

void ViewerScreen::push_highlight_frames() {
    const godot::Dictionary h = model_->get_highlight();
    std::vector<int64_t> gold;
    int64_t place = -1;
    if (!h.is_empty()) {
        const godot::PackedInt64Array settlements = h["settlements"];
        const int64_t place_settlement = h["place_settlement"];
        const godot::Array markers = model_->get_markers();
        for (int64_t i = 0; i < markers.size(); ++i) {
            const godot::Dictionary m = markers[i];
            const godot::String type = m["type"];
            if (type != "standing" && type != "ruin" && type != "faded") {
                continue;
            }
            const int64_t id = m["id"];
            if (type == "standing" && settlements.has(id)) {
                gold.push_back(i);
            }
            if (id == place_settlement && place_settlement != 0) {
                place = i;
            }
        }
    }
    map_view_->set_frames(gold, place);
    map_view_->set_event_place(h.is_empty() ? godot::Vector2i(-1, -1) : godot::Vector2i(h["place"]));
}

void ViewerScreen::on_highlight_changed() {
    const int64_t start = now_nsec();
    if (model_->highlight_mask_version() != mask_version_shown_) {
        mask_version_shown_ = model_->highlight_mask_version();
        map_view_->set_mask(model_->highlight_mask(), model_->highlight_mask_width(), model_->highlight_mask_height());
    }
    push_highlight_frames();
    last_highlight_nsec_ = now_nsec() - start;
    year_steps_.highlight = last_highlight_nsec_;
}

void ViewerScreen::add_link(const godot::String& target, const godot::String& text) {
    // Only the targets the model follows are links (5.2); the others (ra, g,
    // m, si, f, fa) are plain text.
    if (model_->can_follow_link(target)) {
        browser_detail_->push_meta(target, godot::RichTextLabel::META_UNDERLINE_ALWAYS, target);
        browser_detail_->add_text(text);
        browser_detail_->pop();
    } else {
        browser_detail_->add_text(text);
    }
}

void ViewerScreen::add_linked_text(const godot::Dictionary& linked, int64_t from_char) {
    // Built with push_meta, add_text and pop, never by parsing BBCode, so
    // world text cannot inject markup (6.1).
    const godot::String text = linked.get("text", godot::String());
    const godot::Array links = linked.get("links", godot::Array());
    int64_t at = from_char;
    for (int64_t i = 0; i < links.size(); ++i) {
        const godot::Dictionary link = links[i];
        const int64_t start = link["start"];
        const int64_t length = link["length"];
        if (start < at) {
            continue;
        }
        browser_detail_->add_text(text.substr(at, start - at));
        add_link(link["target"], text.substr(start, length));
        at = start + length;
    }
    browser_detail_->add_text(text.substr(at));
}

void ViewerScreen::show_browser_detail() {
    browser_detail_->clear();
    if (!model_->get_browser_available()) {
        browser_detail_->add_text(model_->has_history() ? "The browser is not available for this world."
                                                        : "No history in this world.");
        return;
    }
    const godot::Dictionary sel = model_->get_selection();
    const int64_t civ = int64_t(sel["entity_type"]) == 3 ? int64_t(sel["entity_id"]) : 0;
    const int64_t event = sel["event"];
    const int64_t place_type = sel["place_type"];
    const godot::Dictionary civ_info = civ != 0 ? model_->get_civilization_info(civ) : godot::Dictionary();
    if (event != 0) {
        const godot::Dictionary d = model_->get_event_detail(event);
        if (d.is_empty()) {
            return;
        }
        if (civ != 0) {
            add_link("c" + godot::String::num_int64(civ), civ_info.get("name", godot::String()));
            browser_detail_->newline();
        }
        const int64_t year = d["year"];
        const int64_t end = d["end_year"];
        browser_detail_->add_text(godot::String::num_int64(year) +
                                  (end != year ? "-" + godot::String::num_int64(end) : godot::String()) + "  ");
        add_linked_text(d["sentence"]);
        const godot::String note = d["note"];
        if (!note.is_empty()) {
            browser_detail_->add_text(" (" + note + ")");
        }
        browser_detail_->newline();
        const godot::String settlement_target = d["settlement_target"];
        const godot::String region_target = d["region_target"];
        if (!settlement_target.is_empty() || !region_target.is_empty()) {
            browser_detail_->add_text("at ");
            if (!settlement_target.is_empty()) {
                add_link(settlement_target, d["settlement_name"]);
                browser_detail_->add_text(!region_target.is_empty() ? ", " : "");
            }
            if (!region_target.is_empty()) {
                add_link(region_target, d["region_name"]);
            }
            browser_detail_->newline();
        }
        browser_detail_->add_text("kind " + godot::String(d["kind_key"]) + " (" + godot::String(d["group"]) +
                                  "), salience " + godot::String::num_int64(d["salience"]) +
                                  (bool(d["summary"]) ? ", in the summary" : ""));
        browser_detail_->newline();
        const godot::Array participants = d["participants"];
        if (!participants.is_empty()) {
            browser_detail_->add_text("participants:");
            browser_detail_->newline();
            for (int64_t i = 0; i < participants.size(); ++i) {
                const godot::Dictionary p = participants[i];
                const godot::String name = p["name"];
                browser_detail_->add_text("  " + godot::String(p["role"]) + ": ");
                add_link(p["target"], name.is_empty() ? godot::String(p["target"]) : name);
                browser_detail_->newline();
            }
        }
        const godot::Array values = d["values"];
        if (!values.is_empty()) {
            browser_detail_->add_text("values:");
            browser_detail_->newline();
            for (int64_t i = 0; i < values.size(); ++i) {
                const godot::Dictionary v = values[i];
                browser_detail_->add_text("  " + godot::String(v["key"]) + " = " + godot::String::num_int64(v["value"]));
                browser_detail_->newline();
            }
        }
        for (const auto& [key, heading] : {std::pair<const char*, const char*>{"causes", "caused by:"},
                                           std::pair<const char*, const char*>{"effects", "led to:"}}) {
            const godot::Array rows = d[key];
            if (rows.is_empty()) {
                continue;
            }
            browser_detail_->add_text(heading);
            browser_detail_->newline();
            for (int64_t i = 0; i < rows.size(); ++i) {
                const godot::Dictionary r = rows[i];
                browser_detail_->add_text("  ");
                add_link(r["target"], godot::String(r["target"]) + "  " + godot::String(r["row"]));
                browser_detail_->newline();
            }
        }
        return;
    }
    if (place_type != 0) {
        browser_detail_->add_text(model_->get_place_text());
        browser_detail_->newline();
        browser_detail_->add_text(godot::String::num_int64(event_ids_.size()) + " events in the list above.");
        return;
    }
    if (civ != 0) {
        const godot::Array block = model_->get_civilization_block(civ);
        for (int64_t i = 0; i < block.size(); ++i) {
            add_linked_text(block[i]);
            browser_detail_->newline();
        }
        return;
    }
    browser_detail_->add_text(
        "Choose a civilization, an event or a place on the map. A civilization turns on Follow: "
        "the event steps (Page Up, Page Down) then walk its events.");
}

void ViewerScreen::on_cell_clicked(int64_t x, int64_t y, int64_t marker) {
    // 5.5: a settlement marker selects the settlement, a ring its smallest
    // event, a pass nothing; off any marker, a cell of a region the region.
    if (!model_->get_browser_available()) {
        return;
    }
    const godot::Array markers = model_->get_markers();
    if (marker >= 0 && marker < markers.size()) {
        const godot::Dictionary m = markers[marker];
        const godot::String type = m["type"];
        if (type == "standing" || type == "ruin" || type == "faded") {
            model_->select_place(4, m["id"]);
        } else if (type == "place") {
            const godot::PackedInt64Array events = m.get("events", godot::PackedInt64Array());
            int64_t smallest = 0;
            for (int64_t i = 0; i < events.size(); ++i) {
                smallest = smallest == 0 ? events[i] : std::min(smallest, events[i]);
            }
            if (smallest != 0) {
                model_->select_event(smallest);
            }
        }
        return;
    }
    if (x < 0 || y < 0) {
        return;
    }
    const int64_t region = model_->get_region_at(x, y);
    if (region > 0) {
        model_->select_place(5, region);
    }
}

void ViewerScreen::on_civ_list_selected() {
    if (updating_browser_) {
        return;
    }
    godot::TreeItem* row = civ_list_->get_selected();
    if (row != nullptr) {
        model_->select_civilization(row->get_metadata(0));
    }
}

void ViewerScreen::on_event_row_selected(int64_t index) {
    if (!updating_browser_ && index >= 0 && index < event_ids_.size()) {
        model_->select_event(event_ids_[index]);
    }
}

void ViewerScreen::on_search_changed(const godot::String& text) {
    // 6.3: every change searches (at most 50 rows); the results replace the
    // civilization list while the text is not empty.
    search_results_->clear();
    search_rows_ = text.is_empty() ? godot::Array() : model_->search_names(text);
    for (int64_t i = 0; i < search_rows_.size(); ++i) {
        const godot::Dictionary r = search_rows_[i];
        search_results_->add_item(godot::String(r["name"]) + "   (" + godot::String(r["kind"]) + ")");
    }
    search_results_->set_visible(!text.is_empty());
    civ_list_->set_visible(text.is_empty());
}

void ViewerScreen::on_search_result_selected(int64_t index) {
    if (index >= 0 && index < search_rows_.size()) {
        model_->follow_link(godot::Dictionary(search_rows_[index])["target"]);
    }
}

void ViewerScreen::on_scope_selected(int64_t index) {
    if (!updating_browser_ && index >= 0 && index < 3) {
        model_->set_browser_scope(SCOPES[index]);
    }
}

void ViewerScreen::on_group_selected(int64_t index) {
    if (updating_browser_) {
        return;
    }
    model_->set_browser_group(index <= 0 || index > group_words_.size() ? godot::String("all")
                                                                         : group_words_[index - 1]);
}

void ViewerScreen::on_event_all_toggled(bool on) {
    if (!updating_browser_) {
        model_->set_browser_level(on ? "full" : "summary");
    }
}

void ViewerScreen::on_event_years_changed(double /*value*/) {
    if (!updating_browser_) {
        model_->set_browser_years(static_cast<int64_t>(std::llround(event_from_->get_value())),
                                  static_cast<int64_t>(std::llround(event_to_->get_value())));
    }
}

void ViewerScreen::on_this_year_toggled(bool on) {
    if (!updating_browser_) {
        model_->set_browser_this_year(on);
    }
}

void ViewerScreen::on_follow_toggled(bool on) {
    if (!updating_browser_) {
        model_->set_follow(on);
    }
}

void ViewerScreen::on_follow_first_pressed() {
    model_->follow_first();
}

void ViewerScreen::on_browser_back_pressed() {
    model_->browser_back();
}

void ViewerScreen::on_browser_forward_pressed() {
    model_->browser_forward();
}

void ViewerScreen::on_detail_meta_clicked(const godot::Variant& meta) {
    model_->follow_link(meta.stringify());
}

void ViewerScreen::_input(const godot::Ref<godot::InputEvent>& event) {
    // Escape and Ctrl+F before the focused control sees them (6.5).
    const godot::Ref<godot::InputEventKey> key = event;
    if (!wired_ || key.is_null() || !key->is_pressed() || key->is_echo()) {
        return;
    }
    if (key->get_keycode() == godot::KEY_F && key->is_ctrl_pressed() && !key->is_alt_pressed()) {
        right_pane_->set_visible(true);
        right_pane_->set_current_tab(0);
        browser_search_->grab_focus();
        get_viewport()->set_input_as_handled();
        return;
    }
    if (key->get_keycode() == godot::KEY_ESCAPE && !key->is_ctrl_pressed() && !key->is_alt_pressed()) {
        godot::Control* focus = get_viewport()->gui_get_focus_owner();
        if (focus != nullptr && right_pane_->is_ancestor_of(focus)) {
            focus->release_focus();
            get_viewport()->set_input_as_handled();
        } else if (model_->get_browser_available()) {
            model_->clear_selection();
            get_viewport()->set_input_as_handled();
        }
    }
}

void ViewerScreen::apply_browser_options() {
    if (start_width_ > 0 && start_height_ > 0) {
        get_window()->set_size(godot::Vector2i(static_cast<int32_t>(start_width_), static_cast<int32_t>(start_height_)));
    }
    if (!start_group_.is_empty()) {
        model_->set_browser_group(start_group_);
    }
    if (start_full_) {
        model_->set_browser_level("full");
    }
    if (!start_select_.is_empty()) {
        // Several targets, comma separated, followed in turn.
        const godot::PackedStringArray targets = start_select_.split(",", false);
        for (int64_t i = 0; i < targets.size(); ++i) {
            if (!model_->follow_link(targets[i].strip_edges())) {
                godot::UtilityFunctions::push_warning("Viewer: --select: cannot follow ", targets[i]);
            }
        }
    }
    if (!start_search_.is_empty()) {
        browser_search_->set_text(start_search_);
        on_search_changed(start_search_);
    }
    if (start_tab_ == "legend") {
        right_pane_->set_current_tab(1);
    } else if (start_tab_ == "browser") {
        right_pane_->set_current_tab(0);
    }
}

}  // namespace unmapped

namespace unmapped {

void ViewerScreen::bench_browser_filters() {
    // P4-05 7: filter changes (the model's select, get_event_rows and the
    // %EventList rebuild) and selection changes (detail, highlight, mask,
    // list), each timed as one synchronous call.
    struct Stat {
        int64_t max = 0;
        int64_t total = 0;
        int64_t n = 0;
        void add(int64_t v) {
            max = std::max(max, v);
            total += v;
            ++n;
        }
    };
    const auto nms = [](int64_t nsec) { return godot::String::num(static_cast<double>(nsec) / 1000000.0, 3) + " ms"; };
    const auto timed = [](const auto& call) {
        const int64_t start = now_nsec();
        call();
        return now_nsec() - start;
    };
    model_->clear_selection();
    model_->set_browser_group("all");
    model_->set_browser_level("summary");
    Stat filters;
    Stat full_all;
    Stat full_select;
    Stat full_rebuild;
    int64_t full_rows = 0;
    for (int rep = 0; rep < 5; ++rep) {
        full_all.add(timed([this] { model_->set_browser_level("full"); }));
        full_select.add(model_->browser_state().last_select_nsec());
        full_rebuild.add(last_list_rebuild_nsec_);
        full_rows = event_list_->get_item_count();
        for (int64_t i = 0; i < group_words_.size(); ++i) {
            const godot::String word = group_words_[i];
            filters.add(timed([this, &word] { model_->set_browser_group(word); }));
        }
        filters.add(timed([this] { model_->set_browser_group("all"); }));
        filters.add(timed([this] { model_->set_browser_level("summary"); }));
    }
    godot::UtilityFunctions::print("UNMAPPED-BENCH: filter change to full / all (", full_rows, " rows): max ",
                                   nms(full_all.max), ", mean ", nms(full_all.total / std::max<int64_t>(full_all.n, 1)),
                                   " (select max ", nms(full_select.max), ", get_event_rows and list rebuild max ",
                                   nms(full_rebuild.max), ")");
    godot::UtilityFunctions::print("UNMAPPED-BENCH: other filter changes (groups, summary): ", filters.n, ", max ",
                                   nms(filters.max), ", mean ", nms(filters.total / std::max<int64_t>(filters.n, 1)));
    // Selection changes at full: every civilization, then 50 events spread
    // over the list (each sets the year).
    model_->set_browser_level("full");
    Stat civs;
    const godot::Array list = model_->get_civilization_list();
    for (int64_t i = 0; i < list.size(); ++i) {
        const int64_t id = godot::Dictionary(list[i])["id"];
        civs.add(timed([this, id] { model_->select_civilization(id); }));
    }
    model_->clear_selection();
    const godot::PackedInt64Array ids = model_->get_event_ids();
    Stat events;
    for (int64_t k = 0; k < 50 && !ids.is_empty(); ++k) {
        const int64_t id = ids[k * ids.size() / 50];
        events.add(timed([this, id] { model_->select_event(id); }));
    }
    godot::UtilityFunctions::print("UNMAPPED-BENCH: select_civilization (", civs.n, "): max ", nms(civs.max), ", mean ",
                                   nms(civs.total / std::max<int64_t>(civs.n, 1)), "; select_event (", events.n,
                                   "): max ", nms(events.max), ", mean ",
                                   nms(events.total / std::max<int64_t>(events.n, 1)), "; slowest mask rebuild: model ",
                                   nms(model_->browser_timings().slowest_mask_nsec));
    model_->clear_selection();
    model_->set_browser_level("summary");
}

}  // namespace unmapped
