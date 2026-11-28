#pragma once

// Near cells of the 3D terrain preview's vegetation (spec V3D-03 6).
//
// Plain C++, no Godot types, no threads and no clock: the grid of 256 m
// cells, their boxes and distances, and the state machine that decides
// which cells are wanted, computed, attached and dropped, under the
// per-frame budgets of NearParams. TerrainPreview drives it once per frame
// and runs the work it hands out (worker tasks, nodes); the unit tests
// (gdext/tests/near_cells_test.cpp) drive it with simulated workers. The
// state after any sequence of calls is a function of that sequence alone.

#include <array>
#include <cstdint>
#include <set>
#include <span>
#include <vector>

#include "terrain/ground.hpp"
#include "terrain/terrain_data.hpp"

namespace unmapped::terrain {

inline constexpr int NEAR_CELL_CELLS = 16;            // terrain cells per near cell side
inline constexpr double NEAR_CELL_M = 256.0;          // metres
inline constexpr double NEAR_FULL_END = 1200.0;       // far kinds at full density to here (4.1)
inline constexpr double NEAR_FADE = 100.0;            // fade margin
inline constexpr double NEAR_VISIBLE_MAX = 1400.0;    // NEAR_FULL_END + 2 NEAR_FADE
inline constexpr double NEAR_RADIUS_DEFAULT = 2048.0;
inline constexpr double NEAR_RADIUS_MIN = 1536.0;     // --veg-radius, besides 0
inline constexpr double NEAR_RADIUS_MAX = 8192.0;
inline constexpr double NEAR_BOX_BELOW = 2.0;         // m below the lowest vertex (buried trunks)
inline constexpr double NEAR_BOX_ABOVE = 22.0;        // m above the highest (conifer 15.5 m x 1.40 = 21.7 m)

struct NearParams {
    double radius = NEAR_RADIUS_DEFAULT;
    double hysteresis = 512.0;
    int max_in_flight = 4;
    int max_attach_cells = 4;
    int max_drop_cells = 16;
    std::uint64_t max_attach_instances = 16384;
};

// The box of a near cell in metres: every instance of its 256 terrain cells
// lies inside (6.1).
struct NearBox {
    double x0 = 0.0;
    double x1 = 0.0;
    double z0 = 0.0;
    double z1 = 0.0;
    double bot = 0.0;
    double top = 0.0;
};

struct NearGrid {
    int m = 0;                   // cells per axis, N / 16
    std::vector<NearBox> boxes;  // q = j * m + i
};

// 6.1. Throws std::invalid_argument when heights is not n * n long.
[[nodiscard]] NearGrid build_near_grid(const Layout& layout, std::span<const float> heights);

struct NearViewer {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double heading_deg = 0.0;  // 0 north (-Z), 90 east (+X)
};

// 6.1: the distance from the viewer to the nearest point of the box, its
// horizontal part, and whether the cell is in view.
[[nodiscard]] double near_distance(const NearBox& box, const NearViewer& viewer) noexcept;
[[nodiscard]] double near_distance_h(const NearBox& box, const NearViewer& viewer) noexcept;
[[nodiscard]] bool near_in_view(const NearBox& box, const NearViewer& viewer) noexcept;

enum class NearState : std::uint8_t { empty, queued, computing, ready, filled, dropping };

struct NearCounts {
    std::uint64_t filled = 0;     // filled + dropping
    std::uint64_t queued = 0;
    std::uint64_t computing = 0;
    std::uint64_t ready = 0;
    std::uint64_t instances = 0;  // in filled and dropping cells
    std::uint64_t late = 0;       // wanted episodes seen within NEAR_VISIBLE_MAX before they were filled
    [[nodiscard]] std::uint64_t pending() const noexcept { return queued + computing + ready; }
};

class NearCells {
public:
    NearCells(NearGrid grid, NearParams params);

    // 6.2: the state table, for the cells in the square of half-side
    // R + H + 256 around the viewer and every cell not empty.
    void update(const NearViewer& viewer);
    // Queued cells in priority order, up to max_in_flight minus the cells
    // computing; marked computing.
    std::vector<int> start_computing();
    // A computation finished with n instances: computing -> ready, or ->
    // empty and released when abandoned.
    void computed(int q, std::uint64_t n);
    // Ready cells in priority order within the attach budget; marked filled.
    std::vector<int> take_attach();
    // Dropping cells, farthest first, up to max_drop_cells; marked empty.
    std::vector<int> take_drops();
    // Cells whose ready result the scene must free (since the last call).
    std::vector<int> take_released();

    [[nodiscard]] NearCounts counts() const noexcept;
    [[nodiscard]] bool settled() const;

    [[nodiscard]] const NearGrid& grid() const noexcept { return grid_; }
    [[nodiscard]] const NearParams& params() const noexcept { return params_; }
    [[nodiscard]] NearState state(int q) const { return cells_.at(static_cast<std::size_t>(q)).state; }
    [[nodiscard]] bool abandoned(int q) const { return cells_.at(static_cast<std::size_t>(q)).abandoned; }
    [[nodiscard]] std::uint64_t instances(int q) const { return cells_.at(static_cast<std::size_t>(q)).n; }
    // d from the last update (meaningful for cells it visited).
    [[nodiscard]] double distance(int q) const { return cells_.at(static_cast<std::size_t>(q)).d; }
    // The cells not empty, ascending.
    [[nodiscard]] const std::set<int>& active() const noexcept { return active_; }

private:
    struct Cell {
        NearState state = NearState::empty;
        bool abandoned = false;
        bool late_counted = false;
        bool in_view = false;
        double d = 0.0;
        std::uint64_t n = 0;
    };
    void set_state(int q, NearState s);
    // (in_view ? 0 : 1, d, q) ascending.
    [[nodiscard]] bool before(int a, int b) const;

    NearGrid grid_;
    NearParams params_;
    std::vector<Cell> cells_;
    std::set<int> active_;
    std::vector<int> released_;
    std::vector<int> wanted_;  // cells with d <= R at the last update
    std::uint64_t computing_ = 0;
    std::uint64_t late_ = 0;
};

// 12.1: the terrain column x in [N/8, 7N/8) with the most cells of ground
// type 2 or 3 (forest floor, carr) in rows [N/8, 7N/8); ties the smallest x.
[[nodiscard]] int forest_column(const Layout& layout, const GroundCells& cells);

// True for a cell of ground type 2 or 3 (forest floor, carr).
[[nodiscard]] bool is_forest_cell(const GroundCells& cells, int n, int x, int y);

// The start of the forest path (lead's interim ruling on 12.1, 2026-11-27):
// the forest cell of column `column` nearest to row 7N/8, ties the smaller
// row; when the column has none, the forest cell of the map nearest to
// (column, 7N/8) by squared distance, ties the smaller row, then the smaller
// column; when the map has none (or no ground cells), the map centre with
// found = false. The path heads north (heading 0, decreasing y) or south
// (heading 180), whichever has the longer run of consecutive forest cells
// along the start's column counted from the start cell; ties north.
struct ForestStart {
    int x = 0;
    int y = 0;
    bool north = true;
    bool found = false;
};
[[nodiscard]] ForestStart forest_start_cell(const Layout& layout, const GroundCells& cells, int column);

// The forest path's height above the ground, and its floor above the sea
// plane (y = 0): the camera starts at max(ground + 60, 0 + 60).
inline constexpr float FOREST_PATH_ABOVE = 60.0f;
[[nodiscard]] float forest_start_altitude(float ground);

}  // namespace unmapped::terrain
