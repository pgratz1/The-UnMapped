#pragma once

// Stage P1-02 hydrology (spec docs/specs/p1-02-hydrology.md; P1-00 sections
// 3, 4, 7 and 8).
//
// Analysis of the stored `elevation` layer (P1-00 5.1), which is not
// changed. Its only seed-dependent input is the tie-break noise of H2
// (stream geo.hydrology.flat), which orders cells inside a decimetre flat
// and never changes a lake or a water level. Steps H1 to H9: preconditions
// (H1), the routing flood on decimetre levels with cells of one level popped
// lowest t first (H2; hydrology's own flood, not the shared G3), lakes as the G2 components of
// the cells below their flood level (H3), G4 receivers with one outlet per
// lake (H4), accumulation by Kahn's method (H5), per-cell classification
// (H6), the chamfer coast distance (H7), and the seven layers written in one
// stage transaction (H9).
//
// Every step is a library function on plain arrays and any W x H grid (the
// worked example of P1-02 5.1 is 8 x 7), so the tests of P1-02 section 8 can
// call it alone (the interim seams of 8). Masks are one byte per cell,
// non-zero meaning "in the set". Cell (x, y) has index y * width + x.
//
// Every function raises FatalError (P0-01 4.1) on a contract violation: an
// invalid grid or an array of the wrong length, threads outside [1, 256], or
// a violated precondition. `threads` only schedules the G6 row blocks of the
// parallel steps (P1-00 D10): no result depends on it.

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/data/value.hpp"
#include "worldgen/geo/components.hpp"
#include "worldgen/geo/flow.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/geo/noise.hpp"
#include "worldgen/stages/stage.hpp"

namespace worldgen::stages {

// The stage name recorded in meta.stages (P1-00 3).
inline constexpr std::string_view HYDROLOGY_STAGE = "hydrology";

// The layers the stage writes, in writing order (P1-02 3, H9; P1-00 4).
inline constexpr std::string_view WATER_LAYER = "water";
inline constexpr std::string_view WATER_DEPTH_LAYER = "water_depth";
inline constexpr std::string_view FLOW_DIR_LAYER = "flow_dir";
inline constexpr std::string_view FLOW_ACCUM_LAYER = "flow_accum";
inline constexpr std::string_view RIVER_LAYER = "river";
inline constexpr std::string_view LAKE_LAYER = "lake";
inline constexpr std::string_view COAST_DIST_LAYER = "coast_dist";

// `water` categories (P1-00 4).
inline constexpr std::uint8_t WATER_DRY = 0;
inline constexpr std::uint8_t WATER_RIVER = 1;
inline constexpr std::uint8_t WATER_LAKE = 2;
inline constexpr std::uint8_t WATER_OCEAN = 3;

// Fixed constants of the algorithm (P1-02 4).
inline constexpr std::int32_t RIVER_CLASS_CAP = 15;
inline constexpr std::int64_t COAST_DIST_MAX = 32767;  // |coast_dist| saturates here (P1-00 4)

// Bounds of river_min_area_m2 (P1-02 4, the schema's int(256, 1000000000)).
inline constexpr std::int64_t RIVER_MIN_AREA_LO = 256;
inline constexpr std::int64_t RIVER_MIN_AREA_HI = 1000000000;

// --- Tuning (P1-02 section 4) -------------------------------------------------------

// The fields of definition `worldgen.hydrology`.
struct HydrologyTuning {
    std::int64_t river_min_area_m2 = 0;
    geo::FbmM flat_noise;  // the tie-break noise of H2 (P1-02 4, S2)

    friend bool operator==(const HydrologyTuning&, const HydrologyTuning&) = default;
};

// The tuning held by a validated `worldgen.hydrology` record. Fatal if a
// field is missing or of another kind.
[[nodiscard]] HydrologyTuning hydrology_tuning_from(const data::Record& record);
// The tuning of definition worldgen.hydrology in loaded content. Fatal if
// the content has no such definition.
[[nodiscard]] HydrologyTuning hydrology_tuning_from(const data::ContentSet& content);

// T = (river_min_area_m2 + 255) div 256 cells (P1-02 4): 1 for 256, 2 for
// 257, 1954 for 500000. Fatal outside [256, 10^9].
[[nodiscard]] std::int64_t river_threshold_cells(std::int64_t river_min_area_m2);

// The river class of a non-lake land cell with accumulation a (P1-02 H6):
// 0 if a < t, else min(15, bit_length(a div t)). Fatal if t < 1 or a < 0.
[[nodiscard]] std::uint8_t river_class(std::int64_t a, std::int64_t t);

// --- H1: preconditions -----------------------------------------------------------------

// P1-02 section 2 and H1 on a W x H grid of decimetre heights: no cell is 0;
// at least one cell is ocean (e < 0); every ocean cell is reached by a
// 4-connected flood from the ocean cells on the grid edge. Fatal
// ("hydrology: elevation precondition: ...") otherwise.
void check_hydrology_preconditions(geo::Grid grid, std::span<const std::int16_t> e);

// --- H2 to H5: the drainage network ----------------------------------------------------------

// --- H2 step 1: tie values ---------------------------------------------------------------

// The largest tie value t (P1-02 H2): t is in [0, 65535].
inline constexpr std::int64_t FLAT_TIE_MAX = 65535;
// Keys of the routing flood carry t in their low 16 bits (P1-02 H2).
inline constexpr std::int32_t FLAT_TIE_BITS = 16;

// H2 step 1 on a W x H grid (D1, G6 row blocks): 0 on ocean cells
// (ocean[i] != 0); on land, with X = from_int(16x + 8), Y = from_int(16y +
// 8) and nz = fbm_m(stream_key(seed, "geo.hydrology.flat"), X, Y,
// tuning.flat_noise), t = min(65535, (nz.raw + 2^32) >> 17). Fatal if the
// grid or the mask length is invalid, threads is outside [1, 256], or the
// noise is (for example a coordinate beyond the P0-01 bounds).
[[nodiscard]] std::vector<std::uint16_t> flat_tie(std::uint64_t seed, geo::Grid grid,
                                                  std::span<const std::uint8_t> ocean, const HydrologyTuning& tuning,
                                                  std::uint32_t threads = 1);

// --- H2 to H5: the drainage network ----------------------------------------------------------

// The intermediate results of H2 to H5 (P1-02 8.1 test 1).
struct HydrologyNetwork {
    std::vector<std::uint8_t> ocean;   // 1 where e < 0 (the mask O)
    std::vector<std::int64_t> level_dm;   // H2: flood level L, decimetres (the minimax level)
    std::vector<geo::Direction> parent;   // H2: flood parent, 8 on ocean
    std::vector<geo::CellIndex> order;    // H2: every cell in pop order (smallest (L x 2^16 + t, seq))
    std::vector<geo::Direction> g4;    // H4 step 1: G4 receivers before rerouting (8 on ocean)
    geo::Components lakes;             // H3: lake components (G2, 8-connected, equal L); label = lake id
    std::vector<std::int64_t> lake_level;     // lake_level[k - 1]: the common L (decimetres) of lake k
    std::vector<geo::CellIndex> lake_outlet;  // lake_outlet[k - 1]: the outlet of lake k (H4 step 2)
    std::vector<geo::Direction> rcv;   // H4: final receivers (flow_dir), 8 on ocean
    std::vector<std::uint32_t> accum;  // H5: A, 0 on ocean
};

// H1 then H2 to H5 on a W x H grid of decimetre heights and tie values
// (one per cell, ignored on ocean; an empty span means t = 0 everywhere,
// which gives the routing of keys e alone, P1-02 H2 "t = 0"). Fatal if a
// non-empty tie span has another length than grid.cells().
[[nodiscard]] HydrologyNetwork hydrology_network(geo::Grid grid, std::span<const std::int16_t> e,
                                                 std::span<const std::uint16_t> tie, std::uint32_t threads = 1);

// --- H7: coast distance ---------------------------------------------------------------------

// H7 on any grid and ocean mask (P1-02 8.1 test 8; no other precondition):
// dl = chamfer(source = ocean), do = chamfer(source = land); land cells
// min(32767, (32 dl + 5) div 10) metres, ocean cells -min(32767, (32 do + 5)
// div 10). A grid with no land (or no ocean) saturates at -32767 (32767).
[[nodiscard]] std::vector<std::int16_t> coast_distance(geo::Grid grid, std::span<const std::uint8_t> ocean);

// --- The whole stage ----------------------------------------------------------------------------

// The seven layers of P1-02 section 3, each grid.cells() values.
struct HydrologyLayers {
    std::vector<std::uint8_t> water;
    std::vector<std::uint16_t> water_depth;
    std::vector<std::uint8_t> flow_dir;
    std::vector<std::uint32_t> flow_accum;
    std::vector<std::uint8_t> river;
    std::vector<std::uint32_t> lake;
    std::vector<std::int16_t> coast_dist;
};

// H1 to H8 on a W x H grid of decimetre heights and tie values (as
// hydrology_network; P1-02 8). The stage passes flat_tie(seed, ...).
[[nodiscard]] HydrologyLayers hydrology_layers(geo::Grid grid, std::span<const std::int16_t> e,
                                               std::span<const std::uint16_t> tie, const HydrologyTuning& tuning,
                                               std::uint32_t threads = 1);

// The stage entry point (P1-02 H9, P1-00 3): reads worldgen.hydrology from
// the context's content and the stored `elevation` layer (i16, N x N with N
// = param.map_size; fatal otherwise), runs hydrology_layers and writes the
// seven layers (codec 1) in the order of section 3. The tie values are
// flat_tie(meta.seed, ...).
void run_hydrology(StageContext& context);

}  // namespace worldgen::stages
