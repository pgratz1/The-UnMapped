#pragma once

// Ground detail of the 3D terrain preview (spec V3D-06 sections 3 to 9, 14).
//
// Plain C++, no Godot types, no threads: the fine layouts, the integer
// upsampling of the 16 m heights, the coarse factor fields, the seeded
// detail noise, the pads under markers, the field itself and the fine
// control and colour maps, and the statistics of the bench. Every value
// that reaches a buffer is integer arithmetic (17): uint32 and uint64
// wrapping hashes, int64 products and floor division written out. The
// scene (terrain_preview.cpp) runs build_detail_band on worker tasks, one
// band of rows each; every vertex is a pure function of the DetailPlan and
// its coordinates, so the result is the same for any band split.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "terrain/ground.hpp"
#include "terrain/look.hpp"
#include "terrain/terrain_data.hpp"

namespace unmapped::terrain {

// --- Constants (5 to 7) ---------------------------------------------------------------

inline constexpr std::int32_t A_MAX_CM = 1200;
inline constexpr std::int32_t PAD_FLAT_DM = 60;
inline constexpr std::int32_t PAD_FALL = 48;  // m
inline constexpr std::uint64_t DETAIL_KEY = 0x64657461696C3036ull;
inline constexpr int DETAIL_DEFAULT_SPACING = 4;
inline constexpr int DETAIL_DEFAULT_THREADS = 8;
inline constexpr int DETAIL_MAX_THREADS = 16;
inline constexpr int DETAIL_BAND_ROWS = 256;

struct DetailAmplitude {
    std::int32_t amp_cm;
    std::int32_t ridge_pm;
    std::int32_t billow_pm;
};
// Table 5, indexed by land type 0 to 10.
extern const std::array<DetailAmplitude, 11> DETAIL_AMPLITUDE;

// 6: octave wavelengths (m) and weights (per mille); octave 5 only at s = 2.
inline constexpr std::array<std::int32_t, 6> OCT_LAMBDA = {128, 64, 32, 16, 8, 4};
inline constexpr std::array<std::int32_t, 6> OCT_PM = {361, 252, 177, 124, 86, 60};

// Floor division for b > 0 (C++ division truncates).
[[nodiscard]] constexpr std::int64_t fdiv(std::int64_t a, std::int64_t b) noexcept {
    const std::int64_t q = a / b;
    return (a % b != 0 && a < 0) ? q - 1 : q;
}

// --- Options and layouts (3.2, 13.2) ----------------------------------------------------

// `off` gives 0; 8, 4 or 2 that spacing; anything else nullopt.
[[nodiscard]] std::optional<int> parse_ground_detail(std::string_view v);
[[nodiscard]] std::optional<int> parse_detail_threads(std::string_view v);
[[nodiscard]] std::string ground_detail_refusal(std::string_view v);
[[nodiscard]] std::string detail_threads_refusal(std::string_view v);
inline constexpr const char* DETAIL_LIMIT_NOTE = "--ground-detail 2 is limited to maps of 512 and 1024 cells: 4 m used";
inline constexpr const char* DETAIL_OFF_STATUS = "ground detail: off (16 m, as V3D-05)";

// The spacing used for a requested one on a map of n cells: 2 at 2048 is 4.
[[nodiscard]] int effective_spacing(int n, int s) noexcept;
// The fine layout of 3.2 for a coarse layout and spacing s in {2, 4, 8}.
[[nodiscard]] Layout fine_layout(const Layout& coarse, int s);
// 7 + log2(16 / s).
[[nodiscard]] int detail_mesh_lods(int s) noexcept;

// --- Upsampling (4) --------------------------------------------------------------------

// hd of V3D-01 5.2 in decimetres per cell: e + d where e > 0 and d > 0, else e.
[[nodiscard]] std::vector<std::int32_t> coarse_decimetres(std::span<const std::int16_t> elevation,
                                                          std::span<const std::uint16_t> water_depth);
// The four Catmull-Rom weights W(-1), W(0), W(1), W(2) at a of f, scaled by 2 f^3.
[[nodiscard]] std::array<std::int64_t, 4> catmull_rom_weights(int a, int f) noexcept;
// up_mm at fine vertex (xf, yf) of a map of n cells at factor f.
[[nodiscard]] std::int64_t upsample_mm(std::span<const std::int32_t> hd, int n, int f, int xf, int yf) noexcept;

// --- Coarse factor fields (5) ----------------------------------------------------------

// 3 per orthogonal step, 4 per diagonal; sources 0, others 6 n.
[[nodiscard]] std::vector<std::int32_t> chamfer_distance(const std::vector<std::uint8_t>& source, int n);
// clamp((D - 3) 1000 / 6, 0, 1000), and 0 for D <= 4 (built: the diagonal
// neighbour of a wet cell is 0, so every quad touching one is noise free).
[[nodiscard]] std::int32_t detail_ramp(std::int32_t d) noexcept;

struct DetailFactors {
    int n = 0;
    std::vector<std::int32_t> a_cell;   // cm
    std::vector<std::int16_t> ridge;    // Rb, per mille
    std::vector<std::int16_t> billow;   // Bb, per mille
    std::vector<std::int16_t> w_noise;  // per mille
    std::vector<std::int16_t> w_level;  // per mille
    std::vector<std::int16_t> m;        // M, 1000 to 2500
};

// Section 5 from the elevation (dm), the ground cells (empty: every cell type
// 0, none painted) and the water layer (empty when absent).
[[nodiscard]] DetailFactors build_detail_factors(const Layout& layout, std::span<const std::int16_t> elevation,
                                                 const GroundCells& cells, std::span<const std::uint8_t> water);

// --- Noise (6) -------------------------------------------------------------------------

[[nodiscard]] std::uint32_t octave_key(std::uint64_t seed, int o) noexcept;
[[nodiscard]] std::uint32_t warp_key(std::uint64_t seed, int axis) noexcept;  // axis 1 x, 2 z
// One value-noise octave n_o at (X_m, Z_m) metres from the map's corner.
[[nodiscard]] std::int32_t noise_octave(std::uint32_t key, std::int32_t lambda, std::int64_t xm, std::int64_t zm) noexcept;
// The number of octaves used at spacing s (lambda >= 2 s).
[[nodiscard]] int octave_count(int s) noexcept;
// N(v) of 6 with the given ridge and billow (per mille).
[[nodiscard]] std::int32_t detail_noise(const std::array<std::uint32_t, 6>& keys, int octaves, std::int64_t xm,
                                        std::int64_t zm, std::int32_t ridge, std::int32_t billow) noexcept;

// --- Pads (7) --------------------------------------------------------------------------

// A place a pad lies under: its model cell and r0 (m).
struct PadPlace {
    int cell_x = 0;
    int cell_y = 0;
    std::int32_t r0 = 0;
};

struct Pad {
    int cell_x = 0;
    int cell_y = 0;
    std::int64_t x = 0;  // anchor, metres from the map's corner (16 cell_x)
    std::int64_t z = 0;
    std::int32_t r0 = 0;
    std::int64_t p_mm = 0;
    std::int32_t rel_dm = 0;
    std::int32_t level_pm = 0;
};

// 7: one pad per place in order, none on a model cell of a lake or the sea
// (water 2 or 3) or with E <= 0 (built; the spec's water != 0 would drop the
// river cells most settlements stand on). hd as coarse_decimetres.
[[nodiscard]] std::vector<Pad> build_pads(std::span<const PadPlace> places, int n, std::span<const std::int32_t> hd,
                                          std::span<const std::int16_t> elevation, std::span<const std::uint8_t> water);
// sm(t) of 7.
[[nodiscard]] std::int64_t pad_smooth(std::int64_t t) noexcept;
// rho_i(d) of 7.
[[nodiscard]] std::int64_t pad_rho(const Pad& pad, std::int64_t d) noexcept;

// --- The plan and the bands (8, 9) -----------------------------------------------------

struct DetailPlan {
    int n = 0;   // coarse cells per side
    int s = 4;   // spacing
    int f = 4;   // 16 / s
    int v = 0;   // n f
    std::uint64_t seed = 0;
    std::vector<std::int32_t> hd;
    DetailFactors factors;
    std::vector<Pad> pads;
    // Pads per 256 m block (16 x 16 cells) whose influence square meets it.
    int blocks = 0;
    std::vector<std::vector<std::uint32_t>> block_pads;
    std::array<std::uint32_t, 6> keys{};
    int octaves = 0;
    std::uint32_t warp_x = 0;
    std::uint32_t warp_z = 0;
    // Fine control and colour (9): coarse control (empty: control 0 on every
    // vertex) and colour (RGBA, empty: no colour); look.rock picks the rock id.
    std::span<const std::uint32_t> control;
    std::span<const std::uint8_t> colour;
    bool textured = false;
    Look look;
};

// The plan of a world: factors, pads and keys; spans in `plan` point into
// the caller's buffers, which must outlive every band.
[[nodiscard]] DetailPlan make_detail_plan(const Layout& coarse, int s, std::uint64_t seed,
                                          std::span<const std::int16_t> elevation,
                                          std::span<const std::uint16_t> water_depth, const GroundCells& cells,
                                          std::span<const std::uint8_t> water, std::span<const PadPlace> places,
                                          std::span<const std::uint32_t> control, std::span<const std::uint8_t> colour,
                                          bool textured, const Look& look);

// H (mm) at fine vertex (xf, yf), and N_pad (per mille) there.
[[nodiscard]] std::int64_t field_mm(const DetailPlan& plan, int xf, int yf) noexcept;
[[nodiscard]] std::int32_t pad_noise_factor(const DetailPlan& plan, std::int64_t xm, std::int64_t zm) noexcept;

// Rows y0 <= y < y1 of the outputs, each v * v long (control and colour
// may be empty: not built). Writes only those rows.
void build_detail_band(const DetailPlan& plan, int y0, int y1, std::span<float> heights,
                       std::span<std::uint32_t> control, std::span<std::uint8_t> colour);

// The whole field, control and colour, in `bands` bands in order (U66).
struct DetailBuffers {
    std::vector<float> heights;
    std::vector<std::uint32_t> control;
    std::vector<std::uint8_t> colour;
};
[[nodiscard]] DetailBuffers build_detail_buffers(const DetailPlan& plan, int bands, bool with_maps);
// The plain build of the field alone (the check's path).
[[nodiscard]] GroundField build_ground_field(const DetailPlan& plan);

// 9's rock_f for a slope g_f (mm over 2 s m) at spacing s.
[[nodiscard]] std::int64_t fine_rock(std::int64_t g_f, int s) noexcept;
// 9's control at one fine vertex given the H of its four neighbours.
[[nodiscard]] std::uint32_t fine_control(const DetailPlan& plan, int xf, int yf, std::int64_t h, std::int64_t hl,
                                         std::int64_t hr, std::int64_t hu, std::int64_t hd_) noexcept;
// 9's colour (RGBA) at one fine vertex.
[[nodiscard]] std::array<std::uint8_t, 4> fine_colour(const DetailPlan& plan, int xf, int yf) noexcept;
// The warp of 9 at a fine vertex, 1/16 m.
[[nodiscard]] std::array<std::int32_t, 2> fine_warp(const DetailPlan& plan, int xf, int yf) noexcept;

// --- Stats (14) -------------------------------------------------------------------------

struct DetailTypeStats {
    int type = 0;
    std::uint64_t vertices = 0;
    double rms = 0.0;      // m, median of the 64 m window standard deviations
    double steep_pm = 0.0;  // per mille of vertices with rock_f > 0
    double steep_base_pm = 0.0;  // the same on the upsampled 16 m ground alone
    std::uint64_t windows = 0;
};
[[nodiscard]] std::vector<DetailTypeStats> detail_stats(const DetailPlan& plan, const GroundField& field,
                                                        const GroundCells& cells);
// "<type> rms <m> m, steep <pm>; ..." of 13.3.
[[nodiscard]] std::string detail_stats_text(std::span<const DetailTypeStats> stats);

// FNV-1a 64 of a buffer's bytes (U57, T50, T52).
[[nodiscard]] std::uint64_t fnv1a64(const void* data, std::size_t bytes) noexcept;

}  // namespace unmapped::terrain
