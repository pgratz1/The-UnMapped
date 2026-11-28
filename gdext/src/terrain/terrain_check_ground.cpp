// The terrain check's V3D-02 part (spec V3D-02 11.2): T3 in the default
// mode, T9 to T16. The check keeps its own copy of the formulas of 4 and 5
// (ground cells, river paint, control, colours), of 6.4's exclusions and
// of the cap rule, and compares the preview's buffers and Terrain3D's state
// with them.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "terrain/ground.hpp"
#include "terrain/terrain_check.hpp"
#include "terrain/terrain_check_internal.hpp"
#include "terrain/terrain_preview.hpp"
#include "terrain/vegetation.hpp"
#include "world_layer.hpp"
#include "worldgen/inspect/render.hpp"

namespace unmapped {

namespace {

godot::String num(std::uint64_t v) { return godot::String::num_int64(static_cast<int64_t>(v)); }

// --- The check's own copies ---------------------------------------------------------

struct Own {
    int n = 0;
    std::vector<std::int16_t> e;
    std::vector<std::uint8_t> biome, water, river, flow, timber;
    std::vector<std::int16_t> temp;
    std::vector<std::uint8_t> type, painted, rock, snow;
    std::vector<std::uint32_t> control;
    std::vector<std::uint8_t> rgba;
    [[nodiscard]] std::size_t at(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x);
    }
};

template <class T>
void read(WorldFile& world, const char* name, int n, std::vector<T>& out) {
    if (world.find_layer(name) < 0) {
        return;
    }
    const godot::Ref<WorldLayer> layer = world.read_layer(name);
    if (layer.is_null()) {
        return;
    }
    const auto& grid = layer->layer().grid;
    const auto* cells = grid.get_if<T>();
    if (cells == nullptr || static_cast<int>(grid.width) != n || static_cast<int>(grid.height) != n) {
        return;
    }
    out = *cells;
}

std::int64_t own_isqrt(std::int64_t v) {
    std::int64_t r = static_cast<std::int64_t>(std::sqrt(static_cast<double>(v)));
    while (r > 0 && r * r > v) {
        --r;
    }
    while ((r + 1) * (r + 1) <= v) {
        ++r;
    }
    return r;
}

std::uint8_t own_type(const std::string& key) {
    static const char* keys[] = {"grassland", "temperate_forest", "wet_woodland", "marsh", "fen", "steppe",
                                 "heath", "highland_moor", "alpine", "blighted_waste", "lake", "ocean"};
    static const std::uint8_t ids[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 13, 14};
    for (std::size_t k = 0; k < 12; ++k) {
        if (key == keys[k]) {
            return ids[k];
        }
    }
    return 0;
}

void own_ground(WorldFile& world, Own& o) {
    const int n = o.n;
    const std::size_t total = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    read(world, "biome", n, o.biome);
    read(world, "water", n, o.water);
    read(world, "river", n, o.river);
    read(world, "flow_dir", n, o.flow);
    read(world, "timber", n, o.timber);
    read(world, "temperature", n, o.temp);
    std::uint8_t code_type[256] = {};
    for (const auto& r : world.biome_records()) {
        if (r.id >= 0 && r.id <= 255) {
            code_type[r.id] = own_type(r.key);
        }
    }
    o.type.assign(total, 0);
    for (std::size_t k = 0; k < total && !o.biome.empty(); ++k) {
        o.type[k] = code_type[o.biome[k]];
    }
    // 5.2.
    o.painted.assign(total, 0);
    if (!o.water.empty()) {
        if (o.river.empty()) {
            for (std::size_t k = 0; k < total; ++k) {
                o.painted[k] = o.water[k] == 1 ? 16 : 0;
            }
        } else {
            struct P {
                int x, y, k;
            };
            std::vector<P> list;
            static const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
            for (std::size_t k = 0; k < total; ++k) {
                if (o.water[k] != 1 || o.river[k] == 0) {
                    continue;
                }
                const int x = static_cast<int>(k % static_cast<std::size_t>(n));
                const int y = static_cast<int>(k / static_cast<std::size_t>(n));
                const int cls = o.river[k];
                list.push_back({x, y, cls});
                if (!o.flow.empty() && o.flow[k] <= 7 && (o.flow[k] & 1) != 0) {
                    list.push_back({x + dx[o.flow[k]], y, cls});
                }
                if (cls >= 4) {
                    list.push_back({x + 1, y, cls});
                    list.push_back({x, y + 1, cls});
                }
            }
            std::stable_sort(list.begin(), list.end(), [](const P& a, const P& b) { return a.k < b.k; });
            for (const P& p : list) {
                if (p.x >= 0 && p.y >= 0 && p.x < n && p.y < n && o.water[o.at(p.x, p.y)] <= 1) {
                    o.painted[o.at(p.x, p.y)] = static_cast<std::uint8_t>(p.k);
                }
            }
        }
    }
    // 4.1.
    o.rock.assign(total, 0);
    o.snow.assign(total, 0);
    auto E = [&o, n](int x, int y) {
        return static_cast<std::int64_t>(o.e[o.at(std::clamp(x, 0, n - 1), std::clamp(y, 0, n - 1))]);
    };
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const std::size_t k = o.at(x, y);
            if (o.type[k] > 10) {
                continue;
            }
            const std::int64_t a = E(x + 1, y) - E(x - 1, y);
            const std::int64_t b = E(x, y + 1) - E(x, y - 1);
            const std::int64_t g = own_isqrt(a * a + b * b);
            o.rock[k] = static_cast<std::uint8_t>(g <= 224 ? 0 : g >= 320 ? 255 : ((g - 224) * 255 + 48) / 96);
            if (o.type[k] == 9 && !o.temp.empty()) {
                const std::int64_t T = o.temp[k];
                o.snow[k] = static_cast<std::uint8_t>(T >= -20 ? 0 : T <= -60 ? 255 : ((-20 - T) * 255 + 20) / 40);
            }
        }
    }
    // 4.2.
    o.control.assign(total, 0);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const std::size_t k = o.at(x, y);
            const std::uint32_t t = o.type[k];
            const bool painted = o.painted[k] > 0;
            const std::uint32_t base = painted ? 13 : t;
            std::uint32_t overlay = base;
            std::uint32_t blend = 0;
            if (t <= 10 && !painted) {
                int count[11] = {};
                for (int wy = std::max(0, y - 2); wy <= std::min(n - 1, y + 2); ++wy) {
                    for (int wx = std::max(0, x - 2); wx <= std::min(n - 1, x + 2); ++wx) {
                        const std::uint8_t w = o.type[o.at(wx, wy)];
                        if (w <= 10) {
                            ++count[w];
                        }
                    }
                }
                int other = 0;
                std::uint32_t ov = 0;
                for (std::uint32_t w = 0; w <= 10; ++w) {
                    if (w != t && count[w] > other) {
                        other = count[w];
                        ov = w;
                    }
                }
                const int b = count[t];
                const int border = other == 0 ? 0 : std::min(127, (255 * other + (b + other) / 2) / (b + other));
                std::uint32_t pick_id = 11;
                int pick = o.rock[k];
                if (o.snow[k] > pick) {
                    pick_id = 12;
                    pick = o.snow[k];
                }
                if (border > pick) {
                    pick_id = ov;
                    pick = border;
                }
                if (pick > 0) {
                    overlay = pick_id;
                    blend = static_cast<std::uint32_t>(pick);
                }
            }
            o.control[k] = (base << 27) | (overlay << 22) | (blend << 14);
        }
    }
    // 5.1.
    std::uint32_t lake = 0x4080CC;
    for (const auto& r : world.biome_records()) {
        if (r.key == "lake") {
            lake = static_cast<std::uint32_t>(r.colour);
            break;
        }
    }
    o.rgba.assign(4 * total, 255);
    for (std::size_t k = 0; k < total; ++k) {
        std::uint32_t rgb = 0xFFFFFF;
        if (o.painted[k] > 0) {
            if (o.painted[k] <= 15) {
                const auto c = worldgen::inspect::composite_river_colour(o.painted[k]);
                rgb = (static_cast<std::uint32_t>(c.r) << 16) | (static_cast<std::uint32_t>(c.g) << 8) | c.b;
            } else {
                rgb = 0x3464C8;
            }
        } else if (o.type[k] == 13) {
            rgb = lake;
        }
        o.rgba[4 * k] = static_cast<std::uint8_t>(rgb >> 16);
        o.rgba[4 * k + 1] = static_cast<std::uint8_t>(rgb >> 8);
        o.rgba[4 * k + 2] = static_cast<std::uint8_t>(rgb);
        o.rgba[4 * k + 3] = 128;
    }
}

// 6.4's exclusion, the check's own reading.
bool excluded(const Own& o, std::size_t k) {
    const std::uint8_t t = o.type[k];
    return t < 1 || t > 10 || o.painted[k] > 0 || o.rock[k] >= 128 || o.snow[k] >= 128 ||
           (!o.water.empty() && o.water[k] != 0) || o.e[k] <= 0;
}

std::uint64_t own_mix(std::uint64_t z) {
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

struct Cand {
    int kind;
    float x, z;
    std::uint64_t key;
};

// Every candidate of 6.4 with its cap key (thin << 32 | p).
std::vector<Cand> own_candidates(const Own& o, std::uint64_t seed) {
    std::vector<Cand> out;
    const int n = o.n;
    const std::uint64_t key0 = own_mix(seed ^ 0x7633642D73636174ull);
    const float lo = -8.0f * static_cast<float>(n);
    const float hi = 8.0f * static_cast<float>(n) - 16.0f;
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const std::size_t c = o.at(x, y);
            if (excluded(o, c)) {
                continue;
            }
            const int t = o.timber.empty() ? terrain::VEGETATION_DENSITY[o.type[c]].fallback_timber : o.timber[c];
            const auto e = terrain::expected_counts(o.type[c], t, o.e[c]);
            const std::uint64_t hb =
                own_mix(key0 ^ ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) | static_cast<std::uint32_t>(y)));
            for (std::uint64_t j = 0; j < 5; ++j) {
                const std::uint64_t hc = own_mix(hb + 64 * j);
                const std::uint64_t count = (e[j] >> 8) + ((hc & 0xFF) < (e[j] & 0xFF) ? 1 : 0);
                for (std::uint64_t k = 0; k < count; ++k) {
                    const std::uint64_t h = own_mix(hb + 64 * j + 1 + k);
                    const float X = 16.0f * static_cast<float>(x - n / 2) + static_cast<float>(3 * (h & 0x3FF)) / 256.0f - 6.0f;
                    const float Z = 16.0f * static_cast<float>(y - n / 2) + static_cast<float>(3 * ((h >> 10) & 0x3FF)) / 256.0f - 6.0f;
                    if (X < lo || X > hi || Z < lo || Z > hi) {
                        continue;
                    }
                    const auto thin = static_cast<std::uint32_t>(own_mix(h ^ 0xD1B54A32D192ED03ull) >> 32);
                    out.push_back({static_cast<int>(j), X, Z, (static_cast<std::uint64_t>(thin) << 32) | out.size()});
                }
            }
        }
    }
    return out;
}

double own_lin(double u) { return u <= 0.04045 ? u / 12.92 : std::pow((u + 0.055) / 1.055, 2.4); }
double own_srgb(double u) { return u <= 0.0031308 ? 12.92 * u : 1.055 * std::pow(u, 1.0 / 2.4) - 0.055; }

godot::Object* assets_of(const TerrainPreview& preview) {
    if (preview.terrain_node() == nullptr) {
        return nullptr;
    }
    return preview.terrain_node()->call("get_assets");
}

}  // namespace

void run_ground_checks(TerrainPreview& preview, const TerrainCheckOptions& options, TerrainChecker& c) {
    const TerrainScene& scene = preview.scene();
    WorldFile& world = *preview.world().ptr();
    Own o;
    o.n = scene.layout->n;
    {
        const godot::Ref<WorldLayer> elevation = world.read_layer("elevation");
        o.e = *elevation->layer().grid.get_if<std::int16_t>();
    }
    own_ground(world, o);
    const int n = o.n;
    const std::size_t total = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    const bool have_biome = !o.biome.empty();
    const bool present = options.expect_terrain3d == "present";
    const bool textured = have_biome && !preview.flat_colours_option();
    const bool vegetation = have_biome && !preview.no_trees_option();
    c.check(scene.textured == textured && scene.vegetation == vegetation,
            godot::String("V3D-02 mode: ground ") + (textured ? "textured" : "flat") + ", vegetation " +
                (vegetation ? "on" : "off"));

    // T3 (default mode): the colour image against the check's own 5.1, and
    // every painted cell against the composite map.
    if (textured) {
        const godot::PackedByteArray rgba = preview.colour_image()->get_data();
        std::size_t bad = 0;
        for (std::size_t k = 0; k < 4 * total; ++k) {
            bad += rgba[static_cast<int64_t>(k)] == o.rgba[k] ? 0u : 1u;
        }
        c.check(bad == 0, "T3 colour image equals the check's own 5.1 at every cell (" + num(bad) + " bytes differ)");
        const godot::String png = options.reference_dir.path_join("map.png");
        if (godot::FileAccess::file_exists(png)) {
            const godot::Ref<godot::Image> map = godot::Image::load_from_file(png);
            std::size_t painted = 0;
            std::size_t differ = 0;
            if (map.is_valid() && map->get_width() == n && map->get_height() == n) {
                map->convert(godot::Image::FORMAT_RGB8);
                const godot::PackedByteArray rgb = map->get_data();
                for (std::size_t k = 0; k < total; ++k) {
                    if (o.painted[k] == 0) {
                        continue;
                    }
                    ++painted;
                    for (std::size_t ch = 0; ch < 3; ++ch) {
                        if (rgb[static_cast<int64_t>(3 * k + ch)] != rgba[static_cast<int64_t>(4 * k + ch)]) {
                            ++differ;
                            break;
                        }
                    }
                }
            }
            c.check(map.is_valid() && painted > 0 && differ == 0,
                    "T3 every painted cell (" + num(painted) + ") equals map.png (" + num(differ) + " differ)");
        } else {
            TerrainChecker::info("T3 no map.png for this world: painted cells not compared");
        }
    }

    // T10: control image against the check's own 4.2 (flat: all 0).
    {
        const godot::PackedByteArray control = preview.control_image()->get_data();
        std::size_t bad = 0;
        for (std::size_t k = 0; k < total; ++k) {
            std::uint32_t bits = 0;
            std::memcpy(&bits, control.ptr() + 4 * k, 4);
            const std::uint32_t want = textured ? o.control[k] : 0u;
            bad += bits == want ? 0u : 1u;
        }
        c.check(bad == 0, godot::String("T10 control image equals ") + (textured ? "the check's own 4.2" : "0") +
                              " at every cell (" + num(bad) + " differ)");
        if (present && preview.terrain_node() != nullptr) {
            godot::Object* data = preview.terrain_node()->call("get_data");
            std::vector<std::pair<int, int>> points = {{0, 0}, {n - 1, 0}, {0, n - 1}, {n - 1, n - 1}, {n / 2, n / 2}};
            for (int k = 1; k <= 27; ++k) {
                points.emplace_back((37 * k) % n, (61 * k) % n);
            }
            int wrong = 0;
            for (const auto& [x, y] : points) {
                const godot::Vector3 at(16.0f * static_cast<float>(x - n / 2), 0.0f, 16.0f * static_cast<float>(y - n / 2));
                const auto got = static_cast<std::uint32_t>(static_cast<int64_t>(data->call("get_control", at)));
                wrong += got == (textured ? o.control[o.at(x, y)] : 0u) ? 0 : 1;
            }
            c.check(wrong == 0, "T10 get_control at " + num(points.size()) + " points equals it bit for bit (" +
                                    num(static_cast<std::uint64_t>(wrong)) + " differ)");
        }
    }

    godot::Object* assets = assets_of(preview);
    // T9: the fifteen texture assets (flat: one).
    if (present && assets != nullptr) {
        const int64_t count = assets->call("get_texture_count");
        if (textured) {
            c.check(count == 15, "T9 15 texture assets (" + godot::String::num_int64(count) + ")");
            int bad_image = 0;
            int bad_colour = 0;
            int bad_scale = 0;
            double worst = 0.0;
            for (int id = 0; id < 15 && id < count; ++id) {
                godot::Object* asset = assets->call("get_texture_asset", id);
                if (asset == nullptr) {
                    ++bad_image;
                    continue;
                }
                const terrain::GroundTexture want = terrain::build_ground_texture(static_cast<terrain::GroundType>(id));
                const godot::Ref<godot::Texture2D> albedo = asset->get("albedo_texture");
                const godot::Ref<godot::Texture2D> normal = asset->get("normal_texture");
                for (int which = 0; which < 2; ++which) {
                    const godot::Ref<godot::Texture2D>& tex = which == 0 ? albedo : normal;
                    const std::vector<std::uint8_t>& bytes = which == 0 ? want.albedo : want.normal;
                    const godot::Ref<godot::Image> image = tex.is_valid() ? tex->get_image() : godot::Ref<godot::Image>();
                    bool ok = image.is_valid() && image->get_width() == 256 && image->get_height() == 256 &&
                              image->get_format() == godot::Image::FORMAT_RGBA8 && image->has_mipmaps();
                    if (ok) {
                        const godot::PackedByteArray data = image->get_data();
                        ok = data.size() >= static_cast<int64_t>(bytes.size()) &&
                             std::memcmp(data.ptr(), bytes.data(), bytes.size()) == 0;
                    }
                    bad_image += ok ? 0 : 1;
                }
                // 3.3 from the texture's own bytes, then the sRGB encoding
                // Godot undoes on the way to the shader.
                double sum = 0.0;
                for (std::size_t k = 0; k < want.albedo.size(); k += 4) {
                    sum += want.albedo[k];
                }
                const double m = own_lin(sum / 65536.0 / 255.0);
                const std::uint32_t rgb = terrain::ground_type_colour(static_cast<terrain::GroundType>(id), world.biome_records());
                const godot::Color got = asset->get("albedo_color");
                const double want_c[3] = {own_srgb(own_lin(static_cast<double>((rgb >> 16) & 0xFF) / 255.0) / m),
                                          own_srgb(own_lin(static_cast<double>((rgb >> 8) & 0xFF) / 255.0) / m),
                                          own_srgb(own_lin(static_cast<double>(rgb & 0xFF) / 255.0) / m)};
                const double got_c[3] = {static_cast<double>(got.r), static_cast<double>(got.g), static_cast<double>(got.b)};
                for (int ch = 0; ch < 3; ++ch) {
                    const double d = std::abs(got_c[ch] - want_c[ch]);
                    worst = std::max(worst, d);
                    bad_colour += d <= 1e-6 * std::max(1.0, want_c[ch]) ? 0 : 1;
                }
                const double scale = asset->get("uv_scale");
                bad_scale += std::abs(scale - static_cast<double>(terrain::GROUND_TYPES[static_cast<std::size_t>(id)].uv_scale)) <= 1e-7 ? 0 : 1;
            }
            c.check(bad_image == 0, "T9 albedo and normal images 256 x 256 RGBA8 with mipmaps, level 0 equal to build_ground_texture (" +
                                        num(static_cast<std::uint64_t>(bad_image)) + " differ)");
            c.check(bad_colour == 0, "T9 albedo_color as 3.3, sRGB-encoded, within 1e-6 (worst " + godot::String::num(worst, 9) + ")");
            c.check(bad_scale == 0, "T9 uv_scale as 3.1");
        } else {
            c.check(count == 1, "T9 flat colours: one texture asset (" + godot::String::num_int64(count) + ")");
        }
    }

    // T11: mesh assets (no vegetation: none of ours; Terrain3D keeps a
    // default texture card in an empty mesh list).
    if (present && assets != nullptr) {
        const int64_t count = assets->call("get_mesh_count");
        if (vegetation) {
            c.check(count == 5, "T11 5 mesh assets (" + godot::String::num_int64(count) + ")");
            const int lods[5] = {2, 2, 1, 1, 1};
            int bad = 0;
            for (int id = 0; id < 5 && id < count; ++id) {
                godot::Object* asset = assets->call("get_mesh_asset", id);
                const auto& info = terrain::VEGETATION_KINDS_INFO[static_cast<std::size_t>(id)];
                const int64_t lc = asset->call("get_lod_count");
                const double r0 = asset->call("get_lod_range", 0);
                const double r1 = asset->call("get_lod_range", 1);
                const int64_t shadows = asset->get("cast_shadows");
                bool ok = lc == lods[id] && std::abs(r0 - static_cast<double>(info.lod0_range)) < 1e-3 && shadows == 0;
                if (lods[id] > 1) {
                    ok = ok && std::abs(r1 - static_cast<double>(info.lod1_range)) < 1e-3;
                }
                if (!ok) {
                    ++bad;
                    TerrainChecker::info("T11 mesh " + godot::String::num_int64(id) + ": lods " + godot::String::num_int64(lc) +
                                         ", ranges " + godot::String::num(r0) + " " + godot::String::num(r1) + ", shadows " +
                                         godot::String::num_int64(shadows));
                }
            }
            c.check(bad == 0, "T11 LOD counts 2, 2, 1, 1, 1, the LOD ranges of 6.1, cast_shadows 0");
        } else {
            bool ours = false;
            for (int64_t id = 0; id < count; ++id) {
                godot::Object* asset = assets->call("get_mesh_asset", id);
                const godot::Variant file = asset != nullptr ? asset->get("scene_file") : godot::Variant();
                ours = ours || (file.get_type() == godot::Variant::OBJECT && static_cast<godot::Object*>(file) != nullptr);
            }
            c.check(count <= 1 && !ours, "T11 no vegetation: no mesh asset of the preview (" +
                                            godot::String::num_int64(count) + " in the list, Terrain3D's default card)");
        }
    }

    // T12: the instances stored in the regions.
    if (present && vegetation && preview.terrain_node() != nullptr) {
        godot::Object* data = preview.terrain_node()->call("get_data");
        const godot::TypedArray<godot::Vector2i> locations = data->call("get_region_locations");
        std::uint64_t got[5] = {};
        for (int64_t r = 0; r < locations.size(); ++r) {
            godot::Object* region = data->call("get_region", locations[r]);
            if (region == nullptr) {
                continue;
            }
            const godot::Dictionary instances = region->call("get_instances");
            for (int id = 0; id < 5; ++id) {
                if (!instances.has(id)) {
                    continue;
                }
                const godot::Dictionary cells = instances[id];
                const godot::Array keys = cells.keys();
                for (int64_t k = 0; k < keys.size(); ++k) {
                    const godot::Array triple = cells[keys[k]];
                    if (triple.size() >= 1) {
                        const godot::Array xforms = triple[0];
                        got[id] += static_cast<std::uint64_t>(xforms.size());
                    }
                }
            }
        }
        bool counts = true;
        godot::String text;
        for (int id = 0; id < 5; ++id) {
            const std::uint64_t want = scene.placement.kinds[static_cast<std::size_t>(id)].size();
            counts = counts && got[id] == want;
            text += (id > 0 ? ", " : "") + num(got[id]) + "/" + num(want);
        }
        c.check(counts, "T12 instances in the regions per kind equal the placement (" + text + ")");
        // The first and last instance of each kind at their position.
        int missing = 0;
        for (int id = 0; id < 5; ++id) {
            const auto& list = scene.placement.kinds[static_cast<std::size_t>(id)];
            if (list.empty()) {
                continue;
            }
            for (const terrain::Instance* in : {&list.front(), &list.back()}) {
                const int lx = static_cast<int>(std::floor(in->x / 4096.0f));
                const int lz = static_cast<int>(std::floor(in->z / 4096.0f));
                const godot::Vector3 local(in->x - 4096.0f * static_cast<float>(lx), in->y, in->z - 4096.0f * static_cast<float>(lz));
                godot::Object* region = data->call("get_region", godot::Vector2i(lx, lz));
                bool found = false;
                if (region != nullptr) {
                    const godot::Dictionary instances = region->call("get_instances");
                    const godot::Dictionary cells = instances.get(id, godot::Dictionary());
                    const godot::Array keys = cells.keys();
                    for (int64_t k = 0; k < keys.size() && !found; ++k) {
                        const godot::Array triple = cells[keys[k]];
                        const godot::Array xforms = triple[0];
                        for (int64_t t = 0; t < xforms.size() && !found; ++t) {
                            const godot::Transform3D xf = xforms[t];
                            found = xf.origin.distance_to(local) <= 0.01f;
                        }
                    }
                }
                missing += found ? 0 : 1;
            }
        }
        c.check(missing == 0, "T12 the first and last instance of each kind found at their position within 0.01 m (" +
                                  num(static_cast<std::uint64_t>(missing)) + " missing)");
    }

    // T13: placement against the check's own exclusions and cap rule.
    if (vegetation) {
        std::size_t on_excluded = 0;
        std::size_t far = 0;
        for (const auto& list : scene.placement.kinds) {
            for (const terrain::Instance& in : list) {
                const terrain::CellHit hit = terrain::world_to_cell(*scene.layout, in.x, in.z);
                if (!hit.inside) {
                    ++far;
                    continue;
                }
                const terrain::Vec2 v = terrain::cell_to_world(*scene.layout, hit.x, hit.y);
                far += (std::abs(in.x - v.x) <= 6.0f && std::abs(in.z - v.z) <= 6.0f) ? 0u : 1u;
                on_excluded += excluded(o, o.at(hit.x, hit.y)) ? 1u : 0u;
            }
        }
        c.check(on_excluded == 0 && far == 0, "T13 no instance on an excluded cell (" + num(on_excluded) +
                                                  ") or more than 6 m from its vertex (" + num(far) + ")");
        std::vector<Cand> cands = own_candidates(o, scene.seed);
        const std::uint64_t cap = preview.instance_cap();
        const std::uint64_t S = cands.size();
        const std::uint64_t kept = scene.placement.kept();
        c.check(scene.placement.candidates == S && kept == std::min<std::uint64_t>(S, cap),
                "T13 candidates " + num(S) + ", kept " + num(kept) + " = min(S, " + num(cap) + ")");
        // The kept set: the cap smallest keys, back in placement order.
        if (S > cap) {
            std::vector<std::uint64_t> keys(cands.size());
            for (std::size_t i = 0; i < cands.size(); ++i) {
                keys[i] = cands[i].key;
            }
            std::sort(keys.begin(), keys.end());
            const std::uint64_t limit = cap == 0 ? 0 : keys[cap - 1];
            std::vector<Cand> survivors;
            for (const Cand& cd : cands) {
                if (cap > 0 && cd.key <= limit) {
                    survivors.push_back(cd);
                }
            }
            cands.swap(survivors);
        }
        std::size_t next[5] = {};
        std::size_t differ = 0;
        for (const Cand& cd : cands) {
            const auto& list = scene.placement.kinds[static_cast<std::size_t>(cd.kind)];
            std::size_t& at = next[cd.kind];
            if (at >= list.size() || list[at].x != cd.x || list[at].z != cd.z) {
                ++differ;
            }
            ++at;
        }
        c.check(differ == 0, "T13 the kept instances equal the check's own placement and cap rule (" + num(differ) + " differ)");
    }

    // T14: the sea of section 7.
    {
        const godot::MeshInstance3D* sea = preview.sea();
        const godot::Ref<godot::PlaneMesh> plane = sea != nullptr ? sea->get_mesh() : godot::Ref<godot::Mesh>();
        c.check(plane.is_valid() && plane->get_size() == godot::Vector2(800000.0f, 800000.0f) &&
                    sea->get_position() == godot::Vector3(0.0f, 0.0f, 0.0f),
                "T14 sea plane 800,000 m at the origin");
        c.check(preview.camera()->get_far() == 450000.0f && preview.camera()->get_near() == 1.0f,
                "T14 camera near 1 m, far 450,000 m");
        auto* env_node = godot::Object::cast_to<godot::WorldEnvironment>(preview.get_node_or_null("WorldEnvironment"));
        const godot::Ref<godot::Environment> env = env_node != nullptr ? env_node->get_environment() : godot::Ref<godot::Environment>();
        const std::uint32_t sea_rgb = scene.sea_rgb;
        const godot::Color sea_colour(static_cast<float>((sea_rgb >> 16) & 0xFF) / 255.0f,
                                      static_cast<float>((sea_rgb >> 8) & 0xFF) / 255.0f,
                                      static_cast<float>(sea_rgb & 0xFF) / 255.0f);
        c.check(env.is_valid() && env->is_fog_enabled() && env->get_fog_mode() == godot::Environment::FOG_MODE_DEPTH &&
                    env->get_fog_light_color().is_equal_approx(sea_colour) && env->get_fog_light_energy() == 1.0f &&
                    env->get_fog_density() == 1.0f && env->get_fog_sun_scatter() == 0.0f &&
                    env->get_fog_aerial_perspective() == 0.0f && env->get_fog_sky_affect() == 0.0f &&
                    env->get_fog_depth_begin() == 200000.0f && env->get_fog_depth_end() == 400000.0f &&
                    env->get_fog_depth_curve() == 1.0f,
                "T14 depth fog of section 7 in the sea colour");
    }

    // T15: status lines.
    {
        const godot::String status = preview.status_text();
        std::vector<godot::String> want;
        std::vector<godot::String> not_want;
        if (!have_biome) {
            want.push_back("no biome layer: ground drawn grey, no vegetation");
            not_want.push_back("vegetation: ");
            not_want.push_back("ground: ");
        } else {
            want.push_back(textured ? "ground: textured, 15 ground types" : "ground: flat colours");
            if (vegetation) {
                godot::String line = "vegetation: " + num(scene.placement.kept()) + " instances";
                if (scene.placement.candidates > scene.placement.cap) {
                    line += ", thinned from " + num(scene.placement.candidates) + " to the cap";
                }
                want.push_back(line);
            } else {
                want.push_back("vegetation: off");
            }
            (o.timber.empty() && vegetation ? want : not_want).push_back("no timber layer: tree density from the biome alone");
            (o.water.empty() && textured ? want : not_want).push_back("no water layer: rivers not drawn");
            (o.temp.empty() && textured ? want : not_want).push_back("no temperature layer: no snow");
        }
        (world.get_meta_value("seed").is_empty() && vegetation ? want : not_want)
            .push_back("no seed in meta: vegetation placed with seed 0");
        bool ok = true;
        for (const auto& line : want) {
            ok = ok && status.contains(line);
        }
        for (const auto& line : not_want) {
            ok = ok && !status.contains(line);
        }
        c.check(ok, "T15 status lines of 10.3: '" + status.replace("\n", " / ") + "'");
    }
}

void run_key_checks(TerrainPreview& preview, const TerrainCheckOptions& options, TerrainChecker& c) {
    const bool present = options.expect_terrain3d == "present";
    const TerrainScene& scene = preview.scene();
    if (!scene.layout) {
        return;
    }
    // Key T.
    if (present && scene.vegetation) {
        auto enabled = [&preview](bool want) {
            godot::Object* assets = assets_of(preview);
            if (assets == nullptr) {
                return false;
            }
            const int64_t count = assets->call("get_mesh_count");
            bool all = count == 5;
            for (int64_t id = 0; id < count; ++id) {
                godot::Object* asset = assets->call("get_mesh_asset", id);
                all = all && asset != nullptr && static_cast<bool>(asset->get("enabled")) == want;
            }
            return all;
        };
        const bool before = enabled(true);
        const bool flipped = preview.toggle_trees() && enabled(false);
        const bool back = preview.toggle_trees() && enabled(true);
        c.check(before && flipped && back, "T16 key T flips enabled on all five mesh assets and back");
    } else {
        c.check(!preview.toggle_trees(), "T16 key T does nothing without vegetation in a Terrain3D node");
    }
    // Key G.
    if (scene.biome.empty()) {
        c.check(!preview.toggle_ground(), "T16 key G does nothing without a biome layer");
        return;
    }
    const bool was_textured = scene.textured;
    const godot::Vector3 position = preview.camera()->get_position();
    const godot::Vector3 rotation = preview.camera()->get_rotation();
    auto terrain_count = [&preview]() {
        int count = 0;
        for (int32_t i = 0; i < preview.get_child_count(); ++i) {
            count += preview.get_child(i)->get_class() == "Terrain3D" ? 1 : 0;
        }
        return count;
    };
    const bool first = preview.toggle_ground();
    const bool other = preview.scene().textured != was_textured;
    const bool camera_kept = preview.camera()->get_position() == position && preview.camera()->get_rotation() == rotation;
    const int nodes = terrain_count();
    const bool second = preview.toggle_ground();
    const bool back = preview.scene().textured == was_textured;
    c.check(first && other && camera_kept && (!present || nodes == 1) && second && back,
            "T16 key G reopens in the other ground and back with the camera unchanged" +
                godot::String(present ? " and one Terrain3D node" : ""));
}

}  // namespace unmapped
