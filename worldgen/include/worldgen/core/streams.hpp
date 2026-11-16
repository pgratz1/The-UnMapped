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

// Reserved prefixes for the second-pass stages (P1-00 section 3): their
// names are registered here by the stage that uses them.
//   geo.climate.   geo.biomes.   geo.resources.   geo.regions.
// Stage hydrology uses no stream (P1-00 section 3).

// Every registered name, in registration order.
inline constexpr std::array<std::string_view, 18> all_stream_names = {
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
};

}  // namespace worldgen::streams
