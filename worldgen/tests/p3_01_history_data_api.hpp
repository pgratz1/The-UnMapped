#pragma once

// The one place where the P3-01 tests touch API that spec P3-01 requires but
// does not spell out in C++. Written from the spec before the implementation
// (test-writer rule); integration adapts this file only.
//
// What the spec fixes and what it leaves open:
//   - 6.3: "Settings carries the eight values from P3-01 on": the field
//     names and types are not given.
//   - 6.2: the ids of `world.races` are "checked after the data is loaded,
//     still reported at the value's position": no function is named.
//   - 8.1 test 8: "`all` gives every loaded race in race order": the
//     resolution against the loaded data is not named either.
//
// The tests see only the test-owned view below: History, the eight values
// in the text form of the meta keys of 6.3 (param.history_span and so on,
// races resolved to the present ids in race order, comma separated), and
// two functions. Everything else they use is the existing P0-03 API
// (worldgen::data::parse_settings, load_content, load_content_from_memory,
// builtin_kinds, content_hash_hex).
//
// INTEGRATION: the body of each function below assumes these names (the
// test writer's guess, marked "ASSUMED"); replace them with the engineer's:
//   Settings::history_span       std::int64_t                          default 6000
//   Settings::history_detail     std::string                           "sparse" | "normal" | "dense"
//   Settings::civs_per_race      std::optional<std::int64_t>           std::nullopt = auto
//   Settings::races              std::optional<std::vector<std::string>> std::nullopt = all
//   Settings::event_intensity    std::string                           "calm" | "normal" | "turbulent"
//   Settings::monster_pressure   std::string                           "low" | "normal" | "high"
//   Settings::divine_activity    std::string                           "distant" | "normal" | "meddling"
//   worldgen::data::check_settings_races(const Settings&, const ContentSet&, std::string_view display_path)
//       -> std::vector<LoadError>     the post-load check of world.races (6.2), at the values' positions
//   worldgen::data::present_races(const Settings&, const ContentSet&)
//       -> std::vector<std::string>   the present race ids in race order (6.1, 6.3)

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/settings.hpp"

namespace history_api {

namespace wd = worldgen::data;

// The eight world-creation settings of P3-01 6.1, in the value text of the
// meta keys of 6.3 (param.divine_script is always empty in P3-01).
struct History {
    std::string history_span;      // decimal, e.g. "6000"
    std::string history_detail;    // sparse | normal | dense
    std::string civs_per_race;     // auto | 1 .. 4
    std::string races;             // present ids in race order, comma separated
    std::string event_intensity;   // calm | normal | turbulent
    std::string monster_pressure;  // low | normal | high
    std::string divine_activity;   // distant | normal | meddling
    std::string divine_script;     // empty until P5

    friend bool operator==(const History&, const History&) = default;
};

// The spec's defaults (6.1) with the six shipped races present.
inline History defaults() {
    return History{"6000", "normal", "auto", "elf,goblin,dwarf,faerie,human,orc", "normal", "normal", "normal", ""};
}

// The settings of `s`, with its races resolved against the loaded `content`.
inline History history_of(const wd::Settings& s, const wd::ContentSet& content) {
    History h;
    h.history_span = std::to_string(s.history_span);                                         // ASSUMED
    h.history_detail = std::string(s.history_detail);                                        // ASSUMED
    h.civs_per_race = s.civs_per_race.has_value() ? std::to_string(*s.civs_per_race) : "auto";  // ASSUMED
    for (const std::string& id : wd::present_races(s, content)) {                            // ASSUMED
        h.races += (h.races.empty() ? "" : ",") + id;
    }
    h.event_intensity = std::string(s.event_intensity);    // ASSUMED
    h.monster_pressure = std::string(s.monster_pressure);  // ASSUMED
    h.divine_activity = std::string(s.divine_activity);    // ASSUMED
    h.divine_script = "";
    return h;
}

// A settings file as `generate` reads it (P3-01 6.2): parsed, and once the
// data is loaded its race ids checked against it. Errors sorted as P0-03 9.
inline wd::LoadResult<wd::Settings> parse_settings_with_data(std::string_view display_path, std::string_view text,
                                                             const wd::ContentSet& content) {
    wd::LoadResult<wd::Settings> r = wd::parse_settings(display_path, text);
    if (!r.ok()) {
        return r;
    }
    std::vector<wd::LoadError> errors = wd::check_settings_races(*r.value, content, display_path);  // ASSUMED
    if (!errors.empty()) {
        wd::sort_errors(errors);
        wd::LoadResult<wd::Settings> failed;
        failed.errors = std::move(errors);
        return failed;
    }
    return r;
}

}  // namespace history_api
