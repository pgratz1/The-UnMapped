#pragma once

// Loading helpers for the P3-01 library tests: a data root made of the
// shipped biomes/ and worldgen/ (read from game/data) and the history files
// of p3_01_history_data.hpp, loaded through the production registry; the
// shipped and fixture roots on disk; the golden file p3-01-data.txt.
// Existing P0-03 API only.

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p3_01_history_data.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/schema.hpp"

#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif
#ifndef UNMAPPED_P3_01_GOLDEN_PATH
#error "UNMAPPED_P3_01_GOLDEN_PATH must name worldgen/tests/golden/p3-01-data.txt"
#endif

namespace history_test {

namespace wd = worldgen::data;
using history_data::Files;

inline std::filesystem::path source_root() { return std::filesystem::path(UNMAPPED_TEST_SOURCE_ROOT); }

inline std::string read_text(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << p.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// One directory of a root on disk, as a MemoryDirectory (names sorted).
inline wd::MemoryDirectory directory_from_disk(const std::filesystem::path& root, const std::string& name) {
    wd::MemoryDirectory d{name, {}};
    for (const auto& entry : std::filesystem::directory_iterator(root / name)) {
        d.entries.push_back(wd::MemoryEntry{entry.path().filename().string(), read_text(entry.path()), false});
    }
    std::sort(d.entries.begin(), d.entries.end(),
              [](const wd::MemoryEntry& a, const wd::MemoryEntry& b) { return a.name < b.name; });
    return d;
}

// biomes/ and worldgen/ of the shipped game/data, read once per process.
inline const std::vector<wd::MemoryDirectory>& geography_directories() {
    static const std::vector<wd::MemoryDirectory> dirs = {
        directory_from_disk(source_root() / "game" / "data", "biomes"),
        directory_from_disk(source_root() / "game" / "data", "worldgen"),
    };
    return dirs;
}

struct Load {
    std::optional<wd::ContentSet> content;
    std::vector<wd::LoadError> errors;
    [[nodiscard]] bool ok() const noexcept { return content.has_value(); }
};

inline Load to_load(wd::LoadResult<wd::ContentSet> r) {
    Load out;
    out.errors = std::move(r.errors);
    if (r.value.has_value()) {
        out.content = std::move(*r.value);
    }
    return out;
}

// The shipped geography directories plus `files` (races/, cultures/,
// history/ as given; a directory with no file in `files` is missing).
inline Load load(const Files& files) {
    wd::MemoryDataRoot root;
    root.directories = geography_directories();
    for (wd::MemoryDirectory& d : history_data::directories(files)) {
        root.directories.push_back(std::move(d));
    }
    return to_load(wd::load_content_from_memory(wd::builtin_kinds(), root));
}

inline Load load_disk(const std::filesystem::path& root) {
    return to_load(wd::load_content(wd::builtin_kinds(), root));
}

inline std::string dump(const Load& l) {
    std::string s;
    for (const wd::LoadError& e : l.errors) {
        s += e.to_string() + "\n";
    }
    return s;
}

inline bool contains(std::string_view s, std::string_view part) { return s.find(part) != std::string_view::npos; }

// The content of the spec listings (REQUIREs that it loads).
inline const wd::ContentSet& shipped_content() {
    static const wd::ContentSet c = [] {
        Load l = load(history_data::shipped_files());
        REQUIRE_MESSAGE(l.ok(), dump(l));
        return std::move(*l.content);
    }();
    return c;
}

// The golden file of P3-01 8.3 test 9: "<root> <hex>" per line.
inline std::map<std::string, std::string> golden_hashes() {
    std::map<std::string, std::string> out;
    std::istringstream in(read_text(std::filesystem::path(UNMAPPED_P3_01_GOLDEN_PATH)));
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::size_t sp = line.find(' ');
        REQUIRE(sp != std::string::npos);
        out[line.substr(0, sp)] = line.substr(sp + 1);
    }
    return out;
}

}  // namespace history_test
