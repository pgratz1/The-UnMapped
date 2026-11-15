#pragma once

// Output locations and file writing of worldgen-cli (spec P0-04 sections
// 4.3, 4.4), and the CLI's failure type (4.5, 4.6).

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/db/result.hpp"

namespace cli {

// Exit codes (P0-04 4.6).
enum class Exit : int {
    success = 0,
    internal = 1,
    usage = 2,
    input = 3,
    output = 4,
    mismatch = 5,
};

// A failure that ends the command: printed as "worldgen-cli: error:
// <message>", then each detail line, then (for usage errors) a hint to use
// --help (P0-04 4.1, 4.5).
class Failure : public std::runtime_error {
public:
    Failure(Exit exit, const std::string& message, std::vector<std::string> details = {})
        : std::runtime_error(message), exit_(exit), details_(std::move(details)) {}

    [[nodiscard]] Exit exit() const noexcept { return exit_; }
    [[nodiscard]] const std::vector<std::string>& details() const noexcept { return details_; }

private:
    Exit exit_;
    std::vector<std::string> details_;
};

// The exit code for a database-layer error (P0-04 4.6): output_* errors are
// 4, out_of_memory and sql_rejected 1, everything else from reading a file 3.
[[nodiscard]] Exit exit_for(worldgen::db::ErrorCode code) noexcept;

// Throws Failure for a database-layer error (P0-04 4.6).
[[noreturn]] void fail_with(const worldgen::db::Error& error);

// The default output directory, <build root>/out, created with its parents
// if missing (P0-04 4.3). Build root = UNMAPPED_BUILD_ROOT if set and
// non-empty; otherwise %LOCALAPPDATA%\the-unmapped on Windows,
// $HOME/.cache/the-unmapped elsewhere. A usage error asking for --out if no
// root can be resolved or the chosen variable is not an absolute path; an
// output error if the directory cannot be created.
[[nodiscard]] std::string default_output_dir();

// The absolute, lexically normalized form of a UTF-8 path, as printed on
// stdout by generate and render (P0-04 5, 6).
[[nodiscard]] std::string absolute_utf8(std::string_view path_utf8);

// Writes bytes to path_utf8 through <path>.partial and a rename (P0-04 4.4).
// Output error (exit 4) if the file exists and force is false, if it is a
// directory, if its parent directory does not exist, or if writing or the
// rename fails. <path>.partial is removed on every failure path.
void write_file_atomically(std::string_view path_utf8, std::span<const std::uint8_t> bytes, bool force);

// Checks the output location without writing (same rules as above).
void check_output_location(std::string_view path_utf8, bool force);

}  // namespace cli
