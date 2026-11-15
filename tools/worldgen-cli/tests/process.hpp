#pragma once

// Portable child-process runner for the worldgen-cli integration tests
// (spec P0-04 section 12). The only platform-specific code of the tests
// lives in process.cpp.
//
// Linux: fork, chdir, dup2 of stdout and stderr onto capture files, execve.
// Windows: CreateProcessW. Every argument, the program path, the working
// directory and the environment are converted from UTF-8 to UTF-16 with
// MultiByteToWideChar(CP_UTF8) and the command line is quoted by the rules
// CommandLineToArgvW (and the MSVC CRT) use to split it, so that a
// non-ASCII argument reaches the child's wmain / GetCommandLineW as the
// same text, independent of the ANSI code page.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace clitest {

// One change to the child's environment, applied to a copy of the test
// process's environment: set name to value, or remove name when value is
// std::nullopt. On Windows names compare case-insensitively.
struct EnvChange {
    std::string name;
    std::optional<std::string> value;
};

// What a child process did. exit_code is the process exit status; a child
// killed by a signal (POSIX) is reported as 128 + signal number and
// signalled is set. out and err hold the exact bytes written to stdout and
// stderr (no newline translation).
struct ProcessResult {
    int exit_code = -1;
    bool signalled = false;
    std::string out;
    std::string err;
};

// Less common ways to start a child.
struct ProcessOptions {
    // When set, the child's stdout goes to this file (UTF-8 path; opened for
    // writing, created if missing), e.g. /dev/full; ProcessResult::out is
    // then empty.
    std::optional<std::string> stdout_path;
#if defined(_WIN32)
    // UTF-16 arguments appended after the UTF-8 ones exactly as given, so
    // that a test can pass text UTF-8 cannot carry (a lone surrogate).
    std::vector<std::wstring> wide_args;
    // UTF-16 environment changes applied after the UTF-8 ones, exactly as
    // given (std::nullopt removes the variable).
    std::vector<std::pair<std::wstring, std::optional<std::wstring>>> wide_env;
#endif
};

// Runs program (UTF-8 path; searched on PATH when search_path is true)
// with args (UTF-8, not including the program name), in working directory
// cwd (UTF-8), with the environment changes applied. stdout and stderr are
// redirected to two files created in capture_dir (UTF-8, must exist and is
// never the child's working directory), read back and deleted; stdin is
// the null device. Throws std::runtime_error if the child cannot be
// started at all; a program that is not found under search_path gives
// exit code 127 on Linux and a std::runtime_error on Windows.
[[nodiscard]] ProcessResult run_process(const std::string& program, const std::vector<std::string>& args,
                                        const std::string& cwd, const std::vector<EnvChange>& env,
                                        const std::string& capture_dir, bool search_path = false,
                                        const ProcessOptions& options = {});

// The id of the test process (to keep concurrent runs apart).
[[nodiscard]] std::int64_t current_process_id();

}  // namespace clitest
