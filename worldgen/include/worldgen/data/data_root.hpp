#pragma once
// Where the data root is (spec P0-03 section 7.6 and amendment 15; P1-00
// section 6.3).
//
// Without --data, the data root is the UNMAPPED_DATA_DIR environment
// variable if set, otherwise game/data/ in the source tree as recorded by
// CMake at configure time. The path only locates the files: it is never
// written into a world, and content_hash depends only on the validated
// values (P0-03 8). While no kind is registered the data root is not even
// opened (P0-03 amendment 9), so the choice has no effect on any output.
#include <optional>
#include <string>
#include <string_view>

namespace worldgen::data {

// The environment variable naming the default data root (P0-03 7.6).
inline constexpr std::string_view DATA_DIR_ENV = "UNMAPPED_DATA_DIR";

// Where a resolved data root came from.
enum class DataRootSource : unsigned char {
    flag,         // --data DIR
    environment,  // UNMAPPED_DATA_DIR
    configured,   // game/data/ recorded at configure time
    none,         // no root: no flag, no variable, and none recorded
};

// A resolved data root: a UTF-8 path, and where it came from. path is empty
// exactly when source is none.
struct DataRoot {
    std::string path;
    DataRootSource source = DataRootSource::none;
};

// The game/data/ directory of the source tree as recorded by CMake when the
// build was configured (UTF-8, '/' separators, no trailing separator), or
// empty if the build recorded none.
[[nodiscard]] std::string_view configured_data_root() noexcept;

// Resolves the data root (P0-03 7.6; P1-00 appendix C item 16):
//   - flag_utf8 given and non-empty: that path, source flag;
//   - flag_utf8 given but empty: no data root (path empty, source none). An
//     explicit flag never falls back to the environment or the configured
//     root; the CLI rejects an empty --data as a usage error before calling
//     this, so this case is reached only from other callers;
//   - otherwise env_utf8 if set and non-empty (an empty value counts as
//     unset), source environment;
//   - otherwise configured_data_root() if non-empty, source configured;
//   - otherwise none.
// So path is empty exactly when source is none. A relative path is
// returned as given and resolves against the current directory. The caller
// reads the environment, so this stays a pure function of its arguments
// and the build.
[[nodiscard]] DataRoot resolve_data_root(std::optional<std::string_view> flag_utf8,
                                         std::optional<std::string_view> env_utf8);

}  // namespace worldgen::data
