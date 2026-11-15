// Platform layer of worldgen-cli (spec P0-04 sections 4.1, 4.3, 4.4).
//
// The only file of the CLI that includes <windows.h>, with NOMINMAX and
// WIN32_LEAN_AND_MEAN so that its min/max macros cannot collide with
// worldgen::min / worldgen::max.

#include "platform.hpp"

#include <cstddef>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <fcntl.h>
#include <io.h>

#include <cstdio>
#include <limits>
#else
#include <cstdlib>
#endif

namespace cli::platform {

#if defined(_WIN32)

namespace {

// UTF-16 to UTF-8; std::nullopt for invalid UTF-16 (WC_ERR_INVALID_CHARS
// rejects lone surrogates instead of substituting U+FFFD).
// determinism-lint: allow(wchar-type) -- Windows UTF-16 API boundary, converted to UTF-8 at once (P0-04 4.1)
std::optional<std::string> to_utf8(const wchar_t* text, std::size_t length) {
    if (length == 0) {
        return std::string();
    }
    if (length > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    const int wide_length = static_cast<int>(length);
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, wide_length, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return std::nullopt;
    }
    std::string out(static_cast<std::size_t>(size), '\0');
    const int written =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, wide_length, out.data(), size, nullptr, nullptr);
    if (written != size) {
        return std::nullopt;
    }
    return out;
}

std::wstring to_wide_ascii(const char* name) {
    std::wstring out;
    for (const char* p = name; *p != '\0'; ++p) {
        // determinism-lint: allow(wchar-type) -- Windows UTF-16 API boundary, converted to UTF-8 at once (P0-04 4.1)
        out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));  // names are ASCII
    }
    return out;
}

}  // namespace

void prepare_stdio() noexcept {
    static_cast<void>(_setmode(_fileno(stdout), _O_BINARY));
    static_cast<void>(_setmode(_fileno(stderr), _O_BINARY));
}

// determinism-lint: allow(wchar-type) -- Windows UTF-16 API boundary, converted to UTF-8 at once (P0-04 4.1)
std::optional<std::vector<std::string>> utf8_arguments(int argc, wchar_t** argv, int& bad_index) {
    std::vector<std::string> out;
    for (int i = 1; i < argc; ++i) {
        // determinism-lint: allow(wchar-type) -- Windows UTF-16 API boundary, converted to UTF-8 at once (P0-04 4.1)
        const wchar_t* arg = argv[i];
        std::size_t length = 0;
        while (arg[length] != L'\0') {
            ++length;
        }
        std::optional<std::string> utf8 = to_utf8(arg, length);
        if (!utf8) {
            bad_index = i;
            return std::nullopt;
        }
        out.push_back(std::move(*utf8));
    }
    return out;
}

EnvironmentValue environment_utf8(const char* name) {
    const std::wstring wide_name = to_wide_ascii(name);
    const DWORD needed = GetEnvironmentVariableW(wide_name.c_str(), nullptr, 0);
    if (needed == 0) {
        return {};  // not set (or empty: both mean "no value" here)
    }
    std::wstring value(static_cast<std::size_t>(needed), L'\0');
    const DWORD got = GetEnvironmentVariableW(wide_name.c_str(), value.data(), needed);
    if (got == 0 || got >= needed) {
        return {};  // removed or grown in between: not possible in this single-threaded process
    }
    std::optional<std::string> utf8 = to_utf8(value.data(), static_cast<std::size_t>(got));
    if (!utf8) {
        return EnvironmentValue{std::nullopt, true};  // a lone surrogate: not silently "unset"
    }
    return EnvironmentValue{std::move(utf8), false};
}

#else

void prepare_stdio() noexcept {}

EnvironmentValue environment_utf8(const char* name) {
    const char* value = std::getenv(name);  // NOLINT(concurrency-mt-unsafe): single-threaded
    if (value == nullptr) {
        return {};
    }
    return EnvironmentValue{std::string(value), false};
}

#endif

}  // namespace cli::platform
