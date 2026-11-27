#pragma once
// The history data directories every data root needs from P3-01 on (the
// required kinds race, culture and history, P3-01 sections 3 to 5), for the
// tests of earlier stages that build their own data roots. Copied from the
// committed fixture root worldgen/tests/data/geo, read in place (never
// written).
//
// Added by the generator engineer with P3-01 so that the P1 tests keep
// loading; the P3-01 tests themselves are the test writer's.
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/data/schema.hpp"

#ifndef UNMAPPED_HISTORY_FIXTURE_DIR
#error "UNMAPPED_HISTORY_FIXTURE_DIR must name worldgen/tests/data/geo"
#endif

namespace history_fixture {

// The directories of the history kinds, (P3-08) of the name kinds
// placename and name_filter and (P5-02 4.1, 4.2) of the required kinds beast
// and monster, in this order.
inline constexpr std::string_view DIRECTORIES[] = {"beasts",       "cultures",   "gods",  "history",
                                                   "monsters",     "name_filters", "placenames", "races"};

inline std::filesystem::path fixture_root() {
    return std::filesystem::path(UNMAPPED_HISTORY_FIXTURE_DIR);
}

inline std::string read_all(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// The files of one directory of the fixture, in name order.
inline std::vector<std::filesystem::path> files_of(std::string_view dir) {
    std::vector<std::filesystem::path> out;
    for (const auto& e : std::filesystem::directory_iterator(fixture_root() / std::string(dir))) {
        out.push_back(e.path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

// True when the production registry has the kind `race` (P3-01).
inline bool registered() {
    return worldgen::data::builtin_kinds().find(worldgen::data::RACE_KIND) != nullptr;
}

// Adds the three directories to an in-memory root that lacks them.
inline void add_to(worldgen::data::MemoryDataRoot& root) {
    if (!registered()) {
        return;
    }
    for (const std::string_view dir : DIRECTORIES) {
        bool present = false;
        for (const worldgen::data::MemoryDirectory& d : root.directories) {
            present = present || d.name == dir;
        }
        if (present) {
            continue;
        }
        worldgen::data::MemoryDirectory d{std::string(dir), {}};
        for (const std::filesystem::path& p : files_of(dir)) {
            d.entries.push_back(worldgen::data::MemoryEntry{p.filename().string(), read_all(p), false});
        }
        root.directories.push_back(std::move(d));
    }
}

// Copies the three directories into a data root on disk that lacks them.
inline void copy_to(const std::filesystem::path& root) {
    if (!registered()) {
        return;
    }
    for (const std::string_view dir : DIRECTORIES) {
        const std::filesystem::path target = root / std::string(dir);
        if (std::filesystem::exists(target)) {
            continue;
        }
        std::filesystem::create_directories(target);
        for (const std::filesystem::path& p : files_of(dir)) {
            std::filesystem::copy_file(p, target / p.filename());
        }
    }
}

}  // namespace history_fixture
