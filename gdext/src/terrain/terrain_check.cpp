#include "terrain/terrain_check.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "terrain/fly_state.hpp"
#include "terrain/terrain_check_internal.hpp"
#include "terrain/terrain_preview.hpp"
#include "world_layer.hpp"
#include "worldgen/inspect/layer_catalogue.hpp"

namespace unmapped {

namespace {

godot::String to_godot(const std::string& text) {
    return godot::String::utf8(text.c_str(), static_cast<int64_t>(text.size()));
}

std::string to_utf8(const godot::String& text) {
    const godot::CharString utf8 = text.utf8();
    return {utf8.get_data(), static_cast<std::size_t>(utf8.length())};
}

using Checker = TerrainChecker;

// --- The check's own formulas (5.1 to 5.3) -----------------------------------------

struct Own {
    int n = 0;
    std::vector<std::int16_t> e;
    std::vector<std::uint16_t> d;
    std::vector<float> h;
    float h_min = 0.0f;
    float h_max = 0.0f;
    int highest = 0;  // row-major index of the first highest cell
    int lowest = 0;
    int first_lake = -1;

    [[nodiscard]] float X(int x) const { return 16.0f * static_cast<float>(x - n / 2); }
    [[nodiscard]] float Z(int y) const { return 16.0f * static_cast<float>(y - n / 2); }
    [[nodiscard]] float at(int x, int y) const {
        return h[static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x)];
    }
    // The maximum of the four vertices around (X, Z), clamped to the map.
    [[nodiscard]] float ground(float px, float pz) const {
        const float top = static_cast<float>(n - 1);
        const float u = std::clamp(px / 16.0f + static_cast<float>(n / 2), 0.0f, top);
        const float v = std::clamp(pz / 16.0f + static_cast<float>(n / 2), 0.0f, top);
        const int i = std::min(static_cast<int>(std::floor(u)), n - 2);
        const int j = std::min(static_cast<int>(std::floor(v)), n - 2);
        return std::max(std::max(at(i, j), at(i + 1, j)), std::max(at(i, j + 1), at(i + 1, j + 1)));
    }
};

bool read_own(WorldFile& world, Own& own, godot::String& problem) {
    const godot::Ref<WorldLayer> elevation = world.read_layer("elevation");
    if (elevation.is_null() || elevation->layer().grid.get_if<std::int16_t>() == nullptr) {
        problem = "the elevation layer cannot be read as i16";
        return false;
    }
    own.n = static_cast<int>(elevation->get_width());
    own.e = *elevation->layer().grid.get_if<std::int16_t>();
    if (world.find_layer("water_depth") >= 0) {
        const godot::Ref<WorldLayer> water = world.read_layer("water_depth");
        if (water.is_valid() && water->layer().grid.get_if<std::uint16_t>() != nullptr) {
            own.d = *water->layer().grid.get_if<std::uint16_t>();
        }
    }
    const std::size_t cells = own.e.size();
    own.h.resize(cells);
    for (std::size_t k = 0; k < cells; ++k) {
        const std::int32_t e = own.e[k];
        const std::int32_t d = own.d.empty() ? 0 : static_cast<std::int32_t>(own.d[k]);
        const bool lake = e > 0 && d > 0;
        own.h[k] = lake ? static_cast<float>(e + d) / 10.0f : static_cast<float>(e) / 10.0f;
        if (lake && own.first_lake < 0) {
            own.first_lake = static_cast<int>(k);
        }
    }
    own.highest = static_cast<int>(std::max_element(own.h.begin(), own.h.end()) - own.h.begin());
    own.lowest = static_cast<int>(std::min_element(own.h.begin(), own.h.end()) - own.h.begin());
    own.h_max = own.h[static_cast<std::size_t>(own.highest)];
    own.h_min = own.h[static_cast<std::size_t>(own.lowest)];
    return true;
}

std::string decimal1(double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.1f", v);
    std::string text = buffer;
    return text == "-0.0" ? std::string("0.0") : text;
}

// The HUD line of 8.4, built here from the format, for a camera at the
// vertex of cell (x, y) (or outside the map at (X, Z), whose bilinear
// ground is then given) at altitude Y, speed v and 0 fps.
std::string expected_hud(const Own& own, const WorldFile& world, const TerrainScene& scene, bool inside, int x, int y,
                         float X, float Z, float Y, float ground, int speed) {
    std::string text;
    const std::string sep = "  |  ";
    text = inside ? "cell (" + std::to_string(x) + ", " + std::to_string(y) + ")" : std::string("outside the map");
    text += sep + std::to_string(static_cast<long>(std::lround(X))) + " m E, " +
            std::to_string(static_cast<long>(std::lround(Z))) + " m S";
    text += sep + "altitude " + decimal1(static_cast<double>(Y)) + " m, " + decimal1(static_cast<double>(Y - ground)) +
            " above ground";
    if (inside) {
        const std::size_t k = static_cast<std::size_t>(y) * static_cast<std::size_t>(own.n) + static_cast<std::size_t>(x);
        const std::int64_t e = own.e[k];
        text += sep + "ground " + worldgen::inspect::format_layer_value("elevation", e);
        const std::int64_t d = own.d.empty() ? 0 : own.d[k];
        if (e > 0 && d > 0) {
            text += ", lake surface " + decimal1(static_cast<double>(e + d) / 10.0) + " m";
        }
        if (!scene.biome.empty()) {
            const worldgen::inspect::BiomeRecord* record = world.find_biome(scene.biome[k]);
            if (record != nullptr && !record->name.empty()) {
                text += sep + record->name;
            }
        }
        if (!scene.region.empty()) {
            const std::string name = to_utf8(world.get_region_name(scene.region[k]));
            if (!name.empty()) {
                text += sep + name;
            }
        }
    }
    text += sep + std::to_string(speed) + " m/s" + sep + "0 fps";
    return text;
}

float rf_pixel(const godot::PackedByteArray& data, std::size_t k) {
    float v = 0.0f;
    std::memcpy(&v, data.ptr() + 4 * k, sizeof v);
    return v;
}

// --- Checks ----------------------------------------------------------------------

void check_no_world(TerrainPreview& preview, const TerrainCheckOptions& options, Checker& c) {
    // T5: the pattern world (no elevation) and a missing file.
    const godot::String status = preview.status_text();
    c.check(status == TerrainPreview::NO_ELEVATION_TEXT,
            "T5 status is the no-elevation text (got '" + status + "')");
    c.check(preview.terrain_node() == nullptr, "T5 no Terrain3D node");
    const godot::String missing = options.reference_dir.path_join("no-such-world.db");
    preview.open_world(missing);
    const godot::String missing_status = preview.status_text();
    c.check(missing_status.begins_with(missing + godot::String(": file_unreadable: ")),
            "T5 a missing file: '" + missing_status + "'");
}

}  // namespace

int run_terrain_check(TerrainPreview& preview, const TerrainCheckOptions& options,
                      std::unique_ptr<TerrainCheckRun>* path) {
    Checker c;
    Checker::info("terrain check of " + options.world_path + ", Godot " +
                  godot::String(godot::Engine::get_singleton()->get_version_info()["string"]) +
                  ", expect Terrain3D " + options.expect_terrain3d);
    const TerrainScene& scene = preview.scene();
    if (!scene.layout) {
        check_no_world(preview, options, c);
        godot::UtilityFunctions::print(c.all ? "UNMAPPED-TERRAIN-CHECK: PASS" : "UNMAPPED-TERRAIN-CHECK: FAIL");
        return c.all ? 0 : 1;
    }
    WorldFile& world = *preview.world().ptr();
    Own own;
    godot::String problem;
    if (!read_own(world, own, problem)) {
        c.check(false, "read the world: " + problem);
        godot::UtilityFunctions::print("UNMAPPED-TERRAIN-CHECK: FAIL");
        return 1;
    }
    const int n = own.n;
    const std::size_t cells = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    const terrain::Layout& layout = *scene.layout;

    // T1: layout, image sizes and formats.
    c.check(layout.n == n && layout.spacing == 16.0f && layout.region_size == 256 && layout.region_min == -(n / 512) &&
                layout.region_count == n / 256 && layout.origin == -8.0f * static_cast<float>(n),
            "T1 layout of " + godot::String::num_int64(n) + ": regions " + godot::String::num_int64(layout.region_min) +
                " to " + godot::String::num_int64(layout.region_min + layout.region_count - 1) + ", origin " +
                godot::String::num(static_cast<double>(layout.origin)));
    const godot::Ref<godot::Image> hi = preview.height_image();
    const godot::Ref<godot::Image> ci = preview.control_image();
    const godot::Ref<godot::Image> co = preview.colour_image();
    auto shape_ok = [n](const godot::Ref<godot::Image>& image, godot::Image::Format format) {
        return image.is_valid() && image->get_width() == n && image->get_height() == n &&
               image->get_format() == format && !image->has_mipmaps();
    };
    c.check(shape_ok(hi, godot::Image::FORMAT_RF) && shape_ok(ci, godot::Image::FORMAT_RF) &&
                shape_ok(co, godot::Image::FORMAT_RGBA8),
            "T1 images height RF, control RF, colour RGBA8, all " + godot::String::num_int64(n) + " x " +
                godot::String::num_int64(n) + ", no mipmaps");
    if (!c.all) {
        godot::UtilityFunctions::print("UNMAPPED-TERRAIN-CHECK: FAIL");
        return 1;
    }

    // T2: every height equals the check's own formula exactly.
    {
        const godot::PackedByteArray data = hi->get_data();
        std::size_t bad = 0;
        std::size_t first_bad = 0;
        for (std::size_t k = 0; k < cells; ++k) {
            const float image_h = rf_pixel(data, k);
            if (image_h != own.h[k] || scene.heights[k] != own.h[k]) {
                if (bad == 0) {
                    first_bad = k;
                }
                ++bad;
            }
        }
        c.check(bad == 0, "T2 heights equal the formula at every cell (" + godot::String::num_int64(static_cast<int64_t>(bad)) +
                              " differ" + (bad > 0 ? ", first at index " + godot::String::num_int64(static_cast<int64_t>(first_bad)) : "") +
                              ")");
        c.check(scene.h_min == own.h_min && scene.h_max == own.h_max,
                "T2 min " + to_godot(decimal1(static_cast<double>(own.h_min))) + " m, max " +
                    to_godot(decimal1(static_cast<double>(own.h_max))) + " m");
        if (!scene.textured) {
            const godot::PackedByteArray control = ci->get_data();
            bool zero = true;
            for (int64_t k = 0; k < control.size(); ++k) {
                zero = zero && control[k] == 0;
            }
            c.check(zero, "T2 control map all 0.0f (flat colours)");
        }
    }

    // T3: colours against worldgen's render --layer biome.
    {
        const godot::PackedByteArray rgba = co->get_data();
        bool alpha = true;
        std::size_t magenta = 0;
        for (std::size_t k = 0; k < cells; ++k) {
            alpha = alpha && rgba[static_cast<int64_t>(4 * k + 3)] == 128;
            magenta += (rgba[static_cast<int64_t>(4 * k)] == 255 && rgba[static_cast<int64_t>(4 * k + 1)] == 0 &&
                        rgba[static_cast<int64_t>(4 * k + 2)] == 255)
                           ? std::size_t{1}
                           : std::size_t{0};
        }
        c.check(alpha, "T3 every alpha is 128");
        const godot::String png = options.reference_dir.path_join("biome.png");
        if (scene.textured) {
            Checker::info("T3 textured ground: colours checked against 5.1 below");
        } else if (godot::FileAccess::file_exists(png)) {
            const godot::Ref<godot::Image> ref = godot::Image::load_from_file(png);
            bool ok = ref.is_valid() && ref->get_width() == n && ref->get_height() == n;
            std::size_t bad = 0;
            if (ok) {
                ref->convert(godot::Image::FORMAT_RGB8);
                const godot::PackedByteArray rgb = ref->get_data();
                for (std::size_t k = 0; k < cells; ++k) {
                    for (std::size_t ch = 0; ch < 3; ++ch) {
                        if (rgb[static_cast<int64_t>(3 * k + ch)] != rgba[static_cast<int64_t>(4 * k + ch)]) {
                            ++bad;
                            break;
                        }
                    }
                }
            }
            c.check(ok && bad == 0, "T3 colour RGB equals biome.png at every cell (" + godot::String::num_int64(static_cast<int64_t>(bad)) +
                                        " differ)");
            c.check(magenta == 0, "T3 no magenta cell");
        } else {
            bool grey = true;
            for (std::size_t k = 0; k < cells; ++k) {
                grey = grey && rgba[static_cast<int64_t>(4 * k)] == 128 && rgba[static_cast<int64_t>(4 * k + 1)] == 128 &&
                       rgba[static_cast<int64_t>(4 * k + 2)] == 128;
            }
            c.check(grey, "T3 no biome.png: every cell grey");
            c.check(preview.status_text().contains("no biome layer: ground drawn grey, no vegetation"),
                    "T3 the status notes the missing biome layer");
        }
    }

    // T4: start position and HUD text.
    {
        const terrain::FlyState& start = preview.start_state();
        const float sx = 0.0f;
        const float sz = 6.0f * static_cast<float>(n);
        const float sy = own.h_max + 300.0f;
        c.check(start.x == sx && start.z == sz && start.y == sy && start.heading_deg == 0.0f &&
                    start.pitch_deg == -25.0f && start.speed_index == 4,
                "T4 start at (0, " + to_godot(decimal1(static_cast<double>(sy))) + ", " +
                    godot::String::num_int64(6 * n) + "), heading 0, pitch -25, 200 m/s");
        const godot::Camera3D* camera = preview.camera();
        const godot::Vector3 p = camera->get_position();
        const godot::Vector3 r = camera->get_rotation();
        c.check(p.x == sx && p.y == sy && p.z == sz && std::abs(r.x + 25.0f * 3.14159265f / 180.0f) < 1e-5f &&
                    std::abs(r.y) < 1e-6f,
                "T4 camera placed at the start");
        auto hud_case = [&](const char* name, bool inside, int x, int y, float X, float Z, float Y, float ground) {
            const std::string got = preview.hud_at(X, Y, Z, 0);
            const std::string want = expected_hud(own, world, scene, inside, x, y, X, Z, Y, ground, 200);
            c.check(got == want, godot::String("T4 HUD ") + name + ": '" + to_godot(got) + "'" +
                                     (got == want ? godot::String() : "\n          expected '" + to_godot(want) + "'"));
        };
        const int scx = n / 2;
        const int scy = 7 * n / 8;
        hud_case("at the start", true, scx, scy, sx, sz, sy, own.at(scx, scy));
        hud_case("at cell (0, 0)", true, 0, 0, own.X(0), own.Z(0), 5000.0f, own.at(0, 0));
        if (own.first_lake >= 0) {
            const int lx = own.first_lake % n;
            const int ly = own.first_lake / n;
            hud_case("at the first lake cell", true, lx, ly, own.X(lx), own.Z(ly), 4000.0f, own.at(lx, ly));
        } else {
            Checker::info("T4 this world has no lake cell");
        }
        const float ox = -8.0f * static_cast<float>(n) - 100.0f;
        hud_case("outside the map", false, 0, 0, ox, 0.0f, 3000.0f, own.at(0, n / 2));
    }

    // T6 / T7: Terrain3D present or absent.
    const bool want_present = options.expect_terrain3d == "present";
    std::vector<godot::Node*> terrains;
    for (int32_t i = 0; i < preview.get_child_count(); ++i) {
        godot::Node* child = preview.get_child(i);
        if (child->get_class() == "Terrain3D") {
            terrains.push_back(child);
        }
    }
    if (want_present) {
        c.check(terrains.size() == 1 && preview.terrain_node() == terrains.front(), "T6 one Terrain3D child");
        godot::Object* data = nullptr;
        if (preview.terrain_node() != nullptr) {
            data = preview.terrain_node()->call("get_data");
        }
        c.check(data != nullptr, "T6 Terrain3D has a data object");
        if (data != nullptr) {
            const godot::TypedArray<godot::Vector2i> locations = data->call("get_region_locations");
            std::set<std::pair<int, int>> got;
            for (int64_t i = 0; i < locations.size(); ++i) {
                const godot::Vector2i v = locations[i];
                got.insert({v.x, v.y});
            }
            std::set<std::pair<int, int>> want;
            for (int i = -(n / 512); i < -(n / 512) + n / 256; ++i) {
                for (int j = -(n / 512); j < -(n / 512) + n / 256; ++j) {
                    want.insert({i, j});
                }
            }
            c.check(got == want && static_cast<std::size_t>(locations.size()) == want.size(),
                    "T6 region locations: " + godot::String::num_int64(locations.size()) + ", expected " +
                        godot::String::num_int64(static_cast<int64_t>(want.size())));
            std::vector<std::pair<int, int>> points = {{0, 0},         {n - 1, 0}, {0, n - 1}, {n - 1, n - 1},
                                                       {n / 2, n / 2}, {own.highest % n, own.highest / n},
                                                       {own.lowest % n, own.lowest / n}};
            for (int k = 1; k <= 25; ++k) {
                points.emplace_back((37 * k) % n, (61 * k) % n);
            }
            int height_bad = 0;
            int colour_bad = 0;
            float worst_h = 0.0f;
            float worst_c = 0.0f;
            for (const auto& [x, y] : points) {
                const godot::Vector3 at(own.X(x), 0.0f, own.Z(y));
                const float got_h = static_cast<float>(static_cast<double>(data->call("get_height", at)));
                const float dh = std::abs(got_h - own.at(x, y));
                worst_h = std::max(worst_h, dh);
                height_bad += dh <= 0.001f ? 0 : 1;
                const godot::Color got_c = data->call("get_color", at);
                const std::size_t k = static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x);
                const float want_rgb[3] = {static_cast<float>(scene.colours[4 * k]) / 255.0f,
                                           static_cast<float>(scene.colours[4 * k + 1]) / 255.0f,
                                           static_cast<float>(scene.colours[4 * k + 2]) / 255.0f};
                const float dc = std::max({std::abs(got_c.r - want_rgb[0]), std::abs(got_c.g - want_rgb[1]),
                                           std::abs(got_c.b - want_rgb[2])});
                worst_c = std::max(worst_c, dc);
                colour_bad += dc <= 1.0f / 255.0f + 1e-6f ? 0 : 1;
            }
            c.check(height_bad == 0, "T6 get_height at " + godot::String::num_int64(static_cast<int64_t>(points.size())) +
                                         " points within 0.001 m (worst " + godot::String::num(static_cast<double>(worst_h), 4) + ")");
            c.check(colour_bad == 0, "T6 get_color at the same points within 1/255 (worst " +
                                         godot::String::num(static_cast<double>(worst_c * 255.0f), 3) + "/255)");
        }
    } else {
        c.check(terrains.empty() && preview.terrain_node() == nullptr, "T7 no Terrain3D node");
        c.check(preview.status_text().contains(TerrainPreview::NO_TERRAIN3D_TEXT), "T7 the status says Terrain3D is absent");
        const godot::MeshInstance3D* sea = preview.sea();
        c.check(sea != nullptr && sea->get_mesh().is_valid(), "T7 the sea plane is present");
        c.check(preview.camera() != nullptr && preview.camera()->is_current(), "T7 the camera is present and current");
    }

    // T8: 300 frames of scripted flight stay above ground_clamp + 2.
    {
        terrain::FlyState s = preview.start_state();
        const float bound = 16.0f * static_cast<float>(n);
        const auto ground = [&own](float X, float Z) { return own.ground(X, Z); };
        const float tx = own.X(own.highest % n);
        const float tz = own.Z(own.highest / n);
        bool ok = true;
        float lowest_margin = 1e30f;
        for (int frame = 0; frame < 300; ++frame) {
            terrain::FlyInput in;
            in.forward = 1;
            if (frame < 150) {
                in.yaw_deg = 0.5f;
            } else {
                // Dive at the highest cell: face it and point down.
                const float want_heading = std::atan2(tx - s.x, -(tz - s.z)) * 180.0f / 3.14159265f;
                in.yaw_deg = want_heading - s.heading_deg;
                in.pitch_deg = -89.0f - s.pitch_deg;
                in.boost = true;
                s.speed_index = 7;
            }
            terrain::step(s, in, 1.0f / 60.0f, ground, bound);
            const float margin = s.y - (own.ground(s.x, s.z) + 2.0f);
            lowest_margin = std::min(lowest_margin, margin);
            ok = ok && margin >= -1e-3f;
        }
        c.check(ok, "T8 300 frames above ground_clamp + 2 (lowest margin " +
                        to_godot(decimal1(static_cast<double>(lowest_margin))) + " m)");
    }

    // V3D-02: T3 (default mode), T9 to T15.
    run_ground_checks(preview, options, c);

    // V16: opening a world again replaces the first (one Terrain3D node,
    // the same heights read back).
    if (want_present) {
        const int centre = (n / 2) * n + n / 2;
        const float want_h = own.h[static_cast<std::size_t>(centre)];
        const bool reopened = preview.open_world(options.world_path);
        int count = 0;
        for (int32_t i = 0; i < preview.get_child_count(); ++i) {
            count += preview.get_child(i)->get_class() == "Terrain3D" ? 1 : 0;
        }
        bool height_ok = false;
        if (reopened && preview.terrain_node() != nullptr) {
            godot::Object* data = preview.terrain_node()->call("get_data");
            if (data != nullptr) {
                const double h = data->call("get_height", godot::Vector3(own.X(n / 2), 0.0f, own.Z(n / 2)));
                height_ok = std::abs(static_cast<float>(h) - want_h) <= 0.001f;
            }
        }
        c.check(reopened && count == 1 && height_ok, "T6 reopening the world leaves one Terrain3D node with its heights");
    }

    // V3D-02 T16: keys T and G (G reopens the world twice).
    run_key_checks(preview, options, c);

    // V3D-03: T17 here, then the path (T18 to T24) over frames.
    if (path != nullptr) {
        *path = start_near_check(preview, options, c);
        if (*path) {
            return -1;
        }
    }
    godot::UtilityFunctions::print(c.all ? "UNMAPPED-TERRAIN-CHECK: PASS" : "UNMAPPED-TERRAIN-CHECK: FAIL");
    return c.all ? 0 : 1;
}

}  // namespace unmapped
