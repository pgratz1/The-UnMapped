#pragma once

// Stage P3-04 history (spec docs/specs/p3-04-simulation-engine.md; P3-00
// sections 4 and 6): the schedule of phases and the simulation loop over
// it, with the stub subsystems of P3-04 7 (one civilization and one
// settlement per awakening), writing the thirteen tables of schema 7.
//
// Every function raises FatalError (P0-01 4.1) on a contract violation.

#include <string_view>

#include "worldgen/data/content.hpp"
#include "worldgen/history/engine.hpp"
#include "worldgen/history/inputs.hpp"
#include "worldgen/stages/stage.hpp"

namespace worldgen::stages {

// The stage name recorded in meta.stages (P3-00 5).
inline constexpr std::string_view HISTORY_STAGE = "history";

// The rows of the thirteen tables of P3-04 9.1, each in its write order (9.3).
using HistoryResult = history::HistoryResult;

// P3-04 3.4 items 2 to 5 (tuning, catalogue data, settings, chronology rows;
// in this order, each fatal with its message), then sections 5 to 7 with
// the shipped configuration (6.4): the stub awakening handler, no
// subsystem, no tension source, the empty intervention source and the
// catalogue of the content. Pure: no state is kept between calls.
[[nodiscard]] HistoryResult history_stage(const data::ContentSet& content, const history::HistoryInputs& inputs);

// Writes the rows of a result in the order of 9.3 (tables in DDL order,
// each in ascending key).
void write_history_result(StageContext& context, const HistoryResult& result);

// The stage entry point (P1-00 3): 3.4 item 1 ("history: input
// precondition: no region rows"; "history: input precondition: no age
// rows"), then history_stage(content, history_inputs_from(context)) and the
// write.
void run_history(StageContext& context);

}  // namespace worldgen::stages
