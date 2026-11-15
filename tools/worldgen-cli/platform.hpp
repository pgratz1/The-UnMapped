#pragma once

// Platform layer of worldgen-cli (spec P0-04 sections 4.1, 4.3, 4.4).
// platform.cpp is the only file that includes <windows.h>.

#include <optional>
#include <string>
#include <vector>

namespace cli::platform {

// Switches stdout and stderr to binary mode on Windows so that LF is never
// turned into CR LF (P0-04 4.4). Does nothing elsewhere.
void prepare_stdio() noexcept;

#if defined(_WIN32)
// The UTF-16 command-line arguments of wmain as UTF-8 (P0-04 4.1), program
// name excluded. std::nullopt if an argument is not valid UTF-16 (a lone
// surrogate), with the 1-based index of the bad argument in bad_index.
// determinism-lint: allow(wchar-type) -- Windows UTF-16 API boundary, converted to UTF-8 at once (P0-04 4.1)
[[nodiscard]] std::optional<std::vector<std::string>> utf8_arguments(int argc, wchar_t** argv, int& bad_index);
#endif

// The value of an environment variable as UTF-8 (read as UTF-16 on Windows),
// or std::nullopt if it is not set (P0-04 4.3).
[[nodiscard]] std::optional<std::string> environment_utf8(const char* name);

}  // namespace cli::platform
