#pragma once

#include <string_view>

namespace worldgen {

// The generator version (spec P0-02 section 5.5), recorded as
// meta.generator_version in every world.db. Format MAJOR.MINOR.PATCH,
// decimal, no leading zeros. Changed by hand: any merged change that alters
// output bytes for any input bumps it, and the golden hashes are updated in
// the same branch. Never derived from git, the build date or the compiler.
// 0.1.0 until seed phrases were scrubbed (P0-02 amendment 12); 0.2.0 until
// the first geography stage (elevation, P1-01) and the `worldgen` data kind
// (P1-00 6.4 rule 4); 0.3.0 until the hydrology stage (P1-02) joined the
// default pipeline; now 0.4.0.
inline constexpr std::string_view GENERATOR_VERSION = "0.4.0";

// Returns GENERATOR_VERSION (P0-02 5.5). A world is only guaranteed
// reproducible under the same generator version (design doc section 4).
[[nodiscard]] std::string_view generator_version() noexcept;

}  // namespace worldgen
