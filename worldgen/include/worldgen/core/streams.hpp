#pragma once

// Registry of every stream name used anywhere (spec P0-01 section 7.3).
//
// Every name passed to stream_key() must be a constant here, and must also
// appear in all_stream_names. A test checks that every entry matches the
// stream-name grammar (is_valid_stream_name) and that no name is listed
// twice. Convention: <area>.<stage>[.<purpose>].

#include <array>
#include <string_view>

namespace worldgen::streams {

// Test pattern hash layer (P0-04).
inline constexpr std::string_view test_hash = "test.hash";
// Test pattern noise layer (P0-04).
inline constexpr std::string_view test_noise = "test.noise";
// Test-only stream for comparing wide-arithmetic paths (P0-01 13.3).
inline constexpr std::string_view test_wide = "test.wide";
// Test-only stream for the random hydrology fixtures (P1-02 8.2).
inline constexpr std::string_view test_hydro = "test.hydro";

// Keys of stage elevation (P1-00 section 3, P1-01 section 2), in the order
// of P1-01 section 2. Each is used only as a noise key (noise2 / fbm2
// through the compositions of P1-01 4.1) or a location-hash key (hash_at:
// continent for the centre jitter, erosion for the random receivers); the
// stage draws from no Rng.
inline constexpr std::string_view geo_elevation_continent_warp_x = "geo.elevation.continent_warp_x";
inline constexpr std::string_view geo_elevation_continent_warp_y = "geo.elevation.continent_warp_y";
inline constexpr std::string_view geo_elevation_continent = "geo.elevation.continent";
inline constexpr std::string_view geo_elevation_belt = "geo.elevation.belt";
inline constexpr std::string_view geo_elevation_coast = "geo.elevation.coast";
inline constexpr std::string_view geo_elevation_segment = "geo.elevation.segment";
inline constexpr std::string_view geo_elevation_relief_warp_x = "geo.elevation.relief_warp_x";
inline constexpr std::string_view geo_elevation_relief_warp_y = "geo.elevation.relief_warp_y";
inline constexpr std::string_view geo_elevation_ridge = "geo.elevation.ridge";
inline constexpr std::string_view geo_elevation_hills = "geo.elevation.hills";
inline constexpr std::string_view geo_elevation_roughness = "geo.elevation.roughness";
inline constexpr std::string_view geo_elevation_basin = "geo.elevation.basin";
inline constexpr std::string_view geo_elevation_detail = "geo.elevation.detail";
inline constexpr std::string_view geo_elevation_erosion = "geo.elevation.erosion";
inline constexpr std::string_view geo_elevation_fill = "geo.elevation.fill";
// Pass layout (P1-01 E6.5): hash_at for the col choice and the jitters,
// noise2 of substream 3 for the band width. Registered after
// geo.hydrology.flat (P1-01 section 2).
inline constexpr std::string_view geo_elevation_pass = "geo.elevation.pass";

// Key of stage hydrology (P1-00 section 3, P1-02 section 2): the noise key
// of the H2 tie-break on flats (fbm_m only, no Rng).
inline constexpr std::string_view geo_hydrology_flat = "geo.hydrology.flat";

// Keys of stage climate (P1-00 section 3, P1-03 section 2): the prevailing
// wind (one Rng, one weighted_index call, sequential code only, D9) and the
// noise keys of the temperature and rainfall variation (fbm_m only).
inline constexpr std::string_view geo_climate_wind = "geo.climate.wind";
inline constexpr std::string_view geo_climate_temperature = "geo.climate.temperature";
inline constexpr std::string_view geo_climate_rainfall = "geo.climate.rainfall";
// The per-world temperature offset of C2b (P1-03, generator 0.9.0): one
// Rng, at most one uniform_i64 call, sequential code only (D9).
inline constexpr std::string_view geo_climate_temp_offset = "geo.climate.temp_offset";

// Keys of stage biomes (P1-00 section 3, P1-04 section 2): the noise keys of
// the temperature and rainfall jitter the rules see (fbm_m only, no Rng).
inline constexpr std::string_view geo_biomes_temp_jitter = "geo.biomes.temp_jitter";
inline constexpr std::string_view geo_biomes_rain_jitter = "geo.biomes.rain_jitter";

// Key of stage resources (P1-00 section 3, P1-05 section 2): the noise key
// of the ore deposits (fbm_m only, no Rng).
inline constexpr std::string_view geo_resources_ore = "geo.resources.ore";

// Keys of stage regions (P1-00 section 3, P1-06 section 2): the hash_at key
// of the seed-site tie-break and the noise key of the step cost (no Rng).
inline constexpr std::string_view geo_regions_seed = "geo.regions.seed";
inline constexpr std::string_view geo_regions_cost = "geo.regions.cost";

// Every registered name, in registration order.
inline constexpr std::array<std::string_view, 30> all_stream_names = {
    test_hash,
    test_noise,
    test_wide,
    geo_elevation_continent_warp_x,
    geo_elevation_continent_warp_y,
    geo_elevation_continent,
    geo_elevation_belt,
    geo_elevation_coast,
    geo_elevation_segment,
    geo_elevation_relief_warp_x,
    geo_elevation_relief_warp_y,
    geo_elevation_ridge,
    geo_elevation_hills,
    geo_elevation_roughness,
    geo_elevation_basin,
    geo_elevation_detail,
    geo_elevation_erosion,
    geo_elevation_fill,
    test_hydro,
    geo_hydrology_flat,
    geo_elevation_pass,
    geo_climate_wind,
    geo_climate_temperature,
    geo_climate_rainfall,
    geo_biomes_temp_jitter,
    geo_biomes_rain_jitter,
    geo_climate_temp_offset,
    geo_resources_ore,
    geo_regions_seed,
    geo_regions_cost,
};

}  // namespace worldgen::streams
