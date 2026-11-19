#pragma once

// The history settings as the pipeline sees them (spec P3-03 4.1, 4.2):
// HistoryParams in WorldParams, its eight `meta` keys and their inverse.
//
// WorldParams holds only resolved values (P0-04 10): no file positions and
// no unresolved race list. data::Settings is what loading produces;
// history_params_from turns it into HistoryParams once resolve_races has
// put the race list in race order.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/data/settings.hpp"

namespace worldgen::db {
class WorldReader;
}

namespace worldgen::history {

struct HistoryParams {
    std::int64_t span = data::DEFAULT_HISTORY_SPAN;  // 2000 ... 12000
    data::HistoryDetail detail = data::HistoryDetail::normal;
    std::optional<std::int64_t> civs_per_race;  // std::nullopt = auto; else 1 ... 4
    // The present races, in race order (P3-01 6.3). The default is the
    // design's roster in its fixed order (design 5); the CLI always replaces
    // it with resolve_races' result.
    std::vector<std::string> races = {"elf", "goblin", "dwarf", "faerie", "human", "orc"};
    data::EventIntensity event_intensity = data::EventIntensity::normal;
    data::MonsterPressure monster_pressure = data::MonsterPressure::normal;
    data::DivineActivity divine_activity = data::DivineActivity::normal;
    std::string divine_script;  // empty in P3; else 64 lower-case hex digits (P5)

    friend bool operator==(const HistoryParams&, const HistoryParams&) = default;
};

// HistoryParams of resolved settings (P3-03 4.1 step 1). FatalError
// ("history_params_from: the races are not resolved") unless settings.races
// holds a list and races_origins is empty, i.e. resolve_races ran.
[[nodiscard]] HistoryParams history_params_from(const data::Settings& settings);

// The problem with params as generate_world checks it (P3-03 4.1 step 2):
// the name of the first invalid field ("span", "civs_per_race", "races",
// "divine_script"), or an empty view when every field is valid. Checks:
// span in [2000, 12000]; civs_per_race unset or in [1, 4]; races non-empty,
// each in the P0-03 definition id grammar, none twice; divine_script empty
// or 64 lower-case hex digits. Whether the ids name loaded races, in race
// order, is the chronology stage's check (P3-03 C1).
[[nodiscard]] std::string_view history_params_problem(const HistoryParams& params);

// The eight keys of P3-03 4.2 with their canonical text, in ascending key
// order: param.civs_per_race, param.divine_activity, param.divine_script,
// param.event_intensity, param.history_detail, param.history_span,
// param.monster_pressure, param.races. The key is the full meta key.
// FatalError if history_params_problem(params) is not empty.
[[nodiscard]] std::vector<std::pair<std::string_view, std::string>> history_meta_entries(const HistoryParams& params);

// The inverse of history_meta_entries, from an open world (the reader has
// validated the eight values at open, P3-03 4.2). For resume tests and the
// history stage (H8). FatalError if a key is missing or not canonical
// (only possible with a reader that did not validate them).
[[nodiscard]] HistoryParams history_params_from_meta(const db::WorldReader& world);

}  // namespace worldgen::history
