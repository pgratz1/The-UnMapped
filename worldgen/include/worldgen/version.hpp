#pragma once

#include <string_view>

namespace worldgen {

// Version of the generator. A world is only guaranteed reproducible under
// the same generator version (design doc section 4), so this string is
// recorded in every world.db. Placeholder until the database-layer spec
// defines its format.
[[nodiscard]] std::string_view generator_version() noexcept;

}  // namespace worldgen
