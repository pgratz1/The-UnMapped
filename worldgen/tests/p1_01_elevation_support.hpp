#pragma once

// Support for the P1-01 elevation tests: the tuning texts of spec section
// 4.4, the values the tests need from them, and code written independently
// of the stage from the spec text (flood levels by a Dijkstra-style minimax
// search, components by union-find, the chamfer by Dijkstra, the floor
// search, the E2 cell function, the E10.0 cost and bump), grid hashes and
// the golden-file reader.
//
// Written from docs/specs/p1-01-elevation.md, docs/specs/p1-00-geography-
// overview.md and the P0 specs only. Nothing here calls the stage; the
// stage is reached only through p1_01_elevation_api.hpp.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <ostream>
#include <queue>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/decimal.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/math.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/geo/noise.hpp"

namespace elev_test {

using worldgen::Fx;
using worldgen::geo::Grid;

inline constexpr std::int64_t ONE = std::int64_t{1} << 32;
inline constexpr std::int64_t HALF = std::int64_t{1} << 31;
inline constexpr std::int64_t SQRT2_RAW = 6074001000;
inline constexpr std::int64_t FINE = std::int64_t{1} << 16;
inline constexpr std::int64_t KEY_MAX = 29990;
inline constexpr std::int64_t H_MAX_RAW = std::int64_t{2990} << 32;

// Threads for tests that are not about threads (any count gives the same
// result, P1-00 D10).
inline constexpr std::uint32_t THREADS = 4;

[[nodiscard]] inline Fx R(std::int64_t raw) { return Fx::from_raw(raw); }

// Raw value of a decimal text (P0-01 5.6). Fails the test on bad text.
[[nodiscard]] inline std::int64_t dec(std::string_view text) {
    const worldgen::DecimalParse p = worldgen::parse_decimal(text);
    REQUIRE_MESSAGE(p.ok(), "bad decimal in test: " << std::string(text));
    return p.value.raw();
}

// --- Tuning texts (P1-01 4.4) ---------------------------------------------------------

// One line of [worldgen.elevation]: the field name and its TOML value text.
struct FieldText {
    std::string_view name;
    std::string_view value;
};

// The shipped file of 4.4, in declaration order (identical to the `geo`
// fixture).
inline const std::vector<FieldText>& shipped_fields() {
    static const std::vector<FieldText> f = {
        {"land_fraction", "\"0.40\""},
        {"edge_margin_frac", "\"0.03\""},
        {"continent_warp", "{ cycles = \"1.5\", octaves = 3, gain = \"0.5\" }"},
        {"continent_warp_amp_frac", "\"0.2\""},
        {"continent_noise", "{ cycles = \"3\", octaves = 4, gain = \"0.55\" }"},
        {"continent_noise_amp", "\"1.0\""},
        {"continent_center_jitter", "\"0.06\""},
        {"edge_falloff_frac", "\"0.12\""},
        {"edge_falloff_depth", "2"},
        {"edge_noise_amp", "\"0.15\""},
        {"edge_radial_start", "\"0.8\""},
        {"edge_radial_end", "\"1.0\""},
        {"coast_noise", "{ wavelength_m = 2000, octaves = 5, gain = \"0.5\" }"},
        {"coast_noise_amp", "\"0.1\""},
        {"island_min_area_m2", "50000"},
        {"belt_noise", "{ cycles = \"1.25\", octaves = 2, gain = \"0.5\" }"},
        {"belt_halfwidth_m", "2000"},
        {"belt_halfwidth_frac", "\"0.15\""},
        {"segment_noise", "{ cycles = \"2.5\", octaves = 2, gain = \"0.5\" }"},
        {"segment_contrast", "\"0.75\""},
        {"segment_bias", "\"0.25\""},
        {"segment_on_share", "\"0.08\""},
        {"segment_floor_mask", "\"0.25\""},
        {"belt_offset_steps", "4"},
        {"floor_inland_gm", "\"0.9\""},
        {"mountain_coast_t", "2"},
        {"relief_warp", "{ wavelength_m = 3000, octaves = 3, gain = \"0.5\" }"},
        {"relief_warp_amp_m", "150"},
        {"ridge_noise", "{ wavelength_m = 2000, octaves = 6, gain = \"0.5\" }"},
        {"ridge_weight", "2"},
        {"foothill_fraction", "\"0.15\""},
        {"mountain_height_m", "1900"},
        {"mountain_peak_rank", "\"0.998\""},
        {"mountain_cover_height_m", "700"},
        {"mountain_cover_rank", "\"0.85\""},
        {"lowland_height_m", "160"},
        {"basin_noise", "{ wavelength_m = 12000, octaves = 3, gain = \"0.5\" }"},
        {"basin_amp_m", "12"},
        {"hill_noise", "{ wavelength_m = 1500, octaves = 6, gain = \"0.5\" }"},
        {"roughness_noise", "{ wavelength_m = 8000, octaves = 2, gain = \"0.5\" }"},
        {"roughness_min", "\"0.25\""},
        {"hill_height_m", "90"},
        {"hill_coast_min", "\"0.6\""},
        {"detail_noise", "{ wavelength_m = 250, octaves = 3, gain = \"0.5\" }"},
        {"detail_amp_m", "6"},
        {"ocean_depth_m", "300"},
        {"ocean_shelf_m", "2000"},
        {"erosion_iterations", "24"},
        {"erosion_route_interval", "1"},
        {"erosion_k", "\"0.008\""},
        {"erosion_diffusion", "\"0.0333\""},
        {"talus_slope", "3"},
        {"talus_rate", "\"0.0625\""},
        {"land_floor_m", "\"0.1\""},
        {"lake_min_area_m2", "100000"},
        {"lake_max_area_m2", "3000000"},
        {"lake_area_ref_n", "1024"},
        {"lake_min_depth_m", "2"},
        {"lake_max_share", "\"0.03\""},
        {"fill_slope_dm", "1"},
        {"fill_noise", "{ wavelength_m = 400, octaves = 3, gain = \"0.5\" }"},
        {"fill_cost_amp", "3"},
        {"fill_detail_m", "3"},
        // Fourth version (generator 0.6.0): passes and the final talus.
        {"pass_count_max", "3"},
        {"pass_core_mask", "\"0.25\""},
        {"pass_crest_frac", "\"0.3\""},
        {"pass_min_length_m", "3000"},
        {"pass_spacing_m", "3000"},
        {"pass_size_ref_n", "1024"},
        {"pass_region_mask", "\"0.03\""},
        {"pass_body_m", "300"},
        {"pass_basin_depth_m", "40"},
        {"pass_tarn_min_cells", "300"},
        {"pass_divide_reach_m", "1000"},
        {"pass_saddle_window_m", "400"},
        {"pass_saddle_rise_m", "150"},
        {"pass_saddle_rise_reach_m", "1500"},
        {"pass_min_spacing_m", "2500"},
        {"pass_coast_clear_m", "700"},
        {"pass_barrier_m", "600"},
        {"pass_side_max_m", "3000"},
        {"pass_col_tarn_m", "160"},
        {"pass_grade", "\"0.9\""},
        {"pass_grade_jitter", "\"0.3\""},
        {"pass_min_col_m", "150"},
        {"pass_saddle_max_m", "600"},
        {"pass_max_cut_m", "500"},
        {"pass_peak_rise_m", "150"},
        {"pass_halfwidth_m", "450"},
        {"pass_width_jitter", "\"0.3\""},
        {"pass_width_var", "\"0.35\""},
        {"pass_walk_grade", "\"0.55\""},
        {"pass_texture_keep", "\"0.25\""},
        {"pass_drain_step_m", "\"0.1\""},
        {"final_talus_iterations", "100"},
        {"final_talus_slope", "5"},
        {"final_talus_rate", "\"0.0625\""},
    };
    return f;
}

using Overrides = std::vector<std::pair<std::string, std::string>>;

// The fourteen lines of the small-map fixture (4.4): the third version's
// five and the fourth version's nine pass radii.
inline const Overrides& small_overrides() {
    static const Overrides o = {
        {"segment_on_share", "\"0.12\""},
        {"lake_min_area_m2", "10240"},
        {"lake_max_area_m2", "160000"},
        {"lake_area_ref_n", "128"},
        {"lake_min_depth_m", "1"},
        {"pass_tarn_min_cells", "20"},
        {"pass_divide_reach_m", "400"},
        {"pass_saddle_window_m", "200"},
        {"pass_saddle_rise_m", "75"},
        {"pass_saddle_rise_reach_m", "600"},
        {"pass_min_spacing_m", "800"},
        {"pass_col_tarn_m", "64"},
        {"pass_halfwidth_m", "200"},
        {"pass_spacing_m", "1000"},
    };
    return o;
}

// The neutral settings of 10.4.2 (invariant 11): no pass site and no final
// talus step; with them every grid of the third version (0.5.0) holds.
inline const Overrides& neutral_overrides() {
    static const Overrides o = {{"pass_count_max", "0"}, {"final_talus_iterations", "0"}};
    return o;
}

// The overrides of a golden fixture name: shipped, small, shipped-neutral,
// small-neutral.
inline Overrides fixture_overrides(std::string_view fixture) {
    Overrides o;
    if (fixture.substr(0, 5) == "small") {
        o = small_overrides();
    }
    if (fixture.size() > 8 && fixture.substr(fixture.size() - 8) == "-neutral") {
        o.insert(o.end(), neutral_overrides().begin(), neutral_overrides().end());
    }
    return o;
}

inline Overrides concat(Overrides a, const Overrides& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

// [worldgen.elevation] with each field's value from `overrides` (last one
// wins) when listed, the shipped value otherwise; `omit` drops a field.
inline std::string elevation_toml(const Overrides& overrides = {}, std::string_view omit = {}) {
    std::string s = "[worldgen.elevation]\n";
    for (const FieldText& f : shipped_fields()) {
        if (f.name == omit) {
            continue;
        }
        std::string value(f.value);
        for (const auto& o : overrides) {
            if (o.first == f.name) {
                value = o.second;
            }
        }
        s += std::string(f.name) + " = " + value + "\n";
    }
    return s;
}

// --- Values the tests need, parsed from the same texts ------------------------------

// The value of one field: scalar (raw Fx or int) or a noise record.
struct FieldValue {
    bool record = false;
    std::int64_t scalar = 0;     // fx raw, or the int value
    std::int64_t rec_a = 0;      // cycles (raw) or wavelength_m
    std::int32_t rec_octaves = 0;
    std::int64_t rec_gain = 0;   // raw
};

// Parses a value text of elevation_toml: "\"d\"" (fx), "n" (int or fx
// integer), or "{ k = v, k = v, k = v }".
inline FieldValue parse_value(std::string_view text, bool fx_field) {
    FieldValue v;
    auto parse_scalar = [](std::string_view t, bool as_fx) -> std::int64_t {
        if (!t.empty() && t.front() == '"') {
            return dec(t.substr(1, t.size() - 2));
        }
        const std::int64_t n = std::stoll(std::string(t));
        return as_fx ? Fx::from_int(n).raw() : n;
    };
    if (!text.empty() && text.front() == '{') {
        v.record = true;
        std::string body(text.substr(1, text.size() - 2));
        std::stringstream ss(body);
        std::string part;
        while (std::getline(ss, part, ',')) {
            const std::size_t eq = part.find('=');
            auto trim = [](std::string x) {
                const std::size_t a = x.find_first_not_of(' ');
                const std::size_t b = x.find_last_not_of(' ');
                return a == std::string::npos ? std::string() : x.substr(a, b - a + 1);
            };
            const std::string key = trim(part.substr(0, eq));
            const std::string val = trim(part.substr(eq + 1));
            if (key == "cycles") {
                v.rec_a = parse_scalar(val, true);
            } else if (key == "wavelength_m") {
                v.rec_a = parse_scalar(val, false);
            } else if (key == "octaves") {
                v.rec_octaves = static_cast<std::int32_t>(parse_scalar(val, false));
            } else if (key == "gain") {
                v.rec_gain = parse_scalar(val, true);
            }
        }
        return v;
    }
    v.scalar = parse_scalar(text, fx_field);
    return v;
}

// Integer fields of 4.2 (every other scalar field is fx).
[[nodiscard]] inline bool is_int_field(std::string_view name) {
    static const std::vector<std::string_view> ints = {
        "island_min_area_m2", "belt_offset_steps",   "erosion_iterations", "erosion_route_interval",
        "lake_min_area_m2",   "lake_max_area_m2",    "lake_area_ref_n",    "pass_count_max",
        "pass_min_length_m",  "pass_spacing_m",      "pass_size_ref_n",    "pass_tarn_min_cells",
        "final_talus_iterations",
    };
    return std::find(ints.begin(), ints.end(), name) != ints.end();
}

// The tuning values of one fixture, as the test reads them from its texts.
class Values {
public:
    explicit Values(const Overrides& overrides = {}) {
        for (const FieldText& f : shipped_fields()) {
            std::string value(f.value);
            for (const auto& o : overrides) {
                if (o.first == f.name) {
                    value = o.second;
                }
            }
            map_[std::string(f.name)] = parse_value(value, !is_int_field(f.name));
        }
    }
    [[nodiscard]] std::int64_t raw(std::string_view name) const { return get(name).scalar; }
    [[nodiscard]] Fx fx(std::string_view name) const { return Fx::from_raw(get(name).scalar); }
    [[nodiscard]] std::int64_t i(std::string_view name) const { return get(name).scalar; }
    [[nodiscard]] worldgen::geo::FbmMap map_rec(std::string_view name) const {
        const FieldValue& v = get(name);
        return worldgen::geo::FbmMap{Fx::from_raw(v.rec_a), v.rec_octaves, Fx::from_raw(v.rec_gain)};
    }
    [[nodiscard]] worldgen::geo::FbmM m_rec(std::string_view name) const {
        const FieldValue& v = get(name);
        return worldgen::geo::FbmM{v.rec_a, v.rec_octaves, Fx::from_raw(v.rec_gain)};
    }

    // Derived integers of 4.5.
    [[nodiscard]] std::int64_t margin(std::int64_t n) const {
        return std::max<std::int64_t>(1, (raw("edge_margin_frac") * n) >> 32);
    }
    [[nodiscard]] std::int64_t i_min() const { return (i("island_min_area_m2") + 255) / 256; }
    [[nodiscard]] std::int64_t lake_min_cells() const { return (i("lake_min_area_m2") + 255) / 256; }
    [[nodiscard]] std::int64_t lake_max_cells(std::int64_t n) const {
        const std::int64_t ref = i("lake_area_ref_n");
        const std::int64_t a = (i("lake_max_area_m2") * n * n) / (ref * ref);
        return (a + 255) / 256;
    }
    [[nodiscard]] std::int64_t lake_min_depth_dm() const {
        return worldgen::mul_div(raw("lake_min_depth_m"), 10, std::int64_t{1} << 32);
    }
    [[nodiscard]] std::int64_t fine_slope() const {
        return worldgen::mul_div(raw("fill_slope_dm"), std::int64_t{1} << 16, std::int64_t{1} << 32);
    }
    [[nodiscard]] std::int64_t target(std::int64_t land_cells) const {
        return worldgen::mul(Fx::from_int(land_cells), fx("segment_on_share")).round_int();
    }
    [[nodiscard]] std::int64_t max_share_cells(std::int64_t land_cells) const {
        return std::max<std::int64_t>(1, worldgen::mul(Fx::from_int(land_cells), fx("lake_max_share")).round_int());
    }
    [[nodiscard]] std::int64_t hw(std::int64_t n) const {
        const Fx side = Fx::from_int(16 * n);
        return std::min(fx("belt_halfwidth_m"), worldgen::mul(fx("belt_halfwidth_frac"), side)).raw();
    }

    // Pass integers of 4.5 for a grid of width w (fifths, cells, units).
    struct PassInts {
        std::int64_t num = 0, den = 0, coast_clear = 0, min_len = 0, spacing = 0, side_max = 0, reach5 = 0;
        std::int64_t win = 0, rise_b = 0, spacing_b = 0, tarn_b = 0, rise_u = 0, body_u = 0, walk = 0;
        std::int64_t drop_o = 0, drop_d = 0;  // D_o^f, D_d^f raw
    };
    [[nodiscard]] PassInts pass_ints(std::int64_t w) const {
        using worldgen::mul_div;
        PassInts p;
        const std::int64_t ref = i("pass_size_ref_n");
        p.num = std::min(w, ref);
        p.den = ref;
        p.coast_clear = mul_div(raw("pass_coast_clear_m"), 5 * p.num, 16 * ONE * p.den);
        p.min_len = (i("pass_min_length_m") * 5 * p.num) / (16 * p.den);
        p.spacing = (i("pass_spacing_m") * 5) / 16;
        p.side_max = mul_div(raw("pass_side_max_m"), p.num, p.den);
        p.reach5 = mul_div(raw("pass_divide_reach_m"), 5, 16 * ONE);
        p.win = (raw("pass_saddle_window_m") >> 32) / 16;
        p.rise_b = (raw("pass_saddle_rise_reach_m") >> 32) / 16;
        p.spacing_b = (raw("pass_min_spacing_m") >> 32) / 16;
        p.tarn_b = (raw("pass_col_tarn_m") >> 32) / 16;
        p.rise_u = raw("pass_saddle_rise_m") >> 16;
        p.body_u = raw("pass_body_m") >> 16;
        p.walk = raw("pass_walk_grade") * 48;
        p.drop_o = raw("final_talus_slope") * 16;
        p.drop_d = worldgen::mul(Fx::from_raw(p.drop_o), Fx::SQRT2).raw();
        return p;
    }

private:
    [[nodiscard]] const FieldValue& get(std::string_view name) const {
        const auto it = map_.find(std::string(name));
        REQUIRE_MESSAGE(it != map_.end(), "unknown field " << std::string(name));
        return it->second;
    }
    std::map<std::string, FieldValue> map_;
};

inline const Values& shipped_values() {
    static const Values v;
    return v;
}
inline const Values& small_values() {
    static const Values v(small_overrides());
    return v;
}
// The values of a golden fixture name (fixture_overrides).
inline const Values& fixture_values(std::string_view fixture) {
    static const Values shipped_neutral(fixture_overrides("shipped-neutral"));
    static const Values small_neutral(fixture_overrides("small-neutral"));
    if (fixture == "shipped") {
        return shipped_values();
    }
    if (fixture == "small") {
        return small_values();
    }
    if (fixture == "shipped-neutral") {
        return shipped_neutral;
    }
    REQUIRE_MESSAGE(fixture == "small-neutral", "unknown fixture " << std::string(fixture));
    return small_neutral;
}

// Stream keys of section 2.
[[nodiscard]] inline std::uint64_t key(std::uint64_t seed, std::string_view purpose) {
    return worldgen::stream_key(seed, std::string("geo.elevation.") + std::string(purpose));
}

// --- Grid hashes (P1-01 10.4, P0-04 8) --------------------------------------------

inline void put_le(worldgen::Sha256& h, std::uint64_t v, int bytes) {
    std::uint8_t b[8];
    for (int k = 0; k < bytes; ++k) {
        b[k] = static_cast<std::uint8_t>((v >> (8 * k)) & 0xFFU);
    }
    h.update(std::span<const std::uint8_t>(b, static_cast<std::size_t>(bytes)));
}

[[nodiscard]] inline std::string hash_i64(std::span<const std::int64_t> v) {
    worldgen::Sha256 h;
    for (const std::int64_t x : v) {
        put_le(h, static_cast<std::uint64_t>(x), 8);
    }
    return worldgen::to_hex(h.final());
}

[[nodiscard]] inline std::string hash_u8(std::span<const std::uint8_t> v) {
    return worldgen::to_hex(worldgen::sha256(v));
}

[[nodiscard]] inline std::string hash_i16(std::span<const std::int16_t> v) {
    worldgen::Sha256 h;
    for (const std::int16_t x : v) {
        put_le(h, static_cast<std::uint64_t>(static_cast<std::uint16_t>(x)), 2);
    }
    return worldgen::to_hex(h.final());
}

// The P0-04 8 layer hash of an i16 N x N layer: u32 N, u32 N, u8 4 (i16,
// P0-02 7.1), then the values.
[[nodiscard]] inline std::string layer_hash_i16(std::uint32_t w, std::uint32_t h, std::span<const std::int16_t> v) {
    worldgen::Sha256 s;
    put_le(s, w, 4);
    put_le(s, h, 4);
    put_le(s, 4, 1);
    for (const std::int16_t x : v) {
        put_le(s, static_cast<std::uint64_t>(static_cast<std::uint16_t>(x)), 2);
    }
    return worldgen::to_hex(s.final());
}

// --- Independent grid algorithms -------------------------------------------------------

[[nodiscard]] inline std::size_t cells(Grid g) { return static_cast<std::size_t>(g.cells()); }

// Calls fn(code, m) for every existing 8-neighbour m of i, in code order.
template <typename Fn>
inline void each_neighbour8(Grid g, std::size_t i, Fn&& fn) {
    for (std::uint8_t code = 0; code < 8; ++code) {
        worldgen::geo::CellIndex m = 0;
        if (worldgen::geo::neighbour(g, static_cast<worldgen::geo::CellIndex>(i), code, m)) {
            fn(code, static_cast<std::size_t>(m));
        }
    }
}

// Flood levels by a Dijkstra-style minimax search (P1-02 10.2): L = 0 on
// ocean; L[i] = the minimum over 8-paths to the ocean of the largest key on
// the path (ocean counting as 0). Independent of G3.
inline std::vector<std::int64_t> minimax_levels(Grid g, std::span<const std::int64_t> keys,
                                                std::span<const std::uint8_t> ocean) {
    const std::size_t n = cells(g);
    std::vector<std::int64_t> L(n, INT64_MAX);
    using Item = std::pair<std::int64_t, std::size_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> q;
    for (std::size_t i = 0; i < n; ++i) {
        if (ocean[i] != 0U) {
            L[i] = 0;
            q.emplace(0, i);
        }
    }
    while (!q.empty()) {
        const auto [l, c] = q.top();
        q.pop();
        if (l != L[c]) {
            continue;
        }
        each_neighbour8(g, c, [&](std::uint8_t, std::size_t m) {
            if (ocean[m] != 0U) {
                return;
            }
            const std::int64_t lm = std::max(keys[m], l);
            if (lm < L[m]) {
                L[m] = lm;
                q.emplace(lm, m);
            }
        });
    }
    return L;
}

// Union-find components of mask cells, 8- or 4-connected, joined only where
// `same` holds for the two cells; labels 1, 2, ... in order of each
// component's minimum index (the G2 numbering). Independent of G2.
inline std::vector<std::uint32_t> uf_components(Grid g, std::span<const std::uint8_t> mask, bool eight,
                                                const std::function<bool(std::size_t, std::size_t)>& same) {
    const std::size_t n = cells(g);
    std::vector<std::size_t> parent(n);
    for (std::size_t i = 0; i < n; ++i) {
        parent[i] = i;
    }
    std::function<std::size_t(std::size_t)> find = [&](std::size_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    for (std::size_t i = 0; i < n; ++i) {
        if (mask[i] == 0U) {
            continue;
        }
        each_neighbour8(g, i, [&](std::uint8_t code, std::size_t m) {
            if ((!eight && (code & 1U) != 0U) || mask[m] == 0U || !same(i, m)) {
                return;
            }
            const std::size_t a = find(i);
            const std::size_t b = find(m);
            if (a != b) {
                parent[std::max(a, b)] = std::min(a, b);
            }
        });
    }
    std::vector<std::uint32_t> label(n, 0);
    std::map<std::size_t, std::uint32_t> root_label;
    std::uint32_t next = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (mask[i] == 0U) {
            continue;
        }
        const std::size_t r = find(i);
        auto it = root_label.find(r);
        if (it == root_label.end()) {
            it = root_label.emplace(r, ++next).first;
        }
        label[i] = it->second;
    }
    return label;
}

// Chamfer 5/7 distance to the nearest source cell, by Dijkstra over
// 8-paths (P1-02 10.2: "chamfer by Dijkstra"). Independent of H7's sweeps.
inline std::vector<std::int64_t> chamfer_dijkstra(Grid g, std::span<const std::uint8_t> source) {
    const std::size_t n = cells(g);
    std::vector<std::int64_t> d(n, std::int64_t{1} << 40);
    using Item = std::pair<std::int64_t, std::size_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> q;
    for (std::size_t i = 0; i < n; ++i) {
        if (source[i] != 0U) {
            d[i] = 0;
            q.emplace(0, i);
        }
    }
    while (!q.empty()) {
        const auto [dc, c] = q.top();
        q.pop();
        if (dc != d[c]) {
            continue;
        }
        each_neighbour8(g, c, [&](std::uint8_t code, std::size_t m) {
            const std::int64_t v = dc + ((code & 1U) != 0U ? 7 : 5);
            if (v < d[m]) {
                d[m] = v;
                q.emplace(v, m);
            }
        });
    }
    return d;
}

// E5.3 for one ocean cell at chamfer distance dl.
[[nodiscard]] inline std::int64_t ocean_height(std::int64_t dl, const Values& v) {
    using worldgen::div;
    using worldgen::div_int;
    using worldgen::mul;
    const Fx dist_m = div_int(Fx::from_int(16 * std::min<std::int64_t>(dl, std::int64_t{1} << 20)), 5);
    const Fx q = div(dist_m, dist_m + v.fx("ocean_shelf_m"));
    return (-(Fx::HALF + mul(v.fx("ocean_depth_m"), q))).raw();
}

// E9 for one cell.
[[nodiscard]] inline std::int16_t quantize_cell(std::int64_t h, bool ocean) {
    const std::int64_t q = worldgen::mul_div(h, 10, std::int64_t{1} << 32);
    return static_cast<std::int16_t>(ocean ? std::min<std::int64_t>(-1, q) : std::max<std::int64_t>(1, q));
}

// E10.0 cost and bump grids (the test's own, from fbm_m and the stream keys).
struct CostBump {
    std::vector<std::int64_t> cost, bump;
};

inline CostBump cost_bump(std::uint64_t seed, std::int32_t n, std::span<const std::uint8_t> ocean, const Values& v) {
    using worldgen::clamp;
    using worldgen::mul;
    const std::size_t nn = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    CostBump cb{std::vector<std::int64_t>(nn, ONE), std::vector<std::int64_t>(nn, 0)};
    const std::uint64_t kf = key(seed, "fill");
    const std::uint64_t kd = key(seed, "detail");
    const auto fill_noise = v.m_rec("fill_noise");
    const auto detail_noise = v.m_rec("detail_noise");
    for (std::int32_t y = 0; y < n; ++y) {
        for (std::int32_t x = 0; x < n; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x);
            if (ocean[i] != 0U) {
                continue;
            }
            const Fx X = Fx::from_int(16 * x + 8);
            const Fx Y = Fx::from_int(16 * y + 8);
            const Fx nz = worldgen::geo::fbm_m(kf, X, Y, fill_noise);
            cb.cost[i] = (Fx::ONE + mul(v.fx("fill_cost_amp"), clamp(Fx::HALF + nz, Fx(), Fx::ONE))).raw();
            const Fx dn = worldgen::geo::fbm_m(kd, X, Y, detail_noise);
            const Fx bv = mul(v.fx("fill_detail_m"), worldgen::max(Fx(), dn));
            cb.bump[i] = worldgen::mul_div(bv.raw(), 10 * FINE, std::int64_t{1} << 32);
        }
    }
    return cb;
}

// --- E2 and E5 statements, from the spec text --------------------------------------

struct CellFields {
    std::int64_t c = 0, nb = 0, ns = 0;
};

// E1 continent centre coordinate k (0: cx, 1: cy).
[[nodiscard]] inline std::int64_t centre(std::uint64_t seed, std::int64_t k, const Values& v) {
    const std::uint64_t hv = worldgen::hash_at(key(seed, "continent"), k, 0) >> 32U;
    const std::int64_t r = (static_cast<std::int64_t>(hv) - (std::int64_t{1} << 31)) * 2;
    return (Fx::HALF + worldgen::mul(v.fx("continent_center_jitter"), Fx::from_raw(r))).raw();
}

// E2 for one cell, every statement as written.
inline CellFields e2_cell(std::uint64_t seed, std::int32_t n, std::int32_t x, std::int32_t y, const Values& v) {
    using worldgen::clamp;
    using worldgen::div;
    using worldgen::div_int;
    using worldgen::fade;
    using worldgen::hypot;
    using worldgen::max;
    using worldgen::min;
    using worldgen::mul;
    using worldgen::mul_int;
    using worldgen::geo::fbm_m;
    using worldgen::geo::fbm_map;
    const Fx ONEf = Fx::ONE;
    const Fx X = Fx::from_int(16 * x + 8);
    const Fx Y = Fx::from_int(16 * y + 8);
    const Fx U = Fx::from_ratio(2 * x + 1, 2 * static_cast<std::int64_t>(n));
    const Fx V = Fx::from_ratio(2 * y + 1, 2 * static_cast<std::int64_t>(n));
    const Fx cx = R(centre(seed, 0, v));
    const Fx cy = R(centre(seed, 1, v));
    const Fx wx = fbm_map(key(seed, "continent_warp_x"), U, V, v.map_rec("continent_warp"));
    const Fx wy = fbm_map(key(seed, "continent_warp_y"), U, V, v.map_rec("continent_warp"));
    const Fx Uw = U + mul(v.fx("continent_warp_amp_frac"), wx);
    const Fx Vw = V + mul(v.fx("continent_warp_amp_frac"), wy);
    const Fx d = mul_int(hypot(Uw - cx, Vw - cy), 2);
    const Fx nc = fbm_map(key(seed, "continent"), Uw, Vw, v.map_rec("continent_noise"));
    const Fx Uf = U + div_int(Uw - U, 2);
    const Fx Vf = V + div_int(Vw - V, 2);
    const Fx ed = min(min(Uf, Vf), min(ONEf - Uf, ONEf - Vf));
    const Fx edn = ed + mul(v.fx("edge_noise_amp"), nc);
    const Fx fe = fade(clamp(div(edn, v.fx("edge_falloff_frac")), Fx(), ONEf));
    const Fx dm = mul_int(hypot(Uf - Fx::HALF, Vf - Fx::HALF), 2) - mul(v.fx("edge_noise_amp"), nc);
    const Fx fr = fade(clamp(div(dm - v.fx("edge_radial_start"), v.fx("edge_radial_end") - v.fx("edge_radial_start")),
                             Fx(), ONEf));
    const Fx edge = mul(v.fx("edge_falloff_depth"), max(ONEf - fe, fr));
    const Fx cd = fbm_m(key(seed, "coast"), X, Y, v.m_rec("coast_noise"));
    const Fx c = (((ONEf - d) + mul(v.fx("continent_noise_amp"), nc)) - edge) + mul(v.fx("coast_noise_amp"), cd);
    const Fx nb = fbm_map(key(seed, "belt"), Uw, Vw, v.map_rec("belt_noise"));
    const Fx ns = fbm_map(key(seed, "segment"), Uw, Vw, v.map_rec("segment_noise"));
    return CellFields{c.raw(), nb.raw(), ns.raw()};
}

// E3 step 2 on a W x H grid of nb raw values.
inline std::vector<std::int64_t> belt_gradient(Grid g, std::span<const std::int64_t> nb) {
    std::vector<std::int64_t> out(cells(g));
    const std::int32_t w = g.width;
    const std::int32_t h = g.height;
    for (std::int32_t y = 0; y < h; ++y) {
        for (std::int32_t x = 0; x < w; ++x) {
            const std::int32_t x0 = std::max(x - 1, 0);
            const std::int32_t x1 = std::min(x + 1, w - 1);
            const std::int32_t y0 = std::max(y - 1, 0);
            const std::int32_t y1 = std::min(y + 1, h - 1);
            const Fx gx = worldgen::div_int(R(nb[g.index(x1, y)]) - R(nb[g.index(x0, y)]), x1 - x0);
            const Fx gy = worldgen::div_int(R(nb[g.index(x, y1)]) - R(nb[g.index(x, y0)]), y1 - y0);
            out[g.index(x, y)] = worldgen::hypot(gx, gy).raw();
        }
    }
    return out;
}

[[nodiscard]] inline std::int64_t kth_sorted(std::vector<std::int64_t> v, std::uint64_t k) {
    std::sort(v.begin(), v.end());
    return v[static_cast<std::size_t>(k)];
}

[[nodiscard]] inline std::uint64_t q_rank(std::int64_t q, std::uint64_t n) {
    return std::min<std::uint64_t>(n - 1, (static_cast<std::uint64_t>(q) * n) >> 32U);
}

// rc(a, b) of 5.0.
[[nodiscard]] inline Fx rc(Fx a, Fx b) {
    if (a >= worldgen::mul_int(b, 1024)) {
        return Fx::from_int(1024);
    }
    return worldgen::div(a, b);
}

// E5 coast factor gm of a land cell.
[[nodiscard]] inline std::int64_t gm_of(std::int64_t c, std::int64_t c_sea, std::int64_t s_land, const Values& v) {
    const Fx s = R(c) - R(c_sea);
    const Fx tl = rc(worldgen::abs(s), R(s_land));
    return worldgen::fade(worldgen::clamp(worldgen::div(tl, v.fx("mountain_coast_t")), Fx(), Fx::ONE)).raw();
}

// mask(o, beta) of E5 for one cell.
[[nodiscard]] inline std::int64_t mask_of(std::int64_t nb, std::int64_t ns, std::int64_t gm, std::int64_t o,
                                          std::int64_t beta, std::int64_t w_b, const Values& v) {
    using worldgen::clamp;
    using worldgen::fade;
    using worldgen::mul;
    const Fx a = worldgen::abs(R(nb) - R(o));
    const Fx b = a >= R(w_b) ? Fx() : fade(Fx::ONE - worldgen::div(a, R(w_b)));
    const Fx sg = fade(clamp((Fx::HALF + R(beta)) + mul(v.fx("segment_contrast"), R(ns)), Fx(), Fx::ONE));
    return mul(mul(b, sg), R(gm)).raw();
}

// The test's own E5.1 search over lists of land cells. Also returns the
// bisection mids (empty when the bisection does not run).
struct FloorOwn {
    bool triggered = false;
    std::int64_t target = 0, o_star = 0, beta_star = 0;
    std::vector<std::int64_t> mids;
};

inline FloorOwn floor_own(std::span<const std::int64_t> nb, std::span<const std::int64_t> ns,
                          std::span<const std::int64_t> gm, std::int64_t w_b, const Values& v) {
    const std::size_t n = nb.size();
    const std::int64_t lev = v.raw("segment_floor_mask");
    const std::int64_t inland = v.raw("floor_inland_gm");
    const std::int64_t bias = v.raw("segment_bias");
    const std::int64_t two = Fx::from_int(2).raw();
    auto core = [&](std::int64_t o, std::int64_t beta) {
        std::int64_t k = 0;
        for (std::size_t i = 0; i < n; ++i) {
            k += mask_of(nb[i], ns[i], gm[i], o, beta, w_b, v) >= lev ? 1 : 0;
        }
        return k;
    };
    auto inl = [&](std::int64_t o, std::int64_t beta) {
        std::int64_t k = 0;
        for (std::size_t i = 0; i < n; ++i) {
            k += (gm[i] >= inland && mask_of(nb[i], ns[i], gm[i], o, beta, w_b, v) >= lev) ? 1 : 0;
        }
        return k;
    };
    FloorOwn r;
    r.target = v.target(static_cast<std::int64_t>(n));
    r.beta_star = bias;
    if (core(0, bias) >= r.target) {
        return r;
    }
    r.triggered = true;
    const std::int64_t d = worldgen::div_int(R(w_b), 2).raw();
    std::int64_t best_o = 0;
    std::int64_t best_r = inl(0, two);
    if (best_r < r.target) {
        bool done = false;
        for (std::int64_t s = 1; s <= v.i("belt_offset_steps") && !done; ++s) {
            for (const std::int64_t sign : {std::int64_t{1}, std::int64_t{-1}}) {
                const std::int64_t o = worldgen::mul_int(R(d), sign * s).raw();
                const std::int64_t rr = inl(o, two);
                if (rr > best_r) {
                    best_r = rr;
                    best_o = o;
                }
                if (rr >= r.target) {
                    done = true;
                    break;
                }
            }
        }
    }
    r.o_star = best_o;
    std::int64_t lo = bias;
    std::int64_t hi = two;
    if (core(r.o_star, lo) >= r.target) {
        hi = lo;
    } else {
        for (int step = 0; step < 32; ++step) {
            const std::int64_t mid = (lo + hi) >> 1;
            r.mids.push_back(mid);
            if (core(r.o_star, mid) >= r.target) {
                hi = mid;
            } else {
                lo = mid;
            }
        }
    }
    r.beta_star = hi;
    return r;
}

// --- Golden file (worldgen/tests/golden/p1-01-elevation.txt) ------------------------

struct GoldenCase {
    std::string name;
    std::string fixture;  // shipped | small
    std::int32_t n = 0;
    std::uint64_t seed = 0;
    std::vector<std::pair<std::string, std::string>> items;  // in file (step) order

    [[nodiscard]] std::string get(std::string_view item) const {
        for (const auto& kv : items) {
            if (kv.first == item) {
                return kv.second;
            }
        }
        FAIL("golden case " << name << " has no item " << std::string(item));
        return {};
    }
};

inline std::vector<GoldenCase> read_golden(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << path);
    std::vector<GoldenCase> out;
    std::string line;
    while (std::getline(in, line)) {
        REQUIRE_MESSAGE((line.empty() || line.back() != '\r'), "golden file must be LF: " << path);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ss(line);
        std::string a, b, c;
        ss >> a >> b >> c;
        if (a == "case") {
            GoldenCase gc;
            gc.name = b;
            gc.fixture = c;
            std::int64_t n = 0;
            std::uint64_t seed = 0;
            ss >> n >> seed;
            gc.n = static_cast<std::int32_t>(n);
            gc.seed = seed;
            out.push_back(gc);
        } else {
            REQUIRE_MESSAGE((!out.empty() && out.back().name == a), "golden line outside its case: " << line);
            out.back().items.emplace_back(b, c);
        }
    }
    return out;
}

}  // namespace elev_test
