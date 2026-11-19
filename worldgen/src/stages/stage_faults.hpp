#pragma once

// Private: the stage entry points with a test fault (worldgen/stages/testing.hpp).

#include <span>
#include <string_view>

#include "worldgen/db/writer.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/stages/testing.hpp"

namespace worldgen::stages {

using testing::StageFault;

namespace detail {

// run_test_pattern, failing inside the stage when fault says so.
void run_test_pattern(db::WorldWriter& world, StageFault fault);

// Sets the eight history keys of P3-03 4.2 on writer (generate.cpp).
void write_history_params(db::WorldWriter& writer, const history::HistoryParams& history);

// generate_world with a fault (generate.cpp).
// stages_override, when not null, replaces the geography stages (test hook
// generate_world_with_stages).
[[nodiscard]] db::Status generate_world(const WorldParams& params, const data::ContentSet& content,
                                        std::string_view out_utf8, db::Overwrite overwrite, StageFault fault,
                                        const std::span<const StageInfo>* stages_override = nullptr);

}  // namespace detail

}  // namespace worldgen::stages
