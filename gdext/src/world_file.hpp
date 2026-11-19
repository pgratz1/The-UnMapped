#pragma once

// WorldFile: a world.db opened read-only, bound into Godot.
//
// A thin layer over worldgen::db::WorldReader (spec P0-02 section 9). The
// file is never written. Errors from the reader are kept as a code name
// (e.g. "not_world") and a plain-English message; nothing throws into
// Godot. Every value returned is an integer or text; the decoded layer is a
// WorldLayer, whose to_image() is the only conversion to pixels.
//
// This is the data-access object shared by every screen that reads a world
// (viewer now; create-world screen and in-game map later). Screens hold it
// through their own model (ViewerModel for the viewer).

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "world_layer.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/inspect/geography.hpp"
#include "worldgen/inspect/render.hpp"

namespace unmapped {

class WorldFile : public godot::RefCounted {
    GDCLASS(WorldFile, godot::RefCounted)

public:
    WorldFile() = default;

    // --- Bound to Godot ------------------------------------------------------

    // Opens path (a filesystem path, not res://) read-only, closing any file
    // already open. True on success; on failure get_error_code() and
    // get_error_message() say why, and the WorldFile is closed. The layer
    // list is read here too (headers only, no payload), so a corrupt layer
    // row fails the open.
    bool open(const godot::String& path);
    void close();
    [[nodiscard]] bool is_open() const;
    [[nodiscard]] godot::String get_path() const;

    // The last error: a stable code name from worldgen ("file_unreadable",
    // "not_sqlite", "not_world", "unsupported_schema_version",
    // "meta_key_missing", "meta_value_invalid", "layer_not_found",
    // "corrupt_layer", ...) and a plain-English message naming the file or
    // layer. Both empty after a success.
    [[nodiscard]] godot::String get_error_code() const;
    [[nodiscard]] godot::String get_error_message() const;

    // Meta rows in ascending key order: an Array of Dictionary {key, value}.
    [[nodiscard]] godot::Array get_meta_entries() const;
    // One meta value; empty String when absent.
    [[nodiscard]] godot::String get_meta_value(const godot::String& key) const;

    // The layer list in id order, without decoding (P0-02 9, "Layer list").
    [[nodiscard]] int64_t get_layer_count() const;
    [[nodiscard]] godot::PackedStringArray get_layer_names() const;
    // Dictionary {id, name, width, height, element_type, codec, encoded_size};
    // empty for an index out of range.
    [[nodiscard]] godot::Dictionary get_layer_info(int64_t index) const;

    // Decodes a layer by name. Null on failure (see get_error_*). A layer
    // drawn with the biome palette gets the world's `biome` table (below);
    // if that table is malformed the read fails with "corrupt_table".
    godot::Ref<WorldLayer> read_layer(const godot::String& name);

    // Table `biome` (schema 3, P1-04 9), read once by open(): an Array of
    // Dictionary {id, key, name, colour (0xRRGGBB), water} in id order. Empty
    // when the world has no such table or it is malformed.
    [[nodiscard]] godot::Array get_biome_entries() const;
    // Why table `biome` cannot be used ("" when it is fine or absent).
    [[nodiscard]] godot::String get_biome_table_problem() const;

    // Table `region` (schema 5, P1-06 3.2), read once by open(): an Array of
    // Dictionary {id, name, biome, cells} in id order. Empty when the world
    // has no rows or the table is malformed (then get_region_table_problem()
    // says why).
    [[nodiscard]] godot::Array get_region_entries() const;
    [[nodiscard]] godot::String get_region_table_problem() const;
    // The name of region id (column `name`); empty when there is no such row.
    [[nodiscard]] godot::String get_region_name(int64_t id) const;

    // The composite images worldgen can build from this world's layers, in
    // worldgen's order: "map" when there is an `elevation` layer, "biomes"
    // when there are also a `biome` layer and a usable table `biome` (P1-00
    // 10.2, P1-04 9), "regions" when there is also a `region` layer (P1-06
    // 9, P0-04 amendment 18 (e)). Their layers are checked only when
    // rendered.
    [[nodiscard]] godot::PackedStringArray get_composite_names() const;

    // --- C++ only --------------------------------------------------------------
    [[nodiscard]] const std::vector<worldgen::db::LayerInfo>& layer_infos() const noexcept { return layers_; }
    [[nodiscard]] const std::vector<worldgen::inspect::BiomeColour>& biome_colours() const noexcept {
        return biome_colours_;
    }
    [[nodiscard]] const std::vector<worldgen::inspect::BiomeRecord>& biome_records() const noexcept {
        return biome_records_;
    }
    // The record of biome code id, or null.
    [[nodiscard]] const worldgen::inspect::BiomeRecord* find_biome(std::int64_t id) const noexcept;
    [[nodiscard]] std::vector<worldgen::inspect::Composite> composites() const;
    // Index of the layer called name in layer_infos(), or -1.
    [[nodiscard]] int64_t find_layer(std::string_view name) const;

protected:
    static void _bind_methods();

private:
    void set_error(const worldgen::db::Error& error);
    void set_error(const godot::String& code, const godot::String& message);
    void clear_error();
    void read_biome_table();
    void read_region_table();

    struct RegionRow {
        std::int64_t id = 0;
        godot::String name;
        std::int64_t biome = 0;
        std::int64_t cells = 0;
    };

    std::optional<worldgen::db::WorldReader> reader_;
    std::vector<worldgen::db::LayerInfo> layers_;
    std::vector<worldgen::inspect::BiomeRecord> biome_records_;
    std::vector<worldgen::inspect::BiomeColour> biome_colours_;
    godot::String biome_table_problem_;
    std::vector<RegionRow> regions_;  // ascending id
    godot::String region_table_problem_;
    godot::String path_;
    godot::String error_code_;
    godot::String error_message_;
};

}  // namespace unmapped
