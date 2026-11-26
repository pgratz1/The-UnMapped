#pragma once

// The P5-06 tests' runs: content edits past the loader, the fixture figures-small of 18.3 (as the reference's
// small_data) and generated world files of any content (cached per process). No library name beyond the 0.26.0
// tests'.

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>

#include "p5_03_relations_runs.hpp"

namespace p506run {

namespace wd = worldgen::data;
namespace db = worldgen::db;
using p503api::V;

// Every definition of `kind` with the top-level fields `edits` replaced (naming: the culture naming record's).
inline wd::ContentSet with_fields(const wd::ContentSet& base, const std::string& kind,
                           const std::map<std::string, wd::Value>& edits, bool in_naming = false) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != kind) continue;
        for (wd::Definition& d : k.definitions) {
            const auto edit = [&](const wd::Record& in) {
                wd::Record r;
                for (std::size_t i = 0; i < in.size(); ++i) {
                    const auto it = edits.find(in.name_at(i));
                    r.add(in.name_at(i), it != edits.end() ? it->second : in.value_at(i));
                }
                return r;
            };
            if (!in_naming) {
                d.record = edit(d.record);
                continue;
            }
            wd::Record r;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const bool n = d.record.name_at(i) == "naming";
                r.add(d.record.name_at(i), n ? wd::Value::record(edit(d.record.value_at(i).as_record()))
                                             : d.record.value_at(i));
            }
            d.record = std::move(r);
        }
    }
    return c;
}

// Every monster kind's wake_rate times 2000, at most 10^7 (P5-02's monsters-small).
inline wd::ContentSet wake_x2000(const wd::ContentSet& base) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "monster") continue;
        for (wd::Definition& d : k.definitions) {
            wd::Record r;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const bool w = d.record.name_at(i) == "wake_rate";
                r.add(d.record.name_at(i), w ? V(std::min<std::int64_t>(10000000, d.record.value_at(i).as_int() * 2000))
                                             : d.record.value_at(i));
            }
            d.record = std::move(r);
        }
    }
    return c;
}

// 18.3 figures-small, as the reference's small_data: the alliances stress data (P5-04 18.3: the conflict stress data
// with the alliance edits) with P5-02's monsters-small edits (reading R33, so that heroes rise) and the figures
// edits: general_scale 1000 and every race's general_permille 1000, hero_scale 1000 and hero_permille 1000,
// exile_permille 1000, caster_ppm 1000000, book_permille 1000, blight_ppm 100000, fall_loser_permille 500, every
// culture's regnal_permille 1000, regnal_max 3.
inline const wd::ContentSet& figures_small() {
    static const wd::ContentSet c = [] {
        wd::ContentSet x = p503api::with_history(
            p503api::with_history(conflict_test::stress_content(), "alliances",
                                  {{"form_foe_permille", V(1000)}, {"form_fear_permille", V(1000)},
                                   {"form_trade_permille", V(1000)}, {"form_friend_permille", V(1000)},
                                   {"form_max_permille", V(1000)}, {"defend_permille", V(1000)},
                                   {"refusals_to_break", V(1)}, {"term_years", V(10)}, {"trade_permille", V(1000)},
                                   {"spoils_grant_permille", V(500)}, {"spoils_grant_min", V(300)},
                                   {"spoils_grant_max", V(700)}, {"withheld_to_break", V(1)},
                                   {"mine_permille", V(1000)}, {"mine_settlement_min", V(0)}}),
            "conflict", {{"allied_strain_permille", V(1000)}});
        x = p503api::with_history(x, "monsters", {{"living_permille", V(1000)}});
        x = with_fields(wake_x2000(x), "monster", {{"overrun_permille", V(1000)}});
        x = p503api::with_history(
            x, "figures",
            {{"general_scale_permille", V(1000)}, {"hero_scale_permille", V(1000)}, {"exile_permille", V(1000)},
             {"caster_ppm", V(1000000)}, {"book_permille", V(1000)}, {"blight_ppm", V(100000)},
             {"fall_loser_permille", V(500)}, {"regnal_max", V(3)}});
        x = with_fields(x, "race", {{"general_permille", V(1000)}, {"hero_permille", V(1000)}});
        return with_fields(x, "culture", {{"regnal_permille", V(1000)}}, true);
    }();
    return c;
}

inline std::string generated(std::uint64_t seed, std::int64_t size, const wd::ContentSet& content, const std::string& tag,
                      std::int64_t civs = 0, std::uint32_t threads = 1) {
    static std::map<std::string, std::string> cache;
    const std::string key = tag + "-" + std::to_string(seed) + "-" + std::to_string(size) + "-" + std::to_string(civs) +
                            "-" + std::to_string(threads);
    if (const auto it = cache.find(key); it != cache.end()) return it->second;
    chrono_api::Params p = chrono_test::params_with(6000, chrono_test::races_of_text("all"));
    if (civs > 0) p.civs = civs;
    const std::string path = chrono_test::world_dir().file("fig-" + key + ".db");
    const worldgen::WorldParams wp = chrono_test::world_params(seed, size, p, content, std::nullopt, threads);
    REQUIRE(worldgen::generate_world(wp, content, path, db::Overwrite::refuse).ok());
    cache[key] = path;
    return path;
}

}  // namespace p506run
