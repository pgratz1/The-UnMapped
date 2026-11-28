// The terrain check's ground-detail part (spec V3D-06 15.2, T41 to T53),
// for runs without `--ground-detail off --no-boulders`. It keeps its own
// copy of section 9's control rule and of section 7's pad rule and builds
// the field with the plain build_ground_field (formulas pinned by U53 to
// U58).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "terrain/boulders.hpp"
#include "terrain/detail.hpp"
#include "terrain/markers3d.hpp"
#include "terrain/terrain_check.hpp"
#include "terrain/terrain_check_internal.hpp"
#include "terrain/terrain_preview.hpp"
#include "terrain/vegetation.hpp"

namespace unmapped {

namespace {

godot::String num(std::uint64_t v) { return godot::String::num_int64(static_cast<int64_t>(v)); }
godot::String hex(std::uint64_t v) {
    char buffer[24];
    std::snprintf(buffer, sizeof buffer, "%016llx", static_cast<unsigned long long>(v));
    return godot::String(buffer);
}

std::size_t idx(int n, int x, int y) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x);
}

std::int64_t mm_of(float h) { return std::llround(static_cast<double>(h) * 1000.0); }

std::int64_t own_fdiv(std::int64_t a, std::int64_t b) { return (a % b != 0 && a < 0) ? a / b - 1 : a / b; }

// The check's own copy of 9's control rule at fine vertex (x, y).
std::uint32_t own_control(const terrain::DetailPlan& p, const std::vector<float>& h, int x, int y) {
    if (!p.textured || p.control.empty()) {
        return 0;
    }
    const int v = p.v;
    const auto H = [&](int xx, int yy) {
        return mm_of(h[idx(v, std::clamp(xx, 0, v - 1), std::clamp(yy, 0, v - 1))]);
    };
    const auto warp = terrain::fine_warp(p, x, y);
    const std::int64_t U = 16 * static_cast<std::int64_t>(x) * p.s + warp[0];
    const std::int64_t V = 16 * static_cast<std::int64_t>(y) * p.s + warp[1];
    const int i = static_cast<int>(std::clamp<std::int64_t>(own_fdiv(U, 256), 0, p.n - 1));
    const int j = static_cast<int>(std::clamp<std::int64_t>(own_fdiv(V, 256), 0, p.n - 1));
    const std::int64_t fu = std::clamp<std::int64_t>(U - 256 * i, 0, 255);
    const std::int64_t fv = std::clamp<std::int64_t>(V - 256 * j, 0, 255);
    const int i1 = std::min(i + 1, p.n - 1);
    const int j1 = std::min(j + 1, p.n - 1);
    const std::uint32_t cc[4] = {p.control[idx(p.n, i, j)], p.control[idx(p.n, i1, j)], p.control[idx(p.n, i, j1)],
                                 p.control[idx(p.n, i1, j1)]};
    const std::int64_t w[4] = {(256 - fu) * (256 - fv), fu * (256 - fv), (256 - fu) * fv, fu * fv};
    std::int64_t acc[17] = {};
    int nearest = 0;
    for (int q = 0; q < 4; ++q) {
        const std::int64_t blend = (cc[q] >> 14) & 0xFF;
        acc[(cc[q] >> 27) & 0x1F] += w[q] * (255 - blend);
        acc[(cc[q] >> 22) & 0x1F] += w[q] * blend;
        nearest = w[q] > w[nearest] ? q : nearest;
    }
    const auto is_base = [](int k) { return k <= 10 || k == 13 || k == 14; };
    int k1 = 0;
    for (int k = 1; k <= 16; ++k) {
        if (is_base(k) && acc[k] > acc[k1]) {
            k1 = k;
        }
    }
    if (acc[k1] == 0) {
        k1 = static_cast<int>((cc[nearest] >> 27) & 0x1F);
        k1 = is_base(k1) ? k1 : 0;
    }
    int k2 = k1 == 0 ? 1 : 0;
    for (int k = 0; k <= 16; ++k) {
        if (k != k1 && acc[k] > acc[k2]) {
            k2 = k;
        }
    }
    const std::int64_t w1 = acc[k1];
    const std::int64_t w2 = acc[k2];
    std::uint32_t overlay = w2 == 0 ? static_cast<std::uint32_t>(k1) : static_cast<std::uint32_t>(k2);
    std::int64_t blend = w2 == 0 ? 0 : (255 * w2 + (w1 + w2) / 2) / (w1 + w2);
    if (k1 <= 10) {
        const std::int64_t dx = H(x + 1, y) - H(x - 1, y);
        const std::int64_t dz = H(x, y + 1) - H(x, y - 1);
        const std::int64_t g = terrain::isqrt64(dx * dx + dz * dz);
        const std::int64_t D = 2000 * static_cast<std::int64_t>(p.s);
        const std::int64_t rock = g * 320 <= 224 * D ? 0 : g >= D ? 255 : ((320 * g - 224 * D) * 255 + 48 * D) / (96 * D);
        if (overlay == 12) {
            const std::int64_t gs = g * 320 / D;
            const std::int64_t full = terrain::SNOW_HOLD_FULL_G, none = terrain::SNOW_HOLD_NONE_G;
            const std::int64_t hold = gs <= full ? 255 : gs >= none ? 0 : ((none - gs) * 255 + (none - full) / 2) / (none - full);
            blend = blend * hold / 255;
        }
        if (rock > blend && !(overlay == 12 && blend >= 128)) {
            overlay = p.look.rock ? terrain::rock_variant(x, y, static_cast<std::int32_t>(own_fdiv(H(x, y), 100))) : 11u;
            blend = rock;
        }
    }
    return (static_cast<std::uint32_t>(k1) << 27) | (overlay << 22) | (static_cast<std::uint32_t>(blend) << 14);
}

}  // namespace

bool detail_run(const TerrainPreview& preview) {
    return preview.ground_detail_option() != 0 || !preview.no_boulders_option();
}

void run_detail_checks(TerrainPreview& preview, const TerrainCheckOptions& options, TerrainChecker& c) {
    const TerrainScene& scene = preview.scene();
    const terrain::Layout& layout = *scene.layout;
    const int n = layout.n;
    const terrain::GroundField& field = scene.field;
    const terrain::Layout& fl = field.layout;
    const int s = scene.detail_spacing;
    const godot::String status = preview.status_text();
    const bool present = options.expect_terrain3d == "present";

    if (s == 0) {
        // T49: --ground-detail off: V3D-05's ground.
        const bool coarse = field.f == 1 && fl.n == n && field.heights == scene.heights;
        bool image_ok = false;
        if (preview.height_image().is_valid()) {
            const godot::PackedByteArray bytes = preview.height_image()->get_data();
            image_ok = static_cast<std::size_t>(bytes.size()) == scene.heights.size() * sizeof(float) &&
                       std::memcmp(bytes.ptr(), scene.heights.data(), scene.heights.size() * sizeof(float)) == 0;
        }
        c.check(coarse && image_ok && status.contains(terrain::DETAIL_OFF_STATUS),
                "T49 --ground-detail off: the field is the 16 m buffer, the height image byte-equal, the status line");
        if (preview.boulders_on()) {
            std::size_t bad = 0;
            std::size_t total = 0;
            const int m = n / 16;
            for (int q = 0; q < m * m; q += 7) {
                for (const auto& list : terrain::near_boulders(preview.boulder_inputs(), q % m, q / m)) {
                    for (const terrain::Boulder& b : list) {
                        ++total;
                        const float g = terrain::ground_low(layout, scene.heights, b.x, b.z);
                        bad += b.y <= g ? 0u : 1u;
                    }
                }
            }
            c.check(bad == 0, "T49 boulders on ground_low of the 16 m heights (" + num(total) + " checked, " + num(bad) +
                                  " above)");
        }
    } else {
        // T51: status lines; the spacing used.
        const int asked = preview.ground_detail_option();
        const bool limited = asked == 2 && n >= 2048;
        c.check(s == (limited ? 4 : asked) && status.contains("ground detail: " + godot::String::num_int64(s) + " m, ") &&
                    (status.contains(terrain::DETAIL_LIMIT_NOTE) == limited),
                "T51 ground detail " + godot::String::num_int64(s) + " m, " + godot::String::num_int64(fl.n) + " x " +
                    godot::String::num_int64(fl.n) + " vertices; status line");
        if (asked == 2 && n == 512) {
            c.check(fl.n == 4096, "T51 --ground-detail 2 on the 512 world: V = 4096");
        }
        const terrain::Layout want = terrain::fine_layout(layout, s);
        c.check(fl.n == want.n && fl.region_size == want.region_size && fl.region_count == want.region_count &&
                    field.f == 16 / s && field.heights.size() == static_cast<std::size_t>(fl.n) * static_cast<std::size_t>(fl.n),
                "T41 fine layout V " + godot::String::num_int64(fl.n) + ", region " + godot::String::num_int64(fl.region_size) +
                    ", " + godot::String::num_int64(fl.region_count * fl.region_count) + " regions");

        // T42 (images) and T52: the field equals the plain build bit for bit.
        const terrain::DetailBuffers one = terrain::build_detail_buffers(*scene.plan, 1, true);
        const std::uint64_t h_field = terrain::fnv1a64(field.heights.data(), field.heights.size() * sizeof(float));
        const std::uint64_t h_own = terrain::fnv1a64(one.heights.data(), one.heights.size() * sizeof(float));
        c.check(h_field == h_own, "T52 the field equals one band on one thread (FNV-1a 64 " + hex(h_field) + ")");
        const godot::Ref<godot::Image> hi = preview.height_image();
        const godot::Ref<godot::Image> ci = preview.control_image();
        const godot::Ref<godot::Image> co = preview.colour_image();
        const bool shapes = hi.is_valid() && ci.is_valid() && co.is_valid() && hi->get_width() == fl.n &&
                            ci->get_width() == fl.n && co->get_width() == fl.n;
        c.check(shapes, "T42 the height, control and colour images are V x V");
        if (shapes) {
            const godot::PackedByteArray hb = hi->get_data();
            c.check(std::memcmp(hb.ptr(), field.heights.data(), field.heights.size() * sizeof(float)) == 0,
                    "T42 the height image equals the field bit for bit");
            const godot::PackedByteArray cb = ci->get_data();
            const godot::PackedByteArray ob = co->get_data();
            const std::uint64_t hc = terrain::fnv1a64(cb.ptr(), static_cast<std::size_t>(cb.size()));
            const std::uint64_t ho = terrain::fnv1a64(ob.ptr(), static_cast<std::size_t>(ob.size()));
            const bool same = terrain::fnv1a64(one.control.data(), one.control.size() * 4) == hc &&
                              terrain::fnv1a64(one.colour.data(), one.colour.size()) == ho;
            c.check(same, "T52 control and colour equal one band on one thread (" + hex(hc) + ", " + hex(ho) + ")");
            TerrainChecker::info("detail hashes " + hex(h_field) + " " + hex(hc) + " " + hex(ho));
            // T43: the check's own control rule at fixed vertices and four windows.
            const auto control_at = [&cb](std::size_t k) {
                std::uint32_t v = 0;
                std::memcpy(&v, cb.ptr() + 4 * k, 4);
                return v;
            };
            std::vector<std::pair<int, int>> points;
            for (int k = 0; k < 4096; ++k) {
                points.emplace_back((37 * k) % fl.n, (61 * k) % fl.n);
            }
            // Windows: a shore, a painted river, a biome border, the highest peak.
            std::vector<std::pair<int, int>> corners;
            const auto find_cell = [&](auto pred) -> std::pair<int, int> {
                for (int y = 8; y < n - 8; ++y) {
                    for (int x = 8; x < n - 8; ++x) {
                        if (pred(x, y)) {
                            return {x, y};
                        }
                    }
                }
                return {n / 2, n / 2};
            };
            const auto& cells = scene.ground;
            if (!cells.type.empty()) {
                corners.push_back(find_cell([&](int x, int y) { return cells.type[idx(n, x, y)] == 14 && cells.type[idx(n, x + 1, y)] <= 10; }));
                corners.push_back(find_cell([&](int x, int y) { return cells.painted[idx(n, x, y)] > 0; }));
                corners.push_back(find_cell([&](int x, int y) {
                    const auto a = cells.type[idx(n, x, y)], b = cells.type[idx(n, x + 1, y)];
                    return a <= 10 && b <= 10 && a != b && a != 0 && b != 0;
                }));
            }
            corners.emplace_back(scene.peak.x, scene.peak.y);
            for (const auto& [cx, cy] : corners) {
                const int x0 = std::clamp(cx * field.f - 32, 0, fl.n - 64);
                const int y0 = std::clamp(cy * field.f - 32, 0, fl.n - 64);
                for (int y = y0; y < y0 + 64; ++y) {
                    for (int x = x0; x < x0 + 64; ++x) {
                        points.emplace_back(x, y);
                    }
                }
            }
            std::size_t bad = 0;
            std::size_t bad_bits = 0;
            std::size_t bad_colour = 0;
            for (const auto& [x, y] : points) {
                const std::size_t k = idx(fl.n, x, y);
                const std::uint32_t got = control_at(k);
                bad += got == own_control(*scene.plan, field.heights, x, y) ? 0u : 1u;
                const std::uint32_t base = got >> 27;
                const std::uint32_t overlay = (got >> 22) & 0x1F;
                bad_bits += ((base <= 10 || base == 13 || base == 14) && overlay <= 16 && (got & 0x3FFFu) == 0) ? 0u : 1u;
                const auto rgba = terrain::fine_colour(*scene.plan, x, y);
                for (std::size_t ch = 0; ch < 4; ++ch) {
                    bad_colour += ob[static_cast<int64_t>(4 * k + ch)] == rgba[ch] ? 0u : 1u;
                }
            }
            c.check(bad == 0 && bad_bits == 0 && bad_colour == 0,
                    "T43 control and colour at " + num(points.size()) + " vertices equal the check's own rule (" + num(bad) +
                        " controls, " + num(bad_colour) + " colour channels differ; " + num(bad_bits) + " invalid bits)");
        }

        // T41 / T42 against Terrain3D.
        if (present && preview.terrain_node() != nullptr) {
            godot::Node3D* t3d = preview.terrain_node();
            const double spacing = t3d->get("vertex_spacing");
            const int64_t region = t3d->get("region_size");
            const int64_t lods = t3d->get("mesh_lods");
            godot::Object* data = t3d->call("get_data");
            std::set<std::pair<int, int>> got;
            if (data != nullptr) {
                const godot::TypedArray<godot::Vector2i> locations = data->call("get_region_locations");
                for (int64_t i = 0; i < locations.size(); ++i) {
                    const godot::Vector2i v = locations[i];
                    got.insert({v.x, v.y});
                }
            }
            std::set<std::pair<int, int>> want_set;
            for (int i = fl.region_min; i < fl.region_min + fl.region_count; ++i) {
                for (int j = fl.region_min; j < fl.region_min + fl.region_count; ++j) {
                    want_set.insert({i, j});
                }
            }
            c.check(spacing == static_cast<double>(s) && region == fl.region_size && lods == terrain::detail_mesh_lods(s) &&
                        got == want_set,
                    "T41 Terrain3D: vertex_spacing " + godot::String::num(spacing) + ", region_size " +
                        godot::String::num_int64(region) + ", mesh_lods " + godot::String::num_int64(lods) + ", " +
                        num(got.size()) + " regions centred");
            if (data != nullptr) {
                std::vector<std::pair<int, int>> pts = {{0, 0}, {n - 1, 0}, {0, n - 1}, {n - 1, n - 1}, {n / 2, n / 2}};
                for (int k = 1; k <= 27; ++k) {
                    pts.emplace_back((37 * k) % n, (61 * k) % n);
                }
                for (auto& p : pts) {
                    p = {p.first * field.f, p.second * field.f};
                }
                for (int k = 0; k < 256; ++k) {
                    pts.emplace_back((97 * k + 13) % fl.n, (89 * k + 7) % fl.n);
                }
                int bad = 0;
                float worst = 0.0f;
                for (const auto& [x, y] : pts) {
                    const terrain::Vec2 w = terrain::cell_to_world(fl, x, y);
                    const float h = static_cast<float>(static_cast<double>(data->call("get_height", godot::Vector3(w.x, 0.0f, w.z))));
                    const float d = std::abs(h - field.heights[idx(fl.n, x, y)]);
                    worst = std::max(worst, d);
                    bad += d <= 0.001f ? 0 : 1;
                }
                c.check(bad == 0, "T42 get_height at " + num(pts.size()) + " fine vertices equals the field within 0.001 m (worst " +
                                      godot::String::num(static_cast<double>(worst), 4) + ")");
            }
        }

        // T44: field invariants.
        {
            const terrain::DetailPlan& p = *scene.plan;
            const int f = field.f;
            const auto near_pad = [&p](std::int64_t xm, std::int64_t zm) {
                for (const terrain::Pad& pad : p.pads) {
                    if (std::max(std::llabs(xm - pad.x), std::llabs(zm - pad.z)) < pad.r0 + terrain::PAD_FALL) {
                        return true;
                    }
                }
                return false;
            };
            std::size_t lake_bad = 0, lake_n = 0, wet_bad = 0, wet_n = 0;
            const std::vector<std::uint8_t>& water = scene.water;
            const auto wet_noise = [&](int x, int y) {
                const std::size_t k = idx(n, x, y);
                return (!water.empty() && water[k] != 0) || (!scene.ground.type.empty() && (scene.ground.painted[k] > 0 ||
                                                                                            scene.ground.type[k] > 10));
            };
            for (int y = 1; y < n - 1; ++y) {
                for (int x = 1; x < n - 1; ++x) {
                    const std::int32_t h0 = p.hd[idx(n, x, y)];
                    if (!water.empty() && water[idx(n, x, y)] == 2 && water[idx(n, x + 1, y)] == 2 && water[idx(n, x, y + 1)] == 2 &&
                        water[idx(n, x + 1, y + 1)] == 2 && p.hd[idx(n, x + 1, y)] == h0 && p.hd[idx(n, x, y + 1)] == h0 &&
                        p.hd[idx(n, x + 1, y + 1)] == h0 && !near_pad(16 * x, 16 * y)) {
                        for (int b = 0; b <= f; ++b) {
                            for (int a = 0; a <= f; ++a) {
                                ++lake_n;
                                lake_bad += mm_of(field.heights[idx(fl.n, x * f + a, y * f + b)]) == 100 * h0 ? 0u : 1u;
                            }
                        }
                    }
                    if (wet_noise(x, y) && wet_noise(x - 1, y) && wet_noise(x, y - 1) && wet_noise(x - 1, y - 1) &&
                        !near_pad(16 * x, 16 * y)) {
                        ++wet_n;
                        wet_bad += mm_of(field.heights[idx(fl.n, x * f, y * f)]) == 100 * h0 ? 0u : 1u;
                    }
                }
            }
            c.check(lake_bad == 0 && wet_bad == 0, "T44 lake quads flat at their surface (" + num(lake_n) + " vertices, " +
                                                       num(lake_bad) + " differ); wet-only coarse vertices at 100 hd (" +
                                                       num(wet_n) + ", " + num(wet_bad) + " differ)");
            // Pads: the check's own rule for a lone, fully level pad.
            std::size_t pad_full = 0, pad_bad = 0, pad_vertices = 0;
            for (std::size_t i = 0; i < p.pads.size(); ++i) {
                const terrain::Pad& pad = p.pads[i];
                const int reach = pad.r0 / 16;
                std::int32_t lo = p.hd[idx(n, pad.cell_x, pad.cell_y)];
                std::int32_t top = lo;
                for (int y = std::max(0, pad.cell_y - reach); y <= std::min(n - 1, pad.cell_y + reach); ++y) {
                    for (int x = std::max(0, pad.cell_x - reach); x <= std::min(n - 1, pad.cell_x + reach); ++x) {
                        lo = std::min(lo, p.hd[idx(n, x, y)]);
                        top = std::max(top, p.hd[idx(n, x, y)]);
                    }
                }
                if (top - lo > terrain::PAD_FLAT_DM) {
                    continue;
                }
                bool alone = true;
                for (std::size_t o = 0; o < p.pads.size(); ++o) {
                    if (o != i && std::max(std::llabs(p.pads[o].x - pad.x), std::llabs(p.pads[o].z - pad.z)) <
                                      pad.r0 + p.pads[o].r0 + terrain::PAD_FALL) {
                        alone = false;
                    }
                }
                bool dry = true;
                for (int y = std::max(0, pad.cell_y - reach - 1); y <= std::min(n - 1, pad.cell_y + reach + 1); ++y) {
                    for (int x = std::max(0, pad.cell_x - reach - 1); x <= std::min(n - 1, pad.cell_x + reach + 1); ++x) {
                        dry = dry && p.factors.w_level[idx(n, x, y)] == 1000;
                    }
                }
                if (!alone || !dry) {
                    continue;
                }
                ++pad_full;
                const std::int64_t want_mm = 100 * static_cast<std::int64_t>(p.hd[idx(n, pad.cell_x, pad.cell_y)]);
                for (std::int64_t zm = pad.z - pad.r0; zm <= pad.z + pad.r0; zm += s) {
                    for (std::int64_t xm = pad.x - pad.r0; xm <= pad.x + pad.r0; xm += s) {
                        if (xm < 0 || zm < 0 || xm >= 16 * n || zm >= 16 * n) {
                            continue;
                        }
                        ++pad_vertices;
                        pad_bad += mm_of(field.heights[idx(fl.n, static_cast<int>(xm / s), static_cast<int>(zm / s))]) == want_mm ? 0u : 1u;
                    }
                }
            }
            c.check(pad_bad == 0, "T44 " + num(pad_full) + " lone level pads of " + num(p.pads.size()) + " flat at P (" +
                                      num(pad_vertices) + " vertices, " + num(pad_bad) + " differ)");
            if (preview.no_markers_option()) {
                c.check(p.pads.empty(), "T44 --no-markers: no pads");
            }
            std::size_t anchor_bad = 0;
            for (const terrain::Marker3D& m : preview.markers()) {
                anchor_bad += m.Y == field.heights[idx(fl.n, m.cell_x * f, m.cell_y * f)] ? 0u : 1u;
            }
            c.check(anchor_bad == 0, "T44 every marker's Y is the field at its anchor vertex (" + num(preview.markers().size()) +
                                         " markers)");
            // T46: on a fully levelled pad every house part's base corners at P
            // (parts whose four corners lie on fine quads all at the anchor's
            // height), sunk by SINK only.
            std::size_t parts = 0, parts_bad = 0;
            const auto level_quad = [&](float X, float Z, float Y) {
                const float u = X / fl.spacing + static_cast<float>(fl.n / 2);
                const float w = Z / fl.spacing + static_cast<float>(fl.n / 2);
                const int i = std::clamp(static_cast<int>(std::floor(u)), 0, fl.n - 2);
                const int j = std::clamp(static_cast<int>(std::floor(w)), 0, fl.n - 2);
                return field.heights[idx(fl.n, i, j)] == Y && field.heights[idx(fl.n, i + 1, j)] == Y &&
                       field.heights[idx(fl.n, i, j + 1)] == Y && field.heights[idx(fl.n, i + 1, j + 1)] == Y;
            };
            for (const terrain::Marker3D& m : preview.markers()) {
                if (m.rec.kind != terrain::MarkerKind::settlement) {
                    continue;
                }
                for (const terrain::GroundedPart& part : terrain::grounded_parts(m, fl, field.heights)) {
                    bool level = true;
                    float worst = 0.0f;
                    for (int k = 0; k < 4; ++k) {
                        const float X = m.X + part.corners[static_cast<std::size_t>(2 * k)];
                        const float Z = m.Z + part.corners[static_cast<std::size_t>(2 * k + 1)];
                        level = level && level_quad(X, Z, m.Y);
                        worst = std::max(worst, std::abs(terrain::ground_height(field, X, Z) - m.Y));
                    }
                    if (level) {
                        ++parts;
                        parts_bad += (worst <= 1e-3f && part.bottom >= -terrain::SINK - 1e-3f) ? 0u : 1u;
                    }
                }
            }
            c.check(parts > 0 || preview.markers().empty() || p.pads.empty() ? parts_bad == 0 : false,
                    "T46 house parts on fully levelled pad ground: corners at P, sunk at most SINK (" + num(parts) +
                        " parts, " + num(parts_bad) + " off)");
        }

        // T45: vegetation X, Z, yaw, scale, tint and the far set equal the
        // --ground-detail off placement; Y on the field.
        if (scene.tiers) {
            terrain::VegetationInputs off = preview.vegetation_inputs();
            off.ground = nullptr;
            const terrain::FarTier coarse = terrain::place_far_tier(off, preview.instance_cap());
            std::size_t differ = 0, ybad = 0;
            for (std::size_t k = 0; k < 5; ++k) {
                const auto& a = scene.placement.kinds[k];
                const auto& b = coarse.placement.kinds[k];
                if (a.size() != b.size()) {
                    differ += std::max(a.size(), b.size());
                    continue;
                }
                for (std::size_t i = 0; i < a.size(); ++i) {
                    differ += (a[i].x == b[i].x && a[i].z == b[i].z && a[i].yaw1024 == b[i].yaw1024 &&
                               a[i].scale_pm == b[i].scale_pm && a[i].tint == b[i].tint)
                                  ? 0u
                                  : 1u;
                    ybad += a[i].y == terrain::ground_height(field, a[i].x, a[i].z) ? 0u : 1u;
                }
            }
            c.check(differ == 0 && ybad == 0 && coarse.threshold == scene.far_threshold &&
                        coarse.far_candidates == scene.far_candidates,
                    "T45 far tier: X, Z, yaw, scale, tint, T and S_far equal the 16 m placement (" + num(differ) +
                        " differ), Y on ground_height of the field (" + num(ybad) + " differ)");
            // Near cells: counts equal at 16 cells.
            const int m = n / 16;
            std::size_t cell_bad = 0;
            for (int q = 0; q < m * m; q += std::max(1, m * m / 16)) {
                const auto a = terrain::near_instances(preview.vegetation_inputs(), q % m, q / m, scene.far_threshold);
                const auto b = terrain::near_instances(off, q % m, q / m, scene.far_threshold);
                for (std::size_t k = 0; k < 5; ++k) {
                    cell_bad += a[k].size() == b[k].size() ? 0u : 1u;
                }
            }
            c.check(cell_bad == 0, "T45 near cells: the same counts per kind as the 16 m placement");
        }

        // T48: camera and HUD on the field.
        {
            terrain::FlyState st = preview.start_state();
            const float bound = 16.0f * static_cast<float>(n);
            const auto ground = [&field](float X, float Z) { return terrain::ground_top(field, X, Z); };
            bool ok = true;
            for (int frame = 0; frame < 300; ++frame) {
                terrain::FlyInput in;
                in.forward = 1;
                in.pitch_deg = frame < 150 ? -0.6f : 0.0f;
                in.boost = frame >= 150;
                terrain::step(st, in, 1.0f / 60.0f, ground, bound);
                ok = ok && st.y >= terrain::ground_top(field, st.x, st.z) + 2.0f - 1e-3f;
            }
            const float X = 123.0f, Z = -77.0f, Y = 2500.0f;
            const std::string hud = preview.hud_at(X, Y, Z, 0);
            const std::string above =
                terrain::one_decimal(static_cast<double>(Y - terrain::sample_bilinear(fl, field.heights, X, Z))) + " above ground";
            c.check(ok && hud.find(above) != std::string::npos,
                    "T48 300 frames above ground_top + 2 on the field; the HUD's above-ground value from the field");
        }

        // T50: key J off and back (in the default run only: each J reopens
        // the world, and the debug check must stay well inside its timeout).
        if (present && s == 4 && n == 512 && !preview.no_markers_option() && preview.detail_threads_option() != 0) {
            const terrain::FlyState before = preview.fly_state();
            const bool off = preview.toggle_detail() && preview.scene().detail_spacing == 0 &&
                             preview.status_text().contains(terrain::DETAIL_OFF_STATUS);
            const bool on = preview.toggle_detail() && preview.scene().detail_spacing == s;
            int count = 0;
            for (int32_t i = 0; i < preview.get_child_count(); ++i) {
                count += preview.get_child(i)->get_class() == "Terrain3D" ? 1 : 0;
            }
            const terrain::FlyState after = preview.fly_state();
            const std::uint64_t again = terrain::fnv1a64(preview.scene().field.heights.data(),
                                                         preview.scene().field.heights.size() * sizeof(float));
            c.check(off && on && count == 1 && again == h_field && after.x == before.x && after.y == before.y &&
                        after.z == before.z,
                    "T50 key J: off and back, one Terrain3D node, the camera kept, the field hash equal");
        }
    }

    // T47 (static part): NearBoulders and the status line.
    const bool want_boulders = !preview.no_boulders_option() && scene.tiers && preview.veg_radius() > 0.0 && !scene.ground.type.empty();
    c.check(preview.boulders_on() == want_boulders &&
                (want_boulders ? status.contains("boulders: ") && status.contains(" per near cell at most, near tier only")
                               : (scene.ground.type.empty() || status.contains("boulders: off ("))),
            godot::String("T47 boulders ") + (want_boulders ? "on" : "off") + " with the status line");
    if (!want_boulders) {
        c.check(preview.boulders_root() == nullptr, "T47 no NearBoulders node");
    } else {
        std::size_t excluded = 0, on_pad = 0, total = 0;
        const int m = n / 16;
        const terrain::BoulderInputs& in = preview.boulder_inputs();
        for (int q = 0; q < m * m; q += 3) {
            for (const auto& list : terrain::near_boulders(in, q % m, q / m)) {
                for (const terrain::Boulder& b : list) {
                    ++total;
                    const terrain::CellHit h = terrain::world_to_cell(layout, b.x, b.z);
                    const std::size_t k = idx(n, h.x, h.y);
                    const std::uint8_t t = scene.ground.type[k];
                    excluded += (t < 1 || t > 10 || scene.ground.painted[k] > 0 ||
                                 (!scene.water.empty() && scene.water[k] != 0) || scene.elevation[k] <= 0)
                                    ? 1u
                                    : 0u;
                    on_pad += terrain::pad_noise_factor(*scene.plan, 16 * static_cast<std::int64_t>(h.x),
                                                        16 * static_cast<std::int64_t>(h.y)) == 0
                                  ? 1u
                                  : 0u;
                }
            }
        }
        c.check(excluded == 0 && on_pad == 0, "T47 " + num(total) + " boulders: none on an excluded cell (" + num(excluded) +
                                                  ") or within a full pad (" + num(on_pad) + ")");
    }

    // T53: every mine's beacon colour.
    std::size_t mines = 0, bad_mine = 0;
    for (const terrain::Marker3D& m : preview.markers()) {
        if (m.rec.kind == terrain::MarkerKind::mine) {
            ++mines;
            bad_mine += m.beacon_colour == terrain::MINE_BEACON ? 0u : 1u;
        }
    }
    if (mines == 0) {
        TerrainChecker::info("T53 this world has no mine");
    } else {
        c.check(bad_mine == 0, "T53 " + num(mines) + " mine beacons are MINE_BEACON 0xD6A840");
    }
}

}  // namespace unmapped
