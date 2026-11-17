// Stage P1-03 climate (docs/specs/p1-03-climate.md). Steps C1 to C7, each
// named with its determinism rule of P1-03 section 7.

#include "worldgen/stages/climate.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/parallel.hpp"
#include "worldgen/core/rng.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/geo/blocks.hpp"
#include "worldgen/geo/noise_schema.hpp"
#include "worldgen/stages/elevation.hpp"
#include "worldgen/stages/hydrology.hpp"

namespace worldgen::stages {

namespace {

using geo::CellIndex;
using geo::Direction;
using geo::Grid;

void check_threads(std::uint32_t threads) {
    if (threads < 1U || threads > MAX_THREADS) {
        fatal("climate: thread count outside [1, 256]");
    }
}

// Metres of a cell centre along one axis: from_int(16 k + 8) (P1-03 5).
Fx cell_centre_m(std::int32_t k) {
    return Fx::from_int(geo::CELL_M * k + geo::CELL_M / 2);
}

// clamp on int64 for a range that the caller has not checked.
std::int64_t clamp_checked(std::int64_t v, std::int64_t lo, std::int64_t hi) {
    if (lo > hi) {
        fatal("climate: clamp with lo > hi");
    }
    return std::clamp(v, lo, hi);
}

template <class T>
db::LayerGrid layer_of(Grid grid, std::vector<T> values) {
    db::LayerGrid out;
    out.width = static_cast<std::uint32_t>(grid.width);
    out.height = static_cast<std::uint32_t>(grid.height);
    out.values = std::move(values);
    return out;
}

// A stored input layer of the catalogue type T and N x N cells; fatal
// otherwise (P1-03 section 2).
template <class T>
const std::vector<T>& input_layer(const StageContext& context, std::string_view name, std::int64_t n,
                                  const char* type_problem, const char* size_problem) {
    const db::LayerGrid* grid = context.find_layer(name);
    if (grid == nullptr) {
        fatal("climate: input precondition: a required layer is missing");
    }
    const std::vector<T>* values = grid->get_if<T>();
    if (values == nullptr) {
        fatal(type_problem);
    }
    if (grid->width != static_cast<std::uint32_t>(n) || grid->height != static_cast<std::uint32_t>(n)) {
        fatal(size_problem);
    }
    return *values;
}

}  // namespace

// --- Tuning (4) ------------------------------------------------------------------------------

ClimateTuning climate_tuning_from(const data::Record& record) {
    ClimateTuning t;
    t.temp_north_c = record.get_fx("temp_north_c");
    t.temp_south_c = record.get_fx("temp_south_c");
    t.lapse_c_per_km = record.get_fx("lapse_c_per_km");
    t.temp_noise = geo::fbm_m_from(record.get("temp_noise"));
    t.temp_noise_amp_c = record.get_fx("temp_noise_amp_c");
    const std::vector<data::Value>& weights = record.get_array("wind_weights");
    if (weights.size() != WIND_CODES) {
        fatal("climate: wind_weights does not have 8 entries");
    }
    for (std::size_t k = 0; k < WIND_CODES; ++k) {
        t.wind_weights[k] = weights[k].as_int();
    }
    t.smooth_radius_m = record.get_int("smooth_radius_m");
    t.rain_base_mm = record.get_int("rain_base_mm");
    t.rain_oro_mm = record.get_int("rain_oro_mm");
    t.moisture_path_mm_km = record.get_int("moisture_path_mm_km");
    t.loss_max = record.get_fx("loss_max");
    t.ocean_recharge_m = record.get_int("ocean_recharge_m");
    t.coast_rain_mm = record.get_int("coast_rain_mm");
    t.coast_rain_reach_m = record.get_int("coast_rain_reach_m");
    t.rain_noise = geo::fbm_m_from(record.get("rain_noise"));
    t.rain_noise_amp = record.get_fx("rain_noise_amp");
    t.rain_min_mm = record.get_int("rain_min_mm");
    return t;
}

ClimateTuning climate_tuning_from(const data::ContentSet& content) {
    const data::Definition* d = content.find(data::WORLDGEN_KIND, data::WORLDGEN_CLIMATE_ID);
    if (d == nullptr) {
        fatal("climate: the content has no definition worldgen.climate");
    }
    return climate_tuning_from(d->record);
}

// --- C1: preconditions (D1) -------------------------------------------------------------------

std::vector<std::int64_t> climate_surface(Grid grid, std::span<const std::int16_t> elevation,
                                          std::span<const std::uint8_t> water,
                                          std::span<const std::uint16_t> water_depth) {
    geo::check_grid_array(grid, elevation.size(), "climate: input precondition: elevation size");
    geo::check_grid_array(grid, water.size(), "climate: input precondition: water size");
    geo::check_grid_array(grid, water_depth.size(), "climate: input precondition: water_depth size");
    const std::uint32_t n = grid.cells();
    // Precondition 2 on every cell, then precondition 3 (P1-03 section 2 order).
    for (CellIndex i = 0; i < n; ++i) {
        if (water[i] > WATER_OCEAN) {
            fatal("climate: input precondition: a water value is outside 0..3");
        }
    }
    std::vector<std::int64_t> s(n, 0);
    for (CellIndex i = 0; i < n; ++i) {
        const std::int64_t v = std::int64_t{elevation[i]} + std::int64_t{water_depth[i]};
        if (water[i] == WATER_OCEAN) {
            if (v != 0) {
                fatal("climate: input precondition: S is not 0 on an ocean cell");
            }
        } else if (v < 1) {
            fatal("climate: input precondition: S is below 1 on a non-ocean cell");
        }
        if (v > SURFACE_MAX_DM) {
            fatal("climate: input precondition: S is above 32767");
        }
        s[i] = v;
    }
    return s;
}

// --- C2: prevailing wind (D9, sequential) ------------------------------------------------------

Direction climate_wind(std::uint64_t seed, std::span<const std::int64_t> weights) {
    if (weights.size() != WIND_CODES) {
        fatal("climate_wind: weights must have 8 entries");
    }
    std::array<std::uint64_t, WIND_CODES> w{};
    for (std::size_t k = 0; k < WIND_CODES; ++k) {
        if (weights[k] < 0) {
            fatal("climate_wind: a weight is negative");
        }
        w[k] = static_cast<std::uint64_t>(weights[k]);
    }
    Rng rng(stream_key(seed, streams::geo_climate_wind));
    const std::size_t k = rng.weighted_index(w);  // one call (P0-01 6.3); fatal on a zero sum
    return static_cast<Direction>(k);
}

// --- C3: temperature (D1, G6 row blocks) ---------------------------------------------------------

std::int16_t climate_temperature_cell(std::int64_t y, std::int64_t height, std::int64_t s_dm, Fx noise,
                                      const ClimateTuning& tuning) {
    if (y < 0 || y >= height || height > std::numeric_limits<std::int32_t>::max()) {
        fatal("climate_temperature_cell: row outside the grid");  // also keeps 2 height in int64
    }
    const Fx v = Fx::from_ratio(2 * y + 1, 2 * height);
    const Fx t_sea = tuning.temp_north_c + mul(tuning.temp_south_c - tuning.temp_north_c, v);
    const Fx lap = div_int(mul_int(tuning.lapse_c_per_km, s_dm), 10000);  // S in dm; 10000 dm = 1 km
    const Fx tn = mul(tuning.temp_noise_amp_c, noise);
    const Fx t = (t_sea - lap) + tn;
    const std::int64_t tenths = clamp_checked(mul_int(t, 10).round_int(), TEMPERATURE_MIN, TEMPERATURE_MAX);
    return static_cast<std::int16_t>(tenths);
}

std::vector<std::int16_t> climate_temperature(std::uint64_t seed, Grid grid, std::span<const std::int64_t> surface,
                                              const ClimateTuning& tuning, std::uint32_t threads) {
    check_threads(threads);
    geo::check_grid_array(grid, surface.size(), "climate_temperature: surface");
    const std::uint64_t key = stream_key(seed, streams::geo_climate_temperature);
    std::vector<std::int16_t> out(grid.cells(), 0);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            const Fx ym = cell_centre_m(y);
            for (std::int32_t x = 0; x < grid.width; ++x) {
                const CellIndex i = grid.index(x, y);
                const Fx nz = geo::fbm_m(key, cell_centre_m(x), ym, tuning.temp_noise);
                out[i] = climate_temperature_cell(y, grid.height, surface[i], nz, tuning);
            }
        }
    });
    return out;
}

// --- C4: smoothed surface (D2, G6 row blocks) ----------------------------------------------------

namespace {

// The overflow margin of a box pass (P1-03 section 7). Every value a pass
// sums is at most VALUE_MAX = 32767 x 2^32: pass 1 sums S <= 32767, and pass 2
// sums hs1.raw, where hs1 is a mean of S <= 32767 rounded by RHA, so hs1.raw
// <= 32767 x 2^32 exactly. A sum of at most BOX_TERMS_MAX such values fits
// int64. The sums a pass forms are: the row prefix over a whole row (W
// terms), each row-window sum (min(W, 2r + 1) terms), and the running column
// sum over at most min(H, 2r + 1) row-window sums (one window: the leaving
// row is dropped before the entering one is added). Hence the guard: W <=
// BOX_TERMS_MAX and min(W, 2r + 1) min(H, 2r + 1) <= BOX_TERMS_MAX, with r the
// pass's own half-width.
constexpr std::int64_t BOX_VALUE_MAX = SURFACE_MAX_DM << 32;
constexpr std::int64_t BOX_TERMS_MAX = std::numeric_limits<std::int64_t>::max() / BOX_VALUE_MAX;  // 65538
static_assert(BOX_TERMS_MAX == 65538);
// The stage stays inside the guard: the largest map side, and the largest
// window of the triangle's passes (h = smooth_radius_m div 16 div 2, schema
// bound): raising either bound past the margin breaks the build.
static_assert(ELEVATION_MAX_N <= BOX_TERMS_MAX);
static_assert((2 * (data::CLIMATE_SMOOTH_RADIUS_MAX_M / geo::CELL_M / 2) + 1) *
                  (2 * (data::CLIMATE_SMOOTH_RADIUS_MAX_M / geo::CELL_M / 2) + 1) <=
              BOX_TERMS_MAX);

// Fatal (prefix "climate: input precondition: ") unless a box pass of
// half-width r_pass on this grid stays inside the margin above.
void check_box_margin(Grid grid, std::int64_t r_pass) {
    const std::int64_t w = grid.width;
    const std::int64_t h = grid.height;
    const std::int64_t span = 2 * std::min<std::int64_t>(r_pass, std::max(w, h)) + 1;  // r clipped first: no overflow
    if (w > BOX_TERMS_MAX || std::min(w, span) * std::min(h, span) > BOX_TERMS_MAX) {
        fatal("climate: input precondition: the grid or the smoothing window is too large for exact int64 sums");
    }
}

// The S bound behind C4's overflow argument (P1-03 section 2 precondition 3,
// section 7), checked at entry of both C4 seams, with the window margin of
// the pass half-width r_pass. Order of P1-03 section 10 (entry guards):
// r_cells >= 0; every S in [0, 32767]; W <= MAX; one window <= MAX.
void check_surface_range(Grid grid, std::span<const std::int64_t> surface, std::int64_t r_cells, std::int64_t r_pass,
                         const char* what) {
    geo::check_grid_array(grid, surface.size(), what);
    if (r_cells < 0) {
        fatal("climate: input precondition: negative smoothing radius");
    }
    for (const std::int64_t v : surface) {
        if (v < 0 || v > SURFACE_MAX_DM) {
            fatal("climate: input precondition: S is outside [0, 32767]");
        }
    }
    check_box_margin(grid, r_pass);
}

// One box pass of half-width r_cells (P1-03 C4). raw_input false: the
// values are integers S and the mean is from_ratio(sum, c_w) (pass 1);
// raw_input true: the values are Fx raw values and the mean is
// div_int(from_raw(sum), c_w) (pass 2). Every sum is exact int64 when the
// caller has passed check_box_margin for this half-width (see there); the
// running column sum drops the leaving row before it adds the entering one,
// so it never holds more than the 2r + 1 rows of a window.
std::vector<Fx> box_pass(Grid grid, std::span<const std::int64_t> surface, std::int64_t r_cells,
                         std::uint32_t threads, bool raw_input) {
    check_threads(threads);
    const std::int64_t w = grid.width;
    const std::int64_t h = grid.height;
    const std::int64_t r = std::min<std::int64_t>(r_cells, std::max(w, h));  // a larger r clips to the same window
    const std::uint32_t n = grid.cells();

    // Row pass: rowsum[y][x] = sum of S over x' in [x - r, x + r] clipped.
    std::vector<std::int64_t> rowsum(n, 0);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        std::vector<std::int64_t> prefix(static_cast<std::size_t>(w) + 1U, 0);
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
            for (std::int64_t x = 0; x < w; ++x) {
                const auto ux = static_cast<std::size_t>(x);
                prefix[ux + 1U] = prefix[ux] + surface[row + ux];
            }
            for (std::int64_t x = 0; x < w; ++x) {
                const auto lo = static_cast<std::size_t>(std::max<std::int64_t>(0, x - r));
                const auto hi = static_cast<std::size_t>(std::min<std::int64_t>(w, x + r + 1));
                rowsum[row + static_cast<std::size_t>(x)] = prefix[hi] - prefix[lo];
            }
        }
    });

    // Column pass: a running sum of rowsum over [y - r, y + r] clipped,
    // started afresh in each block (exact integers: any split gives the same
    // sums).
    std::vector<Fx> hs(n);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        std::vector<std::int64_t> acc(static_cast<std::size_t>(w), 0);
        const std::int64_t y0 = block.y_begin;
        for (std::int64_t yy = std::max<std::int64_t>(0, y0 - r); yy <= std::min<std::int64_t>(h - 1, y0 + r); ++yy) {
            const std::size_t row = static_cast<std::size_t>(yy) * static_cast<std::size_t>(w);
            for (std::size_t x = 0; x < acc.size(); ++x) {
                acc[x] += rowsum[row + x];
            }
        }
        for (std::int64_t y = y0; y < block.y_end; ++y) {
            if (y > y0) {
                // Drop the leaving row first, then add the entering one:
                // the same exact sum, and never more than one window held.
                const std::int64_t drop = y - r - 1;
                const std::int64_t add = y + r;
                if (drop >= 0) {
                    const std::size_t row = static_cast<std::size_t>(drop) * static_cast<std::size_t>(w);
                    for (std::size_t x = 0; x < acc.size(); ++x) {
                        acc[x] -= rowsum[row + x];
                    }
                }
                if (add < h) {
                    const std::size_t row = static_cast<std::size_t>(add) * static_cast<std::size_t>(w);
                    for (std::size_t x = 0; x < acc.size(); ++x) {
                        acc[x] += rowsum[row + x];
                    }
                }
            }
            const std::int64_t rows = std::min<std::int64_t>(h - 1, y + r) - std::max<std::int64_t>(0, y - r) + 1;
            const std::size_t out_row = static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
            for (std::int64_t x = 0; x < w; ++x) {
                const std::int64_t cols = std::min<std::int64_t>(w - 1, x + r) - std::max<std::int64_t>(0, x - r) + 1;
                const std::int64_t sum = acc[static_cast<std::size_t>(x)];
                hs[out_row + static_cast<std::size_t>(x)] =
                    raw_input ? div_int(Fx::from_raw(sum), rows * cols) : Fx::from_ratio(sum, rows * cols);
            }
        }
    });
    return hs;
}

}  // namespace

std::vector<Fx> climate_box_mean(Grid grid, std::span<const std::int64_t> surface, std::int64_t r_cells,
                                 std::uint32_t threads) {
    check_surface_range(grid, surface, r_cells, r_cells, "climate_box_mean: surface");
    return box_pass(grid, surface, r_cells, threads, false);
}

std::vector<Fx> climate_triangle_mean(Grid grid, std::span<const std::int64_t> surface, std::int64_t r_cells,
                                      std::uint32_t threads) {
    check_surface_range(grid, surface, r_cells, r_cells / 2, "climate_triangle_mean: surface");
    const std::int64_t h = r_cells / 2;
    std::vector<Fx> hs1 = box_pass(grid, surface, h, threads, false);  // pass 1 finishes before pass 2
    std::vector<std::int64_t> raw(hs1.size());
    for (std::size_t i = 0; i < hs1.size(); ++i) {
        raw[i] = hs1[i].raw();
    }
    hs1.clear();
    hs1.shrink_to_fit();
    return box_pass(grid, raw, h, threads, true);
}

// --- C5: moisture sweep (D4: lines, or a raster for diagonal winds) ------------------------------------------------------------

ClimateSweep climate_sweep(Grid grid, std::span<const std::uint8_t> water, std::span<const Fx> hs, Direction wind,
                           const ClimateTuning& tuning) {
    geo::check_grid_array(grid, water.size(), "climate_sweep: water");
    geo::check_grid_array(grid, hs.size(), "climate_sweep: hs");
    if (wind > 7U) {
        fatal("climate_sweep: wind is not a D8 code");
    }
    const std::int32_t dx = geo::D8_DX[wind];
    const std::int32_t dy = geo::D8_DY[wind];
    const bool diagonal = geo::is_diagonal(wind);
    const Fx step_m = diagonal ? mul_int(Fx::SQRT2, 16) : Fx::from_int(16);
    const Fx step_dm = diagonal ? mul_int(Fx::SQRT2, 160) : Fx::from_int(160);
    const Fx rec = min(Fx::ONE, div(step_m, Fx::from_int(tuning.ocean_recharge_m)));  // once per world
    const Fx base = Fx::from_int(tuning.rain_base_mm);
    const std::int64_t path_div = 1000 * tuning.moisture_path_mm_km;

    const std::uint32_t n = grid.cells();
    ClimateSweep out;
    out.q.assign(n, Fx{});
    out.p.assign(n, Fx{});

    // A tap: off the map, saturated air at sea level (q = ONE, hs = 0).
    struct Tap {
        Fx q;
        Fx hs;
    };
    const auto tap = [&](std::int32_t x, std::int32_t y) {
        if (!grid.contains(x, y)) {
            return Tap{Fx::ONE, Fx{}};
        }
        const CellIndex i = grid.index(x, y);
        return Tap{out.q[i], hs[i]};
    };
    const auto cell = [&](std::int32_t x, std::int32_t y, const Tap& a, const Tap& b, const Tap& c) {
        const CellIndex i = grid.index(x, y);
        const Fx q_up = div_int((a.q + mul_int(b.q, 2)) + c.q, 4);
        const Fx hs_up = div_int((a.hs + mul_int(b.hs, 2)) + c.hs, 4);
        if (water[i] == WATER_OCEAN) {
            out.q[i] = q_up + mul(Fx::ONE - q_up, rec);
            out.p[i] = mul(q_up, base);
            return;
        }
        const Fx rise = max(Fx{}, hs[i] - hs_up);  // dm
        const Fx g = div(rise, step_dm);           // upslope gradient along the wind
        const Fx p = mul(q_up, base + mul_int(g, tuning.rain_oro_mm));
        const Fx lam = min(tuning.loss_max, div_int(mul(p, step_m), path_div));
        out.p[i] = p;
        out.q[i] = q_up - mul(q_up, lam);
    };

    if (diagonal) {
        // Raster order: rows in the order of dy, cells in the order of dx;
        // taps across the wind: a = (x - dx, y), the previous cell of the
        // row, b = (x - dx, y - dy) and c = (x, y - dy) in the previous row.
        for (std::int32_t ky = 0; ky < grid.height; ++ky) {
            const std::int32_t y = dy == 1 ? ky : grid.height - 1 - ky;
            for (std::int32_t kx = 0; kx < grid.width; ++kx) {
                const std::int32_t x = dx == 1 ? kx : grid.width - 1 - kx;
                cell(x, y, tap(x - dx, y), tap(x - dx, y - dy), tap(x, y - dy));
            }
        }
    } else if (dx != 0) {
        // Lines are columns, x ascending for dx = 1 and descending for dx = -1.
        for (std::int32_t k = 0; k < grid.width; ++k) {
            const std::int32_t x = dx == 1 ? k : grid.width - 1 - k;
            const std::int32_t ux = x - dx;  // dy = 0 here
            for (std::int32_t y = 0; y < grid.height; ++y) {
                cell(x, y, tap(ux, y - 1), tap(ux, y), tap(ux, y + 1));
            }
        }
    } else {
        // Lines are rows, y ascending for dy = 1 and descending for dy = -1.
        for (std::int32_t k = 0; k < grid.height; ++k) {
            const std::int32_t y = dy == 1 ? k : grid.height - 1 - k;
            const std::int32_t uy = y - dy;
            for (std::int32_t x = 0; x < grid.width; ++x) {
                cell(x, y, tap(x - 1, uy), tap(x, uy), tap(x + 1, uy));
            }
        }
    }
    return out;
}

// --- C6: rainfall (D1, G6 row blocks) ----------------------------------------------------------------

std::uint16_t climate_rainfall_cell(Fx p, std::int64_t coast_dist, Fx rn, const ClimateTuning& tuning) {
    const Fx f = max(Fx{}, Fx::ONE + mul(tuning.rain_noise_amp, rn));
    const Fx pr = mul(p, f);
    const std::int64_t d = std::max<std::int64_t>(0, coast_dist);  // metres; ocean counts as 0
    const std::int64_t reach = tuning.coast_rain_reach_m;
    const Fx cb = d >= reach ? Fx{} : Fx::from_ratio(tuning.coast_rain_mm * (reach - d), reach);
    const std::int64_t mm = clamp_checked((pr + cb).round_int(), tuning.rain_min_mm, RAINFALL_MAX);
    return static_cast<std::uint16_t>(mm);
}

std::vector<std::uint16_t> climate_rainfall(std::uint64_t seed, Grid grid, std::span<const Fx> p,
                                            std::span<const std::int16_t> coast_dist, const ClimateTuning& tuning,
                                            std::uint32_t threads) {
    check_threads(threads);
    geo::check_grid_array(grid, p.size(), "climate_rainfall: p");
    geo::check_grid_array(grid, coast_dist.size(), "climate_rainfall: coast_dist");
    const std::uint64_t key = stream_key(seed, streams::geo_climate_rainfall);
    std::vector<std::uint16_t> out(grid.cells(), 0U);
    geo::for_each_row_block(grid.height, threads, [&](const geo::RowBlock& block) {
        for (std::int32_t y = block.y_begin; y < block.y_end; ++y) {
            const Fx ym = cell_centre_m(y);
            for (std::int32_t x = 0; x < grid.width; ++x) {
                const CellIndex i = grid.index(x, y);
                const Fx rn = geo::fbm_m(key, cell_centre_m(x), ym, tuning.rain_noise);
                out[i] = climate_rainfall_cell(p[i], coast_dist[i], rn, tuning);
            }
        }
    });
    return out;
}

// --- The whole stage ------------------------------------------------------------------------------------

ClimateLayers climate_layers(Grid grid, std::span<const std::int16_t> elevation, std::span<const std::uint8_t> water,
                             std::span<const std::uint16_t> water_depth, std::span<const std::int16_t> coast_dist,
                             const ClimateTuning& tuning, std::uint64_t seed, std::uint32_t threads) {
    check_threads(threads);
    geo::check_grid_array(grid, coast_dist.size(), "climate: input precondition: coast_dist size");
    const std::vector<std::int64_t> s = climate_surface(grid, elevation, water, water_depth);  // C1
    const Direction wind = climate_wind(seed, tuning.wind_weights);                             // C2
    ClimateLayers out;
    out.temperature = climate_temperature(seed, grid, s, tuning, threads);  // C3
    std::vector<Fx> hs = climate_triangle_mean(grid, s, tuning.smooth_radius_m / geo::CELL_M, threads);  // C4
    ClimateSweep sweep = climate_sweep(grid, water, hs, wind, tuning);                                // C5
    hs.clear();
    hs.shrink_to_fit();
    sweep.q.clear();
    sweep.q.shrink_to_fit();
    out.rainfall = climate_rainfall(seed, grid, sweep.p, coast_dist, tuning, threads);  // C6
    return out;
}

void run_climate(StageContext& context) {
    const ClimateTuning tuning = climate_tuning_from(context.content());
    const std::int64_t n = context.map_size();
    if (n < ELEVATION_MIN_N || n > ELEVATION_MAX_N) {
        fatal("climate: map size outside [16, 2048]");
    }
    // C1: the four input layers with their catalogue types, N x N.
    const auto& e = input_layer<std::int16_t>(context, ELEVATION_LAYER, n,
                                              "climate: input precondition: the elevation layer is not i16",
                                              "climate: input precondition: the elevation layer is not N x N");
    const auto& water = input_layer<std::uint8_t>(context, WATER_LAYER, n,
                                                  "climate: input precondition: the water layer is not u8",
                                                  "climate: input precondition: the water layer is not N x N");
    const auto& depth = input_layer<std::uint16_t>(context, WATER_DEPTH_LAYER, n,
                                                   "climate: input precondition: the water_depth layer is not u16",
                                                   "climate: input precondition: the water_depth layer is not N x N");
    const auto& coast = input_layer<std::int16_t>(context, COAST_DIST_LAYER, n,
                                                  "climate: input precondition: the coast_dist layer is not i16",
                                                  "climate: input precondition: the coast_dist layer is not N x N");
    const Grid grid = geo::square_grid(static_cast<std::int32_t>(n));
    ClimateLayers layers = climate_layers(grid, e, water, depth, coast, tuning, context.seed(), context.threads());
    // C7: one transaction (the pipeline's), temperature then rainfall.
    context.write_layer(TEMPERATURE_LAYER, layer_of(grid, std::move(layers.temperature)));
    context.write_layer(RAINFALL_LAYER, layer_of(grid, std::move(layers.rainfall)));
}

}  // namespace worldgen::stages
