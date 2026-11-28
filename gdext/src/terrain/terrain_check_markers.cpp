// The terrain check's markers (spec V3D-05 12.2, T31 to T37 and T39). The
// check collects its own records from worldgen's MarkerIndex and Timeline
// (section 3 steps 1 to 6) and keeps its own copy of sections 4 (cells), 6
// (beacons), 7 (labels) and 8.3 (the jump view); the meshes are compared
// with build_marker_mesh, which the unit tests check (U43).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "terrain/markers3d.hpp"
#include "terrain/terrain_check.hpp"
#include "terrain/terrain_check_internal.hpp"
#include "terrain/terrain_preview.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/markers.hpp"
#include "worldgen/inspect/territory.hpp"

namespace unmapped {

namespace {

using terrain::MarkerKind;

godot::String to_godot(const std::string& text) {
    return godot::String::utf8(text.c_str(), static_cast<int64_t>(text.size()));
}

godot::String num(std::uint64_t v) {
    return godot::String::num_int64(static_cast<int64_t>(v));
}

// --- The check's own records (section 3) ---------------------------------------------

struct OwnRecord {
    MarkerKind kind = MarkerKind::settlement;
    std::int64_t id = 0;
    int x = 0, y = 0, skind = 0;
    std::int64_t population = 0;
    bool capital = false, declining = false, library = false;
    std::uint32_t colour = 0;
    std::string name;
};

struct OwnLoad {
    bool ok = false;
    bool history = false;
    std::string problem;
    std::int64_t end = -1;
    std::int64_t year = -1;
    std::vector<OwnRecord> records;
};

OwnLoad own_records(const worldgen::db::WorldReader& reader, std::optional<std::int64_t> asked) {
    namespace history = worldgen::history;
    namespace inspect = worldgen::inspect;
    using history::EntityType;
    OwnLoad out;
    try {
        auto loaded = history::load_history(reader);
        if (!loaded.ok()) {
            out.ok = loaded.error().code == worldgen::db::ErrorCode::history_missing;
            out.problem = loaded.error().message;
            return out;
        }
        out.history = true;
        const history::LoadedHistory& h = loaded.value();
        const std::vector<inspect::Rgb> colours = inspect::civilization_colours(h.tables);
        auto index = inspect::MarkerIndex::load(reader, h);
        if (!index.ok()) {
            out.problem = index.error().message;
            return out;
        }
        out.end = h.timeline.end_year();
        out.year = asked ? std::min(*asked, out.end) : out.end;
        const auto colour = [&colours](std::int64_t civ, std::uint32_t none) -> std::uint32_t {
            if (civ <= 0 || static_cast<std::size_t>(civ) > colours.size()) {
                return none;
            }
            const inspect::Rgb c = colours[static_cast<std::size_t>(civ - 1)];
            return (std::uint32_t{c.r} << 16) | (std::uint32_t{c.g} << 8) | std::uint32_t{c.b};
        };
        const auto name = [&h](EntityType type, std::int64_t id, const char* word) {
            const std::string* n = h.view.name(type, id);
            return n != nullptr && !n->empty() ? *n : std::string(word) + " " + std::to_string(id);
        };
        const inspect::YearMarkers ym = index.value().markers_at(h, out.year, inspect::MarkerOptions{true, false, false, true});
        for (const auto& s : ym.settlements) {
            OwnRecord r;
            r.id = s.settlement;
            r.x = static_cast<int>(s.x);
            r.y = static_cast<int>(s.y);
            r.skind = static_cast<int>(s.kind);
            r.population = s.population;
            r.name = name(EntityType::settlement, s.settlement, "settlement");
            if (s.status == 1 || s.status == 2) {
                r.kind = MarkerKind::settlement;
                r.capital = s.capital;
                r.declining = s.status == 2;
                r.colour = colour(s.owner, 0x808080);
            } else if (s.status == 4 || s.old_ruin) {
                r.kind = MarkerKind::old_ruin;
                r.colour = 0x8C8880;
            } else {
                r.kind = MarkerKind::ruin;
                r.colour = 0x78726A;
            }
            out.records.push_back(r);
        }
        std::vector<OwnRecord> sites;
        for (const auto& m : ym.sites) {
            OwnRecord r;
            r.id = m.site;
            r.x = static_cast<int>(m.x);
            r.y = static_cast<int>(m.y);
            r.name = name(EntityType::site, m.site, "site");
            if (m.kind == 3) {
                r.kind = MarkerKind::mine;
                r.colour = colour(m.founder, 0x3C3C3C);
            } else if (m.kind == 4) {
                r.kind = MarkerKind::tower;
                r.library = m.settlement != 0;
                r.colour = 0x783CB4;
            } else {
                continue;
            }
            sites.push_back(r);
        }
        for (const std::int64_t id : h.timeline.sites_existing(out.year)) {
            const history::SiteState st = h.timeline.site_at(id, out.year);
            if (!st.open || (st.kind != 1 && st.kind != 2)) {
                continue;
            }
            OwnRecord r;
            r.id = id;
            r.x = static_cast<int>(st.x);
            r.y = static_cast<int>(st.y);
            r.name = name(EntityType::site, id, "site");
            r.kind = st.kind == 1 ? MarkerKind::lair : MarkerKind::outpost;
            r.colour = st.kind == 1 ? 0x584E42u : colour(st.founder, 0x808080);
            sites.push_back(r);
        }
        std::sort(sites.begin(), sites.end(), [](const OwnRecord& a, const OwnRecord& b) { return a.id < b.id; });
        out.records.insert(out.records.end(), sites.begin(), sites.end());
        out.ok = true;
    } catch (const std::exception& e) {
        out.ok = false;
        out.problem = e.what();
    }
    return out;
}

bool same_record(const OwnRecord& o, const terrain::MarkerRecord& r) {
    return o.kind == r.kind && o.id == r.id && o.x == r.x && o.y == r.y && o.skind == r.settlement_kind &&
           o.population == r.population && o.capital == r.capital && o.declining == r.declining &&
           o.library == r.library && o.colour == r.colour && o.name == r.name;
}

bool is_site(MarkerKind k) {
    return k == MarkerKind::lair || k == MarkerKind::outpost || k == MarkerKind::mine || k == MarkerKind::tower;
}

// --- Own copies of sections 4, 6, 7 and 8.3 ---------------------------------------------

std::vector<std::array<int, 2>> own_cells(const std::vector<OwnRecord>& recs, int n) {
    const auto clamp = [n](int v) { return std::min(std::max(v, 0), n - 1); };
    std::vector<std::array<int, 2>> cells;
    for (const OwnRecord& r : recs) {
        int x = r.x;
        int y = r.y;
        if (r.kind == MarkerKind::mine) {
            x += 7;
            y -= 7;
        } else if (r.kind == MarkerKind::tower && r.library) {
            x -= 7;
            y -= 7;
        }
        x = clamp(x);
        y = clamp(y);
        if (is_site(r.kind)) {
            for (int moves = 0; moves < 3; ++moves) {
                bool near = false;
                for (const auto& c : cells) {
                    near = near || std::max(std::abs(c[0] - x), std::abs(c[1] - y)) <= 4;
                }
                if (!near) {
                    break;
                }
                x = clamp(x + 7);
                y = clamp(y + 7);
            }
        }
        cells.push_back({x, y});
    }
    return cells;
}

struct OwnStyle {
    float h, pw, ph, fade, far;
};

OwnStyle own_style(const terrain::MarkerRecord& r) {
    switch (r.kind) {
        case MarkerKind::settlement: {
            const float hk[6] = {80, 80, 120, 180, 260, 220};
            const int k = r.settlement_kind >= 1 && r.settlement_kind <= 5 ? r.settlement_kind : 1;
            return {hk[k] + (r.capital ? 100.0f : 0.0f), r.capital ? 6.0f : 4.0f, 40.0f, 16000.0f, 24000.0f};
        }
        case MarkerKind::ruin: return {60, 2, 24, 5000, 8000};
        case MarkerKind::old_ruin: return {0, 0, 0, 0, 0};
        case MarkerKind::tower: return {90, 3, 24, 8000, 12000};
        default: return {70, 3, 24, 8000, 12000};
    }
}

std::uint32_t own_beacon_colour(const terrain::MarkerRecord& r) {
    switch (r.kind) {
        case MarkerKind::ruin: return 0x8C8478;
        case MarkerKind::lair: return 0x96281E;
        case MarkerKind::mine: return 0x3C3C3C;
        case MarkerKind::tower: return 0x783CB4;
        default: return r.colour;
    }
}

// The 16 buffer floats of a beacon (rows with the origin, then the colour).
std::array<float, 16> own_beacon(const terrain::Marker3D& m, float cx, float cy, float cz, float a) {
    std::array<float, 16> b{};
    const float bx = m.X;
    const float by = m.top + 2.0f;
    const float bz = m.Z;
    b[3] = bx;
    b[7] = by;
    b[11] = bz;
    const std::uint32_t c = own_beacon_colour(m.rec);
    b[12] = static_cast<float>((c >> 16) & 0xFF) / 255.0f;
    b[13] = static_cast<float>((c >> 8) & 0xFF) / 255.0f;
    b[14] = static_cast<float>(c & 0xFF) / 255.0f;
    b[15] = 1.0f;
    const OwnStyle s = own_style(m.rec);
    const float d = std::sqrt((cx - bx) * (cx - bx) + (cy - by) * (cy - by) + (cz - bz) * (cz - bz));
    if (d >= s.far) {
        return b;
    }
    const float taper = d <= s.fade ? 1.0f : (s.far - d) / (s.far - s.fade);
    const float w = std::max(2.0f, d * s.pw * a) * taper;
    const float h = std::max(s.h, d * s.ph * a);
    const float yaw = std::atan2(cx - bx, cz - bz);
    // Rotation about Y times diag(w, h, 1), written as rows.
    b[0] = std::cos(yaw) * w;
    b[1] = 0.0f;
    b[2] = std::sin(yaw);
    b[4] = 0.0f;
    b[5] = h;
    b[6] = 0.0f;
    b[8] = -std::sin(yaw) * w;
    b[9] = 0.0f;
    b[10] = std::cos(yaw);
    return b;
}

std::vector<std::size_t> own_labels(const std::vector<terrain::Marker3D>& m, float cx, float cy, float cz,
                                    const std::vector<std::size_t>& prev) {
    std::vector<std::pair<float, std::size_t>> wide;
    for (std::size_t i = 0; i < m.size(); ++i) {
        const float dx = cx - m[i].X;
        const float dy = cy - (m[i].top + 2.0f);
        const float dz = cz - m[i].Z;
        const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d <= 6500.0f) {
            wide.emplace_back(d, i);
        }
    }
    std::sort(wide.begin(), wide.end());
    std::vector<std::pair<float, std::size_t>> shown;
    for (std::size_t r = 0; r < wide.size(); ++r) {
        const bool fresh = r < 12 && wide[r].first <= 6000.0f;
        const bool kept = r < 16 && std::find(prev.begin(), prev.end(), wide[r].second) != prev.end();
        if (fresh || kept) {
            shown.push_back(wide[r]);
        }
    }
    std::vector<std::size_t> out;
    for (std::size_t k = 0; k < shown.size() && k < 16; ++k) {
        out.push_back(shown[k].second);
    }
    return out;
}

terrain::FlyState own_jump(const terrain::Marker3D& m, int n, const std::vector<float>& heights) {
    terrain::FlyState s;
    s.x = m.X;
    // 8.3 with the tuned distances (spec 1,200 and 350; README).
    s.z = m.Z + 500.0f;
    s.heading_deg = 0.0f;
    if (m.Z + 500.0f > 8.0f * static_cast<float>(n) - 16.0f) {
        s.z = m.Z - 500.0f;
        s.heading_deg = 180.0f;
    }
    // ground_clamp: the largest of the four heights around the point.
    const float top = static_cast<float>(n - 1);
    const float u = std::clamp(s.x / 16.0f + static_cast<float>(n / 2), 0.0f, top);
    const float v = std::clamp(s.z / 16.0f + static_cast<float>(n / 2), 0.0f, top);
    const int i = std::min(static_cast<int>(std::floor(u)), n - 2);
    const int j = std::min(static_cast<int>(std::floor(v)), n - 2);
    const auto at = [&](int x, int y) {
        return heights[static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x)];
    };
    const float g = std::max(std::max(at(i, j), at(i + 1, j)), std::max(at(i, j + 1), at(i + 1, j + 1)));
    s.y = std::max(m.top + 150.0f, g + 100.0f);
    s.pitch_deg = static_cast<float>(
        std::clamp(-std::atan2(static_cast<double>(s.y - m.top), 500.0) * 180.0 / 3.14159265358979323846, -89.0, 89.0));
    return s;
}

bool close(float a, float b, float rel = 1e-4f) {
    return std::abs(a - b) <= rel * std::max(1.0f, std::max(std::abs(a), std::abs(b)));
}

bool same_view(const terrain::FlyState& a, const terrain::FlyState& b) {
    return close(a.x, b.x) && close(a.y, b.y) && close(a.z, b.z) && close(a.heading_deg, b.heading_deg) &&
           close(a.pitch_deg, b.pitch_deg);
}

godot::Node* markers_node(TerrainPreview& preview) {
    return preview.get_node_or_null(godot::NodePath("Markers"));
}

// T33 and T34 at one camera point: the preview's beacons and labels against
// the check's own. Returns the label set the check expects there.
bool check_point(TerrainPreview& preview, TerrainChecker& c, const godot::String& where, float cx, float cy, float cz,
                 bool report, std::size_t& shown, std::size_t& hidden_labels) {
    const std::vector<std::size_t> prev = preview.shown_labels();
    terrain::FlyState at;
    at.x = cx;
    at.y = cy;
    at.z = cz;
    at.heading_deg = 0.0f;
    at.pitch_deg = -30.0f;
    preview.set_fly_state(at);
    preview.markers_frame(cx, cy, cz);
    const std::vector<terrain::Marker3D>& m = preview.markers();
    const float a = terrain::pixel_angle(70.0f, preview.marker_viewport_height());
    godot::Node* root = markers_node(preview);
    auto* beacons = godot::Object::cast_to<godot::MultiMeshInstance3D>(root->get_node_or_null(godot::NodePath("Beacons")));
    std::size_t bad = 0;
    std::size_t hidden = 0;
    std::size_t count = 0;
    if (beacons != nullptr && beacons->get_multimesh().is_valid()) {
        const godot::PackedFloat32Array buf = beacons->get_multimesh()->get_buffer();
        std::size_t b = 0;
        for (const terrain::Marker3D& k : m) {
            if (k.rec.kind == MarkerKind::old_ruin) {
                continue;
            }
            const std::array<float, 16> want = own_beacon(k, cx, cy, cz, a);
            bool ok = static_cast<std::size_t>(buf.size()) >= 16 * (b + 1);
            for (std::size_t q = 0; ok && q < 16; ++q) {
                ok = close(buf[static_cast<int64_t>(16 * b + q)], want[q]);
            }
            bad += ok ? 0u : 1u;
            hidden += want[5] == 0.0f ? 1u : 0u;
            ++b;
        }
        count = b;
    } else {
        bad = 1;
    }
    if (report) {
        c.check(bad == 0, "T33 beacons at " + where + ": " + num(count) + " transforms and colours equal the check's own (" +
                              num(bad) + " differ, " + num(hidden) + " hidden)");
    }
    const std::vector<std::size_t> want = own_labels(m, cx, cy, cz, prev);
    bool labels_ok = preview.shown_labels() == want;
    // The declutter (beyond the spec): in order, a label whose box (0.55 em
    // a character, two lines of 1.25 em, centred the offset above its
    // projected point) overlaps a nearer drawn one's by more than half the
    // smaller box is not drawn.
    std::vector<bool> drawn(want.size(), true);
    {
        const godot::Camera3D* camera = preview.camera();
        const float em = 20.0f * TerrainPreview::LABEL_PIXEL_SIZE / a;
        const float off = 24.0f * TerrainPreview::LABEL_PIXEL_SIZE / a;
        struct Box {
            float x0, x1, y0, y1;
            bool on;
        };
        std::vector<Box> boxes;
        for (const std::size_t i : want) {
            const godot::Vector3 p(m[i].X, m[i].top + 2.0f, m[i].Z);
            const bool on = !camera->is_position_behind(p);
            const godot::Vector2 sp = on ? camera->unproject_position(p) : godot::Vector2();
            const std::string text = m[i].rec.name + "\n" + m[i].rec.kind_word;
            std::size_t chars = 0;
            std::size_t line = 0;
            for (const char ch : text) {
                if (ch == '\n') {
                    chars = std::max(chars, line);
                    line = 0;
                } else if ((static_cast<unsigned char>(ch) & 0xC0u) != 0x80u) {
                    ++line;
                }
            }
            chars = std::max(chars, line);
            const float w = static_cast<float>(chars) * 0.55f * em;
            const float h = 2.5f * em;
            boxes.push_back({sp.x - w / 2, sp.x + w / 2, sp.y - off - h / 2, sp.y - off + h / 2, on});
        }
        for (std::size_t j = 0; j < boxes.size(); ++j) {
            for (std::size_t i = 0; boxes[j].on && i < j; ++i) {
                if (!drawn[i] || !boxes[i].on) {
                    continue;
                }
                const float ox = std::min(boxes[i].x1, boxes[j].x1) - std::max(boxes[i].x0, boxes[j].x0);
                const float oy = std::min(boxes[i].y1, boxes[j].y1) - std::max(boxes[i].y0, boxes[j].y0);
                const float ai = (boxes[i].x1 - boxes[i].x0) * (boxes[i].y1 - boxes[i].y0);
                const float aj = (boxes[j].x1 - boxes[j].x0) * (boxes[j].y1 - boxes[j].y0);
                if (ox > 0.0f && oy > 0.0f && ox * oy > 0.5f * std::min(ai, aj)) {
                    drawn[j] = false;
                    break;
                }
            }
        }
    }
    labels_ok = labels_ok && preview.labels_drawn() == drawn;
    std::size_t hid = 0;
    for (const bool d : drawn) {
        hid += d ? 0u : 1u;
    }
    hidden_labels += hid;
    godot::Node* labels = root->get_node_or_null(godot::NodePath("Labels"));
    for (std::size_t k = 0; labels != nullptr && k < terrain::LABEL_KEEP; ++k) {
        auto* label = godot::Object::cast_to<godot::Label3D>(labels->get_node_or_null(godot::NodePath("l" + num(k))));
        if (label == nullptr) {
            labels_ok = false;
            continue;
        }
        if (k >= want.size() || !drawn[k]) {
            labels_ok = labels_ok && !label->is_visible();
            continue;
        }
        const terrain::Marker3D& mk = m[want[k]];
        const std::uint32_t mod = mk.rec.kind == MarkerKind::ruin ? 0xC8C8C8u : mk.rec.kind == MarkerKind::old_ruin ? 0xAAAAAAu : 0xFFFFFFu;
        const godot::Color got = label->get_modulate();
        const godot::Vector3 p = label->get_position();
        labels_ok = labels_ok && label->is_visible() && label->get_text() == to_godot(mk.rec.name + "\n" + mk.rec.kind_word) &&
                    label->get_billboard_mode() == godot::BaseMaterial3D::BILLBOARD_ENABLED &&
                    label->get_draw_flag(godot::Label3D::FLAG_FIXED_SIZE) &&
                    label->get_draw_flag(godot::Label3D::FLAG_DISABLE_DEPTH_TEST) && label->get_font_size() == 20 &&
                    label->get_outline_size() == 6 && label->get_offset() == godot::Vector2(0.0f, 24.0f) &&
                    std::lround(got.r * 255.0f) == static_cast<long>((mod >> 16) & 0xFF) &&
                    std::lround(got.b * 255.0f) == static_cast<long>(mod & 0xFF) && p.x == mk.X && p.z == mk.Z &&
                    p.y == mk.top + 2.0f;
    }
    if (report) {
        c.check(labels_ok && labels != nullptr, "T34 labels at " + where + ": " + num(want.size()) + " shown (" +
                                                    num(hid) + " hidden by the declutter), set, order, text and "
                                                    "properties as the check's own");
    }
    shown = want.size();
    return labels_ok && labels != nullptr && bad == 0;
}

}  // namespace

bool markers_only_run(const TerrainPreview& preview) {
    return preview.year_option().has_value() || !preview.at_marker_option().is_empty();
}

void run_marker_checks(TerrainPreview& preview, const TerrainCheckOptions& /*options*/, TerrainChecker& c) {
    const TerrainScene& scene = preview.scene();
    const terrain::Layout& layout = *scene.layout;
    const int n = layout.n;
    const godot::String status = preview.status_text();

    // T39: --no-markers, or a world without history.
    if (preview.no_markers_option()) {
        c.check(markers_node(preview) == nullptr && preview.markers().empty(), "T39 --no-markers: no Markers node");
        c.check(status.contains(terrain::MARKERS_OFF_STATUS), "T39 --no-markers: the status line");
        c.check(preview.markers_hud_text() == "markers off (--no-markers)", "T39 --no-markers: the HUD line");
        c.check(!preview.toggle_markers() && markers_node(preview) == nullptr, "T39 --no-markers: key M does nothing");
        return;
    }
    const worldgen::db::WorldReader* reader = preview.world()->reader();
    const OwnLoad own = own_records(*reader, preview.year_option());
    const terrain::MarkerLoad& load = preview.marker_load();
    if (!own.history) {
        c.check(own.ok && load.status == terrain::MarkerStatus::none, "T39 no history: marker status none (" +
                                                                          to_godot(own.problem) + ")");
        c.check(markers_node(preview) == nullptr, "T39 no history: no Markers node");
        c.check(status.contains("no history in this world: no markers"), "T39 no history: the status line");
        c.check(preview.markers_hud_text() == "markers: none, no history in this world", "T39 no history: the HUD line");
        c.check(!preview.toggle_markers(), "T39 no history: key M does nothing");
        return;
    }
    c.check(own.ok && load.status == terrain::MarkerStatus::loaded,
            "T31 the history and markers load (" + to_godot(own.problem + load.message) + ")");
    if (!own.ok || load.status != terrain::MarkerStatus::loaded) {
        return;
    }

    // T31: the records.
    {
        bool same = own.records.size() == load.records.size() && load.year == own.year && load.end_year == own.end;
        std::size_t first_bad = 0;
        for (std::size_t i = 0; same && i < own.records.size(); ++i) {
            if (!same_record(own.records[i], load.records[i])) {
                same = false;
                first_bad = i;
            }
        }
        std::array<std::size_t, 8> kinds{};
        std::size_t capitals = 0;
        for (const OwnRecord& r : own.records) {
            ++kinds[static_cast<std::size_t>(r.kind)];
            capitals += r.capital ? 1u : 0u;
        }
        c.check(same, "T31 " + num(load.records.size()) + " records at year " + godot::String::num_int64(load.year) +
                          " equal the check's own in order" + (same ? godot::String() : ", first difference at " + num(first_bad)));
        TerrainChecker::info("T31 at year " + godot::String::num_int64(own.year) + " of " + godot::String::num_int64(own.end) +
                             ": " + num(kinds[1]) + " settlements (" + num(capitals) + " capitals), " + num(kinds[2]) +
                             " ruins, " + num(kinds[3]) + " old ruins, " + num(kinds[4]) + " lairs, " + num(kinds[5]) +
                             " outposts, " + num(kinds[6]) + " mines, " + num(kinds[7]) + " towers");
        if (!preview.year_option()) {
            c.check(kinds[1] >= 1 && capitals >= 1 && kinds[2] + kinds[3] >= 1,
                    "T31 the world has a standing settlement, a capital and a ruin");
            for (std::size_t k = 4; k <= 7; ++k) {
                if (kinds[k] == 0) {
                    TerrainChecker::info("T31 this world has no " +
                                         godot::String(k == 4 ? "lair" : k == 5 ? "outpost" : k == 6 ? "mine" : "tower"));
                }
            }
        } else {
            c.check(*preview.year_option() <= own.end, "T35 --year " + godot::String::num_int64(*preview.year_option()) +
                                                           " is within the history (end " + godot::String::num_int64(own.end) + ")");
        }
    }

    // T32: the models.
    const std::vector<terrain::Marker3D>& m = preview.markers();
    godot::Node* root = markers_node(preview);
    c.check(root != nullptr && root == preview.markers_root() && root->get_child_count() == 3,
            "T32 one Markers node with Models, Beacons and Labels");
    if (root == nullptr) {
        return;
    }
    {
        godot::Node* models = root->get_node_or_null(godot::NodePath("Models"));
        const std::vector<std::array<int, 2>> cells = own_cells(own.records, n);
        std::size_t bad_place = 0;
        std::size_t bad_mesh = 0;
        std::size_t bad_range = 0;
        std::size_t unread = 0;
        bool count_ok = models != nullptr && static_cast<std::size_t>(models->get_child_count()) == m.size() &&
                        m.size() == own.records.size();
        for (std::size_t i = 0; count_ok && i < m.size(); ++i) {
            auto* node = godot::Object::cast_to<godot::MeshInstance3D>(models->get_child(static_cast<int32_t>(i)));
            const godot::String want_name = (is_site(m[i].rec.kind) ? "t" : "s") + godot::String::num_int64(m[i].rec.id);
            const float X = 16.0f * static_cast<float>(cells[i][0] - n / 2);
            const float Z = 16.0f * static_cast<float>(cells[i][1] - n / 2);
            const float Y = scene.heights[static_cast<std::size_t>(cells[i][1]) * static_cast<std::size_t>(n) +
                                          static_cast<std::size_t>(cells[i][0])];
            if (node == nullptr || node->get_name() != godot::StringName(want_name)) {
                ++bad_place;
                continue;
            }
            const godot::Vector3 p = node->get_position();
            bad_place += (m[i].cell_x == cells[i][0] && m[i].cell_y == cells[i][1] && p.x == X && p.y == Y && p.z == Z)
                             ? 0u
                             : 1u;
            const float far = m[i].rec.kind == MarkerKind::old_ruin ? 3000.0f : 8000.0f;
            bad_range += (node->get_visibility_range_end() == far && node->get_visibility_range_end_margin() == 500.0f &&
                          node->get_visibility_range_fade_mode() == godot::GeometryInstance3D::VISIBILITY_RANGE_FADE_SELF)
                             ? 0u
                             : 1u;
            const terrain::MeshData want = terrain::build_marker_mesh(m[i], layout, scene.heights);
            const godot::Ref<godot::Mesh> mesh = node->get_mesh();
            if (mesh.is_null() || mesh->get_surface_count() < 1) {
                ++bad_mesh;
                continue;
            }
            const godot::Array arrays = mesh->surface_get_arrays(0);
            if (arrays.size() <= godot::Mesh::ARRAY_VERTEX) {
                ++unread;
                continue;
            }
            const godot::PackedVector3Array v = arrays[godot::Mesh::ARRAY_VERTEX];
            if (v.is_empty()) {
                ++unread;
                continue;
            }
            bool ok = static_cast<std::size_t>(v.size()) * 3 == want.positions.size();
            for (std::size_t q = 0; ok && q < want.positions.size() / 3; ++q) {
                const godot::Vector3 g = v[static_cast<int64_t>(q)];
                ok = g.x == want.positions[3 * q] && g.y == want.positions[3 * q + 1] && g.z == want.positions[3 * q + 2];
            }
            bad_mesh += ok ? 0u : 1u;
        }
        c.check(count_ok, "T32 Models holds one MeshInstance3D per record (" + num(m.size()) + ")");
        c.check(count_ok && bad_place == 0, "T32 names and positions at the check's own cells, Y the height buffer (" +
                                                num(bad_place) + " differ)");
        c.check(count_ok && bad_range == 0, "T32 visibility ranges of 5 (" + num(bad_range) + " differ)");
        if (unread == m.size() && !m.empty()) {
            TerrainChecker::info("T32 the renderer keeps no mesh arrays here: marker meshes not compared");
        } else {
            c.check(count_ok && bad_mesh == 0, "T32 mesh arrays equal build_marker_mesh (" + num(bad_mesh) + " differ, " +
                                                   num(unread) + " unread)");
        }
    }

    // T33 and T34 at three points and along a path.
    const std::vector<std::size_t>& order = preview.marker_jump_order();
    const terrain::FlyState saved = preview.fly_state();
    {
        auto* beacons = godot::Object::cast_to<godot::MultiMeshInstance3D>(root->get_node_or_null(godot::NodePath("Beacons")));
        std::size_t want_count = 0;
        for (const terrain::Marker3D& k : m) {
            want_count += k.rec.kind == MarkerKind::old_ruin ? 0u : 1u;
        }
        c.check(beacons != nullptr && beacons->get_multimesh().is_valid() &&
                    static_cast<std::size_t>(beacons->get_multimesh()->get_instance_count()) == want_count,
                "T33 one beacon per record but old ruins (" + num(want_count) + ")");
        std::size_t shown = 0;
        std::size_t hidden = 0;
        std::size_t path_hidden = 0;
        if (!order.empty()) {
            const terrain::Marker3D& big = m[order.front()];
            check_point(preview, c, "300 m above the largest settlement", big.X, big.top + 300.0f, big.Z, true, shown, hidden);
        }
        const float g = scene.heights[static_cast<std::size_t>(n / 2) * static_cast<std::size_t>(n) + static_cast<std::size_t>(n / 2)];
        check_point(preview, c, "3 km above the centre", 0.0f, g + 3000.0f, 0.0f, true, shown, hidden);
        check_point(preview, c, "30 km south of the map", 0.0f, 3000.0f, 8.0f * static_cast<float>(n) + 30000.0f, true,
                    shown, hidden);
        if (!order.empty()) {
            const terrain::Marker3D& big = m[order.front()];
            bool path_ok = true;
            std::size_t most = 0;
            for (int frame = 0; frame < 40; ++frame) {
                const float z = big.Z + 9000.0f - 450.0f * static_cast<float>(frame);
                path_ok = check_point(preview, c, "", big.X + 300.0f, big.top + 400.0f, z, false, shown, path_hidden) && path_ok;
                most = std::max(most, shown);
            }
            c.check(path_ok, "T34 labels along a 40-frame path north over the largest settlement equal the check's own "
                             "with the previous frame's set (most shown " + num(most) + ", " + num(path_hidden) +
                             " hidden by the declutter in all)");
        }
    }

    // T35: status and HUD lines.
    {
        std::array<std::size_t, 8> k{};
        std::size_t capitals = 0;
        for (const terrain::Marker3D& r : m) {
            ++k[static_cast<std::size_t>(r.rec.kind)];
            capitals += r.rec.capital ? 1u : 0u;
        }
        const std::string line = "markers: " + std::to_string(k[1]) + " settlements (" + std::to_string(capitals) +
                                 " capitals), " + std::to_string(k[2] + k[3]) + " ruins (" + std::to_string(k[3]) + " old), " +
                                 std::to_string(k[4]) + " lairs, " + std::to_string(k[5]) + " outposts, " +
                                 std::to_string(k[6]) + " mines, " + std::to_string(k[7]) + " towers at year " +
                                 std::to_string(own.year) + " of " + std::to_string(own.end);
        c.check(status.contains(to_godot(line)), "T35 the status line '" + to_godot(line) + "'");
        // The HUD at a point 500 m south of the largest settlement (near
        // form) and 30 km south of the map (far form).
        if (!order.empty()) {
            const terrain::Marker3D& big = m[order.front()];
            for (const float back : {500.0f, 8.0f * static_cast<float>(n) + 30000.0f - big.Z}) {
                const float cz = big.Z + back;
                preview.markers_frame(big.X, big.top + 300.0f, cz);
                std::size_t best = 0;
                float bd = 1e30f;
                for (std::size_t i = 0; i < m.size(); ++i) {
                    const float d = (m[i].X - big.X) * (m[i].X - big.X) + (m[i].Z - cz) * (m[i].Z - cz);
                    if (d < bd) {
                        bd = d;
                        best = i;
                    }
                }
                const float dx = m[best].X - big.X;
                const float dz = m[best].Z - cz;
                const float d = std::sqrt(dx * dx + dz * dz);
                std::string want = "markers " + std::to_string(m.size()) + " at year " + std::to_string(own.year) + "; nearest ";
                want += d <= 2000.0f ? terrain::distance_text(d) + " " + terrain::bearing_word(dx, dz) + ": " + m[best].rec.text
                                     : m[best].rec.name + " (" + m[best].rec.kind_word + "), " + terrain::distance_text(d) +
                                           " " + terrain::bearing_word(dx, dz);
                want = terrain::cut_utf8(want, 200);
                const std::string got = preview.markers_hud_text();
                c.check(got == want, "T35 HUD line " + godot::String(d <= 2000.0f ? "near" : "far") + ": '" + to_godot(got) + "'" +
                                         (got == want ? godot::String() : "\n          expected '" + to_godot(want) + "'"));
            }
        }
        if (!preview.year_option()) {
            // A --year above E: the records at E and the note (in process).
            const terrain::MarkerLoad later = terrain::load_marker_records(*reader, own.end + 1000);
            bool same = later.status == terrain::MarkerStatus::loaded && later.year == own.end && later.year_clamped &&
                        later.records.size() == own.records.size();
            for (std::size_t i = 0; same && i < own.records.size(); ++i) {
                same = same_record(own.records[i], later.records[i]);
            }
            const std::string note = "--year " + std::to_string(own.end + 1000) + " is after the history's end: markers at " +
                                     std::to_string(own.end);
            c.check(same && terrain::markers_status_line(later).find(note) != std::string::npos,
                    "T35 a --year after the end gives the records at E and '" + to_godot(note) + "'");
        }
    }

    // T37: the jump order and view, and --at-marker.
    if (!order.empty()) {
        bool sorted = true;
        for (std::size_t i = 1; i < order.size(); ++i) {
            const terrain::MarkerRecord& a = m[order[i - 1]].rec;
            const terrain::MarkerRecord& b = m[order[i]].rec;
            sorted = sorted && (a.population > b.population || (a.population == b.population && a.id < b.id));
        }
        std::size_t standing = 0;
        for (const terrain::Marker3D& k : m) {
            standing += k.rec.kind == MarkerKind::settlement ? 1u : 0u;
        }
        c.check(sorted && order.size() == standing, "T37 the jump order: " + num(order.size()) +
                                                        " standing settlements by population");
        const godot::String at = preview.at_marker_option();
        if (at.is_empty()) {
            TerrainChecker::info("largest settlement " + godot::String::num_int64(m[order.front()].rec.id));
            const terrain::FlyState first = own_jump(m[order[0]], n, scene.heights);
            const terrain::FlyState second = own_jump(m[order[order.size() > 1 ? 1 : 0]], n, scene.heights);
            const terrain::FlyState last = own_jump(m[order.back()], n, scene.heights);
            bool ok = preview.jump_position() == -1;
            ok = ok && preview.jump_marker(1) && same_view(preview.fly_state(), first);
            ok = ok && preview.jump_marker(1) && same_view(preview.fly_state(), second);
            ok = ok && preview.jump_marker(-1) && same_view(preview.fly_state(), first);
            ok = ok && preview.jump_marker(-1) && same_view(preview.fly_state(), last);
            c.check(ok, "T37 N, N, Shift+N, Shift+N: entries 1, 2, 1 and the last (" + num(order.size()) + ")");
        } else if (at == "nowhere") {
            const terrain::FlyState s = preview.start_state();
            c.check(s.x == 0.0f && s.z == 6.0f * static_cast<float>(n) && s.heading_deg == 0.0f && s.pitch_deg == -25.0f,
                    "T37 --at-marker nowhere: the default start");
            const std::string line = terrain::at_marker_miss("nowhere", own.year);
            c.check(status.contains(to_godot(line)), "T37 --at-marker nowhere: the status line '" + to_godot(line) + "'");
        } else {
            const terrain::Marker3D& want = m[order.front()];
            const bool by_id = at.begins_with("settlement:");
            c.check(!by_id || at == "settlement:" + godot::String::num_int64(want.rec.id),
                    "T37 " + at + " names the largest settlement");
            c.check(same_view(preview.start_state(), own_jump(want, n, scene.heights)),
                    "T37 --at-marker " + at + ": the start is the jump view of order entry 1");
        }
    }

    // T36: key M.
    {
        godot::Node* models = root->get_node_or_null(godot::NodePath("Models"));
        const int32_t children = models != nullptr ? models->get_child_count() : -1;
        const bool off = preview.toggle_markers() && !preview.markers_root()->is_visible() && preview.markers_hud_text() == "markers off (M)";
        const bool on = preview.toggle_markers() && preview.markers_root()->is_visible() && preview.markers_hud_text().rfind("markers ", 0) == 0 &&
                        preview.markers_hud_text() != "markers off (M)";
        c.check(off && on && markers_node(preview) == root && models != nullptr && models->get_child_count() == children &&
                    preview.markers().size() == m.size(),
                "T36 key M hides Markers and sets the HUD line, M again restores; nothing rebuilt");
    }
    preview.set_fly_state(saved);
}

}  // namespace unmapped
