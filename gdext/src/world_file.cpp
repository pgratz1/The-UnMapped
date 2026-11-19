#include "world_file.hpp"

#include <algorithm>
#include <exception>
#include <new>
#include <string>
#include <string_view>
#include <utility>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/char_string.hpp>

#include "worldgen/db/result.hpp"
#include "worldgen/db/schema.hpp"

namespace unmapped {

namespace {

godot::String to_godot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

std::string to_utf8(const godot::String& text) {
    const godot::CharString utf8 = text.utf8();
    return {utf8.get_data(), static_cast<std::size_t>(utf8.length())};
}

}  // namespace

bool WorldFile::open(const godot::String& path) {
    close();
    clear_error();
    path_ = path;
    try {
        auto opened = worldgen::db::WorldReader::open(to_utf8(path));
        if (!opened.ok()) {
            set_error(opened.error());
            return false;
        }
        auto layers = opened.value().layers();
        if (!layers.ok()) {
            set_error(layers.error());
            return false;
        }
        layers_ = std::move(layers.value());
        reader_.emplace(std::move(opened.value()));
        read_biome_table();
        read_region_table();
    } catch (const std::bad_alloc&) {
        set_error("out_of_memory", "Out of memory while opening " + path);
        return false;
    } catch (const std::exception& e) {
        // worldgen raises FatalError only for a contract violation (a bug);
        // report it rather than let it reach the engine.
        set_error("internal_error", godot::String("Internal error while opening ") + path + ": " + e.what());
        return false;
    }
    return true;
}

void WorldFile::read_biome_table() {
    biome_records_.clear();
    biome_colours_.clear();
    biome_table_problem_ = godot::String();
    auto rows = reader_->table_rows(worldgen::db::BIOME_TABLE);
    if (!rows.ok()) {
        // A world before schema 3 has no table: biome cells then draw in
        // worldgen's "unknown" colour, as `worldgen-cli render` does. Any
        // other failure makes the table unusable but leaves the file open.
        if (rows.code() != worldgen::db::ErrorCode::table_not_found) {
            biome_table_problem_ = to_godot(rows.error().message);
        }
        return;
    }
    worldgen::inspect::BiomeTableResult table = worldgen::inspect::read_biome_rows(rows.value());
    if (table.problem) {
        biome_table_problem_ = "corrupt table 'biome': " + to_godot(*table.problem);
        return;
    }
    biome_records_ = std::move(table.records);
    for (const auto& record : biome_records_) {
        biome_colours_.push_back(worldgen::inspect::BiomeColour{record.id, record.colour});
    }
}

void WorldFile::read_region_table() {
    regions_.clear();
    region_table_problem_ = godot::String();
    auto rows = reader_->table_rows(worldgen::db::REGION_TABLE);
    if (!rows.ok()) {
        // Before schema 5 there is no table; the reader refuses such files
        // anyway, but a missing table is not an error here.
        if (rows.code() != worldgen::db::ErrorCode::table_not_found) {
            region_table_problem_ = to_godot(rows.error().message);
        }
        return;
    }
    // worldgen's checks first (column count and types, cells >= 1); its
    // record leaves out the name, which the viewer takes from column 1.
    const worldgen::inspect::RegionTableResult table = worldgen::inspect::read_region_rows(rows.value());
    if (table.problem) {
        region_table_problem_ = "corrupt table 'region': " + to_godot(*table.problem);
        return;
    }
    // P1-06 3.2 column order: id, name, biome, x_min, ..., landmass, cells.
    constexpr std::size_t NAME = 1;
    constexpr std::size_t BIOME = 2;
    constexpr std::size_t CELLS = 10;
    for (const auto& row : rows.value()) {
        regions_.push_back(RegionRow{row[0].integer, to_godot(row[NAME].bytes), row[BIOME].integer,
                                     row[CELLS].integer});
    }
    // Rows come in primary-key order already; keep the lookup independent of it.
    std::sort(regions_.begin(), regions_.end(), [](const RegionRow& a, const RegionRow& b) { return a.id < b.id; });
}

void WorldFile::close() {
    reader_.reset();
    layers_.clear();
    biome_records_.clear();
    biome_colours_.clear();
    biome_table_problem_ = godot::String();
    regions_.clear();
    region_table_problem_ = godot::String();
    path_ = godot::String();
}

bool WorldFile::is_open() const {
    return reader_.has_value();
}

godot::String WorldFile::get_path() const {
    return path_;
}

godot::String WorldFile::get_error_code() const {
    return error_code_;
}

godot::String WorldFile::get_error_message() const {
    return error_message_;
}

godot::Array WorldFile::get_meta_entries() const {
    godot::Array entries;
    if (!reader_) {
        return entries;
    }
    for (const auto& entry : reader_->meta_entries()) {
        godot::Dictionary row;
        row["key"] = to_godot(entry.key);
        row["value"] = to_godot(entry.value);
        entries.push_back(row);
    }
    return entries;
}

godot::String WorldFile::get_meta_value(const godot::String& key) const {
    if (!reader_) {
        return {};
    }
    const auto value = reader_->meta(to_utf8(key));
    return value ? to_godot(*value) : godot::String();
}

int64_t WorldFile::get_layer_count() const {
    return static_cast<int64_t>(layers_.size());
}

godot::PackedStringArray WorldFile::get_layer_names() const {
    godot::PackedStringArray names;
    for (const auto& info : layers_) {
        names.push_back(to_godot(info.name));
    }
    return names;
}

godot::Dictionary WorldFile::get_layer_info(int64_t index) const {
    godot::Dictionary row;
    if (index < 0 || index >= get_layer_count()) {
        return row;
    }
    const auto& info = layers_[static_cast<std::size_t>(index)];
    row["id"] = info.id;
    row["name"] = to_godot(info.name);
    row["width"] = static_cast<int64_t>(info.width);
    row["height"] = static_cast<int64_t>(info.height);
    row["element_type"] = element_type_text(info.type);
    row["codec"] = codec_text(info.codec);
    // A blob is at most a few GiB, far below 2^63.
    row["encoded_size"] = static_cast<int64_t>(info.encoded_size);
    return row;
}

godot::Ref<WorldLayer> WorldFile::read_layer(const godot::String& name) {
    clear_error();
    if (!reader_) {
        set_error("not_open", "No world file is open");
        return {};
    }
    try {
        auto read = reader_->read_layer(to_utf8(name));
        if (!read.ok()) {
            set_error(read.error());
            return {};
        }
        godot::Ref<WorldLayer> layer;
        layer.instantiate();
        layer->set_layer(std::move(read.value()), biome_colours_);
        if (layer->palette() == worldgen::inspect::Palette::biome && !biome_table_problem_.is_empty()) {
            set_error("corrupt_table", "Layer " + name + " needs table 'biome': " + biome_table_problem_);
            return {};
        }
        return layer;
    } catch (const std::bad_alloc&) {
        set_error("out_of_memory", "Out of memory while reading layer " + name);
    } catch (const std::exception& e) {
        set_error("internal_error", godot::String("Internal error while reading layer ") + name + ": " + e.what());
    }
    return {};
}

godot::Array WorldFile::get_biome_entries() const {
    godot::Array entries;
    for (const auto& record : biome_records_) {
        godot::Dictionary row;
        row["id"] = record.id;
        row["key"] = to_godot(record.key);
        row["name"] = to_godot(record.name);
        row["colour"] = record.colour;
        row["water"] = record.water;
        entries.push_back(row);
    }
    return entries;
}

godot::String WorldFile::get_biome_table_problem() const {
    return biome_table_problem_;
}

godot::Array WorldFile::get_region_entries() const {
    godot::Array entries;
    for (const auto& region : regions_) {
        godot::Dictionary row;
        row["id"] = region.id;
        row["name"] = region.name;
        row["biome"] = region.biome;
        row["cells"] = region.cells;
        entries.push_back(row);
    }
    return entries;
}

godot::String WorldFile::get_region_table_problem() const {
    return region_table_problem_;
}

godot::String WorldFile::get_region_name(int64_t id) const {
    const auto it = std::lower_bound(regions_.begin(), regions_.end(), id,
                                     [](const RegionRow& row, int64_t key) { return row.id < key; });
    return it != regions_.end() && it->id == id ? it->name : godot::String();
}

const worldgen::inspect::BiomeRecord* WorldFile::find_biome(std::int64_t id) const noexcept {
    for (const auto& record : biome_records_) {
        if (record.id == id) {
            return &record;
        }
    }
    return nullptr;
}

int64_t WorldFile::find_layer(std::string_view name) const {
    for (std::size_t i = 0; i < layers_.size(); ++i) {
        if (layers_[i].name == name) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
}

std::vector<worldgen::inspect::Composite> WorldFile::composites() const {
    std::vector<worldgen::inspect::Composite> result;
    if (find_layer("elevation") < 0) {
        return result;
    }
    for (const worldgen::inspect::Composite composite : worldgen::inspect::ALL_COMPOSITES) {
        const bool needs_biome = composite == worldgen::inspect::Composite::biomes ||
                                 composite == worldgen::inspect::Composite::regions;
        if (needs_biome && (find_layer("biome") < 0 || !biome_table_problem_.is_empty())) {
            continue;
        }
        if (composite == worldgen::inspect::Composite::regions && find_layer("region") < 0) {
            continue;
        }
        result.push_back(composite);
    }
    return result;
}

godot::PackedStringArray WorldFile::get_composite_names() const {
    godot::PackedStringArray names;
    for (const worldgen::inspect::Composite composite : composites()) {
        names.push_back(to_godot(worldgen::inspect::composite_name(composite)));
    }
    return names;
}

void WorldFile::set_error(const worldgen::db::Error& error) {
    set_error(worldgen::db::error_code_name(error.code), to_godot(error.message));
}

void WorldFile::set_error(const godot::String& code, const godot::String& message) {
    error_code_ = code;
    error_message_ = message;
}

void WorldFile::clear_error() {
    error_code_ = godot::String();
    error_message_ = godot::String();
}

void WorldFile::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    ClassDB::bind_method(D_METHOD("open", "path"), &WorldFile::open);
    ClassDB::bind_method(D_METHOD("close"), &WorldFile::close);
    ClassDB::bind_method(D_METHOD("is_open"), &WorldFile::is_open);
    ClassDB::bind_method(D_METHOD("get_path"), &WorldFile::get_path);
    ClassDB::bind_method(D_METHOD("get_error_code"), &WorldFile::get_error_code);
    ClassDB::bind_method(D_METHOD("get_error_message"), &WorldFile::get_error_message);
    ClassDB::bind_method(D_METHOD("get_meta_entries"), &WorldFile::get_meta_entries);
    ClassDB::bind_method(D_METHOD("get_meta_value", "key"), &WorldFile::get_meta_value);
    ClassDB::bind_method(D_METHOD("get_layer_count"), &WorldFile::get_layer_count);
    ClassDB::bind_method(D_METHOD("get_layer_names"), &WorldFile::get_layer_names);
    ClassDB::bind_method(D_METHOD("get_layer_info", "index"), &WorldFile::get_layer_info);
    ClassDB::bind_method(D_METHOD("read_layer", "name"), &WorldFile::read_layer);
    ClassDB::bind_method(D_METHOD("get_biome_entries"), &WorldFile::get_biome_entries);
    ClassDB::bind_method(D_METHOD("get_biome_table_problem"), &WorldFile::get_biome_table_problem);
    ClassDB::bind_method(D_METHOD("get_region_entries"), &WorldFile::get_region_entries);
    ClassDB::bind_method(D_METHOD("get_region_table_problem"), &WorldFile::get_region_table_problem);
    ClassDB::bind_method(D_METHOD("get_region_name", "id"), &WorldFile::get_region_name);
    ClassDB::bind_method(D_METHOD("get_composite_names"), &WorldFile::get_composite_names);
}

}  // namespace unmapped
