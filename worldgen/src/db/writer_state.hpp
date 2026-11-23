#pragma once

// Private state of WorldWriter, shared by writer.cpp and testing.cpp.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "db/sqlite_util.hpp"
#include "worldgen/db/testing.hpp"

namespace worldgen::db::detail {

// The last primary key written to each history table (P3-04 9.3).
struct HistoryKeys {
    using Pair = std::pair<std::int64_t, std::int64_t>;
    std::int64_t event_kind = 0;
    Pair event_kind_role{0, 0};
    Pair event_kind_value{0, 0};
    std::int64_t phase = 0;
    std::int64_t event = 0;
    Pair event_participant{0, 0};
    Pair event_cause{0, 0};
    Pair event_value{0, 0};
    std::int64_t civilization = 0;
    std::int64_t settlement = 0;
    Pair settlement_census{0, 0};
    Pair region_control{0, 0};
    std::tuple<std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::string, std::int64_t> relation{
        0, 0, 0, 0, std::string(), 0};
    Pair region_name{0, 0};  // schema 8
};

struct WriterState {
    Connection db;                 // the working database; closed by finalize
    bool finalized = false;        // finalize() has been entered
    bool stage_open = false;       // a StageWriter transaction is open
    std::int64_t next_layer_id = 1;
    std::int64_t stage_start_layer_id = 1;  // next_layer_id at begin_stage, restored on rollback
    std::int64_t next_pass_id = 1;          // table pass (P1-01 9.1)
    std::int64_t stage_start_pass_id = 1;   // next_pass_id at begin_stage, restored on rollback
    std::int64_t last_biome_id = 0;         // table biome (P1-04 B6): the largest id written
    std::int64_t stage_start_biome_id = 0;  // last_biome_id at begin_stage, restored on rollback
    bool climate_written = false;           // table climate (P1-03 C7): its one row
    bool stage_start_climate = false;       // climate_written at begin_stage, restored on rollback
    // Tables region, region_border, region_biome (P1-06 RG14): the last key
    // written to each ((0, 0) before the first row) and its value at
    // begin_stage, restored on rollback.
    std::int64_t last_region_id = 0;
    std::int64_t stage_start_region_id = 0;
    std::pair<std::int64_t, std::int64_t> last_border_key{0, 0};
    std::pair<std::int64_t, std::int64_t> stage_start_border_key{0, 0};
    std::pair<std::int64_t, std::int64_t> last_region_biome_key{0, 0};
    std::pair<std::int64_t, std::int64_t> stage_start_region_biome_key{0, 0};
    // Tables race, race_biome, culture, god, age (P3-03 5.3): the last key
    // written and its value at begin_stage, restored on rollback.
    std::int64_t last_race_id = 0;
    std::int64_t stage_start_race_id = 0;
    std::pair<std::int64_t, std::int64_t> last_race_biome_key{0, 0};
    std::pair<std::int64_t, std::int64_t> stage_start_race_biome_key{0, 0};
    std::int64_t last_culture_id = 0;
    std::int64_t stage_start_culture_id = 0;
    std::int64_t last_god_id = 0;
    std::int64_t stage_start_god_id = 0;
    std::int64_t last_age_id = 0;
    std::int64_t stage_start_age_id = 0;
    // The thirteen history tables (P3-04 9.3): the last key written to each
    // (all zero before the first row) and their values at begin_stage,
    // restored on rollback.
    HistoryKeys history_keys;
    HistoryKeys stage_start_history_keys;
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
