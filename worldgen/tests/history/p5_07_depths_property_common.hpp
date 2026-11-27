// Shared helpers of the P5-07 depths property tests (history_depths_property_{a,b,c}_test.cpp): the rows of a world
// file, the invariant check, the stats line and the paths of spec 6 a world shows. Moved unchanged from
// history_depths_property_test.cpp when that file was split for the CTest shards.

#pragma once

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p3_04_engine_api.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_07_api.hpp"
#include "p5_07_checks.hpp"
#include "worldgen/db/testing.hpp"

namespace depths_prop {

namespace db = worldgen::db;
namespace wd = worldgen::data;

inline p503check::World rows_of(const std::string& path, const wd::ContentSet& content) {
    const db::WorldReader r = chrono_test::open_world(path);
    return p503check::world_of_file(r, content);
}

inline void check_world(const std::string& path, const wd::ContentSet& content, const std::string& label) {
    CAPTURE(label);
    const p503check::World w = rows_of(path, content);
    CHECK(p507_check::violations(w, p507_check::tuning_of(content)) == std::vector<std::string>{});
}

inline std::string stats_line(const p503check::World& w) {
    std::string s;
    for (const auto& [k, v] : p507_check::stats(w)) s += " " + k + "=" + std::to_string(v);
    return s;
}

// The paths of 6 a world shows.
inline std::set<std::string> paths_of(const p503check::World& w) {
    const p503check::Index x(w);
    std::set<std::string> out;
    if (!x.events_of("delving_deepened").empty()) out.insert("deepened");
    for (const std::int64_t e : x.events_of("delving_deepened")) {
        if (x.val(e, 0) == 3) out.insert("depth 3");
    }
    if (!x.events_of("deep_woken").empty()) out.insert("woken");
    if (!x.events_of("deep_sealed").empty()) out.insert("sealed");
    for (const std::int64_t e : x.events_of("hold_lost")) {
        const std::vector<std::int64_t> cs = x.cause_list(e);
        out.insert(cs.size() == 1U && x.kind_of(cs[0]) == "monster_raid" ? "later take" : "fall at the waking");
    }
    for (const std::int64_t e : x.events_of("refugees_settled")) {
        const std::vector<std::int64_t> cs = x.cause_list(e);
        if (cs.size() == 1U && x.kind_of(cs[0]) == "hold_lost") out.insert("flight");
    }
    for (const std::int64_t e : x.events_of("civ_ended")) {
        const std::vector<std::int64_t> cs = x.cause_list(e);
        if (cs.size() == 1U && x.kind_of(cs[0]) == "hold_lost") out.insert("realm lost");
    }
    // r2 paths
    if (!x.events_of("warren_delved").empty()) out.insert("warren delved");
    for (const std::int64_t e : x.events_of("warren_delved")) {
        if (x.val(e, 0) == 3) out.insert("warren depth 3");
    }
    std::set<std::int64_t> warren_m;
    for (const std::int64_t e : x.events_of("deep_woken")) {
        if (x.part(e, 3) == 0) {
            out.insert("warren woken");
            warren_m.insert(x.part(e, 1));
        }
    }
    if (!x.events_of("warren_sealed").empty()) out.insert("warren sealed");
    for (const std::int64_t e : x.events_of("hold_lost")) {
        if (warren_m.count(x.part(e, 1)) != 0U) out.insert("warren lost");
    }
    std::map<std::int64_t, int> aids;
    for (const std::int64_t e : x.events_of("ally_aided_hold")) {
        out.insert("aid");
        ++aids[x.cause_list(e).front()];
    }
    for (const auto& [w0, k] : aids) {
        if (k >= 2) out.insert("two aids");
    }
    for (const std::int64_t e : x.events_of("region_renamed")) {
        for (const std::int64_t c : x.cause_list(e)) {
            if (x.kind_of(c) == "hold_lost") out.insert("renamed by rule 8");
        }
    }
    return out;
}

}  // namespace depths_prop
