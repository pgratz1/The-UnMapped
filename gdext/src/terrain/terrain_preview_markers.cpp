// The markers of the 3D terrain preview (spec V3D-05 section 10): the
// Markers node with its models, beacons and labels, the frame update, keys M
// and N, the status and HUD lines and the bench lines. The records, their
// placement, meshes, beacon transforms and label choice come from
// markers3d.hpp (plain C++).

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>
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
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "terrain/markers3d.hpp"
#include "terrain/terrain_preview.hpp"

namespace unmapped {

namespace {

godot::String to_godot(const std::string& text) {
    return godot::String::utf8(text.c_str(), static_cast<int64_t>(text.size()));
}

std::string to_utf8(const godot::String& text) {
    const godot::CharString utf8 = text.utf8();
    return {utf8.get_data(), static_cast<std::size_t>(utf8.length())};
}

std::uint64_t now_usec() {
    return godot::Time::get_singleton()->get_ticks_usec();
}

godot::String ms(std::uint64_t usec) {
    return to_godot(terrain::one_decimal(static_cast<double>(usec) / 1000.0));
}

godot::Color srgb(std::uint32_t rgb) {
    return godot::Color(static_cast<float>((rgb >> 16) & 0xFF) / 255.0f, static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                        static_cast<float>(rgb & 0xFF) / 255.0f);
}

godot::Ref<godot::ArrayMesh> array_mesh(const terrain::MeshData& data,
                                        const godot::Ref<godot::StandardMaterial3D>& material) {
    godot::PackedVector3Array vertices;
    godot::PackedVector3Array normals;
    godot::PackedColorArray colours;
    const std::size_t count = data.colours.size();
    vertices.resize(static_cast<int64_t>(count));
    normals.resize(static_cast<int64_t>(count));
    colours.resize(static_cast<int64_t>(count));
    godot::Vector3* vp = vertices.ptrw();
    godot::Vector3* np = normals.ptrw();
    godot::Color* cp = colours.ptrw();
    for (std::size_t v = 0; v < count; ++v) {
        vp[v] = godot::Vector3(data.positions[3 * v], data.positions[3 * v + 1], data.positions[3 * v + 2]);
        np[v] = godot::Vector3(data.normals[3 * v], data.normals[3 * v + 1], data.normals[3 * v + 2]);
        const std::uint32_t c = data.colours[v];
        cp[v] = godot::Color(static_cast<float>((c >> 24) & 0xFF) / 255.0f, static_cast<float>((c >> 16) & 0xFF) / 255.0f,
                             static_cast<float>((c >> 8) & 0xFF) / 255.0f, static_cast<float>(c & 0xFF) / 255.0f);
    }
    godot::Array arrays;
    arrays.resize(godot::Mesh::ARRAY_MAX);
    arrays[godot::Mesh::ARRAY_VERTEX] = vertices;
    arrays[godot::Mesh::ARRAY_NORMAL] = normals;
    arrays[godot::Mesh::ARRAY_COLOR] = colours;
    godot::Ref<godot::ArrayMesh> mesh;
    mesh.instantiate();
    if (count > 0) {
        mesh->add_surface_from_arrays(godot::Mesh::PRIMITIVE_TRIANGLES, arrays);
        mesh->surface_set_material(0, material);
    }
    return mesh;
}

}  // namespace

int TerrainPreview::marker_viewport_height() const {
    const godot::Viewport* viewport = get_viewport();
    if (viewport == nullptr) {
        return 900;
    }
    const int h = static_cast<int>(viewport->get_visible_rect().size.y);
    return h > 0 ? h : 900;
}

void TerrainPreview::markers_close() {
    if (markers_root_ != nullptr) {
        remove_child(markers_root_);
        markers_root_->queue_free();
    }
    markers_root_ = nullptr;
    beacons_ = nullptr;
    labels_.fill(nullptr);
    shown_labels_.clear();
    labels_drawn_.clear();
    beacon_index_.clear();
    markers_.clear();
    jump_order_.clear();
    at_marker_index_.reset();
    marker_load_ = terrain::MarkerLoad{};
    marker_status_.clear();
    marker_hud_.clear();
    marker_mesh_usec_ = 0;
    marker_node_usec_ = 0;
    marker_place_usec_ = 0;
    marker_triangles_ = 0;
}

void TerrainPreview::markers_load() {
    // Called by open_world after markers_close (clear_world), before the
    // ground field: the records of the shown year and, with ground detail,
    // the pads' places of every year (V3D-06 7, Q-V3D-06-4).
    jump_pos_ = -1;
    if (no_markers_) {
        marker_status_.push_back(terrain::MARKERS_OFF_STATUS);
        return;
    }
    const worldgen::db::WorldReader* reader = world_.is_valid() ? world_->reader() : nullptr;
    if (reader == nullptr) {
        marker_load_.status = terrain::MarkerStatus::failed;
        marker_load_.code = "no_reader";
        marker_load_.message = "the world file is not open";
    } else {
        marker_load_ = terrain::load_marker_records(*reader, opt_year_, ground_detail_ != 0, scene_.layout->n);
    }
    marker_status_.push_back(to_godot(terrain::markers_status_line(marker_load_)).split("\n")[0]);
    if (marker_load_.year_clamped) {
        marker_status_.push_back(to_godot(terrain::markers_status_line(marker_load_)).split("\n")[1]);
    }
    if (marker_load_.status != terrain::MarkerStatus::loaded && !at_marker_.is_empty()) {
        marker_status_.push_back(to_godot(terrain::at_marker_miss(to_utf8(at_marker_), marker_load_.year)));
    }
}

void TerrainPreview::markers_open() {
    // After the terrain: placement on the field and the nodes.
    if (no_markers_ || marker_load_.status != terrain::MarkerStatus::loaded) {
        return;
    }
    const std::uint64_t t0 = now_usec();
    markers_ = terrain::place_markers(marker_load_.records, *scene_.layout, scene_.field);
    jump_order_ = terrain::jump_order(markers_);
    marker_place_usec_ = now_usec() - t0;
    if (!at_marker_.is_empty()) {
        at_marker_index_ = terrain::find_marker(markers_, jump_order_, to_utf8(at_marker_));
        if (!at_marker_index_) {
            marker_status_.push_back(to_godot(terrain::at_marker_miss(to_utf8(at_marker_), marker_load_.year)));
        } else {
            const auto it = std::find(jump_order_.begin(), jump_order_.end(), *at_marker_index_);
            if (it != jump_order_.end()) {
                jump_pos_ = static_cast<int>(it - jump_order_.begin());
            }
        }
    }
    markers_build_nodes();
}

void TerrainPreview::markers_build_nodes() {
    std::uint64_t t0 = now_usec();
    if (marker_material_.is_null()) {
        marker_material_.instantiate();
        marker_material_->set_flag(godot::BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
        marker_material_->set_flag(godot::BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
        marker_material_->set_roughness(0.9f);
        marker_material_->set_cull_mode(godot::BaseMaterial3D::CULL_BACK);
        beacon_material_.instantiate();
        beacon_material_->set_shading_mode(godot::BaseMaterial3D::SHADING_MODE_UNSHADED);
        beacon_material_->set_flag(godot::BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
        beacon_material_->set_flag(godot::BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
        beacon_material_->set_cull_mode(godot::BaseMaterial3D::CULL_DISABLED);
    }
    std::vector<godot::Ref<godot::ArrayMesh>> meshes;
    meshes.reserve(markers_.size());
    marker_triangles_ = 0;
    for (const terrain::Marker3D& m : markers_) {
        const terrain::MeshData data = terrain::build_marker_mesh(m, scene_.field.layout, scene_.field.heights);
        marker_triangles_ += data.triangles();
        meshes.push_back(array_mesh(data, marker_material_));
    }
    marker_mesh_usec_ = now_usec() - t0;

    t0 = now_usec();
    markers_root_ = memnew(godot::Node3D);
    markers_root_->set_name("Markers");
    godot::Node3D* models = memnew(godot::Node3D);
    models->set_name("Models");
    markers_root_->add_child(models);
    for (std::size_t i = 0; i < markers_.size(); ++i) {
        const terrain::Marker3D& m = markers_[i];
        const bool site = m.rec.kind == terrain::MarkerKind::lair || m.rec.kind == terrain::MarkerKind::outpost ||
                          m.rec.kind == terrain::MarkerKind::mine || m.rec.kind == terrain::MarkerKind::tower;
        godot::MeshInstance3D* node = memnew(godot::MeshInstance3D);
        node->set_name((site ? "t" : "s") + godot::String::num_int64(m.rec.id));
        node->set_mesh(meshes[i]);
        node->set_position(godot::Vector3(m.X, m.Y, m.Z));
        node->set_cast_shadows_setting(godot::GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        node->set_visibility_range_end(terrain::model_far(m.rec.kind));
        node->set_visibility_range_end_margin(terrain::MODEL_FADE_MARGIN);
        node->set_visibility_range_fade_mode(godot::GeometryInstance3D::VISIBILITY_RANGE_FADE_SELF);
        models->add_child(node);
    }

    // Beacons: one MultiMesh, the quad (-0.5, 0, 0) to (0.5, 1, 0).
    beacon_index_.clear();
    for (std::size_t i = 0; i < markers_.size(); ++i) {
        if (markers_[i].beacon) {
            beacon_index_.push_back(i);
        }
    }
    {
        godot::PackedVector3Array vertices;
        const godot::Vector3 q[4] = {godot::Vector3(-0.5f, 0.0f, 0.0f), godot::Vector3(0.5f, 0.0f, 0.0f),
                                     godot::Vector3(0.5f, 1.0f, 0.0f), godot::Vector3(-0.5f, 1.0f, 0.0f)};
        for (const int k : {0, 1, 2, 0, 2, 3}) {
            vertices.push_back(q[k]);
        }
        godot::PackedVector3Array normals;
        for (int k = 0; k < 6; ++k) {
            normals.push_back(godot::Vector3(0.0f, 0.0f, 1.0f));
        }
        godot::Array arrays;
        arrays.resize(godot::Mesh::ARRAY_MAX);
        arrays[godot::Mesh::ARRAY_VERTEX] = vertices;
        arrays[godot::Mesh::ARRAY_NORMAL] = normals;
        godot::Ref<godot::ArrayMesh> quad;
        quad.instantiate();
        quad->add_surface_from_arrays(godot::Mesh::PRIMITIVE_TRIANGLES, arrays);
        quad->surface_set_material(0, beacon_material_);
        godot::Ref<godot::MultiMesh> mm;
        mm.instantiate();
        mm->set_transform_format(godot::MultiMesh::TRANSFORM_3D);
        mm->set_use_colors(true);
        mm->set_mesh(quad);
        mm->set_instance_count(static_cast<int32_t>(beacon_index_.size()));
        const float half = 8.0f * static_cast<float>(scene_.layout->n) + 30000.0f;
        const godot::AABB box(godot::Vector3(-half, -1000.0f, -half), godot::Vector3(2.0f * half, BEACON_TOP + 1000.0f, 2.0f * half));
        mm->set_custom_aabb(box);
        beacons_ = memnew(godot::MultiMeshInstance3D);
        beacons_->set_name("Beacons");
        beacons_->set_multimesh(mm);
        beacons_->set_custom_aabb(box);
        beacons_->set_cast_shadows_setting(godot::GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        markers_root_->add_child(beacons_);
        for (std::size_t b = 0; b < beacon_index_.size(); ++b) {
            mm->set_instance_color(static_cast<int32_t>(b), srgb(markers_[beacon_index_[b]].beacon_colour));
        }
    }

    // Labels l0 to l15.
    godot::Node3D* labels = memnew(godot::Node3D);
    labels->set_name("Labels");
    markers_root_->add_child(labels);
    for (std::size_t k = 0; k < labels_.size(); ++k) {
        godot::Label3D* label = memnew(godot::Label3D);
        label->set_name("l" + godot::String::num_int64(static_cast<int64_t>(k)));
        label->set_billboard_mode(godot::BaseMaterial3D::BILLBOARD_ENABLED);
        label->set_draw_flag(godot::Label3D::FLAG_FIXED_SIZE, true);
        label->set_draw_flag(godot::Label3D::FLAG_DISABLE_DEPTH_TEST, true);
        label->set_font_size(LABEL_FONT_SIZE);
        label->set_outline_size(LABEL_OUTLINE_SIZE);
        label->set_outline_modulate(godot::Color(0.0f, 0.0f, 0.0f, 1.0f));
        label->set_offset(godot::Vector2(0.0f, LABEL_OFFSET_PX));
        label->set_pixel_size(LABEL_PIXEL_SIZE);
        label->set_visible(false);
        labels->add_child(label);
        labels_[k] = label;
    }
    markers_root_->set_visible(markers_on_);
    add_child(markers_root_);
    marker_node_usec_ = now_usec() - t0;
}

void TerrainPreview::markers_frame(float X, float Y, float Z) {
    if (markers_root_ == nullptr || !markers_root_->is_visible() || beacons_ == nullptr) {
        return;
    }
    const std::uint64_t t0 = now_usec();
    const float a = terrain::pixel_angle(camera_ != nullptr ? camera_->get_fov() : 70.0f, marker_viewport_height());
    // Beacons: one buffer, 12 floats of the transform (rows with the origin)
    // and 4 of the colour per instance.
    godot::PackedFloat32Array buffer;
    buffer.resize(static_cast<int64_t>(beacon_index_.size() * 16));
    float* out = buffer.ptrw();
    for (std::size_t b = 0; b < beacon_index_.size(); ++b) {
        const terrain::Marker3D& m = markers_[beacon_index_[b]];
        const std::array<float, 12> t = terrain::beacon_transform(m, X, Y, Z, a);
        float* p = out + 16 * b;
        p[0] = t[0];
        p[1] = t[3];
        p[2] = t[6];
        p[3] = t[9];
        p[4] = t[1];
        p[5] = t[4];
        p[6] = t[7];
        p[7] = t[10];
        p[8] = t[2];
        p[9] = t[5];
        p[10] = t[8];
        p[11] = t[11];
        const std::uint32_t c = m.beacon_colour;
        p[12] = static_cast<float>((c >> 16) & 0xFF) / 255.0f;
        p[13] = static_cast<float>((c >> 8) & 0xFF) / 255.0f;
        p[14] = static_cast<float>(c & 0xFF) / 255.0f;
        p[15] = 1.0f;
    }
    if (!beacon_index_.empty()) {
        beacons_->get_multimesh()->set_buffer(buffer);
    }
    // Labels, l0 the nearest; a label mostly over a nearer one's is hidden.
    shown_labels_ = terrain::select_labels(markers_, X, Y, Z, shown_labels_);
    labels_drawn_.assign(shown_labels_.size(), true);
    if (terrain::LABEL_DECLUTTER && camera_ != nullptr) {
        const float em = static_cast<float>(LABEL_FONT_SIZE) * LABEL_PIXEL_SIZE / a;
        const float offset = LABEL_OFFSET_PX * LABEL_PIXEL_SIZE / a;
        std::vector<terrain::LabelBox> boxes;
        boxes.reserve(shown_labels_.size());
        for (const std::size_t i : shown_labels_) {
            const terrain::Marker3D& m = markers_[i];
            const godot::Vector3 point(m.X, m.top + terrain::LABEL_LIFT, m.Z);
            const bool behind = camera_->is_position_behind(point);
            const godot::Vector2 sp = behind ? godot::Vector2() : camera_->unproject_position(point);
            boxes.push_back(terrain::label_box(m.rec, sp.x, sp.y, em, offset, !behind));
        }
        labels_drawn_ = terrain::declutter_labels(boxes);
    }
    for (std::size_t k = 0; k < labels_.size(); ++k) {
        godot::Label3D* label = labels_[k];
        if (k >= shown_labels_.size() || !labels_drawn_[k]) {
            if (label->is_visible()) {
                label->set_visible(false);
            }
            continue;
        }
        const terrain::Marker3D& m = markers_[shown_labels_[k]];
        const godot::String text = to_godot(terrain::label_text(m.rec));
        if (label->get_text() != text) {
            label->set_text(text);
            label->set_modulate(srgb(terrain::label_modulate(m.rec.kind)));
        }
        label->set_position(godot::Vector3(m.X, m.top + terrain::LABEL_LIFT, m.Z));
        if (!label->is_visible()) {
            label->set_visible(true);
        }
    }
    marker_hud_ = terrain::markers_hud_line(terrain::MarkersHud::on, markers_, marker_load_.year, X, Z);
    if (marker_bench_view_) {
        marker_frame_usec_.push_back(now_usec() - t0);
    }
}

std::string TerrainPreview::markers_hud_text() const {
    using terrain::MarkersHud;
    if (no_markers_) {
        return terrain::markers_hud_line(MarkersHud::disabled, {}, -1, 0.0f, 0.0f);
    }
    switch (marker_load_.status) {
        case terrain::MarkerStatus::none: return terrain::markers_hud_line(MarkersHud::none, {}, -1, 0.0f, 0.0f);
        case terrain::MarkerStatus::failed: return terrain::markers_hud_line(MarkersHud::failed, {}, -1, 0.0f, 0.0f);
        case terrain::MarkerStatus::loaded: break;
    }
    if (markers_root_ == nullptr || !markers_root_->is_visible()) {
        return terrain::markers_hud_line(MarkersHud::off, markers_, marker_load_.year, 0.0f, 0.0f);
    }
    if (marker_hud_.empty()) {
        return terrain::markers_hud_line(MarkersHud::on, markers_, marker_load_.year, fly_.x, fly_.z);
    }
    return marker_hud_;
}

bool TerrainPreview::toggle_markers() {
    if (markers_root_ == nullptr) {
        return false;
    }
    markers_on_ = !markers_on_;
    markers_root_->set_visible(markers_on_);
    marker_hud_.clear();
    update_hud();
    return true;
}

bool TerrainPreview::jump_marker(int direction) {
    if (jump_order_.empty() || !scene_.layout) {
        return false;
    }
    const int count = static_cast<int>(jump_order_.size());
    if (direction >= 0) {
        jump_pos_ = jump_pos_ < 0 ? 0 : (jump_pos_ + 1) % count;
    } else {
        jump_pos_ = jump_pos_ <= 0 ? count - 1 : jump_pos_ - 1;
    }
    const int speed = fly_.speed_index;
    fly_ = terrain::jump_view(markers_[jump_order_[static_cast<std::size_t>(jump_pos_)]], scene_.field.layout, scene_.field.heights);
    fly_.speed_index = speed;
    apply_camera();
    marker_hud_.clear();
    update_hud();
    return true;
}

void TerrainPreview::print_markers_bench_line() {
    if (marker_load_.status != terrain::MarkerStatus::loaded) {
        return;
    }
    std::size_t s = 0, r = 0, o = 0, l = 0, p = 0, m = 0, t = 0;
    for (const terrain::Marker3D& k : markers_) {
        switch (k.rec.kind) {
            case terrain::MarkerKind::settlement: ++s; break;
            case terrain::MarkerKind::ruin: ++r; break;
            case terrain::MarkerKind::old_ruin: ++o; break;
            case terrain::MarkerKind::lair: ++l; break;
            case terrain::MarkerKind::outpost: ++p; break;
            case terrain::MarkerKind::mine: ++m; break;
            case terrain::MarkerKind::tower: ++t; break;
        }
    }
    const auto num = [](std::size_t v) { return godot::String::num_int64(static_cast<int64_t>(v)); };
    godot::UtilityFunctions::print(
        "UNMAPPED-TERRAIN-BENCH: markers: history ", ms(marker_load_.history_usec), " ms, records ",
        ms(marker_load_.records_usec + marker_place_usec_), " ms, meshes ", ms(marker_mesh_usec_), " ms, nodes ",
        ms(marker_node_usec_), " ms; ", num(markers_.size()), " markers (", num(s), " settlements, ", num(r), " ruins, ",
        num(o), " old, ", num(l), " lairs, ", num(p), " outposts, ", num(m), " mines, ", num(t), " towers), ",
        num(static_cast<std::size_t>(marker_triangles_)), " triangles, beacons ", num(beacon_index_.size()));
}

void TerrainPreview::print_markers_frame_line() {
    if (marker_load_.status != terrain::MarkerStatus::loaded || marker_frame_usec_.empty()) {
        return;
    }
    std::uint64_t sum = 0;
    std::uint64_t max = 0;
    for (const std::uint64_t v : marker_frame_usec_) {
        sum += v;
        max = std::max(max, v);
    }
    godot::UtilityFunctions::print("UNMAPPED-TERRAIN-BENCH: markers per frame over the view: mean ",
                                   godot::String::num_int64(static_cast<int64_t>(sum / marker_frame_usec_.size())),
                                   " us, max ", godot::String::num_int64(static_cast<int64_t>(max)), " us");
}

}  // namespace unmapped
