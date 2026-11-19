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
//   - 8.2 test 8: "`all` gives every loaded race in race order": the
//     resolution against the loaded data is not named either, nor is a
//     library parser of the --races flag text (6.1).
//
// The tests see only the test-owned view below: History, the eight values
// in the text form of the meta keys of 6.3 (param.history_span and so on,
// races resolved to the present ids in race order, comma separated), and
// three functions. Everything else they use is the existing P0-03 API
// (worldgen::data::parse_settings, load_content, load_content_from_memory,
// builtin_kinds, content_hash_hex).
//
// Integration (done after the tests were drafted, against branch
// p3/01-history-data at 65833c4): the engineer's names, used only below:
//   Settings::history_span (int64), history_detail (HistoryDetail),
//   civs_per_race (optional<int64>, nullopt = auto), races
//   (optional<vector<string>>, nullopt = all), event_intensity,
//   monster_pressure, divine_activity (enums) and their *_name() texts;
//   check_settings_races(const Settings&, const ContentSet&) for the
//   post-load race check; resolve_races(Settings&, const ContentSet&) for
//   race order; parse_races_text(text) for the --races flag text.
// Until that branch is merged, this header does not compile on main.

#include <cstdint>
#include <optional>
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
    h.history_span = std::to_string(s.history_span);
    h.history_detail = std::string(wd::history_detail_name(s.history_detail));
    h.civs_per_race = s.civs_per_race.has_value() ? std::to_string(*s.civs_per_race) : "auto";
    wd::Settings resolved = s;
    wd::resolve_races(resolved, content);
    if (resolved.races.has_value()) {
        for (const std::string& id : *resolved.races) {
            h.races += (h.races.empty() ? "" : ",") + id;
        }
    }
    h.event_intensity = std::string(wd::event_intensity_name(s.event_intensity));
    h.monster_pressure = std::string(wd::monster_pressure_name(s.monster_pressure));
    h.divine_activity = std::string(wd::divine_activity_name(s.divine_activity));
    h.divine_script = s.divine_script;
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
    std::vector<wd::LoadError> errors = wd::check_settings_races(*r.value, content);
    if (!errors.empty()) {
        wd::sort_errors(errors);
        wd::LoadResult<wd::Settings> failed;
        failed.errors = std::move(errors);
        return failed;
    }
    return r;
}

// The --races flag text (P3-01 6.1, 6.2) as the races of a Settings value,
// resolved against `content` into the param.races text of 6.3; std::nullopt
// when the text is not a valid form (the usage error of 6.2). Ids are
// assumed known (the unknown-id error is the CLI's, tested there).
inline std::optional<std::string> races_from_flag(std::string_view text, const wd::ContentSet& content) {
    const wd::RacesText parsed = wd::parse_races_text(text);
    if (!parsed.ok) {
        return std::nullopt;
    }
    wd::Settings s;
    s.races = parsed.races;
    return history_of(s, content).races;
}

}  // namespace history_api
