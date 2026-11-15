#include "world_layer.hpp"

#include <cstring>
#include <exception>
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

void WorldLayer::set_layer(worldgen::db::Layer layer) {
    layer_ = std::move(layer);
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
    return to_godot(worldgen::inspect::palette_name(worldgen::inspect::registered_palette(layer_.name)));
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
    if (grid.width > GODOT_IMAGE_MAX_SIDE || grid.height > GODOT_IMAGE_MAX_SIDE ||
        grid.cell_count() > GODOT_IMAGE_MAX_PIXELS) {
        ERR_PRINT(godot::String("Layer ") + get_layer_name() + " is too large for a Godot image");
        return {};
    }
    std::vector<std::uint8_t> rgb;
    try {
        rgb = worldgen::inspect::render_rgb(grid, worldgen::inspect::registered_palette(layer_.name), 1U);
    } catch (const std::exception& e) {
        ERR_PRINT(godot::String("Layer ") + get_layer_name() + " cannot be rendered: " + e.what());
        return {};
    }
    godot::PackedByteArray data;
    data.resize(static_cast<int64_t>(rgb.size()));
    std::memcpy(data.ptrw(), rgb.data(), rgb.size());
    return godot::Image::create_from_data(static_cast<int32_t>(grid.width), static_cast<int32_t>(grid.height),
                                          false, godot::Image::FORMAT_RGB8, data);
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
