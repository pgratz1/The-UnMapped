// Test hooks of the world database writer (worldgen/db/testing.hpp).

#include "worldgen/db/testing.hpp"

#include <algorithm>
#include <limits>
#include <string>

#include "db/sqlite_util.hpp"
#include "db/writer_state.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/db/utf8_path.hpp"

namespace worldgen::db::testing {

namespace {

detail::WriterState& stage_state(detail::WriterState* state, const char* operation) {
    if (state == nullptr) {
        throw FatalError(std::string("testing::WriterAccess::") + operation + ": stage is not open");
    }
    return *state;
}

}  // namespace

void WriterAccess::set_failure_point(WorldWriter& writer, FailurePoint point, FailureMode mode) {
    detail::WriterState& s = detail::live_state(writer.state_.get(), "set_failure_point");
    s.failure_point = point;
    s.failure_mode = mode;
}

void WriterAccess::set_fs_faults(WorldWriter& writer, const FsFaults& faults) {
    detail::live_state(writer.state_.get(), "set_fs_faults").fs_faults = faults;
}

void WriterAccess::set_meta_order(WorldWriter& writer, MetaOrder order) {
    detail::live_state(writer.state_.get(), "set_meta_order").meta_order = order;
}

Status WriterAccess::execute(WorldWriter& writer, std::string_view sql) {
    detail::WriterState& s = detail::live_state(writer.state_.get(), "execute");
    std::string message;
    if (detail::exec(s.db.get(), sql, message) != SQLITE_OK) {
        return Error{ErrorCode::sql_rejected, message};
    }
    return Ok{};
}

void WriterAccess::write_layer_any_id(StageWriter& stage, std::int64_t id, std::string_view name,
                                      const LayerGrid& grid, Codec codec) {
    detail::WriterState& s = stage_state(stage.state_, "write_layer_any_id");
    if (!is_valid_layer_name(name)) {
        throw FatalError("testing::WriterAccess::write_layer_any_id: '" + std::string(name) +
                         "' is not a valid layer name");
    }
    const std::vector<std::uint8_t> blob = encode_layer_blob(grid, codec);
    detail::insert_layer(s, id, name, static_cast<std::int64_t>(grid.width), static_cast<std::int64_t>(grid.height),
                         element_type_name(grid.type()), blob);
    if (id < std::numeric_limits<std::int64_t>::max()) {
        s.next_layer_id = std::max(s.next_layer_id, id + 1);
    }
}

void WriterAccess::insert_layer_row(StageWriter& stage, std::int64_t id, std::string_view name, std::int64_t width,
                                    std::int64_t height, std::string_view format,
                                    std::span<const std::uint8_t> data) {
    detail::insert_layer(stage_state(stage.state_, "insert_layer_row"), id, name, width, height, format, data);
}

Status execute_on_file(std::string_view path_utf8, std::string_view sql) {
    const std::string path(path_utf8);
    sqlite3* raw = nullptr;
    const int rc = sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr);
    detail::Connection db(raw);
    if (rc != SQLITE_OK) {
        return Error{ErrorCode::file_unreadable, "cannot open '" + path + "': " + detail::error_message(raw)};
    }
    std::string message;
    if (detail::exec(db.get(), sql, message) != SQLITE_OK) {
        return Error{ErrorCode::sql_rejected, message};
    }
    if (db.close() != SQLITE_OK) {
        return Error{ErrorCode::sql_rejected, "closing '" + path + "' failed"};
    }
    return Ok{};
}

}  // namespace worldgen::db::testing
