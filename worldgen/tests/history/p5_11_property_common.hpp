// Shared helpers of the P5-11 artifacts property tests (history_artifacts_property_{a,b}_test.cpp) and the reader,
// report and golden tests: the rows of a world file, the invariant check of 13 and the paths of 6 a world shows.

#pragma once

#include <doctest/doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "p5_08_property_common.hpp"
#include "p5_11_api.hpp"
#include "p5_11_checks.hpp"

namespace artifacts_prop {

inline p511_check::World rows_of(const std::string& path) {
    const worldgen::db::WorldReader r = chrono_test::open_world(path);
    return p511_check::world_of_file(r);
}

// 13 items 1 to 8 (rows) and item 9: P5-08's invariants on the same world (p5_08_checks.hpp), with smiths as figures.
inline void check_world(const std::string& path, const worldgen::data::ContentSet& content, const std::string& label) {
    CAPTURE(label);
    const p511_check::World w = rows_of(path);
    CHECK(p511_check::violations(w, p511_check::tuning_of(content)) == std::vector<std::string>{});
    CHECK(p508_check::violations(w.base, p508_check::tuning_of(content)) == std::vector<std::string>{});
}

inline std::string stats_line(const p511_check::World& w) {
    std::string s;
    for (const auto& [k, v] : p511_check::stats(w)) s += " " + k + "=" + std::to_string(v);
    return s;
}

// The paths artifacts-small must show over seeds 0 to 9 at 512 (4.6: "every path of 6 occurs in a 512 world"; the
// reference's REQUIRED_PATHS; the others are reported).
inline const std::vector<std::string>& required_paths() {
    static const std::vector<std::string> v = {
        "maker 1",       "maker 2",       "maker 3",          "maker 4",          "class arms",
        "class armour",  "class regalia", "class relic",      "class staff",      "first kept",
        "first borne",   "first enshrined", "artifact_taken", "artifact_lost",    "artifact_hoarded",
        "artifact_recovered", "artifact_buried"};
    return v;
}
inline const std::vector<std::string>& reported_paths() {
    static const std::vector<std::string> v = {
        "deep",
        "move general_appointed kept>borne quiet",
        "move figure_died borne>enshrined quiet",
        "move figure_died borne>kept quiet",
        "move figure_died borne>buried",
        "move fell_in_battle borne>kept",
        "move fell_in_battle borne>lost",
        "move hero_slain borne>hoarded",
        "move monster_slain_by_hero hoarded>borne",
        "move monster_slain_by_hero hoarded>kept",
        "move monster_slain hoarded>kept",
        "move monster_died hoarded>lost quiet",
        "move monster_raid kept>hoarded",
        "move hold_lost kept>hoarded",
        "move sacked kept>kept",
        "move sacked kept>lost",
        "move settlement_captured kept>kept",
        "move civ_conquered kept>kept",
        "move civ_ended kept>lost",
        "move civ_ended kept>kept",
        "move peace kept>kept",
        "move war_won kept>kept",
        "move peace borne>kept",
        "move cult_suppressed enshrined>enshrined quiet",
        "move cult_suppressed enshrined>kept quiet"};
    return v;
}

}  // namespace artifacts_prop
