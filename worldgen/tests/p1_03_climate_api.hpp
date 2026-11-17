#pragma once

// The one place where the P1-03 climate tests touch the stage's API.
//
// The tests were written from the spec before the implementation existed
// (test-writer rule), against the test-owned functions below, which take and
// return plain integer arrays (Fx values as raw int64). Integration adapts
// this file only.
//
// Seams (P1-03 section 10, revised 2026-11-17, "as implemented in
// worldgen/include/worldgen/stages/climate.hpp", namespace worldgen::stages):
//
//   climate_tuning_from(const data::ContentSet&)
//   climate_surface(Grid, elevation, water, water_depth) -> std::vector<std::int64_t>   (C1)
//   climate_wind(seed, std::span<const std::int64_t> weights) -> geo::Direction        (C2)
//   climate_temperature_cell(std::int64_t y, std::int64_t height, std::int64_t s_dm, Fx noise, tuning)
//   climate_temperature(seed, Grid, surface, tuning, threads) -> std::vector<std::int16_t>   (C3)
//   climate_box_mean(Grid, surface, std::int64_t r_cells, threads) -> std::vector<Fx>   (one box pass)
//   climate_triangle_mean(Grid, surface, std::int64_t r_cells, threads) -> std::vector<Fx>   (C4)
//   climate_sweep(Grid, water, std::span<const Fx> hs, geo::Direction wind, tuning) -> ClimateSweep {q, p}
//   climate_rainfall_cell(Fx p, std::int64_t coast_dist, Fx rn, tuning) -> std::uint16_t
//   climate_rainfall(seed, Grid, std::span<const Fx> p, coast_dist, tuning, threads)   (C6)
//   climate_layers(Grid, elevation, water, water_depth, coast_dist, tuning, seed, threads)
//   run_climate(StageContext&)    (reached through stages::implemented_geography_stages())
//
// A precondition violation throws worldgen::FatalError whose message starts
// "climate: input precondition: " (P1-03 section 2).

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p1_04_biomes_data.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/stages/climate.hpp"

namespace climate_api {

using worldgen::Fx;
using worldgen::geo::Grid;
using Tuning = worldgen::stages::ClimateTuning;

// The two layers of P1-03 section 3, as the tests read them.
struct Layers {
    std::vector<std::int16_t> temperature;
    std::vector<std::uint16_t> rainfall;

    friend bool operator==(const Layers&, const Layers&) = default;
};

// C5's output as raw Fx values.
struct Sweep {
    std::vector<std::int64_t> q;
    std::vector<std::int64_t> p;

    friend bool operator==(const Sweep&, const Sweep&) = default;
};

// The result of loading a data root holding the given worldgen/*.toml
// texts, through the production registry (P0-03, P1-00 6.1).
struct TuningLoad {
    std::optional<worldgen::data::ContentSet> content;
    std::vector<worldgen::data::LoadError> errors;
    [[nodiscard]] bool ok() const noexcept { return content.has_value(); }
};

inline TuningLoad load_tuning_text(std::string_view elevation_toml, std::string_view hydrology_toml,
                                   std::string_view climate_toml) {
    namespace wd = worldgen::data;
    const wd::KindRegistry registry = wd::builtin_kinds();
    wd::MemoryDirectory dir{"worldgen", {}};
    dir.entries.push_back(wd::MemoryEntry{"elevation.toml", std::string(elevation_toml), false});
    dir.entries.push_back(wd::MemoryEntry{"hydrology.toml", std::string(hydrology_toml), false});
    dir.entries.push_back(wd::MemoryEntry{"climate.toml", std::string(climate_toml), false});
    wd::MemoryDataRoot root;
    // The biome data (P1-04 4.1, 4.2), required once the biomes merge registers it.
    biome_data::add_to(registry, root, dir);
    root.directories.push_back(dir);
    wd::LoadResult<wd::ContentSet> r = wd::load_content_from_memory(registry, root);
    TuningLoad out;
    out.errors = r.errors;
    if (r.ok()) {
        out.content = std::move(*r.value);
    }
    return out;
}

inline Tuning tuning_from(const worldgen::data::ContentSet& content) {
    return worldgen::stages::climate_tuning_from(content);
}

namespace detail {
inline std::vector<std::int64_t> raws(const std::vector<Fx>& v) {
    std::vector<std::int64_t> out;
    out.reserve(v.size());
    for (const Fx x : v) {
        out.push_back(x.raw());
    }
    return out;
}
inline std::vector<Fx> fxs(std::span<const std::int64_t> v) {
    std::vector<Fx> out;
    out.reserve(v.size());
    for (const std::int64_t x : v) {
        out.push_back(Fx::from_raw(x));
    }
    return out;
}
}  // namespace detail

// C1 (seam climate_surface): S, or fatal on a violated precondition.
inline std::vector<std::int64_t> surface(Grid g, std::span<const std::int16_t> elevation,
                                         std::span<const std::uint8_t> water,
                                         std::span<const std::uint16_t> water_depth) {
    return worldgen::stages::climate_surface(g, elevation, water, water_depth);
}

// C2 (seam climate_wind).
inline std::uint8_t wind(std::uint64_t seed, std::span<const std::int64_t> weights) {
    return worldgen::stages::climate_wind(seed, weights);
}

// C3 for one cell (seam climate_temperature_cell).
inline std::int16_t temperature_cell(std::int64_t y, std::int64_t height, std::int64_t s_dm, std::int64_t noise_raw,
                                     const Tuning& t) {
    return worldgen::stages::climate_temperature_cell(y, height, s_dm, Fx::from_raw(noise_raw), t);
}

// C3 for a grid (seam climate_temperature).
inline std::vector<std::int16_t> temperature(std::uint64_t seed, Grid g, std::span<const std::int64_t> s,
                                             const Tuning& t, std::uint32_t threads) {
    return worldgen::stages::climate_temperature(seed, g, s, t, threads);
}

// One box pass of half-width r_cells (seam climate_box_mean): hs1, raw.
inline std::vector<std::int64_t> box_mean(Grid g, std::span<const std::int64_t> s, std::int64_t r_cells,
                                          std::uint32_t threads) {
    return detail::raws(worldgen::stages::climate_box_mean(g, s, r_cells, threads));
}

// C4 (seam climate_triangle_mean): hs, raw.
inline std::vector<std::int64_t> triangle_mean(Grid g, std::span<const std::int64_t> s, std::int64_t r_cells,
                                               std::uint32_t threads) {
    return detail::raws(worldgen::stages::climate_triangle_mean(g, s, r_cells, threads));
}

// C5 (seam climate_sweep): q and p per cell, raw.
inline Sweep sweep(Grid g, std::span<const std::uint8_t> water, std::span<const std::int64_t> hs_raw,
                   std::uint8_t wind_code, const Tuning& t) {
    const std::vector<Fx> hs = detail::fxs(hs_raw);
    const worldgen::stages::ClimateSweep r = worldgen::stages::climate_sweep(g, water, hs, wind_code, t);
    return Sweep{detail::raws(r.q), detail::raws(r.p)};
}

// C6 for one cell (seam climate_rainfall_cell).
inline std::uint16_t rainfall_cell(std::int64_t p_raw, std::int64_t coast_dist, std::int64_t rn_raw, const Tuning& t) {
    return worldgen::stages::climate_rainfall_cell(Fx::from_raw(p_raw), coast_dist, Fx::from_raw(rn_raw), t);
}

// C6 for a grid (seam climate_rainfall).
inline std::vector<std::uint16_t> rainfall(std::uint64_t seed, Grid g, std::span<const std::int64_t> p_raw,
                                           std::span<const std::int16_t> coast_dist, const Tuning& t,
                                           std::uint32_t threads) {
    const std::vector<Fx> p = detail::fxs(p_raw);
    return worldgen::stages::climate_rainfall(seed, g, p, coast_dist, t, threads);
}

// The whole stage on any W x H grid (seam climate_layers).
inline Layers layers(Grid g, std::span<const std::int16_t> elevation, std::span<const std::uint8_t> water,
                     std::span<const std::uint16_t> water_depth, std::span<const std::int16_t> coast_dist,
                     const Tuning& t, std::uint64_t seed, std::uint32_t threads) {
    worldgen::stages::ClimateLayers r =
        worldgen::stages::climate_layers(g, elevation, water, water_depth, coast_dist, t, seed, threads);
    Layers out;
    out.temperature = std::move(r.temperature);
    out.rainfall = std::move(r.rainfall);
    return out;
}

}  // namespace climate_api
