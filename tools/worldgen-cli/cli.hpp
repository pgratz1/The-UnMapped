#pragma once

// worldgen-cli command dispatch (spec P0-04).

#include <string>
#include <string_view>
#include <vector>

namespace cli {

// Runs worldgen-cli with its arguments as UTF-8 (program name excluded) and
// returns the exit code (P0-04 4.6). Never throws.
[[nodiscard]] int run(const std::vector<std::string>& args) noexcept;

// Prints "worldgen-cli: error: <message>" to stderr (P0-04 4.5).
void print_error(const std::string& message) noexcept;

// Prints the usage-error hint "worldgen-cli: run 'worldgen-cli [<command>]
// --help' for usage" to stderr (P0-04 4.1, amendment 2); command may be empty.
void print_hint(std::string_view command) noexcept;

}  // namespace cli
