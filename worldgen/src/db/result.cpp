#include "worldgen/db/result.hpp"

namespace worldgen::db {

const char* error_code_name(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::file_unreadable: return "file_unreadable";
        case ErrorCode::not_sqlite: return "not_sqlite";
        case ErrorCode::not_world: return "not_world";
        case ErrorCode::unsupported_schema_version: return "unsupported_schema_version";
        case ErrorCode::meta_key_missing: return "meta_key_missing";
        case ErrorCode::meta_value_invalid: return "meta_value_invalid";
        case ErrorCode::layer_not_found: return "layer_not_found";
        case ErrorCode::corrupt_layer: return "corrupt_layer";
        case ErrorCode::output_exists: return "output_exists";
        case ErrorCode::output_parent_missing: return "output_parent_missing";
        case ErrorCode::output_write_failed: return "output_write_failed";
        case ErrorCode::output_rename_failed: return "output_rename_failed";
        case ErrorCode::sqlite_error: return "sqlite_error";
        case ErrorCode::out_of_memory: return "out_of_memory";
        case ErrorCode::sql_rejected: return "sql_rejected";
    }
    return "unknown";
}

}  // namespace worldgen::db
