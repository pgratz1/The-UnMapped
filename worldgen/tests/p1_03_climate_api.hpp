#pragma once

// The one place where the P1-03 climate tests touch the stage's API.
//
// The tests were written from the spec before the implementation existed
// (test-writer rule), against the test-owned functions below, which take and
// return plain integer arrays (Fx values as raw int64). Integration adapts
// this file only.
//
// Seams (P1-03 10, "interim names; the engineer may add helpers but should
// not change these without a spec note"). The spec gives names and some
// arguments; the signatures below are the test writer's assumption, as
// declared in an assumed worldgen/stages/climate.hpp, namespace
// worldgen::stages:
//
//   tuning        ClimateTuning, climate_tuning_from(const data::ContentSet&)
//                     (as elevation_tuning_from and hydrology_tuning_from)
//   C2            climate_wind(std::uint64_t seed, std::span<const std::int64_t> weights) -> std::uint8_t
//                     (the eight wind_weights as loaded; k_w)
//   C3            climate_temperature_cell(std::int32_t y, std::int32_t height, std::int64_t s_dm,
//                     Fx noise, const ClimateTuning&) -> std::int16_t
//                     (noise: the fbm_m value at the cell, before temp_noise_amp_c;
//                     V = from_ratio(2y + 1, 2 height))
//   C4            climate_box_mean(geo::Grid, std::span<const std::int64_t> s, std::int32_t r,
//                     std::uint32_t threads) -> std::vector<Fx>
//   C5            climate_sweep(geo::Grid, std::span<const std::int64_t> s, std::span<const std::uint8_t> water,
//                     std::span<const Fx> hs, std::uint8_t wind, const ClimateTuning&) -> ClimateSweep {q, p}
//                     (q and p as std::vector<Fx>, one per cell)
//   C6            climate_rainfall_cell(Fx p, std::int16_t coast_dist, Fx rn, const ClimateTuning&)
//                     -> std::uint16_t
//   stage         climate_layers(geo::Grid, std::span<const std::int16_t> elevation,
//                     std::span<const std::uint8_t> water, std::span<const std::uint16_t> water_depth,
//                     std::span<const std::int16_t> coast_dist, const ClimateTuning&, std::uint64_t seed,
//                     std::uint32_t threads) -> ClimateLayers {temperature (i16), rainfall (u16)}
//                 A precondition violation throws worldgen::FatalError whose message starts
//                 "climate: input precondition: " (P1-03 section 2).
//   entry         the stage "climate" of stages::implemented_geography_stages() and
//                 worldgen::generate_world with WorldParams::until = "climate" (P1-00 10.1)

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

// C2 (seam climate_wind).
inline std::uint8_t wind(std::uint64_t seed, std::span<const std::int64_t> weights) {
    return worldgen::stages::climate_wind(seed, weights);
}

// C3 for one cell (seam climate_temperature_cell): row y of a map of the
// given height, surface s_dm, noise = the raw fbm_m value at the cell.
inline std::int16_t temperature_cell(std::int32_t y, std::int32_t height, std::int64_t s_dm, std::int64_t noise_raw,
                                     const Tuning& t) {
    return worldgen::stages::climate_temperature_cell(y, height, s_dm, Fx::from_raw(noise_raw), t);
}

// C4 (seam climate_box_mean): hs per cell, raw.
inline std::vector<std::int64_t> box_mean(Grid g, std::span<const std::int64_t> s, std::int32_t r,
                                          std::uint32_t threads) {
    const std::vector<Fx> hs = worldgen::stages::climate_box_mean(g, s, r, threads);
    std::vector<std::int64_t> out;
    out.reserve(hs.size());
    for (const Fx v : hs) {
        out.push_back(v.raw());
    }
    return out;
}

// C5 (seam climate_sweep): q and p per cell, raw.
inline Sweep sweep(Grid g, std::span<const std::int64_t> s, std::span<const std::uint8_t> water,
                   std::span<const std::int64_t> hs_raw, std::uint8_t wind_code, const Tuning& t) {
    std::vector<Fx> hs;
    hs.reserve(hs_raw.size());
    for (const std::int64_t v : hs_raw) {
        hs.push_back(Fx::from_raw(v));
    }
    const worldgen::stages::ClimateSweep r = worldgen::stages::climate_sweep(g, s, water, hs, wind_code, t);
    Sweep out;
    for (const Fx v : r.q) {
        out.q.push_back(v.raw());
    }
    for (const Fx v : r.p) {
        out.p.push_back(v.raw());
    }
    return out;
}

// C6 for one cell (seam climate_rainfall_cell).
inline std::uint16_t rainfall_cell(std::int64_t p_raw, std::int16_t coast_dist, std::int64_t rn_raw, const Tuning& t) {
    return worldgen::stages::climate_rainfall_cell(Fx::from_raw(p_raw), coast_dist, Fx::from_raw(rn_raw), t);
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
