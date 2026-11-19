#pragma once
// The world settings file (spec P0-03 section 6).
//
//   [world]
//   seed = "18446744073709551615"   # or an integer 0 .. 2^63-1, or "0x..." hex
//   # seed_phrase = "The Unmapped"  # alternative to seed
//   map_size = 1024                 # 512, 1024 or 2048
//   history_span = 6000             # and the other history keys of P3-01 6.1
//
// Every key is optional. Command-line flags override the file key by key;
// that merge is done by the caller (worldgen-cli, P0-04).
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/data/load_error.hpp"

namespace worldgen::data {

struct ContentSet;  // worldgen/data/content.hpp

// Default of world.map_size (P0-03 section 6).
inline constexpr std::int64_t DEFAULT_MAP_SIZE = 1024;

// --- History settings (P3-01 section 6) ---------------------------------------

// world.history_span: 2000 to 12000 years, default 6000.
inline constexpr std::int64_t DEFAULT_HISTORY_SPAN = 6000;
inline constexpr std::int64_t MIN_HISTORY_SPAN = 2000;
inline constexpr std::int64_t MAX_HISTORY_SPAN = 12000;
// world.civs_per_race: "auto" or 1 to MAX_CIVS_PER_RACE.
inline constexpr std::int64_t MIN_CIVS_PER_RACE = 1;
inline constexpr std::int64_t MAX_CIVS_PER_RACE = 4;

enum class HistoryDetail : std::uint8_t { sparse, normal, dense };
enum class EventIntensity : std::uint8_t { calm, normal, turbulent };
enum class MonsterPressure : std::uint8_t { low, normal, high };
enum class DivineActivity : std::uint8_t { distant, normal, meddling };

// The value text of each enum (P3-01 6.1, 6.3: also the meta value text),
// and its inverse (std::nullopt for any other text; exact bytes).
[[nodiscard]] std::string_view history_detail_name(HistoryDetail v) noexcept;
[[nodiscard]] std::string_view event_intensity_name(EventIntensity v) noexcept;
[[nodiscard]] std::string_view monster_pressure_name(MonsterPressure v) noexcept;
[[nodiscard]] std::string_view divine_activity_name(DivineActivity v) noexcept;
[[nodiscard]] std::optional<HistoryDetail> parse_history_detail(std::string_view text) noexcept;
[[nodiscard]] std::optional<EventIntensity> parse_event_intensity(std::string_view text) noexcept;
[[nodiscard]] std::optional<MonsterPressure> parse_monster_pressure(std::string_view text) noexcept;
[[nodiscard]] std::optional<DivineActivity> parse_divine_activity(std::string_view text) noexcept;

// Where a settings-file value was written: the settings path as given, and
// its 1-based line and column. Kept for the checks that run after the data
// is loaded (world.races, P3-01 6.2).
struct ValueOrigin {
    std::string path;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

// World-creation settings (P0-03 sections 3, 6; P3-01 section 6).
struct Settings {
    // The world seed. Set from world.seed, or derived from world.seed_phrase
    // (seed_from_phrase, P0-01 7.3). Empty when neither key is given.
    std::optional<std::uint64_t> seed;
    // The scrubbed phrase (scrub_phrase, P0-01 7.3) when the seed came from
    // world.seed_phrase; empty otherwise.
    std::optional<std::string> seed_phrase;
    // world.map_size: 512, 1024 or 2048; DEFAULT_MAP_SIZE when absent.
    std::int64_t map_size = DEFAULT_MAP_SIZE;

    // P3-01 6.1. Stored in meta from P3-03 on (6.3); unused before.
    std::int64_t history_span = DEFAULT_HISTORY_SPAN;
    HistoryDetail history_detail = HistoryDetail::normal;
    // 1 to 4, or std::nullopt for "auto".
    std::optional<std::int64_t> civs_per_race;
    // std::nullopt for "all"; otherwise distinct race ids, as written until
    // resolve_races puts them in race order (P3-01 6.1).
    std::optional<std::vector<std::string>> races;
    EventIntensity event_intensity = EventIntensity::normal;
    MonsterPressure monster_pressure = MonsterPressure::normal;
    DivineActivity divine_activity = DivineActivity::normal;
    // The divine script's SHA-256 (P3-01 6.3): always empty, since a script
    // is refused until P5.
    std::string divine_script;

    // Where each element of `races` was written, when it came from a
    // settings file (one entry per element); empty otherwise. Not part of
    // the value: operator== ignores it.
    std::vector<ValueOrigin> races_origins;

    friend bool operator==(const Settings& a, const Settings& b) noexcept;
};

// True for 2000 to 12000 (P3-01 6.1).
[[nodiscard]] bool is_valid_history_span(std::int64_t years) noexcept;

// The --races text (P3-01 6.1, 6.2): "all" gives std::nullopt in `races`;
// otherwise a comma-separated list of ids (no spaces), each in the id
// grammar and given once. `ok` is false for an empty list, an empty item,
// an item that is not an id, or a repeated id.
struct RacesText {
    bool ok = false;
    std::optional<std::vector<std::string>> races;
};
[[nodiscard]] RacesText parse_races_text(std::string_view text);

// The ids of the loaded `race` definitions in race order (ascending
// `order`, ties by id; the loader makes orders unique).
[[nodiscard]] std::vector<std::string> race_ids_in_order(const ContentSet& content);

// The errors of a settings file's world.races that need the loaded data
// (P3-01 6.2): for each element that names no race definition, at its
// position, "'world.races[<i>]' is \"<id>\", which is not a race id;
// expected one of <ids in race order, ', ' separated>". Empty when races
// did not come from a file (races_origins empty) or every id is known.
[[nodiscard]] std::vector<LoadError> check_settings_races(const Settings& settings, const ContentSet& content);

// Puts settings.races in race order: "all" (std::nullopt) becomes every
// loaded race, a list keeps its ids in race order (P3-01 6.1: the order
// given does not matter). Clears races_origins. Fatal if an id names no
// race definition (callers check first).
void resolve_races(Settings& settings, const ContentSet& content);

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
// only the keys world.seed, world.seed_phrase, world.map_size and the
// history keys of P3-01 6.1, at most one of the two seed keys;
// world.divine_script is always an error (P3-01 6.2). Race ids in
// world.races are checked against the data later (check_settings_races). Errors carry the path as given (its UTF-8 form,
// native separators). A file that cannot be opened is an error.
[[nodiscard]] LoadResult<Settings> load_settings_file(const std::filesystem::path& path);

// Same, from text in memory (for tests and callers that already hold the
// bytes); display_path is used as the error path.
[[nodiscard]] LoadResult<Settings> parse_settings(std::string_view display_path, std::string_view bytes);

}  // namespace worldgen::data
