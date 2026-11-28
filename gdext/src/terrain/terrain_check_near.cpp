// The terrain check's V3D-03 part (spec V3D-03 11.2): T17 at once, then the
// camera path over frames with T18 to T24. The check keeps its own copy of
// the partition rule (T by sorting), of 6.1's boxes and distance, of 4.1's
// visibility ranges and of 7.2's buffer formula; it uses place_candidates
// only for the candidates.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include "terrain/fly_state.hpp"
#include "terrain/terrain_check.hpp"
#include "terrain/terrain_check_internal.hpp"
#include "terrain/terrain_preview.hpp"
#include "terrain/vegetation.hpp"

namespace unmapped {

namespace {

godot::String num(std::uint64_t v) { return godot::String::num_int64(static_cast<int64_t>(v)); }
godot::String numd(double v) { return godot::String::num(v, 1); }

using Key = std::pair<std::int64_t, std::int64_t>;  // (X, Z) in 1/128 m

Key key_of(double x, double z) { return {std::llround(x * 128.0), std::llround(z * 128.0)}; }
Key key_of(float x, float z) { return key_of(static_cast<double>(x), static_cast<double>(z)); }

// --- The check's own copies ------------------------------------------------------

struct OwnTier {
    std::uint64_t threshold = 0;
    std::uint64_t s_far = 0;
    std::array<std::vector<terrain::Instance>, 5> far;
};

constexpr std::array<bool, 5> OWN_FAR = {true, true, false, false, true};

OwnTier own_tier(const terrain::VegetationInputs& in, std::uint64_t budget) {
    const std::vector<terrain::Candidate> all = terrain::place_candidates(in, 0, 0, in.layout->n, in.layout->n);
    std::vector<std::uint32_t> thins;
    for (const terrain::Candidate& c : all) {
        if (OWN_FAR[c.kind]) {
            thins.push_back(c.thin);
        }
    }
    std::sort(thins.begin(), thins.end());
    OwnTier t;
    t.s_far = thins.size();
    t.threshold = t.s_far <= budget ? (std::uint64_t{1} << 32) : thins[static_cast<std::size_t>(budget)];
    for (const terrain::Candidate& c : all) {
        if (OWN_FAR[c.kind] && c.thin < t.threshold) {
            t.far[c.kind].push_back(c.instance);
        }
    }
    return t;
}

struct OwnBox {
    double x0 = 0, x1 = 0, z0 = 0, z1 = 0, bot = 0, top = 0;
};

std::vector<OwnBox> own_boxes(int n, const std::vector<float>& h) {
    const int m = n / 16;
    std::vector<OwnBox> out(static_cast<std::size_t>(m) * static_cast<std::size_t>(m));
    for (int j = 0; j < m; ++j) {
        for (int i = 0; i < m; ++i) {
            OwnBox& b = out[static_cast<std::size_t>(j * m + i)];
            b.x0 = 16.0 * (16 * i - n / 2) - 8.0;
            b.x1 = b.x0 + 256.0;
            b.z0 = 16.0 * (16 * j - n / 2) - 8.0;
            b.z1 = b.z0 + 256.0;
            double lo = 1e30;
            double hi = -1e30;
            for (int y = 16 * j - 1; y <= 16 * j + 16; ++y) {
                for (int x = 16 * i - 1; x <= 16 * i + 16; ++x) {
                    const int cx = std::clamp(x, 0, n - 1);
                    const int cy = std::clamp(y, 0, n - 1);
                    const auto v = static_cast<double>(h[static_cast<std::size_t>(cy) * static_cast<std::size_t>(n) + static_cast<std::size_t>(cx)]);
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
            }
            b.bot = lo - 2.0;
            b.top = hi + 22.0;
        }
    }
    return out;
}

double own_d(const OwnBox& b, double px, double py, double pz) {
    const double dx = std::max({0.0, b.x0 - px, px - b.x1});
    const double dz = std::max({0.0, b.z0 - pz, pz - b.z1});
    const double dy = std::max({0.0, b.bot - py, py - b.top});
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::vector<float> own_buffer(const std::vector<terrain::Instance>& list, bool tier_colours) {
    std::vector<float> out;
    out.reserve(16 * list.size());
    for (const terrain::Instance& in : list) {
        const float s = static_cast<float>(in.scale_pm) / 1000.0f;
        const float a = static_cast<float>(in.yaw1024) * (6.28318530717958647692f / 1024.0f);
        const float v = static_cast<float>(200 + static_cast<int>(in.tint) * 55 / 255) / 255.0f;
        const float gb = tier_colours ? 0.45f * v : v;
        const float row[16] = {s * std::cos(a), 0.0f, s * std::sin(a), in.x, 0.0f, s, 0.0f, in.y,
                               -(s * std::sin(a)), 0.0f, s * std::cos(a), in.z, v, gb, gb, 1.0f};
        out.insert(out.end(), row, row + 16);
    }
    return out;
}

struct OwnRange {
    float begin, end, margin;
    bool fade;
};

OwnRange own_range(int kind, int lod) {
    if (kind <= 1) {
        return lod == 0 ? OwnRange{0.0f, 600.0f, 0.0f, false} : OwnRange{600.0f, 1200.0f, 100.0f, true};
    }
    if (kind == 2) {
        return {0.0f, 1000.0f, 0.0f, false};
    }
    if (kind == 3) {
        return {0.0f, 800.0f, 0.0f, false};
    }
    return {0.0f, 1200.0f, 100.0f, true};
}

constexpr int OWN_LODS[5] = {2, 2, 1, 1, 1};

// The far tier as Terrain3D stores it, by near cell and kind.
std::map<int, std::array<std::vector<Key>, 5>> far_index(const TerrainPreview& preview, int n, std::array<std::uint64_t, 5>& totals) {
    std::map<int, std::array<std::vector<Key>, 5>> out;
    totals = {};
    if (preview.terrain_node() == nullptr) {
        return out;
    }
    godot::Object* data = preview.terrain_node()->call("get_data");
    if (data == nullptr) {
        return out;
    }
    const int m = n / 16;
    const godot::TypedArray<godot::Vector2i> locations = data->call("get_region_locations");
    for (int64_t r = 0; r < locations.size(); ++r) {
        const godot::Vector2i loc = locations[r];
        godot::Object* region = data->call("get_region", loc);
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
                if (triple.size() < 1) {
                    continue;
                }
                const godot::Array xforms = triple[0];
                for (int64_t t = 0; t < xforms.size(); ++t) {
                    const godot::Transform3D xf = xforms[t];
                    const double X = static_cast<double>(xf.origin.x) + 4096.0 * loc.x;
                    const double Z = static_cast<double>(xf.origin.z) + 4096.0 * loc.y;
                    const int cx = std::clamp(static_cast<int>(std::floor((X + 8.0 * n + 8.0) / 16.0)), 0, n - 1);
                    const int cz = std::clamp(static_cast<int>(std::floor((Z + 8.0 * n + 8.0) / 16.0)), 0, n - 1);
                    out[(cz / 16) * m + cx / 16][static_cast<std::size_t>(id)].push_back(key_of(X, Z));
                    ++totals[static_cast<std::size_t>(id)];
                }
            }
        }
    }
    for (auto& [q, kinds] : out) {
        for (auto& list : kinds) {
            std::sort(list.begin(), list.end());
        }
    }
    return out;
}

bool same_instance(const terrain::Instance& a, const terrain::Instance& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.yaw1024 == b.yaw1024 && a.scale_pm == b.scale_pm && a.tint == b.tint;
}

// --- The path --------------------------------------------------------------------

class NearPath final : public TerrainCheckRun {
public:
    enum class Mode { tiers, radius0, whole };

    NearPath(TerrainPreview& preview, const TerrainCheckOptions& options, TerrainChecker c, Mode mode, OwnTier tier)
        : c_(std::move(c)), options_(options), mode_(mode), tier_(std::move(tier)) {
        const TerrainScene& scene = preview.scene();
        n_ = scene.layout->n;
        const double N = n_;
        auto P = [N](double cx, double cy) { return std::pair<double, double>{16.0 * (cx - N / 2), 16.0 * (cy - N / 2)}; };
        points_ = {P(N / 2, N / 2), P(N / 2, N / 4), P(3 * N / 4, N / 4), P(N / 16, 15 * N / 16), P(N / 2, N / 2)};
        boxes_ = own_boxes(n_, scene.heights);
        tint_ = preview.tier_colours_option();
        threads_ = preview.veg_threads();
        far_ = far_index(preview, n_, far_totals_);
        begin_leg(preview, 0);
    }

    void before_frame(TerrainPreview& preview) override {
        terrain::FlyState s = preview.start_state();
        double x = cur_.first;
        const double z = cur_.second;
        if (stage_ == Stage::wiggle) {
            x += (frames_ % 2 == 0) ? 128.0 : -128.0;
        }
        s.x = static_cast<float>(x);
        s.z = static_cast<float>(z);
        s.y = terrain::ground_clamp(*preview.scene().layout, preview.scene().heights, s.x, s.z) + 60.0f;
        s.heading_deg = static_cast<float>(heading_);
        s.pitch_deg = -10.0f;
        preview.set_fly_state(s);
    }

    bool after_frame(TerrainPreview& preview) override {
        ++total_frames_;
        frame_checks(preview);
        ++frames_;
        switch (stage_) {
            case Stage::move: {
                const double dx = target_.first - cur_.first;
                const double dz = target_.second - cur_.second;
                const double len = std::sqrt(dx * dx + dz * dz);
                if (len <= 64.0) {
                    cur_ = target_;
                    stage_ = Stage::settle;
                    frames_ = 0;
                } else {
                    cur_.first += 64.0 * dx / len;
                    cur_.second += 64.0 * dz / len;
                }
                break;
            }
            case Stage::settle: {
                if (leg_ == 3 && frames_ == 1 && mode_ != Mode::whole) {
                    reopen(preview);
                    break;
                }
                const terrain::NearCells* near = preview.near_cells();
                const bool settled = near == nullptr || near->settled();
                if (settled || frames_ > 900) {
                    point_checks(preview, settled);
                    if (leg_ == 1) {
                        stage_ = Stage::wiggle;
                        frames_ = 0;
                        wiggle_filled_ = wiggle_cells(preview);
                        wiggle_ok_ = true;
                    } else {
                        return next_leg(preview);
                    }
                }
                break;
            }
            case Stage::wiggle: {
                const terrain::NearCells* near = preview.near_cells();
                if (near != nullptr) {
                    for (const int q : wiggle_filled_) {
                        wiggle_ok_ = wiggle_ok_ && near->state(q) == terrain::NearState::filled;
                    }
                    for (const int q : preview.near_stats().dropped) {
                        wiggle_ok_ = wiggle_ok_ && wiggle_filled_.count(q) == 0;
                    }
                }
                if (frames_ >= 60) {
                    c_.check(wiggle_ok_, "T20 at P1 a +-128 m wiggle for 60 frames changes no filled cell (" +
                                             num(wiggle_filled_.size()) + " cells)");
                    return next_leg(preview);
                }
                break;
            }
            case Stage::done:
                return false;
        }
        if (total_frames_ > 8000) {
            c_.check(false, "the path ended within 8000 frames");
            return finish(preview);
        }
        return true;
    }

    [[nodiscard]] int exit_code() const override { return c_.all ? 0 : 1; }

private:
    enum class Stage { move, settle, wiggle, done };

    void begin_leg(TerrainPreview& preview, int leg) {
        (void)preview;
        leg_ = leg;
        frames_ = 0;
        target_ = points_[static_cast<std::size_t>(leg)];
        const auto& toward = points_[static_cast<std::size_t>(leg == 3 ? 4 : (leg == 4 ? 1 : std::min(leg + 1, 4)))];
        if (leg == 1 || leg == 2) {
            stage_ = Stage::move;
            heading_ = heading_to(cur_, target_);
        } else {
            // Jumps: P0, P3, back to P0, heading toward the next point.
            cur_ = target_;
            stage_ = Stage::settle;
            heading_ = heading_to(cur_, toward);
        }
    }

    static double heading_to(const std::pair<double, double>& from, const std::pair<double, double>& to) {
        const double h = std::atan2(to.first - from.first, -(to.second - from.second)) * 180.0 / 3.14159265358979323846;
        return h < 0.0 ? h + 360.0 : h;
    }

    bool next_leg(TerrainPreview& preview) {
        if (leg_ + 1 < static_cast<int>(points_.size())) {
            begin_leg(preview, leg_ + 1);
            return true;
        }
        return finish(preview);
    }

    void reopen(TerrainPreview& preview) {
        // A reopen while cells are computing (the "Open world..." path):
        // every task is waited for and its result dropped.
        const terrain::NearCells* near = preview.near_cells();
        const std::uint64_t computing = near != nullptr ? near->counts().computing : 0;
        const bool ok = preview.open_world(options_.world_path);
        c_.check(ok && (mode_ != Mode::tiers || preview.near_cells() != nullptr) &&
                     (preview.near_cells() == nullptr || preview.near_cells()->active().empty()),
                 "T20 reopening the world with " + num(computing) + " cells computing gives a fresh near tier");
        far_ = far_index(preview, n_, far_totals_);
        attached_.clear();
        frames_ = 1;  // settle from here
        reopened_ = true;
    }

    // The filled cells that stay within R + H at both ends of the wiggle (the
    // camera 60 m above the ground at each), so the wiggle alone must not
    // move them; a cell filled earlier that the wiggle carries past R + H
    // may drop by the rules of 6.2.
    [[nodiscard]] std::set<int> wiggle_cells(const TerrainPreview& preview) const {
        std::set<int> out;
        const TerrainScene& scene = preview.scene();
        const double RH = preview.veg_radius() + 512.0;
        for (const int q : filled_cells(preview)) {
            bool keep = true;
            for (const double off : {128.0, -128.0}) {
                const auto x = static_cast<float>(cur_.first + off);
                const auto z = static_cast<float>(cur_.second);
                const double y = static_cast<double>(terrain::ground_clamp(*scene.layout, scene.heights, x, z)) + 60.0;
                keep = keep && own_d(boxes_[static_cast<std::size_t>(q)], static_cast<double>(x), y, static_cast<double>(z)) <= RH;
            }
            if (keep) {
                out.insert(q);
            }
        }
        return out;
    }

    [[nodiscard]] std::set<int> filled_cells(const TerrainPreview& preview) const {
        std::set<int> out;
        if (const terrain::NearCells* near = preview.near_cells()) {
            for (const int q : near->active()) {
                if (near->state(q) == terrain::NearState::filled) {
                    out.insert(q);
                }
            }
        }
        return out;
    }

    void frame_checks(TerrainPreview& preview) {
        const NearFrameStats& st = preview.near_stats();
        const std::uint64_t limit = threads_ > 0 ? static_cast<std::uint64_t>(threads_) : 2u;
        max_attached_ = std::max<std::uint64_t>(max_attached_, st.attached.size());
        max_dropped_ = std::max<std::uint64_t>(max_dropped_, st.dropped.size());
        max_in_flight_ = std::max(max_in_flight_, st.in_flight);
        budget_ok_ = budget_ok_ && st.attached.size() <= 4 && st.dropped.size() <= 16 && st.in_flight <= limit;
        std::uint64_t instances = 0;
        for (const int q : st.dropped) {
            attached_.erase(q);
        }
        for (const int q : st.attached) {
            twice_ok_ = twice_ok_ && attached_.insert(q).second;
            if (const terrain::NearCells* near = preview.near_cells()) {
                instances += near->instances(q);
            }
        }
        max_attach_instances_ = std::max(max_attach_instances_, instances);
        budget_ok_ = budget_ok_ && (st.attached.size() <= 1 || instances <= 16384);
        if (mode_ == Mode::radius0) {
            radius0_ok_ = radius0_ok_ && preview.near_root() != nullptr && preview.near_root()->get_child_count() == 0 &&
                          preview.near_cells() == nullptr;
        }
        if (mode_ == Mode::whole) {
            whole_ok_ = whole_ok_ && preview.near_root() == nullptr &&
                        preview.get_node_or_null(godot::NodePath("NearVegetation")) == nullptr;
        }
    }

    // T18, T19 (and T21's record) at a point once settled.
    void point_checks(TerrainPreview& preview, bool settled) {
        static const char* names[] = {"P0", "P1", "P2", "P3", "P0 again"};
        const godot::String where = names[leg_];
        if (mode_ != Mode::tiers) {
            TerrainChecker::info("at " + where + ": no near tier in this mode");
            return;
        }
        c_.check(settled, "T18 at " + where + " settled within 900 frames (" + num(static_cast<std::uint64_t>(frames_)) + ")");
        const terrain::NearCells* near = preview.near_cells();
        if (near == nullptr) {
            c_.check(false, "T18 the near tier exists");
            return;
        }
        const terrain::FlyState& f = preview.fly_state();
        const auto px = static_cast<double>(f.x);
        const auto py = static_cast<double>(f.y);
        const auto pz = static_cast<double>(f.z);
        const double R = preview.veg_radius();
        const double RH = R + 512.0;
        const std::set<int> filled = filled_cells(preview);
        std::size_t missing = 0;
        std::size_t too_far = 0;
        std::size_t want = 0;
        for (std::size_t q = 0; q < boxes_.size(); ++q) {
            const double d = own_d(boxes_[q], px, py, pz);
            const bool in = filled.count(static_cast<int>(q)) > 0;
            if (d <= R) {
                ++want;
                missing += in ? 0u : 1u;
            }
            too_far += (in && d > RH) ? 1u : 0u;
        }
        c_.check(missing == 0 && too_far == 0,
                 "T18 at " + where + ": filled " + num(filled.size()) + " cells hold every cell with d <= R (" + num(want) +
                     ", " + num(missing) + " missing) and none beyond R + H (" + num(too_far) + ")");

        // Each filled cell's nodes against the check's own Extra.
        godot::Node3D* root = preview.near_root();
        const terrain::VegetationInputs& in = preview.vegetation_inputs();
        const int m = n_ / 16;
        std::size_t nodes_expected = 0;
        std::size_t bad_count = 0;
        std::size_t bad_buffer = 0;
        std::size_t bad_aabb = 0;
        std::size_t bad_range = 0;
        std::size_t bad_missing = 0;
        std::size_t bad_xform = 0;
        std::size_t xform_unread = 0;
        std::size_t bad_partition = 0;
        std::size_t bad_overlap = 0;
        std::uint64_t instances = 0;
        const bool record = leg_ == 0;
        const bool compare = leg_ == 4;
        std::size_t t21_cells = 0;
        std::size_t t21_differ = 0;
        for (const int q : filled) {
            const int i = q % m;
            const int j = q / m;
            const std::vector<terrain::Candidate> cands =
                terrain::place_candidates(in, 16 * i, 16 * j, 16 * i + 16, 16 * j + 16);
            std::array<std::vector<terrain::Instance>, 5> extra;
            std::array<std::vector<Key>, 5> all_keys;
            for (const terrain::Candidate& cd : cands) {
                const bool far = OWN_FAR[cd.kind] && cd.thin < tier_.threshold;
                if (!far) {
                    extra[cd.kind].push_back(cd.instance);
                }
                all_keys[cd.kind].push_back(key_of(cd.instance.x, cd.instance.z));
            }
            const OwnBox& b = boxes_[static_cast<std::size_t>(q)];
            const bool d_le_r = own_d(b, px, py, pz) <= R;
            for (int kind = 0; kind < 5; ++kind) {
                const auto& list = extra[static_cast<std::size_t>(kind)];
                std::vector<Key> near_keys;
                if (!list.empty()) {
                    const std::vector<float> want_buf = own_buffer(list, tint_);
                    instances += list.size();
                    for (int lod = 0; lod < OWN_LODS[kind]; ++lod) {
                        ++nodes_expected;
                        const godot::String name = "n" + godot::String::num_int64(q) + "k" + godot::String::num_int64(kind) +
                                                   "l" + godot::String::num_int64(lod);
                        auto* node = root != nullptr ? godot::Object::cast_to<godot::MultiMeshInstance3D>(
                                                           root->get_node_or_null(godot::NodePath(name)))
                                                     : nullptr;
                        if (node == nullptr || node->get_multimesh().is_null()) {
                            ++bad_missing;
                            continue;
                        }
                        const godot::Ref<godot::MultiMesh> mm = node->get_multimesh();
                        bad_count += static_cast<std::size_t>(mm->get_instance_count()) == list.size() ? 0u : 1u;
                        const godot::PackedFloat32Array got = mm->get_buffer();
                        const bool buf_ok = static_cast<std::size_t>(got.size()) == want_buf.size() &&
                                            std::memcmp(got.ptr(), want_buf.data(), want_buf.size() * sizeof(float)) == 0;
                        bad_buffer += buf_ok ? 0u : 1u;
                        if (lod == 0 && got.size() >= 16) {
                            for (int64_t k = 0; k + 15 < got.size(); k += 16) {
                                near_keys.push_back(key_of(got[k + 3], got[k + 11]));
                            }
                        }
                        // W16: the first instance's transform as Godot reads it.
                        const godot::Transform3D t0 = mm->get_instance_transform(0);
                        const float* w = want_buf.data();
                        bool xf_ok = std::abs(t0.basis.rows[0][0] - w[0]) < 1e-6f && std::abs(t0.basis.rows[0][2] - w[2]) < 1e-6f &&
                                           std::abs(t0.basis.rows[2][0] - w[8]) < 1e-6f && t0.basis.rows[1][1] == w[5] &&
                                           t0.origin.x == w[3] && t0.origin.y == w[7] && t0.origin.z == w[11];
                        if (t0 == godot::Transform3D()) {
                            ++xform_unread;  // the headless renderer keeps no transforms
                            xf_ok = true;
                        }
                        bad_xform += xf_ok ? 0u : 1u;
                        const godot::AABB a = node->get_custom_aabb();
                        const bool aabb_ok = a.position.x == static_cast<float>(b.x0) && a.position.y == static_cast<float>(b.bot) &&
                                             a.position.z == static_cast<float>(b.z0) && a.size.x == 256.0f &&
                                             a.size.z == 256.0f && a.size.y == static_cast<float>(b.top - b.bot);
                        bad_aabb += aabb_ok ? 0u : 1u;
                        const OwnRange r = own_range(kind, lod);
                        const bool range_ok =
                            node->get_visibility_range_begin() == r.begin && node->get_visibility_range_end() == r.end &&
                            node->get_visibility_range_end_margin() == r.margin &&
                            node->get_visibility_range_begin_margin() == 0.0f &&
                            node->get_visibility_range_fade_mode() ==
                                (r.fade ? godot::GeometryInstance3D::VISIBILITY_RANGE_FADE_SELF
                                        : godot::GeometryInstance3D::VISIBILITY_RANGE_FADE_DISABLED) &&
                            node->get_cast_shadows_setting() == godot::GeometryInstance3D::SHADOW_CASTING_SETTING_OFF &&
                            node->get_layer_mask() == 1u;
                        bad_range += range_ok ? 0u : 1u;
                        if (d_le_r && (record || compare)) {
                            const godot::String id = name;
                            if (record) {
                                first_visit_[id] = got;
                            } else {
                                const auto it = first_visit_.find(id);
                                ++t21_cells;
                                const bool same = it != first_visit_.end() && it->second.size() == got.size() &&
                                                  std::memcmp(it->second.ptr(), got.ptr(),
                                                              static_cast<std::size_t>(got.size()) * sizeof(float)) == 0;
                                t21_differ += same ? 0u : 1u;
                            }
                        }
                    }
                }
                // T19: near and far positions in the box, disjoint and
                // together the cell's candidates.
                std::sort(near_keys.begin(), near_keys.end());
                std::vector<Key> far_keys;
                const auto fit = far_.find(q);
                if (fit != far_.end()) {
                    far_keys = fit->second[static_cast<std::size_t>(kind)];
                }
                std::vector<Key> both;
                std::set_intersection(near_keys.begin(), near_keys.end(), far_keys.begin(), far_keys.end(),
                                      std::back_inserter(both));
                bad_overlap += both.size();
                std::vector<Key> together;
                std::merge(near_keys.begin(), near_keys.end(), far_keys.begin(), far_keys.end(), std::back_inserter(together));
                std::vector<Key> want_keys = all_keys[static_cast<std::size_t>(kind)];
                std::sort(want_keys.begin(), want_keys.end());
                bad_partition += together == want_keys ? 0u : 1u;
            }
        }
        const int64_t children = root != nullptr ? root->get_child_count() : -1;
        c_.check(bad_missing == 0 && children == static_cast<int64_t>(nodes_expected),
                 "T18 at " + where + ": one node per filled cell, kind with instances and LOD (" + num(nodes_expected) +
                     " expected, " + num(bad_missing) + " missing, " + godot::String::num_int64(children) +
                     " children of NearVegetation)");
        c_.check(bad_count == 0 && bad_buffer == 0,
                 "T18 at " + where + ": instance_count and buffer equal the check's own Extra and 7.2 (" + num(instances) +
                     " instances; " + num(bad_count) + " counts, " + num(bad_buffer) + " buffers differ)");
        if (nodes_expected == 0) {
            TerrainChecker::info("T18 at " + where + ": no near node here");
        } else if (xform_unread == nodes_expected) {
            TerrainChecker::info("T18 at " + where + ": get_instance_transform gives the identity here (headless "
                                 "renderer); W16 is checked only with a display");
        } else {
            c_.check(bad_xform == 0, "T18 at " + where + ": the first instance's transform reads back as 7.2 (W16; " +
                                         num(bad_xform) + " differ, " + num(xform_unread) + " unread)");
        }
        c_.check(bad_aabb == 0 && bad_range == 0, "T18 at " + where + ": custom_aabb the box and the ranges of 4.1 (" +
                                                      num(bad_aabb) + " boxes, " + num(bad_range) + " ranges differ)");
        c_.check(bad_partition == 0 && bad_overlap == 0,
                 "T19 at " + where + ": near and Terrain3D's far positions disjoint and together the cell's candidates (" +
                     num(bad_overlap) + " in both, " + num(bad_partition) + " cell kinds differ)");
        if (compare) {
            c_.check(t21_cells > 0 && t21_differ == 0, "T21 back at P0: " + num(t21_cells) +
                                                           " multimeshes of cells with d <= R byte-identical to the first visit (" +
                                                           num(t21_differ) + " differ)" +
                                                           (reopened_ ? godot::String(", after a reopen") : godot::String()));
        }
        if (threads_ == 0) {
            c_.check(bad_buffer == 0 && bad_count == 0,
                     "T23 --veg-threads 0 at " + where + ": the same buffers as the threaded run (the check's own)");
        }
    }

    bool finish(TerrainPreview& preview) {
        stage_ = Stage::done;
        c_.check(budget_ok_ && twice_ok_,
                 "T20 every frame: at most 4 attached (" + num(max_attached_) + ") and 16,384 instances (" +
                     num(max_attach_instances_) + "), 16 dropped (" + num(max_dropped_) + "), " +
                     num(threads_ > 0 ? static_cast<std::uint64_t>(threads_) : 2u) + " in flight (" + num(max_in_flight_) +
                     "); no cell attached twice without a drop" + (twice_ok_ ? godot::String() : godot::String(" (FAILED)")));
        if (mode_ == Mode::radius0) {
            c_.check(radius0_ok_, "T24 --veg-radius 0: NearVegetation has no child over the path");
        }
        if (mode_ == Mode::whole) {
            c_.check(whole_ok_, "T24 --veg-whole-map: no NearVegetation node over the path");
        }
        // T22: HUD line and key T.
        const TerrainScene& scene = preview.scene();
        std::string want;
        if (mode_ == Mode::whole) {
            want = "vegetation whole map " + std::to_string(scene.placement.kept());
        } else {
            const terrain::NearCounts k = preview.near_cells() != nullptr ? preview.near_cells()->counts() : terrain::NearCounts{};
            want = "vegetation near " + std::to_string(k.filled) + " cells, " +
                   std::to_string(k.queued + k.computing + k.ready) + " pending, " + std::to_string(k.instances) +
                   " instances; far " + std::to_string(scene.placement.kept());
        }
        const std::string got = preview.vegetation_hud_line();
        c_.check(got == want, godot::String("T22 HUD line '") + got.c_str() + "'" +
                                  (got == want ? godot::String() : godot::String("\n          expected '") + want.c_str() + "'"));
        auto assets_enabled = [&preview](bool value) {
            godot::Object* assets = preview.terrain_node() != nullptr ? static_cast<godot::Object*>(preview.terrain_node()->call("get_assets")) : nullptr;
            if (assets == nullptr) {
                return false;
            }
            const int64_t count = assets->call("get_mesh_count");
            bool all = count == 5;
            for (int64_t id = 0; id < count; ++id) {
                godot::Object* a = assets->call("get_mesh_asset", id);
                all = all && a != nullptr && static_cast<bool>(a->get("enabled")) == value;
            }
            return all;
        };
        auto near_visible = [&preview, this](bool value) {
            return mode_ == Mode::whole || (preview.near_root() != nullptr && preview.near_root()->is_visible() == value);
        };
        const bool before = assets_enabled(true) && near_visible(true);
        const bool off = preview.toggle_trees() && assets_enabled(false) && near_visible(false) &&
                         preview.vegetation_hud_line() == "vegetation off";
        const bool on = preview.toggle_trees() && assets_enabled(true) && near_visible(true);
        c_.check(before && off && on, godot::String(mode_ == Mode::whole ? "T22 key T " : "T22 key T hides ") +
                                          (mode_ == Mode::whole ? "" : "NearVegetation and ") +
                                          "disables the mesh assets, and T again restores both");
        godot::UtilityFunctions::print(c_.all ? "UNMAPPED-TERRAIN-CHECK: PASS" : "UNMAPPED-TERRAIN-CHECK: FAIL");
        return false;
    }

    TerrainChecker c_;
    TerrainCheckOptions options_;
    Mode mode_;
    OwnTier tier_;
    int n_ = 0;
    bool tint_ = false;
    int threads_ = 4;
    std::vector<std::pair<double, double>> points_;
    std::vector<OwnBox> boxes_;
    std::map<int, std::array<std::vector<Key>, 5>> far_;
    std::array<std::uint64_t, 5> far_totals_{};
    int leg_ = 0;
    Stage stage_ = Stage::settle;
    int frames_ = 0;
    int total_frames_ = 0;
    std::pair<double, double> cur_{0.0, 0.0};
    std::pair<double, double> target_{0.0, 0.0};
    double heading_ = 0.0;
    std::set<int> attached_;
    std::set<int> wiggle_filled_;
    bool wiggle_ok_ = true;
    bool budget_ok_ = true;
    bool twice_ok_ = true;
    bool radius0_ok_ = true;
    bool whole_ok_ = true;
    bool reopened_ = false;
    std::uint64_t max_attached_ = 0;
    std::uint64_t max_dropped_ = 0;
    std::uint64_t max_in_flight_ = 0;
    std::uint64_t max_attach_instances_ = 0;
    std::map<godot::String, godot::PackedFloat32Array> first_visit_;
};

}  // namespace

std::unique_ptr<TerrainCheckRun> start_near_check(TerrainPreview& preview, const TerrainCheckOptions& options,
                                                  TerrainChecker& c) {
    const TerrainScene& scene = preview.scene();
    if (options.expect_terrain3d != "present" || !scene.vegetation || preview.terrain_node() == nullptr || !scene.layout) {
        return nullptr;
    }
    const int n = scene.layout->n;
    NearPath::Mode mode = NearPath::Mode::tiers;
    if (preview.whole_map_option()) {
        mode = NearPath::Mode::whole;
    } else if (preview.veg_radius() <= 0.0) {
        mode = NearPath::Mode::radius0;
    }
    OwnTier tier;
    if (mode != NearPath::Mode::whole) {
        // T17: the far tier against the check's own partition rule, and
        // what Terrain3D stores.
        const std::uint64_t budget = preview.instance_cap();
        tier = own_tier(preview.vegetation_inputs(), budget);
        c.check(scene.tiers && scene.far_threshold == tier.threshold && scene.far_candidates == tier.s_far &&
                    scene.placement.cap == budget,
                "T17 far tier: T " + num(tier.threshold) + ", S_far " + num(tier.s_far) + ", budget " + num(budget));
        std::size_t differ = 0;
        godot::String counts;
        for (std::size_t k = 0; k < 5; ++k) {
            const auto& got = scene.placement.kinds[k];
            const auto& want = tier.far[k];
            if (got.size() != want.size()) {
                differ += std::max(got.size(), want.size());
            } else {
                for (std::size_t p = 0; p < got.size(); ++p) {
                    differ += same_instance(got[p], want[p]) ? 0u : 1u;
                }
            }
            counts += (k > 0 ? ", " : "") + num(got.size());
        }
        const std::uint64_t far_count = scene.placement.kept();
        c.check(differ == 0 && far_count <= budget && scene.placement.kinds[2].empty() && scene.placement.kinds[3].empty(),
                "T17 the far instances equal the check's own {thin < T} of the far kinds, in order (" + counts + "; " +
                    num(differ) + " differ), at most the budget, no shrub or reed");
        std::array<std::uint64_t, 5> totals{};
        (void)far_index(preview, n, totals);
        bool stored = true;
        godot::String text;
        for (std::size_t k = 0; k < 5; ++k) {
            stored = stored && totals[k] == tier.far[k].size();
            text += (k > 0 ? ", " : "") + num(totals[k]);
        }
        c.check(stored, "T17 Terrain3D stores the far count per kind (" + text + ")");
    }
    TerrainChecker::info(godot::String("V3D-03 path: ") +
                         (mode == NearPath::Mode::tiers ? "tiers" : mode == NearPath::Mode::whole ? "whole map" : "radius 0") +
                         ", radius " + numd(preview.veg_radius()) + " m, threads " +
                         godot::String::num_int64(preview.veg_threads()));
    return std::make_unique<NearPath>(preview, options, c, mode, std::move(tier));
}

}  // namespace unmapped
