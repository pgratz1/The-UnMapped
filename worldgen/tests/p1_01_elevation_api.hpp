#pragma once

// The one place where the P1-01 elevation tests touch the stage's API
// (worldgen/stages/elevation.hpp).
//
// The tests were written from the spec before the implementation existed
// (test-writer rule), against the test-owned functions below, which take and
// return plain integer arrays and raw Fx values. This file maps them onto
// the stage's seams (P1-01 section 10). Tests that check one of the stage's
// small helpers (land_profile, mountain_mask, ocean_depth, fill_cost, ...)
// call worldgen::stages directly.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/stages/elevation.hpp"

namespace elev_api {

using Tuning = worldgen::stages::ElevationTuning;
using worldgen::geo::Grid;

// The hydrology fixture of P1-02 section 4 (needed beside elevation.toml
// when the `worldgen` kind's id set includes `hydrology`, P1-00 6.1).
inline constexpr std::string_view HYDROLOGY_TOML = "[worldgen.hydrology]\nriver_min_area_m2 = 125000\nflat_noise = { wavelength_m = 400, octaves = 1, gain = \"0.5\" }\n";

// The result of loading a data root that holds the given elevation.toml
// text, through the production registry (P0-03, P1-00 6.1).
struct TuningLoad {
    std::optional<worldgen::data::ContentSet> content;
    std::vector<worldgen::data::LoadError> errors;
    [[nodiscard]] bool ok() const noexcept { return content.has_value(); }
};

inline TuningLoad load_tuning_text(std::string_view elevation_toml) {
    namespace wd = worldgen::data;
    const wd::KindRegistry registry = wd::builtin_kinds();
    wd::MemoryDirectory dir{"worldgen", {}};
    dir.entries.push_back(wd::MemoryEntry{"elevation.toml", std::string(elevation_toml), false});
    const wd::Kind* kind = registry.find("worldgen");
    if (kind != nullptr && kind->schema_for("hydrology") != nullptr) {
        dir.entries.push_back(wd::MemoryEntry{"hydrology.toml", std::string(HYDROLOGY_TOML), false});
    }
    wd::MemoryDataRoot root;
    root.directories.push_back(dir);
    wd::LoadResult<wd::ContentSet> r = wd::load_content_from_memory(registry, root);
    TuningLoad out;
    out.errors = r.errors;
    if (r.ok()) {
        out.content = std::move(*r.value);
    }
    return out;
}

// The stage's tuning from loaded content.
inline Tuning tuning_from(const worldgen::data::ContentSet& content) {
    return worldgen::stages::elevation_tuning_from(content);
}

// E1 to E3 (seam elevation_fields).
struct Fields {
    std::int32_t n = 0;
    std::vector<std::int64_t> c, nb, ns;                         // raw Fx per cell
    std::int64_t c_sea = 0, w_b = 0, s_land = 0, cx = 0, cy = 0;  // raw
    worldgen::stages::ElevationFields engine;                    // the stage's own result, passed on
};

inline Fields fields(std::uint64_t seed, std::int32_t n, const Tuning& t, std::uint32_t threads) {
    worldgen::stages::ElevationFields f = worldgen::stages::elevation_fields(seed, n, t, threads);
    Fields out;
    out.n = n;
    out.c = f.c;
    out.nb = f.nb;
    out.ns = f.ns;
    out.c_sea = f.c_sea.raw();
    out.w_b = f.w_b.raw();
    out.s_land = f.s_land.raw();
    out.cx = f.centre.cx.raw();
    out.cy = f.centre.cy.raw();
    out.engine = std::move(f);
    return out;
}

// E4 steps 2 and 3 on any W x H grid (seam elevation_ocean_from_provisional):
// returns the ocean mask, 1 = ocean.
inline std::vector<std::uint8_t> ocean_from_provisional(Grid grid, std::span<const std::uint8_t> provisional,
                                                        std::int64_t i_min) {
    return worldgen::stages::elevation_ocean_from_provisional(grid, provisional, i_min);
}

// E4 on the map (seam elevation_classify).
struct Ocean {
    std::vector<std::uint8_t> mask;  // 1 = ocean
    std::int64_t land_cells = 0;     // L_O
};

inline Ocean classify(const Fields& f, const Tuning& t) {
    Ocean out;
    out.mask = worldgen::stages::elevation_classify(f.engine, t);
    for (const std::uint8_t o : out.mask) {
        out.land_cells += o != 0U ? 0 : 1;
    }
    return out;
}

// E5.1 (the floor seam) on lists of the land cells' nb, ns and gm (raw).
struct Floor {
    bool triggered = false;
    std::int64_t target = 0;
    std::int64_t o_star = 0;          // raw
    std::int64_t beta_star = 0;       // raw
    std::vector<std::int64_t> mids;   // bisection mids, raw (empty if none ran)
};

inline Floor floor_of(const worldgen::stages::MountainFloor& m) {
    Floor f{m.triggered, m.target, m.offset.raw(), m.bias.raw(), {}};
    for (const worldgen::Fx x : m.bisection) {
        f.mids.push_back(x.raw());
    }
    return f;
}

inline Floor floor_search(std::span<const std::int64_t> nb, std::span<const std::int64_t> ns,
                          std::span<const std::int64_t> gm, std::int64_t w_b, const Tuning& t) {
    std::vector<worldgen::stages::FloorCell> cells;
    for (std::size_t i = 0; i < nb.size(); ++i) {
        cells.push_back(worldgen::stages::FloorCell{worldgen::Fx::from_raw(nb[i]), worldgen::Fx::from_raw(ns[i]),
                                                   worldgen::Fx::from_raw(gm[i])});
    }
    return floor_of(worldgen::stages::mountain_floor(cells, worldgen::Fx::from_raw(w_b), t));
}

// E5 to E7 (seam elevation_heights).
struct Heights {
    std::vector<std::int64_t> h7;  // raw Fx per cell, land and ocean
    std::int64_t scale = 0;        // raw (E6)
    Floor floor;                   // (o*, beta*) used (E5.1)
};

inline Heights heights(std::uint64_t seed, const Fields& f, const Ocean& ocean, const Tuning& t,
                       std::uint32_t threads) {
    const auto h = worldgen::stages::elevation_heights(seed, f.engine, ocean.mask, t, threads);
    Heights out;
    out.h7 = h.h;
    out.scale = h.scale.raw();
    out.floor = floor_of(h.floor);
    return out;
}

// E8 on any W x H grid (seam erode). Values raw; talus_slope is the
// tuning value (D_o = mul_int(talus_slope, 16), D_d = mul(D_o, SQRT2)).
struct ErosionParams {
    std::int64_t iterations = 0;
    std::int64_t route_interval = 1;
    std::int64_t k = 0;
    std::int64_t diffusion = 0;
    std::int64_t talus_slope = 0;
    std::int64_t talus_rate = 0;
    std::int64_t floor = 0;
    std::uint64_t key = 0;  // K_erosion
};

inline std::vector<std::int64_t> erode(Grid grid, std::span<const std::int64_t> h, std::span<const std::uint8_t> ocean,
                                       const ErosionParams& p, std::uint32_t threads) {
    worldgen::stages::ErosionParams ep;
    ep.iterations = static_cast<std::int32_t>(p.iterations);
    ep.route_interval = static_cast<std::int32_t>(p.route_interval);
    ep.k = worldgen::Fx::from_raw(p.k);
    ep.diffusion = worldgen::Fx::from_raw(p.diffusion);
    ep.talus_drop_o = worldgen::mul_int(worldgen::Fx::from_raw(p.talus_slope), 16);
    ep.talus_drop_d = worldgen::mul(ep.talus_drop_o, worldgen::Fx::SQRT2);
    ep.talus_rate = worldgen::Fx::from_raw(p.talus_rate);
    ep.floor = worldgen::Fx::from_raw(p.floor);
    ep.key = p.key;
    std::vector<std::int64_t> out(h.begin(), h.end());
    worldgen::stages::erode(grid, out, ocean, ep, threads);
    return out;
}

// E9 (seam quantize_elevation), on a 1-row grid of h.size() cells.
inline std::vector<std::int16_t> quantize(std::span<const std::int64_t> h, std::span<const std::uint8_t> ocean) {
    const Grid g{static_cast<std::int32_t>(h.size()), 1};
    return worldgen::stages::quantize_elevation(g, h, ocean, 1);
}

// E10.1 to E10.5 on any W x H grid (the conditioning seam).
struct ConditionParams {
    std::int64_t lake_min_cells = 0;
    std::int64_t lake_max_cells = 0;
    std::int64_t lake_min_depth_dm = 0;
    std::int64_t max_share_cells = 1;
    std::int64_t fine_slope = 65536;
};

inline worldgen::stages::ConditionParams engine_params(const ConditionParams& p) {
    worldgen::stages::ConditionParams cp;
    cp.lake_min_cells = p.lake_min_cells;
    cp.lake_max_cells = p.lake_max_cells;
    cp.lake_min_depth_dm = p.lake_min_depth_dm;
    cp.max_share_cells = p.max_share_cells;
    cp.fine_slope = p.fine_slope;
    return cp;
}

// The full result: e', the E10.2 components and the kept labels.
inline worldgen::stages::Conditioned condition_full(Grid grid, std::span<const std::int16_t> e,
                                                   std::span<const std::uint8_t> ocean,
                                                   std::span<const std::int64_t> cost,
                                                   std::span<const std::int64_t> bump, const ConditionParams& p) {
    return worldgen::stages::condition_depressions(grid, e, ocean, cost, bump, engine_params(p));
}

inline std::vector<std::int16_t> condition(Grid grid, std::span<const std::int16_t> e,
                                           std::span<const std::uint8_t> ocean, std::span<const std::int64_t> cost,
                                           std::span<const std::int64_t> bump, const ConditionParams& p) {
    return condition_full(grid, e, ocean, cost, bump, p).e;
}

// The whole stage (seam elevation_grid): the elevation layer's values.
inline std::vector<std::int16_t> elevation_grid(std::uint64_t seed, std::int32_t n, const Tuning& t,
                                                std::uint32_t threads) {
    return worldgen::stages::elevation_grid(seed, n, t, threads);
}

}  // namespace elev_api
