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
// Test-only stream for the random climate fixtures (P1-03 10.2).
inline constexpr std::string_view test_climate = "test.climate";

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

// Reserved prefixes for the later second-pass stages (P1-00 section 3):
// their names are registered here by the stage that uses them.
//   geo.biomes.   geo.resources.   geo.regions.

// Every registered name, in registration order.
inline constexpr std::array<std::string_view, 25> all_stream_names = {
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
    test_climate,
    geo_climate_wind,
    geo_climate_temperature,
    geo_climate_rainfall,
};

}  // namespace worldgen::streams
