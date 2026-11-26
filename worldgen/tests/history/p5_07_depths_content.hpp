#pragma once

// The fixture depths-small of spec P5-07 4.2: monsters-small (P5-02 19.3) with the deep rules hurried, so that
// every path of 6 (deepening, waking, sealing, the fall of a hold, later takes, a realm's end and the last-refuge
// flight) occurs in a 512 world: deep_years 5, 10, 20, deep_delve_permille 1000, deep_wake_ppm 0, 200000,
// 600000, deep_takes_max 3; r2: goblin delving_permille 1000, deep_warren_population_min 100,
// deep_warren_wake_permille 1000, deep_aid_permille 1000. And the zero tuning of I0 (14): the shipped content with deep_delve_permille 0.
// Built by the generator engineer as data for the test writer; no test reads it yet.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "p5_02_monsters_content.hpp"
#include "worldgen/data/content.hpp"

namespace depths_content {

namespace wd = worldgen::data;

inline wd::Value three(std::int64_t a, std::int64_t b, std::int64_t c) {
    return wd::Value::array(std::vector<wd::Value>{wd::Value::integer(a), wd::Value::integer(b), wd::Value::integer(c)});
}

// The history.monsters edits of depths-small.
inline std::map<std::string, wd::Value> depths_small_edits() {
    return {
        {"living_permille", wd::Value::integer(1000)},  // as monsters-small
        {"deep_years", three(5, 10, 20)},
        {"deep_delve_permille", wd::Value::integer(1000)},
        {"deep_wake_ppm", three(0, 200000, 600000)},
        {"deep_takes_max", wd::Value::integer(3)},
        // r2 4.2: every path of 6.10 and 6.11.
        {"deep_warren_population_min", wd::Value::integer(100)},
        {"deep_warren_wake_permille", wd::Value::integer(1000)},
        {"deep_aid_permille", wd::Value::integer(1000)},
    };
}

// r2 4.2: `base` with race `race`'s field `field` set (added at the end when absent, as an optional field).
inline wd::ContentSet with_race_field(const wd::ContentSet& base, const std::string& race, const std::string& field,
                                      wd::Value value) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "race") continue;
        for (wd::Definition& d : k.definitions) {
            if (d.id != race) continue;
            wd::Record r;
            bool found = false;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const bool hit = d.record.name_at(i) == field;
                found = found || hit;
                r.add(d.record.name_at(i), hit ? value : d.record.value_at(i));
            }
            if (!found) r.add(field, value);
            d.record = std::move(r);
        }
    }
    return c;
}

// 4.2 depths-small: monsters-small's edits (wake_rate x 2000, overrun_permille 1000, living_permille 1000) with
// the deep edits. The deep kinds' wake_rate (their weights) are scaled with the rest; their ratios hold.
inline const wd::ContentSet& depths_small_content() {
    static const wd::ContentSet c = with_race_field(
        monsters_content::with_monsters(conflict_test::stress_content(), depths_small_edits(), {{"wake_rate", 2000}},
                                        {{"overrun_permille", wd::Value::integer(1000)}}),
        "goblin", "delving_permille", wd::Value::integer(1000));
    return c;
}

// 14 I0: `base` with deep_delve_permille 0 (no mine deepens, so none wakes). With forge_depth_permille also 0 the
// history tables equal those of a build without the deep rules.
inline wd::ContentSet zero_depths(const wd::ContentSet& base) {
    return monsters_content::with_monsters(base, {{"deep_delve_permille", wd::Value::integer(0)}}, {}, {});
}

}  // namespace depths_content
