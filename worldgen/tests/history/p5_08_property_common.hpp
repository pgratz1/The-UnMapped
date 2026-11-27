// Shared helpers of the P5-08 pantheon property tests (history_pantheon_property_{a,b,c}_test.cpp): the rows of a
// world file, the invariant check of 13 and the paths of 6 a world shows.

#pragma once

#include <doctest/doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "p5_08_api.hpp"
#include "p5_08_checks.hpp"

namespace pantheon_prop {

inline p508_check::World rows_of(const std::string& path) {
    const worldgen::db::WorldReader r = chrono_test::open_world(path);
    return p508_check::world_of_file(r);
}

inline void check_world(const std::string& path, const worldgen::data::ContentSet& content, const std::string& label) {
    CAPTURE(label);
    const p508_check::World w = rows_of(path);
    CHECK(p508_check::violations(w, p508_check::tuning_of(content)) == std::vector<std::string>{});
}

inline std::string stats_line(const p508_check::World& w) {
    std::string s;
    for (const auto& [k, v] : p508_check::stats(w)) s += " " + k + "=" + std::to_string(v);
    return s;
}

// The paths of 4.1's "every path of 6 occurs in a 512 world" that pantheon-small must show over seeds 0 to 9 (the
// reference's REQUIRED_PATHS; the others are reported).
inline const std::vector<std::string>& required_paths() {
    static const std::vector<std::string> v = {"bless", "curse", "champion", "monster", "mark", "founded",
                                               "cult_spread", "cult_adopted", "cult_suppressed", "prophet founder"};
    return v;
}

}  // namespace pantheon_prop
