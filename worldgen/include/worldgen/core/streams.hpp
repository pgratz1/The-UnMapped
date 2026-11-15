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

// Every registered name, in registration order.
inline constexpr std::array<std::string_view, 3> all_stream_names = {
    test_hash,
    test_noise,
    test_wide,
};

}  // namespace worldgen::streams
