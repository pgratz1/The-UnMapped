// Child-process runner for the worldgen-cli integration tests; see
// process.hpp. This is the only translation unit of the tests that includes
// platform headers.

#include "process.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "worldgen/db/utf8_path.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>

extern char** environ;  // NOLINT: POSIX
#endif

namespace clitest {

namespace {

namespace fs = std::filesystem;

std::string read_and_remove(const std::string& path_utf8) {
    const fs::path path = worldgen::db::path_from_utf8(path_utf8);
    std::string bytes;
    {
        std::ifstream in(path, std::ios::binary);
        if (in) {
            bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
    }
    std::error_code ec;
    fs::remove(path, ec);
    return bytes;
}

unsigned long long next_capture_id() {
    static unsigned long long counter = 0;
    return ++counter;
}

std::string capture_path(const std::string& capture_dir, unsigned long long id, const char* suffix) {
    const fs::path dir = worldgen::db::path_from_utf8(capture_dir);
    return worldgen::db::path_to_utf8(dir / worldgen::db::path_from_utf8("capture-" + std::to_string(id) + suffix));
}

#if defined(_WIN32)

std::wstring widen(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()),
                                      nullptr, 0);
    if (n <= 0) {
        throw std::runtime_error("run_process: argument is not valid UTF-8");
    }
    std::wstring wide(static_cast<std::size_t>(n), L'\0');
    (void)MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), wide.data(),
                              n);
    return wide;
}

std::wstring backslashes(std::wstring path) {
    std::replace(path.begin(), path.end(), L'/', L'\\');
    return path;
}

// Appends arg to a command line so that CommandLineToArgvW and the MSVC CRT
// split it back into exactly arg.
void append_quoted(std::wstring& line, const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        line += arg;
        return;
    }
    line += L'"';
    for (std::size_t i = 0;; ++i) {
        std::size_t slashes = 0;
        while (i < arg.size() && arg[i] == L'\\') {
            ++slashes;
            ++i;
        }
        if (i == arg.size()) {
            line.append(slashes * 2, L'\\');
            break;
        }
        if (arg[i] == L'"') {
            line.append(slashes * 2 + 1, L'\\');
        } else {
            line.append(slashes, L'\\');
        }
        line += arg[i];
    }
    line += L'"';
}

// The variable name of an environment entry "NAME=VALUE". Entries such as
// "=C:=C:\dir" have a name starting with '='.
std::wstring env_name(const std::wstring& entry) {
    const std::size_t eq = entry.find(L'=', 1);
    return eq == std::wstring::npos ? entry : entry.substr(0, eq);
}

int compare_names(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE);
}

void apply_wide_change(std::vector<std::wstring>& entries, const std::wstring& name,
                       const std::optional<std::wstring>& value) {
    std::erase_if(entries, [&](const std::wstring& e) { return compare_names(env_name(e), name) == CSTR_EQUAL; });
    if (value.has_value()) {
        entries.push_back(name + L"=" + *value);
    }
}

std::vector<wchar_t> environment_block(const std::vector<EnvChange>& changes, const ProcessOptions& options) {
    std::vector<std::wstring> entries;
    if (wchar_t* block = GetEnvironmentStringsW(); block != nullptr) {
        for (const wchar_t* p = block; *p != L'\0';) {
            std::wstring entry(p);
            p += entry.size() + 1;
            entries.push_back(std::move(entry));
        }
        FreeEnvironmentStringsW(block);
    }
    for (const EnvChange& change : changes) {
        apply_wide_change(entries, widen(change.name),
                          change.value.has_value() ? std::optional<std::wstring>(widen(*change.value)) : std::nullopt);
    }
    for (const auto& [name, value] : options.wide_env) {
        apply_wide_change(entries, name, value);
    }
    // CreateProcessW expects the block sorted by name, case-insensitively.
    std::stable_sort(entries.begin(), entries.end(), [](const std::wstring& a, const std::wstring& b) {
        return compare_names(env_name(a), env_name(b)) == CSTR_LESS_THAN;
    });
    std::vector<wchar_t> block;
    for (const std::wstring& e : entries) {
        block.insert(block.end(), e.begin(), e.end());
        block.push_back(L'\0');
    }
    if (block.empty()) {
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}

HANDLE open_inheritable(const std::wstring& path, DWORD access, DWORD disposition) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE h = CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, disposition,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("run_process: cannot open a capture file");
    }
    return h;
}

ProcessResult run_platform(const std::string& program, const std::vector<std::string>& args, const std::string& cwd,
                           const std::vector<EnvChange>& env, const std::string& out_path,
                           const std::string& err_path, bool search_path, const ProcessOptions& options) {
    const std::wstring wprogram = backslashes(widen(program));
    std::wstring line;
    append_quoted(line, wprogram);
    for (const std::string& a : args) {
        line += L' ';
        append_quoted(line, widen(a));
    }
    for (const std::wstring& a : options.wide_args) {
        line += L' ';
        append_quoted(line, a);
    }
    std::vector<wchar_t> line_buffer(line.begin(), line.end());
    line_buffer.push_back(L'\0');
    std::vector<wchar_t> env_block = environment_block(env, options);
    const std::wstring wcwd = backslashes(widen(cwd));

    HANDLE h_out = open_inheritable(widen(options.stdout_path.value_or(out_path)), GENERIC_WRITE,
                                    options.stdout_path ? OPEN_ALWAYS : CREATE_ALWAYS);
    HANDLE h_err = open_inheritable(widen(err_path), GENERIC_WRITE, CREATE_ALWAYS);
    HANDLE h_in = open_inheritable(L"NUL", GENERIC_READ, OPEN_EXISTING);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = h_in;
    si.hStdOutput = h_out;
    si.hStdError = h_err;
    PROCESS_INFORMATION pi{};
    const BOOL started = CreateProcessW(search_path ? nullptr : wprogram.c_str(), line_buffer.data(), nullptr,
                                        nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                                        env_block.data(), wcwd.c_str(), &si, &pi);
    CloseHandle(h_out);
    CloseHandle(h_err);
    CloseHandle(h_in);
    if (started == FALSE) {
        throw std::runtime_error("run_process: CreateProcessW failed for " + program);
    }
    (void)WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    (void)GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    ProcessResult result;
    result.exit_code = static_cast<int>(code);
    return result;
}

#else  // POSIX

std::vector<std::string> child_environment(const std::vector<EnvChange>& changes) {
    std::vector<std::string> entries;
    for (char** p = environ; p != nullptr && *p != nullptr; ++p) {
        entries.emplace_back(*p);
    }
    for (const EnvChange& change : changes) {
        const std::string prefix = change.name + "=";
        std::erase_if(entries, [&](const std::string& e) { return e.compare(0, prefix.size(), prefix) == 0; });
        if (change.value.has_value()) {
            entries.push_back(prefix + *change.value);
        }
    }
    return entries;
}

ProcessResult run_platform(const std::string& program, const std::vector<std::string>& args, const std::string& cwd,
                           const std::vector<EnvChange>& env, const std::string& out_path,
                           const std::string& err_path, bool search_path, const ProcessOptions& options) {
    // Everything the child needs is prepared before fork, so that the child
    // only calls async-signal-safe functions.
    std::vector<std::string> arg_strings;
    arg_strings.push_back(program);
    arg_strings.insert(arg_strings.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (std::string& a : arg_strings) {
        argv.push_back(a.data());
    }
    argv.push_back(nullptr);
    std::vector<std::string> env_strings = child_environment(env);
    std::vector<char*> envp;
    for (std::string& e : env_strings) {
        envp.push_back(e.data());
    }
    envp.push_back(nullptr);

    const int fd_out = options.stdout_path
                           ? open(options.stdout_path->c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0644)
                           : open(out_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    const int fd_err = open(err_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    const int fd_in = open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (fd_out < 0 || fd_err < 0 || fd_in < 0) {
        throw std::runtime_error("run_process: cannot open a capture file");
    }

    const pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("run_process: fork failed");
    }
    if (pid == 0) {
        if (dup2(fd_in, 0) < 0 || dup2(fd_out, 1) < 0 || dup2(fd_err, 2) < 0) {
            _exit(126);
        }
        if (chdir(cwd.c_str()) != 0) {
            _exit(126);
        }
        if (search_path) {
            environ = envp.data();
            execvp(argv[0], argv.data());
        } else {
            execve(argv[0], argv.data(), envp.data());
        }
        _exit(127);
    }
    close(fd_out);
    close(fd_err);
    close(fd_in);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            throw std::runtime_error("run_process: waitpid failed");
        }
    }
    ProcessResult result;
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.signalled = true;
        result.exit_code = 128 + WTERMSIG(status);
    }
    return result;
}

#endif

}  // namespace

std::int64_t current_process_id() {
#if defined(_WIN32)
    return static_cast<std::int64_t>(GetCurrentProcessId());
#else
    return static_cast<std::int64_t>(getpid());
#endif
}

ProcessResult run_process(const std::string& program, const std::vector<std::string>& args, const std::string& cwd,
                          const std::vector<EnvChange>& env, const std::string& capture_dir, bool search_path,
                          const ProcessOptions& options) {
    const unsigned long long id = next_capture_id();
    const std::string out_path = capture_path(capture_dir, id, ".out");
    const std::string err_path = capture_path(capture_dir, id, ".err");
    ProcessResult result = run_platform(program, args, cwd, env, out_path, err_path, search_path, options);
    if (!options.stdout_path) {
        result.out = read_and_remove(out_path);
    }
    result.err = read_and_remove(err_path);
    return result;
}

}  // namespace clitest
