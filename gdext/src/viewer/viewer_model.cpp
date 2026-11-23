#include "viewer/viewer_model.hpp"

#include <algorithm>
#include <exception>
#include <new>
#include <span>
#include <optional>
#include <string>
#include <string_view>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

#include "worldgen/history/digest.hpp"
#include "worldgen/history/event_catalogue.hpp"
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

// Each row: label, colours and tooltip (empty unless tooltips is given and
// has one for the label).
godot::Array legend_array(const std::vector<worldgen::inspect::LegendEntry>& entries,
                          std::string_view (*tooltips)(std::string_view) noexcept = nullptr) {
    godot::Array out;
    for (const auto& entry : entries) {
        godot::Dictionary row;
        row["label"] = to_godot(entry.label);
        row["tooltip"] = to_godot(tooltips == nullptr ? std::string_view{} : tooltips(entry.label));
        godot::PackedInt32Array colours;
        for (const std::uint32_t c : entry.colours) {
            colours.push_back(static_cast<int32_t>(c));
        }
        row["colours"] = colours;
        out.push_back(row);
    }
    return out;
}

int64_t now_usec() {
    return static_cast<int64_t>(godot::Time::get_singleton()->get_ticks_usec());
}

}  // namespace

ViewerModel::ViewerModel() {
    world_.instantiate();
}

void ViewerModel::reset_views() {
    composites_.clear();
    layers_.clear();
    images_.clear();
    legend_.clear();
    selected_ = -1;
    history_.reset();
    timings_ = {};
    marker_timings_ = {};
    marker_order_.clear();
    markers_array_ = godot::Array();
}

bool ViewerModel::open_world(const godot::String& path) {
    const bool had_world = has_world();
    reset_views();
    if (had_world) {
        world_->close();
        emit_signal("world_closed");
    }
    if (!world_->open(path)) {
        error_code_ = world_->get_error_code();
        error_message_ = world_->get_error_message();
        emit_signal("world_open_failed", path, error_code_, error_message_);
        return false;
    }
    error_code_ = godot::String();
    error_message_ = godot::String();
    // The history, read once (P4-03 3.1); the territory view first among
    // the composites when it loaded and the composite regions is available.
    if (const worldgen::db::WorldReader* reader = world_->reader()) {
        history_.load(*reader);
        rebuild_marker_order();  // P4-04 8.1: the markers of E, computed by load
    }
    const std::vector<worldgen::inspect::Composite> available = world_->composites();
    if (history_.loaded() &&
        std::find(available.begin(), available.end(), worldgen::inspect::Composite::regions) != available.end()) {
        composites_.push_back(std::nullopt);
    }
    for (const auto composite : available) {
        composites_.emplace_back(composite);
    }
    layers_.resize(static_cast<std::size_t>(get_layer_count()));
    images_.resize(static_cast<std::size_t>(get_view_count()));
    emit_signal("world_opened", path);
    if (get_view_count() > 0) {
        select_view(0);
    }
    return true;
}

void ViewerModel::close_world() {
    if (!has_world()) {
        return;
    }
    world_->close();
    reset_views();
    emit_signal("world_closed");
}

bool ViewerModel::has_world() const {
    return world_->is_open();
}

godot::String ViewerModel::get_world_path() const {
    return world_->get_path();
}

godot::String ViewerModel::get_error_code() const {
    return error_code_;
}

godot::String ViewerModel::get_error_message() const {
    return error_message_;
}

godot::Array ViewerModel::get_meta_entries() const {
    return world_->get_meta_entries();
}

int64_t ViewerModel::get_layer_count() const {
    return world_->get_layer_count();
}

int64_t ViewerModel::get_composite_count() const {
    return static_cast<int64_t>(composites_.size());
}

int64_t ViewerModel::get_view_count() const {
    return get_layer_count() + get_composite_count();
}

godot::Dictionary ViewerModel::get_view_info(int64_t index) const {
    if (index < 0 || index >= get_view_count()) {
        return {};
    }
    if (index < get_layer_count()) {
        godot::Dictionary info = world_->get_layer_info(index);
        info["kind"] = "layer";
        const std::string name = to_utf8(info["name"]);
        info["group"] = to_godot(worldgen::inspect::layer_group_title(worldgen::inspect::layer_group(name)));
        info["unit"] = to_godot(worldgen::inspect::layer_unit(name));
        const godot::Ref<WorldLayer> layer = get_layer(index);
        info["palette"] = layer.is_valid() ? layer->get_palette() : godot::String();
        return info;
    }
    const auto composite = composite_of(index);
    godot::Dictionary info;
    info["kind"] = "composite";
    info["group"] = to_godot(worldgen::inspect::layer_group_title(worldgen::inspect::LayerGroup::composites));
    info["unit"] = "";
    info["name"] = composite ? to_godot(worldgen::inspect::composite_name(*composite)) : godot::String("territory");
    const godot::Dictionary elevation = world_->get_layer_info(world_->find_layer("elevation"));
    info["width"] = elevation.get("width", 0);
    info["height"] = elevation.get("height", 0);
    info["element_type"] = "composite";
    info["palette"] = "composite";
    info["codec"] = "";
    info["encoded_size"] = 0;
    return info;
}

bool ViewerModel::decode_layer(int64_t layer_index) {
    const auto i = static_cast<std::size_t>(layer_index);
    if (layers_[i].is_valid()) {
        return true;
    }
    const godot::String name = world_->get_layer_info(layer_index)["name"];
    godot::Ref<WorldLayer> layer = world_->read_layer(name);
    if (layer.is_null()) {
        error_code_ = world_->get_error_code();
        error_message_ = world_->get_error_message();
        return false;
    }
    layers_[i] = layer;
    return true;
}

bool ViewerModel::build_image(int64_t view_index) {
    const auto v = static_cast<std::size_t>(view_index);
    if (images_[v].is_valid()) {
        return true;
    }
    if (view_index < get_layer_count()) {
        if (!decode_layer(view_index)) {
            return false;
        }
        godot::Ref<godot::Image> image = layers_[v]->to_image();
        if (image.is_null()) {
            error_code_ = "not_displayable";
            error_message_ = "Layer " + layers_[v]->get_layer_name() + " cannot be shown as an image";
            return false;
        }
        images_[v] = image;
        return true;
    }

    if (!composite_of(view_index)) {
        return build_territory_image(view_index);
    }
    // A composite: decode the layers it is built from (kept for reuse).
    const auto composite = *composite_of(view_index);
    const godot::String name = to_godot(worldgen::inspect::composite_name(composite));
    CompositeLayers parts;
    bool failed = false;
    const auto grid = [&](std::string_view layer_name) -> const worldgen::db::LayerGrid* {
        const int64_t index = world_->find_layer(layer_name);
        if (index < 0 || failed) {
            return nullptr;
        }
        if (!decode_layer(index)) {
            failed = true;
            return nullptr;
        }
        return &layers_[static_cast<std::size_t>(index)]->layer().grid;
    };
    parts.elevation = grid("elevation");
    if (composite == worldgen::inspect::Composite::biomes || composite == worldgen::inspect::Composite::regions) {
        parts.biome = grid("biome");
    }
    if (composite == worldgen::inspect::Composite::regions) {
        parts.region = grid("region");
    }
    parts.water = grid("water");
    parts.river = grid("river");
    parts.flow_dir = grid("flow_dir");
    if (failed) {
        return false;  // error_* set by decode_layer
    }
    godot::String problem;
    godot::Ref<godot::Image> image = composite_image(composite, parts, world_->biome_colours(), problem);
    if (image.is_null()) {
        error_code_ = "not_displayable";
        error_message_ = "Composite " + name + " cannot be shown" + (problem.is_empty() ? "" : ": " + problem);
        return false;
    }
    images_[v] = image;
    return true;
}

bool ViewerModel::select_view(int64_t index) {
    if (index < 0 || index >= get_view_count()) {
        return false;
    }
    if (!build_image(index)) {
        emit_signal("view_failed", index, error_code_, error_message_);
        return false;
    }
    if (index == get_territory_view()) {
        redraw_territory();  // P4-03 5.3: drawn at the current Y if the holders differ
    }
    error_code_ = godot::String();
    error_message_ = godot::String();
    selected_ = index;
    if (index < get_layer_count()) {
        const godot::Ref<WorldLayer> layer = get_layer(index);
        legend_ = legend_array(worldgen::inspect::layer_legend(layer->layer().name, layer->palette(),
                                                               layer->layer().grid, world_->biome_records()));
    } else if (const auto composite = composite_of(index)) {
        legend_ = legend_array(worldgen::inspect::composite_legend(*composite, world_->biome_records()));
    } else {
        legend_ = legend_array(history_.legend(world_->biome_records()), &HistoryState::legend_tooltip);
    }
    emit_signal("view_selected", index);
    return true;
}

int64_t ViewerModel::get_selected_view() const {
    return selected_;
}

godot::Ref<WorldLayer> ViewerModel::get_layer(int64_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= layers_.size()) {
        return {};
    }
    return layers_[static_cast<std::size_t>(index)];
}

godot::Ref<godot::Image> ViewerModel::get_view_image(int64_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= images_.size()) {
        return {};
    }
    return images_[static_cast<std::size_t>(index)];
}

godot::String ViewerModel::get_cell_text(int64_t x, int64_t y) const {
    if (selected_ < 0) {
        return {};
    }
    if (selected_ < get_layer_count()) {
        const godot::Ref<WorldLayer> layer = get_layer(selected_);
        return layer.is_valid() ? layer->get_cell_text(x, y) : godot::String();
    }
    const godot::Ref<WorldLayer> elevation = get_layer(world_->find_layer("elevation"));
    if (elevation.is_null()) {
        return {};
    }
    const godot::String value = elevation->get_cell_text(x, y);
    return value.is_empty() ? value : "elevation " + value;
}

const worldgen::db::LayerGrid* ViewerModel::layer_grid(std::string_view name) {
    const int64_t index = world_->find_layer(name);
    if (index < 0 || !decode_layer(index)) {
        return nullptr;
    }
    return &layers_[static_cast<std::size_t>(index)]->layer().grid;
}

godot::String ViewerModel::describe_cell(int64_t x, int64_t y) {
    if (selected_ < 0 || x < 0 || y < 0 || x >= (int64_t{1} << 32) || y >= (int64_t{1} << 32)) {
        return {};
    }
    const auto cx = static_cast<std::uint32_t>(x);
    const auto cy = static_cast<std::uint32_t>(y);
    const godot::String saved_code = error_code_;  // a failed lazy decode is not the view's error
    const godot::String saved_message = error_message_;

    const bool is_layer = selected_ < get_layer_count();
    // The layer whose value is shown: the view's own, or for a composite
    // the elevation.
    const std::string value_layer = is_layer ? get_layer(selected_)->layer().name : std::string("elevation");
    const worldgen::db::LayerGrid* value_grid = layer_grid(value_layer);
    if (value_grid == nullptr || cx >= value_grid->width || cy >= value_grid->height) {
        error_code_ = saved_code;
        error_message_ = saved_message;
        return {};
    }

    std::string text = "x " + std::to_string(cx) + ", y " + std::to_string(cy) + "   " + value_layer + " ";
    std::int64_t value = 0;
    if (worldgen::inspect::cell_value(*value_grid, cx, cy, value)) {
        text += worldgen::inspect::format_layer_value(value_layer, value);
    } else {
        text += to_utf8(get_layer(world_->find_layer(value_layer))->get_cell_text(x, y));
    }

    std::int64_t biome = 0;
    const worldgen::db::LayerGrid* biome_grid = layer_grid("biome");
    if (biome_grid != nullptr && worldgen::inspect::cell_value(*biome_grid, cx, cy, biome)) {
        const worldgen::inspect::BiomeRecord* record = world_->find_biome(biome);
        text += "   |   biome " + (record != nullptr ? record->name : "code " + std::to_string(biome));
    }
    std::int64_t region = 0;
    const worldgen::db::LayerGrid* region_grid = layer_grid("region");
    if (region_grid != nullptr && worldgen::inspect::cell_value(*region_grid, cx, cy, region)) {
        if (region == 0) {
            text += "   |   no region (ocean)";
        } else {
            // P4-01 r3 16.5: the name in force at the year shown; the
            // geographic name (region.name) without history.
            const std::optional<std::string> named = history_.region_name(region);
            const std::string name = named ? *named : to_utf8(world_->get_region_name(region));
            text += "   |   region " + std::to_string(region) + (name.empty() ? std::string() : " " + name);
            text += history_.holding_text(region);  // P4-03 6.1; empty without history
        }
    }
    error_code_ = saved_code;
    error_message_ = saved_message;
    return to_godot(text);
}

godot::Array ViewerModel::get_legend() const {
    return legend_;
}

godot::String ViewerModel::get_legend_title() const {
    if (selected_ < 0) {
        return {};
    }
    const godot::Dictionary info = get_view_info(selected_);
    const godot::String name = info["name"];
    if (info["kind"] == godot::Variant("composite")) {
        if (const auto composite = composite_of(selected_)) {
            return name + godot::String(": ") + to_godot(worldgen::inspect::composite_description(*composite));
        }
        return name + godot::String(": holders in year ") + godot::String::num_int64(history_.year()) +
               godot::String("; free land as in regions");
    }
    const godot::String unit = info.get("unit", "");
    // The unit when the catalogue gives one; otherwise the palette.
    return unit.is_empty() ? name + godot::String(" (palette ") + godot::String(info["palette"]) + godot::String(")")
                           : name + godot::String(": ") + unit;
}

godot::Ref<WorldFile> ViewerModel::get_world() const {
    return world_;
}

std::optional<worldgen::inspect::Composite> ViewerModel::composite_of(int64_t view_index) const {
    const int64_t i = view_index - get_layer_count();
    if (i < 0 || static_cast<std::size_t>(i) >= composites_.size()) {
        return std::nullopt;
    }
    return composites_[static_cast<std::size_t>(i)];
}

int64_t ViewerModel::get_territory_view() const {
    return !composites_.empty() && !composites_.front() ? get_layer_count() : -1;
}

bool ViewerModel::build_territory_image(int64_t view_index) {
    const int64_t start = now_usec();
    bool failed = false;
    const auto grid = [&](std::string_view layer_name) -> const worldgen::db::LayerGrid* {
        const int64_t index = world_->find_layer(layer_name);
        if (index < 0 || failed) {
            return nullptr;
        }
        if (!decode_layer(index)) {
            failed = true;
            return nullptr;
        }
        return &layers_[static_cast<std::size_t>(index)]->layer().grid;
    };
    TerritoryLayers parts;
    parts.elevation = grid("elevation");
    parts.biome = grid("biome");
    parts.region = grid("region");
    parts.water = grid("water");
    parts.river = grid("river");
    parts.flow_dir = grid("flow_dir");
    if (failed) {
        return false;  // error_* set by decode_layer
    }
    godot::Ref<godot::Image> image;
    try {
        if (const auto problem = history_.build_base(parts, world_->biome_colours())) {
            error_code_ = "not_displayable";
            error_message_ = "Composite territory cannot be shown: " + to_godot(*problem);
            return false;
        }
        history_.forget_drawn();
        image = make_rgb_image(history_.base_width(), history_.base_height(), "Composite territory",
                               [this](std::span<std::uint8_t> out) { history_.draw(out); });
    } catch (const std::bad_alloc&) {
        error_code_ = "out_of_memory";
        error_message_ = "Out of memory while building the territory image";
        return false;
    } catch (const std::exception& e) {
        error_code_ = "not_displayable";
        error_message_ = godot::String("Composite territory cannot be shown: ") + e.what();
        return false;
    }
    if (image.is_null()) {
        error_code_ = "not_displayable";
        error_message_ = "Composite territory cannot be shown";
        return false;
    }
    images_[static_cast<std::size_t>(view_index)] = image;
    timings_.first_image_usec = now_usec() - start;
    return true;
}

void ViewerModel::redraw_territory() {
    const int64_t view = get_territory_view();
    if (view < 0 || !history_.has_base() || !history_.needs_draw()) {
        return;
    }
    const godot::Ref<godot::Image> image = images_[static_cast<std::size_t>(view)];
    if (image.is_null()) {
        return;  // drawn on its next selection
    }
    const std::uint32_t width = history_.base_width();
    const std::uint32_t height = history_.base_height();
    if (image->get_format() != godot::Image::FORMAT_RGB8 || image->has_mipmaps() ||
        image->get_width() != static_cast<int32_t>(width) || image->get_height() != static_cast<int32_t>(height)) {
        godot::UtilityFunctions::push_error("ViewerModel: the territory image does not match its base");
        return;
    }
    const int64_t start = now_usec();
    const auto bytes = static_cast<std::size_t>(width) * height * 3U;
    try {
        history_.draw(std::span<std::uint8_t>(image->ptrw(), bytes));
    } catch (const std::exception& e) {
        godot::UtilityFunctions::push_error(godot::String("ViewerModel: cannot redraw the territory image: ") +
                                            e.what());
        return;
    }
    const int64_t took = now_usec() - start;
    ++timings_.redraws;
    timings_.slowest_redraw_usec = std::max(timings_.slowest_redraw_usec, took);
    emit_signal("view_image_changed", view);
}

godot::String ViewerModel::get_history_status() const {
    switch (history_.status()) {
        case HistoryStatus::loaded:
            return "loaded";
        case HistoryStatus::failed:
            return "failed";
        case HistoryStatus::none:
            break;
    }
    return "none";
}

bool ViewerModel::has_history() const {
    return history_.loaded();
}

godot::String ViewerModel::get_history_error_code() const {
    return history_.status() == HistoryStatus::failed ? to_godot(history_.error_code()) : godot::String();
}

godot::String ViewerModel::get_history_error_message() const {
    return history_.status() == HistoryStatus::failed ? to_godot(history_.error_message()) : godot::String();
}

int64_t ViewerModel::get_end_year() const {
    return history_.end_year();
}

int64_t ViewerModel::get_year() const {
    return history_.year();
}

bool ViewerModel::set_year(int64_t year) {
    if (!history_.valid_year(year)) {
        return false;
    }
    if (year == history_.year()) {
        return true;
    }
    try {
        history_.set_year(year);
        redraw_territory();
        update_markers();  // P4-04 8.1: after the image, before the legend
        if (selected_ >= 0 && selected_ == get_territory_view()) {
            legend_ = legend_array(history_.legend(world_->biome_records()), &HistoryState::legend_tooltip);
        }
    } catch (const std::exception& e) {
        godot::UtilityFunctions::push_error(godot::String("ViewerModel.set_year: ") + e.what());
        return false;
    }
    emit_signal("year_changed", year);
    return true;
}

bool ViewerModel::step_year(int64_t delta) {
    if (!history_.loaded()) {
        return false;
    }
    const int64_t y = history_.year();
    const int64_t end = history_.end_year();
    int64_t target = y;
    if (delta > 0) {
        target = delta > end - y ? end : y + delta;
    } else if (delta < 0) {
        target = delta < -y ? 0 : y + delta;
    }
    return set_year(target);
}

bool ViewerModel::step_event(int64_t direction) {
    std::optional<int64_t> target;
    try {
        if (direction == -1) {
            target = history_.previous_event_year();
        } else if (direction == 1) {
            target = history_.next_event_year();
        }
    } catch (const std::exception& e) {
        godot::UtilityFunctions::push_error(godot::String("ViewerModel.step_event: ") + e.what());
        return false;
    }
    return target ? set_year(*target) : false;
}

bool ViewerModel::step_holder_change(int64_t direction) {
    std::optional<int64_t> target;
    if (direction == -1) {
        target = history_.previous_change_year();
    } else if (direction == 1) {
        target = history_.next_change_year();
    }
    return target ? set_year(*target) : false;
}

bool ViewerModel::has_previous_event() const {
    try {
        return history_.previous_event_year().has_value();
    } catch (const std::exception&) {
        return false;
    }
}

bool ViewerModel::has_next_event() const {
    try {
        return history_.next_event_year().has_value();
    } catch (const std::exception&) {
        return false;
    }
}

bool ViewerModel::has_previous_change() const {
    return history_.previous_change_year().has_value();
}

bool ViewerModel::has_next_change() const {
    return history_.next_change_year().has_value();
}

godot::Dictionary ViewerModel::get_year_info() const {
    godot::Dictionary info;
    if (!history_.loaded()) {
        return info;
    }
    const auto& state = history_.state();
    const int64_t y = history_.year();
    const int64_t end = history_.end_year();
    info["year"] = y;
    info["end_year"] = end;
    info["present"] = y == end;
    info["age"] = state.age;
    const worldgen::db::AgeRow* age = history_.age(state.age);
    info["age_name"] = age != nullptr ? to_godot(age->name) : godot::String();
    info["phase"] = state.phase;
    const worldgen::db::PhaseRow* phase = history_.phase(state.phase);
    info["phase_kind"] = phase != nullptr ? to_godot(worldgen::history::phase_kind_word(phase->kind)) : godot::String();
    info["phase_start"] = phase != nullptr ? phase->start_year : int64_t{-1};
    info["phase_end"] = phase != nullptr ? phase->end_year : int64_t{-1};
    info["realms"] = static_cast<int64_t>(state.civilizations.size());
    info["regions_held"] = history_.regions_held();
    info["settlements"] = history_.settlements_standing();
    info["wars"] = history_.wars();
    info["events"] = static_cast<int64_t>(state.events.size());
    info["events_starting"] = static_cast<int64_t>(state.events_starting.size());
    return info;
}

godot::Array ViewerModel::get_year_marks() const {
    godot::Array out;
    for (const YearMark& mark : history_.year_marks()) {
        godot::Dictionary row;
        row["year"] = mark.year;
        row["kind"] = to_godot(mark.kind);
        row["label"] = to_godot(mark.label);
        out.push_back(row);
    }
    return out;
}

godot::Dictionary ViewerModel::get_region_holding(int64_t region) const {
    godot::Dictionary out;
    const auto holding = history_.holding(region);
    if (!holding) {
        return out;
    }
    out["region"] = holding->region;
    out["civilization"] = holding->civ;
    const worldgen::db::CivilizationRow* civ = holding->civ != 0 ? history_.civilization(holding->civ) : nullptr;
    out["name"] = civ != nullptr ? to_godot(civ->name) : godot::String();
    out["since"] = holding->since;
    out["event"] = holding->event;
    return out;
}

godot::Dictionary ViewerModel::get_civilization_info(int64_t civilization) const {
    godot::Dictionary out;
    const worldgen::db::CivilizationRow* civ = history_.civilization(civilization);
    if (civ == nullptr) {
        return out;
    }
    const worldgen::db::RaceRow* race = history_.race(civ->race);
    out["id"] = civ->id;
    out["name"] = to_godot(civ->name);
    out["race"] = civ->race;
    out["race_key"] = race != nullptr ? to_godot(race->key) : godot::String();
    out["race_name"] = race != nullptr ? to_godot(race->name) : godot::String();
    out["race_plural"] = race != nullptr ? to_godot(race->plural) : godot::String();
    out["colour"] = history_.colour_of(civ->id);
    out["founded_year"] = civ->founded_year;
    out["ended_year"] = civ->ended_year ? *civ->ended_year : int64_t{-1};
    return out;
}

godot::PackedInt64Array ViewerModel::get_civilizations_alive() const {
    godot::PackedInt64Array out;
    if (history_.loaded()) {
        for (const int64_t id : history_.state().civilizations) {
            out.push_back(id);
        }
    }
    return out;
}

// --- Markers (P4-04 8.1) -----------------------------------------------------

namespace {

using worldgen::inspect::MarkerType;

constexpr int64_t FADED_FILL = 0x969696;    // MARKER_FADED
constexpr int64_t NO_OWNER_FILL = 0x808080;  // MARKER_NO_OWNER
constexpr int64_t LEGEND_GREY = 0xA0A0A0;    // the key list's icon fill (8.6)

int64_t rgb_of(worldgen::inspect::Rgb c) {
    return (int64_t{c.r} << 16) | (int64_t{c.g} << 8) | int64_t{c.b};
}

const char* marker_type_word(MarkerType type) {
    switch (type) {
        case MarkerType::pass: return "pass";
        case MarkerType::place: return "place";
        case MarkerType::faded: return "faded";
        case MarkerType::ruin: return "ruin";
        case MarkerType::standing: return "standing";
        case MarkerType::site: return "site";
        case MarkerType::figure: return "figure";
    }
    return "";
}

// The draw pass of a settlement entry (6.1: 3 faded, 4 ruin, 5 standing, 6
// capital); 0 for a status outside 1 to 4.
int settlement_pass(const worldgen::inspect::SettlementMarker& m) {
    if (m.status == 4) {
        return 3;
    }
    if (m.status == 3) {
        return 4;
    }
    if (m.status == 1 || m.status == 2) {
        return m.capital ? 6 : 5;
    }
    return 0;
}

bool option_ref(worldgen::inspect::MarkerOptions& options, const godot::String& name, bool*& out) {
    if (name == "faded") {
        out = &options.faded;
    } else if (name == "events") {
        out = &options.events;
    } else if (name == "passes") {
        out = &options.passes;
    } else {
        return false;
    }
    return true;
}

}  // namespace

bool ViewerModel::get_markers_available() const {
    return history_.markers_available();
}

godot::String ViewerModel::get_markers_error_code() const {
    return to_godot(history_.markers_error_code());
}

godot::String ViewerModel::get_markers_error_message() const {
    return to_godot(history_.markers_error_message());
}

bool ViewerModel::get_markers_enabled() const {
    return history_.markers_enabled();
}

bool ViewerModel::set_markers_enabled(bool on) {
    if (!history_.markers_available()) {
        return false;
    }
    if (on != history_.markers_enabled()) {
        history_.set_markers_enabled(on);
        update_markers();
    }
    return true;
}

bool ViewerModel::get_marker_option(const godot::String& name) const {
    if (!history_.markers_available()) {
        return false;
    }
    worldgen::inspect::MarkerOptions options = history_.marker_options();
    bool* value = nullptr;
    return option_ref(options, name, value) && *value;
}

bool ViewerModel::set_marker_option(const godot::String& name, bool on) {
    if (!history_.markers_available()) {
        return false;
    }
    worldgen::inspect::MarkerOptions options = history_.marker_options();
    bool* value = nullptr;
    if (!option_ref(options, name, value)) {
        return false;
    }
    if (*value != on) {
        *value = on;
        history_.set_marker_options(options);
        update_markers();
    }
    return true;
}

void ViewerModel::update_markers() {
    bool changed = false;
    try {
        changed = history_.update_markers();
    } catch (const std::exception& e) {
        godot::UtilityFunctions::push_error(godot::String("ViewerModel: cannot compute the markers: ") + e.what());
        return;
    }
    const int64_t took = history_.last_marker_step_nsec();
    ++marker_timings_.steps;
    marker_timings_.total_nsec += took;
    marker_timings_.slowest_nsec = std::max(marker_timings_.slowest_nsec, took);
    if (changed) {
        rebuild_marker_order();
        ++marker_timings_.changes;
        emit_signal("markers_changed");
    }
}

void ViewerModel::rebuild_marker_order() {
    const worldgen::inspect::YearMarkers& m = history_.markers();
    marker_order_.clear();
    for (std::size_t i = 0; i < m.passes.size(); ++i) {
        marker_order_.push_back(MarkerRef{MarkerType::pass, i});
    }
    for (std::size_t i = 0; i < m.places.size(); ++i) {
        marker_order_.push_back(MarkerRef{MarkerType::place, i});
    }
    for (int pass = 3; pass <= 6; ++pass) {
        for (std::size_t i = 0; i < m.settlements.size(); ++i) {
            if (settlement_pass(m.settlements[i]) != pass) {
                continue;
            }
            const MarkerType type = pass == 3 ? MarkerType::faded : pass == 4 ? MarkerType::ruin : MarkerType::standing;
            marker_order_.push_back(MarkerRef{type, i});
        }
    }
    markers_array_ = godot::Array();
    for (const MarkerRef& ref : marker_order_) {
        markers_array_.push_back(marker_dictionary(ref));
    }
}

godot::Dictionary ViewerModel::marker_dictionary(const MarkerRef& ref) const {
    const worldgen::inspect::YearMarkers& m = history_.markers();
    godot::Dictionary out;
    out["type"] = marker_type_word(ref.type);
    if (ref.type == MarkerType::pass) {
        const auto& p = m.passes[ref.index];
        out["id"] = p.pass;
        out["x"] = p.x;
        out["y"] = p.y;
        out["glyph"] = "pass";
        out["fill"] = int64_t{-1};
        out["capital"] = false;
        out["small"] = false;
        return out;
    }
    if (ref.type == MarkerType::place) {
        const auto& p = m.places[ref.index];
        out["id"] = p.events.empty() ? int64_t{0} : p.events.front();
        out["x"] = p.x;
        out["y"] = p.y;
        out["glyph"] = "ring";
        out["fill"] = rgb_of(worldgen::inspect::event_symbol_colour(p.symbol));
        out["capital"] = false;
        out["small"] = false;
        godot::PackedInt64Array events;
        for (const int64_t e : p.events) {
            events.push_back(e);
        }
        out["events"] = events;
        return out;
    }
    const auto& s = m.settlements[ref.index];
    out["id"] = s.settlement;
    out["x"] = s.x;
    out["y"] = s.y;
    out["glyph"] = to_godot(marker_glyph_name(worldgen::inspect::glyph_of(s)));
    int64_t fill = -1;
    if (ref.type == MarkerType::faded) {
        fill = FADED_FILL;
    } else if (ref.type == MarkerType::standing) {
        fill = s.owner == 0 ? NO_OWNER_FILL : history_.colour_of(s.owner);
    }
    out["fill"] = fill;
    out["capital"] = s.capital;
    // 8.1 and 8.4: hamlets and villages that are not capitals, ruins and
    // faded ruins; a capital is never small.
    out["small"] = ref.type != MarkerType::standing || (!s.capital && (s.kind == 1 || s.kind == 2));
    return out;
}

godot::Array ViewerModel::get_markers() const {
    return markers_array_;
}

godot::Dictionary ViewerModel::get_marker_glyph(const godot::String& name) {
    godot::Dictionary out;
    const std::string key_name = to_utf8(name);
    auto it = glyphs_.find(key_name);
    if (it == glyphs_.end()) {
        const auto key = marker_glyph_key(key_name);
        if (!key) {
            return out;
        }
        try {
            it = glyphs_.emplace(key_name, worldgen::inspect::marker_glyph(*key)).first;
        } catch (const std::exception& e) {
            godot::UtilityFunctions::push_error(godot::String("ViewerModel: no glyph ") + name + ": " + e.what());
            return out;
        }
    }
    godot::PackedByteArray cells;
    cells.resize(static_cast<int64_t>(it->second.cells.size()));
    std::copy(it->second.cells.begin(), it->second.cells.end(), cells.ptrw());
    out["radius"] = int64_t{it->second.radius};
    out["cells"] = cells;
    return out;
}

godot::String ViewerModel::describe_marker(int64_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= marker_order_.size() || !history_.loaded()) {
        return {};
    }
    const MarkerRef& ref = marker_order_[static_cast<std::size_t>(index)];
    const worldgen::inspect::YearMarkers& m = history_.markers();
    const worldgen::history::HistoryView& view = history_.history().view;
    using worldgen::history::EntityType;
    const auto sentence = [&view](int64_t event) -> std::string {
        try {
            return worldgen::history::render_event(view, event);
        } catch (const std::exception& e) {
            return std::string("(") + e.what() + ")";
        }
    };
    std::string text;
    if (ref.type == MarkerType::pass) {
        const auto& p = m.passes[ref.index];
        const worldgen::inspect::MarkerIndex* index_ptr = history_.marker_index();
        int64_t col_dm = 0;
        int64_t kind = p.kind;
        if (index_ptr != nullptr) {
            for (const auto& row : index_ptr->passes()) {
                if (row.id == p.pass) {
                    col_dm = row.col_dm;
                    kind = row.kind;
                }
            }
        }
        const int64_t magnitude = col_dm < 0 ? -col_dm : col_dm;
        text = "pass " + std::to_string(p.pass) + ": " + (kind == 1 ? "lowered pass" : "natural gap") + ", col " +
               (col_dm < 0 ? "-" : "") + std::to_string(magnitude / 10) + "." + std::to_string(magnitude % 10) + " m";
    } else if (ref.type == MarkerType::place) {
        const auto& p = m.places[ref.index];
        text = "In " + std::to_string(history_.year()) + ":";
        for (const int64_t e : p.events) {
            text += " " + sentence(e);
        }
    } else {
        const auto& s = m.settlements[ref.index];
        const std::string* name = view.name(EntityType::settlement, s.settlement);
        text = name != nullptr ? *name : "settlement " + std::to_string(s.settlement);
        const std::string kind(worldgen::history::settlement_kind_word(s.kind));
        if (ref.type == MarkerType::standing) {
            text += ", " + kind + (s.capital ? " (capital)" : "");
            if (s.owner != 0) {
                const std::string* owner = view.name(EntityType::civilization, s.owner);
                text += " of " + (owner != nullptr ? *owner : "civilization " + std::to_string(s.owner));
            }
            text += ", " + std::string(worldgen::history::settlement_status_word(s.status)) + ", population " +
                    std::to_string(s.population) + " recorded in " + std::to_string(s.since) + ", founded in " +
                    std::to_string(s.founded);
        } else {
            text += std::string(ref.type == MarkerType::faded ? ", faded ruin of a " : ", ruin of a ") + kind +
                    ", ruined in " + std::to_string(s.ruined_year);
            if (s.ruin_event != 0) {
                text += ": " + sentence(s.ruin_event);
            }
        }
    }
    return to_godot(text);
}

godot::Array ViewerModel::get_marker_legend() const {
    godot::Array out;
    if (!history_.markers_available() || !history_.markers_enabled()) {
        return out;
    }
    const auto row = [&out](const char* label, const char* glyph, int64_t colour) {
        godot::Dictionary r;
        r["label"] = label;
        r["glyph"] = glyph;
        godot::PackedInt32Array colours;
        if (colour >= 0) {
            colours.push_back(static_cast<int32_t>(colour));
        }
        r["colours"] = colours;
        out.push_back(r);
    };
    for (int64_t kind = 1; kind <= 5; ++kind) {
        const std::string word(worldgen::history::settlement_kind_word(kind));
        row(word.c_str(), word.c_str(), -1);
    }
    row("capital", "town capital", -1);
    row("declining", "village declining", -1);
    row("ruin", "ruin", -1);
    const worldgen::inspect::MarkerOptions& options = history_.marker_options();
    if (options.faded) {
        row("faded ruin", "faded", -1);
    }
    if (options.events) {
        using worldgen::inspect::EventSymbol;
        using worldgen::inspect::event_symbol_colour;
        row("sack or capture", "ring", rgb_of(event_symbol_colour(EventSymbol::sack)));
        row("siege", "ring", rgb_of(event_symbol_colour(EventSymbol::siege)));
        row("battle", "ring", rgb_of(event_symbol_colour(EventSymbol::battle)));
        row("plague", "ring", rgb_of(event_symbol_colour(EventSymbol::plague)));
        row("other event", "ring", rgb_of(event_symbol_colour(EventSymbol::plain)));
    }
    if (options.passes) {
        row("pass", "pass", -1);
    }
    return out;
}

int64_t ViewerModel::marker_legend_grey() {
    return LEGEND_GREY;
}

void ViewerModel::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    using godot::MethodInfo;
    using godot::PropertyInfo;
    using godot::Variant;

    ClassDB::bind_method(D_METHOD("open_world", "path"), &ViewerModel::open_world);
    ClassDB::bind_method(D_METHOD("close_world"), &ViewerModel::close_world);
    ClassDB::bind_method(D_METHOD("has_world"), &ViewerModel::has_world);
    ClassDB::bind_method(D_METHOD("get_world_path"), &ViewerModel::get_world_path);
    ClassDB::bind_method(D_METHOD("get_error_code"), &ViewerModel::get_error_code);
    ClassDB::bind_method(D_METHOD("get_error_message"), &ViewerModel::get_error_message);
    ClassDB::bind_method(D_METHOD("get_meta_entries"), &ViewerModel::get_meta_entries);
    ClassDB::bind_method(D_METHOD("get_layer_count"), &ViewerModel::get_layer_count);
    ClassDB::bind_method(D_METHOD("get_composite_count"), &ViewerModel::get_composite_count);
    ClassDB::bind_method(D_METHOD("get_view_count"), &ViewerModel::get_view_count);
    ClassDB::bind_method(D_METHOD("get_view_info", "index"), &ViewerModel::get_view_info);
    ClassDB::bind_method(D_METHOD("select_view", "index"), &ViewerModel::select_view);
    ClassDB::bind_method(D_METHOD("get_selected_view"), &ViewerModel::get_selected_view);
    ClassDB::bind_method(D_METHOD("get_view_image", "index"), &ViewerModel::get_view_image);
    ClassDB::bind_method(D_METHOD("get_layer", "index"), &ViewerModel::get_layer);
    ClassDB::bind_method(D_METHOD("get_cell_text", "x", "y"), &ViewerModel::get_cell_text);
    ClassDB::bind_method(D_METHOD("describe_cell", "x", "y"), &ViewerModel::describe_cell);
    ClassDB::bind_method(D_METHOD("get_legend"), &ViewerModel::get_legend);
    ClassDB::bind_method(D_METHOD("get_legend_title"), &ViewerModel::get_legend_title);
    ClassDB::bind_method(D_METHOD("get_world"), &ViewerModel::get_world);
    ClassDB::bind_method(D_METHOD("get_history_status"), &ViewerModel::get_history_status);
    ClassDB::bind_method(D_METHOD("has_history"), &ViewerModel::has_history);
    ClassDB::bind_method(D_METHOD("get_history_error_code"), &ViewerModel::get_history_error_code);
    ClassDB::bind_method(D_METHOD("get_history_error_message"), &ViewerModel::get_history_error_message);
    ClassDB::bind_method(D_METHOD("get_end_year"), &ViewerModel::get_end_year);
    ClassDB::bind_method(D_METHOD("get_year"), &ViewerModel::get_year);
    ClassDB::bind_method(D_METHOD("set_year", "year"), &ViewerModel::set_year);
    ClassDB::bind_method(D_METHOD("step_year", "delta"), &ViewerModel::step_year);
    ClassDB::bind_method(D_METHOD("step_event", "direction"), &ViewerModel::step_event);
    ClassDB::bind_method(D_METHOD("step_holder_change", "direction"), &ViewerModel::step_holder_change);
    ClassDB::bind_method(D_METHOD("has_previous_event"), &ViewerModel::has_previous_event);
    ClassDB::bind_method(D_METHOD("has_next_event"), &ViewerModel::has_next_event);
    ClassDB::bind_method(D_METHOD("has_previous_change"), &ViewerModel::has_previous_change);
    ClassDB::bind_method(D_METHOD("has_next_change"), &ViewerModel::has_next_change);
    ClassDB::bind_method(D_METHOD("get_year_info"), &ViewerModel::get_year_info);
    ClassDB::bind_method(D_METHOD("get_year_marks"), &ViewerModel::get_year_marks);
    ClassDB::bind_method(D_METHOD("get_region_holding", "region"), &ViewerModel::get_region_holding);
    ClassDB::bind_method(D_METHOD("get_civilization_info", "civilization"), &ViewerModel::get_civilization_info);
    ClassDB::bind_method(D_METHOD("get_civilizations_alive"), &ViewerModel::get_civilizations_alive);
    ClassDB::bind_method(D_METHOD("get_territory_view"), &ViewerModel::get_territory_view);
    ClassDB::bind_method(D_METHOD("get_markers_available"), &ViewerModel::get_markers_available);
    ClassDB::bind_method(D_METHOD("get_markers_error_code"), &ViewerModel::get_markers_error_code);
    ClassDB::bind_method(D_METHOD("get_markers_error_message"), &ViewerModel::get_markers_error_message);
    ClassDB::bind_method(D_METHOD("get_markers_enabled"), &ViewerModel::get_markers_enabled);
    ClassDB::bind_method(D_METHOD("set_markers_enabled", "on"), &ViewerModel::set_markers_enabled);
    ClassDB::bind_method(D_METHOD("get_marker_option", "name"), &ViewerModel::get_marker_option);
    ClassDB::bind_method(D_METHOD("set_marker_option", "name", "on"), &ViewerModel::set_marker_option);
    ClassDB::bind_method(D_METHOD("get_markers"), &ViewerModel::get_markers);
    ClassDB::bind_method(D_METHOD("get_marker_glyph", "name"), &ViewerModel::get_marker_glyph);
    ClassDB::bind_method(D_METHOD("describe_marker", "index"), &ViewerModel::describe_marker);
    ClassDB::bind_method(D_METHOD("get_marker_legend"), &ViewerModel::get_marker_legend);

    ADD_SIGNAL(MethodInfo("world_opened", PropertyInfo(Variant::STRING, "path")));
    ADD_SIGNAL(MethodInfo("world_open_failed", PropertyInfo(Variant::STRING, "path"),
                          PropertyInfo(Variant::STRING, "code"), PropertyInfo(Variant::STRING, "message")));
    ADD_SIGNAL(MethodInfo("world_closed"));
    ADD_SIGNAL(MethodInfo("view_selected", PropertyInfo(Variant::INT, "index")));
    ADD_SIGNAL(MethodInfo("view_failed", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::STRING, "code"),
                          PropertyInfo(Variant::STRING, "message")));
    ADD_SIGNAL(MethodInfo("year_changed", PropertyInfo(Variant::INT, "year")));
    ADD_SIGNAL(MethodInfo("view_image_changed", PropertyInfo(Variant::INT, "index")));
    ADD_SIGNAL(MethodInfo("markers_changed"));
}

}  // namespace unmapped
