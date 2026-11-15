#pragma once

// Private: the stage entry points with a test fault (worldgen/stages/testing.hpp).

#include <string_view>

#include "worldgen/db/writer.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/stages/testing.hpp"

namespace worldgen::stages {

using testing::StageFault;

namespace detail {

// run_test_pattern, failing inside the stage when fault says so.
void run_test_pattern(db::WorldWriter& world, StageFault fault);

// generate_world with a fault (generate.cpp).
[[nodiscard]] db::Status generate_world(const WorldParams& params, std::string_view out_utf8,
                                        db::Overwrite overwrite, StageFault fault);

}  // namespace detail

}  // namespace worldgen::stages
