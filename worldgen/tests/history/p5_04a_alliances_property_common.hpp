// Shared helpers of the P5-04a alliances property tests (history_alliances_p504a_property_{a,b}_test.cpp), moved
// unchanged from history_alliances_p504a_property_test.cpp when that file was split for the CTest shards.

#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p5_03_relations_runs.hpp"
#include "p5_04_alliances_checks.hpp"
#include "p5_04a_api.hpp"
#include "p5_04a_checks.hpp"
#include "p5_06_figures_runs.hpp"

namespace alp504a {

namespace db = worldgen::db;
namespace wd = worldgen::data;
using p503api::V;

// P5-04 18.3's alliances-small (the conflict stress data with the alliance edits) with 14.1's edits.
inline const wd::ContentSet& alliances_small() {
    static const wd::ContentSet c = p503api::with_history(
        p503api::with_history(conflict_test::stress_content(), "alliances",
                              {{"form_foe_permille", V(1000)}, {"form_fear_permille", V(1000)},
                               {"form_trade_permille", V(1000)}, {"form_friend_permille", V(1000)},
                               {"form_max_permille", V(1000)}, {"defend_permille", V(1000)},
                               {"refusals_to_break", V(1)}, {"term_years", V(10)}, {"trade_permille", V(1000)},
                               {"spoils_grant_permille", V(500)}, {"spoils_grant_min", V(300)},
                               {"spoils_grant_max", V(700)}, {"withheld_to_break", V(1)},
                               {"mine_permille", V(1000)}, {"mine_settlement_min", V(0)},
                               {"spoils_large_share", V(1)}, {"soured_renew_permille", V(0)},
                               {"weary_years", V(0)}}),
        "conflict", {{"allied_strain_permille", V(1000)}});
    return c;
}

// P5-04 13 without its item 12 (replaced at 0.28.0), then 16 item 13.
inline std::vector<std::string> violations(const std::string& path, const wd::ContentSet& content) {
    const db::WorldReader r = chrono_test::open_world(path);
    const p503check::World w = p503check::world_of_file(r, content);
    std::vector<std::string> out;
    for (const std::string& v : p504check::violations(w, p504check::geo_of_file(r), p504check::data_of(content))) {
        if (v.rfind("12:", 0) != 0) out.push_back(v);
    }
    for (const std::string& v : p504a_check::alliance_violations(w, p504a_check::tuning_of(content))) out.push_back(v);
    return out;
}

inline void check_world(const std::string& path, const wd::ContentSet& content, const std::string& label) {
    CAPTURE(label);
    CHECK(violations(path, content) == std::vector<std::string>{});
}

inline std::string coverage_line(const std::map<std::string, std::set<std::int64_t>>& cov) {
    std::string out;
    for (const auto& [k, vs] : cov) {
        out += (out.empty() ? "" : "; ") + k + ":";
        for (const std::int64_t v : vs) out += " " + std::to_string(v);
    }
    return out;
}

}  // namespace alp504a
