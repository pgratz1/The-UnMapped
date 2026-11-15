#pragma once
// The world settings file (spec P0-03 section 6).
//
//   [world]
//   seed = "18446744073709551615"   # or an integer 0 .. 2^63-1, or "0x..." hex
//   # seed_phrase = "The Unmapped"  # alternative to seed
//   map_size = 1024                 # 512, 1024 or 2048
//
// Every key is optional. Command-line flags override the file key by key;
// that merge is done by the caller (worldgen-cli, P0-04).
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "worldgen/data/load_error.hpp"

namespace worldgen::data {

// Default of world.map_size (P0-03 section 6).
inline constexpr std::int64_t DEFAULT_MAP_SIZE = 1024;

// World-creation settings (P0-03 sections 3, 6).
struct Settings {
    // The world seed. Set from world.seed, or derived from world.seed_phrase
    // (seed_from_phrase, P0-01 7.3). Empty when neither key is given.
    std::optional<std::uint64_t> seed;
    // The trimmed phrase when the seed came from world.seed_phrase; empty
    // otherwise.
    std::optional<std::string> seed_phrase;
    // world.map_size: 512, 1024 or 2048; DEFAULT_MAP_SIZE when absent.
    std::int64_t map_size = DEFAULT_MAP_SIZE;

    friend bool operator==(const Settings&, const Settings&) = default;
};

// Parses the string form of a seed (P0-03 section 6, also usable for the
// command line's --seed): decimal `0|[1-9][0-9]*` up to
// 18446744073709551615, or "0x" followed by 1 to 16 hex digits of either
// case. No sign, no whitespace, no underscores, no "0X". std::nullopt for
// anything else.
[[nodiscard]] std::optional<std::uint64_t> parse_seed_text(std::string_view text) noexcept;

// True for 512, 1024 and 2048 (P0-03 section 6).
[[nodiscard]] bool is_valid_map_size(std::int64_t cells) noexcept;

// Reads and validates a settings file (P0-03 sections 5, 6): binary read,
// at most 1 MiB, same parsing, numeric and string rules as data files,
// only the keys world.seed, world.seed_phrase, world.map_size, at most one
// of the two seed keys. Errors carry the path as given (its UTF-8 form,
// native separators). A file that cannot be opened is an error.
[[nodiscard]] LoadResult<Settings> load_settings_file(const std::filesystem::path& path);

// Same, from text in memory (for tests and callers that already hold the
// bytes); display_path is used as the error path.
[[nodiscard]] LoadResult<Settings> parse_settings(std::string_view display_path, std::string_view bytes);

}  // namespace worldgen::data
