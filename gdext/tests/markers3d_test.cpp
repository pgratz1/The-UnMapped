// Spec V3D-05 12.1, U40 to U51: the markers of the 3D preview (no Godot,
// no generated world; hand-made records and height grids on a 512 layout).

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "terrain/ground.hpp"
#include "terrain/markers3d.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/inspect/markers.hpp"

using namespace unmapped::terrain;

namespace {

constexpr int N = 512;

Layout layout512() {
    std::string problem;
    const auto layout = layout_for(N, N, problem);
    REQUIRE(layout.has_value());
    return *layout;
}

std::vector<float> flat_grid(float h = 100.0f) {
    return std::vector<float>(static_cast<std::size_t>(N) * N, h);
}

std::vector<float> slope_grid() {
    std::vector<float> g(static_cast<std::size_t>(N) * N);
    const float rise = 16.0f * std::tan(30.0f * 3.14159265f / 180.0f);
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            g[static_cast<std::size_t>(y) * N + static_cast<std::size_t>(x)] = 100.0f + rise * static_cast<float>(x);
        }
    }
    return g;
}

std::vector<float> spike_grid() {
    std::vector<float> g = flat_grid();
    g[static_cast<std::size_t>(200) * N + 200] = 400.0f;
    return g;
}

MarkerRecord settlement(std::int64_t id, int x, int y, int kind, std::int64_t pop, bool capital = false,
                        bool declining = false) {
    MarkerRecord r;
    r.kind = MarkerKind::settlement;
    r.id = id;
    r.x = x;
    r.y = y;
    r.settlement_kind = kind;
    r.population = pop;
    r.capital = capital;
    r.declining = declining;
    r.colour = 0x00BEC3;
    r.name = "Settlement " + std::to_string(id);
    r.kind_word = std::string(worldgen::history::settlement_kind_word(kind)) + (capital ? ", capital" : "");
    r.text = "text " + std::to_string(id);
    return r;
}

MarkerRecord site(MarkerKind kind, std::int64_t id, int x, int y, bool library = false) {
    MarkerRecord r;
    r.kind = kind;
    r.id = id;
    r.x = x;
    r.y = y;
    r.library = library;
    r.colour = kind == MarkerKind::tower ? MARKER_TOWER : kind == MarkerKind::lair ? LAIR_ROCK : 0xCD1E1E;
    r.name = "Site " + std::to_string(id);
    r.kind_word = kind == MarkerKind::lair ? "lair" : kind == MarkerKind::outpost ? "outpost" : kind == MarkerKind::mine ? "mine" : library ? "library" : "tower";
    return r;
}

MarkerRecord ruin(MarkerKind kind, std::int64_t id, int x, int y, int skind, std::int64_t pop) {
    MarkerRecord r = settlement(id, x, y, skind, pop);
    r.kind = kind;
    r.colour = kind == MarkerKind::ruin ? RUIN_STONE : OLD_RUIN_STONE;
    r.kind_word = "ruin of a town";
    return r;
}

Marker3D placed(const MarkerRecord& rec, const std::vector<float>& heights) {
    const Layout layout = layout512();
    const std::vector<MarkerRecord> one{rec};
    return place_markers(one, layout, heights).front();
}

std::size_t roofless_houses(std::int64_t id, int from, int count) {
    std::size_t n = 0;
    for (int k = from; k < count; ++k) {
        n += ((hash32(static_cast<std::uint32_t>(id), static_cast<std::uint32_t>(k), HOUSE_KEY) >> 21) & 1u) != 0 ? 1u : 0u;
    }
    return n;
}

}  // namespace

TEST_CASE("U40 HOUSE_SLOTS") {
    const auto& s = house_slots();
    REQUIRE(s.size() == 289);
    std::set<std::pair<int, int>> distinct;
    for (const HouseSlot& h : s) {
        CHECK(std::abs(h.i) <= 8);
        CHECK(std::abs(h.j) <= 8);
        distinct.insert({h.i, h.j});
    }
    CHECK(distinct.size() == 289);
    const std::pair<int, int> first[13] = {{0, 0},  {0, -1}, {-1, 0}, {1, 0},  {0, 1},  {-1, -1}, {1, -1},
                                           {-1, 1}, {1, 1},  {0, -2}, {-2, 0}, {2, 0},  {0, 2}};
    for (std::size_t k = 0; k < 13; ++k) {
        CAPTURE(k);
        CHECK(s[k].i == first[k].first);
        CHECK(s[k].j == first[k].second);
    }
    for (std::size_t k = 1; k < s.size(); ++k) {
        const auto key = [](const HouseSlot& h) { return std::array<int, 3>{h.i * h.i + h.j * h.j, h.j, h.i}; };
        CHECK(key(s[k - 1]) < key(s[k]));
    }
}

TEST_CASE("U41 house_count") {
    const int mins[6] = {0, 2, 4, 8, 16, 6};
    for (int kind = 1; kind <= 5; ++kind) {
        for (const std::int64_t pop : {std::int64_t{0}, std::int64_t{7}, std::int64_t{8}, std::int64_t{100},
                                       std::int64_t{1000}, std::int64_t{32767}, std::int64_t{32768},
                                       std::int64_t{10000000}}) {
            CAPTURE(kind);
            CAPTURE(pop);
            const auto root = static_cast<int>(std::floor(std::sqrt(static_cast<double>(pop / 8))));
            CHECK(house_count(kind, pop) == std::min(64, std::max(mins[kind], root)));
        }
    }
    CHECK(house_count(1, 100) == 3);
    CHECK(house_count(1, 1000) == 11);
    CHECK(house_count(1, 5000) == 25);
    CHECK(house_count(1, 32768) == 64);
}

TEST_CASE("U42 place_markers") {
    const Layout layout = layout512();
    const std::vector<float> h = slope_grid();
    std::vector<MarkerRecord> recs;
    recs.push_back(settlement(1, 100, 100, 3, 500));
    recs.push_back(ruin(MarkerKind::ruin, 2, 300, 300, 2, 0));
    recs.push_back(site(MarkerKind::mine, 3, 100, 200));
    recs.push_back(site(MarkerKind::tower, 4, 200, 100, true));
    recs.push_back(site(MarkerKind::tower, 5, 400, 50));
    recs.push_back(site(MarkerKind::lair, 6, 300, 300));  // on the ruin's cell
    // A chain of four colliding sites at (50, 400) and their moved cells.
    recs.push_back(site(MarkerKind::outpost, 7, 50, 400));
    recs.push_back(site(MarkerKind::outpost, 8, 50, 400));
    recs.push_back(site(MarkerKind::outpost, 9, 50, 400));
    recs.push_back(site(MarkerKind::outpost, 10, 50, 400));
    // Clamping at the four map edges.
    recs.push_back(site(MarkerKind::mine, 11, 508, 2));
    recs.push_back(site(MarkerKind::tower, 12, 2, 2, true));
    recs.push_back(settlement(13, 0, 511, 1, 10));
    recs.push_back(site(MarkerKind::outpost, 14, 511, 511));
    const std::vector<Marker3D> m = place_markers(recs, layout, h);
    REQUIRE(m.size() == recs.size());
    CHECK((m[0].cell_x == 100 && m[0].cell_y == 100));
    CHECK((m[1].cell_x == 300 && m[1].cell_y == 300));
    CHECK((m[2].cell_x == 107 && m[2].cell_y == 193));
    CHECK((m[3].cell_x == 193 && m[3].cell_y == 93));
    CHECK((m[4].cell_x == 400 && m[4].cell_y == 50));
    CHECK((m[5].cell_x == 307 && m[5].cell_y == 307 && m[5].moves == 1));
    CHECK((m[6].cell_x == 50 && m[6].cell_y == 400 && m[6].moves == 0));
    CHECK((m[7].cell_x == 57 && m[7].cell_y == 407 && m[7].moves == 1));
    CHECK((m[8].cell_x == 64 && m[8].cell_y == 414 && m[8].moves == 2));
    CHECK((m[9].cell_x == 71 && m[9].cell_y == 421 && m[9].moves == 3));
    CHECK((m[10].cell_x == 511 && m[10].cell_y == 0));
    CHECK((m[11].cell_x == 0 && m[11].cell_y == 0));
    CHECK((m[12].cell_x == 0 && m[12].cell_y == 511 && m[12].moves == 0));
    CHECK((m[13].cell_x == 511 && m[13].cell_y == 511));  // nothing near: no move, clamped stays
    for (const Marker3D& k : m) {
        CHECK(k.Y == h[static_cast<std::size_t>(k.cell_y) * N + static_cast<std::size_t>(k.cell_x)]);
        const Vec2 w = cell_to_world(layout, k.cell_x, k.cell_y);
        CHECK(k.X == w.x);
        CHECK(k.Z == w.z);
        CHECK(k.beacon == (k.rec.kind != MarkerKind::old_ruin));
    }
    // A chain stops after 3 moves even when still crowded.
    std::vector<MarkerRecord> crowd;
    for (int k = 0; k < 4; ++k) {
        crowd.push_back(settlement(20 + k, 200 + 7 * k, 200 + 7 * k, 1, 10));
    }
    crowd.push_back(site(MarkerKind::lair, 30, 200, 200));
    const std::vector<Marker3D> c = place_markers(crowd, layout, h);
    CHECK(c.back().moves == 3);
    CHECK((c.back().cell_x == 221 && c.back().cell_y == 221));
    CHECK_THROWS_AS((void)place_markers(crowd, layout, std::vector<float>(10, 0.0f)), std::invalid_argument);
}

TEST_CASE("U43 build_marker_mesh") {
    const Layout layout = layout512();
    const std::vector<float> flat = flat_grid();
    const std::vector<float> slope = slope_grid();
    const std::vector<float> spike = spike_grid();
    const std::set<std::uint32_t> palette = {HOUSE_WALL, WALL_STONE, RUIN_STONE, OLD_RUIN_STONE, LAIR_ROCK,
                                             LAIR_MOUTH, WOOD,       SPOIL,      TOWER_STONE,    MARKER_TOWER,
                                             0x00BEC3,   (0x00u * 12 / 16) << 16 | (0xBEu * 12 / 16) << 8 | (0xC3u * 12 / 16),
                                             0xCD1E1E,   MARKER_MINE};
    struct Case {
        MarkerRecord rec;
        std::size_t tris;
    };
    std::vector<Case> cases;
    // Settlements: houses, a capital hall, a fortress keep, city and fortress walls, declining.
    {
        const int H = house_count(2, 200);  // 5
        cases.push_back({settlement(41, 250, 250, 2, 200), 18u * static_cast<std::size_t>(H)});
        cases.push_back({settlement(42, 250, 250, 3, 2000, true), 16u + 18u * static_cast<std::size_t>(house_count(3, 2000) - 1)});
        const int Hc = house_count(4, 20000);
        cases.push_back({settlement(43, 250, 250, 4, 20000), 18u * static_cast<std::size_t>(Hc) + 60u});
        const int Hf = house_count(5, 3000);
        cases.push_back({settlement(44, 250, 250, 5, 3000), 16u + 18u * static_cast<std::size_t>(Hf - 1) + 60u});
        cases.push_back({settlement(45, 250, 250, 5, 3000, true), 16u + 18u * static_cast<std::size_t>(Hf - 1) + 60u});
        const int Hd = house_count(3, 4000);
        const std::size_t roofless = roofless_houses(46, 0, Hd);
        cases.push_back({settlement(46, 250, 250, 3, 4000, false, true),
                         18u * static_cast<std::size_t>(Hd) - 6u * roofless});
        const int Hr = house_count(3, 900);
        cases.push_back({ruin(MarkerKind::ruin, 47, 250, 250, 3, 900), 12u * (static_cast<std::size_t>((Hr + 1) / 2) + 5u)});
        cases.push_back({ruin(MarkerKind::old_ruin, 48, 250, 250, 3, 900), 12u * (static_cast<std::size_t>((Hr + 3) / 4) + 3u)});
        cases.push_back({site(MarkerKind::lair, 49, 250, 250), 60u});
        cases.push_back({site(MarkerKind::outpost, 50, 250, 250), 64u});
        cases.push_back({site(MarkerKind::mine, 51, 250, 250), 96u});
        cases.push_back({site(MarkerKind::tower, 52, 250, 250), 16u});
        cases.push_back({site(MarkerKind::tower, 53, 250, 250, true), 34u});
    }
    for (const auto* grid : {&flat, &slope, &spike}) {
        for (const Case& k : cases) {
            CAPTURE(k.rec.id);
            const Marker3D m = placed(k.rec, *grid);
            const MeshData mesh = build_marker_mesh(m, layout, *grid);
            CHECK(mesh.triangles() == k.tris);
            CHECK(mesh.positions.size() == 9 * k.tris);
            CHECK(mesh.normals.size() == 9 * k.tris);
            float top = -1e30f;
            for (std::size_t v = 1; v < mesh.positions.size(); v += 3) {
                top = std::max(top, mesh.positions[v]);
            }
            CHECK(static_cast<double>(m.top) == doctest::Approx(static_cast<double>(m.Y + top)).epsilon(1e-6));
            for (const std::uint32_t c : mesh.colours) {
                CHECK((c & 0xFFu) == 0xFFu);
                CHECK(palette.count(c >> 8) == 1);
            }
            for (const GroundedPart& part : grounded_parts(m, layout, *grid)) {
                float g0 = 1e30f;
                for (std::size_t q = 0; q < 4; ++q) {
                    g0 = std::min(g0, ground_low(layout, *grid, m.X + part.corners[2 * q], m.Z + part.corners[2 * q + 1]));
                }
                CHECK(part.bottom <= g0 - m.Y - SINK + 1e-4f);
            }
            const MeshData again = build_marker_mesh(m, layout, *grid);
            CHECK(again.positions == mesh.positions);
            CHECK(again.normals == mesh.normals);
            CHECK(again.colours == mesh.colours);
        }
    }
    // The roof colour of a declining place is dimmed by 12/16.
    {
        const Marker3D m = placed(settlement(60, 250, 250, 3, 4000, false, true), flat);
        const MeshData mesh = build_marker_mesh(m, layout, flat);
        bool roof = false;
        for (const std::uint32_t c : mesh.colours) {
            CHECK((c >> 8) != 0x00BEC3u);
            roof = roof || (c >> 8) != HOUSE_WALL;
        }
        CHECK(roof);
    }
    CHECK(model_far(MarkerKind::old_ruin) == OLD_RUIN_FAR);
    CHECK(model_far(MarkerKind::settlement) == MODEL_FAR);
}

TEST_CASE("U44 beacon_transform") {
    const std::vector<float> flat = flat_grid();
    CHECK(pixel_angle(70.0f, 900) == doctest::Approx(0.0015560).epsilon(1e-7 / 0.0015560));
    const float a = pixel_angle(70.0f, 900);
    const std::vector<Marker3D> markers = {placed(settlement(1, 256, 256, 3, 1000, true), flat),
                                           placed(ruin(MarkerKind::ruin, 2, 256, 256, 2, 0), flat),
                                           placed(site(MarkerKind::tower, 3, 256, 256), flat)};
    for (const Marker3D& m : markers) {
        CAPTURE(static_cast<int>(m.rec.kind));
        const BeaconStyle s = beacon_style(m.rec);
        for (const float d : {0.0f, 1000.0f, s.fade, 0.5f * (s.fade + s.far), s.far - 1.0f, s.far, 2.0f * s.far}) {
            CAPTURE(d);
            // The camera d away, south-east of B and level with it.
            const float bx = m.X;
            const float by = m.top + 2.0f;
            const float bz = m.Z;
            const float cx = bx + d * 0.6f;
            const float cz = bz + d * 0.8f;
            const std::array<float, 12> t = beacon_transform(m, cx, by, cz, a);
            CHECK(t[9] == bx);
            CHECK(t[10] == by);
            CHECK(t[11] == bz);
            const float dist = std::sqrt((cx - bx) * (cx - bx) + (cz - bz) * (cz - bz));
            if (dist >= s.far) {
                for (int k = 0; k < 9; ++k) {
                    CHECK(t[static_cast<std::size_t>(k)] == 0.0f);
                }
                continue;
            }
            const float taper = dist <= s.fade ? 1.0f : (s.far - dist) / (s.far - s.fade);
            const float w = std::max(2.0f, dist * s.px_w * a) * taper;
            const float h = std::max(s.h, dist * s.px_h * a);
            const float got_w = std::sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
            CHECK(static_cast<double>(got_w) == doctest::Approx(static_cast<double>(w)).epsilon(1e-4));
            CHECK(static_cast<double>(t[4]) == doctest::Approx(static_cast<double>(h)).epsilon(1e-4));
            if (dist > 1.0f) {
                // The quad's normal (column 2) points at the camera.
                CHECK(static_cast<double>(t[6]) == doctest::Approx(static_cast<double>(0.6f)).epsilon(1e-4));
                CHECK(static_cast<double>(t[8]) == doctest::Approx(static_cast<double>(0.8f)).epsilon(1e-4));
            }
        }
    }
    const Marker3D old = placed(ruin(MarkerKind::old_ruin, 9, 256, 256, 2, 0), flat);
    CHECK_FALSE(old.beacon);
    const std::array<float, 12> t = beacon_transform(old, old.X + 100.0f, old.Y, old.Z, a);
    for (int k = 0; k < 9; ++k) {
        CHECK(t[static_cast<std::size_t>(k)] == 0.0f);
    }
    CHECK(beacon_style(settlement(1, 0, 0, 4, 0)).h == 260.0f);
    CHECK(beacon_style(settlement(1, 0, 0, 4, 0, true)).h == 360.0f);
    CHECK(beacon_style(settlement(1, 0, 0, 4, 0, true)).px_w == 6.0f);
    CHECK(beacon_style(site(MarkerKind::lair, 1, 0, 0)).far == 12000.0f);
}

TEST_CASE("U45 select_labels") {
    const Layout layout = layout512();
    const std::vector<float> flat = flat_grid();
    std::vector<MarkerRecord> recs;
    for (int k = 0; k < 30; ++k) {
        // A row of 30 settlements 20 cells (320 m) apart along x, at y 256.
        recs.push_back(settlement(k + 1, 20 + 16 * k, 256, 1, 10));
    }
    const std::vector<Marker3D> m = place_markers(recs, layout, flat);
    std::vector<std::size_t> prev;
    float cx = m[0].X;
    const float cy = m[0].top + 2.0f;
    const float cz = m[0].Z;
    for (int step = 0; step < 20; ++step) {
        const std::vector<std::size_t> shown = select_labels(m, cx, cy, cz, prev);
        CHECK(shown.size() <= LABEL_KEEP);
        // New ones are the 12 nearest within range; every kept one is within
        // the slack and ranks under 16.
        std::vector<std::size_t> all(m.size());
        for (std::size_t i = 0; i < m.size(); ++i) {
            all[i] = i;
        }
        const auto dist = [&](std::size_t i) {
            const float dx = cx - m[i].X;
            const float dy = cy - (m[i].top + 2.0f);
            const float dz = cz - m[i].Z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        };
        std::sort(all.begin(), all.end(), [&](std::size_t a, std::size_t b) {
            return dist(a) != dist(b) ? dist(a) < dist(b) : a < b;
        });
        std::size_t fresh = 0;
        for (const std::size_t i : shown) {
            CHECK(dist(i) <= LABEL_RANGE + LABEL_SLACK);
            const auto rank = static_cast<std::size_t>(std::find(all.begin(), all.end(), i) - all.begin());
            const bool previous = std::find(prev.begin(), prev.end(), i) != prev.end();
            if (!previous) {
                ++fresh;
                CHECK(rank < LABEL_MAX);
            } else {
                CHECK(rank < LABEL_KEEP);
            }
        }
        CHECK(fresh <= LABEL_MAX);
        for (std::size_t k = 1; k < shown.size(); ++k) {
            CHECK(dist(shown[k - 1]) <= dist(shown[k]));
        }
        // No label leaves while its rank stays under 16 (and it is in range).
        for (const std::size_t i : prev) {
            const auto rank = static_cast<std::size_t>(std::find(all.begin(), all.end(), i) - all.begin());
            if (rank < LABEL_KEEP && dist(i) <= LABEL_RANGE + LABEL_SLACK) {
                CHECK(std::find(shown.begin(), shown.end(), i) != shown.end());
            }
        }
        prev = shown;
        cx += 160.0f;  // half a spacing: the 12th rank changes
    }
    // Ties by index: two markers at the same cell.
    std::vector<MarkerRecord> twin = {settlement(1, 100, 100, 1, 10), settlement(2, 100, 100, 1, 10)};
    const std::vector<Marker3D> t = place_markers(twin, layout, flat);
    const std::vector<std::size_t> s = select_labels(t, t[0].X + 50.0f, 300.0f, t[0].Z, {});
    REQUIRE(s.size() == 2);
    CHECK((s[0] == 0 && s[1] == 1));
    // None beyond range.
    CHECK(select_labels(t, t[0].X + 7000.0f, 300.0f, t[0].Z, {}).empty());
    CHECK(label_text(twin[0]) == "Settlement 1\nhamlet");
    CHECK(label_modulate(MarkerKind::settlement) == 0xFFFFFFu);
    CHECK(label_modulate(MarkerKind::ruin) == 0xC8C8C8u);
    CHECK(label_modulate(MarkerKind::old_ruin) == 0xAAAAAAu);
}

TEST_CASE("U46 bearing, distance and the HUD line") {
    const char* words[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    for (int k = 0; k < 8; ++k) {
        const double a = k * 45.0 * 3.14159265358979 / 180.0;
        CHECK(bearing_word(static_cast<float>(std::sin(a)), static_cast<float>(-std::cos(a))) == words[k]);
    }
    const auto at = [](double deg) {
        const double a = deg * 3.14159265358979 / 180.0;
        return bearing_word(static_cast<float>(std::sin(a)), static_cast<float>(-std::cos(a)));
    };
    CHECK(at(22.4) == "N");
    CHECK(at(22.6) == "NE");
    CHECK(at(-22.4) == "N");
    CHECK(at(-22.6) == "NW");
    CHECK(distance_text(0.0f) == "0 m");
    CHECK(distance_text(999.9f) == "999 m");
    CHECK(distance_text(1000.0f) == "1.0 km");
    CHECK(distance_text(12345.0f) == "12.3 km");

    const Layout layout = layout512();
    const std::vector<float> flat = flat_grid();
    std::vector<MarkerRecord> recs = {settlement(1, 256, 256, 3, 1000), settlement(2, 400, 256, 2, 100)};
    recs[0].text = "Thark, town of Somewhere";
    const std::vector<Marker3D> m = place_markers(recs, layout, flat);
    // Near: within 2,000 m, the text; 1,000 m south of marker 0 it lies north.
    CHECK(markers_hud_line(MarkersHud::on, m, 6000, m[0].X, m[0].Z + 1000.0f) ==
          "markers 2 at year 6000; nearest 1.0 km N: Thark, town of Somewhere");
    CHECK(markers_hud_line(MarkersHud::on, m, 6000, m[0].X - 5000.0f, m[0].Z) ==
          "markers 2 at year 6000; nearest Settlement 1 (town), 5.0 km E");
    CHECK(markers_hud_line(MarkersHud::off, m, 6000, 0, 0) == "markers off (M)");
    CHECK(markers_hud_line(MarkersHud::none, {}, -1, 0, 0) == "markers: none, no history in this world");
    CHECK(markers_hud_line(MarkersHud::failed, {}, -1, 0, 0) == "markers: none, the history could not be read");
    CHECK(markers_hud_line(MarkersHud::disabled, {}, -1, 0, 0) == "markers off (--no-markers)");
    // The 200-byte cut at a UTF-8 boundary.
    std::vector<MarkerRecord> longrec = {settlement(1, 256, 256, 3, 1000)};
    std::string text;
    for (int k = 0; k < 120; ++k) {
        text += "\xC3\xA9";  // e acute, two bytes
    }
    longrec[0].text = text;
    const std::vector<Marker3D> lm = place_markers(longrec, layout, flat);
    for (float shift = 0.0f; shift < 8.0f; shift += 1.0f) {
        // Shifting the distance shifts the prefix length by a byte or two.
        const std::string line = markers_hud_line(MarkersHud::on, lm, 6000 + static_cast<std::int64_t>(shift * 1111.0f),
                                                  lm[0].X, lm[0].Z + 100.0f * shift);
        CHECK(line.size() <= HUD_LINE_MAX);
        CHECK(line.substr(line.size() - 3) == "...");
        const std::string body = line.substr(0, line.size() - 3);
        // The last byte before the dots is never the first half of a pair.
        CHECK(static_cast<unsigned char>(body.back()) != 0xC3u);
    }
    CHECK(cut_utf8("short", 200) == "short");
}

TEST_CASE("U47 jump_order and jump_view") {
    const Layout layout = layout512();
    const std::vector<float> flat = flat_grid();
    std::vector<MarkerRecord> recs = {settlement(5, 100, 100, 2, 300), ruin(MarkerKind::ruin, 6, 120, 120, 3, 9000),
                                      settlement(3, 200, 100, 3, 900), settlement(2, 300, 100, 2, 300),
                                      site(MarkerKind::outpost, 7, 50, 50), settlement(9, 256, 508, 1, 40)};
    const std::vector<Marker3D> m = place_markers(recs, layout, flat);
    const std::vector<std::size_t> order = jump_order(m);
    REQUIRE(order.size() == 4);
    CHECK(order[0] == 2);  // 900
    CHECK(order[1] == 3);  // 300, id 2
    CHECK(order[2] == 0);  // 300, id 5
    CHECK(order[3] == 5);
    const FlyState s = jump_view(m[0], layout, flat);
    CHECK(s.x == m[0].X);
    CHECK(s.z == m[0].Z + JUMP_DIST);
    CHECK(s.heading_deg == 0.0f);
    CHECK(static_cast<double>(s.y) == doctest::Approx(static_cast<double>(m[0].top + JUMP_ABOVE)));
    CHECK(s.pitch_deg == doctest::Approx(-std::atan2(static_cast<double>(JUMP_ABOVE), static_cast<double>(JUMP_DIST)) *
                                         180.0 / 3.14159265358979));
    // Near the south edge: north of it, heading 180.
    const FlyState n = jump_view(m[5], layout, flat);
    CHECK(n.z == m[5].Z - JUMP_DIST);
    CHECK(n.heading_deg == 180.0f);
    // Over high ground the altitude follows the ground.
    std::vector<float> ridge = flat_grid();
    for (int y = 125; y < 140; ++y) {  // around 100 + JUMP_DIST / 16
        for (int x = 90; x < 110; ++x) {
            ridge[static_cast<std::size_t>(y) * N + static_cast<std::size_t>(x)] = 900.0f;
        }
    }
    const Marker3D low = placed(settlement(5, 100, 100, 2, 300), ridge);
    const FlyState r = jump_view(low, layout, ridge);
    CHECK(static_cast<double>(r.y) == doctest::Approx(static_cast<double>(1000.0f)));
    CHECK(r.pitch_deg == doctest::Approx(-std::atan2(1000.0 - static_cast<double>(low.top), static_cast<double>(JUMP_DIST)) *
                                         180.0 / 3.14159265358979));
}

TEST_CASE("U48 find_marker") {
    const Layout layout = layout512();
    const std::vector<float> flat = flat_grid();
    std::vector<MarkerRecord> recs = {settlement(3, 100, 100, 2, 300), site(MarkerKind::lair, 3, 300, 300),
                                      site(MarkerKind::mine, 7, 200, 200), settlement(4, 150, 150, 3, 900)};
    recs[0].name = "Thark Hold";
    recs[3].name = "\xC3\x89lan Ford";
    const std::vector<Marker3D> m = place_markers(recs, layout, flat);
    const std::vector<std::size_t> order = jump_order(m);
    CHECK(find_marker(m, order, "settlement:3") == std::optional<std::size_t>{0});
    CHECK(find_marker(m, order, "site:3") == std::optional<std::size_t>{1});
    CHECK(find_marker(m, order, "site:7") == std::optional<std::size_t>{2});
    CHECK(find_marker(m, order, "rank:1") == std::optional<std::size_t>{3});
    CHECK(find_marker(m, order, "rank:2") == std::optional<std::size_t>{0});
    CHECK_FALSE(find_marker(m, order, "rank:0").has_value());
    CHECK_FALSE(find_marker(m, order, "rank:3").has_value());
    CHECK(find_marker(m, order, "Thark Hold") == std::optional<std::size_t>{0});
    CHECK(find_marker(m, order, "thark HOLD") == std::optional<std::size_t>{0});
    CHECK(find_marker(m, order, "\xC3\x89lan ford") == std::optional<std::size_t>{3});
    CHECK_FALSE(find_marker(m, order, "\xC3\xA9lan Ford").has_value());  // bytes >= 0x80 are not folded
    CHECK_FALSE(find_marker(m, order, "nowhere").has_value());
}

TEST_CASE("U49 parse_year") {
    CHECK(parse_year("0") == std::optional<std::int64_t>{0});
    CHECK(parse_year("6000") == std::optional<std::int64_t>{6000});
    for (const char* bad : {"-1", "x", "1.5", "", "+3"}) {
        CAPTURE(bad);
        CHECK_FALSE(parse_year(bad).has_value());
    }
    CHECK(year_refusal("x") == "--year x: a whole number from 0");
}

TEST_CASE("U50 status line and texts") {
    MarkerLoad load;
    load.status = MarkerStatus::loaded;
    load.year = 6000;
    load.end_year = 6000;
    load.records = {settlement(1, 0, 0, 3, 10, true), settlement(2, 0, 0, 2, 10), ruin(MarkerKind::ruin, 3, 0, 0, 2, 0),
                    ruin(MarkerKind::old_ruin, 4, 0, 0, 2, 0), site(MarkerKind::lair, 5, 0, 0),
                    site(MarkerKind::outpost, 6, 0, 0), site(MarkerKind::mine, 7, 0, 0), site(MarkerKind::tower, 8, 0, 0)};
    CHECK(markers_status_line(load) ==
          "markers: 2 settlements (1 capitals), 2 ruins (1 old), 1 lairs, 1 outposts, 1 mines, 1 towers at year 6000 of 6000");
    load.year_clamped = true;
    load.asked_year = 9000;
    CHECK(markers_status_line(load) ==
          "markers: 2 settlements (1 capitals), 2 ruins (1 old), 1 lairs, 1 outposts, 1 mines, 1 towers at year 6000 of 6000\n"
          "--year 9000 is after the history's end: markers at 6000");
    MarkerLoad none;
    CHECK(markers_status_line(none) == "no history in this world: no markers");
    MarkerLoad failed;
    failed.status = MarkerStatus::failed;
    failed.code = "corrupt_layer";
    failed.message = "corrupt table 'site': bad";
    CHECK(markers_status_line(failed) == "markers: the history could not be read: corrupt_layer: corrupt table 'site': bad");
    CHECK(at_marker_miss("nowhere", 6000) ==
          "--at-marker nowhere: no marker of that name or id at year 6000; starting at the default view");

    SettlementTextParts p;
    p.name = "Thark";
    p.kind = 3;
    p.status = 2;
    p.capital = true;
    p.owner = "the Realm";
    p.population = 1200;
    p.since = 5990;
    p.founded = 4100;
    CHECK(standing_text(p) == "Thark, town (capital) of the Realm, declining, population 1200 recorded in 5990, founded in 4100");
    p.capital = false;
    p.owner.clear();
    p.status = 1;
    CHECK(standing_text(p) == "Thark, town, thriving, population 1200 recorded in 5990, founded in 4100");
    CHECK(ruin_text("Old Thark", "ruin of a", 2, 3000, "") == "Old Thark, ruin of a village, ruined in 3000: cause not recorded");
    CHECK(ruin_text("Old Thark", "faded ruin of a", 4, 1000, "It fell.") ==
          "Old Thark, faded ruin of a city, ruined in 1000: It fell.");
    CHECK(mine_text("Deep", "the Hold", 4200, 0) == "Deep (mine of the Hold), since 4200");
    CHECK(mine_text("Deep", "", 4200, 2) ==
          "Deep (mine), since 4200, delved " + std::string(worldgen::inspect::mine_depth_word(2)));
    CHECK(tower_text("Spire", true, "Ael II the Wise", 5000) == "Spire (library of Ael II the Wise), since 5000");
    CHECK(tower_text("Spire", false, "", 5000) == "Spire (tower), since 5000");
    CHECK(lair_text("Den", "Grak", "troll", true, 4000) == "Den (lair of Grak, troll, asleep), since 4000");
    CHECK(lair_text("Den", "Grak", "troll", false, 4000) == "Den (lair of Grak, troll), since 4000");
    CHECK(lair_text("Den", "", "", false, 4000) == "Den (lair, empty), since 4000");
    CHECK(outpost_text("Camp", "the Horde", 5500) == "Camp (outpost of the Horde), since 5500");
    CHECK(outpost_text("Camp", "", 5500) == "Camp (outpost), since 5500");
}

TEST_CASE("U51 constants") {
    CHECK(MARKER_CLEAR_CELLS == 4);
    CHECK(SINK == 0.5f);
    CHECK(HOUSE_KEY == 0x484F5553u);
    CHECK(MIN_HOUSES[1] == 2);
    CHECK(MIN_HOUSES[2] == 4);
    CHECK(MIN_HOUSES[3] == 8);
    CHECK(MIN_HOUSES[4] == 16);
    CHECK(MIN_HOUSES[5] == 6);
    CHECK(MODEL_FAR == 8000.0f);
    CHECK(OLD_RUIN_FAR == 3000.0f);
    CHECK(MODEL_FADE_MARGIN == 500.0f);
    CHECK(LABEL_RANGE == 6000.0f);
    CHECK(LABEL_SLACK == 500.0f);
    CHECK(LABEL_MAX == 12);
    CHECK(LABEL_KEEP == 16);
    CHECK(HUD_TEXT_RADIUS == 2000.0f);
    CHECK(JUMP_DIST == 500.0f);   // spec 1,200, tuned
    CHECK(JUMP_ABOVE == 150.0f);  // spec 350, tuned
    CHECK(MARKER_NO_OWNER == 0x808080u);
    CHECK(RUIN_STONE == 0x78726Au);
    CHECK(OLD_RUIN_STONE == 0x8C8880u);
    CHECK(LAIR_ROCK == 0x584E42u);
    CHECK(MARKER_TOWER == 0x783CB4u);
    CHECK(MARKER_MINE == 0x3C3C3Cu);
    CHECK(HOUSE_WALL == 0xB4A48Cu);
    CHECK(WALL_STONE == 0x968F87u);
    CHECK(LAIR_MOUTH == 0x0C0A0Au);
    CHECK(WOOD == 0x705234u);
    CHECK(RUIN_BEACON == 0x8C8478u);
}

TEST_CASE("U52 label declutter (beyond the spec)") {
    MarkerRecord a = settlement(1, 0, 0, 3, 10);
    a.name = "Abcdefgh";  // 8 characters, the longer line
    const LabelBox ba = label_box(a, 800.0f, 450.0f, 20.0f, 26.0f, true);
    CHECK(static_cast<double>(ba.w) == doctest::Approx(static_cast<double>(8 * LABEL_CHAR_EM * 20.0f)));
    CHECK(static_cast<double>(ba.h) == doctest::Approx(static_cast<double>(2 * LABEL_LINE_EM * 20.0f)));
    CHECK(ba.x == 800.0f);
    CHECK(ba.y == 424.0f);
    // A second box shifted by a quarter width overlaps by three quarters: hidden.
    LabelBox bb = ba;
    bb.x += ba.w / 4;
    // A third box shifted by three quarters overlaps a quarter: drawn.
    LabelBox bc = ba;
    bc.x += 3 * ba.w / 4;
    // A box behind the camera neither hides nor is hidden.
    LabelBox bd = ba;
    bd.on_screen = false;
    const std::vector<LabelBox> boxes = {ba, bb, bc, bd};
    const std::vector<bool> drawn = declutter_labels(boxes);
    CHECK(drawn == std::vector<bool>{true, false, true, true});
    // A hidden box does not hide a later one.
    LabelBox be = bb;
    be.x += 1.0f;
    const std::vector<LabelBox> two = {ba, bb, be};
    CHECK(declutter_labels(two) == std::vector<bool>{true, false, false});
    // UTF-8 characters count once.
    MarkerRecord u = a;
    u.name = "\xC3\x89lan";
    u.kind_word = "x";
    CHECK(static_cast<double>(label_box(u, 0, 0, 10.0f, 0.0f, true).w) == doctest::Approx(static_cast<double>(4 * LABEL_CHAR_EM * 10.0f)));
}
