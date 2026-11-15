#pragma once

// Shared helpers for the worldgen-cli integration tests (spec P0-04
// section 12).
//
// Every test runs the built binary inside a Sandbox: a fresh directory under
// the build tree (UNMAPPED_CLI_TEST_TMP_ROOT) holding
//   root/   the child's UNMAPPED_BUILD_ROOT
//   cwd/    the child's current directory, empty
//   io/     capture files for the child's stdout and stderr
//   files/  inputs and explicit --out targets of the test
// When the Sandbox is destroyed it checks (12.1) that cwd/ is still empty
// and that no *.partial file exists anywhere under the sandbox, then
// removes it.

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ostream>  // doctest needs the full type to print strings with the MSVC STL
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "process.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/db/utf8_path.hpp"

#ifndef UNMAPPED_CLI_TEST_TMP_ROOT
#error "UNMAPPED_CLI_TEST_TMP_ROOT must be defined by tools/worldgen-cli/tests/CMakeLists.txt"
#endif
#ifndef WORLDGEN_CLI_PATH
#error "WORLDGEN_CLI_PATH must be defined by tools/worldgen-cli/tests/CMakeLists.txt"
#endif
#ifndef WORLDGEN_CLI_TEST_HOOKS_PATH
#error "WORLDGEN_CLI_TEST_HOOKS_PATH must be defined by tools/worldgen-cli/tests/CMakeLists.txt"
#endif

namespace clitest {

namespace fs = std::filesystem;

using Bytes = std::vector<std::uint8_t>;

// Exit codes of P0-04 4.6.
inline constexpr int EXIT_OK = 0;
inline constexpr int EXIT_INTERNAL = 1;
inline constexpr int EXIT_USAGE = 2;
inline constexpr int EXIT_INPUT = 3;
inline constexpr int EXIT_OUTPUT = 4;
inline constexpr int EXIT_MISMATCH = 5;

// The content hash of a P0 world (P0-02 8) and the hint line (amendment 2).
inline constexpr std::string_view EMPTY_SHA256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
inline constexpr std::string_view USAGE_HINT = "worldgen-cli: run 'worldgen-cli [<command>] --help' for usage\n";

// "Ünmapped Wörld" as UTF-8 bytes, written as escapes so that the source
// encoding cannot matter.
inline constexpr std::string_view PHRASE_NON_ASCII = "\xC3\x9Cnmapped W\xC3\xB6rld";

inline std::string u8(const fs::path& p) { return worldgen::db::path_to_utf8(p); }
inline fs::path from_u8(std::string_view s) { return worldgen::db::path_from_utf8(s); }

inline Bytes read_bytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    Bytes bytes;
    if (in) {
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    return bytes;
}

inline std::string read_text(const fs::path& path) {
    const Bytes b = read_bytes(path);
    return {b.begin(), b.end()};
}

inline void write_text(const fs::path& path, std::string_view text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

inline bool file_exists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

inline std::string file_sha256_hex(const fs::path& path) { return worldgen::sha256_hex(read_bytes(path)); }

// Splits text at LF; a final LF does not start another line.
inline std::vector<std::string> lines_of(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t lf = text.find('\n', start);
        if (lf == std::string_view::npos) {
            lines.emplace_back(text.substr(start));
            break;
        }
        lines.emplace_back(text.substr(start, lf - start));
        start = lf + 1;
    }
    return lines;
}

inline bool starts_with(std::string_view text, std::string_view prefix) {
    return text.substr(0, prefix.size()) == prefix;
}

inline bool contains(std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; }

// The text a command prints for an output path: the absolute, lexically
// normalized path and a line feed (P0-04 5, 6, amendment 4).
inline std::string printed_path(const fs::path& absolute) { return u8(absolute.lexically_normal()) + "\n"; }

// Every *.partial file under dir (P0-04 4.4, 10).
inline std::vector<std::string> partial_files(const fs::path& dir) {
    std::vector<std::string> found;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = u8(it->path().filename());
        if (name.size() >= 8 && name.compare(name.size() - 8, 8, ".partial") == 0) {
            found.push_back(u8(it->path()));
        }
    }
    return found;
}

// IHDR of a PNG file (P0-04 12.3: signature, width, height, bit depth 8,
// colour type 2).
struct PngHeader {
    bool valid = false;  // signature and IHDR chunk present
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint8_t bit_depth = 0;
    std::uint8_t colour_type = 0;
};

inline PngHeader png_header(const Bytes& b) {
    static constexpr std::uint8_t SIG[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    PngHeader h;
    if (b.size() < 8 + 8 + 13) {
        return h;
    }
    for (std::size_t i = 0; i < 8; ++i) {
        if (b[i] != SIG[i]) {
            return h;
        }
    }
    auto be32 = [&](std::size_t off) {
        return (static_cast<std::uint32_t>(b[off]) << 24U) | (static_cast<std::uint32_t>(b[off + 1]) << 16U) |
               (static_cast<std::uint32_t>(b[off + 2]) << 8U) | static_cast<std::uint32_t>(b[off + 3]);
    };
    if (be32(8) != 13 || b[12] != 'I' || b[13] != 'H' || b[14] != 'D' || b[15] != 'R') {
        return h;
    }
    h.valid = true;
    h.width = be32(16);
    h.height = be32(20);
    h.bit_depth = b[24];
    h.colour_type = b[25];
    return h;
}

// Names of PNG chunks in file order (for the "no time, text or gamma
// chunks" rule of P0-04 6).
inline std::vector<std::string> png_chunk_types(const Bytes& b) {
    std::vector<std::string> types;
    std::size_t off = 8;
    while (off + 12 <= b.size()) {
        const std::uint32_t len = (static_cast<std::uint32_t>(b[off]) << 24U) |
                                  (static_cast<std::uint32_t>(b[off + 1]) << 16U) |
                                  (static_cast<std::uint32_t>(b[off + 2]) << 8U) | static_cast<std::uint32_t>(b[off + 3]);
        types.emplace_back(reinterpret_cast<const char*>(&b[off + 4]), 4);
        off += 12 + static_cast<std::size_t>(len);
    }
    return types;
}

// Options of one run.
struct RunOptions {
    std::vector<EnvChange> env;   // applied after UNMAPPED_BUILD_ROOT is set
    std::optional<fs::path> cwd;  // default: the sandbox's empty cwd/
    ProcessOptions process;       // stdout redirection, raw UTF-16 (Windows)
};

class Sandbox {
public:
    Sandbox() {
        static unsigned counter = 0;
        base_ = from_u8(UNMAPPED_CLI_TEST_TMP_ROOT) /
                from_u8("t" + std::to_string(++counter) + "-" + std::to_string(current_process_id()));
        std::error_code ec;
        fs::remove_all(base_, ec);
        root_ = base_ / "root";
        cwd_ = base_ / "cwd";
        io_ = base_ / "io";
        files_ = base_ / "files";
        for (const fs::path* p : {&root_, &cwd_, &io_, &files_}) {
            fs::create_directories(*p);
        }
    }
    ~Sandbox() {
        std::error_code ec;
        std::vector<std::string> left;
        for (fs::directory_iterator it(cwd_, ec), end; !ec && it != end; it.increment(ec)) {
            left.push_back(u8(it->path().filename()));
        }
        CHECK_MESSAGE(left.empty(), "the current directory of the CLI is not empty: ", join(left));
        const std::vector<std::string> partial = partial_files(base_);
        CHECK_MESSAGE(partial.empty(), ".partial files left behind: ", join(partial));
        fs::remove_all(base_, ec);
    }
    Sandbox(const Sandbox&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;

    [[nodiscard]] const fs::path& root() const noexcept { return root_; }
    [[nodiscard]] const fs::path& cwd() const noexcept { return cwd_; }
    [[nodiscard]] const fs::path& files() const noexcept { return files_; }
    [[nodiscard]] fs::path out_dir() const { return root_ / "out"; }
    // A path under files/ (name is UTF-8, may contain '/').
    [[nodiscard]] fs::path file(std::string_view name) const { return (files_ / from_u8(name)).lexically_normal(); }

    // Runs worldgen-cli with UNMAPPED_BUILD_ROOT = root/ and the current
    // directory cwd/ (P0-04 12).
    [[nodiscard]] ProcessResult cli(const std::vector<std::string>& args, const RunOptions& options = {}) const {
        return run(WORLDGEN_CLI_PATH, args, options);
    }
    // The same with the test-hook build (P0-04 amendment 11).
    [[nodiscard]] ProcessResult hooks(const std::vector<std::string>& args, const RunOptions& options = {}) const {
        return run(WORLDGEN_CLI_TEST_HOOKS_PATH, args, options);
    }
    // Any other program, searched on PATH, in cwd/.
    [[nodiscard]] ProcessResult tool(const std::string& program, const std::vector<std::string>& args) const {
        return run_process(program, args, u8(cwd_), {}, u8(io_), true);
    }

    // generate with the given arguments; REQUIREs exit 0 and returns the
    // written path as printed (without the line feed).
    fs::path generate(std::vector<std::string> args) const {
        args.insert(args.begin(), "generate");
        const ProcessResult r = cli(args);
        INFO("stderr: ", r.err);
        REQUIRE(r.exit_code == EXIT_OK);
        REQUIRE(!r.out.empty());
        REQUIRE(r.out.back() == '\n');
        return from_u8(std::string_view(r.out).substr(0, r.out.size() - 1));
    }

private:
    static std::string join(const std::vector<std::string>& items) {
        std::string s;
        for (const std::string& i : items) {
            s += i;
            s += ' ';
        }
        return s;
    }
    [[nodiscard]] ProcessResult run(const char* program, const std::vector<std::string>& args,
                                    const RunOptions& options) const {
        std::vector<EnvChange> env;
        env.push_back({"UNMAPPED_BUILD_ROOT", u8(root_)});
        env.insert(env.end(), options.env.begin(), options.env.end());
        const fs::path& dir = options.cwd ? *options.cwd : cwd_;
        return run_process(program, args, u8(dir), env, u8(io_), false, options.process);
    }

    fs::path base_;
    fs::path root_;
    fs::path cwd_;
    fs::path io_;
    fs::path files_;
};

}  // namespace clitest
