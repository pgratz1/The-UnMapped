#pragma once

// Private state of WorldWriter, shared by writer.cpp and testing.cpp.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "db/sqlite_util.hpp"
#include "worldgen/db/testing.hpp"

namespace worldgen::db::detail {

struct WriterState {
    Connection db;                 // the working database; closed by finalize
    bool finalized = false;        // finalize() has been entered
    bool stage_open = false;       // a StageWriter transaction is open
    std::int64_t next_layer_id = 1;
    std::int64_t stage_start_layer_id = 1;  // next_layer_id at begin_stage, restored on rollback
    std::vector<std::string> stages;              // committed stages, run order
    std::optional<std::uint64_t> seed;
    std::string seed_phrase;                      // empty for a numeric seed
    std::optional<std::string> content_hash;      // 64 lowercase hex digits
    std::map<std::string, std::string> params;    // full key "param.<name>" -> canonical value

    // Test hooks (worldgen/db/testing.hpp).
    testing::FailurePoint failure_point = testing::FailurePoint::none;
    testing::FailureMode failure_mode = testing::FailureMode::throw_fatal;
    testing::MetaOrder meta_order = testing::MetaOrder::ascending;
    testing::FsFaults fs_faults;
};

// FatalError unless the writer is usable (not moved from, not finalized).
WriterState& live_state(WriterState* state, const char* operation);

// Inserts one layer row; FatalError naming the layer if SQLite rejects it.
void insert_layer(WriterState& state, std::int64_t id, std::string_view name, std::int64_t width,
                  std::int64_t height, std::string_view format, std::span<const std::uint8_t> data);

}  // namespace worldgen::db::detail
