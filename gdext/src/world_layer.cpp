#include "world_layer.hpp"

#include <algorithm>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include "worldgen/inspect/render.hpp"

namespace unmapped {

namespace {

godot::String to_godot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

// Godot's Image limits (core/io/image.h: MAX_WIDTH, MAX_HEIGHT 2^24 and
// MAX_PIXELS 2^28).
constexpr std::uint64_t GODOT_IMAGE_MAX_SIDE = std::uint64_t{1} << 24U;
constexpr std::uint64_t GODOT_IMAGE_MAX_PIXELS = std::uint64_t{1} << 28U;

}  // namespace

godot::String codec_text(worldgen::db::Codec codec) {
    // worldgen's public headers name element types but not codecs; the two
    // names are those of spec P0-02 section 7.1.
    switch (codec) {
        case worldgen::db::Codec::raw:
            return "raw";
        case worldgen::db::Codec::delta_varint:
            return "delta_varint";
    }
    return "unknown";
}

godot::String element_type_text(worldgen::db::ElementType type) {
    return to_godot(worldgen::db::element_type_name(type));
}

bool fits_godot_image(std::uint64_t width, std::uint64_t height) noexcept {
    return width > 0 && height > 0 && width <= GODOT_IMAGE_MAX_SIDE && height <= GODOT_IMAGE_MAX_SIDE &&
           width * height <= GODOT_IMAGE_MAX_PIXELS;
}

godot::Ref<godot::Image> make_rgb_image(std::uint64_t width, std::uint64_t height, const godot::String& what,
                                        const std::function<void(std::span<std::uint8_t>)>& fill) {
    if (!fits_godot_image(width, height)) {
        ERR_PRINT(what + godot::String(" is too large for a Godot image"));
        return {};
    }
    godot::PackedByteArray data;
    const auto bytes = static_cast<std::size_t>(width * height * 3U);
    data.resize(static_cast<int64_t>(bytes));
    try {
        fill(std::span<std::uint8_t>(data.ptrw(), bytes));
    } catch (const std::exception& e) {
        ERR_PRINT(what + godot::String(" cannot be rendered: ") + e.what());
        return {};
    }
    return godot::Image::create_from_data(static_cast<int32_t>(width), static_cast<int32_t>(height), false,
                                          godot::Image::FORMAT_RGB8, data);
}

godot::Ref<godot::Image> composite_image(worldgen::inspect::Composite composite, const CompositeLayers& layers,
                                         std::span<const worldgen::inspect::BiomeColour> biome_table,
                                         godot::String& problem) {
    namespace inspect = worldgen::inspect;
    const godot::String name = to_godot(inspect::composite_name(composite));
    problem = godot::String();
    if (layers.elevation == nullptr) {
        problem = "the world has no layer 'elevation'";
        return {};
    }
    if (composite == inspect::Composite::biomes && layers.biome == nullptr) {
        problem = "the world has no layer 'biome'";
        return {};
    }
    const std::optional<std::string> unsuitable =
        composite == inspect::Composite::biomes
            ? inspect::composite_biomes_problem(*layers.elevation, *layers.biome, layers.water, layers.river,
                                                layers.flow_dir)
            : inspect::composite_map_problem(*layers.elevation, layers.water, layers.river, layers.flow_dir);
    if (unsuitable) {
        problem = to_godot(*unsuitable);
        return {};
    }
    const worldgen::db::LayerGrid& elevation = *layers.elevation;
    if (!fits_godot_image(elevation.width, elevation.height)) {
        problem = "too large for a Godot image";
        return {};
    }
    return make_rgb_image(elevation.width, elevation.height, "Composite " + name, [&](std::span<std::uint8_t> out) {
        // The composite functions return their buffer; one copy.
        const std::vector<std::uint8_t> rgb =
            composite == inspect::Composite::biomes
                ? inspect::render_composite_biomes(elevation, *layers.biome, biome_table, layers.water,
                                                   layers.river, layers.flow_dir, 1U)
                : inspect::render_composite_map(elevation, layers.water, layers.river, layers.flow_dir, 1U);
        if (rgb.size() != out.size()) {
            throw std::length_error("composite buffer size differs from the image");
        }
        std::copy(rgb.begin(), rgb.end(), out.begin());
    });
}

void WorldLayer::set_layer(worldgen::db::Layer layer, std::vector<worldgen::inspect::BiomeColour> biome_table) {
    layer_ = std::move(layer);
    palette_ = worldgen::db::is_valid_layer_grid(layer_.grid)
                   ? worldgen::inspect::auto_palette(layer_.name, layer_.grid)
                   : worldgen::inspect::Palette::gray;
    biome_table_ = palette_ == worldgen::inspect::Palette::biome ? std::move(biome_table)
                                                                 : std::vector<worldgen::inspect::BiomeColour>{};
}

godot::String WorldLayer::get_layer_name() const {
    return to_godot(layer_.name);
}

int64_t WorldLayer::get_width() const {
    return static_cast<int64_t>(layer_.grid.width);
}

int64_t WorldLayer::get_height() const {
    return static_cast<int64_t>(layer_.grid.height);
}

godot::String WorldLayer::get_element_type() const {
    return element_type_text(layer_.grid.type());
}

godot::String WorldLayer::get_codec() const {
    return codec_text(layer_.codec);
}

godot::String WorldLayer::get_palette() const {
    return to_godot(worldgen::inspect::palette_name(palette_));
}

godot::String WorldLayer::get_cell_text(int64_t x, int64_t y) const {
    const auto& grid = layer_.grid;
    if (x < 0 || y < 0 || x >= static_cast<int64_t>(grid.width) || y >= static_cast<int64_t>(grid.height)) {
        return {};
    }
    const auto index = static_cast<std::size_t>(y) * grid.width + static_cast<std::size_t>(x);
    return std::visit(
        [index](const auto& values) -> godot::String {
            using T = typename std::decay_t<decltype(values)>::value_type;
            if (index >= values.size()) {
                return {};
            }
            if constexpr (std::is_signed_v<T>) {
                return to_godot(std::to_string(static_cast<long long>(values[index])));
            } else {
                return to_godot(std::to_string(static_cast<unsigned long long>(values[index])));
            }
        },
        grid.values);
}

godot::String WorldLayer::get_min_text() const {
    if (!worldgen::db::is_valid_layer_grid(layer_.grid)) {
        return {};
    }
    const auto range = worldgen::inspect::value_range(layer_.grid);
    return to_godot(range.is_signed ? std::to_string(range.min_signed) : std::to_string(range.min_unsigned));
}

godot::String WorldLayer::get_max_text() const {
    if (!worldgen::db::is_valid_layer_grid(layer_.grid)) {
        return {};
    }
    const auto range = worldgen::inspect::value_range(layer_.grid);
    return to_godot(range.is_signed ? std::to_string(range.max_signed) : std::to_string(range.max_unsigned));
}

godot::Ref<godot::Image> WorldLayer::to_image() const {
    const auto& grid = layer_.grid;
    if (!worldgen::db::is_valid_layer_grid(grid)) {
        ERR_PRINT("WorldLayer.to_image: no valid layer");
        return {};
    }
    return make_rgb_image(grid.width, grid.height, "Layer " + get_layer_name(), [this](std::span<std::uint8_t> out) {
        worldgen::inspect::render_rgb(layer_.grid, palette_, 1U, biome_table_, out);
    });
}

void WorldLayer::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    ClassDB::bind_method(D_METHOD("get_layer_name"), &WorldLayer::get_layer_name);
    ClassDB::bind_method(D_METHOD("get_width"), &WorldLayer::get_width);
    ClassDB::bind_method(D_METHOD("get_height"), &WorldLayer::get_height);
    ClassDB::bind_method(D_METHOD("get_element_type"), &WorldLayer::get_element_type);
    ClassDB::bind_method(D_METHOD("get_codec"), &WorldLayer::get_codec);
    ClassDB::bind_method(D_METHOD("get_palette"), &WorldLayer::get_palette);
    ClassDB::bind_method(D_METHOD("get_cell_text", "x", "y"), &WorldLayer::get_cell_text);
    ClassDB::bind_method(D_METHOD("get_min_text"), &WorldLayer::get_min_text);
    ClassDB::bind_method(D_METHOD("get_max_text"), &WorldLayer::get_max_text);
    ClassDB::bind_method(D_METHOD("to_image"), &WorldLayer::to_image);
}

}  // namespace unmapped
