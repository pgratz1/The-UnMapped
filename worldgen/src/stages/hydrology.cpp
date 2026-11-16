// Stage P1-02 hydrology (docs/specs/p1-02-hydrology.md). Steps H1 to H9,
// each named with its determinism rule of P1-02 section 7.

#include "worldgen/stages/hydrology.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/math.hpp"
#include "worldgen/core/parallel.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/geo/blocks.hpp"
#include "worldgen/geo/noise_schema.hpp"
#include "worldgen/stages/elevation.hpp"

namespace worldgen::stages {

namespace {

using geo::CellIndex;
using geo::Direction;
using geo::Grid;

void check_threads(std::uint32_t threads) {
    if (threads < 1U || threads > MAX_THREADS) {
        fatal("hydrology: thread count outside [1, 256]");
    }
}

// The cells of one G6 block: indices [first, last).
struct BlockCells {
    CellIndex first;
    CellIndex last;
};

BlockCells block_cells(Grid grid, const geo::RowBlock& block) {
    return BlockCells{grid.index(0, block.y_begin),
                      grid.index(0, block.y_end - 1) + static_cast<CellIndex>(grid.width)};
}

// One entry of the H2 flood queue.
struct FloodEntry {
    std::int64_t key;
    std::uint64_t seq;
    CellIndex cell;
};

// The heap comparison of the H2 flood: true when a pops after b, so the
// std heap functions keep the smallest (key, seq) on top. (key, seq) pairs
// are unique (seq is the global push counter), so this is a strict total
// order and the pop order is defined by it alone, whatever the heap layout.
bool pops_after(const FloodEntry& a, const FloodEntry& b) noexcept {
    return a.key != b.key ? a.key > b.key : a.seq > b.seq;
}

// H2 step 2 (D6): hydrology's own routing flood (P1-02 H2; not the shared
// G3). Pushes neighbour m of the popped cell c with L[m] = max(e[m], L[c])
// and key L[m] x 2^16 + t[m]; pops the smallest (key, seq). Keys are not
// monotone over the pops, so a binary heap is used (G3's radix heap would
// be wrong here). Writes L (decimetres), parent and order.
void tie_flood(Grid grid, std::span<const std::int16_t> e, std::span<const std::uint16_t> tie,
               std::span<const std::uint8_t> ocean, HydrologyNetwork& net) {
    const std::uint32_t n = grid.cells();
    net.level_dm.assign(n, 0);
    net.parent.assign(n, geo::NO_DIRECTION);
    net.order.clear();
    net.order.reserve(n);
    std::vector<std::uint8_t> visited(n, 0U);
    std::vector<FloodEntry> heap;
    heap.reserve(n);
    std::uint64_t seq = 0;
    for (CellIndex i = 0; i < n; ++i) {  // ocean seeds in index order, key 0
        if (ocean[i] != 0U) {
            visited[i] = 1U;
            heap.push_back(FloodEntry{0, seq++, i});
        }
    }
    // determinism-lint: allow(heap-order) -- pops_after orders by (key, seq), seq the unique global push counter: a strict total order (P1-00 D6, P1-02 H2)
    std::make_heap(heap.begin(), heap.end(), pops_after);
    while (!heap.empty()) {
        // determinism-lint: allow(heap-order) -- pops_after orders by (key, seq), seq the unique global push counter: a strict total order (P1-00 D6, P1-02 H2)
        std::pop_heap(heap.begin(), heap.end(), pops_after);
        const CellIndex c = heap.back().cell;
        heap.pop_back();
        net.order.push_back(c);
        const std::int64_t lc = net.level_dm[c];
        for (const Direction code : geo::NEIGHBOURS_8) {
            CellIndex m = 0;
            if (!geo::neighbour(grid, c, code, m) || visited[m] != 0U) {
                continue;
            }
            visited[m] = 1U;  // pushed once, by the first popped neighbour
            const std::int64_t lm = std::max<std::int64_t>(e[m], lc);
            net.level_dm[m] = lm;
            net.parent[m] = static_cast<Direction>((code + 4U) % 8U);
            const std::int64_t t = tie.empty() ? 0 : std::int64_t{tie[m]};
            heap.push_back(FloodEntry{lm * (std::int64_t{1} << FLAT_TIE_BITS) + t, seq++, m});
            // determinism-lint: allow(heap-order) -- pops_after orders by (key, seq), seq the unique global push counter: a strict total order (P1-00 D6, P1-02 H2)
            std::push_heap(heap.begin(), heap.end(), pops_after);
        }
    }
    if (net.order.size() != n) {
        fatal("hydrology: the flood did not reach every cell");
    }
}

// The neighbour of c in direction code; fatal if it does not exist (a
// receiver must point inside the grid).
CellIndex downstream(Grid grid, CellIndex c, Direction code) {
    CellIndex m = 0;
    if (code > 7U || !geo::neighbour(grid, c, code, m)) {
        fatal("hydrology: a receiver does not point to a cell of the grid");
    }
    return m;
}

// RHA(d x 16 / 5) = (32 d + 5) div 10, saturated at 32767 (H7). d >= 0.
std::int64_t chamfer_metres(std::int64_t d) {
    return std::min(COAST_DIST_MAX, (32 * d + 5) / 10);
}

template <class T>
db::LayerGrid layer_of(Grid grid, std::vector<T> values) {
    db::LayerGrid out;
    out.width = static_cast<std::uint32_t>(grid.width);
    out.height = static_cast<std::uint32_t>(grid.height);
    out.values = std::move(values);
    return out;
}

}  // namespace

// --- Tuning (4) ------------------------------------------------------------------------------

HydrologyTuning hydrology_tuning_from(const data::Record& record) {
    HydrologyTuning t;
    t.river_min_area_m2 = record.get_int("river_min_area_m2");
    t.flat_noise = geo::fbm_m_from(record.get("flat_noise"));
    return t;
}

HydrologyTuning hydrology_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::WORLDGEN_KIND, data::WORLDGEN_HYDROLOGY_ID);
    if (d == nullptr) {
        fatal("hydrology: the content has no definition worldgen.hydrology");
    }
    return hydrology_tuning_from(d->record);
}

std::int64_t river_threshold_cells(std::int64_t river_min_area_m2) {
    if (river_min_area_m2 < RIVER_MIN_AREA_LO || river_min_area_m2 > RIVER_MIN_AREA_HI) {
        fatal("hydrology: river_min_area_m2 outside [256, 1000000000]");
    }
    return (river_min_area_m2 + 255) / 256;
}

std::uint8_t river_class(std::int64_t a, std::int64_t t) {
    if (t < 1 || a < 0) {
        fatal("river_class: threshold below 1 or negative accumulation");
    }
    if (a < t) {
        return 0U;
    }
    const std::int32_t k = bit_length(static_cast<std::uint64_t>(a / t));
    return static_cast<std::uint8_t>(std::min(RIVER_CLASS_CAP, k));
}

// --- H1: preconditions (D5) -------------------------------------------------------------------

void check_hydrology_preconditions(Grid grid, std::span<const std::int16_t> e) {
    geo::check_grid_array(grid, e.size(), "hydrology: elevation precondition: grid size");
    const std::uint32_t n = grid.cells();
    std::uint32_t ocean_cells = 0;
    for (CellIndex i = 0; i < n; ++i) {
        if (e[i] == 0) {
            fatal("hydrology: elevation precondition: a cell is 0");
        }
        if (e[i] < 0) {
            ++ocean_cells;
        }
    }
    if (ocean_cells == 0U) {
        fatal("hydrology: elevation precondition: no ocean cell");
    }
    // 4-connected FIFO flood from the edge ocean cells, seeded in index order.
    std::vector<std::uint8_t> reached(n, 0U);
    std::vector<CellIndex> queue;
    queue.reserve(ocean_cells);
    for (CellIndex i = 0; i < n; ++i) {
        const std::int32_t x = grid.x_of(i);
        const std::int32_t y = grid.y_of(i);
        const bool edge = x == 0 || y == 0 || x == grid.width - 1 || y == grid.height - 1;
        if (edge && e[i] < 0) {
            reached[i] = 1U;
            queue.push_back(i);
        }
    }
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const CellIndex c = queue[head];
        for (const Direction code : geo::NEIGHBOURS_4) {
            CellIndex m = 0;
            if (geo::neighbour(grid, c, code, m) && reached[m] == 0U && e[m] < 0) {
                reached[m] = 1U;
                queue.push_back(m);
            }
        }
    }
    if (queue.size() != ocean_cells) {
        fatal("hydrology: elevation precondition: the ocean is not one 4-connected region touching the map edge");
    }
}

// --- H2 step 1: tie values (D1) -----------------------------------------------------------------

std::vector<std::uint16_t> flat_tie(std::uint64_t seed, Grid grid, std::span<const std::uint8_t> ocean,
                                    const HydrologyTuning& tuning, std::uint32_t threads) {
    check_threads(threads);
    geo::check_grid_array(grid, ocean.size(), "flat_tie: ocean");
    const std::uint64_t key = stream_key(seed, streams::geo_hydrology_flat);
    std::vector<std::uint16_t> tie(grid.cells(), 0U);
    // Per-cell function in G6 row blocks: each block writes only its cells.
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            const Fx ym = Fx::from_int(geo::CELL_M * y + geo::CELL_M / 2);
            for (std::int32_t x = 0; x < grid.width; ++x) {
                const CellIndex i = grid.index(x, y);
                if (ocean[i] != 0U) {
                    continue;  // t = 0 on ocean
                }
                const Fx xm = Fx::from_int(geo::CELL_M * x + geo::CELL_M / 2);
                const Fx nz = geo::fbm_m(key, xm, ym, tuning.flat_noise);  // in [-ONE, ONE]
                // nz.raw + 2^32 in [0, 2^33]; >> 17 gives [0, 65536].
                const std::int64_t t = std::min(FLAT_TIE_MAX, (nz.raw() + (std::int64_t{1} << 32)) >> 17);
                tie[i] = static_cast<std::uint16_t>(t);
            }
        }
    });
    return tie;
}

// --- H2 to H5 ----------------------------------------------------------------------------------

HydrologyNetwork hydrology_network(Grid grid, std::span<const std::int16_t> e, std::span<const std::uint16_t> tie,
                                   std::uint32_t threads) {
    check_threads(threads);
    check_hydrology_preconditions(grid, e);  // H1
    const std::uint32_t n = grid.cells();
    if (!tie.empty()) {
        geo::check_grid_array(grid, tie.size(), "hydrology: tie values");
    }
    HydrologyNetwork net;

    // H2 step 2 (D6): the flood ordered by (L x 2^16 + t, seq). Land L is
    // in [1, 32767] (H1), so land keys are in [2^16, 2^31).
    net.ocean.assign(n, 0U);
    for (CellIndex i = 0; i < n; ++i) {
        net.ocean[i] = e[i] < 0 ? 1U : 0U;
    }
    tie_flood(grid, e, tie, net.ocean, net);
    const std::vector<std::int64_t>& level = net.level_dm;

    // H3 (D5, G2): lake cells are land cells with L > e; components with
    // 8-connectivity and the equivalence "equal L".
    {
        std::vector<std::uint8_t> lake_mask(n, 0U);
        for (CellIndex i = 0; i < n; ++i) {
            lake_mask[i] = (net.ocean[i] == 0U && level[i] > e[i]) ? 1U : 0U;
        }
        net.lakes = geo::label_components(grid, lake_mask, geo::Connectivity::eight, level);
    }
    const std::uint32_t lake_count = net.lakes.count;
    net.lake_level.assign(lake_count, 0);
    for (std::uint32_t k = 1; k <= lake_count; ++k) {
        net.lake_level[k - 1U] = level[net.lakes.order[net.lakes.start[k - 1U]]];
    }

    // H4 step 1 (D1, G6 row blocks): G4 receivers on the decimetre levels,
    // the flood parent where no neighbour is strictly lower.
    net.g4 = geo::receivers(grid, level, net.parent, net.ocean, threads);
    net.rcv = net.g4;

    // H4 step 2 (D5): the outlet of each lake is its first cell in `order`.
    constexpr CellIndex NONE = std::numeric_limits<CellIndex>::max();
    net.lake_outlet.assign(lake_count, NONE);
    {
        std::uint32_t found = 0;
        for (const CellIndex c : net.order) {
            if (found == lake_count) {
                break;
            }
            const std::uint32_t k = net.lakes.label[c];
            if (k != 0U && net.lake_outlet[k - 1U] == NONE) {
                net.lake_outlet[k - 1U] = c;
                ++found;
            }
        }
        if (found != lake_count) {
            fatal("hydrology: a lake has no cell in the flood order");
        }
    }
    {
        std::vector<std::uint8_t> visited(n, 0U);
        std::vector<CellIndex> bfs_order;
        for (std::uint32_t k = 1; k <= lake_count; ++k) {
            const CellIndex outlet = net.lake_outlet[k - 1U];
            const Direction p = net.parent[outlet];
            // The outlet's flood parent is a rim cell outside the lake (H3, H4).
            const CellIndex rim = downstream(grid, outlet, p);
            if (net.lakes.label[rim] == k || net.ocean[rim] != 0U || level[rim] != net.lake_level[k - 1U]) {
                fatal("hydrology: a lake outlet's flood parent is not a rim cell at the lake level");
            }
            net.rcv[outlet] = p;
            bfs_order.clear();
            geo::bfs_region(grid, net.lakes.label, outlet, geo::Connectivity::eight, net.rcv, visited, bfs_order);
            if (bfs_order.size() != net.lakes.size(k)) {
                fatal("hydrology: the outlet search did not reach every cell of its lake");
            }
        }
    }

    // H5 (D4): accumulation by Kahn's method (order-independent values).
    net.accum = geo::accumulate_topological(grid, net.rcv, net.ocean);
    return net;
}

// --- H7: coast distance (D4) ----------------------------------------------------------------------

std::vector<std::int16_t> coast_distance(Grid grid, std::span<const std::uint8_t> ocean) {
    geo::check_grid_array(grid, ocean.size(), "coast_distance: ocean");
    const std::uint32_t n = grid.cells();
    std::vector<std::uint8_t> land(n, 0U);
    for (CellIndex i = 0; i < n; ++i) {
        land[i] = ocean[i] != 0U ? 0U : 1U;
    }
    const std::vector<std::int64_t> dl = chamfer_distance(grid, ocean);
    const std::vector<std::int64_t> d_o = chamfer_distance(grid, land);
    std::vector<std::int16_t> out(n, 0);
    for (CellIndex i = 0; i < n; ++i) {
        const std::int64_t m = ocean[i] != 0U ? -chamfer_metres(d_o[i]) : chamfer_metres(dl[i]);
        out[i] = static_cast<std::int16_t>(m);
    }
    return out;
}

// --- The whole stage ----------------------------------------------------------------------------

HydrologyLayers hydrology_layers(Grid grid, std::span<const std::int16_t> e, std::span<const std::uint16_t> tie,
                                 const HydrologyTuning& tuning, std::uint32_t threads) {
    check_threads(threads);
    const std::int64_t t = river_threshold_cells(tuning.river_min_area_m2);
    HydrologyNetwork net = hydrology_network(grid, e, tie, threads);  // H1 to H5
    const std::uint32_t n = grid.cells();

    HydrologyLayers out;
    out.water.assign(n, 0U);
    out.water_depth.assign(n, 0U);
    out.flow_dir.assign(n, 0U);
    out.flow_accum.assign(n, 0U);
    out.river.assign(n, 0U);
    out.lake.assign(n, 0U);

    // H6 and H8 (D1, G6 row blocks): each block writes only its own cells.
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        const BlockCells cells = block_cells(grid, block);
        for (CellIndex i = cells.first; i < cells.last; ++i) {
            const std::int64_t ei = e[i];
            if (net.ocean[i] != 0U) {
                out.water[i] = WATER_OCEAN;
                out.water_depth[i] = static_cast<std::uint16_t>(-ei);  // -e <= 32768
                out.flow_dir[i] = geo::NO_DIRECTION;
                continue;
            }
            out.flow_dir[i] = net.rcv[i];
            out.flow_accum[i] = net.accum[i];
            const std::uint32_t k = net.lakes.label[i];
            if (k != 0U) {
                out.water[i] = WATER_LAKE;
                out.water_depth[i] = static_cast<std::uint16_t>(net.level_dm[i] - ei);  // L - e < 32767
                out.lake[i] = k;
                continue;
            }
            const std::uint8_t cls = river_class(net.accum[i], t);
            if (cls != 0U) {
                out.water[i] = WATER_RIVER;
                out.river[i] = cls;
            }
        }
    });

    // H7.
    out.coast_dist = coast_distance(grid, net.ocean);
    return out;
}

void run_hydrology(StageContext& context) {
    const HydrologyTuning tuning = hydrology_tuning_from(context.content());
    const db::LayerGrid& stored = context.layer(ELEVATION_LAYER);
    const std::int64_t map_size = context.map_size();
    if (map_size < ELEVATION_MIN_N || map_size > ELEVATION_MAX_N) {
        fatal("hydrology: map size outside [16, 2048]");
    }
    const std::vector<std::int16_t>* e = stored.get_if<std::int16_t>();
    if (e == nullptr) {
        fatal("hydrology: elevation precondition: the elevation layer is not i16");
    }
    if (stored.width != static_cast<std::uint32_t>(map_size) || stored.height != static_cast<std::uint32_t>(map_size)) {
        fatal("hydrology: elevation precondition: the elevation layer is not N x N with N = param.map_size");
    }
    const auto n = static_cast<std::int32_t>(map_size);
    const Grid grid = geo::square_grid(n);
    // H2 step 1: the tie values need only the ocean mask O = {e < 0}.
    std::vector<std::uint8_t> ocean(grid.cells(), 0U);
    for (CellIndex i = 0; i < grid.cells(); ++i) {
        ocean[i] = (*e)[i] < 0 ? 1U : 0U;
    }
    const std::vector<std::uint16_t> tie = flat_tie(context.seed(), grid, ocean, tuning, context.threads());
    HydrologyLayers layers = hydrology_layers(grid, *e, tie, tuning, context.threads());
    // H9: one transaction (the pipeline's), in the order of section 3.
    context.write_layer(WATER_LAYER, layer_of(grid, std::move(layers.water)));
    context.write_layer(WATER_DEPTH_LAYER, layer_of(grid, std::move(layers.water_depth)));
    context.write_layer(FLOW_DIR_LAYER, layer_of(grid, std::move(layers.flow_dir)));
    context.write_layer(FLOW_ACCUM_LAYER, layer_of(grid, std::move(layers.flow_accum)));
    context.write_layer(RIVER_LAYER, layer_of(grid, std::move(layers.river)));
    context.write_layer(LAKE_LAYER, layer_of(grid, std::move(layers.lake)));
    context.write_layer(COAST_DIST_LAYER, layer_of(grid, std::move(layers.coast_dist)));
}

}  // namespace worldgen::stages
