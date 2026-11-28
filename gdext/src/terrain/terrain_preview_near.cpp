// The near tier of the preview's vegetation (spec V3D-03 7, 8, 9.2, 12):
// worker tasks, the near nodes, the HUD line and the bench's additions.
//
// Threads: a worker task (compute_near_slot) reads only its NearSlot and
// the VegetationInputs, which point into TerrainScene's buffers; it writes
// only its slot's buffers, counts and time. It makes no Godot call. The
// main thread fills a slot before add_task and reads it only after
// is_task_completed and wait_for_task_completion. near_close() waits for
// every task before the scene's buffers change (another world, key G) or
// the scene exits, so no task outlives its inputs and no result reaches a
// later world.

#include <algorithm>
#include <chrono>
#include <cstring>
#include <utility>

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "terrain/placeholder_meshes.hpp"
#include "terrain/terrain_preview.hpp"

namespace unmapped {

namespace {

std::uint64_t now_usec() { return godot::Time::get_singleton()->get_ticks_usec(); }

godot::String to_godot(const std::string& text) {
    return godot::String::utf8(text.c_str(), static_cast<int64_t>(text.size()));
}

godot::String ms(std::uint64_t usec) { return to_godot(terrain::one_decimal(static_cast<double>(usec) / 1000.0)); }

godot::String num(std::uint64_t v) { return godot::String::num_int64(static_cast<int64_t>(v)); }

double static_mb() {
    return godot::Performance::get_singleton()->get_monitor(godot::Performance::MEMORY_STATIC) / (1024.0 * 1024.0);
}

constexpr std::uint64_t SETTLE_TIMEOUT_USEC = 30000000;  // 30 s (9.1, 12)

// The work of one task: plain C++ on the slot and the shared inputs.
void compute_slot(NearSlot& s) {
    const auto t0 = std::chrono::steady_clock::now();
    const int i = s.cell % s.m;
    const int j = s.cell / s.m;
    const auto instances = terrain::near_instances(*s.inputs, i, j, s.threshold);
    for (std::size_t k = 0; k < instances.size(); ++k) {
        s.counts[k] = static_cast<std::uint32_t>(instances[k].size());
        s.buffers[k] = terrain::instance_buffer(instances[k], s.tint_mode);
    }
    const auto t1 = std::chrono::steady_clock::now();
    s.usec = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
}

// 4.1: the near tier's visibility range of a kind and LOD.
struct Range {
    float begin;
    float end;
    float margin;
    bool fade;
};

Range near_range(int kind, int lod) {
    const auto full = static_cast<float>(terrain::NEAR_FULL_END);
    const auto fade = static_cast<float>(terrain::NEAR_FADE);
    switch (kind) {
        case 0:
        case 1:
            return lod == 0 ? Range{0.0f, 600.0f, 0.0f, false} : Range{600.0f, full, fade, true};
        case 2:
            return {0.0f, 1000.0f, 0.0f, false};
        case 3:
            return {0.0f, 800.0f, 0.0f, false};
        default:
            return {0.0f, full, fade, true};
    }
}

}  // namespace

void TerrainPreview::near_open() {
    ++near_generation_;
    if (!scene_.tiers || terrain_ == nullptr || !scene_.layout) {
        return;
    }
    near_root_ = memnew(godot::Node3D);
    near_root_->set_name("NearVegetation");
    near_root_->set_visible(trees_visible_);
    add_child(near_root_);
    if (veg_radius_ <= 0.0) {
        return;  // --veg-radius 0: the far tier only
    }
    terrain::NearParams params;
    params.radius = veg_radius_;
    params.max_in_flight = veg_threads_ > 0 ? veg_threads_ : 2;
    near_ = std::make_unique<terrain::NearCells>(terrain::build_near_grid(*scene_.layout, scene_.heights), params);
}

void TerrainPreview::near_close() {
    // Wait for every running task; its result belongs to the world being
    // closed and is dropped.
    godot::WorkerThreadPool* pool = godot::WorkerThreadPool::get_singleton();
    for (NearSlot& slot : near_slots_) {
        if (slot.busy) {
            pool->wait_for_task_completion(slot.task);
            slot = NearSlot{};
        }
    }
    near_results_.clear();
    near_nodes_.clear();
    if (near_root_ != nullptr) {
        // Inside the tree's own exit the children go with the tree.
        if (!exiting_ && near_root_->get_parent() == this) {
            remove_child(near_root_);
            near_root_->queue_free();
        }
        near_root_ = nullptr;
    }
    near_.reset();
    near_stats_ = NearFrameStats{};
}

void TerrainPreview::compute_near_slot(int slot) {
    compute_slot(near_slots_[static_cast<std::size_t>(slot)]);
}

void TerrainPreview::finish_slot(NearSlot& slot) {
    const int q = slot.cell;
    const bool current = slot.generation == near_generation_ && near_ != nullptr;
    if (current) {
        NearResult result;
        std::uint64_t n = 0;
        for (std::size_t k = 0; k < result.buffers.size(); ++k) {
            result.buffers[k] = std::move(slot.buffers[k]);
            result.counts[k] = slot.counts[k];
            n += slot.counts[k];
        }
        near_results_[q] = std::move(result);
        near_->computed(q, n);
        compute_usec_sum_ += slot.usec;
        compute_usec_max_ = std::max(compute_usec_max_, slot.usec);
        ++compute_cells_;
    }
    slot = NearSlot{};
}

void TerrainPreview::near_frame() {
    near_stats_.attached.clear();
    near_stats_.dropped.clear();
    near_stats_.attach_usec = 0;
    near_stats_.in_flight = 0;
    if (!near_) {
        return;
    }
    godot::WorkerThreadPool* pool = godot::WorkerThreadPool::get_singleton();
    // 1. Finished tasks, in slot order (the state does not depend on it).
    for (NearSlot& slot : near_slots_) {
        if (slot.busy && pool->is_task_completed(slot.task)) {
            pool->wait_for_task_completion(slot.task);
            finish_slot(slot);
        }
    }
    // 2. The wanted set.
    terrain::NearViewer viewer;
    viewer.x = static_cast<double>(fly_.x);
    viewer.y = static_cast<double>(fly_.y);
    viewer.z = static_cast<double>(fly_.z);
    viewer.heading_deg = static_cast<double>(fly_.heading_deg);
    near_->update(viewer);
    // 3. Results no longer wanted.
    for (const int q : near_->take_released()) {
        near_results_.erase(q);
    }
    // 4. Drops.
    for (const int q : near_->take_drops()) {
        near_free_nodes(q);
        near_stats_.dropped.push_back(q);
    }
    // 5. Attach, in priority order.
    const std::uint64_t t0 = now_usec();
    for (const int q : near_->take_attach()) {
        const auto it = near_results_.find(q);
        if (it != near_results_.end()) {
            near_attach(q, it->second);
            near_results_.erase(it);
        }
        near_stats_.attached.push_back(q);
    }
    if (!near_stats_.attached.empty()) {
        near_stats_.attach_usec = now_usec() - t0;
        attach_usec_max_ = std::max(attach_usec_max_, near_stats_.attach_usec);
    }
    // 6. New tasks.
    for (const int q : near_->start_computing()) {
        NearSlot fill;
        fill.busy = true;
        fill.cell = q;
        fill.generation = near_generation_;
        fill.threshold = scene_.far_threshold;
        fill.inputs = &veg_inputs_;
        fill.m = near_->grid().m;
        fill.tint_mode = tier_colours_ ? 1 : 0;
        if (veg_threads_ == 0) {
            // --veg-threads 0: on the main thread, at most 2 cells a frame.
            compute_slot(fill);
            finish_slot(fill);
            continue;
        }
        std::size_t index = 0;
        while (index < near_slots_.size() && near_slots_[index].busy) {
            ++index;
        }
        if (index == near_slots_.size()) {
            godot::UtilityFunctions::printerr("TerrainPreview: no free near slot");
            continue;
        }
        near_slots_[index] = std::move(fill);
        near_slots_[index].task = pool->add_task(
            callable_mp(this, &TerrainPreview::compute_near_slot).bind(static_cast<int>(index)), false, "near vegetation");
    }
    near_stats_.in_flight = near_->counts().computing;
    if (forest_recording_) {
        forest_filled_ += near_stats_.attached.size();
        forest_dropped_ += near_stats_.dropped.size();
        forest_pending_max_ = std::max(forest_pending_max_, near_->counts().pending());
    }
}

void TerrainPreview::near_attach(int q, NearResult& result) {
    // 7.2: per kind with instances and per LOD, a MultiMesh with the
    // slot's buffer and a MultiMeshInstance3D at the origin.
    const terrain::NearBox& box = near_->grid().boxes[static_cast<std::size_t>(q)];
    const godot::AABB aabb(godot::Vector3(static_cast<float>(box.x0), static_cast<float>(box.bot), static_cast<float>(box.z0)),
                           godot::Vector3(static_cast<float>(box.x1 - box.x0), static_cast<float>(box.top - box.bot),
                                          static_cast<float>(box.z1 - box.z0)));
    std::vector<godot::MultiMeshInstance3D*>& nodes = near_nodes_[q];
    for (int kind = 0; kind < terrain::VEGETATION_KINDS; ++kind) {
        const auto k = static_cast<std::size_t>(kind);
        const std::uint32_t n = result.counts[k];
        if (n == 0) {
            continue;
        }
        godot::PackedFloat32Array buffer;
        buffer.resize(static_cast<int64_t>(result.buffers[k].size()));
        std::memcpy(buffer.ptrw(), result.buffers[k].data(), result.buffers[k].size() * sizeof(float));
        for (int lod = 0; lod < terrain::lod_count(kind); ++lod) {
            godot::Ref<godot::MultiMesh> mm;
            mm.instantiate();
            mm->set_transform_format(godot::MultiMesh::TRANSFORM_3D);
            mm->set_use_colors(true);
            mm->set_mesh(veg_meshes_[k][static_cast<std::size_t>(lod)]);
            mm->set_instance_count(static_cast<int32_t>(n));
            mm->set_buffer(buffer);
            mm->set_custom_aabb(aabb);
            godot::MultiMeshInstance3D* node = memnew(godot::MultiMeshInstance3D);
            node->set_name("n" + godot::String::num_int64(q) + "k" + godot::String::num_int64(kind) + "l" +
                           godot::String::num_int64(lod));
            node->set_multimesh(mm);
            node->set_cast_shadows_setting(godot::GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
            node->set_custom_aabb(aabb);
            const Range r = near_range(kind, lod);
            node->set_visibility_range_begin(r.begin);
            node->set_visibility_range_end(r.end);
            node->set_visibility_range_end_margin(r.margin);
            node->set_visibility_range_fade_mode(r.fade ? godot::GeometryInstance3D::VISIBILITY_RANGE_FADE_SELF
                                                        : godot::GeometryInstance3D::VISIBILITY_RANGE_FADE_DISABLED);
            node->set_layer_mask(1);
            near_root_->add_child(node);
            nodes.push_back(node);
        }
    }
}

void TerrainPreview::near_free_nodes(int q) {
    const auto it = near_nodes_.find(q);
    if (it == near_nodes_.end()) {
        return;
    }
    for (godot::MultiMeshInstance3D* node : it->second) {
        near_root_->remove_child(node);
        node->queue_free();
    }
    near_nodes_.erase(it);
}

bool TerrainPreview::near_settled() const { return !near_ || near_->settled(); }

std::string TerrainPreview::vegetation_hud_line() const {
    if (!scene_.vegetation || !trees_visible_ || terrain_ == nullptr) {
        return "vegetation off";
    }
    if (!scene_.tiers) {
        return "vegetation whole map " + std::to_string(scene_.placement.kept());
    }
    const terrain::NearCounts k = near_ ? near_->counts() : terrain::NearCounts{};
    return "vegetation near " + std::to_string(k.filled) + " cells, " + std::to_string(k.pending()) + " pending, " +
           std::to_string(k.instances) + " instances; far " + std::to_string(scene_.placement.kept());
}

void TerrainPreview::set_fly_state(const terrain::FlyState& state) {
    fly_ = state;
    apply_camera();
}

terrain::FlyState TerrainPreview::forest_start() const {
    terrain::FlyState s = start_;
    if (!scene_.layout) {
        return s;
    }
    const terrain::Vec2 w = terrain::cell_to_world(*scene_.layout, scene_.forest.x, scene_.forest.y);
    s.x = w.x;
    s.z = w.z;
    s.y = terrain::forest_start_altitude(terrain::ground_clamp(*scene_.layout, scene_.heights, w.x, w.z));
    s.heading_deg = scene_.forest.north ? 0.0f : 180.0f;
    s.pitch_deg = -8.0f;
    return s;
}

// --- Screenshot and bench phases (9.1, 12) ------------------------------------------

void TerrainPreview::print_tiers_line() {
    if (!scene_.tiers) {
        return;
    }
    const terrain::NearCounts k = near_ ? near_->counts() : terrain::NearCounts{};
    const godot::String settled =
        settled_seen_ ? "settled " + ms(settled_usec_) + " ms after the first frame" : godot::String("not settled within 30 s");
    godot::UtilityFunctions::print("UNMAPPED-TERRAIN-BENCH: tiers: far ", num(scene_.placement.kept()), " of ",
                                   num(scene_.far_candidates), " (budget ", num(instance_cap_), "), near radius ",
                                   num(static_cast<std::uint64_t>(veg_radius_)), " m, ", settled, ", near cells ",
                                   num(k.filled), ", near instances ", num(k.instances), ", memory static ",
                                   to_godot(terrain::one_decimal(static_mb())), " MB");
}

void TerrainPreview::start_forest_flight(int index) {
    // 12.1 with the lead's ruling: from the forest cell of column x* nearest
    // to row 7N/8, north or south along the longer run of forest, for
    // min(12 N, 8192) m, 60 m above the ground, pitch -8; first wait for the
    // near tier to settle there.
    forest_index_ = index;
    forest_speed_ = index == 0 ? 200.0f : 800.0f;
    forest_length_ = std::min(12.0f * static_cast<float>(scene_.layout->n), 8192.0f);
    forest_done_ = 0.0f;
    fly_ = forest_start();
    fly_.speed_index = index == 0 ? 4 : 5;
    apply_camera();
    phase_ = Phase::forest_settle;
    phase_frames_ = 0;
    phase_start_usec_ = now_usec();
}

void TerrainPreview::bench_phases(float dt) {
    const std::uint64_t t = now_usec();
    // The view point's settling (12.2): counted once the near tier has run
    // a frame there (view_frames_ > 0 in the drawn bench).
    const bool at_view = (phase_ == Phase::headless_settle && phase_frames_ > 0) || phase_ == Phase::view_settle ||
                         (phase_ == Phase::none && view_frames_ > 0);
    if (view_start_usec_ != 0 && !settled_seen_ && at_view && near_settled()) {
        settled_seen_ = true;
        settled_usec_ = t - view_start_usec_;
    }
    const bool timed_out = t - phase_start_usec_ > SETTLE_TIMEOUT_USEC;
    const bool settled_here = phase_frames_ > 0 && near_settled();
    switch (phase_) {
        case Phase::none:
            return;
        case Phase::screenshot_settle:
            if (settled_here || timed_out) {
                if (!settled_here) {
                    godot::UtilityFunctions::print("TerrainPreview: near vegetation not settled");
                }
                phase_ = Phase::none;
                get_tree()->create_timer(2.0)->connect("timeout", callable_mp(this, &TerrainPreview::take_screenshot));
            }
            return;
        case Phase::headless_settle:
            if (settled_seen_ || timed_out) {
                phase_ = Phase::none;
                print_tiers_line();
                get_tree()->quit(0);
            }
            return;
        case Phase::view_settle:
            if (settled_seen_ || timed_out || !scene_.tiers) {
                print_tiers_line();
                if (scene_.vegetation) {
                    start_forest_flight(0);
                } else {
                    phase_ = Phase::none;
                    start_flight();
                }
            }
            return;
        case Phase::forest_settle:
            if (settled_here || timed_out) {
                phase_ = Phase::forest_fly;
                forest_filled_ = 0;
                forest_dropped_ = 0;
                forest_pending_max_ = 0;
                forest_late0_ = near_ ? near_->counts().late : 0;
                compute_usec_sum_ = 0;
                compute_usec_max_ = 0;
                compute_cells_ = 0;
                attach_usec_max_ = 0;
                forest_recording_ = true;
                forest_t0_ = t;
                frame_usec_.clear();
                last_frame_usec_ = t;
                godot::RenderingServer::get_singleton()->connect(
                    "frame_post_draw", callable_mp(this, &TerrainPreview::bench_flight_frame));
            }
            return;
        case Phase::forest_fly: {
            forest_done_ += forest_speed_ * dt;
            const terrain::FlyState s = forest_start();
            fly_.x = s.x;
            const float along = std::min(forest_done_, forest_length_);
            fly_.z = scene_.forest.north ? s.z - along : s.z + along;
            fly_.y = terrain::ground_clamp(*scene_.layout, scene_.heights, fly_.x, fly_.z) + 60.0f;
            apply_camera();
            if (forest_done_ >= forest_length_) {
                finish_forest_flight();
            }
            return;
        }
    }
}

void TerrainPreview::finish_forest_flight() {
    forest_recording_ = false;
    godot::RenderingServer::get_singleton()->disconnect("frame_post_draw",
                                                        callable_mp(this, &TerrainPreview::bench_flight_frame));
    std::uint64_t sum = 0;
    std::uint64_t max = 0;
    std::uint64_t over11 = 0;
    std::uint64_t over16 = 0;
    std::uint64_t over33 = 0;
    for (const std::uint64_t f : frame_usec_) {
        sum += f;
        max = std::max(max, f);
        over11 += f > 11100 ? 1u : 0u;
        over16 += f > 16700 ? 1u : 0u;
        over33 += f > 33300 ? 1u : 0u;
    }
    const std::uint64_t mean = frame_usec_.empty() ? 0 : sum / frame_usec_.size();
    const double seconds = static_cast<double>(now_usec() - forest_t0_) / 1e6;
    const double rate = seconds > 0.0 ? static_cast<double>(forest_filled_) / seconds : 0.0;
    const std::uint64_t late = near_ ? near_->counts().late - forest_late0_ : 0;
    const std::uint64_t compute_mean = compute_cells_ == 0 ? 0 : compute_usec_sum_ / compute_cells_;
    godot::UtilityFunctions::print(
        "UNMAPPED-TERRAIN-BENCH: forest flight at ", num(static_cast<std::uint64_t>(forest_speed_)), " m/s, column ",
        scene_.forest.x, ", ", num(static_cast<std::uint64_t>(forest_length_)), " m: frames ",
        num(frame_usec_.size()), ", mean ", ms(mean), ", max ", ms(max), ", over 11.1 ms ", num(over11),
        ", over 16.7 ms ", num(over16), ", over 33.3 ms ", num(over33), "; cells filled ", num(forest_filled_), " (",
        to_godot(terrain::one_decimal(rate)), " per s), dropped ", num(forest_dropped_), ", late ", num(late),
        ", most pending ", num(forest_pending_max_), "; compute mean ", num(compute_mean), " us, max ",
        num(compute_usec_max_), " us; attach max ", num(attach_usec_max_), " us; memory static ",
        to_godot(terrain::one_decimal(static_mb())), " MB");
    if (forest_index_ == 0) {
        start_forest_flight(1);
    } else {
        phase_ = Phase::none;
        start_flight();
    }
}

}  // namespace unmapped
