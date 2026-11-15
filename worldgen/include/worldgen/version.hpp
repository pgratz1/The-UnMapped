#pragma once

#include <string_view>

namespace worldgen {

// The generator version (spec P0-02 section 5.5), recorded as
// meta.generator_version in every world.db. Format MAJOR.MINOR.PATCH,
// decimal, no leading zeros. Changed by hand: any merged change that alters
// output bytes for any input bumps it, and the golden hashes are updated in
// the same branch. Never derived from git, the build date or the compiler.
// P0 value: 0.1.0 (interim).
inline constexpr std::string_view GENERATOR_VERSION = "0.1.0";

// Returns GENERATOR_VERSION (P0-02 5.5). A world is only guaranteed
// reproducible under the same generator version (design doc section 4).
[[nodiscard]] std::string_view generator_version() noexcept;

}  // namespace worldgen
